#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
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

    int retval = -1;

    #ifdef HDIO_DRIVE_CMD
    retval = ioctl(m_Fd, HDIO_DRIVE_CMD, resp);
    if (-1 != retval)
    {
        retval = 0;
    }
    #endif /* HDIO_DRIVE_CMD */

    return (retval==0);
}
