# Core package for TimeShot Manager
#
############## HEADER: Fixes gone in###########################################
#Bug No	  Fixed By					Bug Description
#2249	    BSR				Day Light savings time affecting the modification time of diff files at CX end.
#4668       bknathan                    File replication RPO alerts for on-demand jobs have been handled. (
#                                                            function - monitorFileRepRPO)
#################End of the Header#############################################

package TimeShotManager;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use File::Basename;
use File::Path;
use Messenger;
use tmanager;
use Utilities;
use Sys::Hostname;
use ConfigServer::Trending::HealthReport;
use POSIX;
use Sys::Hostname;
use Socket;
use HTTP::Date;
use Common::Constants;
use Common::Log;
use Common::DB::Connection;
use PHP::Serialization qw(serialize unserialize);
use Data::Dumper;
use Fcntl qw(:flock SEEK_END);
use RequestHTTP;
use File::Find;

my $dirs_size = 0;
my $folder_to_query = '';

my $AMETHYST_VARS;
# parsing the amethyst.conf
# fix 4234, 4235
BEGIN 
{
  $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
}

#Start - issue no: 2249;
our $HAS_NET_IFCONFIG = eval { require Win32::UTCFileTime ; 1; }; 
#End - issue no: 2249

# Package version
our $VERSION                    = "0.1";

# Debug level
our $DEBUG_LEVEL                = 255;  # Debug levels from 0 - 255
                                        # 00 - 09 are absolutely critical
                                        # 10 - 40 are user reported
                                        # 80 - 255 are for debugging support
our $NOTIFY_ERROR_LEVEL         = 26;   # All errors below this level will result
                                        # in SNMP traps and emails being generated

our $NOTIFY_LOG_ONLY            = 15;   # All errors in this level will be logged
                                        # in the file but not sent via email

# These are just hints at this time
our $DPO_TYPE_IS                = "IS";	# Initial Sync
our $DPO_TYPE_FR                = "FR";	# File Replication

our $IS_RC_PENDING               = 1;
our $IS_RC_COMPLETE              = 0;

our $MAX_FILE_LENGTH             = 255;  #Max file length
our $DOUBLE_BACKWARD_SLASH       = "\\\\";
our $SINGLE_BACKWARD_SLASH       = "\\";
our $DOUBLE_FWDWARD_SLASH       = "//";
our $SINGLE_FWDWARD_SLASH       = "/";

##
# Package global constants
##
# External entity constants
my $cs_installation_path = Utilities::get_cs_installation_path();
my $HOME_DIR;
if(Utilities::isWindows())
{
	$HOME_DIR = "$cs_installation_path\\home\\svsystems"
}
else
{
	$HOME_DIR			= "/home/svsystems";
}
my $REPLICATION_ROOT            =$HOME_DIR;
my $GZIP                        = "gzip -f";
my $BZIP2                       = "bzip2";
my $CP                          = "cp -v";
#Added by BSR to fix issue#1741, one line definition
my $SHUTDOWN_FILE		= ".tmanshutdown" ;

my $TOUCH                       = "$HOME_DIR/bin/inmtouch";
my @SECONDARY_VOLUMES           = ("/","/var/log","/home/svsystems","/tmp");

my $HA_DB_CONN_TRY_COUNT        = 0;
my $HA_DB_CONN_TRY_LIMIT        = 3;
my $HA_HTTP_GET_TRY_COUNT       = 0;
my $HA_HTTP_GET_TRY_LIMIT       = 3;
my $HA_HTTP_GET_TIMEOUT         = 30;
my $HA_HTTP_GET_PAGE            = "index.html";
my $HEARTBEAT_STOP 		= "/usr/lib64/heartbeat/hb_standby";
my $HEARTBEAT_STATUS            = "/etc/init.d/heartbeat status";
my $HEARTBEAT_START             = "/etc/init.d/heartbeat start";
my $MAX_DIFF_THROTTLE_COMPAT_VERSION 		= 9360000;
my $MAX_RESYNC_THROTTLE_COMPAT_VERSION 		= 9360000;

my $tsObj = Utilities::isWindows();
my $logging_obj = new Common::Log();
if ($tsObj)
{  
  @SECONDARY_VOLUMES = ("'".$cs_installation_path."\\home"."'");
  $TOUCH = "inmtouch.exe";
}
my $LS                          = "ls -rt";
my $RM                          = "rm";
my $MV                          = "mv";
my $CHMOD                       = "chmod";
my $MD5SUM                      = "md5sum";
my $MOUNT                       = "mount";
my $DIRNAME                     = "dirname";
my $FTP                         = "lftpget";
my $HTTP                        = "lftpget";
my $SMBD                        = "smbd";
my $NMBD                        = "nmbd";
my $HTTPD                       = "httpd";


my $SCHEDULER                   = "scheduler";
my $DIFFDATASORT;
if(Utilities::isWindows())
{
	$DIFFDATASORT                = "$cs_installation_path\\home\\svsystems\\bin\\diffdatasort";
}
else
{
	$DIFFDATASORT                = "/home/svsystems/bin/diffdatasort";
}
my $WIN_SCHEDULER               = "scheduler.exe";
my $SMB_START                   = "/etc/rc.d/init.d/smb start";
my $HTTPD_START                 = "/etc/rc.d/init.d/httpd start";
my $SCHEDULER_START             = " killall -9 scheduler;  /home/svsystems/bin/scheduler &";
my $SCHEDULER_STOP				= "/usr/bin/killall -15 scheduler";
my $PS_CMD                      = "ps -ef";
my $GZIP_EXTENSION              = ".gz";
my $BZIP2_EXTENSION             = ".bz2";
my $FS_COPY                     = "cp";
my $FTP_COPY                    = "ftp";
my $HTTP_COPY                   = "http";
my $DD                          = "sdd";
my $DD_MODE                     = "dd";
my $EXPAND_CHANGE_BLK_FILE_MODE = "expand-changes";
my $RETURN_HOME_MODE            = "return-home";
my $PARTFILE                    = "/proc/partitions";
my $DF_CMD                      = "df";
my $DU_CMD                      = "du";
my $PATCHRESYNC                 = "patchresync";
my $PATCHEDIFFS                 = "patchediffs";


# Tweakable parameters below
#my $SENTINEL_ERR_THRESHOLD      = 30 * 60;       # 30 minutes
my $SENTINEL_ERR_THRESHOLD      = 30 * 30;       # 15 minutes
my $TM_ERR_THRESHOLD            = 30 * 60;       # 40 minutes
my $OUTPOST_AGENT_ERR_THRESHOLD = 30 * 60;       # 90 minutes
my $DISK_SPACE_WARN_THRESHOLD   = 80;            # Warn if more than 80% of disk space is in use
my $RPO_THRESHOLD               = 2  * 3600;     # 2 hours
my $RESYNC_THROTTLE_THRESHOLD   = $DISK_SPACE_WARN_THRESHOLD; # This will ensure that a warning trap/email goes out
my $MAX_BATCH_RESYNC_FILES      = 10;            # Max number of resync files that will be processed in one go
my $MAX_PENDING_RESYNC_FILES    = 64;            # Max number of resync files that will be held uncompressed at a point in time
my $MAX_BATCH_EC_FILES          = 80;            # Max number of diff files that will be processed in one go
my $MAX_BATCH_PROCESSING_CHUNK  = 10*60;         # Max number of minutes spent on processing each host
# my $FX_TIMEOUT_THRESHOLD        = 900;       # default value for fx timeout.
#
#	getting $MAX_RESYNC_FILES_THRESHOLD,$MAX_DIFF_FILES_THRESHOLD varibale from amethyst.conf file
#   fix 4234, 4235
#
my $MAX_RESYNC_FILES_THRESHOLD  = $AMETHYST_VARS->{MAX_RESYNC_FILES_THRESHOLD};
my $MAX_DIFF_FILES_THRESHOLD  = $AMETHYST_VARS->{MAX_DIFF_FILES_THRESHOLD};
my $CS_IP = $AMETHYST_VARS->{CS_IP};

# Trap listeners

my @SECONDARY_VOLUMES_FILE      = ("volume_test.file","/messages","/etc/amethyst.conf");

my $GUID_FILLER                 = "";
my $MY_AMETHYST_GUID            = $AMETHYST_VARS->{HOST_GUID};
my $NULL_GUID                   = $AMETHYST_VARS->{HOST_GUID};
my $SOURCE_FAILURE              = $AMETHYST_VARS->{"SOURCE_FAILURE"};
# Internal constants
my $PACKAGE_NAME                = "TimeShotManager";

my $TRENDING_DIR = $AMETHYST_VARS->{WEB_ROOT}."/trends";

my $GLOBAL_LOGFILE              = "/var/log/TimeShotManager.log";
my $TRAP_LOGFILE                = $TRENDING_DIR."/logs/TrapLog.txt";
if (-e "/srv/www/htdocs/admin/web/trends/logs/TrapLog.txt")
{
  $TRAP_LOGFILE                 = "/srv/www/htdocs/admin/web/trends/logs/TrapLog.txt";
}
if (Utilities::isWindows())
{
  $GLOBAL_LOGFILE               = "$cs_installation_path\\home\\svsystems\\log\\TimeShotManager.log";
  $TRAP_LOGFILE                 = "$cs_installation_path\\home\\svsystems\\admin\\web\\trends\\logs\\TrapLog.txt";
}

#my $IS_IODIR                   = "initial_sync"; # Both input and output
my $IS_IODIR                    = ""; # Both input and output
my $IS_OFILE_PFX                = "completed_initsync_";
my $IS_RENAME_PFX               = "_tmp_"; # Used for prefixing temp files
my $ECB_IDIR                    = "diffs"; # Only input
my $ECB_IFILE_PFX               = "completed_diff";
#my $ECB_ODIR                    = "ediffs"; # Only output
my $ECB_ODIR                    = ""; # Only output
my $ECB_OFILE_PFX               = "completed_ediff";
my $ECB_RENAME_PFX              = "_tmp_"; # Used for prefixing temp files
my $RH_IDIR                     = "returnhome";	# Only input
my $RH_IFILE_PFX                = "completed_returnhome";

my $RS_IODIR                    = "resync"; # Both input and output
my $RS_IFILE_PFX                = "completed_initialsync";
my $RS_RENAME_PFX               = "_tmp_";
#my $RS_OFILE_PFX                = "completed_resync_";
my $RS_OFILE_PFX                = "resync_";

my $TS_BASELINE_FILE            = "timeshot_baseline_";

my $MD5_CSUM_PFX                = "md5."; # MD5 Checksum file prefix
# These two defines will dictate which sql columns are updated when doInitialSync
# is called
my $DO_IS_MODE_IS               = "is"; # Initial Sync
my $DO_IS_MODE_RE               = "re"; # Resync

# fast resync prefix
my $FS_IFILE_PFX                = "tmp_completed_sync_";
my $FS_OFILE_PFX                = "completed_sync_";
my $FS_HCD_PFX                  = "completed_hcd_";
my $FS_SDNI_PFX                 = "completed_sdni_";

# Database constants (remove them from tmanager once migration is complete)
my $HOSTS_TBL                         = "hosts";
my $HOSTS_id                          = "id";
my $HOSTS_name                        = "name";

#6452
my $HOSTS_ipaddress					  = "ipaddress";

my $HOSTS_vxTimeout                   = "vxTimeout";
my $HOSTS_sentinelEnabled             = "sentinelEnabled";
my $HOSTS_outpostAgentEnabled         = "outpostAgentEnabled";
my $HOSTS_lastHostUpdateTime          = "lastHostUpdateTime";
my $HOSTS_filereplicationAgentEnabled = "filereplicationAgentEnabled";
my $HOSTS_compatibilityNo		    = "compatibilityNo";	

my $LV_TBL                            = "logicalVolumes";
my $LV_hostId                         = "hostId";
my $LV_deviceName                     = "deviceName";
my $LV_capacity                       = "capacity";
my $LV_lun                            = "lun";
my $LV_lastSentinelChange             = "lastSentinelChange";
my $LV_lastOutpostAgentChange         = "lastOutpostAgentChange";
my $LV_dpsId                          = "dpsId";
my $LV_farLineProtected               = "farLineProtected";
my $LV_returnHomeEnabled              = "returnHomeEnabled";
my $LV_doResync                       = "doResync";
my $LV_startingPhysicalOffset         = "startingPhysicalOffset";

my $SRC_DST_LV_TBL                    = "srcLogicalVolumeDestLogicalVolume";
my $SRC_DST_LV_sourceHostId           = "sourceHostId";
my $SRC_DST_LV_sourceDeviceName       = "sourceDeviceName";
my $SRC_DST_LV_destinationHostId      = "destinationHostId";
my $SRC_DST_LV_destinationDeviceName  = "destinationDeviceName";
my $SRC_DST_LV_startTagTime  = "resyncStartTagtime";
my $SRC_DST_LV_fullSyncStatus         = "fullSyncStatus";
my $SRC_DST_LV_jobId		          = "jobId";
my $SRC_DST_LV_planId		          = "planId";

my $FULL_SYNC_STS_NONE                = 0;
my $FULL_SYNC_STS_IS_START            = 1005;
my $FULL_SYNC_STS_IS_IN_PROGRESS      = 1020;
my $FULL_SYNC_STS_IS_FAILED_GEN       = 1161;
my $FULL_SYNC_STS_IS_SUCCEEDED_GEN    = 1280;
my $ZERO_DATETIME                     = "0000-00-00 00:00:00";
my $SRC_DST_LV_fullSyncStartTime      = "fullSyncStartTime";
my $SRC_DST_LV_fullSyncEndTime        = "fullSyncEndTime";
my $SRC_DST_LV_fullSyncBytesSend      = "fullSyncBytesSend";

my $SRC_DST_LV_resyncStartTime        = "resyncStartTime";
my $SRC_DST_LV_resyncEndTime          = "resyncEndTime";
my $SRC_DST_LV_resyncRequired         = "shouldResync";
my $SRC_DST_LV_resyncStatus           = "resyncStatus";
my $RESYNC_STS_NONE                   = 0;
my $RESYNC_STS_START                  = 1005;
my $RESYNC_STS_IN_PROGRESS            = 1020;
my $RESYNC_STS_FAILED_GEN             = 1161;
my $RESYNC_STS_SUCCEEDED_GEN          = 1280;

my $SRC_DST_LV_differentialStartTime  = "differentialStartTime";
my $SRC_DST_LV_differentialEndTime    = "differentialEndTime";
my $SRC_DST_LV_differentialStatus     = "differentialStatus";
my $SRC_DST_LV_returnhomeStartTime    = "returnhomeStartTime";
my $SRC_DST_LV_returnhomeEndTime      = "returnhomeEndTime";
my $RETURNHOME_STS_NONE               = 0;
my $RETURNHOME_STS_RH_START           = 1005; # UI has indicated a desire for RH operation
my $RETURNHOME_STS_RH_IN_PROGRESS     = 1020; # OA has indicated that RH op is in progress
my $RETURNHOME_STS_RH_OA_COMPLETE     = 1040; # OA has indicated that all RH files have been copied
my $RETURNHOME_STS_RH_TM_COMPLETE     = 1050; # TM has indicated all RH files have been applied
                                              # Same as the below for all practical purposes.
my $RETURNHOME_STS_RH_FAILED_GEN          = 1161; # Encountered a failure somewhere...
my $RETURNHOME_STS_RH_SUCCEEDED_GEN       = 1280; # TM has received RH files and applied them successfully
my $SRC_DST_LV_returnhomeStatus           = "returnhomeStatus";
my $SRC_DST_LV_lastSentinelChange         = "lastSentinelChange";
my $SRC_DST_LV_lastTMChange               = "lastTMChange";
my $SRC_DST_LV_lastOutpostAgentChange     = "lastOutpostAgentChange";
my $SRC_DST_LV_remainingDifferentialBytes = "remainingDifferentialBytes";
my $SRC_DST_LV_remainingResyncBytes       = "remainingResyncBytes";
my $SRC_DST_LV_currentRPOTime             = "currentRPOTime";
my $SRC_DST_LV_statusUpdateTime           = "statusUpdateTime";
my $SRC_DST_LV_profilingMode              = "profilingMode";
my $SRC_DST_LV_rpoSLAThreshold            = "rpoSLAThreshold";
my $SRC_DST_LV_tsDir                      = "tsDir";
my $SRC_DST_LV_tsRetentionDuration        = "tsRetentionDuration";
my $SRC_DST_LV_tsInterval                 = "tsInterval";
my $SRC_DST_LV_tsEnable                   = "tsEnable";
my $SRC_DST_LV_remainingQuasiDiffBytes	 = "remainingQuasiDiffBytes";
my $SRC_DST_LV_resyncEndTagtime		 = "resyncEndTagtime";
my $SRC_DST_LV_resyncStartTagtime	  = "resyncStartTagtime";
my $SRC_DST_LV_executionState	  = "executionState";
my $SRC_DST_LV_pairId	  = "pairId";
my $DPS_LV_TBL                            = "dpsLogicalVolumes";
my $DPS_id                                = "id";
my $DPS_deviceName                        = "deviceName";
my $DPS_capacity                          = "capacity";

my $ERR_ALERT_TBL                        = "errAlertable";
my $ERR_ALERT_hostid                     = "hostid";
my $ERR_ALERT_errlvl                     = "errlvl";
my $ERR_ALERT_errMessage                 = "errMessage";
my $ERR_ALERT_errtime                    = "errtime";
my $ERR_ALERT_id                         = "id";
my $ERR_ALERT_module			         = "module"; # Variables used while sending alert emails and traps
my $ERR_ALERT_alert_type                 = "alert_type";  # Variables used while sending alert emails and traps 

my $APlan_TBL                            = "applicationPlan";
my $APlan_name                           = "planName";
my $APlan_ID                             = "planId";

##
# DPS Object members
##
# IN
my $DPSO_type                   = "DPSO_type";                 # Type of object
my $DPSO_basedir                = "DPSO_basedir";              # Base directory for all ops
my $DPSO_initialized            = "DPSO_initialized";          # Indicates if the object has been successfully initialized or not 
my $DPSO_srchostid              = "DPSO_srchostid";            # Source host
my $DPSO_clusterids             = "DPSO_clusterids";           # additional Source hosts for clusters
my $DPSO_srchostname            = "DPSO_srchostname";          # Source host name
my $DPSO_srcvol                 = "DPSO_srcvol";               # Source volume
my $DPSO_dpsvol                 = "DPSO_dpsvol";               # Local volume equivalent of source volume
my $DPSO_dsthostid              = "DPSO_dsthostid";            # Destination host
my $DPSO_dsthostname            = "DPSO_dsthostname";          # Destination host name
my $DPSO_dstvol                 = "DPSO_dstvol";               # Destination volume
my $DPSO_dbh                    = "DPSO_dbh";                  # Database handle
my $DPSO_conn                   = "DPSO_conn";                 #
my $DPSO_msg                    = "DPSO_msg";                  # Messenger handle
my $DPSO_msg_source             = "DPSO_msg_source";           # Message source
my $DPSO_rpoSLAThreshold        = "DPSO_rpoSLAThreshold";      # Per protected pair RPO
my $DPSO_max_resync_threshold   = "DPSO_max_resync_threshold"; # Max Resync files Threshold
my $DPSO_pair_id                = "DPSO_pair_id"; 			   # Pair Id
my $DPSO_throttle_resync        = "DPSO_throttle_resync";      # Throttle Resync
my $DPSO_resync_start_tag_time  = "DPSO_resync_start_tag_time";  # resync_start_tag_time
my $DPSO_resync_end_tag_time  	= "DPSO_resync_end_tag_time";  # resync_end_tag_time
my $DPSO_phy_lun_id				= "DPSO_phy_lun_id";           # phy_lun_id
my $DPSO_dest_os_flag			= "DPSO_dest_os_flag";			# dest_os_flag
my $DPSO_src_os_flag 			= "DPSO_src_os_flag ";			# src_os_flag
my $DPSO_pair_type				= "DPSO_pair_type";				#pair_type
my $DPSO_plan_name				= "DPSO_plan_name";				# Plan name
my $DPSO_ps_id					= "DPSO_ps_id";					# Process Server Id
my $DPSO_is_cluster				= "DPSO_is_cluster";			# is cluster
# Common but necessary on input
my $DPSO_logfile                = "DPSO_logfile";              # Object specific log file
my $DPSO_lockfile               = "DPSO_lockfile";             # Object specific lock file
my $DPSO_status                 = "DPSO_status";               # Object status?
my $DPSO_gbllogfile             = "DPSO_gbllogfile";           # Global logfile
my $DPSO_rpologfile             = "DPSO_rpologfile";           # RPO Trending
my $DPSO_perflogfile            = "DPSO_perflogfile";          # Profiling data change rate, compression ratio
# InitialSync related variables
my $DPSO_is_blocksize           = "DPSO_is_blocksize";         # Input blocksize
my $DPSO_is_blocks_left         = "DPSO_is_blocks_left";       # If blocks are still left
my $DPSO_is_curr_filenum        = "DPS_is_curr_filenum";       # File midfix number
my $DPSO_num_of_initsync_files  = "DPSO_num_of_initsync_files";# Number of initsync_files waiting for oa
my $DPSO_is_count               = "DPSO_is_count";             # Input count
my $DPSO_is_polling_interval    = "DPSO_is_polling_interval";
my $DPSO_is_max_files_pending   = "DPSO_is_max_files_pending";
my $DPSO_is_enable_compression  = "DPSO_is_enable_compression";
my $DPSO_is_enable_pipelining   = "DPSO_is_enable_pipelining";
my $DPSO_is_enable_coalescion   = "DPSO_is_enable_coalescion";
my $DPSO_is_pipeline_file       = "DPSO_is_pipeline_file";
my $DPSO_is_skip                = "DPSO_is_skip";
# ExpandChanges related variables
my $DPSO_ecb_process_loop_wait  = "DPSO_ecb_process_loop_wait";
my $DPSO_ecb_enable_compression = "DPSO_ecb_enable_compression";
my $DPSO_ecb_enable_pipelining  = "DPSO_ecb_enable_pipelining";
my $DPSO_ecb_max_files_pending  = "DPSO_ecb_max_files_pending";
my $DPSO_ecb_pipeline_file      = "DPSO_ecb_pipeline_file";
my $DPSO_ecb_enable_profiling   = "DPSO_ecb_enable_profiling";
# ReturnHome related variables
my $DPSO_rh_enable_pipelining   = "DPSO_rh_enable_pipelining";
my $DPSO_rh_pipeline_file       = "DPSO_rh_pipeline_file";
my $DPSO_rh_max_files_local     = "DPSO_rh_max_files_local";
my $DPSO_rh_copy_mode           = "DPSO_rh_copy_mode";
my $DPSO_rh_process_loop_wait   = "DPSO_rh_process_loop_wait";
my $DPSO_rh_copy_loop_wait      = "DPSO_rh_copy_loop_wait";
# TimeShot related variables
my $DPSO_ts_enable              = "DPSO_ts_enable";
my $DPSO_ts_retentionDuration   = "DPSO_ts_retentionDuration";
my $DPSO_ts_interval            = "DPSO_ts_interval";
my $DPSO_ts_dir                 = "DPSO_ts_dir";

# Error code definitions that will be stored in $DPSO_status
my $DPSO_status_IS_GEN_ERR      = -1;
my $DPSO_status_EB_GEN_ERR      = -2;
my $DPSO_status_RH_GEN_ERR      = -1;

## Message descriptions for notifications via Messenger object.
my $MSG_INITSYNC                 =  "Initial sync message";
my $MSG_RESYNC                   =  "Resync message";
my $MSG_DIFFERENTIAL             =  "Differential message";
my $MSG_RETURN_HOME              =  "Return home message";
my $MSG_FILE                     =  "File replication message";
my $MSG_RPO                      =  "RPO message";
my $MSG_REPLICATION_STATUS       =  "Replication Status message";

my $DPSO_compression_enable 	 =  "compression enable flag";
my $DPSO_execution_state   	 	 =  "pair execution state";
my $DPSO_phy_lunid 				 =  "phyLunid";
my $DPSO_one_to_many_source 	 =	"one to many source flag";
my $DPSO_lun_state				 =  "lun state";
my $DPSO_one_to_many_pairs		 =  "one to many pairs info";
my $DPSO_jobId	 				 =  "jobId";
my $DPSO_host_hash	        	 =  "host info";
my $DPSO_replication_cleanup_options = "replication cleanup options";
my $DPSO_restart_resync_counter      = "restart resync counter";
my $DPSO_resync_start_time           = "resync start time";
my $DPSO_resync_end_time             = "resync end time";
my $DPSO_replication_status          = "replication status";
my $DPSO_tmid                        = "volsync child id";	
my $DPSO_db_state                    = "db status";
my $DPSO_pair_id 					 = "pair id";
##
# Package global variables
##
my $REPORTED_ERROR_LOG             = ""; # Errors that will be emailed
my %PENDING_TRAPS;                         # Traps that will be reported to the manager

my $LOG_ERR_LVL = 128; # Error level for logged messages

my $IR_DR_STUCK_WARNING_THRESHOLD   = $AMETHYST_VARS->{IR_DR_STUCK_WARNING_THRESHOLD};
my $IR_DR_STUCK_CRITICAL_THRESHOLD   = $AMETHYST_VARS->{IR_DR_STUCK_CRITICAL_THRESHOLD};

##
# Trap defaults: Should really be sending more useful information...for wsgr this will suffice
##
my $DEFAULT_REPL_IF_STATUS         = "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ";
my $DEFAULT_REPL_IF_ATTRIBUTES     = "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ".
                                     "                                ";


# Statistics log file prefix for grepping purposes
my $PERF_PFX = "Perf Metric V2";
my $RPO_PFX  = "RPO Metric V2";

# Sample performance log line "PerfLine; Date; SrcHostGuid; SrcVolume; FileName; Size; Compressed Size; \n"

##
#
# Function : new
#
# Purpose  : Returns a new DPS object member. The object
#            represents a protection operation applied to
#            source volume. The caller should treat this
#            object as opaque. All data accesss should be
#            done by invoking methods provided
#
##
sub new {
  
  my ($type, $basedir, $srchostid, $srcvol,  $dsthostid, $dstvol, $msg, $clusterids,$compressionEnable,$executionState,$phyLunid,$oneToManySource,$lunState,$profilingMode,$rpoSLAThreshold,$one_to_many_pairs,$jobId,$replicationCleanupOptions,$restartResyncCounter,$resyncStartTime,$resyncEndTime,$replicationStatus,$host_hash,$tmId,$dbState,$pairid) = @_;
  my $error = 0;
  my $logfile;
  my $lockfile;

  # validate input
  if ($type eq "" or
      ($type ne $DPO_TYPE_IS &&
       $type ne $DPO_TYPE_FR &&
       $type ne $PACKAGE_NAME
      )
     ) {
    print "dps object has no or unknown type specified: $type.\n";
	$logging_obj->log("EXCEPTION","dps object has no or unknown type specified: $type");
    $error = -1;
  }

  if ($type eq $PACKAGE_NAME) {
    $type = $DPO_TYPE_IS;	# Default mode is initial sync
				# followed by change blocks
  }
  
  if ($basedir eq "") {
    print "dps object has no basedir specified.\n";
	$logging_obj->log("EXCEPTION","dps object has no basedir specified");
    $error = -2;
  }

  if ($srchostid eq "") {
    print "dps object has no srchostid specified\n";
	$logging_obj->log("EXCEPTION","dps object has no srchostid specified");
    $error = -3;
  }
  if ($srcvol eq "") {
    print "dps object has no srcvol specified\n";
	$logging_obj->log("EXCEPTION","dps object has no srcvol specified");
    $error = -4;
  }
  
  if ($dsthostid eq "") {
    print "dps object has no dsthostid specified\n";
	$logging_obj->log("EXCEPTION","dps object has no dsthostid specified");
    $error = -6;
  }
  if ($dstvol eq "") {
    print "dps object has no dstvol specified\n";
	$logging_obj->log("EXCEPTION","dps object has no dstvol specified");
    $error = -7;
  }

  if ($msg eq "") {
    print "messenger object does not exist\n";
	$logging_obj->log("EXCEPTION","messenger object does not exist");
    $error = -9;
  }
  my $dps_object = {
		    $DPSO_type                   => $type,
		    $DPSO_basedir                => $basedir,
		    $DPSO_srchostid              => $srchostid,
		    $DPSO_srcvol                 => $srcvol,
		    $DPSO_dsthostid              => $dsthostid,
		    $DPSO_dstvol                 => $dstvol,		    
			$DPSO_clusterids             => $clusterids,
			$DPSO_compression_enable 	=> $compressionEnable,
			$DPSO_execution_state   	=> $executionState,
			$DPSO_phy_lunid 			=> $phyLunid,
			$DPSO_one_to_many_source 	=> $oneToManySource,
			$DPSO_lun_state			    => $lunState,
			$DPSO_ecb_enable_profiling	=> $profilingMode,
			$DPSO_rpoSLAThreshold		=> $rpoSLAThreshold	* 60,
			$DPSO_one_to_many_pairs		=> $one_to_many_pairs,
			$DPSO_jobId					=> $jobId,
			$DPSO_replication_cleanup_options	=> $replicationCleanupOptions,
			$DPSO_restart_resync_counter => $restartResyncCounter,
			$DPSO_resync_start_time      => $resyncStartTime,
			$DPSO_replication_status     => $replicationStatus,
			$DPSO_host_hash				 => $host_hash,
			$DPSO_tmid				     => $tmId,
			$DPSO_resync_end_time        => $resyncEndTime,
			$DPSO_db_state               => $dbState,
			$DPSO_pair_id               => $pairid
		    # The rest will be set in initializeNewObject
		   };
		  
  # Set default values for rest of the object variables
  my $error2 = &initializeNewObject($dps_object);

  # Make sure the right methods are invoked for this object
  # Might have to bless based on type if we subclass in the future
  bless ($dps_object, $PACKAGE_NAME);

  if ($error != 0 || $error2 != 0) {
    print ("Object is not initialized correctly:  $error $error2 \n");
	$logging_obj->log("EXCEPTION","Object is not initialized correctly:  $error $error2");
    $dps_object->{$DPSO_initialized} = 0;
  }
  else {
    $dps_object->{$DPSO_initialized} = 1;
  }

  return $dps_object;
}

##
# Has resync been performed before, 
# Returns 0 if it has been, 1 otherwise
##
sub isResyncNeeded {
	my ($obj) = @_;
	my $rc;



	# @FIX Use quoting to get immunity from data contents
	# INSERT some data into 'foo'. We are using $dbh->quote() for
	# quoting the name.   #$dbh->do("INSERT INTO foo VALUES (1, " . $dbh->quote("Tim") . ")");

	# Get the last full sync start time
	my $lastResyncStartTime = strftime("%Y-%m-%d %H:%M:%S",localtime($obj->{$DPSO_resync_start_time}));
	my $lastResyncEndTime = strftime("%Y-%m-%d %H:%M:%S",localtime($obj->{$DPSO_resync_end_time}));
	if(!$obj->{$DPSO_resync_start_time})
	{
		$lastResyncStartTime = $ZERO_DATETIME;
	}
	elsif( !$obj->{$DPSO_resync_end_time})
	{
		$lastResyncEndTime = $ZERO_DATETIME;
	}
	if ($lastResyncStartTime eq $ZERO_DATETIME || $lastResyncEndTime eq $ZERO_DATETIME) {
		$rc = 1;
	}
	else {
		$rc = 0;
	}
	return $rc;
}

#
# Do the resync process
# All this does is uncompress the files cinitialsync.exe generates
# and prefixes it with what the resync agent expects it to be.
# The compressed input files are expected in desthost/destvol/resync directory
# The output files also reside in the same directory
#
sub doResync 
{
    my ($obj) = @_;
    my $num_files_processed = 0;
    my $sourceFlag;
    my $targetFlag;
    my $pStartTime = time();
    my $ofile_dir =  $obj->{$DPSO_basedir} . "/" . $obj->{$DPSO_dsthostid}. "/" . $obj->{$DPSO_dstvol} . "/" . $RS_IODIR;
	my $dst_os_flag = $obj->{$DPSO_host_hash}->{$DPSO_dsthostid}->{'osFlag'};

    my $ofile_dir_modified = Utilities::makePath($ofile_dir,$dst_os_flag);
	my $src_os_flag = $obj->{$DPSO_host_hash}->{$DPSO_srchostid}->{'osFlag'};

    my $ifile_dir = $ofile_dir; 
    $ifile_dir = Utilities::makePath ($ifile_dir,$dst_os_flag);
    my $destfile_dir =  $obj->{$DPSO_basedir} . "/" . $obj->{$DPSO_dsthostid}. "/" .    $obj->{$DPSO_dstvol};
    $destfile_dir = Utilities::makePath ($destfile_dir,$dst_os_flag);
    #
    # Check if the destination directory has been created
    #
    if (!-d $ofile_dir_modified) 
    {
		$logging_obj->log("EXCEPTION","Resync output directory does not exist yet for ".$obj->{$DPSO_srchostname}." (".$obj->{$DPSO_srcvol}.") ==> ".$obj->{$DPSO_dsthostname}." (".$obj->{$DPSO_dstvol}.") \n");
        return 0;
    }
    
    my $srchostid = $obj->{$DPSO_srchostid};
    my $srcvol = &formatDeviceName($obj->{$DPSO_srcvol});
    my $dsthostid = $obj->{$DPSO_dsthostid};
    my $dstvol = &formatDeviceName($obj->{$DPSO_dstvol});
	
    #
    # Check for compression
    #
    my $compression_enable = $obj->{$DPSO_compression_enable};
    #
    # Check if destination directories have been created for 1 to Many case
    #       
    my %one_to_many_targets;
	my $i = 0; 
	my $one_to_many_targets_array = $obj->{$DPSO_one_to_many_pairs}->{$obj->{$DPSO_srchostid}."!!".$obj->{$DPSO_srcvol}};	
    if ($obj->{$DPSO_one_to_many_source})
    {
        # 1-Many source, check if other directories have been created 
        # before proceeding        
        foreach my $ref (keys %{$one_to_many_targets_array})
        {
			my @destination_info = split(/!!/,$one_to_many_targets_array->{$ref});
			my $new_os_flag = $obj->{$DPSO_host_hash}->{$destination_info[0]}->{'osFlag'};
            my $other_ofile_dir = Utilities::makePath ($obj->{$DPSO_basedir}."/".$destination_info[0]."/".$destination_info[1]."/".$RS_IODIR,$new_os_flag);
            my $other_destfile_dir = Utilities::makePath($obj->{$DPSO_basedir}."/".$destination_info[0]."/".$destination_info[1],$new_os_flag);
            if (!-e $other_ofile_dir)
            {
				mkpath($other_ofile_dir, { mode => 0777 });
				
				$logging_obj->log("EXCEPTION","Created directory for 1-N replication: $other_ofile_dir");
            }
            #if ($destination_info[5] eq 0 )
            #{
             #   $targetFlag=&clearTargetFiles($other_ofile_dir,$other_destfile_dir);
            #}
            $one_to_many_targets{$i}->{'other_ofile_dir'} = $other_ofile_dir;
            $i++;
        }
    }
	
    if( Utilities::isWindows() == 0 )
    {
        #
        # Bug 4590 - Replaced chmod command with perl's chmod function
        #
        chmod (0777, $ofile_dir_modified);
        chmod (0777, $destfile_dir);
    }
    #
    #Added by BSR to fix bug# 1741
    #
    if( &shouldQuit( $obj->{$DPSO_basedir} ) ) 
    {
        $logging_obj->log("DEBUG","Exiting the process as requested");
        exit 1;
    }
    #End of fix for 1741
    #
    # check the JobId Value is Zero
    #
    my $jobId = $obj->{$DPSO_jobId};
	my $http_call_failure = 'FALSE';
	my $db_jobid = 0;
    if ($jobId == 0)
    { 	
		my $this_pair_id = $obj->{$DPSO_pair_id};
		my @jobid_array = split(/\!\!/,$this_pair_id);
		$this_pair_id = $jobid_array[0];
		my $api_output = tmanager::call_cx_api_for_data($this_pair_id,"getPsConfiguration");
		$logging_obj->log("DEBUG","API output is $api_output for paid id $this_pair_id.\n");
		if (! defined $api_output )
		{
            $logging_obj->log("EXCEPTION","API output is $api_output for paid id $this_pair_id . Hence returning from this instruction point.\n");
			return 0;
		}
		else
		{
			my $db_job_id = $api_output;
			$logging_obj->log("DEBUG","DB Job id: $db_job_id for pair $this_pair_id.\n");
			if ($db_job_id != 0)
			{
                $logging_obj->log("EXCEPTION","Jobid has been updated with $db_job_id in database for pair id $this_pair_id, but PS event data cache file is yet to update with CS latest database data. Hence, returning from this instruction point.\n");
				return 0;
			}
			else
			{
                $logging_obj->log("DEBUG","As Jobid is 0 for pair id $this_pair_id, hence, control is allowing to cleanup the target and source folders at process server.\n");
            #
            # Make sure Source and Destination directories are Empty ..
            #
            my $onetomanySourceCount = scalar(keys %{$one_to_many_targets_array});		
            if($onetomanySourceCount <= 1)
            {
                #
                # clear source  Files
                #
                my $source_file_dir = $obj->{$DPSO_basedir} . "/" .$obj->{$DPSO_srchostid} . "/" .$obj->{$DPSO_srcvol};
                my $source_file_dir_modified = Utilities::makePath ($source_file_dir,$src_os_flag);
                $sourceFlag=&clearSourceFiles($source_file_dir_modified);
            }
        
            #
            # clear only target Files
            #
            $targetFlag=&clearTargetFiles($ofile_dir_modified,$destfile_dir);
            #
            # update JOB ID for these pairs
            #
            #Start of Fix 10435
            my $srcCompatibilityNo = $obj->{$DPSO_host_hash}->{$srchostid}->{'compatibilityNo'};
            my $dstCompatibilityNo = $obj->{$DPSO_host_hash}->{$dsthostid}->{'compatibilityNo'};
            my $tmId = $obj->{$DPSO_tmid};
            my $otherClusterHostId = $obj->{$DPSO_clusterids};
            
            $logging_obj->log("DEBUG","this is not exception, doResync:: srcCompatibilityNo & dstCompatibilityNo are $srcCompatibilityNo & $dstCompatibilityNo ");		
            if (($srcCompatibilityNo >= 550000) && ($dstCompatibilityNo >= 550000))
            {		
                my $ref_replicationCleanupOptions = $obj->{$DPSO_replication_cleanup_options};				
                my $ref_replication_status = $obj->{$DPSO_replication_status};
                my $ref_restartResyncCounter = $obj->{$DPSO_restart_resync_counter};
                my $ref_resyncStartTime = $obj->{$DPSO_resync_start_time};
                #DebugTimeshotLog("replicationCleanupOptions, replication_status & ref_resyncCounter values are".$ref_replicationCleanupOptions." , ".$ref_replication_status." & ".$ref_restartResyncCounter."\n");
                $logging_obj->log("DEBUG","this is not exception, doResync:: replicationCleanupOptions, replication_status & ref_resyncCounter values are ".$ref_replicationCleanupOptions." , ".$ref_replication_status." & ".$ref_restartResyncCounter." , ".$ref_resyncStartTime."\n");
                if($ref_restartResyncCounter >= 1)
                {
                    if (($ref_replicationCleanupOptions != 0) && ($ref_replication_status == 42))
                    {	
                        my $ref_agent_heart_beat = $obj->{$DPSO_host_hash}->{$dsthostid}->{'lastHostUpdateTime'};
                        $logging_obj->log("DEBUG","last host update time :: $ref_agent_heart_beat , resyncStartTime :: $ref_resyncStartTime\n");
                        if($ref_agent_heart_beat > $ref_resyncStartTime)
                        {
                            $logging_obj->log("INFO","Restart re-sync update jobid for $srchostid, $srcvol,$dsthostid,$dstvol \n");				
                            #Update replication_cleanupOptions & replication_status with 0
                            #&update_replication_details($dsthostid,$dstvol);
                            
                            #Update jobId at the end
                            &update_jobid($srchostid, $srcvol,$dsthostid,$dstvol,$srcCompatibilityNo,$dstCompatibilityNo,$tmId,$otherClusterHostId,$ref_restartResyncCounter);
                        }
                    }
                    else
                    {
                        return 0;
                    }
                }			
                #need to add elsif cond for newly added pair, if satisfied  do cs-less replication cache & then update jobId 
                elsif($ref_restartResyncCounter == 0)
                {
					$logging_obj->log("INFO","First time jobid update for $srchostid, $srcvol,$dsthostid,$dstvol \n");				
                    &update_jobid($srchostid, $srcvol,$dsthostid,$dstvol,$srcCompatibilityNo,$dstCompatibilityNo,$tmId,$otherClusterHostId,$ref_restartResyncCounter);
                }			
                else
                {
                    return 0;
                }
            }
        }
        }
    }
	#END of Fix 10435

    #
    # Scan for files with input prefix
    # Fast resync
    # Bug 4590 - Removed the use of system ls command and replaced by
    # Utilties::read_directory_contents. This should make it more faster.
    #
    my $sub_string = "nomore";
    my @file_list = Utilities::read_directory_contents ($ifile_dir,'^tmp_completed_sync_.*?\.dat');
    my $file_list_length = @file_list;    
    foreach my $ifile (@file_list)
    {
        #Added by BSR to fix bug# 1741
        if( &shouldQuit( $obj->{$DPSO_basedir} ) ) 
        {
			$logging_obj->log("DEBUG","Exiting the process as requested");
            exit 1;

        }

        my $index = index $ifile,$sub_string;
        #
        # If file list length is more than one and the processed files is 
        # having nomore substring, then don't process it. Once all the 
        # file process finishes, then only do the no more file process.
        #
        if (($file_list_length > 1) && ($index != -1))
        {
            next;
        }

        chomp $ifile;
        $ifile = $ifile_dir . "/" . $ifile;
        my $file_modified = Utilities::makePath($ifile,$dst_os_flag);
        my $ofile = basename ($ifile);
        $ofile =~ s/^$FS_IFILE_PFX(.*)/$FS_OFILE_PFX$1/;
        my $output_file_modified = Utilities::makePath ($ofile_dir . "/" .$ofile,$dst_os_flag);
        my $original_file = $file_modified ;
        #
        # Compress and rename sync data file
        #
        if ($compression_enable == 1 )
        {
			my $file_ext_index = index $original_file,$GZIP_EXTENSION;
			#If the original file is not compressed, then only allow for compression.
			if ( $file_ext_index == -1)
			{
				#added by BSR to fix bug#1801
				$file_modified .= $GZIP_EXTENSION ;
				my $compression_status = &compress_file($obj, $original_file,$file_modified);
				if( $compression_status != 1 )
				{
					$logging_obj->log("EXCEPTION","Compressing the file $file_modified failed, rc: $?");

					#
					# Bug 4824 - If the compression failed, remove the file before 
					# we return out of the loop else this file will never 
					# get removed (even if we overwrite the file during the 
					# next iteration) and can lead to resync stuck issues.
					#
					unlink( $file_modified );
					return 0;
				}
			}
			#If the file is already compressed one, allow it the original file for gzip test.
			else
			{
				$file_modified = $original_file;
			}
            # Bug fix for #1390
            # Check whether file is compressed successfully or not
            my $gzip_test = `gzip -tv "$file_modified"`;
            if ($? != 0) 
            {
                $logging_obj->log("DEBUG","The file $file_modified fails a GZIP test, rc: $gzip_test");
                unlink( $file_modified ) ;
                return 0;
            }
            else 
            {
                $output_file_modified = $output_file_modified .$GZIP_EXTENSION ; 
            }
        }
        my $ren_output = rename($file_modified, $output_file_modified);
        if (!$ren_output) 
        {
			$logging_obj->log("EXCEPTION","Rename for fast resync file failed: $file_modified");
			
            if( $compression_enable == 1 )
            {
                unlink( $file_modified ) ;
            }
            return 0;
        }
        else
        {
            #added by BSR to fix bug#1801
            if( $compression_enable == 1) 
            {
                my $unlink_output = unlink( $original_file ) ;
                if( !$unlink_output )
                {
					$logging_obj->log("EXCEPTION","Deleting fast resync file failed: $original_file\n. This file would be applied multiple times");
                }
            }
        }
    }
    
    #Added by BSR to fix bug# 1741
    if( &shouldQuit( $obj->{$DPSO_basedir} ) ) 
    {
        $logging_obj->log("DEBUG","Exiting the process as requested");
        exit 1;
    }

}

##
#
# Function: Process block changes files
#
##
sub doExpandChanges 
{
	my ($obj) = @_;
	my $num_files_processed = 0;
	my $pStartTime = time();

	#Added by BSR to fix bug# 1801
	if( &shouldQuit( $obj->{$DPSO_basedir} ) ) 
	{
		$logging_obj->log("DEBUG","Exit from TimeshotManager as Exiting the process as requested shouldQuit");
		exit 1;
	}
	#End of fix for 1801
	# Compound constants
	# This is the directory where we will find the Sentinel uploaded files
	my $ifile_base =  $obj->{$DPSO_basedir} . "/" . $obj->{$DPSO_srchostid};
	
	my $src_os_flag = $obj->{$DPSO_host_hash}->{$obj->{$DPSO_srchostid}}->{'osFlag'};
	
	my $dst_os_flag = $obj->{$DPSO_host_hash}->{$obj->{$DPSO_dsthostid}}->{'osFlag'};
	$ifile_base = Utilities::makePath($ifile_base,$src_os_flag);

	my $ifile_dir =  $obj->{$DPSO_basedir} . "/" . $obj->{$DPSO_srchostid} . "/" . $obj->{$DPSO_srcvol} . "/" . ($ECB_IDIR);
	$ifile_dir = Utilities::makePath($ifile_dir,$src_os_flag); 

	my $resync_dir = "$obj->{$DPSO_basedir}/$obj->{$DPSO_srchostid}/" ."$obj->{$DPSO_srcvol}/$RS_IODIR/";
	$resync_dir = Utilities::makePath($resync_dir,$src_os_flag);

	# This is where we will place the compressed difference file
	my $ofile_base =  $obj->{$DPSO_basedir} . "/" . $obj->{$DPSO_dsthostid}; 
	$ofile_base = Utilities::makePath($ofile_base,$dst_os_flag);
	my $ofile_dir =  $obj->{$DPSO_basedir} . "/" . $obj->{$DPSO_dsthostid} . "/" .$obj->{$DPSO_dstvol};
	$ofile_dir = Utilities::makePath($ofile_dir,$dst_os_flag);

	$logging_obj->log("DEBUG","The input & output file directory is $ifile_dir , $ofile_dir \n");

	# Check if the input directory has been created. Create it if it doesn't.
	if (!-d $ifile_dir) 
	{
		if (0 != create_dir($ifile_dir, $obj, $DPSO_logfile)) 
		{
			 return -3;
		}
	}
		
	# set up diffdatasort dirs, this will include associated cluster 
	# dirs (if any) so they will all be sorted at the same time
	# also make sure all cluster input dirs are created

	# my $diffdatasort_dirs = '"'.$ifile_dir.'"/'; 
	# 	fix 3890
	#	End to End wind2k3 sp2_ Replication pair is struck at resynce step2 (source volume is mount point).
	my $diffdatasort_dirs = "\"".$ifile_dir."/\""; 
	foreach my $id (@{$obj->{$DPSO_clusterids}})  
	{
		#4075 & 4156 --- makePath function call has been removed for diffdatasort_dirs to get the proper diffdatasort ouput.
		
		my $diffdpso = "$obj->{$DPSO_basedir}/$id/$obj->{$DPSO_srcvol}/$ECB_IDIR/";
		
		my $cluster_id_os_flag = $obj->{$DPSO_host_hash}->{$id}->{'osFlag'};
		
		$diffdpso = Utilities::makePath($diffdpso,$cluster_id_os_flag);
		$diffdpso = "\"".$diffdpso."/\"";
		$diffdatasort_dirs = "$diffdatasort_dirs"." $diffdpso";
		my $clust_dir = "$obj->{$DPSO_basedir}/$id/$obj->{$DPSO_srcvol}/$ECB_IDIR";
		$clust_dir = Utilities::makePath($clust_dir,$cluster_id_os_flag);
		if (!-d $clust_dir) 
		{
			 if (0 != create_dir($clust_dir, $obj, $DPSO_logfile)) 
			 {
				  return -3;
			 }
		}
	}

	#$diffdatasort_dirs = Utilities::makePath($diffdatasort_dirs);
	if( Utilities::isWindows() == 0 )
	{
		#
		# Bug 4590 - Replaced chmod command with perl's chmod function
		#
		chmod (0777, $ifile_base);
	}
	
	# Create <guid>/<vol>/resync directory if missing.
	
	unless( -d $resync_dir ) 
	{
		eval { mkpath( $resync_dir , { mode => 0777 }); };
		if( $@ )
		{ 
			$logging_obj->log("DEBUG","Could not create $resync_dir");
			return -3;
		}
		$logging_obj->log("DEBUG","Created resync directory $resync_dir");
	}

	# Check if the destination directory has been created
	# if not create the destination directory
	if (!-d $ofile_dir)
	{
		my $mkpath_output = mkpath($ofile_dir, { mode => 0777 });
		if ($? != 0) 
		{
			$logging_obj->log("DEBUG","Could not create output directory $ofile_dir");
			return -3;
		}
		# Make sure the directory allows for deletion by the windows hosts
		if( Utilities::isWindows() == 0 )
		{
		  #
		  # Bug 4590 - Replaced chmod command with perl's chmod function
		  #
		  chmod (0777, $ofile_dir)
		}
		$logging_obj->log("DEBUG","Created output directory $ofile_dir");
	}
	
	if( Utilities::isWindows() == 0 )
	{
		#
		# Bug 4590 - Replaced chmod command with perl's chmod function
		#
		chmod (0777, $ofile_base);
	}

	my $srchostid = $obj->{$DPSO_srchostid};
	my $srcvol = &formatDeviceName($obj->{$DPSO_srcvol});
	my $dsthostid = $obj->{$DPSO_dsthostid};
	my $dstvol = &formatDeviceName($obj->{$DPSO_dstvol});

	# Check for compression
	my $compression_enable;
	my $lun_id;
	my $lun_state;
	my $flag = 0;
	if($obj->{$DPSO_execution_state} == 1 || $obj->{$DPSO_execution_state} == 0 || $obj->{$DPSO_execution_state} == 4)
	{
		$compression_enable = $obj->{$DPSO_compression_enable};
		$lun_id = $obj->{$DPSO_phy_lunid};
		$lun_state = $obj->{$DPSO_lun_state};
		$flag = 1;
	}

	my $is_rpo_valid = 1;
	if($lun_id ne '0' && $lun_id ne '' && $lun_state != Common::Constants::PROTECTED)
	{
		$is_rpo_valid = 0;
	}
	my $one_to_many = $obj->{$DPSO_one_to_many_source};
	# Check if destination directories have been created for 1 to Many case
	my %one_to_many_targets;
	#my @one_to_many_dsthostid;
	#my @one_to_many_dstvol;
	#my @one_to_many_resync_start_times;

	if ($one_to_many)
	{

		# 1-Many source, check if other directories have been created before proceeding		
		my $one_to_many_targets_array = $obj->{$DPSO_one_to_many_pairs}->{$obj->{$DPSO_srchostid}."!!".$obj->{$DPSO_srcvol}};
		my $i = 0;
		foreach my $ref (keys %{$one_to_many_targets_array})
		{
			my @dest_info = split(/!!/,$one_to_many_targets_array->{$ref});
			my $new_os_flag = $obj->{$DPSO_host_hash}->{$dest_info[0]}->{'osFlag'};
			my $other_ofile_dir = Utilities::makePath ( $obj->{$DPSO_basedir}."/".$dest_info[0]."/".$dest_info[1]."/" ,$new_os_flag);
			my $other_resync_ofile_dir = Utilities::makePath ( $obj->{$DPSO_basedir}."/".$dest_info[0]."/".$dest_info[1]."/".$RS_IODIR."/",$new_os_flag);

			if (!-e $other_ofile_dir)
			{
				mkpath($other_ofile_dir, { mode => 0777 });
			}
			if (!-e $other_resync_ofile_dir)
			{
				mkpath($other_resync_ofile_dir, { mode => 0777 });
			}
			$one_to_many_targets{$i}->{'other_ofile_dir'} = $other_ofile_dir;
			$one_to_many_targets{$i}->{'dsthostid'} = $dest_info[0];
			$one_to_many_targets{$i}->{'dstvol'} = $dest_info[1];
			$one_to_many_targets{$i}->{'resyncStartTagtime'} = $dest_info[2];
			$one_to_many_targets{$i}->{'autoResyncStartTime'} =  $dest_info[3];
			$one_to_many_targets{$i}->{'resyncSetCxTimeStamp'} =  $dest_info[4];
			$i++;
		}
	}
	else
	{
		return 0;
	}
	
	#Added by BSR to fix bug# 1801
	if( &shouldQuit( $obj->{$DPSO_basedir} ) ) 
	{
		$logging_obj->log("DEBUG","Exiting the process as requested");
		exit 1;
	}
	#End of fix for 1801
  
    #
    # 5621 rpo missreporting
    #
	my $cluster_ifile_dir;
	my $cluster_host_id;
	my $cluster_monitor_file;
	my $monitor_file = $ifile_dir . "/monitor.txt";
	$monitor_file = Utilities::makePath($monitor_file,$src_os_flag);
	my $fileName;
    #
    # END of 5621 rpo missreporting
    #
  
	###########   DIFF DATA SORT : 9448  ##############
	my $diff_data_sort_command;
	my $diffdatasort = Utilities::makePath ("$DIFFDATASORT");

	#Fix for 10435 - removed -s option from diffdatasort cmd 
	#$diff_data_sort_command = "$diffdatasort -p $ECB_IFILE_PFX $diffdatasort_dirs";		
	$diff_data_sort_command = '"'.$diffdatasort.'"'." -p $ECB_IFILE_PFX $diffdatasort_dirs";		
	$logging_obj->log("DEBUG","diff_data command :: $diff_data_sort_command \n");
	my @ls_output = `$diff_data_sort_command 2>&1`;

	if ($? != 0)
	{
		$logging_obj->log("DEBUG","Error occurred while running diff_data_sort_command : $diff_data_sort_command $!\n");
	}
	else
	{
		$logging_obj->log("DEBUG","Success diffdata sort command diff_data_sort_command : $diff_data_sort_command \n");
	}
	###########   9448  ##############

	if ($? != 0)
	{
		#Added by BSR to fix bug# 1801
		if( &shouldQuit( $obj->{$DPSO_basedir} ) ) 
		{
			$logging_obj->log("DEBUG","Exit from TimeshotManager as Exiting the process as requested shouldQuit");
			exit 1;
		}
		#End of fix for 1801
		return 0;
	}
	else
	{ 	 
		#Added by BSR to fix bug# 1801
		if( &shouldQuit( $obj->{$DPSO_basedir} ) ) 
		{
			$logging_obj->log("DEBUG","Exit from TimeshotManager as Exiting the process as requested shouldQuit");
			exit 1;
		}
		#End of fix for 1801
	
	
	    #
        #5621 rpo missreporting
        #
        my $pre_file_time_stamp = 0;
        my $fileType = 0;
        if($flag)
		{
			if(-e $monitor_file)
			{
				if(!@ls_output)
				{
					open FH, $monitor_file || print "could not open the file for reading";
					my @file_content = <FH>; 
					close(FH);
					chomp($file_content[0]);
					$fileName = $file_content[0];
					my @time_stamps = split(/:/,$file_content[1]);
					$pre_file_time_stamp = $time_stamps[0]; 
					$fileType = $time_stamps[2];
					if($pre_file_time_stamp!=0)
					{
						if($is_rpo_valid)
						{
							&monitor_tmanager($ifile_dir,$pre_file_time_stamp,$fileName,$src_os_flag);
						}
						foreach $cluster_host_id (@{$obj->{$DPSO_clusterids}}) 
						{
							$cluster_ifile_dir = "$obj->{$DPSO_basedir}/$cluster_host_id/$obj->{$DPSO_srcvol}/$ECB_IDIR";
							if($is_rpo_valid)
							{
								&monitor_tmanager($cluster_ifile_dir,$pre_file_time_stamp,$fileName);
							}
						}
					}
				}
			}
			else
			{
				$fileName = "No File";
				$pre_file_time_stamp = '';

				if($is_rpo_valid)
				{
					&monitor_tmanager($ifile_dir,$pre_file_time_stamp,$fileName,$src_os_flag);
				}
				foreach $cluster_host_id (@{$obj->{$DPSO_clusterids}}) 
				{
					$cluster_ifile_dir = "$obj->{$DPSO_basedir}/$cluster_host_id/$obj->{$DPSO_srcvol}/$ECB_IDIR";
					if($is_rpo_valid)
					{
						&monitor_tmanager($cluster_ifile_dir,$pre_file_time_stamp,$fileName);
					}
				}
			}
		}
		
        #END of 5621 rpo missreporting
		my $called_from = "DB_UP";
		if($obj->{$DPSO_db_state})
		{
			$called_from = "DB_DOWN";
		}

        if(scalar(@ls_output) > 0)
        {
            $logging_obj->log("DEBUG","doExpandChanges:: $srchostid,$srcvol,$dsthostid,$dstvol,$ifile_dir,$ofile_dir ,$compression_enable,$src_os_flag,$dst_os_flag,$is_rpo_valid \n\n");
        }
		diffsFileProcess(\@ls_output,\%one_to_many_targets,$srchostid,$srcvol,$dsthostid,$dstvol,$ifile_dir,$ofile_dir,$compression_enable,$src_os_flag,$dst_os_flag,$is_rpo_valid,$obj,$called_from ,$one_to_many);
	}

	# Success
	return 0;
}



##
# Returns the "startingPhysicalOffset" of a (src) volume
#
# Inputs: hostid, volume
##
sub getStartingPhysicalOffset {
  my ($dbh, $hostid, $volume) = @_;
  my $offset = 0;
  $volume = &formatDeviceName($volume);
 

  my $sql = "SELECT $LV_startingPhysicalOffset from $LV_TBL
             WHERE $LV_hostId='$hostid' AND  $LV_deviceName='$volume'";

  #print "executing sql $sql\n";
  my $sth = $dbh->prepare($sql);
  $sth->execute();
  while (my $ref = $sth->fetchrow_hashref()) {
    $offset =  $ref->{$LV_startingPhysicalOffset};
  }

  $sth->finish();

  return $offset;
}

##
# Tries to get additional information about the block volume
# Note: No writes will/should be generated by this method.
#
# Returns an hash:
#  $fs_type, $file_list etc. etc.
##
sub getVolumeInfo() {
  my ($volume) = @_;


}

##
# Miscellaneous helper functions that shouldn't be used outside this package
#
##

##
# Set sensible defaults for all members of the object
##
sub initializeNewObject {
  my ($obj) = @_;
  my $error = 0;
	
  my $src_os_flag = $obj->{$DPSO_host_hash}->{$DPSO_srchostid}->{'osFlag'};
  my $dst_os_flag = $obj->{$DPSO_host_hash}->{$DPSO_dsthostid}->{'osFlag'};
  # Log and Lock files (depend on type)
  if ($obj->{$DPSO_type} eq $DPO_TYPE_IS) 
  {
    $obj->{$DPSO_logfile} = $obj->{$DPSO_basedir}."/".$obj->{$DPSO_srchostid}."/".$obj->{$DPSO_srcvol}."/is.log";
	$obj->{$DPSO_logfile} = Utilities::makePath($obj->{$DPSO_logfile},$src_os_flag);
    $obj->{$DPSO_lockfile} = $obj->{$DPSO_basedir}."/".$obj->{$DPSO_srchostid}."/".$obj->{$DPSO_srcvol}."/is.lock";
  } 
  else 
  {
    $error = -1;
    print "Unknown or missing type info: $obj->{$DPSO_type}\n";
	$logging_obj->log("EXCEPTION","Unknown or missing type info: $obj->{$DPSO_type}");
  }

  # Set the fallback global file value
  $obj->{$DPSO_gbllogfile}                 = $obj->{$DPSO_basedir}."/tm.log";

  $obj->{$DPSO_status}                     = 0;

  $obj->{$DPSO_is_blocksize}               = 8192;  # Keep in sync with count
  $obj->{$DPSO_is_blocks_left}             = 1;     # Are more blocks remaining
  $obj->{$DPSO_num_of_initsync_files}      = 0;     # Start at 0 files waiting for OA
  $obj->{$DPSO_is_curr_filenum}            = 0;     # Current File Number Midfix
  $obj->{$DPSO_is_count}                   = 20480; # This results in a 80MB file with 8k block size
  $obj->{$DPSO_is_polling_interval}        = 10;    # In seconds
  $obj->{$DPSO_is_enable_compression}      = 1;	    # Do not compress data files
  $obj->{$DPSO_is_enable_pipelining}       = 1;	    # Do not pipeline data files
  $obj->{$DPSO_is_max_files_pending}       = 2;     # Num of init sync files generated
                                                    #  before waiting on Outpost Agent
  $obj->{$DPSO_is_pipeline_file}           = "is.pipeline"; # Persistent pipeline file for initial sync state
  $obj->{$DPSO_is_skip}                    = 0;	    # Start at offset 0 on source volume

  $obj->{$DPSO_ecb_process_loop_wait}      = 10;    # In seconds
  $obj->{$DPSO_ecb_enable_compression}     = 1;     # Enable compression of ediff files
  $obj->{$DPSO_is_enable_coalescion}       = 0;     # Enable coalescion of the ediff files
  $obj->{$DPSO_ecb_enable_pipelining}      = 1;     # Enable pipelining of ediffs
  $obj->{$DPSO_ecb_max_files_pending}      = 4;     # Num of ediff files generated
                                                    # before waiting on Outpost Agent
  $obj->{$DPSO_ecb_pipeline_file}          = "ecb.pipeline"; # Persistent pipeline file for initial sync state

  $obj->{$DPSO_rh_enable_pipelining}       = 0;
  $obj->{$DPSO_rh_pipeline_file}           = "rh.pipeline";  # Persistent pipeline file for return home state
  $obj->{$DPSO_rh_max_files_local}         = 2;	 # Hold two rh files max at a time
  $obj->{$DPSO_rh_copy_mode}               = $FS_COPY; # Use plain fs copy command
  $obj->{$DPSO_rh_process_loop_wait}       = 10; # In seconds
  $obj->{$DPSO_rh_copy_loop_wait}          = 10; # In seconds

  $obj->{$DPSO_ecb_process_loop_wait}      = 10; # In seconds


  # Get the src symbolic name 
	$obj->{$DPSO_srchostname} = $obj->{$DPSO_host_hash}->{$obj->{$DPSO_srchostid}}->{'name'};
	if(@{$obj->{$DPSO_clusterids}})
	{
		$obj->{$DPSO_srchostname} = $obj->{$DPSO_srchostname}.",".$obj->{$DPSO_host_hash}->{$obj->{$DPSO_clusterids}}->{'name'};
	}
	
  # Get the destination symbolic name
  $obj->{$DPSO_dsthostname} = $obj->{$DPSO_host_hash}->{$obj->{$DPSO_dsthostid}}->{'name'};

  if ($obj->{$DPSO_ecb_enable_profiling} == 1) {
    printf "Enabled profiling mode for $obj\n";
	$logging_obj->log("DEBUG","Enabled profiling mode for $obj");
  }

  # RPO Log file
  $obj->{$DPSO_rpologfile} = $obj->{$DPSO_basedir}."/".$obj->{$DPSO_srchostid}."/".$obj->{$DPSO_srcvol}."/rpo.log";
  ##Gopal
  $obj->{$DPSO_rpologfile} = Utilities::makePath($obj->{$DPSO_rpologfile},$src_os_flag);

  # Performance log file
  $obj->{$DPSO_perflogfile} = $obj->{$DPSO_basedir}."/".$obj->{$DPSO_srchostid}."/".$obj->{$DPSO_srcvol}."/perf.log";
  $obj->{$DPSO_perflogfile} = Utilities::makePath($obj->{$DPSO_perflogfile},$src_os_flag);

  # Set the default message source for the object
  $obj->{$DPSO_msg_source} = "pair:".$obj->{$DPSO_srchostid}." ".$obj->{$DPSO_srcvol}." ".$obj->{$DPSO_dsthostid}." ".$obj->{$DPSO_dstvol};

  # Get the list of listeners that the traps need to go out to
  return $error;
}

##
#
# Function: Figures out if a file is compressed or not
#           uses the correct decompression program to
#           uncompress the file
#
# Returns: Decompressed file name
#
##
sub decompress_file {
  my($obj, $fullname) = @_;
  my ($file,$dir,$ext) = fileparse($fullname, qr/\.[^.]*/);
  my $unzipped_file = $dir . $file;

  if ($ext eq $GZIP_EXTENSION) {
    chdir($dir);
    my $gzip_output = `$GZIP -d "$fullname"`;
  }
  elsif ($ext eq $BZIP2_EXTENSION) {
    chdir($dir);
    my $bzip2_output = `BZIP2 -d "$fullname"`;
    $logging_obj->log("DEBUG","bunzipped $fullname: output : $bzip2_output");
    #$obj->{$DPSO_msg}->send_message ($obj->{$DPSO_msg_source}, 25, $MSG_FILE, "bunzipped $fullname: output : $bzip2_output\n");
  }
  else {
    $logging_obj->log("DEBUG","Unknown file extension, assuming uncompressed");
    #$obj->{$DPSO_msg}->send_message ($obj->{$DPSO_msg_source}, 25, $MSG_FILE, "Unknown file extension, assuming uncompressed\n");
    return $fullname;
  }

  return $unzipped_file
}

##
#
# Function: Compresses a file. Uses gzip compression
#
# Returns: Compressed file name
#
##
sub compress_file {
  my($obj, $fullname, $zipped_file) = @_;
  my ($file,$dir,$ext) = fileparse($fullname, qr/\.[^.]*/);
  #my $zipped_file = $fullname.$GZIP_EXTENSION;
  my $compression_status  = 0 ;

  if ($ext ne $GZIP_EXTENSION &&
      $ext ne $BZIP2_EXTENSION)
  {
    #my $gzip_output = `$GZIP $fullname`;
	my $gzip_output = `gzip -c "$fullname" > "$zipped_file"`;
    if ($? != 0) {
	  $logging_obj->log("EXCEPTION","Compression step failed ( $fullname > $zipped_file ) :: $gzip_output");

    }
    else {
      #$zipped_file = $fullname . $GZIP_EXTENSION;
	  $compression_status = 1 ;
    }
  }
  else {
      $logging_obj->log("DEBUG","File is already compressed");
      #$obj->{$DPSO_msg}->send_message ($obj->{$DPSO_msg_source}, 25, $MSG_FILE, "File is already compressed\n");
		 $compression_status = 1 ;
  }

  #return $unzipped_file;
  return $compression_status ;
}

sub monitorpairs
{
	my($pair_id,$destinationHostId,$destinationDeviceName,$src_id,$src_vol,$data_hash) = @_;
	my $current_throttle_resync = 0 ;
	my $input;
	my @input_parameters;
	my $pair_data = $data_hash->{'pair_data'};
	my $host_data = $data_hash->{'host_data'};
	my $org_pair_id = $pair_id."!!".$src_id;
	
	my $max_resync_threshold = $pair_data->{$org_pair_id}->{'maxResyncFilesThreshold'};
	my $db_throttle_resync = $pair_data->{$org_pair_id}->{'throttleresync'};
	my $dest_os_flag = $host_data->{$destinationHostId}->{'osFlag'};
	my $srchostname = $host_data->{$src_id}->{'name'};
	my $compatibilityNo = $host_data->{$src_id}->{'compatibilityNo'};
	my $dsthostname = $host_data->{$destinationHostId}->{'name'};
	
	my $resync_files_threshold =($max_resync_threshold =~ /^\d+/) ? $max_resync_threshold : $MAX_RESYNC_FILES_THRESHOLD;
	my $resync_files_path = $REPLICATION_ROOT."/".$destinationHostId."/".$destinationDeviceName."/resync/";
	$resync_files_path = Utilities::makePath($resync_files_path,$dest_os_flag);
	
	my $totaluse;
	my $use;
	my $dataUnitConversionResync;
	
	if($compatibilityNo <= $MAX_RESYNC_THROTTLE_COMPAT_VERSION)
	{
		$totaluse = calculatedirectorysize($resync_files_path);	
		$use = $totaluse - 4096;
		$dataUnitConversionResync = convertDataInto($max_resync_threshold);
		
		if ($use > $resync_files_threshold && $resync_files_threshold != 0) 
		{
			$current_throttle_resync = 1 ;
			my $subject = "TRAP on replication pair";		
			my $err_message = "Resync cache folder for the pair ".$srchostname." (".$src_vol.") ==> ".$dsthostname." (".$destinationDeviceName.")  has exceeded the configured threshold of $dataUnitConversionResync\n\n";				
			my $err_summary = "Resync Data flow controlled - Resync Cache Folder for replication pair has exceeded configured threshold";
			my $err_id = $src_id."_".$src_vol."_".$destinationHostId."_".$destinationDeviceName;
			
			$err_message =~ s/<br>/ /g;
			$err_message =~ s/\n/ /g;
			
			my %err_placeholders;
			$err_placeholders{"SrcHostName"} = $srchostname;
			$err_placeholders{"SrcVolume"} = $src_vol;
			$err_placeholders{"DestHostName"} = $dsthostname;
			$err_placeholders{"DestVolume"} = $destinationDeviceName;
			$err_placeholders{"VolumeSize"} = $dataUnitConversionResync;
			
			my $args;
			$args->{"id"} = $src_id;
			$args->{"error_id"} = $err_id;
			$args->{"summary"} = $err_summary;
			$args->{"err_temp_id"} = "CXSTOR_WARN";
			$args->{"err_msg"} = $err_message;
			$args->{"err_code"} = "EC0110";
			$args->{"err_placeholders"} = \%err_placeholders;
			my $alert_info = serialize($args);
			
			my $trap_args;
			$trap_args->{"err_msg"} = $err_message;
			$trap_args->{"err_temp_id"} = "CXSTOR_WARN";
			my $trap_info = serialize($trap_args);
			&tmanager::add_error_message($alert_info, $trap_info);
			
			$logging_obj->log("EXCEPTION","This is not an Exception : Resync Data flow controlled".$err_message);
			$logging_obj->log("EXCEPTION","This is not an Exception : Differential Data flow controlled".$err_message);
		}
	}
	$logging_obj->log("DEBUG","monitorpairs db_throttle_resync::$db_throttle_resync, current_throttle_resync::$current_throttle_resync");
	if($db_throttle_resync != $current_throttle_resync)
	{
		$input->{0}->{"queryType"} = "UPDATE";
		$input->{0}->{"tableName"} = "$SRC_DST_LV_TBL";
		$input->{0}->{"fieldNames"} = "throttleresync=$current_throttle_resync";
		$input->{0}->{"condition"} = "pairId='$pair_id'";
		push(@input_parameters,$input);
	}			
	undef $current_throttle_resync;
	undef $pair_id;
	undef $max_resync_threshold;
	undef $db_throttle_resync;
	undef $destinationHostId;
	undef $destinationDeviceName;
	undef $current_throttle_resync;
	undef $dest_os_flag;
	undef $resync_files_threshold;
	undef $resync_files_path;
	undef $totaluse;
	undef $use;
	undef $dataUnitConversionResync;
	return \@input_parameters;
	# Any pending traps will be still sent. The coalescing of trap send happens elsewhere    
}

sub convertDataInto
{
	my ($sizeGB) = @_;
	my @names = ('B', 'KB', 'MB', 'GB', 'TB','PB', 'EB');
    my $times = 0;
    while($sizeGB>1024)
    {
        $sizeGB = $sizeGB/1024;
        $times++;
		if($times == 6)
		{
			last;
		}
    }    
	my $size = $sizeGB." ".$names[$times];
    return $size;
}

sub calculatedirectorysize
{
	my($dir) = @_ ;

	#$dir = Utilities::makePath($dir); // COMMENTD BECOZ WE ALREADY CALLED makePath at invocation of calculatedirectorysize
  
	#Start - issue#2249
	my $totalsize ; #= ( stat "$dir" )[7] ;
	if( $HAS_NET_IFCONFIG == 1 )
	{
		 use if Utilities::isWindows(), "Win32::UTCFileTime"  ;
		$totalsize = (Win32::UTCFileTime::file_alt_stat("$dir"))[7] ;
	}
	else
	{
		$totalsize = (stat "$dir" )[7] ;
	}
	#End - issue#2249
	
	if( opendir (DIRHANDLE, $dir) )
	{
		while ( my $file = readdir (DIRHANDLE) )
		{
			#Start - issue#2249
			my $filesize ; #= ( stat "$dir/$file" )[7] ;
			if( $HAS_NET_IFCONFIG ==1 ) 
			{
				 use if Utilities::isWindows(), "Win32::UTCFileTime"  ;
				$filesize = (Win32::UTCFileTime::file_alt_stat("$dir/$file"))[7] ;
			}
			else
			{
				$filesize = (stat  "$dir/$file")[7] ;
			}
			#End - issue#2249
		
			$totalsize += $filesize ;
		}
	}
  return $totalsize ;
}

#  
# Function Name: autoResyncOnResyncSet  
# Description: 
#    Auto resync if required
# Parameters: None 
# Return Value: None
#
sub autoResyncOnResyncSet() 
{
	my ($api_output) = @_;
	$logging_obj->log("DEBUG","Start of autoResyncOnResyncSet");
	my $prev_scenario_id = '';
	my $num_pairs_resyncing;
	my $batch_resync;
	my $batch;
	
	my $pair_data = $api_output->{'pair_data'};
	my $host_data = $api_output->{'host_data'};
	my $plan_data = $api_output->{'plan_data'};
	my $lv_data = $api_output->{'lv_data'};
	my $i=0;
	my $do_pause = 0;
	foreach my $pair_id (keys %{$pair_data}) 
	{
		my $scenario_id = $pair_data->{$pair_id}->{'scenarioId'};
			
		if($pair_data->{$pair_id}->{'resyncOrder'} == 1) {				
			#if already scenarioId exists in the array then update the pair count
			if(exists $batch_resync->{$scenario_id} ) { 
				$batch_resync->{$scenario_id} = $batch_resync->{$scenario_id} + 1;
			} else {	
				$batch_resync->{$scenario_id} = 1;
			}
		}
		
		if($pair_data->{$pair_id}->{'isQuasiflag'} == 0) {		
			
			#if already scenarioId exists in the array then update the pair count
			if(exists $num_pairs_resyncing->{$scenario_id} ) {
				$num_pairs_resyncing->{$scenario_id} = $num_pairs_resyncing->{$scenario_id} + 1;
			} else {	
				$num_pairs_resyncing->{$scenario_id} = 1;
			}
		}	
	}
	
	foreach my $pair_id (keys %{$pair_data}) 
	{
		if($pair_data->{$pair_id}->{"autoResyncStartTime"} !=0 ) 
		{		
			my $input;
			
			my @actual_pair_array = split(/\!\!/,$pair_id);								
			my $org_pair_id = $actual_pair_array[0];
			
			my $srchostid = $pair_data->{$pair_id}->{'sourceHostId'};
			my $dsthostid =  $pair_data->{$pair_id}->{"destinationHostId"};
			my $srcvol = $pair_data->{$pair_id}->{"sourceDeviceName"};
			my $dstvol = $pair_data->{$pair_id}->{"destinationDeviceName"};
			my $auto_resync_start_type = $pair_data->{$pair_id}->{"autoResyncStartType"};
			my $auto_resync_start_time = $pair_data->{$pair_id}->{"autoResyncStartTime"};
			my $auto_resync_start_hours = $pair_data->{$pair_id}->{"autoResyncStartHours"};
			my $auto_resync_start_minutes = $pair_data->{$pair_id}->{"autoResyncStartMinutes"};
			my $auto_resync_stop_hours = $pair_data->{$pair_id}->{"autoResyncStopHours"};
			my $auto_resync_stop_minutes = $pair_data->{$pair_id}->{"autoResyncStopMinutes"};
			my $replication_status = $pair_data->{$pair_id}->{"replication_status"};
			my $hour = $pair_data->{$pair_id}->{"hour"};
			my $minute = $pair_data->{$pair_id}->{"minute"};
			my $ruleId = $pair_data->{$pair_id}->{"ruleId"};				
			my $resync_set_timestamp = $pair_data->{$pair_id}->{"resyncSetCxtimestamp"};
			my $is_resync = $pair_data->{$pair_id}->{"isResync"};
			my $current_timestamp_on_cs = $pair_data->{$pair_id}->{"unix_time"};
			my $execution_state = $pair_data->{$pair_id}->{"executionState"};
			my $scenario_id = $pair_data->{$pair_id}->{"scenarioId"};
			my $phy_lun_id = $pair_data->{$pair_id}->{"Phy_Lunid"};
			my $plan_id = $pair_data->{$pair_id}->{"planId"};
			
			my $is_visible = $lv_data->{$dsthostid."!!".$dstvol}->{'visible'};
			
			my $auto_resync_start_window = $auto_resync_start_hours + ($auto_resync_start_minutes/60);
			my $auto_resync_stop_window = $auto_resync_stop_hours + ($auto_resync_stop_minutes/60);
			my $current_localtime = $hour + ($minute/60);
			
			if ($auto_resync_stop_window < $auto_resync_start_window)
			{
				if (($current_localtime < $auto_resync_start_window) && ($current_localtime < $auto_resync_stop_window))
				{
					$current_localtime += 24;
				}
				$auto_resync_stop_window += 24;
			}		
			
			if (($auto_resync_start_type == 0) && ($current_localtime >= $auto_resync_start_window) && ($current_localtime <= $auto_resync_stop_window))
			{			
				$logging_obj->log("DEBUG","autoResyncOnResyncSet::$phy_lun_id");
				
				my $PROTECTED = Fabric::Constants::PROTECTED ;
				my $RESYNC_MUST_REQUIRED = Fabric::Constants::RESYNC_MUST_REQUIRED ;
				my $RESYNC_PENDING = Fabric::Constants::RESYNC_PENDING ;
				my $RESYNC_NEEDED = Fabric::Constants::RESYNC_NEEDED ;			
				my $PAUSE_PENDING_VAL = Common::Constants::PAUSED_PENDING;
				my $PAUSE_BY_USER = Common::Constants::PAUSE_BY_USER_VAL;
				$logging_obj->log("DEBUG","autoResyncOnResyncSet::auto_resync_start_time:$auto_resync_start_time,current_timestamp_on_cs:$current_timestamp_on_cs,resync_set_timestamp:$resync_set_timestamp,is_resync:$is_resync,is_visible:$is_visible,replication_status:$replication_status");
				
				if (($auto_resync_start_time <= $current_timestamp_on_cs - $resync_set_timestamp) && ($resync_set_timestamp) && (!$is_resync) && (!$is_visible) && ($replication_status == 0))
				{
					$logging_obj->log("DEBUG","autoResyncOnResyncSet::if block::$scenario_id");
					if($scenario_id > 0)
					{
						my $pair_type = 1;
						
						if($phy_lun_id) 
						{
							my $pair_type = $plan_data->{$plan_id}->{'solutionType'};
							if($pair_type == 'ARRAY') {$pair_type = 3;}
							if($pair_type == 'PRISM') {$pair_type = 2;}
						}						
						$logging_obj->log("DEBUG","autoResyncOnResyncSet::pair_type>>$pair_type");
						
						if($pair_type == 2 || $pair_type == 3) # IN CASE OF PRISM
						{	
							$input->{"queryType"} = "UPDATE";
							$input->{"tableName"} = "hostApplianceTargetLunMapping";
							$input->{"fieldNames"} = "state = $RESYNC_PENDING";
							$input->{"condition"} = "sharedDeviceId = '$phy_lun_id' AND state = $PROTECTED AND processServerId IN (SELECT sourceHostId FROM srcLogicalVolumeDestLogicalVolume WHERE Phy_Lunid = '$phy_lun_id')";
							$batch->{$i} = $input;
							$i++;
							
							$input->{"queryType"} = "UPDATE";
							$input->{"tableName"} = "hostSharedDeviceMapping";
							$input->{"fieldNames"} = "resyncRequired = $RESYNC_MUST_REQUIRED";
							$input->{"condition"} = "sharedDeviceId = '$phy_lun_id'";
							$batch->{$i} = $input;
							$i++;
							
							$input->{"queryType"} = "UPDATE";
							$input->{"tableName"} = "srcLogicalVolumeDestLogicalVolume";
							$input->{"fieldNames"} = "lun_state = 1, shouldResync = $RESYNC_NEEDED";
							$input->{"condition"} = "pairId='$org_pair_id'";
							$batch->{$i} = $input;
							$i++;
						}
						
						# Get Batch resync value
						my $batch_resync_value = $batch_resync->{$scenario_id};
						
						# Get no of pairs in resync
						my $pairs_resyncing = $num_pairs_resyncing->{$scenario_id};
						if(!$pairs_resyncing)
						{
							$pairs_resyncing = $num_pairs_resyncing->{$scenario_id} = 0;
						}
						
						if($batch_resync_value!=0)
						{
							if($pairs_resyncing < $batch_resync_value)
							{
								#&resetResyncPairs($ruleId);
								$do_pause = 1;
								$num_pairs_resyncing->{$scenario_id} = 	$num_pairs_resyncing->{$scenario_id} + 1;
							}
							else
							{
								$logging_obj->log("INFO","autoResyncOnResyncSet: queued $org_pair_id");
								# Delay Auto Resync for pairs whose resync Order is not 1. Update executionState to 4(Delay Resync)							
								$input->{"queryType"} = "UPDATE";
								$input->{"tableName"} = "srcLogicalVolumeDestLogicalVolume";
								$input->{"fieldNames"} = "executionState=4";
								$input->{"condition"} = "pairId='$org_pair_id'";
								$batch->{$i} = $input;
								$i++;
							}
						}
						else
						{
							# This block is if batch resyn is not configured
							#&resetResyncPairs($ruleId);
							$do_pause = 1;
						}
					}
					else
					{
						#&resetResyncPairs($ruleId);
						$do_pause = 1;
					}
					if ($do_pause == 1)
					{
					
						my $api_output = tmanager::call_cx_api_for_data($srchostid,"getRollbackStatus");
						my $rollback_status;
						$logging_obj->log("INFO","API output is $api_output for source id $srchostid.\n");
						if (! defined $api_output )
						{
							$logging_obj->log("INFO","API output is $api_output for source id $srchostid . Hence returning from this instruction point.\n");
						}
						else
						{
							$rollback_status = $api_output;
							if ($rollback_status == 0)
							{
								$input->{"queryType"} = "UPDATE";
								$input->{"tableName"} = "srcLogicalVolumeDestLogicalVolume";
								$input->{"fieldNames"} = "replication_status = '$PAUSE_PENDING_VAL', tar_replication_status = 1, pauseActivity = '$PAUSE_BY_USER', autoResume = 0, restartPause = 1 ";
								$input->{"condition"} = "pairId='$org_pair_id' AND tar_replication_status = 0 AND src_replication_status = 0 AND deleted = 0";
								$batch->{$i} = $input;
								$logging_obj->log("INFO","autoResyncOnResyncSet: paused $org_pair_id");
								$i++;
							}
							else
							{
								$logging_obj->log("INFO","autoResyncOnResyncSet: $org_pair_id :, $srchostid , rollback is in progress and hence returning from here without initiating restart resync pause. \n");
							}
						}
					}
				}
			}	
		}
	}

	if($batch ne '') {		
		&tmanager::update_cs_db("auto_resync_on_resync_set",$batch);	
	}
	$logging_obj->log("DEBUG","End of autoResyncOnResyncSet");
}


#  
# Function Name: resetResyncPairs  
# Description: 
#    Reset resync by utilizing PHP scripts
# Parameters: None 
# Return Value: None
#
sub resetResyncPairs() 
{
	my($ruleId) = @_;
	
	my $response;
	
	eval
	{
		my $http_method = "POST";
		my $https_mode = ($AMETHYST_VARS->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $AMETHYST_VARS->{'PS_CS_IP'}, 'PORT' => $AMETHYST_VARS->{'PS_CS_PORT'});
		my $param = {'access_method_name' => "resync_now", 'access_file' => "/resync_now.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my %perl_args = ('rule_id' => $ruleId);
		my $content = {'content' => \%perl_args};
		
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);
		
		$logging_obj->log("DEBUG","Response::".Dumper($response));
		if ($response->{'status'})
		{
			$logging_obj->log("DEBUG","RULE ID posted : ".$ruleId." SUCCESS");
		}
		else
		{
			$logging_obj->log("EXCEPTION","RULE ID post failed: $ruleId");			
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : " . $@);
	}
}
#  
# Function Name: isDrivesVisible  
# Description: 
#    checks whether drive is in visible mode or not
# Parameters: None 
# Return Value: Returns 1 if drive is visible
#
sub isDrivesVisible()
{
	my($conn, $srcvol, $srchostid) = @_;
	my $is_visible = 0;

	my $sql = "SELECT destinationHostId, destinationDeviceName
			 FROM srcLogicalVolumeDestLogicalVolume WHERE 
			 (sourceHostId='$srchostid' AND 
			  sourceDeviceName='$srcvol')";

	my $result = $conn->sql_exec($sql);
	foreach my $ref (@{$result})		  
	{
		my $dest_hostid = $ref->{'destinationHostId'};
		my $dest_vol = $ref->{'destinationDeviceName'};
		$is_visible = $conn->sql_get_value('logicalVolumes', 'visible', "hostId='$dest_hostid' AND 
					 deviceName='$dest_vol'");
		if ($is_visible)
		{
			$is_visible = 1;
		}
	}

	return $is_visible;
}

#  
# Function Name: GetTagFilesInDir  
# Description: 
#    returns files matching the tag file pattern in a dir
# Parameters: directory path 
# Return Value: Returns array of files
#
sub GetTagFilesInDir()
{
	my ($dir) = @_;
	my $file_found = 0;
	my @tag_files = ();
	
	my $pattern = "^completed_ediffcompleted_diff_tag_P";
				
	opendir( DEST_DIR, $dir) || $logging_obj->log("INFO","Failed to opendir $dir") || return @tag_files;
	my @files = sort { $b <=> $a } readdir(DEST_DIR);
	closedir(DEST_DIR);
	
	# $logging_obj->log("INFO","GetTagFilesInDir $dir ".Dumper(\@files));
	
	@tag_files = grep (!(/^\./) && (/$pattern/), @files);
	
	# $logging_obj->log("INFO","GetTagFilesInDir $dir ".Dumper(\@tag_files));
	
	return @tag_files;

}
##
# Monitor the RPO threshold
#
# @FIX: RPO threshold value is common for all replication pairs. This should
#       ideally be configurable on a per replication pair
#
# @FIX: RPO reporting is not exact as the timestamps on the file
#       are newer than what should be due to the additional processing
#       by the TM. The TM should try and preserve the creation time on the
#       the processed files. Alternately we could rely on the sentinel and
#       OA to update the last processed file timestamp in the db and use the
#       difference as the RPO. More exact this way.
#
# Note: This function should be called *after* the sentinel files have been
#       processed.
##
#
#Calculating RPO
#
sub monitorRPO() 
{
	my($ofile_dir,$source_file_dir,$target_folder_diffs,$source_folder_diffs, $src_id, $src_vol, $dest_id, $dest_vol, $plan_name, $data_hash, $rpoSLAThreshold, $pair_id, $src_os_flag, $src_compatibity_number) = @_;
	my $CS_HOST_NAME = $tmanager::CS_HOST_NAME;
	my $PS_HOST_NAME = $tmanager::PS_HOST_NAME;
	
	$logging_obj->log("DEBUG","Entering monitorRPO");
	my $current_time = time ();
	my $current_rpo = 0;
	my $current_rpo_src_time = 0;
	my @target_folder_diffs = @$target_folder_diffs;
	my @source_folder_diffs = @$source_folder_diffs;
    my @other_ifile_dirs = @$source_file_dir;
	my @all_diffs = (@target_folder_diffs, @source_folder_diffs);

	
	if ($src_compatibity_number >= 960000)
	{
		foreach my $src_file_dir (@other_ifile_dirs)
		{
			my $tag_file_dir = $src_file_dir . "\\tags";
		
			$logging_obj->log("DEBUG","Tags dir $tag_file_dir");
			
			if ( -e $tag_file_dir )
			{
				my $rpo_file = $tag_file_dir . "\\lasttagtimestamp.txt";
				
				my (@tag_files) = &GetTagFilesInDir($tag_file_dir);							

				my $ps_os_flag = 1; # 1 = Windows
				my $deleteTagFiles = 0;
				
				my $target_dir = Utilities::makePath($ofile_dir, $ps_os_flag);
				my $target_azure_upload_dir = Utilities::makePath($ofile_dir . "/AzurePendingUploadRecoverable", $ps_os_flag);
				my $target_azure_uploadnc_dir = Utilities::makePath($ofile_dir . "/AzurePendingUploadNonRecoverable", $ps_os_flag);
				
				while ( my $name = shift @tag_files )
				{										
					if ( !$deleteTagFiles )
					{								
						my (@target_files) = &GetTagFilesInDir($target_dir);
						!grep (/$name/, @target_files) || next;
												
						my (@target_azure_upload_files) = &GetTagFilesInDir($target_azure_upload_dir);
						!grep (/$name/, @target_azure_upload_files) || next;
												
						my (@target_azure_uploadnc_files) = &GetTagFilesInDir($target_azure_uploadnc_dir);
						!grep (/$name/, @target_azure_uploadnc_files) || next;								
					
						$logging_obj->log("INFO","Tag file $name already uploaded.");
						
										
						my $tsStartIndex = index($name, "E");
						my $subName = substr($name, $tsStartIndex);
						my $tsEndIndex = index($subName, "_");
						my $tagTime = substr($subName, 1, $tsEndIndex-1);
						
						$logging_obj->log("DEBUG","Tag time is $tagTime.");
						
						# tag time is in 100 ns, get the number of seconds 
						my $rpo_time= int($tagTime / (10 * 1000 * 1000));
						
						# Windows OS epoch starts from year 1601. Deduct time upto 1970
						if ($src_os_flag == 1)
						{
							$rpo_time -= 11644473600;
						}
						
						# TODO - until the CS is ready to consume the source timestamp,
						# use modified time of the last uploaded tag file as the rpo_time			
												
						my $rpo_time_file = $tag_file_dir ."\\". $name;
						
						my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
						$mtime,$ctime,$blksize,$blocks) ;
						if( $HAS_NET_IFCONFIG == 1 )
						{
							 use if Utilities::isWindows(), "Win32::UTCFileTime"  ;
							($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
								$mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat("$rpo_time_file");
								
						}
						else
						{
							($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
								$mtime,$ctime,$blksize,$blocks) = stat("$rpo_time_file");
					  
						}
						if ( (defined($mtime)) && ($mtime ne "") && ($mtime>0)) 
						{   
							if ($current_time - $mtime > $current_rpo) 
							{
								$current_rpo = $current_time - $mtime;
							}
						}
						else
						{
						   $logging_obj->log("DEBUG","Modified time is not defined: $mtime");
							last;
						}
						
						open (RPO_FILE, ">$rpo_file") || $logging_obj->log("EXCEPTION", "Failed to create file $rpo_file.") || last;
						# print RPO_FILE $rpo_time;
						print RPO_FILE $mtime;
						close(RPO_FILE);
						
						$current_rpo_src_time = $rpo_time;
						$deleteTagFiles = 1;
						
						# my $current_utc_time = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
						# $current_rpo = $current_utc_time - $rpo_time;
						
						$logging_obj->log("INFO","Tag time is $rpo_time, RPO is $current_rpo.");
					}

					if ( $deleteTagFiles )
					{
						my $file_to_delete = $tag_file_dir . "\\" . $name;
						$logging_obj->log("DEBUG","Deleting tag file $file_to_delete \n");
						unlink $file_to_delete;
					}
						
				}
				
				if ( !$deleteTagFiles )
				{
					open (RPO_FILE, "<$rpo_file") || $logging_obj->log("EXCEPTION", "Failed to open file $rpo_file.") || next;
					my $rpo_string = <RPO_FILE>;
					close(RPO_FILE);
					
					chomp($rpo_string);
					my $rpo_time = int($rpo_string);					
					# my $current_utc_time = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
					# $current_rpo = $current_utc_time - $rpo_time;
					# $current_rpo_src_time = $rpo_time;
					
					$current_rpo = $current_time - $rpo_time;
					
					$logging_obj->log("INFO","Last stored tag time is $rpo_time, RPO is $current_rpo.");
					next;
				}
			}
		}
	}
	else
	{
		foreach my $file (@all_diffs)
		{
			chomp $file;
			# ls -l doesn't give the full pathname
			my $file_alias = $file;
			$file = $ofile_dir . "/" . $file;
				
			if (!(-e $file))
			{
				foreach my $ifile_dir (@other_ifile_dirs)
				{
					$file = $ifile_dir . "/" . $file_alias;
					if ((-e $file))
					{
						last;
					}
				}
			}
			# Skip directories and special files
			if (!(-f $file)) 
			{
				$logging_obj->log("EXCEPTION","Skipping special file or directory: $file");
				next;
			}
			#4780
			if (-e $file) 
			{
				# Is the oldest file creation time before the RPO threshold
				my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
						$mtime,$ctime,$blksize,$blocks) ;
				if( $HAS_NET_IFCONFIG == 1 )
				{
					 use if Utilities::isWindows(), "Win32::UTCFileTime"  ;
					($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
						$mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat("$file");
						
				}
				else
				{
					($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
						$mtime,$ctime,$blksize,$blocks) = stat("$file");
			  
				}
				if ( (defined($mtime)) && ($mtime ne "") && ($mtime>0)) 
				{   
					if ($current_time - $mtime > $current_rpo) 
					{
						$current_rpo = $current_time - $mtime;
					}
				}
				else
				{
				   $logging_obj->log("DEBUG","Modified time is not defined: $mtime");
					next;
				}
			}
		}
		
		if(!(@source_folder_diffs))
		{
		   my $ifile_dir = $other_ifile_dirs[0];
		   my $monitor_file = $ifile_dir ."/monitor.txt";
		   
			if (-e $monitor_file)
			{
				open(FH,"$monitor_file");
				my @file_content = <FH>;
				close (FH);
				my @time_stamps = split(/:/,$file_content[1]);
				# Here file_time_stamp means CS time when the last file was processed to target.
				# and process_time means current CS time.
				my $file_time_stamp = $time_stamps[0];
				my $process_time = $time_stamps[1]; 
				if($file_time_stamp!=0)
				{
					my $source_rpo = $process_time - $file_time_stamp;
					if ($source_rpo > $current_rpo)
					{
						$current_rpo = $source_rpo;
					}
				}
			}
		}
	}

	$logging_obj->log("INFO","Exit monitorRPO. HostId: $src_id, DiskId: $src_vol, RPO: $current_rpo, RPO_SRC_TIME: $current_rpo_src_time.");
	return ($current_rpo, $current_rpo_src_time);
}

sub get_file_size
{
	eval 
	{
		# Avoid resync folders, if queried for diff. Avoid diff folders, if queried for resync.
		# Don't avoid any folder, when this option isn't set.
		if(($folder_to_query eq 'diff' and $_ eq 'resync') or
		   ($folder_to_query eq 'resync' and $_ eq 'diff'))
		{
			$File::Find::prune = 1;
		}
		
		if(!(-f $_) || !(-e $_))
		{
			return;
		}
			
		my $tmp_size = (lstat($_))[7];
		$dirs_size += $tmp_size;
	};
	if ($@) 
	{		
		$logging_obj->log("EXCEPTION","EXCEPTION occurred in get_file_size :: ".$@);
	}	
}

##
# Update replication status page
##
sub updateReplicationStatusPage 
{
	my ($srcvol,$src_id,$dest_id, $dest_vol,$is_cluster,$cluster_ids,$data_hash,$pair_id) = @_;
	my @cluster_ids;  
	my @other_ifile_dirs;
	my @source_folder_diffs;
	
	my $pair_data = $data_hash->{'pair_data'};
	my $lv_data = $data_hash->{'lv_data'};
	my $host_data = $data_hash->{'host_data'};
	my $plan_data = $data_hash->{'plan_data'};
	
	my $org_pair_id = $pair_id."!!".$src_id;
	
	my $src_os_flag = $host_data->{$src_id}->{'osFlag'};
	my $dst_os_flag = $host_data->{$dest_id}->{'osFlag'};
	my $resyncStartTS = $pair_data->{$org_pair_id}->{'resyncStartTagtime'};
	my $resyncEndTS = $pair_data->{$org_pair_id}->{'resyncEndTagtime'};
	my $rpo_threshold = $pair_data->{$org_pair_id}->{'rpoSLAThreshold'};
	my $jobId = $pair_data->{$org_pair_id}->{'jobId'};
	my $replication_status = $pair_data->{$org_pair_id}->{'replication_status'};
	my $plan_id = $pair_data->{$org_pair_id}->{'planId'};
	my $plan_name = $plan_data->{$plan_id}->{'planName'};
	my $current_timestamp_on_cs = $pair_data->{$org_pair_id}->{"unix_time"};
	my $status_update_timestamp = $pair_data->{$org_pair_id}->{'statusUpdateTime_unix_time'};
	my $db_diff_throttle_flag = $pair_data->{$org_pair_id}->{'throttleDifferentials'};
	my $src_compatibity_number = $host_data->{$src_id}->{'compatibilityNo'};	
	
	my $pair_type = '';
	
	my $is_quasiflag = $pair_data->{$org_pair_id}->{'isQuasiflag'};
	my $execution_state = $pair_data->{$org_pair_id}->{'executionState'};
	my $deleted = $pair_data->{$org_pair_id}->{'deleted'};
	my $src_replication_status = $pair_data->{$org_pair_id}->{'src_replication_status'};
	my $tar_replication_status = $pair_data->{$org_pair_id}->{'tar_replication_status'};
	my $is_resync = $pair_data->{$org_pair_id}->{"isResync"};
	my $src_last_host_updatetime = $host_data->{$src_id}->{'lastHostUpdateTime'};
	my $tar_last_host_updatetime = $host_data->{$dest_id}->{'lastHostUpdateTime'};
	my $ir_progress_update_time = $pair_data->{$org_pair_id}->{"lastTMChange"};
	my $is_communication_from_source = $pair_data->{$org_pair_id}->{"isCommunicationfromSource"};
	my $irdr_file_stuck_health_factor = 0;
	my $health_description = '';

	
	if($plan_data->{$plan_id}->{'solutionType'} eq 'ARRAY') 
	{
		$pair_type = 3;			
	}

	my $pending_diffs_size = 0;
	my $pending_rsyncs_size = 0;
	my $pending_qdiffs_size = 0;
	my $diffStartTimeStamp = 0;
	my $current_rpo;
	my $rpo_time;
	my $pending_diffs_size_src=0;
	my $err_message = "Input directory has not been created";
	my $ifile_dir;
	my @batch;
	my $input;

	if($is_cluster)
	{			
		#for each node construct a input file directory & store in list
		@cluster_ids = @$cluster_ids ;
		foreach my $cluster_host_id (@cluster_ids) 
		{
			#construct the cluster input file directory path
			$ifile_dir = "$HOME_DIR/" . $cluster_host_id . "/" .$srcvol . "/" . ($ECB_IDIR);
			# This is the directory where we will find the Sentinel uploaded files
			# make ensures proper path is constructed as per underlined os
			$ifile_dir = Utilities::makePath($ifile_dir,$src_os_flag);
			#if failed to create the directory then return  
			if (!-d $ifile_dir) 
			{
				$logging_obj->log("EXCEPTION","Input directory has not been created yet $ifile_dir");				
				return \@batch;
			}
			push(@other_ifile_dirs,$ifile_dir);	
		}
	}
	else
	{
		#This block is for non cluster source
		@cluster_ids = $src_id;
		#Contruct the input file directory
		$ifile_dir =  "$HOME_DIR/" . $src_id . "/" .$srcvol . "/" . ($ECB_IDIR);
		$ifile_dir = Utilities::makePath($ifile_dir,$src_os_flag);
		# Return if the input directory has not been created yet
		if (!-d $ifile_dir) 
		{
			$logging_obj->log("EXCEPTION","Input directory has not been created yet $ifile_dir");			
			return \@batch;
		}
		push(@other_ifile_dirs,$ifile_dir);
    }
	#store the input file directory in a list
	
	# This is where the compressed difference files are kept
	my $ofile_dir =  "$HOME_DIR/" . $dest_id . "/" .$dest_vol;
	$ofile_dir = Utilities::makePath($ofile_dir,$dst_os_flag);	
	# Return if the destination directory has been created
	if (!-d $ofile_dir) 
	{
		$logging_obj->log("EXCEPTION","Output directory has not been created yet $ofile_dir");		
		return \@batch;
	}	

	#
	# Get the differential status
	# Bug 4590 - Replaced ls command by perl file glob function.
	# This should improve the performance.
	#
	my @target_folder_diffs = Utilities::read_directory_contents ($ofile_dir, '^completed_ediff');	
	#collect the source files in an array
	#for cluster case consider all nodes invloved in cluster  
	foreach my $source_dirs (@other_ifile_dirs)  
	{
		push(@source_folder_diffs,Utilities::read_directory_contents ($source_dirs, '^completed_diff'));
	}

	$dirs_size = 0;
	$folder_to_query = 'diff';
	find(\&get_file_size, $ofile_dir);
	my $tgt_total_pending_diffs = $dirs_size;
	
	$logging_obj->log("DEBUG","updateReplicationStatusPage ofile_dir: $ofile_dir, tgt_total_pending_diffs: $tgt_total_pending_diffs");
	
	$dirs_size = 0;
	$folder_to_query = 'diff';
	find(\&get_file_size, @other_ifile_dirs);
	my $src_total_pending_diffs = $dirs_size;

	$logging_obj->log("DEBUG","updateReplicationStatusPage ifile_dir: $ifile_dir, src_total_pending_diffs: $src_total_pending_diffs");
	
	my $total_pending_diffs = $tgt_total_pending_diffs + $src_total_pending_diffs;
	
	$pending_qdiffs_size = $total_pending_diffs; 
	
	$pending_diffs_size = $total_pending_diffs;
	
	my $ofile_dir_alias = $ofile_dir;
	$ofile_dir .= "/" . $RS_IODIR;
	
	my @resync_files =  Utilities::read_directory_contents($ofile_dir, "^(completed_resync_|completed_sync_|completed_hcd_|completed_sdni_)");
	if ($? != 0) 
	{
		# This is a normal situation so go ahead and update the status
	}
	else 
	{
		foreach my $file (@resync_files) 
		{
			chomp $file;

			# ls -1 doesn't give the full pathname
			$file = $ofile_dir . "/" . $file;

			# Skip directories and special files
			if (!(-f $file)) 
			{
				$logging_obj->log("DEBUG","5) Skipping special file or directory: $file");
				my $err_id = $src_id."_".$srcvol."_".$dest_id."_".$dest_vol;
				my $err_message = "Skipping special file or directory";
				my $err_summary = "Skipping special file or directory: $file\n";
				next;
			}

			#Start - issue#2249
			# Is the oldest file creation time before the RPO threshold
			my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
					$mtime,$ctime,$blksize,$blocks) ; #= stat($file);
			if( $HAS_NET_IFCONFIG == 1 )
			{
				 use if Utilities::isWindows(), "Win32::UTCFileTime" ;
				($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
					$mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat("$file");
				
			}
			else
			{
				($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
			$mtime,$ctime,$blksize,$blocks) = stat("$file");

			}
			#End - issue#2249
			$pending_rsyncs_size += $size;
		}
	}
	
	my $diffThreshold = &monitorDifferentialThrottle($total_pending_diffs,$src_id, $srcvol, $dest_id, $dest_vol, $is_quasiflag, $src_compatibity_number, $data_hash);
	
	$logging_obj->log("DEBUG","updateReplicationStatusPage total_pending_diffs: $total_pending_diffs, diffThreshold: $diffThreshold");
	
	if(($diffThreshold == 0 && $db_diff_throttle_flag == 0) && ($current_timestamp_on_cs - $status_update_timestamp) < 300)
	{
		$logging_obj->log("DEBUG","We are returning from here for replication srcvol: $srcvol, src_id: $src_id, dest_id: $dest_id, dest_vol: $dest_vol, diffThreshold: $diffThreshold, db_diff_throttle_flag: $db_diff_throttle_flag, current_timestamp_on_cs: $current_timestamp_on_cs, status_update_timestamp: $status_update_timestamp");
		return (\@batch, 0);
	}
	
	$logging_obj->log("DEBUG","monitor RPO pair_type::$pair_type");
	if(($AMETHYST_VARS->{'CUSTOM_BUILD_TYPE'} == 2) && ($pair_type == 3) && ($src_id eq $dest_id)) 
	{
		$current_rpo = 0;
	}
	else
	{
		#Initially when pair is configured
		if($jobId == 0 && $replication_status == 0)
		{
			$rpo_time = "pairConfigureTime";
		}
		else
		{
			my $src_rpo_time = 0;
			
			my $cs_compatibility_no = 0;
			foreach my $host_id (keys %{$host_data})
			{
				if($host_data->{$host_id}->{'csEnabled'} == 1)
				{
					$cs_compatibility_no = $host_data->{$host_id}->{'compatibilityNo'};
				}
			}
			
			if($cs_compatibility_no >= 960000)
			{
				my ($error_code, $health_factor_code, $health_factor, $priority, $component);
				$component = 'PS';
				my $current_unix_timestamp = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
				my $is_pair_state_ir = 0;
				
				if( ($execution_state != 2 || $execution_state != 3) && ($deleted == 0) && ($tar_replication_status == 0) && ($src_replication_status == 0) && ($is_resync == 1)  && ($is_quasiflag == 0) && ( ($current_unix_timestamp - $src_last_host_updatetime) < $SENTINEL_ERR_THRESHOLD ) && (($current_unix_timestamp - $tar_last_host_updatetime) < $SENTINEL_ERR_THRESHOLD) )
				{
					($irdr_file_stuck_health_factor, $health_description) = &VerifyIRFileStuck($ir_progress_update_time, $current_unix_timestamp);
					$is_pair_state_ir = 1;
				}
				elsif( ($is_quasiflag != 0) && ($deleted == 0) && ($tar_replication_status == 0) && ($replication_status == 0) && (($current_unix_timestamp - $tar_last_host_updatetime) < $SENTINEL_ERR_THRESHOLD) )
				{
					($irdr_file_stuck_health_factor, $health_description) = &VerifyDRFileStuck($ofile_dir_alias, \@target_folder_diffs, $dst_os_flag);
				}
				
				if($is_pair_state_ir == 1 || $deleted == 1 || $tar_replication_status == 1)
				{
					my $dr_stuck_file = Utilities::makePath($ofile_dir."/dr_stuck_file.txt", $dst_os_flag);
					unlink($dr_stuck_file) if(-e $dr_stuck_file);
					
				}
				
				my ($query_type, $table_name, $field_names, $condition);
				$table_name = "healthfactors";
				my $disk_health_factor = "DISK";
				
				if($irdr_file_stuck_health_factor == 1)
				{
					$error_code = 'EC0127';
					$health_factor_code = 'ECH00015';
					$health_factor = '1';
					$priority = '1';
					$query_type = "REPLACE";
					$field_names = "sourceHostId, destinationHostId, pairId, errCode, healthFactorCode, healthFactor, priority, component, healthDescription, healthTime, healthFactorType";
					$condition = "'$src_id', '$dest_id', $pair_id, '$error_code', '$health_factor_code', '$health_factor', '$priority', '$component', '$health_description', now(),'$disk_health_factor'";
				}
				elsif($irdr_file_stuck_health_factor == 2)
				{
					$error_code = 'EC0129';
					$health_factor_code = 'ECH00014';
					$health_factor = '2';
					$priority = '2';
					$query_type = "REPLACE";
					$field_names = "sourceHostId, destinationHostId, pairId, errCode, healthFactorCode, healthFactor, priority, component, healthDescription, healthTime, healthFactorType";
					$condition = "'$src_id', '$dest_id', $pair_id, '$error_code', '$health_factor_code', '$health_factor', '$priority', '$component', '$health_description', now(),'$disk_health_factor'";
				}
				else
				{
                    # bug id 2794308 : delete the stale health factors by removing the destHostId clause in the health issue update query.
					$query_type = "DELETE";
					$field_names = "";
					$condition = "sourceHostId= '$src_id' AND pairId= '$pair_id' AND errCode IN ('EC0127', 'EC0129')";
					$health_description = '';
				}
				$input->{0}->{"queryType"} = "$query_type";
				$input->{0}->{"tableName"} = "$table_name";
				$input->{0}->{"fieldNames"} = "$field_names";
				$input->{0}->{"condition"} = "$condition";
				push(@batch,$input);
			}
			
			# Sending pair_id to get data from hash during rpo calculation.
			($current_rpo, $src_rpo_time) = &monitorRPO($ofile_dir_alias,\@other_ifile_dirs,\@target_folder_diffs,\@source_folder_diffs,$src_id, $srcvol, $dest_id, $dest_vol, $plan_name, $data_hash, $rpo_threshold,$pair_id, $src_os_flag, $src_compatibity_number);
			
			# TODO - update this once the CS is able to consume the src rpo time
			# if ($src_compatibity_number >= 960000)
			# {
				# $rpo_time = "FROM_UNIXTIME($src_rpo_time)";
			# }
			# else
			# {
				$rpo_time = "FROM_UNIXTIME(UNIX_TIMESTAMP(now()) - $current_rpo)";
			# }
		}
	}
	$input->{1}->{"queryType"} = "UPDATE";
	$input->{1}->{"tableName"} = "srcLogicalVolumeDestLogicalVolume";
	$input->{1}->{"fieldNames"} = "$SRC_DST_LV_remainingQuasiDiffBytes = $pending_qdiffs_size,$SRC_DST_LV_remainingDifferentialBytes = $pending_diffs_size,$SRC_DST_LV_remainingResyncBytes = $pending_rsyncs_size,$SRC_DST_LV_currentRPOTime = $rpo_time, $SRC_DST_LV_statusUpdateTime = now(), throttleDifferentials = $diffThreshold";
	$input->{1}->{"condition"} = " pairId = '$pair_id'";
	
	push(@batch,$input);
	
	my $diff_file_dir =  "$HOME_DIR/" . $dest_id . "/" .$dest_vol;
	$diff_file_dir= Utilities::makePath($diff_file_dir,$dst_os_flag);
	ConfigServer::Trending::HealthReport::updateRpoTxt($current_rpo,$src_id,$srcvol,$src_os_flag, $data_hash, $org_pair_id);
	
	return (\@batch, $irdr_file_stuck_health_factor);
}


sub VerifyDRFileStuck
{
	my($ofile_dir, $target_folder_diffs, $dst_os_flag) = @_;
	
	my @target_folder_diffs = @$target_folder_diffs;
	my $current_time = time ();
	my $oldest_file_time = 0;
	my $diff_dir_oldest_file_time = 0;
	my $oldest_filename = '';
	my $dr_stuck_file = Utilities::makePath($ofile_dir."/dr_stuck_file.txt", $dst_os_flag);
	my $saved_oldest_filename = '';
	my $dr_file_stuck_health_factor = 0;
	my $health_description = '';
	my $update_dr_stuck_file = 0;
	my $retain_dr_stuck_file = 0;
	
	if(-e $dr_stuck_file)
	{
		open(FH , "<$dr_stuck_file") ;
		$saved_oldest_filename = <FH>;
		$saved_oldest_filename =~ s/\s+$//;
		close(FH);
		
		if($saved_oldest_filename ne '')
		{
			if( $saved_oldest_filename =~ m/^completed_ediff/i )
			{
				$logging_obj->log("DEBUG","VerifyDRFileStuck:: saved_oldest_filename is a valid file: $saved_oldest_filename");
			}
			else
			{
				$logging_obj->log("DEBUG","VerifyDRFileStuck:: not a valid file saved_oldest_filename: $saved_oldest_filename");
				$saved_oldest_filename = '';
			}
		}
	}
	
	if(($saved_oldest_filename ne '') && (-e $ofile_dir.'/'.$saved_oldest_filename) && (-f $ofile_dir.'/'.$saved_oldest_filename))
	{
		my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime, $mtime,$ctime,$blksize,$blocks) ;
		
		if( $HAS_NET_IFCONFIG == 1 )
		{
			 use if Utilities::isWindows(), "Win32::UTCFileTime";
			($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime, $mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat("$dr_stuck_file");
				
		}
		else
		{
			($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime, $mtime,$ctime,$blksize,$blocks) = stat("$dr_stuck_file");
	  
		}
		
		if ( (defined($mtime)) && ($mtime ne "") && ($mtime>0)) 
		{   
			$oldest_file_time = $current_time - $mtime;
			$oldest_filename = $saved_oldest_filename;
			$retain_dr_stuck_file = 1;
		}
		else
		{
		   $logging_obj->log("DEBUG"," VerifyDRFileStuck Modified time is not defined: $mtime");
		}
	}
	else
	{
		foreach my $file (@target_folder_diffs)
		{
			chomp $file;
			# ls -l doesn't give the full pathname
			my $file_name_bkp = $file;
			$file = $ofile_dir . "/" . $file;
				
			if (-e $file)
			{
				# Skip directories and special files
				if (!(-f $file)) 
				{
					$logging_obj->log("EXCEPTION"," VerifyDRFileStuck:: Skipping special file or directory: $file");
					next;
				}
							
				# Is the oldest file creation time before the RPO threshold
				my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime, $mtime,$ctime,$blksize,$blocks) ;
				if( $HAS_NET_IFCONFIG == 1 )
				{
					 use if Utilities::isWindows(), "Win32::UTCFileTime";
					($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime, $mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat("$file");
						
				}
				else
				{
					($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime, $mtime,$ctime,$blksize,$blocks) = stat("$file");
			  
				}
				if ((defined($mtime)) && ($mtime ne "") && ($mtime>0)) 
				{   
					if ($current_time - $mtime > $diff_dir_oldest_file_time) 
					{
						$diff_dir_oldest_file_time = $current_time - $mtime;
						$oldest_filename = $file_name_bkp;
						$update_dr_stuck_file = 1;
					}
				}
				else
				{
				   $logging_obj->log("DEBUG"," VerifyDRFileStuck Modified time is not defined: $mtime");
					next;
				}
			}
		}	
	}
	
	if($oldest_filename ne '' && $update_dr_stuck_file == 1)
	{
		chomp($oldest_filename);	
		open(FH,">$dr_stuck_file");
		print FH $oldest_filename;
		close (FH);
	}
	else
	{
		if(-e $dr_stuck_file && $retain_dr_stuck_file == 0)
		{
			unlink($dr_stuck_file);
		}
	}
	
	if($oldest_file_time > $IR_DR_STUCK_CRITICAL_THRESHOLD)
	{
		#Critical
		$dr_file_stuck_health_factor = 2;
		$health_description = 'DR_FILE_STUCK_CRITICAL';
	}
	elsif($oldest_file_time > $IR_DR_STUCK_WARNING_THRESHOLD)
	{
		#Warning
		$dr_file_stuck_health_factor = 1;
		$health_description = 'DR_FILE_STUCK_WARNING';
	}
	$logging_obj->log("DEBUG"," VerifyDRFileStuck dr_file_stuck_health_factor: $dr_file_stuck_health_factor, health_description: $health_description");
	return ($dr_file_stuck_health_factor, $health_description);
}


sub VerifyIRFileStuck
{
	my($ir_progress_update_time, $current_unix_timestamp) = @_;
	my $ir_file_stuck_health_factor = 0;
	my $health_description = '';
	
	if($current_unix_timestamp - $ir_progress_update_time > $IR_DR_STUCK_CRITICAL_THRESHOLD)
	{
		#Critical
		$ir_file_stuck_health_factor = 2;
		$health_description = 'IR_FILE_STUCK_CRITICAL';
	}
	elsif($current_unix_timestamp - $ir_progress_update_time > $IR_DR_STUCK_WARNING_THRESHOLD)
	{
		#Warning
		$ir_file_stuck_health_factor = 1;
		$health_description = 'IR_FILE_STUCK_WARNING';
	}
	
	$logging_obj->log("DEBUG"," VerifyIRFileStuck ir_file_stuck_health_factor: $ir_file_stuck_health_factor, health_description: $health_description");
	return ($ir_file_stuck_health_factor, $health_description);
}

#Get Differential Threshold for source.

sub monitorDifferentialThrottle{
	my ($pending_diffs_size,$src_id, $srcDevice, $dest_id, $tarDevice, $is_quasiflag, $src_compatibity_number, $data_hash) = @_;
	my $one_To_N_Count;
	my $diffFilesThreshold;
	my $throttleDiff = 0;
	my %pairDetails;
	my $dataUnitConversionDif = convertDataInto($MAX_DIFF_FILES_THRESHOLD);	
	my $src_host_name = $data_hash->{'host_data'}->{$src_id}->{'name'};
	my $dest_host_name = $data_hash->{'host_data'}->{$dest_id}->{'name'};
	my @input_parameters;
	if ($data_hash->{'lv_data'}->{$src_id."!!".$srcDevice})
	{	
		my $db_diff_threshold = $data_hash->{'lv_data'}->{$src_id."!!".$srcDevice}->{'maxDiffFilesThreshold'};
		$diffFilesThreshold = ( $db_diff_threshold =~ /^\d+/ ) ? $db_diff_threshold : $MAX_DIFF_FILES_THRESHOLD;	
		$dataUnitConversionDif = convertDataInto($diffFilesThreshold);
	}
	
	if(($pending_diffs_size >= $diffFilesThreshold) && ($diffFilesThreshold != 0) && ($src_compatibity_number <= $MAX_DIFF_THROTTLE_COMPAT_VERSION))
	{
		$throttleDiff = 1;
		if($is_quasiflag != 0)
		{
			my $subject = "TRAP on replication pair";	
			my $err_message = "Differential sync cache folder for the pair: ".$src_host_name." [".$srcDevice."] -> ".$dest_host_name." [".$tarDevice."]"."has exceeded the configured threshold of $dataUnitConversionDif";
			my $err_summary = "Differential Data flow controlled - Differential Sync Cache Folder for replication pair has exceeded configured threshold";
			my $err_id = $src_id."_".$srcDevice."_".$dest_id."_".$tarDevice;
			$err_message =~ s/<br>/ /g;
			$err_message =~ s/\n/ /g;
			
			my %err_placeholders;
			$err_placeholders{"SrcHostName"} = $src_host_name;
			$err_placeholders{"SrcVolume"} = $srcDevice;
			$err_placeholders{"DestHostName"} = $dest_host_name;
			$err_placeholders{"DestVolume"} = $tarDevice;
			$err_placeholders{"VolumeSize"} = $dataUnitConversionDif;
			
			my $args;
			$args->{"id"} = $src_id;
			$args->{"error_id"} = $err_id;
			$args->{"summary"} = $err_summary;
			$args->{"err_msg"} = $err_message;
			$args->{"err_temp_id"} = "CXSTOR_WARN";
			$args->{"err_code"} = "EC0111";
			$args->{"err_placeholders"} = \%err_placeholders;
			my $alert_info = serialize($args);
			
			my $arguments;
			$arguments->{"err_temp_id"} = "CXSTOR_WARN";
			$arguments->{"err_msg"} = $err_message;
			my $trap_info = serialize($arguments);
			
			$logging_obj->log("EXCEPTION","This is not an Exception : monitorDifferentialThrottle Differential Data flow controlled".$err_message);
			
			&tmanager::add_error_message($alert_info, $trap_info);
		}
	}
	return $throttleDiff;
}

sub getdiffStartTimeStamp($file)
{
 my ($file) = @_;
 my $parttwo;
 #DebugTimeshotLog("getdiffStartTimeStamp --- gettg param file is $file\n");
 my (@filepart) =split(/_S/, $file);
 $parttwo = $filepart[1];
 #DebugTimeshotLog("get_file_format --- parttwo :: $parttwo\n");
 if($parttwo == '')
 {
	 @filepart =split(/_E/, $file);
	 $parttwo = $filepart[1];	 
 }
 #DebugTimeshotLog("get_file_format --- parttwo1 :: $parttwo\n");
 my (@timeStamp) = split(/_/,$parttwo);
 return $timeStamp[0];
}

##
#
# Check if we have hit the warning threshold for disk usage
# Return: Add a trap if the disk usage is hitting danger thresholds
#
##
sub monitorDisks 
{
	my ($data_hash) = @_;
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
	my $count;
	my @input_parameters;
	my $input;
	my $pathDelimiter = '\\';
    my %err_placeholders;
	
	if (Utilities::isLinux()) {
		$pathDelimiter = '/';
	}
	
	# Get the list of listeners that the traps need to go out to
	my $hostName = `hostname`;
	chomp $hostName;

	# Check if the volumes are online by touching a file
	if (Utilities::isLinux())
	{
		for ($count = 0; $count < @SECONDARY_VOLUMES; $count++) 
		{
			my $secVolume = $SECONDARY_VOLUMES [$count];
			my $secVolumeFile = $SECONDARY_VOLUMES_FILE [$count];
			my $modifiedFile = 	$secVolume.$pathDelimiter."volume_test.file";
			my $touch_out = system ("$TOUCH $modifiedFile");
			if (($? != 0) && ($? != 2304)) 
			{
				#Touch is not a valid test in all cases - home directory is created under root
				$logging_obj->log("DEBUG","PS has a problem with storage mounted on $secVolume for $hostName ");
				my $err_id = $MY_AMETHYST_GUID;
				my $err_message = "PS $hostName has a problem with storage mounted on $secVolume.";
				my $err_summary = "PS has a problem with storage mounted on $secVolume for $hostName";
				$err_message =~ s/<br>/ /g;
				$err_message =~ s/\n/ /g;

                $err_placeholders{"VolumeName"} = $secVolume;
                $err_placeholders{"HostName"} = $hostName;

				my $args;
				$args->{"id"} = $err_id;
				$args->{"error_id"} = $err_id;
				$args->{"summary"} = $err_summary;
				$args->{"err_msg"} = $err_message;
				$args->{"err_temp_id"} = "CXSTOR_WARN";
                $args->{"err_code"} = "EC0113";
                $args->{"err_placeholders"} = \%err_placeholders;
				my $alert_info = serialize($args);
				
				my $trap_args;
				$trap_args->{"err_msg"} = $err_message;
				$trap_args->{"err_temp_id"} = "CXSTOR_WARN";
				my $trap_info = serialize($trap_args);
				&tmanager::add_error_message($alert_info, $trap_info);
			}
		}
	}
	# Check if the disk usage has reached warning threshold levels
	# df should fail if the volume is not online, hence we wouldn't send
	# a disk usage threshold trap if the volume is down
	
	my $tbs_data = $data_hash->{'tbs_data'};
	my $sys_health = $data_hash->{'sys_health'};
	my $disk_space_warn_threshold = 80;
	if(exists $tbs_data->{'DISK_SPACE_WARN_THRESHOLD'})
	{
		$disk_space_warn_threshold = $tbs_data->{'DISK_SPACE_WARN_THRESHOLD'};
	}	

	my $ip ;
	my @partitions_to_check = @SECONDARY_VOLUMES;	
	my $throttle_count=0;
	my $key_count = 0;
	my @volume_list;
	for ($count=0; $count < @partitions_to_check; $count++)
	{
		my $secVolume = $partitions_to_check [$count];
		my $i = 0;	

		if (open(PS, "$DF_CMD -k -P $secVolume|")) 
		{
			while (my $process = <PS>) 
			{
				# First line is column headers so skip it
				$i++;
				if ($i == 1) 
				{
				  next;
				}
				chop($process);
				
				my ($filesystem, $OneKblocks, $used, $available, $use, $mounted_on) = split (' ', $process);

				if ($use > $disk_space_warn_threshold) 
				{
					$throttle_count = 1;
				}
				
				my $healthFlag;
				$healthFlag = $sys_health->{'healthFlag'};
				
				if($healthFlag eq 'healthy' && $throttle_count == 1)
				{			
					$input->{$key_count}->{"queryType"} = "UPDATE";
					$input->{$key_count}->{"tableName"} = "systemHealth";
					$input->{$key_count}->{"fieldNames"} = "healthFlag='degraded', startTime=now()";
					$input->{$key_count}->{"condition"} = "component='Space Constraint'";
					push(@input_parameters, $input);
				}
				elsif ($healthFlag eq 'degraded' && $throttle_count == 0)
				{
					$input->{$key_count}->{"queryType"} = "UPDATE";
					$input->{$key_count}->{"tableName"} = "systemHealth";
					$input->{$key_count}->{"fieldNames"} = "healthFlag='healthy', startTime=now()";
					$input->{$key_count}->{"condition"} = "component='Space Constraint'";
					push(@input_parameters, $input);
				}
				$key_count++;
			}
			close PS;
		}
		else
		{		  
		  $logging_obj->log("DEBUG","Could not run $DF_CMD!");
		}
	}
	
	$input->{$key_count}->{"queryType"} = "UPDATE";
	$input->{$key_count}->{"tableName"} = "processServer";
	$input->{$key_count}->{"fieldNames"} = "cummulativeThrottle=$throttle_count";
	$input->{$key_count}->{"condition"} = "processServerId='$host_guid'";	
	push(@input_parameters, $input);
	
	my $final_input;
	my $i = 0;
	foreach my $keys (@input_parameters)
	{
		foreach my $key (keys %{$keys})
		{
			$final_input->{$i} = $keys->{$key};				
			$i++;
		}
	}
	
	if($final_input ne '') {		
		&tmanager::update_cs_db("monitor_disks",$final_input);		
	}
}

sub monitorSpaceConstraint
{
	my ($data_hash) = @_;
	my $space_constraint = 0;
	my @batch;
	my $input = {};
	my $i=0;
	#
	# Check if the disk usage has reached warning threshold levels
	#
	my $tbs_data = $data_hash->{'tbs_data'};
	my $sys_health = $data_hash->{'sys_health'};
	my $disk_space_warn_threshold = $DISK_SPACE_WARN_THRESHOLD;
	
	if ($tbs_data->{'DISK_SPACE_WARN_THRESHOLD'})
	{
		$disk_space_warn_threshold = $tbs_data->{'DISK_SPACE_WARN_THRESHOLD'};
	}
	
	my @partitions_to_check = @SECONDARY_VOLUMES;

	for (my $count=0; $count < @partitions_to_check; $count++)
	{
		my $secVolume = $partitions_to_check [$count];
		my $i = 0;
		if (open(PS, "$DF_CMD -k -P $secVolume|")) 
		{
			while (my $process = <PS>) 
			{
				#
				# First line is column headers so skip it
				#
				$i++;
				if ($i == 1) 
				{
					next;
				}
				chop($process);
				my ($filesystem, $OneKblocks, $used, $available, $use, $mounted_on) = split (' ', $process);

				if ($use > $disk_space_warn_threshold) 
				{
					$space_constraint = 1;
				}
			}
			close PS;
			
			#
			# Fetch DB value for System Constraint
			#
			my $healthFlag;
			$healthFlag = $sys_health->{'healthFlag'};
			
            if($healthFlag eq 'healthy' && $space_constraint == 1)
			{			
				$input->{$i}->{"queryType"} = "UPDATE";
				$input->{$i}->{"tableName"} = "systemHealth";
				$input->{$i}->{"fieldNames"} = "healthFlag='degraded', startTime=now()";
				$input->{$i}->{"condition"} = "component='Space Constraint'";
				push(@batch, $input);
			}
			elsif ($healthFlag eq 'degraded' && $space_constraint == 0)
			{
				$input->{$i}->{"queryType"} = "UPDATE";
				$input->{$i}->{"tableName"} = "systemHealth";
				$input->{$i}->{"fieldNames"} = "healthFlag='healthy', startTime=now()";
				$input->{$i}->{"condition"} = "component='Space Constraint'";
				push(@batch, $input);				
			}
			$i++;
		}
		else
		{			
			$logging_obj->log("DEBUG","Could not run $DF_CMD!");
		}		
	}
	return \@batch;
}

# 
# @FIX: Could probably use ifconfig to check status
#
sub monitorReplicationInterface {
}

##
#
# Function: Returns 1 if the resync expansion should be throttled
#           because of disk space running lower than the allowed threshold
#
#           0 if the resync expansion doesn't need to be throttled
#
# Note:     Do not throttle on errors
#
# Returns
#
##
sub shouldThrottle {
  my ($volumePath, $msg) = @_;
  my $i = 0;
  my $rc = -1;

  $volumePath = `$DIRNAME $volumePath`;
  if ($volumePath eq '') {
   
	$logging_obj->log("DEBUG","Failed while checking if throttling is necessary");
  }
  elsif (open(PS, "$DF_CMD -k $volumePath|")) {
    while (my $process = <PS>) {
      # First line is column headers so skip it
      $i++;
      if ($i == 1) {
	next;
      }
      #print "process is $process\n";
      chop($process);
      my ($filesystem, $OneKblocks, $used, $available, $use, $mounted_on) = split (' ', $process);
      if ($use > $RESYNC_THROTTLE_THRESHOLD) {
	$msg->send_message ("monitor:", 25, "TRAP on disk monitor", "Resync data flow has been controlled due to disk space constraints on $volumePath");
	$rc = 1;
      }
      else {
	$rc = 0;
	last;
      }
    }
	#Fix 10943
	close PS;
  }

  if ($rc == -1) {
   
	$logging_obj->log("DEBUG","Throttle check failed");
    $rc = 0;
  }

  return $rc;
}

##
#
# Make sure the samba processes are running
# Return: 0  if everything is running ok
#         1  if samba had to be restarted, but was restarted successully
#        -1  if samba is dead and could not be restarted
##
sub monitorSamba {
  eval 
  {
	open(PS, "$PS_CMD|");
  };	
  if ($@)
  {
	
	$logging_obj->log("EXCEPTION","Failed to open file in monitorSamba $@");	
	return;
  }
  my $smbd_pid = -1;
  my $nmbd_pid = -1;
  my $rc = 0;

  while (my $process = <PS>) {
    chop($process);
    my ($user, $pid, $ppid, $c, $stime, $tty, $time, $cmd) = split (' ', $process);
    $cmd = basename($cmd);
    if ($cmd eq $SMBD) {
      #print ("smbd is alive\n");
      $smbd_pid = $pid;
    }
    elsif ($cmd eq $NMBD) {
      #print ("nmbd is alive\n");
      $nmbd_pid = $pid;
    }
  }
	#Fix 10943
	close PS;
  # Check if the smbd and nmbd are running
  if ($smbd_pid == -1 || $nmbd_pid == -1) {
    # Kill any leftover process 
    if ($smbd_pid != -1) {
      kill($smbd_pid);
    }
    if ($nmbd_pid == -1) {
      kill($nmbd_pid);
    }
   
	$logging_obj->log("EXCEPTION","Samba NOT running!");
    # Restart samba process
    my $smboutput = `$SMB_START`;
    print ("Restarted Samba: $smboutput\n");
  }
  else {
    #print "Samba running ok\n";
  }

  return $rc;
}

sub monitorScheduler{
my ($scheduler_action) = @_;
  eval 
  {
	open(PS, "$PS_CMD|");
  };	
  if ($@)
  {
		
	$logging_obj->log("EXCEPTION","Failed to oepn file in monitorScheduler$@");
	return;
  }
	
  my $scheduler_pid = -1;
  my $rc = 0;

  if (Utilities::isWindows()) {
	while (my $process = <PS>) {
		chop($process);
		my ($pid, $ppid, $c, $stime, $cmd) = split (' ', $process);
		$cmd = basename($cmd);
    
		if ($cmd eq $WIN_SCHEDULER) {
			#print ("SCHEDULER is alive\n");
			$scheduler_pid = $pid;
			#Fix 10943
			close PS;
			return 0;
		}
    }
  }
  else {
       
	while (my $process = <PS>) {
		chop($process);
		my ($user, $pid, $ppid, $c, $stime, $tty, $time, $cmd) = split (' ', $process);
		$cmd = basename($cmd);
		
		if ( $cmd eq $SCHEDULER ) {
			if($scheduler_action eq 'STOP_SCHEDULER')
			{
				system($SCHEDULER_STOP);				
			}
			else
			{			
				$scheduler_pid = $pid;
				#Fix 10943								
			}
			close PS;
			return 0;
		}					
    }
  }
  #Fix 10943
  close PS;
  
  
 
  # Note this in the log
  
  $logging_obj->log("DEBUG","!scheduler is NOT running!");
  if($scheduler_action eq 'START_SCHEDULER')
  {
	system($SCHEDULER_START);
  }
  
 
  $logging_obj->log("DEBUG","Restarted scheduler: ");

  return $rc;
}

###
# Checks if HA services are running. Returns 0 if everything is ok
##
sub isHeartbeatRunning {
  `$HEARTBEAT_STATUS`;
  return $?;
}


###
# If HA services are NOT running, start the service. Returns 0 if everything is ok
##
sub startHeartbeat {
  `$HEARTBEAT_START`;
  return $?;
}


###
# Checks if this is the HA active node
##
sub isHeartbeatActiveNode {
  my $cluster_ip = tmanager::get_HA_details();
  my $cmd = "ifconfig | grep \"$cluster_ip \"";
  `$cmd`;
  return $?;
}

###
# Monitor http server for HA. Returns 0 if http service is up and running
##
sub monitorHttpd {

	my $CS_PORT  = $AMETHYST_VARS->{'CS_PORT'};
	my $cluster_ip = tmanager::get_HA_details();

	eval
	{
		my $response;
		my $http_method = "GET";
		my $https_mode = ($AMETHYST_VARS->{'CS_HTTP'} eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $cluster_ip, 'PORT' => $CS_PORT, 'TIMEOUT' => $HA_HTTP_GET_TIMEOUT);
		my $param = {'access_method_name' => 'index', 'access_file' => "/".$HA_HTTP_GET_PAGE, 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'content' => ''};
		
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);

		if ($response->{'status'})
		{
			$HA_HTTP_GET_TRY_COUNT = 0;
		}
		else
		{
			$HA_HTTP_GET_TRY_COUNT++;
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : " . $@);
	}
  
	return $HA_HTTP_GET_TRY_COUNT;
}

###
# Monitor mysql server for HA. Returns 0 if mysql service is up and running
##
sub monitorMysqld {
  my @dbinfo = tmanager::get_DB_details_array();
  my $cluster_ip = tmanager::get_HA_details();
  my $conn;
  eval
  {
	 
     $conn = new Common::DB::Connection();
   
  };
  if ($@) 
  {
    $HA_DB_CONN_TRY_COUNT++;
  }
  else 
  {
    my $tables = $conn->sql_tables();
    if ( $tables < 0 ) 
	{
      $HA_DB_CONN_TRY_COUNT++;
    }
    else 
	{
      $HA_DB_CONN_TRY_COUNT = 0;
    }
    #$dbh->disconnect();
  }

  return $HA_DB_CONN_TRY_COUNT;
}

###
# Control heartbeat. Returns 0 if everything is OK
##
sub monitorHeartbeat {

	#14075 Fix
  #if (($HA_HTTP_GET_TRY_COUNT >= $HA_HTTP_GET_TRY_LIMIT) || ($HA_DB_CONN_TRY_COUNT >= $HA_DB_CONN_TRY_LIMIT))
  if (($HA_DB_CONN_TRY_COUNT >= $HA_DB_CONN_TRY_LIMIT))
  {
    stopHeartbeatService();
  }
}

###
#  Stops heartbeat service
##
sub stopHeartbeatService {
  `$HEARTBEAT_STOP`;
  return $?
}

##
#
# @FIX: Do this better
##
sub doSync {
  `sync`;
}
	

##
# Helper function for parsing dd program output
##
sub getNumRecordsWritten {
  my($content) = @_;
  my (@lines)              = split(/\n/, $content);

  # Try and parse the last line: e.g: 3547+1 records out 
  my ($records, $junk)        = split(/\+/, $lines[$#lines]); 

  if ($records eq "") {
    
	$logging_obj->log("EXCEPTION","failed to parse output, $lines[$#ARGV]");
    
	$logging_obj->log("EXCEPTION","failed to parse output");
  } else {
   
	$logging_obj->log("DEBUG","$records records written");
  }				 
  return $records;
}

##
#
# Access methods
#
##
sub isInitialized {
  my $obj = shift;
  @_ ? $obj->{$DPSO_initialized} = shift # modify attribute
    : $obj->{$DPSO_initialized};	# retrieve attribute
}

sub logfile {
  my $obj = shift;
  @_ ? $obj->{$DPSO_logfile} = shift # modify attribute
    : $obj->{$DPSO_logfile};	# retrieve attribute
}
sub lockfile {
  my $obj = shift;
  @_ ? $obj->{$DPSO_lockfile} = shift # modify attribute
    : $obj->{$DPSO_lockfile};	# retrieve attribute
}
sub status {
  my $obj = shift;
  @_ ? $obj->{$DPSO_status} = shift # modify attribute
    : $obj->{$DPSO_status};	# retrieve attribute
}
sub is_blocksize {
  my $obj = shift;
  @_ ? $obj->{$DPSO_is_blocksize} = shift # modify attribute
    : $obj->{$DPSO_is_blocksize}; # retrieve attribute
}
sub is_count {
  my $obj = shift;
  @_ ? $obj->{$DPSO_is_count} = shift # modify attribute
    : $obj->{$DPSO_is_count};	# retrieve attribute
}
sub is_polling_interval {
  my $obj = shift;
  @_ ? $obj->{$DPSO_is_polling_interval} = shift # modify attribute
    : $obj->{$DPSO_is_polling_interval}; # retrieve attribute
}
sub is_enable_compression {
  my $obj = shift;
  @_ ? $obj->{$DPSO_is_enable_compression} = shift # modify attribute
    : $obj->{$DPSO_is_enable_compression}; # retrieve attribute
}
sub is_enable_pipelining {
  my $obj = shift;
  @_ ? $obj->{$DPSO_is_enable_pipelining} = shift # modify attribute
    : $obj->{$DPSO_is_enable_pipelining}; # retrieve attribute
}
sub is_pipeline_file {
  my $obj = shift;
  @_ ? $obj->{$DPSO_is_pipeline_file} = shift # modify attribute
    : $obj->{$DPSO_is_pipeline_file}; # retrieve attribute
}
sub is_skip {
  my $obj = shift;
  @_ ? $obj->{$DPSO_is_skip} = shift # modify attribute
    : $obj->{$DPSO_is_skip};	# retrieve attribute
}
sub ecb_process_loop_wait {
  my $obj = shift;
  @_ ? $obj->{$DPSO_ecb_process_loop_wait} = shift # modify attribute
    : $obj->{$DPSO_ecb_process_loop_wait}; # retrieve attribute
}
sub rh_enable_pipelining {
  my $obj = shift;
  @_ ? $obj->{$DPSO_rh_enable_pipelining} = shift # modify attribute
    : $obj->{$DPSO_rh_enable_pipelining}; # retrieve attribute
}
sub rh_pipeline_file {
  my $obj = shift;
  @_ ? $obj->{$DPSO_rh_pipeline_file} = shift # modify attribute
    : $obj->{$DPSO_rh_pipeline_file}; # retrieve attribute
}
sub rh_max_files_local {
  my $obj = shift;
  @_ ? $obj->{$DPSO_rh_max_files_local} = shift # modify attribute
    : $obj->{$DPSO_rh_max_files_local};	# retrieve attribute
}
sub rh_copy_mode {
  my $obj = shift;
  @_ ? $obj->{$DPSO_rh_copy_mode} = shift # modify attribute
    : $obj->{$DPSO_rh_copy_mode}; # retrieve attribute
}
sub rh_process_loop_wait {
  my $obj = shift;
  @_ ? $obj->{$DPSO_rh_process_loop_wait} = shift # modify attribute
    : $obj->{$DPSO_rh_process_loop_wait}; # retrieve attribute
}
sub rh_copy_loop_wait {
  my $obj = shift;
  @_ ? $obj->{$DPSO_rh_copy_loop_wait} = shift # modify attribute
    : $obj->{$DPSO_rh_copy_loop_wait}; # retrieve attribute
}


##
#
# Get logged error messages by the various hosts.
#
##
sub get_logged_error_messages_old1 # duplicate function definition 
{ 
  my ($dbh, $msg) = @_; 
  my $sql;
  my $sth;
  my $ref;

  &getAmethystGUID($dbh);
  &Messenger::getTrapListeners($dbh,"");

  $sql = "SELECT distinct($ERR_ALERT_errMessage),$ERR_ALERT_id,$ERR_ALERT_hostid,$ERR_ALERT_errlvl,$ERR_ALERT_errtime from $ERR_ALERT_TBL group by $ERR_ALERT_errMessage,$ERR_ALERT_hostid";

  $sth = $dbh->prepare($sql);
  $sth->execute();

  while ($ref = $sth->fetchrow_hashref())
  {
  	my $errid  = $ref->{$ERR_ALERT_id};
	my $errHostId  = $ref->{$ERR_ALERT_hostid};
	my $errlvl     = $ref->{$ERR_ALERT_errlvl};
	my $errMessage = $ref->{$ERR_ALERT_errMessage};
	my $errtime    = $ref->{$ERR_ALERT_errtime};

  	my $hostname_sql = "SELECT $HOSTS_name from $HOSTS_TBL where $HOSTS_id = '$errHostId'";

	my $hostname_sth = $dbh->prepare($hostname_sql);
	$hostname_sth->execute();
	my $hostname_ref = $hostname_sth->fetchrow_hashref();

	my $HostName = $hostname_ref->{$HOSTS_name};
        my $source = "monitor: $errlvl";
    	my $subject = "On $HostName: $errlvl Observed/Logged";

        $msg->send_message ($source, 9, $subject, "$errtime: $errMessage\n");
	
      my $del_log_sql = "delete from $ERR_ALERT_TBL where $ERR_ALERT_id = '$errid' ";
      my $del_log_sth = $dbh->prepare($del_log_sql);
      $del_log_sth->execute();
      $del_log_sth->finish(); 
      $hostname_sth->finish();
  }

  $sth->finish();
}

##
#
# Get logged error messages by the various hosts.
#
##
sub get_logged_error_messages_old 
{ 
  my ($dbh, $msg) = @_; 
  my $sql;
  my $sth;
  my $ref;

  &getAmethystGUID($dbh);
  Messenger::getTrapListeners($dbh,"");

  $sql = "SELECT distinct($ERR_ALERT_errMessage),$ERR_ALERT_id,$ERR_ALERT_hostid,$ERR_ALERT_errlvl,$ERR_ALERT_errtime from $ERR_ALERT_TBL group by $ERR_ALERT_errMessage,$ERR_ALERT_hostid";

  $sth = $dbh->prepare($sql);
  $sth->execute();

  while ($ref = $sth->fetchrow_hashref())
  {
  	my $errid  = $ref->{$ERR_ALERT_id};
	my $errHostId  = $ref->{$ERR_ALERT_hostid};
	my $errlvl     = $ref->{$ERR_ALERT_errlvl};
	my $errMessage = $ref->{$ERR_ALERT_errMessage};
	my $errtime    = $ref->{$ERR_ALERT_errtime};

  	my $hostname_sql = "SELECT $HOSTS_name from $HOSTS_TBL where $HOSTS_id = '$errHostId'";

	my $hostname_sth = $dbh->prepare($hostname_sql);
	$hostname_sth->execute();
	my $hostname_ref = $hostname_sth->fetchrow_hashref();

	my $HostName = $hostname_ref->{$HOSTS_name};
        my $source = "monitor: $errlvl";
    	my $subject = "On $HostName: $errlvl Observed/Logged";

        $msg->send_message ($source, $LOG_ERR_LVL, $subject, "$errtime: $errMessage\n");

      my $del_log_sql = "delete from $ERR_ALERT_TBL where $ERR_ALERT_id = '$errid' ";
      my $del_log_sth = $dbh->prepare($del_log_sql);
      $del_log_sth->execute();
      $del_log_sth->finish(); 
      $hostname_sth->finish();
  }

  $sth->finish();
}
##
#
# Get Amethyst GUID. GUID is saved in global variable $MY_AMETHYST_GUID
#
##
sub getAmethystGUID
{
  return;
  my ($conn) = @_;
  if ($MY_AMETHYST_GUID == $NULL_GUID)
  {
    # Get the assigned GUID
    my $sql="SELECT ValueData FROM transbandSettings where ValueName='GUID';";

  
	my $sth = $conn->sql_query($sql);

	my $ref = $conn->sql_fetchrow_hash_ref($sth);

    $MY_AMETHYST_GUID = $ref->{'ValueData'};

	$conn->sql_finish($sth);
  }
 
  #return $MY_AMETHYST_GUID;
}

#########test code
##
#
# Get logged error messages by the various hosts.
#
##
sub get_logged_error_messages 
{ 
  my ($dbh, $msg) = @_; 
  my $sql;
  my $sth;
  my $ref;
  my $alert_type_value = " ";
  my $errCriticality = 128;

  &getAmethystGUID($dbh);
  

  $sql = "SELECT distinct($ERR_ALERT_errMessage),$ERR_ALERT_id,$ERR_ALERT_hostid,$ERR_ALERT_errlvl,$ERR_ALERT_errtime,$ERR_ALERT_alert_type from $ERR_ALERT_TBL group by $ERR_ALERT_errMessage,$ERR_ALERT_hostid";

  $sth = $dbh->prepare($sql);
  $sth->execute();

  while ($ref = $sth->fetchrow_hashref())
  {
  	my $errid  = $ref->{$ERR_ALERT_id};
	my $errHostId  = $ref->{$ERR_ALERT_hostid};
	my $errlvl     = $ref->{$ERR_ALERT_errlvl};
	my $errMessage = $ref->{$ERR_ALERT_errMessage};
	my $errtime    = $ref->{$ERR_ALERT_errtime};
	my $alert_type = $ref->{$ERR_ALERT_alert_type};

	#6452
  	my $hostname_sql = "SELECT $HOSTS_name,$HOSTS_ipaddress from $HOSTS_TBL where $HOSTS_id = '$errHostId'";

	my $hostname_sth = $dbh->prepare($hostname_sql);
	$hostname_sth->execute();
	my $hostname_ref = $hostname_sth->fetchrow_hashref();

	my $HostName = $hostname_ref->{$HOSTS_name};
	#6452
	my $HostIpAddress = $hostname_ref->{$HOSTS_ipaddress};

        my $source = "monitor: $errlvl";
    	my $subject = "On $HostName: $errlvl Observed/Logged";

	if ($alert_type == 1){
		$errCriticality = 9; #error level
	}

	# Begin alert level 4 with supporting subject line changes depends on message
      
    if ($alert_type == 4) {
		$errCriticality = 3;
        my ($firstpart,@secondpart) = split('\n',$errMessage);
        $errMessage = '';
        foreach my $txt (@secondpart) {
			$errMessage .= $txt."\n";
        }
        $subject = "On $HostName: $firstpart";
    }
	
	# End Fix

		#6452
		chop($errMessage);
		$errMessage =$errMessage."on $HostName($HostIpAddress)";

        if ($errlvl eq 'ALERT')
        
        {
        	    my %arguments;
				$arguments{"id"} = $errHostId;
				$arguments{"errtime"} = $errtime;
				$arguments{"errlvl"} = $errlvl;
				my $params = \%arguments;
				my $err_id = $errHostId;
                my $err_message = "$errtime: $errMessage";
                my $err_summary = $subject;
                &Messenger::add_error_message ($dbh, $err_id, "AGENT_ERROR", $err_summary, $err_message, $errHostId);
				&Messenger::send_traps($dbh,"AGENT_ERROR",$err_message,$params);
	}
	else
	{
       		#$msg->send_message ($source, $LOG_ERR_LVL, $subject, "$errtime: $errMessage\n");
                my $err_id = $errHostId;
                my $err_message = "$errtime: $errMessage";
                my $err_summary = $subject;
                &Messenger::add_error_message ($dbh, $err_id, "AGENT_ERROR", $err_summary, $err_message, $errHostId);
	}

      my $del_log_sql = "delete from $ERR_ALERT_TBL where $ERR_ALERT_id = '$errid' ";
      my $del_log_sth = $dbh->prepare($del_log_sql);
      $del_log_sth->execute();
      $del_log_sth->finish(); 
      $hostname_sth->finish();
  }

  $sth->finish();
}

#########test code ends here
##
# Destructor
##
sub DESTROY {
  my ($obj) = @_;
  #print "Alas, ", $obj->{$DPSO_type}, " is now no longer with us \n";
}

sub create_dir
{
	 my ($dir, $obj, $logfile) = @_;

	 $logging_obj->log("DEBUG","Input directory has not been created yet $dir, creating..");
	 my $mkpath_output = mkpath($dir, { mode => 0777 });
	 
	 if ($? != 0) 
	 {
		  return -1;
	 }
	 # Make sure the directory allows for deletion by the windows hosts
	 if( Utilities::isWindows() == 0 )
	 {
		 #
		 # Bug 4590 - Replaced chmod with perl's chmod function
		 #
		 chmod (0777, $dir);
	 }
	 $logging_obj->log("DEBUG","Created input directory $dir");

	 return 0;
}

##
#
# Returns an array of associated cluster host name for the passed in host id and
# the returned array will not include the passed in source host id.
#
# Notes: Uses the database handle ($dbh) global to this package
#       Establishing connection is the responsibility of the caller
#
# Inputs: 
#    source host id
#    
#      
##
sub formatDeviceName
{
    my ($deviceName) = @_;

    if ($deviceName ne "")
    {
         $deviceName =~ s/\\\\/\\/g;
         $deviceName =~ s/\\/\\\\/g;
    }

    return $deviceName;
}
sub getAssociatedClusterName
{

     my ($conn,$id) = @_;
	 #Fix 10987
	my $dbh = $conn->get_database_handle();
	$dbh->{mysql_auto_reconnect} = 1;
	 my $clusterName;
	$clusterName = $conn->sql_get_value('hosts', 'name', "id = '$id'");	 

	return $clusterName;
}

sub clearTargetFiles
{
	my $targetErrFlag=1;
	my ($resyncFiles,$diffFiles) = @_;

	#clear_log("\n ---- clearTargetFiles ---\n");
	#clear_log("resyncFiles : $resyncFiles \n");
	#clear_log("diffFiles   : $diffFiles \n");

	my @tgt_diff_file_array = Utilities::read_directory_contents ($diffFiles, "/*.dat*");
	#clear_log("tgt_diff_file_array :: ".Dumper(\@tgt_diff_file_array)." \n");
  
	# Remove them each
	foreach my $dir (@tgt_diff_file_array)
	{
		my $rdir_dir_modified = Utilities::makePath("$diffFiles/$dir");
		#clear_log(" diff file path :: $rdir_dir_modified\n");
		my $tgt_unlink_status = unlink($rdir_dir_modified);
		#clear_log("tgt_unlink status :: $tgt_unlink_status\n");
	}
	
	my @tgt_resync_file_array = Utilities::read_directory_contents ($resyncFiles, "/*.dat*");
	#clear_log("tgt_resync_file_array :: ".Dumper(\@tgt_resync_file_array)." \n");
	foreach my $resyncdir (@tgt_resync_file_array)
	{
		my $resyncdir_dir_modified = Utilities::makePath("$resyncFiles/$resyncdir");
		#clear_log("resync file path :: $resyncdir_dir_modified\n");
		my $tgt_resync_unlink_status = unlink($resyncdir_dir_modified);
		#clear_log("tgt_unlink status :: $tgt_resync_unlink_status\n");
	}
	
	#my @tgt_resync_map_file_array = glob($resyncFiles."/*.Map");
	my @tgt_resync_map_file_array = Utilities::read_directory_contents ($resyncFiles, "/*.Map");
	#clear_log(" tgt_resync_map_file_array :: ".Dumper(\@tgt_resync_map_file_array)." \n");
	foreach my $resyncmapdir (@tgt_resync_map_file_array)
	{
		my $resyncmapdir_dir_modified = Utilities::makePath("$resyncFiles/$resyncmapdir");
		#clear_log("resync file path :: $resyncmapdir_dir_modified\n");
		my $tgt_resync_map_unlink_status = unlink($resyncmapdir_dir_modified);
		#clear_log("tgt_unlink status :: $tgt_resync_map_unlink_status\n");
	}
	return 0;
}
sub clearSourceFiles
{
	my ($Path) = @_;
	my $sourceErrFlag=1;
	my $diffFiles = $Path."/diffs";
	my $resyncFiles = $Path."/resync";
	
	#clear_log("\n ---- clearSourceFiles ---\n");
	#clear_log("Path        : $Path \n");
	#clear_log("resyncFiles : $resyncFiles \n");
	#clear_log("diffFiles   : $diffFiles \n");
	
	my @src_diff_file_array = Utilities::read_directory_contents ($diffFiles, "/*.*");
	#clear_log("src_diff_file_array :: ".Dumper(\@src_diff_file_array)." \n");
	
	# Remove them each
	foreach my $dir (@src_diff_file_array)
	{
		my $rdir_dir_modified = Utilities::makePath("$diffFiles/$dir");
		#clear_log("diff file path :: $rdir_dir_modified\n");
		my $src_unlink_status = unlink($rdir_dir_modified);
		#clear_log("src_unlink status :: $src_unlink_status\n");
	}
	
	
	my @src_resync_file_array = Utilities::read_directory_contents ($resyncFiles, "/*.dat*");
	#clear_log("tgt_resync_file_array :: ".Dumper(\@src_resync_file_array)." \n");
	foreach my $resyncdir (@src_resync_file_array)
	{
		my $resyncdir_dir_modified = Utilities::makePath("$resyncFiles/$resyncdir");
		#clear_log("resync file path :: $resyncdir_dir_modified\n");
		my $src_resync_unlink_status = unlink($resyncdir_dir_modified);
		#clear_log("src_unlink status :: $src_resync_unlink_status\n");
	}
	
	#my @tgt_resync_map_file_array = glob($resyncFiles."/*.Map");
	my @src_resync_map_file_array = Utilities::read_directory_contents ($resyncFiles, "/*.Map");
	#clear_log("src_resync_map_file_array :: ".Dumper(\@src_resync_map_file_array)." \n");
	foreach my $resyncmapdir (@src_resync_map_file_array)
	{
		my $resyncmapdir_dir_modified = Utilities::makePath("$resyncFiles/$resyncmapdir");
		#clear_log("resync file path :: $resyncmapdir_dir_modified\n");
		my $src_resync_map_unlink_status = unlink($resyncmapdir_dir_modified);
		#clear_log("src_unlink status :: $src_resync_map_unlink_status\n"); 
	}
				
	#clear_log("--- clearSourceFiles EXITED --- returng 0 \n\n");
	return 0;
}

#Clear all files and directory with the given directory name
sub clearFilesAndDirectory
{
	my ($dir_name) = @_;
	if (-e $dir_name)
	{
		my @files_array = Utilities::read_directory_contents ($dir_name);
		# Remove them each
		foreach my $file (@files_array)
		{
			my $rdir_dir_modified = Utilities::makePath("$dir_name/$file");
			
			#clear_log("diff file path :: $rdir_dir_modified\n");
			if (-e $rdir_dir_modified && -f $rdir_dir_modified)
			{
				my $src_unlink_status = unlink($rdir_dir_modified);
				#clear_log("src_unlink status :: $src_unlink_status\n");
			}
		}
		rmdir($dir_name);
	}
	return 0;
}		

# updates the jobid for all associated pairs (1-to-1 or 1-to-N or cluster or 1-to-N cluster)
sub update_jobid
{  
	my ($sourceId, $sourceDeviceName,$destinationId,$destinationDeviceName,$srcCompatibilityNo,$dstCompatibilityNo,$tmid,$clusterHostIds, $restartResyncCounter) = @_;
	my $ref_jobId;
	my $replication_cleanup_flags = " ";
	my $where_clause = " ";
	my $rand_val = int(rand(1000));
	my $newJobId = time()+$tmid+$rand_val;
	$logging_obj->log("DEBUG","update_jobid :: Jobid generated as $newJobId with tmId $tmid and rand $rand_val for $sourceId,$sourceDeviceName\n");

	my $sql_jobId_update;
	$logging_obj->log("DEBUG", "update_jobid : newjobId value is : $newJobId");
		
	my $hostIdIn = "('$sourceId'";
	if($clusterHostIds)
	{
		foreach my $id (@{$clusterHostIds})
		{
			$hostIdIn = $hostIdIn . ",'" .$id. "'";
		}
	}
	$hostIdIn = $hostIdIn . ")";
	sleep(1);
	my $query;
	if ($restartResyncCounter > 0)
	{
		$replication_cleanup_flags = ",replicationCleanupOptions = 0,replication_status = 0";
        # bug 2690592: Replication stuck because the PS is setting a job id for the rapid resyncs marked but not cleaned up.
        $where_clause = "and replication_status = 42 and replicationCleanUpOptions = 1 and restartResyncCounter > 0";
	}
	else
	{
		$where_clause = "and replication_status = 0 and replicationCleanUpOptions = 0 and restartResyncCounter = 0";
	}
	$query->{'jobid_upate'}->{'queryType'} = "UPDATE"; 
	$query->{'jobid_upate'}->{'tableName'} = "srcLogicalVolumeDestLogicalVolume",
	$query->{'jobid_upate'}->{'fieldNames'} = "jobId='$newJobId',lastResyncOffset=0,resyncStartTagtime=0 $replication_cleanup_flags ",
	$query->{'jobid_upate'}->{'condition'}	= "sourceHostId in $hostIdIn and  sourceDeviceName='$sourceDeviceName' and
					destinationHostId = '$destinationId' and destinationDeviceName = '$destinationDeviceName' and jobId = '0' $where_clause";
	&tmanager::update_cs_db("volsync",$query);
}

sub is_one_to_many_by_source 
{
 	my ($conn,$src_id, $src_dev) = @_;
	my $count = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'count(*)', "sourceHostId = '$src_id' and  sourceDeviceName='$src_dev'");

	if( $count > 1)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

##  Host Based compression Change
 sub getSizeBeforeCompression
 {
     my ($file) =@_;
     my $gzip_test = `gzip -l $file`;
    if ($? != 0)
	{
		$logging_obj->log("DEBUG","exit from TimeshotManager getSizeBeforeCompression as $gzip_test failed");
	     ##Gopal
         return 0;
    }
    else
    {
        $gzip_test =~ s|\s+|=|g or $gzip_test =~ s|\n+|=|g;
        #=compressed=uncompressed=ratio=uncompressed_name=11396=93014=87.8%=databasebackup_dec_13.sql1
        #my ($one,$one1,$one2,$one3,$one4,$compressed,$uncompressed,$ratio,$uncomp_file) = split(/=/,$gzip_test);
       my @result = split(/=/,$gzip_test);
        return $result[6];
    }

  }

sub customerBrandingNames
{
	my ($text_mod) = @_;
	if($AMETHYST_VARS->{'CUSTOM_BUILD_TYPE'} == 2) 
	{
		$text_mod=~s/FX Agent/File Agent/gi;
		$text_mod=~s/VX Agent/Volume Agent/gi;
		
		$text_mod=~s/PS/Process Service/gi;
		$text_mod=~s/Process Server/Process Service/gi;
		
		$text_mod=~s/CS/Control Service/gi;
		$text_mod=~s/Configuration Server/Control Service/gi;
		$text_mod=~s/Control Server/Control Service/gi;
		
		$text_mod=~s/CX/Replication Engine/gi;
		$text_mod=~s/CX Server/Replication Engine/gi;
		$text_mod=~s/Appliance/Replication Engine/gi;
		
		return $text_mod;
	}
	else
	{
		return $text_mod;
	}
}


#Added by BSR to fix issue# 1741
sub shouldQuit
{
	my ($base_path) = @_ ;
	#my $var_path;
	my $shutdown_file;
	if(Utilities::isLinux())
	{
		$shutdown_file = "/var/cx/".$SHUTDOWN_FILE;
	}
	elsif(Utilities::isWindows())
	{
		$shutdown_file = "$cs_installation_path\\home\\svsystems\\".$SHUTDOWN_FILE;
	}

	#my $shutdown_file = Utilities::makePath( $var_path."/".$SHUTDOWN_FILE ) ;
	if ( -e $shutdown_file ) 
	{
		$logging_obj->log("DEBUG","Exit from TimsshotManager shouldQuit because shutdown_file not exists ");
		return 1 ;
	}
	else
	{
		
		return 0 ;
	}
}

sub sleepAndShouldQuit
{
	my ($base_path, $sleep_time) = @_ ;
	#my ($var_path, $count);
	my $count;
	my $shutdown_file;
	if(Utilities::isLinux())
	{
		$shutdown_file = "/var/cx/".$SHUTDOWN_FILE;
	}
	elsif(Utilities::isWindows())
	{
		$shutdown_file = "$cs_installation_path\\home\\svsystems\\".$SHUTDOWN_FILE;
	}

	#my $shutdown_file = Utilities::makePath( $var_path."/".$SHUTDOWN_FILE ) ;
	
	for ($count = 0; $count <= $sleep_time; $count++)
	{
		if ( -e $shutdown_file ) 
		{
			$logging_obj->log("DEBUG","Exit from TimsshotManager shouldQuit because shutdown_file not exists ");
			return 1 ;
		}
		sleep(1); 		
	}
	return 0 ;
}

sub get_compatibility{

	my ($conn,$sourceHostId)=@_;
	my $compatibilityNo = $conn->sql_get_value('hosts', 'compatibilityNo', "$HOSTS_id = '$sourceHostId' and $HOSTS_sentinelEnabled = 1");
    
    if($compatibilityNo)
    {	
	   return $compatibilityNo;
    }
}

sub get_osFlag
{

	my ($conn,$HostId)=@_; 
	my $osFlag = $conn->sql_get_value('hosts', 'osFlag', "$HOSTS_id = '$HostId'");	
   
    if ($osFlag)
    {	
	   return $osFlag;
	}
}
sub checkSwitchThresold
{
    my ($conn,$HostId) = @_;
    my $sql = " SELECT
                    h.SysVolCap,
                    h.SysVolFreeSpace,
                    s.switchAgentSpaceWarnThreshold,
                    h.name,
                    h.ipaddress	
                FROM
                    $HOSTS_TBL h,
                    switchAgents s
                WHERE
                    h.$HOSTS_id = '$HostId' and h.$HOSTS_id = s.switchAgentId";
   
	my $result = $conn->sql_exec($sql);
	
	if (defined $result)
	{
		my @value = @$result;

		my $sysVolCap = $value[0]{'SysVolCap'};
		my $sysVolFreeSpace = $value[0]{'SysVolFreeSpace'};
		my $userThreshold = $value[0]{'switchAgentSpaceWarnThreshold'};
		my $switchName = $value[0]{'name'};
		my $switchIp = $value[0]{'ipaddress'};
		
		my $spaceUsed = (($sysVolCap-$sysVolFreeSpace)/$sysVolCap)*100;
			
		if ($spaceUsed > $userThreshold)
		{
			my $err_id = $HostId;
			my $err_message = "Disk space threshold on switch-BP exceeded for $switchName "."["."$switchIp"."]"."\n";
			my $err_summary = "Disk space threshold on switch-BP exceeded";
			&Messenger::add_error_message ($conn, $err_id, "SWITCH_USAGE", $err_summary, $err_message, $HostId);
			&Messenger::send_traps($conn,"SWITCH_USAGE",$err_message );
		}
		
	}
}

#
# Function name : createHardlink 
# Description   : creates hard link for a source file to target file
#               : uses a list of commands to create hard links for retries
#               : ln - cygwin ln
#               : mklink on MS Windows OS
# Input         : $source, $target
# Return Value  : 0 for success, command returned exit code for failures
#
sub createHardlink
{
    my($source, $target) = @_;
    my $status = 0;
    my $lp_source = "\\\\?\\" . $source;
    my $lp_target = "\\\\?\\" . $target;

    my @hardlink_cmds = ('mklink /H "'.$lp_target.'" "'.$lp_source.'"');
    $hardlink_cmds[1] = 'ln "'.$source.'" "'.$target.'"';

    for (my $count = 0; $count < @hardlink_cmds; $count++)
    {
        my $hardlink_cmd= $hardlink_cmds[$count];
        $logging_obj->log("DEBUG","createHardlink: Creating hard link using command $hardlink_cmd \n\n");
        my $return_val = `$hardlink_cmd 2>&1`;
        $status = $?;
        if ($status != 0) 
        { 
            $logging_obj->log("EXCEPTION","createHardlink: hard link creation failed: cmd $hardlink_cmd : status ($status:$return_val)\n\n");
            next;
        }
        else
        {
            $logging_obj->log("DEBUG", "createHardlink: hard link creation successful. cmd $hardlink_cmd : status ($status:$return_val)\n\n");
            last;
        }
    }

    return $status; 
}

#
# Function name : diffsFileProcess
# Description   : Using to process the pair.Processing the differential file.
#                 This is common function which is called in two cases : one is DB UP & other one is DB DOWN.
# Input         : $srchostid,$srcvol,$dsthostid,$dstvol
# Input         : $ifile_dir,$ofile_dir,$ls_output
# Input         : $one_to_many_targets,$one_to_many_dsthostid,$one_to_many_dstvol
# Input         : $compression_enable,$src_os_flag,$dst_os_flag,$obj,$called_from
# Return Value  : None
#
sub diffsFileProcess
{
	my($ls_output, $one_to_many_targets, $srchostid,$srcvol,$dsthostid,$dstvol,$ifile_dir,$ofile_dir,$compression_enable,$src_os_flag,$dst_os_flag,$is_rpo_valid,$obj,$called_from,$one_to_many) = @_;
	my $return_value;
	eval 
	{
        if(!($ls_output && $one_to_many_targets && $srchostid && $srcvol && $dsthostid && $dstvol && $ifile_dir && $ofile_dir && $src_os_flag && $dst_os_flag  && $called_from))
        { 
            return 0;
        }
		my $pStartTime = time();
		#$logging_obj->log("DEBUG","before diffsFileProcess ls_output check1\n");   
		my @ls_output = @$ls_output;		 
		#$logging_obj->log("DEBUG","diffsFileProcess ls_output  ::@ls_output after diffsFileProcess ls_output check2 \n".Dumper(\@ls_output));
		#my %one_to_many_targets = %$one_to_many_targets;
        #$logging_obj->log("DEBUG"," diffsFileProcess one_to_many_targets ::\n".Dumper(\@one_to_many_targets));
        #my @one_to_many_dsthostid = @$one_to_many_dsthostid;
        #$logging_obj->log("DEBUG","diffsFileProcess one_to_many_dsthostid ::\n".Dumper(\@one_to_many_dsthostid));
		#my @one_to_many_dstvol = @$one_to_many_dstvol;
        #$logging_obj->log("DEBUG","diffsFileProcess one_to_many_dstvol :: \n".Dumper(\@one_to_many_dstvol));
		#my @one_to_many_resync_start_times = @$one_to_many_resync_start_times;
       # $logging_obj->log("DEBUG","diffsFileProcess one_to_many_resync_start_times :: \n".Dumper(\@one_to_many_resync_start_times));           
        # BEGIN  bug10695 :- Added below block to kill volSync if tmanagerd is stopped.No need to process files.
	
		if( &shouldQuit( "$HOME_DIR" ) )
        {
			$logging_obj->log("DEBUG","Exit from TimeShotManager diffsFileProcess shouldQuit as shutdown file exists");
			$logging_obj->log("DEBUG","Exiting the process as requested");
            exit 1;
        }
		#Check if tmansvc is stopped
		if (Utilities::isWindows())
		{
			unless(-e "$cs_installation_path\\home\\svsystems\\var\\tmansvc.pid")
			{
				$logging_obj->log("DEBUG","Exit from TimeShotManager diffsFileProcess as  ".$cs_installation_path."\home\svsystems\var\tmansvc.pid file exists");
				exit(0);
			}
		}
		# END  bug10695 #

		my $num_files_processed = 0;
		my $fileName;
		my $fileType;
		my $file_time;
		my $eventTime;
		my $output_file_modified_without_gz;
		
		my $cluster_ifile_dir;
		my $cluster_host_id;
		my $cluster_monitor_file;
		my $monitor_file = $ifile_dir . "/monitor.txt";        

		# bug 9337 start :- Deleting all processed file from profiler guid If Cs is down.
		if ($called_from ne "DB_UP")
		{
			# unlink is done when profiler taken as target & DB is down.
			foreach my $target (keys %{$one_to_many_targets})
			{
				if($one_to_many_targets->{$target}->{'dsthostid'} eq Common::Constants::PROFILER_HOST_ID )
				{
					unlinking_profiler_files($ofile_dir);
				}
			}
		}		
		# bug 9337 end
		my $clusterName;
		$clusterName = $obj->{$DPSO_host_hash}->{$srchostid}->{'name'};	
		$logging_obj->log("DEBUG","clusterName::$clusterName");
		
        foreach my $file (@ls_output) 
		{		
			# BEGIN  bug10695 :- Added below block to kill volSync if tmanagerd is stopped.No need to process files.
			if( &shouldQuit( "$HOME_DIR" ) )
			{
				$logging_obj->log("DEBUG","Exit from TimeShotManager diffsFileProcess shouldQuit as shutdown file exists");
				exit 1;
			}
		
			# note since there can be more then 1 dir being sorted together by diffdatasort
			# diffdatasort will return full path and file name to make sure all the files
			# get processed
			chomp $file;   
			$fileName = basename($file);
			my $perfline;
			my $file_modified = Utilities::makePath ($file,$src_os_flag);
			my $original_dat_file = $file_modified;

			my ($dev,$ino,$mode,$nlink,$uid,$gid, $rdev,$uncompressed_size,$atime,$mtime,$ctime,$blksize,$blocks);

			if(-e $file_modified)
			{		
        		if( $HAS_NET_IFCONFIG == 1 )
				{
					use if Utilities::isWindows(), "Win32::UTCFileTime" ;
					($dev,$ino,$mode,$nlink,$uid,$gid, $rdev,$uncompressed_size,$atime,$mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat $file_modified;				 
				}
				else
				{
					($dev,$ino,$mode,$nlink,$uid,$gid, $rdev,$uncompressed_size,$atime,$mtime,$ctime,$blksize,$blocks) = stat $file_modified;
				}
			}
			else
			{
				$logging_obj->log("DEBUG","File doesnt exist :: $file_modified");
				return 0;
			}

			###hostBASEd 
			if ($compression_enable == 2) ##Host based compression Change
			{
				$logging_obj->log("DEBUG","in Host based compression Change check3\n"); 
				my $size_before_compression = &getSizeBeforeCompression($file_modified);
				if($size_before_compression ne 0 ) ## in this case $uncompressed_size stands for current file size (comprressed size) bit confused with variable name
				{
					$uncompressed_size = $size_before_compression;
				}
				else
				{
					$logging_obj->log("DEBUG","Host Based Compression failed for file :: $file_modified");
				}
			}
		
			# Skip directories for now
			if (!(-f $file_modified)) 
			{
				$logging_obj->log("DEBUG","3) Skipping special file or directory: $file_modified");
				next;
			}

			my $output_file = $ofile_dir . "/" . $ECB_OFILE_PFX . basename($file);
			my $output_file_modified = Utilities::makePath ($output_file,$dst_os_flag);

			my ($t_file,$t_dir,$t_ext) = fileparse($output_file_modified , qr/\.[^.]*/);

			# start - issue 4496
			if(($mtime ne 0) && (defined($mtime)) && ($mtime ne "") )
			{
				if ($t_ext ne $GZIP_EXTENSION)
				{
					$output_file_modified =~ s/(\.dat$)/_$mtime$1/i;
								                         $output_file_modified_without_gz = $output_file_modified;				
				}
				else
				{
					$output_file_modified =~ s/(\.dat.gz$)/_$mtime$1/i;
				}
			}
			else
			{
				$logging_obj->log("DEBUG","3) Modified time stamp is not defined: $mtime");
				return 0;
			}
			#End -issue 4496
			my $ren_output_file = $ifile_dir . "/" . $ECB_RENAME_PFX . basename($output_file_modified);
			my $ren_output_file_modified = Utilities::makePath ($ren_output_file,$dst_os_flag);

			my $do_compression = "true";

			# Check if it needs to be compressed. Compression is the default behaviour except when coalescion is enabled and successful.
			my $original_file = $file_modified ;
			
			#fix 4095 checking the file extension and if the file is already zipped format don't zipping it again
			my ($o_file,$o_dir,$o_ext) = fileparse($original_file, qr/\.[^.]*/);

			##
			#CHANGE THE CONDITION FOR CS_LESS_REPLICATION 
			##

			if (($o_ext eq ".dat") && $do_compression eq "true" && ($compression_enable == 1)) ##Host based compression Change
			{
				my ($t_file,$t_dir,$t_ext) = fileparse($output_file_modified, qr/\.[^.]*/);

				# Rename output file only if they are not already compressed
				if ($t_ext ne $GZIP_EXTENSION) 
				{
					# Note in the db that the sentinel is still alive, only if this is a new uncompressed file
					$output_file_modified = $output_file_modified . $GZIP_EXTENSION; #".gz";
					$ren_output_file_modified = $ren_output_file_modified .$GZIP_EXTENSION; #".gz";
				}

				################ 9448 : CHECKING IF ZIPPED OUTPUT FILE IS ALREADY PRESENT IN TGT GUID #######
				my $compression_status;

				if(!(-e $ren_output_file_modified))
				{
					$logging_obj->log("DEBUG","diffsFileProcess:: before compressing original_file check4, compress file original_file: $original_file to ren_output_file_modified:$ren_output_file_modified \n");					
        			$compression_status = &compress_file($obj, $original_file, $ren_output_file_modified);
					$logging_obj->log("DEBUG","after compressing original_file check5\n"); 
				}
                ############ 9448 #############
                               
				if( $compression_status != 1 )
				{
					$logging_obj->log("DEBUG","Compressing the file $original_file failed, rc: $?");
					#
					# If the compression failed, remove the file before 
					# we return out of the loop else this file will never 
					# get removed (even if we overwrite the file during the 
					# next iteration) and can lead to resync stuck issues.
					#
                                        
                    $logging_obj->log("DEBUG","before  compressing original_file failed check6\n");  
					unlink($ren_output_file_modified);								
					$logging_obj->log("DEBUG","diffsFileProcess:: compression failed so unlinking  ren_output_file_modified: $ren_output_file_modified, after  compressing original_file failed check7 \n");
					$ren_output_file_modified = $original_file;
					$output_file_modified = $output_file_modified_without_gz;
				}
				else
				{
					# Fix for wedged diff file on reboot
					my $gzip_test = `gzip -tv "$ren_output_file_modified"`;
					if ($? != 0) 
					{
						$logging_obj->log("DEBUG","The file $ren_output_file_modified fails a GZIP test, rc: $gzip_test");
						unlink( $ren_output_file_modified ) ;						
						$logging_obj->log("EXCEPTION","diffsFileProcess:: gzip test failed so unlinking  ren_output_file_modified: $ren_output_file_modified, after gzip failed check9 \n");
						$ren_output_file_modified = $original_file;
						$output_file_modified = $output_file_modified_without_gz;
					}
				}
			}
			else # no compression, directly move the file to output directory
			{
				#14387 Fix		
				if(($o_ext ne ".dat") && ($do_compression eq "true") && ($compression_enable == 1)) ##in-line compression
				{	
					$logging_obj->log("DEBUG","in if check10\n");				
					my $size_before_compression = &getSizeBeforeCompression($file_modified);					
					if($size_before_compression ne 0 ) 
					{
						$uncompressed_size = $size_before_compression;
					}
					else
					{
						$logging_obj->log("DEBUG","CXPS-inline Based Compression failed for file :: $file_modified");
					}					
				}

				if(!-e $ren_output_file_modified)
				{
                    # If the length of file exceeds 255 character, then we should not create the hard link with temp file.
                    # Marking resync if the file length exceed 255
                    # In case of Fail returns 0, else proceeds further
					my $new_temp_file = replace_string($ren_output_file_modified, $DOUBLE_BACKWARD_SLASH, $SINGLE_BACKWARD_SLASH);
					$new_temp_file = replace_string($new_temp_file, $DOUBLE_FWDWARD_SLASH, $SINGLE_FWDWARD_SLASH);
					my $file_length = length $new_temp_file;
					if($file_length > $MAX_FILE_LENGTH)
					{
						$logging_obj->log("DEBUG","diffsFileProcess : File $new_temp_file length exceeds 255. Setting resync for the respective pair\n\n");
						foreach my $target (keys %{$one_to_many_targets})
						{
							my $status = resync_link_failure($srchostid, $srcvol, $one_to_many_targets->{$target}->{'dsthostid'}, $one_to_many_targets->{$target}->{'dstvol'}, $file_modified, $ren_output_file_modified);
							if (0 == $status )
							{
								$logging_obj->log("EXCEPTION","Resync set failed when file : $new_temp_file length exceeds 255.\n");
								return 0;
							}
							else
							{
								$logging_obj->log("INFO","Successfully set Resync when file : $new_temp_file lenght exceeds 255.\n");
							}
						}
						$logging_obj->log("DEBUG","Deleting file : $file_modified\n");
						unlink ($file_modified);
					}
					else
					{
                        # Marking resync if temp file link creation fails
                        # In case of Fail retruns 0, else proceeds further
						$logging_obj->log("DEBUG","diffsFileProcess : Link to temp:: $file_modified-->$ren_output_file_modified \n\n");
                        my $link_status = createHardlink($file_modified, $ren_output_file_modified);
                        if ($link_status != 0)
                        {
                            foreach my $target (keys %{$one_to_many_targets})
                            {
                                my $status = resync_link_failure($srchostid, $srcvol, $one_to_many_targets->{$target}->{'dsthostid'}, $one_to_many_targets->{$target}->{'dstvol'}, $file_modified, $ren_output_file_modified);
                                if (0 == $status )
                                {
                                    $logging_obj->log("EXCEPTION","Resync set failed for failed hard link creation of $file_modified to $ren_output_file_modified.\n");
                                    return 0;
                                }
                                else
                                {
                                    $logging_obj->log("INFO","Successfully set Resync for failed hard link creation of $file_modified to $ren_output_file_modified.\n");									
                                }
                            }
							$logging_obj->log("DEBUG","Deleting files $ren_output_file_modified and $file_modified\n");
							unlink ($ren_output_file_modified);
							unlink ($file_modified);							
                            # what if the unlink fails?
						}
					}
				}	      
			}

			# create hard links for final path name
			# this takes care of both 1->1 and 1-> many case
			#
			my $target_dir;
			my $tgt_num = 0;
			my $update_query;
			my $batch_query;
			foreach my $tgt_num (keys %{$one_to_many_targets})
			{		
				$target_dir = $one_to_many_targets->{$tgt_num}->{'other_ofile_dir'};
				my $other_output_file = $target_dir . basename($output_file_modified);
				#Start of Fix 10435
				my $file_format_hash_ref = get_file_format($other_output_file);
	
				my %file_format_hash = %$file_format_hash_ref;
				my $file_parse_contains_new_file_format = $file_format_hash{'file_parse_contains_new_file_format'};

				my $file_prevEndTimestamp = $file_format_hash{'file_prevEndTimestamp'};
				my $file_prevEndseq = $file_format_hash{'file_prevEndseq'};
				my $file_prevContinuationId = $file_format_hash{'file_prevContinuationId'};
				my $file_currEndTimestamp = $file_format_hash{'file_currEndTimestamp'};
				my $file_currEndseq = $file_format_hash{'file_currEndseq'};
				my $file_currContinuationId = $file_format_hash{'file_currContinuationId'};

				# $target_dir = /home/svsystems/866a6e80-ba5b-4e0c-93de-7e8683835bfd//dev/mapper/VolGroup1-lvol1/				 
				# $target_dir = /home/svsystems/7B57AD2A-4C99-FE4C-A8307160CA0DE7A4/F:\mountF
				my @temp_array;
				my $temp_tgt_host_id;
				my $temp_tgt_vol;

				if ( $dst_os_flag == 1 ) #WINDOWS
				{
					$target_dir =~ s/:/:\\/;
					@temp_array = split('/',$target_dir);
					$temp_tgt_host_id = $temp_array[3];
					$temp_tgt_vol = $temp_array[4];
				}
				else # LINUX or SOLARIS
				{
					@temp_array = split('//',$target_dir);
					my @temp_host_id_array = split('/',$temp_array[0]);
					$temp_tgt_host_id = $temp_host_id_array[3];
					$temp_tgt_vol = $temp_array[1];
					$temp_tgt_vol = substr($temp_tgt_vol,0,length($temp_tgt_vol)-1);
					$temp_tgt_vol = "/".$temp_tgt_vol;					
				}
				
				my $resyncStartTagtime = $one_to_many_targets->{$tgt_num}->{'resyncStartTagtime'};				

				if( $file_parse_contains_new_file_format != 1 ) # OLD FILE FORMAT
				{
					$file_currEndTimestamp = get_pair_end_tag_time($ifile_dir,$file_modified);
				}
				
				if($file_currEndTimestamp >= $resyncStartTagtime)
				{
					if( -e $ren_output_file_modified )
					{
						if(!-e $other_output_file)
						{
							$logging_obj->log("DEBUG","diffsFileProcess : ln $ren_output_file_modified $other_output_file \n\n");
                            my $link_status = createHardlink($ren_output_file_modified, $other_output_file);
                            if ($link_status != 0)
							{
								if ($called_from eq "DB_UP")
								{
									# set resync required for this particular pair								
									#10334 Fix
									my $resyncSetCxtimestamp = $one_to_many_targets->{$tgt_num}->{'resyncSetCxTimeStamp'};
									my $autoResyncStartTime = $one_to_many_targets->{$tgt_num}->{'autoResyncStartTime'};
									#autoResyncStartTime == 0 means autoresync is not enabled for the pair
									
									if(($autoResyncStartTime == 0) || ($resyncSetCxtimestamp == 0))
									{
										# Marking resync if hard link creation fails
										# In case of Fail retruns 0, else proceeds further
										my $status = resync_link_failure($srchostid, $srcvol, $one_to_many_targets->{$tgt_num}->{'dsthostid'}, $one_to_many_targets->{$tgt_num}->{'dstvol'}, $ren_output_file_modified, $other_output_file);
										if (0 == $status )
										{
											$logging_obj->log("EXCEPTION","Resync set failed for failed hard link creation of $ren_output_file_modified to $other_output_file.\n");
											return 0;
										}
										else
										{
											$logging_obj->log("INFO","Successfully set Resync for failed hard link creation of $ren_output_file_modified to $other_output_file.\n");
										}
									}
                                    else
                                    {
                                        $logging_obj->log("EXCEPTION"," autoResyncStartTime=$autoResyncStartTime , resyncSetCxtimestamp=$resyncSetCxtimestamp !eq 0.\n");
                                        # what is behavior here ? should fail or continue?
                                    }
								}
                                else
                                {
								    $logging_obj->log("EXCEPTION"," $called_from !eq DB_UP.\n");
                                    # what is behavior here ? should fail or continue?
                                }
							}
							else 
							{
								#link creation succeeded. make sure permission are properly set for all 
								if( Utilities::isWindows() == 0 )
								{
									#
									# Bug 4590 - Replaced chmod command with perl's chmod function
									#
									chmod(0777, $other_output_file);
								}
							}
						}
						else
						{
							$logging_obj->log("EXCEPTION","diffsFileProcess : !-e $other_output_file \n\n");
						}
					}
					else
					{
						$logging_obj->log("EXCEPTION","diffsFileProcess : -e $ren_output_file_modified \n\n");
					}
				}
				else
				{
					$logging_obj->log("EXCEPTION","diffsFileProcess : $file_currEndTimestamp >= $resyncStartTagtime \n\n");
				}
				$tgt_num++;
			}
			if($batch_query)
			{
				&tmanager::update_cs_db("volsync",$batch_query);
			}	
			#END of Fix 10435
			# end of for loop creating the links
			#Start - issue no: 2249
			#Bug fix 3603
			#The output_file_modified is the hardlink file in target guid, which is created using the ren_output_file_modified in source guid,
			#Fetching the stat information of differential file at target guid immediately after creating the hardlinks. Earlier this was done after 
			#removing the differential file at the source guid. The below file stat information will be logged to perf logs.
			my $compressed_size;
			if( $HAS_NET_IFCONFIG == 1 )
			{
				use if Utilities::isWindows(), "Win32::UTCFileTime"  ;
				($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$compressed_size,$atime,$mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat $ren_output_file_modified ;
			}
			else
			{
				($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$compressed_size,$atime,$mtime,$ctime,$blksize,$blocks)	= stat $ren_output_file_modified;
			}
			#End - issue#2249
			
			
			# Now, remove the tmp file 

			if(-e $ren_output_file_modified)
            {
                $logging_obj->log("DEBUG","diffsFileProcess : Temp file del:: $ren_output_file_modified \n\n");
                my $tmp_unlink_status = unlink( $ren_output_file_modified );			
                if( !$tmp_unlink_status )
                {				
                    $logging_obj->log("EXCEPTION","Temp file $ren_output_file_modified del failed with status $tmp_unlink_status and with system error $!\n\n");
                    return 0;
                    # it should be ok to proceed 
                }
            }
            else
            {
                $logging_obj->log("EXCEPTION","Temp file $ren_output_file_modified not found. skipping unlink.\n\n");
            }


		
			# END bug:- 9337
			# every thing went well, so delete the original file
			if(-e $original_file)
			{
				$logging_obj->log("DEBUG","diffsFileProcess : Orig file del:: $original_file \n\n");
				my $unlink_status = unlink ( $original_file ) ;
				if( !$unlink_status )
				{					
					$logging_obj->log("EXCEPTION","Orig file $original_file failed with status $unlink_status \n\n");
					$logging_obj->log("EXCEPTION","Deleting differential sync file failed: $original_file\n. This file would be applied multiple times\n");
					#returning from here to make sure that target processes this file again and to avoid processing later files...
					return 0;
				}
				
				my $base_file_name = basename($output_file_modified);
				if ( $base_file_name =~ /_tag_/ )
				{
					my $tags_dir  = $ifile_dir . "\\tags";
					mkdir $tags_dir unless -d $tags_dir;
				
					my $tag_file = $tags_dir . "\\" . $base_file_name;
					
					open(TAG_FILE, ">$tag_file") || $logging_obj->log("EXCEPTION","diffsFileProcess : Tag file creattion failed : $tag_file \n\n") || return 0;
					close(TAG_FILE);					
					$logging_obj->log("DEBUG","diffsFileProcess : Tag file created : $tag_file \n\n");
				}
			}
            else
            {
                $logging_obj->log("EXCEPTION","Orig file $original_file not found. skipping unlink.\n\n");
            }


			# Make sure the file is deletable by the Outpost Agent
			if( Utilities::isWindows() == 0 )
			{
				#
				# Bug 4590 - Replaced chmod command with perl's chmod function
				#
				chmod (0777, $output_file_modified);
			}
			# Sample performance log line 
			# "PerfLine; Date; SrcHostGuid; SrcVolume; FileName; Size; Compressed Size; Compression Ratio\n"
			# Make sure we don't crash due to zero sized files

			if (($uncompressed_size != 0) && ($compressed_size != 0)) 
			{
				my $comp_ratio = 100 - ($compressed_size*100/$uncompressed_size);
				$comp_ratio = 100 - ($compressed_size*100/$uncompressed_size);

				## added to handle cluster perf.log issue
				my $Cus_perfline;
				
				## perf.log file
				$Cus_perfline = $PERF_PFX . ", " . localtime($mtime) . ", " . uc($clusterName) . ", " .$file . ", "  . $obj->{$DPSO_srcvol} . ", " . $uncompressed_size . ", " .$compressed_size . ", " .  "\n";
			
				my $assc_clust_perf_log = $obj->{$DPSO_basedir}."/".$obj->{$DPSO_srchostid}."/".$obj->{$DPSO_srcvol}."/perf.log";
				$assc_clust_perf_log = Utilities::makePath($assc_clust_perf_log,$src_os_flag);
	   
				#DPSLOG($obj->{$DPSO_logfile}, $NOTIFY_LOG_ONLY,  $assc_clust_perf_log);		
				DPSLOG($assc_clust_perf_log, $NOTIFY_LOG_ONLY, $Cus_perfline);
				# Update of perf.rrd
				my %report_args = ('basedir'=>$obj->{$DPSO_basedir},'srchostid'=>$obj->{$DPSO_srchostid}, 
									'dsthostid'=>$obj->{$DPSO_dsthostid}, 'srcvol'=>$obj->{$DPSO_srcvol}, 'dstvol'=>$obj->{$DPSO_dstvol});

				my $report = new ConfigServer::Trending::HealthReport(%report_args);
				$report->updatePerfTxt($uncompressed_size, $compressed_size, $obj->{$DPSO_srchostid}, $srcvol, $dsthostid, $dstvol, $src_os_flag, $one_to_many);
			}
			else 
			{
				$logging_obj->log("DEBUG","Zero sized file $file encountered");
			}	
			
			$num_files_processed++;
			$logging_obj->log("DEBUG","file_process ended");
			#Do this based on time slices instead
			if ( (time() - $pStartTime) > $MAX_BATCH_PROCESSING_CHUNK) 
			{
				return 0;
			}
		}
		
		if ($num_files_processed > 0)
		{
			$file_time = '';
			if($is_rpo_valid)
			{
				&monitor_tmanager($ifile_dir,$file_time,$fileName);
			}
			foreach $cluster_host_id (@{$obj->{$DPSO_clusterids}}) 
			{
				$cluster_ifile_dir = "$obj->{$DPSO_basedir}/$cluster_host_id/$obj->{$DPSO_srcvol}/$ECB_IDIR";
				if($is_rpo_valid)
				{
					&monitor_tmanager($cluster_ifile_dir,$file_time,$fileName);
				}
			}
		}
		#
		# If profiling mode is set, delete all the compressed files in the profiler guid
		#
		if ($called_from eq "DB_UP")
		{
			if ($obj->{$DPSO_ecb_enable_profiling} == 1) 
			{	
				unlinking_profiler_files($ofile_dir);
			}
		}
	};
	if ($@) 
	{		
		$logging_obj->log("EXCEPTION","EXCEPTION occurred in diffsFileProcess call :: ".$@);
	}
	return 0;
}


#
# Function name : unlinking_profiler_files
# Description   : Used to unlink processed file from profiler target guid (FOR PROFILER PAIR ONLY)
# Input         : $dir
# Return Value  : None
#
sub unlinking_profiler_files
{
	my $dir = shift;

	my @directory_contents_output = Utilities::read_directory_contents ($dir, '^completed_*');

	if(@directory_contents_output)
	{
		foreach my $file_in_profiler_guid (@directory_contents_output)
		{
			$file_in_profiler_guid = $dir . "/" . $file_in_profiler_guid;
			$file_in_profiler_guid = Utilities::makePath ($file_in_profiler_guid);

			my $profiler_unlink_status = unlink ($file_in_profiler_guid);

			if ($profiler_unlink_status == 1) 
			{	
				$logging_obj->log("DEBUG","Profiling Mode:UNLINK_SUCCESS of file_in_profiler_guid : $file_in_profiler_guid\n");
			}
			else
			{
				$logging_obj->log("DEBUG","Profiling Mode:UNLINK_FAILED of file_in_profiler_guid : $file_in_profiler_guid\n");
			}
		}
	}	
}

#
# Function name : set_dirty_block_cache
# Description   : Used to cache dirty block for diff files.
# Input         : $ifile_dir , $processed_file ,$src_os_flag
# Return Value  : None
#
sub set_dirty_block_cache
{
	my $ifile_dir = shift;
	my $processed_file_with_dir = shift;
	my $src_os_flag = shift;
	
	#$processed_file_with_dir = /home/svsystems/c594b1ca-e9a0-464a-92b6-e67df7a2cfa7//dev/mapper/VG_DATA-four/diffs/completed_diff_tso_S12573374414518760_227599_E12573374414518760_227600_ME1.dat 

	my @processed_file_array =  split('diffs/',$processed_file_with_dir);

	my @arr;
	my $dirty_block;
	my $resync_counter;
	my $processed_file;
	
	#$processed_file = completed_ediffcompleted_diff_tso_S12573551352694354_409944_E12573551352694354_409945_ME1_O1234_R2_1257334073.dat.gz
	#$processed_file = completed_ediffcompleted_diff_S12573552652576742_409948_E12573552652576742_409949_ME1_O1033_R1_1257334203.dat.gz
	
	$processed_file = $processed_file_array[1];

	if ( ($processed_file =~ /_tso_/) || ($processed_file =~ /_tag_/) ) 
	{
		@arr =  split('_',$processed_file);
		$dirty_block = substr($arr[9],1);
		$resync_counter = substr($arr[10],1);
	}
	else
	{
		@arr =  split('_',$processed_file);
		$dirty_block = substr($arr[8],1);
		$resync_counter = substr($arr[9],1);
	}

	my $data = $dirty_block."####".$resync_counter;
	my $dirty_block_cache_file = $ifile_dir."/dirty_block_cache.txt";
	
	$dirty_block_cache_file = Utilities::makePath($dirty_block_cache_file,$src_os_flag);

	open (DIRTY_BLOCK_CACHE_FILE_FH, ">$dirty_block_cache_file");
	print DIRTY_BLOCK_CACHE_FILE_FH $data."\n";
	close(DIRTY_BLOCK_CACHE_FILE_FH);
}



#
# Function name : get_host_compatibility_number
# Description   : Used to get compatibility number of host.
# Input         : $host_id
# Return Value  : None
#
sub get_host_compatibility_number
{
	my $conn  = shift;
	my $host_id = shift;

	my $host_compatiblity_no;
	my $compatiblity_no = $conn->sql_get_value('hosts', 'compatibilityNo', "$HOSTS_id = '$host_id' AND $HOSTS_sentinelEnabled = 1");
	if ($compatiblity_no)
	{	
		$host_compatiblity_no = $compatiblity_no;
	}

	return $host_compatiblity_no;
}

#Added as part of 10435
sub update_replication_details 
{
	my $dest_hostid = shift;
	my $dest_devicename = shift;
		
	$dest_devicename = &formatDeviceName($dest_devicename);
	my $update_query;
	$update_query->{'cleanup_update'}->{"queryType"} = "UPDATE";
	$update_query->{'cleanup_update'}->{"tableName"} = "srcLogicalVolumeDestLogicalVolume";
	$update_query->{'cleanup_update'}->{"fieldNames"} = "replicationCleanupOptions = 0,replication_status = 0";
	$update_query->{'cleanup_update'}->{"condition"} = "destinationHostId='$dest_hostid' AND destinationDeviceName='$dest_devicename'";	
 	&tmanager::update_cs_db("volsync",$update_query);
}	

#Added as part of 10435
sub get_file_format
{
	my $original_file = shift;

	my $file_parse_contains_new_file_format = 0;
	#original_file non-tso :: /home/svsystems/88f6f69d-3043-4a48-a653-767a180e1f78//dev/mapper/VolGroup1-lvol6/diffs/completed_diff_P127794935171250000_15_1_E127794935212187500_20_ME1.dat
	#original_file tso     :: /home/svsystems/88f6f69d-3043-4a48-a653-767a180e1f78//dev/mapper/VolGroup1-lvol6/diffs/completed_diff_tso_P127794935171250000_15_1_E127794935212187500_20_ME1.dat
	
	#original_file tso at target
#/home/svsystems/64ed8f6f-402a-4f42-814e-ba80c0ebc2b7//dev/md/dsk/d2/completed_ediffcompleted_diff_tso_S12620031693193554_53659_E12620031693193560_53660_ME1_1261965005.dat.gz

	#original_file at target
#/home/svsystems/64ed8f6f-402a-4f42-814e-ba80c0ebc2b7//dev/md/dsk/d2/completed_ediffcompleted_diff_S12620031693193554_53659_E12620031693193560_53660_ME1_1261965005.dat.gz

	#DebugTimeshotLog("get_file_format --- original_file :: $original_file\n");
	my %result;
	my $count = 0;
	my @processed_file_array =  split('diffs/',$original_file);	
	my $temp_processed_file = $processed_file_array[1];
	#DebugTimeshotLog("get_file_format --- temp_processed_file :: $temp_processed_file\n");

	if($temp_processed_file == '') # OLD FILE FORMAT
	{
		@processed_file_array =  split('_diff_',$original_file);	
		$count = scalar(@processed_file_array);
		#DebugTimeshotLog("get_file_format --- processed_file_array ::".Dumper(\@processed_file_array)."\n");
		#DebugTimeshotLog("get_file_format --- count :: $count\n");
		$temp_processed_file = $processed_file_array[scalar(@processed_file_array) -1 ];
		#DebugTimeshotLog("get_file_format --- temp_processed_file :: $temp_processed_file\n");

	}

	#$temp_processed_file = completed_diff_P127794935171250000_15_1_E127794935212187500_20_ME1.dat
	#$temp_processed_file = completed_diff_tso_P127794935171250000_15_1_E127794935212187500_20_ME1.dat	
	
	my @file_temp_array = split('.dat',$temp_processed_file);
	my $processed_file = $file_temp_array[0];
	#DebugTimeshotLog("get_file_format --- processed_file :: $processed_file\n");

	#P127794935171250000_15_1_E127794935212187500_20_ME1

	if($processed_file =~ m/P/)
	{
		$file_parse_contains_new_file_format = 1;
	}

	my @arr;
	my @temp_array;
	
	my $file_prevEndTimestamp ;
    my $file_prevEndseq ;
	my $file_prevContinuationId ;	
	my $file_currEndTimestamp ;
	my $file_currEndseq ;
	my $file_currContinuationId ;

	if ($file_parse_contains_new_file_format ==1 ) # NEW FILE FORMAT
	{
		if($count == 0) # src guid file
		{
			if ( $processed_file =~ /_tso_/ ) 
			{
				@arr =  split('diff_tso_',$processed_file);
			}
			elsif ( $processed_file =~ /_tag_/ ) 
			{
				@arr =  split('diff_tag_',$processed_file);
			}
			else
			{
				@arr =  split('diff_',$processed_file);
			}
			@temp_array = split('_',$arr[1]);
			$file_prevEndTimestamp = $temp_array[0];
			$file_prevEndTimestamp = substr($file_prevEndTimestamp,1);

			$file_prevEndseq = $temp_array[1];
			$file_prevContinuationId = $temp_array[2];

			$file_currEndTimestamp = $temp_array[3];	
			$file_currEndTimestamp = substr($file_currEndTimestamp,1);
			
			$file_currEndseq = $temp_array[4];
			$file_currContinuationId = $temp_array[5];
			$file_currContinuationId = substr($file_currContinuationId,2);
			
		}
		else # tgt guid file
		{
			if ( ($processed_file =~ /tso_/) || ($processed_file =~ /tag_/) ) 
			{
				#@arr =  split('diff_tso_',$processed_file);
				@temp_array = split('_',$file_temp_array[0]);
				$file_prevEndTimestamp = $temp_array[1];
				$file_prevEndTimestamp = substr($file_prevEndTimestamp,1);

				$file_prevEndseq = $temp_array[2];
				$file_prevContinuationId = $temp_array[3];

				$file_currEndTimestamp = $temp_array[4];	
				$file_currEndTimestamp = substr($file_currEndTimestamp,1);
				
				$file_currEndseq = $temp_array[5];
				$file_currContinuationId = $temp_array[6];
				$file_currContinuationId = substr($file_currContinuationId,2);
			}
			else
			{
				#@arr =  split('diff_',$processed_file);
				@temp_array = split('_',$file_temp_array[0]);
				$file_prevEndTimestamp = $temp_array[0];
				$file_prevEndTimestamp = substr($file_prevEndTimestamp,1);

				$file_prevEndseq = $temp_array[1];
				$file_prevContinuationId = $temp_array[2];

				$file_currEndTimestamp = $temp_array[3];	
				$file_currEndTimestamp = substr($file_currEndTimestamp,1);
				
				$file_currEndseq = $temp_array[4];
				$file_currContinuationId = $temp_array[5];
				$file_currContinuationId = substr($file_currContinuationId,2);
			}
		}		
	}
	else # OLD_FILE_FORMAT
	{
		@temp_array = split('_',$processed_file);

		if ( $processed_file =~ /tso_/ ) 
		{
			$file_currEndTimestamp = $temp_array[3];
		}
		else
		{
			$file_currEndTimestamp = $temp_array[2];
		}	
		$file_currEndTimestamp = substr($file_currEndTimestamp,1);
	}
	
	
	$result{'processed_file'} = $processed_file;
	$result{'file_parse_contains_new_file_format'} = $file_parse_contains_new_file_format;

	$result{'file_prevEndTimestamp'} = $file_prevEndTimestamp;
	$result{'file_prevEndseq'} = $file_prevEndseq;
	$result{'file_prevContinuationId'} = $file_prevContinuationId;

	$result{'file_currEndTimestamp'} = $file_currEndTimestamp;
	$result{'file_currEndseq'} = $file_currEndseq;
	$result{'file_currContinuationId'} = $file_currContinuationId;
	#DebugTimeshotLog("get_file_format --- result  : ".Dumper(\%result)." \n");
	return \%result;
}

#Added as part of 10435
sub get_pair_end_tag_time
{
	my $ifile_dir = shift;
	my $processed_file_with_dir = shift;
	
	#$processed_file_with_dir = /home/svsystems/c594b1ca-e9a0-464a-92b6-e67df7a2cfa7//dev/mapper/VG_DATA-four/diffs/completed_diff_tso_S12573374414518760_227599_E12573374414518760_227600_ME1.dat 

	my @processed_file_array =  split('diffs/',$processed_file_with_dir);

	my @arr;
	my $pair_endTagTime;
	my $processed_file;
	
	#$processed_file = completed_diff_tso_S12573374414518760_227599_E12573374414518760_227600_ME1.dat
	#$processed_file = completed_diff_S12573374414518760_227599_E12573374414518760_227600_ME1.dat
	
	$processed_file = $processed_file_array[1];

	if ( ( $processed_file =~ /_tso_/ ) || ( $processed_file =~ /_tag_/ ) )
	{
		@arr =  split('_',$processed_file);
		$pair_endTagTime = substr($arr[5],1);
	}
	else
	{
		@arr =  split('_',$processed_file);
		$pair_endTagTime = substr($arr[4],1);
	}
	#DebugTimeshotLog("get_pair_end_tag_time --- pair_endTagTime : $pair_endTagTime \n");
	return  $pair_endTagTime;
}


#sub newDebugTimeshotLog {
#  my ($content) = @_;
#  my $log_file = "/home/svsystems/hard_link.log";

#  my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
#  my $enable_log = $amethyst_vars->{'ENABLE_LOG'};

#  if (Utilities::isWindows())
#  {
#    $log_file = "c:\\home\\svsystems\\debugtimeshot.log";
#  }
#  if ( $enable_log == 1 )
#  {
#	open(LOGFILE, ">>$log_file") || print "Could not open fallback global logfile: debug.log , faulty installation?\n";
#    my $entry   = time() . ": " . $content ; # No newline added at end
#    print LOGFILE $entry;
#    close(LOGFILE);
#  }
#}

#
# Function name : is_device_clustered
# Description   : Used to check if  device is clustered
# Input         : $dbh , $hostid, $volume
# Return Value  : 1,0
#

sub is_device_clustered
{
    my ($dbh, $hostid, $volume) = @_;
    #Escape Device Name
    $volume = &formatDeviceName($volume);
    my $sql = "SELECT 
                *
            FROM 
                clusters
            WHERE 
                hostId='$hostid' 
            AND
                deviceName='$volume'";
    my $sth = $dbh->sql_query($sql);
    if ($dbh->sql_num_rows($sth) > 0)
    {
        return 1;
    }
    else
    {

        return 0;
    }
}

#
# Function name : get_clusterids_as_array
# Description   : Used to get all cluster nodes
# Input         : $dbh , $hostid, $volume
# Return Value  : cluster nodes
#

sub get_clusterids_as_array
{
    my ($dbh, $hostid, $volume) = @_;
    my @clusterIds;
    $volume = &formatDeviceName($volume);

	my $query = "SELECT 
                c2.hostId
            FROM
                clusters c1,
                clusters c2
            WHERE
                c1.hostId = '$hostid' 
            AND
                c1.clusterGroupId = c2.clusterGroupId 
            AND
                c1.deviceName = c2.deviceName 
            AND
                c1.deviceName = '$volume'";

    my $sth = $dbh->sql_query($query);
    
    while( my $row = $dbh->sql_fetchrow_array_ref($sth) ) 
    {
        push( @clusterIds, $row->[0]);
    }
    return @clusterIds;
}

#
# Function name : get_pair_type
# Description   : Used to get pair type
# Input         : $dbh , $scsi_id 
# Return Value  : pair_type 0=host,1=fabric,2=prism
#
sub get_pair_type
{
	my $conn = shift;
	my $scsi_id = shift;
	my $sql = "SELECT
	                ap.solutionType 
                FROM
                    applicationPlan ap,
					srcLogicalVolumeDestLogicalVolume sldl 
			    WHERE
				    sldl.Phy_Lunid = '$scsi_id' 
				AND 
					ap.planId = sldl.planId";
 
	my $result = $conn->sql_exec($sql);
	my $solution = 0;
	
	my @value = @$result;

	my $solution_type = $value[0]{'solutionType'};
	
	if($solution_type eq 'HOST')
	{
		$solution = 0;
	}
	elsif($solution_type eq 'PRISM')
	{
		$solution = 2;
	}
	elsif($solution_type eq 'ARRAY')
	{
		$solution = 3;
	}
		
	return $solution;
}

sub get_bitmap_mode_change
{
	my ($dbh) = @_;
	my $query = "SELECT DISTINCT sourceHostId,sourceDeviceName FROM srcLogicalVolumeDestLogicalVolume";
    my $sth = $dbh->sql_query($query);    
    while( my $row = $dbh->sql_fetchrow_hash_ref($sth) )
    {
		my $data_file = $AMETHYST_VARS->{'INSTALLATION_DIR'}.'/'.$row->{'sourceHostId'}.'/'.$row->{'sourceDeviceName'}.'/datamode.txt';
		open(DAT, $data_file) || die("Could not open file!");
		my $raw_data=<DAT>;
		close(DAT);
		
		my @str_parts = split(/:/, $raw_data);		
		my ($err_id, $err_summary, $err_message);		
		$err_id = 'Source host ('.$row->{'sourceHostId'}.') Source drive ('.$row->{'sourceDeviceName'}.')';		
		
		
		my $str_parts_count = @str_parts;		
		if($str_parts_count == 4 && $str_parts[3] == 0)
		{
			
			 if($str_parts[2] == 2) {				
				$err_summary = "Replication Accuracy for the source drive (".$row->{'sourceDeviceName'}.") of host (".$row->{'sourceHostId'}.") has changed to Guaranteed";
				$err_message = "Replication Accuracy for the source drive (".$row->{'sourceDeviceName'}.") of host (".$row->{'sourceHostId'}.") has changed to Guaranteed";
			  }
			  else 
			  {				
				$err_summary = "Replication Accuracy for the source drive (".$row->{'sourceDeviceName'}.") of host (".$row->{'sourceHostId'}.") has changed to Not Guaranteed";
				$err_message = "Replication Accuracy for the source drive (".$row->{'sourceDeviceName'}.") of host (".$row->{'sourceHostId'}.") has changed to Not Guaranteed";
			  }			
			my %arguments;
			$arguments{"id"} = $row->{'sourceHostId'};
			my $params = \%arguments;
			&Messenger::add_error_message ($dbh, $err_id, "PROTECTION_ALERT", $err_summary, $err_message, $row->{'sourceHostId'});
			&Messenger::send_traps($dbh,"PROTECTION_ALERT_APP",$err_message,$params);
			open(DAT, ">$data_file");
			flock(DAT, LOCK_EX);
			print DAT $str_parts[0].":".$str_parts[1].":".$str_parts[2].":1";
			flock(DAT, LOCK_UN);
			close(DAT);
			my $raw_data2=<DAT>;
			close(DAT);
		}
    }
}

#Find out the host name of the CS Server
sub getCSHostName
{
    my ($conn) = @_;
	
	my ($cond, $appliacneClusterIdOrHostId, $nodeId, $name);
	$appliacneClusterIdOrHostId = $conn->sql_get_value('standByDetails', 'appliacneClusterIdOrHostId', "role = 'PRIMARY'");

	$cond = ($appliacneClusterIdOrHostId) ? "AND (nodeId='$appliacneClusterIdOrHostId' or applianceId='$appliacneClusterIdOrHostId')" : "";
	
	$nodeId = $conn->sql_get_value('applianceNodeMap', 'nodeId', "nodeRole = 'ACTIVE' $cond");
	
	$cond = ($nodeId) ? "AND id='$nodeId'" : (($appliacneClusterIdOrHostId) ? "AND id='$appliacneClusterIdOrHostId'" : '');
	
	$name = $conn->sql_get_value('hosts', 'name', "csEnabled = '1' $cond");
    
	return $name;
}

#Find out the host name of the PS Server
sub getPSHostName 
{    
    my ($ps_id, $conn) = @_;
  
  	my $PSHostName = $conn->sql_get_value('hosts', 'name', "$HOSTS_id = '$ps_id'");

    
    return $PSHostName;
}

sub getCSHostNameForPS
{
	my ($api_output) = @_;
	my $cs_ip = (exists $AMETHYST_VARS->{'PS_CS_IP'}) ? $AMETHYST_VARS->{'PS_CS_IP'} : $AMETHYST_VARS->{'CS_IP'};

	my $app_clus_data = $api_output->{'app_clus_data'};
	my $stand_by_data = $api_output->{'stand_by_data'};
	my $host_data = $api_output->{'host_data'};
	my $app_node_data = $api_output->{'appliance_node_data'};
	my $cluster_ip_count = 0;
	my $active_host_id;
	my $cshostname;
	my $test = scalar keys %{$app_clus_data};	
	
	foreach my $host_id (keys %{$app_clus_data}){
		if($app_clus_data->{$host_id} == $cs_ip) {
			$cluster_ip_count = 1;
		}
	}
		
	
	if($cluster_ip_count) 
	{
		foreach my $host_id (keys %{$app_node_data}) 
		{
			if($app_node_data->{$host_id}->{'nodeRole'} == "ACTIVE") {
				$active_host_id = $host_id;
			}
		}
		
		foreach my $standby_host_id (keys %{$stand_by_data})
		{			
			if($stand_by_data->{$active_host_id}->{'role'} == "PRIMARY") {
				$active_host_id = $standby_host_id;
			}
		}
		
		$cshostname = $host_data->{$active_host_id}->{'name'};
	}
	else
	{
		foreach my $host_id (keys %{$host_data})
		{
			my $ip_address =  $host_data->{$host_id}->{'ipaddress'};	
			if($cs_ip eq $ip_address)
			{
				$cshostname = $host_data->{$host_id}->{'name'};
			}
		}
	}
	return $cshostname;
}

sub cx_maintenance_mode
{
	my ($conn) = @_;
	return 0;
}

#
# Object specific logging
##
sub DPSLOG {
  #my ($obj, $content) = @_;
 my ($logfile, $debug_level, $content) = @_;

  # Change this to suit your logging level
  if ($debug_level > $DEBUG_LEVEL) {
    return;
  }
  #$logging_obj->log("DEBUG","DPSLOG: $content");

  if (!open(LOGFILE, ">>$logfile")) {
    print "Could not open $logfile for writing\n";
        $logging_obj->log("DEBUG","Could not open $logfile for writing");
    return;
  }

#  select(LOGFILE); $| = 1; # make unbuffered
   my $entry;
   if ($logfile eq '*/hosts.log'){
        my $now = time();
        my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst)=localtime(time);

        $year += 1900;
        $mon += 1;
        $entry   = "( ".$year."-".$mon."-".$mday." ".$hour.":".$min.":".$sec . " ): " . $content; # No newline added at end
        }
        else
         {
           $entry   = time() . ": " . $content; # No newline added at end
        }

  print LOGFILE $entry;
  close(LOGFILE);

  if ($debug_level <= $NOTIFY_ERROR_LEVEL &&
      $debug_level != $NOTIFY_LOG_ONLY
     ) {
    # Too important an error to just log and forget, this will be sent
    # to user via trap/email.
    # $REPORTED_ERROR_LOG = $REPORTED_ERROR_LOG . "\n".  $content;
  }
}

#
# This function is used to update the previous File Time Stamp,Name,File    
# type,and Current PS Time into the monitor.txt file.This function is called 
# when there is no file to monitor by tmanager process
#
sub monitor_tmanager
{
    my ($sourceDiffDir,$preFileTimeStamp,$fileName,$src_os_flag) = @_; # PASSED SRC_OS_FLAG TO GET OS_FLAG FOR SOLARIS AGENT.
    my $monitorFile=Utilities::makePath($sourceDiffDir."/monitor.txt",$src_os_flag);
    
	my $fileType;
	if($fileName=~m/_(ME).\./)
	{
		$fileType = $1;
	}
	elsif($fileName=~m/_(MC).\./)
	{
		$fileType = $1;
	}
	elsif($fileName=~m/_(WE).\./)
	{
		$fileType = $1;
	}
	elsif($fileName=~m/_(WC).\./)
	{
		$fileType = $1;
	} 
	
	# getting the current ps time
    my $currentTime = time();

	if($preFileTimeStamp == '')
	{
		$preFileTimeStamp = $currentTime;
	}
	
    my $str = $fileName."\n".$preFileTimeStamp.":".$currentTime.":".$fileType;
    if ($currentTime)
    {
        open(FH,">$monitorFile");
        print FH $str;
        close(FH);
    }
}

#This function is to set resync for the pair where hard link failures observed
#input Source ID
#input Source Device
#input Destination ID
#input Destination Device
#input Original File (Optional)
#input Linker File (Optional)
#returns 0 in case failure and 1 in case of success
sub resync_link_failure
{
	my ($srchostid,$srcvol,$dsthostid,$dstvol, $orgfile, $linkerfile) = @_;
	
	my $delimiter = "!@!@!";
	my $pair_info = "$srchostid$delimiter$srcvol$delimiter$dsthostid$delimiter$dstvol$delimiter$orgfile$delimiter$linkerfile";	
	my $api_output = tmanager::call_cx_api_for_data($pair_info,"setResyncFlag");
									
	$logging_obj->log("INFO","API output is $api_output for API setResyncFlag.\n");
	if (! defined $api_output )
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

#Function modifies original string by replacing sub-strings on finding the required pattern
#input original string
#input Pattern to find
#input Replace with sub-string
#output modified new string
sub replace_string
{
	my ($orgstring,$find,$replace) = @_;
	my $pos = index($orgstring, $find);
	while ( $pos > -1 ) 
	{
		substr($orgstring, $pos, length($find), $replace);
		$pos = index($orgstring, $find, $pos + length($replace));
	}
	return $orgstring;
}


sub monitordatafortelemetry 
{
	my ($srcvol,$src_id,$dest_id, $dest_vol,$is_cluster,$cluster_ids,$data_hash,$pair_id, $sys_boot_unix_time, $cachefolder_usedvol_size) = @_;
	my @cluster_ids;  
	my @other_ifile_dirs;
	my @source_folder_diffs;
	
	my $pair_data = $data_hash->{'pair_data'};
	my $host_data = $data_hash->{'host_data'};
	my $plan_data = $data_hash->{'plan_data'};
	
	my $org_pair_id = $pair_id."!!".$src_id;
	
	my $src_os_flag = $host_data->{$src_id}->{'osFlag'};
	my $dst_os_flag = $host_data->{$dest_id}->{'osFlag'};
	my $rpo_threshold = $pair_data->{$org_pair_id}->{'rpoSLAThreshold'};	
	my $deleted = $pair_data->{$org_pair_id}->{'deleted'};
	my $src_replication_status = $pair_data->{$org_pair_id}->{'src_replication_status'};
	my $tar_replication_status = $pair_data->{$org_pair_id}->{'tar_replication_status'};
	my $is_resync = $pair_data->{$org_pair_id}->{"isResync"};
	my $src_last_host_updatetime = $host_data->{$src_id}->{'lastHostUpdateTime'};
	my $src_appagent_last_host_updatetime = $host_data->{$src_id}->{'lastHostUpdateTimeApp'};
	my $tar_last_host_updatetime = $host_data->{$dest_id}->{'lastHostUpdateTime'};
	my $src_agent_version = $host_data->{$src_id}->{'prod_version'};
	my $ps_version = $host_data->{$MY_AMETHYST_GUID}->{'prod_version'};
	
	#Telemetry data variables declaration
	my %telemetry_data = ();

	$telemetry_data{"CSHostId"} = "";
	$telemetry_data{"SrcAgentVer"} = (defined $src_agent_version) ? $src_agent_version : "";
	$telemetry_data{"SysTime"} = time();
	$telemetry_data{"LastMoveFile"} = "";
	$telemetry_data{"LastMoveFileProcTime"} = 0;
	$telemetry_data{"LastMoveFileModTime"} = 0;
	$telemetry_data{"SrcFldSize"} = 0;
	$telemetry_data{"SrcNumOfFiles"} = 0;
	$telemetry_data{"SrcFirstFile"} = "";
	$telemetry_data{"SrcFF_ModTime"} = 0;
	$telemetry_data{"SrcLastFile"} = "";
	$telemetry_data{"SrcLF_ModTime"} = 0;
	$telemetry_data{"SrcNumOfTagFile"} = 0;
	$telemetry_data{"SrcFirstTagFileName"} = "";
	$telemetry_data{"SrcFT_ModTime"} = 0;
	$telemetry_data{"SrcLastTagFileName"} = "";
	$telemetry_data{"SrcLT_ModTime"} = 0;
	$telemetry_data{"TgtFldSize"} = 0;
	$telemetry_data{"TgtNumOfFiles"} = 0;
	$telemetry_data{"TgtFirstFile"} = "";
	$telemetry_data{"TgtFF_ModTime"} = 0;
	$telemetry_data{"TgtLastFile"} = "";
	$telemetry_data{"TgtLF_ModTime"} = 0;
	$telemetry_data{"TgtNumOfTagFile"} = 0;
	$telemetry_data{"TgtFirstTagFileName"} = "";
	$telemetry_data{"TgtFT_ModTime"} = 0;
	$telemetry_data{"TgtLastTagFileName"} = "";
	$telemetry_data{"TgtLT_ModTime"} = 0;
	# Changed from 1 to 3 in 1707. Newly added columns are DiffThrotLimit,ResyncThrotLimit,ResyncFldSize,VolUsageLimit,UsedVolSize,FreeVolSize,CacheFldSize,CustomJson.
	$telemetry_data{"MessageType"} = 3;
	
	$telemetry_data{"HostId"} = (defined $src_id) ? $src_id : "";
	$telemetry_data{"DiskId"} = (defined $srcvol) ? $srcvol : "";
	$telemetry_data{"PSHostId"} = (defined $MY_AMETHYST_GUID) ? $MY_AMETHYST_GUID : "";
	my $pair_resync_throttle = $pair_data->{$org_pair_id}->{'throttleresync'};
	my $pair_diff_throttle = $pair_data->{$org_pair_id}->{'throttleDifferentials'};	
	my $ps_data = $data_hash->{'ps_data'}; 
	my $pair_cummulative_throttle = 0;
	foreach my $ps_host_id (keys %{$ps_data})
	{
		if($ps_host_id eq $MY_AMETHYST_GUID)
		{
			$pair_cummulative_throttle = $ps_data->{$ps_host_id}->{'cummulativeThrottle'};
		}
	}
	my $src_pause_state = $pair_data->{$org_pair_id}->{'src_replication_status'};
	my $tgt_pause_state = $pair_data->{$org_pair_id}->{'tar_replication_status'};
	
	foreach my $host_id (keys %{$host_data})
	{
		if($host_data->{$host_id}->{'csEnabled'} == 1)
		{
			$telemetry_data{"CSHostId"} = $host_id;
		}
	}
	
	$telemetry_data{"PsAgentVer"} = ($ps_version ne "") ? $ps_version : "";
	$telemetry_data{"SysBootTime"} = (defined $sys_boot_unix_time && $sys_boot_unix_time ne "") ? $sys_boot_unix_time : 0;
	
	my $current_rpo_time  = Utilities::datetimeToUnixTimestamp($pair_data->{$org_pair_id}->{'currentRPOTime'});
	my $status_update_time = Utilities::datetimeToUnixTimestamp($pair_data->{$org_pair_id}->{'statusUpdateTime'});
	my $current_rpo = $status_update_time - $current_rpo_time;
	
	my $pair_rpo_violation = 0;
	$rpo_threshold = $rpo_threshold * 60;
	if ($current_rpo > $rpo_threshold and (!$is_resync))
	{
		$pair_rpo_violation = 1;
	}
	
	$telemetry_data{"SvAgtHB"} = (defined $src_last_host_updatetime && $src_last_host_updatetime ne "") ? $src_last_host_updatetime : 0;
	$telemetry_data{"AppAgtHB"} = (defined $src_appagent_last_host_updatetime && $src_appagent_last_host_updatetime ne "") ? $src_appagent_last_host_updatetime : 0;
	
	my $src_to_ps_file_upload_stuck = 0;
	my $monitor_txt_file_exists = 0;
	my $get_monitor_file = Utilities::makePath($HOME_DIR."/".$src_id."/".$srcvol."/diffs/monitor.txt", $src_os_flag);	
	if (-e $get_monitor_file)
	{
		open(FH,"$get_monitor_file");
		my @file_content = <FH>;
		close (FH);
		$telemetry_data{"LastMoveFile"} = (defined $file_content[0]) ? $file_content[0] : "";
		chomp($telemetry_data{"LastMoveFile"});
		my @time_stamps = split(/:/,$file_content[1]);
		$telemetry_data{"LastMoveFileModTime"} = (defined $time_stamps[0] && $time_stamps[0] ne "") ? $time_stamps[0] : 0;
		$telemetry_data{"LastMoveFileProcTime"} = (defined $time_stamps[1] && $time_stamps[1] ne "") ? $time_stamps[1] : 0;
		
		if(($telemetry_data{"LastMoveFileModTime"} - $telemetry_data{"LastMoveFileProcTime"}) > $SOURCE_FAILURE && $telemetry_data{"LastMoveFileModTime"} != 0)
		{
			$src_to_ps_file_upload_stuck = 1;
		}
		$monitor_txt_file_exists = 1;
	}
	
	# Get diff sync throttle limit - in bytes
	my $diffFilesThreshold = $MAX_DIFF_FILES_THRESHOLD;
	if (exists $data_hash->{'lv_data'}->{$src_id."!!".$srcvol})
	{
		my $db_diff_threshold = $data_hash->{'lv_data'}->{$src_id."!!".$srcvol}->{'maxDiffFilesThreshold'};
		if ($db_diff_threshold =~ /^\d+/ )
		{
			$diffFilesThreshold = $db_diff_threshold;
		}
	}
	$telemetry_data{"DiffThrotLimit"} = $diffFilesThreshold;
	
	# Get resync throttle limit - in bytes
	my $max_resync_threshold = $pair_data->{$org_pair_id}->{'maxResyncFilesThreshold'};
	$telemetry_data{"ResyncThrotLimit"} = ($max_resync_threshold =~ /^\d+/) ? $max_resync_threshold : $MAX_RESYNC_FILES_THRESHOLD;
	
	# Calculate resync folder size
	my $resync_folder_path = $REPLICATION_ROOT."/".$dest_id."/".$dest_vol."/resync/";
	$resync_folder_path = Utilities::makePath($resync_folder_path,$dst_os_flag);
	$dirs_size = 0;
	$folder_to_query = 'resync';
	find(\&get_file_size, $resync_folder_path);
	$telemetry_data{"ResyncFldSize"} = $dirs_size;

	# Get volume usage (cumulative throttle) limit
	my $disk_space_warn_threshold = $DISK_SPACE_WARN_THRESHOLD;
	if(exists $data_hash->{'tbs_data'}->{'DISK_SPACE_WARN_THRESHOLD'})
	{
		$disk_space_warn_threshold = $data_hash->{'tbs_data'}->{'DISK_SPACE_WARN_THRESHOLD'};
	}
	$telemetry_data{"VolUsageLimit"} = $disk_space_warn_threshold;
	$telemetry_data{"CacheFldSize"} = $cachefolder_usedvol_size->{"CacheFldSize"};
	$telemetry_data{"UsedVolSize"} = $cachefolder_usedvol_size->{"UsedVolSize"};
	$telemetry_data{"FreeVolSize"} = $cachefolder_usedvol_size->{"FreeVolSize"};

	my $is_resync = $pair_data->{$org_pair_id}->{"isResync"};
	my $is_quasiflag = $pair_data->{$org_pair_id}->{'isQuasiflag'};
	$is_quasiflag = ($is_resync)? $is_quasiflag : 0;
	
	# Bits:                             10           9                 8                          7                      6                    5                      4                     3                      2                1             0
	$telemetry_data{"PairState"} = $is_quasiflag.$is_resync.$monitor_txt_file_exists.$src_to_ps_file_upload_stuck.$pair_rpo_violation.$pair_resync_throttle.$pair_diff_throttle.$pair_cummulative_throttle.$src_pause_state.$tgt_pause_state.$deleted;
	
	my $ifile_dir;
	my $telemetry_json_str = "";

	if($is_cluster)
	{			
		#for each node construct a input file directory & store in list
		@cluster_ids = @$cluster_ids ;
		foreach my $cluster_host_id (@cluster_ids) 
		{
			#construct the cluster input file directory path
			$ifile_dir = "$HOME_DIR/" . $cluster_host_id . "/" .$srcvol . "/" . ($ECB_IDIR);
			# This is the directory where we will find the Sentinel uploaded files
			# make ensures proper path is constructed as per underlined os
			$ifile_dir = Utilities::makePath($ifile_dir,$src_os_flag);
			#if failed to create the directory then return  
			if (!-d $ifile_dir) 
			{
				$logging_obj->log("EXCEPTION","monitordatafortelemetry Input directory has not been created yet $ifile_dir");				
				return $telemetry_json_str;
			}
			push(@other_ifile_dirs,$ifile_dir);	
		}
	}
	else
	{
		#This block is for non cluster source
		@cluster_ids = $src_id;
		#Contruct the input file directory
		$ifile_dir =  "$HOME_DIR/" . $src_id . "/" .$srcvol . "/" . ($ECB_IDIR);
		$ifile_dir = Utilities::makePath($ifile_dir,$src_os_flag);
		# Return if the input directory has not been created yet
		if (!-d $ifile_dir) 
		{
			$logging_obj->log("EXCEPTION","monitordatafortelemetry Input directory has not been created yet $ifile_dir");			
			return $telemetry_json_str;
		}
		push(@other_ifile_dirs,$ifile_dir);
    }
	#store the input file directory in a list
	
	# This is where the compressed difference files are kept
	my $ofile_dir =  "$HOME_DIR/" . $dest_id . "/" .$dest_vol;
	$ofile_dir = Utilities::makePath($ofile_dir,$dst_os_flag);	
	# Return if the destination directory has been created
	if (!-d $ofile_dir) 
	{
		$logging_obj->log("EXCEPTION","monitordatafortelemetry Output directory has not been created yet $ofile_dir");		
		return $telemetry_json_str;
	}	

	my @target_folder_diffs = Utilities::readDirContentsInSortOrder($ofile_dir, '^completed_ediff');	
	#collect the source files in an array
	#for cluster case consider all nodes invloved in cluster  
	foreach my $source_dirs (@other_ifile_dirs)  
	{
		push(@source_folder_diffs,Utilities::readDirContentsInSortOrder($source_dirs, '^completed_diff'));
	}

	if(@source_folder_diffs)
	{
		my (%src_diffs_hash) = &diffsFolderProcess($ofile_dir, \@other_ifile_dirs, \@source_folder_diffs, 0);
		if(%src_diffs_hash)
		{
			$telemetry_data{"SrcFldSize"} = $src_diffs_hash{"total_pending_diffs"};
			$telemetry_data{"SrcNumOfFiles"} = $src_diffs_hash{"diff_files_count"};
			$telemetry_data{"SrcFirstFile"} = $src_diffs_hash{"diff_first_file_name"};
			$telemetry_data{"SrcFF_ModTime"} = $src_diffs_hash{"diff_first_file_timestamp"};
			$telemetry_data{"SrcLastFile"} = $src_diffs_hash{"diff_last_file_name"};
			$telemetry_data{"SrcLF_ModTime"} = $src_diffs_hash{"diff_last_file_timestamp"};
			$telemetry_data{"SrcNumOfTagFile"} = $src_diffs_hash{"diff_tag_files_count"};
			$telemetry_data{"SrcFirstTagFileName"} = $src_diffs_hash{"diff_first_tag_file_name"};
			$telemetry_data{"SrcFT_ModTime"} = $src_diffs_hash{"diff_first_tag_file_timestamp"};
			$telemetry_data{"SrcLastTagFileName"} = $src_diffs_hash{"diff_last_tag_file_name"};
			$telemetry_data{"SrcLT_ModTime"} = $src_diffs_hash{"diff_last_tag_file_timestamp"};
		}
	}
	
	if(@target_folder_diffs)
	{
		my (%tgt_diffs_hash) = &diffsFolderProcess($ofile_dir, \@other_ifile_dirs, \@target_folder_diffs, 1);
		if(%tgt_diffs_hash)
		{
			$telemetry_data{"TgtFldSize"} = $tgt_diffs_hash{"total_pending_diffs"};
			$telemetry_data{"TgtNumOfFiles"} = $tgt_diffs_hash{"diff_files_count"};
			$telemetry_data{"TgtFirstFile"} = $tgt_diffs_hash{"diff_first_file_name"};
			$telemetry_data{"TgtFF_ModTime"} = $tgt_diffs_hash{"diff_first_file_timestamp"};
			$telemetry_data{"TgtLastFile"} = $tgt_diffs_hash{"diff_last_file_name"};
			$telemetry_data{"TgtLF_ModTime"} = $tgt_diffs_hash{"diff_last_file_timestamp"};
			$telemetry_data{"TgtNumOfTagFile"} = $tgt_diffs_hash{"diff_tag_files_count"};
			$telemetry_data{"TgtFirstTagFileName"} = $tgt_diffs_hash{"diff_first_tag_file_name"};
			$telemetry_data{"TgtFT_ModTime"} = $tgt_diffs_hash{"diff_first_tag_file_timestamp"};
			$telemetry_data{"TgtLastTagFileName"} = $tgt_diffs_hash{"diff_last_tag_file_name"};
			$telemetry_data{"TgtLT_ModTime"} = $tgt_diffs_hash{"diff_last_tag_file_timestamp"};
		}
	}
	
	$telemetry_json_str = Utilities::convertToJsonString(%telemetry_data);
	
	return $telemetry_json_str;
}


sub diffsFolderProcess
{
	my ($ofile_dir, $other_ifile_dirs, $src_tgt_folder_diffs, $isTgtDiffs) = @_;

	my $total_pending_diffs = 0;
	my $is_first_diff_file = 0;
	my $tag_file_count = 0;
	my %diff_folder_process_hash;
	$diff_folder_process_hash{"diff_files_count"} = scalar @{$src_tgt_folder_diffs};
	$diff_folder_process_hash{"diff_first_file_name"} = "";
	$diff_folder_process_hash{"diff_first_file_timestamp"} = 0;
	$diff_folder_process_hash{"diff_last_file_name"} = "";
	$diff_folder_process_hash{"diff_last_file_timestamp"} = 0;
	$diff_folder_process_hash{"total_pending_diffs"} = 0;
	$diff_folder_process_hash{"diff_first_tag_file_name"} = "";
	$diff_folder_process_hash{"diff_first_tag_file_timestamp"} = 0;
	$diff_folder_process_hash{"diff_last_tag_file_name"} = "";
	$diff_folder_process_hash{"diff_last_tag_file_timestamp"} = 0;
	$diff_folder_process_hash{"diff_tag_files_count"} = 0;
	
	foreach my $file (@{$src_tgt_folder_diffs}) 
    {
		chomp $file;
		my $file_alias = $file;
		if($isTgtDiffs)
		{
			$file = $ofile_dir . "/" . $file;
		}
		else
		{
			foreach my $ifile_dir (@{$other_ifile_dirs})
			{
				$file = $ifile_dir . "/" . $file_alias;
				if ((-e $file))
				{
					last;
				}
			}
		}
		
		# Skip directories and special files
		if (!(-f $file)) 
		{
			$logging_obj->log("DEBUG","diffsFolderProcess Skipping special file or directory: $file");
			next;
		}
		#4780
		if (-e $file) 
		{
			#Start - issue#2249
			# Is the oldest file creation time before the RPO threshold
			my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,
					$mtime,$ctime,$blksize,$blocks) ;
			if( $HAS_NET_IFCONFIG == 1 )
			{
				 use if Utilities::isWindows(), "Win32::UTCFileTime"  ;
				($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime, $mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat("$file");		
			}
			else
			{
				($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime, $mtime,$ctime,$blksize,$blocks) = stat("$file");
			}
			
			if($is_first_diff_file == 0)
			{
				$diff_folder_process_hash{"diff_first_file_name"} = $file_alias;
				$diff_folder_process_hash{"diff_first_file_timestamp"} = (defined $mtime && $mtime ne "") ? $mtime : 0;
			}
			$diff_folder_process_hash{"diff_last_file_name"} = $file_alias;
			$diff_folder_process_hash{"diff_last_file_timestamp"} = (defined $mtime && $mtime ne "") ? $mtime : 0;
			
			if($file_alias =~ /^completed_ediffcompleted_diff_tag_P/)
			{
				if($tag_file_count == 0)
				{
					$diff_folder_process_hash{"diff_first_tag_file_name"} = $file_alias;
					$diff_folder_process_hash{"diff_first_tag_file_timestamp"} = (defined $mtime && $mtime ne "") ? $mtime : 0;
				}
				$diff_folder_process_hash{"diff_last_tag_file_name"} = $file_alias;
				$diff_folder_process_hash{"diff_last_tag_file_timestamp"} = (defined $mtime && $mtime ne "") ? $mtime : 0;
				$tag_file_count++;
			}
			
			$total_pending_diffs += $size;
			$is_first_diff_file++;
		}
	}
	$diff_folder_process_hash{"diff_tag_files_count"} = $tag_file_count;
	$diff_folder_process_hash{"total_pending_diffs"} = $total_pending_diffs;
	return (%diff_folder_process_hash);
}

sub telemetry_log_monitor
{
	my (@telemetry_log_strs) = @_;
	
	my $telemetry_log_dir =  "$HOME_DIR/var/"."telemetry_logs";
	$telemetry_log_dir =  Utilities::makePath($telemetry_log_dir);
	
	my $telemetry_current_file = "$telemetry_log_dir/InMageTelemetryPSV2_current.json";
	$telemetry_current_file =  Utilities::makePath($telemetry_current_file);
	
	mkdir $telemetry_log_dir unless -d $telemetry_log_dir;
	
	if(Utilities::isWindows() == 0)
	{
		chmod (0777, $telemetry_log_dir);
	}
	
	if(-e $telemetry_current_file)
	{
		my @telemetry_completed_files = Utilities::readDirContentsInSortOrder($telemetry_log_dir, "^InMageTelemetryPSV2_completed_");
		my $tel_completed_files_count = scalar @telemetry_completed_files;
		
		if($tel_completed_files_count < 3)
		{
			my $telemetry_completed_file = "$telemetry_log_dir/InMageTelemetryPSV2_completed_".time().".json";
			$telemetry_completed_file =  Utilities::makePath($telemetry_completed_file);
			my $return_val = Utilities::move_file($telemetry_current_file, $telemetry_completed_file);
		}
		else
		{
			my $completed_last_file_name = $telemetry_completed_files[$#telemetry_completed_files];
			my $completed_last_file_timestamp = 0;
			my $size = 0;
			
			if( $HAS_NET_IFCONFIG == 1 )
			{
				 use if Utilities::isWindows(), "Win32::UTCFileTime";
				$size = (Win32::UTCFileTime::file_alt_stat("$telemetry_current_file"))[7];
				$completed_last_file_timestamp = (Win32::UTCFileTime::file_alt_stat("$telemetry_log_dir".'/'."$completed_last_file_name"))[9];
			}
			else
			{
				$size = (stat("$telemetry_current_file"))[7];
				$completed_last_file_timestamp = (stat("$telemetry_log_dir".'/'."$completed_last_file_name"))[9];
			}
			
			#Trunkate the telemetry_current_file if size is greater than or equals to 10MB
			if(($size / (1024 * 1024)) >= 10)
			{
				my %telemetry_data = ();
				$telemetry_data{"TruncStart"} = (defined $completed_last_file_timestamp && $completed_last_file_timestamp ne "") ? $completed_last_file_timestamp : 0;
				$telemetry_data{"TruncEnd"} = time();
				$telemetry_data{"MessageType"} = 2;
				my $telemetry_json_str = Utilities::convertToJsonString(%telemetry_data);
				push(@telemetry_log_strs, $telemetry_json_str);
				unlink ($telemetry_current_file);
				
				$logging_obj->log("EXCEPTION","telemetry_log_monitor log file size has been exceeded the limit of 10MB. We are going to truncate telemetry_current_file: $telemetry_current_file");
			}
		}
	}
	
	open(FH, ">>$telemetry_current_file");
	foreach my $telemetry_log_str (@telemetry_log_strs)
	{
		if($telemetry_log_str ne "") 
		{
			print FH $telemetry_log_str;
		}
	}
	close(FH);
}


sub get_system_reboot_time
{
	my $system_reboot_time = "";
	
	eval
	{
		my $system_reboot_time_output = `wmic path  win32_operatingsystem get LastBootUpTime`;
		$system_reboot_time_output  =~ s/^\s+|\s+$//g;
		if( $system_reboot_time_output ne "" )
		{
			my @system_boot_time_arr = split(/\s+/, $system_reboot_time_output);
			if(@system_boot_time_arr && $system_boot_time_arr[1] ne "" )
			{
				$system_reboot_time = $system_boot_time_arr[1];
				if( !$system_reboot_time )
				{
					$logging_obj->log("EXCEPTION","get_system_reboot_time:: system_reboot_time is NULL. system_reboot_time_output: $system_reboot_time_output");
					$system_reboot_time = "";
				}
			}
		}
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","Exception occurred in get_system_reboot_time: ".$@);
	}
	
	return $system_reboot_time;
}


sub monitor_ps_reboot_time
{
	my ($host_data) = @_;

	my $ps_host_id = $host_data->{$MY_AMETHYST_GUID}->{'id'};
	my $ps_host_name = $host_data->{$MY_AMETHYST_GUID}->{'name'};
	my $ps_ip_address = $host_data->{$MY_AMETHYST_GUID}->{'ipaddress'};
	my $system_reboot_time = &get_system_reboot_time();
	$system_reboot_time =~ s/^\s+|\s+$//g;
	#$logging_obj->log("EXCEPTION", "monitor_ps_reboot_time ps_host_id: $ps_host_id, ps_host_name: $ps_host_name, ps_ip_address: $ps_ip_address, system_reboot_time: $system_reboot_time");
	if($system_reboot_time ne '')
	{
		if(Utilities::isWindows())
		{
			my $monitor_dir = $HOME_DIR."\\monitor";

			if(!-d $monitor_dir)
			{
				mkdir $monitor_dir;
			}

			if(-d $monitor_dir)
			{
				my $sys_reboot_time_file = $monitor_dir."\\".'ps_reboot_time_persist.txt';
				#$logging_obj->log("EXCEPTION", "monitor_ps_reboot_time sys_reboot_time_file: $sys_reboot_time_file");
				if(-e $sys_reboot_time_file)
				{
					open(FH,"<$sys_reboot_time_file");
					my @file_contents = <FH>;
					close (FH);
					
					my $sys_reboot_time_in_file = $file_contents[0];
					$sys_reboot_time_in_file =~ s/^\s+|\s+$//g;
					#$logging_obj->log("EXCEPTION", "monitor_ps_reboot_time sys_reboot_time_file exists sys_reboot_time_in_file: $sys_reboot_time_in_file");
					if($sys_reboot_time_in_file ne '' && $system_reboot_time ne $sys_reboot_time_in_file)
					{
						my $err_message = "Server has come up at ".$system_reboot_time." after a reboot or shutdown on ".$ps_host_name."(".$ps_ip_address.")"; 
						my $err_summary = "Process Server Has Logged Alerts";
						$err_message =~ s/<br>/ /g;
						$err_message =~ s/\n/ /g;

						my %err_placeholders;

						$err_placeholders{"HostName"} = $ps_host_name;
						$err_placeholders{"IPAddress"} = $ps_ip_address;
						$err_placeholders{"SystemBootTime"} = $system_reboot_time;

						my $err_temp_id = 'PS_ERROR';
						my $error_code = 'EC0114';

						my $args;

						$args->{"id"} = $ps_host_id;
						$args->{"error_id"} = $ps_host_id;
						$args->{"summary"} = $err_summary;
						$args->{"err_msg"} = $err_message;
						$args->{"err_temp_id"} = $err_temp_id;
						$args->{"err_code"} = $error_code;
						$args->{"err_placeholders"} = \%err_placeholders;

						my $alert_info = serialize($args);

						my $arguments;
						$arguments->{"err_temp_id"} = $err_temp_id;
						$arguments->{"err_msg"} = $err_message;
						my $trap_info = serialize($arguments);

						&tmanager::add_error_message($alert_info, $trap_info);
						
						open(FH,">$sys_reboot_time_file");
						print FH $system_reboot_time;
						close(FH);
						
						$logging_obj->log("EXCEPTION", "Process Server has been rebooted ".$err_message);
					}
				}
				else
				{
					open(FH,">$sys_reboot_time_file");
					print FH $system_reboot_time;
					close(FH);
					#$logging_obj->log("EXCEPTION", "monitor_ps_reboot_time sys_reboot_time_file: $sys_reboot_time_file does not exist");
				}
			}
			else
			{
				$logging_obj->log("EXCEPTION", "monitor_ps_reboot_time monitor_dir: $monitor_dir direcotry does not exist");
			}
		}
	}
}

sub get_ps_version_details
{
	my $ps_version = "";
	my @version = &tmanager::get_inmage_version_array();
	
	if($version[ 0 ] ne '')
	{
		$ps_version = $version[ 0 ];
		$ps_version  =~ s/^\s+|\s+$//g;
	}
	
	return $ps_version;
}

sub monitor_ps_upgrade
{
	my ($host_data) = @_;

	my $ps_version = '';
	my $ps_host_id = $host_data->{$MY_AMETHYST_GUID}->{'id'};
	my $ps_host_name = $host_data->{$MY_AMETHYST_GUID}->{'name'};
	my $ps_ip_address = $host_data->{$MY_AMETHYST_GUID}->{'ipaddress'};
	$ps_version = &get_ps_version_details();
	$ps_version =~ s/^\s+|\s+$//g;
	#$logging_obj->log("EXCEPTION", "monitor_ps_upgrade ps_host_id: $ps_host_id, ps_host_name: $ps_host_name, ps_ip_address: $ps_ip_address, ps_version: $ps_version");
	if($ps_version ne '')
	{
		if(Utilities::isWindows())
		{
			my $monitor_dir = $HOME_DIR."\\monitor";

			if(!-d $monitor_dir)
			{
				mkdir $monitor_dir;
			}

			if(-d $monitor_dir)
			{
				my $ps_version_info_file = $monitor_dir."\\".'ps_version_info_persist.txt';
				#$logging_obj->log("EXCEPTION", "monitor_ps_upgrade ps_version_info_file: $ps_version_info_file");
				if(-e $ps_version_info_file)
				{
					open(FH,"<$ps_version_info_file");
					my @file_contents = <FH>;
					close (FH);
					
					my $ps_upgrade_info_in_file = $file_contents[0];
					$ps_upgrade_info_in_file =~ s/^\s+|\s+$//g;
					#$logging_obj->log("EXCEPTION", "monitor_ps_upgrade ps_version_info_file exists ps_upgrade_info_in_file: $ps_upgrade_info_in_file");
					if($ps_upgrade_info_in_file ne '' && $ps_version ne $ps_upgrade_info_in_file)
					{
						my $err_message = "Process Server is upgraded to ".$ps_version." on ".$ps_host_name."(".$ps_ip_address.")"; 
						my $err_summary = "Process Server Has Logged Alerts";
						$err_message =~ s/<br>/ /g;
						$err_message =~ s/\n/ /g;

						my %err_placeholders;

						$err_placeholders{"HostName"} = $ps_host_name;
						$err_placeholders{"IPAddress"} = $ps_ip_address;
						$err_placeholders{"PSVersion"} = $ps_version;

						my $err_temp_id = 'PS_ERROR';
						my $error_code = 'EC0115';

						my $args;

						$args->{"id"} = $ps_host_id;
						$args->{"error_id"} = $ps_host_id;
						$args->{"summary"} = $err_summary;
						$args->{"err_msg"} = $err_message;
						$args->{"err_temp_id"} = $err_temp_id;
						$args->{"err_code"} = $error_code;
						$args->{"err_placeholders"} = \%err_placeholders;

						my $alert_info = serialize($args);

						my $arguments;
						$arguments->{"err_temp_id"} = $err_temp_id;
						$arguments->{"err_msg"} = $err_message;
						my $trap_info = serialize($arguments);

						&tmanager::add_error_message($alert_info, $trap_info);
						
						open(FH,">$ps_version_info_file");
						print FH $ps_version;
						close(FH);
						
						$logging_obj->log("EXCEPTION", "Process Server was upgraded ".$err_message);
					}
				}
				else
				{
					open(FH,">$ps_version_info_file");
					print FH $ps_version;
					close(FH);
					#$logging_obj->log("EXCEPTION", "monitor_ps_upgrade ps_version_info_file: $ps_version_info_file does not exist");
				}
			}
			else
			{
				$logging_obj->log("EXCEPTION", "monitor_ps_upgrade monitor_dir: $monitor_dir direcotry does not exist");
			}
		}
	}
}

1;