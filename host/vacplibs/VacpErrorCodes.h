#ifndef VACP_ERROR_CODES_H
#define VACP_ERROR_CODES_H

#include <list>

// as most of the Windows VACP code uses the HRESULT as return type
// need the prefix for the FAILED(hr) checks.
#ifdef _WIN32
#define VACP_ERROR_HRESULT_PREFIX   0x80000000
#else
#define VACP_ERROR_HRESULT_PREFIX   0x00000000
#endif

//  On Linux the VACP is spawned use AppCommand, which forks /bin/sh 
// so the exit code received by AppCommand is between 0 to 255
#define VACP_MODULE_COMMON_BASE_ERROR_CODE      130
#define VACP_MODULE_WIN_BASE_ERROR_CODE         170
#define VACP_MODULE_LIN_BASE_ERROR_CODE         190
#define VACP_MODULE_MULTIVM_BASE_ERROR_CODE     200

#define VACP_MODULE_INFO                        220
#define VACP_MODULE_FAILURE_CODE                221        
#define VACP_PARSE_COMMAND_LINE_ARGS_FAILED     222
#define VACP_E_PREPARE_PHASE_FAILED             223
#define VACP_E_QUIESCE_PHASE_FAILED             224
#define VACP_E_RESUME_PHASE_FAILED              225
#define VACP_E_COMMIT_REVOKE_TAG_PHASE_FAILED   226
#define VACP_E_COMMIT_SEND_TAG_PHASE_FAILED     227
#define VACP_MULTIVM_FAILED                     228

enum VacpCommonErrorCodes {
    VACP_UNKNOWN_ERROR = VACP_ERROR_HRESULT_PREFIX + VACP_MODULE_COMMON_BASE_ERROR_CODE,
    VACP_INVALID_ARGUMENT,
    VACP_NO_PROTECTED_VOLUMES,
    VACP_DRIVER_OPEN_FAILED,
    VACP_INVALID_DRIVER_VERSION,
    VACP_MEMORY_ALLOC_FAILURE,
    VACP_DISK_DISCOVERY_FAILED,
    VACP_NO_DISKS_DISCOVERED,
    VACP_DISK_NOT_IN_DATAMODE,
    VACP_INSERT_TAG_FAILED,
    VACP_BARRIER_HOLD_FAILED,
    VACP_BARRIER_RELEASE_FAILED,
    VACP_COMMIT_TAG_TIMEOUT,
    VACP_DRIVER_NOT_INSERTED_TAG_ON_ALL_DISKS,
    VACP_IP_ADDRESS_DISCOVERY_FAILED,
    VACP_PRECHECK_FAILED,
    VACP_HOSTNAME_DISCOVERY_FAILED,
    VACP_CONF_PARAMS_NOT_SUPPLIED,
    VACP_ONE_OR_MORE_DISK_IN_IR, 
    VACP_SHARED_DISK_CLUSTER_INFO_FETCH_FAILED,
    VACP_SHARED_DISK_CLUSTER_HOST_MAPPING_FETCH_FAILED,
    VACP_SHARED_DISK_CURRENT_NODE_EVICTED,
    VACP_SHARED_DISK_CLUSTER_UPDATED
};

enum VacpWinErrorCodes {
    VACP_WINSOCK_INIT_FAILED = VACP_ERROR_HRESULT_PREFIX + VACP_MODULE_WIN_BASE_ERROR_CODE,
    VACP_CREATE_EVENT_FAILED,
    VACP_COM_INIT_FAILED,
    VACP_WMI_INIT_FAILED,
    VACP_WMI_DISK_QUERY_FAILED,
    VACP_VSS_PROVIDER_NOT_FOUND,
    VACP_VSS_WRITER_BAD_STATE,
    VACP_GET_LOGICAL_DRIVES_FAILED,
    VACP_GET_INSTALLPATH_FAILED
};


enum VacpLinuxErrorCodes {
    VACP_APP_FREEZE_FAILED = VACP_ERROR_HRESULT_PREFIX + VACP_MODULE_LIN_BASE_ERROR_CODE,
    VACP_FS_FREEZE_FAILED,
    VACP_FS_THAW_FAILED,
    VACP_APP_THAW_FAILED,
    VACP_READ_CONFIG_FAILED,
    VACP_APP_CONFIG_INIT_FAILED
};


enum VacpMultiVmErrorCodes {
    VACP_MULTIVM_LOCAL_IP_NOT_FOUND = VACP_ERROR_HRESULT_PREFIX + VACP_MODULE_MULTIVM_BASE_ERROR_CODE,
    VACP_MULTIVM_SERVER_CREATE_FAILED,
    VACP_MULTIVM_CLIENT_CONNECT_FAILED,
    VACP_MULTIVM_CLIENT_PREPARE_WAIT_TIMEOUT,
    VACP_MULTIVM_SERVER_PREPARE_ACK_TIMEOUT,
    VACP_MULTIVM_CLIENT_QUIESCE_WAIT_TIMEOUT,
    VACP_MULTIVM_SERVER_QUIESCE_ACK_TIMEOUT,
    VACP_MULTIVM_CLIENT_TAG_WAIT_TIMEOUT,
    VACP_MULTIVM_SERVER_TAG_ACK_TIMEOUT,
    VACP_MULTIVM_CLIENT_RESUME_WAIT_TIMEOUT,
    VACP_MULTIVM_SERVER_RESUME_ACK_TIMEOUT,
};

#define FAIL_MSG_DELM "#"

#define VACP_FAIL_MSG_DELM "|^|"
#define MULTIVM_FAIL_MSG_DELM "|~|"

const std::string vacpFailMsgsTok = "vacpError:";
const std::string multivmFailMsgsTok = "multiVmError:";
const std::string NotConnectedNodes = "Not connected nodes:";
const std::string ExcludedNodes = "Excluded nodes:";

const std::string HostKey = "Host:";
const std::string SearchEndKey = ")";
const std::string CoordNodeKey = "Coord node:";
const std::string NonDataModeError = "Change tracking not in data mode";


struct FailMsg
{
    explicit FailMsg(const int &failureModule, const std::string &errorMsg, const long &errorCode) :
                        m_failureModule(failureModule), m_errorMsg(errorMsg), m_errorCode(errorCode){}

    unsigned long  m_failureModule;
    std::string m_errorMsg;
    unsigned long m_errorCode;

    std::string getMsg() const
    {
        std::stringstream ss;
        ss << m_failureModule;
        ss << FAIL_MSG_DELM;
        ss << m_errorMsg;
        ss << FAIL_MSG_DELM;
        ss << m_errorCode;
        return ss.str();
    }
};

typedef std::list<FailMsg> FailMsgList;

struct VacpLastError
{
    VacpLastError() : m_errorCode(0) {}
    std::string m_errMsg;
    unsigned long m_errorCode;
};

#endif
