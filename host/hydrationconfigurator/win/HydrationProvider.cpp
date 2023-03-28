#include "stdafx.h"
#include "..\HydrationConfigurator.h"
#include "..\common\win32\DiskHelpers.h"
#include "HydrationProvider.h"
#include "wmiutils.h"

#include <winioctl.h>
#include <Windows.h>

#include <exception>
#include <Objbase.h>
#include <atlbase.h>
#include "Shlwapi.h"
#include <Wbemidl.h>
#include <boost/algorithm/string.hpp>
#include <strsafe.h>
#include <map>
#include <vector>
#include <sstream>

#include <boost\foreach.hpp>

namespace OsHydrationTagAttributes {
    const char SANPOLICY[] = "san";
    const char BOOT_DISK_TYPE[] = "boot";
    const char BOOT_INVALID[] = "bootIn";
    const char BOOTDEV_VENDOR[] = "BV";
    const char BOOTDEV_VENDOR_ERROR[] = "BVE";
    const char BOOTDEV_BUS[] = "BBUS";
    const char BOOTDEV_BUS_ERROR[] = "BBUSE";
};

extern BOOL GetBusType(SV_ULONG ulDiskIndex, STORAGE_BUS_TYPE& busType, std::string& errorMessage);
extern BOOL GetBusType(std::string deviceName, STORAGE_BUS_TYPE& busType, std::string& errorMessage);
extern BOOL GetBusType(HANDLE hDisk, STORAGE_BUS_TYPE& busType, std::string& errorMessage);

OSVERSIONINFOEX     SystemInfo::s_VersionInfo = { 0 };
boost::mutex        SystemInfo::s_mutex;

std::map<std::string, DWORD>  g_SanPolicyMap = {
    MAKE_PAIR(ONLINE_ALL),
    MAKE_PAIR(OFFLINE_ALL),
    MAKE_PAIR(OFFLINE_SHARED),
    MAKE_PAIR(OFFLINE_INTERNAL)
};

std::map<std::string, RequirementValidator> g_PropertyHandlerMap = {
    { HydrationAttributes::BOOT_DRIVERS, OsPropertyValidator::ValidateBootDrivers },
    { HydrationAttributes::SYSTEM_DRIVERS, OsPropertyValidator::ValidateSystemDrivers },
    { HydrationAttributes::SANPOLICY, OsPropertyValidator::ValidateSanPolicy },
    { HydrationAttributes::W2K8_SANPOLICY, OsPropertyValidator::ValidateW2k8SanPolicy },
    { HydrationAttributes::W2K19_SANPOLICY, OsPropertyValidator::ValidateW2k19SanPolicy },
    { HydrationAttributes::AUTO_SERVICES, OsPropertyValidator::ValidateAutomaticServices },
    { HydrationAttributes::BOOTDISKLAYOUT, OsPropertyValidator::ValidateBootDiskType },
    { HydrationAttributes::COND_ONDEMAND_DRIVERS, OsPropertyValidator::ValidateConditionalOnDemandDrivers },
    { HydrationAttributes::CONDITION, OsPropertyValidator::ValidateConditions },
    { HydrationAttributes::MSFT_HW_KEY_EXISTS, OsPropertyValidator::ValidateMsftHwKeys },
    { HydrationAttributes::INDSK_BOOT_INFO, OsPropertyValidator::ValidateIndskBootInfo },
    { HydrationAttributes::CONTAINS_DYNAMIC_DISKS, OsPropertyValidator::ValidateDynamicDisks },
    {HydrationAttributes::UNSUPPORTED_OS, OsPropertyValidator::ValidateUnsupportedOS}
};

std::map<std::string, std::string>  g_hydrationTagsMap = {
    { "atapi", "at" },
    { "intelide", "ide" },
    { "storvsc", "ssvc" },
    { "storflt", "sflt" }
};

SV_ULONG                g_ulNumDynDisks = 0;
SV_ULONG                g_ulNumDisks = 0;

DWORD SystemInfo::GetSubKeysList(const std::string& key, std::list<std::string>& subkeys)
{
    DWORD   dwResult = ERROR_SUCCESS;
    CRegKey cregkey;
    std::vector<char>    subKeyBuffer(MAX_PATH + 1, 0);

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "KeyName: %s\n", key.c_str());

    dwResult = cregkey.Open(HKEY_LOCAL_MACHINE, key.c_str(), KEY_READ);
    if (dwResult != ERROR_SUCCESS)
    {
        DebugPrintf(SV_LOG_ERROR, "Error: CRegKey::Open failed for key: %s with error %ld\n", key.c_str(), dwResult);
        return dwResult;
    }


    DWORD subkeyLength = MAX_PATH;

    int index = 0;

    while (ERROR_SUCCESS == (dwResult = RegEnumKey(cregkey, index, &subKeyBuffer[0], subkeyLength)))
    {
        ++index;
        subkeyLength = MAX_PATH;

        DebugPrintf(SV_LOG_DEBUG, "Subkey: %s\n", &subKeyBuffer[0]);

        if (boost::starts_with(subKeyBuffer, "ControlSet")) {
            DebugPrintf(SV_LOG_DEBUG, "Pushing Subkey: %s\n", &subKeyBuffer[0]);
            subkeys.push_back(&subKeyBuffer[0]);
        }

        memset(&subKeyBuffer[0], '\0', MAX_PATH);
    }

    if (ERROR_NO_MORE_ITEMS == dwResult) {
        dwResult = ERROR_SUCCESS;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "EnumKey is failed with error %d\n", dwResult);
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return dwResult;
}

bool SystemInfo::PopulateOSVersion()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    OSVERSIONINFOEX versionEx = { 0 };
    versionEx.dwOSVersionInfoSize = sizeof(versionEx);

    // Being a static variable we will retry to get version info if it failed first time
    if (0 == s_VersionInfo.dwOSVersionInfoSize) {
#pragma warning(disable : 4996)
        // Ideally GetVersionEx function should not be used as it is deprecated
        // But currently only difference between 2016 and 2019 is only build number.
        // There is no function to validate os version based on build number.
        if (!GetVersionEx((OSVERSIONINFO*)&versionEx)) {
            DebugPrintf(SV_LOG_ERROR, "GetVersionEx failed with error=%d\n", GetLastError());
            return false;
        }

#pragma warning(default : 4996)
        {
            boost::mutex::scoped_lock   lock(s_mutex);
            if (0 == s_VersionInfo.dwOSVersionInfoSize) {
                memcpy_s(&s_VersionInfo, sizeof(s_VersionInfo), &versionEx, sizeof(versionEx));
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "OSVersion Major: %d Minor: %d Build: %d\n",
            s_VersionInfo.dwMajorVersion,
            s_VersionInfo.dwMinorVersion,
            s_VersionInfo.dwBuildNumber);
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return true;
}

bool SystemInfo::GetOSVersion(OSVERSIONINFOEX&   versionEx)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    // Being a static variable we will retry to get version info if it failed first time
    if (0 == s_VersionInfo.dwOSVersionInfoSize) {
        if (!PopulateOSVersion()) {
            return false;
        }
    }

    memcpy_s(&versionEx, sizeof(versionEx), &s_VersionInfo, sizeof(s_VersionInfo));

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return true;
}

bool SystemInfo::GetOSVersionString(std::string& OsName)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    OsName = OS_NAME_UNKNOWN;

    if (0 == s_VersionInfo.dwOSVersionInfoSize) {
        if (!PopulateOSVersion()) {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to get OS Version\n", FUNCTION_NAME);
            return false;
        }
    }

    if ((OS_MAJOR_VERSION_W2k19 == s_VersionInfo.dwMajorVersion) &&
        (OS_MINOR_VERSION_W2K19 == s_VersionInfo.dwMinorVersion) &&
        (OS_MIN_BUILD_NUM_W2k19 <= s_VersionInfo.dwBuildNumber)) {
        OsName = OS_NAME_WIN2K19;
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return true;
}

int SystemInfo::QuerySanPolicy()
{
    LONG        lStatus;
    CRegKey     svcKey;
    DWORD       dwSanPolicy = UNKNOWN;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    lStatus = svcKey.Open(HKEY_LOCAL_MACHINE, SourceRegistry::PARTMGR_PARAMS_KEY, KEY_READ);
    if (ERROR_SUCCESS != lStatus) {
        DebugPrintf(SV_LOG_ERROR, "Error: failed to open registry %s error=%d\n", SourceRegistry::PARTMGR_PARAMS_KEY, lStatus);
        return UNKNOWN;
    }

    lStatus = svcKey.QueryDWORDValue(HydrationAttributes::SANPOLICY, dwSanPolicy);
    if (ERROR_SUCCESS != lStatus) {
        DebugPrintf(SV_LOG_ERROR, "Error: failed to query registry %s error=%d\n", HydrationAttributes::SANPOLICY, lStatus);
        return UNKNOWN;
    }

    DebugPrintf(SV_LOG_DEBUG, "SanPolicy: %d\n", dwSanPolicy);
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return dwSanPolicy;
}

DWORD SystemInfo::QueryServiceStartType(std::string rootKey, std::string szDriverName, BOOL isDriver)
{
    LONG    lStatus;
    DWORD   lpType = REG_DWORD;
    DWORD   cbData = sizeof(DWORD);
    DWORD   dwStartType;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    std::vector<CHAR>   szImagePath(MAX_PATH);
    std::vector<CHAR>   szSystemRoot(MAX_PATH);

    ULONG               lPathSize;
    PVOID               OldValue = NULL;
    CRegKey             svcKey;

    std::stringstream   ssRegName;
    std::stringstream   ssCompleteImagePath;

    std::string         strWin32Dir;
    std::string         strSysNativeDir;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    ssRegName << rootKey << "\\" << SourceRegistry::SERVICES_KEY_VALUE << SourceRegistry::REG_PATH_SEPARATOR << szDriverName;
    DebugPrintf(SV_LOG_INFO, "Validating ControlSet %s\n", ssRegName.str().c_str());

    lStatus = svcKey.Open(HKEY_LOCAL_MACHINE, ssRegName.str().c_str(), KEY_READ);
    if (ERROR_SUCCESS != lStatus) {
        DebugPrintf(SV_LOG_ERROR, "Error: failed to open registry %s error=%d\n", ssRegName.str().c_str(), lStatus);
        return ERROR_REGISTRY_OPEN_FAILED;
    }

    lStatus = svcKey.QueryDWORDValue(SourceRegistry::START_TYPE, dwStartType);
    if (ERROR_SUCCESS != lStatus) {
        DebugPrintf(SV_LOG_ERROR, "Error: failed to query Start Type registry %s error=%d\n", ssRegName.str().c_str(), lStatus);
        return ERROR_REGISTRY_START_TYPE_QUERY_FAILED;
    }

    DebugPrintf(SV_LOG_INFO, "%s StartType: %d\n", ssRegName.str().c_str(), dwStartType);

    if (!isDriver) {
        return dwStartType;
    }

    lPathSize = MAX_PATH;
    lStatus = svcKey.QueryStringValue(SourceRegistry::IMAGE_PATH_KEY, &szImagePath[0], &lPathSize);
    if (ERROR_SUCCESS != lStatus) {
        DebugPrintf(SV_LOG_ERROR, "Error: failed to query Image Path registry %s error=%d\n", ssRegName.str().c_str(), lStatus);
        return ERROR_REGISTRY_IMAGE_PATH_QUERY_FAILED;
    }

    if (0 == GetSystemWindowsDirectory(&szSystemRoot[0], MAX_PATH)) {
        DebugPrintf(SV_LOG_ERROR, "Error: GetSystemWindowsDirectory  failed with error=%d\n", GetLastError());
        return ERROR_API_GetSystemWindowsDirectory_FAILED;
    }
    // To avoid wow redirection 
    // 32-bit applications can access the native system directory by 
    // substituting %windir%\Sysnative for %windir%\System32. 
    // WOW64 recognizes Sysnative as a special alias used to indicate that 
    // the file system should not redirect the access.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa384187(v=vs.85).aspx
    strWin32Dir.append(&szSystemRoot[0]).append("\\").append("system32");
    strSysNativeDir.append(&szSystemRoot[0]).append("\\").append("Sysnative");

    boost::algorithm::to_lower(strWin32Dir);
    boost::algorithm::to_lower(strSysNativeDir);
    std::vector<char>   envVarValue(MAX_PATH + 1);

    if ('\\' == szImagePath[0]) {
        std::string imagePath(&szImagePath[0]);
        size_t found = imagePath.find_first_of("\\", 1);
        if (std::string::npos == found) {
            DebugPrintf(SV_LOG_ERROR, "Error: Image Path registry is of invalid format: %s\n", &szImagePath[0]);
            return ERROR_INVALID_ENVVAR_PATH_FORMAT;
        }

        memset(&envVarValue[0], '\0', MAX_PATH + 1);
        std::string envVarName = imagePath.substr(1, found - 1);
        DebugPrintf(SV_LOG_DEBUG, "Environment variable: %s\n", &envVarName[0]);
        DWORD dwStatus = GetEnvironmentVariable(envVarName.c_str(), &envVarValue[0], MAX_PATH);
        if (dwStatus > MAX_PATH) {
            envVarName.resize(dwStatus + 1);
            memset(&envVarValue[0], '\0', dwStatus + 1);
            dwStatus = GetEnvironmentVariable(envVarName.c_str(), &envVarValue[0], MAX_PATH);
        }
        if (0 == dwStatus) {
            DebugPrintf(SV_LOG_ERROR, "Error: Image Path %s GetEnvironmentVariable for %s failed err=%d\n", &szImagePath[0], &envVarName[0], GetLastError());
            return ERROR_API_GetEnvironmentVariable_FAILED;
        }
        ssCompleteImagePath << &envVarValue[0] << SourceRegistry::DIR_PATH_SEPARATOR << imagePath.substr(found + 1);
    }
    else {
        ssCompleteImagePath << &szSystemRoot[0] << SourceRegistry::DIR_PATH_SEPARATOR << &szImagePath[0];
    }

    std::string     sysFilePath = ssCompleteImagePath.str();
    boost::algorithm::to_lower(sysFilePath);

    // For x64 use sysnative path to avoid redirection
    // https://docs.microsoft.com/en-us/windows/desktop/winprog64/file-system-redirector
    // 32-bit process on windows is redirected to %windir%\SysWOW64 when it tries to access %windir%\System32
    // Due to this while validating %windir%\System32\<driver>.sys file it is redirected to
    //                              %windir%\SysWOW64\<driver>.sys and validation fails.
    //
    // 32-bit applications can access the native system directory by substituting %windir%\Sysnative for %windir%\System32. 
    BOOL isWow64 = false;
    bool isWowProcess = (IsWow64Process(GetCurrentProcess(), &isWow64) && isWow64);

    if (isWowProcess) {
        size_t  pos = sysFilePath.find(strWin32Dir);
        if (std::string::npos != pos) {
            sysFilePath.replace(pos, strWin32Dir.size(), strSysNativeDir);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "Driver %s Path: %s\n", szDriverName.c_str(), sysFilePath.c_str());

    if (!PathFileExists(sysFilePath.c_str())) {
        DebugPrintf(SV_LOG_ERROR, "Error: Path %s doesn't exist\n", sysFilePath.c_str());
        return ERROR_DRIVER_PATH_NOT_FOUND;
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);

    return dwStartType;
}

bool OsPropertyValidator::ValidateDriversOrServices(boost::property_tree::ptree const& input, DWORD dwExpectedStartType, std::map<std::string, std::string>& onDiskSystemState, bool isDriver)
{
    bool isStartTypeAsExp = true;
    bool isStartTypeAsExpRet = true;

    DWORD dwStartType = START_TYPE_UNKNOWN;
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    BOOST_FOREACH(boost::property_tree::ptree::value_type it, input) {
        std::string driver = it.second.get_value<std::string>();


        std::stringstream controlKey;
        controlKey << "SYSTEM\\CurrentControlSet";

        DebugPrintf(SV_LOG_INFO, "Checking in ControlSet %s  for driver %s\n", controlKey.str().c_str(), driver.c_str());

        dwStartType = SystemInfo::QueryServiceStartType(controlKey.str(), driver, isDriver);

        if ((onDiskSystemState.find(driver) == onDiskSystemState.end()) ||
            (dwStartType > atoi(onDiskSystemState[driver].c_str()))) {
            onDiskSystemState[driver] = std::to_string(dwStartType);
        }

        isStartTypeAsExp = (dwExpectedStartType == dwStartType);

        if (!isStartTypeAsExp && isDriver) {
            isStartTypeAsExp = (dwStartType <= dwExpectedStartType);
        }

        if (!isStartTypeAsExp) {
            isStartTypeAsExpRet = false;
            DebugPrintf(SV_LOG_INFO, "Start type mismatch service: %s onDiskSystemState: %d excpected: %d\n", driver.c_str(), dwStartType, dwExpectedStartType);
            break;
        }
    }

    return isStartTypeAsExpRet;
}

bool OsPropertyValidator::ValidateBootDrivers(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    return ValidateDriversOrServices(input, SERVICE_BOOT_START, onDiskSystemState, true);
}

bool OsPropertyValidator::ValidateSystemDrivers(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    return ValidateDriversOrServices(input, SERVICE_SYSTEM_START, onDiskSystemState, true);
}

bool OsPropertyValidator::ValidateDemandStartDrivers(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    return ValidateDriversOrServices(input, SERVICE_DEMAND_START, onDiskSystemState, true);
}

bool OsPropertyValidator::ValidateAutomaticServices(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    return ValidateDriversOrServices(input, SERVICE_AUTO_START, onDiskSystemState, false);
}

bool OsPropertyValidator::ValidateW2k8SanPolicy(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    if (IsWindows7OrGreater()) {
        return true;
    }

    return ValidateSanPolicy(input, onDiskSystemState);
}

bool OsPropertyValidator::ValidateW2k19SanPolicy(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    std::string     osName;

    if (!SystemInfo::GetOSVersionString(osName)) {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to get OS String\n", FUNCTION_NAME);
        return false;
    }

    if (!boost::iequals(OS_NAME_WIN2K19, osName)) {
        DebugPrintf(SV_LOG_DEBUG, "OSName is %s.. skipping san check\n", osName.c_str());
        return true;
    }
    return ValidateSanPolicy(input, onDiskSystemState);
}

bool OsPropertyValidator::ValidateSanPolicy(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    bool isSanPolicyAsExpected = false;

    // San Policy is immaterial when number of disks is 1.
    if (1 == g_ulNumDisks) {
        DebugPrintf(SV_LOG_INFO, "San Policy is not applicable as Number of disks: %d\n", g_ulNumDisks);
        return true;
    }

    DWORD dwSanPolicy = SystemInfo::QuerySanPolicy();

    onDiskSystemState[OsHydrationTagAttributes::SANPOLICY] = std::to_string(dwSanPolicy);

    BOOST_FOREACH(boost::property_tree::ptree::value_type it, input) {
        std::string value = it.second.get_value<std::string>();

        if (g_SanPolicyMap.find(value) == g_SanPolicyMap.end()) {
            DebugPrintf(SV_LOG_ERROR, "Invalid San Policy: %s\n", value.c_str());
            break;
        }

        if (dwSanPolicy == g_SanPolicyMap[value]) {
            isSanPolicyAsExpected = true;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "Validation for sanpolicy %s\n", (isSanPolicyAsExpected ? "PASSED" : "FAILED"));
    return isSanPolicyAsExpected;
}

bool OsPropertyValidator::ValidateBootDiskType(boost::property_tree::ptree const& input, std::map<std::string, std::string>& bootDiskType)
{
    std::stringstream   ssError;
    DWORD               Status;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    bool    isBootDiskUefi = IsUEFIBoot();


    bootDiskType[OsHydrationTagAttributes::BOOT_DISK_TYPE] = isBootDiskUefi ? DISK_LAYOUT_TYPE_GPT : DISK_LAYOUT_TYPE_MBR;

    std::string layout = input.get_value<std::string>();
    if (boost::iequals(layout, DISK_LAYOUT_TYPE_MBR)) {
        return !isBootDiskUefi;
        }

    if (boost::iequals(layout, DISK_LAYOUT_TYPE_GPT)) {
        return isBootDiskUefi;
    }

    bootDiskType[OsHydrationTagAttributes::BOOT_INVALID] = "Invalid";
    return false;
}

bool OsPropertyValidator::ValidateConditionalOnDemandDrivers(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskState)
{
    bool    isValidated = true;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    BOOST_FOREACH(boost::property_tree::ptree::value_type rowPair, input) {
        if (ValidateBootDrivers(rowPair.second.get_child("drivers"), onDiskState)) {
            break;

        }

        if (!ValidateDemandStartDrivers(rowPair.second.get_child("drivers"), onDiskState)) {
            isValidated = false;
            break;
        }

        if (!ValidateConditions(rowPair.second.get_child("condition"), onDiskState)) {
            isValidated = false;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return isValidated;
}

bool OsPropertyValidator::ValidateConditions(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskState)
{
    bool isValidated = true;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    BOOST_FOREACH(boost::property_tree::ptree::value_type it, input) {
        if (g_PropertyHandlerMap.find(it.first) == g_PropertyHandlerMap.end()) {
            isValidated = false;
            onDiskState[it.first] = "no";
            continue;
        }
        if (!g_PropertyHandlerMap[it.first](it.second, onDiskState)) {
            isValidated = false;
        }
        DebugPrintf(SV_LOG_DEBUG, "Property %s %s\n", it.first.c_str(), isValidated? "Matched" : "Unmatched");
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return isValidated;
}

bool OsPropertyValidator::ValidateMsftHwKeys(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& onDiskState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    CRegKey cregkey;
    std::vector<char>    subKeyBuffer(MAX_PATH + 1, 0);
    std::string     key(SourceRegistry::MSFT_SCSI_KEYS);

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    std::string strExists = pt.get_value<std::string>();

    bool exists = boost::iequals(strExists, "true");
    bool isValidated = !exists;

    DWORD dwResult = cregkey.Open(HKEY_LOCAL_MACHINE, key.c_str(), KEY_READ);
    if (dwResult != ERROR_SUCCESS)
    {
        DebugPrintf(SV_LOG_ERROR, "Error: CRegKey::Open failed for key: %s with error %ld\n", key.c_str(), dwResult);
        return false;
    }

    DWORD subkeyLength = MAX_PATH;

    int index = 0;

    while (ERROR_SUCCESS == (dwResult = RegEnumKey(cregkey, index, &subKeyBuffer[0], subkeyLength)))
    {
        DebugPrintf(SV_LOG_DEBUG, "SubKey: %s\n", &subKeyBuffer[0]);
        if (boost::icontains(subKeyBuffer, "Disk&Ven_Msft&Prod_Virtual_Disk")) {
            isValidated = exists;
        }
        index++;
    }

    DebugPrintf(SV_LOG_DEBUG, "Validation for attrib msft_key_exists %s\n", (isValidated ? "PASSED" : "FAILED"));
    return isValidated;
}

bool OsPropertyValidator::ValidateDynamicDisks(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    std::string strExists = input.get_value<std::string>();

    bool exists = boost::iequals(strExists, "true");

    return (exists || (0 == g_ulNumDynDisks));
}

bool OsPropertyValidator::ValidateUnsupportedOS(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    std::string strOSType = input.get_value<std::string>();

    std::string currentOSName;
    if (!SystemInfo::GetOSVersionString(currentOSName)) {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to get OS Name\n", FUNCTION_NAME);

        // As OS details are not clear.. it will be treated as unsupported OS
        return true;
    }

    DebugPrintf(SV_LOG_DEBUG, "OS NAME: %s Expected: %s\n", currentOSName.c_str(), strOSType.c_str());

    // As it is unsupported OS... opposite has to be returned
    return !boost::iequals(strOSType, currentOSName);
}

bool OsPropertyValidator::ValidateIndskBootInfo(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    static ULONG    ulSystemDisk = ULONG_MAX;
    static bool     bIndskBootInfoQueried = false;
    static bool     bPopulateBootInfo = true;

    CRegKey         indskfltRegKey;
    DWORD           dwResult;
    DWORD           dwDisableNoHydrationWF = 0;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    std::stringstream   ssDisableNoHydrationWFKey;

    ssDisableNoHydrationWFKey << InDskFltAttributes::DRIVER_KEY << "\\" << InDskFltAttributes::DisableNoHydrationWF;

    dwResult = indskfltRegKey.Open(HKEY_LOCAL_MACHINE, InDskFltAttributes::PARAMETERS_KEY, KEY_READ);
    if (ERROR_SUCCESS != dwResult) {
        DebugPrintf(SV_LOG_ERROR, "Failed to open key %s error=%d\n", InDskFltAttributes::PARAMETERS_KEY, dwResult);
        return false;
    }

    dwResult = indskfltRegKey.QueryDWORDValue(InDskFltAttributes::DisableNoHydrationWF, dwDisableNoHydrationWF);
    if (ERROR_SUCCESS != dwResult) {
        if (ERROR_FILE_NOT_FOUND != dwResult) {
            DebugPrintf(SV_LOG_ERROR, "Failed to read key %s error=%d\n", InDskFltAttributes::DisableNoHydrationWF, dwResult);
            return false;
        }
        dwDisableNoHydrationWF = 0;
    }

    if (0 != dwDisableNoHydrationWF) {
        DebugPrintf(SV_LOG_ERROR, "Driver No hydration WF is disabled dwDisableNoHydrationWF=%d\n", dwDisableNoHydrationWF);
        return false;
    }

    if (ULONG_MAX == ulSystemDisk) {
        std::set<SV_ULONG>  systemDiskIndices;
        std::string         err;
        DWORD               errcode;

        DebugPrintf(SV_LOG_DEBUG, "Querying Boot Disk\n");

        if (!GetSystemDiskList(systemDiskIndices, err, errcode)) {
            DebugPrintf(SV_LOG_ERROR, "Failed to get system disk err=%s errCode=%d\n", err.c_str(), errcode);
            return false;
        }

        if (systemDiskIndices.size() == 0) {
            DebugPrintf(SV_LOG_ERROR, "Failed to get system disk.. No system disk\n");
            return false;
        }

        std::stringstream   ssError;
        if (systemDiskIndices.size() > 1) {
            ssError << "More than one system disks:";
            for (auto diskIndex : systemDiskIndices) {
                ssError << " " << diskIndex;
            }
            DebugPrintf(SV_LOG_ERROR, "%s\n", ssError.str().c_str());
            return false;
        }

        ulSystemDisk = *(systemDiskIndices.begin());
        DebugPrintf(SV_LOG_DEBUG, "Boot Disk Index: %d\n", ulSystemDisk);
    }

    std::list<std::string>     subKeyList;

    dwResult = SystemInfo::GetSubKeysList(InDskFltAttributes::PARAMETERS_KEY, subKeyList);
    if (ERROR_SUCCESS != dwResult) {
        DebugPrintf(SV_LOG_ERROR, "Failed to get subkeys of %s error=%d\n", InDskFltAttributes::PARAMETERS_KEY, dwResult);
        return false;
    }


    dwResult = SystemInfo::GetSubKeysList(InDskFltAttributes::BOOTDISK_REG_KEY, subKeyList);
    if (ERROR_SUCCESS != dwResult) {
        DebugPrintf(SV_LOG_INFO, "Failed to get subkeys of %s error=%d\n", InDskFltAttributes::BOOTDISK_REG_KEY, dwResult);
        DebugPrintf(SV_LOG_ALWAYS, "Populating Information %s\n", InDskFltAttributes::BOOTDISK_REG_KEY);
        if (bPopulateBootInfo && !PopulateIndskfltBootInfo(ulSystemDisk)) {
            DebugPrintf(SV_LOG_ERROR, "Failed to populate Boot Disk Information for indskflt Key: %s\n", InDskFltAttributes::BOOTDISK_REG_KEY);
            bPopulateBootInfo = false;
            return false;
        }
    }

    CRegKey bootKey;

    dwResult = bootKey.Open(HKEY_LOCAL_MACHINE, InDskFltAttributes::BOOTDISK_REG_KEY);
    if (ERROR_SUCCESS != dwResult) {
        DebugPrintf(SV_LOG_ERROR, "Failed to open registry: %s error=%d\n", InDskFltAttributes::BOOTDISK_REG_KEY, dwResult);
        return false;
    }

    DWORD   dwBusType = MAXDWORD;
    bool    bSuccess = true;


    BOOST_FOREACH(boost::property_tree::ptree::value_type it, input) {
        std::vector<char>   strBuffer(MAX_PATH, '\0');

        if (boost::iequals(it.first, InDskFltAttributes::BOOTDEV_BUS)) {
            dwResult = bootKey.QueryDWORDValue(it.first.c_str(), dwBusType);
            if (ERROR_SUCCESS != dwResult) {
                onDiskSystemState[OsHydrationTagAttributes::BOOTDEV_BUS_ERROR] = dwResult;
                bSuccess = false;
                continue;
            }

            DebugPrintf(SV_LOG_DEBUG, "Boot Disk BusType: %d\n", dwBusType);

            onDiskSystemState[OsHydrationTagAttributes::BOOTDEV_BUS] = std::to_string(dwBusType);
            bool busMatches = false;

            BOOST_FOREACH(auto busIt, it.second) {
                if (dwBusType == busIt.second.get_value<DWORD>()) {
                    busMatches = true;
                    break;
                }
            }

            if (!busMatches) {
                bSuccess = false;
            }
            continue;
        }

        ULONG ulSize;
        bool    bAttributeMatches = false;

        dwResult = bootKey.QueryStringValue(it.first.c_str(), &strBuffer[0], &ulSize);
        if (dwResult != ERROR_SUCCESS) {
            bSuccess = false;
            onDiskSystemState[it.first] = std::to_string(dwResult);
            continue;
        }

        onDiskSystemState[it.first] = &strBuffer[0];

        BOOST_FOREACH(auto attribIt, it.second) {
            if (boost::iequals(&strBuffer[0], attribIt.second.get_value<std::string>())) {
                bAttributeMatches = true;
                break;
            }
        }

        if (!bAttributeMatches) {
            DebugPrintf(SV_LOG_DEBUG, "Boot Vendor %s is different from supported vendors\n", &strBuffer[0]);
            bSuccess = false;
        }
        else {
            DebugPrintf(SV_LOG_DEBUG, "Boot Vendor %s is supported vendor\n", &strBuffer[0]);
        }
    }

    return bSuccess;
}

bool OsPropertyValidator::PopulateIndskfltBootInfo(ULONG ulBootDisk)
{
    STORAGE_BUS_TYPE    busType;
    std::string         errorMessage;
    CRegKey             bootDiskKey;
    std::string         vendorId;
    std::string         productId;
    std::string         serialNumber;
    std::string         errormessage;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    DWORD dwStatus = bootDiskKey.Create(HKEY_LOCAL_MACHINE, InDskFltAttributes::BOOTDISK_REG_KEY);

    if (ERROR_SUCCESS != dwStatus) {
        DebugPrintf(SV_LOG_ERROR, "Failed to create registry %s error=%d\n", InDskFltAttributes::BOOTDISK_REG_KEY, dwStatus);
        return false;
    }

    if (!GetBusType(ulBootDisk, busType, errorMessage)) {
        DebugPrintf(SV_LOG_ERROR, "Failed to get system disk bus type. Error: %s\n", errorMessage.c_str());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "Disk %d BusType: %d\n", ulBootDisk, busType);

    if (!GetDeviceAttributes(ulBootDisk, vendorId, productId, serialNumber, errormessage)) {
        DebugPrintf(SV_LOG_ERROR, "Failed to get device attributes for disk %d error = %d\n", ulBootDisk, GetLastError());
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "Disk %d vendor: %s\n", ulBootDisk, vendorId.c_str());

    std::string     deviceId;
    if (!GetDiskGuid(ulBootDisk, deviceId)) {
        DebugPrintf(SV_LOG_ERROR, "Failed to get disk Id for disk %d error: %d\n", ulBootDisk, GetLastError());
        return false;
    }

    if (0 == deviceId.size()) {
        DebugPrintf(SV_LOG_ERROR, "Got empty disk Id for disk %d\n", ulBootDisk);
        return false;
    }

    if ('{' == deviceId.at(0)) {
        deviceId.erase(deviceId.begin());
    }

    if ('}' == deviceId.at(deviceId.size() - 1)) {
        deviceId.erase(deviceId.size() - 1);
    }

    DebugPrintf(SV_LOG_DEBUG, "Disk %d deviceId: %s\n", ulBootDisk, deviceId.c_str());

    dwStatus = bootDiskKey.SetStringValue(InDskFltAttributes::BOOTDEV_VENDOR_ID, vendorId.c_str());
    if (ERROR_SUCCESS != dwStatus) {
        DebugPrintf(SV_LOG_ERROR, "Failed to update key %s error: %d\n", InDskFltAttributes::BOOTDEV_VENDOR_ID, dwStatus);
        return false;
    }

    dwStatus = bootDiskKey.SetStringValue(InDskFltAttributes::BOOTDEV_ID, deviceId.c_str());
    if (ERROR_SUCCESS != dwStatus) {
        DebugPrintf(SV_LOG_ERROR, "Failed to update key %s error: %d\n", InDskFltAttributes::BOOTDEV_ID, dwStatus);
        return false;
    }

    dwStatus = bootDiskKey.SetDWORDValue(InDskFltAttributes::BOOTDEV_BUS, busType);
    if (ERROR_SUCCESS != dwStatus) {
        DebugPrintf(SV_LOG_ERROR, "Failed to update key %s error: %d\n", InDskFltAttributes::BOOTDEV_BUS, dwStatus);
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return true;
}

bool HydrationProvider::IsHydrationRequired(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    bool isHydrationNeeded = false;
    std::stringstream           output;

    Win32DiskDrive  win32DiskDrive;

    HRESULT hr = win32DiskDrive.GetNumberOfDisks(g_ulNumDisks, g_ulNumDynDisks);
    if (FAILED(hr)) {
        DebugPrintf(SV_LOG_ERROR, "GetNumberOfDisks failed hr: 0x%x\n", hr);
        output.str("");
        output << std::hex << hr;
        onDiskSystemState[HydrationAttributeNames::WIN32_DISKDRIVE_WMI_NAME_STR] = output.str();
        isHydrationNeeded = true;
    }
    else {
        DebugPrintf(SV_LOG_INFO, "Number of disks: %d\n", g_ulNumDisks);

        output.str("");
        output << g_ulNumDisks;
        onDiskSystemState[HydrationAttributeNames::NUM_OF_DISKS_STR] = output.str();

        DebugPrintf(SV_LOG_INFO, "Number of dynamic disks: %d\n", g_ulNumDynDisks);
        output.str("");
        output << g_ulNumDynDisks;
        onDiskSystemState[HydrationAttributeNames::NUM_DYNAMIC_DISKS_STR] = output.str();
    }

    BOOST_FOREACH(boost::property_tree::ptree::value_type it, input) {
        DebugPrintf(SV_LOG_DEBUG, "Validating property: %s\n", it.first.c_str());
        if (g_PropertyHandlerMap.find(it.first) == g_PropertyHandlerMap.end()) {
            DebugPrintf(SV_LOG_ERROR, "Could not find handler for property: %s.. Hydration needed\n", it.first.c_str());
            onDiskSystemState[it.first] = "no";
            isHydrationNeeded = true;
            continue;
        }

        if (!g_PropertyHandlerMap[it.first](it.second, onDiskSystemState)) {
            DebugPrintf(SV_LOG_DEBUG, "Validation FAILED for property: %s.. Hydration needed\n", it.first.c_str());
            isHydrationNeeded = true;
        }
        else {
            DebugPrintf(SV_LOG_DEBUG, "Validation PASSED for property: %s\n", it.first.c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "Hydration %srequired....\n", (isHydrationNeeded) ? "" : "not ");

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return isHydrationNeeded;
}


HRESULT Win32DiskDrive::ComInitialize()
{
    HRESULT hr = S_OK;

    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    do
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        if (FAILED(hr))
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED: CoInitializeEx() failed. hr = 0x%08x \n.", hr);
            break;
        }

        hr = CoInitializeSecurity
        (
            NULL,
            -1,
            NULL,
            NULL,
            RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL,
            EOAC_NONE,
            NULL
        );

        if (RPC_E_TOO_LATE == hr) {
            hr = S_OK;
        }

        // Check if the securiy has already been initialized by someone in the same process.
        if (FAILED(hr))
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED: CoInitializeSecurity() failed. hr = 0x%08x \n.", hr);
            CoUninitialize();
            break;
        }
    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG, "Exit: %s hr = 0x%x\n", __FUNCTION__, hr);
    return hr;
}

HRESULT Win32DiskDrive::GetNumberOfDisks(ULONG& ulNumDisks, ULONG& ulNumDynDisks)
{
    HRESULT hr = S_OK;
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);

    ulNumDisks = 0;
    ulNumDynDisks = 0;

    hr = ComInitialize();

    if (FAILED(hr))
    {
        DebugPrintf(SV_LOG_ERROR, "Com Initialization failed. Error Code 0x%0x\n", hr);
        return hr;
    }

    {
        CComPtr<IWbemLocator>           pLoc;
        CComPtr<IWbemServices>          pWbemService;
        CComPtr<IEnumWbemClassObject>   pEnum;

        // get the disks from WMI class Win32_DiskDrive

        do {

            hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLoc);

            if (FAILED(hr))
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to create IWbemLocator object. Error Code 0x%0x\n", hr);
                break;
            }

            hr = pLoc->ConnectServer(BSTR(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pWbemService);

            if (FAILED(hr))
            {
                DebugPrintf(SV_LOG_ERROR, "Could not connect to WMI service. Error Code 0x%0x\n", hr);
                break;
            }

            // Set the IWbemServices proxy so that impersonation
            // of the user (client) occurs.
            hr = CoSetProxyBlanket(
                pWbemService,                         // the proxy to set
                RPC_C_AUTHN_WINNT,            // authentication service
                RPC_C_AUTHZ_NONE,             // authorization service
                NULL,                         // Server principal name
                RPC_C_AUTHN_LEVEL_CALL,       // authentication level
                RPC_C_IMP_LEVEL_IMPERSONATE,  // impersonation level
                NULL,                         // client identity 
                EOAC_NONE                     // proxy capabilities     
            );

            if (FAILED(hr))
            {
                DebugPrintf(SV_LOG_ERROR, "Could not connect to WMI service. Error Code 0x%0x\n", hr);
                break;
            }

            hr = pWbemService->ExecQuery(
                BSTR(L"WQL"),
                BSTR(L"Select * FROM Win32_DiskDrive"),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
                0,                              // Context
                &pEnum
            );

            if (FAILED(hr))
            {
                DebugPrintf(SV_LOG_ERROR, "Query Select * FROM Win32_DiskDrive failed. Error Code 0x%0x\n", hr);
                break;
            }

            while (pEnum)
            {
                ULONG                       uReturned = 0;
                CComPtr<IWbemClassObject>   pDiskObj;
                VARIANT                     varIndex;

                hr = pEnum->Next
                (
                    WBEM_INFINITE,
                    1,
                    &pDiskObj,
                    &uReturned
                );

                if (FAILED(hr)) {
                    DebugPrintf(SV_LOG_ERROR, "pEnum->Next failed. Error Code 0x%0x\n", hr);
                    break;
                }

                if (uReturned == 0)
                    break;

                ++ulNumDisks;

                hr = pDiskObj->Get(L"Index", 0, &varIndex, NULL, NULL);
                if (FAILED(hr)) {
                    DebugPrintf(SV_LOG_ERROR, "pObj->Get Index failed with. Error Code 0x%0x\n", hr);
                    continue;
                }

                bool isDynamic = false;
                DWORD Status = IsDiskDynamic(varIndex.lVal, isDynamic);

                if (ERROR_SUCCESS != Status) {
                    DebugPrintf(SV_LOG_ERROR, "IsDiskDynamic failed with. Error Code 0x%0x\n", GetLastError());
                    continue;
                }

                if (isDynamic) {
                    ++ulNumDynDisks;
                }
            }
        } while (false);
    }

    CoUninitialize();

    if (!FAILED(hr))
        hr = S_OK;

    DebugPrintf(SV_LOG_DEBUG, "Exit: %s hr = 0x%x\n", FUNCTION_NAME, hr);
    return hr;
}
