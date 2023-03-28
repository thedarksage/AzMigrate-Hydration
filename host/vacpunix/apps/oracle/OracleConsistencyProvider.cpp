#include "fstream"
#include "appcommand.h"
#include "localconfigurator.h"
#include "OracleConsistencyProvider.h"

extern void inm_printf(const char* format, ... );
extern void inm_printf(short logLevel, const char* format, ... );

OracleConsistencyProviderPtr OracleConsistencyProvider::m_instancePtr;
boost::mutex OracleConsistencyProvider::m_instanceMutex;

OracleConsistencyProvider::OracleConsistencyProvider()
{

    LocalConfigurator localConfigurator;
    installPath = localConfigurator.getInstallPath() + ACE_DIRECTORY_SEPARATOR_CHAR;
    discoveryOutputFile = installPath + "etc/oraclediscovery.dat";
    logFile = "/tmp/oratags.log";
    bDbsFrozen = false;
}

OracleConsistencyProvider::~OracleConsistencyProvider()
{
}


OracleConsistencyProviderPtr OracleConsistencyProvider::getInstance()
{
    if (m_instancePtr.get() == NULL)
    {
        boost::unique_lock<boost::mutex> lock(m_instanceMutex);
        if (m_instancePtr.get() == NULL)
        {
            m_instancePtr.reset(new OracleConsistencyProvider);
        }
    }

    return m_instancePtr;
}

bool OracleConsistencyProvider::Discover()
{
    std::string script = installPath + "scripts/oracleappagent.sh discover ";
    script += discoveryOutputFile;
    bool bRet = true;
    
    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;

    AppCommand objAppCommand(script, 0, discoveryOutputFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        inm_printf("Oracle database Discovery failed.\n");
        bRet = false;
    }

    inm_printf("%s.\n", output.c_str());

    if (exitCode != 0)
        bRet = false;

    return bRet;
}

bool OracleConsistencyProvider::Freeze()
{
    std::string script = installPath + "scripts/oracle_consistency.sh freeze ALL NONE NONE";
    bool bRet = false;

    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;
    std::ofstream logFileStream(logFile.c_str(), std::ifstream::trunc); 
    logFileStream.close();

    AppCommand objAppCommand(script, 0, logFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        inm_printf("Oracle database Freeze failed.\n");
    }

    inm_printf("%s.\n", output.c_str());

    if (exitCode == 0)
    {
        bRet = true;
        bDbsFrozen = true;
    }
    else if (exitCode == 2)
    {
        bRet = true;
    }

    return bRet;
}

bool OracleConsistencyProvider::Unfreeze()
{
    if (!bDbsFrozen)
        return true;

    std::string script = installPath + "scripts/oracle_consistency.sh unfreeze ALL NONE NONE";
    bool bRet = true;

    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;
    std::ofstream logFileStream(logFile.c_str(), std::ifstream::trunc); 
    logFileStream.close();

    AppCommand objAppCommand(script, 0, logFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        inm_printf("Oracle database Resume failed.\n");
        bRet = false;
    }

    inm_printf("%s.\n", output.c_str());

    if (exitCode != 0)
        bRet = false;

    return bRet;
}
