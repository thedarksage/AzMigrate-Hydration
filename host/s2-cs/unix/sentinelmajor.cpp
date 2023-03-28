#include "sentinel.h"
#include "involfltfeatures.h"

bool Sentinel::IsFilterDriverCompatible(const DRIVER_VERSION &dv)
{
	//Unix: driver is compatible
	return true;
}

int Sentinel::GetDriverFeatures(void)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	int iStatus;
	DRIVER_VERSION dv;
	DeviceStream *p = m_pFilterDriverNotifier->GetDeviceStream();

	//Get version
    iStatus = p->IoControl( IOCTL_INMAGE_GET_DRIVER_VERSION, &dv ) ;
    if (SV_SUCCESS == iStatus) {
		DebugPrintf(SV_LOG_DEBUG, "Major Version: %d\n Minor Version: %d\n Minor Version2: %d\n MinorVersion3:%d\n",
			dv.ulDrMajorVersion, dv.ulDrMinorVersion,
			dv.ulDrMinorVersion2, dv.ulDrMinorVersion3);
        try {
            //Get features
            DeviceFilterFeatures *pf = new InVolFltFeatures(dv);
            m_pDeviceFilterFeatures.reset(pf);
			DebugPrintf(SV_LOG_DEBUG, "Got features.\n");
        }
        catch (std::bad_alloc &e) {
            DebugPrintf(SV_LOG_ERROR, "FAILED: Memory allocation for device filter features object failed with error %s.@LINE %d in FILE %s\n", e.what(), LINE_NO, FILE_NAME);
            iStatus = SV_FAILURE;
        }
	}
	else
        DebugPrintf( SV_LOG_ERROR, "IOCTL to Get Driver Version is failed with error code %d. @LINE %d in FILE %s:\n", p->GetErrorCode(), LINE_NO, FILE_NAME ) ;
    
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return iStatus;
}
