#include "ServiceHelper.h"
#include "localconfigurator.h"
#include "appcommand.h"
#include "service.h"

#ifdef SV_WINDOWS
SVSTATUS ServiceHelper::StartSVAgents(int timeout)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;

    bool active = true;
    SV_ULONG exitCode = 1;
    std::string cmd, cmdOutput;
    InmServiceStatus svcStatus;

    cmd = "net start svagents";

    AppCommand objAppCommand(cmd, timeout);
    boost::chrono::system_clock::time_point endTime =
        boost::chrono::system_clock::now() + boost::chrono::seconds(timeout);

    do
    {
        getServiceStatus("svagents", svcStatus);
        InmService svc(svcStatus);

        if (svcStatus == INM_SVCSTAT_START_PENDING)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : Skip running SVAgents start as status = %s\n",
                FUNCTION_NAME, svc.statusAsStr().c_str());
        }
        else if (svcStatus == INM_SVCSTAT_RUNNING)
        {
            status = SVS_OK;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents service status=%s\n",
                FUNCTION_NAME, svc.statusAsStr().c_str());
            objAppCommand.Run(exitCode, cmdOutput, active, "", NULL);

            getServiceStatus("svagents", svcStatus);
            InmService svc2(svcStatus);

            DebugPrintf(SV_LOG_ALWAYS,
                "%s : SVAgents start command exit code : %d, service status : %s, output : %s\n",
                FUNCTION_NAME, exitCode, svc2.statusAsStr().c_str(), cmdOutput.c_str());

            if (!exitCode && svcStatus == INM_SVCSTAT_RUNNING)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents start succeeded.\n", FUNCTION_NAME);
                status = SVS_OK;
                break;
            }
        }

        boost::chrono::system_clock::time_point curTime = boost::chrono::system_clock::now();
        if (curTime > endTime)
        {
            DebugPrintf(SV_LOG_ERROR, "%s : SVAgents start timedout\n", FUNCTION_NAME);
            status = SVE_ABORT;
            break;
        }

        ACE_OS::sleep(Migration::ServiceOperationSleepTime);
    } while(true);
    DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents start final status : %d\n", FUNCTION_NAME, status);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS ServiceHelper::StopSVAgents(int timeout)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;

    bool active = true;
    SV_ULONG exitCode = 1;
    std::string cmd, cmdOutput;
    InmServiceStatus svcStatus;

    cmd = "net stop svagents";

    AppCommand objAppCommand(cmd, timeout);
    boost::chrono::system_clock::time_point endTime =
        boost::chrono::system_clock::now() + boost::chrono::seconds(timeout);

    do
    {
        getServiceStatus("svagents", svcStatus);
        InmService svc(svcStatus);

        if (svcStatus == INM_SVCSTAT_STOP_PENDING)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : Skip running SVAgents stop as status = %s\n",
                FUNCTION_NAME, svc.statusAsStr().c_str());
        }
        else if (svcStatus == INM_SVCSTAT_STOPPED)
        {
            status = SVS_OK;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents service status=%s\n",
                FUNCTION_NAME, svc.statusAsStr().c_str());

            objAppCommand.Run(exitCode, cmdOutput, active, "", NULL);

            getServiceStatus("svagents", svcStatus);
            InmService svc2(svcStatus);

            DebugPrintf(SV_LOG_ALWAYS,
                "%s : SVAgents stop command exit code : %d, service status : %s, output : %s\n",
                FUNCTION_NAME, exitCode, svc2.statusAsStr().c_str(), cmdOutput.c_str());

            if (!exitCode && svcStatus == INM_SVCSTAT_STOPPED)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents stop succeeded.\n", FUNCTION_NAME);
                status = SVS_OK;
                break;
            }
        }

        boost::chrono::system_clock::time_point curTime = boost::chrono::system_clock::now();
        if (curTime > endTime)
        {
            DebugPrintf(SV_LOG_ERROR, "%s : SVAgents stop timedout\n", FUNCTION_NAME);
            status = SVE_ABORT;
            break;
        }

        ACE_OS::sleep(Migration::ServiceOperationSleepTime);
    } while(true);
    DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents stop final status : %d\n", FUNCTION_NAME, status);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

#else
SVSTATUS ServiceHelper::StartSVAgents(int timeout)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;

    bool active = true;
    SV_ULONG exitCode = 1;
    std::string cmd, cmdOutput;

    LocalConfigurator lc;
    cmd = lc.getInstallPath();
    cmd += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    cmd += "bin";
    cmd += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    cmd += "start";

    AppCommand objAppCommand(cmd, timeout);
    boost::chrono::system_clock::time_point endTime =
        boost::chrono::system_clock::now() + boost::chrono::seconds(timeout);

    do
    {
        objAppCommand.Run(exitCode, cmdOutput, active, "", NULL);
        DebugPrintf(SV_LOG_ALWAYS,
            "%s : SVAgents start exit code : %d, output : %s\n",
            FUNCTION_NAME, exitCode, cmdOutput.c_str());

        if (!exitCode)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents start succeeded.\n", FUNCTION_NAME);
            status = SVS_OK;
            break;
        }

        boost::chrono::system_clock::time_point curTime = boost::chrono::system_clock::now();
        if (curTime > endTime)
        {
            DebugPrintf(SV_LOG_ERROR, "%s : SVAgents start timedout\n", FUNCTION_NAME);
            status = SVE_ABORT;
            break;
        }

        ACE_OS::sleep(Migration::ServiceOperationSleepTime);
    } while(true);

    DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents start final status : %d\n", FUNCTION_NAME, status);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS ServiceHelper::StopSVAgents(int timeout)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVE_FAIL;

    bool active = true;
    SV_ULONG exitCode = 1;
    std::string cmd, cmdOutput;

    LocalConfigurator lc;
    cmd = lc.getInstallPath();
    cmd += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    cmd += "bin";
    cmd += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    cmd += "stop";
    cmd += " svagents";

    AppCommand objAppCommand(cmd, timeout);
    boost::chrono::system_clock::time_point endTime =
        boost::chrono::system_clock::now() + boost::chrono::seconds(timeout);

    do
    {
        objAppCommand.Run(exitCode, cmdOutput, active, "", NULL);
        DebugPrintf(SV_LOG_ALWAYS,
            "%s : SVAgents stop exit code : %d, output : %s\n",
            FUNCTION_NAME, exitCode, cmdOutput.c_str());

        if (!exitCode)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents stop succeeded.\n", FUNCTION_NAME);
            status = SVS_OK;
            break;
        }

        boost::chrono::system_clock::time_point curTime = boost::chrono::system_clock::now();
        if (curTime > endTime)
        {
            DebugPrintf(SV_LOG_ERROR, "%s : SVAgents stop timedout\n", FUNCTION_NAME);
            status = SVE_ABORT;
            break;
        }

        ACE_OS::sleep(Migration::ServiceOperationSleepTime);
    } while(true);

    DebugPrintf(SV_LOG_ALWAYS, "%s : SVAgents stop final status : %d\n", FUNCTION_NAME, status);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
#endif

SVSTATUS ServiceHelper::UpdateSymLinkPath(const std::string &linkPath,
    const std::string &filePath, std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    do
    {
        boost::system::error_code ec;
        int retryCount;

        if (boost::filesystem::exists(linkPath, ec))
        {
            retryCount = 0;
            while (retryCount < Migration::MaxRetryCount)
            {
                boost::filesystem::remove_all(linkPath, ec);
                if (ec == boost::system::errc::success)
                {
                    break;
                }
                retryCount++;
                ACE_OS::sleep(Migration::SleepTime);
            }
            if (ec == boost::system::errc::success)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s : Successfully removed file : %s\n",
                    FUNCTION_NAME, linkPath.c_str());
            }
            else
            {
                errMsg = "Failed to remove symlink file " + linkPath +
                    " with error : " + ec.message();
                status = SVE_FAIL;
                break;
            }
        }

        retryCount = 0;
        while (retryCount < Migration::MaxRetryCount)
        {
            boost::filesystem::create_symlink(filePath, linkPath, ec);
            if (ec == boost::system::errc::success)
            {
                break;
            }
            retryCount++;
            ACE_OS::sleep(Migration::SleepTime);
        }
        if (ec == boost::system::errc::success)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s : Create sym link succeeded from %s to %s.\n",
                FUNCTION_NAME, linkPath.c_str(), filePath.c_str());
        }
        else
        {
            errMsg = "Failed to create symlink from " + linkPath + " to " +
                filePath + " with error : " + ec.message();
            status = SVE_FAIL;
            break;
        }

    } while(false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS ServiceHelper::UpdateCSTypeAndRestartSVAgent(const std::string &csType,
    std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    LocalConfigurator lc;

    do
    {
        status = StopSVAgents(
            Migration::ServiceStopTimeOut);
        if (status != SVS_OK)
        {
            errMsg = "Failed to stop service\n";
            break;
        }

        lc.setCSType(csType);

        std::string installDir = lc.getInstallPath();
        installDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
#ifdef SV_UNIX
        installDir += "bin";
        installDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
#endif
        std::string svagentsLink = installDir + SVAGENTS_LINK;
        std::string svagentsPath = installDir;
        (csType == "CSPrime") ? (svagentsPath += SVAGENTS_RCM) :
            (svagentsPath += SVAGENTS_CS);

        status = UpdateSymLinkPath(svagentsLink, svagentsPath, errMsg);
        if (status != SVS_OK)
        {
            status = SVE_FAIL;
            break;
        }

        std::string s2Path = installDir + (csType == "CSPrime"? S2_RCM : S2_CS);
        lc.setDiffSourceExePathname(s2Path);

        std::string dpPath = installDir + (csType == "CSPrime"? DATAPROTECTION_RCM : DATAPROTECTION_CS);
        lc.setDataProtectionExePathname(dpPath);

        status = ServiceHelper::StartSVAgents(
            Migration::ServiceStartTimeOut);
        if (status != SVS_OK)
        {
            errMsg = "Failed to start service\n";
            break;
        }
    } while(false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

