#ifndef PLATFORM_BASED_SCSI_COMMAND_ISSUER_H
#define PLATFORM_BASED_SCSI_COMMAND_ISSUER_H

#include <string>
#include "scsicmd.h"
#include "platforminfo.h"

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
    typedef bool (PlatformBasedScsiCommandIssuer::*IssueCommand_t)(ScsiCmd_t *scmd, bool &shouldinspectscsistatus);

    struct DeviceAttributes
    {
        std::string m_Device;
        int m_Fd;
        std::string m_Parent;
        unsigned long long m_ScsiID;
        unsigned long long m_LunID;
        unsigned long long m_WWName;
        unsigned long long m_NodeName;
        bool m_IssueCommandToDevice;

        DeviceAttributes();
        ~DeviceAttributes();
        bool SetDevice(const std::string &device, std::string &errmsg);
        void ClearDevice(void);
        void Reset(void);
        void GetScsiLunInfo(const std::string &device, unsigned long long &scsi_id, unsigned long long &lunid, unsigned long long &wwname,
                            unsigned long long &nodename);
    private:
        PlatformInfo m_PlatformInfo;
    };

    bool IssueCommandToParent(ScsiCmd_t *scmd, bool &shouldinspectscsistatus);
    bool IssueCommandToDevice(ScsiCmd_t *scmd, bool &shouldinspectscsistatus);
    bool IssueSciocmd(const int &fd, ScsiCmd_t *scmd, bool &shouldinspectscsistatus);
    bool IssueSciolcmd(const int &fd, ScsiCmd_t *scmd, bool &shouldinspectscsistatus);

private:
    std::string m_ErrMsg;
    DeviceAttributes m_DeviceAttributes;
    IssueCommand_t m_IssueCommands[2];
};

#endif /* PLATFORM_BASED_SCSI_COMMAND_ISSUER_H */
