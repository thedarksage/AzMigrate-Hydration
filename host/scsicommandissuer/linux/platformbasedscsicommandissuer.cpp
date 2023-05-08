#include <string>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <linux/types.h>
#include "scsicmd.h"
#include "logger.h"
#include "portable.h"
#include "platformbasedscsicommandissuer.h"


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
    struct sg_io_hdr io_hdr;

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = scmd->m_CmdLen;
    io_hdr.mx_sb_len = scmd->m_SenseLen;
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = scmd->m_XferLen;
    io_hdr.dxferp = scmd->m_Xfer;
    io_hdr.cmdp = scmd->m_Cmd;
    io_hdr.sbp = scmd->m_Sense;
    io_hdr.timeout = (scmd->m_TimeOut * SECTOMSECFACTOR);

    #ifdef SG_IO
    rval = ioctl(m_Fd, SG_IO, &io_hdr);
    if (-1 != rval)
    {
        scmd->m_RealSenseLen = io_hdr.sb_len_wr;
        scmd->m_ScsiStatus = io_hdr.status & SCSISTATUSMASK;
        bissuccess = true;
        shouldinspectscsistatus = true;
    }
    else
    {
        m_ErrMsg = std::string("For device: ") + m_Device + " ioctl SGIO failed with error: " + strerror(errno);
    }
    #endif /* SG_IO */

    return bissuccess;
}
