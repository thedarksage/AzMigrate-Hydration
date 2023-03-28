#include "entity.h"
#include "synchronize.h"
#include "configurevxagent.h"
#include "localconfigurator.h"
#include "logger.h"
#include "configurator2.h"
#include "portableheaders.h"
#include "sentinel.h"
#include "svdparse.h"
#include "portablehelpersmajor.h"
#include "involfltfeatures.h"
#include "indskfltfeatures.h"

bool Sentinel::IsFilterDriverCompatible(const DRIVER_VERSION &dv)
{
	bool bcompatible = false;

	if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == m_pFilterDriverNotifier->GetDeviceStream()->GetName()) {
		//Disk driver is compatible
		bcompatible = true;
	}
	else {
		//Check volume driver
		if (SV_DRIVER_VERSION(dv.ulDrMajorVersion, dv.ulDrMinorVersion,
			dv.ulDrMinorVersion2, dv.ulDrMinorVersion3)
									> SV_DRIVER_INITIAL_VERSION)
		{
			DebugPrintf(SV_LOG_DEBUG, "Driver version is greater than SV_DRIVER_INITIAL_VERSION. This driver is compatible to run with s2.\n");
			bcompatible = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Filter driver: Major Version: %d\n Minor Version: %d\n Minor Version2: %d\n MinorVersion3:%d which is less than or equal"
				"to SV_DRIVER_INITIAL_VERSION. This driver is not compatible to run with s2.\n", dv.ulDrMajorVersion, dv.ulDrMinorVersion,
				dv.ulDrMinorVersion2, dv.ulDrMinorVersion3);
		}
	}

	return bcompatible;
}

int Sentinel::GetDriverFeatures(void)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	int iStatus;
	DRIVER_VERSION dv;
	DeviceStream *p = m_pFilterDriverNotifier->GetDeviceStream();

	//Get version
    iStatus = p->IoControl(IOCTL_INMAGE_GET_VERSION, NULL, 0, &dv, sizeof dv);
    if (SV_SUCCESS == iStatus) {
		DebugPrintf(SV_LOG_DEBUG, "Major Version: %d\n Minor Version: %d\n Minor Version2: %d\n MinorVersion3:%d\n",
			dv.ulDrMajorVersion, dv.ulDrMinorVersion,
			dv.ulDrMinorVersion2, dv.ulDrMinorVersion3);
		//Check compatiblilty
		iStatus = IsFilterDriverCompatible(dv) ? SV_SUCCESS : SV_FAILURE;
		if (SV_SUCCESS == iStatus) {
			try {
				//Get features
				DeviceFilterFeatures *pf;
				if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME == p->GetName())
					pf = new InDskFltFeatures(dv);
				else
					pf = new InVolFltFeatures(dv);
				m_pDeviceFilterFeatures.reset(pf);
				DebugPrintf(SV_LOG_DEBUG, "Got features.\n");
			}
			catch (std::bad_alloc &e) {
				DebugPrintf(SV_LOG_ERROR, "FAILED: Memory allocation for device filter features object failed with error %s.@LINE %d in FILE %s\n", e.what(), LINE_NO, FILE_NAME);
				iStatus = SV_FAILURE;
			}
		}
	}
	else
		DebugPrintf(SV_LOG_ERROR, "IOCTL to Get Driver Version is failed with error code %d. @LINE %d in FILE %s:\n", p->GetErrorCode(), LINE_NO, FILE_NAME);
    
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return iStatus;
}
