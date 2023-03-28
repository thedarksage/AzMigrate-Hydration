
///
/// \file win/vmwareupdatermajor.cpp
///
/// \brief targt updater major implementation for windows
///

#include <windows.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "scopeguard.h"
#include "vmwareupdatermajor.h"
#include "extractcabfiles.h"

namespace HostToTarget {

    char* SYMMPI_SYS_TARGET = "windows\\system32\\drivers\\symmpi.sys";
    char* SYMMPI_SYS_32 = "32bit\\symmpi.sys";
    char* SYMMPI_SYS_64 = "64bit\\symmpi.sys";

    /// \brief holds the support OsTypes and driver file info that may need to be copied
    SupportedOsTypeAndOptionalDriverFileInfo g_vmwareSupportedOsTypeAndOptionalDriverFileInfo[] =
    {
        { OS_TYPE_XP_32, "symmpi.sys", 0},
        { OS_TYPE_XP_64, "symmpi.sys", 0},
        { OS_TYPE_W2K3_32, "symmpi.sys", 0},
        { OS_TYPE_W2K3_64, "symmpi.sys", 0},
        { OS_TYPE_VISTA_32, 0, 0 },
        { OS_TYPE_VISTA_64, 0, 0 },
        { OS_TYPE_W2K8_32, 0, 0 },
        { OS_TYPE_W2K8_64, 0, 0 },
        { OS_TYPE_W2K8_R2, 0, 0 },
        { OS_TYPE_WIN7, 0, 0 },
        { OS_TYPE_W2K12, 0, 0 },
        { OS_TYPE_WIN8, 0, 0 },
        { OS_TYPE_NOT_SUPPORTED, 0, 0 }
    };

    ERROR_RESULT VmwareUpdaterMajor::validate()
    {
        int i = 0;
        while (OS_TYPE_NOT_SUPPORTED != g_vmwareSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
            if (m_info.m_osType == g_vmwareSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
                break;
            }
            ++i;
        }
        if (OS_TYPE_NOT_SUPPORTED == g_vmwareSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
            return ERROR_RESULT(ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED);
        }
        return ERROR_RESULT(ERROR_OK);
    }

    ERROR_RESULT VmwareUpdaterMajor::update(int opts, ORHKEY hiveKey)
    {
        int i = 0;
        while (OS_TYPE_NOT_SUPPORTED != g_vmwareSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
            if (m_info.m_osType == g_vmwareSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
                break;
            }
            ++i;
        }
        if (OS_TYPE_NOT_SUPPORTED == g_vmwareSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
            return ERROR_RESULT(ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED);
        }

        if (!m_registryUpdater.update(m_info.m_systemVolume, m_info.m_windowsDir, HOST_TO_VMWARE, m_info.m_osType, opts)) {
            return ERROR_RESULT(m_registryUpdater.getInternalError(), m_registryUpdater.getSystemError(), m_registryUpdater.getInternalErrorMsg());
        }
        return ERROR_RESULT(ERROR_OK);
    }

} // namesapce HostToTarget


