
///
/// \file win/hypervisorupdatermajor.cpp
///
/// \brief
///

#include <windows.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "scopeguard.h"
#include "extractcabfiles.h"
#include "hypervisorupdatermajor.h"

namespace HostToTarget {

    SupportedOsTypeAndOptionalDriverFileInfo g_hypervisorSupportedOsTypeAndOptionalDriverFileInfo[] =
    {
        { OS_TYPE_XP_32, 0, 0 },
        { OS_TYPE_XP_64, 0, 0 },
        { OS_TYPE_W2K3_32, 0, 0 },
        { OS_TYPE_W2K3_64, 0, 0 },
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

    ERROR_RESULT HypervisorUpdaterMajor::validate()
    {
        int i = 0;
        while (OS_TYPE_NOT_SUPPORTED != g_hypervisorSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
            if (m_info.m_osType == g_hypervisorSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
                break;
            }
            ++i;
        }
        if (OS_TYPE_NOT_SUPPORTED == g_hypervisorSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
            return ERROR_RESULT(ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED);
        }
        return ERROR_RESULT(ERROR_OK);
    }

    ERROR_RESULT HypervisorUpdaterMajor::update(int opts, ORHKEY hiveKey)
    {
        int i = 0;
        while (OS_TYPE_NOT_SUPPORTED != g_hypervisorSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
            if (m_info.m_osType == g_hypervisorSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
                break;
            }
            ++i;
        }
        if (OS_TYPE_NOT_SUPPORTED == g_hypervisorSupportedOsTypeAndOptionalDriverFileInfo[i].m_osType) {
            return ERROR_RESULT(ERROR_INTERNAL_OS_NAME_NOT_SUPPORTED);
        }

        if (!m_registryUpdater.update(m_info.m_systemVolume, m_info.m_windowsDir, HOST_TO_HYPERVISOR, m_info.m_osType, opts)) {
            ERROR_RESULT(m_registryUpdater.getInternalError(), m_registryUpdater.getSystemError(), m_registryUpdater.getInternalErrorMsg());
        }
        return ERROR_RESULT(ERROR_OK);
    }

} // namesapce HostToTarget
