//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : main.cpp
// 
// Description: 
//
#include <ace/Init_ACE.h>

#include <boost/algorithm/string/replace.hpp>

#include "programargs.h"
#include "dataprotection.h"
#include "dataprotectionexception.h"
#include "HandlerCurl.h"
#include "configurator2.h"
#include "localconfigurator.h"
#include "serializationtype.h"
#include "logger.h"
#include "cdputil.h"

#include <curl/curl.h>
#include "configwrapper.h"
#include "inmageex.h"

#include "portablehelpersmajor.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#ifdef SV_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "utilfdsafeclose.h"
#endif

#include "irprofiling.h"

#include "LogCutter.h"

using namespace Logger;

extern Log gLog;

Configurator* TheConfigurator = NULL; // singleton

/*
* FUNCTION NAME :  InitializeConfigurator
*
* DESCRIPTION : initialize cofigurator to fetch initialsettings
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

bool InitializeConfigurator()
{
    bool rv = false;

    do
    {
        try 
        {
            if(!InitializeConfigurator(&TheConfigurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
            {
                rv = false;
                break;
            }

            TheConfigurator ->Start();

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
            "dataprotection cannot run now. please try again\n");
    }

    return rv;
}

std::string getArgStr(int argc, char* argv[])
{
    int cnt = 0;
    std::string args;
    while (cnt < argc) { args += argv[cnt++]; args += " "; }
    return args;
}

int main(int argc, char* argv[])
{ 
    TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    // PR#10815: Long Path support
    ACE::init();

    //__asm int 3;     

    SV_ULONGLONG retval = 0;
    boost::shared_ptr<Logger::LogCutter> dpLogCutter;
    ProgramArgs progArgs;

    do
    {
        try
        {

            CDPUtil::disable_signal(ACE_SIGRTMIN, ACE_SIGRTMAX);

#ifdef SV_WINDOWS
            // PR #2094
            // SetErrorMode controls  whether the process will handle specified 
            // type of errors or whether the system will handle them
            // by creating a dialog box and notifying the user.
            // In our case, we want all errors to be sent to the program.
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif


#ifdef SV_UNIX
            /* predefining file descriptors 0, 1 and 2
             * so that children never accidentally write 
             * anything to valid file descriptors of
             * service like driver or svagents.log etc,. */
            const char DEV_NULL[] = "/dev/null";
            int fdin, fdout, fderr;

            fdin = fdout = fderr = -1;
            fdin = open(DEV_NULL, O_RDONLY);
            fdout = open(DEV_NULL, O_WRONLY);
            fderr = open(DEV_NULL, O_WRONLY);

            boost::shared_ptr<void> fdGuardIn(static_cast<void*>(0), boost::bind(fdSafeClose, fdin));
            boost::shared_ptr<void> fdGuardOut(static_cast<void*>(0), boost::bind(fdSafeClose, fdout));
            boost::shared_ptr<void> fdGuardErr(static_cast<void*>(0), boost::bind(fdSafeClose, fderr));
            set_resource_limits();
#endif


            tal::GlobalInitialize();
            MaskRequiredSignals();
            LocalConfigurator localConfigurator;

            //
            // PR # 6337
            // set the curl_verbose option based on settings in 
            // drscout.conf
            //
            tal::set_curl_verbose(localConfigurator.get_curl_verbose());

            Curl_setsendwindowsize(localConfigurator.getTcpSendWindowSize());
            Curl_setrecvwindowsize(localConfigurator.getTcpRecvWindowSize());

            SetLogLevel(localConfigurator.getLogLevel());
            SetLogHostId(localConfigurator.getHostId().c_str());
            SetLogRemoteLogLevel(SV_LOG_DISABLE);
            SetSerializeType(localConfigurator.getSerializerType());
            SetLogHttpsOption(localConfigurator.IsHttps());
            SetDaemonMode();

            // TODO: for now we are passing in args eventually we should 
            //       get everything from configurator
            if (!progArgs.Parse(argc, argv)) {
                Usage(argv[0]);
                retval = -1;
                break;
            }

            std::string logPathname = localConfigurator.getLogPathname();
            logPathname += progArgs.getLogDir();
            SVMakeSureDirectoryPathExists(logPathname.c_str());

            logPathname += "dataprotection.log";

            // The log cutter must complete before the CloseDebug() is invoked to close the log files.
            dpLogCutter = boost::make_shared<Logger::LogCutter>(Logger::LogCutter(gLog));
            const int maxCompletedLogFiles = 3;
            const boost::chrono::seconds cutInterval(300); // 5 mins.
            std::set<std::string> setCompLogFilesPaths;
            if (progArgs.m_FastSync)
            {
                std::string completedLogPath(logPathname);

                boost::ireplace_first(completedLogPath, Resync_Folder, localConfigurator.getEvtCollForwIRCompletedFilesMoveDir());
                setCompLogFilesPaths.insert(completedLogPath);
            }
            dpLogCutter->Initialize(logPathname, maxCompletedLogFiles, cutInterval, setCompLogFilesPaths);

            if (!InitializeConfigurator())
            {
                retval = -1;
                break;
            }

            DebugPrintf(SV_LOG_DEBUG, "The log file name is %s\n", logPathname.c_str());

            DataProtection dataProtection(progArgs);
            BOOST_ASSERT(dpLogCutter);
            if (dpLogCutter)
            {
                dpLogCutter->StartCutting();
            }
            dataProtection.Protect();

        }
        catch (DataProtectionNWException& dpe) {
            retval = TRANSPORT_ERROR;
            if (dpe.LogLevel() == SV_LOG_ERROR)
            {
                DebugPrintf(dpe.LogLevel(), "%s, Dataprotection got an exception: %s\n", FUNCTION_NAME, dpe.what());
            }
            IRSummaryPostException jirs;
            jirs.UpdateIRProfilingSummaryOnException(getArgStr(argc, argv));
        }
        catch (DataProtectionException& dpe) {
            DebugPrintf(dpe.LogLevel(), "%s, Dataprotection got an exception: %s\n", FUNCTION_NAME, dpe.what());
            IRSummaryPostException jirs;
            jirs.UpdateIRProfilingSummaryOnException(getArgStr(argc, argv));
        }
        catch (std::exception& e) {
            DebugPrintf(SV_LOG_ERROR, "%s, Dataprotection got an exception: %s\n", FUNCTION_NAME, e.what());
            IRSummaryPostException jirs;
            jirs.UpdateIRProfilingSummaryOnException(getArgStr(argc, argv));
        }
        catch (...) {
            DebugPrintf(SV_LOG_ERROR, "%s, Dataprotection got an unknown exception\n", FUNCTION_NAME);
            IRSummaryPostException jirs;
            jirs.UpdateIRProfilingSummaryOnException(getArgStr(argc, argv));
        }

    } while (false);

    ProfilerFactory::exitProfiler();

    if (dpLogCutter)
    {
        // Only "dataprotection_completed_*.log" files are being processed to upload to mds.
        // Complete current log before exit for IR source/MT.
        // Only IR source/MT log need to be completed as DR-MT run as a full time process.
        if (progArgs.m_FastSync)
        {
            dpLogCutter->StopCutting();
        }
    }

    CloseDebug();

    tal::GlobalShutdown();

    return retval;
}

