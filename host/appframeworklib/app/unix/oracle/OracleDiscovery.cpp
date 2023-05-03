#include <sstream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include "host.h"
#include "OracleDiscovery.h"
//#include "util/service.h"
#include "util/common/util.h"
#include "inmcommand.h"
#include "cdpsnapshotrequest.h"
#include <string>
// #include "localconfigurator.h"
#include "portablehelpers.h"
#include "config/appconfigurator.h"
#include "appcommand.h"
#include "controller.h"

ACE_Recursive_Thread_Mutex OracleDiscoveryImpl::m_mutex;

boost::shared_ptr<OracleDiscoveryImpl> OracleDiscoveryImpl::m_instancePtr ;

boost::shared_ptr<OracleDiscoveryImpl> OracleDiscoveryImpl::getInstance()
{
    if( m_instancePtr.get() == NULL )
    {
        m_instancePtr.reset(new OracleDiscoveryImpl()) ;
    }
    return m_instancePtr ;
}

SVERROR OracleDiscoveryImpl::fillOracleClusterDiscInfo( std::map<std::string, std::string> &clusterDetailsMap, 
    OracleAppDiscoveryInfo &oracleAppDiscInfo)
{

	DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleDiscoveryImpl::%s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;

	if(clusterDetailsMap.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, " Cluster Data is empty\n");
        return SVS_FALSE;
    }

    OracleClusterDiscInfo oracleClusterDiscInfo;

    if (clusterDetailsMap.find("ClusterName") != clusterDetailsMap.end())
    {
        oracleClusterDiscInfo.m_clusterName = clusterDetailsMap.find("ClusterName")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ClusterName not found in cluster info\n");
        return SVS_FALSE;
    }

    if (clusterDetailsMap.find("NodeName") != clusterDetailsMap.end())
    {
        oracleClusterDiscInfo.m_nodeName =  clusterDetailsMap.find("NodeName")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "NodeName not found in cluster info\n");
        return SVS_FALSE;
    }

    std::string otherNodes;
    if (clusterDetailsMap.find("OtherClusterNodes") != clusterDetailsMap.end())
    {
        otherNodes = clusterDetailsMap.find("OtherClusterNodes")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "OtherClusterNodes not found in cluster info\n");
        return SVS_FALSE;
    }

    if (!otherNodes.empty())
    {
        oracleClusterDiscInfo.m_otherNodes = tokenizeString(clusterDetailsMap.find("OtherClusterNodes")->second, ",");

        if("" == oracleClusterDiscInfo.m_otherNodes.back())
        {
            oracleClusterDiscInfo.m_otherNodes.pop_back();
        }
    }

    oracleAppDiscInfo.m_dbOracleClusterDiscInfo.insert(std::make_pair(oracleClusterDiscInfo.m_clusterName, oracleClusterDiscInfo));
    
    bRet = SVS_OK;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;	
}

SVERROR OracleDiscoveryImpl::fillOracleUnregisterInfo( std::map<std::string, std::string> &unregisterDetailsMap, 
    OracleAppDiscoveryInfo &oracleAppDiscInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleDiscoveryImpl::%s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;

	if(unregisterDetailsMap.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, " Unregister Database map is empty\n");
        return SVS_FALSE;
    }

    OracleUnregisterDiscInfo oracleUnregisterDiscInfo;

    if (unregisterDetailsMap.find("Unregister") != unregisterDetailsMap.end())
    {
        oracleUnregisterDiscInfo.m_dbName = unregisterDetailsMap.find("Unregister")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Unregister dbname not found in discovery\n");
        return SVS_FALSE;
    }

    oracleAppDiscInfo.m_dbUnregisterDiscInfo.insert(std::make_pair(oracleUnregisterDiscInfo.m_dbName, oracleUnregisterDiscInfo));

    bRet = SVS_OK;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return bRet;
}

SVERROR OracleDiscoveryImpl::fillOracleAppVersionDiscInfo(std::map<std::string, std::string>& discoveryOutputMap,OracleAppDiscoveryInfo &oracleAppDiscInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleDiscoveryImpl::%s\n", FUNCTION_NAME) ;
	
	OracleAppVersionDiscInfo oracleVersionDiscInfo;
	SVERROR bRet = SVS_FALSE ;
	
	if(discoveryOutputMap.empty())
    {
        DebugPrintf(SV_LOG_ERROR, " Discovery Data not in Correct Format\n");
        return SVS_FALSE;
    }

    LocalConfigurator localConfigurator;
    std::string installPath = localConfigurator.getInstallPath();


    if (discoveryOutputMap.find("PFileLocation") != discoveryOutputMap.end())
    {
        std::string pFileLocation = discoveryOutputMap.find("PFileLocation")->second;
        std::string fileName = basename_r(pFileLocation.c_str(), '/');

        std::string pFileData = getFileContents(pFileLocation);
        if(pFileData.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "PFile is Empty \n");
            return SVS_FALSE;
        }

        oracleVersionDiscInfo.m_filesMap.insert(std::make_pair(fileName, pFileData));
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "PFile is not found in discovery\n");
        return SVS_FALSE;
    }



    if (discoveryOutputMap.find("GeneratedPfile") != discoveryOutputMap.end())
    {
        std::string generatedPfile = discoveryOutputMap.find("GeneratedPfile")->second;
        std::string generatedPfileName = basename_r(generatedPfile.c_str(), '/');

        std::string generatedPfileData = getFileContents(generatedPfile);
        if(generatedPfileData.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "Generated PFile is Empty \n");
            return SVS_FALSE;
        }

        oracleVersionDiscInfo.m_filesMap.insert(std::make_pair(generatedPfileName, generatedPfileData));
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Generated PFile is not found in discovery\n");
        return SVS_FALSE;
    }


    if (discoveryOutputMap.find("ConfigFile") != discoveryOutputMap.end())
    {
        std::string dbConfFileLocation = discoveryOutputMap.find("ConfigFile")->second;
        std::string dbConfFileName = basename_r(dbConfFileLocation.c_str(), '/');

        std::string dbConfFileContents = getFileContents(dbConfFileLocation);
        if(dbConfFileContents.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "ConfigFile is Empty \n");
            return SVS_FALSE;
        }
        oracleVersionDiscInfo.m_filesMap.insert(std::make_pair(dbConfFileName, dbConfFileContents));
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Config file is not found in discovery\n");
        return SVS_FALSE;
    }
 
 
    if (discoveryOutputMap.find("DBName") != discoveryOutputMap.end())
    {
        oracleVersionDiscInfo.m_dbName = discoveryOutputMap.find("DBName")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "DB Name not found in discovery\n");
        return SVS_FALSE;
    }
	
    if (discoveryOutputMap.find("DBVersion") != discoveryOutputMap.end())
    {
        oracleVersionDiscInfo.m_dbVersion = discoveryOutputMap.find("DBVersion") ->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "DB Version not found in discovery\n");
        return SVS_FALSE;
    }

    if (discoveryOutputMap.find("InstallPath") != discoveryOutputMap.end())
    {
        oracleVersionDiscInfo.m_dbInstallPath = discoveryOutputMap.find("InstallPath")->second;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "InstallPath not found in discovery\n");
        return SVS_FALSE;
    }

    if (discoveryOutputMap.find("isCluster") != discoveryOutputMap.end())
    {
        if("NO" == discoveryOutputMap.find("isCluster")->second)
        {
            oracleVersionDiscInfo.m_isCluster = "false";
        }
        else
        {
            oracleVersionDiscInfo.m_isCluster = "true";
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "isCluster not found in discovery\n");
        return SVS_FALSE;
    }

    if (discoveryOutputMap.find("RecoveryLogEnabled") != discoveryOutputMap.end())
    {
        if("NOARCHIVELOG" == discoveryOutputMap.find("RecoveryLogEnabled")->second)
        {
            oracleVersionDiscInfo.m_recoveryLogEnabled = false;    
        }
        else
        {
            oracleVersionDiscInfo.m_recoveryLogEnabled = true;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "RecoveryLogEnabled not found in discovery\n");
        return SVS_FALSE;
    }

	if(oracleVersionDiscInfo.m_isCluster == "true")
	{
        if (discoveryOutputMap.find("NodeName") != discoveryOutputMap.end())
        {
            oracleVersionDiscInfo.m_nodeName = discoveryOutputMap.find("NodeName")->second ;    
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "NodeName not found in discovery\n");
            return SVS_FALSE;
        }

        if (discoveryOutputMap.find("ClusterName") != discoveryOutputMap.end())
        {
            oracleVersionDiscInfo.m_clusterName = discoveryOutputMap.find("ClusterName")->second;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "ClusterName not found in discovery\n");
            return SVS_FALSE;
        }

        std::string otherNodes;
        if (discoveryOutputMap.find("OtherClusterNodes") != discoveryOutputMap.end())
        {
            otherNodes = discoveryOutputMap.find("OtherClusterNodes")->second;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "OtherClusterNodes not found in discovery\n");
            return SVS_FALSE;
        }

        if (!otherNodes.empty())
        {
            oracleVersionDiscInfo.m_otherClusterNodes = tokenizeString(discoveryOutputMap.find("OtherClusterNodes")->second, ",");

            if("" == oracleVersionDiscInfo.m_otherClusterNodes.back())
            {
                oracleVersionDiscInfo.m_otherClusterNodes.pop_back();
            }
        }
	}
    else
        DebugPrintf(SV_LOG_DEBUG, "isCluster = FALSE\n");

	std::pair<std::string, std::string> diskNameLevelPair;

    if (discoveryOutputMap.find("ViewLevels") != discoveryOutputMap.end())
    {
        oracleVersionDiscInfo.m_diskView = tokenizeString(discoveryOutputMap.find("ViewLevels")->second, ";");
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ViewLevels not found in discovery\n");
        return SVS_FALSE;
    }
    
    if (discoveryOutputMap.find("FilterDevices") != discoveryOutputMap.end())
    {
        oracleVersionDiscInfo.m_filterDeviceList = tokenizeString(discoveryOutputMap.find("FilterDevices")->second.c_str(), ",");
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "FilterDevices not found in discovery\n");
        return SVS_FALSE;
    }
 
    oracleAppDiscInfo.m_dbOracleDiscInfo.insert(std::make_pair(oracleVersionDiscInfo.m_dbName, oracleVersionDiscInfo));

    bRet = SVS_OK;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;	

}

SVERROR OracleDiscoveryImpl::getOracleDiscoveryData(OracleAppDiscoveryInfo & OracleAppDiscInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleDiscoveryImpl::%s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK;
    std::multimap<std::string, std::string> attrMap ;

    OracleAppVersionDiscInfo oracleVersionDiscInfo;

	std::string discoveryOutput;

    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_mutex);

    AppLocalConfigurator localconfigurator;
    std::string installPath = localconfigurator.getInstallPath();
    std::string outputFile = installPath + "etc/discoveryoutput.dat";
 
    SV_ULONG exitCode = 1 ;
    std::string strCommnadToExecute ;
    strCommnadToExecute += installPath;
    strCommnadToExecute += "scripts/oracleappagent.sh discover ";
    strCommnadToExecute += outputFile;
    strCommnadToExecute += std::string(" ");

    DebugPrintf(SV_LOG_INFO,"\n The discovery command to execute : %s\n",strCommnadToExecute.c_str());

    AppCommand objAppCommand(strCommnadToExecute, 0, outputFile);

    std::string output;

    if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to spawn oracle discovery script %s.\n", strCommnadToExecute.c_str());
        bRet = SVS_FALSE;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Successfully spawned the oracle discover script.\n");
        DebugPrintf(SV_LOG_INFO,"output. %s\n", output.c_str());

        if (exitCode != 0)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed oracle discover script %s.\n", strCommnadToExecute.c_str());
            bRet = SVS_FALSE;
        }
    }

    if ( bRet == SVS_FALSE )
    {
        return bRet;
    }
                  
    discoveryOutput = getFileContents(outputFile);

    if(discoveryOutput.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "Discovery Output File is Empty \n");
        return SVS_FALSE;
    }

	std::list<std::string> discoveryScriptOutputTokensList, keyValueTokens;
	std::map<std::string, std::string> discoveryDataMap, clusterDetailsMap, unregisterMap;

	std::string viewLevels, filterDevices, outputTokenIterValue;


    // --- Discovery output ---
    //  "OracleDatabaseDiscoveryStart"
    //      "ClusterInfoStart"
    //          ...             // cluster info
    //      "ClusterInfoEnd"
    //      "DatabaseListStart"
    //          "DatabaseStart"
    //              "Unregister=abc"
    //          "DatabaseEnd"
    //          "DatabaseStart"
    //              ...         // config info
    //              "DBName=xyz"
    //              ...`        // view levels + filter devices
    //          "DatabaseEnd"
    //      "DatabaseListEnd"
    //  "OracleDatabaseDiscoveryEnd"


	discoveryScriptOutputTokensList = tokenizeString(discoveryOutput, "\n");

    if ((discoveryScriptOutputTokensList.front() != "OracleDatabaseDiscoveryStart") ||
        (discoveryScriptOutputTokensList.back() != "OracleDatabaseDiscoveryEnd"))
    {
        DebugPrintf(SV_LOG_ERROR, "Unrecognized Output File Format\n");
        return SVS_FALSE;
    }

	std::list<std::string>::iterator outputTokenIter = discoveryScriptOutputTokensList.begin();

    // skip "OracleDatabaseDiscoveryStart"
    outputTokenIter++;
    if(outputTokenIter == discoveryScriptOutputTokensList.end())
    {
        DebugPrintf(SV_LOG_ERROR, "Unexpected end while looking for ClusterInfoStart\n");
        return SVS_FALSE;
    }

    outputTokenIterValue = *outputTokenIter;

    if (outputTokenIterValue.compare("ClusterInfoStart") == 0)
    {
        outputTokenIter++;
        if(outputTokenIter == discoveryScriptOutputTokensList.end())
        {
            DebugPrintf(SV_LOG_ERROR, "Unexpected end while looking for ClusterInfoEnd\n");
            return SVS_FALSE;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ClusterInfoStart not found\n");
        return SVS_FALSE;
    }

    while(outputTokenIter != discoveryScriptOutputTokensList.end())
    {
        outputTokenIterValue = *outputTokenIter;

        if (outputTokenIterValue.compare("ClusterInfoEnd") == 0)
        {
            if(!clusterDetailsMap.empty())
            {
                bRet = fillOracleClusterDiscInfo(clusterDetailsMap, OracleAppDiscInfo);

                if ( bRet == SVS_FALSE )
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to fill oracle cluster info\n");
                    return bRet;
                }
            }
            break;
        }

        keyValueTokens = tokenizeString((*outputTokenIter), "=");

        if(keyValueTokens.size() == 2)
        {
            clusterDetailsMap.insert(std::make_pair(keyValueTokens.front(), keyValueTokens.back()));
        }
        else if(keyValueTokens.size() == 1 && keyValueTokens.front() == "OtherClusterNodes" )
        {
            clusterDetailsMap.insert(std::make_pair(keyValueTokens.front(), ""));
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unexpected cluster tokens in discovery output\n");
            return SVS_FALSE;
        }

        outputTokenIter++;
        if(outputTokenIter == discoveryScriptOutputTokensList.end())
        {
            DebugPrintf(SV_LOG_ERROR, "Unexpected end in cluster info\n");
            return SVS_FALSE;
        }
    }

    outputTokenIter++;
    if(outputTokenIter == discoveryScriptOutputTokensList.end())
    {
        DebugPrintf(SV_LOG_ERROR, "Unexpected end before DatabaseListStart\n");
        return SVS_FALSE;
    }

    outputTokenIterValue = *outputTokenIter;

    if (outputTokenIterValue.compare("DatabaseListStart") == 0)
    {
        outputTokenIter++;
        if(outputTokenIter == discoveryScriptOutputTokensList.end())
        {
            DebugPrintf(SV_LOG_ERROR, "Unexpected end before DatabaseListEnd\n");
            return SVS_FALSE;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Unexpected token instead of DatabaseListStart\n");
        return SVS_FALSE;
    }

    while(outputTokenIter != discoveryScriptOutputTokensList.end())
    {
        outputTokenIterValue = *outputTokenIter;

        if (outputTokenIterValue.compare("DatabaseListEnd") == 0)
        {
            break;
        }

        if (outputTokenIterValue.compare("DatabaseStart") == 0)
        {
            discoveryDataMap.clear();
            discoveryDataMap.insert(clusterDetailsMap.begin(), clusterDetailsMap.end());

            outputTokenIter++;
            if(outputTokenIter == discoveryScriptOutputTokensList.end())
            {
                DebugPrintf(SV_LOG_ERROR, "Unexpected end after DatabaseStart\n");
                return SVS_FALSE;
            }

            outputTokenIterValue = *outputTokenIter;
            keyValueTokens = tokenizeString((*outputTokenIter), "=");

            if((keyValueTokens.size() == 2) && (keyValueTokens.front() == "Unregister"))
            {
                unregisterMap.insert(std::make_pair(keyValueTokens.front(), keyValueTokens.back()));

                outputTokenIter++;
                if(outputTokenIter == discoveryScriptOutputTokensList.end())
                {
                    DebugPrintf(SV_LOG_ERROR, "Unexpected end in Unregister database\n");
                    return SVS_FALSE;
                }

                outputTokenIterValue = *outputTokenIter;
                if (outputTokenIterValue.compare("DatabaseEnd") != 0)
                {
                    DebugPrintf(SV_LOG_ERROR, "Unexpected tokens instead of DatabaseEnd\n");
                    return SVS_FALSE;
                }

                bRet = fillOracleUnregisterInfo(unregisterMap, OracleAppDiscInfo);
                if (bRet == SVS_FALSE)
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to fill oracle unregister info\n");
                    return SVS_FALSE;
                }
                unregisterMap.clear();

            }
            else
            {
                bool fillComplete = false;
                while(outputTokenIter != discoveryScriptOutputTokensList.end())
                {
                    outputTokenIterValue = *outputTokenIter;
                    if (outputTokenIterValue.compare("DatabaseEnd") == 0)
                    {
                        DebugPrintf(SV_LOG_ERROR, "Unexpected token DatabaseEnd\n");
                        return SVS_FALSE;
                    }

                    keyValueTokens = tokenizeString((*outputTokenIter), "=");

                    if(keyValueTokens.size() == 2)
                    {
                        if (keyValueTokens.front() != "DBName")
                        {
                            discoveryDataMap.insert(std::make_pair(keyValueTokens.front(), keyValueTokens.back()));
                        }
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "Unexpected number of token values in Database\n");
                        return SVS_FALSE;
                    }

                    if(keyValueTokens.front() == "DBName")
                    {
                        discoveryDataMap.insert(std::make_pair("DBName", keyValueTokens.back()));
                        viewLevels = keyValueTokens.back() + ",0;";

                        outputTokenIter++;
                        if(outputTokenIter == discoveryScriptOutputTokensList.end())
                        {
                            DebugPrintf(SV_LOG_ERROR, "Unexpected end after token DBName\n");
                            return SVS_FALSE;
                        }

                        while(outputTokenIter != discoveryScriptOutputTokensList.end())
                        {
                            outputTokenIterValue = *outputTokenIter;
                            if (outputTokenIterValue.compare("DatabaseEnd") == 0)
                            {
                                discoveryDataMap.insert(std::make_pair("ViewLevels", viewLevels));
                                discoveryDataMap.insert(std::make_pair("FilterDevices", filterDevices));
                                viewLevels.clear();
                                filterDevices.clear();

                                bRet = fillOracleAppVersionDiscInfo(discoveryDataMap, OracleAppDiscInfo);

                                if (bRet == SVS_FALSE)
                                {
                                    DebugPrintf(SV_LOG_ERROR, "Failed to fill oracle database discovery info\n");
                                    return SVS_FALSE;
                                }

                                discoveryDataMap.clear();
                                fillComplete = true;
                                break;
                            }

                            keyValueTokens = tokenizeString((*outputTokenIter), "=");
                            if(keyValueTokens.front() == "FilterDevice")
                            {
                                filterDevices = filterDevices+keyValueTokens.back() + ",";
                            }
                            else
                            {
                                std::string addViewLevel = keyValueTokens.back();
                                viewLevels += addViewLevel + ";";
                            }

                            outputTokenIter++;
                            if(outputTokenIter == discoveryScriptOutputTokensList.end())
                            {
                                DebugPrintf(SV_LOG_ERROR, "Unexpected tokens in view levels\n");
                                return SVS_FALSE;
                            }
                        }
                    }

                    if (fillComplete)
                        break;

                    outputTokenIter++;
                    if(outputTokenIter == discoveryScriptOutputTokensList.end())
                    {
                        DebugPrintf(SV_LOG_ERROR, "Unexpected end in DatabaseStart\n");
                        return SVS_FALSE;
                    }
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unexpected token instead of DatabaseStart\n");
            return SVS_FALSE;
        }

        outputTokenIter++;
        if(outputTokenIter == discoveryScriptOutputTokensList.end())
        {
            DebugPrintf(SV_LOG_ERROR, "Unexpected token in DatabaseStart\n");
            return SVS_FALSE;
        }
    }

    bRet = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

SVERROR OracleDiscoveryImpl::getOracleAppDevices(OracleAppDiscoveryInfo & OracleAppDiscInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleDiscoveryImpl::%s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK;
	std::string ocrOutput;

    AppLocalConfigurator localconfigurator;
    std::string installPath = localconfigurator.getInstallPath();
    std::string outputFile = installPath + "etc/appdevices.dat";
 
    SV_ULONG exitCode = 1 ;
    std::string strCommnadToExecute ;
    strCommnadToExecute += installPath;
    strCommnadToExecute += "scripts/oracleappagent.sh displaycrsdisks ";
    strCommnadToExecute += outputFile;
    strCommnadToExecute += std::string(" ");

    DebugPrintf(SV_LOG_INFO,"\n The discovery command to execute : %s\n",strCommnadToExecute.c_str());

    AppCommand objAppCommand(strCommnadToExecute, 0, outputFile);

    std::string output;

    if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to spawn oracle appagent script %s.\n", strCommnadToExecute.c_str());
        bRet = SVS_FALSE;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Successfully spawned the oracle appagent script.\n");
        DebugPrintf(SV_LOG_INFO,"output. %s\n", output.c_str());

        if (exitCode != 0)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed oracle appagent script %s.\n", strCommnadToExecute.c_str());
            bRet = SVS_FALSE;
        }
    }

    if ( bRet == SVS_FALSE )
    {
        return bRet;
    }
                  
    ocrOutput = getFileContents(outputFile);

    if(ocrOutput.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "No ocr devices found.\n");
        return SVS_FALSE;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Reading the ocr devices output file\n");
    }

    std::set<std::string> scriptOutputTokenSet;
    std::vector<std::string> tokenizedOutput;

    //  ------- Application devices output ---
    // AppDeviceListStart
    //   VoteDeviceListStart
    //    /dev/sds
    //    /dev/sdt
    //   VoteDeviceListEnd
    //   OcrDeviceListStart
    //    /dev/sdg
    //    /dev/sdh
    //   OcrDeviceListEnd
    // AppDeviceListEnd

    scriptOutputTokenSet.clear();

    Tokenize(ocrOutput, tokenizedOutput, "\n");

    if ((tokenizedOutput.front() != "AppDeviceListStart") ||
        (tokenizedOutput.back() != "AppDeviceListEnd"))
    {
        DebugPrintf(SV_LOG_ERROR, "Unrecognized Output File Format.\n");
        return SVS_FALSE;
    }

    std::vector<std::string>::iterator tokenListIter = tokenizedOutput.begin();

    std::string tokenListIterValue = *tokenListIter;
    if (tokenListIterValue.compare("AppDeviceListStart") != 0)
    {
        DebugPrintf(SV_LOG_ERROR, "Unexpected output for ocr devices.\n");
        return SVS_FALSE;
    }

    tokenListIter++;
    tokenListIterValue = *tokenListIter;
    if (tokenListIterValue.compare("AppDeviceListEnd") == 0)
    {
        DebugPrintf(SV_LOG_DEBUG, "No ocr devices discovered.\n");
        return SVS_OK;
    }

    bool unexpectedEof = true;
    OracleAppDevDiscInfo appDevInfo;

    while(tokenListIter != tokenizedOutput.end())
    {
        tokenListIterValue = *tokenListIter;
        if (tokenListIterValue.compare("VoteDeviceListStart") == 0)
        {
            appDevInfo.m_devNames.clear();
            unexpectedEof=true;
            tokenListIter++;

            while(tokenListIter != tokenizedOutput.end())
            {
                tokenListIterValue = *tokenListIter;

                if (tokenListIterValue.compare("VoteDeviceListEnd") == 0)
                {
                    OracleAppDiscInfo.m_dbOracleAppDevDiscInfo.insert(std::make_pair("VOTE",  appDevInfo));
                    tokenListIter++;
                    unexpectedEof=false;
                    break;
                }
                else
                {
                    appDevInfo.m_devNames.push_back(tokenListIterValue);
                    tokenListIter++;
                }
            }

            if (unexpectedEof)
            {
                DebugPrintf(SV_LOG_ERROR, "Unexpected end of file while looking for application devices.\n");
                return SVS_FALSE;
            }
        }
        else if (tokenListIterValue.compare("OcrDeviceListStart") == 0)
        {
            appDevInfo.m_devNames.clear();
            unexpectedEof=true;
            tokenListIter++;

            while(tokenListIter != tokenizedOutput.end())
            {
                tokenListIterValue = *tokenListIter;

                if (tokenListIterValue.compare("OcrDeviceListEnd") == 0)
                {
                    OracleAppDiscInfo.m_dbOracleAppDevDiscInfo.insert(std::make_pair("OCR",  appDevInfo));
                    tokenListIter++;
                    unexpectedEof=false;
                    break;
                }
                else
                {
                    appDevInfo.m_devNames.push_back(tokenListIterValue);
                    tokenListIter++;
                }
            }

            if (unexpectedEof)
            {
                DebugPrintf(SV_LOG_ERROR, "Unexpected end of file while looking for application devices.\n");
                return SVS_FALSE;
            }
        }
        else if (tokenListIterValue.compare("AppDeviceListEnd") == 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "Application devices discovered.\n");
            return SVS_OK;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unexpected output for application devices.\n");
            return SVS_FALSE;
        }
    }

    if(tokenListIter == tokenizedOutput.end())
    {
        DebugPrintf(SV_LOG_ERROR, "Unexpected end of file while looking for application devices.\n");
        return SVS_FALSE;
    }

    bRet = SVS_OK;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

bool OracleDiscoveryImpl::isInstalled(const std::string& oracleHost)
{
    if(m_isInstalled)
    {
        DebugPrintf(SV_LOG_DEBUG, "Oracle is installed on %s\n", oracleHost.c_str()) ;
        return true ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Oracle is not installed on %s\n", oracleHost.c_str()) ;
        return false ;
    }
}

SVERROR OracleDiscoveryImpl::discoverOracleApplication(OracleAppDiscoveryInfo& oracleVersionDiscInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    
    bRet = getOracleAppDevices(oracleVersionDiscInfo);
    bRet = getOracleDiscoveryData(oracleVersionDiscInfo);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void OracleDiscoveryImpl::Display(const OracleAppVersionDiscInfo& oracleAppInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Is Database Installed - %s\n", m_isInstalled);
    DebugPrintf(SV_LOG_DEBUG, "Is DiskGroup Mounted - %s\n", m_isMounted);
    DebugPrintf(SV_LOG_DEBUG, "Database Version - %s\n",oracleAppInfo.m_dbVersion.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Is Cluster - %s\n", oracleAppInfo.m_isCluster.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Cluster Name - %s\n", oracleAppInfo.m_clusterName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Node Name - %s\n", oracleAppInfo.m_nodeName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void OracleDiscoveryImpl::setHostName(const std::string& hostname)
{
    m_hostName = hostname ;
}


