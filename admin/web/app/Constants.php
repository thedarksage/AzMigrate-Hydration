<?php

$BASE_DIR_FILE          = array ('./base_dir', '../base_dir');
$AMETHYST_CONF          = 'etc/amethyst.conf';
$INMAGE_VERSION_FILE    = 'etc/version';
$VOLSYNC_COUNT          = 24;

if(preg_match ('/Linux/i', php_uname()))
{
$REPLICATION_ROOT="/home/svsystems/";
$AUDIT_LOG_DIRECTORY = "$REPLICATION_ROOT/var/";
$LOG_PATH = $REPLICATION_ROOT."/var";
$WEB_PATH = $REPLICATION_ROOT."/admin/web";
$SCOUT_API_PATH = $REPLICATION_ROOT."/admin/web/ScoutAPI";
$GRAPH_FILE_LOC = "$REPLICATION_ROOT/pm/SystemMonitor/healthgraph.pl";
$PHP_ERR_LOG="$REPLICATION_ROOT"."phperrlog.txt";
$ORIGINAL_REPLICATION_ROOT = $REPLICATION_ROOT;
$PHPDEBUG_FILE = "$REPLICATION_ROOT/var/phpdebug.txt";
$STUB_ERROR_LOG = "$REPLICATION_ROOT/var/stubError.txt";
$CLUSTER_LOG = "$REPLICATION_ROOT/var/clusterlog.txt";
$HOST_LOG_DIRECTORY = "$REPLICATION_ROOT/var/hosts";
$CXAPI_LOG = "$REPLICATION_ROOT/var/cxapi.log";
$API_MANAGER_LOG = "$REPLICATION_ROOT/var/apimanager.log";
$ASRAPI_EVENTS_LOG = "$REPLICATION_ROOT/var/asrapi_ux_events.log";
$ASRAPI_REPORT_LOG = "$REPLICATION_ROOT/var/asrapi_reporting.log";
$ASRAPI_STATUS_LOG = "$REPLICATION_ROOT/var/asrapi_status.log";
$API_ERROR_LOG = "$REPLICATION_ROOT/var/api_error.log";
$CHMOD = "chmod";
$SLASH = "/";
$FIND = "/usr/bin/find";
$XARGS = "/usr/bin/xargs";
$BACKUP_DIR = "/tmp";
$RESTORE_DIR = "/tmp/amethyst_restore";
$MYSQL = "/usr/bin/mysql";
$MYSQL_DUMP = "/usr/bin/mysqldump";
$CP = "/bin/cp";
$MV = "/bin/mv";
$TAR = "/bin/tar";
#5600-Temporary file to capture the unmarshul messages.
$UNMARSHAL_FILE = "/home/svsystems/var/unmarshal.txt";

$REQUESTS_FILE = "/home/svsystems/var/requests.csv";
#11774 fix
$AMETHYST_PATH_CONF = "etc/amethyst.conf";
$LOG_ROOT = "$REPLICATION_ROOT/var/";
$PERL_EXECUTABLE_PATH = "perl";

// OBJECT-TYPE
$TRAP_AMETHYSTGUID                     = "trapAmethystGuid";
$TRAP_HOSTGUID                         = "trapHostGuid";
$TRAP_VOLUMEGUID                       = "trapVolumeGuid";
$TRAP_AGENT_ERROR_MESSAGE              = "trapAgentErrorMessage";
$TRAP_AGENT_LOGGED_ERROR_LEVEL	       = "trapAgentLoggedErrorLevel";
$TRAP_AGENT_LOGGED_ERROR_MESSAGE_TIME  = "trapAgentLoggedErrorMessageTime";	
$TRAP_FILEREP_CONFIG                   = "trapFilerepConfig";
$TRAP_CS_HOSTNAME                      = "trapCSHostname";
$TRAP_PS_HOSTNAME                      = "trapPSHostname";
$TRAP_SEVERITY                         = "trapSeverity";
$TRAP_SOURCE_HOSTNAME                  = "trapSourceHostName";
$TRAP_SOURCE_DEVICENAME                = "trapSourceDeviceName";
$TRAP_TARGET_HOSTNAME                  = "trapTargetHostName";
$TRAP_TARGET_DEVICENAME                = "trapTargetDeviceName";
$TRAP_RETENTION_DEVICENAME             = "trapRetentionDeviceName";
$TRAP_AFFTECTED_SYSTEM_HOSTNAME        = "trapAffectedSystemHostName";

# TRAP-TYPE
$TRAP_VX_NODE_DOWN                               = "trapVxAgentDown";
$TRAP_FX_NODE_DOWN                               = "trapFxAgentDown";
$TRAP_PS_NODE_DOWN                               = "trapPSNodeDown";
$TRAP_APP_NODE_DOWN                              = "trapAppAgentDown";
$TRAP_SECONDARY_STORAGE_UTILIZATION_DANGER_LEVEL = "trapSecondaryStorageUtilizationDangerLevel";
$TRAP_VX_RPOSLA_THRESHOLD_EXCEEDED               = "trapVXRPOSLAThresholdExceeded";
$TRAP_FX_RPOSLA_THRESHOLD_EXCEEDED               = "trapFXRPOSLAThresholdExceeded";
$TRAP_PS_RPOSLA_THRESHOLD_EXCEEDED               = "trapPSRPOSLAThresholdExceeded";
$TRAP_FILEREP_PROGRESS_THRESHOLD_EXCEEDED        = "trapFXJobError";
$TRAP_BITMAP_MODE_CHANGE                         = "trapbitmapmodechange";
$TRAP_SENTINEL_RESYNC_REQUIRED                   = "trapSentinelResyncRequired";
$TRAP_AGENT_LOGGED_ERROR_MESSAGE                 = "trapAgentLoggedErrorMessage";
$TRAP_DISK_SPACE_THRESHOLD_ON_SWITCH_BP_EXCEEDED = "trapDiskSpaceThresholdOnSwitchBPExceeded";
$TRAP_PROCESS_SERVER_UNINSTALLATION 			 = "ProcessServerUnInstallation";
$TRAP_BANDWIDTH_SHAPING                          = "BandwidthShaping";
$TRAP_VERSION_MISMATCH							 = "trapVersionMismatch";
$TRAP_CX_NODE_FAILOVER							 = "CXNodeFailover";
$TRAP_APPLICATION_PROTECTION_ALERTS				 = "ApplicationProtectionAlerts";
$TRAP_FX_JOB_FAILED_WITH_ERROR 					 = "trapFXJobFailedWithError";
$TRAP_FX_PROTECTION_JOB_FAILED_WITH_ERROR        = "trapFXProtectionJobFailedWithError";
$TRAP_FX_RECOVERY_JOB_FAILED_WITH_ERROR          = "trapFXRecoveryJobFailedWithError";
$TRAP_FX_CONSISTENCY_JOB_FAILED_WITH_ERROR       = "trapFXConsistencyJobFailedWithError";
$TRAP_FX_PROTECTION_JOB_SUCCESSFUL               = "trapFXProtectionJobSuccessful";
$TRAP_FX_FAILOVER_JOB_SUCCESSFUL                 = "trapFXFailoverJobSuccessful";
$TRAP_PROCESS_SERVER_FAILOVER 					 = "processServerFailover";
$TRAP_VOLUME_RESIZE 							 = "volumeReSize";
$TRAP_VX_PAIR_INITIAL_DIFFSYNC 				   	 = "trapVXPairInitialDiffSync";
$TRAP_INSUFFICIENT_RETENTION_SPACE 				 = "insufficientRetentionSpace";
$TRAP_MOVE_RETENTION_LOG						 = "moveRetentionLog";
$WEB_CLI_PATH = "$REPLICATION_ROOT/var/cli/";

}
else if(preg_match ('/Windows/i', php_uname()))
{
$install_dir_value = read_installation_directory();
$CS_INSTALLATION_PATH = $install_dir_value["INSTALLATION_PATH"];	
$REPLICATION_ROOT       = $CS_INSTALLATION_PATH."\\home\\svsystems\\";
$AUDIT_LOG_DIRECTORY = "$REPLICATION_ROOT"."var\\";
$LOG_PATH = $REPLICATION_ROOT."var";
$WEB_PATH = $REPLICATION_ROOT."admin\\web";
$SCOUT_API_PATH = $REPLICATION_ROOT."admin\\web\\ScoutAPI";
$GRAPH_FILE_LOC = "$REPLICATION_ROOT\\pm\\SystemMonitor\\healthgraph.pl";
$ORIGINAL_REPLICATION_ROOT = "/home/svsystems";
$PHPDEBUG_FILE = $REPLICATION_ROOT . "var\\phpdebug.txt";
$STUB_ERROR_LOG = $REPLICATION_ROOT."var\\stubError.txt";
$CLUSTER_LOG = $REPLICATION_ROOT . "var\\clusterlog.txt";
$PHP_ERR_LOG=$REPLICATION_ROOT . "phperrlog.txt";
$HOST_LOG_DIRECTORY = $REPLICATION_ROOT."var\\hosts";
$EXCHANGEDEBUG_FILE = $REPLICATION_ROOT . "var\\exchangediscoverylog.txt";
$SQLDEBUG_FILE = $REPLICATION_ROOT . "var\\sqldiscoverylog.txt";
$FILESERVERDEBUG_FILE = $REPLICATION_ROOT . "var\\fileserverdiscoverylog.txt";
$CXAPI_LOG = "$REPLICATION_ROOT"."var\\cxapi.log";
$API_MANAGER_LOG = "$REPLICATION_ROOT"."var\\apimanager.log";
$ASRAPI_EVENTS_LOG = "$REPLICATION_ROOT"."var\\asrapi_ux_events.log";
$ASRAPI_REPORT_LOG = "$REPLICATION_ROOT"."var\\asrapi_reporting.log";
$ASRAPI_STATUS_LOG = "$REPLICATION_ROOT"."var\\asrapi_status.log";
$API_ERROR_LOG = "$REPLICATION_ROOT"."var\\api_error.log";
$CHMOD = "chmod";
$SLASH = "\\";
$FIND = "find";
$XARGS = "xargs";
$BACKUP_DIR = $REPLICATION_ROOT;
$RESTORE_DIR = $REPLICATION_ROOT."amethyst_restore";
$MYSQL = "\"c:\\Program Files (x86)\\MySQL\\MySQL Server 5.1\\bin\\mysql\"";
$MYSQL_DUMP = "\"c:\\Program Files (x86)\\MySQL\\MySQL Server 5.1\\bin\\mysqldump\"";
$CP = "cp";
$MV = "mv";
$TAR = "tar";
#5600-Temporary file to capture the unmarshul messages.
$UNMARSHAL_FILE = $REPLICATION_ROOT."var\\unmarshal.txt";

$REQUESTS_FILE = $REPLICATION_ROOT."var\\requests.csv";
#11774 fix
$AMETHYST_PATH_CONF = "\\etc\\amethyst.conf";
$LOG_ROOT = $REPLICATION_ROOT."var\\";
$TELEMETRY_ROOT = $LOG_PATH."\\cs_telemetry\\";
$CS_LOGS_ROOT = $LOG_PATH."\\cs_logs\\";
$APPINSIGHTS_RECORD_SEPARATOR = "RECORD SEPARATOR";
$PHP_SQL_ERROR = "php_sql_error.log";
$PERL_EXECUTABLE_PATH = "C:\\strawberry\\perl\\bin\\perl.exe";

// OBJECT-TYPE
$TRAP_AMETHYSTGUID                     = "1.3.6.1.4.17282.1.14.1"; 
$TRAP_HOSTGUID                         = "1.3.6.1.4.17282.1.14.5";
$TRAP_VOLUMEGUID                       = "1.3.6.1.4.17282.1.14.10";
$TRAP_AGENT_ERROR_MESSAGE              = "1.3.6.1.4.17282.1.14.50";
$TRAP_AGENT_LOGGED_ERROR_LEVEL	       = "1.3.6.1.4.17282.1.14.55";
$TRAP_AGENT_LOGGED_ERROR_MESSAGE_TIME  = "1.3.6.1.4.17282.1.14.60";
$TRAP_FILEREP_CONFIG                   = "1.3.6.1.4.17282.1.14.65";
$TRAP_CS_HOSTNAME                      = "1.3.6.1.4.17282.1.14.70";
$TRAP_PS_HOSTNAME                      = "1.3.6.1.4.17282.1.14.75";
$TRAP_SEVERITY                         = "1.3.6.1.4.17282.1.14.80";
$TRAP_SOURCE_HOSTNAME                  = "1.3.6.1.4.17282.1.14.85";
$TRAP_SOURCE_DEVICENAME                = "1.3.6.1.4.17282.1.14.90";
$TRAP_TARGET_HOSTNAME                  = "1.3.6.1.4.17282.1.14.95";
$TRAP_TARGET_DEVICENAME                = "1.3.6.1.4.17282.1.14.100";
$TRAP_RETENTION_DEVICENAME             = "1.3.6.1.4.17282.1.14.105";
$TRAP_AFFTECTED_SYSTEM_HOSTNAME        = "1.3.6.1.4.17282.1.14.110";

# TRAP-TYPE
$TRAP_VX_NODE_DOWN = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.240";
$TRAP_FX_NODE_DOWN = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.235";
$TRAP_PS_NODE_DOWN = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.245";
$TRAP_APP_NODE_DOWN = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.230";
$TRAP_SECONDARY_STORAGE_UTILIZATION_DANGER_LEVEL = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.260";
$TRAP_VX_RPOSLA_THRESHOLD_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.280";
$TRAP_FX_RPOSLA_THRESHOLD_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.285";
$TRAP_PS_RPOSLA_THRESHOLD_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.290";
$TRAP_FILEREP_PROGRESS_THRESHOLD_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.385";
$TRAP_BITMAP_MODE_CHANGE = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.435";
$TRAP_SENTINEL_RESYNC_REQUIRED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.215";
$TRAP_AGENT_LOGGED_ERROR_MESSAGE = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.340";
$TRAP_DISK_SPACE_THRESHOLD_ON_SWITCH_BP_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.460";
$TRAP_PROCESS_SERVER_UNINSTALLATION = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.380";
$TRAP_BANDWIDTH_SHAPING = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.375";
$TRAP_VERSION_MISMATCH	= "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.440";
$TRAP_CX_NODE_FAILOVER	= "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.370";
$TRAP_APPLICATION_PROTECTION_ALERTS = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.390";
$TRAP_FX_JOB_FAILED_WITH_ERROR = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.395";
$TRAP_FX_PROTECTION_JOB_FAILED_WITH_ERROR = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.405";
$TRAP_FX_RECOVERY_JOB_FAILED_WITH_ERROR = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.410";
$TRAP_FX_CONSISTENCY_JOB_FAILED_WITH_ERROR = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.415";
$TRAP_FX_PROTECTION_JOB_SUCCESSFUL = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.420";
$TRAP_FX_FAILOVER_JOB_SUCCESSFUL = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.425";
$TRAP_PROCESS_SERVER_FAILOVER = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.350";
$TRAP_VOLUME_RESIZE = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.250";
$TRAP_VX_PAIR_INITIAL_DIFFSYNC = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.430";
$TRAP_INSUFFICIENT_RETENTION_SPACE = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.270";
$TRAP_MOVE_RETENTION_LOG = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.255";
$WEB_CLI_PATH = $REPLICATION_ROOT."var\\cli\\";

}

$ABHAI_MIB = "Abhai-1-MIB-V1";

$SYNC_APP               = 'synchronous_application_protection';
$SYNC_NO_APP            = 'synchronous_replication_protection';
$ASYNC_APP              = 'asynchronous_application_protection';
$ASYNC_NO_APP           = 'asynchronous_replication_protection';
$SYNC_ASYNC_APP        	= 'synchronous_asynchronous_application_protection';
$SYNC_ASYNC_NO_APP      = 'synchronous_asynchronous_replication_protection';

$CX_MODE                = '';
$MAX_TMID               = 0;

$HOST_GUID              = '';
$DB_HOST                = 'localhost';
$DB_USER                = '';
$DB_PASSWORD            = '';
$DB_DATABASE_NAME       = '';
$SESSION_UPDATE         = '';
$CXPS_CONNECTTIMEOUT     = '45';
$CXPS_RESPONSETIMEOUT    = '120';
$INMAGE_PROFILER_HOST_ID= '5C1DAEF0-9386-44a5-B92A-31FE2C79236A';
$INMAGE_PROFILER_DEVICE_NAME = 'P';

$VIRTUAL_CREATE_PENDING	= 2000;
$VIRTUAL_CREATE_PROGRESS=2001;
$VIRTUAL_CREATE_SUCESS=2002;
$VIRTUAL_CREATE_FAIL=2003;
$VIRTUAL_DELETE_PENDING=2004;
$VIRTUAL_DELETE_PROGRESS=2005;
$VIRTUAL_DELETE_SUCESS=2006;
$VIRTUAL_DELETE_FAIL=2007;

$TARGET_PORT_ENABLE_PENDING=2500;
$TARGET_PORT_ENABLE_REQUIRED=2501;
$TARGET_PORT_ENABLE_SUCCESS=2502;

$MIRROR_CREATE_PENDING=3000;
$MIRROR_CREATE_PROGRESS=3001;
$MIRROR_CREATE_SUCESS=3002;
$MIRROR_CREATE_FAIL=3003;
$MIRROR_DELETE_PENDING=3004;
$MIRROR_DELETE_PROGRESS=3005;
$MIRROR_DELETE_SUCESS=3006;
$MIRROR_DELETE_FAIL=3007;

$VOLUME_SYNCHRONIZATION_PROGRESS=5000;

$FABRIC_COMPATIBILITY_NO = 430000;

$UEFI_UNSUPPORTED_TILL_VERSION = 9120000;

$PUSH_USING_VSPHERE_API_VERSION = 9130000;
// the following are the various work states for ITL discovery and protection
// errors are reported as a value < 0 and all the tables could have an error
// value. the perl equivalents are in server/tm/Fabric/Constants.pm
// if you add a new state here add it there as well so that they are always in sync
// even if the perl code doesn't actually use it. That way when new perl ones are
// added it won't use a value that is in here. the table.column_name that can have
// the state is listed in the comment above the state variable name

$PARALLEL_CONSISTENCY_SUPPORT_VERSION = 9200000;

$LINUX_CONSISTENCY_COMMAND_OPTIONS_VERSION = 9470000;

// sanITLNexus.state
$NO_WORK = 0;

// srcLogicalVolumeDestLogicalVolume.lun_state, logicalVolumes.lun_state
$START_PROTECTION_PENDING = 1;

// srcLogicalVolumeDestLogicalVolume.lun_state, logicalVolumes.lun_state
// frBindingNexus.stat
$PROTECTED = 2;

// srcLogicalVolumeDestLogicalVolume.lun_state, logicalVolumes.lun_state
$STOP_PROTECTION_PENDING = 3;

// sanITLNexus.state
$DELETE_PENDING = 4;

// frBindingNexus.state
$CREATE_FR_BINDING_PENDING = 5;

// frBindingNexus.state
$CREATE_FR_BINDING_DONE = 6;

// frBindingNexus.state
$CREATE_AT_LUN_PENDING = 7;

// frBindingNexus.state
$CREATE_AT_LUN_DONE = 8;

// frBindingNexus.state
$DISCOVER_BINDING_DEVICE_PENDING = 9;

// frBindingNexus.state
$DISCOVER_BINDING_DEVICE_DONE = 10;

// frBindingNexus.state
$DELETE_FR_BINDING_PENDING = 11;

// frBindingNexus.state
$DELETE_FR_BINDING_DONE = 12;

# frBindingNexus.state
$DELETE_AT_LUN_PENDING = 13;

# frBindingNexus.state
$DELETE_AT_LUN_DONE = 14;
// frBindingNexus.state
$DELETE_AT_LUN_BINDING_DEVICE_PENDING = 141;

// frBindingNexus.state
$DELETE_AT_LUN_BINDING_DEVICE_DONE = 142;

// sanITNExus.state, discoveryNexus.state
$DISCOVER_IT_LUNS_PENDING = 15;

// sanITNExus.state, discoveryNexus.state
$DISCOVER_IT_LUNS_DONE = 16;

// sanITLNexus.state
$NEW_ENTRY_PENDING = 17;

// frBindingNexus.state
$REBOOT_PENDING = 18;

// host.state
$UNINSTALL_PENDING = 19;

// resynnc notification frBindingNexus.state
$RESYNC_PENDING = 21;

// resynnc acknowledge frBindingNexus.state
$RESYNC_ACKED = 22;
//for host based replication status
$RELEASE_DRIVE_PENDING = 23;
$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING = 24;

// Replication CleanUP States
$LOG_CLEANUP_PENDING = 31;
$LOG_CLEANUP_COMPLETE = 32;
$LOG_CLEANUP_FAILED = -32;

$UNLOCK_PENDING = 33;
$UNLOCK_COMPLETE = 34;
$UNLOCK_FAILED = -34;

$CACHE_CLEANUP_PENDING = 35;
$CACHE_CLEANUP_COMPLETED = 36;
$CACHE_CLEANUP_FAILED = -36;

$VNSAP_CLEANUP_PENDING = 37;
$VNSAP_CLEANUP_COMPLETED = 38;
$VNSAP_CLEANUP_FAILED = -38;

$PENDING_FILES_CLEANUP_PENDING = 39;
$PENDING_FILES_CLEANUP_COMPLETED = 40;
$PENDING_FILES_CLEANUP_FAILED = -40;

$RELEASE_DRIVE_ON_TARGET_DELETE_DONE = 25;
$RELEASE_DRIVE_ON_DELETE_FAIL = 27;
$RELEASE_DRIVE_PENDING_OLD = 28;
$SOURCE_DELETE_PENDING = 29;
$SOURCE_DELETE_DONE = 30;
$PS_DELETE_DONE = 31;
//Paused Status 
$PAUSED_PENDING = 41;
$PAUSE_COMPLETED = 26;

$CREATE_SPLIT_MIRROR_PENDING = 53;
$CREATE_SPLIT_MIRROR_DONE = 54;
$DELETE_SPLIT_MIRROR_PENDING = 55;
$DELETE_SPLIT_MIRROR_DONE = 56;
$CREATE_SPLIT_MIRROR_FAILED = -54;
$DELETE_SPLIT_MIRROR_FAILED = -56;
$MIRROR_SETUP_PENDING_RESYNC_CLEARED = 57;
$PRISM_FABRIC = 1;
$DELETE_ENTRY_PENDING = 60;
$FLUSH_AND_HOLD_WRITES_PENDING = 71;
$FLUSH_AND_HOLD_WRITES = 72;
$FLUSH_AND_HOLD_RESUME_PENDING=73;


// srcLogicalVolumeDestLogicalVolume.lun_state
$NODE_REBOOT_OR_PATH_ADDITION_PENDING = 4;

$MIRROR_ERROR_UNKNOWN = -701;
$AT_LUN_DISCOVERY_FAILED = -702;
$AT_LUN_ID_FAILED = -703;
$AGENT_MEMORY_ERROR = -704;
$AT_LUN_DELETION_FAILED = -705;
$SOURCE_LUN_INVALID = -706;
$DRIVER_MEMORY_ALLOCATION_ERROR = -707;
$DRIVER_MEMORY_COPYIN_ERROR = -708;
$DRIVER_MEMORY_COPYOUT_ERROR = -709;
$AT_LUN_INVALID = -710;
$SOURCE_NAME_CHANGED_ERROR = -711;
$AT_LUN_NAME_CHANGED_ERROR = -712;
$MIRROR_STACKING_ERROR = -713;
$RESYNC_CLEAR_ERROR = -714;
$RESYNC_NOT_SET_ON_CLEAR_ERROR = -715;
$SOURCE_DEVICE_LIST_MISMATCH_ERROR = -716;
$AT_LUN_DEVICE_LIST_MISMATCH_ERROR = -717;
$SOURCE_DEVICE_SCSI_ID_ERROR = -718;
$AT_LUN_DEVICE_SCSI_ID_ERROR = -719;
$MIRROR_NOT_SETUP = -720;
$MIRROR_DELETE_IOCTL_FAILED = -721;
$MIRROR_NOT_SUPPORTED = -722;       
$SRCS_TYPE_MISMATCH = -723;
$SRC_IS_MULTIPATH_MISMATCH = -724;
$SRCS_VENDOR_MISMATCH = -725;
$SRC_NOT_REPORTED = -726;         
$SRC_CAPACITY_INVALID = -727;
$MIRROR_STATE_INVALID = -728;       
$AT_LUN_NUMBER_INVALID = -729;
$AT_LUN_NAME_INVALID = -730;      
$PI_PWWNS_INVALID = -731;
$AT_PWWNS_INVALID = -732;

//User action under which pause is performed
$PAUSE_BY_USER = 'user';
$PAUSE_BY_RETENTION = 'move retention';
$PAUSE_BY_INSUFFICIENT_SPACE ='insufficient retention space';
$PAUSE_BY_RESTART_RESYNC = "restart resync";

$RELEASE_DRIVE_ON_TARGET_DELETE_FAIL = -25;


// The following global variables are specific to HA fail-over
// srcLogicalVolumeDestLogicalVolume.lun_state

//failover processing is pending
$FAILOVER_PRE_PROCESSING_PENDING = 200;

// failover processing is done
$FAILOVER_PRE_PROCESSING_DONE = 201;

// Driver stale data clear pending
$FAILOVER_DRIVER_DATA_CLEANUP_PENDING = 202;

// Driver stale data clear done
$FAILOVER_DRIVER_DATA_CLEANUP_DONE = 203;

// CX stale resync/diffs file clear pending
$FAILOVER_CX_FILE_CLEANUP_PENDING = 204;

// CX stale resync/diffs file clear done
$FAILOVER_CX_FILE_CLEANUP_DONE = 205;

// failover processing is pending
$FAILOVER_RECONFIGURATION_PENDING = 206;

// failover processing is done
$FAILOVER_RECONFIGURATION_DONE = 207;

// target failover processing is pending
$FAILOVER_TARGET_RECONFIGURATION_PENDING = 212;


//CX Sizing Data in MB
$CX_80 = 307200; //300 GB 

$CX_200 = 716800; //700 GB 

$CX_400 = 1048576; //1 TB

// UI sets this for SA exception handling
$RESYNC_NEEDED = 3;
// Config API sets this after SA has acked (for UI)
$RESYNC_MUST_REQUIRED = 4;
// Config API sets this for AT exception handling
$RESYNC_IS_REQUIRED = 5;
// Config API sets this for PT exception handling
$RESYNC_MAY_REQUIRED = 6;
// Config API sets this after SA has acked (For PT)
$RESYNC_RECOMMENDED = 7;
// FS sets this when "resync track counter" is greater than threshold
$NO_RESYNC = 8;
$RESYNC_NEEDED_UI = 9;
$NO_COMMUNICATION_FROM_SWITCH = 10;

// No of retry for resync during SA exception
$MAX_RESYNC_COUNTER = 3;

// SA Exceptions
$SA_EXCEPTION_APPLIANCE_DISK_DOWN = 1;
$SA_EXCEPTION_APPLIANCE_DISK_UP = 0;
$SA_EXCEPTION_APPLIANCE_LUN_IO_ERROR = 0;
$SA_EXCEPTION_PHYSICAL_LUN_IO_ERROR = 1;
$SA_EXCEPTION_PHYSICAL_DISK_DOWN = 1;
$SA_EXCEPTION_PHYSICAL_DISK_UP = 0;
$SA_EXCEPTION_AT_PT_LUN_IO_ERROR = 2;
$SA_EXCEPTION_MIRROR_VOLUME_DISABLE = 3;
$EXCEPTION_SET = 1;
$EXCEPTION_RESET = 0;

// discoveryNexus.state
$DISCOVER_IT_LUNS_SCAN_PROGRESS = 8888;

// sanITNexus deleteFflag values
$SAN_ITNEXUS_CREATE_REQUESTED = 0;
$AN_ITNEXUS_DELETE_REQUESTED = 1;
$SAN_ITNEXUS_RESCAN_REQUESTED = 2;

// sanITNexus.rescanFlag values
$RESCAN_RESET = 0;
$RESCAN_SET = 1;

//error states from FA
$CREATE_AT_LUN_FAILURE= -2000;
$DELETE_AT_LUN_FAILURE = -2001;
$DISCOVER_BINDING_DEVICE_AI_NOT_FOUND = -2002;
$DISCOVER_BINDING_DEVICE_DEVICE_NOT_FOUND = -2003;
$DISCOVER_BINDING_DEVICE_OTHER_FAILURE = -2004;
$DELETE_AT_LUN_BINDING_DEVICE_FAILURE = -2005;

$FRAME_REDIRECT_MESSAGE_SENDING_FAILURE = -1000;
$FRAME_REDIRECT_INVALID_DATA = -1001;
$FRAME_REDIRECT_CREATE_AT_LUN_NOT_ACCESSIBLE = -1002; 
$FRAME_REDIRECT_CREATE_FR_BINDING_CREATE_FAILURE = -1003; 
$FRAME_REDIRECT_DELETE_FR_BINDING_FAILURE = -1004;
$RESYNC_ACKED_FAILED = -1005;

// SA Not Found
$MATCHING_SA_NOT_FOUND = -2;

// Discovery Failed
$DISCOVER_IT_LUNS_FAILED = -3;
$DISCOVER_ITL_FAILED_DONE_CLEANUP = -4;

//error states from ITL Protector
$APPLIANCE_INITIATORS_NOT_CONFIGURED = -5;
$APPLIANCE_TARGETS_NOT_CONFIGURED = -6;

# For VSNAP Purpose
$CREATE_EXPORT_LUN_PENDING = 2100;
$CREATE_EXPORT_LUN_DONE = 2101;

$SNAPSHOT_CREATION_PENDING = 2050;
$SNAPSHOT_CREATION_DONE = 2051;
$REMOVE_SCHEDULED_SNAPSHOT = 2052;

$CREATE_EXPORT_LUN_ID_FAILURE = -2102;
$CREATE_EXPORT_LUN_NUMBER_FAILURE = -2103;

$DELETE_EXPORT_LUN_PENDING = 2110;
$DELETE_EXPORT_LUN_DONE = 2111;

$DELETE_EXPORT_LUN_NUMBER_FAILURE = -2112;
$DELETE_EXPORT_LUN_ID_FAILURE = -2113;

//defining initial lun device name
$INITIAL_LUN_DEVICE_NAME = "/dev/mapper/";

#For ACG Creation and deletion
$CREATE_ACG_PENDING = 3100;
$CREATE_ACG_DONE = 3101;

$DELETE_ACG_PENDING = 3200;
$DELETE_ACG_DONE = 3201;

$CREATE_ACG_FAILUE = -3300;
$DELETE_ACG_FAILURE = -3301;
//used when ports are deleted from ACG group
$DELETE_ACG_PORTS = 4000;
//Used to Enable DB Sync
$DB_SYNC_ENABLED = 1;
//Used to Disable DB Sync
$DB_SYNC_DISABLED = 0;

// Fx DB Sync job application name for high availability 
$HA_DB_SYNC_JOB =  "HA_DB_SYNC";
//HighAvailability Node Roles

// Fx DB Sync job application name for Remote Recovery 
$RR_DB_SYNC_JOB =  "RR_DB_SYNC";

// MODES for BPM Allocation
$DEL_REP = 1;
$UNREG_VX = 2;
$PAIR_PS_FAILOVER = 3;
$PS_FAILOVER = 4;
$UNREG_PS_NIC= 5;
$UNREG_PS = 6;

//HighAvailability Node Roles
$ACTIVE_ROLE = "ACTIVE";
$PASSIVE_ROLE = "PASSIVE";

// LUN capacity change flags for sanLuns
$CAPACITY_CHANGE_RESET = 0;
$CAPACITY_CHANGE_REPORTED = 1;
$CAPACITY_CHANGE_RESCANNED = 2;
$CAPACITY_CHANGE_UPDATED = 3;
$CAPACITY_CHANGE_PROCESSING = 4;

$LUN_RESIZE_PRE_PROCESSING_PENDING = 208;
$LUN_RESIZE_PRE_PROCESSING_DONE = 209;
$LUN_RESIZE_RECONFIGURATION_PENDING = 210;
$LUN_RESIZE_RECONFIGURATION_DONE = 211;



//Snap shot Execution States
$READY_FOR_SNAPSHOT = 1;
$SNAPSHOT_STARTED = 2;
$SNAPSHOT_IN_PROGRESS = 3;
$SNAPSHOT_COMPLETED_AND_RECOVERY_STARTED = 6 ;
$SNAPSHOT_COMPLETED_AND_RECOVERY_IN_PROGRESS = 7 ;

//PS cleanup state - Fix for 10435
$RESTART_RESYNC_CLEANUP	= 42;

//Retention Tag application name mapping
//Oracle
$STREAM_REC_TYPE_ORACLE_TAG = 256;
//SystemFiles
$STREAM_REC_TYPE_SYSTEMFILES_TAG = 257;
//Registry
$STREAM_REC_TYPE_REGISTRY_TAG = 258;
//SQL
$STREAM_REC_TYPE_SQL_TAG = 259;
//EventLog
$STREAM_REC_TYPE_EVENTLOG_TAG = 260;
//WMI
$STREAM_REC_TYPE_WMI_DATA_TAG = 261;
//COM+REGDB
$STREAM_REC_TYPE_COM_REGDB_TAG = 262;
//FS
$STREAM_REC_TYPE_FS_TAG = 263;
//USERDEFINED
$STREAM_REC_TYPE_USERDEFINED_EVENT = 264;
//EXCHANGE
$STREAM_REC_TYPE_EXCHANGE_TAG = 265;
//IISMETABASE
$STREAM_REC_TYPE_IISMETABASE_TAG = 266;
//CA - Certificate Authority
$STREAM_REC_TYPE_CA_TAG = 267;
//AD - Acitve Directory
$STREAM_REC_TYPE_AD_TAG = 268;
//DHCP
$STREAM_REC_TYPE_DHCP_TAG = 269;
//BITS
$STREAM_REC_TYPE_BITS_TAG = 270;
//WINS Jet Writer 
$STREAM_REC_TYPE_WINS_TAG = 271;
//Removable Storage Manager 
$STREAM_REC_TYPE_RSM_TAG = 272;
//TermServLicensing
$STREAM_REC_TYPE_TSL_TAG = 273;
//FRS
$STREAM_REC_TYPE_FRS_TAG = 274;
//BootableState (Win XP)
$STREAM_REC_TYPE_BS_TAG = 275;
//Mircrosoft ServiceState (Win XP)
$STREAM_REC_TYPE_SS_TAG = 276;
//Cluster Service
$STREAM_REC_TYPE_CLUSTER_TAG = 277;
//SQL Server 2005
$STREAM_REC_TYPE_SQL2005_TAG  = 278;
//Exchange 2007- Exchange Information Store  
$STREAM_REC_TYPE_EXCHANGEIS_TAG = 279;
//Exchange 2007- Exchange Replication Service 
$STREAM_REC_TYPE_EXCHANGEREPL_TAG = 280;
//SQL Server 2008
$STREAM_REC_TYPE_SQL2008_TAG = 281;
//SharePoint 
$STREAM_REC_TYPE_SHAREPOINT_TAG = 282;
//Office Search
$STREAM_REC_TYPE_OSEARCH_TAG = 283;
//SharePoint Search
$STREAM_REC_TYPE_SPSEARCH_TAG = 284;
//Hyper-V 
$STREAM_REC_TYPE_HYPERV_TAG = 285;
//Automated System Recovery
$STREAM_REC_TYPE_ASR_TAG = 286;
//Shadow Copy Optimzation
$STREAM_REC_TYPE_SC_OPTIMIZATION_TAG = 287;
//MS Search 
$STREAM_REC_TYPE_MSSEARCH_TAG = 288;
//Task Scheduler 
$STREAM_REC_TYPE_TASK_SCHEDULER_TAG = 289;
//VSS MetaData Store 
$STREAM_REC_TYPE_VSS_METADATA_STORE_TAG = 290;
//Performance Counter 
$STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG = 291;
//IIS Config Writer
$STREAM_REC_TYPE_IIS_CONFIG_TAG	= 292;
//FSRM Writer
$STREAM_REC_TYPE_FSRM_TAG = 293;
//ADAM(instanceN) Writer
$STREAM_REC_TYPE_ADAM_TAG =294;
//Cluster Database Writer
$STREAM_REC_TYPE_CLUSTER_DB_TAG = 295;
//Network Policy Server (NPS) VSS Writer
$STREAM_REC_TYPE_NPS_TAG = 296;
//TS Gateway Writer
$STREAM_REC_TYPE_TSG_TAG = 297;
//Distributed File System Replication
$STREAM_REC_TYPE_DFSR_TAG = 298;
//VMWare Server
$STREAM_REC_TYPE_VMWARESRV_TAG = 299;
//SQL Server 2012
$STREAM_REC_TYPE_SQL2012_TAG = 300;


//Revocation Tag - A special tag to revoke/delete/cancel already issued tags 
$STREAM_REC_TYPE_REVOCATION_TAG = 511;
//TAG GUID, a common unique tag value across multiple volumes for recovery
$STREAM_REC_TYPE_TAGGUID_TAG = 767;

//System level Tag - it includes all applications
$SYSTEM_LEVEL_TAG = 1279;

//Application VSS Writer 
$STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_TAG = 1535;

//Application Scenario States
$CREATION_INCOMPLETE = 90;
$VALIDATION_INPROGRESS = 91;
$VALIDATION_SUCCESS = 92;
$VALIDATION_FAILED = 93;
$ACTIVE = 94;
$INACTIVE = 95;
$CONFIGURATION_CHANGED = 96;
$EDIT_CONFIGURATION = 97;
$PREPARE_TARGET_PENDING = 98;
$PREPARE_TARGET_DONE = 99;
$PREPARE_TARGET_FAILED = 100;
$SCENARIO_DELETE_PENDING = 101;

$FAILOVER_INPROGRESS = 102;
$FAILOVER_DONE = 103;
$FAILOVER_FAILED = 104;

$FAILBACK_INPROGRESS = 105;
$FAILBACK_DONE = 106;
$FAILBACK_FAILED = 107;
$CREATING_REVERSE_REPLICATION_PAIRS = 108;
$CREATING_REPLICATION_PAIRS = 109;
$REVERSE_REPLICATION_ACTIVE = 110;
$REPLICATION_PAIRS_ACTIVE = 111;
$SNAPSHOT_INPROGRESS = 112;
$PRESCRIPT_FAILED = 113;
$SNAPSHOT_FAILED = 114;
$SNAPSHOT_SUCESS = 115;
$ROLLBACK_INPROGRESS = 116;
$ROLLBACK_DONE = 117;
$ROLLBACK_FAILED = 118;
$POSTSCRIPT_FAILED = 119;
$MANUAL_FIX = 121;
$DISABLE = 122;
$ROLLBACK_COMPLETED = 130;

# multi-tenancy Fix
$INMAGE_ACCOUNT_ID = "5C1DAEF0-9386-44a5";
$INMAGE_ACCOUNT_TYPE = "PARTNER";

$EXPORT_PENDING = 124;
$EXPORT_SUCCESS = 125;
$EXPORT_FAILED = -125;

$UNEXPORT_PENDING = 126;
$UNEXPORT_SUCCESS = 127;
$UNEXPORT_FAILED = -127;

$RECOVERY_SCENARIO_DELETE_PENDING = 128;

$TARGET_LUN_MAP_PENDING = 300;
$TARGET_LUN_MAP_DONE = 301;
$TARGET_LUN_MAP_FAILED = 302;

$TARGET_DEVICE_DISCOVERY_PENDING = 303;
$TARGET_DEVICE_DISCOVERY_DONE = 304;
$TARGET_DEVICE_DISCOVERY_FAILED = 305;

$MAP_DISCOVERY_PENDING = 300; 
$MAP_DISCOVERY_DONE = 301;
$MAP_DISCOVERY_FAILED = -301;

$UNMAP_PENDING = 24; 
$UNMAP_DONE = 301;
$UNMAP_FAILED = -301;

$VOLPACK_CREATION_PENDING = 123;
$VOLPACK_CREATION_FAILED = 124;
$VOLPACK_CREATION_SUCCESS = 125;
$SETUP_REPOSITORY_PENDING = 126;
$SETUP_REPOSITORY_DONE = 127;
$SETUP_REPOSITORY_FAILED = 128;
$SCENARIO_DELETE_FAILED  = 123;

$PAUSE_TRACKING_PENDING = 48;
$PAUSE_TRACKING_FAILED = -48;
$PAUSE_TRACKING_COMPLETED = 52;

$UNSUPPORTED_DATA_PLANE = 0;
$INMAGE_DATA_PLANE = 1;
$AZURE_DATA_PLANE = 2;

$APPLICATION_PLAN_TYPE = 1;
$BULK_PLAN_TYPE = 2;
$CLOUD_PLAN_TYPE = 4;

$CLOUD_PLAN_AZURE_SOLUTION_TYPE = 4;
$CLOUD_PLAN_VMWARE_SOLUTION_TYPE = 5;
$CLOUD_PLAN_PHYSICAL_SOLUTION_TYPE = 6;
$CLOUD_PLAN_HYPERV_SOLUTION_TYPE = 7;

#Crash consistency default value in seconds.
$CRASH_CONSISTENCY_INTERVAL = 300;
$DVACP_CRASH_CONSISTENCY_INTERVAL = 300;

$requestXML;
$asyncRequestXML;
$activityId;
$clientRequestId;
$AGENTROLE = array('Agent','MasterTarget');


$SOLUTION_TYPE = array('HOST','FABRIC','PRISM','ARRAY','ESX','AZURE','AWS','VMWARE');
$PLAN_TYPE = array('APPLICATION','BULK','OTHER','CLOUD');

$APPLICATION_PLAN_STATUS = array('Readiness Check Pending','Readiness Check Success','Readiness Check Failed','Recovery Pending','Recovery Success','Recovery Failed','Configuration Pending','Configuration Success','Configuration Failed','Prepare Target Pending','Prepare Target Success','Prepare Target Failed','Active');

/*
Invalid/Unexpected operation. Non-Retryable Errors [0x80790500 - 0x807905FF] 
cpp_quote("#define MT_E_DS_FAILED_OVER                    0x80790500ul")
cpp_quote("#define MT_E_DS_NOT_PROTECTED                  0x80790501ul")
cpp_quote("#define MT_E_STORAGE_NOT_FOUND                 0x80790502ul")
cpp_quote("#define MT_E_REPLICATION_BLOCKED               0x80790503ul")
Currently system sends alerts for below 3 non-retryable bucket.
Refer eventCodes table in database to know about error code details.
*/
$MARS_NON_RETRYABLE_ERRORS_BUCKET = array("80790502"=>"EA0432","80790503"=>"EA0433");
$MARS_NON_RETRYABLE_ERRORS_HF_MAP = array("80790502"=>"ECH00012","80790503"=>"ECH00013");

/*
Re-tryable error bucket.
cpp_quote("#define MT_E_SERVICE_COMMUNICATION_ERROR		  0x80790100ul") 	
cpp_quote("#define MT_E_REGISTRATION_FAILED		  		  0x80790305ul")
cpp_quote("#define MT_E_SERVICE_AUTHENTICATION_ERROR      0x80790300ul")

The below is code reported by MT, not by MARS and in case MARS service is in stopped state.
cpp_quote("#define MT_E_MARS_SERVICE_STOPPED      0x800706baul")
*/
$MARS_RETRYABLE_ERRORS_FABRIC_HF_MAP = array("80790100"=>"EPH00022","80790305"=>"EPH00023","80790300"=>"EPH00024","800706ba"=>"EPH00025");
$MARS_RETRYABLE_ERRORS_PROTECTION_HF_MAP = array("80790100"=>"ECH00032","800706ba"=>"ECH00033");

$MARS_HEALTH_FACTOR_SOURCE_FOR_FABRIC = "Ps";
$MARS_HEALTH_FACTOR_SOURCE_FOR_PROTECTION = "PS";

$MDS_ERROR_CODE_LIST = array("EA0429","EA0430","EA0431","EA0432","EA0433","EC0127","EC0129");
$DROPPED_ERROR_CODE_LIST = array("EA0432", "EA0433", "EC0121", "AA0305", "EC0116", "EC0117", "EC0118", "EC0139", "EC0140", "EC0141", "EC0146");

/*
// InMageVmFilterDriverNotLoaded  - ECH00024 -- Filter not loaded, the param is source id. VM level health factor.
// InMageVmReplicatingDiskRemoved  - ECH00025 -- Disk removed, the param is disk ids. Disk level health factor.
// InMageVmLogUploadLocationNotHealthy  - ECH00026 -- Process server not reachable from source. Data upload path down, the param is source id. VM level health factor.
*/
$MOBILITY_AGENT_HF_LIST = array("ECH00024","ECH00025","ECH00026","ECH00028","ECH00029","ECH00030","ECH00031");
$VM_LEVEL_HF_LIST = array("ECH00024","ECH00026","ECH00030","ECH00031");
$DISK_LEVEL_HF_LIST = array("ECH00025","ECH00028","ECH00029");
$DETECTION_TIME_INCLUSION_HF_LIST = array("ECH00027");
	
$SRC_MT_DSK_MPPNG_STUS = array('Disk Layout Pending','Disk Layout Complete');

// Infrastructure health factor error codes.
$INFRA_HF_ERR_LIST = array("EPH0003", "EPH0004", "EPH0005", "EPH0006",
                            "EPH0007", "EPH0008", "EPH0009", "EPH0010", "EPH0011", "ECH00020");

// This array contains all the column names which contains guid formatted value.
$GUID_FMT_COL_LIST = array("processserverid", "sourcehostid",
                            "destinationhostid", "hostid", "id");

// This array contains all the columns which contains numeric value.
$NUM_FMT_COL_LIST = array("jobid", "pairid", "policyid", "policystate", "replication_status",
                            "tar_replication_status", "src_replication_status", "deleted",
                            "restartresynccounter", "replicationcleanupoptions");

// Pairwise healthevent error codes in string format.
$PAIR_HE_LIST = "('EC0127', 'EC0129')";

// 127.0.0.1 - IPV4 Address
// ::1 - IPV6 Address
$WHITE_LIST_IP_ARR = array('127.0.0.1', 'localhost', '::1');

// DRA API List
$DRA_API_LIST = array('ListScoutComponents',
                        'ListEvents',
                        'InfrastructureDiscovery',
                        'ListInfrastructure',
                        'CreateProtection',
                        'CreateProtectionV2',
                        'ModifyProtection',
                        'ModifyProtectionV2',
                        'CreateRollback',
                        'GetRollbackState',
                        'ListPlans',
                        'ModifyProtectionProperties',
                        'ModifyProtectionPropertiesV2',
                        'RestartResync',
                        'GetCSPSHealth',
                        'RemoveInfrastructure',
                        'InstallMobilityService',
                        'UpdateMobilityService',
                        'StopProtection',
                        'GetProtectionDetails',
                        'ListConsistencyPoints',
                        'CleanRollbackPlans',
                        'UpdateInfrastructureDiscovery',
                        'SetVPNDetails',
                        'PSFailover',
                        'GetErrorLogs',
                        'RefreshHostInfo',
                        'NotifyError',
                        'InsertScriptPolicy',
                        'RenewCertificate',
                        'MigrateACSToAAD',
                        'MachineRegistrationToRcm',
                        'FinalReplicationCycle',
                        'MobilityAgentSwitchToRcm',
                        'ResumeReplicationToCs'
						//'GetHostInfo', // Commenting out as input validation was handled in the corresponding API validation method and also it's a common API being called from DRA and ESXUtil during failback.
						//'GetHostCredentials', // commenting as API definition doesn't exist.
						//'GetRequestStatus', // Commenting out as input validation was handled in the corresponding API validation method.
						//'ListAccounts', // Commenting out as API input is empty.
						//'SetPSNatIP', // commenting as API definition doesn't exist.
						//'UnregisterAgent', // Commenting out as input validation was handled in the corresponding API validation method.
                        );
						
$LIST_OF_TABLES_PS_TO_ALLOW_FOR_DB_UPDATES = array(	'infrastructurehealthfactors',
													'srclogicalvolumedestlogicalvolume',
													'systemhealth',
													'processserver',
													'healthfactors',
													'policyinstance',
													'logrotationlist',
													'hosts');

$DISCOVERY_FAILED_ERROR_CODE = 1000;
$MOBILITY_SERVICE_FAILED_ERROR_CODE = 1001;
$PROTECTION_FAILED_ERROR_CODE = 1002;
$ROLLBACK_FAILED_ERROR_CODE = 1003;
$API_FAILED_ERROR_CODE = 1004;
$PROTECTION_TIMEOUT_ERROR_CODE = 1006;
$REGISTRATION_ERROR_CODE = 1007;
$UNREGISTRATION_ERROR_CODE = 1008;  
$MIGRATION_ERROR_CODE = 1009;  

$DISCOVERY_FAILED_ERROR_CODE_NAME = "Discovery Failed";
$MOBILITY_SERVICE_FAILED_ERROR_CODE_NAME = "Mobility Service Failed";
$PROTECTION_FAILED_ERROR_CODE_NAME = "Protection Failed";
$ROLLBACK_FAILED_ERROR_CODE_NAME = "Rollback Failed";
$API_FAILED_ERROR_CODE_NAME = "API Errors";
$PROTECTION_TIMEOUT_ERROR_CODE_NAME = "Protection Timeout";
$UNREGISTRATION_ERROR_CODE_NAME = "Unregistration";
$REGISTRATION_ERROR_CODE_NAME = "Registration";
$MIGRATION_ERROR_CODE_NAME = "MigrateAcsToAad";

$MDS_ACTIVITY_ID = '9CC8C6E1-2BFB-4544-A850-A23C9EF8162D';

$DISK_PROTECTION_UNIT = 'DISK';
$VOLUME_PROTECTION_UNIT = 'VOLUME';
$CLOUD_PLAN = 'CLOUD';
$CLOUD_PLAN_AZURE_SOLUTION = 'AZURE';
$CLOUD_PLAN_VMWARE_SOLUTION = 'VMWARE';
$CLOUD_PLAN_PHYSICAL_SOLUTION = 'PHYSICAL';
$CLOUD_PLAN_HYPERV_SOLUTION = 'HYPERV';
$E2A = "E2A";
$A2E = "A2E";
$LINUX_SLASH = "/";
$BOOT = "/boot";
$ETC = "/etc";
$INFRASTRUCTURE_VM_ID_DEFAULT = '00000000-0000-0000-0000-000000000000';
$DVACP_DEFAULT_PORT = 20004;
$CLOUD_CONTAINER_CERT = "CloudContainerCertificate.pfx";
$CLOUD_CONTAINER_KEY = "CloudContainerRegistry.hiv";
$PRIVATE_FOLDER_NAME = "private";
$FILE_OPEN_RETRY = "5";
$SLEEP_RETRY_WINDOW = "1";
$CONN_ALIVE_RETRY = "20";
$CONN_INFRA_ALIVE_RETRY = "300";

$MT_VX_SERVICE_HEART_BEAT_HF = "EMH0001";
$MT_APP_SERVICE_HEART_BEAT_HF = "EMH0002";
$MT_RETENTION_VOLUMES_LOW_SPACE_HF = "EMH0003";

$PS_HEART_BEAT_HF = "EPH0001";
$PS_CUMULATIVE_THROTTLE_HF = "EPH0002";

$DEFAULT_HF_DETECTION_TIME = "-1";
$MYSQL_FOREIGN_KEY_VIOLATION = "1452";
$TELEMETRY_FILE_SIZE = 18432; //Bytes
$UNDER_SCORE = "_";
$APPINSIGHTS_WRITE_DIR_COUNT_LIMIT = 12;
$ASRAPI_APPINSIGHTS_LOG = "asrapi_ux_events";
$AUDIT_APPINSIGHTS_LOG = "auditlog";
$TELEMETRY_FOLDER_COUNT_FILE = ".telemetryfolderexhausted";
$MINUS_ONE = "-1";
$FOUR = "4";
$ZERO = "0";
$ONE = "1";
$TWO = "2";
$THREE = "3";
$SLOW_PROGRESS_FORWARD = "SourceAgentSlowResyncProgressOnPremToAzure";
$SLOW_PROGRESS_REVERSE = "SourceAgentSlowResyncProgressAzureToOnPrem";
$NO_PROGRESS = "SourceAgentNoResyncProgress";

$RESYNC_HEALTH_CODE_FOR_BACKWARD_AGENT = "ECH0003";
$MASTER_TARGET = 'MT';
$SOURCE_AGENT = 'SOURCE';
$SOURCE_AGENT_COMPONENT = 'Agent';
$CONFIGURATION_SERVER = 'CS';
$PROCESS_SERVER = 'PS';
$RESYNC_REQD = 'RESYNC_REQD';
$FATAL = 'FATAL';
$WARNING = 'WARNING';
$NOTICE = 'NOTICE';
$CONFIGURATION_SERVER_DISK_RESIZE = 'ConfigurationServerDiskResize';
$PS_FAIL_OVER = 'ConfigurationServerPsFailOver';
$VM_HEALTH = 'VM health';
$YES = 'Yes';
$SOURCE_DATA = 'SOURCE';
$TARGET_DATA = 'TARGET';

abstract class Log
{
	const MDS = 0;
    const APPINSIGHTS = 1;
    const BOTH = 2;
} 

abstract class Extension
{
	const JSON = ".json";
    const LOG = ".log";
}

function read_installation_directory()
{		
	$dirpath = getenv('ProgramData');	
    $result = array();
    if ($dirpath)
    {
        $installDirFile = join(DIRECTORY_SEPARATOR, array($dirpath, 'Microsoft Azure Site Recovery', 'Config', 'App.conf'));
        if (file_exists($installDirFile))
        {
            $file = fopen ($installDirFile, 'r');
            $conf = fread ($file, filesize($installDirFile));
            fclose ($file);
            $conf_array = explode ("\n", $conf);
            foreach ($conf_array as $line)
            {
                $line = trim($line);
                if (empty($line))	continue;
            
                if (! preg_match ("/^#/", $line))
                {
                    list ($param, $value) = explode ("=", $line);
                    $param = trim($param);
                    $value = trim($value);
                    $result[$param] = str_replace('"', '', $value);
                }
            }
        }
    }
	
	return $result;
}
?>
