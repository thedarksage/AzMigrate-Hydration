#include "util/common/util.h"
#include "Oraclevalidators.h"
#include "config/applocalconfigurator.h"
#include "appcommand.h"
#include "controller.h"
#include <sstream>


OracleTestValidator::OracleTestValidator(const std::string& name, 
                        const std::string& description,
                        InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}
OracleTestValidator::OracleTestValidator(InmRuleIds ruleId)
:Validator(ruleId)
{

}

SVERROR OracleTestValidator::evaluate()
{

	SVERROR bRet = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	std::stringstream stream ;

    stream.clear();
    stream << "Oracle Test Validator evaluate done" << std::endl;
	setRuleExitCode(RULE_PASSED);
       setStatus( INM_RULESTAT_PASSED )  ;

    std::string ruleMessage = stream.str();
	setRuleMessage(ruleMessage);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
      return bRet ;

}

SVERROR OracleTestValidator::fix()
{
	SVERROR bRet = SVS_FALSE ;
    	return bRet ;
}

bool OracleTestValidator::canfix()
{
    bool bRet = false ;
    return bRet ;
}

OracleDatabaseRunValidator::OracleDatabaseRunValidator(const std::string& name, 
                        const std::string& description,
                        const OracleAppVersionDiscInfo& appInfo,
                        InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_oracleAppInfo(appInfo)
{
}

OracleDatabaseRunValidator:: OracleDatabaseRunValidator(const OracleAppVersionDiscInfo &appInfo, InmRuleIds ruleId)
:Validator(ruleId),
m_oracleAppInfo(appInfo)
{

}

SVERROR OracleDatabaseRunValidator::evaluate()
{

	SVERROR bRet = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	InmRuleErrorCode errorCode = INM_ERROR_NONE;
	std::stringstream stream ;

    stream.clear();
	if(!(m_oracleAppInfo.m_dbName.empty()))
	{
		stream << "Oracle DatabaseRun Validator evaluate done" << std::endl;
		ruleStatus = INM_RULESTAT_PASSED;
		errorCode = RULE_PASSED;
	}
	else
	{
		stream << "Oracle Database Not Discovered" << std::endl;
		ruleStatus = INM_RULESTAT_FAILED;
		errorCode = DATABASE_NOT_RUNNING;
	}
	
	setRuleExitCode(errorCode);
       setStatus( ruleStatus )  ;
       std::string ruleMessage = stream.str();
	setRuleMessage(ruleMessage);
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
      return bRet ;
}

SVERROR OracleDatabaseRunValidator::fix()
{
	SVERROR bRet = SVS_FALSE ;
    	return bRet ;
}

bool OracleDatabaseRunValidator::canfix()
{
	bool bRet = false ;
    return bRet ;
}

OracleConsistencyValidator::OracleConsistencyValidator(const std::string& name, 
                        const std::string& description,
                        const OracleAppVersionDiscInfo& appInfo,
                        InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_oracleAppInfo(appInfo)
{
}

OracleConsistencyValidator::OracleConsistencyValidator(const OracleAppVersionDiscInfo &appInfo, InmRuleIds ruleId)
:Validator(ruleId),
m_oracleAppInfo(appInfo)
{

}

SVERROR OracleConsistencyValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	InmRuleErrorCode errorCode = INM_ERROR_NONE;
	std::stringstream stream ;
    std::string srcConfigFile;

	SVERROR bRet = SVS_FALSE;
    ruleStatus = INM_RULESTAT_FAILED;
    errorCode = ORACLE_RECOVERY_LOG_ERROR;

    stream.clear();

    if(!(m_oracleAppInfo.m_dbName.empty()))
    {
        if(!m_oracleAppInfo.m_recoveryLogEnabled)
        {
            stream << "Oracle database is not in Archive Log mode. Cannot issue consistency tags." << std::endl;
        }
        else
        {
            stream << "Oracle database is in Archive Log mode." << std::endl;
     
            if(!m_oracleAppInfo.m_filesMap.empty())
            {
                std::map<std::string, std::string>::const_iterator fileMapIter = m_oracleAppInfo.m_filesMap.begin();
                while(fileMapIter != m_oracleAppInfo.m_filesMap.end())
                {
                    std::list<std::string> fileExtension = tokenizeString((fileMapIter)->first, ".");
                    if("conf" == fileExtension.back())
                    {
                        srcConfigFile = "/tmp/" + (fileMapIter)->first;
                        std::ofstream out;
                        out.open(srcConfigFile.c_str(), std::ios::trunc);
                        if (!out) 
                        {
                            DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", srcConfigFile.c_str());
                            break;
                        }
                        out << (fileMapIter)->second;       
                        out.close();
                        break;
                    }
                    fileMapIter++;
                }

                if (!srcConfigFile.empty())
                {
                    SV_ULONG exitCode = 1 ;
                    std::string strCommnadToExecute ;
                    AppLocalConfigurator localconfigurator ;
                    strCommnadToExecute += localconfigurator.getInstallPath();
                    strCommnadToExecute += "/scripts/oracle_consistency.sh ";
                    strCommnadToExecute += "validate ";
                    strCommnadToExecute += m_oracleAppInfo.m_dbName;
                    strCommnadToExecute += " readinesstag ";
                    strCommnadToExecute += srcConfigFile;
                    strCommnadToExecute += " ";

                    DebugPrintf(SV_LOG_INFO,"\n The command to execute : %s\n",strCommnadToExecute.c_str());

                    stream << strCommnadToExecute;

                    std::string outputFileName = "/tmp/tagreadiness.log";
                    ACE_OS::unlink(outputFileName.c_str());

                    AppCommand objAppCommand(strCommnadToExecute, 0, outputFileName);

                    std::string output;

                    if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) != SVS_OK)
                    {
                        DebugPrintf(SV_LOG_ERROR,"Failed to run script.\n");
                        stream << "Failed readiness check. Output - " << output << std::endl ;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_INFO,"Successfully spawned the readiness script.\n");

                        if (exitCode != 0)
                        {
                            DebugPrintf(SV_LOG_ERROR,"script returned error.\n");
                            stream << "Failed readiness check. Output - " << output << std::endl ;
                        }
                        else
                        {
                            stream << "Readiness successful for database " << m_oracleAppInfo.m_dbName << std::endl ;
                            ruleStatus = INM_RULESTAT_PASSED;
                            errorCode = RULE_PASSED;
                            bRet = SVS_OK;
                        }
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR,"Database config file is not found\n", srcConfigFile.c_str());
                    stream << "Database discovery config file not found." << std::endl;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"Database config files are not found\n", srcConfigFile.c_str());
                stream << "Database discovery config files not found." << std::endl;
            }
        }
    }
    else
    {
		stream << "Oracle Database Not Discovered." << std::endl;
		errorCode = ORACLE_RECOVERY_LOG_ERROR;
    }

    setRuleExitCode(errorCode);
    setStatus( ruleStatus )  ;
    std::string ruleMessage = stream.str();
    setRuleMessage(ruleMessage);
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR OracleConsistencyValidator::fix()
{
	SVERROR bRet = SVS_FALSE ;
    	return bRet ;
}

bool OracleConsistencyValidator::canfix()
{
	bool bRet = false ;
    return bRet ;
}

OSVersionCompatibilityCheckValidator:: OSVersionCompatibilityCheckValidator(const std::string &name,
				const std::string &description, 
				std::string &srcOSVersion, 
				std::string &tgtOSVersion, 
				InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_srcOSVersion(srcOSVersion),
m_tgtOSVersion(tgtOSVersion)
{

}

OSVersionCompatibilityCheckValidator:: OSVersionCompatibilityCheckValidator(std::string &srcOSVersion, 
				std::string &tgtOSVersion, 
				InmRuleIds ruleId)
:Validator(ruleId),
m_srcOSVersion(srcOSVersion),
m_tgtOSVersion(tgtOSVersion)
{

}


SVERROR OSVersionCompatibilityCheckValidator::evaluate()
{

	SVERROR bRet = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	InmRuleErrorCode errorCode = INM_ERROR_NONE;
	std::stringstream stream ;

    /*stream.clear();
	if(m_tgtOSVersion == m_srcOSVersion)
	{
		stream << "Oracle OSVersion Validator evaluate done" << std::endl;
		ruleStatus = INM_RULESTAT_PASSED;
		errorCode = RULE_PASSED;
	}
	else
	{
		stream << "OS Versions Do Not Match";
		ruleStatus = INM_RULESTAT_FAILED;
		errorCode = OS_VERSION_MISMATCH_ERROR;
	}
	
	setRuleExitCode(errorCode);
       setStatus( ruleStatus )  ;
       std::string ruleMessage = stream.str();
	setRuleMessage(ruleMessage);
	*/
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
      return bRet ;
}

SVERROR OSVersionCompatibilityCheckValidator::fix()
{
	SVERROR bRet = SVS_FALSE ;
    	return bRet ;
}

bool OSVersionCompatibilityCheckValidator::canfix()
{
	bool bRet = false ;
    return bRet ;
}

DatabaseVersionCompatibilityCheckValidator:: DatabaseVersionCompatibilityCheckValidator(const std::string &name,
				const std::string &description, 
				std::string &srcDatabaseVersion, 
				std::string &tgtDatabaseVersion, 
				InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_srcDatabaseVersion(srcDatabaseVersion),
m_tgtDatabaseVersion(tgtDatabaseVersion)
{

}

DatabaseVersionCompatibilityCheckValidator:: DatabaseVersionCompatibilityCheckValidator(std::string &srcDatabaseVersion, 
				std::string &tgtDatabaseVersion, 
				InmRuleIds ruleId)
:Validator(ruleId),
m_srcDatabaseVersion(srcDatabaseVersion),
m_tgtDatabaseVersion(tgtDatabaseVersion)
{

}


SVERROR DatabaseVersionCompatibilityCheckValidator::evaluate()
{

	SVERROR bRet = SVS_OK;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	InmRuleErrorCode errorCode = INM_ERROR_NONE;
	std::stringstream stream ;

    /*	stream.clear();
	if(m_srcDatabaseVersion == m_tgtDatabaseVersion)
	{
		stream << "Oracle Database Version Validator evaluate done" << std::endl;
		ruleStatus = INM_RULESTAT_PASSED;
		errorCode = RULE_PASSED;
	}
	else
	{
		stream << "Oracle Database Versions are not Compatible";
		ruleStatus = INM_RULESTAT_FAILED;
		errorCode = DB_VERSION_COMPATIBILITY_ERROR;
	}
	
	setRuleExitCode(errorCode);
       setStatus( ruleStatus )  ;
       std::string ruleMessage = stream.str();
	setRuleMessage(ruleMessage);
	*/
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
      return bRet ;
}

SVERROR DatabaseVersionCompatibilityCheckValidator::fix()
{
	SVERROR bRet = SVS_FALSE ;
    	return bRet ;
}

bool DatabaseVersionCompatibilityCheckValidator::canfix()
{
	bool bRet = false ;
    return bRet ;
}

OracleDatabaseShutdownValidator::OracleDatabaseShutdownValidator(const std::string& name, 
                        const std::string& description,
                        const std::list<std::map<std::string,std::string> > &dbInfo,
                        InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_oracleAppInfo(dbInfo)
{
}

OracleDatabaseShutdownValidator::OracleDatabaseShutdownValidator(const std::list<std::map<std::string,std::string> > &dbInfo, InmRuleIds ruleId)
:Validator(ruleId),
m_oracleAppInfo(dbInfo)
{

}

SVERROR OracleDatabaseShutdownValidator::evaluate()
{

    SVERROR bRet = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    InmRuleStatus ruleStatus = INM_RULESTAT_PASSED;
    InmRuleErrorCode errorCode = RULE_PASSED;
    std::stringstream stream ;

    stream.clear();

    std::list<std::map<std::string, std::string> >::iterator dbInfoIter = m_oracleAppInfo.begin();
    while(dbInfoIter != m_oracleAppInfo.end())
    {
        std::string dbInstanceName = dbInfoIter->find("dbName")->second;
        std::string status = dbInfoIter->find("Status")->second;

        if (status.compare("Offline") == 0)
        {
            stream << "Oracle Database " << dbInstanceName << " is shutdown." << std::endl;
        }
        else
        {
            stream << "Oracle Database " << dbInstanceName << " is running." << std::endl;
            bRet = SVS_FALSE;
        }

        dbInfoIter++;
    }


    if (bRet == SVS_FALSE)
    {
        ruleStatus = INM_RULESTAT_FAILED;
        errorCode = DATABASE_NOT_SHUTDOWN;
    }
    else
    {
        stream << "Done." << std::endl ;
    }

	setRuleExitCode(errorCode);
    setStatus( ruleStatus )  ;
    std::string ruleMessage = stream.str();
	setRuleMessage(ruleMessage);
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR OracleDatabaseShutdownValidator::fix()
{
	SVERROR bRet = SVS_FALSE ;
    	return bRet ;
}

bool OracleDatabaseShutdownValidator::canfix()
{
	bool bRet = false ;
    return bRet ;
}

OracleDatabaseConfigValidator::OracleDatabaseConfigValidator(const std::string& name, 
                        const std::string& description,
                        const std::list<std::map<std::string,std::string> > &dbInfo,
                        const std::list<std::map<std::string,std::string> > &volInfo,
                        InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_oracleAppInfo(dbInfo),
m_tgtVolInfo(volInfo)
{
}

OracleDatabaseConfigValidator::OracleDatabaseConfigValidator(
                        const std::list<std::map<std::string,std::string> > &dbInfo,
                        const std::list<std::map<std::string,std::string> > &volInfo,
                        InmRuleIds ruleId)
:Validator(ruleId),
m_oracleAppInfo(dbInfo),
m_tgtVolInfo(volInfo)
{

}

SVERROR OracleDatabaseConfigValidator::evaluate()
{

    SVERROR bRet = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    InmRuleStatus ruleStatus = INM_RULESTAT_PASSED;
    InmRuleErrorCode errorCode = RULE_PASSED;
    std::stringstream stream ;

    stream.clear();

    AppLocalConfigurator localconfigurator;
    std::string tgtVolInfoFile = localconfigurator.getInstallPath() + "/etc/OracleTgtVolInfoFile.dat";
    std::ofstream out;

    out.open(tgtVolInfoFile.c_str(), std::ios::trunc);
    if (!out) 
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", tgtVolInfoFile.c_str());
        bRet = SVS_FALSE;
    }
    else
    { 
        std::list<std::map<std::string, std::string> >::iterator volInfoIter = m_tgtVolInfo.begin();

        while (volInfoIter != m_tgtVolInfo.end())
        {
            std::string deviceName = volInfoIter->find("DeviceName")->second;
            std::string fsType = volInfoIter->find("FileSystemType")->second;
            std::string volType = volInfoIter->find("VolumeType")->second;
            std::string vendor = volInfoIter->find("VendorOrigin")->second;
            std::string mountPoint = volInfoIter->find("MountPoint")->second;
            std::string diskGroup = volInfoIter->find("DiskGroup")->second;
            std::string shouldDestroy = volInfoIter->find("shouldDestroy")->second;

            if (fsType.empty()) fsType = "0";
            if (mountPoint.empty()) mountPoint = "0";
            if (diskGroup.empty()) diskGroup = "0";

            DebugPrintf( SV_LOG_DEBUG, "deviceName: '%s' fsType: '%s' volType: '%s' vendor: '%s' mountPoint: '%s' diskGroup: '%s' shouldDestroy : '%s'\n", deviceName.c_str(), fsType.c_str(), volType.c_str(), vendor.c_str(), mountPoint.c_str(), diskGroup.c_str(), shouldDestroy.c_str()) ;

            std::string line = "DeviceName=" + deviceName + ":";
            line += "FileSystemType=" + fsType + ":";
            line += "VolumeType=" + volType + ":";
            line += "VendorOrigin=" + vendor + ":";
            line += "MountPoint=" + mountPoint + ":";
            line += "DiskGroup=" + diskGroup + ":";
            line += "shouldDestroy=" + shouldDestroy + " ";

            DebugPrintf( SV_LOG_DEBUG, "tgtVolInfo %s\n", line.c_str());

            out << line << std::endl;

            volInfoIter++;
        }

        out.close();
    }

    std::list<std::map<std::string, std::string> >::iterator dbInfoIter = m_oracleAppInfo.begin();
    while(dbInfoIter != m_oracleAppInfo.end())
    {
        std::string dbInstanceName = dbInfoIter->find("dbName")->second;
        std::string fileName = dbInfoIter->find("configFileName")->second;
        std::string config = dbInfoIter->find("configFileContents")->second;

        AppLocalConfigurator localconfigurator ;
        std::string installPath = localconfigurator.getInstallPath();
        // std::string srcConfigFile = installPath + "/etc/" + fileName;
         std::string srcConfigFile = "/tmp/" + fileName;

        std::ofstream out;
        out.open(srcConfigFile.c_str(), std::ios::trunc);
        if (!out)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", srcConfigFile.c_str());
            stream << "Failed to open config file " << srcConfigFile << std::endl ;
            bRet = SVS_FALSE;
            break;
        }
        out << config;
        out.close();

        DebugPrintf( SV_LOG_DEBUG, "Src DB Instance Name: '%s' Conf filename : '%s' \n", dbInstanceName.c_str(), srcConfigFile.c_str()) ;

        SV_ULONG exitCode = 1 ;
        std::string strCommnadToExecute ;
        strCommnadToExecute += localconfigurator.getInstallPath();
        strCommnadToExecute += "/scripts/oracleappagent.sh ";
        strCommnadToExecute += "targetreadiness ";
        strCommnadToExecute += srcConfigFile;
        strCommnadToExecute += std::string(" ");

        DebugPrintf(SV_LOG_INFO,"\n The targetreadiness command to execute : %s\n",strCommnadToExecute.c_str());

        stream << strCommnadToExecute;

        std::string outputFileName = "/tmp/oratgtreadiness.log";
        ACE_OS::unlink(outputFileName.c_str());

        AppCommand objAppCommand(strCommnadToExecute, 0, outputFileName);

        std::string output;

        if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive ) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed target readiness script.\n");
            stream << "Failed target readiness. Output - " << output << std::endl ;
            bRet = SVS_FALSE;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"Successfully spawned the target readiness script.\n");

            if (exitCode != 0)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed oracle prepare target script.\n");
                stream << "Failed target readiness. Output - " << output << std::endl ;
                bRet = SVS_FALSE;
                break;
            }

            stream << "Target readiness successful for db name " << dbInstanceName << std::endl ;
        }

        dbInfoIter++;
    }

    if (bRet == SVS_FALSE)
    {
        ruleStatus = INM_RULESTAT_FAILED;
        errorCode = DATABASE_CONFIG_ERROR;
    }
    else
    {
        stream << "Target Oracle database configuration is in sync with the source." << std::endl ;
    }

	setRuleExitCode(errorCode);
    setStatus( ruleStatus )  ;
    std::string ruleMessage = stream.str();
	setRuleMessage(ruleMessage);
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR OracleDatabaseConfigValidator::fix()
{
	SVERROR bRet = SVS_FALSE ;
    	return bRet ;
}

bool OracleDatabaseConfigValidator::canfix()
{
	bool bRet = false ;
    return bRet ;
}
