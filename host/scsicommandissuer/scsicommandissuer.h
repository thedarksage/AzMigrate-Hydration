#ifndef SCSI_COMMAND_ISSUER_H
#define SCSI_COMMAND_ISSUER_H

#include <string>
#include "scsicmd.h"
#include "platformbasedscsicommandissuer.h"


class ScsiCommandIssuer
{
public:
    bool StartSession(const std::string &device);
    void EndSession(void);
    bool Issue(ScsiCmd_t *scmd);
    bool DoInquiryCmd(INQUIRY_DETAILS *pInqDetails, const unsigned char &evpd,
                      const unsigned char &pagecode, unsigned char *pinqdata, 
                      const unsigned int &pinqdatalen);
    bool GetStdInqValues(INQUIRY_DETAILS *pInqDetails);

    std::string GetErrorMessage(void);

private:
    bool IsScsiCmdSuccess(const int &scsistatus, const unsigned char *sensebuf, const int &sensebuflen);
                         
private:
    PlatformBasedScsiCommandIssuer m_PlatformBasedScsiCommandIssuer;
    std::string m_ErrMsg;
};

#endif /* SCSI_COMMAND_ISSUER_H */
