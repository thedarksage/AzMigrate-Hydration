//                                       
// Copyright (c) 2008 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : cdpmgr.cpp
// 
// Description: 
//

#include <sstream>
#include <fstream>


#include "cdpmgr.h"
#include "cdputil.h"
#include "cdpplatform.h"

#include "localconfigurator.h"
#include "configurator2.h"
#include "configwrapper.h"
#include "inmageex.h"
#include "VacpUtil.h"

#include "logger.h"

#include "HandlerCurl.h"
#include "VsnapUser.h"
#include "portablehelpersmajor.h"
#include "inmalertdefs.h"

using namespace std;

Configurator* TheConfigurator = 0; // singleton

// message types from main thread to retention policy manager tasks
// RM_TIMEOUT: retentionmgr has run for its timeslice.
// RM_QUIT: service quit request
// RM_DELETE: pair deletion
// RM_DISABLED: cdp disabled
// RM_CHANGE: cdp policy change notification
// RM_MOVECDPSTORE: cdp store location is changed

static int const RM_STOP            = 0x2001;
static int const RM_TIMEOUT         = 0x2002;
static int const RM_QUIT            = 0x2003;

// message priorities
static int const RM_STOP_PRIORITY            = 0x01;
static int const RM_TIMEOUT_PRIORITY         = 0x02;
static int const RM_QUIT_PRIORITY            = 0x03;

// message dequeue status
static int const RM_MSG_SUCCESS = 0;
static int const RM_MSG_NONE    = 1;
static int const RM_MSG_FAILURE = 2;

// how long to wait for a message
static int const MsgWaitTimeInSeconds = 60;
static unsigned int IdleWaitTimeInSeconds = 60;

// delay between two successive attempts on encountering error
static unsigned int RetryDelayInSeconds = 30;

// max. no of retry attempts
static unsigned int MaxRetryAttempts = 10;


/*
* FUNCTION NAME :  CDPMgr::CDPMgr
*
* DESCRIPTION : CDPMgr constructor 
*
* INPUT PARAMETERS : argc - no. of arguments
*                    argv - arguments based on operation
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/

CDPMgr::CDPMgr(int argc, char ** argv)
:m_argc(argc), m_argv(argv)
{
	m_bconfig = false;
	m_blocalLog = false;  
#ifdef SV_UNIX
	set_resource_limits();
#endif

}

/*
* FUNCTION NAME :  CDPMgr::~CDPMgr
*
* DESCRIPTION : CDPMgr destructor 
*
*
* INPUT PARAMETERS : none
*                    
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
CDPMgr::~CDPMgr()
{

}

/*
* FUNCTION NAME :  CDPMgr::init
*
* DESCRIPTION :  
*               initialize quit event
*               initialize local logging
*               intiialize configurator
*
* INPUT PARAMETERS : none
*                    
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPMgr::init()
{
	//
	// TODO: CDPMgr is generally invoked by svagents
	// it can be invoked from command line, need to identify the
	// same and set the caller appropriately
	//
	bool callerCli = false;
	bool rv = true;

	try
	{
		do
		{

			if(!initializeTal())
			{
				rv = false;
				break;
			}

			MaskRequiredSignals();
			if(!SetupLocalLogging())
			{
				rv = false;
				break;
			}

			if (!CDPUtil::InitQuitEvent(callerCli))
			{
				rv = false;
				break;
			}

			if(AnotherInstanceRunning())
			{
				rv = false;
				//AnotherInstance is running ,so wait for 5 mins to quit this process,so that immediate launch of same processs can be avoided
				CDPUtil::QuitRequested(300);
				break;
			}

			if(!InitializeConfigurator())
			{
				rv = false;
				break;
			}

		} while(0);
	}

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::uninit
*
* DESCRIPTION :  
*               stop configurator
*               stop local logging
*               destroy quit event
*
* INPUT PARAMETERS : none
*                    
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPMgr::uninit()
{
	StopConfigurator();
	CDPUtil::UnInitQuitEvent();
	m_cdplock.reset();
	StopLocalLogging();
	tal::GlobalShutdown();
	return true;
}

/*
* FUNCTION NAME :  CDPMgr::initializeTal
*
* DESCRIPTION : initialize transport layer 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPMgr::initializeTal()
{
	bool rv = true;

	try 
	{
		tal::GlobalInitialize();
		LocalConfigurator localConfigurator;

		//
		// PR # 6337
		// set the curl_verbose option based on settings in 
		// drscout.conf
		//
		tal::set_curl_verbose(localConfigurator.get_curl_verbose());

		Curl_setsendwindowsize(localConfigurator.getTcpSendWindowSize());
		Curl_setrecvwindowsize(localConfigurator.getTcpRecvWindowSize());
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		std::cerr << FUNCTION_NAME << " encountered exception " << ce.what() << "\n";
	}
	catch( exception const& e )
	{
		rv = false;
		std::cerr << FUNCTION_NAME << " encountered exception " << e.what() << "\n";
	}
	catch ( ... )
	{
		rv = false;
		std::cerr << FUNCTION_NAME << " encountered unknown exception.\n";
	}

	return rv;
}


/*
* FUNCTION NAME :  CDPMgr::SetupLocalLogging
*
* DESCRIPTION : get the local logging settings from configurator 
*               and enable local logging
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPMgr::SetupLocalLogging()
{
	bool rv = false;

	try 
	{

		LocalConfigurator localconfigurator;
		std::string sLogFileName = localconfigurator.getLogPathname()+ "CDPMgr.log";
		SetLogFileName( sLogFileName.c_str() );
		SetLogLevel( localconfigurator.getLogLevel() );		
        SetLogHttpIpAddress(GetCxIpAddress().c_str());
		SetLogHttpPort( localconfigurator.getHttp().port );
		SetLogHostId( localconfigurator.getHostId().c_str() );
		SetLogHttpsOption(localconfigurator.IsHttps());

		//
		// 4.3: remoteloglevel and sysloglevel were added
		//
		SetLogRemoteLogLevel( localconfigurator.getRemoteLogLevel() );
		SetSerializeType( localconfigurator.getSerializerType() ) ;
		SetDaemonMode();
		m_blocalLog = true;
		rv = true;

	}

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	if(!rv)
	{
		std::cerr << "Local Logging initialization failed.\n";
		m_blocalLog = false;
	}

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::AnotherInstanceRunning
*
* DESCRIPTION :  is another instance of CDPMgr already running
*
*
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true if another instance is running,false otherwise
*
*/
bool CDPMgr::AnotherInstanceRunning()
{
	bool rv = false;

	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

		m_cdplock.reset(new CDPLock("CDPMgr.lck"));
		if(m_cdplock -> try_acquire())
		{
			rv = false;
		} else
		{
			DebugPrintf(SV_LOG_ERROR,"An another instance of %s is already running.\n",
				m_argv[0]);
			CDPUtil::QuitRequested(60);
			rv = true;
		}

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	}

	catch ( ContextualException& ce )
	{

		rv = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{

		rv = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{

		rv = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::InitializeConfigurator
*
* DESCRIPTION : initialize cofigurator to fetch initialsettings from 
*               local persistent store.
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool CDPMgr::InitializeConfigurator()
{
	bool rv = false;

	do
	{
		try 
		{
			LocalConfigurator lConfigurator ;
			m_configurator = NULL;
			std::string configcachepath ;
			ESERIALIZE_TYPE type = lConfigurator.getSerializerType();

			if(!::InitializeConfigurator(&m_configurator, USE_ONLY_CACHE_SETTINGS,type, configcachepath))
			{
				rv = false;
				break;
			}

			m_configurator->Start();
			m_bconfig = true;
			rv = true;
		}

		catch ( ContextualException& ce )
		{
			rv = false;
			DebugPrintf(SV_LOG_ERROR, 
				"\n%s encountered exception %s.\n",
				FUNCTION_NAME, ce.what());
		}
		catch( exception const& e )
		{
			rv = false;
			DebugPrintf(SV_LOG_ERROR, 
				"\n%s encountered exception %s.\n",
				FUNCTION_NAME, e.what());
		}
		catch ( ... )
		{
			rv = false;
			DebugPrintf(SV_LOG_ERROR, 
				"\n%s encountered unknown exception.\n",
				FUNCTION_NAME);
		}

	} while(0);

	if(!rv)
	{
		DebugPrintf(SV_LOG_INFO, 
			"Replication pair information is not available.\n"
			"CDPMgr cannot run now. please try again\n");
		m_bconfig = false;
	}

	return rv;
}


/*
* FUNCTION NAME :  CDPMgr::StopConfigurator
*
* DESCRIPTION : stop cofigurator 
*              
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool CDPMgr::StopConfigurator()
{
	try
	{
		if(m_configurator && m_bconfig)
		{
			m_configurator ->Stop();
			m_bconfig = false;
			m_configurator = NULL;
		}
	}

	catch ( ... )
	{
		// we are exiting, let's ignore all exceptions
	}

	return true;
}

/*
* FUNCTION NAME :  CDPMgr::StopLocalLogging
*
* DESCRIPTION : close local log file
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPMgr::StopLocalLogging()
{
	try
	{
		CloseDebug();
		m_blocalLog = false;
	} catch ( ... )
	{
		// we are exiting, let's just ignore exceptions
	}

	return true;
}


/*
* FUNCTION NAME :  CDPMgr::run
*
* DESCRIPTION : parse the command line and carry out appropriate tasks
*
*
* INPUT PARAMETERS : none
*                    
*
* OUTPUT PARAMETERS :  none
*
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPMgr::run()
{
	bool rv = true;


	if(!init())
		return false;

	try
	{
		bool quit = false;
		LocalConfigurator localconfigurator;
		SV_INT timeSlice = localconfigurator.getCDPMgrExitTime();

		ACE_Time_Value startTime = ACE_OS::gettimeofday();
		ACE_Time_Value currentTime = startTime;
		do
		{
			if(!StartReporterTask())
			{
				// unhandled failure, let's us request for a safe shutdown
				DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StartReporterTask\n",FUNCTION_NAME);
				CDPUtil::SignalQuit();
				rv = false;
				break;
			}

			while(true)
			{
				ConfigChangeCallback(m_configurator->getInitialSettings());

				// quit requested by service
				quit = CDPUtil::QuitRequested(IdleWaitTimeInSeconds);
				if(quit)
					break;

				// time slice expired

				// Bug #9034
				// To take care when the system time moves backward
				// (currenttime < starttime)

				currentTime = ACE_OS::gettimeofday();
				if( (currentTime.sec() - startTime.sec() > timeSlice)
					|| (currentTime.sec() < startTime.sec()))
					break;
			}
		}while(false);

		// on timeout, we will send a timeout notification to all tasks
		// and wait for them to exit gracefully ie let them finish their current task
		// in between, if we get a quit request, we will send stop request to all the tasks

		// note: do not remove the below line, removing triggers MSVC internal compiler erro
		quit = CDPUtil::QuitRequested(0);
		NotifyTimeOut();
		while(!quit)
		{
			if(AllTasksExited())
			{
				break;
			}
			quit = CDPUtil::QuitRequested(IdleWaitTimeInSeconds);
		}

	} 

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	StopAllTasks();

	if(!uninit())
		return false;

	return rv;
}

bool CDPMgr::StartReporterTask()
{
	bool rv=true;
	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

		m_reporterTaskPtr.reset(new ReporterTask(this,m_configurator,&m_ThreadManager));
		if( -1 == m_reporterTaskPtr ->open())
		{
			DebugPrintf(SV_LOG_ERROR, "%s encountered error (%d) while creating ReporterTask\n",
				FUNCTION_NAME,  ACE_OS::last_error());
			rv = false;
		}

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR,
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}
	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::getCdpSettings
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
CDP_SETTINGS CDPMgr::getCdpSettings(const std::string & tgtvolname,bool current)
{
	bool rv = true;

	//AutoGuard lock(m_lockSettings); 

	if(current)
	{
		CDPSETTINGS_MAP::iterator iter = m_cdpsettings.find(tgtvolname);
		if(iter != m_cdpsettings.end())
		{
			return iter -> second;
		}
	} else
	{
		CDPSETTINGS_MAP::iterator iter = m_oldcdpsettings.find(tgtvolname);
		if(iter != m_oldcdpsettings.end())
		{
			return iter -> second;
		}
	}

	return CDP_SETTINGS();
} 

/*
* FUNCTION NAME :  CDPMgr::CopyCDPSettings
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
bool CDPMgr::CopyCDPSettings(const CDPSETTINGS_MAP & cdp_settings, bool current)
{
	bool rv = true;

	try
	{
		//AutoGuard lock(m_lockSettings);
		if(current)
			m_cdpsettings = cdp_settings;
		else
			m_oldcdpsettings = cdp_settings;
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}



/*
* FUNCTION NAME :  CDPMgr::ConfigChangeCallback
*
* DESCRIPTION : callback on change to initial settings
*               1. send quit signal to tasks for deleted replication pairs
*               2. send change signal to existing pairs
*               3. start worker task for new replication pairs
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/

void CDPMgr::ConfigChangeCallback(InitialSettings settings)
{
	bool rv = true;

	try
	{
		do
		{
			DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

			AutoGuard lock(m_lockTasks);

			/*
			0. reap dead tasks
			1. run schedule jobs if policymgr is not running for the corresponding retention disk
			a. cdp disable on volume
			b. move cdp store on volume
			c. pair deletion message
			2. create policymgr thread per retention volume
			3. send change notification
			a. CDPSTOP messages to all policy enforcer thread
			*/

			if(!ReapDeadTasks())
			{
				// waiting for the dead tasks failed. let's us request for a safe shutdown
				// otherwise may causes high memory usage
				DebugPrintf(SV_LOG_ERROR,"%s encountered failure in ReapDeadTasks\n",FUNCTION_NAME);
				CDPUtil::SignalQuit();
				rv = false;
				break;
			}

			if(!CopyCDPSettings(settings.cdpSettings))
			{
				// unknown failure. let's us request for a safe shutdown
				DebugPrintf(SV_LOG_ERROR,"%s encountered failure in PersistNewSettings\n",FUNCTION_NAME);
				CDPUtil::SignalQuit();
				rv = false;
				break;
			}

			if(!StartScheduleJobs(settings))
			{
				DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StartScheduleJobs\n",FUNCTION_NAME);
				CDPUtil::SignalQuit();
				rv = false;
				break;
			}

			if(!StartNewPolicyMgrs(settings))
			{
				// unhandled failure, let's us request for a safe shutdown
				DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StartNewPolicyMgrs\n",FUNCTION_NAME);
				CDPUtil::SignalQuit();
				rv = false;
				break;
			}

			// update the old settings as well
			if(!CopyCDPSettings(settings.cdpSettings,false))
			{
				// unknown failure. let's us request for a safe shutdown
				DebugPrintf(SV_LOG_ERROR,"%s encountered failure in PersistNewSettings\n",FUNCTION_NAME);
				CDPUtil::SignalQuit();
				rv = false;
				break;
			}

			DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

		} while (0);
	}

	catch ( ContextualException& ce )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}
}

/*
* FUNCTION NAME :  CDPMgr::StopAllTasks
*
* DESCRIPTION : 
*     Main thread is exiting either due to timeslice completion or
*     recieved quit request from service.
*     1. send quit signal to all tasks
*     2. wait till all exit
*    
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPMgr::StopAllTasks()
{
	bool rv = true;

	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

		AutoGuard lock(m_lockTasks);
		PolicyMgrTasks_t::iterator iter = m_policymgrtasks.begin();
		for( ; iter != m_policymgrtasks.end(); ++iter)
		{
			PolicyMgrTaskPtr taskPtr = (*iter).second;
			taskPtr -> PostMsg(RM_QUIT, RM_QUIT_PRIORITY);
		}
		ScheduleTasks_t::iterator scheduleIter = m_scheduleTasks.begin();
		for( ; scheduleIter != m_scheduleTasks.end(); ++scheduleIter)
		{
			ScheduleTaskPtr scheduleTaskPtr = (*scheduleIter).second;
			if(scheduleTaskPtr->isStart())
				scheduleTaskPtr -> PostMsg(RM_QUIT, RM_QUIT_PRIORITY);
		}
		if(m_reporterTaskPtr)
		{
			m_reporterTaskPtr->PostMsg(RM_QUIT, RM_QUIT_PRIORITY);
		}

		m_ThreadManager.wait();

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	} 

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::NotifyTimeOut
*
* DESCRIPTION : 
*     Main thread has completed its timeslice 
*     1. send timeout notification to all tasks
*    
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPMgr::NotifyTimeOut()
{
	bool rv = true;

	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

		AutoGuard lock(m_lockTasks);

		PolicyMgrTasks_t::iterator iter = m_policymgrtasks.begin();
		for( ; iter != m_policymgrtasks.end(); ++iter)
		{
			PolicyMgrTaskPtr taskPtr = (*iter).second;
			taskPtr -> PostMsg(RM_TIMEOUT, RM_TIMEOUT_PRIORITY);
		}

		ScheduleTasks_t::iterator scheduleIter = m_scheduleTasks.begin();
		for( ; scheduleIter != m_scheduleTasks.end(); ++scheduleIter)
		{
			ScheduleTaskPtr scheduleTaskPtr = (*scheduleIter).second;
			if(scheduleTaskPtr->isStart())
				scheduleTaskPtr -> PostMsg(RM_TIMEOUT, RM_TIMEOUT_PRIORITY);
		}

		if(m_reporterTaskPtr)
		{
			m_reporterTaskPtr->PostMsg(RM_TIMEOUT, RM_TIMEOUT_PRIORITY);
		}

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	} 

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::AllTasksExited
*
* DESCRIPTION : 
*     Main thread has completed its timeslice and 
*     waiting for all tasks 
*     1. send timeout notification to all tasks
*    
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPMgr::AllTasksExited()
{
	bool rv = true;

	try
	{
		do
		{

			DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

			AutoGuard lock(m_lockTasks);
			if(m_reporterTaskPtr && !m_reporterTaskPtr->isDead())
			{
				rv=false;
				break;
			}

			PolicyMgrTasks_t::iterator iter = m_policymgrtasks.begin();
			if(iter == m_policymgrtasks.end())
			{
				rv = true;
				break;
			}

			for( ; iter != m_policymgrtasks.end(); ++iter)
			{
				PolicyMgrTaskPtr taskPtr = (*iter).second;
				if(!taskPtr ->isDead())
				{
					rv = false;
					break;
				}
			}

			if(!rv)
				break;

			ScheduleTasks_t::iterator scheduleIter = m_scheduleTasks.begin();
			if(scheduleIter == m_scheduleTasks.end())
			{
				rv = true;
				break;
			}
			for( ; scheduleIter != m_scheduleTasks.end(); ++scheduleIter)
			{
				ScheduleTaskPtr scheduleTaskPtr = (*scheduleIter).second;
				if((!scheduleTaskPtr ->isDead()) && scheduleTaskPtr->isStart())
				{
					rv = false;
					break;
				}                
			}

			DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

		} while (0);
	} 

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::ReapDeadTasks
*
* DESCRIPTION : 
*     Wait for the dead tasks 
*    
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPMgr::ReapDeadTasks()
{
	bool rv = true;

	try
	{
		do
		{

			DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
			if(m_reporterTaskPtr && m_reporterTaskPtr->isDead())
			{
				m_ThreadManager.wait_task(m_reporterTaskPtr.get());
			}

			std::list<std::string> deadtasks;

			PolicyMgrTasks_t::iterator iter = m_policymgrtasks.begin();
			for( ; iter != m_policymgrtasks.end(); ++iter)
			{
				PolicyMgrTaskPtr taskPtr = (*iter).second;
				if(taskPtr ->isDead())
				{
					m_ThreadManager.wait_task(taskPtr.get());
					deadtasks.push_back((*iter).first);
				}
			}

			std::list<std::string>::iterator deadtasks_iter = deadtasks.begin(); 
			for(; deadtasks_iter != deadtasks.end(); ++deadtasks_iter)
			{
				m_policymgrtasks.erase(*deadtasks_iter);
			}

			deadtasks.clear();
			ScheduleTasks_t::iterator scheduleIter = m_scheduleTasks.begin();
			for( ; scheduleIter != m_scheduleTasks.end(); ++scheduleIter)
			{
				ScheduleTaskPtr scheduleTaskPtr = (*scheduleIter).second;
				if(scheduleTaskPtr ->isDead())
				{
					m_ThreadManager.wait_task(scheduleTaskPtr.get());
					deadtasks.push_back((*scheduleIter).first);
				}
			}
			deadtasks_iter = deadtasks.begin(); 
			for(; deadtasks_iter != deadtasks.end(); ++deadtasks_iter)
			{
				m_scheduleTasks.erase(*deadtasks_iter);
			}

			DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

		} while (0);
	} 

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}

bool CDPMgr::StartScheduleJobs(InitialSettings & settings)
{
	bool rv = true;

	try
	{
		do
		{

			DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

			HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = 
				settings.hostVolumeSettings.volumeGroups;
			HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter = 
				volumeGroups.begin();
			for(; vgiter != volumeGroups.end(); ++vgiter)
			{
				VOLUME_GROUP_SETTINGS vg = *vgiter;
				if(TARGET != vg.direction)
					continue;

				VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter =vg.volumes.begin();

				for (;volumeIter != vg.volumes.end(); volumeIter++)
				{
					std::string volname = volumeIter -> first;
					VOLUME_SETTINGS volsettings = (*volumeIter).second;
					std::string db_dir;
					CdpDelMsg delmsg;
					CdpMoveMsg movemsg;
					CDP_SETTINGS cdp_settings = getCdpSettings(volname); 
					CDP_SETTINGS old_cdp_settings = getCdpSettings(volname,false);
					ScheduleTask * schedulejob = getScheduleTask(volname);

					if(CDPEnabled(cdp_settings))
						db_dir = cdp_settings.CatalogueDir();

					if((!CDPEnabled(cdp_settings)) && (CDPEnabled(old_cdp_settings)))
					{
						if(0 == schedulejob)
						{
							CdpDisableMsg * disablemsg = (CdpDisableMsg *)new CdpDisableMsg;
							disablemsg->dbname = old_cdp_settings.Catalogue();

							ScheduleTaskPtr scheduleTask(new ScheduleTask(this, 
								m_configurator, 
								volname,
								disablemsg,
								CDPDISABLEACTION,
								old_cdp_settings.Catalogue(),
								&m_ThreadManager));

							m_scheduleTasks.insert(m_scheduleTasks.end(),ScheduleTaskPair_t(volname,scheduleTask));
							schedulejob = scheduleTask.get();
						}
						db_dir = old_cdp_settings.CatalogueDir();
					}
					else if(isPairDeleted(volsettings,volname,delmsg))
					{
						if(0 == schedulejob)
						{
							CdpDelMsg * msg = (CdpDelMsg *)new CdpDelMsg;
							msg->vsnapcleanup = delmsg.vsnapcleanup;
							msg->cdpcleanup = delmsg.cdpcleanup;

							ScheduleTaskPtr scheduleTask(new ScheduleTask(this, 
								m_configurator, 
								volname,
								msg,
								CDPPAIRDELETEACTION,
								cdp_settings.Catalogue(),
								&m_ThreadManager));

							m_scheduleTasks.insert(m_scheduleTasks.end(),ScheduleTaskPair_t(volname,scheduleTask));
							schedulejob = scheduleTask.get();
						}
					}
					else if((isCDPMoved(volsettings,volname,movemsg)))
					{
						if(0 == schedulejob)
						{
							CdpMoveMsg * cdpmsg = (CdpMoveMsg *)new CdpMoveMsg;
							cdpmsg->newlocation = movemsg.newlocation;
							cdpmsg->oldlocation = movemsg.oldlocation;

							ScheduleTaskPtr scheduleTask(new ScheduleTask(this, 
								m_configurator, 
								volname,
								cdpmsg,
								CDPMOVEACTION,
								cdp_settings.Catalogue(),
								&m_ThreadManager));

							m_scheduleTasks.insert(m_scheduleTasks.end(),ScheduleTaskPair_t(volname,scheduleTask));
							schedulejob = scheduleTask.get();
						}
					}
					else if((!CDPEnabled(cdp_settings)) && (!CDPEnabled(old_cdp_settings))) 
					{
						if(0 != schedulejob)
						{
							std::string dbname = schedulejob->getDbName();
							if(!dbname.empty())
								db_dir = dirname_r(dbname.c_str());
						}
					}

					if(0 == schedulejob)
						continue;

					if(schedulejob->isStart())
						continue;

					PolicyMgrTask * policymgr = 0;

					if(!db_dir.empty())
					{
						std::string retentionRoot;
						std::string retentionDisk;
						if(!GetVolumeRootPath(db_dir, retentionRoot))
						{
							DebugPrintf(SV_LOG_ERROR, "Unable to determine the root of %s\n", db_dir.c_str());
							continue;
						}
						
						if(!GetDeviceNameForMountPt(retentionRoot, retentionDisk))
						{
							DebugPrintf(SV_LOG_ERROR, "Unable to get the devicename for %s\n", retentionRoot.c_str());
							continue;
						}

						FirstCharToUpperForWindows(retentionDisk);
						policymgr = getPolicyMgr(retentionDisk);
						if(0 != policymgr)
						{
							DebugPrintf(SV_LOG_DEBUG, 
								"Sending RM_STOP to the policymgr thread for retention disk %s for the volume %s\n", 
								retentionDisk.c_str(), volname.c_str());
							policymgr->PostMsg(RM_STOP, RM_STOP_PRIORITY);
						}
					}

					if(0 == policymgr)
					{
						if(-1 == schedulejob->open())
						{
							DebugPrintf(SV_LOG_ERROR, 
								"%s encountered error (%d) while creating scheduleTask for volume %s\n", 
								FUNCTION_NAME,  ACE_OS::last_error(), volname.c_str());
							rv = false;
							continue;
						}
						schedulejob->MarkStart();                                                
					}
				}
			}
			DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

		} while (0);
	} 

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::StartNewPolicyMgrs
*
* DESCRIPTION : 
*    
*    
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/

bool CDPMgr::StartNewPolicyMgrs(InitialSettings & settings)
{
	bool rv = true;

	try
	{
		do
		{

			DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
			LocalConfigurator localconfigurator;
			bool allow_rootvolume_retention = localconfigurator.allowRootVolumeForRetention();

			HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = 
				settings.hostVolumeSettings.volumeGroups;
			HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter = 
				volumeGroups.begin();

			for(; vgiter != volumeGroups.end(); ++vgiter)
			{
				VOLUME_GROUP_SETTINGS vg = *vgiter;
				if(TARGET != vg.direction)
					continue;

				VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter =vg.volumes.begin();

				for (;volumeIter != vg.volumes.end(); volumeIter++)
				{
					std::string volname = volumeIter -> first;
					VOLUME_SETTINGS volsettings = (*volumeIter).second;
					CDP_SETTINGS cdp_settings = getCdpSettings(volname); 
					if(!CDPEnabled(cdp_settings))
						continue;
					if(volsettings.pairState == VOLUME_SETTINGS::DELETE_PENDING) 
						continue;
					if(((volsettings.pairState == VOLUME_SETTINGS::PAUSE) || (volsettings.pairState == VOLUME_SETTINGS::PAUSE_PENDING))
						&& (volsettings.maintenance_action.find("move_retention=yes;")!= std::string::npos))
						continue;

					std::string db_dir = cdp_settings.CatalogueDir();
					std::string retentionRoot;
					std::string retentionDisk;
					if(!GetVolumeRootPath(db_dir, retentionRoot))
					{
						DebugPrintf(SV_LOG_ERROR, "Unable to determine the root of %s\n", db_dir.c_str());
						continue;
					}
					if(!allow_rootvolume_retention)
					{
						if(retentionRoot == "/")
						{
							DebugPrintf(SV_LOG_DEBUG, "Retention %s is under mount point /. So not starting the retention task for /.\n",db_dir.c_str());
							continue;
						}
					}

					if(!GetDeviceNameForMountPt(retentionRoot, retentionDisk))
					{
						DebugPrintf(SV_LOG_ERROR, "Unable to get the devicename for %s\n", retentionRoot.c_str());
						continue;
					}

					FirstCharToUpperForWindows(retentionDisk);
					PolicyMgrTask * policymgr = getPolicyMgr(retentionDisk);

					if(0 == policymgr)
					{                                              
						// start the task
						PolicyMgrTaskPtr policyTask(new PolicyMgrTask(this, 
							m_configurator, 
							retentionDisk,
							retentionRoot,
							&m_ThreadManager)); 

						if( -1 == policyTask ->open()) 
						{
							DebugPrintf(SV_LOG_ERROR, 
								"%s encountered error (%d) while creating policyTask for volume %s\n", 
								FUNCTION_NAME,  ACE_OS::last_error(), volname.c_str());
							rv = false;
							continue;
						}

						m_policymgrtasks.insert(m_policymgrtasks.end(), 
							PolicyMgrTaskPair_t(retentionDisk,policyTask));

					}
				}
			}
			DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

		} while (0);
	} 

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::getPolicyMgr
*
* DESCRIPTION : 
*     1. send change signal to existing policy mgr tasks
*     
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
PolicyMgrTask * CDPMgr::getPolicyMgr(const std::string & volname)
{

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	PolicyMgrTask * policymgr = NULL;

	PolicyMgrTasks_t::iterator policymgrIter = m_policymgrtasks.find(volname);
	if(m_policymgrtasks.end() != policymgrIter)
	{
		PolicyMgrTaskPtr policymgrtaskPtr = (*policymgrIter).second;
		policymgr = policymgrtaskPtr.get();
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return policymgr;
}

ScheduleTask * CDPMgr::getScheduleTask(const std::string & volname)
{

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	ScheduleTask * scheduleTask = NULL;

	ScheduleTasks_t::iterator scheduletaskIter = m_scheduleTasks.find(volname);
	if(m_scheduleTasks.end() != scheduletaskIter)
	{
		ScheduleTaskPtr scheduletaskPtr = (*scheduletaskIter).second;
		scheduleTask = scheduletaskPtr.get();
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return scheduleTask;
}

/*
* FUNCTION NAME :  CDPMgr::CDPEnabled
*
* DESCRIPTION : 
*     1. send change signal to existing policy mgr tasks
*     
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPMgr::CDPEnabled(const CDP_SETTINGS & cdp_settings)
{
	return (cdp_settings.Status() == CDP_ENABLED);
}

/*
* FUNCTION NAME :  CDPMgr::CDPDisabled
*
* DESCRIPTION : 
*     1. send change signal to existing policy mgr tasks
*     
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPMgr::CDPDisabled(const CDP_SETTINGS & cdp_settings)
{
	return (cdp_settings.Status() == CDP_DISABLED);
}



/*
* FUNCTION NAME :  CDPMgr::cleanupPending
*
* DESCRIPTION : 
*     1. send change signal to existing policy mgr tasks
*     
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPMgr::cleanupPending(const CDP_SETTINGS & cdp_settings)
{
	std::string dbname = cdp_settings.Catalogue();
	ACE_stat statbuf = {0};
	// PR#10815: Long Path support
	return (0 == sv_stat(getLongPathName(dbname.c_str()).c_str(), &statbuf));
}

/*
* FUNCTION NAME :  CDPMgr::isPairDeleted
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPMgr::isPairDeleted(VOLUME_SETTINGS & volsettings, 
						   const std::string & volname, 
						   CdpDelMsg &msg)
{
	bool rv = false;


	string cleanup_action = volsettings.cleanup_action;
	rv = ((volsettings.pairState == VOLUME_SETTINGS::DELETE_PENDING)
		&&(cleanup_action.find("pending_action_folder_cleanup=no") != std::string::npos));

	if(rv)
	{
		// assign default values for cleanup actions
		msg.cdpcleanup = false;
		msg.vsnapcleanup = true;

		// pair is undergoing deletion and retention cleanup is requested
		if(cleanup_action.find("log_cleanup=yes;") 
			!= std::string::npos)
		{
			msg.cdpcleanup = true;
		}
	}    

	return rv;
}

/*
* FUNCTION NAME :  CDPMgr::isCDPMoved
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : InitialSettings
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPMgr::isCDPMoved(VOLUME_SETTINGS & volsettings, const std::string & volname, CdpMoveMsg &msg)
{
	bool rv = false;


	do
	{
		// we found the pair and it is paused for move retention
		if((volsettings.pairState == VOLUME_SETTINGS::PAUSE) 
			&&(volsettings.maintenance_action.find("move_retention=yes;")
			!= std::string::npos))
		{

			std::map<string,string> moverequest_params;
			std::string moverequest = volsettings.maintenance_action;
			svector_t svpair = CDPUtil::split(moverequest,";");

			for(size_t i = 0; i < svpair.size(); ++i)
			{
				svector_t currarg = CDPUtil::split(svpair[i],"=");
				if (currarg.size() == 2)
				{
					CDPUtil::trim(currarg[1]);
					CDPUtil::trim(currarg[0]);
					moverequest_params[currarg[0]]=currarg[1];
				}
			}

			std::map<string,string>::iterator iter = 
				moverequest_params.find("move_retention_old_path");
			if((iter == moverequest_params.end()) || (iter->second.empty()))
			{
				DebugPrintf(SV_LOG_ERROR,
					"Retention move request for %s from cx does not contain required parameters.\n",
					volname.c_str());
				rv = false;
				break;
			}

			msg.oldlocation = iter ->second;

			iter = moverequest_params.find("move_retention_path");
			if((iter == moverequest_params.end()) || (iter->second.empty()))
			{
				DebugPrintf(SV_LOG_ERROR,
					"Retention move request for %s from cx does not contain required parameters.\n",
					volname.c_str());
				rv = false;
				break;
			}

			msg.newlocation = iter -> second;
			rv = true;
			break;
		}

	} while (0);

	return rv;
}




// PolicyMgrTask

/*
* FUNCTION NAME :  PolicyMgrTask::open
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/

int PolicyMgrTask::open(void *args)
{
	return this->activate(THR_BOUND);
}

/*
* FUNCTION NAME :  PolicyMgrTask::close
*
* DESCRIPTION : 
*     
*      
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/

int PolicyMgrTask::close(u_long flags)
{
	return 0;
}


/*
* FUNCTION NAME :  PolicyMgrTask::PostMsg
*
* DESCRIPTION : 
*       
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
void PolicyMgrTask::PostMsg(int msgId, int priority)
{
	bool rv = true;

	try
	{
		ACE_Message_Block *mb = new ACE_Message_Block(0, msgId);
		mb->msg_priority(priority);
		msg_queue()->enqueue_prio(mb);
	} 

	catch ( ContextualException& ce )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

}

/*
* FUNCTION NAME :  PolicyMgrTask::svc
*
* DESCRIPTION : 
*       
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/

int PolicyMgrTask::svc()
{
	bool rv = true;
	bool stalefile_cleanup_done = false;
	bool unusable_recoverypts_cleanup_done = false;


	try
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

		do
		{

			FirstCharToUpperForWindows(m_retentionvolname);

			while (true)
			{
				if(QuitRequested() || TimeSliceExpired())
					break;

				ProcessNextMessage();

				if(QuitRequested() || TimeSliceExpired())
					break;

				if(!getCdpSettings())
				{
					// unhandled failure, let's us request for a safe shutdown
					DebugPrintf(SV_LOG_ERROR,"%s %s encountered failure in getCdpSettings\n",
						FUNCTION_NAME, m_retentionmtpt.c_str());
					CDPUtil::SignalQuit();
					rv = false;
					break;
				}

				if(m_policies.empty())
					break;

                if(!stalefile_cleanup_done && m_delete_stalefiles)
				{
					PerformStaleFileCleanup();
					stalefile_cleanup_done = true;
				}

				if(!unusable_recoverypts_cleanup_done && m_delete_unusable_pts)
				{
					DeleteUnusableRecoveryPoints();
					unusable_recoverypts_cleanup_done = true;
				}

				ProcessPoliciesForRetentionVolume();
			}

		} while (0);

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());
	} 

	catch ( ContextualException& ce )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	m_policies.clear();
	m_cdptimeranges.clear();

	this -> MarkDead();
	return 0;
}

/*
* FUNCTION NAME :  PolicyMgrTask::getCdpSettings
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
bool PolicyMgrTask::getCdpSettings()
{
	bool rv = true;

	try
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

		do
		{

			InitialSettings initialSetting = m_configurator->getInitialSettings();
			CDPSETTINGS_MAP::iterator iter = initialSetting.cdpSettings.begin();
			if(m_initialsettings == initialSetting)
			{
				rv = true;
				break;
			}
			m_initialsettings = initialSetting;
			m_policies.clear();
			for(;iter != initialSetting.cdpSettings.end();++iter)
			{
				if((*iter).second.Status() != CDP_ENABLED)
				{
					DebugPrintf(SV_LOG_DEBUG, "Retention is disabled for the target volume %s\n", (*iter).first.c_str());
					continue;
				}
				if(isMoveRetention((*iter).first))
				{
					DebugPrintf(SV_LOG_DEBUG, "Move retention is going on for the target volume %s\n", (*iter).first.c_str());
					continue;
				}
				if(isPairDeleted((*iter).first))
				{
					DebugPrintf(SV_LOG_DEBUG, "Pair deletion is going on for the target volume %s\n", (*iter).first.c_str());
					continue;
				}

				if(isPairFlushAndHoldState((*iter).first))
				{
					DebugPrintf(SV_LOG_DEBUG, "Pair is in FLUSH and HOLD states %s\n", (*iter).first.c_str());
					continue;
				}

				if(!CDPUtil::validate_cdp_settings((*iter).first,(*iter).second))
				{
					DebugPrintf(SV_LOG_ERROR, "invalid cdp settings for %s.\n", (*iter).first.c_str());
					continue;
				}

				std::string db_dir = (*iter).second.CatalogueDir();
				std::string retentionRoot;
				std::string retentionDisk;
				if(!GetVolumeRootPath(db_dir, retentionRoot))
				{
					DebugPrintf(SV_LOG_ERROR, "Unable to determine the root of %s\n", db_dir.c_str());
					continue;
				}

				if(!GetDeviceNameForMountPt(retentionRoot, retentionDisk))
				{
					DebugPrintf(SV_LOG_ERROR, "Unable to get the devicename for %s\n", retentionRoot.c_str());
					continue;
				}

				FirstCharToUpperForWindows(retentionDisk);
				if(retentionDisk != m_retentionvolname)
				{
					continue;
				}

				DebugPrintf(SV_LOG_DEBUG, "Volume %s is set for policy enforcement\n", (*iter).first.c_str());
				m_policies.insert(make_pair((*iter).first,(*iter).second)); 

				if(m_cdptimeranges.find((*iter).first) == m_cdptimeranges.end())
				{
					CdpTimeRange timerange;
					m_cdptimeranges.insert(m_cdptimeranges.end(),CdpTimeRangePair_t((*iter).first,timerange));
				}
			}	
			LocalConfigurator localConfigurator;

			//m_cdpfreespace_trigger  = localConfigurator.getCdpFreeSpaceTrigger();
			m_cdpfreespacecheck_interval = localConfigurator.getCdpFreeSpaceCheckInterval();
			m_cdppolicycheck_interval = localConfigurator.getCdpPolicyCheckInterval();
			m_alertupdate_interval = localConfigurator.getEnforcerAlertInterval();
			m_cxupdates_interval = localConfigurator.getCxUpdateInterval();
			m_cx_cdpdiskusage_update_interval = localConfigurator.getCxCDPDiskUsageUpdateInterval();
			m_delete_pendingupdates = localConfigurator.CanDeleteAllCxCDPPendingUpdates();
			m_delete_pendingupdates_interval = localConfigurator.getDeleteAllCxCDPPendingUpdatesInterval();
			m_cx_update_per_target_volume = localConfigurator.getCDPMgrUpdateCxPerTargetVolume();
			m_cx_event_timerange_rec_per_batch = localConfigurator.getCDPMgrEventTimeRangeRecordsPerBatch();
			m_cx_send_updates_atonce = localConfigurator.getCDPMgrSendUpdatesAtOnce();           
			m_cdpreserved_space = m_policies.size() * localConfigurator.getSizeOfReservedRetentionSpace();
			m_delete_unusable_pts = localConfigurator.getCDPMgrDeleteUnusableRecoveryPoints();
			m_delete_stalefiles = localConfigurator.getCDPMgrDeleteStaleFiles();
			
			SetMinimumAvalableFreeSpace();
		} while (0);

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

	} catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	return rv;
}

/*
* FUNCTION NAME :  PolicyMgrTask::QuitRequested
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
bool PolicyMgrTask::QuitRequested(int seconds)
{
	try
	{
		if(m_bShouldQuit)
			return true;

		ACE_Message_Block *mb;

		ACE_Time_Value wait = ACE_OS::gettimeofday ();
		wait.sec (wait.sec () + seconds);
		if (-1 == this->msg_queue()->peek_dequeue_head(mb, &wait)) 
		{
			if (errno == EWOULDBLOCK)
			{
				m_bShouldQuit = false;
			} else
			{

				DebugPrintf(SV_LOG_ERROR,
					"\n%s encountered error (message queue closed) for %s\n",
					FUNCTION_NAME, m_retentionmtpt.c_str());

				// the message queue has been closed abruptly
				// let's us request for a safe shutdown

				CDPUtil::SignalQuit();
				m_bShouldQuit = true;
			}
		} else
		{
			if((RM_QUIT == mb->msg_type()) ||(RM_TIMEOUT == mb ->msg_type()))
			{
				m_bShouldQuit = true;
			}
		}
	}

	catch ( ContextualException& ce )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		m_bShouldQuit= true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		m_bShouldQuit= true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		m_bShouldQuit = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	return m_bShouldQuit;
}


/*
* FUNCTION NAME :  PolicyMgrTask::FetchNextMessage
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : 
*    RM_MSG_SUCCESS = success, message available
*    RM_MSG_NONE = timeout, no message
*    RM_MSG_FAILURE = failure
*
*/
SV_UINT PolicyMgrTask::FetchNextMessage(ACE_Message_Block ** mb, int seconds)
{
	SV_UINT rv = RM_MSG_SUCCESS;

	try
	{

		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

		ACE_Time_Value wait = ACE_OS::gettimeofday ();
		wait.sec (wait.sec () + seconds);
		if (-1 == this->msg_queue()->dequeue_head(*mb, &wait)) 
		{
			if (errno == EWOULDBLOCK)
			{
				rv = RM_MSG_NONE;
			} else
			{

				DebugPrintf(SV_LOG_ERROR,
					"\n%s encountered error (message queue closed) for %s\n",
					FUNCTION_NAME, m_retentionmtpt.c_str());

				// the message queue has been closed abruptly
				// let's us request for a safe shutdown

				CDPUtil::SignalQuit();
				rv = RM_MSG_FAILURE;
			}
		} else
		{
			rv = RM_MSG_SUCCESS;
		}

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	catch ( ContextualException& ce )
	{
		CDPUtil::SignalQuit();
		rv = RM_MSG_FAILURE;

		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		CDPUtil::SignalQuit();
		rv = RM_MSG_FAILURE;

		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		CDPUtil::SignalQuit();
		rv = RM_MSG_FAILURE;

		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	return rv;
}

/*
* FUNCTION NAME :  PolicyMgrTask::ProcessNextMessage
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
bool PolicyMgrTask::ProcessNextMessage()
{
	bool rv = true;

	try
	{
		do
		{

			DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

			SV_UINT msg_rv = RM_MSG_SUCCESS;
			ACE_Message_Block *mb = NULL;
			SV_INT delaytime = min(m_cdpfreespacecheck_interval,m_cdppolicycheck_interval);
			msg_rv = FetchNextMessage(&mb,delaytime);

			if(msg_rv == RM_MSG_FAILURE)
			{
				// unhandled failure, let's us request for a safe shutdown
				DebugPrintf(SV_LOG_ERROR, "%s %s encountered failure in FetchNextMessage\n",
					FUNCTION_NAME, m_retentionmtpt.c_str());
				CDPUtil::SignalQuit();
				m_bShouldQuit= true;
				rv = false;
				break;
			}

			if(msg_rv == RM_MSG_NONE)
			{
				// no message
				rv = true;
				break;
			}

			if(msg_rv == RM_MSG_SUCCESS)
			{

				switch(mb ->msg_type())
				{
				case RM_TIMEOUT:
					m_bTimeSliceExpired = true;
					mb ->release();
					rv = true;
					break;


				case RM_QUIT:
					m_bShouldQuit = true;
					mb ->release();
					rv = true;
					break;

				case RM_STOP:
					m_bShouldQuit = true;
					mb ->release();
					rv = true;
					break;      

				}

			} // msg_rv == RM_MSG_SUCCESS


			DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());
		} while (0);

	}

	catch ( ContextualException& ce )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		m_bShouldQuit= true;
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		m_bShouldQuit= true;
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		m_bShouldQuit = true;
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	return rv;
}

/*
* FUNCTION NAME :  PolicyMgrTask::Idle
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
void PolicyMgrTask::Idle(int seconds)
{
	(void)QuitRequested(seconds);
}

/*
* FUNCTION NAME :  PolicyMgrTask::initCDP
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
bool PolicyMgrTask::initCDP(const CDP_SETTINGS & cdpsetting)
{
	bool rv = true;

	try
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, cdpsetting.Catalogue().c_str());

		do
		{
			SVERROR rc = SVMakeSureDirectoryPathExists(cdpsetting.CatalogueDir().c_str());
			if(rc.failed())
			{                        
                CdpLogDirCreateFailAlert a(cdpsetting.CatalogueDir(), rc.toString());
				DebugPrintf(SV_LOG_ERROR, "%s", a.GetMessage().c_str());
				SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
				rv = false;
				break;
			}

#ifdef SV_WINDOWS
			LocalConfigurator localconfigurator;
			bool cdpCompressed = localconfigurator.CDPCompressionEnabled();
			if(cdpCompressed)
			{
				if(!EnableCompressonOnDirectory(cdpsetting.CatalogueDir()))
				{
					stringstream l_stderr;
					l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
						<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
						<< "Error during CDP Log Directory :" 
						<< cdpsetting.CatalogueDir() << " compression.\n";

					DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				}
			}
#endif

			CDPDatabase db(cdpsetting.Catalogue());
			if(!db.initialize())
			{
				rv = false;
				break;
			}			

		} while (0);

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, cdpsetting.Catalogue().c_str());
	}

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	return rv;
}

bool PolicyMgrTask::IsInsufficientDiskSpaceOccured()
{
	SV_ULONGLONG diskFreeSpace = 0;
	if(!GetDiskFreeCapacity(m_retentionmtpt, diskFreeSpace))
	{
		DebugPrintf(SV_LOG_ERROR, "Unable to get disk free space for %s\n", m_retentionmtpt.c_str()); 
		return false;
	}
	if(diskFreeSpace > m_cdpfreespace_trigger)
		return false;

	return true;
}

void PolicyMgrTask::SetMinimumAvalableFreeSpace()
{
	CDPUtil::get_lowspace_value(m_retentionmtpt,m_cdpfreespace_trigger);
	m_space_tofree = 2*m_cdpfreespace_trigger; //Free twice the lowspace_trigger to create enough space.
}

bool PolicyMgrTask::FreeSpaceOnInsufficientRetentionDiskSpace()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	try
	{
		do
		{

			cdp_recoveryrange_set_t recoveryRangeSet;
			CDPSETTINGS_MAP::iterator iter = m_policies.begin();
			m_lowspace = true;
			m_purge_onlowspace_alert = true;

			for(;iter != m_policies.end();++iter)
			{
				if((*iter).second.OnSpaceShortage() == CDP_STOP)
				{
					continue;
				}

				SV_ULONGLONG starttime = 0;
				SV_ULONGLONG endtime = 0;
				CDPDatabase db((*iter).second.Catalogue());
				if(!db.exists())
				{
					continue;
				}
				db.getrecoveryrange(starttime,endtime);
				cdp_recoveryrangekey_t recoveryRange(starttime,(*iter).first);
				//pairs which are in initial sync/no time range should be dropped
				if(starttime != 0)
				{
					recoveryRangeSet.insert(recoveryRange);              
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "FreeSpaceOnInsufficientRetentionDiskSpace: Ignoring target %s as start time is zero\n", (*iter).first.c_str());
				}
			}

			SV_ULONGLONG spacefreed = 0;
			GetDiskFreeCapacity(m_retentionmtpt, spacefreed);
			DebugPrintf(SV_LOG_DEBUG, "volume: %s space free " ULLSPEC "\n", m_retentionmtpt.c_str(), 
				spacefreed);

			//freeing up the spaces from retention disk
			while(!QuitRequested() && !recoveryRangeSet.empty() && spacefreed < (getInsufficientSpaceToFree() + m_cdpreserved_space))
			{                                                
				cdp_recoveryrangekey_t recoveryRange = (*recoveryRangeSet.begin());

				iter = m_policies.find(recoveryRange.tgtvolname);
				if(iter == m_policies.end())
					break;

				recoveryRangeSet.erase(recoveryRangeSet.begin());

				// while purge operation is in progress, we do not want
				// any recovery jobs to access the retention files
				CDPLock purge_lock(recoveryRange.tgtvolname, true, ".purgelock");
				purge_lock.acquire();

				CDPDatabase db((*iter).second.Catalogue());
				SV_ULONGLONG start_ts = 0;
				if(!db.delete_olderdata(recoveryRange.tgtvolname, start_ts))
				{
					rv = false;
					continue;
				}

				if(!GetDiskFreeCapacity(m_retentionmtpt, spacefreed))
				{
					rv = false;
					break;
				}

				DebugPrintf(SV_LOG_DEBUG, "volume: %s space free " ULLSPEC "\n", m_retentionmtpt.c_str(), 
					spacefreed);

				if(recoveryRange.start_ts != start_ts)
				{
					recoveryRange.start_ts = start_ts;
					recoveryRangeSet.insert(recoveryRange);
				}
			}


		}while(false);
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "Caught unknown exception\n");
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return rv;
}

bool PolicyMgrTask::PerformStaleFileCleanup()
{
	bool rv = true;
	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

		do
		{

			CDPSETTINGS_MAP::iterator cdpIter = m_policies.begin();
			for(;cdpIter != m_policies.end();++cdpIter)
			{
				CDP_SETTINGS cdpsetting = (*cdpIter).second;
				// if retention is not enabled, just return
				if(cdpsetting.Status() != CDP_ENABLED)
				{
					continue;
				}


				CDPDatabase db(cdpsetting.Catalogue());
				if(!db.exists())
				{
					continue;
				}

				//Stale file cleanup should be done only once to clean unwanted files present in the retention. The actual issue is fixed through 
				//bug id 16768. If the cleanup log exists just continue with next volume.
				std::string stale_file_cleanup_log = cdpsetting.CatalogueDir() + ACE_DIRECTORY_SEPARATOR_CHAR_A + CDPV3_STALE_FILES_DELLOG;
				ACE_stat filestat = {0};

				if(sv_stat(getLongPathName(stale_file_cleanup_log.c_str()).c_str(),&filestat) == 0)
				{
					DebugPrintf(SV_LOG_DEBUG,"Stale file deletion already done for volume %s, Proceed for next volume\n",(*cdpIter).first.c_str());
					continue;
				}

				db.delete_stalefiles();

				if(QuitRequested())
					break;
			}

		}while(false);

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}
	return rv;
}

bool PolicyMgrTask::DeleteUnusableRecoveryPoints()
{
	bool rv = true;
	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

		do
		{

			CDPSETTINGS_MAP::iterator cdpIter = m_policies.begin();
			for(;cdpIter != m_policies.end();++cdpIter)
			{
				CDP_SETTINGS cdpsetting = (*cdpIter).second;
				// if retention is not enabled, just return
				if(cdpsetting.Status() != CDP_ENABLED)
				{
					continue;
				}


				CDPDatabase db(cdpsetting.Catalogue());
				if(!db.exists())
				{
					continue;
				}
				//Delete unusable recovery points should be run only once for each target volume. The actual issue is fixed through bug id 19533
				//The CDPV3_UNUSABLE_RECOVERYPTS_DELLOG is created when the cleanup is done. Continue with the next volume if the file exists.
				std::string unusable_pt_del_log = cdpsetting.CatalogueDir() + ACE_DIRECTORY_SEPARATOR_CHAR_A + CDPV3_UNUSABLE_RECOVERYPTS_DELLOG;
				ACE_stat filestat = {0};

				if(sv_stat(getLongPathName(unusable_pt_del_log.c_str()).c_str(),&filestat) == 0)
				{
					DebugPrintf(SV_LOG_DEBUG,"Unusable recovery point and Unusable bookmark cleanup already done for volume %s, Proceed for next volume\n",(*cdpIter).first.c_str());
					continue;
				}

				db.delete_unusable_recoverypts();

				if(QuitRequested())
					break;
			}

		}while(false);

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}
	return rv;
}

bool PolicyMgrTask::ProcessPoliciesForRetentionVolume()
{
	bool rv = true;
	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
		do
		{
			ACE_Time_Value currentTime = ACE_OS::gettimeofday();
			//call policy check
			if(currentTime.sec() - m_cdppolicycheck_ts.sec() >= m_cdppolicycheck_interval)
			{
				CDPSETTINGS_MAP::iterator cdpIter = m_policies.begin();
				for(;cdpIter != m_policies.end();++cdpIter)
				{
					EnforcePolicies((*cdpIter).first,(*cdpIter).second);
					if(QuitRequested())
						break;
				}
				m_cdppolicycheck_ts = currentTime;
			}

			if(QuitRequested())
				break;

			currentTime = ACE_OS::gettimeofday();
			if(currentTime.sec() - m_cdpfreespacecheck_ts.sec() >= m_cdpfreespacecheck_interval)
			{
				if(IsInsufficientDiskSpaceOccured())
				{
					FreeSpaceOnInsufficientRetentionDiskSpace();
				}                
				m_cdpfreespacecheck_ts = currentTime;
			}

			if(QuitRequested())
				break;

			currentTime = ACE_OS::gettimeofday();            
			if(m_cxupdates_interval && (currentTime.sec() - m_cxupdates_ts.sec() >= m_cxupdates_interval))
			{
				updatecx();
				m_cxupdates_ts = currentTime;
			}

			currentTime = ACE_OS::gettimeofday();
			if(m_delete_pendingupdates && m_delete_pendingupdates_interval && 
				(currentTime.sec() - m_cx_deletepending_updates_ts.sec() >= m_delete_pendingupdates_interval))
			{
				deleteAllPendingUpdates();
				m_cx_deletepending_updates_ts = currentTime;
			}

			currentTime = ACE_OS::gettimeofday();
			if(m_cx_cdpdiskusage_update_interval && 
				(currentTime.sec() - m_cx_cdpdiskusage_updates_ts.sec() >= m_cx_cdpdiskusage_update_interval))
			{
				updateCdpDiskUsageToCX();
				m_cx_cdpdiskusage_updates_ts = currentTime;
			}

			if(QuitRequested())
				break;

			currentTime = ACE_OS::gettimeofday();
			if(currentTime.sec() - m_alertupdate_ts.sec() 
				>=  m_alertupdate_interval)
			{
				sendalerts();
				m_alertupdate_ts = ACE_OS::gettimeofday();
			}

			if(QuitRequested())
				break;   

		}while(false);
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}
	return rv;
}
/*
* FUNCTION NAME :  PolicyMgrTask::updatecx
*
* DESCRIPTION : send request to cx for updating the timerange and tag info
*     
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success otherwise false
*
*/
bool PolicyMgrTask::updatecx()
{
	bool rv = true;   
	bool any_records_pending = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

	try
	{
		while(any_records_pending)
		{
			if(!updatecx_nextbatch(any_records_pending))
			{
				rv = false;
				break;
			}

			if(!m_cx_send_updates_atonce)
			{
				rv = true;
				break;
			}

			if(QuitRequested(0))
			{
				rv = true;
				break;
			}
		}

	} catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());
	return rv;
}

/*
* FUNCTION NAME :  PolicyMgrTask::updatecx_nextbatch
*
* DESCRIPTION : send timerange and bookmarks to cx
*     
* Algorithm:
*  
* updates to cx are sent based on following config variables
* 
* 1) Config variable name: CDPMgrUpdateCxPerTargetVolume 
* Type: boolean (0/1)
* Default: 0
* 0 means, we will send updates for all replications sharing the retention volume
* to cx in single update call.
* 1 means, we will send updates for each replication individually to cx.
*
* 2) Config variable name: CDPMgrEventTimeRangeRecordsPerBatch
* Type: integer 
* Values: 0 to 4294967296
* Default: 0
* 0 has special meaning. it implies no limit send any number of records.
* A positive value means a limit on no. of records that can be sent per replication
* in a single call
*
* 3) If batch request is configured. After sending a batch, what should be done
* for the remaining pending records? We can do either of following:
*     a. Make more calls to cx to complete sending  rest of the updates.
*     b. Send the rest of the updates in next invocation after a delay.
*
* To define the behavior, we introduce another Boolean config parameter
* CDPMgrSendUpdatesAtOnce to decide the behavior. A value of zero means
* behavior (b) , 1 means behavior (a). By default we will set this to zero.
* if it is set to 1, this routine will get called in a loop from updatecx
* until there are no more records to send.
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success otherwise false
*
*/
bool PolicyMgrTask::updatecx_nextbatch(bool & any_records_pending)
{
	bool rv = true;
	any_records_pending = false;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

	// note: not setting rv = false in this routine for following case
	// let's say there is one problem with one of replications sharing the retention
	// we still want to send updates for rest of the pairs sharing the  retention volume

	HostCDPInformation host_cdp_info;
	CDPSETTINGS_MAP::iterator cdpiter = m_policies.begin();
	for(;cdpiter != m_policies.end();++cdpiter)
	{
		CDPDatabase db((*cdpiter).second.Catalogue());
		if(!db.exists())
		{
			continue;
		}
		CDPInformation info;
		if(!db.get_pending_updates(info,m_cx_event_timerange_rec_per_batch))
		{
			continue;
		}

		if(m_cx_update_per_target_volume)
		{
			HostCDPInformation target_cdp_info;
			target_cdp_info.insert(std::make_pair((*cdpiter).first,info));
			if(!updateCdpInformation(target_cdp_info))
			{
				// see note above for not setting rv
				// rv = false;
				continue;
			}

			if(!db.delete_pending_updates(info))
			{
				// see note above for not setting rv
				// rv = false;
				continue;
			}

			if(QuitRequested(0))
			{
				rv = true;
				break;
			}

		} else
		{
			host_cdp_info.insert(std::make_pair((*cdpiter).first,info));
		}

		if(m_cx_event_timerange_rec_per_batch &&
			(info.m_events.size() ==  m_cx_event_timerange_rec_per_batch ||
			info.m_ranges.size() == m_cx_event_timerange_rec_per_batch))
			any_records_pending = true;
	}


	if(!m_cx_update_per_target_volume && !QuitRequested(0)) 
	{ 
		// updates are to be sent for all replications sharing retention volume in single call
		if(updateCdpInformation(host_cdp_info))
		{
			cdpiter = m_policies.begin();
			for(;cdpiter != m_policies.end();++cdpiter)
			{
				HostCDPInformation::iterator host_cdpinfo_iter = host_cdp_info.find((*cdpiter).first);
				if(host_cdpinfo_iter == host_cdp_info.end())
					continue;

				CDPDatabase db((*cdpiter).second.Catalogue());
				if(!db.delete_pending_updates(host_cdpinfo_iter->second))
				{
					// see note above for not setting rv
					//rv = false;
					continue;
				}
			}

		} else // failed to update cx
		{
			rv = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

	return rv;
}


/*
* FUNCTION NAME :  PolicyMgrTask::updateCdpInformation
*
* DESCRIPTION : send request to cx for updating the timerange and tag info
*     
*     
*
* INPUT PARAMETERS : HostCDPInformation
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success otherwise false
*
*/
bool PolicyMgrTask::updateCdpInformation(const HostCDPInformation & hostCdpInfo)
{
	bool rv = true;
	try
	{
		if(!m_configurator->getVxAgent().updateCDPInformationV2(hostCdpInfo))
		{
			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error: updateCDPInformationV2 returned failed status\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
			rv = false ;            
		}
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}
	return rv;
}
/*
* FUNCTION NAME :  PolicyMgrTask::updateCdpDiskUsageToCX
*
* DESCRIPTION : send request to cx for updating the retention disk usage 
*               
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success otherwise false
*
*/
bool PolicyMgrTask::updateCdpDiskUsageToCX()
{
	bool rv = true;   

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	try
	{

		do
		{
			HostCDPRetentionDiskUsage hostCdpRetentionDiskUsage;
			HostCDPTargetDiskUsage    hostCdpTargetDiskUsage;
			CDPSETTINGS_MAP::iterator cdpIter = m_policies.begin();
			for(;cdpIter != m_policies.end();++cdpIter)
			{
				CDPDatabase db((*cdpIter).second.Catalogue());
				if(!db.exists())
				{
					continue;
				}
				CDPRetentionDiskUsage retentionusage;
				CDPTargetDiskUsage    targetusage;
				if(!db.get_cdp_retention_diskusage_summary(retentionusage))
				{
					DebugPrintf(SV_LOG_ERROR," Failed to get diskusage details for volume %s\n",((*cdpIter).first).c_str());
					continue;
				}
				std::string target = (*cdpIter).first;
				FormatVolumeNameForCxReporting(target);
				FirstCharToUpperForWindows(target);
				volpackproperties volpack_instance;
				if(0 == getvolpackproperties(target,volpack_instance))
				{
					targetusage.space_saved_by_thinprovisioning = volpack_instance.m_logicalsize - volpack_instance.m_sizeondisk;
					targetusage.size_on_disk = volpack_instance.m_sizeondisk;
				}
				else
				{
					if(!getSourceCapacity((*m_configurator), target, targetusage.size_on_disk))
					{
						DebugPrintf(SV_LOG_ERROR," Failed to get size details for volume %s\n",((*cdpIter).first).c_str());
						targetusage.size_on_disk = 0;
					}
					targetusage.space_saved_by_thinprovisioning = 0;
				}
				hostCdpRetentionDiskUsage.insert(make_pair((*cdpIter).first,retentionusage));
				hostCdpTargetDiskUsage.insert(make_pair((*cdpIter).first,targetusage));
			}

			//call the cx api to update the about space savings 
			if(!m_configurator->getVxAgent().updateCdpDiskUsage(hostCdpRetentionDiskUsage,hostCdpTargetDiskUsage))
			{
				stringstream l_stdfatal;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "Error: updateCdpDiskUsage   failed " << "\n";

				DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
				rv = false ;
				break;
			}          

		} while (0);

	}

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

/*
* FUNCTION NAME :  PolicyMgrTask::deleteAllPendingUpdates
*
* DESCRIPTION : delete any pending bookmarks, timeranges information from retention
*               change made for SSE where this information does not need to
*               be sent
*               
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success otherwise false
*
*/
bool PolicyMgrTask::deleteAllPendingUpdates()
{
	bool rv = true;   

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	try
	{

		do
		{
			CDPSETTINGS_MAP::iterator cdpIter = m_policies.begin();
			for(;cdpIter != m_policies.end();++cdpIter)
			{
				CDPDatabase db((*cdpIter).second.Catalogue());
				if(!db.exists())
				{
					continue;
				}

				if(!db.delete_all_pending_updates())
				{
					DebugPrintf(SV_LOG_ERROR," Failed to delete pending cdp updates for volume %s\n",((*cdpIter).first).c_str());
					continue;
				}
			}     

		} while (0);

	}

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


bool PolicyMgrTask::EnforcePolicies(const std::string & tgtvolname, const CDP_SETTINGS & cdpsetting)
{
	bool rv = true;

	try
	{

		do
		{
			// if retention is not enabled, just return
			if(cdpsetting.Status() != CDP_ENABLED)
			{
				rv = true;
				break;
			}

			//if(IsPause(tgtvolname))
			//{
			//             DebugPrintf(SV_LOG_DEBUG, "Skipping EnforceTimeAndSpacePolicies as the volume %s is in pause state.\n", tgtvolname.c_str());
			//	rv = true;
			//	break;
			//}

			//if(!initCDP(cdpsetting))
			//{
			//	rv = false;
			//	break;
			//}			
			//

			if(!EnforceTimeAndSpacePolicies(tgtvolname,cdpsetting))
			{
				rv = false;
				break;
			}			

		} while (0);	
	}

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	return rv;

}


/*
* FUNCTION NAME :  PolicyMgrTask::EnforceTimeSpaceAndMinSpacePolicies
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
bool PolicyMgrTask::EnforceTimeAndSpacePolicies(const std::string & tgtvolname, const CDP_SETTINGS & cdpsetting)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for volume %s\n",FUNCTION_NAME,tgtvolname.c_str());
	bool rv = true;

	do
	{
		CDPDatabase db(cdpsetting.Catalogue());
		if(!db.exists())
		{
			rv = true;
			break;
		}

		//TODO: startts and endts needs to be preserved for sparse policies
		SV_ULONGLONG startts = 0;
		SV_ULONGLONG endts = 0;
		CdpTimeRanges_t::iterator iter = m_cdptimeranges.find(tgtvolname);
		if(iter == m_cdptimeranges.end())
		{
			DebugPrintf(SV_LOG_INFO,"Unable to find the last run time range for volume %s\n",tgtvolname.c_str());
		}
		else
		{
			startts = iter->second.startts;
			endts = iter->second.endts;
		}


		// while purge operation is in progress, we do not want
		// any recovery jobs to access the retention files
		CDPLock purge_lock(tgtvolname, true, ".purgelock");
		purge_lock.acquire();
		if(!db.enforce_policies(cdpsetting,startts, endts,tgtvolname))
		{
			rv = false;
			break;
		}

		if(iter != m_cdptimeranges.end())
		{
			iter->second.startts = startts;
			iter->second.endts = endts;
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool PolicyMgrTask::SendPauseRequestToCX(const std::string & volumeName, const std::string & pauseMsg)
{
	bool rv = false;
	std::string requestMsg;
	requestMsg = "pause_components=yes;components=affected;pause_components_status=requested;pause_components_message=;"
		"insufficient_space_components=yes;insufficient_space_status=requested;insufficient_space_message=";
	requestMsg += pauseMsg;
	requestMsg += ";";
	int agentType = (int)HOSTTARGET;
	Configurator* TheConfigurator = NULL;
	if(!GetConfigurator(&TheConfigurator))
	{
		DebugPrintf(SV_LOG_DEBUG, "Error obtaining configurator %s\t%d\n", FUNCTION_NAME, LINE_NO);
		DebugPrintf(SV_LOG_ERROR,"FAILED:SendPauseRequestToCX call failed.\n");
		return rv;
	}
	else 
	{
		if(NotifyMaintenanceActionStatus((*TheConfigurator), volumeName,agentType,requestMsg))
		{
			rv = true; 
		}
	}
	return (rv);
}

bool PolicyMgrTask::IsPause(const std::string & tgtvolname)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	try
	{
		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups 
			= m_initialsettings.hostVolumeSettings.volumeGroups;
		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter 
			= volumeGroups.begin();

		for(; vgiter != volumeGroups.end(); ++vgiter)
		{
			VOLUME_GROUP_SETTINGS vg = *vgiter;
			if(TARGET != vg.direction)
				continue;

			VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter 
				=  vg.volumes.find(tgtvolname);
			if (ctVolumeIter == vg.volumes.end())
			{
				continue;
			}
			if((ctVolumeIter ->second.pairState == VOLUME_SETTINGS::PAUSE) ||
				(ctVolumeIter ->second.pairState == VOLUME_SETTINGS::PAUSE_PENDING))
			{
				rv = true;
				break;
			}            
		}   
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, tgtvolname.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool PolicyMgrTask::isMoveRetention(const std::string & tgtvolname)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	try
	{

		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups 
			= m_initialsettings.hostVolumeSettings.volumeGroups;
		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter 
			= volumeGroups.begin();

		for(; vgiter != volumeGroups.end(); ++vgiter)
		{
			VOLUME_GROUP_SETTINGS vg = *vgiter;
			if(TARGET != vg.direction)
				continue;

			VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter 
				=  vg.volumes.find(tgtvolname);
			if (ctVolumeIter == vg.volumes.end())
			{
				continue;
			}
			// we found the pair and it is paused for move retention
			if(((ctVolumeIter ->second.pairState == VOLUME_SETTINGS::PAUSE) || (ctVolumeIter ->second.pairState == VOLUME_SETTINGS::PAUSE_PENDING))
				&&(ctVolumeIter ->second.maintenance_action.find("move_retention=yes;")
				!= std::string::npos))
			{
				rv = true;
				break;
			}
		}
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, tgtvolname.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool PolicyMgrTask::isPairDeleted(const std::string & tgtvolname)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	try
	{

		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups 
			= m_initialsettings.hostVolumeSettings.volumeGroups;
		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter 
			= volumeGroups.begin();

		for(; vgiter != volumeGroups.end(); ++vgiter)
		{
			VOLUME_GROUP_SETTINGS vg = *vgiter;
			if(TARGET != vg.direction)
				continue;

			VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter 
				=  vg.volumes.find(tgtvolname);
			if (ctVolumeIter == vg.volumes.end())
			{
				continue;
			}
			string cleanup_action = ctVolumeIter ->second.cleanup_action;
			if(ctVolumeIter ->second.pairState == VOLUME_SETTINGS::DELETE_PENDING)
			{
				rv = true;
				break;
			}            
		}
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, tgtvolname.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool PolicyMgrTask::isPairFlushAndHoldState(const std::string & tgtvolname)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	try
	{

		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups 
			= m_initialsettings.hostVolumeSettings.volumeGroups;
		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter 
			= volumeGroups.begin();

		for(; vgiter != volumeGroups.end(); ++vgiter)
		{
			VOLUME_GROUP_SETTINGS vg = *vgiter;
			if(TARGET != vg.direction)
				continue;

			VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter 
				=  vg.volumes.find(tgtvolname);

			if (ctVolumeIter == vg.volumes.end())
			{
				continue;
			}

			if(ctVolumeIter ->second.pairState == VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES_PENDING ||
				ctVolumeIter ->second.pairState == VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES ||
				ctVolumeIter ->second.pairState == VOLUME_SETTINGS::FLUSH_AND_HOLD_RESUME_PENDING )
			{
				rv = true;
				break;
			}            
		}
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, tgtvolname.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;

}
/*
* FUNCTION NAME :  PolicyMgrTask::sendalerts
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*						
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/
bool PolicyMgrTask::sendalerts()
{
	bool rv = true;

	try
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());

		do
		{

			SV_ULONGLONG totalCapacity,freeSpaceLeft;
			if(!GetDiskFreeSpaceP(m_retentionmtpt,
				NULL, 
				&totalCapacity, 
				&freeSpaceLeft))
			{
				rv = false;
				break;
			}

			SV_UINT usedPct = 0;
			bool threshold_alert_sent = false;
			if ( totalCapacity > 0 )
			{
				usedPct = (100 - static_cast<int>( ((freeSpaceLeft * 100)/totalCapacity) ));
			}

			CDPSETTINGS_MAP::iterator iter = m_policies.begin();
			for(;iter != m_policies.end();++iter)
			{

				if (!threshold_alert_sent &&
					(*iter).second.CatalogueThreshold() &&
					(usedPct >= (*iter).second.CatalogueThreshold()) &&
					(usedPct != m_thresholdcrossed_pct)
					)
				{
                    RetentionDiskUsageAlert a(m_retentionmtpt, usedPct, (*iter).second.CatalogueThreshold());
					DebugPrintf(SV_LOG_DEBUG, "%s", a.GetMessage().c_str());
					SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
					m_thresholdcrossed_pct = usedPct;
					threshold_alert_sent = true;
				}

				if(m_lowspace)
				{
					if((*iter).second.OnSpaceShortage() == CDP_STOP)
					{
						if(!IsPause((*iter).first))
						{
							std::stringstream l_stdalert;	
							l_stdalert << " Replication is paused on volume "
								<< (*iter).first
								<< " due to inadequate retention space on"
								<< m_retentionmtpt << ".\n";

							DebugPrintf(SV_LOG_DEBUG, "%s", l_stdalert.str().c_str());
							SendPauseRequestToCX((*iter).first, l_stdalert.str());
						}
					} else
					{

						if(m_purge_onlowspace_alert)
						{
                            RetentionPurgedAlert a(m_retentionmtpt);
							DebugPrintf(SV_LOG_DEBUG, "%s", a.GetMessage().c_str());
							SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);
							m_purge_onlowspace_alert = false;
						}
					}
				} 
			}

			m_lowspace = false;

		} while (0);

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_retentionmtpt.c_str());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_retentionmtpt.c_str());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_retentionmtpt.c_str());
	}

	return rv;
}


/*
* FUNCTION NAME :  ReporterTask::open
*
* DESCRIPTION : 
*  
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : int
*
*/
int ReporterTask::open(void *args)
{
	return this->activate(THR_BOUND);
}

/*
* FUNCTION NAME :  ReporterTask::close
*
* DESCRIPTION : 
*     
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : int
*
*/
int ReporterTask::close(u_long flags)
{
	return 0;
}
/*
* FUNCTION NAME :  ReporterTask::svc
*
* DESCRIPTION : 
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : int
*
*/

int ReporterTask::svc()
{
	bool rv = true;

	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
		LocalConfigurator localConfigurator;
		SV_UINT ReporterInterval = localConfigurator.getPendingVsnapReporterInterval();
		SV_UINT nBatchRequest = localConfigurator.getNumberOfBatchRequestToCX();

		while (!QuitRequested(ReporterInterval))
		{
			if(!VsnapUpdationRequestToCX(nBatchRequest))
			{
				DebugPrintf(SV_LOG_ERROR,"%s encountered failure in VsnapUpdationRequestToCX\n",
					FUNCTION_NAME);
			}
		}

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	}

	catch ( ContextualException& ce )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
	}

	this -> MarkDead();
	return 0;
}

/*
* FUNCTION NAME :  ReporterTask::PostMsg
*
* DESCRIPTION : to post the message
*     
*     
*
* INPUT PARAMETERS : msg,priority
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : None
*
*/
void ReporterTask::PostMsg(int msg, int priority)
{
	bool rv = true;

	try
	{
		ACE_Message_Block *mb = new ACE_Message_Block(0, msg);
		mb->msg_priority(priority);
		msg_queue()->enqueue_prio(mb);
	}

	catch ( ContextualException& ce )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
	}
}
/*
* FUNCTION NAME :  ReporterTask::QuitRequested
*
* DESCRIPTION : report if service made a quit or timeout happened
*     
*     
*
* INPUT PARAMETERS : int seconds to wait
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true if quit requested else false
*
*/
bool ReporterTask::QuitRequested(int seconds)
{
	bool shouldQuit = false;

	try
	{
		ACE_Message_Block *mb;

		ACE_Time_Value wait = ACE_OS::gettimeofday ();
		wait.sec (wait.sec () + seconds);
		if (-1 == this->msg_queue()->peek_dequeue_head(mb, &wait))
		{
			if (errno == EWOULDBLOCK)
			{
				shouldQuit = false;
			} else
			{

				DebugPrintf(SV_LOG_ERROR, "\n%s encountered error (message queue closed)\n",
					FUNCTION_NAME);

				// the message queue has been closed abruptly
				// let's us request for a safe shutdown

				CDPUtil::SignalQuit();
				shouldQuit= true;
			}
		} else
		{
			if((RM_QUIT == mb->msg_type()) ||(RM_TIMEOUT == mb ->msg_type()))
			{
				shouldQuit = true;
			}
		}
	}

	catch ( ContextualException& ce )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		shouldQuit= true;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();
		shouldQuit= true;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		shouldQuit= true;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
	}

	return shouldQuit;
}
/*
* FUNCTION NAME :  ReporterTask::Idle
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
void ReporterTask::Idle(int seconds)
{
	(void)QuitRequested(seconds);
}
/*
* FUNCTION NAME :  ReporterTask::VsnapUpdationRequestToCX
*
* DESCRIPTION : send the pending vsnaps to cx
*     
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success otherwise false
*
*/
bool ReporterTask::VsnapUpdationRequestToCX(SV_UINT nBatchRequest)
{
	bool rv = true;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	try
	{
		do
		{
			VsnapMgr* vsnapmgr;

#ifdef SV_WINDOWS
			WinVsnapMgr mgr;
#else
			UnixVsnapMgr mgr;
#endif
			vsnapmgr = &mgr;

			std::vector<VsnapPersistInfo> createvsnaps;
			std::vector<VsnapDeleteInfo> deletevsnaps;
			if(!vsnapmgr->RetrievePendingVsnapInfo(createvsnaps, deletevsnaps, nBatchRequest))
			{
				DebugPrintf(SV_LOG_ERROR, "Unable to retrieve vsnaps from pending store\n");
				rv = false;
				break;
			}
			DebugPrintf(SV_LOG_DEBUG, "size of created vsnap %d, size of deleted vsnap %d\n",
				createvsnaps.size(),deletevsnaps.size());
			if(!createvsnaps.empty())
			{
				std::vector<bool> results;
				rv = notifyCxOnSnapshotCreation((*m_configurator), createvsnaps, results);
				if(!rv)
				{
					DebugPrintf(SV_LOG_DEBUG, "%s\t%dUnable to send pending vsnap creation info to cx\n",
						FUNCTION_NAME, LINE_NO);
					break;
				}

				std::vector<VsnapPersistInfo>::iterator vsnapIter = createvsnaps.begin();
				for(std::vector<bool>::iterator iter = results.begin(); 
					iter != results.end(); ++iter, ++vsnapIter)
				{
					if(*iter)
					{
						std::ostringstream errstr;
						if(!VsnapUtil::delete_pending_persistent_vsnap_filename(vsnapIter->devicename
							,vsnapIter->target))
						{
							errstr << "Failed to delete pending vsnap information for " 
								<< vsnapIter->devicename
								<< " " << FUNCTION_NAME << " " << LINE_NO << std::endl;
							DebugPrintf(SV_LOG_ERROR, "%s", errstr.str().c_str());
							rv = false;
						}
					}
				}
			}
			if(!deletevsnaps.empty())
			{
				std::vector<bool> results;
				rv = notifyCxOnSnapshotDeletion((*m_configurator), deletevsnaps,results);
				if(!rv)
				{
					DebugPrintf(SV_LOG_DEBUG, "%s\t%dUnable to send pending vsnap deletion info to cx\n",
						FUNCTION_NAME, LINE_NO);
					break;
				}
				std::vector<VsnapDeleteInfo>::iterator vsnapIter = deletevsnaps.begin();
				for(std::vector<bool>::iterator iter = results.begin(); 
					iter != results.end(); ++iter, ++vsnapIter)
				{
					if(*iter)
					{
						std::ostringstream errstr;
						if(!VsnapUtil::delete_pending_persistent_vsnap_filename(vsnapIter->vsnap_device,
							vsnapIter->target_device))
						{
							errstr << "Failed to delete pending vsnap information for " 
								<< vsnapIter->vsnap_device
								<< " " << FUNCTION_NAME << " " << LINE_NO << std::endl;
							DebugPrintf(SV_LOG_ERROR, "%s", errstr.str().c_str());
							rv = false;
						}
					}
				}
			}
			if((createvsnaps.size() < nBatchRequest) && (deletevsnaps.size() < nBatchRequest))
			{
				break;
			}
			if(CDPUtil::Quit())
			{
				break;
			}
		} while (1);
	}
	catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, "Caught exception while sending updates to cx on pending vsnaps\n");
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

	return rv;

}


// ScheduleTask

/*
* FUNCTION NAME :  ScheduleTask::open
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/

int ScheduleTask::open(void *args)
{
	return this->activate(THR_BOUND);
}

/*
* FUNCTION NAME :  ScheduleTask::close
*
* DESCRIPTION : 
*     
*      
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : tbd
*
*/

int ScheduleTask::close(u_long flags)
{
	return 0;
}


/*
* FUNCTION NAME :  PolicyMgrTask::PostMsg
*
* DESCRIPTION : 
*       
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
void ScheduleTask::PostMsg(int msgId, int priority)
{
	bool rv = true;

	try
	{
		ACE_Message_Block *mb = new ACE_Message_Block(0, msgId);
		mb->msg_priority(priority);
		msg_queue()->enqueue_prio(mb);
	} 

	catch ( ContextualException& ce )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_volname.c_str());
	}
	catch( exception const& e )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_volname.c_str());
	}
	catch ( ... )
	{
		// unhandled failure (no return value), let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_volname.c_str());
	}

}

/*
* FUNCTION NAME :  ScheduleTask::FetchNextMessage
*
* DESCRIPTION : 
*     
*     
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : 
*    RM_MSG_SUCCESS = success, message available
*    RM_MSG_NONE = timeout, no message
*    RM_MSG_FAILURE = failure
*
*/
SV_UINT ScheduleTask::FetchNextMessage(ACE_Message_Block ** mb, int seconds)
{
	SV_UINT rv = RM_MSG_SUCCESS;

	try
	{

		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

		ACE_Time_Value wait = ACE_OS::gettimeofday ();
		wait.sec (wait.sec () + seconds);
		if (-1 == this->msg_queue()->dequeue_head(*mb, &wait)) 
		{
			if (errno == EWOULDBLOCK)
			{
				rv = RM_MSG_NONE;
			} else
			{

				DebugPrintf(SV_LOG_ERROR,
					"\n%s encountered error (message queue closed) for %s\n",
					FUNCTION_NAME, m_volname.c_str());

				// the message queue has been closed abruptly
				// let's us request for a safe shutdown

				CDPUtil::SignalQuit();
				rv = RM_MSG_FAILURE;
			}
		} else
		{
			rv = RM_MSG_SUCCESS;
		}

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
	}

	catch ( ContextualException& ce )
	{
		CDPUtil::SignalQuit();
		rv = RM_MSG_FAILURE;

		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_volname.c_str());
	}
	catch( exception const& e )
	{
		CDPUtil::SignalQuit();
		rv = RM_MSG_FAILURE;

		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_volname.c_str());
	}
	catch ( ... )
	{
		CDPUtil::SignalQuit();
		rv = RM_MSG_FAILURE;

		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_volname.c_str());
	}

	return rv;
}

/*
* FUNCTION NAME :  ScheduleTask::QuitRequested
*
* DESCRIPTION : report if service made a quit or timeout happened
*     
*     
*
* INPUT PARAMETERS : int seconds to wait
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true if quit requested else false
*
*/
bool ScheduleTask::QuitRequested(int seconds)
{
	bool shouldQuit = false;

	try
	{
		ACE_Message_Block *mb;

		ACE_Time_Value wait = ACE_OS::gettimeofday ();
		wait.sec (wait.sec () + seconds);
		if (-1 == this->msg_queue()->peek_dequeue_head(mb, &wait))
		{
			if (errno == EWOULDBLOCK)
			{
				shouldQuit = false;
			} else
			{

				DebugPrintf(SV_LOG_ERROR, "\n%s encountered error (message queue closed)\n",
					FUNCTION_NAME);

				// the message queue has been closed abruptly
				// let's us request for a safe shutdown

				CDPUtil::SignalQuit();
				shouldQuit= true;
			}
		} else
		{
			if((RM_QUIT == mb->msg_type()) ||(RM_TIMEOUT == mb ->msg_type()))
			{
				shouldQuit = true;
			}
		}
	}

	catch ( ContextualException& ce )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		shouldQuit= true;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();
		shouldQuit= true;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		shouldQuit= true;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
	}

	return shouldQuit;
}

int ScheduleTask::svc()
{
	bool rv = true;

	try
	{
		DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for target volume %s\n",FUNCTION_NAME,m_volname.c_str());
		do
		{
			if(m_action == CDPMOVEACTION)
			{
				CdpMoveMsg * movemsg;
				movemsg = reinterpret_cast<CdpMoveMsg *>(m_actionmsg);
				MoveRetention(movemsg);
				delete movemsg;
				break;
			}
			if(m_action == CDPPAIRDELETEACTION)
			{
				CdpDelMsg * delmsg;
				delmsg = reinterpret_cast<CdpDelMsg *>(m_actionmsg);
				ProcessPairDeletion(delmsg);
				delete delmsg;
				break;
			}
			if(m_action == CDPDISABLEACTION)
			{
				CdpDisableMsg * disablemsg;
				disablemsg = reinterpret_cast<CdpDisableMsg *>(m_actionmsg);
				DisableRetention();
				delete disablemsg;
				break;
			}
		} while(false);

		DebugPrintf(SV_LOG_DEBUG,"EXITED %s for target volume %s\n",FUNCTION_NAME,m_volname.c_str());
	}

	catch ( ContextualException& ce )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
	}
	catch( exception const& e )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		// unhandled failure, let's us request for a safe shutdown
		CDPUtil::SignalQuit();

		rv = false;
		DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
	}

	this -> MarkDead();
	return 0;
}

bool ScheduleTask::ProcessPairDeletion(CdpDelMsg * delmsg)
{
	bool rv = true;
	std::string dbname;
	std::string errmsg;
	stringstream cxmsg;

	try
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

		do
		{
			//getting the move msg from action msg
			if(!delmsg)
			{
				rv = false;
				break;
			}

			if(delmsg->vsnapcleanup)
			{
				rv = DeleteVsnapsForTargetVolume(m_volname,errmsg);

				VOLUME_SETTINGS::PAIR_STATE state = rv ? 
					VOLUME_SETTINGS::VNSAP_CLEANUP_COMPLETE : VOLUME_SETTINGS::VNSAP_CLEANUP_FAILED;

				cxmsg << "vsnap_cleanup=yes;"
					<< "vsnap_cleanup_status="
					<< state <<";"
					<< "vsnap_cleanup_message="
					<< errmsg <<";";
			}

			if(!rv)
			{
				break;
			}

			if(delmsg->cdpcleanup)
			{
				errmsg.clear();
				if(!m_dbname.empty())
				{
					rv = CDPUtil::CleanRetentionLog(m_dbname, errmsg);
				}

				if(rv)
				{
					cxmsg << "log_cleanup=yes;" 
						<< "log_cleanup_status="
						<< VOLUME_SETTINGS::LOG_CLEANUP_COMPLETE
						<< ";log_cleanup_message=0;";
				}
				else
				{
					cxmsg << "log_cleanup=yes;" 
						<< "log_cleanup_status="
						<< VOLUME_SETTINGS::LOG_CLEANUP_FAILED
						<< ";log_cleanup_message="
						<< errmsg
						<< ";" ;
				}
			}

		} while (0);
		if(CDPUtil::QuitRequested())
		{
			DebugPrintf(SV_LOG_DEBUG, "Service sent a quit signal. So ProcessPairDeletion aborted for %s."
				"Once the service started again it will ProcessPairDeletion.\n",dirname_r(m_dbname.c_str()).c_str());
			return false;
		}
		int cleanupStatusUpdateIntervalInSec = 180;
		do
		{
			// return success always
			bool success = true;
			if(!updateCleanUpActionStatus((*m_configurator),m_volname, cxmsg.str()))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to send updateCleanUpActionStatus for "
					"volume: %s. will be tried again after %d seconds.\n", m_volname.c_str(),
					cleanupStatusUpdateIntervalInSec);
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "Successfully sent the updateCleanUpActionStatus for volume: %s.\n",
					m_volname.c_str());
				break;
			}

			if(QuitRequested())
				break;

		}while(!CDPUtil::QuitRequested(cleanupStatusUpdateIntervalInSec));

		do
		{
			if(QuitRequested())
				break;

			if(!IsPairDeletionInPogress())
				break;
			DebugPrintf(SV_LOG_DEBUG, "Waiting for the pair deletion state to change for volume %s.\n",
				m_volname.c_str());

		}while(!CDPUtil::QuitRequested(IdleWaitTimeInSeconds));

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

	} catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_volname.c_str());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_volname.c_str());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_volname.c_str());
	}

	return rv;
}

bool ScheduleTask::MoveRetention(CdpMoveMsg * movemsg)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	try
	{
		do
		{
			// preparing the argument list for move retention to run from cdpcli 
			std::map<std::string, std::string> moveparams;
			moveparams.insert(std::make_pair("vol", m_volname));
			moveparams.insert(std::make_pair("oldretentiondir", movemsg->oldlocation));
			moveparams.insert(std::make_pair("newretentiondir", movemsg->newlocation));

			if(!CDPUtil::moveRetentionLog((*m_configurator), moveparams))
			{
				rv = false;
				break;
			}

		} while (false);
	}
	catch( std::string const& e ) 
	{
		DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n", FUNCTION_NAME, e.c_str() );
		rv = false;
	}
	catch( char const* e ) 
	{
		DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n",FUNCTION_NAME, e );
		rv = false;
	}
	catch( exception const& e ) 
	{
		DebugPrintf( SV_LOG_ERROR,"%s encountered exception %s\n",FUNCTION_NAME, e.what() );
		rv = false;
	}
	catch(...) 
	{
		DebugPrintf( SV_LOG_ERROR,"%s encountered unknown exception.\n" );
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool ScheduleTask::DisableRetention()
{
	bool rv = true;
	std::string dbname;
	std::string errmsg;

	try
	{
		DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

		do
		{
			rv = DeleteVsnapsForTargetVolume(m_volname,errmsg);
			if(!rv)
			{
				break;
			}

			if(!m_dbname.empty())
			{
				rv = CDPUtil::CleanRetentionLog(m_dbname, errmsg);
			}

		} while (0);

		DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

	} catch ( ContextualException& ce )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, ce.what(), m_volname.c_str());
	}
	catch( exception const& e )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s for %s.\n",
			FUNCTION_NAME, e.what(), m_volname.c_str());
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_volname.c_str());
	}

	return rv;
}

bool ScheduleTask::DeleteVsnapsForTargetVolume(const std::string & DeviceName, std::string & errorMessage)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, DeviceName.c_str());
	bool rv = true;

	VsnapMgr *vsnap;
#ifdef SV_WINDOWS
	WinVsnapMgr wmgr;
	vsnap = &wmgr;
#else
	UnixVsnapMgr umgr;
	vsnap = &umgr;
#endif

	errorMessage.clear();
	DebugPrintf(SV_LOG_DEBUG, "Cleaned up action of vsnaps for %s started.\n", DeviceName.c_str());

	bool nofail = true;
	rv = vsnap->UnMountVsnapVolumesForTargetVolume(DeviceName, nofail);
	if(!rv)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to delete vsnaps for %s.", DeviceName.c_str());
		errorMessage += "Some vsnaps for ";
		errorMessage += DeviceName;
		errorMessage += "could not be cleaned up.";
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Cleaned up action  of vsnaps for %s finished.\n", DeviceName.c_str());
	}
	vsnap->deletePendingPersistentVsnap(DeviceName);
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, DeviceName.c_str());
	return rv;
}

bool ScheduleTask::IsPairDeletionInPogress()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	try
	{
		InitialSettings settings = m_configurator -> getInitialSettings();
		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups 
			= settings.hostVolumeSettings.volumeGroups;
		HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter 
			= volumeGroups.begin();

		for(; vgiter != volumeGroups.end(); ++vgiter)
		{
			VOLUME_GROUP_SETTINGS vg = *vgiter;
			if(TARGET != vg.direction)
				continue;

			VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter 
				=  vg.volumes.find(m_volname);
			if (ctVolumeIter == vg.volumes.end())
			{
				continue;
			}
			string cleanup_action = ctVolumeIter ->second.cleanup_action;
			if(ctVolumeIter ->second.pairState == VOLUME_SETTINGS::DELETE_PENDING)
			{
				rv = true;
				break;
			}            
		}
	}
	catch ( ... )
	{
		rv = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception for %s.\n",
			FUNCTION_NAME, m_volname.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}
