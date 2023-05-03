#include <string>
#include <sstream>
#include "scsiwritecacheinformer.h"
#include "inmsafecapis.h"

ScsiWriteCacheInformer::ScsiWriteCacheInformer()
: m_IsScsiCommandIssuerExternal(false),
  m_pScsiCommandIssuer(0)
{
}


ScsiWriteCacheInformer::ScsiWriteCacheInformer(ScsiCommandIssuer *pscsicommandissuer)
: m_pScsiCommandIssuer(pscsicommandissuer),
  m_IsScsiCommandIssuerExternal(true)
{
}


bool ScsiWriteCacheInformer::Init(void)
{
    bool initialized = true;
   
    if (!m_IsScsiCommandIssuerExternal)
    {
        m_pScsiCommandIssuer = new (std::nothrow) ScsiCommandIssuer();
        initialized = m_pScsiCommandIssuer ? true : false;
    }

    return initialized;
}


ScsiWriteCacheInformer::~ScsiWriteCacheInformer()
{
    if (!m_IsScsiCommandIssuerExternal)
    {
        if (m_pScsiCommandIssuer)
        {
            delete m_pScsiCommandIssuer;
        }
    }
}


VolumeSummary::WriteCacheState 
 ScsiWriteCacheInformer::GetWriteCacheState(const std::string &disk)
{
    VolumeSummary::WriteCacheState state = VolumeSummary::WRITE_CACHE_DONTKNOW;
    if (!m_pScsiCommandIssuer->StartSession(disk))
    {
        /* NOT in use 
        DebugPrintf(SV_LOG_WARNING, "scsi command session start for %s failed with %s\n", 
                    disk.c_str(), m_pScsiCommandIssuer->GetErrorMessage().c_str());
        */
        return state;
    }

    E_MODESENSE emode = E_MODE10;
    state = GetWCFromModeSense(disk, emode);
    if (VolumeSummary::WRITE_CACHE_DONTKNOW == state)
    {
        emode = E_MODE6;
        state = GetWCFromModeSense(disk, emode);
    }

    m_pScsiCommandIssuer->EndSession();
    return state;
}


VolumeSummary::WriteCacheState 
 ScsiWriteCacheInformer::GetWCFromModeSense(const std::string &disk, const E_MODESENSE emode)
{
    VolumeSummary::WriteCacheState state = VolumeSummary::WRITE_CACHE_DONTKNOW;
    /* TODO: Although there are 2 bytes for mode data length 
     * in mode sense 10, we do not want to allocate more than
     * 1 kb */
    unsigned char modedata[TYPICALMODEDATALEN] = {0};
    /* actual page */
    unsigned char modepage[TYPICALMODEPAGELEN] = {0};

    bool bismodesensesuccess = IssueModeSense(emode, LONGLBAACCEPTED, DISABLEBLOCKDESC, PAGECONTROLFIELD, PAGECODEFIELD, 
                                              SUBPAGECODEFIELD, modedata, MODESENSE10HDRLEN,
                                              disk.c_str());
    if (bismodesensesuccess)
    {
        int modedatalen = 0;
        int bdlen = 0;
        int modepagestart = 0;
        int headerlen = MODESENSE10HDRLEN;
        if (E_MODE6 == emode)
        {
            /* + 1 is done since modedata[0] has mode data length excluding itself */
            modedatalen = modedata[0] + 1;
            bdlen = modedata[3];
            headerlen = MODESENSE6HDRLEN;
        }
        else if (E_MODE10 == emode)
        {
            /* + 2 is done since modedata[0] has mode data length excluding itself */
            modedatalen = ((modedata[0] << 8) + modedata[1] + 2);
            bdlen = (modedata[6] << 8) + modedata[7];
        }
        modepagestart = bdlen + headerlen;
        if (modedatalen > TYPICALMODEDATALEN)
        {
            modedatalen = TYPICALMODEDATALEN;
        }

        int modepagelen = modedatalen - modepagestart;
        if (modepagelen > TYPICALMODEPAGELEN)
        {
            modepagelen = TYPICALMODEPAGELEN;
        }
        bool barevalid = AreModeSenseLengthsValid(emode, modedatalen, bdlen, headerlen, modepagestart, modepagelen);

        if (barevalid)
        {
            /* TODO: For now trust that modedata is written upto
               only MODESENSE10HDRLEN. but may need to do
               memset of whole TYPICALMODEDATALEN */
            memset(modedata, 0, MODESENSE10HDRLEN);
            bismodesensesuccess = IssueModeSense(emode, LONGLBAACCEPTED, DISABLEBLOCKDESC, PAGECONTROLFIELD, PAGECODEFIELD, 
                                                 SUBPAGECODEFIELD, modedata, modedatalen,
                                                 disk.c_str());
            if (bismodesensesuccess && (modepagelen > 0))
            {
                inm_memcpy_s(modepage, sizeof(modepage), modedata + modepagestart, modepagelen);
                state = (modepage[2] & 0x4) ? 
                        VolumeSummary::WRITE_CACHE_ENABLED :
                        VolumeSummary::WRITE_CACHE_DISABLED;
            }
        }
    }

    return state;
}


bool ScsiWriteCacheInformer::IssueModeSense(const E_MODESENSE emode, const int &longLBAAccepted, const int &disableblockdesc, 
                                            const int &pagecontrol, const int &pagecode, const int &subpagecode,
                                            unsigned char *xfer, const size_t xferlen, const char *dev)
{
    unsigned char sense[SENSELEN] = {0};
    unsigned char cmd[LENMODESENSE10CMD] = {0};
    size_t cmdlen = (E_MODE6 == emode) ? LENMODESENSE6CMD : LENMODESENSE10CMD;

    FormModeSense_t modes[] = {&ScsiWriteCacheInformer::FormModeSense10Cmd, &ScsiWriteCacheInformer::FormModeSense6Cmd};
    FormModeSense_t formmodesensecmd = modes[emode];

    (this->*formmodesensecmd)(cmd, longLBAAccepted, disableblockdesc, pagecontrol,
                              pagecode, subpagecode, xferlen);

    ScsiCmd_t scmd;
    scmd.setinput(dev, 
                  cmd, cmdlen, 
                  xfer, xferlen,
                  sense, sizeof sense,
                  DEF_TIMEOUT);
    bool biscmdsuccess = m_pScsiCommandIssuer->Issue(&scmd);

    return biscmdsuccess;
}


void ScsiWriteCacheInformer::FormModeSense10Cmd(unsigned char *cmd, const int &longLBAAccepted, const int &disableblockdesc, const int &pagecontrol,
                                                const int &pagecode, const int &subpagecode, const size_t xferlen)
{
    cmd[0] = (unsigned char) MODESENSE10CODE;
    cmd[1] = (unsigned char) ((disableblockdesc ? 0x8 : 0) | (longLBAAccepted ? 0x10 : 0));
    cmd[2] = (unsigned char) (((pagecontrol << 6) & 0xc0) | (pagecode & 0x3f)); 
    cmd[3] = (unsigned char) (subpagecode & 0xff);
    cmd[7] = (unsigned char) ((xferlen >> 8) & 0xff);
    cmd[8] = (unsigned char) (xferlen & 0xff);
}


void ScsiWriteCacheInformer::FormModeSense6Cmd(unsigned char *cmd, const int &longLBAAccepted, const int &disableblockdesc, const int &pagecontrol,
                                               const int &pagecode, const int &subpagecode, const size_t xferlen)
{
    cmd[0] = (unsigned char) MODESENSE6CODE;
    cmd[1] = (unsigned char) (disableblockdesc ? 0x8 : 0);
    cmd[2] = (unsigned char) (((pagecontrol << 6) & 0xc0) | (pagecode & 0x3f));
    cmd[3] = (unsigned char) (subpagecode & 0xff);
    cmd[4] = (unsigned char) (xferlen & 0xff);
}


bool ScsiWriteCacheInformer::AreModeSenseLengthsValid(const E_MODESENSE emode,
                                                      const int &modedatalen,
                                                      const int &bdlen,
                                                      const int &headerlen,
                                                      const int &modepagestart,
                                                      const int &modepagelen)
{
    bool barevalid = false;

    if ((modepagelen > 0) && (modedatalen > modepagestart))
    {
        barevalid = true;
    }
    
    return barevalid;
}
