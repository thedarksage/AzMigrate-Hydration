#include <sstream>
#include "appglobals.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ace/File_Lock.h>
#include <boost/shared_array.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#ifdef SV_WINDOWS
#include <atlbase.h>
#endif

#include "globs.h"
#include "util/common/util.h"
#include "appconfchangemediator.h"
#include "appexception.h"
#include "controller.h"
#include "inm_md5.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include "xmlunmarshal.h"
#include "xmlizeapplicationsettings.h"
#include "portablehelpers.h"
#include "appconfigurator.h"
#include "version.h"
#include "compatibility.h"
#include "host.h"

#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

#include "fingerprintmgr.h"
#include "csgetfingerprint.h"

#include "ConsistencySettings.h"

#define MAX_LOG_SIZE 10*1024*1024
ACE_Recursive_Thread_Mutex appconfigLock ;
AppAgentConfiguratorPtr AppAgentConfigurator::m_configuratorInstance ;


void WriteUpdateIntoFile(std::string update)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AppLocalConfigurator localConfig;
	std::string cxupdateFile;
	cxupdateFile = localConfig.getApplicatioAgentCachePath() ;
	cxupdateFile += "cxupdatecache.dat";
	std::ofstream out;
    std::string lockPath = cxupdateFile + ".lck";
    ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
    lock.acquire();
	 SV_ULONGLONG fileSize;
	if( getSizeOfFile(cxupdateFile, fileSize) == SVS_OK && fileSize >= MAX_LOG_SIZE )
	{
		std::string cxupdateCopyFile;
		cxupdateCopyFile = cxupdateFile + ".1";
		remove(cxupdateCopyFile.c_str());
		rename(cxupdateFile.c_str(), cxupdateCopyFile.c_str());
	}
	out.open(cxupdateFile.c_str(), std::ios::app);
    if (!out) 
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", cxupdateFile.c_str());
        return;
    }
    std::string timeStr =  convertTimeToString(time(NULL));
    out << timeStr;
    out << std::endl;
    out << update;
    out << std::endl;
    out.close();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


static bool IsPeerCertError(std::string& errString)
{
    /// http error 403 is added to handle a one time error hit in the testing
    /// this requires a revision if not occurs again 
    const std::string peerCertError[] = {
        "(60) - Peer certificate cannot be authenticated with given CA certificates",
        "(60) - Peer certificate cannot be authenticated with known CA certificates",
        "SSL peer certificate or SSH remote key was not OK",
        "server returned error: 403"};

    if (errString.find(peerCertError[0]) != std::string::npos ||
        errString.find(peerCertError[1]) != std::string::npos ||
        errString.find(peerCertError[2]) != std::string::npos ||
        errString.find(peerCertError[3]) != std::string::npos)
        return true;

    return false;
}

static bool HandleCertRollover()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);

    static uint32_t certRenewAttempts = 0;
    static boost::posix_time::ptime lastAttemptTime = boost::posix_time::second_clock::universal_time();
    const uint8_t minimumDurationBetweenAttempsInMins = 5 * (certRenewAttempts / 3);
    const uint8_t maxRenewAttempts = 12;

    boost::posix_time::ptime currentTime = boost::posix_time::second_clock::universal_time();
    uint64_t durationInMins = boost::posix_time::time_duration(currentTime - lastAttemptTime).minutes();

    if (certRenewAttempts > 0 && durationInMins < minimumDurationBetweenAttempsInMins)
        return false;

    if (certRenewAttempts > maxRenewAttempts)
    {
        DebugPrintf(SV_LOG_ERROR,
            "Reached maximum CS certificate auto-renew attempts. \
                        Manual intervention required to update the certificate.\n");
        return false;
    }

    certRenewAttempts++;
    lastAttemptTime = currentTime;

    std::string hostId;
    AppLocalConfigurator appLocalConfig;
    std::string passphrase = securitylib::getPassphrase();
    std::string ipAddress = GetCxIpAddress();
    SV_INT port = appLocalConfig.getHttp().port;
    std::string strPort = boost::lexical_cast<std::string>(port);

    bool verifyPassphraeOnly = false;
    bool useSsl = true;
    bool overwrite = false;

    std::string reply;
    bool bret = securitylib::csGetFingerprint(reply, hostId, passphrase, ipAddress, strPort, verifyPassphraeOnly, useSsl, overwrite);

    DebugPrintf(bret ? SV_LOG_DEBUG : SV_LOG_ERROR,
        "csgetfingerprint replied %s\n",
        reply.c_str());

    if (bret)
    {
        std::string prevCSFingerprint = g_fingerprintMgr.getFingerprint(ipAddress, port);

        g_fingerprintMgr.loadFingerprints(true);

        std::string newCSFingerprint = g_fingerprintMgr.getFingerprint(ipAddress, port);

        if (prevCSFingerprint != newCSFingerprint)
        {
            DebugPrintf(SV_LOG_ERROR,
                "This is not an error: The CS fingerprint is successfully refreshed.\n");
            certRenewAttempts = 0;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXIT %s \n", FUNCTION_NAME);
    return bret;
}

void ConfigureApplicationAgentProxy::test(void)
{

}

void ConfigureApplicationAgentProxy::GetApplicationSettings(ApplicationSettings& settings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, std::string> policyTimeStampMap ;
    std::list<ApplicationPolicy>::iterator policyIter = settings.m_policies.begin() ;

    while( policyIter != settings.m_policies.end() )
    {
        std::string policyId, timestamp ;
        std::map<std::string, std::string>& policyProps = policyIter->m_policyProperties ;
        if( policyProps.find("PolicyId") != policyProps.end() )
        {
            policyId = policyProps.find("PolicyId")->second ;
        }
        if( policyProps.find("Timestamp") != policyProps.end() )
        {
            timestamp = policyProps.find("Timestamp")->second ;
        }
        if( !policyId.empty() && !timestamp.empty() )
        {
            policyTimeStampMap.insert(std::make_pair(policyId, timestamp)) ;
        }
        policyIter++ ;
    }
    //std::string request = marshalCxCall(m_dpc,"GetApplicationSettings",policyTimeStampMap) ;
    Serializer sr(m_serializeType);
    const char *api = "GetApplicationSettings";
    const char *apitocall;
    const char *firstargtoapi;
    std::stringstream stream;
    setApiAndFirstArg(api, m_dpc.c_str(), &apitocall, &firstargtoapi);
    sr.Serialize(apitocall, firstargtoapi,policyTimeStampMap);
    std::string tempStr = marshalCxCall(apitocall, firstargtoapi,policyTimeStampMap);
    DebugPrintf(SV_LOG_DEBUG,"get app settings marshalled string %s \n ", tempStr.c_str());
    WriteUpdateIntoFile(tempStr);

    m_transport(sr);
    ApplicationSettings lSettings = sr.UnSerialize<ApplicationSettings>();
    
    stream <<  CxArgObj<ApplicationSettings>(lSettings);
    WriteUpdateIntoFile(stream.str());

    // Remove deprecated policies.
    {
        // A new policy instance is generated with new policy ID in case policy properties change.
        // So remove policy in old settings if same not present in new settings.
        // Remove only in case ApplicationType is CLOUD and PolicyType is Consistency.

        std::list<std::list<ApplicationPolicy>::iterator> listDeprecatedPolicies;

        std::list<ApplicationPolicy>::iterator itrOldPolicies = settings.m_policies.begin();
        for (/*empty*/; itrOldPolicies != settings.m_policies.end(); itrOldPolicies++)
        {
            std::map<std::string, std::string>::const_iterator citrOldApplicationType = itrOldPolicies->m_policyProperties.find("ApplicationType");
            std::map<std::string, std::string>::const_iterator citrOldPolicyType = itrOldPolicies->m_policyProperties.find("PolicyType");
            if ((citrOldApplicationType != itrOldPolicies->m_policyProperties.end()) &&
                (citrOldPolicyType != itrOldPolicies->m_policyProperties.end()) &&
                (0 == citrOldApplicationType->second.compare("CLOUD")) &&
                (0 == citrOldPolicyType->second.compare("Consistency")))
            {
                std::map<std::string, std::string>::const_iterator citrOldPolicyId = itrOldPolicies->m_policyProperties.find("PolicyId");
                std::string oldPolicyId;
                if (citrOldPolicyId != itrOldPolicies->m_policyProperties.end())
                {
                    oldPolicyId = citrOldPolicyId->second;
                }
                else
                {
                    continue;
                }

                std::list<ApplicationPolicy>::const_iterator citrNewPolicies = lSettings.m_policies.begin();
                for (/*empty*/; citrNewPolicies != lSettings.m_policies.end(); citrNewPolicies++)
                {
                    std::map<std::string, std::string>::const_iterator citrNewPolicyId = citrNewPolicies->m_policyProperties.find("PolicyId");
                    if (citrNewPolicyId != citrNewPolicies->m_policyProperties.end())
                    {
                        // break if old policy found in new settings
                        if (0 == oldPolicyId.compare(citrNewPolicyId->second))
                        {
                            break;
                        }
                    }
                }

                if (citrNewPolicies == lSettings.m_policies.end())
                {
                    listDeprecatedPolicies.push_back(itrOldPolicies);
                }
            }

        } // outer for loop

        // Remove deprecated policies.
        std::list<std::list<ApplicationPolicy>::iterator>::const_iterator citrDepricatedPolicy = listDeprecatedPolicies.begin();
        for (/*empty*/; citrDepricatedPolicy != listDeprecatedPolicies.end(); citrDepricatedPolicy++)
        {
            settings.m_policies.erase(*citrDepricatedPolicy);
        }

    } // End: Remove deprecated policies.

    std::list<ApplicationPolicy>::iterator outerPolIter = lSettings.m_policies.begin() ;
    std::list<SV_ULONG> deletePolicies;
    std::list<CxRequestCache> pendingUpdatesList;
    bool bGetPendingUpdates = true;
    while( outerPolIter != lSettings.m_policies.end() )
    {
        std::map<std::string, std::string>& outerPolProperties = outerPolIter->m_policyProperties ;
        std::string outerPolicyId ;
        if( outerPolProperties.find("PolicyId") != outerPolProperties.end() )
        {
            outerPolicyId = outerPolProperties.find("PolicyId")->second ;
        }
        else
        {
            outerPolIter ++ ;
            continue ;
        }
        std::list<ApplicationPolicy>::iterator innerPolIter = settings.m_policies.begin() ;

        while( innerPolIter != settings.m_policies.end() )
        {
            std::map<std::string, std::string>& InnerPolProperties = innerPolIter->m_policyProperties ;
            std::string innerPolicyId ;
            if( InnerPolProperties.find("PolicyId") != InnerPolProperties.end() )
            {
                innerPolicyId = InnerPolProperties.find("PolicyId")->second ;
            }
            if( innerPolicyId.compare(outerPolicyId) == 0 )
            {
                DebugPrintf(SV_LOG_DEBUG, "Recieved same policy %s again.. overwriting the local policy with new one\n", innerPolicyId.c_str());
                if(outerPolProperties.find("ScheduleType") != outerPolProperties.end())
                {
                    if(outerPolProperties.find("ScheduleType")->second.compare("-1") != 0)
                    {    
                        *innerPolIter = *outerPolIter ;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG,"PolicyId is removed from cx %s\n",innerPolicyId.c_str());
                        deletePolicies.push_back(boost::lexical_cast<SV_ULONG>(innerPolicyId));
                    }
                }
                break ;
            }
            innerPolIter++ ;
        }
        if( innerPolIter == settings.m_policies.end() )
        {
            if (bGetPendingUpdates)
            {
                CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
                cxUpdateCachePtr->GetPendingUpdates(pendingUpdatesList);
                bGetPendingUpdates = false;
            }

			SV_ULONG ulOuterPolicyId = boost::lexical_cast<SV_ULONG>(outerPolicyId);
			
            if (VerifyPendingUpdates(ulOuterPolicyId,pendingUpdatesList,outerPolIter->m_policyProperties))
            {
				DebugPrintf(SV_LOG_DEBUG, "Discarding the policy %s\n", outerPolicyId.c_str());
            }
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "Adding Policy %s\n", outerPolicyId.c_str());
				settings.m_policies.push_back(*outerPolIter);
			}
        }
        outerPolIter ++ ;
    }
    settings.m_AppProtectionsettings = lSettings.m_AppProtectionsettings ;
    settings.timeoutInSecs = lSettings.timeoutInSecs ;
    settings.remoteCxs = lSettings.remoteCxs;
    std::list<SV_ULONG>::iterator deletePoliciesIter =  deletePolicies.begin();
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    while(deletePoliciesIter != deletePolicies.end())
    {
        
        configuratorPtr->RemovePolicyFromCache(*deletePoliciesIter,true);
        deletePoliciesIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

// 
// Description: 
// 	  Verifies the pending updates for a give policyId.
// Perameters:
// 	  policyId         - policy Id
// 	  pendingUpdates   - list of pending updates available in cache
// 	  policyProperties - policy properties map.
// Return values: 
//    If the policyId found in pending updates then it returns true. Otheswise false.
// 
bool ConfigureApplicationAgentProxy::VerifyPendingUpdates(SV_ULONG policyId, 
	std::list<CxRequestCache>& pendingUpdates, 
	std::map<std::string, std::string>& policyProperties)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	bool bPolicyFound = false;
	std::string policyType;
	std::string applicationType;

	std::list<CxRequestCache>::const_iterator iterPendingUpdate = pendingUpdates.begin();
	while (iterPendingUpdate != pendingUpdates.end())
	{
		if (iterPendingUpdate->policyId == policyId)
		{
			//Get PolicyType for this policy
			std::map<std::string, std::string>::const_iterator iterPolicyProp = policyProperties.find("PolicyType");
			if (iterPolicyProp != policyProperties.end())
			{
				policyType = iterPolicyProp->second;
			}
			//Get ApplicationType for this policy
			iterPolicyProp = policyProperties.find("ApplicationType");
			if (iterPolicyProp != policyProperties.end())
			{
				applicationType = iterPolicyProp->second;
			}

			break;
		}
		iterPendingUpdate++;
	}

	//TODO: Currently the CloudRecovery and PrepareTarget policy types are considering for pending updates validation.
	//      Extend this validation to all the policy types.
	if (pendingUpdates.end() != iterPendingUpdate &&
		applicationType.compare("CLOUD") == 0 &&
		(policyType.compare("CloudRecovery") == 0 || policyType.compare("PrepareTarget") == 0)
		)
	{
		DebugPrintf(SV_LOG_DEBUG, "%s Policy with policy-Id %lu is found in pending updates cache.\n", policyType.c_str(), policyId);
		bPolicyFound = true;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return bPolicyFound;
}

AppAgentConfiguratorPtr AppAgentConfigurator::GetAppAgentConfigurator()
{
    return m_configuratorInstance  ;
}

AppAgentConfiguratorPtr AppAgentConfigurator::CreateAppAgentConfigurator(const SerializeType_t & serializetype,ConfigSource confSource)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if( m_configuratorInstance.get() == NULL )
    {
        m_configuratorInstance.reset( new AppAgentConfigurator(serializetype,confSource) ) ;
    }
    return m_configuratorInstance ;
}

AppAgentConfiguratorPtr AppAgentConfigurator::CreateAppAgentConfigurator(const SerializeType_t & serializetype)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppLocalConfigurator localConfigurator ;
    if( m_configuratorInstance.get() == NULL )
    {

        m_configuratorInstance.reset(new AppAgentConfigurator(serializetype, GetCxIpAddress(),
            localConfigurator.getHttp().port, 
            localConfigurator.getHostId() ) ) ;
    }    
    return m_configuratorInstance ;
}

AppAgentConfiguratorPtr AppAgentConfigurator::CreateAppAgentConfigurator( const std::string& ipAddress, const SV_INT& portNo, const std::string& hostId,const SerializeType_t &serializetype )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if( m_configuratorInstance.get() == NULL )
    {

        m_configuratorInstance.reset( new AppAgentConfigurator(serializetype, ipAddress, portNo, hostId) ) ;
    }    
    return m_configuratorInstance ;
}

AppAgentConfigurator::AppAgentConfigurator(const SerializeType_t & serializetype,enum ConfigSource confSource)
:m_configSource(confSource),m_serializeType(serializetype),m_apiWrapper(serializetype)
{
    m_cx.reset( new ApplicationCxProxy( m_localConfigurator.getHostId(), 
     m_apiWrapper,
     m_localConfigurator, 
     appconfigLock,
     m_serializeType)) ;

}


AppAgentConfigurator::AppAgentConfigurator(const SerializeType_t & serializetype,
                                           const std::string& ip, 
                                           int port,
                                           const std::string& hostId)
                                           :m_apiWrapper( ip, port, m_serializeType),
                                            m_serializeType(serializetype),
m_configSource(USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE)
{
    m_cx.reset( new ApplicationCxProxy( hostId, 
     m_apiWrapper,
     m_localConfigurator,
     appconfigLock,
     m_serializeType)) ;
}

void AppAgentConfigurator::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
    std::string dbPath = m_localConfigurator.getLogPathname() + "cxupdate.mdb";
    cxUpdateCachePtr->Init(dbPath);
}

// For single-node policy, remove ApplicationPolicy from ApplicationSettings and cache in consistencysettings.json
// For multi-node policy, remove ApplicationPolicy from ApplicationSettings and cache in consistencysettings.json only if
//    param MultiVMConsistencyPolicyScheduleInterval is present
void AppAgentConfigurator::CacheConsistencySettings(ApplicationSettings& appSettings)
{
    if (m_localConfigurator.isMasterTarget())
    {
        // Only source will receive consistency policy.
        return;
    }

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    try
    {
        // - check for single vm consistency policy
        // - check for multi vm consistency policy with param MultiVMConsistencyPolicyScheduleInterval
        // - construct ConsistencySettings object if policy found
        // - check cached file consistencysettings.json present and if found load cached file in memory 
        // - compare policy in cached file with policy in settings and update cache file if change identified
        // - In error case do not update cached file consistencysettings.json but continue removing policy
        //   from appSettings when file consistencysettings.json already present.

        static const std::string csCacheFile = m_localConfigurator.getConsistencySettingsCachePath();
        boost::system::error_code ec;
        const bool bcsCacheFileExists = boost::filesystem::exists(csCacheFile, ec);

        if (!bcsCacheFileExists)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: ConsistencySettings cached file \"%s\" does not exist. Error code: %d\n", FUNCTION_NAME, csCacheFile.c_str(), ec.value());
        }

        bool bMultiVmNew = false, bSingleVm = false;
        bool bUpdateCacheFile = true;

        std::list<ApplicationPolicy>::iterator itrAPolicy = appSettings.m_policies.begin();
        if (itrAPolicy == appSettings.m_policies.end())
        {
            // No policy present
            DebugPrintf(SV_LOG_ALWAYS, "%s: No policy received from CS.\n", FUNCTION_NAME);
            return;
        }

        // construct ConsistencySettings object
        CS::ConsistencySettings cs;

        std::list<std::list<ApplicationPolicy>::iterator> listPoliciesToRemove;
        bool bforceRemovePolicies = false;

        for (/*empty*/; itrAPolicy != appSettings.m_policies.end(); itrAPolicy++)
        {
            // *Note: 1) Never return from this loop as listPoliciesToRemove is updated at last in loop
            //           and Consistency Settings cache file need to be updated in any case
            //        2) Never break from this loop unless old flow identified (!bSingleVm && !bMultiVmNew)
            //           as listPoliciesToRemove is updated at last in loop
            const std::map<std::string, std::string>& policyProps = itrAPolicy->m_policyProperties;

            // Skip ApplicationType other than CLOUD and PolicyType other than Consistency
            std::map<std::string, std::string>::const_iterator citrApplicationType = policyProps.find("ApplicationType");
            std::map<std::string, std::string>::const_iterator citrPolicyType = policyProps.find("PolicyType");
            if ((citrApplicationType != policyProps.end()) &&
                (citrPolicyType != policyProps.end()) &&
                (0 != citrApplicationType->second.compare("CLOUD")) &&
                (0 != citrPolicyType->second.compare("Consistency")))
            {
                continue;
            }

            std::map<std::string, std::string>::const_iterator citrSInterval;
            std::map<std::string, std::string>::const_iterator citrCmdOptions = policyProps.find("CmdOptions");
            const std::string &cmdOptions = (citrCmdOptions != policyProps.end()) ? citrCmdOptions->second : std::string();
            if ((citrSInterval = policyProps.find("MultiVMConsistencyPolicyScheduleInterval")) != policyProps.end())
            {
                // Multi-vm new flow
                bMultiVmNew = true;
                try
                {
                    const int mvcscheduleInterval = boost::lexical_cast<uint32_t>(citrSInterval->second);

                    if (!cmdOptions.empty())
                    {   
                        if (std::string::npos != cmdOptions.find(OPT_CRASH_CONSISTENCY))
                        {
                            cs.m_CmdOptions = cmdOptions;
                            cs.m_CrashConsistencyInterval = mvcscheduleInterval;
                        }
                        else
                        {
                            cs.m_AppConsistencyInterval = mvcscheduleInterval;
                        }
                    }
                    else
                    {
                        // bug in CS - continue processing rest policies
                        DebugPrintf(SV_LOG_ERROR, "%s: CmdOptions missing for multi-node policy.\n", FUNCTION_NAME);
                        bUpdateCacheFile = false;
                    }
                }
                catch (const boost::bad_lexical_cast &e)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Caught exception in parsing param MultiVMConsistencyPolicyScheduleInterval: %s.\n", FUNCTION_NAME, e.what());
                    bUpdateCacheFile = false;
                }
                catch (...)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Caught unknown exception in parsing param MultiVMConsistencyPolicyScheduleInterval.\n", FUNCTION_NAME);
                    bUpdateCacheFile = false;
                }
            }
            else if ((citrSInterval = policyProps.find("ScheduleInterval")) != policyProps.end())
            {
                // This is single-vm or multi-vm old flow.
                // For single-vm always follow new flow
                // Note: ScheduleInterval is always 0 for multi-vm in old flow.
                try
                {
                    const int scheduleInterval = boost::lexical_cast<uint32_t>(citrSInterval->second);
                    if (0 != scheduleInterval)
                    {
                        bSingleVm = true;

                        // Check consistency type app/crash with param CmdOptions
                        // Note: param CmdOptions is always not present for app consistency in single-vm case for Windows
                        if (std::string::npos != cmdOptions.find(OPT_CRASH_CONSISTENCY))
                        {
                            // Case of crash consistency
                            cs.m_CmdOptions = cmdOptions;
                            cs.m_CrashConsistencyInterval = scheduleInterval;
                        }
                        else
                        {
                            // Case of app consistency
                            cs.m_AppConsistencyInterval = scheduleInterval;
                        }
                    }
                }
                catch (const boost::bad_lexical_cast &e)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Caught exception in parsing param ScheduleInterval: %s.\n", FUNCTION_NAME, e.what());
                    bUpdateCacheFile = false;
                }
                catch (...)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Caught unknown exception in parsing param ScheduleInterval.\n", FUNCTION_NAME);
                    bUpdateCacheFile = false;
                }

                if (!bSingleVm && !bMultiVmNew)
                {
                    // This is multi-vm old flow as param MultiVMConsistencyPolicyScheduleInterval is absent
                    // and param ScheduleInterval is zero
                    break;
                }
            }
            else
            {
                // Both MultiVMConsistencyPolicyScheduleInterval and ScheduleInterval missing in consistency policy settings
                // This could be bug in CS - continue processing rest policies
                std::string policyId;
                if (policyProps.find("PolicyId") != policyProps.end())
                {
                    policyId = policyProps.find("PolicyId")->second;
                }
                DebugPrintf(SV_LOG_ERROR, "%s: Both MultiVMConsistencyPolicyScheduleInterval and ScheduleInterval missing in consistency policy %s.\n", FUNCTION_NAME, policyId.c_str());

                if (bcsCacheFileExists)
                {
                    // Ignore policy
                    bUpdateCacheFile = false; // Keep cache unchanged
                    bforceRemovePolicies = true; // Remove policy in settings
                }
                else
                {
                    // Reboot post upgrade case
                    // Reset flags and let policy to be run by appagent - old design
                    // This policy will not be removed from settings.
                    bMultiVmNew = bSingleVm = false;
                    bUpdateCacheFile = false; // Skip creating cache with incomplete data
                    bforceRemovePolicies = false;
                }
            }

            // Note: always update listPoliciesToRemove at end of loop as till now we are not sure about this is new design.
            listPoliciesToRemove.push_back(itrAPolicy);

        } // for loop

        if (bUpdateCacheFile)
        {
            static std::string s_strcsInCacheFile;
            
            // Switch to new flow by caching ConsistencySettings in consistencysettings.json and removing m_policies from appSettings
            // Check cache file consistencysettings.json is already present and load in menory
            std::string strcs;
            try
            {
                std::string strcs = JSON::producer<CS::ConsistencySettings>::convert(cs);

                if ((0 == strcs.compare(s_strcsInCacheFile)))
                {
                    bUpdateCacheFile = false;
                }
                else
                {
                    if (s_strcsInCacheFile.empty() && bcsCacheFileExists)
                    {
                        // appagent restart case: load consistency settings in cache file for comparison
                        // Note: file read lock not required here as this thread is also the only cache writer thread
                        std::ifstream hcsInCacheFile(csCacheFile.c_str());
                        if (hcsInCacheFile.good())
                        {
                            s_strcsInCacheFile = std::string((std::istreambuf_iterator<char>(hcsInCacheFile)), (std::istreambuf_iterator<char>()));
                            hcsInCacheFile.close();
                            if (0 == strcs.compare(s_strcsInCacheFile))
                            {
                                DebugPrintf(SV_LOG_ALWAYS, "%s: ConsistencySettings received from CS matched with one in cached file: %s.\n", FUNCTION_NAME, strcs.c_str());
                                bUpdateCacheFile = false;
                            }
                            else
                            {
                                DebugPrintf(SV_LOG_ALWAYS, "%s: ConsistencySettings received from CS does not match with one in cached file.\n", FUNCTION_NAME);
                            }
                        }
                    }
                    if (bUpdateCacheFile)
                    {
                        DebugPrintf(SV_LOG_ALWAYS, "%s: ConsistencySettings changed.\n", FUNCTION_NAME);
                    }
                }

                if (bUpdateCacheFile || !bcsCacheFileExists)
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Caching ConsistencySettings: %s.\n", FUNCTION_NAME, strcs.c_str());
                    std::string csCacheLockFile(csCacheFile);
                    csCacheLockFile += ".lck";
                    if (!boost::filesystem::exists(csCacheLockFile, ec))
                    {
                        std::ofstream tmpLockFile(csCacheLockFile.c_str());
                        tmpLockFile.close();
                    }
                    boost::interprocess::file_lock csCacheFileLock(csCacheLockFile.c_str());
                    boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(csCacheFileLock);

                    try
                    {
                        std::ofstream csCacheFileHandle(csCacheFile.c_str(), std::ios::out | std::ios::trunc);
                        csCacheFileHandle << strcs;
                        csCacheFileHandle.close();
                        s_strcsInCacheFile = strcs;
                        DebugPrintf(SV_LOG_ALWAYS, "%s: Successfully cached ConsistencySettings: %s.\n", FUNCTION_NAME, strcs.c_str());
                    }
                    catch (std::exception &e)
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: Failed to cache ConsistencySettings to %s . Exception: %s.\n", FUNCTION_NAME, csCacheFile.c_str(), e.what());
                    }
                    catch (...)
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: Failed to cache ConsistencySettings to %s with unknown exception: %s.\n", FUNCTION_NAME, csCacheFile.c_str());
                    }
                }
            }
            catch (std::exception &e)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Caught exception: %s\n", FUNCTION_NAME, e.what());
            }
            catch (...)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Caught unknown exception.\n", FUNCTION_NAME);
            }
        }

        // Remove policy from appSettings only when file consistencysettings.json present.
        if ( ((bSingleVm || bMultiVmNew) && bcsCacheFileExists) || bforceRemovePolicies )
        {
            std::list<std::list<ApplicationPolicy>::iterator>::const_iterator citrPoliciesToRemove = listPoliciesToRemove.begin();
            for (/*empty*/; citrPoliciesToRemove != listPoliciesToRemove.end(); citrPoliciesToRemove++)
            {
                appSettings.m_policies.erase(*citrPoliciesToRemove);
            }
        }
    }
    catch (std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed  with exception: %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed  with unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

ApplicationSettings AppAgentConfigurator::GetApplicationSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    if( m_configSource == USE_ONLY_CACHE_SETTINGS )
    {
        m_settings = GetCachedApplicationSettings() ;
    }
    else if (m_configSource == FETCH_SETTINGS_FROM_CX || 
        m_configSource == USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE )
    {
        try
        {
            GetAppAgent().GetApplicationSettings(m_settings) ;
            m_lastTimeSettingsFetchedFromCx = ACE_OS::gettimeofday();
        }
        catch(std::exception& ex)
        {
            DebugPrintf(SV_LOG_ERROR, "Caught exception while getting application settings %s\n", ex.what()) ;

            std::string errStr = ex.what();
            if (IsPeerCertError(errStr))
                HandleCertRollover();

            m_settings = GetCachedApplicationSettings() ;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_WARNING, "Unable to get the settings from the cx\n") ;
            if( m_configSource == USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE )
            {
                DebugPrintf(SV_LOG_DEBUG, "Trying to get the settings from the cache file\n") ;
                m_settings = GetCachedApplicationSettings() ;
            }
        }
    }
    return m_settings ;
}

ApplicationSettings AppAgentConfigurator::GetCachedApplicationSettings()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;    
    ACE_Guard<ACE_Recursive_Thread_Mutex> lockguard( appconfigLock ) ;

    std::ifstream is;
    unsigned int length =0;

    std::string cachePath;
    std::string lockPath;
    //TODO :: Need to get the config cache path name from configurator..
    AppLocalConfigurator localConfigurator ;
    cachePath = localConfigurator.getAppSettingsCachePath() ;
    lockPath = cachePath + ".lck";

    // acquire the lock
    // PR#10815: Long Path support
    ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
    lock.acquire_read();

    // open the local store for read operation
    is.open(cachePath.c_str(), std::ios::in | std::ios::binary);
    if (!is.is_open())
    {
        return unmarshal<ApplicationSettings> (""); // throw an exception
    }

    // get length of file
    is.seekg (0, std::ios::end);
    length = is.tellg();
    is.seekg (0, std::ios::beg);

    // allocate memory:
    unsigned int buflen;
    INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned int>::Type(length) + 1, INMAGE_EX(length))
    boost::shared_array<char> buffer(new char[buflen]);
    if(!buffer) 
    {
        return unmarshal<ApplicationSettings> (""); // throw an exception
    }

    // read initialsettings as binary data
    is.read (buffer.get(),length);
    *(buffer.get() + length) = '\0';
    // close the handle
    is.close();
    lock.release();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    m_settings = unmarshal<ApplicationSettings> (buffer.get()) ;
    return m_settings ;
}
AppAgentConfigurator::~AppAgentConfigurator()
{
}

ConfigureApplicationAgent& AppAgentConfigurator::GetAppAgent()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    return *(m_cx->m_appAgentPtr) ;
}

ApplicationCxProxy& AppAgentConfigurator::GetCXAgent()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    return *m_cx ;
}
bool AppAgentConfigurator::postMirrorStatusInfo(const std::string& mirrInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    return GetAppAgent().postMirrorStatusInfo(mirrInfo);
}

bool AppAgentConfigurator::postMirrorInfo(const std::string& lmapInfo, const std::string& mirrInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    return GetAppAgent().postMirrorInfo(lmapInfo, mirrInfo);
}

bool ConfigureApplicationAgentProxy::postMirrorStatusInfo(const std::string& mirrInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool rv = false;
    do 
    {
        try 
        {
            Serializer sr(m_serializeType);
            const char *api = "postMirrorStatusInfo" ;
            sr.Serialize(m_dpc, api, mirrInfo ) ;
            m_transport( sr );
            rv = sr.UnSerialize<bool>();
            //std::string str = marshalCxCall(m_dpc,"postMirrorStatusInfo", mirrInfo) ;
            //rv = unmarshal<bool>(m_transport( str ));
        }
        catch( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
        }
        catch( const std::exception & e )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
        }
        catch( ... )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
        }
    } while(0);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return rv;
}
bool AppAgentConfigurator::postLunMapInfo(const std::string& lmapInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    return GetAppAgent().postLunMapInfo(lmapInfo);
}

bool ConfigureApplicationAgentProxy::postMirrorInfo(const std::string& lmapInfo, const std::string& mirrInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool rv = false;
    do 
    {
        try 
        {
         //   std::string str = marshalCxCall(m_dpc,"postMirrorInfo",lmapInfo, mirrInfo) ;
            Serializer sr(m_serializeType);
            const char *api = "postMirrorInfo";
            sr.Serialize(m_dpc, api, lmapInfo,mirrInfo);
            m_transport( sr );
            rv = sr.UnSerialize<bool>();
        }
        catch( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
        }
        catch( const std::exception & e )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
        }
        catch( ... )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
        }
    } while(0);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return rv;
}

bool ConfigureApplicationAgentProxy::postLunMapInfo(const std::string& lmapInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool rv = false;
    do 
    {
        try 
        {
            Serializer sr(m_serializeType);
            const char *api = "postLunMapInfo" ;
            sr.Serialize(m_dpc, api, lmapInfo ) ;
            m_transport( sr );
            rv = sr.UnSerialize<bool>();
            //std::string str = marshalCxCall(m_dpc,"postLunMapInfo",lmapInfo) ;
            //rv = unmarshal<bool>(m_transport( str ));
        }
        catch( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
        }
        catch( const std::exception & e )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
        }
        catch( ... )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
        }
    } while(0);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return rv;
}

bool AppAgentConfigurator::postForceMirrorDeleteInfo(const std::string& info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    return GetAppAgent().postForceMirrorDeleteInfo(info);
}

bool ConfigureApplicationAgentProxy::postForceMirrorDeleteInfo(const std::string& info)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool rv = false;
    do 
    {
        try 
        {
            Serializer sr(m_serializeType);
            const char *api = "postForceMirrorDeleteInfo" ;
            sr.Serialize(m_dpc, api, info ) ;
            m_transport( sr );
            rv = sr.UnSerialize<bool>();
            //std::string str = marshalCxCall(m_dpc,"postForceMirrorDeleteInfo",info) ;
            //rv = unmarshal<bool>(m_transport( str ));
        }
        catch( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
        }
        catch( const std::exception & e )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
        }
        catch( ... )
        {
            DebugPrintf(SV_LOG_ERROR, "\n%s encountered unknown exception.\n", FUNCTION_NAME);
        }
    } while(0);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return rv;
}
bool AppAgentConfigurator::isCxReachable(const std::string& ip, const SV_UINT& port)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool rv = true;
    std::map<std::string, std::string> policyTimeStampMap ;
    do 
    {
        try 
        {
            std::string biosId("");
            GetBiosId(biosId);

            std::string dpc = marshalCxCall("::getVxAgent", m_localConfigurator.getHostId(), biosId);

            TalWrapper tal;
			tal.SetDestIPAndPort(ip,port);
            std::string debug = tal(marshalCxCall(dpc,"GetApplicationSettings",policyTimeStampMap));
            ApplicationSettings settings = unmarshal<ApplicationSettings>( debug );
        }
        catch ( ContextualException& ce )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, ce.what());
        }
        catch( const std::exception & e )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, e.what());
        }
        catch ( ... )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered unknown exception.\n",
                FUNCTION_NAME);
        }
    } while(0);

    return rv;
}

void AppAgentConfigurator::switchToStandbyCx(const SerializeType_t & serializetype,ReachableCx reachableCx)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    m_configuratorInstance.reset( new AppAgentConfigurator(serializetype,reachableCx.ip, 
        reachableCx.port, 
        m_localConfigurator.getHostId() ) ) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
ACE_Time_Value AppAgentConfigurator::lastTimeSettingsGotFromCX()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    return m_lastTimeSettingsFetchedFromCx ;
}

ACE_Time_Value AppAgentConfigurator::lastTimeSettingsGotFromCache()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    return m_lastTimeSettingsFetchedFromCache ;
}
ConfigSource AppAgentConfigurator::getConfigSource()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    return m_configSource ;
}

void AppAgentConfigurator::UpdateDiscoveryInfo(const DiscoveryInfo& discoveryInfo,SV_ULONG policyId,SV_ULONGLONG instanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    RemovePolicyFromCache(policyId);
    GetAppAgent().UpdateDiscoveryInfo(discoveryInfo,policyId,instanceId) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ConfigureApplicationAgentProxy::UpdateDiscoveryInfo(const DiscoveryInfo& discoveryInfo,SV_ULONG policyId,SV_ULONGLONG instanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    InmPolicyUpdateStatus retValue = POLICY_UPDATE_FAILED;
 //   std::string str = marshalCxCall(m_dpc,"UpdateDiscoveryInfo",discoveryInfo) ;
    Serializer sr(m_serializeType);
    const char *api = "UpdateDiscoveryInfo";
    if (PHPSerialize == m_serializeType)
    {
        sr.Serialize(m_dpc, api, discoveryInfo);
    }
    else if (Xmlize == m_serializeType)
    {
        sr.Serialize(api, discoveryInfo);
    }
    CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
     CxRequestCache cxRequest;
    std::string str = sr.getMarshalledString();
    if(	policyId == 0 )
    {
        unsigned char current_hash[16];
        INM_MD5_CTX ctx;
        INM_MD5Init(&ctx);
        INM_MD5Update(&ctx, (unsigned char*)str.c_str(), str.size());				
        INM_MD5Final(current_hash, &ctx);
        if( memcmp(m_defaultDiscoveryHashp, current_hash, sizeof(current_hash)) != 0 )
        {
            DebugPrintf(SV_LOG_DEBUG, "There is a change in discovery info.\n ");
            if(PHPSerialize == m_serializeType)
            {
               
            cxRequest.policyId = policyId;
            cxRequest.instanceId = instanceId;
            cxRequest.updateString = str;
            cxRequest.status = CX_UPDATE_PENDING;
            cxRequest.noOfRetries = 1;
            cxUpdateCachePtr->AddCxUpdateRequestEx(cxRequest);
            //cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);
                //  WriteUpdateIntoFile(str);
                WriteUpdateIntoFile(sr.getMarshalledString());
            }
            m_transport( sr );
//            DebugPrintf(SV_LOG_DEBUG,"For Policy %ld response string %s\n",policyId,retValueStr.c_str());
            try
            {
                retValue = sr.UnSerialize<InmPolicyUpdateStatus>();
                cxRequest.status = CX_UPDATE_FAILED;
				if(retValue == POLICY_UPDATE_COMPLETED)
				{
					cxRequest.status = CX_UPDATE_SUCCESSFUL;
					inm_memcpy_s(m_defaultDiscoveryHashp, sizeof(m_defaultDiscoveryHashp), current_hash, sizeof(current_hash)); 
				}
                cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);
            }
            catch ( ContextualException& ce )
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, ce.what());
            }
            catch( const std::exception & e )
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered exception %s.\n",
                    FUNCTION_NAME, e.what());
            }
            catch ( ... )
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "\n%s encountered unknown exception.\n",
                    FUNCTION_NAME);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "There is no change in discovery info.\n ");
        }
    }
    else
    {
        CxRequestCache cxRequest;
        cxRequest.policyId = policyId;
        cxRequest.instanceId = instanceId;
        cxRequest.updateString = str;
        cxRequest.status = CX_UPDATE_PENDING;
        cxRequest.noOfRetries = 1;
        cxUpdateCachePtr->AddCxUpdateRequestEx(cxRequest);
        //cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);
        //WriteUpdateIntoFile(str);
        WriteUpdateIntoFile(sr.getMarshalledString());
        m_transport( sr );	
        //DebugPrintf(SV_LOG_DEBUG,"For Policy %ld response string %s\n",policyId,retValueStr.c_str());
        try
        {
            retValue = sr.UnSerialize<InmPolicyUpdateStatus>();
            if(retValue == POLICY_UPDATE_DELETED)
            {
                AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
                AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
                configuratorPtr->RemovePolicyFromCache(policyId, true);
                schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_DELETED) ;
            }
            cxRequest.status = CX_UPDATE_SUCCESSFUL;
            cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);
        }
        catch ( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, ce.what());
        }
        catch( const std::exception & e )
        {
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, e.what());
        }
        catch ( ... )
        {
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered unknown exception.\n",
                FUNCTION_NAME);
        }


    }
    DebugPrintf(SV_LOG_DEBUG, "Discovery string %s\n", str.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
/*
void ConfigureApplicationAgentProxy::ConsistencyPolicyUpdate(SV_ULONG policyId,std::string strErrorCode,std::string strLog)
{
std::string str = marshalCxCall(m_dpc,"UpdateConsistencyPolicy",policyId,strErrorCode,strLog) ;
WriteUpdateIntoFile(str);

CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
CxRequestCache cxRequest;
cxRequest.policyId = policyId;
cxRequest.updateString = str;
cxRequest.status = CX_UPDATE_PENDING;
cxRequest.noOfRetries = 1;
cxUpdateCachePtr->AddCxUpdateRequest(cxRequest);
m_transport(str) ;

cxRequest.status = CX_UPDATE_SUCCESSFUL;
cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);


DebugPrintf(SV_LOG_DEBUG, "Readinesscheck Info string is %s\n", str.c_str()) ;
DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
void AppAgentConfigurator::ConsistencyPolicyUpdate(SV_ULONG policyId,std::string strErrorCode,std::string strLog)
{
DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
AutoGuard lock( m_lockSettings ) ;
removeRunOncePolicyIfExists(policyId);
GetAppAgent().ConsistencyPolicyUpdate(policyId,strErrorCode,strLog) ;
}
*/
void ConfigureApplicationAgentProxy::PolicyUpdate(SV_ULONG policyId, SV_ULONGLONG instanceId, std::string strErrorCode, std::string strLog, std::string updateMsg, bool noLogUpload)
{
    strLog = "" ;
    std::string str;
    CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
    CxRequestCache cxRequest;
    Serializer sr(m_serializeType);
    const char *api = "UpdatePolicy";

    if(updateMsg.empty())
    {
        str = marshalCxCall(m_dpc,"UpdatePolicy",policyId,instanceId,strErrorCode,strLog) ;
		if (PHPSerialize == m_serializeType)
		{
			sr.Serialize(m_dpc, api, policyId, instanceId, strErrorCode, strLog);

			// Add update to the pending updates cache database. This entry will be removed on successful policy update.
			cxRequest.policyId = policyId;
			cxRequest.instanceId = instanceId;
			cxRequest.updateString = str;
			//
			// Set CX_UPDATE_PENDING_NOLOGUPLOAD status if the policy update is "InProgress" or the policy update does not do log-upload.
			// This flag will be verified when the policy update is retried from pending updates cache. 
			// If CX_UPDATE_PENDING_NOLOGUPLOAD status is set the the log-upload will not be attempted for this policy update.
			//
			cxRequest.status = (noLogUpload || !strErrorCode.compare("InProgress")) ? CX_UPDATE_PENDING_NOLOGUPLOAD : CX_UPDATE_PENDING;
			cxRequest.noOfRetries = 1;
			cxUpdateCachePtr->AddCxUpdateRequestEx(cxRequest);
		}
        else if (Xmlize == m_serializeType)
        {
            sr.Serialize(api, policyId,instanceId,strErrorCode,strLog, updateMsg);
        }
    }
    else
    {
        str = marshalCxCall(m_dpc, "UpdatePolicy", policyId, instanceId, strErrorCode, updateMsg) ;
        if (PHPSerialize == m_serializeType)
        {
            sr.Serialize(m_dpc, api, policyId,instanceId,strErrorCode,updateMsg) ;

			// Add update to the pending updates cache database. This entry will be removed on successful policy update.
			cxRequest.policyId = policyId;
			cxRequest.instanceId = instanceId;
			cxRequest.updateString = str;
			//
			// Set CX_UPDATE_PENDING_NOLOGUPLOAD status if the policy update is "InProgress" or the policy update does not do log-upload.
			// This flag will be verified when the policy update is retried from pending updates cache. 
			// If CX_UPDATE_PENDING_NOLOGUPLOAD status is set the the log-upload will not be attempted for this policy update.
			//
			cxRequest.status = (noLogUpload || !strErrorCode.compare("InProgress")) ? CX_UPDATE_PENDING_NOLOGUPLOAD : CX_UPDATE_PENDING;
			cxRequest.noOfRetries = 1;
			cxUpdateCachePtr->AddCxUpdateRequestEx(cxRequest);
        }
        else if (Xmlize == m_serializeType)
        {
            sr.Serialize(api, policyId,instanceId,strErrorCode,strLog, updateMsg);
        }
    }
    WriteUpdateIntoFile(sr.getMarshalledString()) ;
    InmPolicyUpdateStatus retValue = POLICY_UPDATE_FAILED;
    try
    {
        m_transport(sr) ;
        retValue = sr.UnSerialize<InmPolicyUpdateStatus>( );

		DebugPrintf(SV_LOG_DEBUG, "Policy-Update return code: %d\n",retValue);

		//
		//On Update Failure: POLICY_UPDATE_FAILED
		//------------------
		//   If policy update return fail status then retain the update entry in pending updates cache database.
		//   Configurator thread will retry the failed pending updates until the updates successfuly posted to CS.
		//
		//On Update Success:  POLICY_UPDATE_COMPLETED (or) POLICY_UPDATE_DELETED (or) POLICY_UPDATE_DUPLICATE
		//------------------
		//   If any of the above three return codes are returned then delete the update entry from pending updates cache database.
		//   If CS successfuly inserts the policy update into its DB then the POLICY_UPDATE_COMPLETED will be returned.
		//   If CS finds that the same update is already reported then it returns POLICY_UPDATE_DUPLICATE.
		//   If policy entry is removed from CS by the time the policy update reported then it returns POLICY_UPDATE_DELETED.
		//
		if (retValue != POLICY_UPDATE_FAILED)
		{
			std::stringstream stream;
			stream << cxArg<InmPolicyUpdateStatus>(retValue);
			WriteUpdateIntoFile(stream.str());
			if (retValue == POLICY_UPDATE_DELETED)
			{
				AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
				AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
				configuratorPtr->RemovePolicyFromCache(policyId, true);
				schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_DELETED);
			}
			if (PHPSerialize == m_serializeType)
			{
				cxRequest.status = CX_UPDATE_SUCCESSFUL;
				cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);
			}
			unsigned long bytesSent;
			if (!noLogUpload && strErrorCode.compare("InProgress") != 0)
			{
				if (PHPSerialize == m_serializeType && uploadFileToCX(policyId, 0, instanceId, bytesSent) != SVS_OK)
				{
					DebugPrintf(SV_LOG_DEBUG, "Faile to upload the file for policy: %d \n", policyId);
				}
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Policy-Update failed for the policy %lu\n",policyId);
		}
    }
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch( const std::exception & e )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
void AppAgentConfigurator::PolicyUpdate(SV_ULONG policyId, SV_ULONGLONG instanceId, std::string strErrorCode, std::string strLog, std::string updateMsg, bool noLogUpload)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    if( strErrorCode.compare("InProgress") != 0)
    {
        RemovePolicyFromCache(policyId);
    }
    //Check for internal policy
    if (policyId == RESUME_REPLICATION_POLICY_INTERNAL)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Skipping policy update as policy id : %llu is internal.\n", FUNCTION_NAME, policyId) ;
    }
    else
    {
        GetAppAgent().PolicyUpdate(policyId,instanceId,strErrorCode,strLog, updateMsg, noLogUpload) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
/*
void ConfigureApplicationAgentProxy::PrepareTargetPolicyUpdate(SV_ULONG policyId, std::string strErrorCode,std::string strLog)
{
std::string str = marshalCxCall(m_dpc,"UpdatePrepareTarget",policyId,strErrorCode,strLog) ;
WriteUpdateIntoFile(str);

CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
CxRequestCache cxRequest;
cxRequest.policyId = policyId;
cxRequest.updateString = str;
cxRequest.status = CX_UPDATE_PENDING;
cxRequest.noOfRetries = 1;
cxUpdateCachePtr->AddCxUpdateRequest(cxRequest);
m_transport(str) ;

cxRequest.status = CX_UPDATE_SUCCESSFUL;
cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);

DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void AppAgentConfigurator::PrepareTargetPolicyUpdate (SV_ULONG policyId, std::string strErrorCode,std::string strLog)
{
DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
AutoGuard lock( m_lockSettings ) ;
removeRunOncePolicyIfExists(policyId);

GetAppAgent().PrepareTargetPolicyUpdate(policyId,strErrorCode,strLog) ;
}
*/
void ConfigureApplicationAgentProxy::persistAppSettings(const ApplicationSettings& appsettings)
{
    std::string str = marshalCxCall(m_dpc,"PersistAppSettings",appsettings) ;
    WriteUpdateIntoFile(str);
    DebugPrintf(SV_LOG_DEBUG, "Discovery string %s\n", str.c_str()) ;

}

void AppAgentConfigurator::persistAppSettings(const ApplicationSettings& appsettings)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    GetAppAgent().persistAppSettings(appsettings) ;
}
void AppAgentConfigurator::UpdateReadinessCheckInfo(const ReadinessCheckInfo& readinessCheckInfo,SV_ULONG policyId,SV_ULONGLONG instanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    std::list<ReadinessCheckInfo> readinesscheckInfos ;
    readinesscheckInfos.push_back(readinessCheckInfo) ;
    RemovePolicyFromCache(policyId);
    UpdateReadinessCheckInfo(readinesscheckInfos,policyId,instanceId) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
SVERROR AppAgentConfigurator::perisitClusterResourceInfo(const std::list<RescoureInfo>& resourceInfoList) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    clusterResourceInfo objclusterResourceInfo ;
    objclusterResourceInfo.resourceListInfo = resourceInfoList;
    SVERROR bRet = SVS_FALSE ;
    try
    {
        AppLocalConfigurator configurator ;
        std::ofstream out;
        std::string cachePath;
		cachePath = configurator.getApplicatioAgentCachePath() ;
		cachePath += "clusterResrc.dat";
        out.open(cachePath.c_str(), std::ios::trunc);
        if (!out.is_open()) 
        {
            throw AppException("File could not be opened in rw mode\n") ;
        }
        out <<  CxArgObj<clusterResourceInfo>(objclusterResourceInfo);
        out.flush() ;
        out.close();
        bRet = SVS_OK;
    }
    catch(AppException& exception)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to marshalling clusterResourceInfo %s\n", exception.to_string().c_str()) ;
    }
    catch(std::exception& exception)
    {
        DebugPrintf(SV_LOG_ERROR, "caught exception while serializing clusterResourceInfo %s\n", exception.what()) ;
    }
    catch(...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to serialize the strings.\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}



void AppAgentConfigurator::UpdateReadinessCheckInfo(const std::list<ReadinessCheckInfo>& readinessCheckInfos,SV_ULONG policyId,SV_ULONGLONG instanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    RemovePolicyFromCache(policyId) ;
    GetAppAgent().UpdateReadinessCheckInfo(readinessCheckInfos,policyId,instanceId) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ConfigureApplicationAgentProxy::UpdateReadinessCheckInfo(const std::list<ReadinessCheckInfo>& readinessCheckInfos,SV_ULONG policyId,SV_ULONGLONG instanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    InmPolicyUpdateStatus retValue = POLICY_UPDATE_DELETED;
    std::list<ReadinessCheckInfo> readinessCheckInfoList ;
    ReadinessCheckInfoUpdate readinessCheckInfoUpdate ;
    readinessCheckInfoUpdate.m_readinessCheckInfos = readinessCheckInfos ;
    //std::string str = marshalCxCall(m_dpc, "UpdateReadinessCheckInfo", readinessCheckInfoUpdate) ;
    Serializer sr(m_serializeType);
    std::string str;
    const char *api = "UpdateReadinessCheckInfo";
    CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
    CxRequestCache cxRequest;
    if (PHPSerialize == m_serializeType)
    {
        sr.Serialize(m_dpc, api, readinessCheckInfoUpdate);
        str = sr.getMarshalledString();
        WriteUpdateIntoFile(str);
        
    cxRequest.policyId = policyId;
    cxRequest.instanceId = instanceId;
    cxRequest.updateString = str;
    cxRequest.status = CX_UPDATE_PENDING;
    cxRequest.noOfRetries = 1;
    cxUpdateCachePtr->AddCxUpdateRequestEx(cxRequest);
    }
    else if (Xmlize == m_serializeType)
    {
        sr.Serialize(api, readinessCheckInfoUpdate);
    }
    m_transport( sr );
    try
    {
        retValue = sr.UnSerialize<InmPolicyUpdateStatus>();
        if(retValue == POLICY_UPDATE_DELETED)
        {
            AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
            configuratorPtr->RemovePolicyFromCache(policyId, true);
            AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
            schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_DELETED) ;
        }
        if (PHPSerialize == m_serializeType)
        {
            cxRequest.status = CX_UPDATE_SUCCESSFUL;
            cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);
        }
    }
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch( const std::exception & e )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "Readinesscheck Info string is %s\n", str.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


SVERROR AppAgentConfigurator::UnserializeClusterResourceInfo(std::list<RescoureInfo>& listClusterResources)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    SVERROR bRet = SVS_FALSE ;
    std::ifstream inFile;
    unsigned int length =0;
    clusterResourceInfo objclusterResourceInfo;
    std::string cachePath;
	AppLocalConfigurator configurator;
	cachePath = configurator.getApplicatioAgentCachePath() ;
	cachePath += "clusterResrc.dat";

    std::string lockPath;
    SV_ULONG fileSize = 0 ;
    inFile.open(cachePath.c_str(), std::ios::in) ;
    if( inFile.good() )
    {
        inFile.seekg(0, std::ios::end) ;
        fileSize = inFile.tellg() ;
        inFile.close() ;
    }
    if( fileSize != 0 )
    {
        inFile.open(cachePath.c_str(), std::ios::in);
        if (!inFile.is_open())
        {
            throw AppException("File could not be opened in read mode\n") ;
        }

        // get length of file
        inFile.seekg (0, std::ios::end);
        length = inFile.tellg();
        inFile.seekg (0, std::ios::beg);

        // allocate memory:
        unsigned int buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned int>::Type(length) + 1, INMAGE_EX(length))
        boost::shared_array<char> buffer(new char[buflen]);
        if(!buffer) 
        {
            throw AppException("couldnot allocate enough memory for buffer\n") ;
            inFile.close();
        }

        // read initialsettings as binary data
        inFile.read (buffer.get(),length);
        *(buffer.get() + length) = '\0';
        // close the handle
        inFile.close();
        try
        {
            objclusterResourceInfo =  unmarshal<clusterResourceInfo> (buffer.get());
            listClusterResources = objclusterResourceInfo.resourceListInfo;
            bRet = SVS_OK ;
        }
        catch(AppException& exception) // here control will never come because exception thrown is not of appexception type
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal clusterResourceInfo %s\n", exception.to_string().c_str()) ;
        }
        catch(std::exception ex)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal clusterResourceInfo %s\n", ex.what()) ;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Unknown exception while unserializing the clusterResourceInfo information\n") ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "No infomration about cluster Resources exists.\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR AppAgentConfigurator::getApplicationPolicyPropVal(SV_ULONG policyId,const std::string & policy_prop, std::string & prop_value)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    SVERROR ret = SVS_FALSE ;
	prop_value = "";
	
	std::list<ApplicationPolicy>& policiesFromSettings = m_settings.m_policies ;
    std::list<ApplicationPolicy>::iterator policyIter = policiesFromSettings.begin() ;
    std::string policyIdStr = boost::lexical_cast<std::string> (policyId) ;
    DebugPrintf(SV_LOG_DEBUG, "PolicyId param after convering to string %s\n", policyIdStr.c_str()) ;
    while( policyIter != policiesFromSettings.end() )
    {
        std::map<std::string, std::string>& policyProps = policyIter->m_policyProperties ;
        if( policyProps.find("PolicyId") != policyProps.end() )
        {
            DebugPrintf(SV_LOG_DEBUG, "Policy Id %s\n", policyProps.find("PolicyId")->second.c_str()) ;
            if( policyProps.find("PolicyId")->second.compare( policyIdStr ) == 0 )
            {
                if( policyProps.find(policy_prop) != policyProps.end() )
                {
					DebugPrintf(SV_LOG_DEBUG, "%s %s\n",policy_prop.c_str(), policyProps.find(policy_prop)->second.c_str()) ;
                    prop_value = policyProps.find(policy_prop)->second ;
                    ret  = SVS_OK ;
                    break ;
                }
                else
                {
					DebugPrintf(SV_LOG_ERROR, "%s is not present for the policy %s", policy_prop.c_str(), policyIdStr.c_str()) ;
                }
            }
        }
        policyIter++ ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return ret;
}
SVERROR AppAgentConfigurator::getApplicationPolicyType(SV_ULONG policyId, std::string& policyType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    SVERROR bRet = SVS_FALSE ;
    if( policyId == 0 )
    {
        policyType = "Discovery" ;
        bRet  = SVS_OK ;
    }
    else if( policyId == RESUME_REPLICATION_POLICY_INTERNAL )
    {
        policyType = "RcmResumeReplication" ;
        bRet  = SVS_OK ;
    }
    else
    {
        std::list<ApplicationPolicy>& policiesFromSettings = m_settings.m_policies ;
        std::list<ApplicationPolicy>::iterator policyIter = policiesFromSettings.begin() ;
        std::string policyIdStr = boost::lexical_cast<std::string> (policyId) ;
        DebugPrintf(SV_LOG_DEBUG, "Requested policy Id after convering to string %s\n", policyIdStr.c_str()) ;
        while( policyIter != policiesFromSettings.end() )
        {
            std::map<std::string, std::string>& policyProps = policyIter->m_policyProperties ;
            if( policyProps.find("PolicyId") != policyProps.end() )
            {
                DebugPrintf(SV_LOG_DEBUG, "Policy Id %s\n", policyProps.find("PolicyId")->second.c_str()) ;
                if( policyProps.find("PolicyId")->second.compare( policyIdStr ) == 0 )
                {
                    if( policyProps.find("PolicyType") != policyProps.end() )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Policy type %s\n", policyProps.find("PolicyType")->second.c_str()) ;
                        policyType = policyProps.find("PolicyType")->second ;
                        bRet  = SVS_OK ;
                        break ;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, "PolicyType is not present for the policyId %s", policyIdStr.c_str()) ;
                    }
                }
            }
            policyIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
void AppAgentConfigurator::findInstancesForDefaultDiscovery(const std::string& appType, 
                                                            std::list<std::string>& instanceList,
                                                            std::map<std::string, std::list<std::string> > instancesMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    std::list<ApplicationPolicy>& policiesFromSettings = m_settings.m_policies ;
    std::list<ApplicationPolicy>::iterator policyIter = policiesFromSettings.begin() ;
    while( policyIter != policiesFromSettings.end() )
    {
        const std::map<std::string, std::string>& propertiesMap = policyIter->m_policyProperties;
        if( propertiesMap.find("PolicyType")->second.compare("Discovery") == 0 )
        {
            bool hostLevelPolicy = false;
            bool appTypeLevelPolicy = false ;
            bool instanceLevelPolicy = false ;
            std::string applicationType ;
            std::string instanceName ;
            if( propertiesMap.find("ApplicationType") == propertiesMap.end() )
            {
                hostLevelPolicy = true ;
            }
            else
            {
                applicationType = propertiesMap.find("ApplicationType")->second ;
                if( applicationType.compare("0") == 0 )
                {
                    applicationType = "" ;
                    hostLevelPolicy = true ;
                }
                if( propertiesMap.find("InstanceName") != propertiesMap.end() && 
                    propertiesMap.find("InstanceName")->second.compare("0") != 0 )
                {
                    instanceLevelPolicy = true ;
                    instanceName = propertiesMap.find("InstanceName")->second ;
                }
                else
                {
                    appTypeLevelPolicy = true ;
                }
            }
            if( hostLevelPolicy )
            {
                instanceList.clear() ;
                break ;
            }
            else
            {
                std::list<std::string> appVersionInstances ;
                if( strstr(applicationType.c_str(), appType.c_str()) != NULL && 
                    instancesMap.find(applicationType) != instancesMap.end() )
                {
                    if( appTypeLevelPolicy )
                    {
                        appVersionInstances = instancesMap.find(applicationType)->second ;
                    }
                    else
                    {
                        appVersionInstances.push_back(instanceName) ;
                    }
                }
                std::list<std::string>::iterator instanceNameIter = instanceList.begin() ;
                while( instanceNameIter != instanceList.end() )
                {
                    if( std::find(appVersionInstances.begin(), appVersionInstances.end(), *instanceNameIter) != appVersionInstances.end() )
                    {
                        std::list<std::string>::iterator tempIter = instanceNameIter ;
                        instanceNameIter++ ;
                        instanceList.erase(tempIter) ;
                        continue ;
                    }
                    instanceNameIter++ ;
                }
            }
        }
        policyIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool AppAgentConfigurator::getApplicationPolicies(SV_ULONG policyId,
                                                  std::string appType,
                                                  ApplicationPolicy& policy)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    bool isValidPolicy = false ;
    if( policyId == 0 )
    {
        std::map<std::string, std::string>& policyPropsMap = policy.m_policyProperties ;
        policyPropsMap.insert(std::make_pair("PolicyType", "Discovery")) ;
        policyPropsMap.insert(std::make_pair("PolicyId", "0")) ;
        isValidPolicy = true ;
    }
    else if( policyId == RESUME_REPLICATION_POLICY_INTERNAL )
    {
        std::map<std::string, std::string>& policyPropsMap = policy.m_policyProperties ;
        policyPropsMap["PolicyType"] = "RcmResumeReplication" ;
        policyPropsMap["PolicyId"] = boost::lexical_cast<std::string>(policyId) ;
        isValidPolicy = true ;
    }
    else
    {
        std::list<ApplicationPolicy>& policiesFromSettings = m_settings.m_policies ;
        std::list<ApplicationPolicy>::iterator policyIter = policiesFromSettings.begin() ;
        std::string policyIdStr = boost::lexical_cast<std::string>(policyId) ;
        while( policyIter != policiesFromSettings.end() )
        {
            std::string policyIdFromSettings, appTypeFromSettings = "" ;
            const std::map<std::string, std::string>& propsMap = policyIter->m_policyProperties ;
            if( propsMap.find("PolicyId") != propsMap.end() )
            {
                policyIdFromSettings = propsMap.find("PolicyId")->second ;
            }
            if( policyIdFromSettings.compare(policyIdStr) == 0 )
            {
                if( propsMap.find("ApplicationType") != propsMap.end() )
                {
                    appTypeFromSettings = propsMap.find("ApplicationType")->second ;
                } 
                if( appTypeFromSettings.compare("0") == 0 || 
                    appTypeFromSettings.compare("") == 0 )
                {
                    appTypeFromSettings = "" ;
                    isValidPolicy = true ;    
                }
                else
                {
                    boost::algorithm::to_upper(appTypeFromSettings) ;                
                    size_t index = appTypeFromSettings.find(appType);
                    if( index != std::string::npos )
                    {
                        isValidPolicy = true ;
                    }
                }
                if( isValidPolicy )
                {
                    policy = *policyIter ;
                    std::map<std::string, std::string>& propsMap = policy.m_policyProperties ;
                    if( propsMap.find("InstanceName") != propsMap.end() && 
                        propsMap.find("InstanceName")->second.compare("0") == 0)
                    {
                        propsMap.erase(propsMap.find("InstanceName")) ;
                    }
                    if( propsMap.find("VirtualServerName") != propsMap.end() &&
                        propsMap.find("VirtualServerName")->second.compare("0") == 0)
                    {
                        propsMap.erase(propsMap.find("VirtualServerName")) ;
                    }
                    if( propsMap.find("ConsistencyCmd") != propsMap.end() &&
                        propsMap.find("ConsistencyCmd")->second.compare("0") == 0)
                    {
                        propsMap.erase(propsMap.find("ConsistencyCmd")) ;
                    }
                    if( appTypeFromSettings.compare("") == 0 && 
                        propsMap.find("ApplicationType") != propsMap.end())
                    {
                        propsMap.erase(propsMap.find("ApplicationType")) ;
                    }
			        if( propsMap.find("SourceHostName") != propsMap.end() && 
                        propsMap.find("SourceHostName")->second.compare("0") == 0)
                    {
                        propsMap.erase(propsMap.find("SourceHostName")) ;
                    }
			        if( propsMap.find("TargetHostName") != propsMap.end() && 
                        propsMap.find("TargetHostName")->second.compare("0") == 0)
                    {
                        propsMap.erase(propsMap.find("SourceHostName")) ;
                    }
			        if( propsMap.find("TgtVirtualServerName") != propsMap.end() && 
                        propsMap.find("TgtVirtualServerName")->second.compare("0") == 0)
                    {
                        propsMap.erase(propsMap.find("TgtVirtualServerName")) ;
                    }
			        if( propsMap.find("SrcVirtualServerName") != propsMap.end() && 
                        propsMap.find("SrcVirtualServerName")->second.compare("0") == 0)
                    {
                        propsMap.erase(propsMap.find("SrcVirtualServerName")) ;
                    }
                    break ;
                }
            }
            policyIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return isValidPolicy ;
}


void AppAgentConfigurator::SerializeApplicationSettings(const ApplicationSettings& settings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

    try
    {
        AppLocalConfigurator configurator ;
        std::ofstream out;
        std::string cachePath;
        cachePath = configurator.getAppSettingsCachePath() ;
        std::string lockPath = cachePath + ".lck";

        // PR#10815: Long Path support
        ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        lock.acquire();
        out.open(cachePath.c_str(), std::ios::trunc);
        if (!out) 
        {
            return;
        }
        out <<  CxArgObj<ApplicationSettings>(settings);
    } 
    catch(AppException& exception) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to cache the Application settings %s\n", exception.to_string().c_str());
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to cache the Application settings %s\n", ex.what());
    }
    catch(...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to cache the Application settings.\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void AppAgentConfigurator::start(ConfigChangeMediatorPtr configChangeMediatorPtr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    if( configChangeMediatorPtr.get() == NULL )
    {
        DebugPrintf(SV_LOG_ERROR, "cannot start the config change finder. Un-initialized ConfigChangeMediator object passed\n") ;
        throw AppException("Failed to start the config change mediator\n") ;
    }
    if( -1 == configChangeMediatorPtr->open() )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to start ConfigChangeMediator thread %d\n", ACE_OS::last_error()) ;
        throw AppException("Failed to start ConfigChangeMediator thread\n") ;
    }
}



SVERROR AppAgentConfigurator::getApplicationType(SV_ULONG policyId, std::string& AppType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    if(policyId == RESUME_REPLICATION_POLICY_INTERNAL )
    {
        AppType = "CLOUD" ;
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
        return SVS_OK ;
    }

    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    SVERROR bRet = SVS_FALSE ;
    std::string policyIdStr = boost::lexical_cast<std::string>(policyId) ;
    std::list<ApplicationPolicy>& policiesFromSettings = m_settings.m_policies ;
    std::list<ApplicationPolicy>::iterator policyIter = policiesFromSettings.begin() ;  

    while( policyIter != policiesFromSettings.end() )
    {
        std::map<std::string, std::string>& policyProps = policyIter->m_policyProperties ;
        std::string policyIdfromSettings ;
        if( policyProps.find("PolicyId") != policyProps.end()) 
        {    
            policyIdfromSettings = policyProps.find("PolicyId")->second ;
        }
        if( policyIdfromSettings.compare(policyIdStr) == 0 )
        {
            if( policyProps.find("ApplicationType") != policyProps.end() )
            {
                AppType = policyProps.find("ApplicationType")->second ;
                bRet  = SVS_OK ;
                break ;
            }
        }
        policyIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SV_ULONG AppAgentConfigurator::GetRequestStatus(SV_ULONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    SVERROR bRet = SVS_OK;
    std::string responseString;
    CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
    SV_ULONG status;
    CxRequestCache requestCache;

    cxUpdateCachePtr->GetUpdateStatus(policyId,requestCache);
    if(requestCache.status == CX_UPDATE_PENDING ||
        requestCache.status == CX_UPDATE_FAILED)
    {
        bRet = GetAppAgent().PostUpdate(requestCache.updateString,responseString,policyId,requestCache.instanceId);
        if(bRet == SVS_OK)
        {
            cxUpdateCachePtr->DeleteFromUpdateCache(policyId);
            status =CX_UPDATE_SUCCESSFUL;
        }
    }
    else
    {
        status = requestCache.status;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return status;
}

void AppAgentConfigurator::UpdatePendingAndFailedRequests()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    std::string responseString;
    SVERROR bRet;    
    CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
    std::list<CxRequestCache> requestCacheList;
    cxUpdateCachePtr->GetPendingUpdates(requestCacheList);
    if(requestCacheList.size() == 0)
    {
        DebugPrintf(SV_LOG_DEBUG,"No pending or failed requests\n");
    }
    else
    {
        std::list<CxRequestCache>::iterator iter = requestCacheList.begin();
        int pendingRequests = 0;
        while (iter != requestCacheList.end() && pendingRequests < 5 && !Controller::getInstance()->QuitRequested(1))
        {
            DebugPrintf(SV_LOG_DEBUG,"Post update for the policyId %ld\n",(*iter).policyId);
            bRet = GetAppAgent().PostUpdate((*iter).updateString,responseString,(*iter).policyId,(*iter).instanceId);
            if(bRet == SVS_OK)
            {
                DebugPrintf(SV_LOG_DEBUG," updating policyId %ld from db\n",(*iter).policyId);
                (*iter).status = CX_UPDATE_SUCCESSFUL;
                cxUpdateCachePtr->UpdateCxUpdateRequest(*iter);
            }
            pendingRequests++;
            iter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR ConfigureApplicationAgentProxy::PostUpdate(const std::string& str,std::string& responseString,SV_ULONG policyId,SV_ULONGLONG instanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE;
    InmPolicyUpdateStatus retValue = POLICY_UPDATE_DELETED;
    Serializer sr(m_serializeType);
    if (PHPSerialize == m_serializeType)
    {
        sr.Serialize(str,false) ;
    }
    
    try
    {
		m_transport(sr);
		retValue = sr.UnSerialize<InmPolicyUpdateStatus>();

		DebugPrintf(SV_LOG_DEBUG, "Post Policy-Update return code %d\n Update-String : %s\n", retValue, str.c_str());

		//
		// On Update Failure: POLICY_UPDATE_FAILED
		// ------------------
		//   If policy update return fail status then retain the update entry in pending updates cache database.
		//   Configurator thread will retry the failed pending updates until the updates successfuly posted to CS.
		//
		// On Update Success:  POLICY_UPDATE_COMPLETED (or) POLICY_UPDATE_DELETED (or) POLICY_UPDATE_DUPLICATE
		// ------------------
		//   If any of the above three return codes are returned then delete the update entry from pending updates cache database.
		//   If CS successfuly inserts the policy update into its DB then the POLICY_UPDATE_COMPLETED will be returned.
		//   If CS finds that the same update is already reported then it returns POLICY_UPDATE_DUPLICATE.
		//   If policy entry is removed from CS by the time the policy update reported then it returns POLICY_UPDATE_DELETED.
		//
		if (retValue != POLICY_UPDATE_FAILED)
		{
			CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();

			if (retValue == POLICY_UPDATE_DELETED)
			{
				AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator();
				configuratorPtr->RemovePolicyFromCache(policyId, true);
				AppSchedulerPtr schedulerPtr = AppScheduler::getInstance();
				schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_DELETED);
				cxUpdateCachePtr->DeleteFromUpdateCache(policyId);
			}
			else
			{
				unsigned long bytesSent;
				CxRequestCache pendingCxRequest;
				cxUpdateCachePtr->GetUpdateStatus(policyId, pendingCxRequest);

				if (pendingCxRequest.status != CX_UPDATE_PENDING_NOLOGUPLOAD && 
					PHPSerialize == m_serializeType && 
					uploadFileToCX(policyId, -1, instanceId, bytesSent) != SVS_OK)
				{
					DebugPrintf(SV_LOG_DEBUG, "Failed to upload the file for policy: %d \n", policyId);
				}
				else
				{
					bRet = SVS_OK;
				}
			}
		}
		else
		{
			DebugPrintf(SV_LOG_WARNING, "Post Policy-Update failed for the policy %lu.\n", policyId);
		}
    }
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch( const std::exception & e )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

bool AppAgentConfigurator::isThisProtected(const std::string& scenarioId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
	DebugPrintf(SV_LOG_DEBUG, "Checking the scenario(ID: %s) potrcted or not.\n", scenarioId.c_str());
	bool bRet = false;
	DebugPrintf(SV_LOG_DEBUG, "Protection settings size %d\n", m_settings.m_AppProtectionsettings.size()) ;
	std::map<std::string, AppProtectionSettings>::const_iterator protectionUnitIter = m_settings.m_AppProtectionsettings.begin();
	while( protectionUnitIter != m_settings.m_AppProtectionsettings.end() )
	{
		if( protectionUnitIter->first.compare(scenarioId) == 0 )
		{
			const AppProtectionSettings& protectionSettings = protectionUnitIter->second ;
			if( protectionSettings.m_properties.find("Protected") != protectionSettings.m_properties.end() && 
				protectionSettings.m_properties.find("Protected")->second.compare("1") == 0 )
			{
				bRet = true;
				DebugPrintf(SV_LOG_DEBUG, "This scenario protected at target. \n");
				break ;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG, "This scenario protected at scource. \n");
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "ScenarioId Property not found in thr properties map.\n");
		}
		protectionUnitIter++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

void AppAgentConfigurator::getTasksFromConfiguratorCache(std::map<SV_ULONG, TaskInformation>& tasksMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ApplicationSettings objApplicationSettings;
    SV_ULONG configId = 0 ;
    try
    {
        objApplicationSettings  = GetCachedApplicationSettings();
        std::list<ApplicationPolicy> policyList = objApplicationSettings.m_policies;

        std::list<ApplicationPolicy>::iterator iterBegin = policyList.begin();
        for (/*empty*/; iterBegin != policyList.end(); iterBegin++)
        {
            bool disabled = true ;
            int scheduleType = 0 ;           
            bool policyValid = false ;
            TaskInformation objScheduleTask;
            std::map<std::string, std::string>& policyProps = iterBegin->m_policyProperties;
            try
            {
                if(policyProps.find("PolicyId") != policyProps.end() )
                {
                    configId = boost::lexical_cast<SV_ULONG>(policyProps.find("PolicyId")->second ) ;
                    policyValid = true ;

                }	        	
                if(policyProps.find("ScheduleInterval") != policyProps.end() )
                {
                    objScheduleTask.m_RunEvery = boost::lexical_cast<SV_ULONG>(policyProps.find("ScheduleInterval")->second) ;
                }
                // No scheduling for single vm consistency
                // Disable schedular task for single vm consistency - upgrade case
                std::map<std::string, std::string>::const_iterator citrApplicationType = policyProps.find("ApplicationType");
                std::map<std::string, std::string>::const_iterator citrPolicyType = policyProps.find("PolicyType");
                if ((citrApplicationType != policyProps.end()) &&
                    (citrPolicyType != policyProps.end()) &&
                    (0 == citrApplicationType->second.compare("CLOUD")) &&
                    (0 == citrPolicyType->second.compare("Consistency")) &&
                    0 != objScheduleTask.m_RunEvery) // single vm consistency
                {
                    DebugPrintf(SV_LOG_DEBUG, "%s: Disabling scheduling for policy %lu - single vm consistency.\n", FUNCTION_NAME, configId);
                    objScheduleTask.m_bRemoveFromExisting = true;
                }
                if(policyProps.find("StartTime") != policyProps.end())
                {
                    objScheduleTask.m_firstTriggerTime = boost::lexical_cast<SV_ULONG>(policyProps.find("StartTime")->second) ;
                }
                if(policyProps.find("ExcludeFrom") != policyProps.end() )
                {
                    objScheduleTask.m_ScheduleExcludeFromTime = boost::lexical_cast<SV_ULONG>(policyProps.find("ExcludeFrom")->second) ;
                }
                if(policyProps.find("ExcludeTo") != policyProps.end() )
                {
                    objScheduleTask.m_ScheduleExcludeToTime = boost::lexical_cast<SV_ULONG>(policyProps.find("ExcludeTo")->second) ;
                }
                if( policyProps.find("ScheduleType") != policyProps.end() )
                {
                    scheduleType = boost::lexical_cast<int>(policyProps.find("ScheduleType")->second ) ;
                    if( scheduleType == 0 )
                    {
                        disabled = true ;
                    }
                    else
                    {
                        disabled = false ;
                    }
                }

            }
            catch(boost::bad_lexical_cast &)
            {
            }
            if( !policyValid )
            {
                continue ;
            }
            else
            {
                if( disabled )
                {
                    objScheduleTask.m_bIsActive = false;
                }                 
                DebugPrintf(SV_LOG_DEBUG, "Inserting the task %ld\n", configId) ;
                DebugPrintf(SV_LOG_DEBUG, "ScheduleInterval the task %ld\n", objScheduleTask.m_RunEvery) ;
                tasksMap.insert(std::make_pair(configId, objScheduleTask)) ;

            }

        } // for loop

    }
    catch(...)
    {
        DebugPrintf(SV_LOG_WARNING,"\n Exception caught while unserializing the configurator cache.Ignoring safely.\n");

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void  AppAgentConfigurator::getAppProtectionPolicies(std::map<std::string,AppProtectionSettings>& protectionsettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    protectionsettings = m_settings.m_AppProtectionsettings;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
#ifdef SV_WINDOWS
std::string AppAgentConfigurator::getCacheDirPath()  const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    CRegKey reg;
    LONG rc = reg.Open( HKEY_LOCAL_MACHINE, SV_VXAGENT_VALUE_NAME, KEY_READ );
    if( ERROR_SUCCESS != rc ) {
        DebugPrintf(SV_LOG_ERROR, "Couldn't open vxagent registry key\n" );
    }

    char szPathname[ MAX_PATH ] = "";
    ULONG cch = sizeof( szPathname );
    rc = reg.QueryStringValue( SV_CONFIG_PATHNAME_VALUE_NAME, szPathname, &cch );
    if( ERROR_SUCCESS != rc ) {
        DebugPrintf(SV_LOG_ERROR,"Failed to get path\n");       
    }
    std::string confFilePath = szPathname;
    size_t found;
    found=confFilePath.find_last_of("/\\");
    if(found !=std::string::npos)
        confFilePath =  confFilePath.substr(0,found+1);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return std::string( confFilePath );

}
#endif

SVERROR AppAgentConfigurator::SerializeFailoverJobs(const FailoverJobs& failoverJobs )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    SVERROR bRet = SVS_FALSE ;
    try
    {
        AppLocalConfigurator configurator ;
        std::ofstream out;
        std::string cachePath;
        cachePath = configurator.getApplicatioAgentCachePath();
        cachePath +=  "FailoverJobsCache.dat";
        out.open(cachePath.c_str(), std::ios::trunc);
        if (!out.is_open()) 
        {
            throw AppException("File could not be opened in rw mode\n") ;
        }
        out <<  CxArgObj<FailoverJobs>(failoverJobs);
        out.flush() ;
        out.close();
        bRet = SVS_OK;
    }
    catch(AppException& exception)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to marshalling FailoverJobs %s\n", exception.to_string().c_str()) ;
    }
    catch(std::exception& exception)
    {
        DebugPrintf(SV_LOG_ERROR, "caught exception while FailoverJobs Info %s\n", exception.what()) ;
    }
    catch(...) 
    {
        DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to cache the Application settings.\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR AppAgentConfigurator::UnserializeFailoverJobs(FailoverJobs& failoverJobs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::ifstream inFile;
    unsigned int length =0;

    std::string cachePath;
    AppLocalConfigurator configurator ;
    cachePath =  configurator.getApplicatioAgentCachePath();
    cachePath +=  "FailoverJobsCache.dat";
    std::string lockPath = cachePath + ".lck";
    ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
    lock.acquire();
    SV_ULONG fileSize = 0 ;
    inFile.open(cachePath.c_str(), std::ios::in) ;
    if( inFile.good() )
    {
        inFile.seekg(0, std::ios::end) ;
        fileSize = inFile.tellg() ;
        inFile.close() ;
    }
    // open the local store for read operation
    if( fileSize != 0 )
    {
        inFile.open(cachePath.c_str(), std::ios::in);
        if (!inFile.is_open())
        {
            throw AppException("File could not be opened in read mode\n") ;
        }

        // get length of file
        inFile.seekg (0, std::ios::end);
        length = inFile.tellg();
        inFile.seekg (0, std::ios::beg);

        // allocate memory:
        unsigned int buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned int>::Type(length) + 1, INMAGE_EX(length))
        boost::shared_array<char> buffer(new char[buflen]);
        if(!buffer) 
        {
            throw AppException("couldnot allocate enough memory for buffer\n") ;
            inFile.close();
        }

        // read initialsettings as binary data
        inFile.read (buffer.get(),length);
        *(buffer.get() + length) = '\0';
        // close the handle
        inFile.close();
        try
        {
            failoverJobs =  unmarshal<FailoverJobs> (buffer.get());
            bRet = SVS_OK ;
        }
        catch(AppException& exception) // here control will never come because exception thrown is not of appexception type
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal FailoverJobs %s\n", exception.to_string().c_str()) ;
        }
        catch(std::exception ex)
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unmarshal FailoverJobs %s\n", ex.what()) ;
        }
        catch(...)
        {
            DebugPrintf(SV_LOG_ERROR, "Unknown exception while unserializing the scheduling information\n") ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "No failoverjobs are available\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


void AppAgentConfigurator::UpdateFailoverJobs(const FailoverJob& failoverJob,SV_ULONG policyId,SV_ULONGLONG instanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    RemovePolicyFromCache(policyId) ;
    UpdatePolicyInCache(policyId);
    GetAppAgent().UpdateFailoverJobs(failoverJob,policyId,instanceId) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ConfigureApplicationAgentProxy::UpdateFailoverJobs(const FailoverJob& failoverJob,SV_ULONG policyId,SV_ULONGLONG instanceId)
{
    std::list<FailoverCommandCx> failoverCommandsList;
    CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
    CxRequestCache cxRequest;
    AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
    std::list<FailoverCommand>::const_iterator failoverCmdIter = failoverJob.m_FailoverCommands.begin();
    InmPolicyUpdateStatus retValue = POLICY_UPDATE_DELETED;
	SV_ULONGLONG ulAltInstanceId = 0;
    std::string tempStr;
    bool disabled = true ;
    std::string status ;
    if( failoverJob.m_JobState == TASK_STATE_COMPLETED ) 
    {
        disabled = false ;
        status = "Success" ;
    }
    else if( failoverJob.m_JobState == TASK_STATE_FAILED )
    {
        disabled = false ;
        status = "Failed" ;
    }
    else
    {
        disabled = true ;
        status = "Disabled" ;
    }
    while(failoverCmdIter != failoverJob.m_FailoverCommands.end())
    {
        FailoverCommandCx failoverCmdCx;
        if(failoverCmdIter->m_StepId == 1)
        {   
#ifdef SV_WINDOWS
			//For cluster failover, system will reboot after converting the standalone to cluster.
			//If reboot happens then the instanceId will be change by the scheduler. 
			//If instanceId changes the the output file for group ids 5 & 8 will need to construct with old instanceId.
			if(5 == failoverCmdIter->m_GroupId)
				ulAltInstanceId = failoverCmdIter->m_InstanceId;
#endif
            tempStr = boost::lexical_cast<std::string>(failoverCmdIter->m_GroupId);
            failoverCmdCx.m_Properties.insert(std::make_pair("GroupId",tempStr));
            failoverCmdCx.m_Properties.insert(std::make_pair("Command",failoverCmdIter->m_Command));
            if( !disabled)
            {
                failoverCmdCx.m_Properties.insert(std::make_pair("StartTime",failoverCmdIter->m_StartTime));
                failoverCmdCx.m_Properties.insert(std::make_pair("EndTime",failoverCmdIter->m_EndTime));
				if(failoverCmdIter->m_JobState == TASK_STATE_NOT_STARTED)
				{
					failoverCmdCx.m_Properties.insert(std::make_pair("ExitCode","-1"));
				}
				else
				{
					tempStr = boost::lexical_cast<std::string>(failoverCmdIter->m_ExitCode);
					failoverCmdCx.m_Properties.insert(std::make_pair("ExitCode",tempStr));
				}
            }
            failoverCommandsList.push_back(failoverCmdCx);
        }
        failoverCmdIter++;
    }
    std::string str;// = marshalCxCall(m_dpc,"FailoverCommandsUpdate",policyId,instanceId,status,failoverCommandsList) ;
    Serializer sr(m_serializeType);
    const char *api = "FailoverCommandsUpdate";
    if (PHPSerialize == m_serializeType)
    {
        sr.Serialize(m_dpc, api,policyId,instanceId,status,failoverCommandsList) ;
        str = sr.getMarshalledString();
    WriteUpdateIntoFile(str);
    cxRequest.policyId = policyId;
    cxRequest.instanceId = instanceId;
    cxRequest.updateString = str;
    cxRequest.status = CX_UPDATE_PENDING;
    cxRequest.noOfRetries = 1;
    cxUpdateCachePtr->AddCxUpdateRequest(cxRequest);

    }
    else if (Xmlize == m_serializeType)
    {
        sr.Serialize(api,policyId,instanceId,status,failoverCommandsList) ;
    }
    
    
   
    

    try
    {
        m_transport( sr );    
        //DebugPrintf(SV_LOG_DEBUG,"For Policy %ld response string %s\n",policyId,retValueStr.c_str());
        retValue = sr.UnSerialize<InmPolicyUpdateStatus>( );
        if(retValue == POLICY_UPDATE_DELETED)
        {
            AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
            configuratorPtr->RemovePolicyFromCache(policyId, true);
            schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_DELETED) ;
        }
        if (PHPSerialize == m_serializeType)
        {
            cxRequest.status = CX_UPDATE_SUCCESSFUL;
            cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);
        }
        std::list<FailoverCommandCx>::iterator failoverCommandsListBegIter = failoverCommandsList.begin();
        std::list<FailoverCommandCx>::iterator failoverCommandsListEndIter = failoverCommandsList.end();
        while( failoverCommandsListBegIter != failoverCommandsListEndIter )
        {
            SV_ULONG groupId = boost::lexical_cast<SV_ULONG>(failoverCommandsListBegIter->m_Properties.find("GroupId")->second);
            unsigned long bytesSent = 0;
            if( PHPSerialize == m_serializeType && uploadFileToCX( policyId, groupId, instanceId, bytesSent ) != SVS_OK )
            {
                DebugPrintf( SV_LOG_DEBUG, "Faile to upload the file for policy: %d \n", policyId ) ;
#ifdef SV_WINDOWS
				//If uploading failed then try with alternate instnace Id.
				if(ulAltInstanceId != 0)
					if(uploadFileToCX(policyId, groupId, ulAltInstanceId, bytesSent) != SVS_OK )
					{
						DebugPrintf(SV_LOG_DEBUG, "Failed to upload the file with alternate instance Id %ld for policy: %d",ulAltInstanceId,policyId);
					}
#endif
            }
            
            failoverCommandsListBegIter++;
        }    	
    }
    catch ( ContextualException& ce )
    {

        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, ce.what());
    }
    catch( const std::exception & e )
    {

        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {

        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }


    DebugPrintf(SV_LOG_DEBUG, "FailoverCommandsUpdate Info string is %s\n", str.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}

void AppAgentConfigurator::RemovePolicyFromCache(SV_ULONG& policyId, bool force)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    std::string strPolicyId;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    try
    {
        strPolicyId = boost::lexical_cast<std::string>(policyId);
        std::list<ApplicationPolicy>& policiesFromSettings = m_settings.m_policies ;
        std::list<ApplicationPolicy>::iterator policyIter = policiesFromSettings.begin() ;
        DebugPrintf(SV_LOG_DEBUG, "Policy Id %ld\n", policyId) ;
        while( policyIter != policiesFromSettings.end() )
        {
            std::map<std::string, std::string>& policyProps = policyIter->m_policyProperties ;
            if( policyProps.find("PolicyId") != policyProps.end() )
            {
                if( policyProps.find("PolicyId")->second.compare( strPolicyId ) == 0 )
                {
                    bool shouldDelete = false ;
                    if( force )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Deleting the policy forcefully\n") ;
                        shouldDelete = true ;
                    }
                    else
                    {
                        if(policyProps.find("ScheduleType") != policyProps.end() )
                        {
                            if(policyProps.find("ScheduleType")->second.compare("0") == 0)
                            {
                                DebugPrintf(SV_LOG_DEBUG,"Policy is in Disable mode,skipping policy deletion from cache\n");
                            }
                            else
                            { 
                                if(policyProps.find("ScheduleInterval") != policyProps.end() )
                                {
                                    if(policyProps.find("ScheduleInterval")->second.compare("0") == 0)
                                    {
                                        DebugPrintf(SV_LOG_DEBUG, "Deleting the policy as ScheduleInterval is 0\n") ;
                                        shouldDelete = true;
                                    }
                                }
                                else
                                {
                                    shouldDelete = true ;
                                    DebugPrintf(SV_LOG_ERROR, "ScheduleInterval is not present for the policyId %s. assuming it as run once policy", strPolicyId.c_str()) ;
                                }
                            }
                        }
                    }
                    if( shouldDelete )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Deleting policy %ld\n", policyId) ;
                        std::list<ApplicationPolicy>::iterator tempIter = policyIter ;
                        policyIter++ ;
                        policiesFromSettings.erase(tempIter);
                        SerializeApplicationSettings(m_settings) ;
                        schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_DELETED) ;
                        break;
                    }
                }
            }
            policyIter++ ;
        }
    }
    catch( const boost::bad_lexical_cast & exObj)
    {
        DebugPrintf(SV_LOG_ERROR,"Exception Caught. Exception :%s ",exObj.what());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void AppAgentConfigurator::UpdatePolicyInCache(SV_ULONG& policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    std::string strPolicyId;
    AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
    try
    {
        strPolicyId = boost::lexical_cast<std::string>(policyId);
        std::list<ApplicationPolicy>& policiesFromSettings = m_settings.m_policies ;
        std::list<ApplicationPolicy>::iterator policyIter = policiesFromSettings.begin() ;
        DebugPrintf(SV_LOG_DEBUG, "Policy Id %ld\n", policyId) ;
        while( policyIter != policiesFromSettings.end() )
        {
            std::map<std::string, std::string>& policyProps = policyIter->m_policyProperties ;
            if( policyProps.find("PolicyId") != policyProps.end() )
            {
                if( policyProps.find("PolicyId")->second.compare( strPolicyId ) == 0 )
                {
                    DebugPrintf(SV_LOG_DEBUG,"Adding CommandUpdateSent Key in policy property map\n");
                    policyProps.insert(std::make_pair("CommandUpdateSent","1"));
                    SerializeApplicationSettings(m_settings) ;
                    break;
                }
            }
            policyIter++ ;
        }
    }
    catch( const boost::bad_lexical_cast & exObj)
    {
        DebugPrintf(SV_LOG_ERROR,"Exception Caught. Exception :%s ",exObj.what());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SVERROR ConfigureApplicationAgentProxy::uploadFileToCX( const SV_ULONG& policyId, int groupId, const SV_ULONGLONG& instanceId, unsigned long& bytesSent )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;     	

    SVERROR retStatus = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG, "Upload Log File(s) Statistics\n");
    DebugPrintf(SV_LOG_DEBUG, "==============================\n");
    DebugPrintf( SV_LOG_DEBUG, "PolicyId: %ld\n", policyId );
    if( groupId != -1 )
    {
        DebugPrintf( SV_LOG_DEBUG, "GroupId: %d\n", groupId );
    }
    DebugPrintf( SV_LOG_DEBUG, "InstanceId: " ULLSPEC "\n", instanceId);
    std::map<SV_INT,std::string> groupIdToFileName;
    if( groupId != -1 )
    {
        std::string filePath = getOutputFileName( policyId, groupId,instanceId);
        groupIdToFileName.insert( std::make_pair( groupId, filePath ) ); //PolicyUpdate : 0
        //UpdateFailoverJobs 1-N
    }
    else
    {
        getFileNameForUpLoad( policyId, instanceId , groupIdToFileName ); //Pending Update : -1
    }
    std::map<SV_INT,std::string>::iterator groupIdToFileNameBegIter = groupIdToFileName.begin();
    std::map<SV_INT,std::string>::iterator groupIdToFileNameEndIter = groupIdToFileName.end();
    while( groupIdToFileNameBegIter != groupIdToFileNameEndIter )
    {
        groupId = groupIdToFileNameBegIter->first;
        std::string filePath = groupIdToFileNameBegIter->second;
        int sendBuffMaxSize = 1024; // TO DO.. get from drscout.conf ....
        std::ifstream is;
        is.open( filePath.c_str(), std::ios::in | std::ios::binary );
        if( is.is_open() )
        {
            DebugPrintf( SV_LOG_DEBUG, "\nStarting Upload of File: %s\n", filePath.c_str() );
            boost::shared_array<char> buffer(new char[sendBuffMaxSize+1]);
            is.seekg(0, std::ios::end);
            std::ifstream::pos_type length = is.tellg();
            is.seekg(0, std::ios::beg);
            unsigned long lbytesSent = 0;
            std::ifstream::pos_type offSet = 0 ;
            while( offSet < length )
            {
                if( (length - offSet) < sendBuffMaxSize )
                {
                    sendBuffMaxSize = (length - offSet);
                }
                is.read( buffer.get(), sendBuffMaxSize );
                buffer[sendBuffMaxSize] = '\0';
                lbytesSent = 0;
                DebugPrintf( SV_LOG_DEBUG, "Offset: %d \n", (int)offSet );
                DebugPrintf( SV_LOG_DEBUG, "Sending Buufer Size: %d \n", sendBuffMaxSize );
                if( transportToCX( policyId, groupId, instanceId, offSet, buffer.get(), lbytesSent ) != SVS_OK )
                {
                    DebugPrintf( SV_LOG_DEBUG, "Successfully Sent Buffer Size: %ld \n", lbytesSent );
                    retStatus = SVS_FALSE;
                    is.close();
                    break; 
                }
                DebugPrintf( SV_LOG_DEBUG, "Successfully Sent Buffer Size: %ld \n", lbytesSent );
                bytesSent += lbytesSent;
                offSet = is.tellg();
            }
            DebugPrintf( SV_LOG_DEBUG, "Total Bytes Sent: %ld\n", bytesSent );
            is.close();
        }
        else
        {
            DebugPrintf( SV_LOG_ERROR, "Failed to open the file: %s\n", filePath.c_str() );
            retStatus = SVS_FALSE;
        }
        /*
        if( retStatus == SVS_OK )
        {	
        if( remove(filePath.c_str() ) != 0 )
        {
        DebugPrintf( SV_LOG_ERROR, "Error While removeing the file %s \n", filePath.c_str() );
        }
        }
        */
        groupIdToFileNameBegIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
    return retStatus;
}



SVERROR ConfigureApplicationAgentProxy::transportToCX( const SV_ULONG& policyId, const int& groupId, const SV_ULONGLONG& instanceId, const int& offset, const std::string& Data, unsigned long& bytesSent  )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;     	

    SVERROR retStatus = SVS_FALSE;
    std::stringstream transportStaream;
    transportStaream << "[PolicyId:" << policyId << "]" << std::endl;
    if( groupId != -1 )
    {
        transportStaream << "[GroupId:" << groupId << "]" << std::endl ;
    }
    transportStaream << "[InstanceId:" << instanceId << "]" << std::endl ;
    transportStaream << "[Offset:" << offset << "]" << std::endl;
    transportStaream << "[Length:" << Data.size() << "]" << std::endl;
    transportStaream << Data;
    AppLocalConfigurator appLocalConfig;
    std::string cxIpAddress = GetCxIpAddress();
    SV_INT cxPortNo = appLocalConfig.getHttp().port; 
    std::string transportURL = "/UpdateLog.php";

    char *szRecvBuffer = NULL;
	SVERROR ret = postToCx( cxIpAddress.c_str(), cxPortNo, transportURL.c_str(), transportStaream.str().c_str(), &szRecvBuffer,appLocalConfig.IsHttps() );
    if( ret != SVS_OK ) 
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to post data to CX.\n" );
        bytesSent = 0;
    }
    else
    {
        retStatus = SVS_OK;
        bytesSent = Data.size();
    }
    SAFE_DELETE(szRecvBuffer);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
    return retStatus;
}

void AppAgentConfigurator::ChangeTransport(const ReachableCx& reachableCx,const SerializeType_t serializeType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
    ApiWrapper apiwrapper(reachableCx.ip,reachableCx.port,serializeType);
    m_apiWrapper = apiwrapper;
    m_cx.reset( new ApplicationCxProxy( m_localConfigurator.getHostId(), 
                        m_apiWrapper,
             m_localConfigurator,
                        appconfigLock,serializeType)) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

std::string AppAgentConfigurator::getTgtOracleInfo(std::string &instanceName)
{
    return GetAppAgent().getTgtOracleInfo(instanceName);
}

std::string ConfigureApplicationAgentProxy::getTgtOracleInfo(std::string &instanceName)
{
 //   std::string request = marshalCxCall(m_dpc,"GetTargetOracleInfo", instanceName);
    Serializer sr(m_serializeType);
    const char *api = "GetTargetOracleInfo";
    if (PHPSerialize == m_serializeType)
    {
        sr.Serialize(m_dpc, api, instanceName);
    }
    else if (Xmlize == m_serializeType)
    {
        sr.Serialize(api, instanceName);
    }
    m_transport( sr );
    return sr.UnSerialize<std::string>();

}

void ConfigureApplicationAgentProxy::setApiAndFirstArg(const char *apiname,
                                const char *dpc, 
                                const char **apitocall,
                                const char **firstargtoapi) const
{
    if (PHPSerialize == m_serializeType)
    {
        *apitocall = dpc;
        *firstargtoapi = apiname;
    }
    else if (Xmlize == m_serializeType)
    {
        *apitocall = apiname;
        *firstargtoapi = dpc;
    }
}

SerializeType_t AppAgentConfigurator::getSerializationType()
{
    return m_serializeType;
}

bool AppAgentConfigurator::registerApplicationAgent()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;

	AppLocalConfigurator localconfigurator ;
    std::string host_id = localconfigurator.getHostId();
    std::string prod_version = PROD_VERSION;
	std::string agent_version = INMAGE_PRODUCT_VERSION_STR;
	std::string agent_installPath = localconfigurator.getInstallPath();
	std::string host_name = Host::GetInstance().GetHostName();
	std::string host_ip = Host::GetInstance().GetIPAddress();
	const Object & osinfo = OperatingSystem::GetInstance().GetOsInfo();
    const ENDIANNESS & endianness = OperatingSystem::GetInstance().GetEndianness();
    const OS_VAL & osval = OperatingSystem::GetInstance().GetOsVal();

    return GetAppAgent().registerApplicationAgent(host_id,
		agent_version,
		host_name,
		host_ip,
		agent_installPath,
		CurrentCompatibilityNum(),
		getTimeZone(),
		getInmPatchHistory(agent_installPath),
		prod_version,
		osval,
		osinfo,
		endianness);
}

//This call will return true in case success. will return false in case of failure.
bool ConfigureApplicationAgentProxy::registerApplicationAgent(const std::string& hostId,
		const std::string& Agentversion,
		const std::string& HostName,
		const std::string& HostIP,
		const std::string& InstallPath,
		SV_ULONG compatabilityNum,
		const std::string& timeZone,
		const std::string& PatchHistory,
		const std::string& ProdVersion,
		const SV_ULONG& osval,
		const Object& osinfo,
		const SV_ULONG& endianness)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
    Serializer sr(m_serializeType);
    const char *api = "registerApplicationAgent";
	
    if (PHPSerialize == m_serializeType)
    {
		sr.Serialize(m_dpc, api, hostId, Agentversion, HostName, HostIP, InstallPath, compatabilityNum, timeZone, PatchHistory, ProdVersion, osval, osinfo.m_Attributes, endianness) ;
	}
    else if (Xmlize == m_serializeType)
    {
        sr.Serialize(api, hostId, Agentversion, HostName, HostIP, InstallPath, compatabilityNum, timeZone, PatchHistory, ProdVersion) ;
    }
	else
	{
		DebugPrintf(SV_LOG_WARNING, "Unknown serialization type. Can not register host\n");
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return bRet;
	}

	DebugPrintf(SV_LOG_DEBUG,"Application Agent Host Registration: \n%s\n",sr.getMarshalledString().c_str());

	unsigned char currHash[APP_HASH_LENGTH] = {0};
	INM_MD5_CTX ctx;
	INM_MD5Init(&ctx);
	INM_MD5Update(&ctx,(unsigned char*)sr.getMarshalledString().c_str(),sr.getMarshalledString().size());
	INM_MD5Final(currHash,&ctx);
	if(memcmp(currHash,m_registerApplicationAgentHashp,sizeof(m_registerApplicationAgentHashp)) == 0)
	{
		DebugPrintf(SV_LOG_INFO,"No change in configuration. Skipping registration.\n");
		bRet = true;
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
		return bRet;
	}
	else
	{
		try 
		{
			m_transport(sr);
			bRet = sr.UnSerialize<bool>( );
			if(bRet)
				inm_memcpy_s(m_registerApplicationAgentHashp, sizeof(m_registerApplicationAgentHashp), currHash, sizeof(currHash)); 
		}
		catch( ContextualException& ce )
		{
			DebugPrintf(SV_LOG_WARNING, "\n%s encountered exception %s.\n", FUNCTION_NAME, ce.what());
		}
		catch( const std::exception& e )
		{
			DebugPrintf(SV_LOG_WARNING, "\n%s encountered exception %s.\n", FUNCTION_NAME, e.what());
		}
	}
    if(bRet)
    {
        DebugPrintf(SV_LOG_DEBUG, "Registering Application Agent is success. \n");
    }
    else
    {
        DebugPrintf(SV_LOG_WARNING, "Registering Application Agent is Failed or it is not supported. \n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

void AppAgentConfigurator::UpdateUnregisterApplicationInfo(SV_ULONG policyId,SV_ULONGLONG instanceId,std::string strErrorCode,std::string strLog)
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
   ACE_Guard<ACE_Recursive_Thread_Mutex> lock( appconfigLock ) ;
   AppSchedulerPtr schedulerPtr = AppScheduler::getInstance() ;
   DebugPrintf(SV_LOG_DEBUG, "Erasing Policy from Cache \n");
   try
   {
      std::string strPolicyId = boost::lexical_cast<std::string>(policyId);
      std::list<ApplicationPolicy>& policiesFromSettings = m_settings.m_policies ;
      std::list<ApplicationPolicy>::iterator policyIter = policiesFromSettings.begin() ;
      while( policyIter != policiesFromSettings.end() )
      {
        std::map<std::string, std::string>& policyProps = policyIter->m_policyProperties ;
        if( policyProps.find("PolicyId") != policyProps.end() )
        {
          if( policyProps.find("PolicyId")->second.compare( strPolicyId ) == 0 )
          {
            std::list<ApplicationPolicy>::iterator tempIter = policyIter ;
            policyIter++ ;
            policiesFromSettings.erase(tempIter);
            SerializeApplicationSettings(m_settings);
            //schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_DELETED) ;
            break;
          }
        }
        policyIter++ ;
      }
   }
   catch( const boost::bad_lexical_cast & exObj)
   {
      DebugPrintf(SV_LOG_ERROR,"Exception Caught. Exception :%s ",exObj.what());
   }
   //RemovePolicyFromCache(policyId) ;
   //UpdatePolicyInCache(policyId);
   GetAppAgent().UpdateUnregisterApplicationInfo(policyId,instanceId,strErrorCode,strLog);
   //configuratorPtr->RemovePolicyFromCache(policyId, true);
   //schedulerPtr->UpdateTaskStatus(policyId, TASK_STATE_DELETED) ;
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void ConfigureApplicationAgentProxy::UpdateUnregisterApplicationInfo(SV_ULONG policyId,SV_ULONGLONG instanceId,std::string strErrorCode,std::string strLog)
{
   strLog = "" ;
   std::string str;
   CxUpdateCachePtr cxUpdateCachePtr = CxUpdateCache::getInstance();
   CxRequestCache cxRequest;
   Serializer sr(m_serializeType);
   AppAgentConfiguratorPtr configuratorPtr = AppAgentConfigurator::GetAppAgentConfigurator() ;
   const char *api = "UpdatePolicy";
                                     
   str = marshalCxCall(m_dpc,"UpdatePolicy",policyId,instanceId,strLog) ;
   if (PHPSerialize == m_serializeType)
   {
       sr.Serialize(m_dpc, api, policyId,instanceId,strErrorCode,strLog) ;
       WriteUpdateIntoFile(sr.getMarshalledString());

       cxRequest.policyId = policyId;
       cxRequest.instanceId = instanceId;
       cxRequest.updateString = str;
       cxRequest.status = CX_UPDATE_PENDING;
       cxRequest.noOfRetries = 1;
       cxUpdateCachePtr->AddCxUpdateRequestEx(cxRequest);    
   }
   else if (Xmlize == m_serializeType)
   {
       sr.Serialize(api, policyId,instanceId,strErrorCode,strLog);
   }
                                              
   WriteUpdateIntoFile(sr.getMarshalledString()) ;
   InmPolicyUpdateStatus retValue = POLICY_UPDATE_FAILED;
   try
   {
       m_transport(sr) ;
       retValue = sr.UnSerialize<InmPolicyUpdateStatus>( );
       std::stringstream stream;
       stream << cxArg<InmPolicyUpdateStatus>(retValue);
       WriteUpdateIntoFile(stream.str());
       DebugPrintf(SV_LOG_DEBUG, "retValue of UnSerialize : %d\n",retValue);

       if (PHPSerialize == m_serializeType)
       {
          cxRequest.status = CX_UPDATE_SUCCESSFUL;
          cxUpdateCachePtr->UpdateCxUpdateRequest(cxRequest);
       }
       DebugPrintf(SV_LOG_DEBUG, "CX Request status :%d\n",cxRequest.status);
       unsigned long bytesSent;
       if( strErrorCode.compare("InProgress") != 0)
       {
          if( PHPSerialize == m_serializeType && uploadFileToCX( policyId, 0, instanceId, bytesSent ) != SVS_OK )
          {
             DebugPrintf( SV_LOG_DEBUG, "Faile to upload the file for policy: %d \n", policyId ) ;
          }
       }
     }
    catch ( ContextualException& ce )
    {
       DebugPrintf(SV_LOG_ERROR, 
                     "\n%s encountered exception %s.\n",
                               FUNCTION_NAME, ce.what());
    }
    catch( const std::exception & e )
    {
       DebugPrintf(SV_LOG_ERROR, 
                       "\n%s encountered exception %s.\n",
                         FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
       DebugPrintf(SV_LOG_ERROR, 
                         "\n%s encountered unknown exception.\n",
                                      FUNCTION_NAME);
    }

   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

