#include <string>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/scsi/generic/commands.h>
#include <sys/scsi/generic/inquiry.h>
#include <sys/scsi/impl/uscsi.h>
#include "scsicmd.h"
#include "logger.h"
#include "portable.h"
#include "platformbasedscsicommandissuer.h"

#include "boost/algorithm/string/replace.hpp"

/* TODO: unify this call */
std::string GetNameToIssueUscsiCommand(const std::string &dskname);

PlatformBasedScsiCommandIssuer::PlatformBasedScsiCommandIssuer()
: m_Fd(-1)
{
}


PlatformBasedScsiCommandIssuer::~PlatformBasedScsiCommandIssuer()
{
    EndSession();
}


bool PlatformBasedScsiCommandIssuer::StartSession(const std::string &device)
{
    EndSession();

    std::string rawdevice = GetNameToIssueUscsiCommand(device);
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


void PlatformBasedScsiCommandIssuer::EndSession(void)
{
    if (-1 != m_Fd)
    {
        close(m_Fd);
        m_Fd = -1;
        m_Device.clear();
    }
}


std::string PlatformBasedScsiCommandIssuer::GetErrorMessage(void)
{
    return m_ErrMsg;
}


bool PlatformBasedScsiCommandIssuer::Issue(ScsiCmd_t *scmd, bool &shouldinspectscsistatus)
{
    if (-1 == m_Fd)
    {
        m_ErrMsg = "Session did not start\n";
        return false;
    }

    bool bissuccess = false;
    int rval = -1;
    struct uscsi_cmd ucmd;

    memset(&ucmd, 0, sizeof ucmd);
    /* This library supports for now only read; refer man uscsi(7I) */
    ucmd.uscsi_flags = (USCSI_DIAGNOSE | USCSI_ISOLATE | USCSI_READ | USCSI_SILENT | USCSI_RQENABLE);
    /* TODO: remove casts */
    ucmd.uscsi_bufaddr = (char *)scmd->m_Xfer;
    ucmd.uscsi_buflen = scmd->m_XferLen;
    ucmd.uscsi_cdb =(char *) scmd->m_Cmd;
    ucmd.uscsi_cdblen = scmd->m_CmdLen;
    ucmd.uscsi_rqbuf = (char *)scmd->m_Sense;
    ucmd.uscsi_rqlen = scmd->m_SenseLen;
    ucmd.uscsi_timeout = scmd->m_TimeOut;

    rval = ioctl(m_Fd, USCSICMD,&ucmd);
    if (-1 != rval)
    {
        size_t xferlen = ucmd.uscsi_buflen - ucmd.uscsi_resid;
        uchar_t senselen = ucmd.uscsi_rqlen - ucmd.uscsi_rqresid;
        scmd->m_RealSenseLen = senselen;
        scmd->m_RealXferLen = xferlen;
        scmd->m_ScsiStatus = ucmd.uscsi_status;
        bissuccess = true;
        shouldinspectscsistatus = true;
    }
    else
    {
        m_ErrMsg = std::string("For device: ") + m_Device + " ioctl USCSICMD failed with error: " + strerror(errno);
    }

    return bissuccess;
}


std::string GetNameToIssueUscsiCommand(const std::string &dskname)
{
    std::string rdskname = dskname;
    boost::algorithm::replace_first(rdskname, DSKDIR, RDSKDIR);
    boost::algorithm::replace_first(rdskname, DMPDIR, RDMPDIR);
    return rdskname;
}
