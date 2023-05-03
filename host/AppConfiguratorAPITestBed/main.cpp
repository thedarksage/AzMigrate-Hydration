#include <ace/Init_ACE.h>

#include "logger.h"
#include "config/applicationsettings.h"
#include "config/appconfigurator.h"
#include "serializationtype.h"
#include <boost/lexical_cast.hpp>
#include <iomanip>
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"
using namespace std;

void usage( char *exename )
{
    stringstream out;
    out << "usage: " << exename << " --default --fromcache\n"
        << "       " << exename << " --custom --ip <cx-ip> --port <cx-port> --hostid <hostid>\n"
        << "\n where --fromcache is optional, this option should given if we want to fetch data from cache\n";
    cout << out.str().c_str();
}

SVERROR dumpPolicies( const std::list<ApplicationPolicy>& appPolicies )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_OK;
	if( appPolicies.size() != 0 )
	{
		std::cout << std::endl << "POLICY INFORAMATION " <<std::endl ;
		std::cout << "--------------------" << std::endl ;
		std::list<ApplicationPolicy>::const_iterator appPolicyBegIter = appPolicies.begin();
		std::list<ApplicationPolicy>::const_iterator appPolicyEndIter = appPolicies.end();
		while( appPolicyBegIter != appPolicyEndIter )
		{
			std::map<std::string, std::string>::const_iterator policyPropsMapBegIter = appPolicyBegIter->m_policyProperties.begin();
			std::map<std::string, std::string>::const_iterator policyPropsMapEndIter = appPolicyBegIter->m_policyProperties.end();			
			std::map<std::string, std::string>::const_iterator policyPropsMapTempIter;
			policyPropsMapTempIter = appPolicyBegIter->m_policyProperties.find("PolicyType");
			std::string policyType;
			if( policyPropsMapTempIter != policyPropsMapEndIter )
			{
				policyType = policyPropsMapTempIter->second ;
			}			
			std::cout << std::endl << std::setw(32) << " Application Policy Type : " << policyType << std::endl ;
			std::cout << std::setw(42) << "-------------------------------------" << std::endl << std::endl ;
			while( policyPropsMapBegIter != policyPropsMapEndIter )
			{
				std::cout << std::setw(30) << policyPropsMapBegIter->first << " : " << policyPropsMapBegIter->second << std::endl;
				policyPropsMapBegIter++;
			}
			policyPropsMapTempIter = appPolicyBegIter->m_policyProperties.find("ApplicationType");
			std::string appType ;
			if( policyPropsMapTempIter != policyPropsMapEndIter )
			{
				appType = policyPropsMapTempIter->second ;
			}
			std::string marshalledSrcReadinessCheckString = appPolicyBegIter->m_policyData;
			if( strcmpi( policyType.c_str(), "TargetReadinessCheck") == 0 )
			{
				std::cout << std::endl << std::setw(38) << "Source Info:" << std::endl;
				std::cout << std::setw(38) << "------------" << std::endl;
				if( strstr( appType.c_str(), "EXCHANGE" ) != 0 )
				{
					ExchangeTargetReadinessCheckInfo srcInfo;
					srcInfo = unmarshal<ExchangeTargetReadinessCheckInfo>(marshalledSrcReadinessCheckString) ;
					std::map<std::string, std::string>::const_iterator srcPropsMapBegIter = srcInfo.m_properties.begin();
					std::map<std::string, std::string>::const_iterator srcPropsMapEndIter = srcInfo.m_properties.end();
					while( srcPropsMapBegIter != srcPropsMapEndIter )
					{
						std::cout << srcPropsMapBegIter->first << " : " << srcPropsMapBegIter->second << std::endl;	
						srcPropsMapBegIter++;
					}
					std::list<std::map<std::string, std::string>>::const_iterator volCapacityBegIter = srcInfo.m_volCapacities.begin();
					std::cout << std::endl << "Source and Target Volume Capacity Info:" << std::endl;
					while( volCapacityBegIter != srcInfo.m_volCapacities.end() )
					{
						SV_ULONGLONG srcCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacityBegIter->find("srcVolCapacity")->second);
						SV_ULONGLONG tgtCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacityBegIter->find("tgtVolCapacity")->second);
						std::cout << "Source Volume & Capacity: " << std::setw(30) << volCapacityBegIter->find("srcVolName")->second << " : " << srcCapacity << std::endl;
						std::cout << "Target Volume & Capacity: " << std::setw(30) << volCapacityBegIter->find("tgtVolName")->second << " : " << tgtCapacity << std::endl;
						volCapacityBegIter++;
					}
				}
				else if( strstr( appType.c_str(), "SQL" ) != 0 )
				{
					MSSQLTargetReadinessCheckInfo srcSqlTgtReadinessChekInfo;
					srcSqlTgtReadinessChekInfo = unmarshal<MSSQLTargetReadinessCheckInfo>(marshalledSrcReadinessCheckString ) ;
					std::map<std::string, std::map<std::string, std::string>>::const_iterator sqlInstanceTgtReadinessCheckInfoBegIter = srcSqlTgtReadinessChekInfo.m_sqlInstanceTgtReadinessCheckInfo.begin();
					std::map<std::string, std::map<std::string, std::string>>::const_iterator sqlInstanceTgtReadinessCheckInfoEndIter = srcSqlTgtReadinessChekInfo.m_sqlInstanceTgtReadinessCheckInfo.end();
					
					while( sqlInstanceTgtReadinessCheckInfoBegIter != sqlInstanceTgtReadinessCheckInfoEndIter )
					{
						std::cout << std::endl << std::setw(35) << "INSTANCE NAME: " << sqlInstanceTgtReadinessCheckInfoBegIter->first << std::endl ;
						std::cout << std::setw(45) << "--------------------------" << std::endl << std::endl;
						std::map<std::string, std::string> sqlInstanceTgtReadinessCheckInfo	= sqlInstanceTgtReadinessCheckInfoBegIter->second;
						std::map<std::string, std::string>::const_iterator srcPropsMapBegIter = sqlInstanceTgtReadinessCheckInfo.begin();
						while( srcPropsMapBegIter != sqlInstanceTgtReadinessCheckInfo.end() )
						{
							std::cout << srcPropsMapBegIter->first << " : " << srcPropsMapBegIter->second << std::endl;	
							srcPropsMapBegIter++;
						}
						sqlInstanceTgtReadinessCheckInfoBegIter++;
					}
					std::list<std::map<std::string, std::string>>::const_iterator volCapacityBegIter = srcSqlTgtReadinessChekInfo.m_volCapacities.begin();
					std::cout << std::endl << "Source and Target Volume Capacity Info:" << std::endl;
					while( volCapacityBegIter != srcSqlTgtReadinessChekInfo.m_volCapacities.end() )
					{
						SV_ULONGLONG srcCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacityBegIter->find("srcVolCapacity")->second);
						SV_ULONGLONG tgtCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacityBegIter->find("tgtVolCapacity")->second);
						std::cout << "Source Volume & Capacity: " << std::setw(30) << volCapacityBegIter->find("srcVolName")->second << " : " << srcCapacity << std::endl;
						std::cout << "Target Volume & Capacity: " << std::setw(30) << volCapacityBegIter->find("tgtVolName")->second << " : " << tgtCapacity << std::endl;
						volCapacityBegIter++;
					}
				}
				else if( strcmpi( appType.c_str(), "FILESERVER" ) != 0 )
				{
					FileServerTargetReadinessCheckInfo srcFSTgtReadinessChekInfo;
					srcFSTgtReadinessChekInfo = unmarshal<FileServerTargetReadinessCheckInfo>(marshalledSrcReadinessCheckString ) ;

					std::list<std::map<std::string, std::string>>::const_iterator volCapacityBegIter = srcFSTgtReadinessChekInfo.m_volCapacities.begin();
					std::cout << std::endl << "Source and Target Volume Capacity Info:" << std::endl;
					while( volCapacityBegIter != srcFSTgtReadinessChekInfo.m_volCapacities.end() )
					{
						SV_ULONGLONG srcCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacityBegIter->find("srcVolCapacity")->second);
						SV_ULONGLONG tgtCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacityBegIter->find("tgtVolCapacity")->second);
						std::cout << "Source Volume & Capacity: " << std::setw(30) << volCapacityBegIter->find("srcVolName")->second << " : " << srcCapacity << std::endl;
						std::cout << "Target Volume & Capacity: " << std::setw(30) << volCapacityBegIter->find("tgtVolName")->second << " : " << tgtCapacity << std::endl;
						volCapacityBegIter++;
					}
				}
			}
			appPolicyBegIter++;
		}
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

SVERROR dumpProtectionSettings( const std::map<std::string,AppProtectionSettings>& appProtectionsettings )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
    SVERROR retStatus = SVS_OK;
    if( appProtectionsettings.size() != 0 )
    {
        std::cout << std::endl << std::endl << "PROTECTION SETTINGS INFORAMATION" <<std::endl ;
        std::cout << "-----------------------------------" << std::endl << std::endl;
        std::map<std::string, AppProtectionSettings>::const_iterator appProtectionSettingsInnerMapBegIter;
        std::map<std::string, AppProtectionSettings>::const_iterator appProtectionSettingsInnerMapEndIter;
        appProtectionSettingsInnerMapBegIter = appProtectionsettings.begin();
        appProtectionSettingsInnerMapEndIter = appProtectionsettings.end();
        while( appProtectionSettingsInnerMapBegIter != appProtectionSettingsInnerMapEndIter )
        {
            std::cout << std::setw(25) << "Protection Settings Id: " << appProtectionSettingsInnerMapBegIter->first << std::endl ;
            std::cout << std::setw(56) << "-----------------------------------------------------" << std::endl << std::endl ;
            std::cout << std::setw(25) << "Instance Name : " << appProtectionSettingsInnerMapBegIter->first << std::endl ;
            std::cout << std::setw(41) << "---------------------------------" << std::endl << std::endl; 
            std::cout << std::setw(34) << "Application Protection Version : " << appProtectionSettingsInnerMapBegIter->second.m_version << std::endl ;
            std::cout << std::setw(34) << "Application Volumes : " ;
            std::list<std::string>::const_iterator volumeListBegIter = appProtectionSettingsInnerMapBegIter->second.appVolumes.begin();
            std::list<std::string>::const_iterator volumeListEndIter = appProtectionSettingsInnerMapBegIter->second.appVolumes.end();
            while( volumeListBegIter != volumeListEndIter )
            {
                std::cout << *volumeListBegIter ;
                volumeListBegIter++;
                if( volumeListBegIter != volumeListEndIter )
                {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
            std::cout << std::setw(34) << "Other Volumes : " ;
            volumeListBegIter = appProtectionSettingsInnerMapBegIter->second.CustomVolume.begin();
            volumeListEndIter = appProtectionSettingsInnerMapBegIter->second.CustomVolume.end();
            while( volumeListBegIter != volumeListEndIter )
            {
                std::cout << *volumeListBegIter ;
                volumeListBegIter++;
                if(volumeListBegIter != volumeListEndIter)
                {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
            std::map<std::string, std::string>::const_iterator propsBegIter = appProtectionSettingsInnerMapBegIter->second.m_properties.begin();
            std::map<std::string, std::string>::const_iterator propsEndIter = appProtectionSettingsInnerMapBegIter->second.m_properties.end();
            while( propsBegIter != propsEndIter )
            {
                std::cout << std::setw(31) << propsBegIter->first << " : " << propsBegIter->second << std::endl ;
                propsBegIter++;
            }
            std::map<std::string, std::string>::const_iterator propsTempIter = appProtectionSettingsInnerMapBegIter->second.m_properties.find("ApplicationType");

            std::map<std::string, ReplicationPair>::const_iterator replicationPairBegIter = appProtectionSettingsInnerMapBegIter->second.Pairs.begin();
            std::map<std::string, ReplicationPair>::const_iterator replicationPairEndIter = appProtectionSettingsInnerMapBegIter->second.Pairs.end();
            std::cout << std::endl ;
            std::cout << std::endl << std::setw(38) << "Replication Pair Info" << std::endl ;
            std::cout << std::setw(39) << "-----------------------" ;
            while( replicationPairBegIter != replicationPairEndIter )
            {
                std::cout << std::endl << std::endl << std::setw(31) << "Version" << " : " << replicationPairBegIter->second.m_version << std::endl ;
                std::cout << std::setw(31) << "Mount Point " << " : "<< replicationPairBegIter->first << std::endl ;
                std::map<std::string, std::string>::const_iterator propsMapBegIer = replicationPairBegIter->second.m_properties.begin();
                std::map<std::string, std::string>::const_iterator propsMapEndIer = replicationPairBegIter->second.m_properties.end();
                while( propsMapBegIer != propsMapEndIer )
                {
                    std::cout << std::setw(31) << propsMapBegIer->first << " : " << propsMapBegIer->second << std::endl ;
                    propsMapBegIer++;
                }
                replicationPairBegIter ++;
            }

            std::string appType = "";
            if( propsTempIter != propsEndIter )
            {
                appType = propsTempIter->second;
            }
            std::cout << std::endl ;
            std::string marshalledString = appProtectionSettingsInnerMapBegIter->second.srcDiscoveryInformation;
            if( marshalledString.empty() == false )
            {
                if( strstr( appType.c_str(), "EXCHANGE" ) != 0 )
                {
                    std::cout << std::setw(30) << "Source Exchnage Info " << std::endl;
                    std::cout << std::setw(30) << "--------------------" << std::endl << std::endl;
                    SrcExchangeDiscInfo srcExchDiscInfo = unmarshal<SrcExchangeDiscInfo>(marshalledString);
                    std::cout << std::setw(30) << "Verion : " <<  srcExchDiscInfo.m_version << std::endl;
                    std::map<std::string, std::string>::const_iterator propsBegIter = srcExchDiscInfo.m_properties.begin();
                    std::map<std::string, std::string>::const_iterator propsEndIter = srcExchDiscInfo.m_properties.end();
                    while( propsBegIter != propsEndIter )
                    {
                        std::cout << std::setw(31) << propsBegIter->first << " : " << propsBegIter->second << std::endl ;
                        propsBegIter++;
                    }

                    std::map<std::string, SrcExchStorageGrpInfo>::const_iterator storageGrpMapBegIter = srcExchDiscInfo.m_storageGrpMap.begin();
                    std::map<std::string, SrcExchStorageGrpInfo>::const_iterator storageGrpMapEndIter = srcExchDiscInfo.m_storageGrpMap.end();
                    while( storageGrpMapBegIter != storageGrpMapEndIter )
                    {

                        std::map<std::string, std::string>::const_iterator propsBegIter = storageGrpMapBegIter->second.m_properties.begin();
                        std::map<std::string, std::string>::const_iterator propsEndIter = storageGrpMapBegIter->second.m_properties.end();
                        while( propsBegIter != propsEndIter )
                        {
                            std::cout << std::setw(31) << propsBegIter->first << " : " << propsBegIter->second << std::endl ;
                            propsBegIter++;
                        }
                        std::map<std::string, SrcExchMailStoreInfo>::const_iterator mailStoresBegIter = storageGrpMapBegIter->second.m_mailStores.begin();
                        std::map<std::string, SrcExchMailStoreInfo>::const_iterator mailStoresEndIter = storageGrpMapBegIter->second.m_mailStores.end();
                        while( mailStoresBegIter != mailStoresEndIter )
                        {
                            std::cout << mailStoresBegIter->first << std::endl ;
                            std::map<std::string, std::string>::const_iterator propsBegIter = mailStoresBegIter->second.m_properties.begin();
                            std::map<std::string, std::string>::const_iterator propsEndIter = mailStoresBegIter->second.m_properties.end();
                            while( propsBegIter != propsEndIter )
                            {
                                std::cout << std::setw(31) << propsBegIter->first << " : " << propsBegIter->second << std::endl ;
                                propsBegIter++;
                            }
                            std::map<std::string, std::string>::const_iterator edbFilesBegIter = mailStoresBegIter->second.mailStoreFiles.begin() ;
                            std::map<std::string, std::string>::const_iterator edbFilesEndIter = mailStoresBegIter->second.mailStoreFiles.end() ;
                            while( edbFilesBegIter != edbFilesEndIter )
                            {
                                std::cout << edbFilesBegIter->second << " " ;
                                edbFilesBegIter++;
                            }
                            mailStoresBegIter++;													
                        }
                        storageGrpMapBegIter++;
                    }
                }
                else if( strstr( appType.c_str(), "SQL" ) != 0 )
                {
                    std::cout << std::setw(36) << "Source SQL Info " <<  std::endl;
                    std::cout << std::setw(36) << "-----------------" << std::endl;
                    SourceSQLDiscoveryInformation srcSQLDiscInfo = unmarshal<SourceSQLDiscoveryInformation>(marshalledString);
                    std::map<std::string, SourceSQLInstanceInfo>::const_iterator srcsqlInstanceInfoBegIter = srcSQLDiscInfo.m_srcsqlInstanceInfo.begin();
                    std::map<std::string, SourceSQLInstanceInfo>::const_iterator srcsqlInstanceInfoEndIter = srcSQLDiscInfo.m_srcsqlInstanceInfo.end();
                    while( srcsqlInstanceInfoBegIter != srcsqlInstanceInfoEndIter )
                    {
                        std::cout << std::endl << std::setw(34) << "Instance Name : "<< srcsqlInstanceInfoBegIter->first << std::endl ;
                        std::map<std::string, std::string>::const_iterator propsBegIter = srcsqlInstanceInfoBegIter->second.properties.begin();
                        std::map<std::string, std::string>::const_iterator propsEndIter = srcsqlInstanceInfoBegIter->second.properties.end();
                        while( propsBegIter != propsEndIter )
                        {
                            std::cout << std::setw(31) << propsBegIter->first << " : " << propsBegIter->second << std::endl ;
                            propsBegIter++;
                        }
                        std::map<std::string, SourceSQLDatabaseInfo>::const_iterator databasesBegIter = srcsqlInstanceInfoBegIter->second.m_databases.begin();
                        std::map<std::string, SourceSQLDatabaseInfo>::const_iterator databasesEndIter = srcsqlInstanceInfoBegIter->second.m_databases.end();
                        while( databasesBegIter != databasesEndIter )
                        {
                            std::cout << std::endl << std::setw(33) << "DataBase Name : " << databasesBegIter->first << std::endl ;
                            std::cout << std::setw(39) << "-----------------------" << std::endl ;
                            std::list<std::string>::const_iterator filesBegIter = databasesBegIter->second.m_files.begin();
                            std::list<std::string>::const_iterator filesEndIter = databasesBegIter->second.m_files.end();
                            while( filesBegIter != filesEndIter )
                            {
                                std::cout << "File : " << *filesBegIter << std::endl ;
                                filesBegIter++;
                            }
                            databasesBegIter++;
                        }
                        std::list<std::string>::const_iterator volBegIter = srcsqlInstanceInfoBegIter->second.m_volumes.begin();
                        std::list<std::string>::const_iterator volEndIter = srcsqlInstanceInfoBegIter->second.m_volumes.end();
                        std::cout << std::setw(31) << "Volumes : " ; 
                        while( volBegIter != volEndIter )
                        {
                            std::cout << *volBegIter ;
                            volBegIter++;
                            if( volBegIter != volEndIter )
                            {
                                std::cout << ", ";
                            }
                        }
                        std::cout << std::endl << std::endl;
                        srcsqlInstanceInfoBegIter++;
                    }
                }
                else if( strcmpi( appType.c_str(), "FILESERVER" ) != 0 )
                {
                    std::cout << std::setw(30) << "Source FileServer Info " << std::endl;
                    std::cout << std::setw(30) << "------------------------" << std::endl << std::endl;;
                    SrcFileServerDiscoveryInfomation srcFSDiscInfo = unmarshal<SrcFileServerDiscoveryInfomation>(marshalledString);
                    std::cout << std::setw(30) << "Verion : " <<  srcFSDiscInfo.m_version << std::endl;
                    std::map<std::string, std::string>::const_iterator propsBegIter = srcFSDiscInfo.m_Properties.begin();
                    std::map<std::string, std::string>::const_iterator propsEndIter = srcFSDiscInfo.m_Properties.end();
                    while( propsBegIter != propsEndIter )
                    {
                        std::cout << std::setw(31) << propsBegIter->first << " : " << propsBegIter->second << std::endl ;
                        propsBegIter++;
                    }
                    std::map<std::string,std::string>::const_iterator shareInfoMapBegIter = srcFSDiscInfo.m_ShareInfoMap.begin();
                    std::map<std::string,std::string>::const_iterator shareInfoMapEndIter = srcFSDiscInfo.m_ShareInfoMap.end();
                    while( shareInfoMapBegIter != shareInfoMapEndIter )
                    {
                        std::cout << std::setw(31) << shareInfoMapBegIter->first << " : " << shareInfoMapBegIter->second << std::endl;
                        shareInfoMapBegIter++;
                    }
                    std::map<std::string,std::string>::const_iterator securityInfoMapBegIter = srcFSDiscInfo.m_SecurityInfoMap.begin();
                    std::map<std::string,std::string>::const_iterator securityInfoMapEndIter = srcFSDiscInfo.m_SecurityInfoMap.end();
                    while( securityInfoMapBegIter != securityInfoMapEndIter )
                    {
                        std::cout << std::setw(31) << securityInfoMapBegIter->first << " : " << securityInfoMapBegIter->second << std::endl;
                        securityInfoMapBegIter++;
                    }
                    std::list<std::string>::const_iterator volumeListBegIter = srcFSDiscInfo.m_VolumeList.begin();
                    std::list<std::string>::const_iterator volumeListEndIter = srcFSDiscInfo.m_VolumeList.end();
                    while( volumeListBegIter != volumeListEndIter )
                    {
                        std::cout << *volumeListBegIter << " " ;
                        volumeListBegIter++;
                    }
					std::map<std::string,std::string>::const_iterator shareTypeMapBegIter = srcFSDiscInfo.m_ShareTypeMap.begin();
                    std::map<std::string,std::string>::const_iterator shareTypeMapEndIter = srcFSDiscInfo.m_ShareTypeMap.end();
                    while( shareTypeMapBegIter != shareTypeMapEndIter )
                    {
                        std::cout << std::setw(31) << shareTypeMapBegIter->first << " : " << shareTypeMapBegIter->second << std::endl;
                        shareTypeMapBegIter++;
                    }
					std::map<std::string,std::string>::const_iterator shareUsersMapBegIter = srcFSDiscInfo.m_ShareUsersMap.begin();
                    std::map<std::string,std::string>::const_iterator shareUsersMapEndIter = srcFSDiscInfo.m_ShareUsersMap.end();
                    while( shareUsersMapBegIter != shareUsersMapEndIter )
                    {
                        std::cout << std::setw(31) << shareUsersMapBegIter->first << " : " << shareUsersMapBegIter->second << std::endl;
                        shareUsersMapBegIter++;
                    }
					std::map<std::string,std::string>::const_iterator absolutePathMapBegIter = srcFSDiscInfo.m_AbsolutePathMap.begin();
                    std::map<std::string,std::string>::const_iterator absolutePathMapEndIter = srcFSDiscInfo.m_AbsolutePathMap.end();
                    while( absolutePathMapBegIter != absolutePathMapEndIter )
                    {
                        std::cout << std::setw(31) << absolutePathMapBegIter->first << " : " << absolutePathMapBegIter->second << std::endl;
                        absolutePathMapBegIter++;
                    }
					std::map<std::string,std::string>::const_iterator sharePwdMapBegIter = srcFSDiscInfo.m_SharePwdMap.begin();
                    std::map<std::string,std::string>::const_iterator sharePwdMapEndIter = srcFSDiscInfo.m_SharePwdMap.end();
                    while( sharePwdMapBegIter != sharePwdMapEndIter )
                    {
                        std::cout << std::setw(31) << sharePwdMapBegIter->first << " : " << sharePwdMapBegIter->second << std::endl;
                        sharePwdMapBegIter++;
                    }
					std::map<std::string,std::string>::const_iterator shareRemarkMapBegIter = srcFSDiscInfo.m_ShareRemarkMap.begin();
                    std::map<std::string,std::string>::const_iterator shareRemarkMapEndIter = srcFSDiscInfo.m_ShareRemarkMap.end();
                    while( shareRemarkMapBegIter != shareRemarkMapEndIter )
                    {
                        std::cout << std::setw(31) << shareRemarkMapBegIter->first << " : " << shareRemarkMapBegIter->second << std::endl;
                        shareRemarkMapBegIter++;
                    }
					std::map<std::string,std::string>::const_iterator mountPointMapBegIter = srcFSDiscInfo.m_mountPointMap.begin();
                    std::map<std::string,std::string>::const_iterator mountPointMapEndIter = srcFSDiscInfo.m_mountPointMap.end();
                    while( mountPointMapBegIter != mountPointMapEndIter )
                    {
                        std::cout << std::setw(31) << mountPointMapBegIter->first << " : " << mountPointMapBegIter->second << std::endl;
                        mountPointMapBegIter++;
                    }
                }
            }
            else
            {
                std::cout << std::setw(30) << "Source Information is not available " << std::endl;
            }
            appProtectionSettingsInnerMapBegIter++;
        }
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
    return retStatus;
}

SVERROR dumpSettings( const ApplicationSettings& appsettings )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
	SVERROR retStatus = SVS_OK;
	std::cout << std::endl << "APPLICATION SETTINGS INFORMATION " << std::endl ;
	std::cout << "---------------------------------" << std::endl ;
	std::cout << std::endl << std::setw(41) << "Application Settings Version: " << appsettings.m_version << std::endl;
	std::cout << std::setw(45) << "-----------------------------------" << std::endl ;
	dumpPolicies( appsettings.m_policies );
	dumpProtectionSettings( appsettings.m_AppProtectionsettings );

	//Remote CX..

	//Time Out..
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return retStatus;
}

void run( int argc, char* argv[] )
{
	if (argc < 2)
	{
		usage(argv[0]);
	}	
	else
	{
		try
		{
			bool bCreate = false;
			AppAgentConfiguratorPtr configuratorPtr;
            LocalConfigurator lConfigurator ;
            SerializeType_t serializeType = lConfigurator.getSerializerType() ;
            
			if((0 == stricmp( argv[1], "--h")) || (0 == stricmp(argv[1], "--help")))
			{
				usage( argv[0] );
			}
			else if(0 == stricmp( argv[1], "--default" ))
			{
				if(( argc > 2 ) && (0 == stricmp(argv[2], "--fromcache")))
				{
					std::cout << "\nParsing cache setting \n\n";
					configuratorPtr  = AppAgentConfigurator::CreateAppAgentConfigurator( serializeType, USE_ONLY_CACHE_SETTINGS ) ;
				}
				else
				{
					std::cout << "\nParsing CX setting \n\n";
					configuratorPtr  = AppAgentConfigurator::CreateAppAgentConfigurator( serializeType, FETCH_SETTINGS_FROM_CX ) ;
				}
				bCreate = true;
			}
			else if(0 == stricmp(argv[1], "--custom"))
			{
				std::map<string, string> options;
				if (argc != 8) 
				{
					usage(argv[0]);
				}
				else
				{
					for (int i=2;i < 8; i+=2)
					{
						options[argv[i]] = argv[i+1];
					}
					std::map<string, string>::iterator endit = options.end();
					if ((options.find("--ip") == endit) || (options.find("--port") == endit) ||
						(options.find("--hostid") == endit))
					{
						usage(argv[0]);
					}
					else
					{
						int const port = atoi(options["--port"].c_str());
						configuratorPtr  = AppAgentConfigurator::CreateAppAgentConfigurator( options["--ip"], port, options["--hostid"], serializeType ) ;
						bCreate = true;
					}
				}
			}
			else 
			{
				std::cout << "Error..." << endl;
				usage(argv[0]);
			}
			if( bCreate )
			{
				configuratorPtr->init() ;
				ApplicationSettings appsettings;
				appsettings = configuratorPtr->GetApplicationSettings();
				dumpSettings( appsettings );
			}
		}
		catch( ContextualException& ce )
		{
			DebugPrintf( SV_LOG_ERROR, "\n Encountered exception %s.\n", ce.what() );
		}
		catch( std::exception const& e )
		{
			DebugPrintf( SV_LOG_ERROR, "\n Encountered exception %s.\n", e.what() );
		}
		catch(...) 
		{
			DebugPrintf( SV_LOG_ERROR, "\n Encountered unknown exception.\n", FUNCTION_NAME );
		}			
	}
	std::cout << std::endl <<std::endl;
}
int main( int argc, char* argv[] )
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );

	ACE::init();
	run( argc, argv );
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return 0;
}

