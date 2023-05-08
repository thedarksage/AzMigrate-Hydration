#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include "platformbasedatacommandissuer.h"


PlatformBasedAtaCommandIssuer::PlatformBasedAtaCommandIssuer()
: m_Fd(-1)
{
}


PlatformBasedAtaCommandIssuer::~PlatformBasedAtaCommandIssuer()
{
    EndSession();
}


bool PlatformBasedAtaCommandIssuer::StartSession(const std::string &device)
{
    EndSession();

    /* For scsi commands, aix does not need rhdisk to be
     * opened. TODO: verify for aix 5.3 */
    m_Fd = open(device.c_str(), O_RDONLY);
    if (-1 == m_Fd)
    {
        m_ErrMsg = std::string("Failed to open:") + device + " with error: " + strerror(errno);
    }
    else
    {
        m_Device = device;
    }

    return (-1 != m_Fd);
}


void PlatformBasedAtaCommandIssuer::EndSession(void)
{
    if (-1 != m_Fd)
    {
        close(m_Fd);
        m_Fd = -1;
        m_Device.clear();
    }
}


std::string PlatformBasedAtaCommandIssuer::GetErrorMessage(void)
{
    return m_ErrMsg;
}


bool
 PlatformBasedAtaCommandIssuer::Issue(const unsigned char *resp)
{
    if (-1 == m_Fd)
    {
        m_ErrMsg = "Session did not start\n";
        return false;
    }

    /* TODO: implement */

    return false;
}
