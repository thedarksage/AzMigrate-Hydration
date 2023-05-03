#include "Handler.h"
#include "inmageex.h"
#include "inmstrcmp.h"
#include "scenariodetails.h"
#include "planobj.h"
#include "confschemamgr.h"
#include "inmsdkutil.h"
#include "InmXmlParser.h"
#include "protectionpairobject.h"
#include "host.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string/trim.hpp>
#include "confengine/agentconfig.h"
#include "../inmsafecapis/inmsafecapis.h"

void Handler::setHandlerInfo(HandlerInfo& hInfo)
{
    m_handler.access = hInfo.access;
    m_handler.funcSupported = hInfo.funcSupported;
    m_handler.funcReqId = hInfo.funcReqId;
    m_handler.functionName = hInfo.functionName;
    m_handler.funcHandler = hInfo.funcHandler;
    m_handler.authId = hInfo.authId;
    m_handler.funcIncludes = hInfo.funcIncludes;
}
INM_ERROR Handler::initConfig()
{
	LocalConfigurator lconfigurator ;
    INM_ERROR retStatus = E_UNKNOWN;// INM_FAIL;
    do
    {
        try 
        {
            if( m_vxConfigurator  != NULL )
            {
                m_vxConfigurator = NULL ;
                m_bconfig = false ;
            }
			std::string configcachepath = getConfigCachePath(lconfigurator.getRepositoryLocation(), 
				lconfigurator.getHostId()) ;

            m_vxConfigurator->instanceFlag = false ;
            if(!::InitializeConfigurator(&m_vxConfigurator, USE_ONLY_CACHE_SETTINGS, Xmlize, configcachepath))
            {
                break;
            }
            m_bconfig = true;
            retStatus = E_SUCCESS ;
        } 
        catch ( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
        }
        catch( std::exception const& e )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
        }
        catch(...) 
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
        }
    }while(0);

    if( retStatus == E_UNKNOWN)
    {
        DebugPrintf(SV_LOG_INFO, "Replication pair information is not available in local cache.\n" );
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
    return retStatus;
}

INM_ERROR Handler::process(FunctionInfo& request)
{
    m_ResponseData.m_RequestFunctionName = request.m_RequestFunctionName;
    m_ResponseData.m_RequestPgs = request.m_RequestPgs;
    m_ResponseData.m_FuntionId = request.m_FuntionId;
    m_ResponseData.m_ReqstInclude = request.m_ReqstInclude;
    m_ResponseData.m_RequestFunctionName = request.m_RequestFunctionName;

    return E_SUCCESS;
}
INM_ERROR Handler::validate(FunctionInfo& request)
{
    INM_ERROR errCode = E_SUCCESS;
    if(!hasSupport())
        errCode = E_FUNCTION_NOT_SUPPORTED;
    return errCode;
}
INM_ERROR Handler::updateErrorCode(INM_ERROR errCode)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_ResponseData.m_ReturnCode = getStrErrCode(errCode);
    if(m_errMessage.empty())
        m_ResponseData.m_Message = getErrorMessage(errCode);
    else
        m_ResponseData.m_Message = m_errMessage;
	DebugPrintf(SV_LOG_DEBUG, "EXITED  %s\n", FUNCTION_NAME) ;
    return errCode;
}
INM_ERROR Handler::processFromStub(const std::string& stubFile,FunctionInfo& request)
{
    XmlResponseValueObject m_stubResponse;
    INM_ERROR errCode = E_INTERNAL;
    if(!stubFile.empty())
    {
        try
        {
            m_stubResponse.InitializeXmlResponseObject(stubFile);
            std::list<FunctionInfo> listFunctionInfo = m_stubResponse.getFunctionList();
            std::list<FunctionInfo>::iterator iterFuncInfo = listFunctionInfo.begin();
            if( iterFuncInfo != listFunctionInfo.end())
            {
                //Considerng first iteration only
                //m_ResponseData = *iterFuncInfo;//iterFuncInfo->m_FunResponseObj;
                request = *iterFuncInfo;
                errCode = E_SUCCESS;
            }
        }
        catch(...)
        {
            m_errMessage.assign("Stub information not found.");
        }
    }
    return errCode;
}

bool GetHostDetailsToRepositoryConf(std::list<std::map<std::string, std::string> >& hostInfoList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool ok = false ;
    try
    {
        std::string smbConfFile ;
        ACE_Configuration_Heap m_inifile ;
		std::string repoLocation = ConfSchemaMgr::GetInstance()->getRepoLocation() ;
#ifdef SV_WINDOWS
        if( repoLocation.length() == 1 )
        {
            repoLocation += ":" ;
        }
#endif
        if( repoLocation[repoLocation.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR_A )
        {
            repoLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        }

        std::string repoConfigPath = repoLocation + "repository.ini" ;
        if( m_inifile.open() == 0 )
        {
            ACE_Ini_ImpExp importer( m_inifile );

            if( importer.import_config(ACE_TEXT_CHAR_TO_TCHAR(repoConfigPath.c_str())) == 0 )
            {
                ACE_Configuration_Section_Key sectionKey; 

                ACE_TString value;
                ACE_Configuration_Section_Key shareSectionKey;
                int sectionIndex = 0 ;
                ACE_TString tStrsectionName;
                while( m_inifile.enumerate_sections(m_inifile.root_section(), sectionIndex, tStrsectionName) == 0 )
                {
                    std::map<std::string, std::string> hostpropertyMap ;
                    m_inifile.open_section(m_inifile.root_section(), tStrsectionName.c_str(), 0, sectionKey) ;
                    std::string hostname = ACE_TEXT_ALWAYS_CHAR(tStrsectionName.c_str());

                    ACE_TString tStrKeyName, tStrValue;
                    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;
                    m_inifile.get_string_value(sectionKey, ACE_TEXT_CHAR_TO_TCHAR("hostid"), tStrValue) ; 
                    std::string hostId = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());
                    m_inifile.get_string_value(sectionKey, ACE_TEXT_CHAR_TO_TCHAR("ipaddress"), tStrValue) ; 
                    std::string ipaddress = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());
                    hostpropertyMap.insert(std::make_pair("hostid", hostId)) ;
                    hostpropertyMap.insert(std::make_pair("hostname", hostname)) ;
                    hostpropertyMap.insert(std::make_pair("ipaddress", ipaddress)) ;
                    hostInfoList.push_back(hostpropertyMap) ;
                    sectionIndex++; 
                }
            }
        }
    }
    catch(ContextualException& ex)
    {
        LocalConfigurator configurator ;
        std::map<std::string, std::string> hostpropertyMap ;
        hostpropertyMap.insert(std::make_pair("hostid", configurator.getHostId())) ;
        hostpropertyMap.insert(std::make_pair("hostname", Host::GetInstance().GetHostName())) ;
        hostpropertyMap.insert(std::make_pair("ipaddress", Host::GetInstance().GetIPAddress())) ;
        hostInfoList.push_back(hostpropertyMap) ;
        ok = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return ok ;
}

bool AddHostDetailsToRepositoryConf(const std::string& repoPath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool ok = false ;
	
	std::string smbConfFile ;
	ACE_Configuration_Heap m_inifile ;
	std::string repoLocation ;
	if( repoPath.empty() )
	{
		repoLocation = ConfSchemaMgr::GetInstance()->getRepoLocation() ;
	}
	else
	{
		repoLocation = repoPath ;
	}
#ifdef SV_WINDOWS
    if( repoLocation.length() == 1 )
    {
        repoLocation += ":" ;
    }
#endif
    if( repoLocation[repoLocation.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR_A )
    {
        repoLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    }
    std::string repoConfigPath = repoLocation + "repository.ini" ;
	DebugPrintf(SV_LOG_DEBUG, "Adding the host details to %s\n", repoConfigPath.c_str()); 
	std::string errmsg;
	if( m_inifile.open() == 0 )
	{
		ACE_Ini_ImpExp importer( m_inifile );

		importer.import_config(ACE_TEXT_CHAR_TO_TCHAR(repoConfigPath.c_str())) ;
        ACE_Configuration_Section_Key sectionKey; 
        ACE_TString value;
        ACE_Configuration_Section_Key shareSectionKey;
		LocalConfigurator lc ;
		std::string hostId = lc.getHostId() ;
        std::string ipAddress = Host::GetInstance().GetIPAddress() ;
        if( ipAddress.empty() )
        {
            ipAddress = "127.0.0.1" ;
        }
        std::string hostName = Host::GetInstance().GetHostName() ;
        m_inifile.open_section( m_inifile.root_section(), ACE_TEXT_CHAR_TO_TCHAR(hostName.c_str()), true, shareSectionKey) ;
        m_inifile.set_string_value( shareSectionKey, ACE_TEXT_CHAR_TO_TCHAR("hostid"), ACE_TEXT_CHAR_TO_TCHAR(hostId.c_str())  ) ;
        m_inifile.set_string_value( shareSectionKey, ACE_TEXT_CHAR_TO_TCHAR("ipaddress"), ACE_TEXT_CHAR_TO_TCHAR(ipAddress.c_str())  ) ;
		m_inifile.set_string_value( shareSectionKey, ACE_TEXT_CHAR_TO_TCHAR("repopath"), ACE_TEXT_CHAR_TO_TCHAR(repoLocation.c_str())  ) ;
		if( importer.export_config(ACE_TEXT_CHAR_TO_TCHAR(repoConfigPath.c_str())) ==  0 )
        {
            ok = true ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "failed to store the host details in the %s\n",repoConfigPath.c_str() ) ;
        }
		if ( SyncFile(repoConfigPath,errmsg) != true)
		{
			DebugPrintf (SV_LOG_ERROR, "Error : %s ",errmsg.c_str()); 
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "failed to open the m_inifile %d\n", ACE_OS::last_error()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return ok ;
}

bool IsRepositoryInIExists()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false;
	std::string repoLocation = ConfSchemaMgr::GetInstance()->getRepoLocation() ;
#ifdef SV_WINDOWS
    if( repoLocation.length() == 1 )
    {
        repoLocation += ":" ;
    }
#endif
    if( repoLocation[repoLocation.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR_A )
    {
        repoLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    }

    std::string repoConfigPath = repoLocation + "repository.ini" ;
	if ( boost::filesystem::exists(repoConfigPath) ) 
		bRet = true;
	 DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}
bool isHostNameChanged (std::string &currentName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool hostNameChange = true ;
	std::list<std::map<std::string, std::string> > hostInfoList;
	std::list<std::map<std::string, std::string> >::iterator hostInfoListIter;
	GetHostDetailsToRepositoryConf(hostInfoList);
	hostInfoListIter = hostInfoList.begin();
	while (hostInfoListIter != hostInfoList.end())
	{
		std::map<std::string, std::string> hostpropertyMap = *hostInfoListIter;
		std::map<std::string, std::string>::iterator hostpropertyMapIter = hostpropertyMap.find("hostid");
		if( hostpropertyMapIter != hostpropertyMap.end() && AgentConfig::GetInstance()->getHostId() == hostpropertyMapIter->second )
		{
			hostpropertyMapIter = hostpropertyMap.find("hostname") ;
			if (hostpropertyMapIter != hostpropertyMap.end() )
			{
				boost::trim(currentName);
				boost::trim (hostpropertyMapIter->second);
				if (currentName == hostpropertyMapIter->second)
				{
					hostNameChange = false;
					break;
				}
			}
		}
		hostInfoListIter ++;
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return hostNameChange;
}

bool isRepoPathChanged()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool repopathchange = true ;
	std::list<std::map<std::string, std::string> > hostInfoList;
	std::list<std::map<std::string, std::string> >::iterator hostInfoListIter;
	GetHostDetailsToRepositoryConf(hostInfoList);
	hostInfoListIter = hostInfoList.begin();
	std::string repoLocation = ConfSchemaMgr::GetInstance()->getRepoLocation() ;
#ifdef SV_WINDOWS
    if( repoLocation.length() == 1 )
    {
        repoLocation += ":" ;
    }
#endif
    if( repoLocation[repoLocation.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR_A )
    {
        repoLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    }

	while (hostInfoListIter != hostInfoList.end())
	{
		std::map<std::string, std::string> hostpropertyMap = *hostInfoListIter;
		std::map<std::string, std::string>::iterator hostpropertyMapIter = hostpropertyMap.find("hostid");
		if( hostpropertyMapIter != hostpropertyMap.end() && AgentConfig::GetInstance()->getHostId() == hostpropertyMapIter->second )
		{
			hostpropertyMapIter = hostpropertyMap.find("repopath") ;
			if (hostpropertyMapIter != hostpropertyMap.end() )
			{
				boost::trim(repoLocation);
				boost::trim (hostpropertyMapIter->second);
				if (repoLocation == hostpropertyMapIter->second)
				{
					repopathchange = false;
					break;
				}
			}
		}
		hostInfoListIter ++;
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return repopathchange;
}

void GetSparseFileVolPackMap(const LocalConfigurator& lc, std::map<std::string, std::string>& sparseFileDeviceMap)
{
	int counter = lc.getVirtualVolumesId();
    int novols = 1;
    while(counter != 0) 
    {
        char regData[26];
		inm_sprintf_s(regData ARRAYSIZE(regData), "VirVolume%d", counter);
        std::string data = lc.getVirtualVolumesPath(regData);
        std::string sparsefilename;
        std::string volume;
        ParseSparseFileNameAndVolPackName(data, sparsefilename, volume) ;
		if( !volume.empty() )
		{
			sparseFileDeviceMap.insert( std::make_pair( sparsefilename, volume )) ;
		}
		counter--;
    }
}



