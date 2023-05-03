///
/// \file CommitTag.cpp
///
/// \brief
///

#include "CommitTag.h"
#include "filterdrvifmajor.h"
#include "appcommand.h"
#include "inmsafeint.h"
#include "RcmConfigurator.h"
#include "portablehelpersmajor.h"

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>


#ifdef SV_UNIX
#include "executecommand.h"
#include "devicefilter.h"
#include "involfltfeatures.h"
#else
#include <boost/process.hpp>
#include "indskfltfeatures.h"
#endif

TagCommitNotifier::TagCommitNotifier()
{
    m_tagCommitNotifyState = TAG_COMMIT_NOTIFY_STATE_NOT_INPROGRESS;
}

TagCommitNotifier::~TagCommitNotifier()
{
}

void TagCommitNotifier::Init(DeviceStream::Ptr pDriverStream, bool blockDrain)
{
    m_pDriverStream = pDriverStream;
    m_blockDrain = blockDrain;
}

TagCommitNotifyState TagCommitNotifier::GetTagCommitNotifierState()
{
    return m_tagCommitNotifyState;
}

void TagCommitNotifier::SetTagCommitNotifierState(TagCommitNotifyState tagCommitNotifyState)
{
    m_tagCommitNotifyState = tagCommitNotifyState;
}

bool TagCommitNotifier::CheckAnotherInstance(TagCommitNotifyState &expected)
{
    return m_tagCommitNotifyState.compare_exchange_strong(expected,
        TAG_COMMIT_NOTIFY_STATE_NOT_ISSUED);
}

SVSTATUS TagCommitNotifier::PrepareFinalBookmarkCommand(
    const CommitTagBookmarkInput &commitTagBookmarkInput, const std::string &jobId,
    std::string &vacpArgs, std::string &appPolicyLogPath, std::string &preLogPath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    LocalConfigurator lc;
    std::string installPath, logDirName;
    ReplicationBookmarkInput bookmarkInput = commitTagBookmarkInput.BookmarkInput;

    installPath = lc.getInstallPath();
#ifdef SV_WINDOWS
    logDirName = lc.getCacheDirectory();
#else
    logDirName = lc.getLogPathname();
#endif
    appPolicyLogPath = logDirName;
    appPolicyLogPath += ACE_DIRECTORY_SEPARATOR_CHAR;
    appPolicyLogPath += "ApplicationPolicyLogs";
    appPolicyLogPath += ACE_DIRECTORY_SEPARATOR_CHAR;
    SVMakeSureDirectoryPathExists(appPolicyLogPath.c_str());

    ACE_Time_Value aceTv = ACE_OS::gettimeofday();
    std::stringstream fileName;
    fileName << "VacpLog_FinalBookmark_" << jobId << "_" << GetTimeInSecSinceEpoch1970() << ".log";

    preLogPath = appPolicyLogPath + "pre_" + fileName.str();
    appPolicyLogPath += fileName.str();

    // prepare vacp args
    vacpArgs = " -systemlevel ";

    if (bookmarkInput.BookmarkType == ConsistencyBookmarkTypes::SingleVmCrashConsistent ||
        bookmarkInput.BookmarkType == ConsistencyBookmarkTypes::MultiVmCrashConsistent ||
        bookmarkInput.BookmarkType == ConsistencyBookmarkTypes::CrashConsistent)
    {
        vacpArgs += " -cc";
    }

#ifndef _WIN32
    vacpArgs += " -prescript ";
    vacpArgs += installPath;
    vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
    vacpArgs += "scripts";
    vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
    vacpArgs += "vCon";
    vacpArgs += ACE_DIRECTORY_SEPARATOR_CHAR;
    vacpArgs += "AzureDiscovery.sh ";
#endif

    // Not used in drain block
    if (bookmarkInput.BookmarkType == ConsistencyBookmarkTypes::MultiVmCrashConsistent ||
        bookmarkInput.BookmarkType == ConsistencyBookmarkTypes::MultiVmApplicationConsistent)
    {
        ConsistencySettings cs;
        RcmConfigurator::getInstance()->GetConsistencySettings(cs);

        if (cs.MultiVmConsistencyParticipatingNodes.size() > 1)
        {
            // Multi-vm
            vacpArgs += " -distributed -mn ";
            vacpArgs += cs.CoordinatorMachine;
            vacpArgs += " -cn \"";

            std::vector<MultiVmConsistencyParticipatingNode>::const_iterator itr =
                cs.MultiVmConsistencyParticipatingNodes.begin();
            std::vector<MultiVmConsistencyParticipatingNode>::const_iterator itrEnd =
                cs.MultiVmConsistencyParticipatingNodes.end();

            std::string coordNodes;
            for (/*empty*/; itr != itrEnd; itr++)
            {
                if (!coordNodes.empty())
                {
                    coordNodes += ",";
                }

                coordNodes += itr->Name;
            }
            coordNodes += "\"";
            vacpArgs += coordNodes;
        }
        else
        {
            return SVE_FAIL;
        }
    }

    vacpArgs += " -tagguid ";
    vacpArgs += bookmarkInput.BookmarkId;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SV_ULONG TagCommitNotifier::RunFinalBookmarkCommand(std::string &vacpArgs,
    std::string &appPolicyLogPath, std::string &preLogPath, SV_UINT uTimeOut, bool active)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    LocalConfigurator lc;

    const std::string vacpCmd = (boost::filesystem::path(lc.getInstallPath()) / INM_VACP_SUBPATH).string();

    DebugPrintf(SV_LOG_ALWAYS, "%s: Running Final bookmark command \"%s\" %s.\n", FUNCTION_NAME, vacpCmd.c_str(), vacpArgs.c_str());

    std::string cmdOutput;
    SV_ULONG lInmexitCode = 1;
    AppCommand objAppCommand(vacpCmd, vacpArgs, uTimeOut, preLogPath);
    objAppCommand.SetLogLevel(SV_LOG_ALWAYS);
    objAppCommand.Run(lInmexitCode, cmdOutput, active, "", NULL);

    if (ACE_OS::rename(preLogPath.c_str(), appPolicyLogPath.c_str()) < 0)
    {
        int err = ACE_OS::last_error();
        std::stringstream msg;
        msg << "failed to rename " << preLogPath << ". error: " << err << "\n";
        ACE_OS::unlink(preLogPath.c_str());
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, msg.str().c_str());

        // continue even if the log file rename fails.
    }

    if (lInmexitCode != 0)
    {
        DebugPrintf(SV_LOG_ERROR, "final bookmark command failed.\n %s\n", cmdOutput.c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ALWAYS, "final bookmark command completed. output %s\n",
            cmdOutput.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return lInmexitCode;
}

bool TagCommitNotifier::IsCommitTagNotifySupported()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_pDriverStream)
    {
        throw INMAGE_EX("Driver stream not initialized.\n");
    }

    FilterDrvIfMajor drvif;
    DeviceFilterFeatures::Ptr pDeviceFilterFeatures;

    DRIVER_VERSION dv;
    drvif.GetDriverVersion(m_pDriverStream.get(), dv);
    DebugPrintf(SV_LOG_ALWAYS, "%s: Driver version : %hu.%hu.%hu.%hu\n", FUNCTION_NAME,
            dv.ulDrMajorVersion, dv.ulDrMinorVersion, dv.ulDrMinorVersion2, dv.ulDrMinorVersion3);
    DebugPrintf(SV_LOG_ALWAYS, "%s: Product version : %hu.%hu.%hu.%hu\n", FUNCTION_NAME,
            dv.ulPrMajorVersion, dv.ulPrMinorVersion, dv.ulPrMinorVersion2, dv.ulPrBuildNumber);
#ifdef SV_WINDOWS
    pDeviceFilterFeatures.reset(new InDskFltFeatures(dv));
#else
    pDeviceFilterFeatures.reset(new InVolFltFeatures(dv));
#endif

    bool bSupported = m_blockDrain ?
        pDeviceFilterFeatures->IsBlockDrainTagSupported() :
        pDeviceFilterFeatures->IsCommitTagNotifySupported();
    if (!bSupported)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: %s not supported.\n", FUNCTION_NAME,
            m_blockDrain ? "block drain" : "commit tag");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bSupported;
}

void TagCommitNotifier::CancelCommitTagNotify()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (m_pDriverStream)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: commit notify state: %d.\n", FUNCTION_NAME,
            m_tagCommitNotifyState.load());

        if (m_tagCommitNotifyState == TAG_COMMIT_NOTIFY_STATE_WAITING_FOR_COMPLETION)
        {
            FilterDrvIfMajor drvif;
            bool status = drvif.CancelTagCommitNotify(m_pDriverStream.get());
            if (!status)
            {
                DebugPrintf(SV_LOG_ERROR, "%s: cancel tag commit notify failed.\n", FUNCTION_NAME);
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void TagCommitNotifier::HandleCommitTagNotifyTimeout(const boost::system::error_code& error)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (error != boost::asio::error::operation_aborted) {
        DebugPrintf(SV_LOG_ALWAYS, "%s: canceling the commit notify.\n", FUNCTION_NAME);
        CancelCommitTagNotify();
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVSTATUS TagCommitNotifier::IssueCommitTagNotify(CommitTagBookmarkInput &commitTagBookmarkInput,
    CommitTagBookmarkOutput &commitTagBookmarkOutput,
    const std::vector<std::string>& protectedDiskIds, std::stringstream &ssPossibleCauses)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    FilterDrvIfMajor drvif;
    ReplicationBookmarkInput bookmarkInput = commitTagBookmarkInput.BookmarkInput;

    do {
        if (protectedDiskIds.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: no protected disks in settings.\n", FUNCTION_NAME);
            status = SVE_FAIL;
            break;
        }

        size_t diskIdLength = 0;
        uint32_t ulInputSize = 0, ulOutputSize = 0, ulTotalSize = 0;
        uint32_t ulNumDiskForDriverOutput = protectedDiskIds.size();

#ifdef SV_WINDOWS
        // Input and Output structure will be aligned at data bus width
        // so input length and output length has to be aligned at data bus width
        ulInputSize = ALIGNED_SIZE((FIELD_OFFSET(TAG_COMMIT_NOTIFY_INPUT,
            DeviceId[ulNumDiskForDriverOutput])), (sizeof(ULONG)));
        ulOutputSize = ALIGNED_SIZE((FIELD_OFFSET(TAG_COMMIT_NOTIFY_OUTPUT,
            TagStatus[ulNumDiskForDriverOutput])), (sizeof(ULONG)));
        ulTotalSize = ulInputSize;

        std::vector<char> outputBuffer(ulOutputSize, 0);
#else
        diskIdLength = sizeof(VOLUME_GUID);
        INM_SAFE_ARITHMETIC(ulInputSize = InmSafeInt<size_t>::Type(
            sizeof(TAG_COMMIT_NOTIFY_INPUT)) + diskIdLength * (ulNumDiskForDriverOutput - 1),
            INMAGE_EX(sizeof(TAG_COMMIT_NOTIFY_INPUT)) (diskIdLength)(ulNumDiskForDriverOutput - 1))
        INM_SAFE_ARITHMETIC(ulOutputSize = InmSafeInt<size_t>::Type(
            sizeof(TAG_COMMIT_NOTIFY_OUTPUT)) +
            sizeof(TAG_COMMIT_STATUS) * (ulNumDiskForDriverOutput - 1),
            INMAGE_EX(sizeof(TAG_COMMIT_NOTIFY_OUTPUT))
            (sizeof(TAG_COMMIT_STATUS)) (ulNumDiskForDriverOutput - 1))
        INM_SAFE_ARITHMETIC(ulTotalSize = InmSafeInt<uint32_t>::Type(ulInputSize) +
            ulOutputSize, INMAGE_EX(ulInputSize) (ulOutputSize))
#endif

        std::vector<char> inputBuffer(ulTotalSize, 0);

        TAG_COMMIT_NOTIFY_INPUT *ptrCommitTagNotifyInput = NULL;
        TAG_COMMIT_NOTIFY_OUTPUT  *ptrCommitTagNotifyOutput = NULL;

        // set input buffer
        ptrCommitTagNotifyInput = (TAG_COMMIT_NOTIFY_INPUT *)&inputBuffer[0];

        ptrCommitTagNotifyInput->ulNumDisks = ulNumDiskForDriverOutput;
        if (m_blockDrain)
        {
            ptrCommitTagNotifyInput->ulFlags |= TAG_COMMIT_NOTIFY_BLOCK_DRAIN_FLAG;
        }
#ifdef SV_WINDOWS
        StringCchPrintfA((char*)ptrCommitTagNotifyInput->TagGUID,
            ARRAYSIZE(ptrCommitTagNotifyInput->TagGUID),
            bookmarkInput.BookmarkId.c_str());
#else
        inm_memcpy_s(ptrCommitTagNotifyInput->TagGUID, sizeof(ptrCommitTagNotifyInput->TagGUID),
            bookmarkInput.BookmarkId.c_str(), GUID_LEN);
#endif

        uint32_t deviceIdIndex = 0;
        FormattedDeviceIdDiskId_t deviceNameForDriverToProtectedDiskIdMap;

        std::vector<std::string>::const_iterator devIter = protectedDiskIds.begin();
        for (/*empty*/; devIter != protectedDiskIds.end(); devIter++, deviceIdIndex++)
        {
            FilterDrvVol_t fmtddevicename = drvif.GetFormattedDeviceNameForDriverInput(
                devIter->c_str(), VolumeSummary::DISK);
#ifdef SV_WINDOWS
            StringCchPrintfW(ptrCommitTagNotifyInput->DeviceId[deviceIdIndex],
                ARRAYSIZE(ptrCommitTagNotifyInput->DeviceId[deviceIdIndex]),
                fmtddevicename.c_str());
#else
            inm_memcpy_s(ptrCommitTagNotifyInput->DeviceId[deviceIdIndex].volume_guid, NELEMS(
                ptrCommitTagNotifyInput->DeviceId[deviceIdIndex].volume_guid),
                fmtddevicename.c_str(), fmtddevicename.length());
#endif
            deviceNameForDriverToProtectedDiskIdMap[fmtddevicename] = *devIter;
        }

#ifdef SV_WINDOWS
        ptrCommitTagNotifyOutput = (TAG_COMMIT_NOTIFY_OUTPUT *)&outputBuffer[0];
#else
        ptrCommitTagNotifyOutput = (TAG_COMMIT_NOTIFY_OUTPUT *)&inputBuffer[ulInputSize];
#endif
        m_tagCommitNotifyState = TAG_COMMIT_NOTIFY_STATE_WAITING_FOR_COMPLETION;

        bool notifyComplete = false;

#ifdef SV_WINDOWS
        notifyComplete = drvif.NotifyTagCommit(m_pDriverStream.get(), ptrCommitTagNotifyInput,
            ulInputSize, ptrCommitTagNotifyOutput, ulOutputSize);
#else
        notifyComplete = drvif.NotifyTagCommit(m_pDriverStream.get(), ptrCommitTagNotifyInput,
            ulTotalSize);
#endif
        m_tagCommitNotifyState = TAG_COMMIT_NOTIFY_STATE_COMPLETED;

        if (!notifyComplete)
        {
            // Linux: EBUSY (16, 0x10), EINTR (4) and ETIMEDOUT (110, 0x6E) are returned for
            // another commit notify in-progress, cancel and timeout respectively
            // do not interpret the output data
            int errorCode = m_pDriverStream->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s: NotifyTagCommit failed with error 0x%x.\n",
                FUNCTION_NAME, errorCode);
            status = SVE_FAIL;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "%s: NotifyTagCommit completed for tag guid "
                "%s with number of disks %ld flags 0x%lx.\n",
                FUNCTION_NAME,
                ptrCommitTagNotifyOutput->TagGUID,
                ptrCommitTagNotifyOutput->ulNumDisks,
                ptrCommitTagNotifyOutput->ulFlags);
        }

        commitTagBookmarkOutput.BookmarkOutput.BookmarkId = bookmarkInput.BookmarkId;
        commitTagBookmarkOutput.BookmarkOutput.BookmarkType = bookmarkInput.BookmarkType;

        if (ptrCommitTagNotifyOutput->ulNumDisks != protectedDiskIds.size())
        {
            DebugPrintf(SV_LOG_DEBUG,
                "%s: driver returned status for %d disks against %d protected disks in settings.\n",
                FUNCTION_NAME,
                ptrCommitTagNotifyOutput->ulNumDisks,
                protectedDiskIds.size());

            // set failed statues and continue to process the output
            status = SVE_FAIL;
        }

        std::string timeStamp, value, count;

        if (TEST_CXFLAG(ptrCommitTagNotifyOutput->vmCxStatus.ullFlags, VM_CXSTATUS_PEAKCHURN_FLAG))
        {
            timeStamp = boost::lexical_cast<std::string>(
                ptrCommitTagNotifyOutput->vmCxStatus.firstPeakChurnTS);
            value = boost::lexical_cast<std::string>(
                ptrCommitTagNotifyOutput->vmCxStatus.ullMaximumPeakChurnInBytes);
            count = boost::lexical_cast<std::string>(
                ptrCommitTagNotifyOutput->vmCxStatus.ullTotalExcessChurnInBytes);

            std::stringstream msg;
            msg << "VM churn more than supported peak churn observed.";
            msg << " Timestamp : " << timeStamp << "UTC";
            msg << " Churn : " << value << " bytes";
            msg << " Accumulated Churn : " << count << " bytes." << std::endl;

            ssPossibleCauses << msg.str();
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, msg.str().c_str());
        }

        if (TEST_CXFLAG(ptrCommitTagNotifyOutput->vmCxStatus.ullFlags,
            VM_CXSTATUS_CHURNTHROUGHPUT_FLAG))
        {
            timeStamp = boost::lexical_cast<std::string>(
                ptrCommitTagNotifyOutput->vmCxStatus.CxEndTS);
            value = boost::lexical_cast<std::string>(
                ptrCommitTagNotifyOutput->vmCxStatus.ullDiffChurnThroughputInBytes);

            std::stringstream msg;
            msg << "High log upload network latencies observed.";
            msg << " Timestamp : " << timeStamp << "UTC";
            msg << " ChurnThroughputDiff : " << value << " bytes." << std::endl;

            ssPossibleCauses << msg.str();
            DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, msg.str().c_str());
        }

        std::string device;
        uint32_t numProtectedDisksWithCommitedTags = 0;
        for (uint64_t deviceIndex = 0; deviceIndex < ptrCommitTagNotifyOutput->ulNumDisks;
            deviceIndex++)
        {
            TAG_COMMIT_STATUS *ptrDevTagCommitStatus =
                &(ptrCommitTagNotifyOutput->TagStatus[deviceIndex]);
#ifdef SV_WINDOWS
            std::vector<WCHAR> vDeviceId(GUID_SIZE_IN_CHARS + 1, 0);
            inm_memcpy_s(&vDeviceId[0], vDeviceId.size() * sizeof(WCHAR),
                ptrDevTagCommitStatus->DeviceId, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
            FilterDrvVol_t drvDeviceId(&vDeviceId[0]);
#else
            FilterDrvVol_t drvDeviceId(ptrDevTagCommitStatus->DeviceId.volume_guid);
#endif
            FormattedDeviceIdDiskId_t::iterator iterDiskId =
                find_if(deviceNameForDriverToProtectedDiskIdMap.begin(),
                    deviceNameForDriverToProtectedDiskIdMap.end(),
                    EqDiskId(drvDeviceId));

            if (deviceNameForDriverToProtectedDiskIdMap.end() == iterDiskId)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: Failed to get protected DeviceId for disk "
                    "%s device status: 0x%x, tag status: 0x%x.\n",
                    FUNCTION_NAME,
#ifdef SV_WINDOWS
                    convertWstringToUtf8(drvDeviceId).c_str(),
#else
                    drvDeviceId.c_str(),
#endif
                    ptrDevTagCommitStatus->Status,
                    ptrDevTagCommitStatus->TagStatus);
                continue;
            }

            device = iterDiskId->second;

            if (ptrDevTagCommitStatus->TagStatus == TAG_STATUS_COMMITTED)
            {
                numProtectedDisksWithCommitedTags++;
                FinalReplicationBookmarkDetails devBookmarkDetails;
                devBookmarkDetails.BookmarkTime = ptrDevTagCommitStatus->TagInsertionTime;
                devBookmarkDetails.BookmarkSequenceId = ptrDevTagCommitStatus->TagSequenceNumber;
                commitTagBookmarkOutput.BookmarkOutput.SourceDiskIdToBookmarkDetailsMap[device] =
                    devBookmarkDetails;

                DebugPrintf(SV_LOG_DEBUG,
                    "%s: tag committed for %s, tag insertion time %lld, sequence number %lld\n",
                    FUNCTION_NAME,
                    device.c_str(),
                    devBookmarkDetails.BookmarkTime,
                    devBookmarkDetails.BookmarkSequenceId);
            }
            else
            {
                status = SVE_FAIL;

                DebugPrintf(SV_LOG_ERROR,
                    "%s: tag not committed for disk %s, device status: 0x%x, "
                    "tag status: 0x%x, devicecxstats.flags 0x%x.\n",
                    FUNCTION_NAME,
                    device.c_str(),
                    ptrDevTagCommitStatus->Status,
                    ptrDevTagCommitStatus->TagStatus,
                    ptrDevTagCommitStatus->DeviceCxStats.ullFlags);

                if (TEST_CXFLAG(ptrDevTagCommitStatus->DeviceCxStats.ullFlags,
                    DISK_CXSTATUS_NWFAILURE_FLAG))
                {
                    timeStamp = boost::lexical_cast<std::string>(
                        ptrDevTagCommitStatus->DeviceCxStats.lastNwFailureTS);
                    value = boost::lexical_cast<std::string>(
                        ptrDevTagCommitStatus->DeviceCxStats.ullLastNWErrorCode);
                    count = boost::lexical_cast<std::string>(
                        ptrDevTagCommitStatus->DeviceCxStats.ullTotalNWErrors);

                    std::stringstream msg;
                    msg << "Log upload failed with network error.";
                    msg << " Timestamp : " << timeStamp << "UTC";
                    msg << " Device : " << device;
                    msg << " Last Error : " << value;
                    msg << " Error Count : " << count << "." << std::endl;

                    ssPossibleCauses << msg.str();
                    DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, msg.str().c_str());
                }

                if (TEST_CXFLAG(ptrDevTagCommitStatus->DeviceCxStats.ullFlags,
                    DISK_CXSTATUS_PEAKCHURN_FLAG))
                {
                    timeStamp = boost::lexical_cast<std::string>(
                        ptrDevTagCommitStatus->DeviceCxStats.firstPeakChurnTS);
                    value = boost::lexical_cast<std::string>(
                        ptrDevTagCommitStatus->DeviceCxStats.ullMaximumPeakChurnInBytes);
                    count = boost::lexical_cast<std::string>(
                        ptrDevTagCommitStatus->DeviceCxStats.ullTotalExcessChurnInBytes);

                    std::stringstream msg;
                    msg << "Disk churn more than supported peak churn observed.";
                    msg << " Timestamp : " << timeStamp << "UTC";
                    msg << " Device : " << device;
                    msg << " Churn : " << value << " bytes";
                    msg << " Accumulated Churn : " << count << " bytes." << std::endl;

                    ssPossibleCauses << msg.str();
                    DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, msg.str().c_str());
                }

                if (TEST_CXFLAG(ptrDevTagCommitStatus->DeviceCxStats.ullFlags,
                    DISK_CXSTATUS_CHURNTHROUGHPUT_FLAG))
                {
                    timeStamp = boost::lexical_cast<std::string>(
                        ptrDevTagCommitStatus->DeviceCxStats.CxEndTS);
                    value = boost::lexical_cast<std::string>(
                        ptrDevTagCommitStatus->DeviceCxStats.ullDiffChurnThroughputInBytes);

                    std::stringstream msg;
                    msg << "High log upload network latencies observed.";
                    msg << " Timestamp : " << timeStamp << "UTC";
                    msg << " Device : " << device;
                    msg << " ChurnThroughputDiff : " << value << " bytes." << std::endl;

                    ssPossibleCauses << msg.str();
                    DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, msg.str().c_str());
                }

                if (TEST_CXFLAG(ptrDevTagCommitStatus->DeviceCxStats.ullFlags,
                    DISK_CXSTATUS_DISK_REMOVED))
                {
                    ssPossibleCauses << "Device : " << device << " is removed." << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "%s: disk %s is removed\n",
                        FUNCTION_NAME, device.c_str());
                }

                if (TEST_CXFLAG(ptrDevTagCommitStatus->DeviceCxStats.ullFlags,
                    DISK_CXSTATUS_DISK_NOT_FILTERED))
                {
                    ssPossibleCauses << "Device : " << device << " is not tracked." << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "%s: disk %s is not filtered\n",
                        FUNCTION_NAME, device.c_str());
                }
            }
        }

        if (numProtectedDisksWithCommitedTags != protectedDiskIds.size())
        {
            status = SVE_FAIL;
            DebugPrintf(SV_LOG_ERROR,
                "%s: driver commited tags for %d disks against %d protected disks in settings.\n",
                FUNCTION_NAME,
                numProtectedDisksWithCommitedTags,
                protectedDiskIds.size());
        }

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS TagCommitNotifier::UnblockDrain(const std::vector<std::string> &protectedDiskIds,
    std::stringstream &strErrMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    FilterDrvIfMajor drvif;

    do {
        if (protectedDiskIds.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: no protected disks in settings.\n", FUNCTION_NAME);
            status = SVE_FAIL;
            break;
        }

        size_t diskIdLength = 0;
        uint32_t ulInputSize = 0, ulOutputSize = 0, ulTotalSize = 0;
        uint32_t ulNumDiskForDriverOutput = protectedDiskIds.size();

#ifdef SV_WINDOWS
        // Input and Output structure will be aligned at data bus width
        // so input length and output length has to be aligned at data bus width
        ulInputSize = ALIGNED_SIZE((FIELD_OFFSET(SET_DRAIN_STATE_INPUT,
            DeviceId[ulNumDiskForDriverOutput])), (sizeof(ULONG)));
        ulOutputSize = ALIGNED_SIZE((FIELD_OFFSET(SET_DRAIN_STATE_OUTPUT,
            diskStatus[ulNumDiskForDriverOutput])), (sizeof(ULONG)));
        ulTotalSize = ulInputSize;

        std::vector<char> outputBuffer(ulOutputSize, 0);
#else
        diskIdLength = sizeof(VOLUME_GUID);
        INM_SAFE_ARITHMETIC(ulInputSize = InmSafeInt<size_t>::Type(
            sizeof(SET_DRAIN_STATE_INPUT)) + diskIdLength * (ulNumDiskForDriverOutput - 1),
            INMAGE_EX(sizeof(SET_DRAIN_STATE_INPUT)) (diskIdLength)(ulNumDiskForDriverOutput - 1))
        INM_SAFE_ARITHMETIC(ulOutputSize = InmSafeInt<size_t>::Type(
            sizeof(SET_DRAIN_STATE_OUTPUT)) +
            sizeof(SET_DRAIN_STATUS) * (ulNumDiskForDriverOutput - 1),
            INMAGE_EX(sizeof(SET_DRAIN_STATE_OUTPUT))
            (sizeof(SET_DRAIN_STATUS)) (ulNumDiskForDriverOutput - 1))
        INM_SAFE_ARITHMETIC(ulTotalSize = InmSafeInt<uint32_t>::Type(ulInputSize) +
            ulOutputSize, INMAGE_EX(ulInputSize) (ulOutputSize))
#endif

        std::vector<char> inputBuffer(ulTotalSize, 0);

        SET_DRAIN_STATE_INPUT *ptrSetDrainStateInput = NULL;
        SET_DRAIN_STATE_OUTPUT *ptrSetDrainStateOutput = NULL;

        // set input buffer
        ptrSetDrainStateInput = (SET_DRAIN_STATE_INPUT *)&inputBuffer[0];
        ptrSetDrainStateInput->ulNumDisks = ulNumDiskForDriverOutput;

        uint32_t deviceIdIndex = 0;
        FormattedDeviceIdDiskId_t deviceNameForDriverToProtectedDiskIdMap;

        std::vector<std::string>::const_iterator devIter = protectedDiskIds.begin();
        for (/*empty*/; devIter != protectedDiskIds.end(); devIter++, deviceIdIndex++)
        {
            FilterDrvVol_t fmtddevicename = drvif.GetFormattedDeviceNameForDriverInput(
                devIter->c_str(), VolumeSummary::DISK);
#ifdef SV_WINDOWS
            StringCchPrintfW(ptrSetDrainStateInput->DeviceId[deviceIdIndex],
                ARRAYSIZE(ptrSetDrainStateInput->DeviceId[deviceIdIndex]),
                fmtddevicename.c_str());
#else
            inm_memcpy_s(ptrSetDrainStateInput->DeviceId[deviceIdIndex].volume_guid, NELEMS(
                ptrSetDrainStateInput->DeviceId[deviceIdIndex].volume_guid),
                fmtddevicename.c_str(), fmtddevicename.length());
#endif
            deviceNameForDriverToProtectedDiskIdMap[fmtddevicename] = *devIter;
        }

        bool bUnblockDrainSuccess = false;
#ifdef SV_WINDOWS
        ptrSetDrainStateOutput = (SET_DRAIN_STATE_OUTPUT *)&outputBuffer[0];
        bUnblockDrainSuccess = drvif.UnblockDrain(m_pDriverStream.get(), ptrSetDrainStateInput,
            ulInputSize, ptrSetDrainStateOutput, ulOutputSize);
#else
        ptrSetDrainStateOutput = (SET_DRAIN_STATE_OUTPUT *)&inputBuffer[ulInputSize];
        bUnblockDrainSuccess = drvif.UnblockDrain(m_pDriverStream.get(), ptrSetDrainStateInput,
            ulTotalSize);
#endif

        if (!bUnblockDrainSuccess)
        {
            // Linux: EBUSY (16, 0x10), EINTR (4) and ETIMEDOUT (110, 0x6E) are returned for
            // another commit notify in-progress, cancel and timeout respectively
            // do not interpret the output data
            int errorCode = m_pDriverStream->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s: Unblock Drain failed with error 0x%x.\n",
                FUNCTION_NAME, errorCode);
            status = SVE_FAIL;
            break;
        }

        if (ptrSetDrainStateOutput->ulNumDisks != protectedDiskIds.size())
        {
            DebugPrintf(SV_LOG_DEBUG,
                "%s: driver returned status for %d disks against %d protected disks in settings.\n",
                FUNCTION_NAME,
                ptrSetDrainStateOutput->ulNumDisks,
                protectedDiskIds.size());

            // set failed statues and continue to process the output
            status = SVE_FAIL;
        }

        if (bUnblockDrainSuccess)
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "%s: Unblock drain completed successfully.\n",
                FUNCTION_NAME);
            break;
        } 

        std::string device;
        uint32_t numProtectedDisksWithDrainUnblocked = 0;
        for (uint64_t deviceIndex = 0; deviceIndex < ptrSetDrainStateOutput->ulNumDisks;
            deviceIndex++)
        {
            SET_DRAIN_STATUS *ptrSetDrainStatus =
                &(ptrSetDrainStateOutput->diskStatus[deviceIndex]);
#ifdef SV_WINDOWS
            std::vector<WCHAR> vDeviceId(GUID_SIZE_IN_CHARS + 1, 0);
            inm_memcpy_s(&vDeviceId[0], vDeviceId.size() * sizeof(WCHAR),
                ptrSetDrainStatus->DeviceId, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
            FilterDrvVol_t drvDeviceId(&vDeviceId[0]);
#else
            FilterDrvVol_t drvDeviceId(ptrSetDrainStatus->DeviceId.volume_guid);
#endif
            FormattedDeviceIdDiskId_t::iterator iterDiskId =
                find_if(deviceNameForDriverToProtectedDiskIdMap.begin(),
                    deviceNameForDriverToProtectedDiskIdMap.end(),
                    EqDiskId(drvDeviceId));

            if (deviceNameForDriverToProtectedDiskIdMap.end() == iterDiskId)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: Failed to get protected DeviceId for disk "
                    "%s disk status: %d, internal error: %llu.\n",
                    FUNCTION_NAME,
#ifdef SV_WINDOWS
                    convertWstringToUtf8(drvDeviceId).c_str(),
#else
                    drvDeviceId.c_str(),
#endif
                    ptrSetDrainStatus->Status,
                    ptrSetDrainStatus->ulInternalError);
                continue;
            }

            device = iterDiskId->second;

            if (ptrSetDrainStatus->Status == SET_DRAIN_STATUS_SUCCESS)
            {
                numProtectedDisksWithDrainUnblocked++;
            }
            else
            {
                status = SVE_FAIL;

                DebugPrintf(SV_LOG_ERROR,
                    "%s: UnblockDrain failed for disk %s, disk status: %d, internal error: %llu.\n",
                    FUNCTION_NAME,
                    device.c_str(),
                    ptrSetDrainStatus->Status,
                    ptrSetDrainStatus->ulInternalError);

                if (ptrSetDrainStatus->Status == SET_DRAIN_STATUS_DEVICE_NOT_FOUND)
                {
                   strErrMsg << "Device : " << device << " is not tracked." << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "%s: disk %s is not filtered\n",
                        FUNCTION_NAME, device.c_str());
                }
            }
        }

        if (numProtectedDisksWithDrainUnblocked != protectedDiskIds.size())
        {
            status = SVE_FAIL;
            DebugPrintf(SV_LOG_ERROR,
                "%s: Drain unblocked for %d disks against %d protected disks in settings.\n",
                FUNCTION_NAME,
                numProtectedDisksWithDrainUnblocked,
                protectedDiskIds.size());
        }

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS TagCommitNotifier::VerifyDrainStatus(const std::vector<std::string> &protectedDiskIds,
    uint64_t flag)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    FilterDrvIfMajor drvif;

    do {
        if (protectedDiskIds.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: no protected disks in settings.\n", FUNCTION_NAME);
            status = SVE_FAIL;
            break;
        }

        if (!IsCommitTagNotifySupported())
        {
            status = SVE_NOTIMPL;
            break;
        }

        size_t diskIdLength = 0;
        uint32_t ulInputSize = 0, ulOutputSize = 0, ulTotalSize = 0;
        uint32_t ulNumDiskForDriverOutput = protectedDiskIds.size();

#ifdef SV_WINDOWS
        // Input and Output structure will be aligned at data bus width
        // so input length and output length has to be aligned at data bus width
        ulInputSize = ALIGNED_SIZE((FIELD_OFFSET(GET_DISK_STATE_INPUT,
            DeviceId[ulNumDiskForDriverOutput])), (sizeof(ULONG)));
        ulOutputSize = ALIGNED_SIZE((FIELD_OFFSET(GET_DISK_STATE_OUTPUT,
            diskState[ulNumDiskForDriverOutput])), (sizeof(ULONG)));
        ulTotalSize = ulInputSize;

        std::vector<char> outputBuffer(ulOutputSize, 0);
#else
        diskIdLength = sizeof(VOLUME_GUID);
        INM_SAFE_ARITHMETIC(ulInputSize = InmSafeInt<size_t>::Type(
            sizeof(GET_DISK_STATE_INPUT)) + diskIdLength * (ulNumDiskForDriverOutput - 1),
            INMAGE_EX(sizeof(GET_DISK_STATE_INPUT)) (diskIdLength)(ulNumDiskForDriverOutput - 1))
        INM_SAFE_ARITHMETIC(ulOutputSize = InmSafeInt<size_t>::Type(
            sizeof(GET_DISK_STATE_OUTPUT)) +
            sizeof(DISK_STATE) * (ulNumDiskForDriverOutput - 1),
            INMAGE_EX(sizeof(GET_DISK_STATE_OUTPUT))
            (sizeof(DISK_STATE)) (ulNumDiskForDriverOutput - 1))
        INM_SAFE_ARITHMETIC(ulTotalSize = InmSafeInt<uint32_t>::Type(ulInputSize) +
            ulOutputSize, INMAGE_EX(ulInputSize) (ulOutputSize))
#endif

        std::vector<char> inputBuffer(ulTotalSize, 0);

        GET_DISK_STATE_INPUT *ptrGetDrainStateInput = NULL;
        GET_DISK_STATE_OUTPUT *ptrGetDrainStateOutput = NULL;

        // set input buffer
        ptrGetDrainStateInput = (GET_DISK_STATE_INPUT *)&inputBuffer[0];
        ptrGetDrainStateInput->ulNumDisks = ulNumDiskForDriverOutput;

        uint32_t deviceIdIndex = 0;
        FormattedDeviceIdDiskId_t deviceNameForDriverToProtectedDiskIdMap;

        std::vector<std::string>::const_iterator devIter = protectedDiskIds.begin();
        for (/*empty*/; devIter != protectedDiskIds.end(); devIter++, deviceIdIndex++)
        {
            FilterDrvVol_t fmtddevicename = drvif.GetFormattedDeviceNameForDriverInput(
                devIter->c_str(), VolumeSummary::DISK);
#ifdef SV_WINDOWS
            StringCchPrintfW(ptrGetDrainStateInput->DeviceId[deviceIdIndex],
                ARRAYSIZE(ptrGetDrainStateInput->DeviceId[deviceIdIndex]),
                fmtddevicename.c_str());
#else
            inm_memcpy_s(ptrGetDrainStateInput->DeviceId[deviceIdIndex].volume_guid, NELEMS(
                ptrGetDrainStateInput->DeviceId[deviceIdIndex].volume_guid),
                fmtddevicename.c_str(), fmtddevicename.length());
#endif
            deviceNameForDriverToProtectedDiskIdMap[fmtddevicename] = *devIter;
        }

        bool getDrainStatus = false;
#ifdef SV_WINDOWS
        ptrGetDrainStateOutput = (GET_DISK_STATE_OUTPUT *)&outputBuffer[0];
        getDrainStatus = drvif.GetDrainState(m_pDriverStream.get(), ptrGetDrainStateInput,
            ulInputSize, ptrGetDrainStateOutput, ulOutputSize);
#else
        ptrGetDrainStateOutput = (GET_DISK_STATE_OUTPUT *)&inputBuffer[ulInputSize];
        getDrainStatus = drvif.GetDrainState(m_pDriverStream.get(), ptrGetDrainStateInput,
            ulTotalSize);
#endif


        if (!getDrainStatus)
        {
            // Linux: EBUSY (16, 0x10), EINTR (4) and ETIMEDOUT (110, 0x6E) are returned for
            // another commit notify in-progress, cancel and timeout respectively
            // do not interpret the output data
            int errorCode = m_pDriverStream->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s: Get Drain status failed with error 0x%x.\n",
                FUNCTION_NAME, errorCode);
            status = SVE_FAIL;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "%s: Get drain status completed, number of disks %ld.\n",
                FUNCTION_NAME,
                ptrGetDrainStateOutput->ulNumDisks);
        }

        if (ptrGetDrainStateOutput->ulNumDisks != protectedDiskIds.size())
        {
            DebugPrintf(SV_LOG_DEBUG,
                "%s: driver returned status for %d disks against %d protected disks in settings.\n",
                FUNCTION_NAME,
                ptrGetDrainStateOutput->ulNumDisks,
                protectedDiskIds.size());

            // set failed statues and continue to process the output
            status = SVE_FAIL;
        }

        std::string device;
        uint32_t numDisksVerified = 0;
        for (uint64_t deviceIndex = 0; deviceIndex < ptrGetDrainStateOutput->ulNumDisks;
            deviceIndex++)
        {
            DISK_STATE *ptrGetDrainStatus =
                &(ptrGetDrainStateOutput->diskState[deviceIndex]);
#ifdef SV_WINDOWS
            std::vector<WCHAR> vDeviceId(GUID_SIZE_IN_CHARS + 1, 0);
            inm_memcpy_s(&vDeviceId[0], vDeviceId.size() * sizeof(WCHAR),
                ptrGetDrainStatus->DeviceId, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
            FilterDrvVol_t drvDeviceId(&vDeviceId[0]);
#else
            FilterDrvVol_t drvDeviceId(ptrGetDrainStatus->DeviceId.volume_guid);
#endif
            FormattedDeviceIdDiskId_t::iterator iterDiskId =
                find_if(deviceNameForDriverToProtectedDiskIdMap.begin(),
                    deviceNameForDriverToProtectedDiskIdMap.end(),
                    EqDiskId(drvDeviceId));

            if (deviceNameForDriverToProtectedDiskIdMap.end() == iterDiskId)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: Failed to get protected DeviceId for disk "
                    "%s disk status: %llu, flags: %llu.\n",
                    FUNCTION_NAME,
#ifdef SV_WINDOWS
                    convertWstringToUtf8(drvDeviceId).c_str(),
#else
                    drvDeviceId.c_str(),
#endif
                    ptrGetDrainStatus->Status,
                    ptrGetDrainStatus->ulFlags);
                continue;
            }

            device = iterDiskId->second;

            if (ptrGetDrainStatus->ulFlags & flag)
            {
                numDisksVerified++;
            }
            else
            {
                status = SVE_FAIL;
                DebugPrintf(SV_LOG_ERROR,
                    "%s: Draining status for disk %s : %llu, flags: %llu.\n",
                    FUNCTION_NAME,
                    device.c_str(),
                    ptrGetDrainStatus->Status,
                    ptrGetDrainStatus->ulFlags);
            }
        }

        if (numDisksVerified != protectedDiskIds.size())
        {
            status = SVE_FAIL;
            DebugPrintf(SV_LOG_ERROR,
                "%s: Drain verified for %d disks against %d protected disks in settings.\n",
                FUNCTION_NAME,
                numDisksVerified,
                protectedDiskIds.size());
        }

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS TagCommitNotifier::VerifyDrainBlock(const std::vector<std::string> &protectedDiskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    status = VerifyDrainStatus(protectedDiskIds, DISK_STATE_DRAIN_BLOCKED);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS TagCommitNotifier::VerifyDrainUnblock(const std::vector<std::string> &protectedDiskIds)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    status = VerifyDrainStatus(protectedDiskIds, DISK_STATE_FILTERED);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

#ifdef SV_UNIX
SVSTATUS TagCommitNotifier::GetPNameFromDriver(const std::string &devName,
    std::string &driverPName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    FilterDrvIfMajor drvif;

    do
    {
        uint32_t ulInputSize = 0;

        INM_SAFE_ARITHMETIC(ulInputSize = InmSafeInt<size_t>::Type(
            sizeof(vol_name_map_t)),
            INMAGE_EX(sizeof(vol_name_map_t)))

        std::vector<char> inputBuffer(ulInputSize, 0);
        vol_name_map_t *volNameMap = NULL;

        // set input buffer
        volNameMap = (vol_name_map_t *)&inputBuffer[0];
        volNameMap->vnm_flags = INM_VOL_NAME_MAP_GUID;
        inm_memcpy_s(volNameMap->vnm_request,
            NELEMS(volNameMap->vnm_request),
            devName.c_str(), devName.length());

        bool getVolNameMap = drvif.GetVolNameMap(m_pDriverStream.get(), volNameMap, ulInputSize);

        if (!getVolNameMap)
        {
            int errorCode = m_pDriverStream->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s: Get volume name map failed with error 0x%x.\n",
                FUNCTION_NAME, errorCode);
            status = SVE_FAIL;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Get volume name map succeeded.\n",
                FUNCTION_NAME);
            driverPName = volNameMap->vnm_response;
        }
    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS TagCommitNotifier::ModifyPersistentName(const std::string &devName,
    const std::string &curPName, const std::string &finalPName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;
    FilterDrvIfMajor drvif;

    do
    {
        uint32_t ulInputSize = 0;

        INM_SAFE_ARITHMETIC(ulInputSize = InmSafeInt<size_t>::Type(
            sizeof(MODIFY_PERSISTENT_DEVICE_NAME_INPUT)),
            INMAGE_EX(sizeof(MODIFY_PERSISTENT_DEVICE_NAME_INPUT)))

        std::vector<char> inputBuffer(ulInputSize, 0);
        MODIFY_PERSISTENT_DEVICE_NAME_INPUT *modifyPNameInput = NULL;

        // set input buffer
        modifyPNameInput = (MODIFY_PERSISTENT_DEVICE_NAME_INPUT *)&inputBuffer[0];
        inm_memcpy_s(modifyPNameInput->DevName.volume_guid,
            NELEMS(modifyPNameInput->DevName.volume_guid),
            devName.c_str(), devName.length());
        inm_memcpy_s(modifyPNameInput->OldPName.volume_guid,
            NELEMS(modifyPNameInput->OldPName.volume_guid),
            curPName.c_str(), curPName.length());
        inm_memcpy_s(modifyPNameInput->NewPName.volume_guid,
            NELEMS(modifyPNameInput->NewPName.volume_guid),
            finalPName.c_str(), finalPName.length());

        bool modifyPNameComplete = drvif.ModifyPersistentDeviceName(
            m_pDriverStream.get(), modifyPNameInput, ulInputSize);


        if (!modifyPNameComplete)
        {
            int errorCode = m_pDriverStream->GetErrorCode();
            DebugPrintf(SV_LOG_ERROR, "%s: Modify persistent name failed with error 0x%x.\n",
                FUNCTION_NAME, errorCode);
            status = SVE_FAIL;
            break;
        }
        else
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: Modify persistent name succeeded.\n",
                FUNCTION_NAME);
        }
    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

SVSTATUS TagCommitNotifier::ModifyPName(const std::string &devName,
    const std::string &pName, std::stringstream &strErrMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVSTATUS status = SVS_OK;

    do {
        if (devName.empty()) {
            strErrMsg << "Invalid device name : " << devName;
            status = SVE_FAIL;
            break;
        }

        if (pName.empty()) {
            strErrMsg << "Invalid persistent name : " << pName;
            status = SVE_FAIL;
            break;
        }

        std::string driverPName;
        status = GetPNameFromDriver(devName, driverPName);
        if (status != SVS_OK)
        {
            strErrMsg << "Failed to get persistent name from driver\n";
            break;
        }

        if (boost::iequals(pName, driverPName))
        {
            DebugPrintf(SV_LOG_ALWAYS,
                "%s : Required PName is same as driver PName : %s."
                "Skipping ioctl to modify pname\n",
                FUNCTION_NAME, driverPName.c_str());
            break;
        }

        status = ModifyPersistentName(devName, driverPName, pName);
        if (status != SVS_OK)
        {
            strErrMsg << "Modify persistent device name failed\n";
            break;
        }

    } while (false);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
#endif
