#include <fstream>
#include "appcommand.h"
#include "localconfigurator.h"
#include "Db2ConsistencyProvider.h"

extern void inm_printf(const char* format, ... );
extern void inm_printf(short logLevel, const char* format, ... );

Db2ConsistencyProviderPtr Db2ConsistencyProvider::m_instancePtr;
boost::mutex Db2ConsistencyProvider::m_instanceMutex;

Db2ConsistencyProvider::Db2ConsistencyProvider()
{

    LocalConfigurator localConfigurator;
    installPath = localConfigurator.getInstallPath() + ACE_DIRECTORY_SEPARATOR_CHAR;
    discoveryOutputFile = installPath + "etc/db2discovery.dat";
    logFile = "/tmp/db2tags.log";
    bDbsFrozen = false;
}

Db2ConsistencyProvider::~Db2ConsistencyProvider()
{
}

Db2ConsistencyProviderPtr Db2ConsistencyProvider::getInstance()
{
    if (m_instancePtr.get() == NULL)
    {
        boost::unique_lock<boost::mutex> lock(m_instanceMutex);
        if (m_instancePtr.get() == NULL)
        {
            m_instancePtr.reset(new Db2ConsistencyProvider);
        }
    }

    return m_instancePtr;
}

bool Db2ConsistencyProvider::Discover()
{
    std::string script = installPath + "scripts/db2appagent.sh discover ";
    script += discoveryOutputFile;

    bool bRet = true;

    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;

    AppCommand objAppCommand(script, 0, discoveryOutputFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        inm_printf("Db2 database Discovery failed.\n");
        bRet = false;
    }

    inm_printf("%s.\n", output.c_str());

    if (exitCode != 0)
        bRet = false;

    return bRet;
}   

bool Db2ConsistencyProvider::Freeze()
{
    std::string script = installPath + "scripts/db2_consistency.sh freeze ALL ALL NONE NONE";
    bool bRet = false;

    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;
    std::ofstream logFileStream(logFile.c_str(), std::ifstream::trunc); 
    logFileStream.close();

    AppCommand objAppCommand(script, 0, logFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        inm_printf("Db2 database Freeze failed.\n");
    }

    inm_printf("%s.\n", output.c_str());

    if (exitCode == 0)
    {
        bDbsFrozen = true;
        bRet = true;
    }
    else if (exitCode == 2)
    {
        bRet = true;
    }

    return bRet;
}

bool Db2ConsistencyProvider::Unfreeze()
{

    if (!bDbsFrozen)
        return true;

    std::string script = installPath + "scripts/db2_consistency.sh unfreeze ALL ALL NONE NONE";
    bool bRet = true;

    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;
    std::ofstream logFileStream(logFile.c_str(), std::ifstream::trunc); 
    logFileStream.close();

    AppCommand objAppCommand(script, 0, logFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        inm_printf("Db2 database Resume failed.\n");
        bRet = false;
    }

    inm_printf("%s.\n", output.c_str());

    if (exitCode != 0)
        bRet = false;

    return bRet;
}
