#ifndef PLATFORM_BASED_SCSI_COMMAND_ISSUER_H
#define PLATFORM_BASED_SCSI_COMMAND_ISSUER_H

#include <string>
#include "scsicmd.h"

class PlatformBasedScsiCommandIssuer
{
public:
    PlatformBasedScsiCommandIssuer();
    ~PlatformBasedScsiCommandIssuer();
    bool Issue(ScsiCmd_t *scmd, bool &shouldinspectscsistatus);
    bool StartSession(const std::string &device);
    void EndSession(void);
    std::string GetErrorMessage(void);

private:
    std::string m_Device;
    int m_Fd;
    std::string m_ErrMsg;
};

#endif /* PLATFORM_BASED_SCSI_COMMAND_ISSUER_H */
