#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/devinfo.h>
#include <sys/scsi.h>
#include <sys/scsi_buf.h>
#include <sys/scdisk.h>
#include "utildirbasename.h"
#include "platformbasedscsicommandissuer.h"
#include "inmdefines.h"

PlatformBasedScsiCommandIssuer::PlatformBasedScsiCommandIssuer()
{
    m_IssueCommands[0] = &PlatformBasedScsiCommandIssuer::IssueCommandToParent;
    m_IssueCommands[1] = &PlatformBasedScsiCommandIssuer::IssueCommandToDevice;
}


PlatformBasedScsiCommandIssuer::~PlatformBasedScsiCommandIssuer()
{
    EndSession();
}


bool PlatformBasedScsiCommandIssuer::StartSession(const std::string &device)
{
    EndSession();
    return m_DeviceAttributes.SetDevice(device, m_ErrMsg);
}


void PlatformBasedScsiCommandIssuer::EndSession(void)
{
    m_DeviceAttributes.ClearDevice();
}


std::string PlatformBasedScsiCommandIssuer::GetErrorMessage(void)
{
    return m_ErrMsg;
}


bool PlatformBasedScsiCommandIssuer::Issue(ScsiCmd_t *scmd, bool &shouldinspectscsistatus)
{
    if (-1 == m_DeviceAttributes.m_Fd)
    {
        m_ErrMsg = "Session did not start.";
        return false;
    }

    return (this->*m_IssueCommands[m_DeviceAttributes.m_IssueCommandToDevice])(scmd, shouldinspectscsistatus);
}


bool PlatformBasedScsiCommandIssuer::IssueCommandToParent(ScsiCmd_t *scmd, bool &shouldinspectscsistatus)
{
    /* DebugPrintf("ENTERED %s for %s\n", FUNCTION_NAME, scmd->m_TgtDev); */

    bool issued = false;
    if(m_DeviceAttributes.m_Parent.empty())
    {
        m_ErrMsg = std::string("Parent device of child device ") + scmd->m_TgtDev + " is not present in system.";
    }
    else
    {
        std::string parent = std::string("/dev/") + m_DeviceAttributes.m_Parent;
        int parentFd = open(parent.c_str(), O_RDONLY);
        if (-1 == parentFd )
        {
            m_ErrMsg = std::string("Failed to open parent device ") + parent + " of child device " + scmd->m_TgtDev + " with error: " + strerror(errno);
        }
        else
        {
            if ((0 == m_DeviceAttributes.m_Parent.find("fscsi")) || 
                (0 == m_DeviceAttributes.m_Parent.find("iscsi")))
            {
                issued = IssueSciolcmd(parentFd, scmd, shouldinspectscsistatus);
            }
            else if (0 == m_DeviceAttributes.m_Parent.find("vscsi"))
            {
                issued = IssueSciolcmd(parentFd, scmd, shouldinspectscsistatus);
                /* TODO: manual page mentions scioinqu works on vscsi not sciocmd,
                 * hence verify this */
                if (!issued)
                {
                    /* DebugPrintf(SV_LOG_DEBUG, "For device %s, SCIOLCMD ioctl to give scsi command failed. Trying SCIOCMD since its parent is vscsi\n", 
                                                 scmd->m_TgtDev); */
                    issued = IssueSciocmd(parentFd, scmd, shouldinspectscsistatus);
                }
            }
            else if (0 == m_DeviceAttributes.m_Parent.find("scsi"))
            {
                issued = IssueSciocmd(parentFd, scmd, shouldinspectscsistatus);
            }
            else
            {
                m_ErrMsg = std::string("Parent ") + m_DeviceAttributes.m_Parent + " of child device " + scmd->m_TgtDev + " does not name scsi device.";
            }
            close(parentFd);
        }
    }

    /* DebugPrintf("EXITED %s\n", FUNCTION_NAME); */
    return issued;
}


bool PlatformBasedScsiCommandIssuer::IssueSciolcmd(const int &fd, ScsiCmd_t *scmd, bool &shouldinspectscsistatus)
{
    /* DebugPrintf("ENTERED %s for %s\n", FUNCTION_NAME, scmd->m_TgtDev); */

    struct scsi_iocmd cmd;
    const int NRETRIESONEIO = 3;

    int i = 0;
    int rval;
    int saveerrno;
    do
    {
        memset(&cmd, 0, sizeof cmd);
        cmd.version = SCSI_VERSION_2;
        cmd.flags = B_READ;
        cmd.buffer = (char *)scmd->m_Xfer;
        cmd.data_length = scmd->m_XferLen;
        cmd.variable_cdb_ptr = (char *)scmd->m_Cmd;
        cmd.variable_cdb_length = scmd->m_CmdLen;
        cmd.autosense_buffer_ptr = (char *)scmd->m_Sense;
        cmd.autosense_length = scmd->m_SenseLen;
        cmd.timeout_value = scmd->m_TimeOut;

        cmd.scsi_id = m_DeviceAttributes.m_ScsiID;
        cmd.lun_id = m_DeviceAttributes.m_LunID;
        cmd.world_wide_name =  m_DeviceAttributes.m_WWName;
        cmd.node_name = m_DeviceAttributes.m_NodeName;

        rval = ioctl(fd, SCIOLCMD, &cmd);
        if (-1 != rval)
        {
            shouldinspectscsistatus = false;
            break;
        }
        else  
        {
            saveerrno = errno;
            std::stringstream ss;
            ss << "For device " << scmd->m_TgtDev
               << ", parent " << m_DeviceAttributes.m_Parent
               << ", ioctl SCIOLCMD failed with error " << strerror(saveerrno) 
               << ", status_validity = " << (unsigned int)cmd.status_validity
               << ", scsi_bus_status = " << (unsigned int)cmd.scsi_bus_status
               << ", adapter_status = " << (unsigned int)cmd.adapter_status;
            /* DebugPrintf(SV_LOG_WARNING, "%s\n", ss.str().c_str()); */
 
            if (EIO != saveerrno)
            {
                m_ErrMsg = ss.str();
                break;
            }
            else if ((i+1) < NRETRIESONEIO)
            {
                /* DebugPrintf(SV_LOG_WARNING, "Retrying as errno is EIO\n"); */
            }
            else
            {
                m_ErrMsg = ss.str();
            }
        }
        i++;
    } while(i < NRETRIESONEIO);

    /* DebugPrintf("EXITED %s\n", FUNCTION_NAME); */
    return (-1 != rval);
}


bool PlatformBasedScsiCommandIssuer::IssueSciocmd(const int &fd, ScsiCmd_t *scmd, bool &shouldinspectscsistatus)
{
    /* DebugPrintf("ENTERED %s for %s\n", FUNCTION_NAME, scmd->m_TgtDev); */

    struct sc_passthru cmd;
    const int NRETRIESONEIO = 3;

    int i = 0;
    int rval;
    int saveerrno;
    do
    {
        memset(&cmd, 0, sizeof cmd);
        cmd.version = SCSI_VERSION_2;
        cmd.flags = B_READ;
        cmd.buffer = (char *)scmd->m_Xfer;
        cmd.data_length = scmd->m_XferLen;
        cmd.variable_cdb_ptr = (char *)scmd->m_Cmd;
        cmd.variable_cdb_length = scmd->m_CmdLen;
        cmd.autosense_buffer_ptr = (char *)scmd->m_Sense;
        cmd.autosense_length = scmd->m_SenseLen;
        cmd.timeout_value = scmd->m_TimeOut;

        cmd.scsi_id = m_DeviceAttributes.m_ScsiID;
        cmd.lun_id = m_DeviceAttributes.m_LunID;
        cmd.world_wide_name =  m_DeviceAttributes.m_WWName;
        cmd.node_name = m_DeviceAttributes.m_NodeName;

        rval = ioctl(fd, SCIOCMD, &cmd);
        if (-1 != rval)
        {
            shouldinspectscsistatus = false;
            break;
        }
        else  
        {
            saveerrno = errno;
            std::stringstream ss;
            ss << "For device " << scmd->m_TgtDev
               << " ioctl SCIOCMD failed with error " << strerror(errno) 
               << ", einval_arg = " << cmd.einval_arg
               << ", status_validity = " << (unsigned int)cmd.status_validity
               << ", scsi_bus_status = " << (unsigned int)cmd.scsi_bus_status
               << ", adap_status_type = " << (unsigned int)cmd.adap_status_type
               << ", adapter_status = " << (unsigned int)cmd.adapter_status;
            /* DebugPrintf(SV_LOG_WARNING, "%s\n", ss.str().c_str()); */
 
            if (EIO != saveerrno)
            {
                m_ErrMsg = ss.str();
                break;
            }
            else if ((i+1) < NRETRIESONEIO)
            {
                /* DebugPrintf(SV_LOG_WARNING, "Retrying as errno is EIO\n"); */
            }
            else
            {
                m_ErrMsg = ss.str();
            }
        }
        i++;
    } while(i < NRETRIESONEIO);

    /* DebugPrintf("EXITED %s\n", FUNCTION_NAME); */
    return (-1 != rval);
}


bool PlatformBasedScsiCommandIssuer::IssueCommandToDevice(ScsiCmd_t *scmd, bool &shouldinspectscsistatus)
{
    /* DebugPrintf("ENTERED %s for %s\n", FUNCTION_NAME, scmd->m_TgtDev); */

    if (0 == m_DeviceAttributes.m_Parent.find("vscsi"))
    {
        // inquiry on vscsi adapters causing errors, so bypass
        return false;
    }

    struct sc_passthru passdata;
    memset(&passdata, 0, sizeof(struct sc_passthru));

    /* TODO: Following have to be determined:
     * 1. should we mix this with IO; curretnly 0 means quiesce(pause) io for this
     *    this request 
     * 2. The manual does not ask to check scsi status byte if ioctl is successful 
     * 3. residue ? (for linux and sun too ?)
     * 4. what about SCSI_VERSION_1 which does not support variable cdb ? 
     */
    passdata.version = SCSI_VERSION_2;
    passdata.flags = B_READ;

    passdata.buffer = (char *)scmd->m_Xfer;
    passdata.data_length = scmd->m_XferLen;
    passdata.variable_cdb_ptr = (char *)scmd->m_Cmd;
    passdata.variable_cdb_length = scmd->m_CmdLen;
    passdata.autosense_buffer_ptr = (char *)scmd->m_Sense;
    passdata.autosense_length = scmd->m_SenseLen;
    passdata.timeout_value = scmd->m_TimeOut;

    int rval = ioctl(m_DeviceAttributes.m_Fd, DK_PASSTHRU, &passdata);
    if (-1 != rval)
    {
        shouldinspectscsistatus = false;
    }
    else
    {
        std::stringstream ss;
        ss << "For device " << scmd->m_TgtDev
           << " ioctl DK_PASSTHRU failed with error " << strerror(errno) 
           << ", einval_arg = " << passdata.einval_arg
           << ", status_validity = " << (unsigned int)passdata.status_validity
           << ", scsi_bus_status = " << (unsigned int)passdata.scsi_bus_status
           << ", adap_status_type = " << (unsigned int)passdata.adap_status_type
           << ", adapter_status = " << (unsigned int)passdata.adapter_status;
        m_ErrMsg = ss.str();
    }
   
    /* DebugPrintf("EXITED %s\n", FUNCTION_NAME); */
    return  (-1 != rval);
}


PlatformBasedScsiCommandIssuer::DeviceAttributes::DeviceAttributes()
{
    Reset();
}


PlatformBasedScsiCommandIssuer::DeviceAttributes::~DeviceAttributes()
{
    ClearDevice();
}


bool PlatformBasedScsiCommandIssuer::DeviceAttributes::SetDevice(const std::string &device, std::string &errmsg)
{
    /* For scsi commands, aix does not need rhdisk to be
     * opened. TODO: verify for aix 5.3 */
    m_Fd = open(device.c_str(), O_RDONLY);
    if (-1 == m_Fd)
    {
        errmsg = std::string("Failed to open:") + device + " with error: " + strerror(errno);
    }
    else
    {
        m_Device = device;
         
        std::string basename = BaseName(m_Device);
        std::string nerrmsg;
        m_Parent = m_PlatformInfo.GetCustomizedDeviceParent(basename, nerrmsg);
        /* DebugPrintf("parent of %s is %s\n", m_Device.c_str(), m_Parent.c_str()); */
        if (!m_Parent.empty())
        {
            GetScsiLunInfo(basename, m_ScsiID, m_LunID, m_WWName, m_NodeName);
            std::stringstream ss;
            ss << "For device " << m_Device 
               << ", scsi id is " << m_ScsiID
               << ", lun id is " << m_LunID
               << ", ww name id is " << m_WWName
               << ", node name id is " << m_NodeName;

            /* DebugPrintf("%s\n", ss.str().c_str()); */
            if (m_ScsiID && m_LunID && m_WWName && m_NodeName)
            {
                m_IssueCommandToDevice = false;
            }
        }
    }

    return (-1 != m_Fd);
}


void PlatformBasedScsiCommandIssuer::DeviceAttributes::ClearDevice(void)
{
    if (-1 != m_Fd)
    {
        close(m_Fd);
        Reset();
    }
}


void PlatformBasedScsiCommandIssuer::DeviceAttributes::Reset(void)
{
    m_Device.clear();
    m_Fd = -1;
    m_Parent.clear();
    m_ScsiID = m_LunID = m_WWName = m_NodeName = 0;
    m_IssueCommandToDevice = true;
}


void PlatformBasedScsiCommandIssuer::DeviceAttributes::GetScsiLunInfo(const std::string &device, unsigned long long &scsi_id, unsigned long long &lunid, 
                                                    unsigned long long &wwname, unsigned long long &nodename)
{
    std::string errmsg;

    std::string sscsi_id = m_PlatformInfo.GetCustomizedAttributeValue(device, "scsi_id", errmsg);
    std::string slunid = m_PlatformInfo.GetCustomizedAttributeValue(device, "lun_id", errmsg);
    std::string swwname = m_PlatformInfo.GetCustomizedAttributeValue(device, "ww_name", errmsg);
    std::string snodename = m_PlatformInfo.GetCustomizedAttributeValue(device, "node_name", errmsg);

    InmHexStrToInteger(sscsi_id, scsi_id);
    InmHexStrToInteger(slunid, lunid);
    InmHexStrToInteger(swwname, wwname);
    InmHexStrToInteger(snodename, nodename);
}
