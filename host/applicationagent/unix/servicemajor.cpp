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

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
template class ACE_Singleton<UnixService, ACE_Mutex>;
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
#pragma instantiate ACE_Singleton<UnixService, ACE_Mutex>
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

#endif /* !SV_WINDOWS */
