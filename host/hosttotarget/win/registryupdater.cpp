
///
/// \file registryupdater.cpp
///
/// \brief
///
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <map>
#include <set>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "scopeguard.h"
#include "registryupdater.h"
#include "fiopipe.h"

# pragma comment(lib, "wbemuuid.lib")

namespace HostToTarget {

    char const * const SYSTEM_HIVE_LOAD_REGKEY = "InMageSystemRegkey";       ///< registry key name used loading system hive
    char const * const SYSTEM_HIVE_PATH = "config\\system";                  ///< system hive to be loaded
    char const * const SOFTWARE_HIVE_LOAD_REGKEY = "InMageSoftwareRegkey";   ///< registry key name used when loading software hive
    char const * const SOFTWARE_HIVE_PATH = "config\\software";              ///< software hive to be loaded

    struct RegKeySimpleGuard {
        RegKeySimpleGuard()
            : m_key(0)
            {
            }
        RegKeySimpleGuard(HKEY key)
            : m_key(key)
            {
            }

        ~RegKeySimpleGuard()
            {
                if (0 != m_key) {
                    RegCloseKey(m_key);
                }
            }

        void set(HKEY key)
            {
                m_key = key;
            }

        void dismiss()
            {
                m_key = 0;
            }
    private:
        HKEY m_key;
    };

    // holds all the key/values that need to be added under HKEY_LOCAL_MACHINE\SYSTEM\ControlSet* for LSI_SCSI
    // (where ControlSets* will be expanded to all ControlSet entrys e.g. ControlSet001)
    // as such all keys name need to be relative and will be prepended with the proper SYSTEM\ControlSet prefix
    RegistryUpdater::RegistryKeyValue g_registrySystemControlSetsKeyValueLsiScsi[] =
    {
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Control\\CriticalDeviceDatabase\\pci#ven_1000&dev_0030" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Service",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "symmpi" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "LSI_SCSI" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Control\\CriticalDeviceDatabase\\pci#ven_1000&dev_0030" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ClassGUID",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "{4D36E97B-E325-11CE-BFC1-08002BE10318}" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Control\\CriticalDeviceDatabase\\pci#ven_1000&dev_0030" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "DriverPackageId",
            REG_SZ,
            {
                { OS_TYPE_W2K8_R2, "lsi_scsi.inf_amd64_neutral_cfbbf0b0b66ba280" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Group",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "SCSI miniport" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ImagePath",
            REG_EXPAND_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "system32\\DRIVERS\\symmpi.sys" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "system32\\DRIVERS\\LSI_SCSI.sys" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ErrorControl",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Start",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "0" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Type",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Tag",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "33" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64, "64" },
                { OS_TYPE_W2K8_R2, "34" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_W2K8_R2, "Services\\LSI_SCSI" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "DriverPackageId",
            REG_SZ,
            {
                { OS_TYPE_W2K8_R2, "lsi_scsi.inf_amd64_neutral_cfbbf0b0b66ba280" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi\\Parameters" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI\\Parameters" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "BusType",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi\\Parameters\\PnpInterface" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI\\Parameters\\PnpInterface" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "5",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi\\Enum" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Count",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi\\Enum" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "NextInstance",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "Services\\symmpi\\Enum" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Services\\LSI_SCSI\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "0",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64, "PCI\\VEN_1000&DEV_0030&SUBSYS_00000000&REV_01\\3&61aaa01&0&80" },
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "PCI\\VEN_1000&DEV_0030&SUBSYS_197615AD&REV_01\\4&b70f118&0&1088" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // must always be present and the last entry to indicate the end
        {
            {
                { OS_TYPE_NOT_SUPPORTED }
            }
        }
    };

    // holds all the key/values that need to be added under HKEY_LOCAL_MACHINE\SYSTEM\ControlSet*
    // (where ControlSets* will be expanded to all ControlSet entrys e.g. ControlSet001)
    // as such all keys name need to be relative and will be prepended with the proper SYSTEM\ControlSet prefix
    RegistryUpdater::RegistryKeyValue g_registrySystemControlSetsKeyValueLsiSas[] =
    {
        // "Control\\CriticalDeviceDatabase"
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Control\\CriticalDeviceDatabase\\PCI#VEN_1000&DEV_0054" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ClassGUID",
            REG_SZ,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "{4D36E97B-E325-11CE-BFC1-08002BE10318}" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Control\\CriticalDeviceDatabase\\PCI#VEN_1000&DEV_0054" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Service",
            REG_SZ,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2, "Control\\CriticalDeviceDatabase\\PCI#VEN_1000&DEV_0054" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "DriverPackageId",
            REG_SZ,
            {
                { OS_TYPE_W2K8_R2, "lsi_sas.inf_amd64_neutral_a4d6780f72cbd5b4" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // "Services\\<device>"
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Start",
            REG_DWORD,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "0" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Type",
            REG_DWORD,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ErrorControl",
            REG_DWORD,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ImagePath",
            REG_EXPAND_SZ,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "system32\\drivers\\lsi_sas.sys" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Group",
            REG_SZ,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "SCSI miniport" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Tag",
            REG_DWORD,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64, "64" },
                { OS_TYPE_W2K8_R2, "34" },
                { OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "68" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_W2K8_R2, "Services\\LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "DriverPackageId",
            REG_SZ,
            {
                { OS_TYPE_W2K8_R2, "lsi_sas.inf_amd64_neutral_cfbbf0b0b66ba280" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "DriverName",
            REG_SZ,
            {
                { OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "lsi_sas.inf" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // "Services\\<device>\\Parameters"
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 |  OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS\\Parameters" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "BusType",
            REG_DWORD,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "10" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // "Services\\<device>\\Parameters\\PnpInterface"
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS\\Parameters\\PnpInterface" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "5",
            REG_DWORD,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8 , "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // "Services\\<device>\\Parameters\\PnpInterface\\Device"
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS\\Parameters\\PnpInterface\\Device" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "DriverParameter",
            REG_SZ,
            {
                { OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "PlaceHolder=0;" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS\\Parameters\\PnpInterface\\Device" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "EnableQueryAccessAlignment",
            REG_DWORD,
            {
                { OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // "Services\\<device>\\enum"
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "0",
            REG_SZ,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "PCI\\VEN_1000&DEV_0054&SUBSYS_197615AD&REV_01\\4&1f16fef7&0&00A8" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Count",
            REG_DWORD,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_VMWARE,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\LSI_SAS\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "NextInstance",
            REG_DWORD,
            {
                { OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // must always be present and the last entry to indicate the end
        {
            {
                { OS_TYPE_NOT_SUPPORTED }
            }
        }
    };

    // holds all the key/values that need to be added under HKEY_LOCAL_MACHINE\SYSTEM\ControlSet*
    // (where ControlSets* will be expanded to all ControlSet entrys e.g. ControlSet001)
    // as such all keys name need to be relative and will be prepended with the proper SYSTEM\ControlSet prefix
    RegistryUpdater::RegistryKeyValue g_registrySystemControlSetsKeyValueIntelIde[] =
    {
        // "Control\\CriticalDeviceDatabase"
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Control\\CriticalDeviceDatabase\\pci#VEN_8086&CC_0101" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ClassGUID",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "{4d36e97d-e325-11ce-bfc1-08002be10318}" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Control\\CriticalDeviceDatabase\\pci#VEN_8086&CC_0101" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Service",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "intelide" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // services
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\IntelIde" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ErrorControl",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\IntelIde" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Group",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "System Bus Extender" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\IntelIde" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Start",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "0" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\IntelIde" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Tag",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "4" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\IntelIde" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Type",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\IntelIde" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ImagePath",
            REG_EXPAND_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "system32\\DRIVERS\\intelide.sys" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // services\sstorvsc
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Group",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Base" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ImagePath",
            REG_EXPAND_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "system32\\DRIVERS\\storvsc.sys" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ErrorControl",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Start",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "0" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Type",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "BusType",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Tag",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "21" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "0",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "VMBUS\\{7bf79f37-bad7-4672-887d-148b09f44b93}\\5&296c0f0e&0&{7bf79f37-bad7-4672-887d-148b09f44b93}" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Count",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\storvsc\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "NextInstance",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // services\vmbus
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "DisplayName",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Virtual Machine Bus" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Group",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "System Bus Extender" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ImagePath",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "system32\\DRIVERS\\vmbus.sys" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "ErrorControl",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Start",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "0" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Type",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Tag",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "7" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // Services\vmbus\Enum
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "0",
            REG_SZ,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "ACPI\\VMBus\\4&215d0f95&0" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "Count",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        {
            HOST_TO_HYPERVISOR,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "Services\\vmbus\\Enum" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            },
            "NextInstance",
            REG_DWORD,
            {
                { OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_W2K8_32 | OS_TYPE_VISTA_64 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8, "1" },
                { OS_TYPE_NOT_SUPPORTED } // must always be present and the last entry to indicate the end
            }
        },
        // must always be present and the last entry to indicate the end
        {
            {
                { OS_TYPE_NOT_SUPPORTED }
            }
        }
    };

    typedef std::set<std::wstring> scsiControllers_t;

    HRESULT getScsiControllers(scsiControllers_t& scsiControllers)
    {
        HRESULT hres;
        IWbemLocator *pLoc = NULL;
        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc);
        if (FAILED(hres)) {
            return hres;
        }
        IWbemServices *pSvc = NULL;
        hres = pLoc->ConnectServer(L"ROOT\\CIMV2", NULL, NULL, 0, NULL, 0, 0, &pSvc);
        if (FAILED(hres)) {
            pLoc->Release();
            return hres;
        }
        hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        if (FAILED(hres)) {
            pSvc->Release();
            pLoc->Release();
            return hres;
        }
        IEnumWbemClassObject* pEnumerator = NULL;
        hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(L"select * from Win32_SCSIController"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
        if (FAILED(hres)) {
            pSvc->Release();
            pLoc->Release();
        }
        IWbemClassObject *pclsObj = 0;
        ULONG uReturn = 0;
        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) {
                break;
            }
            VARIANT vtProp;
            hr = pclsObj->Get(L"DriverName", 0, &vtProp, 0, 0);
            if (WBEM_S_NO_ERROR == hr) {
                scsiControllers.insert((wchar_t*)vtProp.bstrVal);
            }
            VariantClear(&vtProp);
        }
        pSvc->Release();
        pLoc->Release();
        pEnumerator->Release();
        if (0 != pclsObj) {
            pclsObj->Release();
        }
        return hres;
    }

    bool RegistryUpdater::setRegistrySystemKeyValue(TargetTypes targetType, OsTypes osType)
    {
        m_registrySystemControlSetsKeyValue = 0;
        switch (targetType) {
            case HOST_TO_VMWARE:
                return setLsiRegistrySystemKeyValue(osType);
            case HOST_TO_HYPERVISOR:
                m_registrySystemControlSetsKeyValue = g_registrySystemControlSetsKeyValueIntelIde;
                return true;
            default:
                break;
        }
        return false;
    }

    bool RegistryUpdater::setLsiRegistrySystemKeyValue(OsTypes osType)
    {
        bool ok = false;

        // TODO: for now w2k3 always uses scsi
        if (0 != ((OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64) & osType)) {
            m_registrySystemControlSetsKeyValue = g_registrySystemControlSetsKeyValueLsiScsi;
            return true;
        }

        scsiControllers_t scsiControllers;
        HRESULT hres = getScsiControllers(scsiControllers);
        if (scsiControllers.empty()) {
            return setDefaultLsi(osType);
        } else {
            scsiControllers_t::iterator iter(scsiControllers.begin());
            scsiControllers_t::iterator iterEnd(scsiControllers.end());
            for (/* emtpy */; iter!= iterEnd; ++iter) {
                // TODO: for now use the first one found
                if (boost::algorithm::iequals(*iter, "LSI_SAS")) {
                    m_registrySystemControlSetsKeyValue = g_registrySystemControlSetsKeyValueLsiSas;
                    return true;
                } else if (boost::algorithm::iequals(*iter, "LSI_SCSI")) {
                    m_registrySystemControlSetsKeyValue = g_registrySystemControlSetsKeyValueLsiScsi;
                    return true;
                }
            }
            // no scsi controlers found that match what is expected. set to defaults
            return setDefaultLsi(osType);
        }
        return false;
    }

    bool RegistryUpdater::setDefaultLsi(int osType)
    {
        if (0 != ((OS_TYPE_XP_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_32 | OS_TYPE_W2K3_64 | OS_TYPE_VISTA_32 | OS_TYPE_VISTA_64) & osType)) {
            m_registrySystemControlSetsKeyValue = g_registrySystemControlSetsKeyValueLsiScsi;
            return true;
        }
        if (0 != ((OS_TYPE_W2K8_32 | OS_TYPE_W2K8_64 | OS_TYPE_W2K8_R2 | OS_TYPE_WIN7 | OS_TYPE_W2K12 | OS_TYPE_WIN8) & osType)) {
            m_registrySystemControlSetsKeyValue = g_registrySystemControlSetsKeyValueLsiSas;
            return true;
        }
        return false;
    }

    bool RegistryUpdater::update(std::string const& systemVolume, std::string const& windowsDir, TargetTypes targetType, OsTypes osType, int opts)
    {
        std::string systemDir(windowsDir + "system32\\");
        clearErrors();
        ERR_TYPE err = setPrivilege(SE_BACKUP_NAME);
        if (ERROR_OK != err) {
            setError(ERROR_INTERNAL_BACKUP_PRIVILEGE, err);
            return false;
        }
        err = setPrivilege(SE_RESTORE_NAME);
        if (ERROR_OK != err) {
            setError(ERROR_INTERNAL_RESTORE_PRIVILEGE, err);
            return false;
        }
        if (!setRegistrySystemKeyValue(targetType, osType)) {
            setError(ERROR_INTERNAL_OS_NO_SUPPORTED_SCSI_CONTROLLERS_FOUND);
            return false;
        }
        bool result = true;
        if (0 == (UPDATE_OPTION_REGISTRY_UPDATE_SOFTWARE_HIVE_ONLY & opts)) {
            result = updateSystemHive(systemDir, targetType, osType, 0 != (opts & UPDATE_OTPION_MANUAL_START_SERVICES));
        }
        // for now ignore errors updating system hive since it just sets runonce and if that fails user can still
        // bring scsi disks online
        updateSoftwareHive(systemVolume, systemDir, targetType, osType, opts);
        return result;
    }

    bool RegistryUpdater::updateSystemHive(std::string const& systemDir, TargetTypes targetType, OsTypes osType, bool setServicesToManualStart)
    {
        std::string systemHive(systemDir);
        systemHive += SYSTEM_HIVE_PATH;
        std::string systemHiveBackup(systemHive + ".backup");
        if (!CopyFile(systemHive.c_str(), systemHiveBackup.c_str(), FALSE)) {
            // FIXME: copy file failed
            std::string msg(systemHive);
            msg += " -> ";
            msg += systemHiveBackup;
            setError(ERROR_INTERNAL_BACKUP_HIVE, GetLastError(), msg);
            return false;
        }

        long result = RegLoadKey(HKEY_LOCAL_MACHINE, SYSTEM_HIVE_LOAD_REGKEY, systemHive.c_str());
        if (ERROR_SUCCESS != result) {
            setError(ERROR_INTERNAL_LOAD_HIVE, result);
            return false;
        }
        ON_BLOCK_EXIT(RegUnLoadKey, HKEY_LOCAL_MACHINE, SYSTEM_HIVE_LOAD_REGKEY);

        if (!updateControlSets(targetType, osType, setServicesToManualStart)) {
            MoveFileEx(systemHiveBackup.c_str(), systemHive.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            return false;
        }
        return true;
    }

    bool RegistryUpdater::updateSoftwareHive(std::string const& systemVolume, std::string const& systemDir, TargetTypes targetType, OsTypes osType, int opts)
    {
        std::string softwareHive(systemDir);
        softwareHive += SOFTWARE_HIVE_PATH;
        std::string softwareHiveBackup(softwareHive + ".backup");
        if (!CopyFile(softwareHive.c_str(), softwareHiveBackup.c_str(), FALSE)) {
            // FIXME: copy file failed
            std::string msg(softwareHive);
            msg += " -> ";
            msg += softwareHiveBackup;
            setError(ERROR_INTERNAL_BACKUP_HIVE, GetLastError(), msg);
            return false;
        }
        long result = RegLoadKey(HKEY_LOCAL_MACHINE, SOFTWARE_HIVE_LOAD_REGKEY, softwareHive.c_str());
        if (ERROR_SUCCESS != result) {
            setError(ERROR_INTERNAL_LOAD_HIVE, result);
            return false;
        }
        ON_BLOCK_EXIT(RegUnLoadKey, HKEY_LOCAL_MACHINE, SOFTWARE_HIVE_LOAD_REGKEY);

        // for now ignore errors setting up run once
        setVhdOnlineRunOnce(systemVolume, targetType, osType, false);
        disableWindowsErrorRecovery(systemVolume, osType, false);
        // TODO: need to figure out how to prevent Shutdown Event Tracker from being launched
        // currently the issue that if it is disabled and needs to be reset, the reset completes
        // before windows checks if it needs to launch the Shutdown Event Tracker, so it ends up
        // getting launched. Adding delays to resetting does not seem to help. Note disabling and
        // not resetting does work, but we can not disable it as the user may want it enabled
        // disableShutdownEventTracker(systemVolume, false);
        return true;
    }

    bool RegistryUpdater::disableShutdownEventTracker(std::string const& systemVolume, bool setErrors)
    {
        bool deleteEntry = false;
        std::string keyName(SOFTWARE_HIVE_LOAD_REGKEY);
        keyName += "\\Policies\\Microsoft\\Windows NT\\Reliability";
        DWORD value = 1;
        DWORD valueType;
        DWORD valueSize = sizeof(value);
        DWORD disp;
        HKEY key;
        long result = RegGetValue(HKEY_LOCAL_MACHINE, keyName.c_str(), "ShutdownReasonOn", RRF_RT_REG_DWORD, &valueType, &value, &valueSize);
        switch (result) {
            case ERROR_SUCCESS:
                if (0 == value) {
                    return true; // already disabled
                }
                deleteEntry = false;
                break;
            case ERROR_FILE_NOT_FOUND:
                deleteEntry = true;
                break;
            default:
                if (setErrors) {
                    // TODO:
                }
                return false;
        }
        result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp);
        if (ERROR_SUCCESS != result) {
            if (setErrors) {
                // TODO:
            }
            return false;
        }
        ON_BLOCK_EXIT(&RegCloseKey, key);

        value = 0;
        result = RegSetValueEx(key, "ShutdownReasonOn", 0, REG_DWORD, reinterpret_cast<BYTE*>(&value), sizeof(value));
        if (ERROR_SUCCESS != result) {
            if (setErrors) {
                // TODO:
            }
            return false;
        }
        result = RegDeleteValue(key, "ShutdownReasonUI");
        if (!(ERROR_SUCCESS == result || ERROR_FILE_NOT_FOUND == result)) {
            if (setErrors) {
                // TODO:
            }
        }
        return resetShutdownEventTracker(systemVolume, deleteEntry, setErrors);
    }

    bool RegistryUpdater::resetShutdownEventTracker(std::string const& systemVolume, bool deleteEntry, bool setErrors)
    {
        std::string batName(systemVolume);
        batName += "resetshutdowneventtracker.bat";
        std::string batScript("@echo off\n");
        if (deleteEntry) {
            batScript += "@reg delete \"HKLM\\software\\Policies\\Microsoft\\Windows NT\\Reliability\" /f\n";
        } else {
            batScript += "@reg add \"HKLM\\software\\Policies\\Microsoft\\Windows NT\\Reliability\" /v ShutdownReasonOn /t REG_DWORD /d 1 /f\n";
            batScript += "@reg add \"HKLM\\software\\Policies\\Microsoft\\Windows NT\\Reliability\" /v ShutdownReasonUI /t REG_DWORD /d 1 /f\n";
        }
        batScript += "@del /Q /F %0\nexit";
        if (!createBatFile(batName, batScript, setErrors)) {
            return false;
        }
        if (!createBatFile(batName, batScript, setErrors)) {
            return false;
        }

        return setRunOnce("!resetshutdowneventtracker", batName, setErrors);
    }

    bool RegistryUpdater::disableWindowsErrorRecovery(std::string const& systemVolume, OsTypes osType, bool setErrors)
    {
        try {
            if (0 != ((OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64) & osType)) {
                return true;
            }

            char windowsPath[MAX_PATH];
            if (0 == GetWindowsDirectory(windowsPath, sizeof(windowsPath))) {
                if (setErrors) {
                    setError(ERROR_INTERNAL_OPEN_FILE, GetLastError());
                }
                return false;
            }

            std::string bcdEdit(windowsPath);
            bcdEdit += "\\system32\\bcdedit.exe";
            {
                // simplest way to make sure it works if 32bit app running on 64bit OS
                // do inside scope so file is closed as soon as no longer needed
                std::ifstream bcdEditFile(bcdEdit.c_str());
                if (!bcdEditFile.good()) {
                    bcdEdit = windowsPath;
                    bcdEdit += "\\Sysnative\\bcdedit.exe";
                }
            }
            std::string bcdEditOut;
            {
                FIO::cmdArgs_t vargs;
                vargs.push_back(bcdEdit.c_str());
                vargs.push_back("/enum");
                vargs.push_back((char*)0);
                FIO::FioPipe fioPipe(vargs, true); // use true to tie stderr to stdout
                long bytes;
                char buffer[1024];
                do {
                    while ((bytes = fioPipe.read(buffer, sizeof(buffer), false)) > 0) {
                        bcdEditOut.append(buffer, bytes);
                    }
                } while (FIO::FIO_WOULD_BLOCK == fioPipe.error() && bcdEditOut.empty());
            }
            std::string::size_type idx = bcdEditOut.find("bootstatuspolicy");
            if (!boost::algorithm::icontains(bcdEditOut, "ignoreallfailures")) {
                std::string bcdName = getBcdFileName(systemVolume);
                if (bcdName.empty()) {
                    return false;
                }
                FIO::cmdArgs_t vargs;
                vargs.push_back(bcdEdit.c_str());
                vargs.push_back("/store");
                vargs.push_back(bcdName.c_str());
                vargs.push_back("/set");
                vargs.push_back("{default}");
                vargs.push_back("bootstatuspolicy");
                vargs.push_back("ignoreallfailures");
                vargs.push_back((char*)0);
                FIO::FioPipe fioPipe(vargs, true); // use true to tie stderr to stdout
                long bytes;
                char buffer[1024];
                do {
                    while ((bytes = fioPipe.read(buffer, sizeof(buffer), false)) > 0) {
                        bcdEditOut.append(buffer, bytes);
                    }
                } while (FIO::FIO_WOULD_BLOCK == fioPipe.error() && bcdEditOut.empty());

                // set ignore all failures and setup runonce to reset it
                setResetBootStatusPolicyRunOnce(systemVolume, setErrors);
            }
        } catch (std::exception const& e) {
            if (setErrors) {
                // TODO: if we ever care about these errors
            }
        } catch (...) {
            if (setErrors) {
                // TODO: if we ever care about these errors
            }
        }
        return false;
    }

    std::string RegistryUpdater::getBcdFileName(std::string const& systemVolume)
    {
        // first try systemVolume
        std::string bcdName(systemVolume);
        bcdName += "boot\\bcd";
        try {
            if (boost::filesystem::exists(bcdName)) {
                return bcdName;
            }
        } catch (...) {
        }
        // next see if our default mount point is there and has the file
        bcdName = systemVolume;
        bcdName += "srv\\boot\\bcd";
        try {
            if (boost::filesystem::exists(bcdName)) {
                return bcdName;
            }
        } catch (...) {
        }
        // try all drives note could skip system drive since it was already tried and failed
        // but not reason to complicate the code with that
        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (0 != (drives & (1 << i))) {
                bcdName = 'A' + i;
                bcdName += ":\\boot\\bcd";
                try {
                    if (boost::filesystem::exists(bcdName)) {
                        return bcdName;
                    }
                } catch (...) {
                }
            }
        }
        return std::string();
    }

    bool RegistryUpdater::createBatFile(std::string const& batName, std::string const& batScript, bool setErrors)
    {
        std::ofstream runOnceBat(batName.c_str());
        if (!runOnceBat.good()) {
            if (setErrors) {
                setError(ERROR_INTERNAL_OPEN_FILE, GetLastError());
            }
            return false;
        }
        runOnceBat << batScript;
        return true;
    }

    bool RegistryUpdater::setRunOnce(char const* name, std::string const& cmd, bool setErrors)
    {
        HKEY key;
        std::string runOnceKeyName(SOFTWARE_HIVE_LOAD_REGKEY);
        runOnceKeyName += "\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
        long rc = RegOpenKey(HKEY_LOCAL_MACHINE, runOnceKeyName.c_str(), &key);
        if (ERROR_SUCCESS != rc) {
            if (setErrors) {
                setError(ERROR_INTERNAL_REG_OPEN_KEY, rc);
            }
            return false;
        }
        ON_BLOCK_EXIT(&RegCloseKey, key);
        rc = RegSetValueEx(key, name, 0, REG_SZ, (BYTE*)cmd.c_str(), (DWORD)cmd.size() + 1);
        if (ERROR_SUCCESS != rc) {
            if (setErrors) {
                setError(ERROR_INTERNAL_REG_UPDATE, rc);
            }
            return false;
        }
        return true;
    }

    bool RegistryUpdater::setResetBootStatusPolicyRunOnce(std::string const& systemVolume, bool setErrors)
    {
        std::string batName(systemVolume);
        batName += "resetbootstatuspolicy.bat";
        std::string batScript("@echo off\n"
                              "@bcdedit /set {current} bootstatuspolicy displayallfailures\n"
                              "@del /Q /F %0\n"
                              "exit");
        if (!createBatFile(batName, batScript, setErrors)) {
            return false;
        }
        return setRunOnce("!resetbootstatuspolicy", batName, setErrors);
    }

    bool RegistryUpdater::setVhdOnlineRunOnce(std::string const& systemVolume, TargetTypes targetType, OsTypes osType, bool setErrors)
    {
        std::string diskParam;
        std::string clearReadOnly;
        if (0 == ((OS_TYPE_XP_32 | OS_TYPE_W2K3_32 | OS_TYPE_XP_64 | OS_TYPE_W2K3_64) & osType)) {
            diskParam = "disk";
            clearReadOnly = "            @echo attributes disk clear readonly >> ";
            clearReadOnly += systemVolume;
            clearReadOnly += "vhdonline.dps\n";
        }
        std::stringstream batScript;
        // the following script attempt to bring volumes online after first boot when completing host-to-traget
        // it performs 4 passes as follows
        // each pass list disks to get current disk status (%%k will have the status)
        // pass 1 (%%x == 1):
        //   any disk reporting error, clear read only and bring offline
        //   as sometimes bringing it offline and then online fixes the errors
        // pass 2 (%%x == 2):
        //   any disks reporting offline, clear read only and bring online
        // pass 3 (%%x == 3):
        //   any disks reporting foreign, import it
        //   note in realy only need to do this for one disk and could skip the others as import imports
        //   all dynamic disks in the group
        // pass 4 (%%x == 4):
        //   any disks reporting error, recover it
        //
        // if import or recover is needed, it will reboot the system
        batScript << "@echo off\n"
                  << "@setlocal\n"
                  << "@setlocal EnableDelayedExpansion\n"
                  << "@setlocal EnableExtensions\n"
                  << "@set needsReboot=n\n"
                  << "@echo > " << systemVolume << "vhdonline.dps\n"
                  << "@echo list disk > " << systemVolume << "vhddisks.dps\n"
                  << "@for %%x in (1 2 3 4) do (\n"
                  << "    @for /F \"tokens=1,2,3\" %%i in ('diskpart /s " << systemVolume << "vhddisks.dps') do (\n"
                  << "        @if %%x == 1 (\n"
                  << "            @if %%k == Errors (\n"
                  << "                @echo select disk %%j > " << systemVolume << "vhdonline.dps\n"
                  << clearReadOnly
                  << "                @echo offline " << diskParam << " >> " << systemVolume << "vhdonline.dps\n"            
                  << "                @echo exit >> " << systemVolume << "vhdonline.dps\n"
                  << "                @diskpart /s " << systemVolume << "vhdonline.dps\n"
                  << "            )\n"
                  << "        )\n"
                  << "        @if %%x == 2 (\n"
                  << "            @if %%k == Offline (\n"
                  << "                @echo select disk %%j > " << systemVolume << "vhdonline.dps\n"
                  << clearReadOnly
                  << "                @echo online " << diskParam << " >> " << systemVolume << "vhdonline.dps\n"
                  << "                @echo exit  >> " << systemVolume << "vhdonline.dps\n"
                  << "                @diskpart /s " << systemVolume << "vhdonline.dps\n"
                  << "            )\n"
                  << "        )\n"
                  << "        @if %%x == 3 (\n"
                  << "            @if %%k == Foreign (\n"
                  << "                @set needsReboot=y\n"
                  << "                @echo select disk %%j > " << systemVolume << "vhdonline.dps\n"
                  << "                @echo import >> " << systemVolume << "vhdonline.dps\n"
                  << "                @echo exit  >> " << systemVolume << "vhdonline.dps\n"
                  << "                @diskpart /s " << systemVolume << "vhdonline.dps\n"
                  << "            )\n"
                  << "        )\n"
                  << "        @if %%x == 4 (\n"
                  << "            @if %%k == Errors (\n"
                  << "                @set needsReboot=y\n"
                  << "                @echo select disk %%j > " << systemVolume << "vhdonline.dps\n"
                  << "                @echo recover >> " << systemVolume << "vhdonline.dps\n"            
                  << "                @echo exit >> " << systemVolume << "vhdonline.dps\n"
                  << "                @diskpart /s " << systemVolume << "vhdonline.dps\n"
                  << "            )\n"
                  << "        )\n"
                  << "    )\n"
                  << ")\n"
                  << "@del /Q /F " << systemVolume << "vhddisks.dps\n"
                  << "@del /Q /F " << systemVolume << "vhdonline.dps\n"
                  << "@del /Q /F %0\n"
                  << "@if !needsReboot! == y (\n"
                  << "    @shutdown -r -t 5 -d pu:0:0\n"
                  << ")\n"
                  << "exit\n";
        std::string batName(systemVolume);
        batName += "vhdrunonce.bat";
        if (!createBatFile(batName, batScript.str(), setErrors)) {
            return false;
        }
        return setRunOnce("!vhdonline", batName, setErrors);
    }

    bool RegistryUpdater::updateControlSets(TargetTypes targetType, OsTypes osType, bool setServicesToManualStart)
    {
        HKEY key;
        long result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, SYSTEM_HIVE_LOAD_REGKEY, 0, KEY_ALL_ACCESS, &key);
        if (ERROR_SUCCESS != result) {
            setError(ERROR_INTERNAL_REG_OPEN_KEY, result);
            return false;
        }
        ON_BLOCK_EXIT(&RegCloseKey, key);

        int idx = 0;
        char controlSetName[255];
        DWORD controlSetNameSize;
        do {
            controlSetNameSize = sizeof(controlSetName);
            result = RegEnumKeyEx(key, idx, controlSetName, &controlSetNameSize, 0, 0, 0, 0);
            if (ERROR_SUCCESS == result) {
                if (boost::algorithm::istarts_with(controlSetName, "ControlSet")) {
                    if (!updateControlSet(controlSetName, targetType, osType)) {
                        return false;
                    }
                }
            }
            ++idx;
        } while (ERROR_SUCCESS == result);
        return true;
    }

    bool RegistryUpdater::updateControlSet(char const* controlSetName, TargetTypes targetType, OsTypes osType)
    {
        if (0 ==  m_registrySystemControlSetsKeyValue) {
            return true;
        }

        std::string currentKeyName;
        HKEY key = 0;

        RegKeySimpleGuard regkeyGuard;

        std::string controlSetKeyName(SYSTEM_HIVE_LOAD_REGKEY);
        controlSetKeyName += "\\";
        controlSetKeyName += controlSetName;
        for (int idx = 0; HOST_TO_UNKNOWN != m_registrySystemControlSetsKeyValue[idx].m_targetType; ++idx) {
            if (0 == (targetType & m_registrySystemControlSetsKeyValue[idx].m_targetType)) {
                continue;
            }
            int keyIdx = 0;
            for (/* empty */; keyIdx < OS_TYPES_COUNT; ++keyIdx) {
                if (OS_TYPE_NOT_SUPPORTED == m_registrySystemControlSetsKeyValue[idx].m_key[keyIdx].m_osType) {
                    break;
                }
                if (0 == (m_registrySystemControlSetsKeyValue[idx].m_key[keyIdx].m_osType & osType)) {
                    continue;
                }
                int dataIdx = 0;
                for (/* empty */; dataIdx < OS_TYPES_COUNT; ++dataIdx) {
                    if (OS_TYPE_NOT_SUPPORTED == m_registrySystemControlSetsKeyValue[idx].m_data[dataIdx].m_osType) {
                        break;
                    }
                    if (0 == (m_registrySystemControlSetsKeyValue[idx].m_data[dataIdx].m_osType & osType)) {
                        continue;
                    }
                    std::string keyName(controlSetKeyName);
                    keyName += "\\";
                    keyName += m_registrySystemControlSetsKeyValue[idx].m_key[keyIdx].m_value;
                    if (currentKeyName.empty() || !boost::algorithm::iequals(keyName, currentKeyName)) {
                        if (0 != key) {
                            RegCloseKey(key);
                            regkeyGuard.dismiss();
                        }
                        currentKeyName = keyName;
                        DWORD disp;
                        long result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp);
                        if (ERROR_SUCCESS != result) {
                            setError(ERROR_INTERNAL_REG_OPEN_KEY, result);
                            return false;
                        }
                        if (REG_OPENED_EXISTING_KEY == disp) {
                            // FIXME: may need to make sure if the host is a virtual machine that we do not mess up its current settins
                        }
                    }
                    regkeyGuard.set(key);
                    long result;
                    switch (m_registrySystemControlSetsKeyValue[idx].m_type) {
                        case REG_SZ:
                        case REG_EXPAND_SZ:
                            result = RegSetValueEx(key,
                                                   m_registrySystemControlSetsKeyValue[idx].m_name,
                                                   0,
                                                   m_registrySystemControlSetsKeyValue[idx].m_type,
                                                   reinterpret_cast<BYTE const*>(m_registrySystemControlSetsKeyValue[idx].m_data[dataIdx].m_value),
                                                   (DWORD)strlen(m_registrySystemControlSetsKeyValue[idx].m_data[dataIdx].m_value) + 1 );
                            if (ERROR_SUCCESS != result) {
                                setError(ERROR_INTERNAL_REG_UPDATE, result);
                                return false;
                            }
                            break;
                        case REG_BINARY:
                            try {
                                binary_t data;
                                convertHexNumberStringToBinary(m_registrySystemControlSetsKeyValue[idx].m_data[dataIdx].m_value, data);
                                result = RegSetValueEx(key,
                                                       m_registrySystemControlSetsKeyValue[idx].m_name,
                                                       0,
                                                       m_registrySystemControlSetsKeyValue[idx].m_type,
                                                       &data[0],
                                                       (DWORD)data.size());
                                if (ERROR_SUCCESS != result) {
                                    setError(ERROR_INTERNAL_REG_UPDATE, result);
                                    return false;
                                }
                            } catch (...) {
                                std::string msg("could not convert '");
                                msg += m_registrySystemControlSetsKeyValue[idx].m_data[dataIdx].m_value;
                                msg += "' to binary hex number";
                                setError(ERROR_INTERNAL_REG_INVALID_VALUE, ERROR_OK, msg.c_str());
                                return false;
                            }
                            break;
                        case REG_DWORD:
                            try {
                                DWORD data = boost::lexical_cast<DWORD>(m_registrySystemControlSetsKeyValue[idx].m_data[dataIdx].m_value);
                                result = RegSetValueEx(key,
                                                       m_registrySystemControlSetsKeyValue[idx].m_name,
                                                       0,
                                                       m_registrySystemControlSetsKeyValue[idx].m_type,
                                                       reinterpret_cast<BYTE*>(&data),
                                                       sizeof(DWORD));
                                if (ERROR_SUCCESS != result) {
                                    setError(ERROR_INTERNAL_REG_UPDATE, result);
                                    return false;
                                }
                            } catch (...) {
                                std::string msg("could not convert '");
                                msg += m_registrySystemControlSetsKeyValue[idx].m_data[dataIdx].m_value;
                                msg += "' to number";
                                setError(ERROR_INTERNAL_REG_INVALID_VALUE, ERROR_OK, msg.c_str());
                                return false;
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                }
                break;
            }
        }
        return true;
    }

    DWORD RegistryUpdater::loadAdvapiAddresses()
    {
        char systemDir[MAX_PATH];

        UINT result = GetSystemDirectory(systemDir, sizeof(systemDir));
        if (0 == result) {
            return GetLastError();
        }

        std::string advapi32DllName(systemDir);
        advapi32DllName += "\\Advapi32.dll";
        m_advapi32Dll = LoadLibrary(advapi32DllName.c_str());
        if (0 == m_advapi32Dll) {
            return GetLastError();
        }

        // maybe check os arch instead of always trying 64 and then 32
        try {
            m_regDeleteKeyEx = getProcAddress<RegDeleteKeyEx_t>(m_advapi32Dll, "RegDeleteKeyExA");
        } catch (DWORD d1) {
            try {
                m_regDeleteKey = getProcAddress<RegDeleteKey_t>(m_advapi32Dll, "RegDeleteKeyA");
            } catch (...) {
                return d1;
            }
        }
        return ERROR_SUCCESS;
    }

    BYTE RegistryUpdater::convertHexNumberCharToBinary(char c)
    {
        switch (c) {
            case 'a':
            case 'A':
                return 0xa;

            case 'b':
            case 'B':
                return 0xb;
            case 'c':
            case 'C':
                return 0xc;
            case 'd':
            case 'D':
                return 0xd;
            case 'e':
            case 'E':
                return 0xe;
            case 'f':
            case 'F':
                return 0xf;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return c - '0';
            default:
                break;
        }
        std::string msg("invalid hex char: ");
        msg += c;
        throw std::bad_cast(msg.c_str());
    }

    void RegistryUpdater::convertHexNumberStringToBinary(std::string const& src, binary_t& binary)
    {
        binary.clear();
        binary_t::size_type srcIdx = 0;
        binary_t::size_type binaryIdx = 0;
        binary_t::size_type size = src.size();
        if (0 == size % 2) {
            binary.resize(size / 2);
            binary[binaryIdx] = (convertHexNumberCharToBinary(src[srcIdx]) << 4) | convertHexNumberCharToBinary(src[srcIdx + 1]);
            srcIdx = 2;
        } else {
            binary.resize(size / 2 + 1);
            binary[binaryIdx] = convertHexNumberCharToBinary(src[srcIdx]);
            srcIdx = 1;
        }
        ++binaryIdx;
        for (/* empty*/; srcIdx < size; srcIdx += 2, ++binaryIdx) {
            binary[binaryIdx] = (convertHexNumberCharToBinary(src[srcIdx]) << 4) | convertHexNumberCharToBinary(src[srcIdx + 1]);
        }
    }

} // namespace HostToTarget

