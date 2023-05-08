#ifndef _CONSISTENCY_THREAD_H
#define _CONSISTENCY_THREAD_H

#include <ace/Thread_Manager.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Event.h>

#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>
#include <boost/filesystem.hpp>

#include "LogCutter.h"
#include "inmquitfunction.h"
#include "sigslot.h"
#include "TagTelemetry.h"
#include "inmalert.h"
#include "VacpErrors.h"
#include "AgentHealthContract.h"

typedef enum ConsistencyType {
    CONSISTENCY_TYPE_UNKNOWN = 0,
    CONSISTENCY_TYPE_APP = 1,
    CONSISTENCY_TYPE_CRASH =2
};

#define CONSISTENCY_SETTINGS_FETCH_INTERVAL 300 // secs

class ConsistencyThread : public sigslot::has_slots<>
{
public:
    ConsistencyThread();
    ~ConsistencyThread();
    void Start();
    void Stop();

    /// \brief waits for quit event for given seconds and
    ///  returns true if quit is set (even before timeout expires).
    bool QuitRequested(int seconds); ///< seconds

    void SetTagStatusCallBack(boost::function<void(const TagTelemetry::TagType&, const bool&, const std::list<HealthIssue>&)> setLastTagStatusCallback)
    {
        m_SetLastTagStatusCallback = setLastTagStatusCallback;
    }

protected:
    static Logger::Log m_TagTelemetryLog;

    std::string m_hostId;
    std::string m_logDirName;
    std::string m_configDirName;

    boost::chrono::steady_clock::time_point m_prevSettingsFetchTime;

    ACE_Thread_Manager m_threadManager;
    static ACE_THR_FUNC_RETURN ThreadFunc(void* pArg);
    ACE_THR_FUNC_RETURN run();
    void TruncatePolicyLogs();

    bool GetFileContent(const std::string& filename, std::string& output);
    uint64_t GetLastConsistencyScheduleTime(ConsistencyType type);
    void ParseCmdOptions(const std::string& cmdOptions, TagTelemetry::TagStatus& tagStatus);

    virtual void ProcessConsistencyLogs();
    virtual void GetConsistencyIntervals(uint64_t &crashInterval, uint64_t &appInterval) = 0; // TODO: should be part of vacp cmdOutput
    virtual bool IsMasterNode() const = 0;
    virtual void SendAlert(InmAlert &inmAlert) const = 0;
    virtual void AddDiskInfoInTagStatus(const TagTelemetry::TagType &tagType,
        const std::string &cmdOutput,
        TagTelemetry::TagStatus &tagStatus) const = 0;
    virtual uint32_t GetNoOfNodes() = 0; // TODO: should be part of vacp cmdOutput
    virtual bool IsTagFailureHealthIssueRequired() const = 0;


private:
    boost::shared_ptr<ACE_Event> m_QuitEvent;
    bool m_started;
    ACE_Mutex m_ConsistencyMutex;
    boost::filesystem::path m_scheduleFilePath;
    bool m_active;

    boost::function<void(const TagTelemetry::TagType&, const bool&, const std::list<HealthIssue>&)> m_SetLastTagStatusCallback;

    void ProcessVacpAndTagsHealthIssues(TagTelemetry::TagType& tagtype, bool bSuccess, std::string strTagGuid, std::list<HealthIssue>&vacpAndTagsHealthIssues);
};


#endif // _CONSISTENCY_THREAD_H
