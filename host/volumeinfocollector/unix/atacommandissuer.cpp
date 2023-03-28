#include "atacommandissuer.h"


bool AtaCommandIssuer::StartSession(const std::string &device)
{
    bool bstarted = m_PlatformBasedAtaCommandIssuer.StartSession(device);

    if (!bstarted)
    {
        m_ErrMsg = m_PlatformBasedAtaCommandIssuer.GetErrorMessage();
    }

    return bstarted;
}


void AtaCommandIssuer::EndSession(void)
{
    m_PlatformBasedAtaCommandIssuer.EndSession();
}


std::string AtaCommandIssuer::GetErrorMessage(void)
{
    return m_ErrMsg;
}


bool AtaCommandIssuer::Issue(const unsigned char *resp)
{
    return m_PlatformBasedAtaCommandIssuer.Issue(resp);
}
