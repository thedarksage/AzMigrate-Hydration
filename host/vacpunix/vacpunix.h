#include <set>

#include <boost/thread/mutex.hpp>
#include <boost/chrono.hpp>

#include "Communicator.h"
#include "PerfCounter.h"

#include "VacpErrorCodes.h"
#include "TagTelemetryKeys.h"
#include "VacpCommon.h"

#if defined linux | defined SV_AIX
#include "ApplicationConsistencyController.h"
#endif

#define	OPTION_DISTRIBUTED_PROTOCOL_TIMEOUT         "dtimeout"

#define VACP_E_OPTIONS_NOT_SUPPORTED                                   10000
#define VACP_E_INVALID_COMMAND_LINE                                    10001
#define VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED                           10002
#define VACP_E_DRIVER_IN_NON_DATA_MODE                                 10003
#define VACP_E_NO_PROTECTED_VOLUMES                                    10004
#define VACP_E_NON_FIXED_DISK                                          10005
#define VACP_E_ZERO_EFFECTED_VOLUMES                                   10006
#define VACP_E_VACP_SENT_REVOCATION_TAG                                10007
#define VACP_E_PRESCRIPT_FAILED                                        10009
#define VACP_E_POSTSCRIPT_FAILED                                       10010
#define VACP_FAILED_TO_SEND_TAGS_TO_DRIVER                             10014
#define VACP_E_INVALID_COMPONENT                                       10015
#define VACP_FAILED_TO_CREATE_EVENT_RESOURCES                          10050
#define VACP_TIMEOUT_WHILE_WAITING_FOR_MASTER_NODE                     10051
#define VACP_TIMEOUT_WHILE_WAITING_FOR_COORDINATOR_NODE                10052
#define ERROR_FAILED_TO_GET_COMMUNICATOR                               10053
#define ERROR_CREATING_SERVER_SOCKET                                   10054
#define ERROR_CREATING_CLIENT_SOCKET                                   10055
#define ERROR_CREATING_COORDINATOR_SOCKET                              10056
#define ERROR_PREPARING_VACP_SPAWNED_ACK_PACKET                        10057
#define ERROR_SENDING_VACP_SPAWNED_CMD                                 10058
#define ERROR_FAILED_TO_SEND_SPAWNED_ACK                               10059
#define ERROR_FAILED_TO_GET_PREPARE_CMD                                10060
#define ERROR_FAILED_TO_SEND_PREPARE_CMD                               10061
#define ERROR_FAILED_TO_SEND_QUIESCE_CMD                               10062
#define ERROR_FAILED_TO_SEND_PREPARE_ACK                               10063
#define ERROR_FAILED_TO_SEND_QUIESCE_ACK                               10064
#define ERROR_FAILED_TO_SEND_RESUME_ACK                                10065
#define ERROR_FAILED_TO_SEND_TAG_GUID_TO_COORDINATOR_NODES             10066
#define ERROR_SENDING_TAG_COMMIT_COMMAND                               10067
#define ERROR_SENDING_TAG_COMMIT_ACK                                   10068
#define ERROR_CONNECTING_TO_COORDINATOR_VACP_NODE                      10069
#define ERROR_TIMEOUT_WAITING_FOR_PREPARE_ACK                          10070
#define ERROR_CONNECTING_CLIENT_TO_SERVER_SOCKET                       10071
#define ERROR_TIMEOUT_WAITING_FOR_QUIESCE_ACK                          10072
#define ERROR_FAILED_TO_SEND_STATUS_INFO_TO_MASTER                     10073
#define ERROR_DISTRIBUED_MASTER_IN_NON_DATA_MODE                       10074
#define ERROR_TIMEOUT_WAITING_FOR_INSERT_TAG_ACK                       10075
#define ERROR_TIMEOUT_WAITING_FOR_RESUME_ACK                           10076
#define ERROR_FAILED_TO_GET_QUIESCE_CMD                                10077
#define ERROR_FAILED_TO_GET_INSERT_TAG_CMD                             10078
#define ERROR_FAILED_TO_GET_RESUME_CMD                                 10079
#define ERROR_FAILED_TO_RECEIVE_TAG_GUID_FROM_MASTER_NODE              10080
#define ERROR_RECEIVED_INVALID_TAG_GUID_FROM_MASTER_NODE               10081
#define ERROR_FAILED_TO_GET_CONSISTENCY_SCHEDULES                      10082

#define VACP_PRESCRIPT_WAIT_TIME_EXCEEDED                              10998
#define VACP_POSTSCRIPT_WAIT_TIME_EXCEEDED                             10999

enum CONSISTENCY_STATUS {
    CONSISTENCY_STATUS_INITIALIZED = 0x1,
    CONSISTENCY_STATUS_PREPARE_SUCCESS = 0x2,
    CONSISTENCY_STATUS_QUIESCE_SUCCESS = 0x4,
    CONSISTENCY_STATUS_INSERT_TAG_SUCCESS = 0x8,
    CONSISTENCY_STATUS_RESUME_SUCCESS = 0x10,
    CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS = 0x20,
    CONSISTENCY_STATUS_SUCCESS_BITS = 0x3F
};

enum COMMUNICATOR_STATE {
    COMMUNICATOR_STATE_INITIALIZED = 0x1,
    COMMUNICATOR_STATE_SERVER_SOCK_CREATE_COMPLETED = 0x02,
    COMMUNICATOR_STATE_SERVER_SPAWN_ACK_COMPLETED = 0x04,
    COMMUNICATOR_STATE_SERVER_TAG_GUID_COMPLETED = 0x08,
    COMMUNICATOR_STATE_SERVER_PREPARE_CMD_COMPLETED = 0x10,
    COMMUNICATOR_STATE_SERVER_PREPARE_ACK_COMPLETED = 0x11,
    COMMUNICATOR_STATE_SERVER_QUIESCE_CMD_COMPLETED = 0x12,
    COMMUNICATOR_STATE_SERVER_QUIESCE_ACK_COMPLETED = 0x14,
    COMMUNICATOR_STATE_SERVER_INSERT_TAG_CMD_COMPLETED = 0x18,
    COMMUNICATOR_STATE_SERVER_INSERT_TAG_ACK_COMPLETED = 0x20,
    COMMUNICATOR_STATE_SERVER_RESUME_CMD_COMPLETED = 0x21,
    COMMUNICATOR_STATE_SERVER_RESUME_ACK_COMPLETED = 0x22,

    COMMUNICATOR_STATE_CLIENT_SOCK_CREATE_COMPLETED = 0x100,
    COMMUNICATOR_STATE_CLIENT_SPAWN_ACK_COMPLETED = 0x200,
    COMMUNICATOR_STATE_CLIENT_TAG_GUID_COMPLETED = 0x400,
    COMMUNICATOR_STATE_CLIENT_PREPARE_CMD_COMPLETED = 0x800,
    COMMUNICATOR_STATE_CLIENT_PREPARE_ACK_COMPLETED = 0x1000,
    COMMUNICATOR_STATE_CLIENT_QUIESCE_CMD_COMPLETED = 0x1100,
    COMMUNICATOR_STATE_CLIENT_QUIESCE_ACK_COMPLETED = 0x1200,
    COMMUNICATOR_STATE_CLIENT_INSERT_TAG_CMD_COMPLETED = 0x1400,
    COMMUNICATOR_STATE_CLIENT_INSERT_TAG_ACK_COMPLETED = 0x1800,
    COMMUNICATOR_STATE_CLIENT_RESUME_CMD_COMPLETED = 0x2000,
    COMMUNICATOR_STATE_CLIENT_RESUME_ACK_COMPLETED = 0x2100
};

typedef struct TagInfo
{
    char * tagData;
    unsigned short tagLen;
} TagInfo_t;

enum FREEZE_STATE {
    FREEZE_STATE_INIT = 0,
    FREEZE_STATE_LOCKED,
    FREEZE_STATE_UNLOCKED
};
struct FsVolInfo
{
    FsVolInfo(const std::string & s, const std::string & t, const std::string & m)
    {
        fd = -1;
        volName = s;
        fsType = t;
        mntDir = m;
        freezeState = FREEZE_STATE_INIT;
    }
    int fd;
    std::string volName;
    std::string fsType;
    std::string mntDir;
    int         freezeState;
};

typedef std::map<std::string, struct FsVolInfo> t_fsVolInfoMap;

class Consistency
{
public:
    Consistency(bool bDistributedVacp, const std::string& strType, const TAG_TYPE TagType) : 
        m_bDistributedVacp(bDistributedVacp),
        m_strType(strType),
        m_TagType(TagType),
        m_exitCode(0),
        m_bInternallyRescheduled(false),
        m_lastRunErrorCode(0) {}
    
    virtual void Process();
    
    virtual void InitializeCommunicator();
    virtual void StartCommunicator();
    virtual void DistributedTagGuidPhase(std::string & uuid, const set<std::string> & inputtags, std::string& uniqueIdStr, set<std::string> & distributedInputtags);
    virtual void PreparePhase();
    virtual void PrepareAckPhase();
    virtual void QuiescePhase();
    virtual void QuiesceAckPhase();
    virtual void TagPhase();
    virtual void TagAckPhase();
    virtual void ResumePhase();
    virtual void ResumeAckPhase();
    virtual bool CommitRevokeTags();
    
    virtual bool Quiesce() = 0;
    virtual void CheckDistributedQuiesceState() = 0;
    virtual bool Resume() = 0;
    
    virtual bool SendTagsToLocalDriver(const char * buffer);
    
    virtual bool GenerateTagNames(set<TagInfo_t> & finaltags, set<string> & inputtags, 
                      /*set<string & inputApps, */  const bool FSTag, std::string & uuid, std::string& uniqueIdStr);
                      
    virtual unsigned int CreateTagIoctlBufferForOlderDriver(char ** tagBuffer, const set<std::string> &affectedVolumes, const set<TagInfo_t> &finalTags);
    
    virtual unsigned int CreateTagIoctlBuffer(char ** tagBuffer, const set<std::string> &volumes, const set<TagInfo_t> &tags);
    
    virtual bool CheckTagsStatus(const char *buffer, const unsigned int size, const int wait_time, const set<string> volumes);
    
    virtual bool BuildAndAddStreamBasedTag(set<TagInfo_t> & finaltags, unsigned short tagId, const char * inputTag, const std::string &sTagGuid);
    
    virtual void ExitDistributedConsistencyMode();
    virtual void Conclude(unsigned long hr);
    
    void FacilitateDistributedConsistency();

    void InitSchedule();
    void SetSchedule();
    void SetNextSchedule();
    boost::chrono::steady_clock::time_point GetSchedule(bool bSignalQuit);

    uint32_t GetConsistencyInterval() { return m_interval; }

    virtual void AddToVacpFailMsgs(const int &failureModule, const std::string &errorMsg, const long &errorCode)
    {
        m_VacpFailMsgsList.push_back(FailMsg(failureModule, errorMsg, errorCode));
    }

    virtual void AddToMultivmFailMsgs(const int &failureModule, const std::string &errorMsg, const long &errorCode)
    {
        m_MultivmFailMsgsList.push_back(FailMsg(failureModule, errorMsg, errorCode));
    }

    virtual void UpdateStartTimeTelInfo();

    virtual void IRModeStamp();
    
    virtual ~Consistency(){}
 
public:
    std::string     m_strType;
    std::string     m_strContextUuid; // used as a context id for the driver to distiguish different tag ioctls
    set<string>     m_volMntDirs;  //Set of mount dirs to be freezed
    t_fsVolInfoMap  m_fsVolInfoMap;
    int             m_exitCode;
    
    const TAG_TYPE  m_TagType;
    bool            m_bDistributedVacp;
    
    unsigned int    m_status;
    unsigned int    m_DistributedProtocolStatus;
    unsigned int    m_DistributedProtocolState;
    
    CommunicatorPtr         m_pCommunicator;
    CommunicatorTimeouts    m_CommTimeouts;
    
    boost::chrono::steady_clock::time_point m_exitTime;

    uint32_t                m_interval;
    boost::chrono::steady_clock::time_point m_prevRunTime;
    
    PerfCounter m_perfCounter;
    
    FailMsgList             m_VacpFailMsgsList;
    FailMsgList             m_MultivmFailMsgsList;
    VacpLastError           m_vacpLastError;

    TagTelemetryInfo        m_tagTelInfo;

    bool                    m_bInternallyRescheduled;

    // this should be retained across the multiple runs
    // in parallel mode
    uint32_t                m_lastRunErrorCode;
    std::string             m_hydrationTag;
};

class CrashConsistency : public Consistency
{
public:
    static CrashConsistency& CreateInstance();
    static CrashConsistency& Get();

    virtual bool Quiesce();
    virtual void CheckDistributedQuiesceState();
    virtual bool Resume();
    virtual bool CreateIoBarrier(const long timeout);
    virtual bool RemoveIoBarrier();
    
    virtual ~CrashConsistency(){}
    
private:
    static CrashConsistency* m_theCrashC;
    static boost::mutex m_theCrashCLock;
    bool m_bDistributedIoBarrierCreated;

private:
    CrashConsistency(bool bDistributedVacp) :
            Consistency(bDistributedVacp, ConsistencyTypeCrash, CRASH),
            m_bDistributedIoBarrierCreated(false){}
};

class AppConsistency : public Consistency
{
public:
    static AppConsistency& CreateInstance();
    static AppConsistency& Get();

    virtual bool Quiesce();
    virtual void CheckDistributedQuiesceState(){/* empty - place holder function */}
    virtual bool Resume();
    virtual bool FreezeDevices(const set<string> &inputVolumes);
    virtual bool ThawDevices(const set<string> &inputVolumes);
    
    virtual ~AppConsistency(){}

private:
    static AppConsistency* m_theAppC;
    static boost::mutex m_theAppCLock;
    
#if defined (linux) | defined (SV_AIX)
    ApplicationConsistencyControllerPtr m_appConPtr;
#endif

private:
    AppConsistency(bool bDistributedVacp) :
            Consistency(bDistributedVacp, ConsistencyTypeApp, APP)
    {
#if defined (linux) | defined (SV_AIX)  
        m_appConPtr = ApplicationConsistencyController::getInstance();
    }
#endif
};

class BaselineConsistency : public Consistency
{
public:
    static BaselineConsistency& CreateInstance();
    static BaselineConsistency& Get();
    
    virtual bool Quiesce() { return true; }
    virtual void CheckDistributedQuiesceState() {}
    virtual bool Resume() { return true; }

    virtual void Process();
    virtual ~BaselineConsistency(){}

private:
    static BaselineConsistency* m_theBaseline;
    static boost::mutex m_theBaselineLock;

private:
    BaselineConsistency() :
        Consistency(false, ConsistencyTypeBaseline, BASELINE) {}

};


