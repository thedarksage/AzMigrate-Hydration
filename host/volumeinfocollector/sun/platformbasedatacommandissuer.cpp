#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include "volumemanagerfunctions.h"
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

    std::string rawdevice = GetRawDeviceName(device);
    m_Fd = open(rawdevice.c_str(), O_RDONLY);
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

    /* TODO: find */
    return false;
}
