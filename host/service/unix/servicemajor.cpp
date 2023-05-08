#ifndef SV_WINDOWS

#include "servicemajor.h"
#include "logger.h"
#include "host.h"
#include <dlfcn.h>

UnixService::UnixService() : service_started(false)
{
}

UnixService::~UnixService()
{
}

int UnixService::start_daemon()
{
    //
    // PR # 6107
    // Issue: svagents stopped local logging after logging
    // for first few message.
    // The last message that was logged was
    // "UnixService::run_startup_code"
    // The issue got in when we moved the local logging initialization code 
    // much ahead in  the main before the first debugprintf gets called (see PR#5519).
    // on unix, svagents calls ACE::daemonize in run_startup_code 
    // to runs service as deamon. ACE::ACE::daemonize was invoked with option
    // to close all handles. As a result even local log file handle was closed.
    // This was not a issue earlier because local logging was being initialized 
    // after it was setup as deamon. But it had other problems, that debugprintfs
    // were being called even without opening the local log.
    // This issue has been fixed as follows:
    //  1. First close all handles in main
    //  2. Setup local logging in main before first debugprintf gets invoked
    //  3. Start as deamon but do not close handles
    //

    // step 3 for PR #6107 : deamonize but do not close the I/O handles

	ACE::daemonize( ACE_TEXT ("/"), 0 /* do not close handles */ );
	service_started = true;
	//this->run_code();
	return 0;
}

int UnixService::install_service()
{
	return start_daemon();
}

int UnixService::start_service()
{
	return start_daemon();
}

int UnixService::run_startup_code()
{
	int status = -1;

    DebugPrintf( "UnixService::run_startup_code\n" );

	(void)start_daemon();
    DebugPrintf( "UnixService::run_startup_code 2\n" );
   //No startup code to run, return always -1.
   if (startfn != NULL) 
   {
	status = (*startfn)(startfn_data);
   }
    DebugPrintf( "UnixService::run_startup_code 3\n" );
   return status;
}

bool UnixService::isInstalled()
{
	return service_started;
}

int UnixService::RegisterStartCallBackHandler(int (*fn)(void *), void *fn_data)
{
	startfn = fn;
	startfn_data = fn_data;
	return 0;
}

int UnixService::RegisterStopCallBackHandler(int (*fn)(void *), void *fn_data)
{
	stopfn = fn;
	stopfn_data = fn_data;
	return 0;
}

/*
int UnixService::RegisterServiceEventHandler(int (*fn)(signed long,void *), void *data)
{
	eventhandlerfn = fn;
	eventhandlerfn_data = data;
	return 0;
}
*/

bool UnixService::InstallDriver()
{
    bool bInstalledDriver = false;
    return( bInstalledDriver );
}

bool UnixService::UninstallDriver()
{
    if ( !isDriverInstalled() )
        return false;

	bool bUninstalledDriver = false;
    return( bUninstalledDriver );
}

bool UnixService::startVolumeMonitor()
{
	bool rv = true;	

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);	

	m_VolumeMonitor.reset(new VolumeMonitorTask());
	
	m_VolumeMonitor->getVolumeMonitorSignal().connect(this,&UnixService::ProcessVolumeMonitorSignal);
	if( -1 == m_VolumeMonitor->open()) 
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED VolumeMonitorTask::open: %d \n", ACE_OS::last_error());
		rv = false;		
	}	
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool UnixService::stopVolumeMonitor()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	if(m_VolumeMonitor.get())
	{
		ACE_Thread_Manager::instance()->cancel_task(m_VolumeMonitor.get());
		m_VolumeMonitor->wait();
	}
	DebugPrintf(SV_LOG_DEBUG,"Volume Monitoring thread exited \n");
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return true;
}

bool UnixService::VolumeAvailableForNode(std::string const & volumeName)
{
    ACE_stat statbuf = {0};
	// PR#10815: Long Path support
    return (0 == sv_stat(getLongPathName(volumeName.c_str()).c_str(), &statbuf));
}

bool UnixService::EnableResumeFilteringForOfflineVolume(std::string const & volumeName)
{
    DebugPrintf(SV_LOG_DEBUG, "Enabling resume filtering for %s\n", volumeName.c_str());

    ACE_HANDLE handle = ACE_INVALID_HANDLE ;
    std::string volname =  volumeName;
    std::string dirpath, filepath;
    char disablefiltering = '0';

    std::replace( volname.begin(), volname.end(), '/', '_' );
    dirpath = "/etc/vxagent/involflt/";
    dirpath += volname;
    if(SVMakeSureDirectoryPathExists(dirpath.c_str()).failed())
    {
        DebugPrintf(SV_LOG_ERROR,"%s : Failed to create directory %s.\n", FUNCTION_NAME, dirpath.c_str());
        return false;
    }

    filepath = dirpath;
    filepath += "/VolumeFilteringDisabled";

	// PR#10815: Long Path support
    handle = ACE_OS::open( getLongPathName(filepath.c_str()).c_str(),O_WRONLY | O_CREAT );
    if(ACE_INVALID_HANDLE == handle)
    {
        DebugPrintf(SV_LOG_ERROR,
            "Function %s @LINE %d in FILE %s :failed to open %s \n", 
            FUNCTION_NAME, LINE_NO, FILE_NAME, filepath.c_str());
        return false;
    }

    if(sizeof(char) != ACE_OS::write(handle, &disablefiltering, sizeof(char)))
    {
        DebugPrintf(SV_LOG_ERROR,
            "Function %s @LINE %d in FILE %s :failed to write to %s \n", 
            FUNCTION_NAME, LINE_NO, FILE_NAME, filepath.c_str());
        ACE_OS::close(handle);
        return false;
    }

    ACE_OS::close(handle);

    return true;
}


void UnixService::ProcessVolumeMonitorSignal(void* param)
{
	m_VolumeMonitorSignal(param);            //Cservice signal
}

/*
 * FUNCTION NAME : DeleteAgentQuitEventFiles
 *
 * DESCRIPTION : 
 *  If an agent process crashes or is terminated, memory-mapped files created
 *  to implement the quit event are left behind in a random state (unix only).
 *  We delete these files to prevent future processes from re-using the files.
 *
 * INPUT PARAMETERS : filename - quitevent file name 
 *
 * OUTPUT PARAMETERS :  none
 *
 * NOTES : 
 *
 * return value : none
 *
*/


void DeleteAgentQuitEventFiles( std::string const& filename ) 
{
	// PR#10815: Long Path support
    (void) ACE_OS::shm_unlink( getLongPathName(filename.c_str()).c_str() );
}

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
template class ACE_Singleton<UnixService, ACE_Mutex>;
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
#pragma instantiate ACE_Singleton<UnixService, ACE_Mutex>
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */


//stub funtions for unix 
bool UnixService::InitializeJobObject()
{
    return true;
}

void UnixService::closeJobObjectHandle()
{
}

#endif /* !SV_WINDOWS */
