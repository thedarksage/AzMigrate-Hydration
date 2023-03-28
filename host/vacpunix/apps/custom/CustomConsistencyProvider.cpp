#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "fstream"
#include "appcommand.h"
#include "localconfigurator.h"
#include "CustomConsistencyProvider.h"

#define CUSTOM_SCRIPT_PATH "scripts/customscript.sh"
#define MAX_LOG_SIZE    10485760 // 10 MB

#define CUSTOM_SCRIPT_OPT_FREEZE    "--pre"
#define CUSTOM_SCRIPT_OPT_THAW      "--post"

extern void inm_printf(const char* format, ... );
extern void inm_printf(short logLevel, const char* format, ... );

CustomConsistencyProviderPtr CustomConsistencyProvider::m_instancePtr;
boost::mutex CustomConsistencyProvider::m_instanceMutex;

CustomConsistencyProvider::CustomConsistencyProvider()
{
    LocalConfigurator localConfigurator;
    m_scriptPath = localConfigurator.getInstallPath() + ACE_DIRECTORY_SEPARATOR_CHAR + CUSTOM_SCRIPT_PATH;
    m_logFile = "/var/log/ApplicationPolicyLogs/customscript.log";
}

CustomConsistencyProvider::~CustomConsistencyProvider()
{
}

CustomConsistencyProviderPtr CustomConsistencyProvider::getInstance()
{
    if (m_instancePtr.get() == NULL)
    {
        boost::unique_lock<boost::mutex> lock(m_instanceMutex);
        if (m_instancePtr.get() == NULL)
        {
            m_instancePtr.reset(new CustomConsistencyProvider);
        }
    }

    return m_instancePtr;
}

void CustomConsistencyProvider::RunCommand(const std::string &command)
{
    unsigned long exitCode = 0;
    std::string output;
    bool bProcessActive = true;
    std::ofstream logFileStream(m_logFile.c_str(), ios::out | ios::app);
    int currPos = logFileStream.tellp();
    if (currPos > MAX_LOG_SIZE)
    {
        logFileStream.close();
        logFileStream.open(m_logFile.c_str(), ios::out | ios::trunc);
    }
    logFileStream.close();

    AppCommand objAppCommand(command, 0, m_logFile);

    inm_printf("Running custom application script %s.\n", command.c_str());
    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        inm_printf("Custom application script %s failed with %d.\n", command.c_str(), exitCode);
    }

    return;
}

bool CustomConsistencyProvider::Run(const std::string &action)
{
    try {
        if (boost::filesystem::exists(m_scriptPath))
        {
            std::string command = m_scriptPath;
            command += " ";
            command += action;
            RunCommand(command);
        }
    }
    catch (const std::exception &e)
    {
        inm_printf("%s : custom consistency exception %s\n",
            __FUNCTION__,
            e.what());
    }
    catch (...)
    {
        inm_printf("%s : custom consistency unknown exception\n",
            __FUNCTION__);
    }

    // always return success so that the post script
    // will be run irrespective of prescript failure
    return true;
}

bool CustomConsistencyProvider::Discover()
{
    return true;
}

bool CustomConsistencyProvider::Freeze()
{
    return Run(CUSTOM_SCRIPT_OPT_FREEZE);
}

bool CustomConsistencyProvider::Unfreeze()
{
    return Run(CUSTOM_SCRIPT_OPT_THAW);
}
