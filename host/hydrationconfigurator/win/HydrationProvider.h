#include "..\vacp\stdafx.h"
#include <Windows.h>
#include "..\HydrationConfigurator.h"
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>
#include <boost\thread\mutex.hpp>


#define MAKE_PAIR(x)    std::make_pair(#x, x)
#define SAFE_RELEASE(p)     {if (NULL != p) p->Release(); p = NULL;}

#define DOS_NAME_PREFIX     "\\\\?\\"
#define DISK_NAME_PREFIX    "\\\\.\\PhysicalDrive"

#define DISK_LAYOUT_TYPE_MBR    "MBR"
#define DISK_LAYOUT_TYPE_GPT    "GPT"

// Build number
// https://en.wikipedia.org/wiki/Windows_Server_2019
// https://docs.microsoft.com/en-us/windows-server/get-started/windows-server-release-info

#define OS_MAJOR_VERSION_W2k19              10
#define OS_MINOR_VERSION_W2K19              0
#define OS_MIN_BUILD_NUM_W2k19              17763

#define OS_NAME_UNKNOWN                 "UNKNOWN"
#define OS_NAME_WIN2K19                 "WIN2K19"

typedef bool(*SystemPropertyChecker) (std::string propertyName, std::map<std::string, std::string>& onDiskSystemState);

const unsigned long UNKNOWN = 0x0;
const unsigned long ONLINE_ALL = 0x1;
const unsigned long OFFLINE_SHARED = 0x2;
const unsigned long OFFLINE_ALL = 0x3;
const unsigned long OFFLINE_INTERNAL = 0x4;

const DWORD START_TYPE_UNKNOWN = -1;

const int ERROR_DRIVER_PATH_NOT_FOUND = -2;
const int ERROR_REGISTRY_OPEN_FAILED = -3;
const int ERROR_REGISTRY_START_TYPE_QUERY_FAILED = -4;
const int ERROR_REGISTRY_IMAGE_PATH_QUERY_FAILED = -5;
const int ERROR_API_GetSystemWindowsDirectory_FAILED = -6;
const int ERROR_API_GetEnvironmentVariable_FAILED = -7;
const int ERROR_INVALID_ENVVAR_PATH_FORMAT = -8;

extern bool IsUEFIBoot();

class OsPropertyValidator{
private:
    static bool ValidateDriversOrServices(boost::property_tree::ptree const& input, DWORD dwExpectedStartType, std::map<std::string, std::string>& onDiskSystemState, bool isDriver);
    static bool PopulateIndskfltBootInfo(ULONG ulBootDisk);
public:
    static bool ValidateBootDrivers(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& onDiskSystemState);
    static bool ValidateSystemDrivers(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& onDiskSystemState);
    static bool ValidateDemandStartDrivers(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& onDiskSystemState);
    static bool ValidateSanPolicy(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& onDiskSystemState);
    static bool ValidateAutomaticServices(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& onDiskSystemState);
    static bool ValidateBootDiskType(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& bootDiskType);
    static bool ValidateConditionalOnDemandDrivers(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& bootDiskType);
    static bool ValidateConditions(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& bootDiskType);
    static bool ValidateW2k8SanPolicy(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& bootDiskType);
    static bool ValidateW2k19SanPolicy(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState);
    static bool ValidateMsftHwKeys(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& bootDiskType);
    static bool ValidateIndskBootInfo(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& onDiskSystemState);
    static bool ValidateDynamicDisks(boost::property_tree::ptree const& pt, std::map<std::string, std::string>& onDiskSystemState);
    static bool ValidateUnsupportedOS(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState);
};

class Win32DiskDrive{
private:
    HRESULT ComInitialize();

public:
    HRESULT GetNumberOfDisks(ULONG& ulNumDisks, ULONG& ulNumDynDisks);
};

class SystemInfo {
private:
    static OSVERSIONINFOEX      s_VersionInfo;
    static boost::mutex         s_mutex;
    static bool PopulateOSVersion();

public:
    static int QuerySanPolicy();
    static bool GetOSVersion(OSVERSIONINFOEX&   versionEx);
    static bool GetOSVersionString(std::string& OsName);
    static DWORD QueryServiceStartType(std::string rootKey, std::string szDriverName, BOOL isDriver);
    static DWORD GetSubKeysList(const std::string& key, std::list<std::string>& subkeys);
};

namespace SourceRegistry{
    const char  SERVICES_KEY_VALUE[] = "Services";
    const char  PARTMGR_PARAMS_KEY[] = "SYSTEM\\CurrentControlSet\\Services\\partmgr\\Parameters";
    const char IMAGE_PATH_KEY[] = "ImagePath";
    const char  REG_PATH_SEPARATOR[] = "\\";
    const char  DIR_PATH_SEPARATOR[] = "\\";
    const char START_TYPE[] = "Start";
    const char MSFT_SCSI_KEYS[] = "SYSTEM\\CurrentControlSet\\Enum\\SCSI";
}

namespace HydrationAttributeNames {
    const char WIN32_DISKDRIVE_WMI_NAME_STR[] = "Win32DiskDriveHr";
    const char NUM_OF_DISKS_STR[] = "Disk";
    const char NUM_DYNAMIC_DISKS_STR[] = "Dyn";
    const char CTRL_SET_QUERY_ERR_STR[] = "CtrlSetQErr";
}

namespace HydrationAttributeValueNames {
    const char* ATTRIBUTE_NO_HANDLER_NAME = "NoHandler";
}

namespace HydrationAttributes{
    const char BOOT_DRIVERS[] = "boot_drivers";
    const char SYSTEM_DRIVERS[] = "system_drivers";
    const char DEMAND_START_DRIVERS[] = "demandstart_drivers";
    const char AUTO_SERVICES[] = "automatic_services";
    const char SANPOLICY[] = "sanpolicy";
    const char W2K8_SANPOLICY[] = "w2k8_sanpolicy";
    const char W2K19_SANPOLICY[] = "w2k19_sanpolicy";
    const char BOOTDISKLAYOUT[] = "bootdisklayout";
    const char COND_ONDEMAND_DRIVERS[] = "cond_ondemanddrivers";
    const char CONDITION[] = "condition";
    const char MSFT_HW_KEY_EXISTS[] = "msft_hwkeys_exists";
    const char INDSK_BOOT_INFO[] = "indsk_bootinfo";
    const char CONTAINS_DYNAMIC_DISKS[] = "contains_dynamic_disks";
    const char UNSUPPORTED_OS[] = "unsupported_os";
};


namespace InDskFltAttributes{
    const char PARAMETERS_KEY[] = "SYSTEM\\CurrentControlSet\\Services\\InDskFlt\\Parameters";
    const char BOOTDISK_REG_KEY[] = "SYSTEM\\CurrentControlSet\\Services\\InDskFlt\\Parameters\\BootDiskInfo";
    const char DRIVER_KEY[] = "SYSTEM\\CurrentControlSet\\Services\\InDskFlt";
    const char BOOTINFORMATION_KEY[] = "BootDiskInfo";
    const char BOOTDEV_VENDOR_ID[] = "VendorId";
    const char BOOTDEV_ID[] = "DeviceId";
    const char BOOTDEV_BUS[] = "BusType";
    const char DisableNoHydrationWF[] = "DisableNoHydrationWF";
};

