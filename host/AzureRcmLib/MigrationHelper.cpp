#include "MigrationHelper.h"
#include "ServiceHelper.h"
#include "VacpErrorCodes.h"
#include "filterdrvifmajor.h"
#include "volumeinfocollector.h"

#include <boost/lexical_cast.hpp>

using namespace RcmClientLib;

#define MIGRATION_HELPER_CATCH_EXCEPTION(errMsg, status) \
    catch (const std::exception& ex) \
        { \
        errMsg += ex.what(); \
        status = SVE_ABORT; \
        } \
    catch (const char *msg) \
        { \
        errMsg += msg; \
        status = SVE_ABORT; \
        } \
    catch (const std::string &msg) \
        { \
        errMsg += msg; \
        status = SVE_ABORT; \
        } \
    catch (...) \
        { \
        errMsg += "unknown error"; \
        status = SVE_ABORT; \
        }

MigrationHelper::MigrationHelper(bool initRequired)
    :m_QuitEvent(new ACE_Event(1, 0))
{
    if (initRequired) {
        std::string errMsg;
        SVSTATUS status = CreateDriverStream(errMsg);
        if (status != SVS_OK)
        {
            throw INMAGE_EX(errMsg.c_str());
        }
        m_commitTag.Init(m_pDriverStream, true);
    }
}

SVSTATUS MigrationHelper::CreateDriverStream(std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;

    FilterDrvIfMajor drvif;
    std::string driverName = drvif.GetDriverName(VolumeSummary::DISK);

    if (!m_pDriverStream) {
        try {
            m_pDriverStream.reset(new DeviceStream(Device(driverName)));
            DebugPrintf(SV_LOG_DEBUG, "Created device stream.\n");
        }
        catch (std::bad_alloc &e) {
            errMsg = "Memory allocation failed for creating driver stream object with exception: ";
            errMsg += e.what();
            DebugPrintf(SV_LOG_ERROR, "%s : error %s.\n", FUNCTION_NAME, errMsg.c_str());
            status = SVE_OUTOFMEMORY;
        }
    }

    //Open device stream in asynchrnous mode
    if (m_pDriverStream && !m_pDriverStream->IsOpen()) {
        if (SV_SUCCESS == m_pDriverStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_Asynchronous | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW))
            DebugPrintf(SV_LOG_DEBUG, "Opened driver %s in async mode.\n", driverName.c_str());
        else
        {
            errMsg = "Failed to open driver";
            DebugPrintf(SV_LOG_ERROR, "%s : error %s %d.\n",
                FUNCTION_NAME, errMsg.c_str(), m_pDriverStream->GetErrorCode());
            status = SVE_FILE_OPEN_FAILED;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

SVSTATUS MigrationHelper::initializeRcmSettings(const RcmRegistratonMachineInfo& machineInfo, 
    std::string& errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    LocalConfigurator localConfig;
    std::string rcmClientSettingsPath = localConfig.getRcmSettingsPath();
    try {
        boost::system::error_code ec;
        if (!boost::filesystem::exists(rcmClientSettingsPath, ec))
        {
            std::ofstream sourceSettingFile(rcmClientSettingsPath.c_str());
            sourceSettingFile.close();
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s RCMInfo.conf already present at path=%s\n", FUNCTION_NAME, rcmClientSettingsPath.c_str());
        }
        RcmClientSettings sourceSetting(rcmClientSettingsPath);
        sourceSetting.m_BiosId = GetSystemUUID();
        sourceSetting.PersistRcmClientSettings();
        localConfig.setRcmSettingsPath(rcmClientSettingsPath);
        const bool isCredLessDiscovery = boost::lexical_cast<bool>(machineInfo.IsCredentialLessDiscovery);
        localConfig.setIsCredentialLessDiscovery(isCredLessDiscovery);
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Initialization of RCMInfo.conf file failed.Location =%s, Exception=%s\n", FUNCTION_NAME, rcmClientSettingsPath.c_str(), e.what());
        errMsg+= "Initialization of RCMInfo.conf file failed.Location=" + rcmClientSettingsPath  + "Exception=" + e.what();
        status = SVE_FAIL;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

SVSTATUS MigrationHelper::RegisterAgent(const RcmRegistratonMachineInfo& machineInfo,
    ExtendedErrorLogger::ExtendedErrors& extendedErrors)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::string errMsg;
    LocalConfigurator lc;
    lc.setMigrationState(Migration::REGISTRATION_PENDING);
    extendedErrors.Errors.clear();

    try 
    {
        do {
            status = initializeRcmSettings(machineInfo, errMsg);
            if (status != SVS_OK)
            {
                ExtendedErrorLogger::ExtendedError extendedError;
                extendedError.error_name = Migration::REGISTER_APPLIANCE_INITIALIZATION_FAILED;
                extendedError.default_message = errMsg;
                extendedErrors.Errors.push_back(extendedError);
                break;
            }
            status = RcmConfigurator::getInstance()->ApplianceLogin(machineInfo, errMsg);
            if (status != SVS_OK)
            {
                ExtendedErrorLogger::ExtendedError extendedError;
                extendedError.error_name = Migration::SOURCE_AGENT_UNABLE_TO_CONNECT_WITH_APPLIANCE;
                extendedError.error_params[VM_NAME] = RcmConfigurator::getInstance()->GetFqdn();
                extendedError.error_params[APPLIANCE_FQDN] = machineInfo.ApplianceFqdn;
                extendedError.default_message = errMsg;
                extendedErrors.Errors.push_back(extendedError);
                break;
            }
            status = RcmConfigurator::getInstance()->CompleteAgentRegistration();
            if (status != SVS_OK)
            {
                errMsg += "Agent Registation Failed.";
                ExtendedErrorLogger::ExtendedError extendedError;
                extendedError.error_name = Migration::RCM_REGISTRATION_POLICY_FAILURE;
                extendedError.default_message = errMsg;
                extendedErrors.Errors.push_back(extendedError);
                break;
            }

            lc.setMigrationState(Migration::REGISTRATION_SUCCESS);
        } while (false);        
    }
    catch (const std::exception& e)
    {
        status = SVE_FAIL;
        errMsg += "RegisterAgent failed." + std::string(e.what());
        DebugPrintf(SV_LOG_ERROR, "%s RegisterAgent failed with ErrorMessage= %s.\n",
            FUNCTION_NAME, e.what());     
    }

    if (m_errMsg.empty())
        m_errMsg = errMsg;

    if (status != SVS_OK)
        lc.setMigrationState(Migration::REGISTRATION_FAILURE);

    return status;
}

SVSTATUS MigrationHelper::RequestVacp(VacpConf::State requestState,
    VacpConf::State finalState, std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    VacpConf::State state;
    ACE_Time_Value waitTime(VacpConf::WaitTime, 0);

    LocalConfigurator lc;
    lc.setVacpState(requestState);
    boost::chrono::system_clock::time_point endTime =
        boost::chrono::system_clock::now() +
        boost::chrono::seconds(VacpConf::RequestTimeOut);
    boost::chrono::system_clock::time_point lastChecked =
        boost::chrono::system_clock::time_point::min();

    while(!m_threadManager.testcancel(ACE_Thread::self()))
    {
        boost::chrono::system_clock::time_point curTime = boost::chrono::system_clock::now();
        if (curTime > endTime)
        {
            std::string error = "Vacp stop timedout";
            errMsg += error;
            status = SVE_ABORT;
            break;
        }

        if (curTime > lastChecked + boost::chrono::seconds(VacpConf::PollTime))
        {
            LocalConfigurator lc;
            state = lc.getVacpState();
            DebugPrintf(SV_LOG_ALWAYS, "%s : Vacp state : %d\n", FUNCTION_NAME, state);
            if(state == finalState)
            {
                status = SVS_OK;
                break;
            }
            lastChecked = curTime;
        }
        m_QuitEvent->wait(&waitTime, 0);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS MigrationHelper::RequestStartVacp(std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    status = RequestVacp(VacpConf::VACP_START_REQUESTED, VacpConf::VACP_STARTED, errMsg);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS MigrationHelper::RequestStopVacp(std::string &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    status = RequestVacp(VacpConf::VACP_STOP_REQUESTED, VacpConf::VACP_STOPPED, errMsg);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS MigrationHelper::BlockDrainTag(
    const RcmFinalReplicationCycleInfo& replicationCycleInfo,
    const std::string &policyId,
    const std::vector<std::string>& protectedDiskIds,
    ExtendedErrorLogger::ExtendedErrors &extendedErrors)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg = "Failed to issue final bookmark.";

    int retryCount = 0;
    bool vacpStopped = false;

    boost::thread *jobThread;
    boost::thread *ioServiceThread;
    boost::thread_group threadGroup;
    boost::thread_group ioServiceThreadGroup;

    boost::asio::io_service ioService;
    boost::asio::deadline_timer commitTimeoutTimer(ioService);

    // this is both error message and output in case of success
    m_errMsg.clear();

    try {
        do {
            TagCommitNotifyState expected = TAG_COMMIT_NOTIFY_STATE_NOT_INPROGRESS;
            if (!m_commitTag.CheckAnotherInstance(expected))
            {
                errMsg = "Another instance of tag commit notify is inprogress. state: " +
                    boost::lexical_cast<std::string>(expected);
                status = SVE_INVALIDARG;
                break;
            }

            if (!m_pDriverStream->IsOpen())
            {
                errMsg = "Filter driver is not initialized.";
                status = SVE_FAIL;
                break;
            }

            if (!m_commitTag.IsCommitTagNotifySupported())
            {
                errMsg = "Filter driver version doesn't support block drain tag.";
                status = SVE_FAIL;
                ExtendedErrorLogger::ExtendedError extendedError;
                extendedError.error_name = Migration::DRIVER_NOT_SUPPORTING_BLOCK_DRAIN_TAG;
                extendedError.default_message = errMsg;
                extendedErrors.Errors.push_back(extendedError);
                break;
            }

            CommitTagBookmarkInput commitTagBookmarkInput(replicationCycleInfo.TagType,
                replicationCycleInfo.TagGuid);
            std::string vacpArgs, appPolicyLogPath, preLogPath;

            status = m_commitTag.PrepareFinalBookmarkCommand(commitTagBookmarkInput, policyId,
                vacpArgs, appPolicyLogPath, preLogPath);
            if (status != SVS_OK)
            {
                errMsg = "No concistency settings found.";
                status = SVE_FAIL;
                break;
            }

            retryCount = 0;
            while (retryCount < VacpConf::MaxRetryCount)
            {
                // stop regular consistency bookmarks
                std::string strMsg;
                status = RequestStopVacp(strMsg);
                if (status == SVS_OK)
                    break;
                retryCount++;
                errMsg = strMsg;
            }
            if (status != SVS_OK)
            {
                break;
            }

            vacpStopped = true;

            // issue commit tag notify
            jobThread = new boost::thread(boost::bind(&MigrationHelper::IssueCommitTagNotify,
                this, replicationCycleInfo, protectedDiskIds));
            threadGroup.add_thread(jobThread);

            DebugPrintf(SV_LOG_ALWAYS, "waiting for commit tag ioctl to be issued to driver.\n",
                FUNCTION_NAME);

            do {
                // wait until the ioctl is issued
                ACE_Time_Value waitTime(10, 0);
                m_QuitEvent->wait(&waitTime, 0);
            } while (!m_threadManager.testcancel(ACE_Thread::self()) &&
                (m_commitTag.GetTagCommitNotifierState() == TAG_COMMIT_NOTIFY_STATE_NOT_ISSUED));

            if (m_threadManager.testcancel(ACE_Thread::self()))
            {
                // no need to update the job on service quit
                DebugPrintf(SV_LOG_ALWAYS, "%s: skipping final bookmark command on quit\n", FUNCTION_NAME);
                return SVE_ABORT;
            }

            if (m_commitTag.GetTagCommitNotifierState() >
                TAG_COMMIT_NOTIFY_STATE_WAITING_FOR_COMPLETION)
            {
                // the job status is set by the IssueCommitNotify, so no need to update the status here
                errMsg = "tag commit notify completed before issuing final bookmark command.";
                status = SVE_FAIL;
                break;
            }

            // set timeout to a little bit more than the max tag commit time for vacp process to run
            SV_UINT uTimeOut =
                (RcmConfigurator::getInstance()->getVacpTagCommitMaxTimeOut() / 1000) + 30;

            commitTimeoutTimer.expires_from_now(boost::posix_time::seconds(uTimeOut + 30));
            commitTimeoutTimer.async_wait(boost::bind(
                &TagCommitNotifier::HandleCommitTagNotifyTimeout,
                &m_commitTag, boost::asio::placeholders::error));

            ioServiceThread = new boost::thread(boost::bind(&boost::asio::io_service::run, &ioService));
            ioServiceThreadGroup.add_thread(ioServiceThread);

            SV_ULONG lInmexitCode;
            lInmexitCode = m_commitTag.RunFinalBookmarkCommand(vacpArgs, appPolicyLogPath,
                preLogPath, uTimeOut, true);

            if (lInmexitCode != 0)
            {
                status = SVE_FAIL;
                if (lInmexitCode == VACP_DISK_NOT_IN_DATAMODE)
                {
                    errMsg = "Failed to issue bookmark as the disk is not in write order mode.";
                    ExtendedErrorLogger::ExtendedError extendedError;
                    extendedError.error_name = Migration::DISK_NOT_IN_WO_MODE;
                    extendedError.default_message = errMsg;
                    extendedErrors.Errors.push_back(extendedError);
                }
                break;
            }
        } while (0);
    }
    MIGRATION_HELPER_CATCH_EXCEPTION(errMsg, status);

    threadGroup.join_all();
    commitTimeoutTimer.cancel();
    ioService.stop();
    ioServiceThreadGroup.join_all();

    if (status != SVE_INVALIDARG)
    {
        if (m_commitTag.GetTagCommitNotifierState() == TAG_COMMIT_NOTIFY_STATE_FAILED)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Tag notifier failed, changing status to failure\n", FUNCTION_NAME);

            status = SVE_FAIL;
        }

        m_commitTag.SetTagCommitNotifierState(TAG_COMMIT_NOTIFY_STATE_NOT_INPROGRESS);
    }

    // check if the final bookmark details exist
    if (status == SVS_OK)
    {
        bool validBookmarkOutput = false;
        try {
            ReplicationBookmarkOutput bookmarkOutput =
                JSON::consumer<ReplicationBookmarkOutput>::convert(m_errMsg);

            if (bookmarkOutput.SourceDiskIdToBookmarkDetailsMap.empty())
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Final bookmark are empty, changing status to failure\n", FUNCTION_NAME);
            }
            else if (protectedDiskIds.size() != bookmarkOutput.SourceDiskIdToBookmarkDetailsMap.size())
            {
                DebugPrintf(SV_LOG_ERROR, "%s: Final bookmark details disk list mismatch, changing status to failure\n", FUNCTION_NAME);
            }
            else
            {
                validBookmarkOutput = true;
            }
        }
        catch (...)
        {
            // failure to deserialize is an error
        }

        if (!validBookmarkOutput)
            status = SVE_FAIL;
    }

    if (vacpStopped)
    {
        SVSTATUS startVacpStatus = SVS_OK;
        std::string startVacpMsg;
        retryCount = 0;
        while (retryCount < VacpConf::MaxRetryCount)
        {
            // restart regular consistency bookmarks
            std::string strMsg;
            startVacpStatus = RequestStartVacp(strMsg);
            if (startVacpStatus == SVS_OK)
                break;
            retryCount++;
            startVacpMsg = strMsg;
        }

        // this is required for failures in uploading the tags, need to keep the replication healthy
        if ((status == SVS_OK) && (startVacpStatus != SVS_OK))
        {
            status = startVacpStatus;
            errMsg = startVacpMsg;
        }
    }

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "Failed to issue final bookmark."
            "policy Id : %s, TagType : %s, TagGuid : %s, error: %s\n",
            policyId.c_str(),
            replicationCycleInfo.TagType.c_str(),
            replicationCycleInfo.TagGuid.c_str(),
            errMsg.c_str());

        // set the status here only if the IssueCommitTagNotify() has not set
        if (m_errMsg.empty())
        {
            m_errMsg = errMsg;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void MigrationHelper::IssueCommitTagNotify(
    const RcmFinalReplicationCycleInfo& replicationCycleInfo,
    const std::vector<std::string>& protectedDiskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    std::string errMsg = "Failed to issue final bookmark.";
    std::stringstream ssPossibleCauses;
    CommitTagBookmarkInput commitTagBookmarkInput(replicationCycleInfo.TagType,
        replicationCycleInfo.TagGuid);
    CommitTagBookmarkOutput commitTagBookmarkOutput;

    try {
        do {
            status = m_commitTag.IssueCommitTagNotify(commitTagBookmarkInput,
                commitTagBookmarkOutput, protectedDiskIds, ssPossibleCauses);
            if (status != SVS_OK)
            {
                if (!ssPossibleCauses.str().empty())
                {
                    errMsg = ssPossibleCauses.str();
                }
                break;
            }

#ifdef SV_UNIX
            uint64_t epochdiff = (static_cast<uint64_t>(116444736) * static_cast<uint64_t>(1000000000));
            std::map<std::string, FinalReplicationBookmarkDetails>& SourceDiskIdToBookmarkDetailsMap =
                commitTagBookmarkOutput.BookmarkOutput.SourceDiskIdToBookmarkDetailsMap;                       
            std::map<std::string, FinalReplicationBookmarkDetails>::iterator itr;
            for (itr = SourceDiskIdToBookmarkDetailsMap.begin(); itr != SourceDiskIdToBookmarkDetailsMap.end(); itr++)
            {
                itr->second.BookmarkTime -= epochdiff;
            }
#endif

            std::string strJobOutput =
                JSON::producer<ReplicationBookmarkOutput>::convert(
                commitTagBookmarkOutput.BookmarkOutput);
            DebugPrintf(SV_LOG_ALWAYS, "%s : Issue commit tag bookmark output : %s\n",
                FUNCTION_NAME, strJobOutput.c_str());
            m_errMsg = strJobOutput;
        } while (false);
    }
    MIGRATION_HELPER_CATCH_EXCEPTION(errMsg, status);

    if (status != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,
            "Failed to issue commit tag notify. TagType : %s, TagGuid : %s, error: %s\n",
            replicationCycleInfo.TagType.c_str(),
            replicationCycleInfo.TagGuid.c_str(),
            errMsg.c_str());

            m_errMsg = errMsg;
    }

    m_commitTag.SetTagCommitNotifierState((status == SVS_OK) ? TAG_COMMIT_NOTIFY_STATE_JOB_STATUS_UPDATED : TAG_COMMIT_NOTIFY_STATE_FAILED);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void MigrationHelper::GetPnameDiskList(const std::set<std::string>& protectedDiskIds,
    std::vector<std::string>& pnameDiskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string pname;

    for (std::set<std::string>::const_iterator diskIter = protectedDiskIds.begin();
        diskIter != protectedDiskIds.end(); diskIter++)
    {
        pname = *diskIter;
        // persistent name for /dev/sda is _dev_sda
        boost::replace_all(pname, "/", "_");
        pnameDiskIds.push_back(pname);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

#ifdef SV_UNIX
SVSTATUS MigrationHelper::UpdatePNameInBookmarkOutput()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    std::map<std::string, std::string> pNameMap;
    std::map<std::string, std::string>::iterator pNameIter;
    std::string hypervisorName;
    VolumeSummaries_t volumeSummaries;
    VolumeDynamicInfos_t volumeDynamicInfos;
    VolumeInfoCollector volumeCollector(
        (DeviceNameTypes_t)GetDeviceNameTypeToReport(hypervisorName, true));
    volumeCollector.GetVolumeInfos(volumeSummaries, volumeDynamicInfos, false);
	VolumeSummaries_t::iterator volIt = volumeSummaries.begin();
	for (; volIt != volumeSummaries.end(); volIt++)
	{
        if (volIt->type != VolumeSummary::DISK)
        {
            continue;
        }

        ConstAttributesIter_t cit;
        cit = volIt->attributes.find(NsVolumeAttributes::DEVICE_NAME);
        if (cit == volIt->attributes.end())
        {
            continue;
        }
        pNameMap[cit->second] = volIt->name;
    }

    ReplicationBookmarkOutput bookmarkOutput =
        JSON::consumer<ReplicationBookmarkOutput>::convert(m_errMsg);
    std::map<std::string, FinalReplicationBookmarkDetails> diskIdOldMap =
        bookmarkOutput.SourceDiskIdToBookmarkDetailsMap;
    std::map<std::string, FinalReplicationBookmarkDetails> diskIdNewMap;
    std::map<std::string, FinalReplicationBookmarkDetails>::iterator it;
    for(it = diskIdOldMap.begin(); it != diskIdOldMap.end(); it++)
    {
        std::string diskName = it->first;
        boost::replace_all(diskName, "_", "/");
        pNameIter = pNameMap.find(diskName);
        if (pNameIter == pNameMap.end())
        {
            m_errMsg = "Failed to fetch new pname for " + diskName;
            status = SVE_FAIL;
            break;
        }
        diskIdNewMap[pNameIter->second] = it->second;
    }

    if (status == SVS_OK)
    {
        bookmarkOutput.SourceDiskIdToBookmarkDetailsMap = diskIdNewMap;
        m_errMsg = JSON::producer<ReplicationBookmarkOutput>::convert(bookmarkOutput);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
#endif

SVSTATUS MigrationHelper::UnblockDrain(const std::vector<std::string> &pnameDiskIds,
    std::stringstream &errMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    status = m_commitTag.UnblockDrain(pnameDiskIds, errMsg);
    if (status == SVS_OK)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s : Unblock drain succeeded.\n",
            FUNCTION_NAME);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "%s : Unblock drain failed with error : %s.\n",
            FUNCTION_NAME, errMsg.str().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS MigrationHelper::FinalReplicationCycle(
    const RcmFinalReplicationCycleInfo& replicationCycleInfo,
    const std::string& policyId, const std::set<std::string>& protectedDiskIds,
    ExtendedErrorLogger::ExtendedErrors &extendedErrors)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::vector<std::string> pnameDiskIds;
    std::string errMsg;
    std::stringstream strErrMsg;
    LocalConfigurator lc;
    bool doUnblockDrain = false;

    try
    {
        do
        {
            GetPnameDiskList(protectedDiskIds, pnameDiskIds);
            status = m_commitTag.VerifyDrainUnblock(pnameDiskIds);
            if (status != SVS_OK)
            {
                if (status == SVE_NOTIMPL)
                {
                    errMsg = "Filter driver version doesn't support block drain tag.";
                    status = SVE_FAIL;
                    ExtendedErrorLogger::ExtendedError extendedError;
                    extendedError.error_name = "DriverNotSupportingBlockDrainTag";
                    extendedError.default_message = errMsg;
                    extendedErrors.Errors.push_back(extendedError);
                    break;
                }
                status = UnblockDrain(pnameDiskIds, strErrMsg);
            }
            if (status != SVS_OK)
            {
                errMsg = strErrMsg.str();
                break;
            }

            doUnblockDrain = true;
            status = BlockDrainTag(replicationCycleInfo, policyId, pnameDiskIds, extendedErrors);
            if (status != SVS_OK)
            {
                break;
            }
#ifdef SV_UNIX
            status = UpdatePNameInBookmarkOutput();
            if (status == SVS_OK)
            {
                break;
            }
#endif
        } while(false);

        //In case of failure unblock the drain
        if (doUnblockDrain && status != SVS_OK)
        {
            UnblockDrain(pnameDiskIds, strErrMsg);
        }
    }
    MIGRATION_HELPER_CATCH_EXCEPTION(errMsg, status);

    if (status == SVS_OK)
    {
        lc.setMigrationState(Migration::FINAL_REPLICATION_CYCLE_POLICY_SUCCESS);
        DebugPrintf(SV_LOG_ALWAYS,
            "%s : Final Replication Cycle Policy Succeeded with output details : %s.\n",
            FUNCTION_NAME, m_errMsg.c_str());
    }
    else
    {
        if (m_errMsg.empty())
        {
            m_errMsg = errMsg;
        }

        lc.setMigrationState(Migration::FINAL_REPLICATION_CYCLE_POLICY_FAILURE);
        DebugPrintf(SV_LOG_ERROR, "%s : Final Replication Cycle Policy failed. Error : %s\n",
            FUNCTION_NAME, m_errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

#ifdef SV_UNIX
SVSTATUS MigrationHelper::ModifyPName(const std::set<std::string>& protectedDiskIds,
    const std::vector<std::string>& pnameDiskIds, std::stringstream &strErrMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    std::vector<std::string>::const_iterator pNameIter = pnameDiskIds.begin();
    std::set<std::string>::const_iterator diskIter = protectedDiskIds.begin();
    while(diskIter != protectedDiskIds.end() && pNameIter != pnameDiskIds.end())
    {
        std::string devName = *diskIter;
        std::string pName = *pNameIter;
        status = m_commitTag.ModifyPName(devName, pName, strErrMsg);
        if (status != SVS_OK)
        {
            break;
        }
        diskIter++;
        pNameIter++;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
#endif

SVSTATUS MigrationHelper::ResumeReplication(
    const std::set<std::string>& protectedDiskIds,
    ExtendedErrorLogger::ExtendedErrors &extendedErrors)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    std::stringstream strErrMsg;
    std::vector<std::string> pnameDiskIds;
    LocalConfigurator lc;
    m_errMsg = "Resume replication failed with errors";

    do
    {
        GetPnameDiskList(protectedDiskIds, pnameDiskIds);

#ifdef SV_UNIX
        status = ModifyPName(protectedDiskIds, pnameDiskIds, strErrMsg);
        if (status != SVS_OK)
        {
            break;
        }
#endif

        status = UnblockDrain(pnameDiskIds, strErrMsg);
    } while(false);

    if (status == SVS_OK)
    {
        lc.setMigrationState(Migration::RESUME_REPLICATION_POLICY_SUCCESS);
        DebugPrintf(SV_LOG_ALWAYS, "%s : Resume Replication Policy Succeeded.\n",
            FUNCTION_NAME);
    }
    else
    {
        if (!strErrMsg.str().empty())
        {
            m_errMsg = strErrMsg.str();
        }
        lc.setMigrationState(Migration::RESUME_REPLICATION_POLICY_FAILURE);
        DebugPrintf(SV_LOG_ALWAYS, "%s : Resume Replication Policy failed. Error : %s\n",
            FUNCTION_NAME, m_errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS MigrationHelper::Migrate(ExtendedErrorLogger::ExtendedErrors &extendedErrors)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    LocalConfigurator lc;
    m_errMsg = "Migration policy failed with errors";
    std::string errMsg;


    lc.setMigrationState(Migration::MIGRATION_SWITCHED_TO_RCM_PENDING);
    status = ServiceHelper::UpdateCSTypeAndRestartSVAgent(std::string("CSPrime"), errMsg);
    
    if (status != SVS_OK)
    {
        if (!errMsg.empty())
        {
            m_errMsg = errMsg;
        }
        lc.setMigrationState(Migration::MIGRATION_ROLLBACK_PENDING);
        DebugPrintf(SV_LOG_ALWAYS, "%s : Migration Policy failed. Error : %s\n",
            FUNCTION_NAME, m_errMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
