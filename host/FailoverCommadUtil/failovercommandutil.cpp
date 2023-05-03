#include "failovercommandutil.h"
#include "logger.h"
#include <iomanip>
#include <stdio.h>
#include <boost/lexical_cast.hpp>
#include "serializeapplicationsettings.h"
#include "util/common/util.h"
#include "appexception.h"
#include "controller.h"
#include "serializationtype.h"
#include "service.h"

bool g_bStartSvAgentsAfterFailover = false;
extern SVERROR StopService(const std::string& svcName, SV_UINT timeout);

void FailoverCommandUtil::GetApplicationSettings()
{
	AppLocalConfigurator configurator ;
    ESERIALIZE_TYPE type = configurator.getSerializerType();
    AppAgentConfiguratorPtr configuratorPtr  = AppAgentConfigurator::CreateAppAgentConfigurator(type, USE_ONLY_CACHE_SETTINGS) ;
    try
    {
        m_AppSettings = configuratorPtr->GetApplicationSettings();	          
    }
    catch(AppException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got exception %s\n", ex.to_string().c_str()) ;
    }
    catch(std::exception &ex1)
    {
        DebugPrintf(SV_LOG_ERROR, "Got exception %s\n", ex1.what()) ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Got exception\n") ;
    }
    
}

SVERROR FailoverCommandUtil::GetProtectionSettings(SV_ULONG policyId,AppProtectionSettings& appProtecionSettings)
{
    SVERROR retValue = SVS_FALSE;
    std::map<std::string,AppProtectionSettings> protSettingsMap ;
    std::map<std::string, std::string>& propsMap = m_DisabledAppPolicies.find(policyId)->second.m_policyProperties ;
    std::map<std::string,AppProtectionSettings>& appProtectionsettingsMap = m_AppSettings.m_AppProtectionsettings;
    if(propsMap.find("ApplicationType") != propsMap.end())
    {
        const std::string& appType = propsMap.find("ApplicationType")->second;
        if(propsMap.find("ScenarioId") != propsMap.end())
        {
            const std::string& senarioId = propsMap.find("ScenarioId")->second;
            if(appProtectionsettingsMap.find(senarioId) != appProtectionsettingsMap.end())
            {
                appProtecionSettings =  appProtectionsettingsMap.find(senarioId)->second;  
                retValue = SVS_OK;
            }
        }
        else
        {
            std::cout<<"\nProtectionSettingsId not found in policies propeties map.Unable to proceed";
        }
    }
    else
    {
        std::cout<<"\nApplicationType not found in policies propeties map.Unable to proceed";
    }
    return retValue;
}
SVERROR FailoverCommandUtil::GetFailoverData(SV_ULONG policyId,FailoverPolicyData& failoverData)
{
    SVERROR bRet = SVS_FALSE;
    std::string marshalledTgtFailoverData = m_DisabledAppPolicies.find(policyId)->second.m_policyData;
    try
    {
        failoverData = unmarshal<FailoverPolicyData>(marshalledTgtFailoverData) ;
        bRet = SVS_OK;
    }   
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal FailoverPolicyData %s\n", ex.what()) ;
    }
    return bRet;
}
void FailoverCommandUtil::GetDisabledPolices()
{
    std::list<ApplicationPolicy>::const_iterator appPolicyIter =  m_AppSettings.m_policies.begin();
    while(appPolicyIter != m_AppSettings.m_policies.end())
    {
        if(appPolicyIter->m_policyProperties.find("PolicyType") != appPolicyIter->m_policyProperties.end())
        {
            const std::string& policyType = appPolicyIter->m_policyProperties.find("PolicyType")->second;
            if(appPolicyIter->m_policyProperties.find("ScheduleType") != appPolicyIter->m_policyProperties.end())
            {
                const std::string& policyState = appPolicyIter->m_policyProperties.find("ScheduleType")->second;
                if(appPolicyIter->m_policyProperties.find("PolicyId") != appPolicyIter->m_policyProperties.end())
                {
                    const std::string& policyIdStr = appPolicyIter->m_policyProperties.find("PolicyId")->second;
                    SV_ULONG policyId = boost::lexical_cast<SV_ULONG>(policyIdStr);
                    if(policyType.compare("DRServerUnPlannedFailover") == 0 ||
                       policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 ||
                       policyType.compare("DRServerUnPlannedFailoverSetupApplicationEnvironment") == 0 ||
                       policyType.compare("DRServerUnPlannedFailoverStartApplication") == 0 ) 
                    {
                        if(policyState.compare("0") == 0)
                        {
                            m_DisabledAppPolicies.insert(std::make_pair(policyId,*appPolicyIter));
                        }
                    }
                }
                else
                {
                     std::cout<<"\nPolicyId not found in policies propeties map.Unable to proceed";
                }
            }
            else
            {
                 std::cout<<"\nScheduleType not found in policies propeties map.Unable to proceed";
            }
        }
        else
        {
            std::cout<<"\nPolicyType not found in policies propeties map.Unable to proceed";
        }
        appPolicyIter++;
    }
}

void FailoverCommandUtil::ShowDisabledPolices()
{
    TagDiscover tagDiscoverObj;
    std::map<SV_ULONG,ApplicationPolicy>::iterator appPolicyMapIter =  m_DisabledAppPolicies.begin();
    while(appPolicyMapIter != m_DisabledAppPolicies.end())
    {
        ApplicationPolicy& appPolicy = appPolicyMapIter->second;
        AppProtectionSettings appProtectionSettings;
        GetProtectionSettings(appPolicyMapIter->first,appProtectionSettings);  

        bool showRecoveryOptions = false;
        std::map<std::string,std::string>::iterator appPolicyIter = appPolicy.m_policyProperties.begin();
        while( appPolicyIter != appPolicy.m_policyProperties.end())
        {
            const std::string& policyType = appPolicy.m_policyProperties.find("PolicyType")->second;
            if (policyType.compare("DRServerUnPlannedFailover") == 0 ||
                policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 )
            {
                showRecoveryOptions = true;
            }

            if(appPolicyIter->first.compare("PolicyId") == 0 ||
                appPolicyIter->first.compare("ApplicationType") == 0 ||
                appPolicyIter->first.compare("ProtectionType") == 0 ||
                appPolicyIter->first.compare("PolicyType") == 0)
            {

                std::cout<<std::setw(30);
                std::cout<<appPolicyIter->first;
                std::cout<<": "<<appPolicyIter->second<<std::endl;
            }
             appPolicyIter++;
        }

        if (! showRecoveryOptions)
        {
            std::cout<<std::endl;
            appPolicyMapIter++;
            continue;
        }

        std::cout<<std::setw(30);
        std::cout<<"ApplictionVolumes";
        std::cout<<": ";
        std::list<std::string>::iterator volIter = appProtectionSettings.appVolumes.begin();
        while( volIter != appProtectionSettings.appVolumes.end())
        {
             std::cout<<*volIter<<" ";
             volIter++;
        }
        std::cout<<std::endl;
        std::cout<<std::setw(30);
        std::cout<<"OtherVolumes";
        std::cout<<": ";
        volIter = appProtectionSettings.CustomVolume.begin();
        while( volIter != appProtectionSettings.CustomVolume.end())
        {
             std::cout<<*volIter<<" ";
             volIter++;
        } 
        std::cout<<std::endl;
		std::cout<<std::setw(31);
		std::cout<<"Replication Pair status:";
		std::map<std::string,ReplicationPair>::const_iterator iterPair = 
			appProtectionSettings.Pairs.begin();
		bool bResyncPairFound = false;
		while(iterPair != appProtectionSettings.Pairs.end())
		{
			std::string RemoteMountPoint,PairState;
			std::map<std::string, std::string>::const_iterator iterProp;

			iterProp = iterPair->second.m_properties.find("RemoteMountPoint");
			if(iterProp != iterPair->second.m_properties.end())
				RemoteMountPoint = iterProp->second;

			iterProp = iterPair->second.m_properties.find("PairState");
			if(iterProp != iterPair->second.m_properties.end())
				PairState = iterProp->second;

			if(RemoteMountPoint.empty() || PairState.empty())
			{
				std::cout << "WARNING: Protection settings are missing for "<< iterPair->first.c_str() 
					<<". Recovery scenario may fail" << std::endl;
			}
			else
			{
				std::cout<<" "<<RemoteMountPoint.c_str() << " => " << PairState.c_str() << std::endl;
				if(strcmpi(PairState.c_str(),"Diff"))
					bResyncPairFound = true;
				std::cout<<std::setw(32);
			}
			iterPair++;
		}

		m_PoliciesWithResyncPair[appPolicyMapIter->first] = bResyncPairFound;

        if( tagDiscoverObj.init() == SVS_OK )
        {
            std::list<std::string> volumeList;
            volumeList = appProtectionSettings.appVolumes;
            std::list<std::string>::iterator volumeIter = appProtectionSettings.CustomVolume.begin();
            while(volumeIter != appProtectionSettings.CustomVolume.end())
            {
                volumeList.push_back(*volumeIter);
                volumeIter++;
            }
            
            CDPEvent cdpCommonEvent;
            unsigned long long eventTime;
            std::string commonTimeStr;
            std::cout<<std::setw(30);
            std::cout<<"LatestCommonTime";
            if(tagDiscoverObj.getRecentCommonConsistencyPoint(volumeList, eventTime) == SVS_OK)
            {
                tagDiscoverObj.getTimeinDisplayFormat(eventTime,commonTimeStr);
                std::cout<<": "<<commonTimeStr<<std::endl;
            }
            else
            {
                 std::cout<<": "<<"Not Available"<<std::endl;
            }
            std::cout<<std::setw(30);
            std::cout<<"LatestCommonConsistencyPoint";
            if(tagDiscoverObj.getRecentConsistencyPoint(volumeList,"FS",cdpCommonEvent) == SVS_OK)
            {
                commonTimeStr.clear();
                tagDiscoverObj.getTimeinDisplayFormat(cdpCommonEvent.c_eventtime,commonTimeStr);
                std::cout<<": "<<commonTimeStr<<std::endl; 
                std::cout<<std::setw(30);
                std::cout<<" ";
                std::cout<<"  TagName : "<<cdpCommonEvent.c_eventvalue<<std::endl; 
                std::cout<<std::setw(30);
                std::cout<<" ";
                std::cout<<"  TagType : "<<"FS"<<std::endl; 
            }
            else
            {
                std::cout<<": "<<"Not Available"<<std::endl;
            }
        }

        std::cout<<std::endl;
        appPolicyMapIter++;
    }
}

SVERROR FailoverCommandUtil::EnablePolicy(SV_ULONG policyId)
{
    FailoverPolicyData failoverData;
    SVERROR retVal = SVS_FALSE;
    if(GetFailoverData(policyId,failoverData) == SVS_OK)
    {
        AppProtectionSettings appProtecionSettings;
        GetProtectionSettings(policyId,appProtecionSettings);
        std::list<std::string> volumeList;
        volumeList = appProtecionSettings.appVolumes;
        std::list<std::string>::iterator volumeIter = appProtecionSettings.CustomVolume.begin();
        while(volumeIter != appProtecionSettings.CustomVolume.end())
        {
           volumeList.push_back(*volumeIter);
           volumeIter++;
        }

            std::list<ApplicationPolicy>::iterator policyIter =  m_AppSettings.m_policies.begin();
            while(policyIter != m_AppSettings.m_policies.end())
            {
            std::map<std::string,std::string>::iterator iter = policyIter->m_policyProperties.begin();
                if(iter != policyIter->m_policyProperties.end())
                {
                const std::string& policyIdStr = policyIter->m_policyProperties.find("PolicyId")->second;
                const std::string& policyType = policyIter->m_policyProperties.find("PolicyType")->second;

                    SV_ULONG appPolicyId = boost::lexical_cast<SV_ULONG>(policyIdStr);

                    if(appPolicyId == policyId)
                    {
                    SVERROR retStatus = SVS_OK;

                    if (policyType.compare("DRServerUnPlannedFailover") == 0 ||
                        policyType.compare("DRServerUnPlannedFailoverRollbackVolumes") == 0 )
                    {
                        retStatus = GetRecoverOptions(volumeList,failoverData);
                    }
                    if (retStatus == SVS_OK)
                    {
                        if(policyIter->m_policyProperties.find("ScheduleType") != policyIter->m_policyProperties.end()) 
                        {
                           PrintSummary(failoverData);
                           std::cout<<std::endl<<"Do you want to proceed: ";
                           char charInput;
                           std::string tempStr = "yYnN";
                           while(true)
                           {
                               std::cout<<"Choose y/n :";
                               std::cin>> charInput;
                               if(tempStr.find(charInput) != std::string::npos)
                               {
                                   if(charInput == 'Y' ||
                                       charInput == 'y')
                                   {
									   /*
									   If selected policy's protection settings having any pair with Resync state 
									   then the svagent service should be in stopped state at the time of unplanned failover.
									   */
									   std::map<SV_ULONG,bool>::const_iterator iterResyncPair =
										   m_PoliciesWithResyncPair.find(policyId);
									   if((iterResyncPair != m_PoliciesWithResyncPair.end()) && iterResyncPair->second)
									   {
										   retVal = StopSVAgent();
										   if(retVal != SVS_OK)
											   break;
									   }
                                       policyIter->m_policyProperties.find("ScheduleType")->second = "1";
                                       std::stringstream targetTargetDataStream;
                                       targetTargetDataStream <<  CxArgObj<FailoverPolicyData>(failoverData);
                                       policyIter->m_policyData = targetTargetDataStream.str();
                                       SerializeAppSettings();
                                       std::cout<<"\n\nPolicy Enabled successfully\n\n";
                                       retVal = SVS_OK;
                                       break;
                                   }
                                   else if(charInput == 'N' ||
                                       charInput == 'n')
                                   {
                                       std::cout<<std::endl<<std::endl<<"Policy not enabled"<<std::endl;
                                       retVal = SVS_FALSE;
                                       break;
                                   }
                           }
                        }
                    }
                }
                else
                {
                        std::cout<<"\n Policy not enabled";
                }
            }
        } 
        else
        {
                std::cout<<"Wrong PolicyId chosen\n";
            }
            policyIter++;
        }
    }
    else
    {
        std::cout<<"\nUnable to proceed";
    }
    return retVal;
}
SVERROR FailoverCommandUtil::GetRecoverOptions(std::list<std::string>& volumeList,FailoverPolicyData& failoverData)
{
    SVERROR retValue = SVS_FALSE;
    TagDiscover tagDiscoverObj;
    failoverData.m_recoveryPoints.clear();
    failoverData.m_properties.clear();
    if( tagDiscoverObj.init() == SVS_OK )
	{
        int recoveryType = GetRecoveryType(volumeList);
        if(recoveryType == 1)
        {
            std::cout<<std::endl<<"Latest Common Time choosen"<<std::endl;
            if(failoverData.m_properties.find("RecoveryPointType") != failoverData.m_properties.end())
            {
                failoverData.m_properties.find("RecoveryPointType")->second = "LCT";
            }
            else
            {
                failoverData.m_properties.insert(std::make_pair("RecoveryPointType","LCT"));
            }
            retValue = SVS_OK;
        }
        
        else if(recoveryType == 2)
        {
            std::cout<<std::endl<<"Latest Common Consistent Point choosen"<<std::endl;
            if(failoverData.m_properties.find("RecoveryPointType") != failoverData.m_properties.end())
            {
                failoverData.m_properties.find("RecoveryPointType")->second = "LCCP";
            }
            else
            {
                failoverData.m_properties.insert(std::make_pair("RecoveryPointType","LCCP"));
            }
            retValue = SVS_OK;
        }
        else if(recoveryType == 3)
        {
            if(ChooseCustomRecovery(volumeList,failoverData)== SVS_OK)
            {
                retValue = SVS_OK;
            }
        }
    }
        return retValue;
}

SV_ULONG FailoverCommandUtil::GetNoOfPolicies()
{
    return m_DisabledAppPolicies.size();
}

ApplicationSettings FailoverCommandUtil::ReadSettings (std::string const & filename)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ApplicationSettings l_appsettings ;

    std::ifstream l_is;
    unsigned int l_length =0;

    std::string lockpath = filename + ".lck";

    ACE_File_Lock l_lock(getLongPathName(lockpath.c_str()).c_str(),O_CREAT | O_RDONLY, SV_LOCK_PERMISSIONS,0);
    if( -1 == l_lock.acquire_read())
    {
       DebugPrintf(SV_LOG_ERROR, "Failed to acquired lock for the file %s\n", lockpath.c_str()) ;
       return unmarshal<ApplicationSettings> ("");
    }
   
    l_is.open(filename.c_str(), std::ios::in | std::ios::binary);
    if (!l_is.is_open())
    {
       return unmarshal<ApplicationSettings> ("");
    }

    l_is.seekg (0, std::ios::end);
    l_length = l_is.tellg();
    l_is.seekg (0, std::ios::beg);

    boost::shared_array<char> l_bufSettings(new char[l_length + 1]);
    if(!l_bufSettings)
    {
        return unmarshal<ApplicationSettings> ("");
    }

    l_is.read (l_bufSettings.get(),l_length);
    *(l_bufSettings.get() + l_length) = '\0';

    l_is.close();
    l_lock.release();

    try {
          l_appsettings = unmarshal<ApplicationSettings> (l_bufSettings.get()) ;
          DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
          return l_appsettings;
    }
    catch ( ContextualException& ce )
    {
       DebugPrintf(SV_LOG_ERROR,"\n%s encountered exception %s.\n",FUNCTION_NAME, ce.what());
       return unmarshal<ApplicationSettings> ("");
    }
    catch(std::exception ex)
    {
       DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal ApplicationSettings %s\n", ex.what()) ;
       return unmarshal<ApplicationSettings> ("");
    }
}

bool FailoverCommandUtil::CopyCacheFile(std::string const & SourceFile, std::string const & DestinationFile)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    bool l_rv = true;
    const std::size_t l_buf_sz = 4096;
    try
    {
        char buf[l_buf_sz];
        ACE_HANDLE l_infile=ACE_INVALID_HANDLE , outfile=ACE_INVALID_HANDLE;
        ACE_stat from_stat = {0};
        if ( ACE_OS::lstat((const char*) SourceFile.c_str(), &from_stat ) == 0
            && (l_infile = ACE_OS::open(SourceFile.c_str(), O_RDONLY )) != ACE_INVALID_HANDLE
            && (outfile = ACE_OS::open( DestinationFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC )) != ACE_INVALID_HANDLE )
        {
            ssize_t l_sz, l_sz_read=1, l_sz_write;
            while ( l_sz_read > 0
                && (l_sz_read = ACE_OS::read( l_infile, buf, l_buf_sz )) > 0 )
            {
                l_sz_write = 0;
                do
                {
                    if ( (l_sz = ACE_OS::write( outfile, buf + l_sz_write, l_sz_read - l_sz_write )) < 0 )
                    {
                        l_sz_read = l_sz; // cause read loop termination
                        break;        //  and error to be thrown after closes
                    }
                    l_sz_write += l_sz;
                } while ( l_sz_write < l_sz_read );
            }
            if ( ACE_OS::close( l_infile) < 0 )
                l_sz_read = -1;
            if ( ACE_OS::close( outfile) < 0 )
                l_sz_read = -1;
            if ( l_sz_read < 0 )
            {
                l_rv = false;
            }
        }
        else
        {
            l_rv = false;
            if ( outfile != ACE_INVALID_HANDLE)
                ACE_OS::close( outfile );
            if ( l_infile != ACE_INVALID_HANDLE)
                ACE_OS::close( l_infile );
        }
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Caught Exception [%s] while copying the file %s to %s \n", ex.what(),SourceFile.c_str(),DestinationFile.c_str()) ;
        l_rv =false;
    }
    return l_rv;
}

bool FailoverCommandUtil::CopySettingsForRerun(SV_ULONG PolicyId )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    
    bool b_rv=true;

    AppLocalConfigurator localConfigurator ;
    std::string cachePath = localConfigurator.getAppSettingsCachePath() ;
    std::string copyCachePath = cachePath + ".copy";

    ApplicationSettings m_SrcFileSettings = ReadSettings(copyCachePath);

    ApplicationPolicy l_appPolicy;
    std::list<ApplicationPolicy>::iterator l_policyIter =  m_SrcFileSettings.m_policies.begin();
    while(l_policyIter != m_SrcFileSettings.m_policies.end())
    {
       std::map<std::string,std::string>::iterator iter = l_policyIter->m_policyProperties.begin();
       if(iter != l_policyIter->m_policyProperties.end())
       {
          const std::string& policyIdStr = l_policyIter->m_policyProperties.find("PolicyId")->second;
          const std::string& policyType = l_policyIter->m_policyProperties.find("PolicyType")->second;
          SV_ULONG appPolicyId = boost::lexical_cast<SV_ULONG>(policyIdStr);

          if ( appPolicyId == PolicyId )
          {
              l_policyIter->m_policyProperties.find("ScheduleType")->second = "0";
              l_appPolicy = *l_policyIter;
          }
       } 
       l_policyIter++;
    }

    m_AppSettings = ReadSettings(cachePath);
    ApplicationPolicy policyToErase;

    int l_isfound=0;
    std::list<ApplicationPolicy>::iterator destPolicyItr = m_AppSettings.m_policies.begin();
    while(destPolicyItr != m_AppSettings.m_policies.end())
    {
       const std::string& destPolicyIdStr = destPolicyItr->m_policyProperties.find("PolicyId")->second;
       SV_ULONG destPolicyId = boost::lexical_cast<SV_ULONG>(destPolicyIdStr);

       if ( destPolicyId == PolicyId )
       {
          if ( destPolicyItr->m_policyProperties.find("ScheduleType")->second != "1" )
          {
              l_isfound = 1;
          }
          else
          {
              policyToErase = *destPolicyItr;
          }
       }
       destPolicyItr++;
    }
    if ( l_isfound != 1 )
    {
       m_AppSettings.m_policies.remove(policyToErase);
       m_AppSettings.m_policies.push_back(l_appPolicy); 
       SerializeAppSettings();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return b_rv;
}

bool FailoverCommandUtil::ValidatePolyId(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG," Entering %s\n",__FUNCTION__);

    bool m_retValue = false;

    ApplicationPolicy l_appPolicy;
    std::string depPolicyStr="";
    FailoverJobs m_FailoverJobs;
    FailoverJob failoverjob;
    getFailoverJobs(m_FailoverJobs);
    SV_ULONG depPolicyId = 0;
    AppLocalConfigurator localConfigurator ;
    std::string cachePath,copyCachePath;

    std::map<SV_ULONG,ApplicationPolicy>::iterator policyIter = m_DisabledAppPolicies.begin();
    while(policyIter != m_DisabledAppPolicies.end())
    {
        if(policyIter->first == policyId)
        {
            l_appPolicy = policyIter->second;
            m_appType = l_appPolicy.m_policyProperties.find("ApplicationType")->second;

            if ( m_appType == "ORACLE" || m_appType == "DB2" )
            {
                depPolicyStr = l_appPolicy.m_policyProperties.find("DependantPolicyId")->second;
                DebugPrintf(SV_LOG_DEBUG," Dependant Policy  : %s\n",depPolicyStr.c_str());
                depPolicyId = boost::lexical_cast<SV_ULONG>(depPolicyStr);

                DebugPrintf(SV_LOG_DEBUG," Dependant Policy Id : %d\n",depPolicyId);
                if ( depPolicyId != 0 )
                {
                   std::map<SV_ULONG, FailoverJob>::iterator failoverJobIter =  m_FailoverJobs.m_FailoverJobsMap.find(depPolicyId);
                   if(failoverJobIter != m_FailoverJobs.m_FailoverJobsMap.end())
                   {
                      failoverjob = failoverJobIter->second;
                      DebugPrintf(SV_LOG_DEBUG," Dependant Policy status : %d\n",failoverjob.m_JobState);

                      if(failoverjob.m_JobState == TASK_STATE_COMPLETED)
                      {
                         cachePath = localConfigurator.getAppSettingsCachePath() ;
                         copyCachePath = cachePath + ".copy";
                         CopyCacheFile(cachePath,copyCachePath);
                         m_retValue = true;
                         break;
                      }
                      else
                      {
                         std::cout<<"Dependant Policy "<<depPolicyId<< " is not completed"<<std::endl;
                         std::cout<<"Policy "<<policyId << " cannot be executed" <<std::endl;
                         break;
                      }
                   }
                   else
                   {
                       cachePath = localConfigurator.getAppSettingsCachePath() ;
                       copyCachePath = cachePath + ".copy";
                       CopyCacheFile(cachePath,copyCachePath);
                       m_retValue = true;
                       break;
                   }

                }
                else
                {
                   cachePath = localConfigurator.getAppSettingsCachePath() ;
                   copyCachePath = cachePath + ".copy";
                   CopyCacheFile(cachePath,copyCachePath);
                   m_retValue = true;
                   break;
                }
            }
            else
            {
                m_retValue = true;
                break;
            }
        }
        policyIter++;
    }
    
    return m_retValue;
}

void FailoverCommandUtil::SerializeAppSettings()
{
	AppLocalConfigurator configurator ;
    ESERIALIZE_TYPE type = configurator.getSerializerType();
    AppAgentConfiguratorPtr configuratorPtr  = AppAgentConfigurator::CreateAppAgentConfigurator( type ) ;
    configuratorPtr->SerializeApplicationSettings(m_AppSettings);
}

void FailoverCommandUtil::ChooseLastestCommonTag(std::map<std::string, std::vector<CDPEvent> > eventInfoMap,FailoverPolicyData& failoverData)
{
    TagDiscover tagDiscoverObj;
    if( tagDiscoverObj.init() == SVS_OK )
	{
        std::map<std::string, std::vector<CDPEvent> >::iterator eventInfoMapIter =  eventInfoMap.begin();
        while(eventInfoMapIter != eventInfoMap.end())
        {
            std::cout<< "\n\n\nVolume:"<<eventInfoMapIter->first;
            std::map<std::string, std::vector<CDPEvent> > tempMap;
            tempMap.insert(std::make_pair(eventInfoMapIter->first,eventInfoMapIter->second));
            tagDiscoverObj.printListOfEvents(tempMap);
            std::vector<CDPEvent>::iterator eventIter = eventInfoMapIter->second.begin();
            std::cout<<"Choose event number  from TAG above table:";
            int eventNumber;
            std::cin >> eventNumber;
            while(eventIter !=  eventInfoMapIter->second.end())
            {
                if(eventNumber == eventIter->c_eventno)
                {
                    RecoveryPoints rpoints;
                    rpoints.m_properties.insert(std::make_pair("RecoveryPointType","TAG"));
                    rpoints.m_properties.insert(std::make_pair("TagName",eventIter->c_eventvalue));
                    std::string tagTypeStr = tagDiscoverObj.getTagType(eventIter->c_eventtype);
                    rpoints.m_properties.insert(std::make_pair("TagType",tagTypeStr));
                    rpoints.m_properties.insert(std::make_pair("Timestamp",""));
                    if(failoverData.m_recoveryPoints.find(eventInfoMapIter->first) != failoverData.m_recoveryPoints.end())
                    {
                        failoverData.m_recoveryPoints.find(eventInfoMapIter->first)->second = rpoints;
                    }
                    else
                    {
                        failoverData.m_recoveryPoints.insert(make_pair(eventInfoMapIter->first,rpoints));
                    }
                    break;
                }
                eventIter++;
                if(eventIter == eventInfoMapIter->second.end())
                {
                    std::cout<<"No event found with number "<< eventNumber <<"\n";
                }
            }
            eventInfoMapIter++;
        }
    }
}

void FailoverCommandUtil::ChooseLastestCommonTime(std::map< std::string, std::vector<CDPTimeRange> > timeRangeInfoMap,FailoverPolicyData& failoverData)
{
    TagDiscover tagDiscoverObj;
    if( tagDiscoverObj.init() == SVS_OK )
	{
        std::map<std::string,std::vector<CDPTimeRange> >::iterator timeRangeInfoIter = timeRangeInfoMap.begin();
        while(timeRangeInfoIter != timeRangeInfoMap.end())
        {
            std::cout<< "\n\n\nVolume:"<<timeRangeInfoIter->first;
            std::map< std::string, std::vector<CDPTimeRange> > tempTimeRangeMap;
            if(timeRangeInfoIter->second.size() != 0)
            {
            tempTimeRangeMap.insert(std::make_pair(timeRangeInfoIter->first,timeRangeInfoIter->second));
            tagDiscoverObj.printTimeRanges(tempTimeRangeMap,3);
            int eventNumber;
			CDPTimeRange timeRange;
			do {
				std::cout<<"\nChoose event number from TIME table:";
				std::cin>>eventNumber;
				std::vector<CDPTimeRange>::const_iterator iterTimeRange = timeRangeInfoIter->second.begin();
				std::vector<CDPTimeRange>::const_iterator iterTimeRangeEnd = timeRangeInfoIter->second.end();
				while(iterTimeRangeEnd != iterTimeRange)
				{
					if(iterTimeRange->c_entryno == eventNumber)
						break;
					iterTimeRange++;
				}
				if(iterTimeRange != iterTimeRangeEnd)
				{
					timeRange = timeRangeInfoIter->second[eventNumber-1];
					break;
				}
				else
				{
					std::cout << "Invalid Event number choosen" << std::endl;
				}
			}while(1);
            std::string fromTimeStr;
            std::string toTimeStr;
            tagDiscoverObj.getTimeinDisplayFormat(timeRange.c_starttime,fromTimeStr);
            tagDiscoverObj.getTimeinDisplayFormat(timeRange.c_endtime,toTimeStr);
              /*  char str[30];
            while(true)
            {
                std::cout<<"\nChoose the Time between "<<fromTimeStr<< " - " << toTimeStr << " : ";
                    std::cout<<"\nEnter Time in YYYY-MM-DD HH:MM:SS format ";
                gets(str);
                time_t timeVal = convertStringToTime(str);
                if(timeRange.c_starttime <= timeVal && timeVal <=timeRange.c_endtime )
                {
                    break;
                }
                }*/
            RecoveryPoints rpoints;
            rpoints.m_properties.insert(std::make_pair("RecoveryPointType","TIME"));
            rpoints.m_properties.insert(std::make_pair("TagName",""));
            rpoints.m_properties.insert(std::make_pair("TagType",""));
                rpoints.m_properties.insert(std::make_pair("Timestamp",toTimeStr));
            if(failoverData.m_recoveryPoints.find(timeRangeInfoIter->first) != failoverData.m_recoveryPoints.end())
            {
                failoverData.m_recoveryPoints.find(timeRangeInfoIter->first)->second = rpoints;
            }
            else
            {
                failoverData.m_recoveryPoints.insert(make_pair(timeRangeInfoIter->first,rpoints));
            }
            }
            else
            {
                std::cout<<"Time range for this volume not available\n";
            }
            timeRangeInfoIter++;
        }
    }

}
SVERROR FailoverCommandUtil::LastestCommonTag(std::vector<CDPEvent>& cdpEvents,std::string volumeName,FailoverPolicyData& failoverData)
{
	SVERROR ret = SVS_FALSE;
    TagDiscover tagDiscoverObj;
    if( tagDiscoverObj.init() == SVS_OK )
    {
    std::vector<CDPEvent>::iterator eventIter = cdpEvents.begin();
    std::cout<<"Choose number from the TAGS table:";
    int eventNumber;
    std::cin >> eventNumber;
    while(eventIter !=  cdpEvents.end())
    {
        if(eventNumber == eventIter->c_eventno)
        {
			ret = SVS_OK;
            RecoveryPoints rpoints;
            rpoints.m_properties.insert(std::make_pair("RecoveryPointType","TAG"));
            rpoints.m_properties.insert(std::make_pair("TagName",eventIter->c_eventvalue));
                std::string tagTypeStr = tagDiscoverObj.getTagType(eventIter->c_eventtype);
            rpoints.m_properties.insert(std::make_pair("TagType",tagTypeStr));
            rpoints.m_properties.insert(std::make_pair("Timestamp",""));
            if(failoverData.m_recoveryPoints.find(volumeName) != failoverData.m_recoveryPoints.end())
            {
                failoverData.m_recoveryPoints.find(volumeName)->second = rpoints;
            }
            else
            {
                failoverData.m_recoveryPoints.insert(make_pair(volumeName,rpoints));
            }
            break;
        }
        eventIter++;
        if(eventIter == cdpEvents.end())
        {
            std::cout<<"No event found with number "<< eventNumber <<"\n";
        }
    }
    }
    return ret;
}

SVERROR FailoverCommandUtil::LastestCommonTime(std::vector<CDPTimeRange>& cdpTimeRanges,std::string volumeName,FailoverPolicyData& failoverData)
{
	SVERROR ret = SVS_FALSE;
    TagDiscover tagDiscoverObj;
    if( tagDiscoverObj.init() == SVS_OK )
    {
        SV_ULONGLONG eventNumber;
        std::cout<<"\nChoose event number from TIME table:";
        std::cin>>eventNumber;
		size_t index=0;
		std::vector<CDPTimeRange>::const_iterator iterTimeRange = cdpTimeRanges.begin();
		while(cdpTimeRanges.end() != iterTimeRange)
		{
			if(eventNumber == iterTimeRange->c_entryno){ ret = SVS_OK; break; }
			iterTimeRange++;
		}
		if(cdpTimeRanges.end() == iterTimeRange)
		{
			std::cout << "No event found with the number " << eventNumber << std::endl ;
			return ret;
		}
		CDPTimeRange timeRange = *iterTimeRange;
        std::string fromTimeStr;
        std::string toTimeStr;
        tagDiscoverObj.getTimeinDisplayFormat(timeRange.c_starttime,fromTimeStr);
        tagDiscoverObj.getTimeinDisplayFormat(timeRange.c_endtime,toTimeStr);
        std::string timeStr;
				char ch;
				SV_ULONGLONG timeVal;
        while(true)
        {
					  timeStr.clear();
					  fflush(stdin);
						std::cout<<"\nChoose the Time between "<<fromTimeStr<< " - " << toTimeStr << " : ";
						std::cout<<"\nEnter Time in yyyy/mm/dd [hr][:min][:sec][:millisec][:microsec][:hundrednanosec] format:";
						ch=getchar();
						while(ch!='\n')
						{
							timeStr+=ch;
							ch=getchar();
						}
						if(timeStr.length()>0)
						{
							CDPUtil::InputTimeToFileTime(timeStr,timeVal);
							if(timeRange.c_starttime <= timeVal && timeVal <=timeRange.c_endtime )
							{						
								break;
							}
						}
        }

        RecoveryPoints rpoints;
        rpoints.m_properties.insert(std::make_pair("RecoveryPointType","TIME"));
        rpoints.m_properties.insert(std::make_pair("TagName",""));
        rpoints.m_properties.insert(std::make_pair("TagType",""));
        rpoints.m_properties.insert(std::make_pair("Timestamp",timeStr));
        if(failoverData.m_recoveryPoints.find(volumeName) != failoverData.m_recoveryPoints.end())
        {
            failoverData.m_recoveryPoints.find(volumeName)->second = rpoints;
        }
        else
        {
            failoverData.m_recoveryPoints.insert(make_pair(volumeName,rpoints));
        }
    }
	return ret;
}
int FailoverCommandUtil::GetRecoveryType(std::list<std::string>& volumeList)
{
    TagDiscover tagDiscoverObj;
    int recoveryType = -1;
   
    if( tagDiscoverObj.init() == SVS_OK )
	{
        bool bLCCP = false;
        bool bLCT = false;
        std::map<std::string, std::vector<CDPEvent> > eventInfoMap;
        std::map< std::string, std::vector<CDPTimeRange> > timeRangeInfoMap;
        CDPEvent cdpCommonEvent;
        unsigned long long eventTime;
        std::string commonTimeStr;
        std::cout<<std::endl;
        std::cout<<std::setw(30);
        std::cout<<"1.LatestCommonTime";
        if(tagDiscoverObj.getRecentCommonConsistencyPoint(volumeList, eventTime) == SVS_OK)
        {
            bLCT = true;
            tagDiscoverObj.getTimeinDisplayFormat(eventTime,commonTimeStr);
            std::cout<<": "<<commonTimeStr<<std::endl;
        }
        else
        {
             std::cout<<": "<<"Not available\n";   
        }
        std::cout<<std::setw(30);
        std::cout<<"2.LatestCommonConsistencyPoint";
        if(tagDiscoverObj.getRecentConsistencyPoint(volumeList,"FS",cdpCommonEvent) == SVS_OK)
        {
            bLCCP = true;
            tagDiscoverObj.getTimeinDisplayFormat(cdpCommonEvent.c_eventtime,commonTimeStr);
            std::cout<<": "<<commonTimeStr<<std::endl; 
            std::cout<<std::setw(30);
            std::cout<<" ";
            std::cout<<"  TagName : "<<cdpCommonEvent.c_eventvalue<<std::endl; 
            std::cout<<std::setw(30);
            std::cout<<" ";
            std::cout<<"  TagType : "<<"FS"<<std::endl; 
        }
        else
        {
             std::cout<<": "<<"Not available\n";   
        }
        std::cout<<std::setw(30);
        std::cout<<"3.Custom"<<std::endl;
      
        if(bLCCP == true && bLCT == true)
        {
            std::cout<<std::endl<<std::endl<<"Choose one of the above to proceed";
            while (true)
            {
                std::cout<<"(Choose 1 OR 2 OR 3):" ;
                std::cin>> recoveryType;
                if(recoveryType >= 1 && recoveryType <=3)
                {
                    break;
                }
                
            }
        }
        else if(bLCCP == true && bLCT == false)
        {
            std::cout<<std::endl<<std::endl<<"Choose one of the above to proceed";
            while (true)
            {
                std::cout<<"(Choose 2 OR 3):" ;   
                std::cin>> recoveryType;
                if(recoveryType >= 2 && recoveryType <=3)
                {
                    break;
                }
            }
        }
        else if(bLCCP == false && bLCT == true)
        {
            std::cout<<std::endl<<std::endl<<"Choose one of the above to proceed";
            while (true)
            {
                std::cout<<"(Choose 1 OR 3):" ;   
                std::cin>> recoveryType;
                if(recoveryType == 1 ||  recoveryType ==3)
                {
                    break;
                }
            }
        }
        else
        {
            std::cout<<"\nCommonTime and CommontPoint not available:";
            std::cout<<"Do you want to proceed with Custom Recovery for each volume";
            char charInput;
            std::string tempStr = "yYnN";
            while(true)
            {
                 std::cout<<"Choose y/n :";
                 std::cin>> charInput;
                if(tempStr.find(charInput) != std::string::npos)
                {
                    if(charInput == 'Y' ||
                       charInput == 'y')
                    {
                        recoveryType = 3;
                        break;
                    }
                    if(charInput == 'N' ||
                       charInput == 'n')
                    {
                        recoveryType = -1;
                    }
                    break;
                }
            }
        }
    }
    return recoveryType;
}

SVERROR FailoverCommandUtil::ChooseCustomRecovery(std::list<std::string>& volumeList,FailoverPolicyData& failoverData)
{
    TagDiscover tagDiscoverObj;
    SVERROR retVal = SVS_FALSE;
    failoverData.m_recoveryPoints.clear();
    failoverData.m_properties.clear();
    int input = 0;
    std::map<std::string, std::vector<CDPEvent> > eventInfoMap;
    std::map< std::string, std::vector<CDPTimeRange> > timeRangeInfoMap;
    std::vector<CDPEvent> cdpEventVector;
    std::vector<CDPTimeRange> cdpTimeRangeVector;
    if( tagDiscoverObj.init() == SVS_OK )
	{
        tagDiscoverObj.getListOfEvents(volumeList, eventInfoMap );
        tagDiscoverObj.getTimeRanges(volumeList,ACCURACY_EXACT,timeRangeInfoMap);
        if(failoverData.m_properties.find("RecoveryPointType") != failoverData.m_properties.end())
        {
            failoverData.m_properties.find("RecoveryPointType")->second = "CUSTOM";
        }
        else
        {
            failoverData.m_properties.insert(std::make_pair("RecoveryPointType","CUSTOM"));
        }
        std::list<std::string>::iterator volListIter = volumeList.begin();
        bool bEvents = false;
        bool bTimeRange = false;
        while(volListIter != volumeList.end())
        {
            if(eventInfoMap.find(*volListIter) != eventInfoMap.end())
            {
                cdpEventVector = eventInfoMap.find(*volListIter)->second;
                if(cdpEventVector.size() != 0)
                {
                   bEvents = true;
                }
            }
            if(timeRangeInfoMap.find(*volListIter) != timeRangeInfoMap.end())
            {
                cdpTimeRangeVector = timeRangeInfoMap.find(*volListIter)->second;
                if(cdpTimeRangeVector.size() != 0)
                {
                    bTimeRange = true;
                }
            }
            if(bEvents & bTimeRange)
            {
                std::map<std::string, std::vector<CDPEvent> > tempMap;
                tempMap.insert(std::make_pair(*volListIter,cdpEventVector));
                tagDiscoverObj.printListOfEvents(tempMap);
                std::map< std::string, std::vector<CDPTimeRange> > tempTimeRangeMap;
                tempTimeRangeMap.insert(std::make_pair(*volListIter,cdpTimeRangeVector));
                tagDiscoverObj.printTimeRanges(tempTimeRangeMap,3);
                while (true)
                {
                    std::cout<<"\nChoose 1.TAG 2.Time  : ";
                    std::cin >> input;
                    if(input == 1 ||  input == 2)
                    {   
                        break;
                    }
                }
            }
            else if(!bEvents & bTimeRange)
            {
                std::map< std::string, std::vector<CDPTimeRange> > tempTimeRangeMap;
                tempTimeRangeMap.insert(std::make_pair(*volListIter,cdpTimeRangeVector));
                tagDiscoverObj.printTimeRanges(tempTimeRangeMap,3);
                input = 2;
            }
            else if (bEvents & !bTimeRange)
            {
                std::map<std::string, std::vector<CDPEvent> > tempMap;
                tempMap.insert(std::make_pair(*volListIter,cdpEventVector));
                tagDiscoverObj.printListOfEvents(tempMap);
                input = 1;
            }
            if( input == 1)
            {
                retVal = LastestCommonTag(cdpEventVector,*volListIter,failoverData);
				if(retVal != SVS_OK) break;
            }
            else if(input == 2)
            {
                retVal = LastestCommonTime(cdpTimeRangeVector,*volListIter,failoverData);
				if(retVal != SVS_OK) break;
            }
            else
            {
                std::cout<<"No common time or tag available\n";
            }
            volListIter++;
        }
    }
    return retVal;
}


SVERROR FailoverCommandUtil::PrintLogFile(std::string outputFileName)
{
    int inm_length;
    char * inm_buffer;
    std::ifstream inm_is;
    SVERROR bRet = SVS_FALSE;
    inm_is.open (outputFileName.c_str(), std::ios::binary );
    if (!inm_is.is_open() )
    {
        std::cout<<"Failed to open output file "<<outputFileName.c_str();
        
    }
    else
    {
        inm_is.seekg (0, std::ios::end);
        inm_length = inm_is.tellg();
        inm_is.seekg (0, std::ios::beg);
        inm_buffer = new char [inm_length];
        inm_is.read (inm_buffer,inm_length);
        inm_is.close();
        std::cout.write (inm_buffer,inm_length);
        delete[] inm_buffer;
        bRet = SVS_OK;
    }
    return bRet;
}
void FailoverCommandUtil::getFailoverJobs(FailoverJobs& FailoverJobs)
{
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    try
    {
        if(appConfiguratorPtr->UnserializeFailoverJobs(FailoverJobs) != SVS_OK )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unserialize the scheduling Info\n") ;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Previously saved failover jobs information loaded to memory\n ") ;
        }
    }
    catch(std::exception &ex)
    {
        DebugPrintf(SV_LOG_DEBUG,"Exception caught while unserializing failoverjobs info.Ignoring it safely.%s\n",ex.what());
    }
}
void FailoverCommandUtil::monitorFailoverExecution(SV_ULONG policyId)
{
    AppAgentConfiguratorPtr appConfiguratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
    AppLocalConfigurator configurator ;
    std::string policyIdStr = boost::lexical_cast<std::string>(policyId);
    std::string cacheDirPath = configurator.getApplicationPolicyLogPath() ;
    FailoverJobs m_FailoverJobs;
    FailoverJob failoverjob;
    bool bFind = false;
    AppSchedulerPtr schedulerPtr ;
    std::cout<<"Waiting for UnPlanned Failover execution .";
    while( !bFind && !Controller::getInstance()->QuitRequested(10) )
    {
        getFailoverJobs(m_FailoverJobs);
        std::map<SV_ULONG, FailoverJob>::iterator failoverJobIter =  m_FailoverJobs.m_FailoverJobsMap.find(policyId);
        if(failoverJobIter != m_FailoverJobs.m_FailoverJobsMap.end())
        {
            
            failoverjob = failoverJobIter->second;
            if(failoverjob.m_JobState == TASK_STATE_STARTED ||
                failoverjob.m_JobState == TASK_STATE_COMPLETED ||
                failoverjob.m_JobState == TASK_STATE_FAILED)
            {
                  bFind = true;
            }
            else
            {
                std::cout<<" .";
            }
        }
        else
        {
            std::cout<<" ."; 
        }
    }
    if(failoverjob.m_JobState == TASK_STATE_STARTED ||
        failoverjob.m_JobState == TASK_STATE_COMPLETED ||
        failoverjob.m_JobState == TASK_STATE_FAILED)
    {
        std::cout<<std::endl<<"UnPlanned failover is executing .";

        bool bRet = false;
        while( !bRet && !Controller::getInstance()->QuitRequested(5) )
        {
            DebugPrintf(SV_LOG_DEBUG,"Job status of policyId : %d is :%d\n",policyId,failoverjob.m_JobState);
            if(failoverjob.m_JobState == TASK_STATE_COMPLETED || 
                failoverjob.m_JobState == TASK_STATE_FAILED)
            {
                bRet = true;
                break;
            }
            else
            {
                std::cout<<" .";
                getFailoverJobs(m_FailoverJobs);
                std::map<SV_ULONG, FailoverJob>::iterator failoverJobIter =  m_FailoverJobs.m_FailoverJobsMap.find(policyId);
                if(failoverJobIter != m_FailoverJobs.m_FailoverJobsMap.end())
                {
                    failoverjob = failoverJobIter->second;
                }

            }
        }
        if ( m_appType == "ORACLE" || m_appType == "DB2" )
        {
            if(failoverjob.m_JobState == TASK_STATE_FAILED)
            { 
               if(true != CopySettingsForRerun(policyId))
               {
                   DebugPrintf(SV_LOG_ERROR,"Unable to copy settings for policyId : %d from backup file for re-run\n",policyId);
               }

               std::map<SV_ULONG, FailoverJob>::iterator failovrJobIter =  m_FailoverJobs.m_FailoverJobsMap.find(policyId);
               FailoverJob failovrjob;
               if(failovrJobIter != m_FailoverJobs.m_FailoverJobsMap.end())
               {
                   failovrjob = failovrJobIter->second;
                   failovrjob.m_JobState = TASK_STATE_NOT_STARTED;
                   DebugPrintf(SV_LOG_DEBUG,"Changed the Job status of policyId : %d to :%d\n",policyId,failovrjob.m_JobState);
                   failovrJobIter->second = failovrjob;
               }
               appConfiguratorPtr->SerializeFailoverJobs(m_FailoverJobs);
            }
        }

        std::cout<<std::endl;
        std::list<FailoverCommand> failoverCommands = failoverjob.m_FailoverCommands;
        std::list<FailoverCommand>::iterator failoverCommandsIter = failoverCommands.begin();
        std::string outputFileName;
        std::string groupIdStr;
        std::string instanceIdStr; 
        while(failoverCommandsIter != failoverCommands.end())
        {
            groupIdStr = boost::lexical_cast<std::string>(failoverCommandsIter->m_GroupId);
            instanceIdStr = boost::lexical_cast<std::string>(failoverCommandsIter->m_InstanceId);
            outputFileName = cacheDirPath ;
            outputFileName += "policy_" + policyIdStr + "_groupId_"+ groupIdStr + "_"+ instanceIdStr + ".log";
            PrintLogFile(outputFileName);
            std::cout<<std::endl;
            failoverCommandsIter++;
        }
    }
   
}

void FailoverCommandUtil::PrintSummary(FailoverPolicyData& failoverData)
{
    std::cout<<std::endl;
    std::cout<<"Recovery Options Summary:"<<std::endl<<std::endl;
    std::map<std::string,std::string>::iterator failoverDataIter =  failoverData.m_properties.begin();
    while(failoverDataIter != failoverData.m_properties.end())
    {
        std::cout<<std::setw(18);
        std::cout<<failoverDataIter->first;
        std::cout<<": "<<failoverDataIter->second<<std::endl;
        if(failoverDataIter->second.compare("CUSTOM") == 0)
        {
            std::map<std::string, RecoveryPoints> recoveryPoints =  failoverData.m_recoveryPoints; 
            std::map<std::string,RecoveryPoints>::iterator recoveryPointsIter =  recoveryPoints.begin();
            std::cout<<std::endl<<"CUSTOM RecoveryOptions :"<<std::endl<<std::endl;
            while(recoveryPointsIter != recoveryPoints.end())
            {
                
                std::cout<<std::setw(18);
                std::cout<<"Volume";
                std::cout<<": "<<recoveryPointsIter->first<<std::endl;
                std::map<std::string,std::string>::iterator recoveryPointPropsIter = recoveryPointsIter->second.m_properties.begin();
                while(recoveryPointPropsIter != recoveryPointsIter->second.m_properties.end())
                {
                    std::cout<<std::setw(18);
                    std::cout<<recoveryPointPropsIter->first;
                    std::cout<<": "<<recoveryPointPropsIter->second<<std::endl;
                    recoveryPointPropsIter++;
                }
                recoveryPointsIter++;
            }
        }
         failoverDataIter++;
    }

}

SVERROR FailoverCommandUtil::StopSVAgent()
{
	SVERROR svRet = SVS_OK;
	InmServiceStatus status;
	std::cout << "Stopping the svagent... ";
	if(getServiceStatus("svagents",status) == SVS_OK)
	{
	   if(status == INM_SVCSTAT_STOPPED)
	   {
		   std::cout << 
			   "<!> svagents service is in stopped state. User is responsible for starting the service after failover."
			   << std::endl;
	   }
	   else
	   {
		   svRet = StopService("svagents",120);
		   if(svRet != SVS_OK)
		   {
			   std::cout<<"Failed to stop the svagents. Stop the svagents manually and try again." << std::endl;
		   }
		   else
		   {
			   std::cout << "Successfully stopped svagents." << std::endl
				   <<"Info: Make sure that svagents service is running after failover. If not please start it manually." << std::endl;
			   g_bStartSvAgentsAfterFailover = true;
		   }
	   }
	}
	else
	{
	   std::cout<<"Failed to get the svagents service status. Please try again."<< std::endl;
	   svRet = SVS_FALSE;
	}
	return svRet;
}

SVERROR FailoverCommandUtil::StartSVAgent()
{
	SVERROR svRet = SVS_OK;
	if(g_bStartSvAgentsAfterFailover)
	{
		svRet = StartSvc("svagents");
		if(svRet != SVS_OK)
			std::cout << "INFO : Start the svagents service manually." << std::endl;
	}
	return svRet;
}
