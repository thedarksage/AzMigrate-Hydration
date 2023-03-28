//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : dataprotectionsync.cpp
//
// Description:
//

#ifdef SV_WINDOWS
#include <windows.h>
#include "hostagenthelpers.h"
#include <io.h>
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include <ace/config.h>
#include <ace/Time_Value.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/Process_Manager.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_unistd.h>
#include <ace/File_Lock.h>
#include <ace/Guard_T.h>
#include <ace/OS_NS_stdio.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>

#include "dataprotectionsync.h"
#include "dataprotectionutils.h"
#include "dataprotectionexception.h"
#include "theconfigurator.h"
#include "configurevxagent.h"
#include "hostagenthelpers_ported.h"
#include "volumeinfo.h"
#include "dpdrivercomm.h"
#include "cdpplatform.h"

#include "cdpdatabase.h"
#include "cdputil.h"
#include "cxpsheaders.h"
#include "decompress.h"
#include "cxpsheaders.h"
#include "portablehelpers.h"
#include "portablehelperscommonheaders.h"
#include "localconfigurator.h"
#include "compressmode.h"
#include "volumeop.h"
#include "usecfs.h"
#include "configwrapper.h"
#include "inmalertdefs.h"
#include "Telemetry/TelemetrySharedParams.h"
#include "Telemetry/RequestTelemetryData.h"

#include "setpermissions.h"
#ifdef SV_UNIX
#include "persistentdevicenames.h"
#endif

/** Added by BSR - Configurator Exception Handling **/
#include "inmageex.h"
/** End of the change **/

using namespace std;

extern void GetHypervisorInfo(HypervisorInfo_t &hypervisorinfo);


// TODO: use configurator
static std::string const UncompressExe("UncompressExe");
static std::string const SyncDir("/resync/");

#define DATAPATHTHROTTLE "dataPathThrottle"

const SV_UINT DefaultThrottleWaitTimeInSec = 120;

DataProtectionSync::DataProtectionSync(VOLUME_GROUP_SETTINGS const& volumeGrpSettings,const bool syncThroughThread)
    : m_Protect(true),
      m_Source(SOURCE == volumeGrpSettings.direction),
      m_JobId( (*volumeGrpSettings.volumes.begin()).second.jobId), /** Fast Sync TBC - BSR **/
// m_JobId( volumeGrpSettings.jobId ),
      m_GrpId(volumeGrpSettings.id),
      m_Protocol((*volumeGrpSettings.volumes.begin()).second.transportProtocol),
      m_Settings((*volumeGrpSettings.volumes.begin()).second),
      m_Http(GetHttpSettings()),
      m_dumpHcdsHandle(ACE_INVALID_HANDLE),
    m_TransportSettings(volumeGrpSettings.transportSettings),
    m_SyncThroughThread(syncThroughThread),
    m_SourceReadRetries(GetConfigurator().getSourceReadRetries()),
    m_bZerosForSourceReadFailures(GetConfigurator().getZerosForSourceReadFailures()),
    m_SourceReadRetriesInterval(GetConfigurator().getSourceReadRetriesInterval()),
    m_pClusterBitmapCustomizationInfos(0),
    m_SrcStartOffset(0),
    m_profileVerbose(false),
    m_bThrottlePersisted(false)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if(m_Source)
    {
        m_Secure = (VOLUME_SETTINGS::SECURE_MODE_SECURE == (*volumeGrpSettings.volumes.begin()).second.sourceToCXSecureMode);
    }
    else if(TARGET == volumeGrpSettings.direction)
    {
        m_Secure = (VOLUME_SETTINGS::SECURE_MODE_SECURE == (*volumeGrpSettings.volumes.begin()).second.secureMode);
    }
    VOLUME_SETTINGS::options_t::const_iterator iter(m_Settings.options.find("useCfs"));
    if (m_Settings.options.end() != iter && "1" == (*iter).second) {
        m_cfsPsData.m_useCfs = true;
    }
    iter = m_Settings.options.find("psId");
    if (m_Settings.options.end() != iter) {
        m_cfsPsData.m_psId = (*iter).second;
    }

    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    CDPUtil::InitQuitEvent();

    //Added for making pre allocation configurable. See Bug#6984
    LocalConfigurator localConfigurator;
    m_PreAllocate = localConfigurator.isCdpDataFilePreAllocationEnabled();

    if(localConfigurator.AsyncOpEnabledForPhysicalVolumes() || localConfigurator.AsyncOpEnabled())
    {
        m_asyncops = true;
    } else
    {
        m_asyncops = false;
    }
    m_useNewApplyAlgorithm = localConfigurator.useNewApplyAlgorithm();

    m_bCheckCacheDirSpace = 0;
    m_endianTagSize = (Settings().OtherSiteCompatibilityNum >= ENDIANTAG_COMPATIBILITY_NUMBER)?sizeof(SV_UINT):0;

    bool isfilterdriveravailable = localConfigurator.IsFilterDriverAvailable();
    if (m_Source)
    {
        /* TODO: move allocation to an init routine that
         * can check return value */
        m_pClusterBitmapCustomizationInfos = new (std::nothrow) cdp_volume_util::ClusterBitmapCustomizationInfos_t;
        FillClusterBitmapCustomizationInfos(isfilterdriveravailable);
        if(isfilterdriveravailable)
        {
            DpDriverComm driverComm(Settings().isFabricVolume()?
                                    Settings().sanVolumeInfo.virtualName :
                                    Settings().deviceName, Settings().devicetype);
            m_SrcStartOffset = driverComm.GetSrcStartOffset(Settings());
            std::stringstream sssrcstartoffset;
            sssrcstartoffset << m_SrcStartOffset;
            DebugPrintf(SV_LOG_DEBUG, "source start offset: %s\n", sssrcstartoffset.str().c_str());
        }
    }

    SetDeviceType();

    if (m_Source || (cdp_volume_t::CDP_DISK_TYPE == DeviceType())) 
    {
        //Collect vic cache
        SetVolumesCache();
    }

    // for azure, we are doing full disk replication. We will always be using non fs aware resync 
    // (earlier implementation of non-pipelined fastsync TBC)
    // see design doc for TFS Feature 2166919
    m_IsFsAwareResync = ((Settings().OtherSiteCompatibilityNum >= FSAWARE_RESYNC_COMPATIBILITY_NUMBER)
        && (Settings().getTargetDataPlane() != VOLUME_SETTINGS::AZURE_DATA_PLANE));

    m_ResyncCopyMode = Settings().GetResyncCopyMode();
    DebugPrintf(SV_LOG_DEBUG, "resync copy mode is: %s\n", StrResyncCopyMode[m_ResyncCopyMode]);
    
    /*
      for (VOLUME_SETTINGS::constOptionsIter_t oit = Settings().options.begin(); oit != Settings().options.end(); oit++)
      {
      DebugPrintf("%s -- %s\n", oit->first.c_str(), oit->second.c_str());
      }
    */

    /// Set profiler type
    std::string deviceNames = localConfigurator.getProfileDeviceList();
    boost::char_separator<char> delm(",");
    boost::tokenizer < boost::char_separator<char> > strtokens(deviceNames, delm);
    for (boost::tokenizer< boost::char_separator<char> > ::iterator it = strtokens.begin(); it != strtokens.end(); ++it)
    {
        /// remove leading and trailing white space if any
        size_t firstws = (*it).find_first_not_of(" ");
        if (std::string::npos != Settings().deviceName.find((*it).substr(firstws, (*it).find_last_not_of(" ")-firstws+1)))
        {
            m_profileVerbose = true;
            /// Set profiling path
            m_profilingPath = localConfigurator.getInstallPath();
            m_profilingPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            m_profilingPath += Perf_Folder;
            m_profilingPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            m_profilingPath += Resync_Folder;
            m_profilingPath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            m_profilingPath += GetVolumeDirName(Settings().deviceName);
            break;
        }
    }
    m_dataPathThrottleProfiler = ProfilerFactory::getProfiler(GENERIC_PROFILER, DATAPATHTHROTTLE,
        new ProfilingBuckets(DATAPATHTHROTTLE), ProfileVerbose(), ProfilingPath(), false);

    if (m_Source)
    {
        CreateHealthCollator();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

#ifdef DP_SYNC_RCM
const RcmClientLib::RcmConfigurator& DataProtectionSync::GetConfigurator() const
{
    RcmClientLib::RcmConfiguratorPtr rcmConfigurator(RcmClientLib::RcmConfigurator::getInstance());
    return *rcmConfigurator;
}
#else
ConfigureVxAgent& DataProtectionSync::GetConfigurator() const
{
    return TheConfigurator->getVxAgent();
}
#endif

HTTP_CONNECTION_SETTINGS DataProtectionSync::GetHttpSettings() const
{
    LocalConfigurator lc;
    HTTP_CONNECTION_SETTINGS hcs = lc.getHttp();
    // Check for failback scenario.
    if (IsAgentRunningOnAzureVm())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Using CS ipAddress %s from CsAddressForAzureComponents in config.\n",
            FUNCTION_NAME, lc.getCsAddressForAzureComponents().c_str());
        inm_strcpy_s(hcs.ipAddress, ARRAYSIZE(hcs.ipAddress), lc.getCsAddressForAzureComponents().c_str());
    }
    return hcs;
}

// Note: this routine is not thread safe. should not
// be called from two different threads simultaneously
void DataProtectionSync::SetShouldCheckForCacheDirSpace()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // m_bCheckCacheDirSpace values:
    // 0 - uninitialized
    // 1 - should check
    // 2 - need not check, using file tal and cachedir and remotedir on same partition

    do {
        if(0 == m_bCheckCacheDirSpace)
        {
            if(Protocol() != TRANSPORT_PROTOCOL_FILE ) {
                m_bCheckCacheDirSpace = 1;
                break;
            }

            if(OnSameRootVolume(m_CacheDir, m_RemoteSyncDir)) {
                m_bCheckCacheDirSpace = 2;
                break;
            }

            m_bCheckCacheDirSpace = 1;
        }

    } while (0);

}

bool DataProtectionSync::ShouldCheckForCacheDirSpace()
{
    return ( m_bCheckCacheDirSpace == 1);
}

// --------------------------------------------------------------------------
// used for throtlling CPU or to prevent tight loops when there was
// no work to be done
// --------------------------------------------------------------------------
void DataProtectionSync::Idle(int seconds)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    QuitRequested(seconds);
}

// --------------------------------------------------------------------------
// checks if we should throttle based on space
// --------------------------------------------------------------------------
void DataProtectionSync::ThrottleSpace()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    do
    {
        bool throttle = Source() ? TheConfigurator->getVxAgent().shouldThrottleResync( m_Settings.deviceName, m_Settings.endpoints.begin()->deviceName, 
                                                                                       m_GrpId ) :
            TheConfigurator->getVxAgent().shouldThrottleTarget( m_Settings.deviceName );
        if( !throttle )
        {
            break;
        }

        if (Source())
        {
            DebugPrintf(SV_LOG_ERROR, "Throttling is set for deviceName %s and endpointdevicename %s grpid %d\n", 
                        m_Settings.deviceName.c_str(), m_Settings.endpoints.begin()->deviceName.c_str(), 
                        m_GrpId);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Throttling is set for deviceName %s\n",
                        m_Settings.deviceName.c_str());
        }
        if(QuitRequested(30))
            break;

    } while( true );
}

// --------------------------------------------------------------------------
// get file list based on file spec places them in the Files_t
// --------------------------------------------------------------------------
bool DataProtectionSync::GetFileList(CxTransport::ptr cxTransport, std::string const & fileSpec, Files_t& files)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    FileInfos_t fileInfos;

    if (!cxTransport->listFile(fileSpec, fileInfos))
    {
        DebugPrintf(SV_LOG_ERROR, "FAILED DataProtectionSync::GetFileList cxTransport->GetFileList: %s\n", cxTransport->status());
        return false;
    }

    if (fileInfos.empty()) {
        files.clear();
        return true;
    }

    // now copy the filelist data into Files_t
    FileInfos_t::iterator iter(fileInfos.begin());
    FileInfos_t::iterator iterEnd(fileInfos.end());
    for (/* empty */; iter != iterEnd; ++iter) {
        files.push_back((*iter).m_name);
    }

    return true;
}

// --------------------------------------------------------------------------
// checks for quit requested
// TODO: update once threads are being used instead of a separate process
// --------------------------------------------------------------------------
bool DataProtectionSync::QuitRequested(int seconds)
{
    //DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (!m_Protect || CDPUtil::Quit()) {
        return true;
    }

    if(CDPUtil::QuitRequested(seconds)) {
        Stop();
    }

    return (!m_Protect || CDPUtil::Quit());
}

// --------------------------------------------------------------------------
// checks if service has requested a quit
// true: quit requested
// --------------------------------------------------------------------------
bool DataProtectionSync::ServiceRequestedQuit(int waitTimeSeconds)
{
    if(CDPUtil::QuitRequested(waitTimeSeconds)) {
        Stop();
        return true;
    }

    return false;
}

void DataProtectionSync::HandleDatapathThrottle(const ResponseData &rd)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::stringstream sserr;
    SV_ULONGLONG ttlsec = 0;
    std::string healthIssueCode;

    sserr << FUNCTION_NAME << ": device " << Settings().deviceName;

    //Note: When Cumulative Throttle is there, then no need to raise ResyncThrottle Health Issue

    if (rd.headers.find(CxpsTelemetry::CUMULATIVE_THROTTLE) != rd.headers.end())
    {
        sserr << " is cumulative throttled";
        //For V2A-RCM Provider, agent has to  raise cumulative throttle, unlike in V2A Provider where PS will raise HI
        if(IsRcmMode())
        {
            healthIssueCode = AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode;
        }
    }
    else if (rd.headers.find(CxpsTelemetry::RESYNC_THROTTLE) != rd.headers.end())
    {
        sserr << " is resync throttled";
        healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::IRThrottle::HealthCode;
    }
    else
    {
        sserr << " is throttled but throttle info is missing in ResponseData.";
        healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::IRThrottle::HealthCode;//in this case safely assuming as IR Throttle
    }
    if (m_Source && !healthIssueCode.empty())
    {
        SetThrottleHealthIssue(healthIssueCode);
    }

    try
    {
        responseHeaders_t::const_iterator itr = rd.headers.find(CxpsTelemetry::THROTTLE_TTL);
        if (itr != rd.headers.end())
        {
            ttlsec = boost::lexical_cast<SV_ULONGLONG>(itr->second);
            (ttlsec /= 1000) ? ttlsec : ttlsec = 1;
        }
        else
        {
            sserr << " TTL is missing in ResponseData using default TTL=" << DefaultThrottleWaitTimeInSec << " sec";
        }
    }
    catch (const std::exception &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s caught exception %s\n", FUNCTION_NAME, ex.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s:Caught generic exception.\n", FUNCTION_NAME);
    }

    if (!ttlsec)
    {
        ttlsec = DefaultThrottleWaitTimeInSec;
    }

    sserr << " Current TTL=" << ttlsec << " sec";

    DatapathThrottleLogger::LogDatapathThrottleInfo(sserr.str(), boost::chrono::steady_clock::now(), ttlsec);

    sserr << "\n";

    DebugPrintf(SV_LOG_DEBUG, "%s honoring current TTL %lu sec before next data upload attempt\n", FUNCTION_NAME, ttlsec);
    m_dataPathThrottleProfiler->start();
    QuitRequested(ttlsec);
    m_dataPathThrottleProfiler->stop(0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DataProtectionSync::CreateHealthCollator()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    try
    {
        if (m_healthCollatorPtr)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: HealthCollator instance already created.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        
        std::string strDiskId = Settings().deviceName;
        boost::replace_all(strDiskId, "/", "_");

        boost::filesystem::path dpHealthIssuesFilePath(GetConfigurator().getHealthCollatorDirPath());
        dpHealthIssuesFilePath /= NSHealthCollator::DPPrefix + NSHealthCollator::UnderScore +
            GetConfigurator().getHostId() + NSHealthCollator::UnderScore +
            strDiskId + NSHealthCollator::ExtJson;

        m_healthCollatorPtr.reset(new HealthCollator(dpHealthIssuesFilePath.string()));
        if (!m_healthCollatorPtr)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed to create HealthCollator instance, dpHealthIssuesFilePath=%s.\n",
                FUNCTION_NAME, dpHealthIssuesFilePath.string().c_str());
        }
        else
        {
            //check if throttle health issue is already there or not
            std::string strThrottleHealthIssueId;
            COMBINE_DISKID_WITH_HEALTH_ISSUE_CODE(strDiskId,
                AgentHealthIssueCodes::DiskLevelHealthIssues::IRThrottle::HealthCode,
                strThrottleHealthIssueId);
            if (m_healthCollatorPtr->IsThisHealthIssueFound(strThrottleHealthIssueId))
            {
                m_bThrottlePersisted = true;
            }
            else
            {
                //check for cumulative throttle also
                if (m_healthCollatorPtr->IsThisHealthIssueFound(AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode))
                {
                    m_bThrottlePersisted = true;
                }
                else
                {
                    m_bThrottlePersisted = false;
                }
            }
        }
    }
    catch (const JSON::json_exception& jsone)
	{
		DebugPrintf(SV_LOG_ERROR, "%s failed - JSON parser failed with exception %s.", FUNCTION_NAME, jsone.what());
	}
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception.\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DataProtectionSync::SetThrottleHealthIssue(const std::string& healthIssueCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        if (m_bThrottlePersisted.load(boost::memory_order_consume))
        {
            //return immediately as Throttle Health Issue is already persisted
            DebugPrintf(SV_LOG_DEBUG, "%s: Throttle Health Issue is already persisted.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }

        if (!m_healthCollatorPtr)
        {
            throw INMAGE_EX("HealthCollator instance not initialized.")(Settings().deviceName);
        }

        //Raise IR Throttle health issue and persist to the .json file
        std::map<std::string, std::string> msgParams;

        //Create Health Issue Params
        msgParams.insert(std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::IRThrottle::ObservationTime,
            m_healthCollatorPtr->GetCurrentObservationTime()));

        //Create the Disk level Health Issue object
        AgentDiskLevelHealthIssue diskHI(Settings().deviceName, healthIssueCode, msgParams);

        //Collate the Disk level Health Issue 
        if (!m_healthCollatorPtr->SetDiskHealthIssue(diskHI))
        {
            DebugPrintf(SV_LOG_ERROR, "%s:Failed to raise the health issue %s for disk %s\n",
                FUNCTION_NAME, diskHI.IssueCode.c_str(), diskHI.DiskContext.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s:Disk level health issue %s is set for the disk %s\n",
                FUNCTION_NAME, diskHI.IssueCode.c_str(), diskHI.DiskContext.c_str());

            m_bThrottlePersisted.store(true, boost::memory_order_release);
        }
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DataProtectionSync::ResetThrottleHealthIssue()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    //Reset Throttle Health Issue if it was set earlier

    try
    {
        if (!m_bThrottlePersisted.load(boost::memory_order_consume))
        {
            //return immediately if it is not throttled
            DebugPrintf(SV_LOG_DEBUG, "%s: Pair is not throttled.\n", FUNCTION_NAME);
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        if (!m_healthCollatorPtr)
        {
            throw INMAGE_EX("HealthCollator instance not initialized.")(Settings().deviceName);
        }
        if (m_healthCollatorPtr->IsThereAnyHealthIssue())
        {
            std::vector<std::string> vResetHICodes;
            vResetHICodes.push_back(AgentHealthIssueCodes::DiskLevelHealthIssues::IRThrottle::HealthCode);
            if (IsRcmMode())
            {
                vResetHICodes.push_back(AgentHealthIssueCodes::VMLevelHealthIssues::CumulativeThrottle::HealthCode);
            }

            //For V2A Provider, reset only the Resync Throttle Health Issue
            if (!m_healthCollatorPtr->ResetDiskHealthIssues(vResetHICodes, Settings().deviceName))
            {
                DebugPrintf(SV_LOG_ERROR, "%s:Failed to Remove the health issue %s for disk %s\n",
                    FUNCTION_NAME, AgentHealthIssueCodes::DiskLevelHealthIssues::IRThrottle::HealthCode.c_str(),
                    Settings().deviceName.c_str());
            }
            else
            {
                m_bThrottlePersisted.store(false, boost::memory_order_release);
            }
        }
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

// --------------------------------------------------------------------------
// sends data to location based on transport settings
// --------------------------------------------------------------------------
bool DataProtectionSync::SendData(CxTransport::ptr cxTransport, std::string const & fileName, char const * buffer, int length, bool moreData, COMPRESS_MODE compressMode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool retval = false;
    do
    {
        std::map<std::string, std::string> headers;
        headers.insert(std::make_pair(HTTP_PARAM_TAG_FILETYPE, boost::lexical_cast<std::string>((int)(CxpsTelemetry::FileType_Resync))));
        headers.insert(std::make_pair(HTTP_PARAM_TAG_DISKID, Settings().deviceName));
        if (!(retval = cxTransport->putFile(fileName, length, buffer, moreData, compressMode, headers)))
        {
            const ResponseData rd = cxTransport->getResponseData();

            if (rd.responseCode == CLIENT_RESULT_NOSPACE)
            {
                HandleDatapathThrottle(rd);

                // Just continue retry untill throttle is overcome.
                // This has added benefit in case of re-protect as otherwise that will cause dataprotection on MT to keep on exiting after honoring TTL.
                //     - Re-protect case where selected on-prem MT is stand alone(not inbuilt CSPSMT) so using data path to PS.
                //     - This will allow sync applier thread to drain sync files on PS IR cache in usual pace otherwise sync files will be drained only in TTL window resulting
                //       in to very slow IR/resync.
                continue;
            }            
            DebugPrintf(SV_LOG_ERROR, "%s FAILED error %s, for file %s, length %d, moreData %s, compressMode %d\n",
                FUNCTION_NAME, cxTransport->status(), fileName.c_str(), length, STRBOOL(moreData), compressMode);
            SendAbort(cxTransport, fileName);
            break;
        }
        else
        {
            if(m_bThrottlePersisted.load(boost::memory_order_consume))
            {
                ResetThrottleHealthIssue();
            }
        }
    } while (!retval && !CDPUtil::QuitRequested());

    return (retval == true);
}

// --------------------------------------------------------------------------
// sends file to location based on transport settings
// --------------------------------------------------------------------------
bool DataProtectionSync::SendFile(CxTransport::ptr cxTransport, const std::string & LocalFile, const std::string & uploadFilename, COMPRESS_MODE compressMode)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (!cxTransport->putFile(LocalFile, uploadFilename, compressMode)) {
        DebugPrintf(SV_LOG_ERROR, "FAILED DataProtectionSync::SendFile for uploadFilename %s, LocalFile %s, compressMode %d, with error: %s\n", 
                    uploadFilename.c_str(), LocalFile.c_str(), compressMode, cxTransport->status());
        return false;
    }
    return true;
}

// --------------------------------------------------------------------------
// abort send and remove the remote file
// --------------------------------------------------------------------------
bool DataProtectionSync::SendAbort(CxTransport::ptr cxTransport, std::string const & fileName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (!cxTransport->abortRequest(fileName)) {
        DebugPrintf(SV_LOG_ERROR, "FAILED DataProtectionSync::SendAbort cxTransport->SendAbort for file %s with error: %s\n", 
                    fileName.c_str(), cxTransport->status());
        //return false;
    }
    if (!cxTransport->deleteFile(fileName)) {
        DebugPrintf(SV_LOG_ERROR, "FAILED DataProtectionSync::SendAbort cxTransport->RemoveRemoteFile for filename %s with error %s\n",
                    fileName.c_str(), cxTransport->status());
        return false;
    }
    return true;
}

/* old function not removed since used by offload sync too */
bool DataProtectionSync::SetCheckPointOffsetWithRename(std::string const & agent,
                                                       SV_LONGLONG offset,
                                                       SV_LONGLONG bytes,
                                                       bool matched,
                                                       std::string const & name,
                                                       std::string const & newName)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return SendSetCheckPointOffset(agent, offset, bytes, matched, name, newName, std::string(), std::string());
}

/* old function not removed since used by offload sync too */
bool DataProtectionSync::SetCheckPointOffsetWithDelete(std::string const & agent,
                                                       SV_LONGLONG offset,
                                                       SV_LONGLONG bytes,
                                                       bool matched,
                                                       std::string const & name)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return SendSetCheckPointOffset(agent, offset, bytes, matched, std::string(), std::string(), name, std::string());
}

bool DataProtectionSync::SetCheckPointOffsetWithDelete(std::string const & agent,
                                                       SV_LONGLONG offset,
                                                       SV_LONGLONG bytes,
                                                       bool matched,
                                                       std::string const & delete1,
                                                       std::string const & delete2)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return SendSetCheckPointOffset(agent, offset, bytes, matched, std::string(), std::string(), delete1, delete2);
}


bool DataProtectionSync::SetFastSyncProgressBytes(const SV_LONGLONG &bytesApplied,
                                                  const SV_ULONGLONG &bytesMatched,
                                                  const std::string &resyncStatsJson)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string sourceHostId ;
    std::string destHostId ;
    std::string sourceDeviceName ;
    std::string destDeviceName ;
    int errorCode = -1;
    getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
    try
    {
        errorCode =  TheConfigurator->getVxAgent().SetFastSyncUpdateProgressBytesWithStats(
            sourceHostId,
            sourceDeviceName,
            destHostId,
            destDeviceName,
            bytesApplied,
            bytesMatched,
            m_JobId.c_str(),
            resyncStatsJson);
        DebugPrintf(SV_LOG_DEBUG, "sourceHostId %s sourceDeviceName %s destHostId %s \
                                  destDeviceName %s bytes" ULLSPEC "bytesMatched " ULLSPEC " jobId %s resyncStatsJson %s\n",
                    sourceHostId.c_str(),
                    sourceDeviceName.c_str(),
                    destHostId.c_str(),
                    destDeviceName.c_str(),
                    bytesApplied,
                    bytesMatched,
                    m_JobId.c_str(),
                    resyncStatsJson.c_str());

        if( errorCode != 0 )
        {
            DebugPrintf(SV_LOG_WARNING, "SetFastSyncUpdateFullSyncBytes failed.. This is not a error.\n");
            DebugPrintf(SV_LOG_DEBUG, "sourceHostId %s sourceDeviceName %s destHostId %s \
                                      destDeviceName %s bytes" ULLSPEC "bytesMatched " ULLSPEC " jobId %s resyncStatsJson %s error code %d\n",
                        sourceHostId.c_str(),
                        sourceDeviceName.c_str(),
                        destHostId.c_str(),
                        destDeviceName.c_str(),
                        bytesApplied,
                        bytesMatched,
                        m_JobId.c_str(),
                        resyncStatsJson.c_str(),
                        errorCode);
        }
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "SetFastSyncProgressBytes function call to CX failed with exception %s\n",
                     exception.what() );
    }
    catch( ... )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "SetFastSyncProgressBytes function call to CX failed with unknown errors\n" ) ;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return errorCode == 0;
}

bool DataProtectionSync::SetFastSyncMatchedBytes( SV_ULONGLONG bytesMatched, int during_resyncTransition)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string sourceHostId ;
    std::string destHostId ;
    std::string sourceDeviceName ;
    std::string destDeviceName ;
    int  errorCode = -1;
    getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
    try
    {
        errorCode = TheConfigurator->getVxAgent().SetFastSyncUpdateMatchedBytes( sourceHostId,
                                                                                 sourceDeviceName,
                                                                                 destHostId,
                                                                                 destDeviceName,
                                                                                 bytesMatched,
                                                                                 during_resyncTransition,
                                                                                 m_JobId.c_str() ) ;
        DebugPrintf(SV_LOG_DEBUG, "sourceHostId %s sourceDeviceName %s destDeviceName %s \
                                  fullSyncBytes" ULLSPEC "  jobId %s\n",
                    sourceHostId.c_str(),
                    sourceDeviceName.c_str(),
                    destDeviceName.c_str(),
                    bytesMatched,
                    m_JobId.c_str());

        if( errorCode != 0 )
        {
            DebugPrintf(SV_LOG_WARNING, "SetFastSyncMatchedBytes failed.. This is not a error.\n");
            DebugPrintf(SV_LOG_DEBUG, "sourceHostId %s sourceDeviceName %s destDeviceName %s \
                                      fullSyncBytes" ULLSPEC "  jobId %s error code %d\n",
                        sourceHostId.c_str(),
                        sourceDeviceName.c_str(),
                        destDeviceName.c_str(),
                        bytesMatched,
                        m_JobId.c_str(),
                        errorCode);

        }
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "SetFastSyncMatchedBytes function call to CX failed with exception %s\n",
                     exception.what() );
    }
    catch( ... )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "SetFastSyncMatchedBytes function call to CX failed with unknown errors\n" ) ;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return errorCode == 0;
}


bool DataProtectionSync::SetFastSyncFullyUnusedBytes( SV_ULONGLONG bytesunused)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    int  errorCode = -1;
    std::string sourceHostId ;
    std::string destHostId ;
    std::string sourceDeviceName ;
    std::string destDeviceName ;

    getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
    try
    {
        errorCode = TheConfigurator->getVxAgent().SetFastSyncFullyUnusedBytes( sourceHostId,
                                                                               sourceDeviceName,
                                                                               destHostId,
                                                                               destDeviceName,
                                                                               bytesunused,
                                                                               m_JobId.c_str() ) ;
        DebugPrintf(SV_LOG_DEBUG, "sourceHostId %s sourceDeviceName %s destDeviceName %s \
                                  unused bytes: " ULLSPEC "  jobId %s\n",
                    sourceHostId.c_str(),
                    sourceDeviceName.c_str(),
                    destDeviceName.c_str(),
                    bytesunused,
                    m_JobId.c_str());

        if( errorCode != 0 )
        {
            DebugPrintf(SV_LOG_ERROR, "SetFastSyncFullyUnusedBytes failed.\n");
            DebugPrintf(SV_LOG_ERROR, "sourceHostId %s sourceDeviceName %s destDeviceName %s \
                                      unused bytes: " ULLSPEC "  jobId %s error code %d\n",
                        sourceHostId.c_str(),
                        sourceDeviceName.c_str(),
                        destDeviceName.c_str(),
                        bytesunused,
                        m_JobId.c_str(),
                        errorCode);

        }
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "SetFastSyncFullyUnusedBytes function call to CX failed with exception %s\n",
                     exception.what() );
    }
    catch( ... )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "SetFastSyncFullyUnusedBytes function call to CX failed with unknown errors\n" ) ;
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return errorCode == 0;
}


bool DataProtectionSync::SetFastSyncFullSyncBytes(SV_LONGLONG fullSyncBytes)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string sourceHostId ;
    std::string destHostId ;
    std::string sourceDeviceName ;
    std::string destDeviceName ;
    int  errorCode = -1 ;
    getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
    try {
        errorCode = TheConfigurator->getVxAgent().SetFastSyncUpdateFullSyncBytes(sourceHostId,
                                                                                 sourceDeviceName,
                                                                                 destHostId,
                                                                                 destDeviceName,
                                                                                 fullSyncBytes,
                                                                                 m_JobId.c_str());
        DebugPrintf(SV_LOG_DEBUG, "src id %s src dev name %s dest id %s det dev name %s \
                                  fullSyncBytes" ULLSPEC " jobId %s\n",
                    sourceHostId.c_str(),
                    sourceDeviceName.c_str(),
                    destHostId.c_str(),
                    destDeviceName.c_str(),
                    fullSyncBytes,
                    m_JobId.c_str());

        if( errorCode != 0 )
        {
            DebugPrintf(SV_LOG_WARNING, "SetFastSyncUpdateFullSyncBytes failed.. This is not a error.\n");
            DebugPrintf(SV_LOG_DEBUG, "sourceHostId %s sourceDeviceName %s destDeviceName %s \
                                      fullSyncBytes" ULLSPEC " jobId %s error code %d\n",
                        sourceHostId.c_str(),
                        sourceDeviceName.c_str(),
                        destDeviceName.c_str(),
                        fullSyncBytes,
                        m_JobId.c_str(),
                        errorCode);

        }
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "SetFastSyncFullSyncBytes function call to CX failed with exception %s\n",
                     exception.what() );
    }
    catch( ... )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "SetFastSyncFullSyncBytes function call to CX failed with unknown errors\n" ) ;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return errorCode == 0;

}
/*End of Change*/
// --------------------------------------------------------------------------
// sends the last sync offset to the cx and also sets in the registry.
// --------------------------------------------------------------------------
bool DataProtectionSync::SendSetCheckPointOffset(std::string const & agent,
                                                 SV_LONGLONG offset,
                                                 SV_LONGLONG bytes,
                                                 bool matched,
                                                 std::string const & oldName,
                                                 std::string const & newName,
                                                 std::string const & deleteName1,
                                                 std::string const & deleteName2)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // TODO: shouldn't have to cast constness away
    int nReturn = -1 ;
    /*Added by BSR for Fast Sync TBC */
    try
    {
        if( m_Settings.syncMode == VOLUME_SETTINGS::SYNC_FAST_TBC  || m_Settings.syncMode == VOLUME_SETTINGS::SYNC_DIRECT )
        {
            std::string sourceHostId ;
            std::string destHostId ;
            std::string sourceDeviceName ;
            std::string destDeviceName ;

            getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;

            nReturn = TheConfigurator->getVxAgent().setResyncProgressFastsync( sourceHostId,
                                                                               sourceDeviceName,
                                                                               destHostId,
                                                                               destDeviceName,
                                                                               offset,
                                                                               bytes,
                                                                               matched,
                                                                               (m_JobId.empty()? "" : m_JobId.c_str()),
                                                                               (oldName.empty() ? "" : oldName.c_str()),
                                                                               (newName.empty()? "" : newName.c_str()),
                                                                               (deleteName1.empty()? "" : deleteName1.c_str()),
                                                                               (deleteName2.empty()? "" : deleteName2.c_str()) );

        }
        /*End of the change */
        else
        {
            nReturn = TheConfigurator->getVxAgent().setResyncProgress( m_Settings.deviceName,
                                                                       offset,
                                                                       bytes,
                                                                       matched,
                                                                       (m_JobId.empty()? "" : m_JobId.c_str()),
                                                                       (oldName.empty() ? "" : oldName.c_str()),
                                                                       (newName.empty()? "" : newName.c_str()),
                                                                       (deleteName1.empty()? "" : deleteName1.c_str()),
                                                                       (deleteName2.empty()? "" : deleteName2.c_str()),
                                                                       agent.c_str());
        }
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "setResyncProgroess function call to CX failed with exception %s\n",
                     exception.what() );
    }
    catch( ... )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "setResyncProgress function call to CX failed with unknown errors\n" ) ;
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return ( nReturn == 0 ) ;
}

/* Added by BSR for Fast Sync TBC */
int DataProtectionSync::setResyncTransitionStepOneToTwo( std::string const& syncNoMoreFile )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    std::string sourceHostId ;
    std::string destHostId ;
    std::string sourceDeviceName ;
    std::string destDeviceName ;
    int nReturn = -1 ;

    this->getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
    try
    {
        nReturn = TheConfigurator->getVxAgent().setResyncTransitionStepOneToTwo( sourceHostId,
                                                                                 sourceDeviceName,
                                                                                 destHostId,
                                                                                 destDeviceName,
                                                                                 JobId(),
                                                                                 syncNoMoreFile ) ;
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "setResyncTransitionStepOneToTwo failed with exception: %s\n",
                     exception.what() ) ;
    }
    catch(...)
    {
        DebugPrintf( SV_LOG_ERROR,
                     "setResyncTransitionStepOneToTwo failed with unknow reason" ) ;
    }
    DebugPrintf( SV_LOG_DEBUG, "ABOUT TO EXIT %s\n", FUNCTION_NAME ) ;
    return nReturn ;
}

bool DataProtectionSync::getVolumeCheckpoint( JOB_ID_OFFSET& jobIdOffset )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

    bool bReturn = true ;
    try
    {
        if( m_Settings.syncMode == VOLUME_SETTINGS::SYNC_FAST ||
            m_Settings.syncMode == VOLUME_SETTINGS::SYNC_OFFLOAD )
        {
            jobIdOffset =  TheConfigurator->getVxAgent().getVolumeCheckpoint( m_Settings.deviceName ) ;
        }
        else
        {
            std::string sourceHostId ;
            std::string destHostId ;
            std::string sourceDeviceName ;
            std::string destDeviceName ;

            this->getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;

            jobIdOffset = TheConfigurator->getVxAgent().getVolumeCheckpointFastsync( sourceHostId,
                                                                                     sourceDeviceName,
                                                                                     destHostId,
                                                                                     destDeviceName ) ;
        }
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "getVolumeCheckpoint call to CX failed with exception: %s\n",
                     exception.what() ) ;
        bReturn = false ;
    }
    catch(...)
    {
        DebugPrintf( SV_LOG_ERROR,
                     "getVolumeCheckpoint call to CX failed with unknown Exception \n" ) ;
        bReturn = false ;
    }
    return bReturn ;
}

void DataProtectionSync::getPairDetails( std::string& sourceHostId,
                                         std::string& sourceDeviceName,
                                         std::string& destHostId,
                                         std::string& destDeviceName )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME ) ;
    if( Source() )
    {
        sourceHostId = m_Settings.hostId ;
        sourceDeviceName = m_Settings.deviceName ;
        VOLUME_SETTINGS::endpoints_iterator volumeIterator = m_Settings.endpoints.begin() ;
        destHostId = (*volumeIterator).hostId ;
        destDeviceName = (*volumeIterator).deviceName ;
    }
    else
    {
        destHostId = m_Settings.hostId ;
        destDeviceName = m_Settings.deviceName ;
        VOLUME_SETTINGS::endpoints_iterator volumeIterator = m_Settings.endpoints.begin() ;
        sourceHostId = (*volumeIterator).hostId ;
        sourceDeviceName = (*volumeIterator).deviceName ;
    }
    DebugPrintf( SV_LOG_DEBUG, "ABOUT TO EXIT %s\n", FUNCTION_NAME ) ;
    return ;
}

bool DataProtectionSync::getResyncStartTimeStamp( ResyncTimeSettings& rts, std::string hostType )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

    bool bReturn  = true ;
    try
    {
        if( VOLUME_SETTINGS::SYNC_FAST_TBC == m_Settings.syncMode || VOLUME_SETTINGS::SYNC_DIRECT == m_Settings.syncMode)
        {
            std::string sourceHostId ;
            std::string destHostId ;
            std::string sourceDeviceName ;
            std::string destDeviceName ;

            this->getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;

            rts = TheConfigurator->getVxAgent().getResyncStartTimeStampFastsync( sourceHostId,
                                                                                 sourceDeviceName,
                                                                                 destHostId,
                                                                                 destDeviceName,
                                                                                 JobId() ) ;
        }
        else
        {
            rts.time = m_Settings.srcResyncStarttime;
            rts.seqNo = m_Settings.srcResyncStartSeq;

        }
    }
    catch ( const std::exception & exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "getResyncStartTimeStampFastsync() for the volume %s Failed wirh following error: %s\n",
                     Settings().deviceName.c_str(),
                     exception.what() )  ;
        bReturn = false ;
    }

    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED DataProtectionSync in ResyncStartNotify while calling getResyncStartTimeStamp for volume  %s\n",
                    Settings().deviceName.c_str());
        bReturn = false;
    }
    return bReturn ;
}

int DataProtectionSync::sendResyncStartTimeStamp( SV_ULONGLONG ts,
                                                  SV_ULONGLONG seq,
                                                  std::string hostType  )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

    int nReturn = -1 ;
    try
    {
        if( VOLUME_SETTINGS::SYNC_FAST_TBC == m_Settings.syncMode || VOLUME_SETTINGS::SYNC_DIRECT == m_Settings.syncMode )
        {
            std::string sourceHostId ;
            std::string destHostId ;
            std::string sourceDeviceName ;
            std::string destDeviceName ;

            this->getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
            nReturn = TheConfigurator->getVxAgent().sendResyncStartTimeStampFastsync( sourceHostId,
                                                                                      sourceDeviceName,
                                                                                      destHostId,
                                                                                      destDeviceName,
                                                                                      JobId(),
                                                                                      ts,
                                                                                      seq ) ;
        }
        else
        {
            nReturn = TheConfigurator->getVxAgent().sendResyncStartTimeStamp( m_Settings.deviceName,
                                                                              JobId(),
                                                                              ts,
                                                                              seq,
                                                                              hostType ) ;
        }
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "Failed to Send Resync Start Time Stamp. Call failed with: %s\n",
                     exception.what() ) ;
    }
    catch(...)
    {
        DebugPrintf( SV_LOG_ERROR,
                     "Send Resync Start Time Stamp call failed with Unknown Error\n" ) ;
    }
    return nReturn ;
}

bool DataProtectionSync::getResyncEndTimeStamp( ResyncTimeSettings& rts, std::string hostType )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

    bool bReturn = true ;
    try
    {
        if( VOLUME_SETTINGS::SYNC_FAST_TBC == m_Settings.syncMode || VOLUME_SETTINGS::SYNC_DIRECT == m_Settings.syncMode)
        {
            std::string sourceHostId ;
            std::string destHostId ;
            std::string sourceDeviceName ;
            std::string destDeviceName ;

            this->getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
            rts = TheConfigurator->getVxAgent().getResyncEndTimeStampFastsync( sourceHostId,
                                                                               sourceDeviceName,
                                                                               destHostId,
                                                                               destDeviceName,
                                                                               JobId() ) ;
        }
        else
        {
            rts.seqNo = m_Settings.srcResyncEndSeq;
            rts.time = m_Settings.srcResyncEndtime;
        }
    }
    catch( ContextualException &exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     " Get Resync End Time Stamp has failed with Exception: %s\n",
                     exception.what() ) ;
        bReturn = false ;
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED DataProtectionSync in ResyncEndNotify while calling getResyncEndTimeStampFastsync for volume  %s\n",
                    Settings().deviceName.c_str());
        bReturn = false;
    }
    return bReturn ;
}

int DataProtectionSync::sendResyncEndTimeStamp( SV_ULONGLONG ts,
                                                SV_ULONGLONG seq,
                                                std::string hostType )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;

    int nReturn = -1 ;
    try
    {
        if( VOLUME_SETTINGS::SYNC_FAST_TBC == m_Settings.syncMode || VOLUME_SETTINGS::SYNC_DIRECT == m_Settings.syncMode )
        {
            std::string sourceHostId ;
            std::string destHostId ;
            std::string sourceDeviceName ;
            std::string destDeviceName ;

            this->getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
            nReturn = TheConfigurator->getVxAgent().sendResyncEndTimeStampFastsync( sourceHostId,
                                                                                    sourceDeviceName,
                                                                                    destHostId,
                                                                                    destDeviceName,
                                                                                    JobId(),
                                                                                    ts,
                                                                                    seq ) ;
        }
        else
        {
            nReturn = TheConfigurator->getVxAgent().sendResyncEndTimeStamp( m_Settings.deviceName,
                                                                            JobId(),
                                                                            ts,
                                                                            seq,
                                                                            hostType ) ;
        }
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "Send Resync End Time Stamp has failed with exception %s\n",
                     exception.what() ) ;
    }
    catch( ... )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "Send Resync End Time Stamp has failed with unknow error\n" ) ;
    }
    return nReturn ;
}
/* End of the change */

int DataProtectionSync::setLastResyncOffsetForDirectSync(const SV_ULONGLONG &offset, const SV_ULONGLONG &filesystemunusedbytes)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    int errorCode = -1 ;
    try
    {
        if( VOLUME_SETTINGS::SYNC_DIRECT == m_Settings.syncMode )
        {
            std::string sourceHostId ;
            std::string destHostId ;
            std::string sourceDeviceName ;
            std::string destDeviceName ;

            this->getPairDetails( sourceHostId, sourceDeviceName, destHostId, destDeviceName ) ;
            std::stringstream msg;
            msg << "Direct Sync Pair Details are :"
                << sourceHostId <<" "
                << sourceDeviceName << " "
                << destHostId << " "
                << destDeviceName << " "
                << JobId() << " "
                << offset << " "
                << filesystemunusedbytes << "\n";
            DebugPrintf("%s",msg.str().c_str());
            errorCode =  TheConfigurator->getVxAgent().setLastResyncOffsetForDirectSync( sourceHostId,
                                                                                         sourceDeviceName,
                                                                                         destHostId,
                                                                                         destDeviceName,
                                                                                         JobId(),
                                                                                         offset,
                                                                                         filesystemunusedbytes) ;
        }
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "set Last Resync Offset For Direct Sync has failed with exception %s\n",
                     exception.what() ) ;
    }
    catch( ... )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "set Last Resync Offset For Direct Sync has failed with unknow error\n" ) ;
    }

    //5 means job id mismatch is there and pair settings or under change
    //Other errors we can ignore in case of direct sync
    return !( errorCode == 5 );
}


// --------------------------------------------------------------------------
// creates the local cache dirs
// --------------------------------------------------------------------------
bool DataProtectionSync::CreateFullLocalCacheDir(std::string const & cleanupPattern1, std::string const & cleanupPattern2)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_CacheDir = GetConfigurator().getCacheDirectory();
    m_CacheDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    m_CacheDir += GetConfigurator().getHostId();
    m_CacheDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    m_CacheDir += (GetVolumeDirName(m_Settings.deviceName));
    m_CacheDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;

    try
    {
        SVMakeSureDirectoryPathExists(m_CacheDir.c_str());
        securitylib::setPermissions(m_CacheDir, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
    }
    catch(std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to create %s with exception %s.\n", FUNCTION_NAME, m_CacheDir.c_str(), e.what());
        return false;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed to create %s with an unknown exception.\n", FUNCTION_NAME, m_CacheDir.c_str());
        return false;
    }

    CleanupDirectory(m_CacheDir.c_str(), cleanupPattern1.c_str());
    if (!cleanupPattern2.empty()) {
        CleanupDirectory(m_CacheDir.c_str(), cleanupPattern2.c_str());
    }

    return true;
}

bool DataProtectionSync::isCompressedFile(const std::string &filename)
{
    std::string::size_type idx = filename.rfind(".gz");
    if (std::string::npos == idx || (filename.length() - idx) > 3) {
        return false;
    }
    return true;
}

// --------------------------------------------------------------------------
// uncompress the file if it ends in .gz otherwise do nothing
// --------------------------------------------------------------------------
bool DataProtectionSync::UncompressIfNeeded(std::string& filePath)
{
    bool rv = false;
    DebugPrintf(SV_LOG_DEBUG,"Inside %s %s\n",FUNCTION_NAME, filePath.c_str());
    do
    {
        GzipUncompress uncompresser;
        std::string uncompressedFileName = filePath;

        std::string::size_type idx = filePath.rfind(".gz");
        if (std::string::npos == idx || (filePath.length() - idx) > 3) 
        {
            rv = true;
            break; // uncompress not needed;
        }

        uncompressedFileName.erase(idx, uncompressedFileName.length());
        if(!uncompresser.UnCompress(filePath,uncompressedFileName))
        {
            std::ostringstream msg;
            msg << "Uncompress failed for "
                << filePath
                << "\n";

            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());

            // remove partial uncompressed file
            ACE_OS::unlink(getLongPathName(uncompressedFileName.c_str()).c_str());
            rv = false;
            break;
        } else
        {
            // remove the .gz
            ACE_OS::unlink(getLongPathName(filePath.c_str()).c_str());
            filePath.erase(idx, filePath.length());
            rv = true;
            break;
        }

    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool DataProtectionSync::UncompressToMemory(BoostArray_t & buffer, SV_ULONG & buflen)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED FUNCTION %s\n", FUNCTION_NAME ) ;
    char * pgunzip =  NULL;
    SV_ULONG gunzipLen = 0;
    bool rv = false;

    GzipUncompress uncompresser;

    rv = uncompresser.UnCompress(buffer.get() , buflen, &pgunzip, gunzipLen);

    if(rv)
    {
        // reset the input buffer to point to uncompressed buffer
        buffer.reset(pgunzip, free);
        buflen = gunzipLen;
    } else
    {
        if(pgunzip) {
            free(pgunzip);
            pgunzip = NULL;
        }
    }

    DebugPrintf( SV_LOG_DEBUG, "EXITED FUNCTION %s\n", FUNCTION_NAME ) ;
    return rv;
}

bool DataProtectionSync::UncompressToDisk(BoostArray_t & buffer, const SV_ULONG & buflen, std::string &localFile)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED FUNCTION %s\n", FUNCTION_NAME ) ;
    bool rv = false;

    // remove the .gz
    std::string::size_type idx = localFile.rfind(".gz");
    localFile.erase(idx, localFile.length());

    GzipUncompress uncompresser;

    rv = uncompresser.UnCompress(buffer.get() , buflen, localFile);
    DebugPrintf( SV_LOG_DEBUG, "EXITED FUNCTION %s\n", FUNCTION_NAME ) ;
    return rv;
}

void DataProtectionSync::RemoveLocalFile(std::string const & name)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // PR#10815: Long Path support
    ACE_OS::unlink(getLongPathName(name.c_str()).c_str());
}


// --------------------------------------------------------------------------
// builds the outpost remote sync dir where data should be sent/read from
// TODO: should get this from configurator
// --------------------------------------------------------------------------
bool DataProtectionSync::BuildRemoteSyncDir()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // need to build something like
    // /home/svsystems/remotehoistid/F/resync/
    m_RemoteSyncDir = GetConfigurator().getResyncSourceDirectoryPath();

    m_RemoteSyncDir += '/';
    m_RemoteSyncDir += GetConfigurator().getHostId();
    m_RemoteSyncDir += '/';
    m_RemoteSyncDir += GetVolumeDirName(Settings().deviceName);
    m_RemoteSyncDir += SyncDir;

    return true;
}

// --------------------------------------------------------------------------
// gets the remote sync dir where data should be sent/read from
// TODO: should get this from configurator
// --------------------------------------------------------------------------
bool DataProtectionSync::GetSyncDir()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (m_Source)
    {

        /**
         * 1 to N sync:
         * Now this resync directory has to be got from 
         * the dataprotector settings not from configurator since configutor 
         * matches the first source. hence we see always first one's resync directory
         */

        m_RemoteSyncDir = Settings().resyncDirectory;
        if(m_RemoteSyncDir.empty())
        {
            return false;
        }
        m_RemoteSyncDir += '/';
        return true;
    }
    else
    {
        return BuildRemoteSyncDir();
    }
}

// --------------------------------------------------------------------------
// extracts the drives portion of remote sync info
// TODO: should get this from configurator
// --------------------------------------------------------------------------
bool DataProtectionSync::ExtractSyncDrives(boost::shared_array<char>& buffer, unsigned long * drives)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    char * pos = &buffer[0];
    if (m_Source) {
        if (0 != strncmp(pos, "drives=", strlen("drives="))) {
            return false;
        }
        pos += strlen("drives=");
    }

    char * end = strchr(pos, '&');
    if (0 != end) {
        (*end) = '\0';
    }

    // check for sync versions
    end = strchr(pos, ',');
    if (NULL == end) {
        end = strchr(pos, '\n');
        if (NULL == end) {
            end = pos + (strlen(pos) - 1);
            while (end != pos && !std::isprint((*end), std::cout.getloc())) {
                --end;
            }
            ++end;
        }
    }

    (*end) = '\0';

    (*drives) = boost::lexical_cast<unsigned long>(pos);

    return true;
}

/** Fast Sync TBC - BSR
 *  JobId is added as part of Volume Group Settings, If there is any change in the
 *  settings dataprotection would be exited itself, so need to have separate calls for the same.
 **/
//bool DataProtectionSync::GetRemoteJobId()
//{
//      DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
//    return GetRemoteJobId(m_JobId);
//}

//bool DataProtectionSync::GetRemoteJobId(std::string& jobId)
//{
//      DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
//    jobId = TheConfigurator->getVxAgent().getTargetReplicationJobId( m_Settings.deviceName );
//    return true;
//}


bool DataProtectionSync::DoTransportOp(const TransportOp &top, CxTransport::ptr &cxTransport)
{
    bool bopdone = false;
    std::stringstream msg;

    msg << "For volume " << Settings().deviceName << ", ";
    /* TODO: use elseif construction instead of switch */
    if (TransportOp::DeleteFile == top.m_op)
    {
        for (unsigned int i = 0; (i < top.m_nRetries) && (!QuitRequested(0)); i++)
        {
            if (!cxTransport->deleteFile(top.m_fileName))
            {
                std::stringstream errmsg;
                errmsg << msg.str();
                errmsg << "could not delete file " << top.m_fileName
                       << ". Transport status: " << cxTransport->status() << '.';

                if ((i + 1) != top.m_nRetries)
                {
                    errmsg << "Retrying after ";
                    if (top.m_shouldReinit)
                    {
                        errmsg << "reinitialization of cxtransport, ";
                        cxTransport.reset(new CxTransport(this->Protocol(),
                                                          this->TransportSettings(),
                                                          this->Secure(),
                                                          m_cfsPsData.m_useCfs,
                                                          m_cfsPsData.m_psId));
                    }
                    errmsg << top.m_RetryDelay << " seconds delay.";
                    QuitRequested(top.m_RetryDelay);
                }
                DebugPrintf(SV_LOG_ERROR, "%s\n", errmsg.str().c_str());
                continue;
            }
            bopdone = true;
            break;
        }
    }
    else
    {
        msg << "unknown transport operation specified";
        DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());
    }

    return bopdone;
}

// send resync start time stamp
bool DataProtectionSync::ResyncStartNotify() // TODO: nichougu - add out error string param for failure case - same for rest similar functions
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool retVal = true;
    ResyncTimeSettings rts;
    do
    {
        std::string devNameForDriverInput = Settings().deviceName;
#ifdef SV_UNIX
        devNameForDriverInput = GetDiskNameForPersistentDeviceName(Settings().deviceName, m_VolumesCache.m_Vs);
#endif

        DpDriverComm driverComm(Settings().isFabricVolume() ?
            Settings().sanVolumeInfo.virtualName :
            devNameForDriverInput,
            Settings().devicetype,
            Settings().deviceName);

        // do not send start time stamp to cx if the other side is not upgraded
        if (Settings().OtherSiteCompatibilityNum < RequiredCompatibility())
        {
            DebugPrintf(SV_LOG_WARNING, "%s: Resync Start Timestamp for %s is not sent since other side \
                is not upgraded\n", FUNCTION_NAME, Settings().deviceName.c_str());
            break;
        }

        if (IsResyncStartTimeStampAlreadySent(rts))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Resync Start Timestamp for %s is already sent.\n", FUNCTION_NAME, Settings().deviceName.c_str());
            break;
        }

        ClearDiffsState_t st = CanClearDifferentials();

        if (DontKnowClearDiffsState == st)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: ClearDiffsState not found %d.\n", FUNCTION_NAME, st);
            retVal = false;
            break;
        }

        bool canClearDiffs = (ClearDiffs == st);
        if (!IsNoDataTransferSync() && canClearDiffs)
        {
            driverComm.ClearDifferentials();
            driverComm.StartFiltering(Settings(), &m_VolumesCache.m_Vs);
        }
         
        if (IsNoDataTransferSync())
        {
            getResyncTimeSettingsForNoDataTransfer(rts);
        }
        else if (!driverComm.ResyncStartNotify(rts))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: driver failed in ResyncStartNotify.\n", FUNCTION_NAME);
            retVal = false;
            break;
        }

        if (!NotifyResyncStartToControlPath(rts, canClearDiffs))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: NotifyResyncStartToControlPath failed.\n", FUNCTION_NAME);
            retVal = false;
            break;
        }

        DebugPrintf(SV_LOG_ALWAYS, "%s: Resync start for %s TS:" ULLSPEC" Seq:" ULLSPEC"\n", FUNCTION_NAME, Settings().deviceName.c_str(), rts.time, rts.seqNo);

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retVal;
}

bool DataProtectionSync::StartProactor(SVProactorTask*& proactorTask, ACE_Thread_Manager& threadManager)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        if(!useNewApplyAlgorithm() || !AsyncOpEnabled())
        {
            rv = true;
            break;
        }

        if(!proactorTask)
        {
            proactorTask = new (nothrow) SVProactorTask(&threadManager);
            if(!proactorTask)
            {
                DebugPrintf(SV_LOG_ERROR, "proactor creation failed.\n");
                rv = false;
                break;
            }

            proactorTask ->open();
            proactorTask -> waitready();
        }
    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool DataProtectionSync::StopProactor(SVProactorTask*& proactorTask, ACE_Thread_Manager& threadManager)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {
        if(!useNewApplyAlgorithm() || !AsyncOpEnabled())
        {
            rv = true;
            break;
        }

        if(proactorTask)
        {
            ACE_Proactor::end_event_loop();
            threadManager.wait_task(proactorTask);
            ACE_Proactor::instance( ( ACE_Proactor* )0 );
            delete proactorTask;
            proactorTask = NULL;
        }
    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool DataProtectionSync::ResyncEndNotify()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool rv = true;
    ResyncTimeSettings rts;
    do
    {
        if (IsResyncEndTimeStampAlreadySent(rts))
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Resync End Timestamp for %s is already sent.\n", FUNCTION_NAME, Settings().deviceName.c_str());
            break;
        }

        std::string devNameForDriverInput = Settings().deviceName;
#ifdef SV_UNIX
        devNameForDriverInput = GetDiskNameForPersistentDeviceName(Settings().deviceName, m_VolumesCache.m_Vs);
#endif

        DpDriverComm driverComm(Settings().isFabricVolume() ?
            Settings().sanVolumeInfo.virtualName :
            devNameForDriverInput,
            Settings().devicetype,
            Settings().deviceName);

        if (IsNoDataTransferSync())
        {
            getResyncTimeSettingsForNoDataTransfer(rts);
        }
        else if (!driverComm.ResyncEndNotify(rts))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: driver failed in ResyncEndNotify.\n", FUNCTION_NAME);
            rv = false;
            break;
        }

        /* Added this function that will just
         * return true in linux and windows,
         * but in sun, since driver is
         * not yet functional, it calculates
         * rts time and return true if
         * time calculation was successful
         * This call and the function GenerateResyncTimeIfReq
         * has to be thrown away once driver is functional
         */

         /**

            if (!GenerateResyncTimeIfReq(rts))
            {
            return false;
            }

         */


        if (!NotifyResyncEndToControlPath(rts))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: NotifyResyncEndToControlPath failed.\n", FUNCTION_NAME);
            rv = false;
            break;
        }

        DebugPrintf(SV_LOG_ALWAYS, "Resync end for %s TS:" ULLSPEC" Seq:" ULLSPEC".\n", Settings().deviceName.c_str(), rts.time, rts.seqNo);

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return rv;
}

ClearDiffsState_t DataProtectionSync::CanClearDifferentials()
{
    ClearDiffsState_t st = DontKnowClearDiffsState;
    try
    {
        bool bReturn = TheConfigurator->getVxAgent().canClearDifferentials(Settings().deviceName);
        st = bReturn ? ClearDiffs : DontClearDiffs;
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "canClearDifferentials call to CX failed with exception: %s\n",
                     exception.what() ) ;
    }
    catch( ... )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "canClearDifferentials call to CX failed with unknown error \n" ) ;
    }
    return st ;
}



void DataProtectionSync::openCheckumsFile()
{
    if(GetConfigurator().getDICheck() )
    {
        std::string filename ;
        std::string deviceName = Settings().deviceName ;

        replace(deviceName.begin(), deviceName.end(), ':' , '_');
        replace(deviceName.begin(), deviceName.end(), '/' , '_');
        replace(deviceName.begin(), deviceName.end(), '\\' , '_');
        replace(deviceName.begin(), deviceName.end(), ' ' , '_');

        filename = GetConfigurator().getTargetChecksumsDir() ;
        filename += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        filename += "checksums" ;
        SVMakeSureDirectoryPathExists(filename.c_str()) ;
        filename += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        filename += Resync_Folder;
        SVMakeSureDirectoryPathExists(filename.c_str()) ;
        filename += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        filename += deviceName;
        SVMakeSureDirectoryPathExists(filename.c_str()) ;
        filename += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        filename += Settings().jobId ;
        filename += "_checksums.log" ;

// PR#10815: Long Path support
#ifdef WIN32
        m_dumpHcdsHandle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), O_WRONLY| O_APPEND | O_CREAT, ACE_DEFAULT_FILE_PERMS );
#else
        m_dumpHcdsHandle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), O_WRONLY| O_APPEND | O_CREAT, 0644);
#endif
        if( m_dumpHcdsHandle == ACE_INVALID_HANDLE )
        {
            std::stringstream msg;
            msg << "FAILED FastSync::Failed to open " << filename << " in append mode. errno: " << ACE_OS::last_error()
                << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            throw DataProtectionException( msg.str().c_str() ) ;
        }
    }
}
void DataProtectionSync::dumpHashesToFile(ACE_HANDLE handle, std::string str)
{
    AutoGuard guard(m_dumpHasheslock) ;
    ssize_t sizeWritten ;
    if( (sizeWritten = ACE_OS::write(handle, str.c_str(), str.length())) != str.length() )
    {
        DebugPrintf(SV_LOG_ERROR, "Requested Write Size: %d. Actually Written: %d. Error %d\n", str.length(), sizeWritten, ACE_OS::last_error()) ;
    }

}

DataProtectionSync::~DataProtectionSync()
{
    if (m_pClusterBitmapCustomizationInfos)
    {
        delete m_pClusterBitmapCustomizationInfos;
    }
    if(!m_SyncThroughThread)
    {
        CDPUtil::UnInitQuitEvent();
    }
    if( m_dumpHcdsHandle != ACE_INVALID_HANDLE )
    {
        ACE_OS::close(m_dumpHcdsHandle) ;
    }
}


unsigned int DataProtectionSync::GetEndianTagSize(void)
{
    return m_endianTagSize;
}

void DataProtectionSync::SetDeviceType(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    cdp_volume_t::CDP_VOLUME_TYPE devType;
    if (!m_Source && m_Settings.isAzureDataPlane()){
        devType = cdp_volume_t::CDP_DUMMY_VOLUME_TYPE;
    }
    else {
        devType = ((VolumeSummary::DISK == Settings().devicetype) ? cdp_volume_t::CDP_DISK_TYPE : cdp_volume_t::CDP_REAL_VOLUME_TYPE);
    }

    cdp_volume_t::CDP_VOLUME_TYPE effectivetype = (cdp_volume_t::CDP_DISK_TYPE == devType) ? cdp_volume_t::GetCdpVolumeTypeForDisk() : devType;
    m_deviceType = effectivetype;
    
    DebugPrintf(SV_LOG_DEBUG, "Device %s type = %s\n", 
        Settings().deviceName.c_str(), cdp_volume_t::GetStrVolumeType(effectivetype).c_str());


    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DataProtectionSync::SetVolumesCache(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    const int WAIT_SECS_FOR_CACHE = 10;

    if (!Source() && m_Settings.isInMageDataPlane() && (m_Settings.transportProtocol == TRANSPORT_PROTOCOL_BLOB))
    {
#ifdef SV_WINDOWS
        HypervisorInfo_t hypervinfo;
        if (HypervisorInfo_t::HypervisorStateUnknown == hypervinfo.state)
        {
            GetHypervisorInfo(hypervinfo);
        }

        // volume information
        VolumeDynamicInfos_t volumeDynamicInfos;

        VolumeInfoCollector volumeCollector((DeviceNameTypes_t)GetDeviceNameTypeToReport(hypervinfo.name));
        volumeCollector.GetVolumeInfos(m_VolumesCache.m_Vs, volumeDynamicInfos, true);
#endif
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    do
    {
        DataCacher dataCacher;
        if (dataCacher.Init()) {
            DataCacher::CACHE_STATE cs = dataCacher.GetVolumesFromCache(m_VolumesCache);
            if (DataCacher::E_CACHE_VALID == cs) {
                DebugPrintf(SV_LOG_DEBUG, "volume summaries are present in local cache.\n");
                break;
            }
            else if (DataCacher::E_CACHE_DOES_NOT_EXIST == cs) {
                DebugPrintf(SV_LOG_DEBUG, "volume summaries are not present in local cache. Waiting for it to be created.\n");
            }
            else {
                DebugPrintf(SV_LOG_ERROR, "Dataprotection failed getting volumeinfocollector cache with error: %s for volume %s.@LINE %d in FILE %s.\n",
                    dataCacher.GetErrMsg().c_str(), Settings().deviceName.c_str(), LINE_NO, FILE_NAME);
            }
        }
        else {
            DebugPrintf(SV_LOG_ERROR, "Dataprotection failed to initialize data cacher to get volumes cache for volume %s.@LINE %d in FILE %s.\n",
                Settings().deviceName.c_str(), LINE_NO, FILE_NAME);
        }
    } while (!QuitRequested(WAIT_SECS_FOR_CACHE));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool DataProtectionSync::IsTransportProtocolAzureBlob() const
{
    return m_Protocol == TRANSPORT_PROTOCOL_BLOB;
}

bool DataProtectionSync::IsResyncStartTimeStampAlreadySent(ResyncTimeSettings &rts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool retVal = false;

    if (GetConfigurator().getIsCXPatched())
    {
        // check if we have already sent the resync start timestamp
        if (!getResyncStartTimeStamp(rts))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed in getResyncStartTimeStamp for %s.\n", FUNCTION_NAME, Settings().deviceName.c_str());
            retVal = false;
        }
        else if (rts.time)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Resync Start Timestamp for %s has already been sent as TS:" ULLSPEC" Seq:" ULLSPEC".\n",
                FUNCTION_NAME, Settings().deviceName.c_str(), rts.time, rts.seqNo);
            retVal = true;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Resync Start Timestamp for %s is not sent.\n",
                FUNCTION_NAME, Settings().deviceName.c_str());
            retVal = false;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return retVal;
}

bool DataProtectionSync::NotifyResyncStartToControlPath(const ResyncTimeSettings &rts, const bool canClearDiffs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool retVal = true;
    LocalConfigurator lc;
    std::string outFileName = lc.getCacheDirectory() + ACE_DIRECTORY_SEPARATOR_STR_A + "ResyncStartNotify.log";

    std::string outFileNameLock = outFileName + ".lck";
    // PR#10815: Long Path support
    ACE_File_Lock lock(getLongPathName(outFileNameLock.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS, 0);
    ACE_Guard<ACE_File_Lock> guard(lock);

    securitylib::setPermissions(outFileName);
    std::ofstream out(outFileName.c_str(), std::ios::app);
    if (!out)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to open local log file %s for tracking resyncCounter. error no: %d\n",
            outFileName.c_str(), ACE_OS::last_error());
    }
    else
    {
        time_t ltime;
        struct tm *today;
        time(&ltime);

#ifdef SV_WINDOWS
        today = localtime(&ltime);
#else
        struct tm t;
        localtime_r(&ltime, &t);
        today = &t;
#endif

        std::vector<char> vCurrentTime(70, '\0');
        inm_sprintf_s(&vCurrentTime[0], vCurrentTime.size(), "(%02d-%02d-20%02d %02d:%02d:%02d): ",
            today->tm_mon + 1,
            today->tm_mday,
            today->tm_year - 100,
            today->tm_hour,
            today->tm_min,
            today->tm_sec
        );

        out.seekp(0, std::ios::end);
        out << "LocalTimeStamp" << &vCurrentTime[0] << "SrcVol: " << Settings().deviceName << ", JobId: " << JobId()
            << ", canClearDiffs: " << canClearDiffs << ", ResyncTS: " << rts.time << ", ResyncSeqNo: "
            << rts.seqNo << std::endl;
        out.close();
    }

    if (0 != sendResyncStartTimeStamp(rts.time, rts.seqNo))
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << " Error Message: "
            << "failed to send resync start time stamp, will try again on next invocation\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        retVal = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return retVal;
}

bool DataProtectionSync::IsResyncEndTimeStampAlreadySent(ResyncTimeSettings &rts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool retVal = false;

    if (GetConfigurator().getIsCXPatched())
    {
        // check if we have already sent the resync end timestamp
        if (!getResyncEndTimeStamp(rts))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed in getResyncEndTimeStamp for %s.\n", FUNCTION_NAME, Settings().deviceName.c_str());
            retVal = false;
        }
        else if (rts.time)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Resync End Timestamp for %s has already been sent as TS:" ULLSPEC" Seq:" ULLSPEC".\n",
                FUNCTION_NAME, Settings().deviceName.c_str(), rts.time, rts.seqNo);
            retVal = true;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Resync End Timestamp for %s is not sent.\n",
                FUNCTION_NAME, Settings().deviceName.c_str());
            retVal = false;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return retVal;
}

bool DataProtectionSync::NotifyResyncEndToControlPath(const ResyncTimeSettings &rts)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool retVal = true;
    if (0 != sendResyncEndTimeStamp(rts.time, rts.seqNo))
    {
        stringstream l_stderr;
        l_stderr << "Error detected  " << "in Function:" << FUNCTION_NAME
            << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
            << " Error Message: "
            << "failed to send resync end time stamp, will try again on next invocation\n";

        DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
        retVal = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return retVal;
}

void DataProtectionSync::SendAlert(SV_ALERT_TYPE alertType, SV_ALERT_MODULE alertingModule, const InmAlert &alert) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED: %s\n", FUNCTION_NAME);

    if (SendAlertToCx(alertType, alertingModule, alert))
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Successfully sent alert %s.\n", FUNCTION_NAME, alert.GetID().c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to send alert %s.\n", FUNCTION_NAME, alert.GetID().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED: %s\n", FUNCTION_NAME);
}

