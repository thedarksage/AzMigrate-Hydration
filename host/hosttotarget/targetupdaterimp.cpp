
///
/// \file targetupdaterimp.cpp
///
/// \brief
///

#include "errorexception.h"
#include "targetupdaterimp.h"

namespace HostToTarget {
    std::string TargetUpdaterImp::getErrorAsString()
    {
        std::string err;
        // check if internal error
        switch (m_internalError) {
            case ERROR_OK:
                break;
            case ERROR_INTERNAL_NOT_IMPLEMENTED:
                err = "feature not implemented";
                break;
            case ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED:
                err = osTypeToStr(m_info.m_osType) + " not supported";
                break;
            case ERROR_INTERNAL_MISSING_FILES_TO_INSTALL_PATH:
                err = "missing required files to install path";
                break;
            case ERROR_INTERNAL_COPYING_DRIVER_FILES:
                err = "error copying storage driver files";
                break;
            case ERROR_INTERNAL_BACKUP_HIVE:
                err = "error backing up hive";
                break;
            case ERROR_INTERNAL_BACKUP_PRIVILEGE:
                err = "error obtaining backup privilege";
                break;
            case ERROR_INTERNAL_RESTORE_PRIVILEGE:
                err = "error obtaining restore privilege";
                break;
            case ERROR_INTERNAL_LOAD_HIVE:
                err = "error loading hive";
                break;
            case ERROR_INTERNAL_REG_OPEN_KEY:
                err = "error opening registry key";
                break;
            case ERROR_INTERNAL_REG_UPDATE:
                err = "error updating registry value";
                break;
            case ERROR_INTERNAL_REG_INVALID_VALUE:
                err = "error invalid registry value";
                break;
            case ERROR_INTERNAL_SERVICE_MANUAL_START:
                err = "error setting services to start manually";
                break;
            case ERROR_INTERNAL_CONFIG_FILE_NOT_FOUND:
                err = "error unable to find config file";
                break;
            case ERROR_INTERNAL_CLEAR_CONFIG_SETTINGS:
                err = "error clearing config settings";
                break;
            case ERROR_INTERNAL_OS_NO_SUPPORTED_SCSI_CONTROLLERS_FOUND:
                err = "no supported scsi controllers found.";
                break;
            case ERROR_INTERNAL_OPEN_FILE:
                err = "error opening/creating file.";
                break;
            case ERROR_INTERNAL_MISSING_DRIVER_FILES:
                err = "missing required driver files";
                break;
            case ERROR_INTERNAL_REG_ENUM_KEY:
                err = "error enumerating registry key";
                break;
            case ERROR_INTERNAL_REG_DELETE_KEY:
                err = "error deleting registry key";
                break;
            case ERROR_INTERNAL_SAVE_HIVE:
                err = "error saving hive";
                break;
            case ERROR_INTERNAL_REG_GET_VALUE:
                err = "error getting registry value";
                break;
            case ERROR_INTERNAL_UPDATE_HAL:
                err = "error updating hal";
                break;
            case ERROR_INTERNAL_OTB_DIRECTORY:
                err = "error creating directory for out of the box drivers";
                break;
            case ERROR_INTERNAL_OTB_APPEND_DEVICEPATH:
                err = "error appending out of the box driver path to DevicePath";
                break;
            case ERROR_INTERNAL_RENAME_HIVE:
                err = "error renaming hive";
                break;
            case ERROR_INTERNAL_INTEL_IDE_SYS:
                err = "error installing intelide.sys driver needed for hyper-v";
                break;
            case ERROR_INTERNAL_INF_DEVICE_INSTALLER:
                err = "error searching inf files for needed devices";
                break;
            case ERROR_INTERNAL_INF_DEVICE_INSTALLER_NOT_IMPLEMENTED:
                err = "error inf full device install not implemented";
                break;
            default:
                // really should not happen, but could if new internal error added
                // but not added it to this fucntion.
                err = "internal error (";
                err += boost::lexical_cast<std::string>(m_internalError);
                err += ")";
                break;
        }
        if (!m_errMsg.empty()) {
            err += " ";
            err += m_errMsg;
        }
        if (ERROR_OK != m_systemError) {
            err += " - ";
            err += errorToString(m_systemError);
        }
        return err;
    }

} // namesapce HostToTarget
