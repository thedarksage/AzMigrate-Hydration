#include <iostream>
#include <sstream>

#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_time.h>
#include <ace/OS_NS_sys_time.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "host.h"
#include "logger.h"
#include "cdputil.h"
#include "svtypes.h"
#include "dataprotectionRcm.h"
#include "dataprotectionexception.h"

DataProtectionRcm::DataProtectionRcm(const DpRcmProgramArgs &args,
    const boost::shared_ptr<ResyncStateManager> resyncStateManager) :
    m_Args(args),
    m_ResyncStateManager(resyncStateManager),
    m_RcmConfigurator(RcmClientLib::RcmConfigurator::getInstance())
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ResyncRcmSettings::GetInstance(m_ResyncRcmSettings);
    m_bLogFileXfer = m_RcmConfigurator->getLogFileXfer();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

DataProtectionRcm::~DataProtectionRcm()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    /* Order is important since after all uploads and download finish,
     * stop logging thread */
    if (m_CxTransportXferLogMonitor)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Waiting for file tal log monitor\n", FUNCTION_NAME);
        cxTransportStopXferLogMonitor();
        m_CxTransportXferLogMonitor->join();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// Returns log path for current dataprotectionRcm instance
//
std::string DataProtectionRcm::GetLogDir()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    boost::filesystem::path logDir(m_RcmConfigurator->getLogPathname());
    logDir /= m_Args.GetLogDir();
    
    return logDir.string();
}

//
// Prepares input for resync tasks and start resync
//
void DataProtectionRcm::Protect()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // Note: acquire lock only in Protect function and no in any other internal functions called by Protect to avoid recursive lock by same thread
    LockGuard lock(m_Lock);

    //  - Process sync settings to initialize m_HostVolGroupSettings
    //  - Create fastsyncRcm instance
    //  - Setup file transfer log
    //  - Start fastsync and return
    //      - Main thread will continue with state manager and also wait for resync tasks

    ProcessHostVolumeGroupSettings(m_HostVolGroupSettings);

    Create();

    SetUpFileXferLog(m_HostVolGroupSettings.volumeGroups.begin()->volumes.begin()->second);

    Start();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DataProtectionRcm::ProcessVolumeGroupSettingsForSource(VOLUME_GROUP_SETTINGS& volGroupSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_Args.IsSourceSide())
    {
        throw INMAGE_EX("Not a source side.");
    }

	const ResyncStateInfo resyncState = m_ResyncStateManager->GetResyncState();

    volGroupSettings.id = 0;
    volGroupSettings.direction = SOURCE;
    volGroupSettings.status = PROTECTED;
    
    if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_PROCESS_SERVER))
    {
        RcmClientLib::ProcessServerTransportSettings processServerDetails =
            JSON::consumer<RcmClientLib::ProcessServerTransportSettings>::convert(m_ResyncRcmSettings->GetDataPathTransportSettings(), true);

        volGroupSettings.transportSettings.ipAddress = processServerDetails.GetIPAddress();
        volGroupSettings.transportSettings.sslPort = boost::lexical_cast<std::string>(processServerDetails.Port);
    }
    else if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB))
    {
        volGroupSettings.transportSettings.ipAddress =
            m_ResyncRcmSettings->GetSourceAgentSyncSettings().TransportSettings.BlobContainerSasUri;
    }

    // Fill VOLUME_SETTINGS
    VOLUME_SETTINGS vs;
    vs.deviceName = m_Args.GetDiskId();
    vs.hostname = Host::GetInstance().GetHostName();
    vs.hostId = m_RcmConfigurator->getHostId();
    vs.secureMode = VOLUME_SETTINGS::SECURE_MODE_SECURE;
    vs.sourceToCXSecureMode = VOLUME_SETTINGS::SECURE_MODE_SECURE;
    vs.syncMode = VOLUME_SETTINGS::SYNC_FAST_TBC;
    if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_PROCESS_SERVER))
    {
        vs.transportProtocol = TRANSPORT_PROTOCOL_HTTP;

        if (m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder.empty())
        {
            std::stringstream sserr;
            sserr << "LogFolder received is blank for " << m_ResyncRcmSettings->GetDataPathTransportType();
            DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());
            throw INMAGE_EX(sserr.str().c_str());
        }
        vs.resyncDirectory = m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder;
    }
    else if(boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB))
    {
        vs.transportProtocol = TRANSPORT_PROTOCOL_BLOB;
    }

	std::map<std::string, std::string>::const_iterator sourceDiskRawSizeItr = 
		resyncState.Properties.find(SourcePropertyDiskRawSize);
	if (sourceDiskRawSizeItr == resyncState.Properties.end())
	{
		std::stringstream sserr;
		sserr << SourcePropertyDiskRawSize << " not found in source state.";
		DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());
		throw INMAGE_EX(sserr.str().c_str());
	}
	vs.sourceCapacity = boost::lexical_cast<SV_ULONGLONG>(sourceDiskRawSizeItr->second.c_str());
    
    vs.srcResyncStarttime = m_ResyncStateManager->GetResyncState().ResyncStartTimestamp;
    vs.srcResyncEndtime = m_ResyncStateManager->GetResyncState().ResyncEndTimestamp;
    vs.OtherSiteCompatibilityNum = CurrentCompatibilityNum();
    vs.sourceOSVal = OperatingSystem::GetInstance().GetOsVal();
    vs.compressionEnable = COMPRESS_NONE;
    vs.jobId = m_ResyncStateManager->GetReplicationSessionId();
    
    // VOLUME_SETTINGS::endpoints target details is not required for resync
    
    vs.pairState = VOLUME_SETTINGS::PAIR_PROGRESS;
    vs.diffsPendingInCX = 0;
    vs.currentRPO = 0;
    vs.applyRate = 0;
    vs.srcResyncStartSeq = m_ResyncStateManager->GetResyncState().ResyncStartTimestampSeqNo;
    vs.srcResyncEndSeq = m_ResyncStateManager->GetResyncState().ResyncEndTimestampSeqNo;
    vs.sourceRawSize = boost::lexical_cast<SV_ULONGLONG>(sourceDiskRawSizeItr->second.c_str());
    vs.srcStartOffset = 0;
    vs.devicetype = 0;

    // Fill options map
    {
        vs.options[NsVOLUME_SETTINGS::NAME_RESYNC_COPY_MODE] = NsVOLUME_SETTINGS::VALUE_FULL;
        vs.options[NsVOLUME_SETTINGS::NAME_RAWSIZE] = sourceDiskRawSizeItr->second;
        vs.options[NsVOLUME_SETTINGS::NAME_PROTECTION_DIRECTION] = NsVOLUME_SETTINGS::VALUE_FORWARD;
        vs.options[NsVOLUME_SETTINGS::NAME_TARGET_DATA_PLANE] = m_RcmConfigurator->IsAzureToOnPremReplication() ?
            NsVOLUME_SETTINGS::VALUE_INMAGE_DATA_PLANE : NsVOLUME_SETTINGS::VALUE_AZURE_DATA_PLANE;
    }

    // Update hostVolumeGroupSettings
    volGroupSettings.volumes.clear();
    volGroupSettings.volumes[vs.deviceName] = vs;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DataProtectionRcm::ProcessVolumeGroupSettingsForTarget(VOLUME_GROUP_SETTINGS& volGroupSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_Args.IsSourceSide())
    {
        throw INMAGE_EX("Not a target side.");
    }

	const ResyncStateInfo sourceResyncState = m_ResyncStateManager->GetPeerResyncState();

    volGroupSettings.id = 0;
    volGroupSettings.direction = TARGET;
    volGroupSettings.status = PROTECTED;

    if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_PROCESS_SERVER))
    {
        RcmClientLib::ProcessServerTransportSettings processServerDetails =
            JSON::consumer<RcmClientLib::ProcessServerTransportSettings>::convert(m_ResyncRcmSettings->GetDataPathTransportSettings(), true);

        volGroupSettings.transportSettings.ipAddress = processServerDetails.GetIPAddress();
        volGroupSettings.transportSettings.sslPort = boost::lexical_cast<std::string>(processServerDetails.Port);
    }
    else if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB))
    {
        volGroupSettings.transportSettings.ipAddress =
            m_ResyncRcmSettings->GetSourceAgentSyncSettings().TransportSettings.BlobContainerSasUri;
    }

    // Fill VOLUME_SETTINGS
    VOLUME_SETTINGS vs;
    std::string deviceName = "C:\\" + m_Args.GetSourceHostId() + "\\" + m_Args.GetDiskId();
    boost::replace_all(deviceName, "/", "\\");
    boost::replace_all(deviceName, "\\\\", "\\");
    vs.deviceName = deviceName;
    vs.hostname = Host::GetInstance().GetHostName(); // target host name
    vs.secureMode = VOLUME_SETTINGS::SECURE_MODE_SECURE;
    vs.sourceToCXSecureMode = VOLUME_SETTINGS::SECURE_MODE_SECURE;
    vs.syncMode = VOLUME_SETTINGS::SYNC_FAST_TBC;

    if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_PROCESS_SERVER))
    {
        vs.transportProtocol = TRANSPORT_PROTOCOL_HTTP;

        if (m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder.empty())
        {
            std::stringstream sserr;
            sserr << "LogFolder received is blank for " << m_ResyncRcmSettings->GetDataPathTransportType();
            DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());
            throw INMAGE_EX(sserr.str().c_str());
        }
        vs.resyncDirectory = m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder;
    }
    else if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_LOCAL_FILE))
    {
        vs.transportProtocol = TRANSPORT_PROTOCOL_FILE;

        if (m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder.empty())
        {
            std::stringstream sserr;
            sserr << "LogFolder received is blank for " << m_ResyncRcmSettings->GetDataPathTransportType();
            DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());
            throw INMAGE_EX(sserr.str().c_str());
        }
        vs.resyncDirectory = m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder;
    }
    else if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB))
    {
        vs.transportProtocol = TRANSPORT_PROTOCOL_BLOB;
    }

	std::map<std::string, std::string>::const_iterator sourceDiskRawSizeItr = 
		sourceResyncState.Properties.find(SourcePropertyDiskRawSize);
	if (sourceDiskRawSizeItr == sourceResyncState.Properties.end())
	{
		std::stringstream sserr;
		sserr << SourcePropertyDiskRawSize << " not found in source state.";
		DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());
		throw INMAGE_EX(sserr.str().c_str());
	}
    vs.sourceCapacity = boost::lexical_cast<SV_ULONGLONG>(sourceDiskRawSizeItr->second.c_str());

    vs.srcResyncStarttime = m_ResyncStateManager->GetPeerResyncState().ResyncStartTimestamp;
    vs.srcResyncStartSeq = m_ResyncStateManager->GetPeerResyncState().ResyncStartTimestampSeqNo;
    vs.srcResyncEndtime = m_ResyncStateManager->GetPeerResyncState().ResyncEndTimestamp;
    vs.OtherSiteCompatibilityNum = CurrentCompatibilityNum();
    vs.resyncRequiredFlag = false;
    vs.sourceOSVal = OperatingSystem::GetInstance().GetOsVal(); // TODO: need to be passed by rcm in settings
    vs.compressionEnable = COMPRESS_NONE;
    vs.jobId = m_ResyncStateManager->GetReplicationSessionId();

    // Update required VOLUME_SETTINGS details for endpoint source
    {
        VOLUME_SETTINGS vsep;
        vsep.deviceName = m_Args.GetDiskId();
        vsep.devicetype = 0;
		std::map<std::string, std::string>::const_iterator sourceHostNameItr =
			sourceResyncState.Properties.find(SourcePropertyHostName);
		if (sourceHostNameItr == sourceResyncState.Properties.end())
		{
			std::stringstream sserr;
			sserr << SourcePropertyHostName << " not found in source state.";
			DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());
			throw INMAGE_EX(sserr.str().c_str());
		}
		vsep.hostname = sourceHostNameItr->second; // Source host name
        vsep.hostId = m_Args.GetSourceHostId(); // Source host Id
		vsep.sourceCapacity = boost::lexical_cast<SV_ULONGLONG>(sourceDiskRawSizeItr->second.c_str()); // Source disk raw size
        vsep.options[NsVOLUME_SETTINGS::NAME_RAWSIZE] = sourceDiskRawSizeItr->second;
        vs.endpoints.push_back(vsep);
    }

    vs.pairState = VOLUME_SETTINGS::PAIR_PROGRESS;
    vs.diffsPendingInCX = 0;
    vs.currentRPO = 0;
    vs.applyRate = 0;
    vs.srcResyncStartSeq = m_ResyncStateManager->GetPeerResyncState().ResyncStartTimestampSeqNo;
    vs.srcResyncEndSeq = m_ResyncStateManager->GetPeerResyncState().ResyncEndTimestampSeqNo;
    vs.sourceRawSize = boost::lexical_cast<SV_ULONGLONG>(sourceDiskRawSizeItr->second.c_str());
    vs.srcStartOffset = 0;
    vs.devicetype = 0;

    // Fill options map
    {
        vs.options[NsVOLUME_SETTINGS::NAME_RESYNC_COPY_MODE] = NsVOLUME_SETTINGS::VALUE_FULL;
        vs.options[NsVOLUME_SETTINGS::NAME_RAWSIZE] = sourceDiskRawSizeItr->second;
        vs.options[NsVOLUME_SETTINGS::NAME_PROTECTION_DIRECTION] = NsVOLUME_SETTINGS::VALUE_FORWARD;
        vs.options["useCfs"] = "0";
        vs.options["psId"] = m_RcmConfigurator->getHostId(); // TODO: reading hostID in config file on apliance - this should be part of global config
                                                             // Currently reading MachineIdentifier from rcmreplicationagent.json from registry key
                                                             // "Azure Site Recovery On-Premise to Azure Replication Agent" => "ConfigPath"
        vs.options[NsVOLUME_SETTINGS::NAME_TARGET_DATA_PLANE] = m_Args.IsOnpremReprotectTargetSide() ?
            NsVOLUME_SETTINGS::VALUE_INMAGE_DATA_PLANE : NsVOLUME_SETTINGS::VALUE_AZURE_DATA_PLANE;
    }

    // Update hostVolumeGroupSettings
    volGroupSettings.volumes.clear();
    volGroupSettings.volumes[vs.deviceName] = vs;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// Read settings from rcm configurator and prepares VOLUME_GROUP_SETTINGS
//
void DataProtectionRcm::ProcessHostVolumeGroupSettings(HOST_VOLUME_GROUP_SETTINGS& hostVolGroupSettings)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    // Prepare VOLUME_GROUP_SETTINGS
    VOLUME_GROUP_SETTINGS vgs;
    if (m_Args.IsSourceSide())
    {
        ProcessVolumeGroupSettingsForSource(vgs);
    }
    else
    {
        ProcessVolumeGroupSettingsForTarget(vgs);
    }

    hostVolGroupSettings.volumeGroups.clear();
    hostVolGroupSettings.volumeGroups.push_back(vgs);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// Create fastsyncRcm instance
//
void DataProtectionRcm::Create()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // Check for duplicate dataprotectionRcm process for this device with unique file lock
    // Create and try aquiring file lock corresponding to this device

    const VOLUME_GROUP_SETTINGS& volGroupSettings = m_HostVolGroupSettings.volumeGroups.front();

	std::string fileLock;

    if (TARGET == volGroupSettings.direction)
    {
        fileLock = m_Args.GetSourceHostId() + "_" + m_Args.GetDiskId();
    }
    else if (SOURCE == volGroupSettings.direction)
    {
		fileLock = m_Args.GetDiskId();
    }
    else
    {
        std::stringstream sserr("Not a valid direction ");
        sserr << volGroupSettings.direction << '.';
        throw INMAGE_EX(sserr.str().c_str());
    }

	fileLock += "_instance";
    
    CDPLock::Ptr cdplock = boost::make_shared<CDPLock>(fileLock);
    if (cdplock->try_acquire())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Lock acquired sucessfully. lock name = %s\n", FUNCTION_NAME, fileLock.c_str());
        m_cdplocks.push_back(cdplock);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Another DataprotectionSyncRcm instance is running for same device with lock name = %s. Exiting newly created process %d\n",
            FUNCTION_NAME, fileLock.c_str(), ACE_OS::getpid());

        // Newly launched process should quit so return from here
        // Wait till agents settings fetch interval sec before quit to avoid respawn
        CDPUtil::QuitRequested(m_RcmConfigurator->getReplicationSettingsFetchInterval());

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }
    m_FastSyncJob.reset();
    m_FastSyncJob = boost::make_shared<FastSyncRcm>(volGroupSettings, m_ResyncStateManager);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// Start fastsyncRcm
//
void DataProtectionRcm::Start()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_FastSyncJob)
    {
        m_FastSyncJob->Start();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// Start fastsyncRcm
//
void DataProtectionRcm::Stop()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (m_FastSyncJob)
    {
        m_FastSyncJob->Stop();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
