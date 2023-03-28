#include <algorithm>

#include <ace/Time_Value.h>
#include <boost/thread.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "host.h"
#include "logger.h"
#include "cdputil.h"
#include "inmageex.h"
#include "datacacher.h"
#include "Monitoring.h"
#include "portablehelpers.h"
#include "inmerroralertdefs.h"
#include "resyncStateManager.h"
#include "volumegroupsettings.h"
#include "dataprotectionutils.h"
#include "inmerroralertnumbers.h"
#include "globalHealthCollator.h"

#define UNDERSCORE '_'
#define JSONEXTENSION ".json"

boost::atomic<bool> ResyncStateManager::m_IsInitialized;

boost::mutex ResyncStateManager::m_Lock;

const int WaitTimeInSeconds = 60;

const int FileNameWaitInMilliSec = 15;

const char* SyncStateToString(const int syncState)
{
    switch(syncState)
    {
    case SYNC_STATE_RESYNC_INIT:
        return "SYNC_STATE_RESYNC_INIT";
    case SYNC_STATE_SOURCE_START_TIME_TO_DRIVER_DONE:
        return "SYNC_STATE_SOURCE_START_TIME_TO_DRIVER_DONE";
    case SYNC_STATE_SOURCE_END_TIME_TO_DRIVER_DONE:
        return "SYNC_STATE_SOURCE_END_TIME_TO_DRIVER_DONE";
    case SYNC_STATE_RESYNC_FAILED:
        return "SYNC_STATE_RESYNC_FAILED";
    case SYNC_STATE_TARGET_CONCLUDED_RESYNC:
        return "SYNC_STATE_TARGET_CONCLUDED_RESYNC";
    case SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM:
        return "SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM";
    case SYNC_STATE_TARGET_RESTART_RESYNC_INLINE:
        return "SYNC_STATE_TARGET_RESTART_RESYNC_INLINE";
    case SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE:
        return "SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE";
    default:
        DebugPrintf(SV_LOG_ERROR, "%s: Invalid syncState param received 0x%04x\n", FUNCTION_NAME, syncState);
        return "";
    }
}

// Helper smart buffer
struct RemoteDataSmartBuffer : private boost::noncopyable
{
public:
    RemoteDataSmartBuffer() : dataBuffer(NULL) {}

    ~RemoteDataSmartBuffer()
    {
        if (NULL != dataBuffer)
        {
            free(dataBuffer);
        }
    }

public:
    char* dataBuffer;
};

////////////////////////////
/////ResyncStateManager/////
////////////////////////////

ResyncStateManager::ResyncStateManager(const DpRcmProgramArgs &args) :
    m_Args(args),
    m_Mode(m_Args.IsSourceSide() ? Agent_Mode_Source : Agent_Mode_Target),
    m_PeerMode(m_Args.IsSourceSide() ? Agent_Mode_Target : Agent_Mode_Source),
    m_RcmConfigurator(RcmClientLib::RcmConfigurator::getInstance()),
    m_PeerRemoteResyncStateParseRetryCount(m_RcmConfigurator->getPeerRemoteResyncStateParseRetryCount()),
    m_RemoteStateFileSeqNo(0)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // Initialize ResyncRcmSettings
    if (!ResyncRcmSettings::GetInstance(m_ResyncRcmSettings))
    {
        throw INMAGE_EX("Failed to get ResyncRcmSettings instance.");
    }

    boost::filesystem::path sourceRemoteResyncStateFileSpec;
    boost::filesystem::path targetRemoteResyncStateFileSpec;

    // Initialize data path transport
    InitializeDatapathTransport();

    // Initialize remote resync state file format
    {
        sourceRemoteResyncStateFileSpec = targetRemoteResyncStateFileSpec = 
            m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder;

        sourceRemoteResyncStateFileSpec /= RemoteSourceResyncStateFile + "_*";
        targetRemoteResyncStateFileSpec /= RemoteTargetResyncStateFile + "_*";

        m_RemoteResyncStateFileSpec = m_Args.IsSourceSide() ? sourceRemoteResyncStateFileSpec.string() : targetRemoteResyncStateFileSpec.string();
        m_PeerRemoteResyncStateFileSpec = m_Args.IsSourceSide() ? targetRemoteResyncStateFileSpec.string() : sourceRemoteResyncStateFileSpec.string();
    }

    // TODO: ResyncStateOwner should not be changed oncce assigned.
    // TODO: nichougu - Write wrapper on ResyncStateInfo which will internally update members and wrapper will be exposed to use of ResyncStateInfo - resync classes here 
    m_ResyncState.ResyncStateOwner = m_Args.IsSourceSide() ? SYNC_STATE_OWNER_SOURCE : SYNC_STATE_OWNER_TARGET;
    m_PeerResyncState.ResyncStateOwner = m_Args.IsSourceSide() ? SYNC_STATE_OWNER_TARGET : SYNC_STATE_OWNER_SOURCE;

    if (m_Args.IsSourceSide())
    {
        m_ResyncState.Properties[SourcePropertyHostName] = Host::GetInstance().GetHostName();

        // Fill disk size from volumes cache
        std::string dataCacherError;
        DataCacher::VolumesCache volumesCache;
        if (!DataCacher::UpdateVolumesCache(volumesCache, dataCacherError))
        {
            std::stringstream ss;
            ss << "Failed to read volumes cache with error " << dataCacherError;
            throw INMAGE_EX(ss.str().c_str());
        }

        for (ConstVolumeSummariesIter vsIter = volumesCache.m_Vs.begin(); vsIter != volumesCache.m_Vs.end(); vsIter++)
        {
            if (VolumeSummary::DISK == vsIter->type)
            {
                if (0 == vsIter->name.compare(m_Args.GetDiskId()))
                {
                    m_ResyncState.Properties[SourcePropertyDiskRawSize] = boost::lexical_cast<std::string>(vsIter->rawcapacity);
                    break;
                }
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void ResyncStateManager::InitializeDatapathTransport()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    TRANSPORT_CONNECTION_SETTINGS transportSettings;
    TRANSPORT_PROTOCOL transportProtocol;

    if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_PROCESS_SERVER))
    {
        transportProtocol = TRANSPORT_PROTOCOL_HTTP;

        const RcmClientLib::ProcessServerTransportSettings processServerDetails =
            JSON::consumer<RcmClientLib::ProcessServerTransportSettings>::convert(m_ResyncRcmSettings->GetDataPathTransportSettings(), true);

        transportSettings.ipAddress = processServerDetails.GetIPAddress();
        transportSettings.sslPort = boost::lexical_cast<std::string>(processServerDetails.Port);
    }
    else if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_LOCAL_FILE))
    {
        transportProtocol = TRANSPORT_PROTOCOL_FILE;
    }
    else if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB))
    {
        transportProtocol = TRANSPORT_PROTOCOL_BLOB;

        transportSettings.ipAddress = m_ResyncRcmSettings->GetSourceAgentSyncSettings().TransportSettings.BlobContainerSasUri;
    }
    else
    {
        // Unsupported transport type received - throw
        throw INMAGE_EX("Unsupported transport type received " + m_ResyncRcmSettings->GetDataPathTransportType());
    }

    m_CxTransport.reset( new CxTransport(transportProtocol, transportSettings, VOLUME_SETTINGS::SECURE_MODE_SECURE));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

}

//
// Prepares unique remote persistent resync state file name
//
// Out param preTarget contains pre_ prepended file name
// Out param target contains actual file name
//
void ResyncStateManager::NextRemoteResyncStateFileName(boost::filesystem::path& preTarget, boost::filesystem::path& target)
{
    boost::mutex::scoped_lock guard(m_FileNameLock);

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    std::ostringstream name;

    name << PREUNDERSCORE;
    name << (m_Args.IsSourceSide() ? RemoteSourceResyncStateFile : RemoteTargetResyncStateFile);
    name << UNDERSCORE;
    name << std::setw(16) << std::setfill('0') << ++m_RemoteStateFileSeqNo << JSONEXTENSION;

    preTarget = target = m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder;

    preTarget /= name.str();

    target /= name.str().substr(strlen(PREUNDERSCORE));
    
    ACE_Time_Value wait;
    wait.msec(FileNameWaitInMilliSec);
    ACE_OS::sleep(wait);
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    
    return;
}

bool ResyncStateManager::SerializeResyncState(ResyncStateInfo& resyncState, std::string &strResyncState, std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        strResyncState = JSON::producer<ResyncStateInfo>::convert(resyncState);
    }
    catch (const JSON::json_exception &jsone)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << " failed - JSON parser failed with exception " << jsone.what();
        errMsg = sserror.str();
        return false;
    }
    catch (const std::exception &e)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << " failed with exception " << e.what();
        errMsg = sserror.str();
        return false;
    }
    catch (...)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << " failed with an unknown exception.";
        errMsg = sserror.str();
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool ResyncStateManager::DeserializeResyncState(ResyncStateInfo& resyncState, std::string &strResyncState, std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool retval = true;
    try
    {
        resyncState = JSON::consumer<ResyncStateInfo>::convert(strResyncState, true);
    }
    catch (const boost::property_tree::json_parser::json_parser_error &jsonparsere)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << " failed - boost::property_tree::read_json failed with exception " << jsonparsere.what();
        errMsg = sserror.str();
        retval = false;
    }
    catch (const JSON::json_exception &jsone)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME  << " failed - JSON parser failed with exception " << jsone.what();
        errMsg = sserror.str();
        retval = false;
    }
    catch (const std::exception &e)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << " failed with exception " << e.what();
        errMsg = sserror.str();
        retval = false;
    }
    catch (...)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << " failed with an unknown exception.";
        errMsg = sserror.str();
        retval = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

/************************************************************************************************************************************
//
// - Note:
//    - Blocking function - dataproection to block on InitializeState
//    - Always check quit signal
//    - Retry on failure cases until success.
//        - JSON de-serialization failure
//            - source:
//                - own state
//                    - Ignore remote and send a fresh state
//                    - then proceed with resync tasks as usual
//                        - this will not hamper resync current state
//                        - If tasks already in progress they will continue from where they are
//                - target state
//                    - Retry until 12 atempts * 5 min = 1 hr - fail resync and mark re-start resync to RCM with internal eror only if fail to deserialize *remote* peer state after 12 failed attempts
//
//            - target:
//                - own state
//                    - Ignore remote and send a fresh state
//                    - Then pull source state and wait till source state becomes start time stamp send
//                    - then proceed with resync tasks as usual
//                        - this will not hamper resync current state
//                            - If tasks already in progress they will continue from where they are
//                - Source state
//                    - Retry until 12 atempts * 5 min = 1 hr - fail own resync state and mark re-start resync only if fail to deserialize *remote* peer state after 12 failed attempts
//
//       - JSON serialization failure:
//            - New state is a temp state object
//            - Throw if serialization fails
//
//       - Network failure - data path
//            - Never fail resync
//            - retry until success
//            - Send health event // TODO
//
//       - Control path failure
//            - Never fail resync
//            - retry until success
//            - Send health event // TODO
//
// - Logic:
//    - Initialize my state
//        - List own state file on PS cache
//            - If list fail retry until success
//            - If file present pull and update in-memory own state
//               - If fail in parsing then ignore and send a fresh state
//        - If remote own state not present send a fresh state
//            - Create in-memory state - default state RESYNC_INIT
//            - Push in-memory state to PS cache
//                - If push fail - retry until success
//        - Mark own state initialized
//
//    - Initialize peer state
//        - List peer state file on PS cache
//            - If list fail retry until success
//        - If file present pull and update in-memory peer state
//            - If fail in parsing then retry 12 attempts and then fail resync and mark re-start resync
//        - If remote peer state not present
//            - Wait until remote peer state arrives
//        - Mark peer state initialized
//
************************************************************************************************************************************/
//
// Initializes resync state for own either from remote state or with a fresh start.
// Initializes resync state for peer from remote state.
// BLocking function until resync state initialization is successful for both source and target unless no deserialization failure for peer remote state.
//
// Throws only if resync state serialization fails for own state.
//
// Returns false if resync state initialization fails and quit event is requested, true otherwise.
//
bool ResyncStateManager::InitializeState()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string errMsg;
    bool IsRemoteStateInitialized = false;
    bool IsPeerRemoteStateInitialized = false;

    try
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Initializing %s resync state.\n", FUNCTION_NAME, m_Mode.c_str());
        bool isRemoteStatePresent = false;
        std::string remoteStateFileName;
        std::string strRemoteState;
        std::size_t remoteFileSize = 0;
        do
        {
            // Check remote resync state file present
            if (!isRemoteStatePresent && !ListRemoteState(m_RemoteResyncStateFileSpec, isRemoteStatePresent, remoteStateFileName, remoteFileSize, errMsg))
            {
                std::stringstream sserr;
                sserr << FUNCTION_NAME << ": ListRemoteState failed with error " << errMsg << ". Retrying after  " << WaitTimeInSeconds << " seconds." << '\n';
                DatapathTransportErrorLogger::LogDatapathTransportError(sserr.str(), boost::chrono::steady_clock::now());
                continue;
            }

            if (isRemoteStatePresent)
            {
                // Remote state present, pull it and initialize in-memory own state - retry until success
                // First extract current sequence number in remoteStateFileName
                const size_t first = remoteStateFileName.find(UNDERSCORE);
                const size_t last = remoteStateFileName.find(JSONEXTENSION);
                if ((first != std::string::npos) && (last != std::string::npos))
                {
                    try
                    {
                        m_RemoteStateFileSeqNo = boost::lexical_cast<int>(remoteStateFileName.substr(first + 1, last - first - 1));
                    }
                    catch (...)
                    {
                        m_RemoteStateFileSeqNo = 1000000000000000; // 16 digit long number to make sure current sequence number is latest
                        DebugPrintf(SV_LOG_WARNING, "%s failed to read current RemoteStateFileSeqNo in %s\n",
                            FUNCTION_NAME, remoteStateFileName.c_str());
                    }
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Invalid remoteStateFileName format identified %s, deleting.\n", FUNCTION_NAME, remoteStateFileName.c_str());
                    m_CxTransport->deleteFile(remoteStateFileName);
                    continue;
                }

                if (!PullRemoteState(remoteStateFileName, strRemoteState, errMsg, remoteFileSize))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: PullRemoteState failed - retrying after %d seconds. Error: %s.\n", FUNCTION_NAME, WaitTimeInSeconds, errMsg.c_str());
                    continue;
                }
                    
                // Deserialize remote sync state
                // For own state no retry if deserialization fail - push a fresh state
                if (!DeserializeResyncState(m_ResyncState, strRemoteState, errMsg))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to deserialize remote resync state file %s with error %s.\n",
                        FUNCTION_NAME, remoteStateFileName.c_str(), errMsg.c_str());
                    break;
                }
                    
                // Pull and deserialization sussessful
                DebugPrintf(SV_LOG_ALWAYS, "%s: %s resync state successfully initialized from remote state file %s. Current state=%s.\n",
                    FUNCTION_NAME, m_Mode.c_str(), remoteStateFileName.c_str(), SyncStateToString(m_ResyncState.CurrentState));
                IsRemoteStateInitialized = true;
            }
            else
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: %s remote resync state not found.\n", FUNCTION_NAME, m_Mode.c_str());
            }

            break;

        } while (!IsRemoteStateInitialized && !CDPUtil::QuitRequested(WaitTimeInSeconds));

        // List/Pull failed and quit requested - return
        if (!IsRemoteStateInitialized && CDPUtil::QuitRequested())
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: %s resync state initialization failed from remote state and quit event identified while retry.\n", FUNCTION_NAME, m_Mode.c_str());
            return false;
        }

        if (!IsRemoteStateInitialized)
        {
            // Remote state not found, push in-memory fresh state - retry until success.
            DebugPrintf(SV_LOG_ALWAYS, "%s: Creating fresh %s remote persistent resync state with state %s.\n",
                FUNCTION_NAME, m_Mode.c_str(), SyncStateToString(SYNC_STATE_RESYNC_INIT));
            do
            {
                if (!SerializeResyncState(m_ResyncState, strRemoteState, errMsg))
                {
                    throw INMAGE_EX(errMsg.c_str());
                }

                if (!PushState(strRemoteState, errMsg))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: PushState failed - retrying after %d seconds. Error: %s.\n", FUNCTION_NAME, WaitTimeInSeconds, errMsg.c_str());
                    continue;
                }

                IsRemoteStateInitialized = true;

            } while (!IsRemoteStateInitialized && !CDPUtil::QuitRequested(WaitTimeInSeconds));

            // Push failed and quit requested - return
            if (!IsRemoteStateInitialized && CDPUtil::QuitRequested())
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Creating fresh %s remote persistent resync state failed and quit event identified while retry.\n", FUNCTION_NAME, m_Mode.c_str());
                return false;
            }

            DebugPrintf(SV_LOG_ALWAYS, "%s: Successfully created fresh %s remote persistent resync state with state=%s.\n",
                FUNCTION_NAME, m_Mode.c_str(), SyncStateToString(m_ResyncState.CurrentState));
        }

        DebugPrintf(SV_LOG_ALWAYS, "%s: Initializing %s resync state.\n", FUNCTION_NAME, m_PeerMode.c_str());

        bool isPeerRemoteStatePresent = false;
        std::string peerRemoteStateFileName;
        std::string strPeerRemoteState;
        std::size_t peerRemoteFileSize = 0;
        uint16_t peerJsonParseFailCnt = 0;
        static boost::chrono::steady_clock::time_point s_startTimeStamp = boost::chrono::steady_clock::now();

        do
        {
            if (m_Args.IsSourceSide() && RcmClientLib::RcmConfigurator::getInstance()->IsAzureToOnPremReplication() &&
                ((s_startTimeStamp + boost::chrono::seconds(300)) <= boost::chrono::steady_clock::now())) {

                std::string blobContainerSasUrl;
                std::string cacheStorageAccountName;
                const std::string healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::OnPremToAzureConnectionNotHealthy::HealthCode;
                if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB)) {
                    blobContainerSasUrl = m_ResyncRcmSettings->GetSourceAgentSyncSettings().TransportSettings.BlobContainerSasUri;
                }
                else {
                    std::stringstream errMsg;
                    errMsg << "Dataprotection initialization failed because of invalid data transport path " + m_ResyncRcmSettings->GetDataPathTransportType();
                    throw INMAGE_EX(errMsg.str().c_str());
                }
                if (!blobContainerSasUrl.empty()) {
                    ExtractCacheStorageNameFromBlobContainerSasUrl(blobContainerSasUrl, cacheStorageAccountName);
                }
                if (cacheStorageAccountName.empty())
                {
                    continue;
                }
                std::map<std::string, std::string> msgParams;
                msgParams.insert(std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::OnPremToAzureConnectionNotHealthy::CacheStorageAccountName,
                    cacheStorageAccountName.c_str()));
                msgParams.insert(std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::OnPremToAzureConnectionNotHealthy::ObservationTime,
                    DataProtectionGlobalHealthCollator::getInstance()->GetCurrentObservationTime()));
                AgentDiskLevelHealthIssue diskHI(m_Args.GetDiskId(), healthIssueCode, msgParams);
                if (!DataProtectionGlobalHealthCollator::getInstance()->SetDiskHealthIssue(diskHI))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to raise the health issue %s for the disk %s.\n", FUNCTION_NAME, healthIssueCode.c_str(), m_Args.GetDiskId().c_str());
                }
            }
            // Get peer remote resync state - retry until peer remote state file arrives
            if (!isPeerRemoteStatePresent)
            {   
                if (!ListRemoteState(m_PeerRemoteResyncStateFileSpec, isPeerRemoteStatePresent, peerRemoteStateFileName, peerRemoteFileSize, errMsg))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: ListRemoteState failed for %s - retrying after %d seconds. Error: %s.\n", FUNCTION_NAME, m_PeerMode.c_str(), WaitTimeInSeconds, errMsg.c_str());
                    continue;
                }
                if (!isPeerRemoteStatePresent)
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: %s remote resync state not present - retrying after %d seconds.\n", FUNCTION_NAME, m_PeerMode.c_str(), WaitTimeInSeconds);
                    continue;
                }
            }

            // Peer remote state present, pull it and initialize in-memory peer state.
            if (isPeerRemoteStatePresent && !PullRemoteState(peerRemoteStateFileName, strPeerRemoteState, errMsg, peerRemoteFileSize))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: PullRemoteState failed for %s - retrying after %d seconds. Error: %s\n", FUNCTION_NAME, m_PeerMode.c_str(), WaitTimeInSeconds, errMsg.c_str());
                continue;
            }

            // Deserialize peer remote resync state
            // If peer remote resync state deserialization fail consecutively for 12 times - 60 min then complete resync with fail status.
            if (!DeserializeResyncState(m_PeerResyncState, strPeerRemoteState, errMsg))
            {
                if (++peerJsonParseFailCnt % m_PeerRemoteResyncStateParseRetryCount)
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to deserialize %s remote resync state - retrying after %d seconds. Error: %s.\n",
                        FUNCTION_NAME, m_PeerMode.c_str(), WaitTimeInSeconds, errMsg.c_str());
                    // Reset flag to start a fresh pull
                    isPeerRemoteStatePresent = false;
                    peerRemoteStateFileName.clear();
                    strPeerRemoteState.clear();
                    peerRemoteFileSize = 0;
                    continue;
                }

                DebugPrintf(SV_LOG_ERROR, "%s: Failed to deserialize %s remote resync state consecutively for last %d secs. Recent error:  %s.\n",
                    FUNCTION_NAME, m_PeerMode.c_str(), m_PeerRemoteResyncStateParseRetryCount * WaitTimeInSeconds, errMsg.c_str());

                // Call MonitorPeerState in non unblocking mode - this fail resync if peer resync state is corrupt.
                MonitorPeerState();
                break;
            }

            IsPeerRemoteStateInitialized = true;
            //reset the raised health issue
            const std::string healthIssueCode = AgentHealthIssueCodes::DiskLevelHealthIssues::OnPremToAzureConnectionNotHealthy::HealthCode;
            if (m_Args.IsSourceSide() && RcmClientLib::RcmConfigurator::getInstance()->IsAzureToOnPremReplication()
                && DataProtectionGlobalHealthCollator::getInstance()->IsDiskHealthIssuePresent(healthIssueCode, m_Args.GetDiskId()))
            {
                if (!DataProtectionGlobalHealthCollator::getInstance()->ResetDiskHealthIssue(healthIssueCode, m_Args.GetDiskId()))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to reset the health issue %s for the disk %s.\n", FUNCTION_NAME, healthIssueCode.c_str(), m_Args.GetDiskId().c_str());
                }
            }
            DebugPrintf(SV_LOG_ALWAYS, "%s: %s resync state successfully initialized from remote state. Current state=%s.\n",
                FUNCTION_NAME, m_PeerMode.c_str(), SyncStateToString(m_PeerResyncState.CurrentState));

        } while (!IsPeerRemoteStateInitialized && !CDPUtil::QuitRequested(WaitTimeInSeconds));
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with exception %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return IsRemoteStateInitialized && IsPeerRemoteStateInitialized;
}

//
// List remote resync state file
// 
// In Param remoteStateFileFormat is a file format to list
// Out param isRemoteStatePresent indicates presence of remote state file
// Out param remoteStateFileName contains most recent remote resync state file name when isRemoteStatePresent is true
// Out param remoteFileSize contain file size of remoteStateFileName
// Out param errMsg contains error message on failure in transport
//
// Returns false if failed in transport true otherwise
//
bool ResyncStateManager::ListRemoteState(const std::string& remoteStateFileSpec, bool &isRemoteStatePresent, std::string& remoteStateFileName, std::size_t &remoteFileSize, std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    FileInfosPtr fileInfos(new FileInfos_t);
    if (!m_CxTransport->listFile(remoteStateFileSpec, *fileInfos.get()))
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": Getting file list to know whether remote resync state file " << remoteStateFileSpec << " is present has failed: " << m_CxTransport->status() << '.';
        errMsg = sserror.str();

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    if (0 == fileInfos->size())
    {
        isRemoteStatePresent = false;
        DebugPrintf(SV_LOG_DEBUG, "%s: Remote resync state file %s is not present.\n", FUNCTION_NAME, remoteStateFileSpec.c_str());
    }
    else
    {
        // Sort to make sure latest file is at end.
        std::sort(fileInfos->begin(), fileInfos->end());

        isRemoteStatePresent = true;
        boost::filesystem::path remoteStateFile;
        remoteStateFile = m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder;
        remoteStateFile /= fileInfos->back().m_name; // Always pick latest state file.

        remoteStateFileName = remoteStateFile.string();
        remoteFileSize = fileInfos->back().m_size;
        DebugPrintf(SV_LOG_DEBUG, "%s: Remote resync state file %s is present, total files = %d.\n", FUNCTION_NAME, remoteStateFileName.c_str(), fileInfos->size());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

//
// Retrieves resync state from remote state file
// 
// In Param remoteStateFileFormat is a file format to list
// Out param strRemoteState contains serialized remote state file
// Out param errMsg contains error message on failure in transport
// Out param remoteFileSize contain file size of remote state file
//
// Returns false if pull fail in transport true otherwise
//
bool ResyncStateManager::PullRemoteState(const std::string& remoteStateFileSpec, std::string& strRemoteState, std::string &errMsg, const std::size_t remoteFileSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_CxTransportLock);

    bool isRemoteStatePresent = false;
    std::size_t fileSize = remoteFileSize;
    std::string remoteStateFileName = remoteStateFileSpec;

    if (!fileSize)
    {
        if(!ListRemoteState(remoteStateFileSpec, isRemoteStatePresent, remoteStateFileName, fileSize, errMsg))
        {
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return false;
        }
        else if (!isRemoteStatePresent)
        {
            std::stringstream sserror;
            sserror << FUNCTION_NAME << ": Remote resync state file " << remoteStateFileSpec << " not present.";
            errMsg = sserror.str();

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return false;
        }
    }

    RemoteDataSmartBuffer inBuffer;
    size_t bytesReturned = 0;
    if (m_CxTransport->getFile(remoteStateFileName, fileSize, &(inBuffer.dataBuffer), bytesReturned))
    {
        strRemoteState = std::string(inBuffer.dataBuffer, fileSize);
    }
    else
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": Retrieving remote resync state file " << remoteStateFileName << " failed with transport status " << m_CxTransport->status() << '.';
        errMsg = sserror.str();

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

//
// Push resync state to remote state file
//
// In param strResyncState contains serialized resync state
// Out param errMsg contains error message on failure in transport
//
// Returns false if push fail in transport true otherwise.
//
bool ResyncStateManager::PushState(const std::string& strResyncState, std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_CxTransportLock);

    boost::filesystem::path preTarget, target;
    NextRemoteResyncStateFileName(preTarget, target);
    if (boost::iequals(m_ResyncRcmSettings->GetDataPathTransportType(), RcmClientLib::TRANSPORT_TYPE_AZURE_BLOB))
    {
        preTarget = target;
    }
    if (!m_CxTransport->putFile(preTarget.string(), strResyncState.length(), strResyncState.c_str(), false, COMPRESS_NONE))
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": Failed to push resync state file " << preTarget.string() << " with transport status " << m_CxTransport->status() << '.';
        
        sserror << "\nAborting failed push attempt.\n";
        if (!m_CxTransport->abortRequest(preTarget.string()))
        {
            sserror << "abortRequest failed with transport status " << m_CxTransport->status() << '.';
        }

        sserror << "\nDeleting remote file as push attempt failed.\n";
        if (!m_CxTransport->deleteFile(preTarget.string()))
        {
            sserror << "Failed to delete remote file with transport status " << m_CxTransport->status() << ".\n";
        }

        errMsg = sserror.str();
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    boost::tribool tb = m_CxTransport->renameFile(preTarget.string(), target.string(), COMPRESS_NONE);
    if (!tb || CxTransportNotFound(tb))
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": Failed to rename remote resync state file " << preTarget.string() << " to " << target.string() << " with transport status " << m_CxTransport->status() << '.';
        errMsg = sserror.str();

        m_CxTransport->deleteFile(preTarget.string());
        
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

//
// Calls RCM resync completion API with completion reason
//
// In param reason contains resync completion reason
//
bool ResyncStateManager::CompleteSync(RcmClientLib::SourceAgentCompleteSyncInput & completeSyncInput)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string strCompleteSyncInput;
    std::string errMsg;
    if (!SerializeCompleteSyncInput(completeSyncInput, strCompleteSyncInput, errMsg))
    {
        throw INMAGE_EX(errMsg.c_str());
    }

    // CompleteSync with RCM - retry until success - retry after re-spawn
    if ((SVS_OK != m_RcmConfigurator->GetRcmClient()->CompleteSync(strCompleteSyncInput)))
    {
        const int errorCode = m_RcmConfigurator->GetRcmClient()->GetErrorCode();
        DebugPrintf(SV_LOG_ERROR, "%s: CompleteSync to RCM failed with error code %d.\n", FUNCTION_NAME, errorCode);
        CDPUtil::QuitRequested(m_RcmConfigurator->getReplicationSettingsFetchInterval());
        return false;
    }
    
    DebugPrintf(SV_LOG_ALWAYS, "%s: CompleteSync to RCM successful.\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool ResyncStateManager::SerializeCompleteSyncInput(RcmClientLib::SourceAgentCompleteSyncInput& completeSyncInput,
    std::string &strCompleteSyncInput,
    std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    try
    {
        strCompleteSyncInput = JSON::producer<RcmClientLib::SourceAgentCompleteSyncInput>::convert(completeSyncInput);
    }
    catch (const JSON::json_exception &jsone)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": failed - JSON parser failed with exception " << jsone.what();
        errMsg = sserror.str();
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }
    catch (const std::exception &e)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": failed with exception " << e.what();
        errMsg = sserror.str();
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }
    catch (...)
    {
        std::stringstream sserror;
        sserror << FUNCTION_NAME << ": failed with an unknown exception.";
        errMsg = sserror.str();
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

sigslot::signal1<uint32_t &> & ResyncStateManager::GetMatchedBytesRatioSignal()
{
    return m_MatchedBytesRatioSignal;
}

ResyncStateInfo ResyncStateManager::GetResyncState() const
{
    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);
    return m_ResyncState;
}

ResyncStateInfo ResyncStateManager::GetPeerResyncState() const
{
    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);
    return m_PeerResyncState;
}

//
// Checks whether inline restart resync is marked, for example target mark inline restart resync if AzureAgent call
// fail with cancel operation exception.
//
bool ResyncStateManager::IsInlineRestartResyncMarked()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool retval = false;
    if (m_Args.IsSourceSide())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Ignoring - called in source mode.\n", FUNCTION_NAME);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return retval;
    }

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);

    if ((m_ResyncState.CurrentState & SYNC_STATE_TARGET_RESTART_RESYNC_INLINE) &&
        (m_PeerResyncState.CurrentState & SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE))
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Inline restart resync is marked. target state = %s, Source state = %s.",
            FUNCTION_NAME,
            SyncStateToString(m_ResyncState.CurrentState),
            SyncStateToString(m_PeerResyncState.CurrentState));

        retval = true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

bool ResyncStateManager::ResetTargetResyncState()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool retval = false;
    if (m_Args.IsSourceSide())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Ignoring - called in source mode.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return retval;
    }

    // Reset resync state to SYNC_STATE_RESYNC_INIT and cleanup any alerts if marked already.
    ResyncStateInfo resyncStateInfo;
    resyncStateInfo.ResyncStateOwner = SYNC_STATE_OWNER_TARGET;
    resyncStateInfo.CurrentState = SYNC_STATE_RESYNC_INIT;
    if (retval = UpdateState(resyncStateInfo))
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Reset resync state succeeded, new target state=%s.\n",
            FUNCTION_NAME, SyncStateToString(resyncStateInfo.CurrentState));
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Reset resync state failed.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

//
// Cleans up resync files (bitmap progress files, hcd files, sync files) on remote cache excluding state files.
//
bool ResyncStateManager::CleanupRemoteCache()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    bool retval = false;
    if (m_Args.IsSourceSide())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Ignoring - called in source mode.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return retval;
    }

    try
    {
        // Using scope to free m_CxTransportLock immediately.
        {
            boost::unique_lock<boost::recursive_mutex> guard(m_CxTransportLock);
            // **Note: never cleanup resync state files as part of cleanup remote cache.
            const std::string cleanupFilesSpec("*.Map;*.dat;*.gz");
            if (m_CxTransport->deleteFile(m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder, cleanupFilesSpec))
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: cleanup resync files on remote cache succeeded, attempting to reset resync state.\n", FUNCTION_NAME);
                retval = ResetTargetResyncState();
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Failed to cleanup remote resync cache \"%s\". CxTransport status = %s\n", FUNCTION_NAME,
                    m_ResyncRcmSettings->GetSourceAgentSyncSettings().LogFolder.c_str(), m_CxTransport->status());
            }
        }
    }
    catch (const std::exception& e)
    {
        retval = false;
        DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        retval = false;
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception.\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

void ResyncStateManager::DumpCurrentResyncState() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);

    if ((SYNC_STATE_OWNER_UNKNOWN == m_ResyncState.ResyncStateOwner)||
        (SYNC_STATE_OWNER_UNKNOWN == m_PeerResyncState.ResyncStateOwner))
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed: ResyncStateOwner is unknown.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    const ResyncStateInfo &sourceResyncStateInfo =
        (SYNC_STATE_OWNER_SOURCE & m_ResyncState.ResyncStateOwner) ? m_ResyncState : m_PeerResyncState;
    const ResyncStateInfo &targetResyncStateInfo =
        (SYNC_STATE_OWNER_TARGET & m_ResyncState.ResyncStateOwner) ? m_ResyncState : m_PeerResyncState;

    DebugPrintf(SV_LOG_ALWAYS, "%s: source state = %s, target state = %s.\n", FUNCTION_NAME,
        SyncStateToString(sourceResyncStateInfo.CurrentState),
        SyncStateToString(targetResyncStateInfo.CurrentState));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool ResyncStateManager::CanContinueResync() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);

    if ((SYNC_STATE_OWNER_UNKNOWN == m_ResyncState.ResyncStateOwner) ||
        (SYNC_STATE_OWNER_UNKNOWN == m_PeerResyncState.ResyncStateOwner))
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed: ResyncStateOwner is unknown.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return false;
    }

    bool retval = true;

    const ResyncStateInfo &sourceResyncStateInfo =
        (SYNC_STATE_OWNER_SOURCE & m_ResyncState.ResyncStateOwner) ? m_ResyncState : m_PeerResyncState;
    const ResyncStateInfo &targetResyncStateInfo =
        (SYNC_STATE_OWNER_TARGET & m_ResyncState.ResyncStateOwner) ? m_ResyncState : m_PeerResyncState;

    // 
    // Source DP should continue until it complete resync to RCM (success/failure).
    // So source DP should conclude based only on source resync state.
    // Target DP should check both source and target resync state.
    //
    if ((sourceResyncStateInfo.CurrentState == SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM) ||
        (!m_Args.IsSourceSide() &&
        ((sourceResyncStateInfo.CurrentState == SYNC_STATE_RESYNC_FAILED) ||
        (targetResyncStateInfo.CurrentState == SYNC_STATE_TARGET_CONCLUDED_RESYNC) ||
        (targetResyncStateInfo.CurrentState == SYNC_STATE_RESYNC_FAILED))))
    {
        retval = false;
        
        DebugPrintf(SV_LOG_ALWAYS, "%s: Can not continue resync. Source state = %s, target state = %s.\n", FUNCTION_NAME,
            SyncStateToString(sourceResyncStateInfo.CurrentState),
            SyncStateToString(targetResyncStateInfo.CurrentState));

        // Wait till agents settings fetch interval to avoid respawn.
        CDPUtil::QuitRequested(m_RcmConfigurator->getReplicationSettingsFetchInterval());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

//
// Called from main after resync tasks have been started
// Periodically pull remote peer state - 5 min interval
// Takes action if peer state is changed
// If fail to parse remote peer state consecutively for 12 times * 5 mins = last 1 hr then marks own resync state to failed with reason peer remote state corrupt
//      In this case source will complete resync in RCM with failure reason and push own failed state to remote state and exit
//      In this case target will push own failed state to remote state and exit
//
// Param isBlocking if true will run until peer resyc state change or quit requested, default is false to run once and exit
//
void ResyncStateManager::MonitorPeerState(const bool isBlocking)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string errMsg;
    uint16_t peerJsonParseFailCnt = 0;
    bool IsPeerJsonParseFailed = false;
    ResyncStateInfo tmpPeerResyncState;
    static const int sPeerRemoteResyncStateMonitorInterval(m_RcmConfigurator->getPeerRemoteResyncStateMonitorInterval());
    
    // Make sure both source and target are healthy and then start monitoring.
    if (!CanContinueResync())
    {
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }
    do
    {
        std::stringstream ssJsonParseError;
        // Always use scoped lock MonitorPeerState is blocking until quit requested
        {
            // Pull peer state - retry until success
            std::string strPeerRemoteState;
            if (!PullRemoteState(m_PeerRemoteResyncStateFileSpec, strPeerRemoteState, errMsg))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: PullRemoteState failed for %s - retrying after %d seconds. Error: %s\n", FUNCTION_NAME, m_PeerMode.c_str(), WaitTimeInSeconds, errMsg.c_str());
                continue;
            }
            
            // Deserialize peer remote sync state
            // If peer remote resync state deserialization fail consecutively for 12*5=60 min then complete resync with fail status.
            if (!DeserializeResyncState(tmpPeerResyncState, strPeerRemoteState, errMsg))
            {
                if (++peerJsonParseFailCnt % m_PeerRemoteResyncStateParseRetryCount)
                {
                    // Continue only in blocking mode, non blocking mode is used for instant action - fail resync in this case
                    if (isBlocking)
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s: Failed to deserialize %s remote resync state - retrying after %d seconds. Error: %s.\n", FUNCTION_NAME, m_PeerMode.c_str(), WaitTimeInSeconds, errMsg.c_str());
                        continue;
                    }
                }
                IsPeerJsonParseFailed = true;
                ssJsonParseError << "Marking current resync session failed and setting re-start resync - failed to deserialize "
                    << m_PeerMode << " remote resync state. Recent error: " << errMsg + '.';
            }
            else
            {
                // Reset count as deserialization succeeded
                peerJsonParseFailCnt = 0;
            }
        }

        if (IsPeerJsonParseFailed)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, ssJsonParseError.str().c_str());
        }
        else if (tmpPeerResyncState.CurrentState & SYNC_STATE_RESYNC_FAILED)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: %s failed resync. Reason: %s.\n", FUNCTION_NAME, m_PeerMode.c_str(),
                tmpPeerResyncState.FailureReason.c_str());
        }
        else if ((tmpPeerResyncState.CurrentState & SYNC_STATE_TARGET_RESTART_RESYNC_INLINE) && m_Args.IsSourceSide())
        {
            DebugPrintf(SV_LOG_ERROR, "%s: %s initiated inline restart resync. Reason: %s.\n", FUNCTION_NAME, m_PeerMode.c_str(),
                tmpPeerResyncState.FailureReason.c_str());
        }
        else if ((tmpPeerResyncState.CurrentState & SYNC_STATE_SOURCE_END_TIME_TO_DRIVER_DONE) && !m_Args.IsSourceSide())
        {
            // Source completed resync - stop monitoring - return
            // Update in-mem peer state

            UpdateInMemPeerState(tmpPeerResyncState);
            
            DebugPrintf(SV_LOG_ALWAYS, "%s: Source generated resync end time stamp - resync completed internally. Source resync state = %s.\n",
                FUNCTION_NAME, SyncStateToString(tmpPeerResyncState.CurrentState));
            // Raise process quit signal - this will quit resync task threads on target.
            // Main thread in target will do CompleteResyncToAzureAgent and update target resync state to SYNC_STATE_TARGET_CONCLUDED_RESYNC.
            CDPUtil::SignalQuit();

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        else if ((tmpPeerResyncState.CurrentState & SYNC_STATE_TARGET_CONCLUDED_RESYNC) && m_Args.IsSourceSide())
        {
            // Target completed resync to Azure Agent - stop monitoring - return
            // Update in-mem peer state
            UpdateInMemPeerState(tmpPeerResyncState);
            DebugPrintf(SV_LOG_ALWAYS, "%s: Target completed resync to Azure. Target resync state = %s.\n",
				FUNCTION_NAME, SyncStateToString(tmpPeerResyncState.CurrentState));
            // Raise process quit signal - this will quit resync task threads on source.
            // Main thread in source will do CompleteResyncToRcm and update target resync state to SOURCE_COMPLETED_RESYNC_TO_RCM.
            CDPUtil::SignalQuit();

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        else if ((tmpPeerResyncState.CurrentState & SYNC_STATE_RESYNC_INIT) &&
            (GetResyncState().CurrentState & SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE) &&
            m_Args.IsSourceSide())
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Target acknowledged source state SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE and did reset target state."
                " Updating replication session ID followed by reset source state.\n", FUNCTION_NAME);
            UpdateReplicationSessionId();

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return;
        }
        else
        {
            // Peer state changed but in healthy state - acquire lock and update in-mem peer state
            if(m_PeerResyncState.CurrentState != tmpPeerResyncState.CurrentState)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: %s state changed but in healthy state. Old state=%s, new state=%s.\n",
                    FUNCTION_NAME, m_PeerMode.c_str(), SyncStateToString(m_PeerResyncState.CurrentState), SyncStateToString(tmpPeerResyncState.CurrentState));
            }
            // Update in-mem peer state
            UpdateInMemPeerState(tmpPeerResyncState);
            DumpCurrentResyncState();

            // Continue monitoring
            continue;
        }

        // Fail own resync state only if fail to parse peer remote state consecutively for 1 hr or first time in non blocking mode
        // What if peer is down for more than 1 hr - lets fail own resync state
        if (IsPeerJsonParseFailed)
        {
            ResyncStateInfo curr_ResyncState = GetResyncState();
            curr_ResyncState.CurrentState = SYNC_STATE_RESYNC_FAILED;
            curr_ResyncState.RestartResync = true;
            curr_ResyncState.FailureReason = ssJsonParseError.str();

            // Call UpdateState to push own failed state to remote state and exit - in source mode this will also call complete resync in RCM with failure reason.
            UpdateState(curr_ResyncState);
        }
        else if (m_Args.IsSourceSide())
        {
            // Check for target marked any health event
            if (tmpPeerResyncState.ErrorAlertNumber == E_AZURE_CANCEL_OPERATION_ERROR_ALERT)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Target set AzureCancelOperationErrorAlert %d.\n", FUNCTION_NAME,
                    E_AZURE_CANCEL_OPERATION_ERROR_ALERT);

                AzureCancelOperationErrorAlert a(tmpPeerResyncState.FailureReason);
                if (RcmClientLib::MonitoringLib::SendAlertToRcm(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a, m_RcmConfigurator->getHostId()))
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Successfully raised health event %s\n", FUNCTION_NAME, a.GetID().c_str()); // TODO: nichougu - print alert message
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to raise health event %s\n", FUNCTION_NAME, a.GetID().c_str());
                }
            }
            else if (tmpPeerResyncState.ErrorAlertNumber == E_RESYNC_BITMAPS_INVALID_ALERT)
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Target set ResyncBitmapsInvalidAlert %d.\n", FUNCTION_NAME,
                    E_RESYNC_BITMAPS_INVALID_ALERT);
                ResyncBitmapsInvalidAlert a(m_Args.GetDiskId());
                if(RcmClientLib::MonitoringLib::SendAlertToRcm(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, a, m_RcmConfigurator->getHostId()))
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Successfully raised health event %s\n", FUNCTION_NAME, a.GetID().c_str());
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: Failed to raise health event %s\n", FUNCTION_NAME, a.GetID().c_str());
                }
            }

            if(tmpPeerResyncState.CurrentState & SYNC_STATE_TARGET_RESTART_RESYNC_INLINE)
            {
                // Source to pause resync as target marked for inline resync
                ResyncStateInfo curr_ResyncState = GetResyncState();
                curr_ResyncState.CurrentState = SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE;

                UpdateState(curr_ResyncState);
            }
            else if (tmpPeerResyncState.CurrentState & SYNC_STATE_RESYNC_FAILED)
            {
                std::stringstream msg;
                msg << FUNCTION_NAME << ": Target marked for resync fail with reason " << tmpPeerResyncState.FailureReason;
                if (tmpPeerResyncState.RestartResync)
                {
                    msg << " and marked for restart resync";
                }
                msg << ". Posting resync failure to RCM.";
                DebugPrintf(SV_LOG_ALWAYS, "%s: %s.\n", FUNCTION_NAME, msg.str().c_str());

                // This is a "target failed resync" case.
                // So no need to change/update own state.
                // Just completed resync in RCM with fail status and failure reason.
                // Then update in-mem update peer state.
                
                // Prepare RcmJobError
                RcmClientLib::RcmJobError rcmJobError;
                rcmJobError.ErrorCode;                              // TODO
                rcmJobError.Message = tmpPeerResyncState.FailureReason;
                rcmJobError.PossibleCauses;                         // TODO
                rcmJobError.RecommendedAction;                      // TODO
                rcmJobError.Severity;                               // TODO

                // Prepare CompleteSyncInput
                const ResyncStateInfo currResyncState(GetResyncState());
                RcmClientLib::SourceAgentCompleteSyncInput completeSyncInput;
                m_RcmConfigurator->GetReplicationPairMessageContext(m_Args.GetDiskId(), completeSyncInput.ReplicationContext);
                completeSyncInput.ReplicationPairId = m_ResyncRcmSettings->GetSourceAgentSyncSettings().ReplicationPairId;
                completeSyncInput.ReplicationSessionId = m_ResyncRcmSettings->GetSourceAgentSyncSettings().ReplicationSessionId;
                completeSyncInput.Status = RcmClientLib::ResyncCompletionStatus::FAILED;
                completeSyncInput.IsFailureRetriable = tmpPeerResyncState.RestartResync;
                completeSyncInput.SyncSessionStartTime = currResyncState.ResyncStartTimestamp;
                completeSyncInput.SyncSessionStartSequenceId = currResyncState.ResyncStartTimestampSeqNo;
                completeSyncInput.SyncSessionEndTime = currResyncState.ResyncEndTimestamp;
                completeSyncInput.SyncSessionEndSequenceId = currResyncState.ResyncEndTimestampSeqNo;
                m_MatchedBytesRatioSignal(completeSyncInput.MatchedBytesRatio);
                completeSyncInput.ElapsedTime = completeSyncInput.SyncSessionEndTime - completeSyncInput.SyncSessionStartTime;
                completeSyncInput.Errors.push_back(rcmJobError);
                completeSyncInput.ErrorReportingComponent = RcmClientLib::SourceComponent;

                if (CompleteSync(completeSyncInput))
                {
                    // Update in-mem peer state
                    UpdateInMemPeerState(tmpPeerResyncState);
                    DumpCurrentResyncState();
                }
                else
                {
                    // Log and quit - CompleteSync will be retried after re-spawn.
                    DebugPrintf(SV_LOG_ERROR, "%s: CompleteSync to RCM failed - it will be retried after re-spawn.\n", FUNCTION_NAME);
                }
            }
        }

        // Wait till agents settings fetch interval to avoid respawn
        CDPUtil::QuitRequested(m_RcmConfigurator->getReplicationSettingsFetchInterval());

        // Raise process quit signal
        CDPUtil::SignalQuit();

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;

    } while (isBlocking && !CDPUtil::QuitRequested(sPeerRemoteResyncStateMonitorInterval));
}

bool ResyncStateManager::ValidateNewState(const SYNC_STATE &newSyncState)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);
    
    bool retval = true;
    
    do
    {
        if(newSyncState & m_ResyncState.CurrentState)
        {
            // same state is valid state
            break;
        }

        switch (newSyncState)
        {
        case SYNC_STATE_RESYNC_INIT:
            if ((m_ResyncState.CurrentState & SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE) ||
                (m_ResyncState.CurrentState & SYNC_STATE_TARGET_RESTART_RESYNC_INLINE))
            {
                break;
            }

        case SYNC_STATE_SOURCE_START_TIME_TO_DRIVER_DONE:
            if (m_ResyncState.CurrentState & SYNC_STATE_RESYNC_INIT)
            {
                break;
            }

        case SYNC_STATE_SOURCE_END_TIME_TO_DRIVER_DONE:
            if (m_ResyncState.CurrentState & SYNC_STATE_SOURCE_START_TIME_TO_DRIVER_DONE)
            {
                break;
            }

        case SYNC_STATE_RESYNC_FAILED:
            break;

        case SYNC_STATE_TARGET_CONCLUDED_RESYNC:
            if (m_ResyncState.CurrentState & SYNC_STATE_RESYNC_INIT)
            {
                break;
            }

        case SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM:
            if (m_ResyncState.CurrentState & SYNC_STATE_SOURCE_END_TIME_TO_DRIVER_DONE)
            {
                break;
            }

        case SYNC_STATE_TARGET_RESTART_RESYNC_INLINE:
            if (m_ResyncState.CurrentState & SYNC_STATE_RESYNC_INIT)
            {
                break;
            }

        case SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE:
            if (m_ResyncState.CurrentState & SYNC_STATE_SOURCE_START_TIME_TO_DRIVER_DONE)
            {
                break;
            }

        default:
            retval = false;
            break;
        }
    
    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    
    return retval;
}
//
// Updates own new state to remote resync state.
// Also complete resync in RCM if resync is completed or failed in new state.
//
// In param newResyncState contains new state to be updated to current in-mem state and remote state.
//
bool ResyncStateManager::UpdateState(ResyncStateInfo & newResyncState)
{
    // Return true if state is same
    // If state is changed
        // call RCM API if required
        // push new state to remote resync state - retry until success
        // Acquire lock and update in-memory state
    
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);
    
    bool retval = false;
    
    if (!ValidateNewState((SYNC_STATE)newResyncState.CurrentState))
    {
        std::stringstream ss;
        ss << "UpdateState failed as new state " << SyncStateToString(newResyncState.CurrentState) <<
            " is not consistent with current state=" << SyncStateToString(m_ResyncState.CurrentState);
        throw INMAGE_EX(ss.str().c_str());
    }

    if (CompareMap(newResyncState.Properties, m_ResyncState.Properties) &&
        (m_ResyncState.CurrentState & newResyncState.CurrentState))
    {
        // Ignore and return - same state.
        DebugPrintf(SV_LOG_DEBUG, "%s: Ignoring update - same state identified. Current state=%s, new state=%s.\n",
            FUNCTION_NAME, SyncStateToString(m_ResyncState.CurrentState), SyncStateToString(newResyncState.CurrentState));
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return true;
    }

    if (m_Args.IsSourceSide() &&
        ((SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM & newResyncState.CurrentState) ||
        (SYNC_STATE_RESYNC_FAILED & newResyncState.CurrentState)))
    {
        bool isRcmCallSuccessful = true;

        // Prepare CompleteSyncInput
        RcmClientLib::SourceAgentCompleteSyncInput completeSyncInput;

        // Update common properties
        m_RcmConfigurator->GetReplicationPairMessageContext(m_Args.GetDiskId(), completeSyncInput.ReplicationContext);
        completeSyncInput.ReplicationPairId = m_ResyncRcmSettings->GetSourceAgentSyncSettings().ReplicationPairId;
        completeSyncInput.ReplicationSessionId = m_ResyncRcmSettings->GetSourceAgentSyncSettings().ReplicationSessionId;
        completeSyncInput.SyncSessionStartTime = newResyncState.ResyncStartTimestamp;
        completeSyncInput.SyncSessionStartSequenceId = newResyncState.ResyncStartTimestampSeqNo;
        completeSyncInput.SyncSessionEndTime = newResyncState.ResyncEndTimestamp;
        completeSyncInput.SyncSessionEndSequenceId = newResyncState.ResyncEndTimestampSeqNo;
        m_MatchedBytesRatioSignal(completeSyncInput.MatchedBytesRatio);
        completeSyncInput.ElapsedTime = newResyncState.ResyncEndTimestamp - newResyncState.ResyncStartTimestamp;

        if (SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM & newResyncState.CurrentState)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Source marked for complete sync to RCM with resync success. Posting CompleteSync to RCM.\n", FUNCTION_NAME);

            completeSyncInput.Status = RcmClientLib::ResyncCompletionStatus::SUCCESS;
            completeSyncInput.IsFailureRetriable = false;

            isRcmCallSuccessful = CompleteSync(completeSyncInput);
        }
        else if (SYNC_STATE_RESYNC_FAILED & newResyncState.CurrentState)
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Source marked for rsync failure. Posting CompleteSync with resync failure to RCM.\n", FUNCTION_NAME);

            // Prepare RcmJobError
            RcmClientLib::RcmJobError rcmJobError;
            rcmJobError.ErrorCode;                              // TODO
            rcmJobError.Message = newResyncState.FailureReason;
            rcmJobError.PossibleCauses;                         // TODO
            rcmJobError.RecommendedAction;                      // TODO
            rcmJobError.Severity;                               // TODO

            completeSyncInput.Status = RcmClientLib::ResyncCompletionStatus::FAILED;
            completeSyncInput.IsFailureRetriable = newResyncState.RestartResync;
            completeSyncInput.Errors.push_back(rcmJobError);
            completeSyncInput.ErrorReportingComponent = RcmClientLib::SourceComponent;

            isRcmCallSuccessful = CompleteSync(completeSyncInput);
        }

        // If RCM call is failed and quit requested then do not push state to PS cache and return.
        if (!isRcmCallSuccessful && CDPUtil::QuitRequested())
        {
            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return false;
        }
    }

    do
    {
        std::string strRemoteState;
        std::string errMsg;
        if (!SerializeResyncState(newResyncState, strRemoteState, errMsg))
        {
            std::stringstream ss;
            ss << "SerializeResyncState failed with error " << errMsg << '.';
            throw INMAGE_EX(ss.str().c_str());
        }

        if (!PushState(strRemoteState, errMsg))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: PushState failed - retrying after %d seconds. Error: %s.\n", FUNCTION_NAME, WaitTimeInSeconds, errMsg.c_str());
            continue;
        }
        else
        {
            // update in-memory state
            m_ResyncState = newResyncState;
            DebugPrintf(SV_LOG_ALWAYS, "%s: Successfully updated remote %s resync state to %s.\n",
                FUNCTION_NAME, m_Mode.c_str(), SyncStateToString(m_ResyncState.CurrentState));
        }

        // Safe to check this in both source and target
        switch(newResyncState.CurrentState)
        {
        case SYNC_STATE_SOURCE_COMPLETED_RESYNC_TO_RCM:     // True only on source
        case SYNC_STATE_TARGET_CONCLUDED_RESYNC:            // True only on target
        case SYNC_STATE_TARGET_RESTART_RESYNC_INLINE:       // True only on target
        case SYNC_STATE_RESYNC_FAILED:                      // This is source/target

            // Wait till agents settings fetch interval to avoid respawn
            CDPUtil::QuitRequested(m_RcmConfigurator->getReplicationSettingsFetchInterval());

            // Issue process quit signal
            CDPUtil::SignalQuit();
        
        default:
            break;
        }
        
        retval = true;
        
        break;

    } while (!CDPUtil::QuitRequested(WaitTimeInSeconds));

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return retval;
}

void ResyncStateManager::UpdateInMemPeerState(const ResyncStateInfo & newPeerResyncState)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // Acquire lock and update in-mem peer state
    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);
    m_PeerResyncState = newPeerResyncState;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

const std::string ResyncStateManager::GetRemoteResyncStateFileSpec() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_RemoteResyncStateFileSpec;
}

void ResyncStateManager::ResetSourceResyncState()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_Args.IsSourceSide())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Ignoring - called in target mode.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    ResyncStateInfo resyncState = GetResyncState();
    const std::map<std::string, std::string>::iterator itr = resyncState.Properties.find(SourcePropertyReplicationSessionId);
    if ((itr != resyncState.Properties.end()) && itr->second.empty())
    {
        throw INMAGE_EX("ReplicationSessionId is empty");
    }

    const std::size_t found = itr->second.find_last_of(UNDERSCORE);
    const size_t len = itr->second.length();
    int currInlineResyncCount = 0;
    if (found != std::string::npos)
    {
        try
        {
            currInlineResyncCount = boost::lexical_cast<int>(itr->second.substr(found + 1));
        }
        catch (const std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed to read current inline resync count in ReplicationSessionId %s with exception %s.\n",
                FUNCTION_NAME, itr->second.c_str(), e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed to read current inline resync count in ReplicationSessionId %s.\n",
                FUNCTION_NAME, itr->second.c_str());
        }
    }

    if (currInlineResyncCount)
    {
        try
        {
            itr->second.replace(found + 1, len - found, boost::lexical_cast<std::string>(++currInlineResyncCount));
        }
        catch (const std::exception& e)
        {
            throw INMAGE_EX(std::string("failed with exception ") + e.what());
        }
        catch (...)
        {
            throw INMAGE_EX(std::string("failed with an unknown exception."));
        }
    }
    else
    {
        // 1st inline restart resync of current session - append current ReplicationSessionId with inline restart resync count 1.
        itr->second += UNDERSCORE + boost::lexical_cast<std::string>(++currInlineResyncCount);
    }
    resyncState.CurrentState = SYNC_STATE_RESYNC_INIT;
    resyncState.ResyncStartTimestamp = 0;
    resyncState.ResyncStartTimestampSeqNo = 0;
    resyncState.ResyncEndTimestamp = 0;
    resyncState.ResyncEndTimestampSeqNo = 0;
    resyncState.FailureReason.clear();
    resyncState.RestartResync = false;
    if (!UpdateState(resyncState))
    {
        throw INMAGE_EX("Failed to update ReplicationSessionId in remote resync state");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void ResyncStateManager::UpdateReplicationSessionId()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_Args.IsSourceSide())
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Ignoring - called in target mode.\n", FUNCTION_NAME);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
        return;
    }

    ResyncStateInfo resyncState = GetResyncState();
    const std::map<std::string, std::string>::iterator itr = resyncState.Properties.find(SourcePropertyReplicationSessionId);
    if ((itr == resyncState.Properties.end()) || ((itr != resyncState.Properties.end()) && itr->second.empty()))
    {
        // Upgrade case where older version of DataprotecctionSyncRcm does not contain ReplicationSessionId property.
        resyncState.Properties[SourcePropertyReplicationSessionId] = m_ResyncRcmSettings->GetSourceAgentSyncSettings().ReplicationSessionId;
       
        if (!UpdateState(resyncState))
        {
            throw INMAGE_EX("Failed to update ReplicationSessionId in remote resync state");
        }
    }

    if ((resyncState.CurrentState & SYNC_STATE_SOURCE_PAUSE_RESYNC_DONE) &&
        (GetPeerResyncState().CurrentState & SYNC_STATE_RESYNC_INIT))
    {
        ResetSourceResyncState();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

std::string ResyncStateManager::GetReplicationSessionId() const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::unique_lock<boost::recursive_mutex> guard(m_ResyncStateManagerLock);

    std::string replicationSessionId = m_ResyncRcmSettings->GetSourceAgentSyncSettings().ReplicationSessionId;
    
    std::map<std::string, std::string>::const_iterator citrReplicationSessionId;
    if (m_Args.IsSourceSide())
    {
        citrReplicationSessionId = m_ResyncState.Properties.find(SourcePropertyReplicationSessionId);
        if (citrReplicationSessionId != m_ResyncState.Properties.end())
        {
            replicationSessionId = citrReplicationSessionId->second;
        }
    }
    else
    {
        citrReplicationSessionId = m_PeerResyncState.Properties.find(SourcePropertyReplicationSessionId);
        if (citrReplicationSessionId != m_PeerResyncState.Properties.end())
        {
            replicationSessionId = citrReplicationSessionId->second;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return replicationSessionId;
}


const boost::shared_ptr<ResyncStateManager> ResyncStateManager::GetResyncStateManager(
    const DpRcmProgramArgs &dpProgramArgs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    boost::shared_ptr<ResyncStateManager> resyncStateManager;

    if (!m_IsInitialized.load(boost::memory_order_consume))
    {
        boost::mutex::scoped_lock guard(m_Lock);
        if (!m_IsInitialized.load(boost::memory_order_consume))
        {
            resyncStateManager.reset(new ResyncStateManager(dpProgramArgs));

            if (!resyncStateManager->InitializeState())
            {
                throw INMAGE_EX("Failed to initialize ResyncStateManager instance.");
            }

            m_IsInitialized = true;

            resyncStateManager->UpdateReplicationSessionId();
        }
    }
    else
    {
        throw INMAGE_EX("An instance of ResyncStateManager is already initialized earlier, only once ResyncStateManager instance is allowed.");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return resyncStateManager;
}

