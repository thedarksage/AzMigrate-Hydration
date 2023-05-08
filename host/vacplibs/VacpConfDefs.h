#ifndef _VACP_CONF_DEFS_H
#define _VACP_CONF_DEFS_H

#define VACP_LOG_FILE_NAME                          "vacp.log";

#define VACP_CONF_FILE_NAME                         "vacp.conf"
#define VACP_CONF_PARAM_DRAIN_BARRIER_TIMEOUT       "DrainBarrierTimeout"
#define VACP_CONF_PARAM_TAG_COMMIT_MAX_TIMEOUT      "TagCommitMaxTimeOut"
#define VACP_CONF_PARAM_IO_BARRIER_TIMEOUT          "IoBarrierTimeout"
#define VACP_CONF_PARAM_APP_CONSISTENCY_INTERVAL    "AppInterval"
#define VACP_CONF_PARAM_CRASH_CONSISTENCY_INTERVAL  "CrashInterval"
#define VACP_CONF_PARAM_EXIT_TIME                   "ExitTime"
#define VACP_CONF_PARAM_REMOTE_SCHEDULE_INTERVAL    "RemoteSchedInterval"
#define VACP_CONF_PARAM_MASTER_NODE_FINGERPRINT     "MasterNodeFingerPrint"
#define VACP_CONF_PARAM_RCM_SETTINGS_PATH           "RcmSettingsPath"
#define VACP_CONF_PARAM_PROXY_SETTINGS_PATH         "ProxySettingsPath"
#define VACP_CONF_PARAM_DISKLIST                    "DiskList"
#define VACP_CONF_EXCLUDED_VOLUMES                  "ExcludeVolumes"
#define VACP_CLUSTER_HOSTID_NODE_MAPPING		    "ClusterNodeHostIdMappings"

#define VACP_CONF_PARAM_APPCONSISTENCY_SERVER_RESUMEACK_WAITTIME    "AppConsistencyServerResumeAckWaitTime"
#define VACP_SERVER_RESUMEACK_TIMEDELTA                             (30 * 1000) // in ms, 30 sec

#endif