
///
/// \file win/hosttotargetmajor.cpp
///
/// \brief host to target major implementations of common functions
///

#include <windows.h>
#include <sstream>

#include <boost/algorithm/string.hpp>

#include "hosttotargetmajor.h"

namespace HostToTarget {

    typedef std::pair<ERR_TYPE, DWORD> hostToTargetError_t;

    std::string errorToString(ERR_TYPE err)
    {
        std::string msg;
        char* buffer = 0;
        if (0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL,
                               err,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPTSTR) &buffer,
                               0,
                               NULL)) {
            try {
                std::stringstream str;
                str << "failed to get message for error code: " << err;
                return str.str();
            } catch (...) {
                // no throw
            }
        } else {
            try {
                msg = buffer;
            } catch (...) {
                // no throw
            }
        }
        if (0 != buffer) {
            LocalFree(buffer);
        }
        return msg;
    }

    long setPrivilege(char const* privilege)
    {
        TOKEN_PRIVILEGES tp;
        LUID luid;
        HANDLE token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
            return GetLastError();
        }
        // ON_BLOCK_EXIT(&CloseHandle, token);
        if (!LookupPrivilegeValue(NULL, privilege, &luid)) {
            return GetLastError();
        }
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (!AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, 0)) {
            return GetLastError();
        }
        long err = GetLastError();
        if (ERROR_SUCCESS != err) {
            return err;
        }
        return ERROR_OK;
    }

    std::string toVolumeNameFormat(std::string const& systemVolume)
    {
        // make sure system volume is formatted properly
        std::string formattedSystemVolume(systemVolume);
        switch (formattedSystemVolume.size()) {
            case 1:
                if (('a' <= formattedSystemVolume[0] && formattedSystemVolume[0] <= 'z')
                    || ('A' <= formattedSystemVolume[0] && formattedSystemVolume[0] <= 'Z')) {
                    formattedSystemVolume += ":\\";
                }
                break;
            case 2:
                if (':' == formattedSystemVolume[1]) {
                    formattedSystemVolume += "\\";
                }
                break;
            case 3:
                if (!(':' == formattedSystemVolume[1] && ('\\' == formattedSystemVolume[2] || '/' == formattedSystemVolume[2]))) {
                    // FIXME: may not be valid system volume on windows
                }
            default:
                // FIXME: may not be valid system volume on windows
                break;
        }
        if (!('\\' == formattedSystemVolume[formattedSystemVolume.size() -1] || '/' == formattedSystemVolume[formattedSystemVolume.size() - 1])) {
            formattedSystemVolume += "\\";
        }

        return formattedSystemVolume;
    }

    std::string getWindowsDir(std::string const& systemVolume, std::string const& windowsDir)
    {
        std::string fullWindowsDir(systemVolume);
        if (windowsDir.empty()) {
            fullWindowsDir += "Windows\\";
        } else {
            std::string::size_type idx = windowsDir.find_first_of(":");
            if (std::string::npos == idx) {
                if ('\\' == windowsDir[0] || '/' == windowsDir[0]) {
                    fullWindowsDir += windowsDir.substr(1);
                } else {
                    fullWindowsDir += windowsDir;
                }
            } else {
                fullWindowsDir += windowsDir.substr(idx+2);
            }
            
            if (!('\\' == fullWindowsDir[fullWindowsDir.size() -1] || '/' == fullWindowsDir[fullWindowsDir.size() - 1])) {
                fullWindowsDir += "\\";
            }
        }
        return fullWindowsDir;
    }

    /// \brief mapping between os ver that includes the arch in the name to its OsTypes
    struct OsNameOsTypeMapping {
        char* m_osNamer;
        OsTypes m_osType;
    };

    /// \brief holds supported mappings between os ver that includes the arch in the name to its OsTypes
    static OsNameOsTypeMapping g_osNameOsTypeMappings[] =
    {
        // FIXME: need desktop oses too
        { "WINDOWS_2003_32", OS_TYPE_W2K3_32 },
        { "WINDOWS_2003_64", OS_TYPE_W2K3_64 },
        { "WINDOWS_2008_32", OS_TYPE_W2K8_32 },
        { "WINDOWS_2008_64", OS_TYPE_W2K8_64 },
        { "WINDOWS_2008_R2", OS_TYPE_W2K8_R2 },
        { "WINDOWS_8", OS_TYPE_WIN8 },   // need to verify if this will be the string for windows 8 in vCon

        // must always be last
        { 0, OS_TYPE_NOT_SUPPORTED }
    };

    OsTypes osNameToOsType(std::string const& osName)
    {
        // these do not need the arch paramater
        for (int i = 0; OS_TYPE_NOT_SUPPORTED != g_osNameOsTypeMappings[i].m_osType; ++i) {
            if (osName == g_osNameOsTypeMappings[i].m_osNamer) {
                return g_osNameOsTypeMappings[i].m_osType;
            }
        }
        return OS_TYPE_NOT_SUPPORTED;
    }

    // Windows 8                            6.2 HKLM\ConrtolSet000n\Control\ProductVersion\ProductType == winNT
    // Windows Server 2012                  6.2 HKLM\ConrtolSet000n\Control\ProductVersion\ProductType != winNT
    // Windows 7                            6.1 HKLM\ConrtolSet000n\Control\ProductVersion\ProductType == winNT
    // Windows Server 2008 R2               6.1 HKLM\ConrtolSet000n\Control\ProductVersion\ProductType != winNT
    // Windows Server 2008                  6.0 HKLM\ConrtolSet000n\Control\ProductVersion\ProductType != winNT
    // Windows Vista                        6.0 HKLM\ConrtolSet000n\Control\ProductVersion\ProductType == winNT
    // Windows Server 2003 R2               5.2 GetSystemMetrics(SM_SERVERR2) != 0
    // Windows Home Server                  5.2 OSVERSIONINFOEX.wSuiteMask & VER_SUITE_WH_SERVER
    // Windows Server 2003                  5.2 GetSystemMetrics(SM_SERVERR2) == 0
    // Windows XP Professional x64 Edition  5.2 HKLM\ConrtolSet000n\Control\ProductVersion\ProductType == winNT && x64
    // Windows XP                           5.1

    /// \brief mapping between os ver maj.min and arch to its OsTypes
    struct OsMajMinArchOsTypeMapping {
        char* m_osMaj;
        char* m_osMin;
        char* m_arch;
        OsTypes m_osType;
    };

    /// \brief holds supported mappings between server os ver maj.min and arch to its OsTypes
    static OsMajMinArchOsTypeMapping g_osMajMinArchOsTypeServerMappings[] =
    {
        { "6", "2", "x64", OS_TYPE_W2K12 },
        { "6", "1", "x64", OS_TYPE_W2K8_R2 },
        { "6", "0", "x86", OS_TYPE_W2K8_32 },
        { "6", "0", "x64", OS_TYPE_W2K8_64 },
        { "5", "2", "x86", OS_TYPE_W2K3_32 },
        { "5", "2", "x64", OS_TYPE_W2K3_64 },

        // must always be last
        { 0, 0, 0, OS_TYPE_NOT_SUPPORTED }
    };

    /// \brief holds supported mappings between workstation os ver maj.min and arch to its OsTypes
    static OsMajMinArchOsTypeMapping g_osMajMinArchOsTypeWorkstationMappings[] =
    {
        { "6", "2", "x64", OS_TYPE_WIN8 },
        { "6", "1", "x64", OS_TYPE_WIN7 },
        { "6", "0", "x86", OS_TYPE_VISTA_32 },
        { "6", "0", "x64", OS_TYPE_VISTA_64 },
        { "5", "1", "x86", OS_TYPE_XP_32 },
        { "5", "1", "x64", OS_TYPE_XP_64 },

        // must always be last
        { 0, 0, 0, OS_TYPE_NOT_SUPPORTED }
    };

    bool osTypeToMajMinArch(OsTypes osType, int& maj, int& min, int& arch)
    {
        switch (osType) {
            case OS_TYPE_W2K12:
            case OS_TYPE_WIN8:
                maj = 6;
                min = 2;
                arch = 64;
                break;
            case OS_TYPE_W2K8_R2:
            case OS_TYPE_WIN7:
                maj = 6;
                min = 1;
                arch = 64;
                break;
            case OS_TYPE_W2K8_32:
            case OS_TYPE_VISTA_32:
                maj = 6;
                min = 0;
                arch = 86;
                break;
            case OS_TYPE_W2K8_64:
            case OS_TYPE_VISTA_64:
                maj = 6;
                min = 0;
                arch = 64;
                break;
            case OS_TYPE_W2K3_32:
                maj = 5;
                min = 2;
                arch = 86;
                break;
            case OS_TYPE_W2K3_64:
                maj = 5;
                min = 2;
                arch = 64;
                break;
            case OS_TYPE_XP_32:
                maj = 5;
                min = 1;
                arch = 86;
                break;
            case OS_TYPE_XP_64:
                maj = 5;
                min = 1;
                arch = 64;
                break;
            default:
                return false;
        }
        return true;
    }
    
    OsTypes osMajMinArchToOsType(std::string const& osMajorVersion, std::string const& osMinorVersion, std::string const& arch, bool serverVersion)
    {
        OsMajMinArchOsTypeMapping* osMajMinArchOsTypeMappings = (serverVersion ? g_osMajMinArchOsTypeServerMappings : g_osMajMinArchOsTypeWorkstationMappings);
        for (int i = 0; OS_TYPE_NOT_SUPPORTED != osMajMinArchOsTypeMappings[i].m_osType; ++i) {
            if (osMajorVersion == osMajMinArchOsTypeMappings[i].m_osMaj
                && osMinorVersion == osMajMinArchOsTypeMappings[i].m_osMin
                && arch == osMajMinArchOsTypeMappings[i].m_arch) {
                return osMajMinArchOsTypeMappings[i].m_osType;
            }
        }
        return OS_TYPE_NOT_SUPPORTED;
    }

}
