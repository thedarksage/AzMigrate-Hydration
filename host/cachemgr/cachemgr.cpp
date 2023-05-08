// Copyright (c) 2008 InMage.
//
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : cachemgr.cpp
//
// Description:
//

#include <sstream>
#include <fstream>
using namespace std;

#include "cachemgr.h"
#include "cdputil.h"
#include "localconfigurator.h"
#include "inmageex.h"
#include "logger.h"
#include "cacheinfo.h"
#include "cdpplatform.h"
#include "configwrapper.h"
#include "file.h"
#include "decompress.h"

#include "basicio.h"
#include "error.h"

#include "HandlerCurl.h"

#include "validatesvdfile.h"
#include "inmcommand.h"
#include "inmalertdefs.h"

#include "LogCutter.h"

#include "AgentHealthIssueNumbers.h"

using namespace Logger;

extern Log gLog;

// message types from main thread to diffgettask
// CM_TIMEOUT: cachemgr has run for its timeslice.
// CM_QUIT: service quit request
// CM_DELETE: replication pair deletion
// CM_CHANGE: replication change notification
// CM_RESTARTRESYNC: resync has been re-started; cleanup cache

// message types from worker thread (rename task) to diffgettask
// CM_FETCHNEXTLIST: current set of files are all processed.
//                   fetch next list
//
static int const CM_TIMEOUT         = 0x2000;
static int const CM_QUIT            = 0x2001;
static int const CM_DELETE          = 0x2002;
static int const CM_CHANGE          = 0x2003;
static int const CM_RESTARTRESYNC   = 0x2004;
static int const CM_FETCHNEXTLIST   = 0x2005;
static int const CM_ERROR_INWORKER  = 0x2006;

// message priorities
static int const CM_ERROR_INWORKER_PRIORITY  = 0x01;
static int const CM_FETCHNEXTLIST_PRIORITY   = 0x02;
static int const CM_CHANGE_PRIORITY          = 0x03;
static int const CM_TIMEOUT_PRIORITY         = 0x04;
static int const CM_RESTARTRESYNC_PRIORITY   = 0x05;
static int const CM_DELETE_PRIORITY          = 0x06;
static int const CM_QUIT_PRIORITY            = 0x07;

// message types from diffgettask to worker threads
// CM_MSG_QUIT: service quit request
// CM_MSG_PROCESS: process the files
//
static int const CM_MSG_QUIT             = 0x2006;
static int const CM_MSG_PROCESS          = 0x2007;

// message priority
static int const CM_NORMAL_PRIORITY = 0x01;
static int const CM_HIGH_PRIORITY   = 0x10;

// return values for ScheduleDownloads routine
static int const CM_SCHEDULE_SUCCESS       = 0x00;
static int const CM_SCHEDULE_QUIT          = 0x01;
static int const CM_SCHEDULE_DELETE        = 0x02;
static int const CM_SCHEDULE_RESTARTRESYNC = 0x03;
static int const CM_SCHEDULE_TIMEOUT       = 0x04;
static int const CM_SCHEDULE_EXCEPTION     = 0x05;


// how long to wait for a message
static int const MsgWaitTimeInSeconds = 60;
static unsigned int IdleWaitTimeInSeconds = 30;

// delay between two successive attempts on encountering error
static unsigned int RetryDelayInSeconds = 30;

// max. no of retry attempts
static unsigned int MaxRetryAttempts = 10;


///
/// differential file name prefix
///
static const std::string diffMatchFiles = "completed_ediffcompleted_diff_*.dat*" ;
static const std::string tmpPrefix = "tmp_";
//Bug #8025
static const std::string tmpDir = "tmp";
static const std::string tmpMatchFiles  = "tmp_completed_ediffcompleted_diff_*.dat*";

static const std::string uncompressDir = "uncompressed";

static const std::string Tso_File_SubString = "_tso_";

static bool shouldCopyLogs = false;

/*
 * FUNCTION NAME :  CacheMgr::CacheMgr
 *
 * DESCRIPTION : CacheMgr constructor
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

CacheMgr::CacheMgr(int argc, char ** argv)
    :m_argc(argc), m_argv(argv),
     m_MaxAllowedDiskUsagePerPair(0),
     m_MaxInMemoryCompressedFileSize(0),
     m_MaxInMemoryUncompressedFileSize(0),
     m_MaxMemoryUsage(0),
     m_maxnwTasks(0),
     m_maxioTasks(0),
     m_reporterInterval(0),
     m_bCMVerifyDiff(false),
     m_bCMPreserveBadDiff(false),
     m_mincache_free_pct(100),
     m_mincache_free_bytes(0),
     m_bconfig(false),
     m_blocalLog(false)
{

}

/*
 * FUNCTION NAME :  CacheMgr::~CacheMgr
 *
 * DESCRIPTION : CacheMgr destructor
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
CacheMgr::~CacheMgr()
{

}

/*
 * FUNCTION NAME :  CacheMgr::init
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
bool CacheMgr::init()
{
    //
    // TODO: cachemgr is generally invoked by svagents
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

            if(!CDPUtil::InitQuitEvent(callerCli))
            {
                rv = false;
                break;
            }

            if (AnotherInstanceRunning())
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

            if(!FetchConfigurationValues())
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
 * FUNCTION NAME :  CacheMgr::uninit
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
bool CacheMgr::uninit()
{
    StopConfigurator();
    CDPUtil::UnInitQuitEvent();
    m_cdplock.reset();
    m_LogCutter->StopCutting();
    StopLocalLogging();
    CopyLocalLog();
    // TODO: not needed if using new transport
    tal::GlobalShutdown();
    return true;
}

/*
 * FUNCTION NAME :  CacheMgr::initializeTal
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
bool CacheMgr::initializeTal()
{
    bool rv = true;

    try
    {
        tal::GlobalInitialize();

        //
        // PR # 6337
        // set the curl_verbose option based on settings in
        // drscout.conf
        //
        tal::set_curl_verbose(m_lc.get_curl_verbose());

        Curl_setsendwindowsize(m_lc.getTcpSendWindowSize());
        Curl_setrecvwindowsize(m_lc.getTcpRecvWindowSize());
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
 * FUNCTION NAME :  CacheMgr::SetupLocalLogging
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
bool CacheMgr::SetupLocalLogging()
{
    bool rv = false;

    try
    {

        int logSize = m_lc.getLocalLogSize();
        m_LogPath = m_lc.getLogPathname();
        std::string sLogFileName = m_LogPath + "CacheMgr.log";
        SetLogLevel( m_lc.getLogLevel() );
        SetLogHostId( m_lc.getHostId().c_str() );
        SetLogInitSize(logSize);
        //
        // 4.3: remoteloglevel and sysloglevel were added
        //
        SetLogRemoteLogLevel(SV_LOG_DISABLE);
        SetSerializeType( m_lc.getSerializerType() ) ;
        SetLogHttpsOption(m_lc.IsHttps());

        SetDaemonMode();
        m_blocalLog = true;

        // The log cutter must complete before the CloseDebug() is invoked to close the log files.
        m_LogCutter = boost::make_shared<Logger::LogCutter>(Logger::LogCutter(gLog));
        const int maxCompletedLogFiles = 3;
        const boost::chrono::seconds cutInterval(300); // 5 mins.
        BOOST_ASSERT(m_LogCutter);
        m_LogCutter->Initialize(sLogFileName, maxCompletedLogFiles, cutInterval);

        if (m_LogCutter)
        {
            m_LogCutter->StartCutting();
        }

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
/*    FUNCTION NAME: CacheMgr::CopyLocalLog
 *
 *       DESCRIPTION : copy local logs
 *
 *
 *       INPUT PARAMETERS: none
 *
 *       OUTPUT PARAMETERS: none
 *
 *       NOTES   :
 *
 *
 *       return value: true on success, false otherwise
 */
bool CacheMgr::CopyLocalLog()
{
    bool rv = true;

    if(shouldCopyLogs)
    {
        std::string sLogFileName = m_LogPath + "CacheMgr.log";
        std::string sLogFileCopy = m_LogPath + "CacheMgr.log.copy";

        ACE_stat statbuf = {0};
        // PR#10815: Long Path support
        if(0 != sv_stat(getLongPathName(sLogFileCopy.c_str()).c_str(), &statbuf))
            rv = CDPUtil::CopyFile(sLogFileName, sLogFileCopy);
    }

    return rv;
}


/*
 * FUNCTION NAME :  CacheMgr::StopLocalLogging
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
bool CacheMgr::StopLocalLogging()
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
 * FUNCTION NAME :  CacheMgr::InitializeConfigurator
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

bool CacheMgr::InitializeConfigurator()
{
    bool rv = false;

    do
    {
        try
        {
            m_configurator = NULL;
            if(!::InitializeConfigurator(&m_configurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
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
                    "CacheMgr cannot run now. please try again\n");
        m_bconfig = false;
    }

    return rv;
}

/*
 * FUNCTION NAME :  CacheMgr::StopConfigurator
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

bool CacheMgr::StopConfigurator()
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
 * FUNCTION NAME :  CacheMgr::help
 *
 * DESCRIPTION : print the usage message
 *
 *
 * INPUT PARAMETERS :
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 *
 * return value : none
 *
 */
void CacheMgr::help()
{
    usage(m_argv[0],false);
}

/*
 * FUNCTION NAME :  CacheMgr::parseInput
 *
 * DESCRIPTION : parse the input and form the name/value pairs
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
bool CacheMgr::parseInput()
{
    bool rv = true;
    return rv;
}

/*
 * FUNCTION NAME :  CacheMgr::usage
 *
 * DESCRIPTION : display usage/command line syntax
 *
 *
 * INPUT PARAMETERS : progname - program name
 *                    error - if true, print to stderr
 *                            else print to stdout
 *
 * OUTPUT PARAMETERS : none
 *
 *
 * NOTES :
 *
 *
 * return value : none
 *
 */
void CacheMgr::usage(char * progname, bool error)
{
    stringstream out;

    out << "usage: " << progname << "\n\n";
    if(error)
    {
        DebugPrintf(SV_LOG_ERROR, "%s", out.str().c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "%s\n", out.str().c_str());
    }
}

/*
 * FUNCTION NAME :  CacheMgr::run
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
bool CacheMgr::run()
{
    bool rv = true;

    if(!init())
        return false;

    //
    // 1. Register callback with the configurator
    // 2. wait on quit event with timeout equal to the exit time
    // on exit:
    //   stop all the tasks
    //   perform cleanup action

    try
    {
        do
        {
            ConfigChangeCallback(m_configurator->getInitialSettings());
            m_configurator->getInitialSettingsSignal().connect( this, &CacheMgr::ConfigChangeCallback );
            if(!StartReporterTask())
            {
                // unhandled failure, let's us request for a safe shutdown
                DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StartReporterTask\n",FUNCTION_NAME);
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }


            // QuitRequested would return
            //  true : if quit signal was recieved
            //  false : if the timeslice expired
            bool quit = CDPUtil::QuitRequested(m_lc.getCacheMgrExitTime());

            // on timeout, we will send a timeout notification to all tasks
            // and wait for them to exit gracefully ie let them finish their current task
            // in between, if we get a quit request, we will send stop request to all the tasks
            while(!quit)
            {
                NotifyTimeOut();
                if(AllTasksExited())
                {
                    break;
                }
                quit = CDPUtil::QuitRequested(IdleWaitTimeInSeconds);
            }
        }while(false);
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

    m_configurator-> getInitialSettingsSignal().disconnect(this);
    StopAllTasks();

    if(!uninit())
        return false;

    return rv;
}

/*
 * FUNCTION NAME :  CacheMgr::AnotherInstanceRunning
 *
 * DESCRIPTION :  is another instance of cachemgr already running
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
bool CacheMgr::AnotherInstanceRunning()
{
    bool rv = false;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        m_cdplock.reset(new CDPLock("cachemgr.lck"));
        if(m_cdplock -> try_acquire())
        {
            rv = false;
        } else
        {
            DebugPrintf(SV_LOG_ERROR,"An another instance of %s is already running.\n",
                        m_argv[0]);
            
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
 * FUNCTION NAME :  CacheMgr::ConfigChangeCallback
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

void CacheMgr::ConfigChangeCallback(InitialSettings settings)
{
    bool rv = true;

    try
    {
        do
        {
            DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

            AutoGuard lock(m_lockTasks);
            if(!StartNewPairs(settings))
            {
                // unhandled failure, let's us request for a safe shutdown
                DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StartNewPairs\n",FUNCTION_NAME);
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }

            if(!SendChangeNotification(settings))
            {
                // unhandled failure, let's us request for a safe shutdown
                DebugPrintf(SV_LOG_ERROR,"%s encountered failure in SendChangeNotification\n",FUNCTION_NAME);
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }

            if(!ReapDeadTasks())
            {
                // waiting for the dead tasks failed. let's us request for a safe shutdown
                // otherwise may causes high memory usage
                DebugPrintf(SV_LOG_ERROR,"%s encountered failure in ReapDeadTasks\n",FUNCTION_NAME);
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
 * FUNCTION NAME :  CacheMgr::ReapDeadTasks
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
bool CacheMgr::ReapDeadTasks()
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

            DiffGetTasks_t::iterator iter = m_gettasks.begin();
            for( ; iter != m_gettasks.end(); ++iter)
            {
                DiffGetTaskPtr taskPtr = (*iter).second;
                if(taskPtr ->isDead())
                {
                    m_ThreadManager.wait_task(taskPtr.get());
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
 * FUNCTION NAME :  CacheMgr::SendChangeNotification
 *
 * DESCRIPTION :
 *     1. send quit signal to tasks for deleted replication pairs
 *     2. send change signal to existing pairs
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
bool CacheMgr::SendChangeNotification(InitialSettings & settings)
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        DiffGetTasks_t::iterator iter = m_gettasks.begin();
        for( ; iter != m_gettasks.end(); ++iter)
        {
            std::string volname = (*iter).first;
            DiffGetTaskPtr taskPtr = (*iter).second;
            if(isPairDeleted(settings, volname))
            {
                taskPtr -> PostMsg(CM_DELETE, CM_DELETE_PRIORITY);
            }
            else if(isTargetRestartResynced(settings,volname))
            {
                taskPtr -> PostMsg(CM_RESTARTRESYNC,CM_RESTARTRESYNC_PRIORITY);
            } else
            {
                taskPtr -> PostMsg(CM_CHANGE, CM_CHANGE_PRIORITY);
            }
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
 * FUNCTION NAME :  CacheMgr::StartNewPairs
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

bool CacheMgr::StartNewPairs(InitialSettings & settings)
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = settings.hostVolumeSettings.volumeGroups;
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter = volumeGroups.begin();
        for(; vgiter != volumeGroups.end(); ++vgiter)
        {
            VOLUME_GROUP_SETTINGS vg = *vgiter;
            if(TARGET != vg.direction)
                continue;

            VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter =  vg.volumes.begin();
            for (;ctVolumeIter != vg.volumes.end(); ctVolumeIter++)
            {
                std::string volname = ctVolumeIter -> first;
                DiffGetTasks_t::iterator getiter = m_gettasks.find(volname);
                if(m_gettasks.end() != getiter)
                {

                    DiffGetTaskPtr gettaskPtr = (*getiter).second;
                    if(gettaskPtr ->isDead())
                    {
                        m_ThreadManager.wait_task(gettaskPtr.get());
                        m_gettasks.erase(volname);
                    }

                    getiter = m_gettasks.find(volname);
                }

                if(m_gettasks.end() == getiter)
                {
                    bool launchGetTask = false;
                    bool launchForCleanup = false;

                    if(((ctVolumeIter->second.syncMode == VOLUME_SETTINGS::SYNC_DIFF)
                        ||(ctVolumeIter->second.syncMode == VOLUME_SETTINGS::SYNC_QUASIDIFF))
                       &&(ctVolumeIter->second.transportProtocol != TRANSPORT_PROTOCOL_FILE))
                    {
                        launchGetTask = true;
                        launchForCleanup = false;
                    }
                    else if( ((ctVolumeIter->second.syncMode != VOLUME_SETTINGS::SYNC_DIFF)
                              && (ctVolumeIter->second.syncMode != VOLUME_SETTINGS::SYNC_QUASIDIFF))
                             && (ctVolumeIter ->second.pairState == VOLUME_SETTINGS::RESTART_RESYNC_CLEANUP) )
                    {
                        launchGetTask = true;
                        launchForCleanup = true;
                    }

                    if(launchGetTask)
                    {
                        DiffGetTaskPtr getTask(new DiffGetTask(this, m_configurator, volname,
                                                               &m_ThreadManager, launchForCleanup));

                        if( -1 == getTask ->open())
                        {
                            DebugPrintf(SV_LOG_ERROR,
                                        "%s encountered error (%d) while creating getTask for volume %s\n",
                                        FUNCTION_NAME,  ACE_OS::last_error(), volname.c_str());
                            rv = false;
                            continue;
                        }

                        m_gettasks.insert(m_gettasks.end(), DiffGetTaskPair_t(volname,getTask));
                    }
                }
            }
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

bool CacheMgr::StartReporterTask()
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
 * FUNCTION NAME :  CacheMgr::isTargetRestartResynced
 *
 * DESCRIPTION : check if the pair is in resync and cleanup is requested
 *
 *
 * INPUT PARAMETERS : InitialSettings
 *                    volume name
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 *
 * return value : true if the pair is in resync and cleanup is requested, otherwise false.
 *
 */
bool CacheMgr::isTargetRestartResynced(InitialSettings & settings, const std::string & volname)
{
    bool rv = false;

    try
    {
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = settings.hostVolumeSettings.volumeGroups;
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter = volumeGroups.begin();
        for(; vgiter != volumeGroups.end(); ++vgiter)
        {
            VOLUME_GROUP_SETTINGS vg = *vgiter;
            if(TARGET != vg.direction)
                continue;

            VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter =  vg.volumes.find(volname);
            if (ctVolumeIter == vg.volumes.end())
            {
                continue;
            }
            if( ((ctVolumeIter->second.syncMode != VOLUME_SETTINGS::SYNC_DIFF)
                 && (ctVolumeIter->second.syncMode != VOLUME_SETTINGS::SYNC_QUASIDIFF))
                && (ctVolumeIter ->second.pairState == VOLUME_SETTINGS::RESTART_RESYNC_CLEANUP) )
            {
                rv = true;
                break;
            }
        }
    }

    // in case of exceptions, we will consider the pair is still there in diffsync and set rv = true
    // otherwise we may end up cleaning cache for an existing pair by mistake
    catch ( ContextualException& ce )
    {
        // unhandled failure (ambibious return value overload), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        // unhandled failure (ambibious return value overload), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        // unhandled failure (ambibious return value overload), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    return rv;
}

/*
 * FUNCTION NAME :  CacheMgr::isPairDeleted
 *
 * DESCRIPTION : check if the pair is deleted/undergoing deletion
 *
 *
 * INPUT PARAMETERS : InitialSettings
 *                    target volume name
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 *
 * return value : true if the pair is deleted/undergoing deletion, false otherwise
 *
 */
bool CacheMgr::isPairDeleted(InitialSettings & settings, const std::string & volname)
{
    bool rv = true;

    try
    {
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = settings.hostVolumeSettings.volumeGroups;
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter = volumeGroups.begin();
        for(; vgiter != volumeGroups.end(); ++vgiter)
        {
            VOLUME_GROUP_SETTINGS vg = *vgiter;
            if(TARGET != vg.direction)
                continue;

            VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter =  vg.volumes.find(volname);
            if (ctVolumeIter == vg.volumes.end())
            {
                continue;
            }

            // we found the pair and it is not undergoing deletion
            if(ctVolumeIter ->second.pairState != VOLUME_SETTINGS::DELETE_PENDING)
            {
                rv = false;
                break;
            }
        }
    }

    // in case of exceptions, we will consider the pair is still there and set rv = false
    // otherwise we may end up cleaning cache for an existing pair by mistake
    catch ( ContextualException& ce )
    {
        // unhandled failure (ambibious return value overload), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
    }
    catch( exception const& e )
    {
        // unhandled failure (ambibious return value overload), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        // unhandled failure (ambibious return value overload), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
    }

    return rv;
}

/*
 * FUNCTION NAME :  CacheMgr::StopAllTasks
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
bool CacheMgr::StopAllTasks()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        AutoGuard lock(m_lockTasks);
        DiffGetTasks_t::iterator iter = m_gettasks.begin();
        for( ; iter != m_gettasks.end(); ++iter)
        {
            DiffGetTaskPtr taskPtr = (*iter).second;
            taskPtr -> PostMsg(CM_QUIT, CM_QUIT_PRIORITY);
        }

        if(m_reporterTaskPtr)
        {
            m_reporterTaskPtr->PostMsg(CM_QUIT, CM_HIGH_PRIORITY);
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
 * FUNCTION NAME :  CacheMgr::NotifyTimeOut
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
bool CacheMgr::NotifyTimeOut()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

        AutoGuard lock(m_lockTasks);
        DiffGetTasks_t::iterator iter = m_gettasks.begin();
        for( ; iter != m_gettasks.end(); ++iter)
        {
            DiffGetTaskPtr taskPtr = (*iter).second;
            taskPtr -> PostMsg(CM_TIMEOUT, CM_TIMEOUT_PRIORITY);
        }

        if(m_reporterTaskPtr)
        {
            m_reporterTaskPtr->PostMsg(CM_TIMEOUT, CM_HIGH_PRIORITY);
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
 * FUNCTION NAME :  CacheMgr::AllTasksExited
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
bool CacheMgr::AllTasksExited()
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

            DiffGetTasks_t::iterator iter = m_gettasks.begin();
            if(iter == m_gettasks.end())
            {
                rv = true;
                break;
            }

            for( ; iter != m_gettasks.end(); ++iter)
            {
                DiffGetTaskPtr taskPtr = (*iter).second;
                if(!taskPtr ->isDead())
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
 * FUNCTION NAME :  CacheMgr::RemoveDeadTask
 *
 * DESCRIPTION :
 *     invoked when a getTask terminates
 *     remove the corresponding task from the getTasks map
 *
 *
 * INPUT PARAMETERS : volume name
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 *
 * return value : true on success, false otherwise
 *
 */
//bool CacheMgr::RemoveDeadTask(std::string & volname)
//{
//    bool rv = true;
//
//    try
//    {
//        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, volname.c_str());
//
//        AutoGuard lock(m_lockTasks);
//        m_gettasks.erase(volname);
//
//        //DiffGetTasks_t::iterator iter = m_gettasks.find(volname);
//        //if(m_gettasks.end() != iter)
//        //{
//        //    m_gettasks.erase(iter);
//        //}
//
//        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, volname.c_str());
//
//    }
//
//    catch ( ContextualException& ce )
//    {
//        rv = false;
//        DebugPrintf(SV_LOG_ERROR,
//            "\n%s encountered exception %s for %s.\n",
//            FUNCTION_NAME, ce.what(), volname.c_str());
//    }
//    catch( exception const& e )
//    {
//        rv = false;
//        DebugPrintf(SV_LOG_ERROR,
//            "\n%s encountered exception %s for %s.\n",
//            FUNCTION_NAME, e.what(), volname.c_str());
//    }
//    catch ( ... )
//    {
//        rv = false;
//        DebugPrintf(SV_LOG_ERROR,
//            "\n%s encountered unknown exception for %s.\n",
//            FUNCTION_NAME, volname.c_str());
//    }
//
//    return rv;
//}


bool CacheMgr::FetchConfigurationValues()
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
        m_MaxAllowedDiskUsagePerPair = m_lc.getMaxDiskUsagePerReplication();
        m_MaxInMemoryCompressedFileSize =     m_lc.getMaxInMemoryCompressedFileSize();
        m_MaxInMemoryUncompressedFileSize = m_lc.getMaxInMemoryUnCompressedFileSize();
        m_MaxMemoryUsage = m_lc.getMaxMemoryUsagePerReplication();
        m_maxnwTasks = m_lc.getNWThreadsPerReplication();
        m_maxioTasks = m_lc.getIOThreadsPerReplication();
        m_reporterInterval = m_lc.getPendingDataReporterInterval();
        m_bCMVerifyDiff = m_lc.getCMVerifyDiff();
        m_bCMPreserveBadDiff = m_lc.getCMPreserveBadDiff();
        m_difftgt_dir_prefix = m_lc.getDiffTargetCacheDirectoryPrefix();
        m_difftgt_fname_prefix = m_lc.getDiffTargetFilenamePrefix();
        m_diffsrc_dir_prefix = m_lc.getDiffTargetSourceDirectoryPrefix();
        m_hostid = m_lc.getHostId();
        m_mincache_free_pct = m_lc.getMinCacheFreeDiskSpacePercent();
        m_mincache_free_bytes = m_lc.getMinCacheFreeDiskSpace();
        RetryDelayInSeconds = m_lc.getCMRetryDelayInSeconds();
        MaxRetryAttempts = m_lc.getCMMaxRetries();
        IdleWaitTimeInSeconds = m_lc.getCMIdleWaitTimeInSeconds();

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

// DiffGetTask

/*
 * FUNCTION NAME :  DiffGetTask::open
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

int DiffGetTask::open(void *args)
{
    return this->activate(THR_BOUND);
}

/*
 * FUNCTION NAME :  DiffGetTask::close
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

int DiffGetTask::close(u_long flags)
{
    return 0;
}


/*
 * FUNCTION NAME :  DiffGetTask::PostMsg
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
void DiffGetTask::PostMsg(int msgId, int priority)
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
 * FUNCTION NAME :  DiffGetTask::svc
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

int DiffGetTask::svc()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        do
        {
            if(!FetchInitialSettings())
            {
                // unhandled failure, let's us request for a safe shutdown
                //DebugPrintf(SV_LOG_ERROR,"%s %s encountered failure in FetchSettings\n",
                //    FUNCTION_NAME, m_volname.c_str());
                CDPUtil::SignalQuit();

                rv = false;
                break;
            }

            if(m_launchedForCleanup)
            {
                if(!CleanCache())
                {
                    // ignore the failure
                }

                int cleanupStatusUpdateIntervalInSec = 180;
                do
                {
                    // return success always
                    bool success = true;
                    if(!updateRestartResyncCleanupStatus((*m_configurator),m_volname, success, "1"))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to send restart resync cleanup status for "
                                    "volume: %s. will be tried again after %d seconds.\n", m_volname.c_str(),
                                    cleanupStatusUpdateIntervalInSec);
                    }
                    else
                        break;
                }while(!CDPUtil::QuitRequested(cleanupStatusUpdateIntervalInSec));

                rv = true;
                break;
            }

            if(Protocol() == TRANSPORT_PROTOCOL_FILE)
            {
                rv = true;
                break;
            }

            try
            {
                m_cxTransport.reset(new CxTransport(Protocol(), m_transportSettings, Secure(), m_cfsPsData.m_useCfs, m_cfsPsData.m_psId));
                DebugPrintf(SV_LOG_DEBUG,"%s Created Cx Transport using PS ip: %s\n",    FUNCTION_NAME, m_transportSettings.ipAddress.c_str());
            }
            catch (std::exception const & e)
            {
                DebugPrintf(SV_LOG_ERROR,"%s create CxTransport failed: %s\n",    FUNCTION_NAME, e.what());
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR,"%s %s create CxTransport failed: unknonw error\n", FUNCTION_NAME);
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }

            if(!CreateWorkerThreads())
            {
                // unhandled failure, let's us request for a safe shutdown
                DebugPrintf(SV_LOG_ERROR,"%s %s encountered failure in CreateWorkerThreads\n",
                            FUNCTION_NAME, m_volname.c_str());
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }

            if(!CleanTmpFiles())
            {
                // ignore errors
                //rv = false;
                //break;
            }

            int rs = ScheduleDownload();

            if(CM_SCHEDULE_EXCEPTION == rs)
            {
                // unhandled failure, let's us request for a safe shutdown
                DebugPrintf(SV_LOG_ERROR, "%s %s encountered failure in ScheduleDownload\n",
                            FUNCTION_NAME, m_volname.c_str());

                CDPUtil::SignalQuit();
                rv = false;
                break;
            }
            else if((CM_SCHEDULE_QUIT == rs) || (CM_SCHEDULE_TIMEOUT == rs))
            {
                DebugPrintf(SV_LOG_DEBUG, "%s %s recieved quit/timeout in ScheduleDownload\n",
                            FUNCTION_NAME, m_volname.c_str());

                rv = true;
                break;
            }

            // return values CM_SCHEDULE_DELETE,CM_SCHEDULE_RESTARTRESYNC
            // for ScheduleDownload are handled
            // on reading the message

            bool timesliceexpired = false;

            while (true)
            {
                ACE_Message_Block *mb = NULL;
                if(!FetchNextMessage(&mb))
                {
                    // unhandled failure, let's us request for a safe shutdown
                    DebugPrintf(SV_LOG_ERROR, "%s %s encountered failure in FetchNextMessage\n",
                                FUNCTION_NAME, m_volname.c_str());
                    CDPUtil::SignalQuit();
                    rv = false;
                    break;
                }

                if(mb ->msg_type() == CM_TIMEOUT)
                {
                    timesliceexpired = true;
                    mb ->release();

                    if(Pause() || Throttle())
                    {
                        // PR#9634:
                        // As per the design, cachemgr finishes current tasks and will
                        // not schedule next download on expiry of time slice.
                        // However, when pair is in pause/target throttle state,
                        // the network threads may never finish the current download(s).
                        // So, this scenario has to be treated special as a quit
                        // request and force the threads to stop

                        DebugPrintf(SV_LOG_DEBUG,
                                    "Recieved time slice expire notification for volume %s.\n",
                                    m_volname.c_str());
                        DebugPrintf(SV_LOG_DEBUG,
                                    "Sending Quit request to worker threads for volume %s since it is in pause/throttle state\n",
                                    m_volname.c_str());

                        if(!StopWorkerThreads())
                        {
                            // nothing to do on this failure, quit is already set, just ignore
                        }
                        rv =true;
                        break;
                    }

                    // PR# 15609
                    // on timeout, we want the worker threads to exit asap after finish their curent
                    // task. For this, we are trimming the pending list so that new downloads are
                    // not triggered

                    TrimListOnTimeOut();

                    continue;
                }

                if(mb ->msg_type() == CM_QUIT)
                {
                    if(!StopWorkerThreads())
                    {
                        // nothing to do on this failure, quit is already set, just ignore
                    }

                    mb -> release();
                    rv = true;
                    break;
                }

                if(mb ->msg_type() == CM_DELETE)
                {
                    if(!StopWorkerThreads())
                    {
                        // unhandled failure, let's us request for a safe shutdown
                        DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StopWorkerThreads\n",
                                    FUNCTION_NAME);
                        CDPUtil::SignalQuit();
                    }

                    bool rc = CleanCache();
                    string info;
                    int sendCacheCleanupStatusIntervalInSec = 180;
                    do
                    {
                        if(rc)
                        {
                            // send cleanup success notification to cx
                            info = "0";
                            DebugPrintf(SV_LOG_DEBUG, "Sending cache cleanup success notification to cx for volume %s\n", m_volname.c_str());
                            if (!sendCacheCleanupStatus((*m_configurator), m_volname, true, info))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to update Cx with the cache clean up status for %s. will be tried again after %d seconds.", m_volname.c_str(), sendCacheCleanupStatusIntervalInSec);
                            }
                            else
                            {
                                DebugPrintf(SV_LOG_DEBUG, "Successfully sent cache cleanup succeeded notification to cx for volume %s\n", m_volname.c_str());
                                break;
                            }
                        } else
                        {
                            // send cleanup fail notification to cx
                            info = "some files in ";
                            info += CacheLocation();
                            info += "could not be cleaned up. please clean up manually.\n";
                            DebugPrintf(SV_LOG_DEBUG, "failed sending to cx %s for volum %s\n",info.c_str(), m_volname.c_str());
                            if(!sendCacheCleanupStatus((*m_configurator),m_volname, false, info))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Failed to update Cx with the cache clean up status for %s. will be tried again after %d seconds.", m_volname.c_str(),sendCacheCleanupStatusIntervalInSec);
                            }
                            else
                            {
                                DebugPrintf(SV_LOG_DEBUG, "Successfully sent cache cleanup failed notification to cx for volume %s\n", m_volname.c_str());
                                break;
                            }
                        }
                    }while(!CDPUtil::QuitRequested(sendCacheCleanupStatusIntervalInSec));
                    mb -> release();
                    rv = true;
                    break;
                }

                if(mb ->msg_type() == CM_CHANGE)
                {
                    if(!ConfigChange())
                    {
                        // unhandled failure, let's us request for a safe shutdown
                        //DebugPrintf(SV_LOG_ERROR,"%s encountered failure in FetchSettings\n",
                        //    FUNCTION_NAME);
                        CDPUtil::SignalQuit();
                    }

                    mb -> release();
                    continue;
                }

                if(mb ->msg_type() == CM_RESTARTRESYNC)
                {
                    if(!StopWorkerThreads())
                    {
                        // unhandled failure, let's us request for a safe shutdown
                        DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StopWorkerThreads\n",
                                    FUNCTION_NAME);
                        CDPUtil::SignalQuit();
                    }

                    if(!CleanCache())
                    {
                        // ignore the failure
                    }

                    int cleanupStatusUpdateIntervalInSec = 180;
                    do
                    {
                        // return success always
                        bool success = true;
                        if(!updateRestartResyncCleanupStatus((*m_configurator),m_volname, success, "1"))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Failed to send restart resync cleanup status for "
                                        "volume: %s. will be tried again after %d seconds.\n", m_volname.c_str(),
                                        cleanupStatusUpdateIntervalInSec);
                        }
                        else
                            break;
                    }while(!CDPUtil::QuitRequested(cleanupStatusUpdateIntervalInSec));

                    mb -> release();
                    rv = true;
                    break;
                }

                if((mb -> msg_type() == CM_FETCHNEXTLIST) ||
                   (mb -> msg_type() == CM_ERROR_INWORKER))
                {
                    if(timesliceexpired)
                    {
                        if(!StopWorkerThreads())
                        {
                            // unhandled failure, let's us request for a safe shutdown
                            DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StopWorkerThreads\n",
                                        FUNCTION_NAME);
                            CDPUtil::SignalQuit();
                        }

                        mb ->release();
                        rv = true;
                        break;

                    } else
                    {
                        if (mb -> msg_type() == CM_ERROR_INWORKER)
                        {
                            if(!StopWorkerThreads())
                            {
                                // unhandled failure, let's us request for a safe shutdown
                                DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StopWorkerThreads\n",
                                            FUNCTION_NAME);
                                CDPUtil::SignalQuit();
                                mb -> release();
                                rv = true;
                                break;
                            }

                            if(!CreateWorkerThreads())
                            {
                                // unhandled failure, let's us request for a safe shutdown
                                DebugPrintf(SV_LOG_ERROR,"%s %s encountered failure in CreateWorkerThreads\n",
                                            FUNCTION_NAME, m_volname.c_str());
                                CDPUtil::SignalQuit();
                                rv = false;
                                break;
                            }
                        }

                        if(!CleanTmpFiles())
                        {
                            // ignore errors
                            //rv = false;
                            //break;
                        }

                        rs = ScheduleDownload();

                        if(CM_SCHEDULE_SUCCESS == rs)
                        {
                            // nothing to be done, just continue
                        }
                        else if(CM_SCHEDULE_TIMEOUT == rs)
                        {
                            // worker threads will have to be stopped
                            DebugPrintf(SV_LOG_DEBUG, "%s %s recieved quit in ScheduleDownload\n",
                                        FUNCTION_NAME, m_volname.c_str());

                            if(!StopWorkerThreads())
                            {
                                // unhandled failure, let's us request for a safe shutdown
                                DebugPrintf(SV_LOG_ERROR,"%s encountered failure in StopWorkerThreads\n",
                                            FUNCTION_NAME);
                                CDPUtil::SignalQuit();
                            }

                            mb ->release();
                            rv = true;
                            break;
                        }
                        else if(CM_SCHEDULE_EXCEPTION == rs)
                        {
                            // unhandled failure, let's us request for a safe shutdown
                            // and process quit message in next round, asking the worker threads
                            // to quit
                            DebugPrintf(SV_LOG_ERROR,"%s %s encountered failure in ScheduleDownload\n",
                                        FUNCTION_NAME, m_volname.c_str());
                            CDPUtil::SignalQuit();
                        }
                        else if(CM_SCHEDULE_QUIT == rs)
                        {
                            // nothing to do, worker threads will be stopped in next round on
                            // fetching quit message.
                            DebugPrintf(SV_LOG_DEBUG, "%s %s recieved quit in ScheduleDownload\n",
                                        FUNCTION_NAME, m_volname.c_str());
                        }
                        else if(CM_SCHEDULE_DELETE == rs)
                        {
                            // nothing to do, worker threads will be stopped and
                            // cache will be cleaned in next round on fetching
                            // delete message.
                            DebugPrintf(SV_LOG_DEBUG, "%s %s recieved pair deletion in ScheduleDownload\n",
                                        FUNCTION_NAME, m_volname.c_str());
                        }
                        else if(CM_SCHEDULE_RESTARTRESYNC == rs)
                        {
                            // nothing to do, worker threads will be stopped and
                            // cache will be cleaned in next round on fetching
                            // restart message.
                            DebugPrintf(SV_LOG_DEBUG, "%s %s recieved pair restart in ScheduleDownload\n",
                                        FUNCTION_NAME, m_volname.c_str());
                        }

                        mb ->release();
                        continue;
                    }
                }

            }

        } while(0);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    StopWorkerThreads();
    this -> MarkDead();
    return 0;
}


/*
 * FUNCTION NAME :  DiffGetTask::FetchNextMessage
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
bool DiffGetTask::FetchNextMessage(ACE_Message_Block ** mb)
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        while(true)
        {
            m_cxTransport->heartbeat();

            ACE_Time_Value wait = ACE_OS::gettimeofday ();
            int waitSeconds = MsgWaitTimeInSeconds;
            wait.sec (wait.sec () + waitSeconds);
            if (-1 == this->msg_queue() -> dequeue_head(*mb, &wait))
            {
                if (errno == ESHUTDOWN)
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error (message queue closed) for %s\n",
                                FUNCTION_NAME, m_volname.c_str());

                    rv = false;
                    break;
                }
            }
            else
            {
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  DiffGetTask::AbortSchedule
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
 * return value : true if scheduledownload has to be aborted
 *
 */
bool DiffGetTask::AbortSchedule(int & reasonForAbort, int seconds)
{
    bool shouldAbort = false;

    try
    {
        ACE_Message_Block *mb;

        ACE_Time_Value wait = ACE_OS::gettimeofday ();
        wait.sec (wait.sec () + seconds);
        if (-1 == this->msg_queue()->peek_dequeue_head(mb, &wait))
        {
            if (errno == EWOULDBLOCK)
            {
                shouldAbort = false;
            } else
            {

                DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error (message queue closed) for %s\n",
                            FUNCTION_NAME, m_volname.c_str());

                // the message queue has been closed abruptly
                // let's us request for a safe shutdown

                CDPUtil::SignalQuit();
                shouldAbort= true;
                reasonForAbort = CM_SCHEDULE_EXCEPTION;
            }
        } else
        {

            switch(mb ->msg_type())
            {
                case CM_QUIT:
                    shouldAbort= true;
                    reasonForAbort = CM_SCHEDULE_QUIT;
                    break;
                case CM_DELETE:
                    shouldAbort= true;
                    reasonForAbort = CM_SCHEDULE_DELETE;
                    break;
                case CM_CHANGE:
                    if(!ConfigChange())
                    {
                        CDPUtil::SignalQuit();
                        shouldAbort= true;
                        reasonForAbort = CM_SCHEDULE_EXCEPTION;
                    } else
                    {
                        shouldAbort= false;
                        reasonForAbort = CM_SCHEDULE_SUCCESS;
                    }

                    break;
                case CM_RESTARTRESYNC:
                    shouldAbort= true;
                    reasonForAbort = CM_SCHEDULE_RESTARTRESYNC;
                    break;
                case CM_TIMEOUT:
                    shouldAbort= true;
                    reasonForAbort = CM_SCHEDULE_TIMEOUT;
                    break;
                default:
                    shouldAbort= false;
                    reasonForAbort = CM_SCHEDULE_SUCCESS;
                    // bug fix 7932 - start
                    CDPUtil::QuitRequested(IdleWaitTimeInSeconds);
                    // bug fix 7932 - end
                    break;
            }
        }
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldAbort= true;
        reasonForAbort = CM_SCHEDULE_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldAbort= true;
        reasonForAbort = CM_SCHEDULE_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldAbort= true;
        reasonForAbort = CM_SCHEDULE_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return shouldAbort;
}


/*
 * FUNCTION NAME :  DiffGetTask::QuitRequested
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
bool DiffGetTask::QuitRequested(int seconds)
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

                DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error (message queue closed) for %s\n",
                            FUNCTION_NAME, m_volname.c_str());

                // the message queue has been closed abruptly
                // let's us request for a safe shutdown

                CDPUtil::SignalQuit();
                shouldQuit= true;
            }
        } else
        {
            if((CM_QUIT == mb->msg_type()) ||(CM_DELETE == mb ->msg_type()))
            {
                shouldQuit = true;
            } else
            {
                shouldQuit = CDPUtil::QuitRequested(seconds);
            }
        }
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return shouldQuit;
}

/*
 * FUNCTION NAME :  DiffGetTask::Idle
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
void DiffGetTask::Idle(int seconds)
{
    m_cxTransport->heartbeat();
    (void)QuitRequested(seconds);
}

/*
 * FUNCTION NAME :  DiffGetTask::FetchSettings
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
 * return value : return true on success, false otherwise
 *
 */


bool DiffGetTask::FetchInitialSettings()
{
    bool rv = false;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        InitialSettings settings = m_configurator -> getInitialSettings();
        cfs::getCfsPsData(settings, m_volname, m_cfsPsData);
        m_transportSettings = settings.getTransportSettings(m_volname);
        m_transportInit = true;
        m_throttled = settings.shouldThrottleTarget(m_volname);

        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = settings.hostVolumeSettings.volumeGroups;
       
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter = volumeGroups.begin();
        for(; vgiter != volumeGroups.end(); ++vgiter)
        {
            VOLUME_GROUP_SETTINGS vg = *vgiter;
            if(TARGET != vg.direction)
                continue;

            VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter =  vg.volumes.find(m_volname);
            if (ctVolumeIter == vg.volumes.end())
            {
                continue;
            }

            m_paused = !(ctVolumeIter->second.shouldOperationRun("cachemgr"));
            m_transport = ctVolumeIter->second.transportProtocol;
            m_mode = ctVolumeIter->second.secureMode;

            m_cxLocation = m_cachemgr -> getDiffTargetSourceDirectoryPrefix();
            m_cxLocation += "/";
            m_cxLocation +=  m_cachemgr -> getHostId();
            m_cxLocation += '/';
            m_cxLocation += GetVolumeDirName(m_volname);

            m_cacheLocation = m_cachemgr -> getDiffTargetCacheDirectoryPrefix();
            m_cacheLocation += ACE_DIRECTORY_SEPARATOR_STR_A;
            m_cacheLocation += m_cachemgr -> getHostId();
            m_cacheLocation += ACE_DIRECTORY_SEPARATOR_STR_A;
            m_cacheLocation += GetVolumeDirName(m_volname);

            m_tmpCacheLocation = m_cacheLocation + ACE_DIRECTORY_SEPARATOR_CHAR_A;
            m_tmpCacheLocation += tmpDir;

            m_uncompressLocation = m_tmpCacheLocation + ACE_DIRECTORY_SEPARATOR_CHAR_A;
            m_uncompressLocation += uncompressDir;

            rv = true;
            break;
        }

        if(!rv)
        {
            DebugPrintf(SV_LOG_ERROR,
                        "\n%s encountered error. could not find the pair details for %s in settings received from CS.\n",
                        FUNCTION_NAME,  m_volname.c_str());
            return rv;
        }

        SVERROR sve = SVMakeSureDirectoryPathExists(m_tmpCacheLocation.c_str());
        if(sve.failed())
        {
            DebugPrintf(SV_LOG_ERROR,
                        "\n%s encountered error %s while creating directory %s.\n",
                        FUNCTION_NAME, sve.toString(), m_tmpCacheLocation.c_str());

            // unhandled failure, let's us request for a safe shutdown
            CDPUtil::SignalQuit();
            rv = false;
            return rv;
        }

        sve = SVMakeSureDirectoryPathExists(m_uncompressLocation.c_str());
        if(sve.failed())
        {
            DebugPrintf(SV_LOG_ERROR,
                        "\n%s encountered error %s while creating directory %s.\n",
                        FUNCTION_NAME, sve.toString(), m_uncompressLocation.c_str());

            CDPUtil::SignalQuit();
            rv = false;
            return rv;
        }

        GetDiskFreeSpaceP(m_cacheLocation, NULL, &m_Diskcapacity, NULL);
        SV_ULONGLONG pct = m_cachemgr -> getMinCacheFreeDiskSpacePercent();
        SV_ULONGLONG expectedFreeSpace1 = ((m_Diskcapacity * pct)/100);
        SV_ULONGLONG expectedFreeSpace2 = m_cachemgr  -> getMinCacheFreeDiskSpace();
        m_MinUnusedDiskSpace = min(expectedFreeSpace1,expectedFreeSpace2);

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

bool DiffGetTask::ConfigChange()
{
    bool rv = false;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        TRANSPORT_PROTOCOL transport = TRANSPORT_PROTOCOL_UNKNOWN;
        VOLUME_SETTINGS::SECURE_MODE transport_mode = VOLUME_SETTINGS::SECURE_MODE_NONE;

        AutoLock SetLock( m_SettingsLock );

        InitialSettings settings = m_configurator -> getInitialSettings();
        cfs::getCfsPsData(settings, m_volname, m_cfsPsData);

        TRANSPORT_CONNECTION_SETTINGS transportsettings = settings.getTransportSettings(m_volname);

        if(!(m_transportSettings == transportsettings))
        {
            DebugPrintf(SV_LOG_ERROR,
                        "\n%s note: this is not an error.Exiting cachemgr as the transport settings changed.\n",
                        FUNCTION_NAME);
            CDPUtil::SignalQuit();
            return false;
        }

        m_throttled = settings.shouldThrottleTarget(m_volname);

        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = settings.hostVolumeSettings.volumeGroups;
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter = volumeGroups.begin();
        for(; vgiter != volumeGroups.end(); ++vgiter)
        {
            VOLUME_GROUP_SETTINGS vg = *vgiter;
            if(TARGET != vg.direction)
                continue;

            VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter =  vg.volumes.find(m_volname);
            if (ctVolumeIter == vg.volumes.end())
            {
                continue;
            }

            m_paused = !(ctVolumeIter->second.shouldOperationRun("cachemgr"));
            transport = ctVolumeIter->second.transportProtocol;
            transport_mode = ctVolumeIter->second.secureMode;
            rv = true;
            break;
        }

        if(!rv)
        {
            DebugPrintf(SV_LOG_ERROR,
                        "\n%s encountered error. could not find the pair details for %s in settings received from CS.\n",
                        FUNCTION_NAME,  m_volname.c_str());
            return rv;
        }

        if((transport != m_transport) || (transport_mode != m_mode))
        {
            DebugPrintf(SV_LOG_ERROR,
                        "\n%s note: this is not an error.Exiting cachemgr as the transport settings changed. protocol values: (old :%d, new:%d), mode:(old:%d, new:%d)\n",
                        FUNCTION_NAME, m_transport, transport, m_mode, transport_mode);
            CDPUtil::SignalQuit();
            rv = false;
            return rv;
        }

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


bool  DiffGetTask::Throttle()
{
    AutoLock SetLock( m_SettingsLock );
    return m_throttled;
}

bool  DiffGetTask::Pause()
{
    AutoLock SetLock( m_SettingsLock );
    return m_paused;
}



/*
 * FUNCTION NAME :  DiffGetTask::CleanTmpFiles
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
 * return value : return true on success, false otherwise
 *
 */

bool DiffGetTask::CleanTmpFiles()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        SVERROR sve;

        //Bug #8025
        std::string tmpcachelocation = TmpCacheLocation();
        sve = CleanupDirectory(tmpcachelocation.c_str(), tmpMatchFiles.c_str());
        if (sve.failed())
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR,
                        "%s encountered error (%s) when performing cleanup on %s for %s\n",
                        FUNCTION_NAME, sve.toString(), tmpcachelocation.c_str(), m_volname.c_str());
        }

        std::string cachelocation = CacheLocation();
        sve = CleanupDirectory(cachelocation.c_str(), tmpMatchFiles.c_str());
        if (sve.failed())
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR,
                        "%s encountered error (%s) when performing cleanup on %s for %s\n",
                        FUNCTION_NAME, sve.toString(), cachelocation.c_str(), m_volname.c_str());
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  DiffGetTask::CleanCache
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
 * return value : return true on success, false otherwise
 *
 */

bool DiffGetTask::CleanCache()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());
        //Bug #8025
        string cachelocation = CacheLocation();
        string tmpCacheLocation = TmpCacheLocation();
        string uncompressLocation = UncompressLocation();

        SVERROR sve = CleanupDirectory(uncompressLocation.c_str(), tmpMatchFiles.c_str());
        if(sve.failed())
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR,
                        "%s encountered error (%s) when performing cleanup on %s for %s\n",
                        FUNCTION_NAME, sve.toString(), uncompressLocation.c_str(), m_volname.c_str());
        }


        sve = CleanupDirectory(tmpCacheLocation.c_str(), tmpMatchFiles.c_str());
        if (sve.failed())
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR,
                        "%s encountered error (%s) when performing cleanup on %s for %s\n",
                        FUNCTION_NAME, sve.toString(), tmpCacheLocation.c_str(), m_volname.c_str());
        }

        do
        {
            const std::string lockPath = cachelocation + ACE_DIRECTORY_SEPARATOR_CHAR_A + "pending_diffs.lck";
            // PR#10815: Long Path support
            ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
            ACE_Guard<ACE_File_Lock> guard(lock);

            if(!guard.locked())
            {
                DebugPrintf(SV_LOG_DEBUG,
                            "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                            FUNCTION_NAME,ACE_OS::last_error(),
                            ACE_OS::strerror(ACE_OS::last_error()),lockPath.c_str());
                rv = false;
                break;
            }

            sve = CleanupDirectory(cachelocation.c_str(), diffMatchFiles.c_str());
            if (sve.failed())
            {
                rv = false;
                DebugPrintf(SV_LOG_ERROR,
                            "%s encountered error (%s) when performing cleanup on %s for %s\n",
                            FUNCTION_NAME, sve.toString(), cachelocation.c_str(), m_volname.c_str());
            }
        }while(false);

        std::string fileInfo(CacheLocation() + ACE_DIRECTORY_SEPARATOR_CHAR_A + "deletepending.dat");
        // PR#10815: Long Path support
        if((ACE_OS::unlink(getLongPathName(fileInfo.c_str()).c_str()) < 0) && (ACE_OS::last_error() != ENOENT))
        {
            rv = false;
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                        FUNCTION_NAME,ACE_OS::last_error(), fileInfo.c_str());
        }

        std::string appliedInfoPath = CDPUtil::get_last_fileapplied_info_location(m_volname);
        std::string appliedInfoDir = dirname_r(appliedInfoPath.c_str());
        appliedInfoDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;

        std::string resyncRequiredFile = appliedInfoDir + "resyncRequired";
        // PR#10815: Long Path support
        if((ACE_OS::unlink(getLongPathName(resyncRequiredFile.c_str()).c_str()) < 0) && (ACE_OS::last_error() != ENOENT))
        {
            rv = false;
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                        FUNCTION_NAME,ACE_OS::last_error(), resyncRequiredFile.c_str());
        }

        std::string sourceSystemCrashedFile = appliedInfoDir + "sourceSystemCrashed";
        // PR#10815: Long Path support
        if((ACE_OS::unlink(getLongPathName(sourceSystemCrashedFile.c_str()).c_str()) < 0)
           && (ACE_OS::last_error() != ENOENT))
        {
            rv = false;
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                        FUNCTION_NAME,ACE_OS::last_error(), sourceSystemCrashedFile.c_str());
        }

        std::string appliedInfoFile = appliedInfoPath;
        // PR#10815: Long Path support
        if((ACE_OS::unlink(getLongPathName(appliedInfoFile.c_str()).c_str()) < 0)
           && (ACE_OS::last_error() != ENOENT))
        {
            rv = false;
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                        FUNCTION_NAME,ACE_OS::last_error(), appliedInfoFile.c_str());
        }

        appliedInfoFile += ".lck";
        // PR#10815: Long Path support
        if((ACE_OS::unlink(getLongPathName(appliedInfoFile.c_str()).c_str()) < 0)
           && (ACE_OS::last_error() != ENOENT))
        {
            rv = false;
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                        FUNCTION_NAME,ACE_OS::last_error(), appliedInfoFile.c_str());
        }


        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    }

    catch ( ContextualException& ce )
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



/*
 * FUNCTION NAME :  DiffGetTask::FetchPendingFileList
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
 * return value : return true on success, false otherwise
 *
 */

bool DiffGetTask::FetchPendingFileList()
{
    bool rv = true;

    try
    {
        do
        {
            DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

            m_FileList.clear();
            std::string fullLocation(CxLocation());
            fullLocation += '/';

            fullLocation += m_cachemgr -> getDiffTargetFilenamePrefix();

#ifdef SV_WINDOWS
            static __declspec(thread) long long lastNetworkErrLogTime = 0;
            static __declspec(thread) unsigned int networkFailCnt = 0;
#else
            static __thread long long lastNetworkErrLogTime = 0;
            static __thread unsigned int networkFailCnt = 0;
#endif 

            FileInfos_t fileInfos;
            if (!m_cxTransport->listFile(fullLocation, fileInfos))
            {
                ACE_Time_Value currentTime = ACE_OS::gettimeofday();
                LocalConfigurator localConfigurator;
                if (lastNetworkErrLogTime && ((currentTime.sec() - lastNetworkErrLogTime) > localConfigurator.getTransportErrorLogInterval()))
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "\n%s encountered error (%s) while fetching file list for %s from PS ip %s. Failures since %dsec=%d.\n",
                        FUNCTION_NAME, m_cxTransport->status(), m_volname.c_str(), m_transportSettings.ipAddress.c_str(),
                        localConfigurator.getTransportErrorLogInterval(), networkFailCnt);

                    lastNetworkErrLogTime = ACE_OS::gettimeofday().sec();
                    networkFailCnt = 0;

                    VxToPSCommunicationFailAlert a(m_transportSettings.ipAddress, m_transportSettings.port, m_transportSettings.sslPort,
                        GetStrTransportProtocol(Protocol()), Secure());
                    SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFERENTIALSYNC, a);
                }
                else if (!lastNetworkErrLogTime)
                {
                    lastNetworkErrLogTime = ACE_OS::gettimeofday().sec();
                }

                rv = false;
                break;
            }
            else
            {
                lastNetworkErrLogTime = 0;
                networkFailCnt = 0;
            }

            //
            // now copy the filelist data
            //
            if (!fileInfos.empty()) {
                FileInfos_t::iterator iter(fileInfos.begin());
                FileInfos_t::iterator iterEnd(fileInfos.end());
                for (/* empty */; iter != iterEnd; ++iter)
                {
                    CacheInfo::CacheInfoPtr cacheInfo(new CacheInfo((*iter).m_name, (*iter).m_size));
                    m_FileList.insert(cacheInfo);
                }
            }

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        } while(0);

    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  DiffGetTask::TrimToSize
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
 * return value : return true on success, false otherwise
 *
 */

bool DiffGetTask::TrimToSize(SV_ULONGLONG maxsize)
{
    bool rv = true;
    SV_ULONGLONG cummulativeSize = 0;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s " ULLSPEC "\n",FUNCTION_NAME, m_volname.c_str(), maxsize);

        CacheInfo::CacheInfosOrderedEndTime_t::iterator curIter = m_FileList.begin();
        CacheInfo::CacheInfosOrderedEndTime_t::iterator endIter = m_FileList.end();
        for ( ; curIter != endIter; ++curIter)
        {
            cummulativeSize += (*curIter) -> Size();

            if(cummulativeSize > maxsize)
            {
                m_FileList.erase(curIter, m_FileList.end());
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s " ULLSPEC "\n",FUNCTION_NAME, m_volname.c_str(), maxsize);
    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  DiffGetTask::TrimListOnTimeOut
 *
 * DESCRIPTION : on timeout, we want the worker threads
 *               to exit asap after finish their curent
 *               task. For this, we are trimming the
 *               pending list so that new downloads are
 *               not triggered
 *
 *
 * INPUT PARAMETERS : none
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 *
 * return value : return true on success, false otherwise
 *
 */
bool DiffGetTask::TrimListOnTimeOut()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    AutoLock SetDownloadIndexLock( m_nextdownloadLock );
    AutoLock SetWriteIndexLock( m_nextwriteLock );
    AutoLock SetRenameIndexock( m_nextrenameLock );

    size_t index = std::max( std::max(m_downloadindex, m_writeindex), m_renameindex);
    m_pendingfiles -> resize(index);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    return true;
}

/*
 * FUNCTION NAME :  DiffGetTask::BuildOrderedList
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
 * return value : return true on success, false otherwise
 *
 */

bool DiffGetTask::BuildOrderedList()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        CacheInfo::CacheInfosOrderedEndTime_t::iterator citer = m_FileList.begin();
        CacheInfo::CacheInfosOrderedEndTime_t::iterator cend = m_FileList.end();

        m_pendingfiles.reset(new std::vector<CacheInfo::CacheInfoPtr>);
        for(; citer != cend; ++citer)
        {
            m_pendingfiles ->push_back(*citer);
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
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
//Bug #8149

/*
 * FUNCTION NAME :  DiffGetTask::SkipAlreadyDownloadedFiles
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
 * return value : return true on success, false otherwise
 *
 */

bool DiffGetTask::SkipAlreadyDownloadedFiles()
{
    bool rv = true;

    try
    {
        do
        {
            DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

            std::string fileInfo(CacheLocation() + ACE_DIRECTORY_SEPARATOR_CHAR_A + "deletepending.dat");


            std::ifstream skipfiles(fileInfo.c_str());
            if(skipfiles.good())
            {
                std::string skipfile;
                skipfiles >> skipfile;
                if(!skipfiles.good() && !skipfiles.eof())
                {
                    DebugPrintf(SV_LOG_ERROR, "\n%s:encountered error while retrieving filename from %s\n",
                                FUNCTION_NAME, fileInfo.c_str());
                    rv = false;
                    break;
                }

                CacheInfo::CacheInfosOrderedList_t::iterator iter = m_pendingfiles->begin();
                CacheInfo::CacheInfosOrderedList_t::iterator endIter = m_pendingfiles->end();

                for(; iter != endIter; ++iter)
                {
                    if((*iter)->Name() == skipfile)
                    {
                        m_pendingfiles -> erase(std::remove(m_pendingfiles->begin(),
                                                            m_pendingfiles->end(), (*iter)), m_pendingfiles -> end());
                        break;
                    }
                }
            }

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
        } while (0);
    }

    catch ( ContextualException& ce )
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


/*
 * FUNCTION NAME :  CacheMgr::CurrentDiskUsage
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
 * return value : return true on success, false otherwise
 *
 */

SV_ULONGLONG CacheMgr::CurrentDiskUsage( std::string &cachelocation )
{
    bool rv = true;
    SV_ULONGLONG totalUsage = 0;
    SV_ULONGLONG curUsage = 0;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, cachelocation.c_str());

        do
        {
            // open directory
            // PR#10815: Long Path support
            ACE_DIR *dirp = sv_opendir(cachelocation.c_str());

            if (dirp == NULL)
            {
                int lasterror = ACE_OS::last_error();
                if (ENOENT == lasterror || ESRCH == lasterror)
                {
                    rv = true;
                    break;
                } else
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error %d while reading from %s\n", FUNCTION_NAME,
                                lasterror, cachelocation.c_str());
                    rv = false;
                    break;
                }
            }

            struct ACE_DIRENT *dp = NULL;

            do
            {
                ACE_OS::last_error(0);

                // get directory entry
                if ((dp = ACE_OS::readdir(dirp)) != NULL)
                {
                    // find a matching entry
                    // PR#10815: Long Path support
                    std::string fileName = ACE_TEXT_ALWAYS_CHAR(dp->d_name);
                    if (RegExpMatch(diffMatchFiles.c_str(), fileName.c_str()))
                    {
                        string filePath;

                        filePath = cachelocation.c_str();
                        filePath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                        filePath += fileName;

                        ACE_stat s;
                        // PR#10815: Long Path support
                        curUsage = ( sv_stat( getLongPathName(filePath.c_str()).c_str(), &s ) < 0 ) ? 0 : s.st_size;
                        totalUsage += curUsage;
                    }
                }

            } while(dp);

            // close directory
            ACE_OS::closedir(dirp);

        } while(false);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, cachelocation.c_str());
    }
    catch ( ContextualException& ce )
    {
        // unhandled failure (overloaded return value), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), cachelocation.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure (overloaded return value), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), cachelocation.c_str());
    }
    catch ( ... )
    {
        // unhandled failure (overloaded return value), let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, cachelocation.c_str());
    }

    // TBD: we should avoid overloading return value with output
    return totalUsage;
}

/*
 * FUNCTION NAME :  DiffGetTask::AllowedDiskUsage
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
 * return value : return true on success, false otherwise
 *
 */

SV_ULONGLONG DiffGetTask::AllowedDiskUsage()
{
    SV_ULONGLONG MaxUsage = MaxAllowedDiskUsage();
    std::string cacheLocation = CacheLocation();
    SV_ULONGLONG CurUsage = m_cachemgr->CurrentDiskUsage(cacheLocation);
    if(CurUsage < MaxUsage)
    {
        return (MaxUsage - CurUsage);
    } else
    {
        return 0;
    }
}

/*
 * FUNCTION NAME :  DiffGetTask::CreateWorkerThreads
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
 * return value : return true on success, false otherwise
 *
 */
bool DiffGetTask::CreateWorkerThreads()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        for(SV_UINT i = 0 ; i < m_cachemgr ->MaxNwTasks(); ++i)
        {
            NWTaskPtr nwTask(new NWTask(m_cachemgr,this, m_configurator,m_volname, &m_workerThreadMgr, m_cfsPsData));
            nwTask->open();
            m_nmtasks.push_back(nwTask);
        }

        for(SV_UINT i = 0 ; i < m_cachemgr ->MaxIoTasks(); ++i)
        {
            IOTaskPtr ioTask(new IOTask(m_cachemgr,this, m_configurator,m_volname, &m_workerThreadMgr));
            ioTask->open();
            m_iotasks.push_back(ioTask);
        }

        RenameTaskPtr renameTask(new RenameTask(m_cachemgr,this, m_configurator,m_volname, &m_workerThreadMgr, m_cfsPsData));
        renameTask -> open();
        m_renametasks.push_back(renameTask);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  DiffGetTask::StopWorkerThreads
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
 * return value : return true on success, false otherwise
 *
 */
bool DiffGetTask::StopWorkerThreads()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        NWTasks_t::iterator nwiter = m_nmtasks.begin();
        NWTasks_t::iterator nwend = m_nmtasks.end();

        for(; nwiter != nwend; ++nwiter)
        {
            NWTaskPtr nwtask = *nwiter;
            nwtask -> PostMsg(CM_MSG_QUIT, CM_HIGH_PRIORITY);
        }

        IOTasks_t::iterator ioiter = m_iotasks.begin();
        IOTasks_t::iterator ioend = m_iotasks.end();

        for(; ioiter != ioend; ++ioiter)
        {
            IOTaskPtr iotask = *ioiter;
            iotask -> PostMsg(CM_MSG_QUIT, CM_HIGH_PRIORITY);
        }

        RenameTasks_t::iterator reniter = m_renametasks.begin();
        RenameTasks_t::iterator renend = m_renametasks.end();

        for(; reniter != renend; ++reniter)
        {
            RenameTaskPtr rentask = *reniter;
            rentask -> PostMsg(CM_MSG_QUIT, CM_HIGH_PRIORITY);
        }

        m_workerThreadMgr.wait();

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  DiffGetTask::ScheduleDownload
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
 * return value : return true on success, false otherwise
 *
 */
int DiffGetTask::ScheduleDownload()
{
    int rv = CM_SCHEDULE_SUCCESS;
    int reasonForAbort = 0;

    try
    {

        //__asm int 3;
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        while(true)
        {
            if(AbortSchedule(reasonForAbort))
            {
                rv = reasonForAbort;
                break;
            }

            if(!FetchPendingFileList())
            {
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(m_FileList.empty())
            {
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(!TrimToSize(AllowedDiskUsage()))
            {
                CDPUtil::SignalQuit();
                rv = CM_SCHEDULE_EXCEPTION;
                break;
            }

            if(m_FileList.empty())
            {
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(!BuildOrderedList())
            {
                CDPUtil::SignalQuit();
                rv = CM_SCHEDULE_EXCEPTION;
                break;
            }

            // bug 8149
            if(!SkipAlreadyDownloadedFiles())
            {
                CDPUtil::SignalQuit();
                rv = CM_SCHEDULE_EXCEPTION;
                break;
            }

            if(m_pendingfiles->empty())
            {
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            m_downloadindex = 0;
            m_writeindex = 0;
            m_renameindex = 0;


            NWTasks_t::iterator nwiter = m_nmtasks.begin();
            NWTasks_t::iterator nwend = m_nmtasks.end();

            for(; nwiter != nwend; ++nwiter)
            {
                NWTaskPtr nwtask = *nwiter;
                nwtask -> PostMsg(CM_MSG_PROCESS, CM_NORMAL_PRIORITY);
            }

            IOTasks_t::iterator ioiter = m_iotasks.begin();
            IOTasks_t::iterator ioend = m_iotasks.end();

            for(; ioiter != ioend; ++ioiter)
            {
                IOTaskPtr iotask = *ioiter;
                iotask -> PostMsg(CM_MSG_PROCESS, CM_NORMAL_PRIORITY);
            }


            RenameTasks_t::iterator reniter = m_renametasks.begin();
            RenameTasks_t::iterator renend = m_renametasks.end();

            for(; reniter != renend; ++reniter)
            {
                RenameTaskPtr rentask = *reniter;
                rentask -> PostMsg(CM_MSG_PROCESS, CM_NORMAL_PRIORITY);
            }

            rv = CM_SCHEDULE_SUCCESS;
            break;
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
    {
        rv = CM_SCHEDULE_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        rv = CM_SCHEDULE_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        rv = CM_SCHEDULE_EXCEPTION;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return rv;
}

/*
 * FUNCTION NAME :  DiffGetTask::FileFitsInMemory
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
 * return value : return true on success, false otherwise
 *
 */
bool DiffGetTask::FileFitsInMemory(CacheInfo::CacheInfoPtr cacheinfoPtr)
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s %s %lu\n",FUNCTION_NAME, m_volname.c_str(),
                    cacheinfoPtr -> Name().c_str(), cacheinfoPtr -> Size());

        do
        {
            AutoLock SetLock( m_MemoryUsageLock );

            if( cacheinfoPtr -> isCompressed())
            {
                if(cacheinfoPtr -> Size() > m_cachemgr ->MaxInMemoryCompressedFileSize())
                {
                    rv = false;
                    break;
                }

            } else
            {
                if(cacheinfoPtr -> Size() > m_cachemgr ->MaxInMemoryUnCompressedFileSize())
                {
                    rv = false;
                    break;
                }
            }

            if (m_MemoryUsage + cacheinfoPtr -> Size() <= m_cachemgr ->MaxMemoryUsage())
            {
                m_MemoryUsage += cacheinfoPtr -> Size();
                DebugPrintf(SV_LOG_DEBUG,
                            "Volume:%s InMemory Usage: %lu MB\n", m_volname.c_str(), (m_MemoryUsage/(1024*1024)));
                rv = true;
                break;
            }

            // file does not fit in memory
            rv = false;
            break;

        } while(false);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s %s %lu\n",FUNCTION_NAME, m_volname.c_str(),
                    cacheinfoPtr -> Name().c_str(), cacheinfoPtr -> Size());
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return rv;
}

/*
 * FUNCTION NAME :  DiffGetTask::AdjustMemoryUsage
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
 * return value : return true on success, false otherwise
 *
 */
void DiffGetTask::AdjustMemoryUsage(bool decrement, SV_ULONG size)
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s %lu\n",FUNCTION_NAME, m_volname.c_str(),size);

        do
        {
            AutoLock SetLock( m_MemoryUsageLock );
            if(decrement)
                m_MemoryUsage -= size;
            else
                m_MemoryUsage += size;

        } while (false);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s %lu\n",FUNCTION_NAME, m_volname.c_str(),size);
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }
}

/*
 * FUNCTION NAME :  DiffGetTask::DiskSpaceAvailable
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
 * return value : return true on success, false otherwise
 *
 */
bool DiffGetTask::DiskSpaceAvailable(SV_ULONG size, bool sendalert)
{
    bool rv = true;

    try
    {

        // TBD: should we make this a critical section ie only one thread allowed?
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s %lu\n",FUNCTION_NAME, m_volname.c_str(),size);

        SV_ULONGLONG availableSpace;
        string cachelocation = CacheLocation();
        GetDiskFreeCapacity(cachelocation, availableSpace);
        if(availableSpace - size > MinUnusedDiskSpace())
        {
            rv = true;
        } else
        {
            rv = false;

            CacheDirLowAlert a(cachelocation, m_Diskcapacity, availableSpace, MinUnusedDiskSpace());
            DebugPrintf(SV_LOG_DEBUG,"%s", a.GetMessage().c_str());

            if(sendalert)
            {
                SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFERENTIALSYNC, a);
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s %lu\n",FUNCTION_NAME, m_volname.c_str(),size);
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return rv;
}

/*
 * FUNCTION NAME :  DiffGetTask::FetchNextDiffForDownload
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
CacheInfo::CacheInfoPtr DiffGetTask::FetchNextDiffForDownload(bool & done)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    AutoLock SetNextDownloadLock( m_nextdownloadLock );
    size_t index = 0;

    if(m_downloadindex >= m_pendingfiles -> size())
    {
        done = true;
        index = 0;
        return  CacheInfo::CacheInfoPtr();
    } else
    {
        done = false;
        index = m_downloadindex;
        ++m_downloadindex;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    return (*m_pendingfiles.get())[index];
}

/*
 * FUNCTION NAME :  DiffGetTask::FetchNextDiffForWrite
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
CacheInfo::CacheInfoPtr DiffGetTask::FetchNextDiffForWrite(bool & done)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    AutoLock SetNextDownloadLock( m_nextwriteLock );
    size_t index = 0;

    if(m_writeindex >= m_pendingfiles -> size())
    {
        done = true;
        index = 0;
        return  CacheInfo::CacheInfoPtr();
    } else
    {
        done = false;
        index = m_writeindex;
        ++m_writeindex;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    return (*m_pendingfiles.get())[index];
}

/*
 * FUNCTION NAME :  DiffGetTask::FetchNextDiffForRename
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
CacheInfo::CacheInfoPtr DiffGetTask::FetchNextDiffForRename(bool & done)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    AutoLock SetNextDownloadLock( m_nextrenameLock );
    size_t index = 0;

    if(m_renameindex >= m_pendingfiles -> size())
    {
        done = true;
        index = 0;
        return  CacheInfo::CacheInfoPtr();
    } else
    {
        done = false;
        index = m_renameindex;
        ++m_renameindex;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    return (*m_pendingfiles.get())[index];
}

// NWTask

/*
 * FUNCTION NAME :  NWTask::open
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
int NWTask::open(void *args)
{
    return this->activate(THR_BOUND);
}

/*
 * FUNCTION NAME :  NWTask::close
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
int NWTask::close(u_long flags)
{
    return 0;
}

/*
 * FUNCTION NAME :  NWTask::svc
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
int NWTask::svc()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        do
        {
            TRANSPORT_CONNECTION_SETTINGS settings(m_configurator ->getTransportSettings(m_volname));

            try
            {
                m_cxTransport.reset(new CxTransport(m_gettask ->Protocol(), settings, m_gettask->Secure(), m_cfsPsData.m_useCfs, m_cfsPsData.m_psId));
                DebugPrintf(SV_LOG_DEBUG,"%s Created Cx Transport using PS ip: %s\n",    FUNCTION_NAME, settings.ipAddress.c_str());
            }
            catch (std::exception const & e)
            {
                DebugPrintf(SV_LOG_ERROR,"%s create CxTransport failed: %s\n",    FUNCTION_NAME, e.what());
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR,"%s %s create CxTransport failed: unknonw error\n", FUNCTION_NAME);
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }

            m_bCMVerifyDiff = m_cachemgr -> getCMVerifyDiff();
            m_bCMPreserveBadDiff = m_cachemgr -> getCMPreserveBadDiff();
            while (true)
            {
                ACE_Message_Block *mb = NULL;
                if(!FetchNextMessage(&mb))
                {
                    // unhandled failure, let's us request for a safe shutdown
                    DebugPrintf(SV_LOG_ERROR,"%s encountered failure in FetchNextMessage\n",FUNCTION_NAME);
                    CDPUtil::SignalQuit();
                    rv = false;
                    break;
                }

                if(mb ->msg_type() == CM_MSG_QUIT)
                {
                    mb -> release();
                    rv = false;
                    break;
                }

                if(mb ->msg_type() == CM_MSG_PROCESS)
                {
                    if(!DownloadRequestedFiles())
                    {

                        DebugPrintf(SV_LOG_ERROR,"%s encountered failure in DownloadRequestedFiles\n",
                                    FUNCTION_NAME);

                        mb -> release();
                        Idle(IdleWaitTimeInSeconds);
                        m_gettask ->PostMsg(CM_ERROR_INWORKER, CM_ERROR_INWORKER_PRIORITY);
                        rv = false;
                        break;
                    }
                }

                mb -> release();
            }

        } while(0);


        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return 0;
}

/*
 * FUNCTION NAME :  NWTask::PostMsg
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
void NWTask::PostMsg(int msg, int priority)
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
 * FUNCTION NAME :  NWTask::FetchNextMessage
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
bool NWTask::FetchNextMessage(ACE_Message_Block ** mb)
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        while(true)
        {
            m_cxTransport->heartbeat();

            ACE_Time_Value wait = ACE_OS::gettimeofday ();
            int waitSeconds = MsgWaitTimeInSeconds;
            wait.sec (wait.sec () + waitSeconds);
            if (-1 == this->msg_queue() -> dequeue_head(*mb, &wait))
            {
                if (errno == ESHUTDOWN)
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error (message queue closed) for %s\n",
                                FUNCTION_NAME, m_volname.c_str());

                    rv = false;
                    break;
                }
            }
            else
            {
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  NWTask::QuitRequested
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
bool NWTask::QuitRequested(int seconds)
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
                DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error (message queue closed) for %s\n",
                            FUNCTION_NAME, m_volname.c_str());

                // the message queue has been closed abruptly
                // let's us request for a safe shutdown

                CDPUtil::SignalQuit();
                shouldQuit= true;
            }
        } else
        {
            if(CM_MSG_QUIT == mb -> msg_type())
            {
                shouldQuit = true;
            } else
            {
                shouldQuit = CDPUtil::QuitRequested(seconds);
            }
        }
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return shouldQuit;
}

/*
 * FUNCTION NAME :  NWTask::Idle
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
void NWTask::Idle(int seconds)
{
    m_cxTransport->heartbeat();
    (void)QuitRequested(seconds);
}


/*
 * FUNCTION NAME :  NWTask::DownloadRequestedFiles
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
bool NWTask::DownloadRequestedFiles()
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());



        bool done = false;

        while(true)
        {

            CacheInfo::CacheInfoPtr cacheinfoPtr =  m_gettask ->FetchNextDiffForDownload(done);
            if(done || !cacheinfoPtr)
            {
                rv = true;
                break;
            }
            //Bug #8149
            if(!DownloadFile(cacheinfoPtr))
            {
                rv = false;
                break;
            }

            // reset the bytes downloaded counter
            // this is to allow disk usage checks per 64 mb downloads
            if(m_bytesDownloaded > 67108864)
            {
                m_bytesDownloaded = 0;
            }

            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  NWTask::DownloadFile
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
bool NWTask::DownloadFile(CacheInfo::CacheInfoPtr cacheinfoPtr)
{
    bool rv = true;
    SV_ULONG attempts = 0;
    bool sendalert = true;
    bool bdownloaded = false;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

        std::string remoteLocation = m_gettask -> CxLocation();
        remoteLocation += '/';
        remoteLocation += cacheinfoPtr -> Name();

        //Bug #8025
        std::string localFileName = m_gettask ->TmpCacheLocation();
        localFileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        localFileName += tmpPrefix;
        localFileName += cacheinfoPtr -> Name();

        std::string finalFileName = m_gettask ->CacheLocation();
        finalFileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        finalFileName += cacheinfoPtr -> Name();


        // check if the file was already downloaded in previous
        // run, but deletion on cx did not happen
        ACE_stat statbuf = {0};
        // PR#10815: Long Path support
        if(0 == sv_stat(getLongPathName(finalFileName.c_str()).c_str(), &statbuf))
        {
            m_bytesDownloaded += cacheinfoPtr ->Size();
            cacheinfoPtr -> SetReadyToDeleteFromPS();
            bdownloaded = true;
            rv = true;
        } else
        {
            std::string::size_type idx = finalFileName.rfind(".gz");
            if (std::string::npos == idx || (finalFileName.length() - idx) > 3)
            {
                bdownloaded = false;
            } else
            {
                finalFileName.erase(idx, finalFileName.length());
                // PR#10815: Long Path support
                if(0 == sv_stat(getLongPathName(finalFileName.c_str()).c_str(), &statbuf))
                {
                    m_bytesDownloaded += cacheinfoPtr ->Size();
                    cacheinfoPtr -> SetReadyToDeleteFromPS();
                    bdownloaded = true;
                    rv = true;
                }
            }
        }

        while (!bdownloaded)
        {
            // TBD: should we avoid downloading tso files?

            if( cacheinfoPtr ->Size() == 0 )
            {
                std::stringstream msg;
                std::string corruptFileName = m_gettask -> CxLocation();
                corruptFileName += '/';
                corruptFileName += "corrupt_" ;
                corruptFileName += cacheinfoPtr -> Name();
                const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
                msg << "Differential file " << remoteLocation << " is zero bytes, "
                    << "renaming it to " << corruptFileName
                    << ". Marking resync for the target device " << m_volname << " with resyncReasonCode=" << resyncReasonCode;
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                ResyncReasonStamp resyncReasonStamp;
                STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

                // step 1
                if(!CDPUtil::store_and_sendcs_resync_required(m_volname, msg.str(), resyncReasonStamp))
                {
                    rv = false;
                    break;
                }

                if(!RenameRemoteFile(remoteLocation, corruptFileName))
                {
                    rv = false;
                    break;
                }
                cacheinfoPtr -> SetDeletedFromPS();
            }

            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

            if(!cacheinfoPtr -> isDownLoadRequired())
            {
                DebugPrintf(SV_LOG_DEBUG, "download is not required for %s.\n",
                            finalFileName.c_str());
                rv = true;
                break;
            }

            if(m_gettask ->Throttle())
            {
                DebugPrintf(SV_LOG_DEBUG, "%s is in throttled state\n", m_volname.c_str());
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(m_gettask ->Pause())
            {
                DebugPrintf(SV_LOG_DEBUG, "%s is in pause state\n", m_volname.c_str());
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(m_gettask ->FileFitsInMemory(cacheinfoPtr))
            {

                char * buffer = 0;
                SV_ULONG bufsize = cacheinfoPtr ->Size() +1;

                // need the cast to quiet the compiler when building 32 bit and have 64 bit warnings
                // enabled. as when building for 32 bit and checking against 64 bit, the compiler
                // uses the 32 bit type for size_t not its 64 bit type (which is big enough to hold
                // an SV_ULONG)
                if(m_cxTransport->getFile(remoteLocation, bufsize, &buffer, reinterpret_cast<std::size_t&>(bufsize)))
                {
                    cacheinfoPtr ->SetReadyForWriteToDisk(buffer);
                    bdownloaded = true;
                    rv = true;
                    break;
                } else
                {
                    m_gettask ->AdjustMemoryUsage(true, cacheinfoPtr ->Size());
                }
            }

            // we check for disk space availability
            //  for every 64 mb chunk download
            //  or on encountering failure to download the file
            // however low disk space alert will be sent only once
            // to avod unncessary flooding of messages to cx
            //
            if(((m_bytesDownloaded > 67108864) || attempts ) &&
               (!m_gettask ->DiskSpaceAvailable(cacheinfoPtr ->Size(),sendalert)))
            {
                sendalert = false;
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(m_cxTransport->getFile(remoteLocation, localFileName))
            {
                ACE_stat statbuf = {0};
                // PR#10815: Long Path support
                if(0 == sv_stat(getLongPathName(localFileName.c_str()).c_str(), &statbuf))
                {
                    if(statbuf.st_size == cacheinfoPtr ->Size())
                    {
                        m_bytesDownloaded += cacheinfoPtr ->Size();
                        cacheinfoPtr ->SetReadyToVerify();
                        bdownloaded = true;
                        rv = true;
                        break;
                    }
                    else
                    {
                        std::stringstream msg;
                        msg << FUNCTION_NAME << " encountered error: "
                            << "GetRemoteDataToFile() returned success for " << localFileName.c_str()
                            << " but the file contents are not proper.\nActual size of the file: "
                            << cacheinfoPtr ->Size() << ", size on disk: " << statbuf.st_size << "\n";
                        if(m_bCMPreserveBadDiff)
                        {
                            std::string copy_path;
                            std::ostringstream path;

                            path << "copy." << cacheinfoPtr->Name();
                            copy_path = m_gettask->CacheLocation() + ACE_DIRECTORY_SEPARATOR_CHAR_A + path.str();

                            msg << "A copy of this file is created  at: " << copy_path << "\n";
                            if(ACE_OS::rename(getLongPathName(localFileName.c_str()).c_str(),
                                              getLongPathName(copy_path.c_str()).c_str()) < 0)
                            {
                                DebugPrintf(SV_LOG_DEBUG,
                                            "\n%s encountered error (%d) while trying rename operation for file %s.\n",
                                            FUNCTION_NAME,ACE_OS::last_error(), copy_path.c_str());
                            }
                        }

                        msg << "Retrying the download of the file again...\n";
                        DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                    }
                }
            }

            ++attempts;

            if(attempts > MaxRetryAttempts)
            {
                DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error (%s) while trying to download file %s from ip %s."
                            "Attempt =%lu, reached max. attempts.\n",
                            FUNCTION_NAME, m_cxTransport->status(), remoteLocation.c_str(), m_gettask->TransportIP().c_str(), attempts);

                rv = false;
                break;

            } else
            {

                DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error (%s) while trying to download file %s from ip %s."
                            "Attempt =%lu, retry after %d seconds\n",
                            FUNCTION_NAME, m_cxTransport->status(), remoteLocation.c_str(), m_gettask->TransportIP().c_str(), attempts,  RetryDelayInSeconds);

                Idle(RetryDelayInSeconds);
            }

        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());
    }

    catch ( ContextualException& ce )
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

// RenameTask

/*
 * FUNCTION NAME :  RenameTask::open
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
int RenameTask::open(void *args)
{
    return this->activate(THR_BOUND);
}

/*
 * FUNCTION NAME :  RenameTask::close
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
int RenameTask::close(u_long flags)
{
    return 0;
}
//Bug #8149
/*
 * FUNCTION NAME :  RenameTask::DeletePendingFile
 *
 * DESCRIPTION : Opens deletepending.dat, reads the filename, removes it from the CX
 *                               and deletes deletepending.dat if deletion from cx is successful
 *
 *
 * INPUT PARAMETERS : none
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 *
 * return value : true on success, else false
 *
 */
bool RenameTask::DeletePendingFile()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    bool rv = true;
    std::string fileInfo(m_gettask->CacheLocation() + ACE_DIRECTORY_SEPARATOR_CHAR_A + "deletepending.dat");

    do
    {
        //if the file doesn't exist, it is not an error

        ACE_stat statbuf = {0};
        // PR#10815: Long Path support
        if(0 == sv_stat(getLongPathName(fileInfo.c_str()).c_str(), &statbuf))
        {

            std::ifstream in(fileInfo.c_str());
            if(!in.good())
            {
                DebugPrintf(SV_LOG_ERROR,
                            "\nencountered error while opening %s\n",
                            fileInfo.c_str());
                rv = false;
                break;
            }

            std::string file;
            in >> file;
            if(!in.good() && !in.eof())
            {
                DebugPrintf(SV_LOG_ERROR,
                            "\nencountered error while retrieving filename from %s\n",
                            fileInfo.c_str());
                rv = false;
                break;
            }
            bool addentry = false;
            if(!DeleteRemoteFile(file, addentry))
            {
                DebugPrintf(SV_LOG_ERROR, "\nencountered error while deleting %s\n", file.c_str());
                rv = false;
                break;
            }

            // DeleteRemoteFile may have returned true without deleting the file from cx
            // if it got a quit Request. in such a case, the entry should not be removed
            // from deletepending.dat file
            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

            in.close();
            // PR#10815: Long Path support
            if(ACE_OS::unlink(getLongPathName(fileInfo.c_str()).c_str()) < 0 && (ACE_OS::last_error() != ENOENT))
            {
                DebugPrintf(SV_LOG_DEBUG,
                            "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                            FUNCTION_NAME,ACE_OS::last_error(), fileInfo.c_str());
                rv = false;
                break;
            }
        }


    } while (0);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    return rv;
}

/*
 * FUNCTION NAME :  RenameTask::svc
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
int RenameTask::svc()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());


        // bug 8149
        do
        {
            TRANSPORT_CONNECTION_SETTINGS settings(m_configurator ->getTransportSettings(m_volname));

            try
            {
                m_cxTransport.reset(new CxTransport(m_gettask ->Protocol(), settings, m_gettask->Secure(), m_cfsPsData.m_useCfs, m_cfsPsData.m_psId));
                DebugPrintf(SV_LOG_DEBUG,"%s Created Cx Transport using PS ip: %s\n",    FUNCTION_NAME, settings.ipAddress.c_str());

            }
            catch (std::exception const & e)
            {
                DebugPrintf(SV_LOG_ERROR,"%s create CxTransport failed: %s\n",    FUNCTION_NAME, e.what());
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR,"%s %s create CxTransport failed: unknonw error\n", FUNCTION_NAME);
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }

            if(!DeletePendingFile())
            {
                // unhandled failure, let's us request for a safe shutdown
                DebugPrintf(SV_LOG_ERROR,
                            "%s encountered failure in DeletePendingFile\n",FUNCTION_NAME);
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }

            while (true)
            {
                ACE_Message_Block *mb = NULL;
                if(!FetchNextMessage(&mb))
                {
                    // unhandled failure, let's us request for a safe shutdown
                    DebugPrintf(SV_LOG_ERROR,
                                "%s encountered failure in FetchNextMessage\n",
                                FUNCTION_NAME);
                    CDPUtil::SignalQuit();
                    rv = false;
                    break;
                }

                if(mb -> msg_type() == CM_MSG_QUIT)
                {
                    mb -> release();
                    rv = false;
                    break;
                }

                if(mb -> msg_type() == CM_MSG_PROCESS)
                {
                    if(!RenameRequestedFiles())
                    {

                        DebugPrintf(SV_LOG_ERROR,
                                    "%s encountered failure in RenameRequestedFiles\n",
                                    FUNCTION_NAME);

                        mb -> release();
                        Idle(IdleWaitTimeInSeconds);
                        m_gettask ->PostMsg(CM_ERROR_INWORKER, CM_ERROR_INWORKER_PRIORITY);
                        rv = false;
                        break;
                    }
                }
                mb -> release();

            } // end of while(true)

        } while (0);

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return 0;
}

/*
 * FUNCTION NAME :  RenameTask::PostMsg
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
void RenameTask::PostMsg(int msg, int priority)
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
 * FUNCTION NAME :  RenameTask::FetchNextMessage
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
bool RenameTask::FetchNextMessage(ACE_Message_Block ** mb)
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        while(true)
        {
            m_cxTransport->heartbeat();

            ACE_Time_Value wait = ACE_OS::gettimeofday ();
            int waitSeconds = MsgWaitTimeInSeconds;
            wait.sec (wait.sec () + waitSeconds);
            if (-1 == this->msg_queue() -> dequeue_head(*mb, &wait))
            {
                if (errno == ESHUTDOWN)
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error (message queue closed) for %s\n",
                                FUNCTION_NAME, m_volname.c_str());

                    rv = false;
                    break;
                }
            }
            else
            {
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
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

/*
 * FUNCTION NAME :  RenameTask::QuitRequested
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
bool RenameTask::QuitRequested(int seconds)
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
                DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error (message queue closed) for %s\n",
                            FUNCTION_NAME, m_volname.c_str());

                // the message queue has been closed abruptly
                // let's us request for a safe shutdown

                CDPUtil::SignalQuit();
                shouldQuit= true;
            }
        } else
        {
            if(CM_MSG_QUIT == mb -> msg_type())
            {
                shouldQuit = true;
            } else
            {
                shouldQuit = CDPUtil::QuitRequested(seconds);
            }
        }
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return shouldQuit;
}

/*
 * FUNCTION NAME :  RenameTask::Idle
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
void RenameTask::Idle(int seconds)
{
    m_cxTransport->heartbeat();
    (void)QuitRequested(seconds);
}


/*
 * FUNCTION NAME :  RenameTask::RenameRequestedFiles
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
bool RenameTask::RenameRequestedFiles()
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        bool done = false;

        while(true)
        {

            CacheInfo::CacheInfoPtr cacheinfoPtr =  m_gettask ->FetchNextDiffForRename(done);
            if(done || !cacheinfoPtr)
            {
                // send msg to diffgettask thread to fetch next list
                m_gettask ->PostMsg(CM_FETCHNEXTLIST, CM_FETCHNEXTLIST_PRIORITY);
                rv = true;
                break;
            }

            std::string filename = cacheinfoPtr -> Name();
            SV_ULONG size = cacheinfoPtr -> Size();
            //Bug #8025
            if(!RenameandDeleteFile(cacheinfoPtr))
            {
                rv = false;
                break;
            }

            if(QuitRequested(0))
            {
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
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


/*
 * FUNCTION NAME :  RenameTask::isTsoFile
 *
 * DESCRIPTION :
 *
 *
 *
 * INPUT PARAMETERS : filename
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 *
 * return value : true if tso file else false
 *
 */
bool RenameTask::isTsoFile(const std::string & filePath)
{
    bool rv = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    std::string::size_type idx = filePath.find(Tso_File_SubString);
    if (std::string::npos == idx ) {
        rv = false; // not a tso file
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}


/*
 * FUNCTION NAME :  RenameTask::RenameandDeleteFile
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
bool RenameTask::RenameandDeleteFile(CacheInfo::CacheInfoPtr cacheinfoPtr)
{
    bool rv = true;

    try
    {
        //__asm int 3;
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

        do
        {
            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

            if(!RetryableRenameFile(cacheinfoPtr))
            {
                rv = false;
                break;
            }

            if(!DeleteRemoteFile(cacheinfoPtr))
            {
                rv = false;
                break;
            }

            rv = true;
            break;
        } while(0);

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

    }
    catch ( ContextualException& ce )
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
//Bug #8025
/*
 * FUNCTION NAME :  RenameTask::RenameFile
 *
 * DESCRIPTION : Acquires the lock. and then renames the file from oldpath to newpath.
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
 * return value : false if either rename failed or acquiring the lock fails
 *
 */
bool RenameTask::RenameFile(const std::string& oldpath, const std::string& newpath)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s %s\n",
        FUNCTION_NAME, m_volname.c_str(), oldpath.c_str(),newpath.c_str());

    bool rv = true;

    do
    {
        const std::string lockPath = m_gettask ->CacheLocation() + ACE_DIRECTORY_SEPARATOR_CHAR_A + "pending_diffs.lck";
        // PR#10815: Long Path support
        ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        AutoGuard guard(lock);

        if(!guard.locked())
        {
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                        FUNCTION_NAME,ACE_OS::last_error(),
                        ACE_OS::strerror(ACE_OS::last_error()),lockPath.c_str());
            rv = false;
            break;
        }

        // PR#10815: Long Path support
        if(ACE_OS::rename(getLongPathName(oldpath.c_str()).c_str(),
                          getLongPathName(newpath.c_str()).c_str()) < 0)
        {
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) while trying rename operation for file %s.\n",
                        FUNCTION_NAME,ACE_OS::last_error(), newpath.c_str());
            rv = false;
            break;
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s %s\n",
        FUNCTION_NAME, m_volname.c_str(), oldpath.c_str(), newpath.c_str());

    return rv;
}
/*
 * FUNCTION NAME :  RenameTask::RetryableRenameFile
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
//Bug #8025
bool RenameTask::RetryableRenameFile(CacheInfo::CacheInfoPtr cacheinfoPtr)
{

    bool rv = true;
    SV_ULONG attempts = 0;
    bool brenamed = false;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());
        //Bug #8025
        std::string tmpFileName = m_gettask ->TmpCacheLocation();
        tmpFileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        tmpFileName += tmpPrefix;
        tmpFileName += cacheinfoPtr -> Name();

        std::string localFileName = m_gettask ->CacheLocation();
        localFileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        localFileName += cacheinfoPtr -> Name();

        while (!brenamed)
        {
            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

            if(!cacheinfoPtr -> isRenameRequired())
            {
                DebugPrintf(SV_LOG_DEBUG, "rename for %s is not required.\n",
                            tmpFileName.c_str());
                rv = true;
                break;
            }

            if(!cacheinfoPtr -> isReadyForRename())
            {
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(!RenameFile(tmpFileName, localFileName))
            {
                ++attempts;

                if(attempts > MaxRetryAttempts)
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error while trying rename operation for file %s."
                                "Attempt =%lu, reached max. attempts\n",
                                FUNCTION_NAME, localFileName.c_str(),
                                attempts);

                    rv = false;
                    break;

                } else
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error while trying rename operation for file %s."
                                "Attempt =%lu, retry after %d seconds\n",
                                FUNCTION_NAME, localFileName.c_str(),
                                attempts, RetryDelayInSeconds);

                    Idle(RetryDelayInSeconds);
                    continue;
                }
            }
            else
            {
                cacheinfoPtr ->SetReadyToDeleteFromPS();
                brenamed = true;
                rv = true;
                break;
            }

        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

    }
    catch ( ContextualException& ce )
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


/*
 * FUNCTION NAME :  RenameTask::DeleteRemoteFile
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
bool RenameTask::DeleteRemoteFile(CacheInfo::CacheInfoPtr cacheinfoPtr)
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

        std::string remoteLocation = m_gettask -> CxLocation();
        remoteLocation += '/';
        remoteLocation += cacheinfoPtr -> Name();

        while(true)
        {
            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

            if(!cacheinfoPtr -> isDeletionRequired())
            {
                DebugPrintf(SV_LOG_DEBUG, "deletion from PS for %s is not required.\n",
                            remoteLocation.c_str());
                rv = true;
                break;
            }

            if(!cacheinfoPtr -> isReadyForDeletion())
            {
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(!DeleteRemoteFile(cacheinfoPtr -> Name()))
            {
                rv = false;
                break;
            }else
            {
                cacheinfoPtr ->SetDeletedFromPS();
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

    }
    catch ( ContextualException& ce )
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


/*
 * FUNCTION NAME :  RenameTask::DeleteRemoteFile
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
bool RenameTask::DeleteRemoteFile(const std::string& filename, bool addentry)
{
    bool rv = true;
    SV_ULONG attempts = 0;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), filename.c_str());


        std::string remoteLocation = m_gettask -> CxLocation();
        remoteLocation += '/';
        remoteLocation += filename;


        while (true)
        {
            if(QuitRequested(0))
            {
                //Bug #8149
                // add entry to file (if boolean passed)
                if(addentry)
                {
                    AddPendingFile(filename);
                }
                rv = true;
                break;
            }
            boost::tribool tb = m_cxTransport->deleteFile(remoteLocation);
            ++attempts;
            if (CxTransportNotFound(tb)) {
                rv = true;
                break;
            } else if (!tb) {
                if(attempts > MaxRetryAttempts) {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error (%s) while trying delete operation for file %s, PS ip %s."
                                "Attempt =%lu, reached max. attempts\n",
                                FUNCTION_NAME, m_cxTransport->status(), remoteLocation.c_str(), m_gettask->TransportIP().c_str(),
                                attempts);

                    // add entry to file
                    if(addentry) {
                        AddPendingFile(filename);
                    }
                    rv = false;
                    break;

                } else {
#ifdef SV_WINDOWS
                    static __declspec(thread) long long lastTransErrLogTime = 0;
#else
                    static __thread long long lastTransErrLogTime = 0;
#endif 
                    ACE_Time_Value currentTime = ACE_OS::gettimeofday();
                    LocalConfigurator localConfigurator;
                    if ((currentTime.sec() - lastTransErrLogTime) > localConfigurator.getTransportErrorLogInterval())
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error (%s) while trying delete operation for file %s , PS ip %s."
                            "Attempt =%lu, retry after %d seconds\n",
                            FUNCTION_NAME, m_cxTransport->status(), remoteLocation.c_str(), m_gettask->TransportIP().c_str(),
                            attempts, RetryDelayInSeconds);
                        lastTransErrLogTime = ACE_OS::gettimeofday().sec();
                    }

                    Idle(RetryDelayInSeconds);
                    continue;
                }
            } else {
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), filename.c_str());

    }
    catch ( ContextualException& ce )
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

//Bug #8149

/*
 * FUNCTION NAME :  RenameTask::AddPendingFile
 *
 * DESCRIPTION : open (deletepending.dat) and add the filename to it
 *
 * INPUT PARAMETERS : filename
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 *
 *
 * return value : none
 *
 */
void RenameTask::AddPendingFile(const std::string& filename)
{
    std::string fileInfo(m_gettask->CacheLocation() + ACE_DIRECTORY_SEPARATOR_CHAR_A + "deletepending.dat");

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    do
    {
        std::ofstream out(fileInfo.c_str(), std::ios::trunc);

        if(!out.good())
        {
            DebugPrintf(SV_LOG_ERROR,
                        "FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
                        FUNCTION_NAME, fileInfo.c_str(), LINE_NO,FILE_NAME);
        }

        out << filename;

        if(!out.good())
        {
            DebugPrintf(SV_LOG_ERROR,
                        "FAILED: Encountered error while AddPendingFile %s. @LINE %d in FILE %s\n",
                        filename.c_str(), LINE_NO,FILE_NAME);
        }

    } while(0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}
// IOTask

/*
 * FUNCTION NAME :  IOTask::open
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
int IOTask::open(void *args)
{
    return this->activate(THR_BOUND);
}

/*
 * FUNCTION NAME :  IOTask::close
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
int IOTask::close(u_long flags)
{
    return 0;
}

/*
 * FUNCTION NAME :  IOTask::svc
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
int IOTask::svc()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        m_bCMVerifyDiff = m_cachemgr -> getCMVerifyDiff();
        m_bCMPreserveBadDiff = m_cachemgr -> getCMPreserveBadDiff();

        while (true)
        {
            ACE_Message_Block *mb = NULL;
            if(!FetchNextMessage(&mb))
            {
                // unhandled failure, let's us request for a safe shutdown
                DebugPrintf(SV_LOG_ERROR,"%s encountered failure in FetchNextMessage\n",FUNCTION_NAME);
                CDPUtil::SignalQuit();
                rv = false;
                break;
            }

            if(mb -> msg_type() == CM_MSG_QUIT)
            {
                mb -> release();
                rv = false;
                break;
            }

            if(mb -> msg_type() == CM_MSG_PROCESS)
            {
                if(!WriteRequestedFiles())
                {

                    DebugPrintf(SV_LOG_ERROR,"%s encountered failure in WriteRequestedFiles\n",
                                FUNCTION_NAME);

                    mb -> release();
                    Idle(IdleWaitTimeInSeconds);
                    m_gettask ->PostMsg(CM_ERROR_INWORKER, CM_ERROR_INWORKER_PRIORITY);
                    rv = false;
                    break;
                }
            }

            mb -> release();
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        rv = false;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return 0;
}

/*
 * FUNCTION NAME :  IOTask::FetchNextMessage
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
void IOTask::PostMsg(int msg, int priority)
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
 * FUNCTION NAME :  IOTask::FetchNextMessage
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
bool IOTask::FetchNextMessage(ACE_Message_Block ** mb)
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        while(true)
        {
            ACE_Time_Value wait = ACE_OS::gettimeofday ();
            int waitSeconds = MsgWaitTimeInSeconds;
            wait.sec (wait.sec () + waitSeconds);
            if (-1 == this->msg_queue() -> dequeue_head(*mb, &wait))
            {
                if (errno == ESHUTDOWN)
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error (message queue closed) for %s\n",
                                FUNCTION_NAME, m_volname.c_str());

                    rv = false;
                    break;
                }
            }
            else
            {
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());

    }

    catch ( ContextualException& ce )
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

bool IOTask::VerifyFileContentsIfEnabled(const std::string & localFileName)
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, localFileName.c_str());

        do
        {
            if(m_bCMVerifyDiff)
            {
                std::string::size_type idx = localFileName.rfind(".gz");
                if (std::string::npos == idx || (localFileName.length() - idx) > 3)
                {
                    if(ValidateSVFile(localFileName.c_str(), NULL,NULL, NULL) != 1)
                    {
                        std::stringstream errmsg;
                        errmsg<< "svdcheck  failed for file "
                              << localFileName
                              << " for the target volume "
                              << m_volname;

                        DebugPrintf(SV_LOG_ERROR,"%s\n", errmsg.str().c_str());
                        rv = false;
                        break;
                    }
                }else
                {
                    GzipUncompress uncompresser;

                    if(!uncompresser.Verify(localFileName))
                    {
                        std::stringstream errmsg;
                        errmsg<< "uncompress test  failed for file "
                              << localFileName
                              << " for the target volume "
                              << m_volname;

                        DebugPrintf(SV_LOG_ERROR,"%s\n", errmsg.str().c_str());
                        rv = false;
                        break;
                    }
                }
            }

        } while (0);

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, localFileName.c_str());
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

/*
 * FUNCTION NAME :  IOTask::QuitRequested
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
bool IOTask::QuitRequested(int seconds)
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
                DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error (message queue closed) for %s\n",
                            FUNCTION_NAME, m_volname.c_str());

                // the message queue has been closed abruptly
                // let's us request for a safe shutdown

                CDPUtil::SignalQuit();
                shouldQuit= true;
            }
        } else
        {
            if(CM_MSG_QUIT == mb -> msg_type())
            {
                shouldQuit = true;
            } else
            {
                shouldQuit = CDPUtil::QuitRequested(seconds);
            }
        }
    }

    catch ( ContextualException& ce )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, ce.what(), m_volname.c_str());
    }
    catch( exception const& e )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered exception %s for %s.\n",
                    FUNCTION_NAME, e.what(), m_volname.c_str());
    }
    catch ( ... )
    {
        // unhandled failure, let's us request for a safe shutdown
        CDPUtil::SignalQuit();

        shouldQuit= true;
        DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered unknown exception for %s.\n",
                    FUNCTION_NAME, m_volname.c_str());
    }

    return shouldQuit;
}

/*
 * FUNCTION NAME :  IOTask::Idle
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
void IOTask::Idle(int seconds)
{
    (void)QuitRequested(seconds);
}


/*
 * FUNCTION NAME :  IOTask::WriteRequestedFiles
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
bool IOTask::WriteRequestedFiles()
{
    bool rv = true;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_volname.c_str());

        bool done = false;

        while(true)
        {

            CacheInfo::CacheInfoPtr cacheinfoPtr =  m_gettask ->FetchNextDiffForWrite(done);
            if(done || !cacheinfoPtr)
            {
                rv = true;
                break;
            }

            if(!WriteFile(cacheinfoPtr))
            {
                rv = false;
                break;
            }

            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

            if(!VerifyFile(cacheinfoPtr))
            {
                rv = false;
                break;
            }

            // reset the bytes written counter
            // this is to allow disk usage checks per 64 mb write
            if(m_bytesWritten > 67108864)
            {
                m_bytesWritten = 0;
            }

            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_volname.c_str());
    }

    catch ( ContextualException& ce )
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

bool IOTask::WriteFile(CacheInfo::CacheInfoPtr cacheinfoPtr)
{
    bool rv = true;
    SV_ULONG attempts = 0;
    bool sendalert = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

        //Bug #8025
        std::string localFileName = m_gettask ->TmpCacheLocation();
        localFileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        localFileName += tmpPrefix;
        localFileName += cacheinfoPtr -> Name();

        while (true)
        {
            if(QuitRequested(0))
            {
                rv = true;
                break;
            }

            if(!cacheinfoPtr -> isWriteToDiskRequired())
            {
                DebugPrintf(SV_LOG_DEBUG, "write to disk is not required for %s\n",
                            localFileName.c_str());
                rv = true;
                break;
            }

            if(!cacheinfoPtr -> isReadyForWriteToDiskOrVerification())
            {
                Idle(IdleWaitTimeInSeconds);
                continue;
            }

            if(cacheinfoPtr -> isReadyToVerify())
            {
                rv = true;
                break;
            }

            char * buffer = cacheinfoPtr -> buffer();
            SV_ULONG bufsize = cacheinfoPtr ->Size();

            // we check for disk space availability
            //  for every 64 mb chunk write
            //  or on encountering failure to write the file
            // however low disk space alert will be sent only once
            // to avod unncessary flooding of messages to cx
            //
            if(( (m_bytesWritten > 67108864) || attempts ) &&
               (!m_gettask ->DiskSpaceAvailable(cacheinfoPtr ->Size(),sendalert)))
            {
                sendalert = false;
                Idle(IdleWaitTimeInSeconds);
                continue;
            }


            BasicIo bIO(localFileName, BasicIo::BioWrite|BasicIo::BioOverwrite|BasicIo::BioShareAll);
            SV_ULONG cbRemaining = cacheinfoPtr ->Size();
            SV_ULONG cbCopied = 0;
            SV_UINT chunkSize = 8388608; // 8MB
            SV_UINT bytesToCopy = 0;
            SV_UINT cbWrite = 0;

            bool bFailure = false;
            while(cbRemaining > 0)
            {
                bytesToCopy = min( (SV_ULONG)chunkSize, cbRemaining );
                cbWrite = bIO.Write(buffer + cbCopied, bytesToCopy);
                if ( !bIO.Good() || (cbWrite != bytesToCopy) )
                {
                    stringstream l_stderr;
                    l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME
                             << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                             << "Error during write operation\n"
                             << "Data File:" << localFileName  << "\n"
                             << "Error Code: " << bIO.LastError() << "\n"
                             << "Error Message: " << Error::Msg(bIO.LastError()) << "\n";

                    DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
                    bFailure = true;
                    break;
                }

                cbRemaining -= cbWrite;
                cbCopied += cbWrite;
            }
            if(ACE_OS::fsync(bIO.Handle()) < 0)
            {
                DebugPrintf(SV_LOG_ERROR,
                            "\n%s encountered error while trying to flush the filesystem for the file %s. Failed with error: %d\n",
                            FUNCTION_NAME, localFileName.c_str(),ACE_OS::last_error());
                rv = false;
                break;
            }
            bIO.Close();


            if(bFailure)
            {
                ++attempts;

                if(attempts > MaxRetryAttempts)
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error while trying to write file %s."
                                "Attempt =%lu, reached max. attempts\n",
                                FUNCTION_NAME, localFileName.c_str(), attempts);

                    rv = false;
                    break;

                } else
                {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error while trying to write file %s."
                                "Attempt =%lu, retry after %d seconds\n",
                                FUNCTION_NAME, localFileName.c_str(),
                                attempts, RetryDelayInSeconds);

                    Idle(RetryDelayInSeconds);
                    continue;
                }
            }
            else
            {
                m_bytesWritten += cacheinfoPtr ->Size();
                m_gettask ->AdjustMemoryUsage(true, cacheinfoPtr ->Size());
                cacheinfoPtr ->SetReadyToVerify();
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());
    }

    catch ( ContextualException& ce )
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

bool IOTask::VerifyFile(CacheInfo::CacheInfoPtr cacheinfoPtr)
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

        //Bug #8025
        std::string localFileName = m_gettask ->TmpCacheLocation();
        localFileName += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        localFileName += tmpPrefix;
        localFileName += cacheinfoPtr -> Name();

        do
        {
            if(!VerifyFileContentsIfEnabled(localFileName))
            {
                std::stringstream msg;

                msg << FUNCTION_NAME << " encountered error: "
                    << "Transport layer returned successful download/write for " << localFileName.c_str()
                    << " but the file contents are not proper.\n";

                if(m_bCMPreserveBadDiff)
                {
                    std::string copy_path;
                    std::ostringstream path;

                    path << "copy." << cacheinfoPtr->Name();
                    copy_path = m_gettask->CacheLocation() + ACE_DIRECTORY_SEPARATOR_CHAR_A + path.str();

                    msg << "A copy of this file is created  at: " << copy_path << "\n";
                    if(ACE_OS::rename(localFileName.c_str(),copy_path.c_str()) < 0)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                                    "\n%s encountered error (%d) while trying rename operation for file %s.\n",
                                    FUNCTION_NAME,ACE_OS::last_error(), copy_path.c_str());
                    }
                }

                msg << "File will be downloaded again...\n";
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());

                rv = false;
                break;
            } else
            {
                cacheinfoPtr ->SetReadyToRename();
                rv = true;
                break;
            }

        } while (0);

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), cacheinfoPtr -> Name().c_str());

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

// ReporterTask

/*
 * FUNCTION NAME :  ReporterTask::open
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
int ReporterTask::open(void *args)
{
    return this->activate(THR_BOUND);
}

/*
 * FUNCTION NAME :  ReporterTask::close
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
int ReporterTask::close(u_long flags)
{
    return 0;
}

/*
 * FUNCTION NAME :  ReporterTask::svc
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
int ReporterTask::svc()
{
    bool rv = true;

    try
    {
        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
        SV_UINT ReporterInterval = m_cachemgr -> getPendingDataReporterInterval();

        while (!QuitRequested(ReporterInterval))
        {
            if(!SendPendingDataInfo())
            {
                DebugPrintf(SV_LOG_ERROR,"%s encountered failure in SendPendingDataInfo\n",
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
            if((CM_QUIT == mb->msg_type()) ||(CM_TIMEOUT == mb ->msg_type()))
            {
                shouldQuit = true;
            } else
            {
                shouldQuit = CDPUtil::QuitRequested(seconds);
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
 * return value : tbd
 *
 */
void ReporterTask::Idle(int seconds)
{
    (void)QuitRequested(seconds);
}

/*
 * FUNCTION NAME :  ReporterTask::SendPendingDataInfo
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
bool ReporterTask::SendPendingDataInfo()
{
    bool rv = true;
    std::map<std::string,SV_ULONGLONG> pendingDataInfo;
    try
    {
        InitialSettings settings = m_configurator -> getInitialSettings();
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t volumeGroups = settings.hostVolumeSettings.volumeGroups;
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator vgiter = volumeGroups.begin();
        for(; vgiter != volumeGroups.end(); ++vgiter)
        {
            VOLUME_GROUP_SETTINGS vg = *vgiter;
            if(TARGET != vg.direction)
                continue;

            VOLUME_GROUP_SETTINGS::volumes_iterator ctVolumeIter =  vg.volumes.begin();
            for (;ctVolumeIter != vg.volumes.end(); ctVolumeIter++)
            {
                if(((ctVolumeIter->second.syncMode == VOLUME_SETTINGS::SYNC_DIFF)
                    ||(ctVolumeIter->second.syncMode == VOLUME_SETTINGS::SYNC_QUASIDIFF))
                   &&(ctVolumeIter->second.transportProtocol != TRANSPORT_PROTOCOL_FILE))
                {
                    std::string volname = ctVolumeIter -> first;
                    std::string cacheLocation = m_cachemgr -> getDiffTargetCacheDirectoryPrefix();
                    cacheLocation += ACE_DIRECTORY_SEPARATOR_STR_A;
                    cacheLocation += m_cachemgr -> getHostId();
                    cacheLocation += ACE_DIRECTORY_SEPARATOR_STR_A;
                    cacheLocation += GetVolumeDirName(volname);

                    SV_ULONGLONG pendingData = m_cachemgr->CurrentDiskUsage(cacheLocation);
                    pendingDataInfo.insert(make_pair(volname,pendingData));
                }
            }
        }
        if(!pendingDataInfo.empty() && !updatePendingDataInfo((*m_configurator),pendingDataInfo))
        {
            std::stringstream l_stderror;
            l_stderror << "Error detected  " << "in Function:" << FUNCTION_NAME
                       << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                       << "Error: updatePendingDataInfo returned failed status\n";

            DebugPrintf(SV_LOG_ERROR, "%s", l_stderror.str().c_str());
            rv = false;
        }
        std::map<std::string,SV_ULONGLONG>::iterator iter = pendingDataInfo.begin();
        std::map<std::string,SV_ULONGLONG>::iterator end = pendingDataInfo.end();
        for(;iter!=end;iter++)
        {
            std::stringstream l_stddebug;
            l_stddebug << "Pendingdata for vol:" << iter->first << " is " << iter->second << std::endl;
            DebugPrintf(SV_LOG_DEBUG, "%s", l_stddebug.str().c_str());
        }
    }
    catch(  ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
        rv = false;
    }
    catch(exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
        rv = false;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
        rv = false;
    }
    return rv;
}


bool NWTask::RenameRemoteFile(const std::string& oldName, const std::string& newName)
{
    bool rv = true;
    SV_ULONG attempts = 0;

    try
    {

        DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), oldName.c_str(), newName.c_str());


        while (true)
        {
            if(QuitRequested(0))
            {
                rv = false;
                break;
            }
            boost::tribool tb = m_cxTransport->renameFile(oldName,newName,COMPRESS_NONE);
            ++attempts;
            if (CxTransportNotFound(tb)) {
                rv = true;
                break;
            } else if (!tb) {
                if(attempts > MaxRetryAttempts) {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error (%s) while trying rename operation for file %s, PS ip %s."
                                "Attempt =%lu, reached max. attempts\n",
                                FUNCTION_NAME, m_cxTransport->status(), oldName.c_str(), m_gettask->TransportIP().c_str(),
                                attempts);

                    rv = false;
                    break;

                } else {
                    DebugPrintf(SV_LOG_ERROR,
                                "\n%s encountered error (%s) while trying rename operation for file %s , PS ip %s."
                                "Attempt =%lu, retry after %d seconds\n",
                                FUNCTION_NAME, m_cxTransport->status(), oldName.c_str(), m_gettask->TransportIP().c_str(),
                                attempts, RetryDelayInSeconds);

                    Idle(RetryDelayInSeconds);
                    continue;
                }
            } else {
                rv = true;
                break;
            }
        }

        DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s %s %s\n",
                    FUNCTION_NAME, m_volname.c_str(), oldName.c_str(),newName.c_str());

    }
    catch ( ContextualException& ce )
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
