#include <memory>
#include <set>
#include <string>
#include <new>

#include "entity.h"
#include "volumemanager.h"
#include "volume.h"
#include "synchronize.h"
#include "error.h"
#include "portablehelpers.h"

VolumeManager* VolumeManager::theVolMan = NULL;
Lockable VolumeManager::m_CreateLock;

VolumeManager::VolumeManager()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

VolumeManager::~VolumeManager()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

}

int VolumeManager::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	m_bInitialized = true;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return 1;
}

cdp_volume_t::Ptr VolumeManager::CreateVolume(const std::string &name, const int &devicetype, const VolumeSummaries_t *pVolumeSummariesCache)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with name %s, devicetype %d, pVolumeSummaries %p\n", FUNCTION_NAME, name.c_str(), devicetype, pVolumeSummariesCache);
	cdp_volume_t::Ptr pVolume;

	if ( IsInitialized() ) 
	{
		cdp_volume_t::CDP_VOLUME_TYPE vtype = (VolumeSummary::DISK == devicetype) ? cdp_volume_t::CDP_DISK_TYPE : cdp_volume_t::CDP_REAL_VOLUME_TYPE;
		try {
			cdp_volume_t *p = new cdp_volume_t(name, false, vtype, pVolumeSummariesCache);
			pVolume.reset(p);
		} catch (std::bad_alloc &e) {
			DebugPrintf(SV_LOG_ERROR, "FAILED: Memory allocation for cdp_volume_t object failed for device %s with error %s.@LINE %d in FILE %s\n", name.c_str(), e.what(), LINE_NO, FILE_NAME);
		}
	}
    else
    {   
		DebugPrintf(SV_LOG_ERROR, "FAILED: Volume Manager is not initialized.@LINE %d in FILE %s\n", LINE_NO, FILE_NAME);
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return pVolume;

}

bool VolumeManager::Destroy()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if (theVolMan)
    {
        AutoLock CreateGuard(m_CreateLock);
        if (theVolMan)
        {
            delete theVolMan;
            theVolMan = NULL;
        }
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return true;
}

VolumeManager& VolumeManager::GetInstance()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if (!theVolMan)
    {
        AutoLock CreateGuard(m_CreateLock);
        if (!theVolMan)
        {
            theVolMan = new VolumeManager();
            theVolMan->Init();
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return *theVolMan;
	
}
