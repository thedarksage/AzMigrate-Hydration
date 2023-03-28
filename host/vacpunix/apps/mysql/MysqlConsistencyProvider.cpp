#include "fstream"
#include "appcommand.h"
#include "localconfigurator.h"
#include "MysqlConsistencyProvider.h"

MysqlConsistencyProviderPtr MysqlConsistencyProvider::m_instancePtr;
boost::mutex MysqlConsistencyProvider::m_instanceMutex;

MysqlConsistencyProvider::MysqlConsistencyProvider()
{

    LocalConfigurator localConfigurator;
    installPath = localConfigurator.getInstallPath() + ACE_DIRECTORY_SEPARATOR_CHAR;
    discoveryOutputFile = installPath + "etc/mysqldiscovery.dat";
    logFile = "/tmp/mysqltags.log";
}

MysqlConsistencyProvider::~MysqlConsistencyProvider()
{
}


MysqlConsistencyProviderPtr MysqlConsistencyProvider::getInstance()
{
    if (m_instancePtr.get() == NULL)
    {
        boost::unique_lock<boost::mutex> lock(m_instanceMutex);
        if (m_instancePtr.get() == NULL)
        {
            m_instancePtr.reset(new MysqlConsistencyProvider);
        }
    }

    return m_instancePtr;
}

bool MysqlConsistencyProvider::Discover()
{
    bool bRet = true;
    std::string script = installPath + "scripts/mysqlappagent.sh discover ";
    script += discoveryOutputFile;
    
    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;

    AppCommand objAppCommand(script, 0, discoveryOutputFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        std::cout << "MySQL Discovery failed" << std::endl;
        bRet = false;
    }

    std::cout << output.c_str() << std::endl;

    if (exitCode != 0)
        bRet = false;

    return bRet;
}

bool MysqlConsistencyProvider::Freeze()
{
    std::string script = installPath + "scripts/mysql_consistency.sh freeze ALL NONE NONE";
    bool bRet = true;

    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;
    std::ofstream logFileStream(logFile.c_str(), std::ifstream::trunc); 
    logFileStream.close();

    AppCommand objAppCommand(script, 0, logFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        std::cout << "MySQL Freeze failed" << std::endl;
        bRet = false;
    }

    std::cout << output.c_str() << std::endl;

    if (exitCode != 0)
        bRet = false;

    return bRet;
}

bool MysqlConsistencyProvider::Unfreeze()
{
    std::string script = installPath + "scripts/mysql_consistency.sh unfreeze ALL NONE NONE";
    bool bRet = true;

    SV_ULONG exitCode;
    std::string output;
    bool bProcessActive = true;
    std::ofstream logFileStream(logFile.c_str(), std::ifstream::trunc); 
    logFileStream.close();

    AppCommand objAppCommand(script, 0, logFile);

    if (objAppCommand.Run(exitCode, output, bProcessActive) != SVS_OK)
    {
        std::cout << "MySQL Unfreeze failed" << std::endl;
        bRet = false;
    }

    std::cout << output.c_str() << std::endl;

    if (exitCode != 0)
        bRet = false;

    return bRet;
}
