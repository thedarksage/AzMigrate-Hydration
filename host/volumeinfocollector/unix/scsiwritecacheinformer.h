#ifndef _SCSIWRITE__CACHE__INFORMER__H_
#define _SCSIWRITE__CACHE__INFORMER__H_

#include "volumegroupsettings.h"
#include "scsicommandissuer.h"


typedef enum modesense
{
    E_MODE10,
    E_MODE6

} E_MODESENSE;

const int CACHEMODEPAGE = 0x8;

/* TODO: for DISABLEBLOCKDESC need to check if to set or not */
/* DISABLEBLOCKDESC and LONGLBAACCEPTED zero for now */
/* PAGECONTROLFIELD 0 specifies we need current values */
const int PAGECONTROLFIELD = 0;
const int PAGECODEFIELD = CACHEMODEPAGE;
const int SUBPAGECODEFIELD = 0;
const int DISABLEBLOCKDESC = 0;
const int LONGLBAACCEPTED = 0;


class ScsiWriteCacheInformer
{
public:
    ScsiWriteCacheInformer();
    ScsiWriteCacheInformer(ScsiCommandIssuer *pscsicommandissuer);
    bool Init(void);
    ~ScsiWriteCacheInformer();

    VolumeSummary::WriteCacheState 
     GetWriteCacheState(const std::string &disk);

private:
    typedef void (ScsiWriteCacheInformer::*FormModeSense_t)(unsigned char *cmd, const int &longLBAAccepted, const int &disableblockdesc, 
                                                            const int &pagecontrol, const int &pagecode, const int &subpagecode,
                                                            const size_t xferlen);

    void FormModeSense10Cmd(unsigned char *cmd, const int &longLBAAccepted, const int &disableblockdesc, const int &pagecontrol,
                            const int &pagecode, const int &subpagecode, const size_t xferlen);
    void FormModeSense6Cmd(unsigned char *cmd, const int &longLBAAccepted, const int &disableblockdesc, const int &pagecontrol,
                           const int &pagecode, const int &subpagecode, const size_t xferlen);
    bool IssueModeSense(const E_MODESENSE emode, const int &longLBAAccepted, const int &disableblockdesc, const int &pagecontrol, 
                        const int &pagecode, const int &subpagecode, unsigned char *xfer, const size_t xferlen, 
                        const char *dev);
    bool AreModeSenseLengthsValid(const E_MODESENSE emode,
                                  const int &modedatalen,
                                  const int &bdlen,
                                  const int &headerlen,
                                  const int &modepagestart,
                                  const int &modepagelen);
    VolumeSummary::WriteCacheState 
     GetWCFromModeSense(const std::string &disk, const E_MODESENSE emode);

private:
    ScsiCommandIssuer *m_pScsiCommandIssuer;
    bool m_IsScsiCommandIssuerExternal;
};

#endif /* _SCSIWRITE__CACHE__INFORMER__H_ */
