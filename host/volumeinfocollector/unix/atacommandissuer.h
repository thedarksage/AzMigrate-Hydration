#ifndef ATA_COMMAND_ISSUER_H
#define ATA_COMMAND_ISSUER_H

#include <string>
#include "platformbasedatacommandissuer.h"

class AtaCommandIssuer
{
public:
    bool StartSession(const std::string &device);
    void EndSession(void);
    bool Issue(const unsigned char *resp);
    std::string GetErrorMessage(void);

private:
    PlatformBasedAtaCommandIssuer m_PlatformBasedAtaCommandIssuer;
    std::string m_ErrMsg;
};

#endif /* ATA_COMMAND_ISSUER_H */
