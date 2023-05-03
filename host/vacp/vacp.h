#ifndef _INMAGE_VSS_APP_CONSISTENCY_PROVIDER__H
#define _INMAGE_VSS_APP_CONSISTENCY_PROVIDER__H

#include <winioctl.h>

//#include <ace/Event.h>
#include <boost/thread/mutex.hpp>
#include <boost/chrono.hpp>

#include "../common/version.h"
#include "InMageVssProvider_i.h"

#include "Communicator.h"
#include "VacpErrorCodes.h"
#include "VacpConfDefs.h"
#include "TagTelemetryKeys.h"
#include "VacpCommon.h"

#include "common.h"
#include "event.h"

#include "VacpErrors.h"

#define GUID_SIZE_IN_CHARS      36
#define FILE_DEVICE_SVSYS   0x00008001
#define PRESCRIPT_WAIT_TIME 300000
#define POSTSCRIPT_WAIT_TIME 300000

#define MAX_RETRYOPERATION                      "MaxRetryOperation"
#define MAX_SNAPSHOT_IN_PROGRESS_RETRY_COUNT    "MaxSnapShotInProgressRetryCount"
#define VACP_REG_KEY                            "Software\\SV Systems\\VACP"
#define AGENT_REG_KEY                           "Software\\SV Systems\\"
#define CX_SERVER_IP                            "ServerName"
#define CX_SERVER_PORT                          "ServerHttpPort"


#define OPTION_VOLUME                   "v"
#define OPTION_APPLICATION              "a"
#define OPTION_FULLBACKUP               "f"
#define OPTION_TAGNAME                  "t"
#define OPTION_WRITER_INSTANCE_NAME     "w"
#define OPTION_EXCLUDEAPP               "x"
#define OPTION_PRINT                    "p" 
#define OPTION_SKIPDRVCHK               "s"
#define OPTION_HELP                     "h"
#define OPTION_QUESTIONMARK             "?"
#define OPTION_SYNC_TAG                 "sync"
#define OPTION_TAG_TIMEOUT              "tagtimeout"
#define OPTION_CRASH_CONSISTENCY        "cc"
#define OPTION_BASELINE                 "baseline"
#define OPTION_TAG_GUID                 "tagguid"
#define OPTION_DELAYED_START            "ds"
#define OPTION_HYDRATION                "hydration"
#define OPTION_VERBOSE                  "verbose"
#define OPTION_PARALLEL_RUN              "parallel"
#define OPTION_VTOA_MODE                "vtoa"
#define OPTION_IR_MODE                  "irmode"
#define OPTION_CLUSTER                  "cluster"

#define OPTION_PROVIDER                 "provider"
#define OPTION_NOTAG                    "notag"
#define OPTION_REMOTE                   "remote"
#define OPTION_SERVER_DEVICE            "serverdevice"
#define OPTION_SERVER_IP                "serverip"
#define OPTION_SERVER_PORT              "serverport"
#define DEFAULT_SERVER_PORT             20003
#define OPTION_VOLUME_GUID              "guid"
#define OPTION_VACP_VERIFY              "verify"
#define OPTION_PERSIST_SNAPSHOTS        "persist"
#define OPTION_DEL_SNAPSHOTS            "delete"
#define OPTION_DELETE_ALL_VACP_SNAPSHOTS "all"
#define OPTION_DELETE_SNAPSHOTIDS       "shadowcopyids"
#define OPTION_DELETE_SNAPSHOTSETS      "shadowcopysets"
#define OPTION_CS_IP                    "csip"
#define OPTION_CS_PORT                  "csport"
#define OPTION_IGNORE_NON_DATA_MODE     "ignoreNonDataMode"
#define OPTION_TAG_LIFE_TIME            "tagLifeTime"

#define OPTION_TAG_LIFE_TIME_MINS       "minutes"
#define OPTION_TAG_LIFE_TIME_HOURS      "hours"
#define OPTION_TAG_LIFE_TIME_DAYS       "days"
#define OPTION_TAG_LIFE_TIME_WEEKS      "weeks"
#define OPTION_TAG_LIFE_TIME_MONTHS     "months"
#define OPTION_TAG_LIFE_TIME_YEARS      "years"
#define OPTION_TAG_LIFE_TIME_FOREVER    "forever"
#define OPTION_SKIP_UNPROTECTED         "su"
#define OPTION_ENUMERATE_SYSTEMWRITER   "EnumSW"
#define OPTION_SYSTEMLEVELTAG           "systemlevel"
#define OPTION_PRESCRIPT                "prescript"
#define OPTION_POSTSCRIPT               "postscript"
#define OPTION_DISTRIBUTED_VACP         "distributed"
#define OPTION_MASTER_NODE              "mn" //"masternode"
#define OPTION_COORDINATOR_NODES        "cn" //"Coordinator nodes/client nodes
#define OPTION_DISTRIBUTED_PORT         "dport"
#define OPTION_MASTER_NODE_FINGERPRINT  "mnf"
#define OPTION_RCM_SETTINGS_PATH        "rcmSetPath"
#define OPTION_PROXY_SETTINGS_PATH      "proxySetPath"

#define	OPTION_DISTRIBUTED_PROTOCOL_TIMEOUT         "dtimeout"


#define VACP_E_SUCCESS                                                 0
#define VACP_E_GENERIC_ERROR                                           1
#define VACP_E_OPTIONS_NOT_SUPPORTED                                   10000
#define VACP_E_INVALID_COMMAND_LINE                                    10001
#define VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED                           10002
#define VACP_E_DRIVER_IN_NON_DATA_MODE                                 10003
#define VACP_E_NO_PROTECTED_VOLUMES                                    10004
#define VACP_E_NON_FIXED_DISK                                          10005
#define VACP_E_ZERO_EFFECTED_VOLUMES                                   10006
#define VACP_E_VACP_SENT_REVOCATION_TAG                                10007
#define VACP_E_ERROR_GETTING_SYSVOL_FOR_SYSTEMWRITER                   10008
#define VACP_E_VACP_MODULE_FAIL                                        0x80004005L
#define VACP_E_PRESCRIPT_FAILED                                        10009
#define VACP_E_POSTSCRIPT_FAILED                                       10010
#define VACP_FAILED_TO_DELETE_SHADOW_COPIES                            10011
#define VACP_FAILED_TO_DELETE_A_SPECIFIC_SHADOW_COPY                   10012
#define VACP_FAILED_TO_DELETE_A_SPECIFIC_SHADOW_COPY_SET               10013
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

#define ERROR_NODE_HAS_MORE_VOLS_THAN_SNAPSHOT_SET_LIMIT               10083
#define ERROR_FAILED_TO_INITIALIZE_VSS_REQUESTOR                       10084
#define ERROR_FAILED_TO_INITIALIZE_VSS_PROVIDER                        10085

#define ERROR_FAILED_TO_INITIALIZE_WINSOCK_LIBRARY                     10900
#define ERROR_VACP_TERMINATING_VSS_PROVIDER_EXIT_CODE                  10901

#define VACP_PRESCRIPT_WAIT_TIME_EXCEEDED                              10998
#define VACP_POSTSCRIPT_WAIT_TIME_EXCEEDED                             10999

#define VACP_MAX_TAG_LENGTH             256
#define MAX_LENGTH_OF_ALL_TAGS          2048

#define VACP_IOBARRIER_TIMEOUT              300 // in ms
#define VACP_IOBARRIER_MAX_TIMEOUT          500 // in ms

#define VACP_IOBARRIER_MAX_RETRY_TIME   500 // in ms
#define VACP_IOBARRIER_RETRY_WAIT_TIME  100

// for single node crash tag 
#define VACP_CRASHTAG_MAX_RETRY_TIME   10000 // in ms, 10 sec
#define VACP_CRASHTAG_RETRY_WAIT_TIME  1000 // in ms, 1 sec

#define VACP_TAG_IOCTL_MAX_RETRY_TIME   500 // in ms
#define VACP_TAG_IOCTL_RETRY_WAIT_TIME  100

// The following default time can be changed by vacp.conf param 'DrainBarrierTimeout'
// In case of multi-vm consistency, the conf param 'AppConsistencyServerResumeAckWaitTime'
// should also be set the same.
#define VACP_TAG_DISK_IOCTL_COMMIT_TIMEOUT                180000 // in msec, 3 min
#define VACP_TAG_DISK_IOCTL_COMMIT_MAX_TIMEOUT            1800000 // in msec, 30 min


#define DISK_DRIVER_MAX_TAGDATA_LEN     3072

enum VACP_DELETE_TYPE 
{
  Enum_SnapshotId =0,
  Enum_SnapshotSetId
};

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

class CopyrightNotice
{	
public:
    CopyrightNotice()
    {
        std::cout<<"\n"<<INMAGE_COPY_RIGHT<<"\n\n";
    }
};

#if (_WIN32_WINNT > 0x500)

class VssAppConsistencyProvider
{

public:

    VssAppConsistencyProvider() 
    {
        m_dwProtectedDriveMask = 0;
        CallBackHandler = (void (*)(void *))NULL;
        CallBackContext = NULL;
        vwriter = NULL;
        bCoInitializeSucceeded = false;
        bIsCoInitializedByCaller = false;
        bSentTagsSuccessfully = false;
        m_bDoFullBackup = false;
        hInMageRemoteSubKey = NULL;
        hInMageLocalSubKey = NULL;		
        hInMageRegKey = NULL;
        m_hVssProviderCallbackThread = NULL;
    }

    VssAppConsistencyProvider(DWORD dwDriveMask, bool bCoInitializedAlready = false)
    {
        m_dwProtectedDriveMask = dwDriveMask;
        CallBackHandler = (void (*)(void *))NULL;
        CallBackContext = NULL;
        vwriter = NULL;
        bCoInitializeSucceeded = false;
        bIsCoInitializedByCaller = bCoInitializedAlready;
        bSentTagsSuccessfully = false;
        m_bDoFullBackup = false;
        hInMageRemoteSubKey = NULL;
        hInMageLocalSubKey = NULL;		
        hInMageRegKey = NULL;
        m_hVssProviderCallbackThread = NULL;
    }

    VssAppConsistencyProvider(ACSRequest_t Request, bool bPerformFullBackup = false)
    {
        CallBackHandler = (void (*)(void *))NULL;
        CallBackContext = NULL;
        vwriter = NULL;
        bCoInitializeSucceeded = false;
        bIsCoInitializedByCaller = false;
        bSentTagsSuccessfully = false;
        hInMageRemoteSubKey = NULL;
        hInMageLocalSubKey = NULL;		
        hInMageRegKey = NULL;
        m_hVssProviderCallbackThread = NULL;

        m_ACSRequest = Request;
        m_dwProtectedDriveMask = m_ACSRequest.volumeBitMask;
        m_bDoFullBackup = bPerformFullBackup;
    }

    VssAppConsistencyProvider(ACSRequest_t Request, InMageVssRequestorPtr vrPtr)
    {
        CallBackHandler = (void(*)(void *))NULL;
        CallBackContext = NULL;
        vwriter = NULL;
        bCoInitializeSucceeded = false;
        bIsCoInitializedByCaller = true;
        bSentTagsSuccessfully = false;
        hInMageRemoteSubKey = NULL;
        hInMageLocalSubKey = NULL;
        hInMageRegKey = NULL;
        m_hVssProviderCallbackThread = NULL;
        m_bDoFullBackup = true;

        m_ACSRequest = Request;
        m_dwProtectedDriveMask = m_ACSRequest.volumeBitMask;
        m_vssRequestorPtr = vrPtr;
    }

    ~VssAppConsistencyProvider()
    {
        StopRpcServer();
        DeleteRequestor();
        DeleteWriter();
        if(hInMageRemoteSubKey != NULL)
        {
            RegCloseKey(hInMageRemoteSubKey);
            hInMageRemoteSubKey = NULL;
        }
        if(hInMageLocalSubKey != NULL)
        {
            RegCloseKey(hInMageLocalSubKey);
            hInMageLocalSubKey = NULL;
        }
        if(hInMageRegKey != NULL)
        {
            RegCloseKey(hInMageRegKey);
            hInMageLocalSubKey = NULL;
        }

        if (m_hVssProviderCallbackThread != NULL)
        {
            TerminateThread(m_hVssProviderCallbackThread, 0);
            m_hVssProviderCallbackThread = NULL;
        }

        m_ACSRequest.Reset();
    }

    ACSRequest_t& GetACSRequest()
    {
        return m_ACSRequest;
    }
    
    InMageVssRequestorPtr GetRequestorPtr()
    {
        return m_vssRequestorPtr;
    }

    void RegisterCallBackHandler(void (*fn)(void *), void *ctx)
    {
        if (fn != NULL)
        {
            CallBackHandler = fn;
            if (ctx != NULL)
            {
                CallBackContext = ctx;
            }
        }
    }

    void SetCallBackContext(void *ctx)
    {
        if (ctx != NULL)
        {
            CallBackContext = ctx;
        }
    }

    void *GetCallBackContext()
    {
        return CallBackContext;
    }

    void SetProtectedDriveMask(DWORD dwDriveMask)
    {
        m_dwProtectedDriveMask |= dwDriveMask;
    }

    void ClearProtectedDriveMask(DWORD dwDriveMask)
    {
        m_dwProtectedDriveMask &= ~dwDriveMask;
    }

    void ResetProtectedDriveMask(DWORD dwDriveMask)
    {
        m_dwProtectedDriveMask = 0;
    }

    DWORD GetProtectedDriveMask()
    {
        return m_dwProtectedDriveMask;
    }

    bool GetApplicationConsistencyState();
    
    void SetApplicationConsistencyState(bool bTagsGeneratedStatus)
    {
        bSentTagsSuccessfully = bTagsGeneratedStatus;
    }

    //HRESULT Prepare();
    HRESULT Prepare(CLIRequest_t CmdRequest);

    HRESULT Quiesce();

    HRESULT Resume();

    HRESULT Cancel();

    HRESULT Initialize()
    {
        bCoInitializeSucceeded = true;
        return S_OK;
    };

    void UnInitialize()
    {
        if (bCoInitializeSucceeded)
        {
            //CoUninitialize();
            bCoInitializeSucceeded = false;
        }
        m_ACSRequest.vVolumes.clear();
        m_ACSRequest.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints.clear();
        m_ACSRequest.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint = 0;
        m_ACSRequest.vApplyTagToTheseVolumes.clear();
    }

    HRESULT EstablishRemoteConnection (u_short port, const char *host);
    HRESULT SendTagsToRemoteServer(ACSRequest_t &Request);
    HRESULT SendRevocationTagsToRemoteServer(ACSRequest_t &Request);
    HRESULT SendTagsToRemoteServeViaConfigurator(ACSRequest_t &Request,bool bRevocationTag);
    HRESULT VssAppConsistencyProvider::PrepareForRemoteServer();

    void CloseRemoteConnection()
    {
        closesocket(ClientSocket);
    }

    HRESULT SendTagsToDriver();
    HRESULT SendTagsToDiskDriver();
    HRESULT SendRevocationTagsToDriver();
    #if (_WIN32_WINNT > 0x500)
        HRESULT HandleInMageVssProvider();
        RPC_STATUS StopRpcServer();
        HRESULT IsItRemoteTagIssue(USHORT uRemoteSend);
        HRESULT IsItSyncTagIssue(USHORT uSyncTag);
        HRESULT EstablishRemoteConInfoForInMageVssProvider(USHORT uPort,const unsigned char* serverIp);
        HRESULT StoreRemoteTagsBuffer(unsigned char* TagDataBuffer,ULONG ulTagBufferLen);
        HRESULT StoreLocalTagsBuffer(unsigned char* TagDataBuffer, 
                                     ULONG ulTagBufferLen,
                                     ULONG ulOutBufLen);
        HRESULT IsTagIssuedSuccessfullyDuringCommitTime(unsigned long* ulTagSent);	
        HRESULT CreateInMageVssProviderRegEntries();
    #endif

private:

    inline bool IsInitialized()
    {
        return (bCoInitializeSucceeded || bIsCoInitializedByCaller) ;
    }


    void DeleteWriter()
    {
        if (vwriter != NULL)
        {
            delete vwriter;
            vwriter = NULL;
        }
    }

    void DeleteRequestor()
    {
        if (m_vssRequestorPtr.get() != NULL)
        {
            (void)m_vssRequestorPtr->CleanupSnapShots();
            m_vssRequestorPtr.reset();
        }
    }
    HRESULT ProcessExposingSnapshots();		
private:

    bool bCoInitializeSucceeded;
    bool bIsCoInitializedByCaller;
    bool m_bDoFullBackup;

    bool bSentTagsSuccessfully;

    InMageVssWriter      *vwriter;
    DWORD m_dwProtectedDriveMask;
    void (*CallBackHandler)(void *);
    void *CallBackContext;
    vector<VssAppInfo_t>   AppInfo;
    ACSRequest_t  m_ACSRequest;
    SOCKET ClientSocket;

    //InMageVssProvider related Registry entries
    HKEY hInMageRegKey;
    HKEY hInMageRemoteSubKey;
    HKEY hInMageLocalSubKey;

    HANDLE m_hVssProviderCallbackThread;
    InMageVssRequestorPtr   m_vssRequestorPtr;
};
#endif

#define VACP_SERVER_RESPONSE_STRING_LEN    1024

typedef struct _VACP_SERVER_RESPONSE_ {
        
    int iResult; // return value of server success or failure
    int val;	// Success or Failuew code
    char str[VACP_SERVER_RESPONSE_STRING_LEN];  //string related to val
        
}VACP_SERVER_RESPONSE;

void usage();
HRESULT ParseCommandLineArgs(int argc, _TCHAR* argv[], CLIRequest_t &CmdRequest);
HRESULT Validatecommandline(const CLIRequest_t &CmdRequest);

HRESULT InitAgentSettings(_TCHAR* argv[], CLIRequest_t &CmdRequest);
HRESULT ValidateAppConsistencySettings(CLIRequest_t &CmdRequest);
HRESULT ValidateMultiVmConsistencySettings(const CLIRequest_t &CmdRequest);

void GenericCallBackHandler(void *context);
bool DriveLetterToGUID(char DriveLetter, WCHAR *VolumeGUID, ULONG ulBufLen);
HRESULT SendTagsToDriver(ACSRequest_t &Request);
HRESULT SendRevocationTagsToDriver(ACSRequest_t &Request);

bool SkipChkDriverMode(ACSRequest_t& acsRequest);
int ReadVACPRegistryValue();
bool IsVolumeAlreadyExists(vector<std::string>& targetVolumeList,const char* addVolume );
bool IsServerDeviceIDAlreadyExists(vector<ServerDeviceID_t*>& vServerDeviceID_t, const char* pServerDeviceID);
int vacpserver(int argc, _TCHAR* argv[]);
void PrintTagStatusOutputData(PTAG_VOLUME_STATUS_OUTPUT_DATA StatusOutputData,vector<string> &vVolumes);
HRESULT SendTagsToDiskDriver(ACSRequest_t &Request);

HRESULT GetInMageVssProviderInstance();
HRESULT RegisterInMageVssProvider();
HRESULT UnregisterInMageVssProvider();
HRESULT StopLongRunningInMageVssProvider(ULONG ulMaxWaitTimeInSec);

class Consistency
{
public:
    Consistency(const CLIRequest_t& CmdRequest, const std::string& strType, const TAG_TYPE TagType) :
        m_CmdRequest(CmdRequest),
        m_strType(strType),
        m_TagType(TagType),
        m_bDistributedVacp(false),
        m_interval(0),
        m_status(CONSISTENCY_STATUS_INITIALIZED),
        m_pCommunicator(NULL),
        m_bInternallyRescheduled(false),
        m_lastRunErrorCode(0) 
    {
        m_CmdRequest.TagType = TagType;
    }

    virtual HRESULT DiscoverDisksAndVolumes();

    virtual void InitializeCommunicator();
    virtual void StartCommunicator();
    virtual void DistributedTagGuidPhase();
    virtual HRESULT PreparePhase();
    virtual void PrepareAckPhase();
    virtual void QuiescePhase();
    virtual void QuiesceAckPhase();
    virtual void TagPhase();
    virtual void TagAckPhase();
    virtual void ResumePhase();
    virtual void ResumeAckPhase();
    virtual HRESULT CommitRevokeTags(ACSRequest_t &Request);

    void Conclude(HRESULT hr);
    HRESULT GenerateDrivesMappingFile(std::string &errmsg);
    
    void FacilitateDistributedConsistency();
    void ExitDistributedConsistencyMode();

    HRESULT CreateContextGuid();

    void InitSchedule();
    void SetSchedule();
    void SetNextSchedule();
    steady_clock::time_point GetSchedule(bool bSignalQuit);

    uint32_t GetConsistencyInterval() { return m_interval; }

    void SetStatus(uint32_t& status) { m_status = status; }

    uint32_t GetStatus() { return m_status; }

    virtual void Process() = 0;
    virtual HRESULT SendTagsToDriver(ACSRequest_t &Request);
    virtual HRESULT SendTagsToDiskDriver(ACSRequest_t &Request);
    virtual HRESULT SendRevocationTagsToDriver(ACSRequest_t &Request);
#if (_WIN32_WINNT > 0x500)
    HRESULT SendRemoteTagsBufferToInMageVssProvider(ACSRequest_t &Request);
    HRESULT CheckAndValidateDriverMode(ACSRequest_t Request);
    HRESULT CheckProtectionState(ACSRequest_t& Request);
    HRESULT SendLocalTagsBufferToInMageVssProvider(ACSRequest_t &Request);
#endif

    virtual void AddToVacpFailMsgs(const int &failureModule, const std::string &errorMsg, const unsigned long &errorCode)
    {
        m_VacpFailMsgsList.push_back(FailMsg(failureModule, errorMsg, errorCode));
    }

    virtual void AddToMultivmFailMsgs(const int &failureModule, const std::string &errorMsg, const unsigned long &errorCode)
    {
        m_MultivmFailMsgsList.push_back(FailMsg(failureModule, errorMsg, errorCode));
    }

    virtual void UpdateStartTimeTelInfo();

    virtual void IRModeStamp();

    virtual ~Consistency(){}

public:
    const std::string       m_strType;
    std::string             m_strContextGuid;
    CLIRequest_t            m_CmdRequest;
    ACSRequest_t            m_AcsRequest;
    
    const TAG_TYPE          m_TagType;
    bool            m_bDistributedVacp;

    uint32_t        m_status;
    HRESULT         m_DistributedProtocolStatus;

    HRESULT         m_DistributedProtocolState;

    CommunicatorPtr         m_pCommunicator;
    CommunicatorTimeouts    m_CommTimeouts;
    
    FailMsgList             m_VacpFailMsgsList;
    FailMsgList             m_MultivmFailMsgsList;
    VacpLastError           m_vacpLastError;

    boost::chrono::steady_clock::time_point m_exitTime;

    uint32_t                m_interval;
    boost::chrono::steady_clock::time_point m_prevRunTime;

    PerfCounter             m_perfCounter;

    TagTelemetryInfo        m_tagTelInfo;

    VacpErrorInfo           m_vacpErrorInfo;

    bool                    m_bInternallyRescheduled;

    // this should be retained across the multiple runs
    // in parallel mode
    uint32_t                m_lastRunErrorCode;


};

class CrashConsistency : public Consistency
{
public:
    static CrashConsistency& CreateInstance(const CLIRequest_t &CmdRequest, bool IoBarrierRequired);
    static CrashConsistency& Get();
    virtual void Process();
    virtual HRESULT CreateIoBarrier(const ACSRequest_t &Request);
    HRESULT ReleaseIoBarrier();
    virtual ~CrashConsistency(){}

private:
    const bool m_IoBarrierRequired;
    static CrashConsistency* m_theCrashC;
    static boost::mutex m_theCrashCLock;

private:
    CrashConsistency(const CLIRequest_t &CmdRequest, bool IoBarrierRequired) :
        Consistency(CmdRequest, ConsistencyTypeCrash, CRASH),
        m_IoBarrierRequired(IoBarrierRequired) {}
};

class AppConsistency : public Consistency
{
public:
    static AppConsistency& CreateInstance(const CLIRequest_t &CmdRequest);
    static AppConsistency& Get();
    
    virtual void Process();
    virtual ~AppConsistency(){ CleanupVssProvider(); }
    void CleanupVssProvider();

private:
    static AppConsistency* m_theAppC;
    static boost::mutex m_theAppCLock;

private:
    AppConsistency(const CLIRequest_t &CmdRequest) :
        Consistency(CmdRequest, ConsistencyTypeApp, TAG_TYPE::APP) {}
};

class BaselineConsistency : public Consistency
{
public:
    static BaselineConsistency& CreateInstance(const CLIRequest_t &CmdRequest);
    static BaselineConsistency& Get();

    virtual void Process();
    virtual ~BaselineConsistency(){}

private:
    static BaselineConsistency* m_theBaseline;
    static boost::mutex m_theBaselineLock;

private:
    BaselineConsistency(const CLIRequest_t &CmdRequest) :
        Consistency(CmdRequest, ConsistencyTypeBaseline, TAG_TYPE::BASELINE) {}

};

void AddToVacpFailMsgs(const int &failureModule, const std::string &errorMsg, const unsigned long &errorCode, const TAG_TYPE tagType);

VacpLastError& GetVacpLastError(const TAG_TYPE tagType);

void ExitVacp(HRESULT hr, bool bExitWithThisValue);

extern std::set<std::string>        gExcludedVolumes;
#endif /* _INMAGE_VSS_APP_CONSISTENCY_PROVIDER_H */
