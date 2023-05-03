#include <sstream>
#include <inttypes.h>
#include "scsiluncapacityinformer.h"
#include "scsicmd.h"

ScsiLunCapacityInformer::ScsiLunCapacityInformer()
: m_IsScsiCommandIssuerExternal(false),
  m_pScsiCommandIssuer(0)
{
}


ScsiLunCapacityInformer::ScsiLunCapacityInformer(ScsiCommandIssuer *pscsicommandissuer)
: m_pScsiCommandIssuer(pscsicommandissuer),
  m_IsScsiCommandIssuerExternal(true)
{
}


bool ScsiLunCapacityInformer::Init(void)
{
    bool initialized = true;
   
    if (!m_IsScsiCommandIssuerExternal)
    {
        m_pScsiCommandIssuer = new (std::nothrow) ScsiCommandIssuer();
        initialized = m_pScsiCommandIssuer ? true : false;
    }

    return initialized;
}


ScsiLunCapacityInformer::~ScsiLunCapacityInformer()
{
    if (!m_IsScsiCommandIssuerExternal)
    {
        if (m_pScsiCommandIssuer)
        {
            delete m_pScsiCommandIssuer;
        }
    }
}


void ScsiLunCapacityInformer::GetCapacity(const std::string &device, Capacity_t *pluncap)
{
    if (!m_pScsiCommandIssuer->StartSession(device))
    {
        /* NOT in use 
        DebugPrintf(SV_LOG_WARNING, "scsi command session start for %s failed with %s\n", 
                    disk.c_str(), m_pScsiCommandIssuer->GetErrorMessage().c_str());
        */
        return;
    }

    const int READCAPLEN = 10;
    const int READCAPCMD = 0x25;
    const int READCAPOUTLEN = 8;
    unsigned char sense[SENSELEN] = {0};
    unsigned char cmd[READCAPLEN] = {READCAPCMD};
    unsigned char xfer[READCAPLEN] = {0};

    ScsiCmd_t scmd;
    scmd.setinput(device.c_str(), 
                  cmd, sizeof cmd, 
                  xfer, sizeof xfer,
                  sense, sizeof sense,
                  DEF_TIMEOUT);
    bool biscmdsuccess = m_pScsiCommandIssuer->Issue(&scmd);

    if (biscmdsuccess)
    {
        uint32_t lastblk = 0;
        uint32_t blksz = 0;
  
        lastblk = 0xff & *(xfer + 3);
        lastblk |= (0xff & *(xfer + 2)) << 8;
        lastblk |= (0xff & *(xfer + 1)) << 16;
        lastblk |= (0xff & *xfer) << 24;
        blksz = 0xff & *(xfer + 7);
        blksz |= (0xff & *(xfer + 6)) << 8;
        blksz |= (0xff & *(xfer + 5)) << 16;
        blksz |= (0xff & *(xfer + 4)) << 24;

        pluncap->m_Nblks = lastblk + 1;
        pluncap->m_BlkSz = blksz;
    }

    m_pScsiCommandIssuer->EndSession();
}

