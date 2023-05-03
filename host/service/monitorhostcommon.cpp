#include <vector>
#include <sstream>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/range/algorithm/remove_if.hpp>
#include <boost/chrono.hpp>

#include <monitorhostthread.h>
#include "logger.h"
#include "portablehelpers.h"
#include "inmquitfunction.h"
#include "inmageproduct.h"
#include "securityutils.h"
#include "error.h"
#include "filterdrvifmajor.h"
#include "involfltfeatures.h"
#include "host.h"
#include "defaultdirs.h"

#include <functional>

#ifdef SV_WINDOWS
#include "portablehelpersmajor.h"
#include "indskfltfeatures.h"
#endif

#include "consistencythread.h"
#include "listfile.h"
#include "HealthCollator.h"

using namespace boost::chrono;

#define MILLISEC_IN_SECOND 1000

#define CONTINUE_IF_HEALTH_EVENT_ISSUED(bEventIssued, waitTime) \
            {   \
                if (!bEventIssued)    \
                    QuitRequested(waitTime);    \
                continue; \
            }

/// \brief Removes all health issues from VM/Disk level health issues with given health codes
template<typename T>
void RemoveHealthIssues(std::list<T>& issues, std::set<std::string>& healthCodes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    for (std::set<std::string>::const_iterator it = healthCodes.begin(); healthCodes.end() != it; it++) {
        issues.remove_if(std::bind2nd(std::mem_fun_ref(&HealthIssue::is_equal), *it));
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void MonitorHostThread::InsertHealthIssues(const std::list<HealthIssue>& healthissues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::set<std::string> issueCodes;
    for (std::list<HealthIssue>::const_iterator it = healthissues.begin(); healthissues.end() != it; it++) {
        issueCodes.insert(it->IssueCode);
    }
    RemoveHealthIssues(m_vmHealthIssues, issueCodes);
    m_vmHealthIssues.insert(m_vmHealthIssues.end(), healthissues.begin(), healthissues.end());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void MonitorHostThread::RemoveCxHealthIssues(bool isDiskLevel)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (isDiskLevel) {
        ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_diskHealthMutex);
        std::string issueCodes[] = { AgentHealthIssueCodes::DiskLevelHealthIssues::PeakChurn::HealthCode ,
                                    AgentHealthIssueCodes::DiskLevelHealthIssues::HighLatency::HealthCode };

        std::set<std::string>   issueCodesSet(issueCodes, issueCodes + INM_ARRAY_SIZE(issueCodes));
        RemoveHealthIssues(m_diskHealthIssues, issueCodesSet);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void MonitorHostThread::RemoveConsistencyTagHealthIssues(TagTelemetry::TagType& tagtype)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_agentHealthMutex);
    std::set<std::string>   issueCodesSet;
    if (TagTelemetry::CRASH_CONSISTENCY == tagtype) {
        issueCodesSet.insert(AgentHealthIssueCodes::VMLevelHealthIssues::CrashConsistencyTagFailure::HealthCode);
    }
    else if(TagTelemetry::APP_CONSISTENCY == tagtype){
        issueCodesSet.insert(AgentHealthIssueCodes::VMLevelHealthIssues::AppConsistencyTagFailure::HealthCode);
    }
    RemoveHealthIssues(m_vmHealthIssues, issueCodesSet);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void MonitorHostThread::RemoveVacpHealthIssues()
{
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_agentHealthMutex);
#if defined(WIN32)
        std::string issueCodes[] = { AgentHealthIssueCodes::VMLevelHealthIssues::VssProviderMissing::HealthCode ,
                                    AgentHealthIssueCodes::VMLevelHealthIssues::VssWriterFailure::HealthCode};
#else
        std::string issueCodes[] = {};//Need to define the Linux Issue Codes here.
#endif

        std::set<std::string>   issueCodesSet(issueCodes, issueCodes + INM_ARRAY_SIZE(issueCodes));
        RemoveHealthIssues(m_vmHealthIssues, issueCodesSet);
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

SVSTATUS MonitorHostThread::CreateDriverStream()
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
            std::string errMsg("Memory allocation failed for creating driver stream object with exception: ");
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
            std::string errMsg("Failed to open driver");
            DebugPrintf(SV_LOG_ERROR, "%s : error %s %d.\n",
                FUNCTION_NAME, errMsg.c_str(), m_pDriverStream->GetErrorCode());
            status = SVE_FILE_OPEN_FAILED;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return status;
}

bool MonitorHostThread::VerifyDiskCxEvents(VM_CXFAILURE_STATS *ptrCxStats, bool& bHealthEventRequired)
{
    assert(ptrCxStats);
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    RemoveCxHealthIssues(true);

    bHealthEventRequired = false;

    SV_ULONGLONG ullMaxS2LatencyBetweenCommitDBAndGetDB = 0;
    SV_ULONGLONG ullCxClearHealthCount = 0;

    try {
        LocalConfigurator lc;
        ullMaxS2LatencyBetweenCommitDBAndGetDB = lc.getMaxS2LatencyBetweenCommitDBAndGetDB();
        ullCxClearHealthCount = lc.getCxClearHealthCount();
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get the cx config params with exception %s\n",
            FUNCTION_NAME,
            e.what());
        return 0;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get the cx config params with unknown exception.\n",
            FUNCTION_NAME);
        return 0;
    }

    // post the events to control plane
    std::string             timeStamp, device, value, count;

    if (TEST_FLAG(ptrCxStats->ullFlags, VM_CXSTATUS_TIMEJUMP_FWD_FLAG))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Time jump forward of %llu msec observed at %llu.\n",
            FUNCTION_NAME, ptrCxStats->ullTimeJumpInMS, ptrCxStats->TimeJumpTS);

        uint64_t jumpInSecs = ptrCxStats->ullTimeJumpInMS / MILLISEC_IN_SECOND;
        timeStamp = boost::lexical_cast<std::string>(ptrCxStats->TimeJumpTS);
        value = boost::lexical_cast<std::string>(jumpInSecs);

        bHealthEventRequired = true;
        TimeJumpForwardAlert a(timeStamp, value);
        return SendHealthAlert(a);
    }

    if (TEST_FLAG(ptrCxStats->ullFlags, VM_CXSTATUS_TIMEJUMP_BCKWD_FLAG))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Time jump backward of %llu msec observed at %llu.\n",
            FUNCTION_NAME, ptrCxStats->ullTimeJumpInMS, ptrCxStats->TimeJumpTS);

        uint64_t jumpInSecs = ptrCxStats->ullTimeJumpInMS / MILLISEC_IN_SECOND;
        timeStamp = boost::lexical_cast<std::string>(ptrCxStats->TimeJumpTS);
        value = boost::lexical_cast<std::string>(jumpInSecs);

        bHealthEventRequired = true;
        TimeJumpBackwardAlert a(timeStamp, value);
        return SendHealthAlert(a);
    }

    // prefer the disk level events first
    for (uint64_t deviceIndex = 0; deviceIndex < ptrCxStats->ullNumDisks; deviceIndex++)
    {
        DEVICE_CXFAILURE_STATS *ptrDevCxStats = &(ptrCxStats->DeviceCxStats[deviceIndex]);
#ifdef SV_WINDOWS
        std::vector<WCHAR>      vDeviceId(GUID_SIZE_IN_CHARS + 1, 0);
        PWCHAR                  DeviceId = &vDeviceId[0];

        inm_memcpy_s(DeviceId, vDeviceId.size() * sizeof(WCHAR), ptrDevCxStats->DeviceId, GUID_SIZE_IN_CHARS * sizeof(WCHAR));
        do
        {
            if (m_drvDevIdtoProtectedIdMap.end() != m_drvDevIdtoProtectedIdMap.find(DeviceId))
            {
                device = m_drvDevIdtoProtectedIdMap[DeviceId];
                break;
            }
            boost::to_upper(DeviceId);
            if (m_drvDevIdtoProtectedIdMap.end() != m_drvDevIdtoProtectedIdMap.find(DeviceId))
            {
                device = m_drvDevIdtoProtectedIdMap[DeviceId];
                break;
            }

        } while (false);

        if (device.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to get protected DeviceId for disk %s CxFlags: 0x%llx.\n", FUNCTION_NAME, convertWstringToUtf8(std::wstring(DeviceId)).c_str(), ptrDevCxStats->ullFlags);
            continue;
        }
#else
        if (m_drvDevIdtoProtectedIdMap.end() == m_drvDevIdtoProtectedIdMap.find(ptrDevCxStats->DeviceId.volume_guid))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to get protected DeviceId for disk %s CxFlags: 0x%llx.\n", FUNCTION_NAME, ptrDevCxStats->DeviceId.volume_guid, ptrDevCxStats->ullFlags);
            continue;
        }

        device = m_drvDevIdtoProtectedIdMap[ptrDevCxStats->DeviceId.volume_guid];
#endif
        if (ptrDevCxStats->ullFlags) {
            DebugPrintf(SV_LOG_ALWAYS, "%s: GetCxStats completed with disk stats 0x%llx for disk %s.\n", 
                                                        FUNCTION_NAME,
                                                        ptrDevCxStats->ullFlags, device.c_str());
        }

        if (ptrDevCxStats->ullMaxS2LatencyInMS > ullMaxS2LatencyBetweenCommitDBAndGetDB)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: for disk %s drain agent max latency is %llu ms.\n", FUNCTION_NAME, device.c_str(), ptrDevCxStats->ullMaxS2LatencyInMS);
        }

        if (TEST_FLAG(ptrDevCxStats->ullFlags, DISK_CXSTATUS_NWFAILURE_FLAG))
        {
            timeStamp = boost::lexical_cast<std::string>(ptrDevCxStats->lastNwFailureTS);
            value = boost::lexical_cast<std::string>(ptrDevCxStats->ullLastNWErrorCode);
            count = boost::lexical_cast<std::string>(ptrDevCxStats->ullTotalNWErrors);

            std::stringstream msg;
            msg << "Log upload failed with network error.";
            msg << " Timestamp : " << timeStamp;
            msg << " Device : " << device;
            msg << " Last Error : " << value;
            msg << " Error Count : " << count << ".";

            DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());

            LogUploadNetworkFailureAlert a(timeStamp, device, value, count, Host::GetInstance().GetHostName());
            bHealthEventRequired = true;
            return SendHealthAlert(a);
        }

        if (TEST_FLAG(ptrDevCxStats->ullFlags, DISK_CXSTATUS_PEAKCHURN_FLAG))
        {
            timeStamp = boost::lexical_cast<std::string>(ptrDevCxStats->firstPeakChurnTS);
            timeStamp += std::string(" UTC");
            value = boost::lexical_cast<std::string>(ptrDevCxStats->ullMaximumPeakChurnInBytes);
            count = boost::lexical_cast<std::string>(ptrDevCxStats->ullTotalExcessChurnInBytes);

            std::stringstream msg;
            msg << "Disk churn more than supported peak churn observed.";
            msg << " Timestamp : " << timeStamp;
            msg << " Device : " << device;
            msg << " Churn : " << value << " bytes";
            msg << " Accumulated Churn : " << count << " bytes.";

            DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());

            {
                ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_diskHealthMutex);

                std::pair<std::string, std::string> healthParams[] = {
                    std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::PeakChurn::ChurnRate,
                                                InmGetFormattedSize(ptrDevCxStats->ullDiffChurnThroughputInBytes)),
                    std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::PeakChurn::UploadPending,
                                                InmGetFormattedSize(ptrDevCxStats->ullTotalExcessChurnInBytes)),
                    std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::PeakChurn::ObservationTime, timeStamp)

                };

                std::map<std::string, std::string>  params(healthParams, healthParams + INM_ARRAY_SIZE(healthParams));

                m_diskHealthIssues.push_back(AgentDiskLevelHealthIssue(device,
                    AgentHealthIssueCodes::DiskLevelHealthIssues::PeakChurn::HealthCode,
                    params));

                m_CxClearHealthIssueCounter = ullCxClearHealthCount;
            }

            AgentPeakChurnAlert a(timeStamp, device, value, count, Host::GetInstance().GetHostName());
            bHealthEventRequired = true;
            return SendHealthAlert(a);
        }

        if (TEST_FLAG(ptrDevCxStats->ullFlags, DISK_CXSTATUS_CHURNTHROUGHPUT_FLAG))
        {
            timeStamp = boost::lexical_cast<std::string>(ptrDevCxStats->CxEndTS);
            timeStamp += std::string(" UTC");
            value = boost::lexical_cast<std::string>(ptrDevCxStats->ullDiffChurnThroughputInBytes);

            std::stringstream msg;
            msg << "High log upload network latencies observed.";
            msg << " Timestamp : " << timeStamp;
            msg << " Device : " << device;
            msg << " ChurnThroughputDiff : " << value << " bytes";
            DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());

            {
                ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_diskHealthMutex);

                std::pair<std::string, std::string> healthParams[] = {
                    std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::HighLatency::UploadPending,
                                                InmGetFormattedSize(ptrDevCxStats->ullTotalExcessChurnInBytes)),
                    std::make_pair(AgentHealthIssueCodes::DiskLevelHealthIssues::HighLatency::ObservationTime, timeStamp)

                };

                std::map<std::string, std::string>  params(healthParams, healthParams + (sizeof(healthParams) / sizeof(healthParams[0])));

                m_diskHealthIssues.push_back(AgentDiskLevelHealthIssue(device, 
                                                AgentHealthIssueCodes::DiskLevelHealthIssues::HighLatency::HealthCode,
                                                params));
                m_CxClearHealthIssueCounter = ullCxClearHealthCount;
            }

            LogUploadNetworkLatencyAlert a(timeStamp, device, value, Host::GetInstance().GetHostName());
            bHealthEventRequired = true;
            return SendHealthAlert(a);
        }

        if (TEST_FLAG(ptrDevCxStats->ullFlags, DISK_CXSTATUS_DISK_REMOVED))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: disk %s is removed\n", FUNCTION_NAME, device.c_str());
        }
        else if (TEST_FLAG(ptrDevCxStats->ullFlags, DISK_CXSTATUS_DISK_NOT_FILTERED))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: disk %s is not filtered\n", FUNCTION_NAME, device.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: additional CX disk status 0x%llx for disk %s \n", FUNCTION_NAME, ptrDevCxStats->ullFlags, device.c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool MonitorHostThread::VerifyVMCxEvents(VM_CXFAILURE_STATS *ptrCxStats, bool& bHealthEventRequired)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // post the events to control plane
    std::string             timeStamp, device, value, count;

    bHealthEventRequired = false;
    // VM level events
    device = "VM";

    uint64_t ullMaxS2LatencyBetweenCommitDBAndGetDB = 0;

    try {
        LocalConfigurator lc;
        ullMaxS2LatencyBetweenCommitDBAndGetDB = lc.getMaxS2LatencyBetweenCommitDBAndGetDB();
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get the cx config params with exception %s\n",
            FUNCTION_NAME,
            e.what());
        return 0;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get the cx config params with unknown exception.\n",
            FUNCTION_NAME);
        return 0;
    }

    if (ptrCxStats->ullMaxS2LatencyInMS > ullMaxS2LatencyBetweenCommitDBAndGetDB)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: drain agent max latency is %llu ms.\n", FUNCTION_NAME, ptrCxStats->ullMaxS2LatencyInMS);
    }

    if (TEST_FLAG(ptrCxStats->ullFlags, VM_CXSTATUS_PEAKCHURN_FLAG))
    {
        timeStamp = boost::lexical_cast<std::string>(ptrCxStats->firstPeakChurnTS);
        timeStamp += std::string(" UTC");
        value = boost::lexical_cast<std::string>(ptrCxStats->ullMaximumPeakChurnInBytes);
        count = boost::lexical_cast<std::string>(ptrCxStats->ullTotalExcessChurnInBytes);

        std::stringstream msg;
        msg << "VM churn more than supported peak churn observed.";
        msg << " Timestamp : " << timeStamp;
        msg << " Churn : " << value << " bytes";
        msg << " Accumulated Churn : " << count << " bytes.";

        DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());

        AgentPeakChurnAlert a(timeStamp, device, value, count, Host::GetInstance().GetHostName());
        bHealthEventRequired = true;
        return SendHealthAlert(a);
    }

    if (TEST_FLAG(ptrCxStats->ullFlags, VM_CXSTATUS_CHURNTHROUGHPUT_FLAG))
    {
        timeStamp = boost::lexical_cast<std::string>(ptrCxStats->CxEndTS);
        timeStamp += std::string(" UTC");
        value = boost::lexical_cast<std::string>(ptrCxStats->ullDiffChurnThroughputInBytes);

        std::stringstream msg;
        msg << "High log upload network latencies observed.";
        msg << " Timestamp : " << timeStamp;
        msg << " ChurnThroughputDiff : " << value << " bytes";

        DebugPrintf(SV_LOG_ERROR, "%s\n", msg.str().c_str());

        bHealthEventRequired = true;
        LogUploadNetworkLatencyAlert a(timeStamp, device, value, Host::GetInstance().GetHostName());
        return SendHealthAlert(a);
    }

    if (ptrCxStats->ullFlags)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Additional CX stats flag %llu.\n", FUNCTION_NAME, ptrCxStats->ullFlags);
    }
    else if (!ptrCxStats->ullNumDisks)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: no disk level CX stats.\n", FUNCTION_NAME);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}

ACE_THR_FUNC_RETURN MonitorHostThread::MonitorCxEvents()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    FilterDrvIfMajor drvif;
    DeviceFilterFeatures::Ptr pDeviceFilterFeatures;

#ifdef SV_WINDOWS
    ULARGE_INTEGER prevTransactionId;
    prevTransactionId.QuadPart = 0;
#else
    uint64_t prevTransactionId = 0;
#endif

    uint32_t ulInputSize = 0, ulOutputSize = 0, ulTotalSize = 0;
    uint32_t ulNumDiskForDriverOutput = 0;

    std::set<std::string> deviceIds;
    GET_CXFAILURE_NOTIFY *ptrCxNotify = NULL;
    VM_CXFAILURE_STATS  *ptrCxStats = NULL;
    bool haveStats = false;
    bool bHealthEventIssued = false, bHealthEventRequired = false;

    uint64_t ullMaxDiskChurnSupportedMBps = 0, ullMaxVMChurnSupportedMBps = 0;
    uint64_t ullMaximumTimeJumpFwdAcceptableInMs = 0, ullMaximumTimeJumpBwdAcceptableInMs = 0;
    uint64_t ullMinConsecutiveTagFailures = 0;
    uint64_t ullMaxWaitTimeForHealthEventCommitFailureInSec = 0;

    try {
        LocalConfigurator lc;
        ullMaxDiskChurnSupportedMBps = lc.getMaxDiskChurnSupportedMBps();
        ullMaxVMChurnSupportedMBps = lc.getMaxVMChurnSupportedMBps();
        ullMaximumTimeJumpBwdAcceptableInMs = lc.getMaximumTimeJumpBackwardAcceptableInMs();
        ullMaximumTimeJumpFwdAcceptableInMs = lc.getMaximumTimeJumpForwardAcceptableInMs();
        ullMinConsecutiveTagFailures = lc.getMinConsecutiveTagFailures();
        ullMaxWaitTimeForHealthEventCommitFailureInSec = lc.getMaxWaitTimeForHealthEventCommitFailureInSec();
    }
    catch (const std::exception &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get the cx config params with exception %s\n",
            FUNCTION_NAME,
            e.what());
        return 0;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to get the cx config params with unknown exception.\n",
            FUNCTION_NAME);
        return 0;
    }

    while (!m_threadManager.testcancel(ACE_Thread::self()))
    {
        try
        {
            if (SVS_OK != CreateDriverStream())
            {
                QuitRequested(300);
                continue;
            }

            if (!pDeviceFilterFeatures)
            {
                DRIVER_VERSION dv;
                drvif.GetDriverVersion(m_pDriverStream.get(), dv);
#ifdef SV_WINDOWS
                pDeviceFilterFeatures.reset(new InDskFltFeatures(dv));
#else
                pDeviceFilterFeatures.reset(new InVolFltFeatures(dv));
#endif
                if (!pDeviceFilterFeatures->IsCxEventsSupported())
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: CX events not supported, quitting.\n", FUNCTION_NAME);
                    break;
                }
                else
                {
                    m_bFilterDriverSupportsCxSession = true;
                }
            }

            {
                boost::unique_lock<boost::mutex> lock(m_mutexMonitorDeviceIds);
                deviceIds = m_monitorDeviceIds;
            }

            if (deviceIds.empty())
            {
                QuitRequested(30);
                continue;
            }

            size_t diskIdLength = 0;
#ifdef SV_WINDOWS
            diskIdLength = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
#else
            diskIdLength = sizeof(VOLUME_GUID);
#endif

            if (ulNumDiskForDriverOutput < deviceIds.size())
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: Updating num of disks for driver output from %lu to %lu.\n",
                    FUNCTION_NAME, ulNumDiskForDriverOutput, deviceIds.size());
                ulNumDiskForDriverOutput = deviceIds.size();
            }

            INM_SAFE_ARITHMETIC(ulInputSize = InmSafeInt<size_t>::Type(sizeof(GET_CXFAILURE_NOTIFY)) + diskIdLength * (ulNumDiskForDriverOutput - 1), INMAGE_EX(sizeof(GET_CXFAILURE_NOTIFY)) (diskIdLength)(ulNumDiskForDriverOutput - 1))

            INM_SAFE_ARITHMETIC(ulOutputSize = InmSafeInt<size_t>::Type(sizeof(VM_CXFAILURE_STATS)) + sizeof(DEVICE_CXFAILURE_STATS) * (ulNumDiskForDriverOutput - 1), INMAGE_EX(sizeof(VM_CXFAILURE_STATS)) (sizeof(DEVICE_CXFAILURE_STATS)) (ulNumDiskForDriverOutput - 1))

            INM_SAFE_ARITHMETIC(ulTotalSize = InmSafeInt<uint32_t>::Type(ulInputSize) + ulOutputSize, INMAGE_EX(ulInputSize) (ulOutputSize))

            std::vector<char> inputBuffer(ulTotalSize, 0);

            // set input buffer
            ptrCxNotify = (GET_CXFAILURE_NOTIFY *)&inputBuffer[0];

            ptrCxNotify->ullFlags = bHealthEventIssued ? CXSTATUS_COMMIT_PREV_SESSION : 0;
            ptrCxNotify->ullMaxDiskChurnSupportedMBps = ullMaxDiskChurnSupportedMBps;
            ptrCxNotify->ullMaxVMChurnSupportedMBps = ullMaxVMChurnSupportedMBps;
            ptrCxNotify->ullMinConsecutiveTagFailures = ullMinConsecutiveTagFailures;

            ptrCxNotify->ullMaximumTimeJumpFwdAcceptableInMs = ullMaximumTimeJumpFwdAcceptableInMs;
            ptrCxNotify->ullMaximumTimeJumpBwdAcceptableInMs = ullMaximumTimeJumpBwdAcceptableInMs;
            ptrCxNotify->ulNumberOfProtectedDisks = deviceIds.size();
            ptrCxNotify->ulNumberOfOutputDisks = ulNumDiskForDriverOutput;
            ptrCxNotify->ulTransactionID = prevTransactionId;

            uint32_t deviceIdIndex = 0;
            std::set<std::string>::iterator devIter = deviceIds.begin();
            for (/*empty*/; devIter != deviceIds.end(); devIter++, deviceIdIndex++)
            {
                FilterDrvVol_t fmtddevicename = drvif.GetFormattedDeviceNameForDriverInput(devIter->c_str(), VolumeSummary::DISK);
#ifdef SV_WINDOWS
                inm_wmemcpy_s(ptrCxNotify->DeviceIdList[deviceIdIndex], NELEMS(ptrCxNotify->DeviceIdList[deviceIdIndex]), fmtddevicename.c_str(), fmtddevicename.length());
#else
                inm_memcpy_s(ptrCxNotify->DeviceIdList[deviceIdIndex].volume_guid, NELEMS(ptrCxNotify->DeviceIdList[deviceIdIndex].volume_guid), fmtddevicename.c_str(), fmtddevicename.length());
#endif
                m_drvDevIdtoProtectedIdMap[fmtddevicename] = *devIter;
            }

            // set output buffer
            ptrCxStats = (VM_CXFAILURE_STATS *)&inputBuffer[ulInputSize];

#ifdef SV_WINDOWS
            haveStats = drvif.GetCxStats(m_pDriverStream.get(), ptrCxNotify, ulInputSize, ptrCxStats, ulOutputSize);
#else
            haveStats = drvif.GetCxStats(m_pDriverStream.get(), ptrCxNotify, ulTotalSize);
#endif
            if (!haveStats)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: GetCxStats completed without stats.\n", FUNCTION_NAME);
#ifdef SV_WINDOWS
                if (m_pDriverStream->GetErrorCode() == ERR_MORE_DATA)
#else
                if (m_pDriverStream->GetErrorCode() == ERR_AGAIN)
#endif
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: driver is filtering more than %lu disks. Number of protected disks %lu.\n",
                        FUNCTION_NAME,
                        ulNumDiskForDriverOutput,
                        deviceIds.size());

                    ulNumDiskForDriverOutput += 4;
                }
                continue;
            }

            DebugPrintf(SV_LOG_ALWAYS, "%s: GetCxStats completed with stats 0x%llx.\n", FUNCTION_NAME, ptrCxStats->ullFlags);

            prevTransactionId = ptrCxStats->ulTransactionID;
            bHealthEventIssued = false;
            bHealthEventRequired = false;

            // post the events to control plane
            std::string             timeStamp, device, value, count;

            bHealthEventIssued = VerifyDiskCxEvents(ptrCxStats, bHealthEventRequired);

            if (bHealthEventRequired)
            {
                CONTINUE_IF_HEALTH_EVENT_ISSUED(bHealthEventIssued, ullMaxWaitTimeForHealthEventCommitFailureInSec);
            }

            // VerifyVM
            bHealthEventIssued = VerifyVMCxEvents(ptrCxStats, bHealthEventRequired);

            if (bHealthEventRequired)
            {
                CONTINUE_IF_HEALTH_EVENT_ISSUED(bHealthEventIssued, ullMaxWaitTimeForHealthEventCommitFailureInSec);
            }

        }
        catch (const std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: monitoring events caught exception %s\n", FUNCTION_NAME, e.what());
            QuitRequested(30);
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught unknown exception\n", FUNCTION_NAME);
            QuitRequested(30);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return 0;
}

std::string MonitorHostThread::GetPersistentDeviceIdForDriverInput(const std::string& deviceId)
{
    std::string persistentDeviceId = deviceId;

#ifdef SV_UNIX
    // for UNIX driver, the persistent names are like _dev_sda for /dev/sda
    boost::replace_all(persistentDeviceId, "/", "_");
#endif

    return persistentDeviceId;
}

std::string MonitorHostThread::GetDeviceIdForPersistentDeviceId(const std::string& persistentDeviceId)
{
    std::string deviceId = persistentDeviceId;

#ifdef SV_UNIX
    // for UNIX driver, for persistent _dev_sda  device name would be /dev/sda
    boost::replace_all(deviceId, "_", "/");
#endif

    return deviceId;
}

void MonitorHostThread::SetLastTagState(TagTelemetry::TagType tagType, bool bSuccess, const std::list<HealthIssue> &vacpAndTagsHealthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (bSuccess)
    {
        RemoveCxHealthIssues(true);
        RemoveConsistencyTagHealthIssues(tagType);

        if(TagTelemetry::APP_CONSISTENCY == tagType)
        {
            RemoveVacpHealthIssues();
        }
    }
    else
    {
        ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_vmHealthMutex);
        DebugPrintf(SV_LOG_DEBUG, "%s:Generated a count of %d VSS Writer/Provider and Tags Health Issues...\n", FUNCTION_NAME, vacpAndTagsHealthIssues.size());
        InsertHealthIssues(vacpAndTagsHealthIssues);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool MonitorHostThread::IsDriverMissing(std::string &msg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string driverVersion = InmageProduct::GetInstance().GetDriverVersion();
    if (driverVersion.empty())
    {
        std::string strKernelVersion;
        GetKernelVersion(strKernelVersion);
        msg = "Host name: ";
        msg += Host::GetInstance().GetHostName();
        msg += ", Kernel version: ";
        msg += strKernelVersion;

        return true;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return false;
}

void MonitorHostThread::CreateAgentHealthIssueMap(SourceAgentProtectionPairHealthIssues&   healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string     msg;

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_agentHealthMutex);

    DebugPrintf(SV_LOG_DEBUG, "%s: Validating Filter Driver\n", FUNCTION_NAME);
    if (IsDriverMissing(msg))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Filter Driver is not loaded error: %s\n", FUNCTION_NAME, msg.c_str());
        healthIssues.HealthIssues.push_back(HealthIssue(AgentHealthIssueCodes::VMLevelHealthIssues::FilterDriverNotLoaded::HealthCode));
        return;
    }

    DebugPrintf(SV_LOG_DEBUG, "%s: Validating protected disk list\n", FUNCTION_NAME);
    UpdateMissingDiskList(healthIssues.DiskLevelHealthIssues);

    DebugPrintf(SV_LOG_DEBUG, "%s: Validating Log location\n", FUNCTION_NAME);
    UpdateLogUploadLocationStatus(healthIssues.HealthIssues);

    // add health issues from all the component/processes like s2, dataprotection
    if (!CollateAllComponentsHealthIssues(healthIssues))
    {
        // Insert Collated Health Issues
        DebugPrintf(SV_LOG_ERROR, "%s: failed to add component health issues\n", FUNCTION_NAME);
    }

    // Insert CX Events
    {
        ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_diskHealthMutex);
        DebugPrintf(SV_LOG_DEBUG, "%s:Number of Disk level Health issues available to be posted = %d .\n",
            FUNCTION_NAME, m_diskHealthIssues.size());
        healthIssues.DiskLevelHealthIssues.insert(healthIssues.DiskLevelHealthIssues.end(),
            m_diskHealthIssues.begin(),
            m_diskHealthIssues.end());
    }

    // Insert any VM issues like Writer Failures
    {
        DebugPrintf(SV_LOG_DEBUG, "%s:Number of VM Level health issues (likeVSS Writer/Provider) available to be posted =  %d .\n",
            FUNCTION_NAME, m_vmHealthIssues.size());
        ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_vmHealthMutex);
        healthIssues.HealthIssues.insert(healthIssues.HealthIssues.end(),
            m_vmHealthIssues.begin(),
            m_vmHealthIssues.end());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}

void MonitorHostThread::HealthIssueReported()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ACE_Guard<ACE_Mutex> PropertyChangeGuard(m_agentHealthMutex);
    if (--m_CxClearHealthIssueCounter <= 0)
    {
        RemoveCxHealthIssues(true);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

bool MonitorHostThread::CollateAllComponentsHealthIssues(SourceAgentProtectionPairHealthIssues& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool bRet = false;
    try
    {
        //get the list of disks protected
        std::set<std::string>   setProtectedDiskIds;
        SVSTATUS status = GetProtectedDeviceIds(setProtectedDiskIds);
        if (status != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "%s Failed to get protected deviceIds with %d and hence cannot process IR and DR Health Issues.\n",
                FUNCTION_NAME,
                status);
            return bRet;
        }

        std::set<std::string>   setResyncDiskIds;
        if (!GetDevicesInResyncMode(setResyncDiskIds))
        {
            DebugPrintf(SV_LOG_ERROR, "%s Failed to get resync deviceIds hence cannot process IR and DR Health Issues.\n",
                FUNCTION_NAME,
                status);
            return bRet;
        }

        std::string fileSpec = m_strHealthCollatorPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + "*.json";

        std::string jsonHealthFiles;
        ListFile::listFileGlob(fileSpec, jsonHealthFiles, false);
        if (jsonHealthFiles.empty())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: There are no .json files available in %s\n",
                FUNCTION_NAME, m_strHealthCollatorPath.c_str());
            return true;
        }

        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
        boost::char_separator<char> sep("\n");
        tokenizer_t tokens(jsonHealthFiles, sep);
        tokenizer_t::iterator iter(tokens.begin());
        tokenizer_t::iterator iterEnd(tokens.end());

        for ( /* empty */; (iter != iterEnd) && (!QuitRequested(0)); ++iter)
        {
            //Process the Health Issues in each .json file
            SourceAgentProtectionPairHealthIssues compHealthIssues;
            if (GetHealthIssues( *iter, compHealthIssues))
            {
                bRet |= PopulateHealthIssues(setProtectedDiskIds, setResyncDiskIds, compHealthIssues, healthIssues);
            }
        }
    }
    catch (const boost::filesystem::filesystem_error &e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught exception %s.\n",FUNCTION_NAME, e.what());
    }
    catch (const std::exception &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: caught Standard exception %s.\n",FUNCTION_NAME, ex.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s:Failed with a generic exception.\n",FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}


bool MonitorHostThread::GetHealthIssues(const std::string&strJsonHealthFile,
    SourceAgentProtectionPairHealthIssues& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = false;
    try
    {
        //1. Process all health issues
        //(e.g.:<install folder>\AgentHealth\s2_<hostid>.json
        //(e.g.:<install folder>\AgentHealth\s2_<hostid>_<diskid>.json
        //(e.g.:<install folder>\AgentHealth\dp_<hostid>_<diskid>.json
        //Considering the files with current host id only
        if (strJsonHealthFile.find(m_hostId) == std::string::npos)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s:The Health Isues from an unrelated hostId file %s is being ignored and deleted.\n", 
                FUNCTION_NAME,strJsonHealthFile.c_str());
            boost::system::error_code ec;
            if (!boost::filesystem::remove(strJsonHealthFile, ec))
            {
                DebugPrintf(SV_LOG_ERROR, "%s,Failed to delete the obsolete health file %s. error code %d, error %s.\n",
                    FUNCTION_NAME, strJsonHealthFile.c_str(), ec.value(), ec.message().c_str());
            }
            return bRet;
        }

        HealthCollatorPtr healthCollatorPtr;
        healthCollatorPtr.reset(new HealthCollator(strJsonHealthFile));
        if (healthCollatorPtr.get() == NULL)
        {
            DebugPrintf(SV_LOG_ERROR, "%s:No such health file %s found in the system.\n",
                FUNCTION_NAME,strJsonHealthFile.c_str());
            return bRet;
        }

        //Since, just above the Init() method in constructor has filled the json content and hence no need to read it afresh
        if (!healthCollatorPtr->GetAllHealthIssues(healthIssues))
        {
            DebugPrintf(SV_LOG_ERROR, "%s:failed to get health issues from file %s.\n",
                FUNCTION_NAME, strJsonHealthFile.c_str());
            return bRet;
        }

        bRet = true;
    }
    catch (const std::exception &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception: %s.\n", FUNCTION_NAME, ex.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

bool MonitorHostThread::PopulateHealthIssues(const std::set<std::string>& setProtectedDiskIds,
    const std::set<std::string>& setResyncDiskIds,
    const SourceAgentProtectionPairHealthIssues& componentHealthIssues,
    SourceAgentProtectionPairHealthIssues& healthIssues)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    const std::set<std::string> setIRIssueCodes(g_IRissueCodes, g_IRissueCodes + INM_ARRAY_SIZE(g_IRissueCodes));

    bool bRet = false;

    try
    {
        //If the pair is not in Resync, then remove Resync Throttle Health Issue
        std::vector<AgentDiskLevelHealthIssue>::const_iterator diskHealthIssueIter;
        for (diskHealthIssueIter = componentHealthIssues.DiskLevelHealthIssues.begin();
            diskHealthIssueIter != componentHealthIssues.DiskLevelHealthIssues.end();
            diskHealthIssueIter++)
        {
            if (setProtectedDiskIds.find(diskHealthIssueIter->DiskContext) == setProtectedDiskIds.end())
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: dropping health issue for disk %s is not protected.\n",
                    FUNCTION_NAME, diskHealthIssueIter->DiskContext.c_str());

                continue;
            }

            bool bInResync = setResyncDiskIds.find(diskHealthIssueIter->DiskContext) != setResyncDiskIds.end();
            bool bResyncCode = setIRIssueCodes.find(diskHealthIssueIter->IssueCode) != setIRIssueCodes.end();

            if (!bInResync && bResyncCode)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: dropping health issue %s for the disk %s as it is not in IR/Resync.\n",
                    FUNCTION_NAME, diskHealthIssueIter->IssueCode.c_str(), diskHealthIssueIter->DiskContext.c_str());

                continue;
            }

            healthIssues.DiskLevelHealthIssues.push_back(*diskHealthIssueIter);
        }

        healthIssues.HealthIssues.insert(healthIssues.HealthIssues.end(),
            componentHealthIssues.HealthIssues.begin(),
            componentHealthIssues.HealthIssues.end());

        bRet = true;
    }
    catch (const std::exception &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception: %s.\n", FUNCTION_NAME, ex.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bRet;
}

void MonitorHostThread::UpdateFilePermissions()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

#ifdef SV_UNIX

    using namespace boost::filesystem;

    const steady_clock::duration interval(boost::chrono::seconds(900));
    static steady_clock::time_point s_prevRunTime = steady_clock::time_point::min();
    steady_clock::time_point currTime = steady_clock::now();

    if (currTime < s_prevRunTime + interval)
    {
        DebugPrintf(SV_LOG_DEBUG, "EXITED %s as time not lapsed for update.\n", FUNCTION_NAME);
        return;
    }

    try {
        LocalConfigurator lc;
        std::stringstream ssFilesWithWorldWritablePerms;

        std::set<path> pathList;
        pathList.insert(lc.getInstallPath());
        pathList.insert(securitylib::securityTopDir());
        pathList.insert("/etc/vxagent/");

        pathList.insert(path(lc.getLogPathname() + "vxlogs"));
        pathList.insert(path(lc.getLogPathname() + "ApplicationData"));
        pathList.insert(path(lc.getLogPathname() + "ApplicationPolicyLogs"));

        std::string additionalPaths = lc.getAdditionalInstallPaths();
        if (!additionalPaths.empty())
        {
            typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
            boost::char_separator<char> sep(",");
            tokenizer_t tokens(additionalPaths, sep);
            tokenizer_t::iterator iter(tokens.begin());
            tokenizer_t::iterator iterEnd(tokens.end());

            for ( /* empty */; (iter != iterEnd) && (!QuitRequested(0)); ++iter)
            {
                boost::system::error_code ec;
                bool bExists = boost::filesystem::exists(*iter, ec);
                if (!ec && bExists)
                {
                    DebugPrintf(SV_LOG_ALWAYS, "%s: adding additional path %s\n", FUNCTION_NAME, iter->c_str());
                    pathList.insert(*iter);
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "%s: failed to add additional path %s. exists = %d,  error %d: %s\n",
                        FUNCTION_NAME,
                        iter->c_str(),
                        bExists,
                        ec.value(),
                        ec.message().c_str());
                }
            }
        }

        std::set<path>::const_iterator pathIter = pathList.begin();
        for (/*empty*/; pathIter != pathList.end() && !QuitRequested(0); pathIter++)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: searching in %s\n", FUNCTION_NAME, pathIter->string().c_str());

            boost::system::error_code ec;
            recursive_directory_iterator dirIter(*pathIter, ec), end;
            if (ec)
            {
                DebugPrintf(SV_LOG_DEBUG, "%s: not searching in %s. error %d: %s\n",
                    FUNCTION_NAME,
                    pathIter->string().c_str(),
                    ec.value(),
                    ec.message().c_str());

                continue;
            }

            for (/*empty*/; dirIter != end && !QuitRequested(0); dirIter++)
            {
                file_status fileStatus = status(*dirIter);
                perms p = fileStatus.permissions();
                if ((p & others_write) != no_perms)
                {
                    if (!ssFilesWithWorldWritablePerms.str().empty())
                        ssFilesWithWorldWritablePerms << ", ";

                    ssFilesWithWorldWritablePerms << dirIter->path().string() << ":" << std::oct << p;
                    boost::system::error_code ec;
                    permissions(*dirIter, remove_perms | others_write, ec);
                    if (ec)
                    {
                        ssFilesWithWorldWritablePerms << ":" << std::oct << p;

                        DebugPrintf(SV_LOG_ERROR, "%s: failed to set permissions for file %s with error %d: %s\n",
                            FUNCTION_NAME,
                            dirIter->path().string().c_str(),
                            ec.value(),
                            ec.message().c_str());
                    }
                    else
                    {
                        fileStatus = status(*dirIter);
                        p = fileStatus.permissions();
                        ssFilesWithWorldWritablePerms << ":" << std::oct << p;
                    }
                }
            }
        }

        if (!ssFilesWithWorldWritablePerms.str().empty())
        {
            DebugPrintf(SV_LOG_ALWAYS, "%s: files with world writable permissions: %s\n",
                FUNCTION_NAME, ssFilesWithWorldWritablePerms.str().c_str());
        }

        s_prevRunTime = currTime;
    }
    catch (const std::exception &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed with exception: %s.\n", FUNCTION_NAME, ex.what());
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s failed with an unknown exception.\n", FUNCTION_NAME);
    }

#endif

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return;
}
