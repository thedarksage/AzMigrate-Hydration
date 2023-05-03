#ifndef PLATFORM_BASED_ATA_COMMAND_ISSUER_H
#define PLATFORM_BASED_ATA_COMMAND_ISSUER_H

class PlatformBasedAtaCommandIssuer
{
public:
    PlatformBasedAtaCommandIssuer();
    ~PlatformBasedAtaCommandIssuer();
    bool Issue(const unsigned char *resp);
    bool StartSession(const std::string &device);
    void EndSession(void);
    std::string GetErrorMessage(void);

private:
    std::string m_Device;
    int m_Fd;
    std::string m_ErrMsg;
};

#endif /* PLATFORM_BASED_ATA_COMMAND_ISSUER_H */
