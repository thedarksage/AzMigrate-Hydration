#include "fileserverapplication.h"
#include "service.h"
#include <sstream>
#include "ruleengine.h"
#include "util\win32\system_win.h"
FileServerAplication::FileServerAplication( )
{
	m_fileServerDiscoveryImplPtr.reset(new FileServerDiscoveryImpl());
}

SVERROR FileServerAplication::discoverApplication( )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    SVERROR bRet = SVS_OK ;
	if( discoverServices() != SVS_OK )
	{
		bRet = SVS_FALSE;		
	}
	m_fileServerDiscoveryImplPtr->discoverFileServer(m_fileShareInstanceDiscoveryMap, m_fileShareInstanceMetaDataMap);
    dumpSharedResourceInfo();
	m_registryVersion = "REGEDIT4";//TO DO.......
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bRet ;
}

SVERROR FileServerAplication::discoverServices() 
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	
	SVERROR bRet = SVS_OK ;
	InmService lanmanService("lanmanserver");
	if( QuerySvcConfig(lanmanService) == SVS_OK )
	{
		if( lanmanService.m_svcStatus != INM_SVCSTAT_RUNNING )
		{
			DebugPrintf( SV_LOG_ERROR, "lanman server is not running. Can not Perform discover of shared resouces int this system. \n" ) ;
		}
		m_services.push_back( lanmanService );
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "QuerySvcConfig failed\n") ;
		bRet = SVS_FALSE ;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
	return bRet;
}
SVERROR FileServerAplication::getNetworkNameList( std::list<std::string>& networkNameList )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	SVERROR retStatus = SVS_OK ;
	std::map<std::string, FileServerInstanceMetaData>::iterator fileShareInstanceMetaDataMapBeginIter;
	std::map<std::string, FileServerInstanceMetaData>::iterator fileShareInstanceMetaDataMapEndIter;
	fileShareInstanceMetaDataMapBeginIter = m_fileShareInstanceMetaDataMap.begin();
	fileShareInstanceMetaDataMapEndIter = m_fileShareInstanceMetaDataMap.end();
	while( fileShareInstanceMetaDataMapBeginIter != fileShareInstanceMetaDataMapEndIter )
	{
		networkNameList.push_back( fileShareInstanceMetaDataMapBeginIter->first.c_str() );
		fileShareInstanceMetaDataMapBeginIter++;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
	return retStatus;
}
SVERROR FileServerAplication::getActiveInstanceNameList( std::list<std::string>& instanceNameList )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	SVERROR retStatus = SVS_OK ;
	std::map<std::string, FileServerInstanceMetaData>::iterator fileShareInstanceMetaDataMapBeginIter;
	std::map<std::string, FileServerInstanceMetaData>::iterator fileShareInstanceMetaDataMapEndIter;
	fileShareInstanceMetaDataMapBeginIter = m_fileShareInstanceMetaDataMap.begin();
	fileShareInstanceMetaDataMapEndIter = m_fileShareInstanceMetaDataMap.end();
	while( fileShareInstanceMetaDataMapBeginIter != fileShareInstanceMetaDataMapEndIter )
	{
		if( fileShareInstanceMetaDataMapBeginIter->second.m_ErrorCode == DISCOVERY_SUCCESS )
		{
			instanceNameList.push_back( fileShareInstanceMetaDataMapBeginIter->first.c_str() );
		}
		fileShareInstanceMetaDataMapBeginIter++;
	}

	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
	return retStatus;
}
SVERROR FileServerAplication::getDiscoveredDataByInstance( const std::string& instanceName, FileServerInstanceDiscoveryData& instanceDiscoveredData )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	SVERROR retStatus = SVS_FALSE ;
	std::map<std::string, FileServerInstanceDiscoveryData>::iterator instanceDataIter = m_fileShareInstanceDiscoveryMap.find(instanceName);
	if( instanceDataIter != m_fileShareInstanceDiscoveryMap.end() )
	{
		instanceDiscoveredData = instanceDataIter->second;
		retStatus = SVS_OK;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
	return retStatus;
}
SVERROR FileServerAplication::getDiscoveredMetaDataByInstance( const std::string& instanceName, FileServerInstanceMetaData& instanceDiscoveredMetaData )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	SVERROR retStatus = SVS_OK ;
	std::map<std::string, FileServerInstanceMetaData>::iterator instanceMetaDataIter = m_fileShareInstanceMetaDataMap.find(instanceName);
	if( instanceMetaDataIter != m_fileShareInstanceMetaDataMap.end() )
	{
		instanceDiscoveredMetaData = instanceMetaDataIter->second;
		retStatus = SVS_OK;	
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
	return retStatus;
}
void FileServerAplication::dumpSharedResourceInfo()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	std::stringstream ipStream, volumeStream ;
	std::map<std::string, FileServerInstanceMetaData>::iterator fileShareInstanceMetaDataMapIter = m_fileShareInstanceMetaDataMap.begin();
	DebugPrintf( SV_LOG_DEBUG,"##################################################\n" );
	DebugPrintf( SV_LOG_DEBUG,"	  *******SHARED RESOURCE INFO************		 \n" );
	DebugPrintf( SV_LOG_DEBUG,"##################################################\n\n" );
	while( fileShareInstanceMetaDataMapIter != m_fileShareInstanceMetaDataMap.end() )
	{
		DebugPrintf( SV_LOG_DEBUG,"NetWork Name: %s \n", fileShareInstanceMetaDataMapIter->first.c_str() );
		DebugPrintf( SV_LOG_DEBUG,"************\n" );
		ipStream.str("");
		std::map<std::string, FileServerInstanceDiscoveryData>::iterator instanceIter = m_fileShareInstanceDiscoveryMap.find(fileShareInstanceMetaDataMapIter->first);
		if(instanceIter != m_fileShareInstanceDiscoveryMap.end())
		{
			std::list<std::string>::iterator ipIter = instanceIter->second.m_ips.begin();
			while(ipIter != instanceIter->second.m_ips.end())
			{
				ipStream << *ipIter << " ";
				ipIter++;
			}
		}
		DebugPrintf(SV_LOG_DEBUG, "Ip Address(es): %s \n", ipStream.str().c_str());
		DebugPrintf( SV_LOG_DEBUG,"***************\n" );
		std::list<std::string>::iterator volumeListIter = fileShareInstanceMetaDataMapIter->second.m_volumes.begin() ;
		volumeStream.str("");
		while(volumeListIter != fileShareInstanceMetaDataMapIter->second.m_volumes.end())
		{
			volumeStream << *volumeListIter << " " ;
			volumeListIter++;
		}
		DebugPrintf(SV_LOG_DEBUG, "Discovered Share Volume(s): %s \n", volumeStream.str().c_str());
		DebugPrintf( SV_LOG_DEBUG,"******************\n\n" );
		std::map<std::string, FileShareInfo>::iterator instanceMapIter = fileShareInstanceMetaDataMapIter->second.m_shareInfo.begin();
		while(instanceMapIter != fileShareInstanceMetaDataMapIter->second.m_shareInfo.end())
		{
			DebugPrintf(SV_LOG_DEBUG, "\tShareName: %s\n", instanceMapIter->second.m_shareName.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\tAbsolute Path: %s\n", instanceMapIter->second.m_absolutePath.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\tVolume Path Name: %s\n", instanceMapIter->second.m_mountPoint.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\tUNC Path: %s\n", instanceMapIter->second.m_UNCPath.c_str());
			DebugPrintf(SV_LOG_DEBUG, "\tShare Ascii: %s\n", instanceMapIter->second.m_sharesAscii.c_str() );
			DebugPrintf(SV_LOG_DEBUG, "\tSecurity Ascii: %s\n", instanceMapIter->second.m_securityAscii.c_str() );
			if( isClusterNode() )
			{
				OSVERSIONINFOEX osVersionInfo ;
				getOSVersionInfo(osVersionInfo) ;
				if(osVersionInfo.dwMajorVersion == 6) //making this condition true for windows 2008 sp1,sp2 and R2
				{
					DebugPrintf(SV_LOG_DEBUG, "\tMax Users: %lu\n", instanceMapIter->second.m_shi503_max_uses);
					//DebugPrintf(SV_LOG_DEBUG, "\tSecurity Password: %s\n", instanceMapIter->second.m_shi503_passwd.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\tFileShare Remarks: %s\n", instanceMapIter->second.m_shi503_remark.c_str());
					DebugPrintf(SV_LOG_DEBUG, "\tFileShare Type: %lu\n", instanceMapIter->second.m_shi503_type);
				}
			}
			instanceMapIter++;
		}
		DebugPrintf( SV_LOG_DEBUG,"\n");
		fileShareInstanceMetaDataMapIter++;
	}
	DebugPrintf( SV_LOG_DEBUG,"##################################################\n" );
	
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
}

std::string FileServerAplication::getSummary( std::list<ValidatorPtr> list)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    std::stringstream summaryStream ;
    std::list<ValidatorPtr>::iterator ruleiter = list.begin() ;
    summaryStream << "\n\tFile Server Summary."<<std::endl;
    summaryStream << "\n\t\tFile Server Instance Pre-Requisites Validation Results" << std::endl <<std::endl ;
    while (ruleiter!= list.end() )
    {
        RuleEngine::getInstance()->RunRule((*ruleiter)) ;
		std::string RuleId = Validator::getInmRuleIdStringFromID((*ruleiter)->getRuleId() );
        summaryStream << "\t\t\tRule Name\t\t:" << RuleId << "\n";
        summaryStream << "\t\t\tResult\t\t\t:" << (*ruleiter)->statusToString() << std::endl ;
	    ruleiter++ ;
    }
   	summaryStream << std::endl ;
    
    std::map<std::string, FileServerInstanceMetaData>::iterator fileShareInstanceMetaDataMapIter = m_fileShareInstanceMetaDataMap.begin() ;
    std::map<std::string, FileServerInstanceDiscoveryData>::iterator discoveryInter ;

    if( fileShareInstanceMetaDataMapIter != m_fileShareInstanceMetaDataMap.end() )
    {
        summaryStream<<"\n\t\tDiscoverd Shared Resources Information \n\n";
    }
    while( fileShareInstanceMetaDataMapIter != m_fileShareInstanceMetaDataMap.end() )
    {
        FileServerInstanceMetaData& fileshareInstanceMetaData = fileShareInstanceMetaDataMapIter->second ;
        discoveryInter = m_fileShareInstanceDiscoveryMap.find(fileShareInstanceMetaDataMapIter->first) ;
        std::map<std::string, FileShareInfo>::iterator fileSharesMapIter = fileshareInstanceMetaData.m_shareInfo.begin() ;
        summaryStream<< "\t\tHost Name :" << discoveryInter->first << "\n" ;
        std::list<std::string> ips = discoveryInter->second.m_ips ;
        std::list<std::string>::iterator ipIter =  ips.begin() ;
        summaryStream<< "\t\tIP Address(es) : \n" ;
        while( ipIter != ips.end() )
        {
            summaryStream<< "\t\t\t" << ipIter->c_str() << "\n" ;
            ipIter++ ;
        }
        summaryStream<< "\n" ;
        while( fileSharesMapIter != fileshareInstanceMetaData.m_shareInfo.end() )
        {
            std::string shareName = fileSharesMapIter->first ;
            FileShareInfo& shareInfo = fileSharesMapIter->second ;
            std::list<std::pair<std::string, std::string> >shareProperties = shareInfo.getProperties() ;
            std::list<std::pair<std::string, std::string> >::iterator sharePropsIter = shareProperties.begin() ;
            summaryStream<< "\t\t\tShare Name : "<< shareName << "\n\n" ;
            while( sharePropsIter != shareProperties.end() )
            {
                summaryStream<< "\t\t\t\t " << sharePropsIter->first << " : " << sharePropsIter->second <<std::endl ;
                sharePropsIter++ ;
            }

            summaryStream<< "\n\n" ;           
            fileSharesMapIter++ ;
        }
    	summaryStream << std::endl << std::endl ;
        fileShareInstanceMetaDataMapIter++ ;
    }
    

    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return summaryStream.str();
}

bool FileServerAplication::isInstalled()
{
    return true ;
}

void FileServerAplication::clean()
{
   DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
   m_services.clear(); 
   m_fileShareInstanceDiscoveryMap.clear();
   m_fileShareInstanceMetaDataMap.clear();
   DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
}