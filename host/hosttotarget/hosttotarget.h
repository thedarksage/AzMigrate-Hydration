
///
/// \file hosttotarget.h
///
/// \brief host to target header common to all
///

#ifndef HOSTTOTARGET_H
#define HOSTTOTARGET_H

#include <boost/lexical_cast.hpp>

#include "errorexception.h"

/// \brief the host to target library namespace
namespace HostToTarget {

    /// \brief supported targets for host-to-target
    ///
    /// use one of these when instantiating a TargetUpdater
    enum TargetTypes {
        HOST_TO_UNKNOWN    = 0x00000000,   ///< used to indicate target type no set
        HOST_TO_VMWARE     = 0x00000001,   ///< vmware target
        HOST_TO_XEN        = 0x00000002,   ///< xen target
        HOST_TO_HYPERVISOR = 0x00000004,   ///< hypervisor target
        HOST_TO_KVM        = 0x00000008,   ///< kvm target
        HOST_TO_PHYSICAL   = 0x00000010    ///< physical target
    };

    /// \brief options to be used when updating
    enum UPDATE_OPTIONS {
        UPDATE_OPTION_NORMAL                              = 0x0000,  ///< perform normal updates basically update registry settings and copy files as needed
        UPDATE_OTPION_MANUAL_START_SERVICES               = 0x0001,  ///< set well known services to manual start (e.g. backupagent service)
        UPDATE_OPTION_CLEAR_CONFIG_SETTINGS               = 0x0002,  ///< clear config (drscout.conf) settings related to backupagent
        UPDATE_OPTION_INSTALL_DRIVERS_ONLY                = 0x0004,  ///< only install drivers, skip all other updates   
        UPDATE_OPTION_REGISTRY_UPDATE_SOFTWARE_HIVE_ONLY  = 0x0008,  ///< only update software hive when performing registry update
        UPDATE_OPTION_CLEAN_OLD_DRIVERS                   = 0x0010,  ///< clean old drivers (delete enum key and all its subkeys)
        UPDATE_OPTION_VM_DO_NOT_USE_INF                   = 0x0020   ///< use built in vm values instead of inf to install drivers (default:  use inf)
    };

    inline std::string targetTypeToString(TargetTypes targetType)
    {
        switch (targetType) {
            case HOST_TO_UNKNOWN:
                return std::string("Unknown");
            case HOST_TO_VMWARE:
                return std::string("VMware");
            case HOST_TO_XEN:
                return std::string("Xen");
            case HOST_TO_HYPERVISOR:
                return std::string("Hyper-V");
            case HOST_TO_KVM:
                return std::string("KVM");
            case HOST_TO_PHYSICAL:
                return std::string("Physical");
            default:
                break;
        }
        std::string s;
        s = boost::lexical_cast<std::string>(targetType);
        s += " is not a valid target type value";
        return s;
    }

    /// \brief os types supported
    enum OsTypes {
        // NOTE: must keep to 7 hex digits as this is used in places where negative values are not allowed
        OS_TYPE_NOT_SUPPORTED = 0x0000000,   ///< unsupported os
        OS_TYPE_XP_32         = 0x0000001,   ///< xp 32-bit
        OS_TYPE_XP_64         = 0x0000002,   ///< xp 64-bit
        OS_TYPE_W2K3_32       = 0x0000004,   ///< windows server 2003 32-bit
        OS_TYPE_W2K3_64       = 0x0000008,   ///< windows server 2003 64-bit
        OS_TYPE_VISTA_32      = 0x0000010,   ///< windows vista 32 bit
        OS_TYPE_VISTA_64      = 0x0000020,   ///< windows vista 64 bit
        OS_TYPE_W2K8_32       = 0x0000040,   ///< windows server 2008 32-bit
        OS_TYPE_W2K8_64       = 0x0000080,   ///< windows server 2008 64-bit
        OS_TYPE_W2K8_R2       = 0x0000100,   ///< windows server 2008 r2
        OS_TYPE_WIN7          = 0x0000200,   ///< windows 7
        OS_TYPE_W2K12         = 0x0000400,   ///< windows server 2012
        OS_TYPE_WIN8          = 0x0000800,   ///< windows 8
    };

    int const OS_TYPES_COUNT = 13; // must match the count of OsTypes enums

    OsTypes osNameToOsType(std::string const& osName); // os name. any format the implemenation understands and can convert to an OsTypes

    // returns false if osType not one of the know types
    bool osTypeToMajMinArch(OsTypes osType, // OsType to break up into parts
                            int& maj,       // set to major version
                            int& min,       // set to minor version
                            int& arch       // set to arch (32 or 64)
                            );
    
    OsTypes osMajMinArchToOsType(std::string const& osMajorVersion,  // os major version number
                                 std::string const& osMinorVersion,  // os minor version numbers
                                 std::string const& arch,            // architiecture should be 32 or 64
                                 bool serverVersion
                                 );

    inline std::string osTypeToStr(OsTypes osType)
    {
        std::string osStr;
        switch (osType) {
            case OS_TYPE_NOT_SUPPORTED:
                osStr = "OS_TYPE_NOT_SUPPORTED";
                break;
            case OS_TYPE_XP_32:
                osStr = "OS_TYPE_XP_32";
                break;
            case OS_TYPE_XP_64:
                osStr = "OS_TYPE_XP_64";
                break;
            case OS_TYPE_W2K3_32:
                osStr = "OS_TYPE_W2K3_32";
                break;
            case OS_TYPE_W2K3_64:
                osStr = "OS_TYPE_W2K3_64";
                break;
            case OS_TYPE_W2K8_32:
                osStr = "OS_TYPE_W2K8_32";
                break;
            case OS_TYPE_W2K8_64:
                osStr = "OS_TYPE_W2K8_64";
                break;
            case OS_TYPE_W2K8_R2:
                osStr = "OS_TYPE_W2K8_R2";
                break;
            case OS_TYPE_WIN7:
                osStr = "OS_TYPE_WIN7";
                break;
            case OS_TYPE_W2K12:
                osStr = "OS_TYPE_W2k12";
                break;
            case OS_TYPE_WIN8:
                osStr = "OS_TYPE_WIN8";
                break;
            default:
                osStr = "unknown os type: ";
                osStr += boost::lexical_cast<std::string>(osType);
                break;
        }
        return osStr;
    }

    /// \brief formats system volume to proper name format
    std::string toVolumeNameFormat(std::string const& systemVolume);

    /// \brief gets the target system direcotry including repalcing original drive to new target drive if needed
    std::string getWindowsDir(std::string const& systemVolume, std::string const& windowsDir);

    /// \brief holds information about the host and target
    struct HostToTargetInfo {
        /// \brief consructor
        HostToTargetInfo()
            {
            }

        /// \brief constructor
        HostToTargetInfo(TargetTypes targetType,                                ///< target type
                         OsTypes osType,                                        ///< host os type
                         std::string const& installPath,                        ///< InMage product install path
                         std::string const& systemVolume = std::string(),       ///< system volume (required for windows, otherwise otpional)
                         std::string const& windowsDir = std::string(),         ///< windows dir (required for windows, otherwise otpional)
                         std::string const& outOfTheBoxDrivers = std::string()  ///< currently needs to be directory that has all driver files needed
                         )
            : m_targetType(targetType),
              m_osType(osType),
              m_installPath(installPath),
              m_outOfTheBoxDrivers(outOfTheBoxDrivers)
            {
                if (!systemVolume.empty()) {
                    m_systemVolume = toVolumeNameFormat(systemVolume);
                }
                if (!windowsDir.empty()) {
                    m_windowsDir = getWindowsDir(m_systemVolume, windowsDir);
                }
            }

        TargetTypes m_targetType;          ///< target type (required)
        OsTypes m_osType;                  ///< os type (required)
        std::string m_installPath;         ///< InMage product install path (required)
        std::string m_systemVolume;        ///< name of the system volume (required on windows, optional otherwise)
        std::string m_windowsDir;          ///< windows directory (required on windows, optional otherwise)
        std::string m_outOfTheBoxDrivers;  ///< out of the box drivers (can be either directory or specific file (.inf))
    };

    typedef long ERR_TYPE; ///< error type

    // NOTE: if adding a new internal error make sure to add it to TargetUpdaterImp::getErrorAsString()
    ERR_TYPE const ERROR_INTERNAL_NOT_IMPLEMENTED(-1);                          ///< feature not implmented for host to target combination
    ERR_TYPE const ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED(-2);                    ///< provided os name not supported for target
    ERR_TYPE const ERROR_INTERNAL_MISSING_FILES_TO_INSTALL_PATH(-3);            ///< missing files to install path when it was required
    ERR_TYPE const ERROR_INTERNAL_COPYING_DRIVER_FILES(-4);                     ///< error copying driver files
    ERR_TYPE const ERROR_INTERNAL_BACKUP_HIVE(-5);                              ///< failed while backing up hive file
    ERR_TYPE const ERROR_INTERNAL_BACKUP_PRIVILEGE(-6);                         ///< failed to get backup privilege
    ERR_TYPE const ERROR_INTERNAL_RESTORE_PRIVILEGE(-7);                        ///< failed to get restore priviliege
    ERR_TYPE const ERROR_INTERNAL_LOAD_HIVE(-8);                                ///< failed to load hive
    ERR_TYPE const ERROR_INTERNAL_REG_OPEN_KEY(-9);                             ///< failed to open the registry key
    ERR_TYPE const ERROR_INTERNAL_REG_UPDATE(-10);                              ///< failed to update registry info
    ERR_TYPE const ERROR_INTERNAL_REG_INVALID_VALUE(-11);                       ///< failed invalid registry value
    ERR_TYPE const ERROR_INTERNAL_SERVICE_MANUAL_START(-12);                    ///< failed to set services to start manually
    ERR_TYPE const ERROR_INTERNAL_CONFIG_FILE_NOT_FOUND(-13);                   ///< config file not found while trying to clear config settings
    ERR_TYPE const ERROR_INTERNAL_OS_NO_SUPPORTED_SCSI_CONTROLLERS_FOUND(-14);  ///< failed to find any supported scsi controllers
    ERR_TYPE const ERROR_INTERNAL_CLEAR_CONFIG_SETTINGS(-15);                   ///< failed to properly clear config settings
    ERR_TYPE const ERROR_INTERNAL_OPEN_FILE(-16);                               ///< failed to open file
    ERR_TYPE const ERROR_INTERNAL_MISSING_DRIVER_FILES(-17);                    ///< missing required driver files
    ERR_TYPE const ERROR_INTERNAL_REG_ENUM_KEY(-18);                            ///< failded enumerating registry key
    ERR_TYPE const ERROR_INTERNAL_REG_DELETE_KEY(-19);                          ///< failed delete registry key
    ERR_TYPE const ERROR_INTERNAL_SAVE_HIVE(-20);                               ///< failed to save hive
    ERR_TYPE const ERROR_INTERNAL_REG_GET_VALUE(-21);                           ///< failed to get registry value
    ERR_TYPE const ERROR_INTERNAL_UPDATE_HAL(-22);                              ///< failed to update hal
    ERR_TYPE const ERROR_INTERNAL_OTB_DIRECTORY(-23);                           ///< failed to create out of the box driver directory
    ERR_TYPE const ERROR_INTERNAL_OTB_COPY(-24);                                ///< failed to copy device files
    ERR_TYPE const ERROR_INTERNAL_OTB_APPEND_DEVICEPATH(-25);                   ///< failed to append device path
    ERR_TYPE const ERROR_INTERNAL_RENAME_HIVE(-26);                             ///< failed to rename hive
    ERR_TYPE const ERROR_INTERNAL_INTEL_IDE_SYS(-27);                           ///< failed to install intelide.sys file needed for hyprv
    ERR_TYPE const ERROR_INTERNAL_INF_DEVICE_INSTALLER(-28);                    ///< inf device isntaller encountered an error
    ERR_TYPE const ERROR_INTERNAL_INF_DEVICE_INSTALLER_NOT_IMPLEMENTED(-29);    ///< inf device isntaller full install not implemented yet

    /// \brief returns the error message for the given error
    ///
    /// \return
    /// \li std::string holding the error message for the given error code in err
    ///  if the error message can not be found, returns error message stating
    ///  "failed to get message for error code: err"
    std::string errorToString(ERR_TYPE err); ///< error code to look up

    /// \brief dummy system volume name used when going calling validation before knowing the actual system volume
    std::string const SystemVolumeForValidation("dummy");

} // namespace HostToTarget


#endif // HOSTTOTARGET_H
