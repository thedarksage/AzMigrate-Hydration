#include "cservice.h"

CService::CService() 
   : opt_install (0), 
     opt_remove (0), 
     opt_start (0), 
     opt_kill (0), 
     daemon(0), 
     startfn(NULL),
     stopfn(NULL),
     startfn_data(NULL),
     stopfn_data(NULL),
     stopAgentsForOfflineResource(false)
{
}

CService::~CService()
{
  //ACE::fini ();	
}

int CService::install_service()
{
	return -1;  
}

int CService::remove_service()
{
	return -1;  
}

int CService::start_service()
{
	return -1;  
}

int CService::stop_service()
{
	return -1;  
}

int CService::run_startup_code()
{
	return -1;  
}

int CService::daemonized()
{
	return -1;  
}

bool CService::isInstalled()
{
	return false;  
}

void CService::name(const char *name, const char *long_name, const char *desc)
{
	//Do nothing
}

int CService::RegisterStartCallBackHandler(int (*fn)(void *), void *fn_data)
{
	return 0;
}

int CService::RegisterStopCallBackHandler(int (*fn)(void *), void *fn_data)
{
	return 0;
}

bool CService::InstallDriver()
{
    return false;
}

bool CService::UninstallDriver()
{
    return false;
}

bool CService::isDriverInstalled() const
{
    return false;
}

bool CService::startClusterMonitor()
{
    return false;
}

bool CService::stopClusterMonitor()
{
    return false;
}

bool CService::startVolumeMonitor()
{
    return false;
}

bool CService::stopVolumeMonitor()
{
    return false;
}

void CService::ProcessVolumeMonitorSignal(void* param)
{
    //m_VolumeMonitorSignal(param);
}

sigslot::signal1<void*>& CService::getVolumeMonitorSignal()
{
    return m_VolumeMonitorSignal;
}

void CService::ProcessBitlockerMonitorSignal(void *param)
{
}

sigslot::signal1<void*>& CService::getBitlockerMonitorSignal()
{
    return m_BitlockerMonitorSignal;
}