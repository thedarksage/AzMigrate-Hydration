package tmanager;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use DBI();
use POSIX;
use POSIX qw(ceil floor);
use POSIX qw(strftime);
use Sys::Hostname;
use Logfile::Rotate;
use Messenger;
use Config;
use Utilities;
use TimeShotManager;
use ProcessServer::Register;
use ProcessServer::Unregister;
use ProcessServer::Trending::NetworkTrends;
use ConfigServer::Trending::HealthReport;
use ConfigServer::Application;
use ConfigServer::HighAvailability::Register;
use Data::Dumper;
use Time::HiRes qw(gettimeofday);
use ProcessServer::Trending::BandwidthTrends;
use Common::Log;
use Common::Constants;
use Common::DB::Connection;
use Fabric::Constants;
use PHP::Serialization qw(serialize unserialize);
use XML::Simple;
use File::Path;
use File::Find;
use RequestHTTP;
use Time::Local;
use File::Glob ':glob';
use Win32::Service;

# Global constants
my $cs_installation_path = Utilities::get_cs_installation_path();
my $msg = "";
our $AMETHYST_CONF_FILE               = "etc/amethyst.conf";
our $INMAGE_VERSION_FILE              = "etc/version";
our $PATCH_VERSION_FILE               = "patch.log";
our $HAS_NET_IFCONFIG = eval { require Win32::UTCFileTime ; 1; }; 
our $BASE_DIR;
if(Utilities::isWindows())
{
	$BASE_DIR = "$cs_installation_path\\home\\svsystems";
}
else
{
	$BASE_DIR = "/home/svsystems";
}
my $DF_CMD                      = "df";

our $CS_HOST_NAME					= '';
our $PS_HOST_NAME					= '';

# Database constants
my $HOSTS_TBL                        = "hosts";
my $HOSTS_id                         = "id";
my $HOSTS_name                       = "name";
my $HOSTS_sentinelEnabled            = "sentinelEnabled";
my $HOSTS_outpostAgentEnabled        = "outpostAgentEnabled";
my $HOSTS_PROFILER = '5C1DAEF0-9386-44a5-B92A-31FE2C79236A';

my $LV_TBL                           = "logicalVolumes";
my $LV_hostId                        = "hostId";
my $LV_deviceName                    = "deviceName";
my $LV_capacity                      = "capacity";
my $LV_lun                           = "lun";
my $LV_lastSentinelChange            = "lastSentinelChange";
my $LV_lastOutpostAgentChange        = "lastOutpostAgentChange";
my $LV_farLineProtected              = "farLineProtected";
my $LV_returnHomeEnabled             = "returnHomeEnabled";
my $LV_dpsId                         = "dpsId";
my $LV_doResync                      = "doResync";
my $LV_startingPhysicalOffset        = "startingPhysicalOffset";
my $LV_tmId                          = "tmId";

my $SRC_DST_LV_TBL                   ="srcLogicalVolumeDestLogicalVolume";
my $SRC_DST_LV_sourceHostId          = "sourceHostId";
my $SRC_DST_LV_sourceDeviceName      = "sourceDeviceName";
my $SRC_DST_LV_destinationHostId     = "destinationHostId";
my $SRC_DST_LV_destinationDeviceName = "destinationDeviceName";
my $SRC_DST_LV_replicationStatus     = "replicationStatus";
my $SRC_DST_LV_fullSyncStartTime     = "fullSyncStartTime";
my $SRC_DST_LV_fullSyncEndTime       = "fullSyncEndTime";
my $SRC_DST_LV_fullSyncBytesSend     = "fullSyncBytesSend";
my $SRC_DST_LV_differentialStartTime = "differentialStartTime";
my $SRC_DST_LV_differentialEndTime   = "differentialEndTime";
my $SRC_DST_LV_phy_lunid             = "Phy_Lunid";
my $SRC_DST_LV_lun_state	         = "lun_state";
my $SRC_DST_LV_offset				 = "offset";
my $SRC_DST_LV_executionState	     = "executionState"; 
my $SRC_DST_LV_jobId                 = "jobId";
my $DPS_LV_TBL                       = "dpsLogicalVolumes";
my $DPS_deviceName                   = "deviceName";
my $DPS_id                           = "id";
my $DPS_capacity                     = "capacity";


my $SANPHYSICAL_appliance_lun_name	 = "appliance_lun_name";
my $SANPHYSICAL_appliance_lun_no	 = "appliance_lun_no";
my $SANPHYSICAL_capacity			 = "capacity";
my $SANPHYSICAL_lun_state			 = "lun_state";
my $SANPHYSICAL_Rpairdeleted		 = "Rpairdeleted";
my $SANPHYSICAL_uid				     = "uid";

my $SANAPPLIANCE_uid			     = "uid";

my $ITL_NEXUS_initnodewwn			 = "initnodewwn";
my $ITL_NEXUS_initportwwn			 = "initportwwn";
my $ITL_NEXUS_tgtnodewwn			 = "tgtnodewwn";
my $ITL_NEXUS_tgtportwwn			 = "tgtportwwn";
my $ITL_NEXUS_id					 = "id";
my $ITL_NEXUS_lun_no				 = "lun_no";
my $ITL_NEXUS_uid					 = "uid";
my $ITL_NEXUS_vendor_name			 = "vendor_name";



my $CXINITIATOR_TBL					 = "cxIntiatorPorts";
my $CXINITIATOR_host_id				 = "host_id";
my $CXINITIATOR_cx_nodewwn			 = "cx_nodewwn";
my $CXINITIATOR_cx_portwwn			 = "cx_portwwn";
my $CXINITIATOR_setFlag				 = "setFlag";
my $IN_PENDING						 = 1;
my $IN_PROGRESS						 = 2;
my $SUCCESS							 = 3;
my $FAILURE							 = 4;


# Double defines, Need to consolidate all of them under TimeShotManager.pm instead
my $DO_IS_MODE_IS                     = "is"; # Initial Sync
my $DO_IS_MODE_RE                     = "re"; # Resync

my $coalesceReportError = 0; # Used to report compress error reporting
my $lastUpdateTime = 0;      # Used to guarantee timely status checking for traps and the like

our $dbh;                     # Database handle global to this package, should hide this behind a method instead
our %states;                  # List of cyclical events and their functions
our %freq;                    # "                               " frequencies
our %phase;                   # "                               " phases
my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
my $SOURCE_FAILURE       = $AMETHYST_VARS->{"SOURCE_FAILURE"};
my $HA_TRY_COUNT_LIMIT  = $AMETHYST_VARS->{'MAX_RETRY_LIMIT'};
my $ACTUAL_INSTALLATION_DIR  = $AMETHYST_VARS->{'ACTUAL_INSTALLATION_DIR'};
my $MYSQL_PATH;
our $HA_TRY_COUNT = 0;

my $tmanager_enable = "/home/svsystems/var/tmanager_enable.log";
my $usage_logfile = "/home/svsystems/var/tmanager_event.log";

my $AZURE_PENDING_UPLOAD_NON_RECOVERABLE = "AzurePendingUploadNonRecoverable";
my $AZURE_PENDING_UPLOAD_RECOVERABLE = "AzurePendingUploadRecoverable";
my $AZURE_UPLOADED = "AzureUploaded";

my $CS_LOGS_PATH = Common::Constants::CS_LOGS_PATH;
my $CS_TELEMETRY_PATH = Common::Constants::CS_TELEMETRY_PATH;
my $APPINSIGHTS_WRITE_DIR_COUNT_LIMIT = Common::Constants::APPINSIGHTS_WRITE_DIR_COUNT_LIMIT;
my $TELEMETRY_FOLDER_COUNT_FILE = Common::Constants::TELEMETRY_FOLDER_COUNT_FILE;
my $TELEMETRY_FOLDER_COUNT_FILE_PATH = Common::Constants::TELEMETRY_FOLDER_COUNT_FILE_PATH;
my $WIN_SLASH = Common::Constants::SLASH;
my $MDS_AND_TELEMETRY = Common::Constants::LOG_TO_MDS_AND_TELEMETRY;
my $TELEMETRY = Common::Constants::LOG_TO_TELEMETRY;
my $MAX_RETRY = Common::Constants::MAX_RETRY;

if (Utilities::isWindows())
{
	$tmanager_enable = "$cs_installation_path\\home\\svsystems\\var\\tmanager_enable.log";
	$usage_logfile = "$cs_installation_path\\home\\svsystems\\var\\tmanager_event.log";
	$MYSQL_PATH = $AMETHYST_VARS->{'MYSQL_PATH'};
}

my $hosts_type ="type";
my $logging_obj = new Common::Log();
my $version_vars = {};
my $BASE_DIR = $tmanager'BASE_DIR;
my $conn;
my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
my $HTTP = ($AMETHYST_VARS->{'PS_CS_HTTP'}) ? $AMETHYST_VARS->{'PS_CS_HTTP'} : $AMETHYST_VARS->{'CS_HTTP'};
my $home_dir_size = 0;
my $folder_to_query = '';

my %service_status_code = (
	  Stopped => 1,
	  StartPending => 2,
	  StopPending => 3,
	  Running => 4,
	  ResumePending => 5,
	  PausePending => 6,
	  Paused => 7
	);

my $powershell_path = "C:\\Windows\\SysNative\\WindowsPowerShell\\v1.0\\powershell.exe";
my $cs_key_file_path = "C:\\ProgramData\\Microsoft Azure Site Recovery\\private\\";
my $cs_crt_file_path = "C:\\ProgramData\\Microsoft Azure Site Recovery\\certs\\";
my $cs_fingerprint_file_path = "C:\\ProgramData\\Microsoft Azure Site Recovery\\fingerprints\\";
my $gen_cert_bin_path = "$cs_installation_path\\home\\svsystems\\bin\\";

sub tmanager_event_log
{
	my ($event_name,$event_start_time) = @_;
	my $event_end_time = getCurrentTime();
	my $cs_ip = $AMETHYST_VARS->{'CS_IP'};
	if (-e "$tmanager_enable")
	{
		open(USAGE_FH,">>$usage_logfile");		
		my $diff_time = $event_end_time - $event_start_time;
		my $entry   = $cs_ip.",".$event_name.",".$event_start_time.",".$event_end_time.",".$diff_time."\n" ; # No newline added at end
		print USAGE_FH $entry;
		close(USAGE_FH);
	}
}

sub getCurrentTime
{
	my ($epochseconds, $microseconds) = gettimeofday;
	my $time_interval =   $epochseconds.".".$microseconds;
	return $time_interval;
}
##
# Subroutine to initialize NEXT TMID once.
##
sub init_next_tmid ($)
{
   #my ($dbh) = @_;
   my ($conn) = @_;
   eval
   {
        # dbh ping to check whether valid db resource or not and check the mysql availlable
         if ($conn->sql_ping_test())
         {
           
			  my $conn = new Common::DB::Connection();
		
         }


	  my $next_tmid;
	  $next_tmid = $conn->sql_get_value('transbandSettings', 'ValueData',"ValueName='NEXTTMID'");

	  if ($next_tmid)
	  {
	    return $next_tmid;
	  }
	  else
	  {
	    $next_tmid = 1;

        my $sql1 = "INSERT INTO transbandSettings ".
	                          "(ValueName, ValueData) ".
        	                  "VALUES ('NEXTTMID', '$next_tmid')";
	 
		my $res = $conn->sql_query($sql1);
		$conn->sql_finish($res);
	    return $next_tmid;
	  }  
      };
      if ($@)
      {
	   
	   $logging_obj->log("DEBUG","init_next_tmid:$@");
	   return;
      }
}

##
#
# Returns a hash of hostids of the hosts on which the sentinel
# is enabled
#
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: None
#
##
sub getSentinelHosts() 
{

  my ($conn)=@_;
  my %sentinelHosts;
  my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};

  
  eval
  {
	   my $sql = "SELECT
						a.id
					FROM
						hosts a,
						srcLogicalVolumeDestLogicalVolume b
					WHERE
						 a.id=b.sourceHostId AND
						 a.sentinelEnabled = 1 AND
						 b.processServerId = '$host_guid'";


		my $sentinel_hosts = $conn->sql_get_hash($sql,'id','id');	
		%sentinelHosts = %$sentinel_hosts;
   };
   if ($@)
   {
      
	  $logging_obj->log("EXCEPTION","getSentinelHosts:".$@);
      return;
   }   
   return %sentinelHosts;
}
##
#
# Returns an hash of pairs specific to current process server if specified or else all the pairs respective to current CS
#
# Notes: This sub routine is used get all the pair info 
#
# Inputs: connection object, type(whether calling host required only ps pairs or all pairs)
#
##
sub getPairDetails() 
{
  my ($conn,$type)=@_;
  my %pair_array;
  my %host_array;
  my @host_ids;
  my @plan_ids;
  my %entry;
  my %plan_array;
  my @source_array;
  my $host_type = $$type;
  my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
  my $cond = "";
  
  eval
  {
	if($host_type eq "ps")
	{
		$cond = "WHERE processServerId = '$host_guid'";		
	}
	$PS_HOST_NAME = TimeShotManager::getPSHostName($host_guid,$conn);
	$CS_HOST_NAME = TimeShotManager::getCSHostName($conn);	
	
	my $sql = "SELECT
					sourceHostId,
					sourceDeviceName,
					destinationHostId,
					destinationDeviceName,
					pairId,
					isQuasiflag,
					throttleresync,
					maxResyncFilesThreshold,
					resyncStartTagtime,
					resyncEndTagtime,
					Phy_Lunid,
					planId,
					processServerId
				FROM
					srcLogicalVolumeDestLogicalVolume $cond";
	
	$logging_obj->log("DEBUG","pair details :: SQL :: $sql\n");	
	my $result = $conn->sql_exec($sql);

	foreach my $ref (@{$result})
	{
		push(@host_ids,$ref->{$SRC_DST_LV_sourceHostId});
		push(@host_ids,$ref->{$SRC_DST_LV_destinationHostId});
		push(@plan_ids,$ref->{'planId'});
		
		$pair_array{ $ref->{'pairId'}} { $ref->{$SRC_DST_LV_sourceHostId}.'!&*!'.$ref->{$SRC_DST_LV_sourceDeviceName} }  = $ref->{$SRC_DST_LV_destinationHostId}."!&*!".$ref->{$SRC_DST_LV_destinationDeviceName}."!&*!".$ref->{'isQuasiflag'}."!&*!".$ref->{'maxResyncFilesThreshold'}."!&*!".$ref->{'throttleresync'}."!&*!".$ref->{'resyncStartTagtime'}."!&*!".$ref->{'resyncEndTagtime'}."!&*!".$ref->{'Phy_Lunid'}."!&*!".$ref->{'planId'}; 			
	}

	if(@host_ids)
	{
		@host_ids = grep { ! $entry{ $_ }++ } @host_ids;	
		my $host_id = join("','",@host_ids);
			
		my $sql_host = "SELECT id,osFlag FROM hosts where id in ('$host_id')";
		my $result1 = $conn->sql_get_hash($sql_host,'id','osFlag');	
		%host_array = %$result1;
	}
	
	if(@plan_ids)
	{
		@plan_ids = grep { ! $entry{ $_ }++ } @plan_ids;	
		my $plan_ids = join("','",@plan_ids);
		my $sql_plan = "SELECT
						solutionType,
						planId,
						planName
					FROM
						applicationPlan 
					WHERE
						planId IN ('$plan_ids')";
		
		my $result_plan = $conn->sql_exec($sql_plan);

		foreach my $row_plan (@{$result_plan})
		{
			my $pair_type = '';
			if ($row_plan->{'solutionType'} == 'ARRAY')
			{
				$pair_type = 3;
			}
			$plan_array{$row_plan->{'planId'}} = $row_plan->{'planName'}.'!&*!'.$pair_type;			
		}	
	}
	
	foreach my $pair_id (keys %pair_array)
	{
		foreach my $source_host (keys %{$pair_array{$pair_id}})
		{			 
			@source_array = split(/\!\&\*\!/,$source_host);	
			my @dest_info = split(/\!\&\*\!/,($pair_array{$pair_id}{$source_host}));
			my $dest_flag = '';			
			my $source_flag = '';
			my $pair_type = '';
			my $plan_name = '';
			$source_flag = $host_array{$source_array[0]};
			$dest_flag =  $host_array{$dest_info[0]};
			
			my @pair_info = split(/\!\&\*\!/,$plan_array{$dest_info[8]});
			if($dest_info[7])
			{
				$pair_type = $pair_info[1];
			}
			$plan_name = $pair_info[0];
			$pair_array{$pair_id}{$source_host} = $dest_info[0].'!&*!'.$dest_info[1].'!&*!'.$dest_info[2].'!&*!'.$dest_info[3].'!&*!'.$dest_info[4].'!&*!'.$dest_info[5].'!&*!'.$dest_info[6].'!&*!'.$dest_info[7].'!&*!'.$dest_flag.'!&*!'.$source_flag.'!&*!'.$pair_type.'!&*!'.$plan_name;	
		}
	}	
   };

	if ($@)
	{      
	  $logging_obj->log("EXCEPTION","getPairDetails ::".$@);
	  return;
	}
	return %pair_array;
}
##
#
# Returns an array of protected devices
#
# Notes: This sub routine is used get all the protected source device info based on calling volsync.It returns 2 arrays
# %lv_hash::Contains all protected devices for which resync and diff sync both are required
# %resync_lv_hash::Contains all protected devices for which only resync is required
#
# Inputs: connection object, pair_info,is_fork(To know whether result need to be specific to calling volsync)
#
##
sub getProtectedDeviceInfo() 
{
	my ($conn,$pair_info,$tmid_str)= @_;
	my %pair_info = %{$pair_info};
	my %pair_data;
	my $i = 0;
	my (@source_host,@source_array,@source_device);
	foreach my $pair_id (keys %pair_info)
	{
		foreach my $source_host (keys %{$pair_info{$pair_id}})
		{			
			@source_array = split(/\!\&\*\!/,$source_host);
			$source_array[1] = TimeShotManager::formatDeviceName($source_array[1]);
			push(@source_host,$source_array[0]);
			push(@source_device , $source_array[1]);
		}
	}
	my %host_entry;
	my %device_entry;

	@source_host = grep { ! $host_entry{ $_ }++ } @source_host;	
	@source_device = grep { ! $device_entry{ $_ }++ } @source_device;

	my $source_host_id = join("','",@source_host);
	my $source_device_name = join("','",@source_device);  
	my $condition = "";
	if($tmid_str)
	{
		$condition = " AND tmId IN ('$tmid_str')"
	}
	my $lv_sql = "SELECT
					hostId,
					deviceName,
					doResync,
					tmId
				FROM
					logicalVolumes
				WHERE
					deviceName in ('$source_device_name')
				AND
					hostId in ('$source_host_id')
				AND 
					farLineProtected = 1 $condition";

	my $i=0;
	my %lv_hash;
	my %resync_lv_hash;
	my $result = $conn->sql_exec($lv_sql);

	foreach my $ref (@{$result})
	{
		if($ref->{'doResync'} == 1)
		{
			$resync_lv_hash{$ref->{'hostId'}.'!&*!'.$ref->{'deviceName'}}{'tmId'} = $ref->{'tmId'};
		}
		$lv_hash{$ref->{'hostId'}.'!&*!'.$ref->{'deviceName'}}{'tmId'} = $ref->{'tmId'};
	}
	
	return (\%lv_hash,\%resync_lv_hash);
}
##
#
# Returns a hash of hostids of the hosts 
#
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: None
#
##
sub getHosts
{   
    my ($conn) = @_;
	my %Hosts;
    $logging_obj->log("DEBUG","getHosts");
	my $sql = "SELECT 
                    id,
                    type
                FROM
                    $HOSTS_TBL
                WHERE
                    id != '$HOSTS_PROFILER'";

    $logging_obj->log("DEBUG","getHosts::$sql");
	my $result = $conn->sql_exec($sql);

	foreach my $ref (@{$result})
	{
		# Put the id's in a hash
        $Hosts{ $ref->{$HOSTS_id} } = $ref->{$HOSTS_id};
        if ($ref->{$hosts_type} eq 'Switch')
        {
            TimeShotManager::checkSwitchThresold($conn,$ref->{$HOSTS_id});
        }       
    }
    return %Hosts;
}

sub getHostsInfo 
{
	my ($conn) = @_;

	my %Hosts;
	my $sql = "SELECT id, name FROM $HOSTS_TBL WHERE id != '$HOSTS_PROFILER'";
	my $sth = $conn->sql_query($sql);
  
	while (my $ref = $conn->sql_fetchrow_hash_ref($sth))
    	{
        	$Hosts{ $ref->{$HOSTS_id} } = $ref->{$HOSTS_name};
	}
	$conn->sql_finish($sth);
  
	#
	# Include the Switch agents, as there is no licensing model for them
	#
	my $sql = "SELECT id,name  FROM $HOSTS_TBL where type = 'Switch'";
	my $sth = $conn->sql_query($sql);
	while (my $ref = $conn->sql_fetchrow_hash_ref($sth))
	{
        	$Hosts{ $ref->{$HOSTS_id} } = $ref->{$HOSTS_name};
	}
	$conn->sql_finish($sth);
  
	return %Hosts;
}

##
#
# Returns  hostid of the host 
#
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: None
#
##
sub getHostIdForHostName {
	my ($conn,$hostName) = @_;
	my $hostId;
	$hostId = $conn->sql_get_value('hosts', 'id', "name = '$hostName'");
	return $hostId;
}

##
#
# Returns log rotaions enabled or not 
#
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: None
#
##
sub isLogRotaionEnabled 
{
	my ($conn) = @_;
	my $enabledFalg;
	$enabledFalg = $conn->sql_get_value('transbandSettings', 'ValueData',"ValueName='isEnabledLogRotation'");
	return $enabledFalg;

}

##
#
# Returns an array of associated cluster host ids for the passed in host id and device 
# the returned array will not include the passed in source host id and will 
# be empty if passed in hostid and device are not part of a cluster
#
# Notes: Uses the database handle ($dbh) global to this package
#       Establishing connection is the responsibility of the caller
#
# Inputs: 
#    source host id
#    source device name
#
##
sub getAssociatedClusterIds
{
	my ($id, $device,$conn) = @_;
 	my @clusterIds;

	$device =  TimeShotManager::formatDeviceName($device);

	 my $sql = "select c1.hostId from clusters c1, clusters c2 ".
		  "where c2.hostId = '$id' and c1.hostId != c2.hostId ".
		  "and c1.clusterGroupId = c2.clusterGroupId ".
		  "and  c1.deviceName = '$device' ".
		  "group by c1.hostId, c1.deviceName; ";
	
	my $result = $conn->sql_exec($sql);

	foreach my $ref (@{$result})
	{
		push(@clusterIds, $ref->{hostId});
	}

	 return @clusterIds;
}
##
#
# Returns an hash of cluster volumes with cluster id as key
#
# Inputs: connection object
#
##
sub getClusterHash
{
	my ($conn) = @_;
	my @cluster_array;
	my %cluster_hash;
	my $sql = "select hostId , deviceName , clusterId from clusters order by clusterId";
	
	my $tmp = "";
	my $result = $conn->sql_exec($sql);

	foreach my $ref (@{$result})
	{
		if($ref->{clusterId} ne $tmp)
		{
			@cluster_array = ();
		}
		push(@cluster_array,$ref->{hostId}.'!&*!'.$ref->{deviceName});
		$cluster_hash{$ref->{clusterId}} = [@cluster_array];
		$tmp = $ref->{clusterId};
	}	
	return %cluster_hash;
}
##
# Returns a hash of farLineProtected volumes for a given host
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: hostid
##
sub getFarlineProtectedVolumes {
  my ($hostid,$conn) = @_;
  my %fpVolumes;
  
  my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
				
  my $sql = "SELECT
				a.deviceName
			FROM
				$LV_TBL a,
				$SRC_DST_LV_TBL b
			WHERE
				a.hostid = b.sourceHostId AND
				a.deviceName = b.sourceDeviceName AND
				a.hostid = '$hostid' AND
				a.farLineProtected = 1 AND
				a.dpsId != '' AND
				b.processServerId = '$host_guid'";
  

	my $result = $conn->sql_get_hash($sql,'deviceName','deviceName');	
	%fpVolumes = %$result;

	return %fpVolumes;
}

##
# Returns a hash of  volumes that have resync pending 
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: hostid
##
sub getResyncVolumes 
{
	my ($hostid,$conn) = @_;
	my %resyncVolumes;

	my $sql = "SELECT $LV_deviceName from $LV_TBL WHERE hostid='$hostid'
				AND $LV_doResync = 1 AND $LV_dpsId !='' ";
				
	my $result = $conn->sql_get_hash($sql,'deviceName','deviceName');	
	%resyncVolumes = %$result;			

	return %resyncVolumes;
}

##
# Returns a scalar dps volume associated with the source host:vol pair
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: hostid
#
# Should be moved into TimeShotManager.pm
#
##
sub getVolumesTmId {
  my ($shost, $svol,$conn) = @_;
  my ($tmId);

  # Get the dpsId
  $svol =  TimeShotManager::formatDeviceName($svol);
  $tmId = $conn->sql_get_value('logicalVolumes', 'tmId', "hostid='$shost' AND  deviceName='$svol'");

  return $tmId;
}

##
# Returns a scalar dps volume associated with the source host:vol pair
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: hostid
#
# Should be moved into TimeShotManager.pm
#
##
sub getDPSVolume {
	my ($shost, $svol,$conn) = @_;
	my ($dpsId, $dpsDeviceName);
	$svol =  TimeShotManager::formatDeviceName($svol);

	# Get the dpsId
	$dpsId = $conn->sql_get_value('logicalVolumes', 'dpsId', "hostid='$shost' AND  deviceName='$svol'");

	# Get the deviceName for dpsId
	$dpsDeviceName = $conn->sql_get_value('dpsLogicalVolumes', 'deviceName', "id='$dpsId';");

	return $dpsDeviceName;
}


##
# Returns a hash of the destination volumes and destination hosts
# for an input (near or far line) protected volume. The hash is
# indexed by the destination volume, the value is the destination
# host.
#
# Input: Sentinel Host
#        Protected volume
#
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
##
sub getDestinationHostsAndVolumes 
{
	my ($hostid, $srcVolume,$conn) = @_;
	my %destVolumes;
	my $DB_TYPE = $AMETHYST_VARS->{"DB_TYPE"};
	# Retrieve data from the table.
	if ($DB_TYPE eq "mysql") 
	{
		$srcVolume =~ s/\\/\\\\/g ; # Double backslashes (for mysql);
	}
	$srcVolume =  TimeShotManager::formatDeviceName($srcVolume);

	my $sql = "SELECT $SRC_DST_LV_destinationDeviceName , $SRC_DST_LV_destinationHostId from $SRC_DST_LV_TBL WHERE  $SRC_DST_LV_sourceDeviceName='$srcVolume' AND $SRC_DST_LV_sourceHostId='$hostid'";

	my $result = $conn->sql_exec($sql);

	foreach my $ref (@{$result})
	{
		#$destVolumes{ $ref->{$SRC_DST_LV_destinationDeviceName}.",".$ref->{$SRC_DST_LV_destinationHostId} } = $ref->{$SRC_DST_LV_destinationHostId}; # OLD CODE
		$destVolumes{ $ref->{$SRC_DST_LV_destinationDeviceName}.'$'.$ref->{$SRC_DST_LV_destinationHostId} } = $ref->{$SRC_DST_LV_destinationHostId};  # FOR SOLARIS
	}
	
	return %destVolumes;
}


##
# Returns a hash of the destination volumes and destination hosts
# for an input (near or far line) protected volume. The hash is
# indexed by the destination volume, the value is the destination
# host.
#
# Input: Sentinel Host
#        Protected volume
#
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
##
sub getDestinationHostsAndVolumesPS 
{
	my ($hostid, $srcVolume,$conn) = @_;
	my %destVolumes;
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
	my $DB_TYPE = $AMETHYST_VARS->{"DB_TYPE"};
	# Retrieve data from the table.
	if ($DB_TYPE eq "mysql") {
		$srcVolume =~ s/\\/\\\\/g ; # Double backslashes (for mysql);
	}
	$srcVolume =  TimeShotManager::formatDeviceName($srcVolume);

  my $sql = "SELECT $SRC_DST_LV_destinationDeviceName , $SRC_DST_LV_destinationHostId from $SRC_DST_LV_TBL WHERE  $SRC_DST_LV_sourceDeviceName='$srcVolume' AND $SRC_DST_LV_sourceHostId='$hostid' AND processServerId='$host_guid'";

  my $result = $conn->sql_exec($sql);
  foreach my $ref (@{$result})
  {
	  #$destVolumes{ $ref->{$SRC_DST_LV_destinationDeviceName}.",".$ref->{$SRC_DST_LV_destinationHostId} } = $ref->{$SRC_DST_LV_destinationHostId}; # OLD CODE
      $destVolumes{ $ref->{$SRC_DST_LV_destinationDeviceName}.'$'.$ref->{$SRC_DST_LV_destinationHostId} } = $ref->{$SRC_DST_LV_destinationHostId};  # FOR SOLARIS
  }
  return %destVolumes;
}

##
#
# Returns a hash of hostids of the hosts on which returnhome
# operation has to be performed
#
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: None
#
##
sub getReturnHomeHosts() {
  my %returnhomeHosts;

  my $sql = "SELECT id FROM $HOSTS_TBL where $HOSTS_outpostAgentEnabled = 1;";

  my $sth = $dbh->prepare($sql);
  $sth->execute();
  # Put the id's in a hash
  while (my $ref = $sth->fetchrow_hashref()) {
    $returnhomeHosts{ $ref->{$HOSTS_id} } = $ref->{$HOSTS_id};
  }
  $sth->finish();
  return %returnhomeHosts;
}


##
#
# Returns a hash of hostids of the hosts on which resync
# operation has to be performed
#
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: None
#
##
sub getResyncHosts() {
  my ($conn) = @_;
  #return &getReturnHomeHosts();
  return &getSentinelHosts($conn);
}

##
# Make sure the read-only QLogic 2200 driver
# is loaded
##
sub monitorQlogicDriver {
  # @FIX: Dumb way of doing it, will suffice for now
  `insmod qla2200 2> /dev/null`;
}

##
# Cleanup before starting a new run
##
sub cleanupPreviousLogs {
  my ($basedir) = @_;
  if ($basedir ne "" && $basedir ne "/") {
      #`find $basedir -name is.log | xargs rm -f`;
      #`find $basedir -name is.pipeline | xargs rm -f`;
      #`find $basedir -name ecb.log | xargs rm -f`;
      #`find $basedir -name ecb.pipeline | xargs rm -f`;
      #`find $basedir -name dpsglobal.log | xargs rm -f`;
      #`find $basedir -name tmanager.log | xargs rm -f`;
      #
      # Bug fix 4590 - Replace the use of find with native perl api
      #
      my @files = Utilities::read_directory_contents($basedir, '(/.*?log/) || (/.*?pipeline/)');
     foreach my $file (@files)
     {
	if (($file eq "is.log") || 
	    ($file eq "is.pipeline") || 
	    ($file eq "ecb.log") ||
	    ($file eq "ecb.pipeline") ||
	    ($file eq "dpsglobal.log") ||
	    ($file eq "tmanager.log") 
           )
	{
	   unlink ($file);
	}
     }
  }
  else {
    print "Refusing to cleanup logfiles for basedir: $basedir as a precaution\n";
	$logging_obj->log("EXCEPTION","Refusing to cleanup logfiles for basedir: $basedir as a precaution");
  }
}

##
# Cleanup partial Sentinel uploads: i.e old pre_completed files
##
sub cleanupPartialHostUploads {
  my ($basedir) = @_;

  my $OLDER_THAN = '+0'; # Cleanup files that are more than a day old

  $basedir = Utilities::makePath($basedir);

  if ($basedir ne "" && $basedir ne "/") 
  {
     #
     # Bug Fix 4590 - Replaced the use of find with native perl API's
     #	
     my @files = Utilities::read_directory_contents($basedir, '(/pre_.*?\.dat/) || (/temp_.*?\.dat/)');
     
     foreach my $file (@files)
     {
	 #
	 # Get the last modified time. Delete the file 
	 # if it is a day old.
	 # 
         my $days_old = ((time - +(stat($file))[9]) / 86400);
         if (($days_old >= 1) && ($days_old < 2))
	 {
              unlink ($file);
         }
     }
  }
}

##
#
#
#
##
sub synchronize
{
	my ($msg, $TM_ID,$data_hash,$DB_DOWN) = @_;
	my $dpso;
	my $DPSO_logfile                = "DPSO_logfile";

	
	select(STDERR); $| = 1;	  # make unbuffered
	select(STDOUT); $| = 1;	  # make unbuffered

	# List for holding pending Initial Sync DPSO's
	my %pendingInitSyncDpsoList;

	# List for holding pending Resync DPSO's
	my %pendingResyncDpsoList;
	
	# Below hash (pair_array)represents pair hash which contains all the pair info.Here the key is pair id and value is sourcehost,sourcedevicename to 		desthost,destdevicename
	#{
    # 	'1673770695' => {
	#						'8A327988-F14B-684D-8CB736FF49D56E25!&*!G:\\mnt6' => '394203C6-D532-3248-B2FC51F024F25DC7!&*!F',
	#						'9945E04F-A530-4945-A3F08AB89EF570A4!&*!G:\\mnt6' => '394203C6-D532-3248-B2FC51F024F25DC7!&*!F'
	#					}
	#	'901606529' => 	{
	#						'394203C6-D532-3248-B2FC51F024F25DC7!&*!E=>394203C6-D532-3248-B2FC51F024F25DC7!&*!I',
	#					}
	#	'630621603' => 	{
	#						'394203C6-D532-3248-B2FC51F024F25DC7!&*!E=>394203C6-D532-3248-B2FC51F024F25DC7!&*!G:\\mnt1',
	#					}						 
    #};
	#
	
	if( TimeShotManager::shouldQuit( $BASE_DIR ) )
	{
		$logging_obj->log("DEBUG","Exit from tmanager synchronize shouldQuit as shutdown file exists");
		exit 1;
	}

	#Check if tmansvc is stopped
	if (Utilities::isWindows())
	{
		unless(-e "$cs_installation_path\\home\\svsystems\\var\\tmansvc.pid")
		{
			$logging_obj->log("DEBUG","Exit from tmanager synchronize as  ".$cs_installation_path."\home\svsystems\var\tmansvc.pid file exists");
			exit(0);
		}
	}

	if(scalar(keys %{$data_hash->{'pair_data'}}))
	{

		#my %diff_device_hash = %$diff_device_hash;#This array contains all the protected source volumes info with respective to calling volsync child
		#my %resync_device_hash = %$resync_device_hash;#This array contains all the source volumes info which are in resync with respect to calling volsync child
		#Both %diff_device_hash,%resync_device_hash are in the below format
		#$VAR1 = {
		#      '17D3BF7A-4EEF-0846-BC402F4A3BE18C1B!&*!G:\\Mount 1' => {
		#                                                      'tmId' => '9'
		#                                                    },
		#      '236A3E34-FDCE-1B41-8D9C7189E3BDE4D9!&*!G:\\Mount 1' => {
		#                                                      'tmId' => '9'
		#                                                    }'
		#		'394203C6-D532-3248-B2FC51F024F25DC7!&*!M' => {
		#												  'tmId' => '8'
		#                                                    }
		#   	};
		# This %cluster_hash contains all the cluster volumes info related to all the clusters of current CS.
		# To differentiate each cluster volumes , the hash contains cluster id as key and respective node hostid and volumes  as values
		#$VAR1 = {
		#      '39f6c3dd-0975-4167-a948-9d653c320381' => [
		#                                               	'9945E04F-A530-4945-A3F08AB89EF570A4!&*!F',
		#                                                	'9945E04F-A530-4945-A3F08AB89EF570A4!&*!L',
		#                                                	'9945E04F-A530-4945-A3F08AB89EF570A4!&*!J:\\mnt5',
		#												 	'8A327988-F14B-684D-8CB736FF49D56E25!&*!F',
		#                                                	'8A327988-F14B-684D-8CB736FF49D56E25!&*!L',
		#                                                	'8A327988-F14B-684D-8CB736FF49D56E25!&*!J:\\mnt5'
		#												  ]
		#   	 }
		my @clusterids;
		my @resyncClusterIds;
		# Below code is to get all the differential sync DPO's and Resync DPO's.
		# To do that we are looping all the #pair_array based on each pair id and searching for the protected source info using the protected device
		# array(diff_device_hash)
		# If source info exists in pair_array then we will get the corresponding destination info and creates the TimeShotManager object
		# Also process cluster hash based on source host id to get all the remaining node hostid's .
		my $cluster_data = 	$data_hash->{'cluster_data'};
		my $one_to_many_targets = $data_hash->{'one_to_many_targets'};
		my $cluster_hash;
		
		foreach my $clusterKey (keys %{$cluster_data}) 
		{
			my @cluster_array = split(/\!\!/,$clusterKey);
			push(@{$cluster_hash->{$cluster_array[0]}},$cluster_data->{$clusterKey}->{'cluster'});
		}		
		foreach my $pairid (keys %{$data_hash->{'pair_data'}})
		{
			my $pair_data = $data_hash->{'pair_data'}->{$pairid};
			if($pair_data->{'executionState'} != 2 && $pair_data->{'executionState'} != 3)
			{
				my $pair_source_key = $pair_data->{'sourceHostId'}."!!".$pair_data->{'sourceDeviceName'};
				my $lv_data = $data_hash->{'lv_data'}->{$pair_source_key};
				my $tmid = $lv_data->{'tmId'};				
				my $farLineProtected = $lv_data->{'farLineProtected'};
				$logging_obj->log("DEBUG","in synchronize farLineProtected :: $farLineProtected \n tmid :: $tmid \n TM_ID :: $TM_ID \n lv_data ::".Dumper($lv_data)."\n");
				if($tmid == $TM_ID )#To check whether calling volsync is associated with any of the replication pairs.
				{	
					if($lv_data &&  $farLineProtected)
					{
						my $doResync = $lv_data->{'doResync'};
						my $key = $pair_data->{'sourceHostId'}."!!".$pair_data->{'sourceDeviceName'}."!!".$pair_data->{'destinationHostId'}."!!".$pair_data->{'destinationDeviceName'};
						$logging_obj->log("DEBUG","in synchronize diff key :: $key \n");
						my $flag = 0;
						if (!defined $pendingInitSyncDpsoList{$key}) 
						{
							my @cluster_id;
							foreach my $clusterId (keys %{$cluster_hash}) 
							{		
								@clusterids = ();								
								foreach my $cluster_data (@{$cluster_hash->{$clusterId}})
								{
									my @cluster_array = split(/\!\!/,$cluster_data);								
									push(@clusterids,$cluster_array[0]);								
								}
								my %entry;
								@clusterids = grep { ! $entry{ $_ }++ } @clusterids;				
								
								if( grep {$_ eq $pair_data->{'sourceHostId'}.'!!'.$pair_data->{'sourceDeviceName'}} @{$cluster_hash->{$clusterId}})								
								{
									@cluster_id = (grep  $_ ne $pair_data->{'sourceHostId'},@clusterids ) ;
								}
							}							
							$logging_obj->log("DEBUG","in synchronize clusterids ".Dumper(\@cluster_id)."\n");
							$dpso = TimeShotManager::new($TimeShotManager::DPO_TYPE_IS, $BASE_DIR, $pair_data->{'sourceHostId'}, $pair_data->{'sourceDeviceName'}, $pair_data->{'destinationHostId'}, $pair_data->{'destinationDeviceName'}, $msg, \@cluster_id,$pair_data->{'compressionEnable'},$pair_data->{'executionState'},$pair_data->{'Phy_Lunid'},$pair_data->{'oneToManySource'},$pair_data->{'lunState'},$pair_data->{'profilingMode'},$pair_data->{'rpoSLAThreshold'},$one_to_many_targets,$pair_data->{'jobId'},$pair_data->{'replicationCleanupOptions'},$pair_data->{'restartResyncCounter'},$pair_data->{'resyncStartTime'},$pair_data->{'resyncEndTime'},$pair_data->{'replication_status'},$data_hash->{'host_data'},$TM_ID,$DB_DOWN);		
							if ($dpso->isInitialized()) 
							{
								$pendingInitSyncDpsoList{$key} = $dpso;
							}						
						}				
						if($doResync && !$DB_DOWN)
						{
							
							$logging_obj->log("DEBUG","in synchronize resync key :: $key \n");		
							my $check=0;
							if (!defined $pendingResyncDpsoList{$key}) 
							{
								my @resync_cluster_id;					
								foreach my $resyncClusterId (keys %{$cluster_hash})
								{									
									@resyncClusterIds = ();							
									foreach my $resync_cluster_data (@{$cluster_hash->{$resyncClusterId}})
									{
										my @resync_cluster_array = split(/\!\!/,$resync_cluster_data);								
										push(@resyncClusterIds,$resync_cluster_array[0]);
									}
									my %resync_entry;
									@resyncClusterIds = grep { ! $resync_entry{ $_ }++ } @resyncClusterIds;				
									
									if( grep {$_ eq $pair_data->{'sourceHostId'}.'!!'.$pair_data->{'sourceDeviceName'}} @{$cluster_hash->{$resyncClusterId}})								
									{
										@resync_cluster_id = (grep  $_ ne $pair_data->{'sourceHostId'},@resyncClusterIds ) ;
									}								
									
								}
								
								# Create a DPSO
								$logging_obj->log("DEBUG","in synchronize resync block clusterids ".Dumper(\@resync_cluster_id)."\n");
								$dpso = TimeShotManager::new($TimeShotManager::DPO_TYPE_IS, $BASE_DIR, $pair_data->{'sourceHostId'}, $pair_data->{'sourceDeviceName'}, $pair_data->{'destinationHostId'}, $pair_data->{'destinationDeviceName'}, $msg, 
								\@resync_cluster_id,$pair_data->{'compressionEnable'},$pair_data->{'executionState'},$pair_data->{'Phy_Lunid'},$pair_data->{'oneToManySource'},$pair_data->{'lunState'},$pair_data->{'profilingMode'},$pair_data->{'rpoSLAThreshold'},$one_to_many_targets,$pair_data->{'jobId'},$pair_data->{'replicationCleanupOptions'},$pair_data->{'restartResyncCounter'},$pair_data->{'resyncStartTime'},$pair_data->{'resyncEndTime'},$pair_data->{'replication_status'},$data_hash->{'host_data'},$TM_ID,$DB_DOWN,$pairid);
								if ($dpso->isInitialized()) 
								{
									$pendingResyncDpsoList{$key} = $dpso;
								}
							}
						}
						
					}
					else
					{
						# Early exit if we have no pairs to process  
						$logging_obj->log("DEBUG","Exit from tmanager synchronize as pairCount is ZERO for the calling volsync id $TM_ID");
						if( TimeShotManager::sleepAndShouldQuit( $BASE_DIR, 15 ) )
						{
							$logging_obj->log("DEBUG","Exit from tmanager synchronize shouldQuit as shutdown file exists");
							exit 1;
						}
						return 0;
					}
				}
			}
		}
	}	
	else
	{
		$logging_obj->log("DEBUG","Exit from tmanager synchronize as there are no pairs configured with current process server");
		return 0;
	}

	# Now do the real initial sync work on the pending objects as necessary
	foreach my $key (keys %pendingInitSyncDpsoList) 
	{
		$dpso = $pendingInitSyncDpsoList{$key};
		
		eval
		{
			# Start Initial Sync if it has not already completed
			#if ($dpso->isFirstInitialSync()) {
			if ($dpso->doExpandChanges != 0) 
			{
				if ($dpso->isResyncNeeded()) 
				{
					 $logging_obj->log("EXCEPTION","TM($TM_ID) : Error while trying to process changed files before resync finished");
				} 
				else
				{
					 $logging_obj->log("EXCEPTION","TM($TM_ID) : Error while trying to process changed files");
				}
			}
		};
		if ($@) 
		{		   
			$logging_obj->log("EXCEPTION","TM($TM_ID) : Exception occured in doExpandChanges call".$@);
		}

		# Delete the processed object
		delete $pendingInitSyncDpsoList{$key};
	}  
	# Now do the real resync work on the pending objects as necessary
	foreach my $key (keys %pendingResyncDpsoList) 
	{
		$dpso = $pendingResyncDpsoList{$key};
		
		eval
		{
			if ($dpso->isResyncNeeded()) 
			{ 
				# We want to gunzip the initial sync files that
				# the cinitialsync.exe generates from the host and rename them to
				# the prefix the resync agent expects.
				my $rc = $dpso->doResync();
				if ($rc != 0)
				{
					$logging_obj->log("EXCEPTION","TM($TM_ID) : Resync had issues");
				}
			}
		};
		if ($@) 
		{		   
			$logging_obj->log("EXCEPTION","TM($TM_ID) : Exception occured in doResync call".$@);
		}
		
		# Delete the processed object
		delete $pendingResyncDpsoList{$key};
	}
}

sub get_tmid_batch
{
	my($data_hash) = @_;
	my $monitor_pairs_child_count = $AMETHYST_VARS->{"MONITOR_PAIRS_CHILD_COUNT"};
	my $fork_monitor_pairs = $AMETHYST_VARS->{"FORK_MONITOR_PAIRS"};
	my $cnt = 1;
	my $batch;	
	my @tmIds;
	my %entry;
	my $lv_data = $data_hash->{'lv_data'};
	
	foreach my $key (keys %{$lv_data}) {
		push(@tmIds,$lv_data->{$key}->{'tmId'});		
	}
	
	@tmIds = grep { ! $entry{ $_ }++ } @tmIds;

	# Creating a monitor_pair forking batch for the number of forks
	my $batch_count = ceil(($#tmIds+1)/$monitor_pairs_child_count);
	
	if(@tmIds && $batch_count)
	{		
		for(my $i = 0; $i<= $#tmIds; $i++)
		{
			push(@{$batch->{$cnt}},$tmIds[$i]);
			
			if($fork_monitor_pairs)
			{
				if(($i-1)%$batch_count == 0) {$cnt++;}
			}
		}
	}
	return $batch;
}

##
#
#
#
##
sub monitor_pairs
{	
	my ($package, $extra_params) = @_;
	
	my $data_hash = $extra_params->{'data_hash'};
	$logging_obj->log("DEBUG","Entered monitor_pairs function");
	eval
	{
		my $batch = &get_tmid_batch($data_hash);	
		foreach my $batch_id (keys %$batch)
		{		
			my @tmids = @{$batch->{$batch_id}};
			$logging_obj->log("DEBUG","Entered monitor_pairs function for batch '".$batch."'".Dumper(\@tmids));
            my $tmid_str = join("','",@tmids);
			$logging_obj->log("DEBUG","Entered monitor_pairs function for '".$tmid_str."'");
			
            my @updated_pairs_ids;
			my ($keys,$cluster_ids) = &getPSPairDetails($data_hash,@tmids);
			my @keys = @$keys;
			my @cluster_id = @$cluster_ids;
			my $i = 0;
			my @batch;
			my $input;
			my %ir_dr_health_factor_data;
		
			if(scalar @keys)
            {		
                foreach my $key (@keys)
                {
					my @values = split(/\!\&\*\!/, $key);
					my $src_id = $values[0];
                    my $src_vol = $values[1];					
                    my $dest_id = $values[2];
                    my $dest_vol = $values[3];
                    my $pair_id = $values[4];
                    my $is_cluster = $values[5];
					
					if(!(grep  $_ eq $pair_id,@updated_pairs_ids ))
					{	
						# Now do the real monitor work on the pending objects as necessary
						my ($rpo_input, $irdr_file_stuck_health_factor) = TimeShotManager::updateReplicationStatusPage($src_vol,$src_id,$dest_id, $dest_vol,$is_cluster,\@cluster_id,$data_hash,$pair_id);
						my @input_query = @$rpo_input;
						if(scalar(@input_query) > 0)
						{
							push(@batch, @input_query);	
						}
						
						if(defined $irdr_file_stuck_health_factor && $irdr_file_stuck_health_factor != 0)
						{
							$ir_dr_health_factor_data{$src_id}->{$pair_id} = $src_vol."!!".$dest_id."!!".$dest_vol."!!".$irdr_file_stuck_health_factor;
						}
					}					
                }
				
				foreach my $keys (@batch)
				{
					foreach my $key (keys %{$keys})
					{
						$input->{$i} = $keys->{$key};
						$i++;
					}
				}
				
				if($input ne "") {
					&update_cs_db("monitor_pairs", $input);
				}
            }
        }
		$logging_obj->log("DEBUG","End of monitor_pairs function");
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","monitor_pairs : ".$@);
    }
}

sub monitor_resync_throttle
{	
	my ($package, $extra_params) = @_;
	
	my $data_hash = $extra_params->{'data_hash'};
	$logging_obj->log("DEBUG","Entered monitor_resync_throttle function");
	eval
	{
		# PS Renew Certificate Invocation.
		my %application_args = ('conn'=>$conn, 'csData' => $data_hash);
		my $application = new ConfigServer::Application(%application_args);	
		$logging_obj->log("DEBUG","monitor_ps function".Dumper($data_hash));		
		$application->monitorPSRenewCertificatePolicies();
		
		$logging_obj->log("DEBUG","End of monitor_resync_throttle function");
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","monitor_resync_throttle : ".$@);
    }
}

sub getSentinelHostsAll
{
	my ($conn)=@_;
	my %sentinelHosts;

	my $sql = "SELECT id FROM $HOSTS_TBL where $HOSTS_sentinelEnabled = 1;";
	my $sentinel_hosts = $conn->sql_get_hash($sql,'id','id');
	%sentinelHosts = %$sentinel_hosts;

	return %sentinelHosts;
}

sub getFarlineProtectedVolumesAll
{
	my ($hostid,$conn) = @_;
	my %fpVolumes;

	my $sql = "SELECT $LV_deviceName from $LV_TBL WHERE hostid='$hostid'
				 AND $LV_farLineProtected = 1 AND $LV_dpsId !='' ";
	my $result = $conn->sql_get_hash($sql,'deviceName','deviceName');	
	%fpVolumes = %$result;				 

	return %fpVolumes;
}

sub accumulate_network_data
{
	my ($package) = @_;
	$logging_obj->log("DEBUG","Entered accumulate_network_data function\n"); 
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
	my $flag = 0;
		
	#
	# Get the device statistics
	#
	$logging_obj->log("DEBUG","Starting the Accumulation of network stats\n");
	eval 
	{
	   ProcessServer::Trending::NetworkTrends::getPsNetworkStats();
	   $logging_obj->log("DEBUG","End of accumulate_network_data function\n"); 
	};
	if($@) 
	{
		$logging_obj->log("EXCEPTION","Failed to start getPsNetworkStats at accumulate_network_data: $@\n");
	}
}


sub monitor_rrds
{	
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	
	$logging_obj->log("DEBUG","Entered monitor_rrds function\n");
	my $tmp_dbh;
	#
	# dbh ping to check whether valid db resource or not and check the mysql availlable
	#
	
	
	eval
	{	
		my $dbh = $conn->get_database_handle();
		
		# List for holding pending Initial Sync DPSO's
	    my %pendingMonitorDpsoList;
	    #my %diffProcessedIds;
        my ($keys_ref, $clusterids_ref) = &getKeysOfProtectedSourceDestHosts($conn,"all",0);
		my @keys = @$keys_ref;
		my @clusterids = @$clusterids_ref;
		
	    foreach my $key (@keys)
		{
		    my @values = split(/\!\&\*\!/, $key);
			my $host = $values[0];
			my $volume = $values[1];
			my $dinfo_id = $values[2];
			my $dinfo_volume = $values[3];
			#$logging_obj->log("EXCEPTION","Entered monitor_rrds function\n key = $key \n host = $host \n volume = $volume \n dpsvol = $dpsvol \n dinfo1 = $dinfo_id \n dinfo0 = $dinfo_volume");
			#print "Entered monitor_rrds function\n key =" .$key."\n host =".$host."\n volume =".$volume."\n dpsvol =" .$dpsvol."\n dinfo1 =" .$dinfo_id."\n dinfo0 =".$dinfo_volume;
			
			my $source_dir_path = $BASE_DIR."/".$host."/".$volume;
			my $os_flag = TimeShotManager::get_osFlag($conn,$host);
			$source_dir_path = Utilities::makePath($source_dir_path);
			#print "source_dir_path::$source_dir_path\n";		
			if (!-e $source_dir_path) 
			{ 
				my $mkpath_output = mkpath($source_dir_path, { mode => 0777 });
				if ($? != 0) 
				{
					$logging_obj->log("EXCEPTION","Could not create source directory $source_dir_path:monitor_rrds: $@");
					print "Could not create source directory $source_dir_path:monitor_rrds:".$@;
					return;
				}
				my $chmod_input_dir;		
				# Make sure the directory allows for deletion by the windows hosts
				if( Utilities::isWindows() == 0 )
				{
					my $guid_folder = Utilities::makePath($BASE_DIR."/".$host);
					$chmod_input_dir = `chmod -R 777 "$guid_folder"`;								
				}
			}
			
			
			my $dest_path = $BASE_DIR."/".$dinfo_id;
			my $os_flag = TimeShotManager::get_osFlag($conn,$dinfo_id);
			$dest_path = Utilities::makePath($dest_path);
			if (!-e $dest_path) 
			{ 
				my $mkpath_output = mkpath($dest_path, { mode => 0777 });
				if ($? != 0) 
				{
					$logging_obj->log("EXCEPTION","Could not create source directory $dest_path:monitor_rrds: $@");
					print "Could not create source directory $dest_path:monitor_rrds:".$@;
					return;
				}
				
				if( Utilities::isWindows() == 0 )
				{
					my $dest_guid_folder = Utilities::makePath($BASE_DIR."/".$dinfo_id);
					my $chmod_dest_dir = `chmod -R 777 "$dest_guid_folder"`;								
				}
			}



			if (!defined $pendingMonitorDpsoList{$key}) 
			{						
				
							
				foreach my $host_id (@clusterids) 
				{
					my $source_path = $BASE_DIR."/".$host_id."/".$volume;
					my $os_flag = TimeShotManager::get_osFlag($conn,$host_id);
					$source_path = Utilities::makePath($source_path);
					#print "source_path::$source_path\n";		
					if (!-e $source_path) 
					{ 
						my $mkpath_output = mkpath($source_path, { mode => 0777 });
						if ($? != 0) 
						{
							$logging_obj->log("EXCEPTION","Could not create source directory $source_path:monitor_rrds: $@");
							print "Could not create source directory $source_path:monitor_rrds:".$@;
							return;
							}
					}
					my $chmod_input_dir;		
					# Make sure the directory allows for deletion by the windows hosts
					if( Utilities::isWindows() == 0 )
					{
						my $guid_folder = Utilities::makePath($BASE_DIR."/".$host_id);
						$chmod_input_dir = `chmod -R 777 "$guid_folder"`;								
					}
							
				}
								
				my %report_args = ('conn'=>$conn,'dbh'=>$dbh, 'basedir'=>$BASE_DIR,'srchostid'=>$host, 'dsthostid'=>$dinfo_id, 'srcvol'=>$volume, 'dstvol'=>$dinfo_volume);
				my $report = new ConfigServer::Trending::HealthReport(%report_args);

				if((&isPrimaryActive($conn) != 0) || (&isStandAloneCX($conn) == 1))
				{
					$report->updatePerfCsTxt();
					if($AMETHYST_VARS->{'CUSTOM_BUILD_TYPE'} == 3){ next; }
					$report->updateRpoRrd();
					$report->updateRetentionRrd();
					$report->updateHealthRrd();
				}	
				$report->checkThrottle();
				$pendingMonitorDpsoList{$key} = 1;				
			}		  
	    }	 
	};
	if ($@)
	{
		
		$logging_obj->log("EXCEPTION","Failed to connect to DB:monitor_rrds: $@");
	}
}

sub register_ha
{ 
	my ($package, $extra_params) = @_;
    $logging_obj->log("DEBUG","Entered register_ha function\n"); 
	
    eval
    {
		if (Utilities::isLinux()) 
        { 
            if(-f("/etc/ha.d/haresources"))
            {
				$logging_obj->log("DEBUG","register_ha:: callg register_node\n");
                ConfigServer::HighAvailability::Register::register_node();
            }
		}
    };
    if ($@)
    {
        
        $logging_obj->log("EXCEPTION","ERROR in register_ha node : $@");
    }
}

sub monitor_ha
{
	my $INSTALLATION_DIR       = $AMETHYST_VARS->{"INSTALLATION_DIR"};
	my $logFile = Utilities::makePath($INSTALLATION_DIR."/var/ha_failover.log");
    
    $logging_obj->log("DEBUG","Entered monitor_ha function\n"); 
	eval
    {
        if (Utilities::isLinux()) 
        { 
			my $isHeartbeatRunning = TimeShotManager::isHeartbeatRunning();
			my $isHeartbeatActive = TimeShotManager::isHeartbeatActiveNode();
			if ($isHeartbeatRunning == 0 && $isHeartbeatActive == 0)
			{
				# print "Monitoring Heartbeat, Httpd and Mysqld\n"; 

				my $ha_http_conn_cnt = &TimeShotManager::monitorHttpd();
				&TimeShotManager::monitorMysqld();
				&TimeShotManager::monitorHeartbeat();

				# print "HA retry count value: $HA_TRY_COUNT\n";
				
				#14075 Fix
				#if($HA_TRY_COUNT >= $HA_TRY_COUNT_LIMIT || $ha_http_conn_cnt >= $HA_TRY_COUNT_LIMIT)
				if($HA_TRY_COUNT >= $HA_TRY_COUNT_LIMIT)
				{
					# print "HA retry count has exceeded thresold, hence setting it to stand-by mode\n";

					if (!open(FAILOVER_LOG, ">>$logFile")) 
					{
						print "Could not open $logFile for writing:$!\n";
						$logging_obj->log("DEBUG","Could not open $logFile for writing:$!");
					}
					else
					{
						if($HA_TRY_COUNT >= $HA_TRY_COUNT_LIMIT)
						{
							my $logMsg = "Problem with connecting to MySQL db. HA retry count ($HA_TRY_COUNT) has hit the threshold ($HA_TRY_COUNT_LIMIT). ";
							$logMsg .= "Failover is being induced by monitor_ha";
							print FAILOVER_LOG localtime()." :  $logMsg\n";
						}
						else
						{
							my $logMsg = "Problem with apache. HA retry count ($ha_http_conn_cnt) has hit the threshold ($HA_TRY_COUNT_LIMIT). ";
							$logMsg .= "Failover is being induced by monitor_ha";
							print FAILOVER_LOG localtime()." :  $logMsg\n";
						}
						close(FAILOVER_LOG);
					}

					$logging_obj->log("DEBUG","HA retry count has hit threshold, hence setting it to stand-by mode");
					#TimeShotManager::stopHeartbeatService; 
					$HA_TRY_COUNT = 0;
				}
			}
        }
    };
    if ($@)
    {
		$HA_TRY_COUNT ++;
        if( $HA_TRY_COUNT >= $HA_TRY_COUNT_LIMIT)
        {
            print "problem with apache \n";
			$logging_obj->log("DEBUG","problem with apache");
            if (!open(FAILOVER_LOG, ">>$logFile")) 
            {    
                print "Could not open $logFile for writing:$!\n";
            }
            else
            {
                my $logMsg = "Problem with apache. HA retry count ($HA_TRY_COUNT) has hit the threshold ($HA_TRY_COUNT_LIMIT). ";
                $logMsg .= "Failover is being induced by monitor_ha";
                print FAILOVER_LOG localtime()." :  $logMsg\n";
                close(FAILOVER_LOG);
            }
             #TimeShotManager::stopHeartbeatService;
			 $HA_TRY_COUNT = 0;
        }

        $logging_obj->log("EXCEPTION","Error in monitor_ha: $@");
        return;
    }
}

sub monitor_disks
{  
	my ($package, $extra_params) = @_;
	my $data_hash = $extra_params->{'data_hash'};
	
	my $cx_type = $AMETHYST_VARS->{'CX_TYPE'};
	$logging_obj->log("DEBUG","Entered monitor_disks function\n"); 

	eval
	{
		select(STDERR); $| = 1;               # make unbuffered
		select(STDOUT); $| = 1;               # make unbuffered
		if($cx_type == 2 || $cx_type == 3)
		{
			TimeShotManager::monitorDisks($data_hash);
		}
		$logging_obj->log("DEBUG","End of monitor_disks function\n"); 
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","monitor_disks: $@");
	}
}

sub get_ps_patch_version {
   
	my @ps_patch_details;
	my @patch_details_array;
	eval 
    {
    	open(F, "<$BASE_DIR/$PATCH_VERSION_FILE");
    };	
    if ($@)
    {
		$logging_obj->log("EXCEPTION","Couldn't open $INMAGE_VERSION_FILE: $@");
		return;
    }	
	foreach my $line (<F>) 
    {
      my @data = split(/,/,$line);
      my $ps_patch_version = $data[0];
	  my $patch_install_time = $data[1];
	  @ps_patch_details = {
								'ps_patch_version' => $ps_patch_version, 
								'patch_install_time' => $patch_install_time
						  };
	   push(@patch_details_array, @ps_patch_details);
	}
	close( F );
	
    return @patch_details_array;
}
sub get_inmage_version_array {
    eval 
    {
    	open(F, "<$BASE_DIR/$INMAGE_VERSION_FILE");
    };	
    if ($@)
    {
	
	$logging_obj->log("EXCEPTION","Couldn't open $INMAGE_VERSION_FILE: $@");
	return;
    }
	my @file_contents = <F>;
	close (F);
   
   #
   # Read all the config parameters in amethyst.conf and 
   # put them in a global hash.
   #
   foreach my $line (@file_contents)
   {
      chomp $line;
      if (($line ne "") && ($line !~ /^#/))
      {
          my @arr = split (/=/, $line);
          $arr[0] =~ s/\s+$//g;
		  $arr[1] =~ s/^\s+//g;
          $arr[1] =~ s/\"//g;
		  $arr[1] =~ s/\s+$//g;
		  #$logging_obj->log("EXCEPTION","Pushing $arr[1] as a value to $arr[0]\n");
          $version_vars->{$arr[0]} = $arr[1];
      }
   }	
   my $date = $version_vars->{"INSTALLATION_DATE"};
   open( F, "<$BASE_DIR/$INMAGE_VERSION_FILE" ) 
   or die "Couldn't open $INMAGE_VERSION_FILE: $!";
   $_ = <F>; chomp; my $long = $_;
   $_ = <F>; chomp; my $short = $_;
   return( $long, $short , $date);
}


sub get_DB_details_array {

  my ($DB_TYPE, $DB_NAME, $DB_HOST, $DB_USER, $DB_PASSWD);
  open DBCON, Utilities::makePath($BASE_DIR."/".$AMETHYST_CONF_FILE);
  while (<DBCON>)
  {
    if (/([\w]+)[\s]*=[\s]*"(.+)"/ && !(/^[\s]*#/))
    {
      if    ($1 eq "DB_TYPE")      { $DB_TYPE      = $2; }
      elsif ($1 eq "DB_NAME")      { $DB_NAME      = $2; }
      elsif ($1 eq "DB_HOST")      { $DB_HOST      = $2; }
      elsif ($1 eq "DB_USER")      { $DB_USER      = $2; }
      elsif ($1 eq "DB_PASSWD")    { $DB_PASSWD    = $2; }    
    }
  }
  close DBCON;
    return( $DB_TYPE, $DB_NAME, $DB_HOST, $DB_USER, $DB_PASSWD );
}

###
# Returns HA details
##
sub get_HA_details {
  my $cluster_ip = "";
  my $ha_resources_file = "/etc/ha.d/haresources";
  open (HA_RESOURCES, $ha_resources_file);
  while (<HA_RESOURCES>)
  {
    if (/^[a-zA-Z\-_0-9\.]+\s+([\d\.]+)\s+/)
    {
      $cluster_ip = $1;
    }
  }
  return $cluster_ip;
}

#
##

sub monitor_logs
{  
	my ($package, $extra_params) = @_;
	$logging_obj->log("DEBUG","Entered monitor_logs function\n");
	
	# Used to prune the oms stats file here, which now is being done in PS Monitor log analytics process.
	# While porting to RCM Prime remember to handle oms files one way or another.
	&monitor_cs_telemetry_folders();
}

sub inmage_rotate($@)
{
	my ($repeat,@logPath) = @_;
    my @inmage_logs;
	my $GZIP_PATH;
	if( Utilities::isWindows() )
	{
        $GZIP_PATH = 'gzip.exe';
	}
	else
	{
	    $GZIP_PATH = '/bin/gzip';
	}

    for (my $i; $i < @logPath; $i++)
    {
		#print "logpath: $logPath[$i], repeat:$repeat\n";
                
   		@inmage_logs =  <$logPath[$i]>;

   		 if ((my $count = @inmage_logs) == 0) {
    			 
    	 } 
   		 else 
   		 {
                
			if (-e $logPath[$i])
			{
				if (index($logPath[$i], "asrapi_reporting") != -1) 
				{
					$repeat = 15;
				}
				my $log = new Logfile::Rotate( File=>"$logPath[$i]", Count =>$repeat, Gzip =>$GZIP_PATH, Flock=>'no');
				#process log file   
				$log->rotate();
				undef $log;
			}
         }	
	}
}



#LOGROTATION TO UTILIZE SPACE ON CX
sub log_rotate
{
#Rotating the phpdebug,xferlog,wtmp,tls.log,boot.log,mysqld.log,rsyncd.log if Operating system is linux
if (Utilities::isLinux()) {

#check for varlogs.conf file, if not exists creates it.
unless(-e "/home/svsystems/bin/varlogs.conf")
{
open(VARFILE, '>/home/svsystems/bin/varlogs.conf');

#Rotating the xferlog,wtmp,tls.log,boot.log,mysqld.log,rsyncd.log using logrotate utility based on size

print VARFILE <<VARCONFFILE;
compress
"/var/log/xferlog" "/var/log/wtmp" "/var/log/tls.log" "/var/log/boot.log" "/var/log/mysqld.log" /var/log/rsyncd.log {
    missingok
    daily
    copytruncate
    rotate 2
    size 10M
}
VARCONFFILE
} #end of unless

#check for tmplogs.conf file, if not exists creates it.
unless(-e "/home/svsystems/bin/tmplogs.conf")
{
#Rotating the phpdebug using logrotate utility based on size
open(TMPFILE, '>/home/svsystems/bin/tmplogs.conf');
print TMPFILE <<TMPCONFFILE;
compress
/tmp/phpdebug.txt {
    missingok
    daily
    copytruncate
    rotate 2
    size 10M
}
TMPCONFFILE
} #end of unless

#Rotating xferlog,wtmp,tls.log,boot.log,mysqld.log,rsyncd.log if var is used beyond 80%
system("/usr/sbin/logrotate -s /home/svsystems/var/logrotate.status /home/svsystems/bin/varlogs.conf") if `df -k -v -P /var |grep -v Use`=~/(\d+)%/ and $1 > 80;

#Rotating phpdebug if it size is greater than 10MB
system("/usr/sbin/logrotate -s /home/svsystems/var/logrotate.status /home/svsystems/bin/tmplogs.conf");
}
}

#sub run_discovery
#{
#   my( $dbh, $msg ) = @_;
#   print ("running discovery \n");
#   my $discovery_cmd = Utilities::makePath("$BASE_DIR/bin/discovery");
#   #print "discovery command : $discovery_cmd \n";
#   my $ret =  `$discovery_cmd`;
#   print ("completed running discovery \n");
#}
sub run_discovery
{
    my( $dbh, $msg ) = @_;

    
}

#start of BSR's modifications
sub update_disk_space
{
  my ($dbh) = @_;
  my ($bytes_per_cluster, $total_clusters, $secots_per_cluster, $bytes_per_cluster, $free_clusters);
  if( Utilities::isWindows() )
  {
    open( DFINFO, "df C: |" ) ;
    my $dfinformation = readline DFINFO ;

    my @temp ;
    while( $dfinformation = readline( DFINFO ) )
    {
      $dfinformation = trim ( $dfinformation ) ;
      my ($temp2) = split( / /, $dfinformation ) ;
      push( @temp, $temp2 ) ;
    }
    $dfinformation = "@temp" ;
    ($secots_per_cluster, $bytes_per_cluster, $total_clusters, $free_clusters ) = split( / /, $dfinformation) ;
    $secots_per_cluster = convertstringtonumber($secots_per_cluster) ;
    $bytes_per_cluster = convertstringtonumber($bytes_per_cluster) ;

    my $total_space = convertstringtonumber($total_clusters) * $secots_per_cluster * $bytes_per_cluster ;
    my $free_space = convertstringtonumber($free_clusters) * $secots_per_cluster * $bytes_per_cluster  ;

    #print "\n Total disk space:". $total_space ;
    #print "\n Free disk space:". $free_space ;
    my $sql = "UPDATE transbandSettings SET ValueData=$free_space where ValueName='diskfreespace'" ;
    my $sth = $dbh->prepare($sql);
    $sth->execute();

    $sql = "UPDATE transbandSettings SET ValueData=$total_space where ValueName='disktotalspace'" ;
    $sth = $dbh->prepare($sql);
    $sth->execute();
    close( DFINFO ) ;
  }
  else
  {
    my ($total_space, $used_space, $free_space) = map {$_*=1024} (`df -k -P --sync /home/svsystems` =~ /(?<=\s)(\d+)(?=\s)/g); 

    my $sql = "UPDATE transbandSettings SET ValueData=$free_space where ValueName='diskfreespace' ;" ;
    my $sth = $dbh->prepare($sql);
    $sth->execute();
    my $sql = "UPDATE transbandSettings SET ValueData=$total_space where ValueName='disktotalspace' ;" ;
    my $sth = $dbh->prepare($sql);
    $sth->execute();    
  }
}

##
# Returns maxCompressed/maxUncompressed value.
#
# Notes: Uses the database handle ($dbh) global to this package
#       Establishing connection is the responsibility of the caller
#
# Inputs: 
#    source host id,SourceDeviceNAme
#   from -0 means from uncompression,1-means coompression
##

sub getMaxCompUnCompinDb
{
	my ($conn,$id,$vol,$from) = @_;
	my $value;
	
	if($from == 0)
	{
		$value = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'max(maxUnCompression)', "sourceHostId = '$id' and  sourceDeviceName = '$vol' ");
	}
	else
	{
		$value = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'max(maxCompression)', "sourceHostId = '$id' and  sourceDeviceName = '$vol' ");
	}

	return $value;
}

sub is_dev_clustered 
{

	my ($conn,$id,$vol) = @_;
	my $cnt = 0;

	$cnt = $conn->sql_get_value('clusters', 'count(*)', "hostId='$id' and  deviceName='$vol'");
 
	if ($cnt > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

sub updatecumulativeCompUncomp
{
   my ($conn,$id,$vol,$uncompressValue,$compressValue,$from,$forDate,$counter) =@_ ;
  my $serialized_comp_value;
  my $serialized_uncomp_value;
  my @arrayvalueComp;
  my @arrayvalueUnComp;
  my $compValue = 0;
  my $uncompValue = 0;
  my $destid ;
  my $srcid;
  my $destvol;
  my $unique_profileId;
  my $insertFlag = 0;
  my $newProfileId = 0;
  my $recpid = 0;
  my $forDate_db;
  my $isCluster = 0;
  my ($sth,$sth1,$sth2,$sth3,$sth4,$ref,$ref1,$ref2,$sql,$sql1,$sql2,$sql3,$sql4);


  $sql = "select count(*) as count from srcLogicalVolumeDestLogicalVolume src ".
		  "where sourceHostId = '$id' and  sourceDeviceName = '$vol' limit 1";

  $isCluster = &is_dev_clustered ($conn,$id,$vol);

  my $sth = $conn->sql_query($sql);
  my $ref = $conn->sql_fetchrow_hash_ref($sth);
	if ($ref->{count} > 0) 
	{

		$sql = "select profilingId,destinationHostId,destinationDeviceName from srcLogicalVolumeDestLogicalVolume src ".
		  "where sourceHostId = '$id' and  sourceDeviceName = '$vol' limit 1";

		my $sth = $conn->sql_query($sql);
        my $ref = $conn->sql_fetchrow_hash_ref($sth);
        $unique_profileId = $ref->{profilingId};
		$destid = $ref->{destinationHostId};
        $destvol = $ref->{destinationDeviceName};

		my $sql_fromupdate = "select fromUpdate from profilingDetails where profilingId = '$unique_profileId'";

		my $sth_fromupdate = $conn->sql_query($sql_fromupdate);
		my $ref_fromupdate = $conn->sql_fetchrow_hash_ref($sth_fromupdate);
        $conn->sql_finish($sth_fromupdate);
         # to handle the case where tmanagerd is restarted
		if($counter eq 0 && $sth_fromupdate->{fromUpdate} ne 1)
		{
	      my $sqlAllDelete = "delete from profilingDetails where profilingId = '$unique_profileId'";
          my $sthAllDelete = $conn->sql_query($sqlAllDelete);
		  $conn->sql_finish($sthAllDelete);
		}

	 if ($unique_profileId le 0 or $unique_profileId eq '')
	 {
		 $newProfileId = int(rand(time));
		 # print "Time: ".time." Random Number: ".$newProfileId . "\n";
		$insertFlag = 1;
	 }
	 else
	 {
		$newProfileId = $unique_profileId;
	 }

	 $conn->sql_finish($sth);

		$sql1 = "select id,cumulativeCompression,cumulativeUnCompression,forDate from profilingDetails where fordate = '".$forDate."'";

	     
           my $sth1 = $conn->sql_query($sql1);
		   
	 	
		
		my $ref1 = $conn->sql_fetchrow_hash_ref($sth1);
        $conn->sql_finish($sth1);
		$recpid = $ref1->{id};
		$compValue = $ref1->{cumulativeCompression};
		$uncompValue = $ref1->{cumulativeUnCompression};
		$forDate_db = $ref1->{forDate};

	if ($from eq 0) 
	{
		$serialized_comp_value = $compValue + $compressValue;
		$serialized_uncomp_value = $uncompValue + $uncompressValue;

	#print ("Hourly addition uncomp: ".$serialized_uncomp_value."\n");
	}
	else
	{

		if ($compressValue ne 0){
			$serialized_comp_value = $compressValue;
		}
		
		if ($uncompressValue ne 0){
			$serialized_uncomp_value = $uncompressValue;	
		}

		#if ($recpid){
		my $sqlDelRecord = "select count(*) NoOfDaysData from profilingDetails ".
		  "where profilingId = '$newProfileId' group by profilingId";
	
		#print $sqlDelRecord."\n";

	       
			my $sthRec = $conn->sql_query($sqlDelRecord);

		
         my $refRec = $conn->sql_fetchrow_hash_ref($sthRec);
		my $countDays = $refRec->{NoOfDaysData};
		#print "$countDays \n";

		my $sqlIdRecord = "select id,forDate from profilingDetails where profilingId = '$newProfileId' order by id asc limit 1";

	     
			my $sthIdRec = $conn->sql_query($sqlIdRecord);

	
        my $refIdRec = $conn->sql_fetchrow_hash_ref($sthIdRec);

		my $toDelId = $refIdRec->{id};
		my $firstDate = $refIdRec->{forDate};
			#print $toDelId;
		if ($countDays ge 7)
		{  #if all 7 days records there. So 7 rows.
			#$newProfileId = $unique_profileId;
			my $sqlDelRecord1 = "delete from profilingDetails where id = $toDelId";
		#	print "$sqlDelRecord1\n";
		     
                my $sthRec1 = $conn->sql_query($sqlDelRecord1);
		}
		else
		{
			my $datefirst = "select forDate from profilingDetails where profilingId = '$newProfileId' order by forDate asc limit 1";
			
            my $datefirstRec = $conn->sql_query($datefirst);
			
            my $refdatefirstRec = $conn->sql_fetchrow_hash_ref($datefirstRec);
			my $datefirstvalue = $refdatefirstRec->{forDate};
			my $datelast = "select forDate from profilingDetails where profilingId = '$newProfileId' order by forDate desc limit 1";
			
            my $datelastRec = $conn->sql_query($datelast);
			
			my $refdatelastRec = $conn->sql_fetchrow_hash_ref($datelastRec);
			my $datelastvalue = $refdatelastRec->{forDate};
			my $datediff = "select to_days('$datelastvalue') - to_days('$datefirstvalue') as diff";
		
            my $datediffRec = $conn->sql_query($datediff);
			
            my $refdatediffRec = $conn->sql_fetchrow_hash_ref($datediffRec);

			my $datediffvalue = $refdatediffRec->{diff};
			if($datediffvalue ge 7) {
			my $sqlDelRecord1 = "delete from profilingDetails where id = $toDelId";
		#	print "$sqlDelRecord1\n";
		     
				my $sthRec1 = $conn->sql_query($sqlDelRecord1);
			} 

		# If say 4 records are there from date 15 th to 22nd. That means some days no data change is there. check if date diff is greater or equal to 7. If true, than delete the first entry for the profile Id. If false, nothing.
		#	my $sqlLastRecord = "select id,forDate from profilingDetails where profilingId = $newProfileId order by id asc limit $countDays,1";
#
#	       	my $sthLastRec = $dbh->prepare($sqlLastRecord);
#      			$sthLastRec->execute();
#
#			my $refIdRec = $sthLastRec->fetchrow_hashref();	
#	
#			my $lastId = $refIdRec->{id};
#			my $lastDate = $refIdRec->{forDate};
#			my datediffValue = &calculateDateDiff($firstDate,$lastDate);
		}
		#if (!$recpid){
			$insertFlag = 1;
		#}
  	}

	#update the database with the new cumulative uncompression/compression string.


	if ($isCluster == 1){ # If cluster device


     #$sql2 = "select sourceHostId, sourceDeviceName from srcLogicalVolumeDestLogicalVolume src ".
	#  "where destinationHostId = '".$destid."' and destinationDeviceName = '".$destvol."' ";

	$vol = TimeShotManager::formatDeviceName($vol);
    $destvol = TimeShotManager::formatDeviceName($destvol);

	#$sql2 = "select sourceHostId, sourceDeviceName from srcLogicalVolumeDestLogicalVolume src, hosts h, clusters cus where sourceHostId = h.id and h.id = cus.hostId and  src.sourceDeviceName = cus.deviceName and src.destinationHostId = '$destid' and  src.destinationDeviceName = '$destvol' and cus.deviceName = '$vol'"; 
	
	$sql2 = "SELECT distinct(cs.hostId) as sourceHostId FROM clusters cs WHERE cs.clusterId = (SELECT distinct(clusterId) FROM   clusters WHERE hostId = '$id')";

	#print "$sql2 \n";
	#and profilingId = $unique_profileId";

     
	   $sth2 = $conn->sql_query($sql2);

		if ($insertFlag eq 1){
			$sql3 = "insert into profilingDetails (profilingId,cumulativeCompression,cumulativeUnCompression,forDate,fromUpdate) VALUES ('$newProfileId','$serialized_comp_value','$serialized_uncomp_value','".$forDate."',1)";
			$insertFlag = 0;
		}
		else
		{
			$sql3 = "update profilingDetails set cumulativeCompression = '".$serialized_comp_value."', cumulativeUnCompression = '".$serialized_uncomp_value."' ".
		  	",fromUpdate = 0 where  profilingId = '$unique_profileId' and fordate = '".$forDate_db."'";
		}

			# print $sql3."\n";
			
			$sth3 = $conn->sql_query($sql3);
			$conn->sql_finish($sth3);

		
        while ($ref2 = $conn->sql_fetchrow_hash_ref($sth2)){
		$srcid = $ref2->{sourceHostId};
       	#$srcvol = $ref2->{sourceDeviceName};

       		# Fix for 4817
			#$srcvol = TimeShotManager::formatDeviceName($srcvol);
       		$destvol = TimeShotManager::formatDeviceName($destvol);

			 $sql4 = "update srcLogicalVolumeDestLogicalVolume set profilingId = '$newProfileId' where sourceHostId = '".$srcid."' and  sourceDeviceName = '".$vol."'";

		#	print $sql4."\n";
			
			$sth4 = $conn->sql_query($sql4);
			$conn->sql_finish($sth4);

	#	print $sql;

		
		
	}
     	
		$conn->sql_finish($sth2);

  } else {

       if ($insertFlag eq 1){
			$sql3 = "insert into profilingDetails (profilingId,cumulativeCompression,cumulativeUnCompression,forDate,fromUpdate) VALUES ('$newProfileId','$serialized_comp_value','$serialized_uncomp_value','".$forDate."',1)";
			$insertFlag = 0;
		}
		else
		{
			$sql3 = "update profilingDetails set cumulativeCompression = '".$serialized_comp_value."', cumulativeUnCompression = '".$serialized_uncomp_value."' ".
		  	",fromUpdate = 0 where  profilingId = '$unique_profileId' and fordate = '".$forDate_db."'";
		}

			# print $sql3."\n";
	#		print "sql3".$sql3."\n";
			
			$sth3 = $conn->sql_query($sql3);
			$sql4 = "update srcLogicalVolumeDestLogicalVolume set profilingId = '$newProfileId' where sourceHostId = '".$id."' and  sourceDeviceName = '".$vol."'";
		    # print $sql4."\n";
	#	    print "sql4".$sql4."\n";
			
			$sth4 = $conn->sql_query($sql4);
			$conn->sql_finish($sth4);

			#	print $sql;
		
			$conn->sql_finish($sth3);

  }
#}
       # print $serialized_value ;

 }
	
}

sub updatecumulativeCompression
{
  my ($dbh,$id,$vol,$uncompressValue,$compressValue,$from,$forDate) =@_ ;
  my $serialized_comp_value;
  my $serialized_uncomp_value;
  my @arrayvalueComp;
  my @arrayvalueUnComp;
  my $compValue = 0;
  my $uncompValue = 0;
	 my $destid ;
	 my $destvol;
  my ($sth,$ref,$sql,$sql1,$ref1,$sth1,$sth2);

  my $random_number = int(rand(time));
  # print "Time: ".time." Random Number: ".$random_number . "\n";

      $sql = "select  destinationHostId,destinationDeviceName,cumulativeCompression, cumulativeUnCompression from srcLogicalVolumeDestLogicalVolume src ".
		  "where sourceHostId = '$id' and  sourceDeviceName = '$vol' limit 1";

	#print ($sql."\n");

      $sth = $dbh->prepare($sql);

	 $sth->execute();

	#while ($ref = $sth->fetchrow_hashref()){
	$ref = $sth->fetchrow_hashref();
	$sth->finish();
        $serialized_comp_value = $ref->{cumulativeCompression};
        $serialized_uncomp_value = $ref->{cumulativeUnCompression};
   	$destid = $ref->{destinationHostId};
       $destvol = $ref->{destinationDeviceName};
	if ($from eq 0) {
		if($serialized_comp_value ne 0) {
					@arrayvalueComp = split(/,/, $serialized_comp_value);
					#my $arraylength = scalar @arrayvalueComp;
					$compValue = shift(@arrayvalueComp);
				}
					#print ("From hourly comp before: ".$compValue." $compressValue\n");
					$compValue += $compressValue;
					##$compValue = $compressValue;
					#print ("From hourly comp after: ".$compValue." $compressValue\n");
					unshift(@arrayvalueComp, $compValue);	

	$serialized_comp_value = join(",", @arrayvalueComp);
	#print ("Hourly addition comp: ".$serialized_comp_value."\n");

	if($serialized_uncomp_value ne 0) {
					@arrayvalueUnComp = split(/,/, $serialized_uncomp_value);
					my $arraylength = scalar @arrayvalueUnComp;
					$uncompValue = shift(@arrayvalueUnComp);
				}
					#print ("From hourly uncomp before: ".$uncompValue."\n");	
					$uncompValue += $uncompressValue;
					##$uncompValue = $uncompressValue;
					#print ("From hourly uncomp after: ".$uncompValue."\n");			
					unshift(@arrayvalueUnComp, $uncompValue);

	$serialized_uncomp_value = join(",", @arrayvalueUnComp);
	#print ("Hourly addition uncomp: ".$serialized_uncomp_value."\n");
	}
	else
	{
		if($serialized_comp_value ne 0) {
					@arrayvalueComp = split(/,/, $serialized_comp_value);
					my $arraylength = scalar @arrayvalueComp;
					$compValue = shift(@arrayvalueComp);
					$compValue = $compressValue;
					unshift(@arrayvalueComp, $compValue);
					if($arraylength >= 7)
					{
						pop(@arrayvalueComp);
					}
				}
					#print ("From daily comp before: $compressValue\n");
					##unshift(@arrayvalueComp, $compressValue);	
					unshift(@arrayvalueComp, 0);		


	$serialized_comp_value = join(",", @arrayvalueComp);
      
	#print ("Daily value set to: $serialized_comp_value \n");
	#update the database with the new cumulative compression string.

		if($serialized_uncomp_value ne 0) {
					@arrayvalueUnComp = split(/,/, $serialized_uncomp_value);
					my $arraylength = scalar @arrayvalueUnComp;
					$compValue = shift(@arrayvalueUnComp);
					$uncompValue = $uncompressValue;
					unshift(@arrayvalueUnComp, $uncompValue);
					if($arraylength >= 7)
					{
						pop(@arrayvalueUnComp);
					}
				}
				#	print ("From daily comp before: $uncompressValue\n");			
				##	unshift(@arrayvalueUnComp, $uncompressValue);
					unshift(@arrayvalueUnComp, 0);

		$serialized_uncomp_value = join(",", @arrayvalueUnComp);
       	# print ("Daily value set to: $serialized_uncomp_value \n");

   }

	#update the database with the new cumulative uncompression/compression string.


     $sql1 = "select sourceHostId, sourceDeviceName from srcLogicalVolumeDestLogicalVolume src ".
	  "where destinationHostId = '$destid' and  destinationDeviceName = '$destvol' ";

       $sth2 = $dbh->prepare($sql1);

       $sth2->execute();
		while ($ref1 = $sth2->fetchrow_hashref()){
		my $srcid = $ref1->{sourceHostId};
       	my $srcvol = $ref1->{sourceDeviceName};
		$sql = "update srcLogicalVolumeDestLogicalVolume set cumulativeCompression = '$serialized_comp_value', cumulativeUnCompression = '$serialized_uncomp_value' ".
		  "where  (sourceHostId = '$srcid' and  sourceDeviceName = '$srcvol') or (destinationHostId = '$destid' and  destinationDeviceName = '$destvol')";

	#	print $sql;

		$sth1 = $dbh->prepare($sql);
		$sth1->execute();
		$sth1->finish();
	}
     	$sth2->finish();
#}
       # print $serialized_value ;
	
}

sub clear_cumulativeValues
{
	my ($dbh) = @_;
	my $sql = "update srcLogicalVolumeDestLogicalVolume set cumulativeCompression = '0', cumulativeUnCompression = '0' ";
     	my $sth = $dbh->prepare($sql);

	$sth->execute();
     	$sth->finish();
}

sub convertstringtonumber
{
  my ($string) = @_ ;
  my @array = split( /,/, $string ) ;
  my $returnvalue ;

  foreach my $element (@array)
  {
    $returnvalue .= $element ;
  }
  return $returnvalue ;
}

sub trim($)
{
	my $string = shift;
	$string =~ s/^\s+//;
	$string =~ s/\s+$//;
	return $string;
}

sub monitor_hosts
{
	my ($package, $extra_params) = @_;
	my %log_data;
	my $conn = $extra_params->{'conn'};
	$logging_obj->log("DEBUG","Entered monitor_hosts function\n");
	my %telemetry_metadata = ('RecordType' => 'MonitorHost');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	my $host_guid = $AMETHYST_VARS->{"HOST_GUID"};
	eval
	{
		my $file_path = $BASE_DIR."/"."var/hosts/";    
		select(STDERR); $| = 1;		# make unbuffered
		select(STDOUT); $| = 1;		# make unbuffered

		#get all hosts 
		my %allHosts = &getHostsInfo($conn);
		
		#get only ps
		%allHosts = (&getOnlyPs($conn),%allHosts);
		
		my $cs_host_name = $allHosts{$host_guid};
	
		ProcessServer::Unregister::unregister_ps($conn);
		
		$logging_obj->log("EXCEPTION","monitor_hosts conn::".Dumper($conn));
		
		my $sql_plan = "SELECT
							certExpiryDetails
						FROM
							hosts 
						WHERE
							id = '$host_guid'";
		
		my $rs_data = $conn->sql_exec($sql_plan);
		my $cert_expiry_data;
		my $requestData;
		if (defined $rs_data)
		{
			my @result_arr = @{$rs_data};
			$cert_expiry_data = $result_arr[0]{'certExpiryDetails'};
			
			$logging_obj->log("EXCEPTION","monitor_hosts host_guid::$host_guid \n cs_host_name:: $cs_host_name \n cert_expiry_data::$cert_expiry_data");
		
			if($cert_expiry_data)
			{
				my $cert_details = unserialize($cert_expiry_data);
				&checkCertExpiryAlerts($conn, $cert_details->{"CsCertExpiry"}, 1, $host_guid, $cs_host_name);
			}
		}
	}; 
	if ($@)
	{
		$log_data{"RecordType"} = "MonitorHost";
		$log_data{"ErrorMessage"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
	}
}

#
# This function calculates the difference between the cxtime and last processed
# file time which are present in monitor.txt for each replication pair.If the
# difference is greater than 30 minutes,it updates the database.
#
sub file_monitor
{
	my ($package, $extra_params) = @_;
	my $output = $extra_params->{'data_hash'};
	
	$logging_obj->log("DEBUG","Entered file_monitor function\n");
	my @input_parameters;

    eval
    {	
		my $pair_data = $output->{'pair_data'};
		my $host_data = $output->{'host_data'};
		my $input;
		my $i=0;
		
		foreach my $pair_id (keys %{$pair_data}) {
			my $execution_state = $pair_data->{$pair_id}->{'executionState'};
			my $input;
			
			my @actual_pair_array = split(/\!\!/,$pair_id);								
			my $org_pair_id = $actual_pair_array[0];
			
			if($execution_state == '1' or $execution_state == '0' or $execution_state == '4') {
			
				my $sourceHostId = $pair_data->{$pair_id}->{'sourceHostId'};
				my $destinationHostId =  $pair_data->{$pair_id}->{"destinationHostId"};
				my $sourceDeviceName = TimeShotManager::formatDeviceName($pair_data->{$pair_id}->{"sourceDeviceName"});
				my $destinationDeviceName = TimeShotManager::formatDeviceName($pair_data->{$pair_id}->{"destinationDeviceName"});
				my $jobId =  $pair_data->{$pair_id}->{"jobId"};
				my $isCommunicationfromSource =  $pair_data->{$pair_id}->{"isCommunicationfromSource"};
				
				my $srcHostName = $host_data->{$sourceHostId}->{'name'};
				my $destHostName = $host_data->{$destinationHostId}->{'name'};			
				my $src_os_flag = $host_data->{$sourceHostId}->{'osFlag'};
				
				my $get_monitor_file = Utilities::makePath($BASE_DIR."/".$sourceHostId."/".$sourceDeviceName."/diffs/monitor.txt",$src_os_flag);
				
				if (-e $get_monitor_file)
				{
					open(FH,"$get_monitor_file");
					my @file_content = <FH>;
					close (FH);
					my $fileName = $file_content[0];
					chomp($fileName);
					my @time_stamps = split(/:/,$file_content[1]);
					my $file_time_stamp = $time_stamps[0];
					my $process_time = $time_stamps[1];
					my $fileType = $time_stamps[2];
					my $timeDelay = 0;
					$timeDelay = $process_time-$file_time_stamp;
					my $timeinMin = int $timeDelay/60;
					my $notify_user = 0;
					
					if ($timeDelay > $SOURCE_FAILURE && $file_time_stamp!=0)
					{
						my $src_disk_name = '';
						if($src_os_flag == 1)
						{
							my $pair_source_key = $sourceHostId."!!".$pair_data->{$pair_id}->{"sourceDeviceName"};
							my $lv_data = $output->{'lv_data'}->{$pair_source_key};
							my $display_name = $lv_data->{'display_name'};
							my ($physical_drive, $disk_number) = split('PHYSICALDRIVE', $display_name, 2);
							$src_disk_name = 'Disk'.$disk_number;
						}
						else
						{
							$src_disk_name = $sourceDeviceName;
						}
					
						$notify_user = 1;
						$input->{$i}->{"queryType"} = "UPDATE";
						$input->{$i}->{"tableName"} = "$SRC_DST_LV_TBL";
						$input->{$i}->{"fieldNames"} = "isCommunicationfromSource = '$notify_user'";
						$input->{$i}->{"condition"} = "pairId='$org_pair_id'";						
					}
					else
					{
						if($isCommunicationfromSource)
						{
							$input->{$i}->{"queryType"} = "UPDATE";
							$input->{$i}->{"tableName"} = "$SRC_DST_LV_TBL";
							$input->{$i}->{"fieldNames"} = "isCommunicationfromSource = '$notify_user'";
							$input->{$i}->{"condition"} = "pairId='$org_pair_id'";
						}				
					}
					push(@input_parameters, $input);
					$i++;					
				}				
			}
		}
		foreach my $keys (@input_parameters)
		{
			foreach my $key (keys %{$keys})
			{
				$input->{$i} = $keys->{$key};				
				$i++;
			}
		}
		
		if($input ne '') {		
			&update_cs_db("file_monitor",$input);		
		}
		$logging_obj->log("DEBUG","End of file_monitor function\n");
    };    
    if ($@)
    {    	
		$logging_obj->log("EXCEPTION","file_monitor".$@);
    }

    return 1;
}



sub get_host_name
{
	my ($hostId,$conn) = @_;
	my $tmp_dbh;
	my $hostname;
    eval
	{
        	# dbh ping to check whether valid db resource or not and check the mysql availlable
	        if ($conn->sql_ping_test())
        	{
	        	
                my $conn = new Common::DB::Connection();
				
	        }
    	};
	if ($@)
	{
        	
			$logging_obj->log("EXCEPTION","get_host_name".$@);
		return;
    	}
	##Host Based Compression Change
	$hostname = $conn->sql_get_value('hosts', 'name', "id='$hostId'");
	return $hostname;
}


#
# returns the Host ID based on Host IP
#
sub get_host_id {
    
    my ($hostIp) = @_;
    
    my ($DB_TYPE ,$DB_NAME,$DB_HOST,$DB_USER,$DB_PASSWD );
    open CONF, Utilities::makePath($BASE_DIR."/".$AMETHYST_CONF_FILE);
    while (<CONF>)
    {
        if (/([\w]+)[\s]*=[\s]*"(.+)"/ && !(/^[\s]*#/))
        {
            if    ($1 eq "DB_TYPE")      { $DB_TYPE      = $2; }
            elsif ($1 eq "DB_NAME")      { $DB_NAME      = $2; }
            elsif ($1 eq "PS_CS_IP")      { $DB_HOST      = $2; }
            elsif ($1 eq "DB_USER")      { $DB_USER      = $2; }
            elsif ($1 eq "DB_PASSWD")    { $DB_PASSWD    = $2; }
        }
    }
    close CONF;
    
    #my $dbh = DBI->connect_cached("DBI:$DB_TYPE:database=$DB_NAME;host=$DB_HOST",
    #$DB_USER, $DB_PASSWD, {'RaiseError' => 0, 'PrintError' => 1, 'AutoCommit' => 1});	
    my %args = ( 'DB_NAME' => $DB_NAME,
                 'DB_PASSWD'=> $DB_PASSWD,
                 'DB_HOST' => $DB_HOST
               );
    my $args_ref = \%args;
    my $conn = new Common::DB::Connection($args_ref);
    #my $conn = new Common::DB::Connection();
    
    my $hostid = $conn->sql_get_value('hosts', 'id', "ipaddress='$hostIp'");

    return $hostid;
}

sub getProtectionHosts
{
    #my $obj = shift;
    
    my ($host_id) = @_;
    
    #print "\nHOST ID ::: $host_id\n";
    
    my ($DB_TYPE ,$DB_NAME,$DB_HOST,$DB_USER,$DB_PASSWD );
    open CONF, Utilities::makePath($BASE_DIR."/".$AMETHYST_CONF_FILE);
    while (<CONF>)
    {
        if (/([\w]+)[\s]*=[\s]*"(.+)"/ && !(/^[\s]*#/))
        {
            if    ($1 eq "DB_TYPE")      { $DB_TYPE      = $2; }
            elsif ($1 eq "DB_NAME")      { $DB_NAME      = $2; }
            elsif ($1 eq "DB_HOST")      { $DB_HOST      = $2; }
            elsif ($1 eq "DB_USER")      { $DB_USER      = $2; }
            elsif ($1 eq "DB_PASSWD")    { $DB_PASSWD    = $2; }
        }
    }
    close CONF;
		
    my $sql;
    my @host_list;

    my $count = 0;
	$count = $conn->sql_get_value('clusters', 'count(*)', "hostId='$host_id'");
   
    if($count > 0)
    {
	    my $clusterId = $conn->sql_get_value('clusters', 'clusterId', "hostId='$host_id'");	
	     
	    $sql = "SELECT  hostId FROM  clusters WHERE clusterId = '$clusterId' GROUP BY hostId";
	    
	    my $result = $conn->sql_exec($sql);

		foreach my $ref (@{$result})
        {
            push(@host_list, $ref->{'hostId'});
        }
    }
    else
    {
     push(@host_list, $host_id);
    }
   
   
    return @host_list;
}

#
# returns the Host ID based on Host IP
#
sub get_host_ids {
    
   my ($conn) = @_; 
    my @ids;
    my $sql = "SELECT 
					id 
				FROM 
					hosts 
				WHERE 
					(sentinelEnabled = '1' 
				OR 
					outpostAgentEnabled = '1') 
				AND 
					id <> '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";

  	my $result = $conn->sql_exec($sql);

	foreach my $ref (@{$result})
	{
		push(@ids, $ref->{"id"});
	}
  
    return @ids;
}

#
# Returns host_id from supplied host_name
#
sub getHostId
{

    my ($conn, $hostname) = @_;
	my $id = $conn->sql_get_value('hosts', 'id', "name = '$hostname'");	

	
	return $id;
}

#
# Monitor the process server. If the process server is not
# registered with CS, register it first.
#
sub monitor_ps
{
	my ($package, $extra_params) = @_;
	$logging_obj->log("DEBUG","Entered monitor_ps function\n");
	my $data_hash = $extra_params->{'data_hash'};
	my $host_data = $data_hash->{'host_data'};
	my $ps_data = $data_hash->{'ps_data'}; 
	my $count = 0;
	my $i = 0;
	my $cx_type = $AMETHYST_VARS->{'CX_TYPE'};
	my $cx_mode = $AMETHYST_VARS->{'CX_MODE'};
	
	if(exists($host_data->{$host_guid}) && exists($ps_data->{$host_guid}))
	{
		if($host_data->{$host_guid}->{'processServerEnabled'} == 1)
		{
			$count = 1;
		}	
	}
	
	eval
	{		 
		ProcessServer::Register::registerHost();
		if ($count and (!$cx_mode) and ($cx_type == Common::Constants::PROCESS_SERVER or $cx_type == Common::Constants::CONFIG_PROCESS_SERVER)) 
		{ 
			if($^O =~ /lin/i) {			
				ProcessServer::Register::update_hba_ports();
			}  
		}

		# Commenting out below events as these events are ported to ProcessServerMonitor service
		# TimeShotManager::monitor_ps_reboot_time($host_data);
		# TimeShotManager::monitor_ps_upgrade($host_data);

		$logging_obj->log("DEBUG","End of monitor_ps function\n");
	};
	if ($@)
	{
		
		$logging_obj->log("EXCEPTION","monitor_ps: $@");
	}
}

sub monitor_standby_details
{
	my ($package, $extra_params) = @_;
	my $api_output = $extra_params->{'data_hash'};
	$logging_obj->log("DEBUG","monotor_standby_details");
	my $stand_by_details = $api_output->{'stand_by_data'};
	
	eval
	{
		my $primary_ip_address = '';
		my $primary_port_number = '';
		my $PS_CS_IP  = $AMETHYST_VARS->{"PS_CS_IP"};
		my $ps_port  = $AMETHYST_VARS->{"PS_CS_PORT"};
		my $cx_type  = $AMETHYST_VARS->{"CX_TYPE"};
		
		my $is_node_up = &is_node_reachable($PS_CS_IP,$ps_port);
		
		my ($log_file, $file_name);
		if (Utilities::isWindows())
		{ 
			$log_file  = "$cs_installation_path\\home\\svsystems\\standby_ip.txt";	
			$file_name = "$cs_installation_path\\home\\svsystems\\etc\\amethyst.conf";
		}
		else
		{
			$log_file = "/home/svsystems/standby_ip.txt";
			$file_name ="/home/svsystems/etc/amethyst.conf";	
		}
		
		my $self_host_id;
		if(exists($AMETHYST_VARS->{'CLUSTER_NAME'}))
		{
			$self_host_id = $AMETHYST_VARS->{'CLUSTER_NAME'};
		}
		else
		{
			$self_host_id = $AMETHYST_VARS->{'HOST_GUID'};
		}
		
		$logging_obj->log("DEBUG","monitor_snadby is_node_up".$is_node_up);
		if($is_node_up)
		{
			if($stand_by_details->{$self_host_id}->{'role'} ne 'PRIMARY') 
			{
				my $standby_ip_address = $stand_by_details->{$self_host_id}->{'ip_address'};
				open(LOGFILE, ">$log_file") || print "not able to open the file";
				print LOGFILE $standby_ip_address;
				close(LOGFILE);	
			}
			else
			{				
				unlink($log_file) if(-e $log_file);
			}            
		}
		else
		{
			open(FH , "<$log_file ") ;
			my $contents = <FH>;
			my $db_host = chomp($contents);
			close(FH);
			
			my $role = &get_cx_role($contents);		
			$logging_obj->log("DEBUG","monitor_standby : role :: $role");
            if($role eq "")
            {
                return;
            }
			if ($role eq "Primary")
			{
				$logging_obj->log("DEBUG","monitor_standby : primary_ip_address :: $contents, primary_port_number::$ps_port");

				&file_content_replace($file_name, $contents, $cx_type, $ps_port);
			}
		}
		$logging_obj->log("DEBUG","End of monotor_standby_details");
	};
	if ($@)
	{ 
		$logging_obj->log("EXCEPTION","monotor_standby_details: $@");
	}

}

sub is_node_reachable
{
	my ($ip_address , $ps_port) = @_;
	my $flag = 0;
	eval
	{
		my $response;
		my $http_method = "GET";
		my $https_mode = ($AMETHYST_VARS->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;
	
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $ip_address, 'PORT' => $ps_port);
		my $param = {'access_method_name' => "request_handler", 'access_file' => "/request_handler.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'content' => ''};
			
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);
			
		if ($response->{'status'})
		{
			$flag = 1;
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : " . $@);
	}
	return $flag;
}


sub file_content_replace
{

	my($FILE , $replcae_string, $cx_type, $primary_port_number) = @_;

	$replcae_string = '"'.$replcae_string.'"';
	$logging_obj->log("EXCEPTION","file_content_replace FILE: $FILE");
	$logging_obj->log("EXCEPTION","file_content_replace replcae_string: $replcae_string");
	# File operations for back up file
	unless ( open(GH, ">${FILE}.orig") )
	{
		$logging_obj->log("EXCEPTION","Cannot open ${FILE} for Writing!");
	   print "Cannot open ${FILE}.new for Writing!\n";
	   exit 4;
	}

	# File opernations for source file
	unless ( open(FH , "<$FILE") )
	{
		$logging_obj->log("EXCEPTION","Cannot open $FILE for reading!");
		print "Cannot open $FILE for reading!\n";
		exit 3;
	}
	local $/ = undef;
	# Reading contents of source file
	my $contents = <FH>;
	$logging_obj->log("EXCEPTION","file_content_replace contents: ". $contents);
	close(FH);


	# Backin up the contents to the back up file
	print GH $contents;
	close(GH);

	# Replacing the contents where ever required
	$contents =~ s/^[\s\t]*PS_CS_IP[\s\t]*=.*$/PS_CS_IP = $replcae_string/m;
	$contents =~ s/^[\s\t]*PS_CS_PORT[\s\t]*=.*$/PS_CS_PORT = $primary_port_number/m;
	if($cx_type == 2)
	{
		$contents =~ s/^[\s\t]*CS_IP[\s\t]*=.*$/CS_IP = $replcae_string/m;
		$contents =~ s/^[\s\t]*CS_PORT[\s\t]*=.*$/CS_PORT = $primary_port_number/m;
	}

	# Deleting the source File
	unlink("$FILE");

	# Creating the source file
	unless ( open(WH , ">$FILE") )
	{

		$logging_obj->log("EXCEPTION","Cannot open $FILE for Writing!");
		print "Cannot open $FILE for Writing!\n";
		exit 3;
	}
	# Writing the replaced content to the file
	print WH $contents;
	
    my $restartmanagerd_file;
    if (Utilities::isWindows())
    { 
        $restartmanagerd_file  = "$cs_installation_path\\home\\svsystems\\bin\\restartmanagerd";
    }
    else
    {
        $restartmanagerd_file  = "/home/svsystems/bin/restartmanagerd";	
    }    
    
	open(FH , ">$restartmanagerd_file") or $logging_obj->log("EXCEPTION","Cannot create $restartmanagerd_file");
	close(FH);	
	
	close(WH);
}
#
# Return the IP address of the host
#
sub getHostIP
{
	use Socket;
	my $packed_ip;
	my $hostname = `hostname`;
	chomp $hostname;
	$packed_ip = gethostbyname("$hostname");
	my $ip_address;
	if (defined $packed_ip) 
	{
	    $ip_address = inet_ntoa($packed_ip);
	}
	return $ip_address;
}

#
# Upload the perf and rpo log files from CS to PS
#
sub upload_log_files
{
	my ($package, $extra_params) = @_;
	my $data_hash = $extra_params->{'data_hash'};
    $logging_obj->log("DEBUG","Entered upload_log_files function\n");
	
	eval
	{		
		#
		# Should not process log files if, there is no pair configured
		#
		#$logging_obj->log("DEBUG","upload_log_files :: data_hash ::\n".Dumper($data_hash));
		my $host_data = $data_hash->{'host_data'};
		my $pair_data = $data_hash->{'pair_data'};
		my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
		my $num_hosts = 0;

		foreach my $pair_id (keys %{$pair_data}) {
			my $source_id = $pair_data->{$pair_id}->{'sourceHostId'};
			if($host_data->{$source_id}->{'sentinelEnabled'} == 1)
			{
				$num_hosts = 1;
				last;
			}
		}

		if($num_hosts == 0) { return; }
		
		ProcessServer::Register::upload_log_files($data_hash,$BASE_DIR);
		$logging_obj->log("DEBUG","End of upload_log_files function\n");
	};
	if ($@)
	{		
		$logging_obj->log("EXCEPTION","upload_log_files: $@");
	}
}

sub upload_log_files_perf
{
	my ($package, $extra_params) = @_;
	my $data_hash = $extra_params->{'data_hash'};
    $logging_obj->log("DEBUG","Entered upload_log_files function\n");
	
	eval
	{		
		#
		# Should not process log files if, there is no pair configured
		#
		my $host_data = $data_hash->{'host_data'};
		my $pair_data = $data_hash->{'pair_data'};
		my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
		my $num_hosts = 0;

		foreach my $pair_id (keys %{$pair_data}) {
			my $source_id = $pair_data->{$pair_id}->{'sourceHostId'};
			if($host_data->{$source_id}->{'sentinelEnabled'} == 1)
			{
				$num_hosts = 1;
				last;
			}
		}
		
		if($num_hosts == 0) { return; }
		
		ProcessServer::Register::upload_log_files_perf($data_hash,$BASE_DIR);
	};
	if ($@)
	{		
		$logging_obj->log("EXCEPTION","upload_log_files: $@");
	}
}

# to delete the replication pair
sub delete_replication_cs
{
	my ($package, $extra_params) = @_;
	my %log_data;
	my $conn = $extra_params->{'conn'};
	$logging_obj->log("DEBUG","Entered delete_replication_cs function\n");
	my %telemetry_metadata = ('RecordType' => 'DeleteReplicationCS');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
	eval
	{   
		my $PS_DELETE_DONE = 31;
		my $check_query = "select 
							   sourceHostId,
							   sourceDeviceName,
							   destinationHostId,
							   destinationDeviceName,
							   processServerId,
							   scenarioId
						   from 
							   srcLogicalVolumeDestLogicalVolume 
						   where 
							   replication_status = $PS_DELETE_DONE";		
		my $result = $conn->sql_exec($check_query);

		foreach my $ref (@{$result})
		{
			my $sourceHostId = $ref->{"sourceHostId"};
			my $sourceDeviceName = $ref->{"sourceDeviceName"};
			my $destinationHostId = $ref->{"destinationHostId"};
			my $destinationDeviceName = $ref->{"destinationDeviceName"};
			my $process_server_id = $ref->{"processServerId"};
			my $scenario_id = $ref->{"scenarioId"};
			my $num_pairs = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'count(destinationHostId)', "processServerId = '$process_server_id'");
			if($num_pairs == 1)
			{
				my $bpm_sql = "select sourceHostId from BPMAllocations where  targetHostId = '$destinationHostId' group by targetHostId";
				my $result = $conn->sql_exec($bpm_sql);
				
				if (defined $result)
				{
					my @value = @$result;

					my $bpm_id = $value[0]{'sourceHostId'};	
					my $unset_sql = "update BPMPolicies set isBPMEnabled = 2 where sourceHostId = '$bpm_id'";
					$conn->sql_query($unset_sql);
				}
			}
			if (Utilities::isWindows())
			{    
				my $stop_replication_path = "$cs_installation_path\\home\\svsystems\\admin\\web\\stop_unlock_replication.php";
				`$AMETHYST_VARS->{'PHP_PATH'} "$stop_replication_path" "$sourceHostId" "$sourceDeviceName" "$destinationHostId" "$destinationDeviceName" "$scenario_id"`; 						
			}
			else
			{
				if (-e "/usr/bin/php")
				{
					  `cd $AMETHYST_VARS->{WEB_ROOT}/; php stop_unlock_replication.php  "$sourceHostId" "$sourceDeviceName" "$destinationHostId" "$destinationDeviceName" "$scenario_id"`;
				}
				elsif(-e "/usr/bin/php5")
				{
					`cd $AMETHYST_VARS->{WEB_ROOT}/; php5 stop_unlock_replication.php  "$sourceHostId" "$sourceDeviceName" "$destinationHostId" "$destinationDeviceName" "$scenario_id"`;
				}
			}
		}	
	};
	# end fix 2341
	if ($@)
	{
        $log_data{"RecordType"} = "DeleteReplicationCS";
		$log_data{"ErrorMessage"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
	}
}

sub delete_replication_ps
{
	my ($package, $extra_params) = @_;
	my $api_output = $extra_params->{'data_hash'};
	$logging_obj->log("DEBUG","Entered delete_replication_ps function\n");   
   
	eval
	{ 
		my $pair_data = $api_output->{'pair_data'};
		my $plan_data = $api_output->{'plan_data'};
		my $SOURCE_DELETE_DONE =30;
		my $StatesNotOkToProcessDeletion = Fabric::Constants::PS_DELETE_DONE;
		my @batch;
		my @updated_pairs_ids;
		my @phy_lun_ids;
		my @pair_deletion;
		my $phy_lun_ids;
		my $pair_deletion;		
		my $input;
		my $i=0;

		my $http_method = "POST";
		my $https_mode = ($AMETHYST_VARS->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $AMETHYST_VARS->{'PS_CS_IP'}, 'PORT' => $AMETHYST_VARS->{'PS_CS_PORT'});
		my $param = {'access_method_name' => "stop_replication_ps", 'access_file' => "/stop_replication_ps.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $request_http_obj = new RequestHTTP(%args);

		foreach my $pair_id (keys %{$pair_data}) {
			my $phy_lun_id = $pair_data->{$pair_id}->{'Phy_Lunid'};		
			if($phy_lun_id) {				
				if(exists $phy_lun_ids->{$phy_lun_id} ) { 
					$phy_lun_ids->{$phy_lun_id} = $phy_lun_ids->{$phy_lun_id} + 1;
				} else {	
					$phy_lun_ids->{$phy_lun_id} = 1;
				}
			}
			
			my $replication_status = $pair_data->{$pair_id}->{'replication_status'};
			if(($replication_status == $SOURCE_DELETE_DONE || $replication_status == $StatesNotOkToProcessDeletion) and ($phy_lun_id)) {
					if(exists $pair_deletion->{$phy_lun_id} ) { 
					$pair_deletion->{$phy_lun_id} = $pair_deletion->{$phy_lun_id} + 1;
				} else {	
					$pair_deletion->{$phy_lun_id} = 1;
				}
			}
		}
		
		foreach my $pair_id (keys %{$pair_data})
		{
			my $replication_status = $pair_data->{$pair_id}->{'replication_status'};
			
			if($replication_status == 30)
			{
				my $lunId = $pair_data->{$pair_id}->{"Phy_LunId"};
				my $sourceHostId = $pair_data->{$pair_id}->{"sourceHostId"};
				my $sourceDeviceName = $pair_data->{$pair_id}->{"sourceDeviceName"};
				my $destinationHostId = $pair_data->{$pair_id}->{"destinationHostId"};
				my $destinationDeviceName = $pair_data->{$pair_id}->{"destinationDeviceName"};
				my $planId = $pair_data->{$pair_id}->{"planId"};
				my $solution_type = $plan_data->{$planId}->{'solutionType'};
				
				$logging_obj->log("DEBUG","Verifying solution Type to proceed with the pillar deletion solution_type::$solution_type for plan: $planId");
				if($solution_type eq "ARRAY" && $lunId)
				{
					$logging_obj->log("DEBUG","Array Type check passed so Cleaning stale entries");
					my $tot_pair_count = $phy_lun_ids->{$lunId};
					my $del_pair_count = $pair_deletion->{$lunId};
					my $batch = &deleteStaleEntries($sourceHostId,$lunId,$destinationHostId,$destinationDeviceName,$host_guid, $tot_pair_count, $del_pair_count, $api_output);
					my @stale_batch = @$batch;
					push(@batch,@stale_batch);
				}
				
				my $response;
				my %perl_args = ('sourceHostId' => $sourceHostId,'sourceDeviceName' => $sourceDeviceName ,'destinationHostId' => $destinationHostId ,'destinationDeviceName' => $destinationDeviceName);
				my $content = {'content' => \%perl_args};			
				
				$response = $request_http_obj->call($http_method, $param, $content);
				
				my @actual_pair_array = split(/\!\!/,$pair_id);								
				my $org_pair_id = $actual_pair_array[0];
				
				if(!(grep  $_ eq $pair_id,@updated_pairs_ids))
				{
				
					my $src_os_flag = $api_output->{'host_data'}->{$sourceHostId}->{'osFlag'};
					my $tgt_os_flag = $api_output->{'host_data'}->{$destinationHostId}->{'osFlag'};

					my $source_file_dir = $BASE_DIR."/".$sourceHostId."/".$sourceDeviceName;
					my $source_file_dir_modified = Utilities::makePath ($source_file_dir, $src_os_flag);

					my $ofile_dir =  $BASE_DIR."/".$destinationHostId."/".$destinationDeviceName."/".'resync';
					my $ofile_dir_modified = Utilities::makePath($ofile_dir, $tgt_os_flag);

					my $destfile_dir =  $BASE_DIR."/".$destinationHostId."/".$destinationDeviceName;
					$destfile_dir = Utilities::makePath ($destfile_dir, $tgt_os_flag);

					my $one_to_many_target_arry = $api_output->{'one_to_many_targets'}->{$sourceHostId."!!".$sourceDeviceName};
					my $onetomany_src_cnt = scalar(keys %{$one_to_many_target_arry});

					if($onetomany_src_cnt <= 1)
					{
						my $srcfiles_cleanup_status = &TimeShotManager::clearSourceFiles($source_file_dir_modified);
					}

					my $tgtfiles_cleanup_status = &TimeShotManager::clearTargetFiles($ofile_dir_modified, $destfile_dir);
					my $tgt_azr_dir_path = $destfile_dir."/".$AZURE_PENDING_UPLOAD_NON_RECOVERABLE;
					my $files_cleanup_status = &TimeShotManager::clearFilesAndDirectory($tgt_azr_dir_path);
					$logging_obj->log("DEBUG","$tgt_azr_dir_path = $files_cleanup_status");
					my $tgt_azr_dir_path = $destfile_dir."/".$AZURE_PENDING_UPLOAD_RECOVERABLE;
					my $files_cleanup_status = &TimeShotManager::clearFilesAndDirectory($tgt_azr_dir_path);
					$logging_obj->log("DEBUG","$tgt_azr_dir_path = $files_cleanup_status");
					my $tgt_azr_dir_path = $destfile_dir."/".$AZURE_UPLOADED;
					my $files_cleanup_status = &TimeShotManager::clearFilesAndDirectory($tgt_azr_dir_path);
					$logging_obj->log("DEBUG","$tgt_azr_dir_path = $files_cleanup_status");
		
					
					my $PS_DELETE_DONE = 31;
					$sourceDeviceName =~ s/\\/\\\\/g;
					$destinationDeviceName =~ s/\\/\\\\/g;
					
					$input->{$i}->{"queryType"} = "UPDATE";
					$input->{$i}->{"tableName"} = "srcLogicalVolumeDestLogicalVolume";
					$input->{$i}->{"fieldNames"} = "replication_status = $PS_DELETE_DONE";
					$input->{$i}->{"condition"} = "replication_status = $SOURCE_DELETE_DONE AND processServerId = '$host_guid' AND pairId = '$org_pair_id'";			

					push(@updated_pairs_ids,$pair_id);
					$i++;
				}								
			}
		}
		
		if($input ne '') {
			&update_cs_db("delete_replication_ps",$input);
		}
	};

	if ($@)
	{        	
		$logging_obj->log("EXCEPTION","delete_replication_ps".$@);
	}
}


#update the delete flag
sub update_delete_flag
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	$logging_obj->log("DEBUG","Entered update_delete_flag function\n");

	my $RELEASE_DRIVE_PENDING_OLD = 28;
	my $SOURCE_DELETE_DONE =30;
	

	eval
	{	   
		
		my $check_query = "update 
				   srcLogicalVolumeDestLogicalVolume 
			   set 
				  replication_status = $SOURCE_DELETE_DONE
			   where 
				   replication_status = $RELEASE_DRIVE_PENDING_OLD";
		$logging_obj->log("DEBUG","update_delete_flag ::check_query : $check_query\n");				
		my $check_sth = $conn->sql_query($check_query);				
	};
	#end fix 2341
	if ($@)
	{
		$logging_obj->log("EXCEPTION","update_delete_flag".$@);
	}    				   
}

sub monitor_application
{
	my ($package, $extra_params) = @_;
	my %log_data;
	my $conn = $extra_params->{'conn'};
	my %telemetry_metadata = ('RecordType' => 'MonitorApplication');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	#Calls the csUpAndRunningAlert subroutine to send 'cs up and running' notification trap
    if((&isPrimaryActive($conn) != 0) || (&isStandAloneCX($conn) == 1))
	{
		&Messenger::csUpAndRunningAlert($conn);	
    }
	$logging_obj->log("DEBUG","Entered monitor_application function\n");
	
    eval
    {
		my %application_args = ('conn'=>$conn);
		my $application = new ConfigServer::Application(%application_args);

		$application->updateScenario();
		$application->update_policy_after_rollback();
		$application->deleteScenario();
		$application->monitorPolicyRotation();
		#$application->monitorPartialProtection();
		$application->update_oracle_devices();
		$application->monitorRecoveryScenarioDeletion();
		$application->monitorRunOncePolices();
		$application->activateCloudPlan();
	};
	if ($@)
	{
		$log_data{"RecordType"} = "MonitorApplication";
		$log_data{"ErrorMessage"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
	}			
}

sub monitor_application_consistency
{	
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	$logging_obj->log("DEBUG","Entered monitor_application_consistency function\n");
	
    eval
    {
		
		my %application_args = ('conn'=>$conn);
		my $application = new ConfigServer::Application(%application_args);

		$application->commonConsistencyPointAvailable();
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","monitor_application_consistency: $@");
	}			
}

sub restart_tmanager_services
{	
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	eval 
	{	
        # CS Renew Certificate Invocation.
		my %application_args = ('conn'=>$conn);
		my $application = new ConfigServer::Application(%application_args);		
		$application->monitorCSRenewCertificatePolicies();
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","Error: Unable to restart tmanager services: $@");
		return;
	}	
}

sub remote_ha_db_sync
{
	my ($package, $extra_params) = @_;
	my $api_output = $extra_params->{'data_hash'};
    my $conn = $extra_params->{'conn'};
	eval
    {
		$logging_obj->log("DEBUG","remote_ha_db_sync:: Entered remote_ha_db_sync function\n");
		my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
		my $cluster_name = $AMETHYST_VARS->{'CLUSTER_NAME'};
		my $app_node_data = $api_output->{'appliance_node_data'};
		my $app_clus_data = $api_output->{'app_clus_data'};
		my $stand_by_data = $api_output->{'stand_by_data'};
		my $num_rows = 0;
		my $standby_ip_address;
		
		if($cluster_name and $stand_by_data->{$cluster_name}) {
			$num_rows = 1;
			$standby_ip_address = $stand_by_data->{$cluster_name}->{'ip_address'};
		} 
		elsif($host_guid && $stand_by_data->{$host_guid}) {
			$num_rows = 1;
			$standby_ip_address = $stand_by_data->{$host_guid}->{'ip_address'};
		}
		
		if($num_rows > 0)
		{
            my $cx_role = &get_cx_role($standby_ip_address);
			my $count =0;
			my ($cxDbBackupPath, $cxDbBackupFile,$cxDbRestorePath,$remote_cx_ip,$remote_cx_port);
			
			if($app_clus_data->{$cluster_name}) {
				$count = 1;
			}
		
			my $node_role = $app_node_data->{$host_guid}->{'nodeRole'};
			
			$logging_obj->log("DEBUG","remote_ha_db_sync:: node_role: $node_role, count: $count \n");
			
			foreach my $host_id (keys %{$stand_by_data})
			{
				if($stand_by_data->{$host_id}->{'role'} ne "PRIMARY")
				{
					$remote_cx_ip = $stand_by_data->{$host_id}->{'ip_address'};
					$remote_cx_port = $stand_by_data->{$host_id}->{'port_number'};
					last;
				}
			}
			
			if(($count && ($node_role eq "ACTIVE")) || (!$count))
			{				
				my $DB_HOST              = $AMETHYST_VARS->{"DB_HOST"};
				my $DB_NAME              = $AMETHYST_VARS->{"DB_NAME"};
				my $DB_USER              = $AMETHYST_VARS->{"DB_USER"};
				my $DB_PASSWD            = $AMETHYST_VARS->{"DB_PASSWD"};
				my $CX_TYPE              = $AMETHYST_VARS->{"CX_TYPE"};
				my $MYSQL_DUMP_CMD       = "mysqldump";
				my $COMMAND_EXEC_FAILURE = -2;
				my $COMMAND_EXEC_SUCCESS = 0;
				
				if(Utilities::isWindows())
				{
					$MYSQL_DUMP_CMD = '"'.$MYSQL_PATH."\\bin\\mysqldump".'"';
					my $cxDbPath = "$cs_installation_path\\home\\svsystems\\rr_dbsync\\";
					$cxDbBackupPath = "$cs_installation_path\\home\\svsystems\\rr_dbsync\\backup\\";
					$cxDbBackupFile = "$cs_installation_path\\home\\svsystems\\rr_dbsync\\backup\\".$DB_NAME.".sql.bak";
					
					my $mkdir_rr_output = mkdir "$cxDbPath", 0777 unless -d "$cxDbPath";
					if ($? != 0) 
					{
						$logging_obj->log("EXCEPTION","remote_ha_db_sync:: Could not create cxDbPath: $cxDbPath directory returng exception: $? , mkdir_rr_output: $mkdir_rr_output, MKDIR $cxDbPath , 0777 unless -d $cxDbPath \n");
						return 0;
					}
					else
					{
						my $mkdir_backup_output = mkdir "$cxDbBackupPath", 0777 unless -d "$cxDbBackupPath";
						if ($? != 0) 
						{
							$logging_obj->log("EXCEPTION","remote_ha_db_sync:: Could not create cxDbBackupPath: $cxDbBackupPath directory returng exception: $? , mkdir_backup_output: $mkdir_backup_output, MKDIR $cxDbBackupPath , 0777 unless -d $cxDbBackupPath \n");
							return 0;
						}
					}
				}
				else
				{
					$cxDbBackupPath = "/home/svsystems/rr_dbsync/backup/";
					$cxDbBackupFile = "/home/svsystems/rr_dbsync/backup/".$DB_NAME.".sql.bak";
					
					if (!-d $cxDbBackupPath)
					{
						my $mkpath_output = mkpath($cxDbBackupPath, { mode => 0777 });
								
						if ($? != 0) 
						{
							$logging_obj->log("EXCEPTION","remote_ha_db_sync:: Could not create cxDbBackupPath: $cxDbBackupPath directory returng exception: $? , mkpath_output: $mkpath_output");
							return 0;
						}
					}
				}				
				
				if($cx_role eq "Primary")				
				{										
					my $DbDumpCmd = "$MYSQL_DUMP_CMD --host=$DB_HOST -u $DB_USER -p$DB_PASSWD -C $DB_NAME> $cxDbBackupFile 2>&1";
					my $retValRr = &execute_command($DbDumpCmd);					
					$logging_obj->log("DEBUG","remote_ha_db_sync:: retValRr:$retValRr, COMMAND_EXEC_SUCCESS: $COMMAND_EXEC_SUCCESS \n");
					
					if($COMMAND_EXEC_SUCCESS != $retValRr)
					{
						$logging_obj->log("EXCEPTION","remote_ha_db_sync:: Failed to take DB dump \n");
					}
					&primary_files_upload($cxDbBackupPath,$remote_cx_ip, $remote_cx_port);
				}
				elsif($cx_role eq "Secondary")
				{
					my $MYSQL = "mysql";
					
					if(Utilities::isWindows())
					{
						$MYSQL = '"'.$MYSQL_PATH."\\bin\\mysql".'"';
					}
					
					$logging_obj->log("DEBUG","remote_ha_db_sync:: CX_TYPE:$CX_TYPE \n cx_role : $cx_role, \n num_rows: $num_rows \n");					
					
					if(($CX_TYPE != 2) && ($cx_role eq "Secondary" || ($num_rows == 0)) && (-e $cxDbBackupFile))
					{
						my $DbRestoreCmd = "$MYSQL -u $DB_USER $DB_NAME -p$DB_PASSWD < $cxDbBackupFile 2>&1";
						my $retValRr = &execute_command($DbRestoreCmd);
						$logging_obj->log("DEBUG","remote_ha_db_sync:: retValRr: $retValRr \n");
						if($COMMAND_EXEC_SUCCESS != $retValRr)
						{
							$logging_obj->log("EXCEPTION","remote_ha_db_sync:: Failed to restore DB dump \n");
						}
						unlink ( $cxDbBackupFile );
						$logging_obj->log("DEBUG","remote_ha_db_sync:: unlink is done \n");
					}
					&remote_CX_files_sync($cxDbBackupPath);
				}								
			}			
		}
		else {
			#Clean standby tables if any records are present
			my $count = scalar keys %{$stand_by_data};	
			
			$logging_obj->log("DEBUG","End of remote_ha_db_sync:: count::$count\n");
			if( $count > 0) {
				my $input;
				$input->{0}->{"queryType"} = "DELETE";
				$input->{0}->{"tableName"} = "standByDetails";
				$input->{0}->{"fieldNames"} = "";
				$input->{0}->{"condition"} = "";				
				$logging_obj->log("DEBUG","End of remote_ha_db_sync:: input\n".Dumper($input));
				&update_cs_db("remote_ha_db_sync",$input);					
			}
		}
		$logging_obj->log("DEBUG","End of remote_ha_db_sync:: Entered remote_ha_db_sync function\n");
        &ha_db_sync($conn);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","remote_ha_db_sync:: ERROR:: $@\n");
	}
}

sub primary_files_upload
{
	my ($cxDbBackupPath, $remote_cx_ip, $remote_cx_port) = @_;
    
    $logging_obj->log("INFO","primary_files_upload : remote_cx_ip : $remote_cx_ip, remote_cx_port : $remote_cx_port");
    eval
	{
		my $CS_IP    = ($AMETHYST_VARS->{'CX_TYPE'} == 1) ? $AMETHYST_VARS->{'CS_IP'} : $AMETHYST_VARS->{'PS_CS_IP'};
		my $CS_PORT  = ($AMETHYST_VARS->{'CX_TYPE'} == 1) ? $AMETHYST_VARS->{'CS_PORT'} : $AMETHYST_VARS->{'PS_CS_PORT'};
		my $INSTALLATION_DIR = $AMETHYST_VARS->{"INSTALLATION_DIR"};
		my $DB_NAME              = $AMETHYST_VARS->{"DB_NAME"};
		my $cxBackupFile = $cxDbBackupPath.$DB_NAME.".sql.bak";
		my $http = ($AMETHYST_VARS->{'CX_TYPE'} == 1)?$AMETHYST_VARS->{'CS_HTTP'}:$AMETHYST_VARS->{'PS_CS_HTTP'};
		my %file_info;
				
		my $http_method = "POST";
		my $https_mode = ($http eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $CS_IP, 'PORT' => $CS_PORT);
		my $param = {'access_method_name' => "create_tar_for_remote", 'access_file' => "/create_tar_for_remote.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'content' => ''};
		
		my $request_http_obj = new RequestHTTP(%args);
		my $response = $request_http_obj->call($http_method, $param, $content);
		
		if($response->{'status'})
		{
			my $tarball_src_path = $response->{'content'};
			chomp $tarball_src_path;
			$file_info{$tarball_src_path} = $cxDbBackupPath if $tarball_src_path ne '';       
		}
		
		$file_info{$cxBackupFile} = $cxDbBackupPath;
		
		&upload_file(\%file_info,$remote_cx_ip, $remote_cx_port);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : " . $@);
	}	
}

sub remote_CX_files_sync
{
    my ($cxDbBackupPath) = @_;
    my $INSTALLATION_DIR = $AMETHYST_VARS->{"INSTALLATION_DIR"};
	 
	my $tarball_src_path = $cxDbBackupPath."syncFiles.tar.gz";
    
    if(-e $tarball_src_path)
    {
		my $tarball_dest_path = $INSTALLATION_DIR."/var/syncFiles.tar.gz";
		if (Utilities::isWindows())
		{
			$tarball_dest_path = "$cs_installation_path\\home\\svsystems\\var\\syncFiles.tar.gz";
		}
		my $return_val = Utilities::move_file($tarball_src_path, $tarball_dest_path);
	   
		if(-e "$tarball_dest_path")
        {
			my $give_permission = &execute_command("chmod 777 $tarball_dest_path");
			if (Utilities::isLinux()) 
			{
				my $untar_tarball = &execute_command("tar -zpxf $tarball_dest_path -C /");
			}
			elsif (Utilities::isWindows()) 
			{
				use if Utilities::isWindows(), "Archive::Zip";
				my $prev_dir = getcwd;
				chdir("C:\\");
				my $tar = Archive::Zip->new($tarball_dest_path);
				$tar->extractTree();
				chdir($prev_dir);
			}
			my $remove_tarball = &execute_command("rm -f $tarball_dest_path");
        }
    }
}

sub ha_db_sync
{
    my ($conn) = @_;
    eval
    {
        my ($cxDbBackupPath,$cxDbRestorePath,$cxHARestorePath);
        my ($cliVmDataSrcPath,$cliVmDataTgtPath);
        my ($cxSourceMBRSrcPath,$cxSourceMBRTgtPath);
        my ($vxInstallPathFailoverSrcPath,$vxInstallPathFailoverTgtPath);
        my ($DbDumpCmd,$DbRestoreCmd,$message,$input);
        my ($auditLogFileCopyCmd,$cliVmDataCopyCmd,$sourceMBRFilesCopyCmd,$vxInstallPathFailoverFilesCopyCmd);
        my $DB_NAME = $AMETHYST_VARS->{"DB_NAME"};
        my $DB_HOST = $AMETHYST_VARS->{"DB_HOST"};
        my $DB_USER = $AMETHYST_VARS->{"DB_USER"};
        my $DB_PASSWD = $AMETHYST_VARS->{"DB_PASSWD"};
        my $INSTALLATION_DIR = $AMETHYST_VARS->{"INSTALLATION_DIR"};
        my $cluster_name = $AMETHYST_VARS->{'CLUSTER_NAME'};
        my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
        my $ps_cs_ip = $AMETHYST_VARS->{'PS_CS_IP'};
        my $ps_cs_port = $AMETHYST_VARS->{'PS_CS_PORT'};
        my $cs_ip = $AMETHYST_VARS->{'CS_IP'};
        my $cs_port = $AMETHYST_VARS->{'CS_PORT'};
        my $MYSQL_DUMP_CMD = "mysqldump";
        my $MYSQL_RESTORE_CMD = "mysql";
        my $COMMAND_EXEC_FAILURE = -2;
        my $COMMAND_EXEC_SUCCESS = 0;        
    
        if($cluster_name)
        {            
			my $isHeartbeatActive = TimeShotManager::isHeartbeatActiveNode();
            if($isHeartbeatActive == 0) #HA Active
            {
                my $db_sync_job_state = $conn->sql_get_value('applianceCluster', 'dbSyncJobState', "applianceId='$cluster_name'"); 
                if($db_sync_job_state eq 'ENABLED')
                {               
                    my $procDir = "/etc/vxagent/bin/inm_dmit";
                    my $seqNo = `$procDir --get_attr SequenceNumber`;
                    my $success_flag = 1;

                    if ($seqNo != undef) 
                    {
                        chomp $seqNo;
                        my @param_array = ($seqNo, $host_guid);
                        my $param_array_ref = \@param_array;
                        my $sqlstmnt = $conn->sql_query("UPDATE applianceNodeMap SET sequenceNumber = ? WHERE nodeId = ?",$param_array_ref);
                    }

                    #collect the following files and folders. svsdb1 dump file, SourceMBR, var/cli/vcon, vcon directories
                    $cxHARestorePath = $INSTALLATION_DIR."/ha/restore/";
                    $cxDbBackupPath = $INSTALLATION_DIR."/ha/backup/".$DB_NAME.".HA.sql.bak";
                    $DbDumpCmd = "$MYSQL_DUMP_CMD --host=$DB_HOST -u $DB_USER -p$DB_PASSWD -C $DB_NAME > $cxDbBackupPath 2>&1";
                    my $retVal_DbDumpCmd = &execute_command($DbDumpCmd);
                    if($COMMAND_EXEC_SUCCESS != $retVal_DbDumpCmd)
                    {
                        $logging_obj->log("EXCEPTION","ha_db_sync:: Failed to take DB dump \n");
                        $message = "lastDBSyncStatusLog = 'Failed to take DB dump', lastDBSyncStatus = 'Failed'";
                        $success_flag = 0;
                    }

                    if($success_flag == 1)
                    {
                       	my $http_method = "POST";
						my $https_mode = ($AMETHYST_VARS->{'CS_HTTP'} eq 'https') ? 1 : 0;
						my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $cs_ip, 'PORT' => $cs_port);
						my $param = {'access_method_name' => "create_tar_for_passive", 'access_file' => "/create_tar_for_passive.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
						my $content = {'content' => ''};
						
						my $request_http_obj = new RequestHTTP(%args);
						my $response = $request_http_obj->call($http_method, $param, $content);

						my %file_info;
                        if ($response->{'status'})
                        {
							my $tarball_src_path = $response->{'content'};
							chomp $tarball_src_path;
							$file_info{$tarball_src_path} = $cxHARestorePath if $tarball_src_path ne '';       
                        }

                        $file_info{$cxDbBackupPath} = $cxHARestorePath;
                        my $passive_ip = $conn->sql_get_value('applianceNodeMap', 'nodeIp', "nodeRole='PASSIVE' AND applianceId='$cluster_name'");
                        my $response = &upload_file(\%file_info, $passive_ip, $cs_port);
                        if (! $response->{'status'})
                        {
                            $message = "lastDBSyncStatusLog = 'Failed to upload files to passive node', lastDBSyncStatus = 'Failed'";
                            $success_flag = 0;
                        }
                    }
                    
                    if(!$message && $success_flag == 1)
                    {
                        $message = "lastDBSyncStatusLog = 'DB Sync is successful', lastDBSyncStatus = 'Success', lastDBSyncTime=now()";
                    }
                    
                    if($message)
                    {
                        $input->{0}->{"queryType"} = "UPDATE";
                        $input->{0}->{"tableName"} = "applianceNodeMap";
                        $input->{0}->{"fieldNames"} = $message;
                        $input->{0}->{"condition"} = "nodeId = '$host_guid'";
                        $logging_obj->log("EXCEPTION","ha_db_sync:: input ::".Dumper($input));
                        if($input ne '') {	
                            &update_cs_db("ha_db_sync",$input);		
                        }
                    }
                }                
            }
            else  #HA Passive
            {                
                my $success_flag = 1;
                $cxDbRestorePath = $INSTALLATION_DIR."/ha/restore/".$DB_NAME.".HA.sql.bak";
                $DbRestoreCmd = "$MYSQL_RESTORE_CMD -u $DB_USER -p$DB_PASSWD $DB_NAME < $cxDbRestorePath 2>&1";

                my $retVal_DbRestoreCmd = 1;
                if(-e "$cxDbRestorePath")
                {
                    $retVal_DbRestoreCmd = system($DbRestoreCmd);
                }
                else
                {
                    return;
                }
                
                my $db_sync_job_state = $conn->sql_get_value('applianceCluster', 'dbSyncJobState', "applianceId='$cluster_name'");
                
                #
                # Update last Db sync timeStamp irrespective of DB restoration success or failure
                #
                
                #my @param_array = ($host_guid);
                #my $param_array_ref = \@param_array;
                #my $sqlUpdate = $conn->sql_query("UPDATE applianceNodeMap SET dbTimeStamp = now() WHERE nodeId = ?",$param_array_ref);
                
                if($COMMAND_EXEC_SUCCESS != $retVal_DbRestoreCmd)
                {
                    $logging_obj->log("EXCEPTION","ha_db_sync:: Failed to restore DB\n");
                    $message = "lastDBSyncStatusLog = 'Failed to restore db dump (at ".localtime().")' , lastDBSyncStatus = 'Failed'";
                    $success_flag = 0;
                }
                else
                {
                    if(-e "$cxDbRestorePath")
                    {
                        if(!unlink($cxDbRestorePath))
                        {
                            $logging_obj->log("EXCEPTION","ha_db_sync :: Failed to delete ($cxDbRestorePath) after restoring\n");
                            $success_flag = 0;
                        }
                    }
                }
                
                my $tarball_src_path = $INSTALLATION_DIR."/ha/restore/haSyncFiles.tar.gz";
                if(-e $tarball_src_path && ($success_flag == 1))
                {
                    my $tarball_dest_path = $INSTALLATION_DIR."/var/haSyncFiles.tar.gz";
                    my $return_val = Utilities::move_file($tarball_src_path, $tarball_dest_path);
                   
                    if(-e "$tarball_dest_path")
                    {
                        my $give_permission = &execute_command("chmod 777 $tarball_dest_path");
                        my $untar_tarball = &execute_command("tar -zpxf $tarball_dest_path -C /");

                        my $remove_tarball = &execute_command("rm -f $tarball_dest_path");
                    }
                }
                if($message eq '' && $success_flag == 1)
                {
                    $message = "lastDBSyncStatusLog = 'DB Sync is successful', lastDBSyncStatus = 'Success', lastDBSyncTime=now()";
                }
                
                # && $message && ($db_sync_job_state eq 'ENABLED' || $retVal_DbRestoreCmd != 0)
                if($db_sync_job_state eq 'ENABLED' || $db_sync_job_state eq '')
                {
                    $input->{0}->{"queryType"} = "UPDATE";
                    $input->{0}->{"tableName"} = "applianceNodeMap";
                    $input->{0}->{"fieldNames"} = $message;
                    $input->{0}->{"condition"} = "nodeId = '$host_guid'";
                    $logging_obj->log("EXCEPTION","ha_db_sync:: input ::".Dumper($input));
                    if($input ne '') {	
                        &update_cs_db("ha_db_sync",$input);		
                    }
                }
            }
            $logging_obj->log("EXCEPTION","ha_db_sync:: message:: $message \n host_guid::$host_guid \n");

        }
        return;
    };
	if($@)
	{
		$logging_obj->log("EXCEPTION","ha_db_sync:: ERROR:: $@\n");
	}
}

sub execute_command
{
    my ($command) = @_;
    my $retVal;
    eval 
    {
       if (Utilities::isLinux()) 
       {
           $retVal = system($command);
       }
       else
       {    
           #chdir("C:\\Program Files\\MySQL\\MySQL Server 4.1\\bin");
           $retVal = system($command);    
       }
    };
    if ($@)
    {
        print "Error executing $command: $@\n";
		$logging_obj->log("EXCEPTION","execute_command:: returng -2 failure \n");
        return -2;
    }
    else
    {
		$logging_obj->log("EXCEPTION","execute_command:: returng 0 success \n");
        return 0;
    }
}

sub get_pair_count
{
	my($conn,$sharedDeviceId) = @_;
	$logging_obj->log("DEBUG", "get_pair_count");
	my @par_array = ($sharedDeviceId);
	my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
						"SELECT DISTINCT ".
						" Phy_Lunid, destinationHostId, destinationDeviceName ".
						" FROM ".
						" srcLogicalVolumeDestLogicalVolume  ".
						" WHERE ".
						" Phy_Lunid = ?",$par_array_ref);
	
    # 1-to-n 
    $logging_obj->log("DEBUG","get_one_to_n_count ::  ".$conn->sql_num_rows($sqlStmnt));
    return $conn->sql_num_rows($sqlStmnt);
}

sub get_delete_pending_one_to_n
{
	my($conn,$sharedDeviceId) = @_;
	$logging_obj->log("DEBUG", "get_delete_pending_one_to_n");
	my @par_array = ($sharedDeviceId);
	my $par_array_ref = \@par_array;
	my $StatesNotOkToProcessDeletion = Fabric::Constants::SOURCE_DELETE_DONE .
        "," . Fabric::Constants::PS_DELETE_DONE;
	my $sqlStmnt = $conn->sql_query(
						"SELECT DISTINCT ".
						" Phy_Lunid, destinationHostId, destinationDeviceName ".
						" FROM ".
						" srcLogicalVolumeDestLogicalVolume  ".
						" WHERE ".
						" Phy_Lunid = ? AND ".
						" replication_status ".
						" IN($StatesNotOkToProcessDeletion)",$par_array_ref);
    $logging_obj->log("DEBUG","get_delete_pending_one_to_n ::  ".$conn->sql_num_rows($sqlStmnt));
	
    # 1-to-n 
    return $conn->sql_num_rows($sqlStmnt);
}

sub deleteStaleEntries
{
	my($srcHostId,$sharedDeviceId,$destHostId,$destDevName,$processServerId, $tot_pair_count, $del_pair_count, $api_output) = @_;
	my $input_params;
	my @batch;
	my $app_lun_data = $api_output->{'appliance_node_data'};
	my $lv_data = $api_output->{'lv_data'};
	my $i=0;
	if ( $tot_pair_count == 1 or ($tot_pair_count == $del_pair_count) ) 
	{	
		$logging_obj->log("DEBUG","deleteStaleEntries ::  get_one_to_n_count and get_delete_pending_one_to_n check passed");
		$logging_obj->log("DEBUG"," deleteStaleEntries :: Array execution cleanup and others\n");
		
		my $atLunId = $app_lun_data->{$srcHostId."!!".$sharedDeviceId}->{'applianceTargetLunMappingId'};
		
		if($atLunId)
		{
			my $at_lun_del_command = "/usr/local/InMage/Vx/bin/inm_dmit --op=del_lun --lun_name=".$atLunId;
			my $at_lun_del_op = `$at_lun_del_command`;
			if($at_lun_del_op !~ /success/g)
			{
				$logging_obj->log("EXCEPTION"," deleteStaleEntries :: AT Lun cleaning failed using command : $at_lun_del_command\nreturned $at_lun_del_op");
			}
			else
			{
				$logging_obj->log("DEBUG"," deleteStaleEntries :: AT Lun cleaned using command : $at_lun_del_command\nreturned $at_lun_del_op");
			}
		}
		else
		{
			$logging_obj->log("EXCEPTION"," deleteStaleEntries :: Not able to get AT Lun from the table");
		}
		$input_params->{$i}->{'queryType'} = "DELETE";
		$input_params->{$i}->{"tableName"} = "hostApplianceTargetLunMapping";
		$input_params->{$i}->{"fieldNames"} = "";
		$input_params->{$i}->{"condition"} = "sharedDeviceId = '$sharedDeviceId'";
		$i++;
		push(@batch, $input_params);
		
		$input_params->{$i}->{'queryType'} = "DELETE";
		$input_params->{$i}->{"tableName"} = "sharedDevices";
		$input_params->{$i}->{"fieldNames"} = "";
		$input_params->{$i}->{"condition"} = "sharedDeviceId = '$sharedDeviceId'";
		$i++;
		push(@batch, $input_params);
		
		$input_params->{$i}->{'queryType'} = "UPDATE";
		$input_params->{$i}->{"tableName"} = "srcLogicalVolumeDestLogicalVolume";
		$input_params->{$i}->{"fieldNames"} = "replication_status = '".Fabric::Constants::SOURCE_DELETE_DONE."'";
		$input_params->{$i}->{"condition"} = "destinationHostId = '$destHostId' AND destinationDeviceName='$destDevName' AND replication_status = '".Fabric::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING."'";
		$i++;
		push(@batch, $input_params);
		
		$input_params->{$i}->{'queryType'} = "DELETE";
		$input_params->{$i}->{"tableName"} = "arrayLunMapInfo";
		$input_params->{$i}->{"fieldNames"} = "";
		$input_params->{$i}->{"condition"} = "sourceSharedDeviceId = '%".$sharedDeviceId."%' OR sharedDeviceId like '%".$sharedDeviceId."%'";		
		$i++;
		push(@batch, $input_params);		
	}
	if($destDevName ne "P")
	{
		my $destSharedDevId;
		$destSharedDevId = $lv_data->{$destHostId."!!".$destDevName}->{'Phy_Lunid'};
		my $del_ab_sql;
		if($destSharedDevId)
		{
			$input_params->{$i}->{'queryType'} = "DELETE";
			$input_params->{$i}->{"tableName"} = "arrayLunMapInfo";
			$input_params->{$i}->{"fieldNames"} = "";
			$input_params->{$i}->{"condition"} = "sharedDeviceId like '%".$destSharedDevId."%' and lunType = 'TARGET'";		
		}
		else
		{
			$logging_obj->log("EXCEPTION"," deleteStaleEntries :: Couldnt get the scsi Id from logicalVolumes table for device : $destDevName\n");
			my @destIds = split("/",$destDevName);
			
			$input_params->{$i}->{'queryType'} = "DELETE";
			$input_params->{$i}->{"tableName"} = "arrayLunMapInfo";
			$input_params->{$i}->{"fieldNames"} = "";
			$input_params->{$i}->{"condition"} = "sharedDeviceId like '%".$destIds[3]."%' and lunType = 'TARGET'";			
		}
		push(@batch, $input_params);
	}
	$logging_obj->log("EXCEPTION","This is a DEBUG message. deleteStaleEntries :: batch \n".Dumper(\@batch));
	return @batch;
}

sub check_array_type
{
	my ($conn,$sh_dev_id) = @_;
	$logging_obj->log("DEBUG","check_array_type :: came inside");
	my $sql = "	SELECT  
					ap.solutionType,                                         
				FROM
					applicationPlan ap,
					srcLogicalVolumeDestLogicalVolume slvdlv
				WHERE
					slvdlv.planId = ap.planId
				AND
					slvdlv.Phy_Lunid = ?";
	my @par_array = ($sh_dev_id);
	my $par_array_ref = \@par_array;
	$logging_obj->log("DEBUG","check_array_type :: SQL :: $sql \nValues passed are $sh_dev_id, only\n");
	my $sql_stmt = $conn->sql_query($sql,$par_array_ref);
	my ($sol_type, $app_type, $appl_type);
	my @array = (\$sol_type, \$app_type, \$appl_type);
	$conn->sql_bind_columns($sql_stmt, @array);
	while ($conn->sql_fetch($sql_stmt)) 
	{
		if($sol_type eq Fabric::Constants::ARRAY_SOLUTION) #&& $app_type == "BULK" && $appl_type = "BULK")
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
}

sub update_network_devices
{	
	my ($package, $extra_params) = @_;
	my %log_data;
	my $conn = $extra_params->{'conn'};
	my %telemetry_metadata = ('RecordType' => 'UpdateNetworkDevices');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
    $logging_obj->log("DEBUG","Entered update_network_devices function\n");
	eval
	{		
		my $CX_TYPE = $AMETHYST_VARS->{'CX_TYPE'};
		my $conn_local = new Common::DB::Connection();
		my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
		my $ipaddress = $AMETHYST_VARS->{'CS_IP'};
		my $host_name = `hostname`;
		chomp $host_name;
		
		my $count = $conn->sql_get_value('hosts', 'count(id)', "id='$host_guid'");	
		#Fix for multi-tenancy
		my $account_id = $AMETHYST_VARS->{'ACCOUNT_ID'};
		my $account_type = $AMETHYST_VARS->{'ACCOUNT_TYPE'};
		if($account_id == '')
		{
			$account_id = Common::Constants::INMAGE_ACCOUNT_ID;
			$account_type = Common::Constants::INMAGE_ACCOUNT_TYPE;
		}
		$logging_obj->log("DEBUG","update_network_devices:: account_id:".$AMETHYST_VARS->{'ACCOUNT_ID'}.", account_type: ".$AMETHYST_VARS->{'ACCOUNT_TYPE'}.", account_id: $account_id, account_type: $account_type\n");		
		
		my (@osname,@osversion,@build,$osname,$osversion,$buildnumber, $os_type,$os_flag);
		use if Utilities::isWindows(), "Win32";
		if (Utilities::isWindows())
		{			
			eval
			{
			   @osname = Win32::GetOSName();
			   #@osversion = Win32::GetOSVersion();
			   #@build = Win32::BuildNumber();
			};
			if ($@)
			{
			   $logging_obj->log("EXCEPTION","update_network_devices ::Unable to determine OS name/version for $host_name\n");
			}
			$osname = join(" ", @osname);
			#$osversion = " Version ".join ("", @osversion);
			#$buildnumber = " Build ".join(" ", @build);
			#$os_type = $osname.$buildnumber;
			$os_type = $osname;

			#
			# Update OS flag to PS_WINDOWS if the osType
			# is windows
			#
			$os_flag = Common::Constants::PS_WINDOWS;
		}
		else
		{
			$os_type = `uname -a 2>&1`;
			chomp $os_type;

			#
			# Update OS flag to PS_LINUX is the osType
			# is linux.
			#
			$os_flag = 2;
		}
		
		my $set_values_host;
		if($CX_TYPE eq 3 || $CX_TYPE eq 1)
		{
			if( $count == 0)
			{
				$set_values_host  = "INSERT
									 INTO
										hosts
											(id,
											osType,
											osFlag,
											name,
											ipaddress,
											accountId,
											accountType,
											csEnabled)
									VALUES
											(\'$host_guid\',
											\'$os_type\',
											$os_flag,
											\'$host_name\',
											\'$ipaddress\',
											'$account_id',
											'$account_type',
											1)";				
			}
			else
			{
                my $ipaddress_update = ($CX_TYPE eq 1) ? ", ipaddress = \'$ipaddress\' " : "";
				$set_values_host  = "UPDATE
										hosts
									SET
										osType = \'$os_type\',
										osFlag = \'$os_flag\',
										name = \'$host_name\',
										accountId = \'$account_id\',
										accountType = \'$account_type\',
										csEnabled = 1 $ipaddress_update
									WHERE
										id = '$host_guid'";
			}		
			$conn_local->sql_query($set_values_host);		
			$logging_obj->log("DEBUG","update_network_devices :: Query :: $set_values_host \nexecuted");
			
			my $custom_build_type = $AMETHYST_VARS->{'CUSTOM_BUILD_TYPE'};
			if($custom_build_type ne '3')
			{
				my %args = (
						'conn' => $conn,
						'host_guid' => $host_guid,					
						'cs_ip' => $ipaddress,
						'cx_type' => $CX_TYPE,
						'base_dir' => $BASE_DIR
						);
				#
				# Create a new object of the class and initialize
				# it with some values
				#
				my $psreg_obj = new ProcessServer::Register(%args);
				$psreg_obj->update_cs_network_devices();
			}
		}
	};
	if($@)
	{
		$log_data{"RecordType"} = "UpdateNetworkDevices";
		$log_data{"ErrorMessage"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
	}	
}

sub getOnlyPs 
{
	my ($conn) = @_;
	my %Hosts;
	my $sql = "SELECT id,name  FROM $HOSTS_TBL where sentinelEnabled = 0 and outpostAgentEnabled = 0 and filereplicationAgentEnabled = 0 and processServerEnabled = 1 ";
	my $result = $conn->sql_get_hash($sql,'id','name');	
	if (defined $result)
	{
		%Hosts = %$result;
	}
    return %Hosts;
}

sub monitor_mbr_info
{
	$logging_obj->log("DEBUG","monitor_mbr_info \n");
	eval
	{
		my $response;
		my $http_method = "POST";
		my $https_mode = ($AMETHYST_VARS->{'CS_HTTP'} eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $AMETHYST_VARS->{'CS_IP'}, 'PORT' => $AMETHYST_VARS->{'CS_PORT'});
		my $param = {'access_method_name' => "delete_mbr_files", 'access_file' => "/delete_mbr_files.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'content' => ''};
		
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);
		
		$logging_obj->log("DEBUG","Response::".Dumper($response));
		if (!$response->{'status'})
		{
			$logging_obj->log("EXCEPTION","Failed to post the details for monitor_mbr_info");
		}
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","update_mbr_info: $@");
		return;
	}	
}

sub delete_mbr_files
{
	my ($host_id , $logs_from) = @_;
	my $i = 1;
	my $mbr_path;
	my %mbr_content;
	if (Utilities::isWindows()) 
	{
		$mbr_path = $BASE_DIR."\\admin\\web\\SourceMBR\\".$host_id."\\";
	}
	else
	{
		$mbr_path = $BASE_DIR."/admin/web/SourceMBR/".$host_id."/";
	}
	#$logging_obj->log("EXCEPTION","monitor_mbr_info mbr_path $mbr_path \n");
	if( opendir (DIRHANDLE, $mbr_path) )
	{
		while ( my $file = readdir (DIRHANDLE) )
		{
			my @file_contents = split('_',$file);
			if($logs_from ne "all")
			{
				if($file_contents[1] > $logs_from)
				{
					$mbr_content{'$file_contents[1]'} = $mbr_path.$file;
				}
			}
			else
			{
				$mbr_content{'$file_contents[1]'} = $mbr_path.$file;
			}
		}
		closedir (DIRHANDLE);
	}

	my $mbr_lenth = scalar keys %mbr_content;
	foreach my $key (sort (keys(%mbr_content))) 
	{
		if($i < $mbr_lenth)
		{
			unlink($mbr_content{$key});
		}
		$i++;
	}
}

#
# This function will fetch and return the required data from transbandSettings table
#
sub getTransbandSettingsData
{
	my ($conn, $outputColumn, $inputColumn, $columnValue) = @_;
	my $transbandRef;
	my $sql = "SELECT ".$outputColumn." FROM transbandSettings WHERE ".$inputColumn."='".$columnValue."'";
	$transbandRef = $conn->sql_exec($sql);

	return $transbandRef;
}

sub monitor_request_data
{
	my ($conn) = @_;
	$logging_obj->log("DEBUG","Entered monitor_request_data \n");
	my ($state,$apiRequestId);
	my ($idCount);
	my @apiArray = (\$apiRequestId);
	
	my $selectSql = "SELECT distinct apiRequestId FROM apiRequest";
	$logging_obj->log("DEBUG"," monitor_request_data::$selectSql\n");
	my $sqlStmt = $conn->sql_query($selectSql);
	$idCount = $conn->sql_num_rows($sqlStmt);
	$logging_obj->log("DEBUG","Entered monitor_request_data::idCount:$idCount\n");
	if($idCount > 0)
	{
		$conn->sql_bind_columns($sqlStmt, @apiArray);
		while($conn->sql_fetch($sqlStmt))
		{
			my @par_array = ($apiRequestId);
			my $par_array_ref = \@par_array;
			my $sql = "SELECT distinct state FROM apiRequestDetail WHERE apiRequestId = ?";
			$logging_obj->log("DEBUG"," monitor_request_data::$sql\n");
			my $sth = $conn->sql_query($sql,$par_array_ref);
			my $stateCount = $conn->sql_num_rows($sth);
			$logging_obj->log("DEBUG"," monitor_request_data::stateCount:$stateCount for apiRequestId $apiRequestId\n");	
			my @stateArray = (\$state);
			$conn->sql_bind_columns($sth, @stateArray);
			if($stateCount > 1)
			{
				
				# state : 1=pending, 2=in progress, 3=success, 4=failure
				while($conn->sql_fetch($sth))
				{
					$logging_obj->log("DEBUG"," monitor_request_data::got state:$state for apiRequestId $apiRequestId");
					if($state == $FAILURE)
					{
						# if request state is failure(4) then need to update apiRequest with failure.
						$logging_obj->log("DEBUG"," monitor_request_data::update failure state");
						my @par_array = ($state,$apiRequestId);
						my $par_array_ref = \@par_array;
						my $sqlStmnt = $conn->sql_query(
						"update apiRequest ".
						" set state = ? ".
						"where apiRequestId = ? ",$par_array_ref);
					}
				}
			}
			elsif($stateCount == 1)
			{
				$conn->sql_fetch($sth);
				$logging_obj->log("DEBUG"," monitor_request_data::got common state $state for apiRequestId $apiRequestId ");
				if(($state == $IN_PROGRESS) || ($state == $SUCCESS) || ($state == $FAILURE))
				{
					$logging_obj->log("DEBUG"," monitor_request_data::updating state in apiRequest:$state");
					my @par_array = ($state,$apiRequestId);
					my $par_array_ref = \@par_array;
					my $sqlStmnt = $conn->sql_query(
					"update apiRequest ".
					" set state = ? ".
					"where apiRequestId = ? ",$par_array_ref);
				}
				
			}	
		}
	}
}


sub get_host_guid {
    
    my ($host_guid,$conn) = @_;    

	my $host_id;
	$host_id = $conn->sql_get_value('hosts', 'id', "id='$host_guid'");

	if($host_id)
	{
		return $host_guid ;
	}
	else
	{
		return;
	}
}

sub getConnectionForPs
{
	my $db_name = $AMETHYST_VARS->{'DB_NAME'};
	my $db_passwd = $AMETHYST_VARS->{'DB_PASSWD'};
	my $db_host = $AMETHYST_VARS->{'PS_CS_IP'};
	my %args = ( 'DB_NAME' => $db_name,
			'DB_PASSWD'=> $db_passwd,
			'DB_HOST' => $db_host
			);
	my $args_ref = \%args;
	$conn = new Common::DB::Connection($args_ref);
	return $conn;
}

sub getPSPairDetails
{
	my ($data_hash, @tmIds) = @_;
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
	my $pair_data = $data_hash->{'pair_data'};
	my $plan_data = $data_hash->{'plan_data'};
	my $host_data = $data_hash->{'host_data'};
	my $lv_data = $data_hash->{'lv_data'};
	my $cluster_list = $data_hash->{'cluster_data'};
	my @clusterId;
	my @final_clutser_id;
	my $key_value;
	my @keys;
	my @cluster_array;
	
	$PS_HOST_NAME = $host_data->{$host_guid}->{'name'};
	$CS_HOST_NAME = TimeShotManager::getCSHostNameForPS($data_hash);
	
	foreach my $pair_id (keys %{$pair_data})
	{
		my @clusterids;	
		@clusterId  = ();
		
		my @actual_pair_array = split(/\!\!/,$pair_id);								
		my $org_pair_id = $actual_pair_array[0];
		
		my $key = $pair_data->{$pair_id}->{'sourceHostId'}."!!". $pair_data->{$pair_id}->{'sourceDeviceName'};
		my $tmid = $lv_data->{$key}->{'tmId'};
		my $far_line_protected = $lv_data->{$key}->{'farLineProtected'};

		if((grep  $_ eq $tmid,@tmIds ) && $far_line_protected == 1) {
				
			foreach my $cluster_id (keys %{$cluster_list}) 
			{
				foreach my $cluster_data (values %{$cluster_list->{$cluster_id}})
				{
					my @cluster_array = split(/\!\!/,$cluster_data);								
					push(@clusterids,$cluster_array[0]);								
				}
				my %entry;	
				if(grep {$_ eq $key} $cluster_list->{$cluster_id})
				{ 
					@clusterId = grep { ! $entry{ $_ }++ } @clusterids;
				}
				@clusterids = ();
			}
			
			if(@clusterId)
			{
				foreach my $clustid (@clusterId) 
				{
					$key_value = $clustid ."!&*!". $pair_data->{$pair_id}->{'sourceDeviceName'} ."!&*!". $pair_data->{$pair_id}->{'destinationHostId'} ."!&*!". $pair_data->{$pair_id}->{'destinationDeviceName'} ."!&*!". $org_pair_id."!&*!". 1;
					push (@keys, $key_value);
				}
				@final_clutser_id = @clusterId;
			}
			else
			{
				$key_value = $pair_data->{$pair_id}->{'sourceHostId'} ."!&*!". $pair_data->{$pair_id}->{'sourceDeviceName'} ."!&*!". $pair_data->{$pair_id}->{'destinationHostId'} ."!&*!". $pair_data->{$pair_id}->{'destinationDeviceName'} ."!&*!". $org_pair_id ."!&*!". 0;
				push (@keys, $key_value);
			}
		}
	}
	my %data;	
	my @final_keys = grep { ! $data{ $_ }++ } @keys;
	return (\@final_keys,\@final_clutser_id);		
}

sub getKeysOfProtectedSourceDestHosts
{
  	my ($conn,$source,$is_fork) = @_;
	my @keys;
	my $key_value;
	# Get all hosts on which Initial Sync or Differences could possibly
	# be initiated from. This will let us handle 1.5 differential 
	# file compression and rename
	
	my %pair_hash = &getPairDetails($conn,\$source);
	
	my($diff_lv_hash) = &getProtectedDeviceInfo($conn,\%pair_hash,$is_fork);
	my %diff_lv_hash = %$diff_lv_hash;	
	my %cluster_list = getClusterHash($conn);
	my $dest_info;	
	my @clusterId;
	my @final_clutser_id;
	foreach my $pair_id (keys %pair_hash)
	{
		foreach my $pair (keys %{$pair_hash{$pair_id}})
		{
			foreach my $lv_device (keys %diff_lv_hash)
			{
				my @source_details = split(/\!\&\*\!/,$lv_device);
				if(exists $pair_hash{$pair_id}{$lv_device})
				{
					$dest_info = $pair_hash{$pair_id}{$lv_device};
					my @dinfo = split (/\!\&\*\!/, $dest_info); 
					my @clusterids;	
					@clusterId  = ();
			
					foreach my $cluster_id (keys %cluster_list) 
					{												
						foreach my $cluster_data (@{$cluster_list{$cluster_id}})
						{
							my @cluster_array = split(/\!\&\*\!/,$cluster_data);								
							push(@clusterids,$cluster_array[0]);								
						}
						
						my %entry;	
						if(grep {$_ eq $source_details[0].'!&*!'.$source_details[1]} @{$cluster_list{$cluster_id}})
						{ 
							@clusterId = grep { ! $entry{ $_ }++ } @clusterids;
						}
						@clusterids = ();
					}
					if(@clusterId)
					{
						foreach my $clustid (@clusterId) 
						{
							$key_value = $clustid ."!&*!". $source_details[1] ."!&*!". $dinfo[0] ."!&*!". $dinfo[1] ."!&*!". $pair_id."!&*!". $dinfo[3]."!&*!". $dinfo[4]."!&*!". $dinfo[5]."!&*!". $dinfo[6]."!&*!". $dinfo[7]."!&*!". $dinfo[8]."!&*!". $dinfo[9]."!&*!". $dinfo[10]."!&*!". $dinfo[11]."!&*!". 1;
						    push (@keys, $key_value);
						}
						@final_clutser_id = @clusterId;
					}
					else
					{
						$key_value = $source_details[0] ."!&*!". $source_details[1] ."!&*!". $dinfo[0] ."!&*!". $dinfo[1]."!&*!". $pair_id."!&*!". $dinfo[3]."!&*!". $dinfo[4]."!&*!". $dinfo[5]."!&*!". $dinfo[6]."!&*!". $dinfo[7]."!&*!". $dinfo[8]."!&*!". $dinfo[9]."!&*!". $dinfo[10]."!&*!".  $dinfo[11];
					    push (@keys, $key_value);
					}	
						
				}
			}
		}
	}
	my %data;	
	my @final_keys = grep { ! $data{ $_ }++ } @keys;
	$logging_obj->log("DEBUG","getKeysOfProtectedSourceDestHosts final_keys ".Dumper(\@final_keys)."\n");
	return (\@final_keys, \@final_clutser_id);
}

sub get_db_connection
{
	my $db_name = $AMETHYST_VARS->{'DB_NAME'};
	my $db_passwd = $AMETHYST_VARS->{'DB_PASSWD'};
	my $cluster_ip = $AMETHYST_VARS->{'CLUSTER_IP'};
	my $db_host = $AMETHYST_VARS->{'DB_HOST'};

	my %args = ( 'DB_NAME' => $db_name,
			'DB_PASSWD'=> $db_passwd,
			'DB_HOST' => $db_host
			);
	my $args_ref = \%args;
	my $conn = new Common::DB::Connection($args_ref);
	
	return $conn;
}

#  
# Function Name: isPrimaryActive  
# Description: 
#    Return count of rows if CX is Primary And Acitve Node
# Parameters: None 
# Return Value: None
#
sub isPrimaryActive
{
	my ($conn) = @_;
	my $count = 0;
	my ($sth,$sql);
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};	
	
	# If HA node
	if(exists($AMETHYST_VARS->{'CLUSTER_IP'}))
	{
		# Check if remote CX exists
		my $remote_cx_count = $conn->sql_get_value('standByDetails', 'count(appliacneClusterIdOrHostId)', "appliacneClusterIdOrHostId = '$host_guid'");
			
		# If Not remote CX Check if Active	
		if($remote_cx_count == 0)
		{
			$sql = "SELECT
							nodeId							
						FROM
							applianceNodeMap 							
						WHERE
							nodeRole = 'ACTIVE' AND
							nodeId = '$host_guid'";
		}
		# If remote CX exists check Primary and Active
		else
		{
			$sql = "SELECT
								a.appliacneClusterIdOrHostId,
								b.nodeRole,
								a.role
							FROM
								standByDetails a,
								applianceNodeMap b
							WHERE 
								b.nodeId = '$host_guid' AND
								((a.appliacneClusterIdOrHostId = b.applianceId) OR (a.appliacneClusterIdOrHostId = b.nodeId) ) AND 
								((a.role = 'PRIMARY') OR (b.nodeRole = 'ACTIVE'))";
		}					
	}
	else
	{
		$sql = "SELECT
							appliacneClusterIdOrHostId							
						FROM
							standByDetails 							
						WHERE
							role = 'PRIMARY' AND
							appliacneClusterIdOrHostId = '$host_guid'";
				
	}
	$logging_obj->log("DEBUG","isPrimaryActive :: SQL ::$sql");
	$sth = $conn->sql_query($sql);
	$count = $conn->sql_num_rows($sth);
	return $count;
}

#  
# Function Name: isStandAloneCX  
# Description: 
#    Normal CX
# Parameters: None 
# Return Value: None
#
sub isStandAloneCX
{
	my ($conn) = @_;	
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
	
	if(exists($AMETHYST_VARS->{'CLUSTER_IP'}))
	{
		return 0;
	}
	else
	{
		my $remote_cx_count = $conn->sql_get_value('standByDetails', 'count(*)', "appliacneClusterIdOrHostId = '$host_guid'");
		
		if($remote_cx_count != 0)
		{
			return 0;
		}
	}
	return 1;
}

#  
# Function Name: monitor_queued_pairs  
# Description: 
#    Triggers the Queued Pairs, if any, in a plan
# Parameters: None 
# Return Value: None
#
sub monitor_queued_pairs
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	
	my $sql = "SELECT
				planId,
				batchResync
			   FROM
				applicationPlan 
			   WHERE
				planId > '1'";
					
	#$logging_obj->log("INFO","monitor_queued_pairs :: $sql");
	my $rs = $conn->sql_exec($sql);

	foreach my $row (@{$rs})
	{
		my $plan_id = $row->{'planId'};
		my $batch_resync = $row->{'batchResync'};
		#$logging_obj->log("INFO","monitor_queued_pairs :: batch_resync::$batch_resync");
		if($batch_resync > 0) {
			my $pair_count = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'count(*)', "planId='$plan_id' AND deleted='0'");
			
			#$logging_obj->log("INFO","monitor_queued_pairs :: pair_count::$pair_count");
			if($pair_count > 0) {
				###
				# Check if all the pairs are in Queued in a plan
				###
				my $queued_pair_count = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'count(*)', "planId='$plan_id' AND executionState IN ('2') AND deleted='0'");
				
				#$logging_obj->log("INFO","monitor_queued_pairs :: queued_pair_count::$queued_pair_count");				
				###
				# Check for No pairs and are resync-step1 and plan has queued pairs
				###
				my $num_pairs_in_step1 = 1;
				if($queued_pair_count) {
					$num_pairs_in_step1 = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'count(*)', "planId='$plan_id' AND executionState='1' AND deleted='0' AND isQuasiflag='0'");
				}
				#$logging_obj->log("INFO","monitor_queued_pairs :: num_pairs_in_step1::$num_pairs_in_step1");
				if(($pair_count == $queued_pair_count) or ($num_pairs_in_step1 == 0)) {
					my $update = "UPDATE srcLogicalVolumeDestLogicalVolume SET executionState='1' WHERE planId='$plan_id' AND executionState IN ('2') AND deleted='0' LIMIT $batch_resync";
					my $update_rs = $conn->sql_query($update);
				}
				
				###
				# Check for 1-N case, update executionState to 2 if there are no Primary Pairs
				###
				
				my $one_to_n_sql = "SELECT  sourceHostId,sourceDeviceName,destinationHostId,destinationDeviceName,oneToManySource from srcLogicalVolumeDestLogicalVolume WHERE planId='$plan_id' AND executionState IN ('3') AND deleted='0'";
				#$logging_obj->log("INFO","monitor_queued_pairs :: one_to_n_sql::$one_to_n_sql");
				my $result = $conn->sql_exec($one_to_n_sql);
				if(defined $result)
				{
					foreach my $ref (@{$result})
					{
						my $src_id = $ref->{'sourceHostId'};
						my $src_device = TimeShotManager::formatDeviceName($ref->{'sourceDeviceName'});
						my $trg_id = $ref->{'destinationHostId'};
						my $trg_device = TimeShotManager::formatDeviceName($ref->{'destinationDeviceName'});
						my $is_one_to_many = $ref->{'oneToManySource'};
						
						my $count = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'count(*)', "sourceHostId='$src_id' AND sourceDeviceName='$src_device' AND executionState NOT IN ('3','4') AND deleted='0' AND planId='$plan_id'");
						
						#$logging_obj->log("INFO","monitor_queued_pairs :: ONE TO N COunt::$count");
						if(!$count) {
							my $update_sql = "UPDATE srcLogicalVolumeDestLogicalVolume SET executionState='2' WHERE sourceHostId='$src_id' AND sourceDeviceName='$src_device' AND destinationHostId='$trg_id' AND destinationDeviceName='$trg_device' AND executionState IN ('3') AND deleted='0' AND planId='$plan_id' AND oneToManySource='1' LIMIT 1";
							my $update_rs = $conn->sql_query($update_sql);
						}
					}
				}
				
			}
		}
	}	

	return 1;
}

#  
# Function Name: auto_resync_on_resync_set  
# Description: 
#    Monitors auto resync 
# Parameters: None 
# Return Value: None
#
sub auto_resync_on_resync_set
{
	my ($package, $extra_params) = @_;
	my $data_hash = $extra_params->{'data_hash'};
	$logging_obj->log("DEBUG","Entered auto_resync_on_resync_set function\n");
	&TimeShotManager::autoResyncOnResyncSet($data_hash);
	$logging_obj->log("DEBUG","End of auto_resync_on_resync_set function\n");
}

#  
# Function Name: isSecondaryPassive  
# Description: 
#    Return count of rows if CX is Passive Node
# Parameters: None 
# Return Value: None
#
sub isSecondaryPassive
{
	my ($conn) = @_;
	my $count = 0;
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};	
	
	# If HA node
	if(exists($AMETHYST_VARS->{'CLUSTER_IP'}))
	{
		$count = $conn->sql_get_value('applianceNodeMap', 'count(nodeId)', "nodeRole = 'PASSIVE' AND nodeId = '$host_guid'");
	}

	return $count;
}

#  
# Function Name: auto_correct_report_data  
# Description: 
#    Removes report data with future date from db
# Parameters: None 
# Return Value: None
#
sub auto_correct_report_data
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	my $correct_report_data_txt;
	my $check_report_date_txt;
	
	eval
	{
		my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};

		if (Utilities::isWindows())
		{
			$correct_report_data_txt = $AMETHYST_VARS->{'INSTALLATION_DIR'}."\\var\\correct_report_data.txt";
			$check_report_date_txt = $AMETHYST_VARS->{'INSTALLATION_DIR'}."\\var\\check_report_date.txt";
		}
		else
		{
			$correct_report_data_txt = $AMETHYST_VARS->{'INSTALLATION_DIR'}."/var/correct_report_data.txt";
			$check_report_date_txt = $AMETHYST_VARS->{'INSTALLATION_DIR'}."/var/check_report_date.txt";
		}

		my $sql_curr_time = "SELECT unix_timestamp(DATE_ADD(CURDATE(),INTERVAL 1 DAY)) as 'current_timestamp'";
			
		my $rs_curr_time = $conn->sql_query($sql_curr_time);
		my $ref = $conn->sql_fetchrow_hash_ref($rs_curr_time);
		my $current_timestamp;
		if ($ref->{"current_timestamp"})
		{
			$current_timestamp = $ref->{"current_timestamp"};
		}
		
		if (!-e $check_report_date_txt)
		{	
			if(!open(FH,">$check_report_date_txt"))
			{
				$logging_obj->log("EXCEPTION","auto_correct_report_data :: 1 unable to open  : $check_report_date_txt  $!");
				return;
			}
			print FH $current_timestamp;
			close(FH);
			return;
		}
		else
		{
			if(!open(FR,"<$check_report_date_txt"))
			{
				$logging_obj->log("EXCEPTION","auto_correct_report_data :: 2 unable to open  : $check_report_date_txt  $!");
				return;
			}
			else
			{			
				my @file_content = <FR>;
				close (FR);

				my $previous_timestamp = $file_content[0];
				
				if($current_timestamp < $previous_timestamp)
				{
					if (!-e $correct_report_data_txt)
					{	
						#send alert
						my $err_summary = "System Time is modified";
						my $err_message = "Configuration Sever time is moved backward. Please reset to correct time to avoid data loss from Data Base";
						my $err_id = $host_guid;
						my $host_id = $host_guid;
						
						&Messenger::add_error_message ($conn, $err_id, "PROTECTION_ALERT", $err_summary, $err_message, $host_id);
						
						if(!open(FH,">$correct_report_data_txt"))
						{
							$logging_obj->log("EXCEPTION","auto_correct_report_data :: 3 unable to open  : $correct_report_data_txt  $!");
							return;
						}
						close(FH);
						return;
					}
					else
					{
						
						$logging_obj->log("EXCEPTION"," auto_correct_report_data :: call to update_db_tables, delete records if db holds date greater than : $current_timestamp");
						
						unlink($correct_report_data_txt);
						
						my $sql_curr_time = "SELECT from_unixtime($current_timestamp) as 'current_datetime'";
			
						my $rs_curr_time = $conn->sql_query($sql_curr_time);
						my $ref = $conn->sql_fetchrow_hash_ref($rs_curr_time);
						my $current_datetime;
						if ($ref->{"current_datetime"})
						{
							$current_datetime = $ref->{"current_datetime"};
						}
						else
						{
							$logging_obj->log("EXCEPTION","auto_correct_report_data :: unable to fetch data using sql - $sql_curr_time");
						}
						my $audit_msg = "System time has moved back. RRD timestamp is updated with new timestamp";
						
						my %application_args = ('conn'=>$conn);
						my $application = new ConfigServer::Application(%application_args);
						
						$application->writeLog($audit_msg);
						&update_db_tables_rrd_timestamp($conn,$current_datetime);
						$logging_obj->log("EXCEPTION","auto_correct_report_data :: after update db tables");
						
						if(!open(FH,">$check_report_date_txt"))
						{
							$logging_obj->log("EXCEPTION","auto_correct_report_data :: 4 unable to open  : $check_report_date_txt  $!");
							return;
						}
						print FH $current_timestamp;
						close(FH);
						return;
					}
				}
				else
				{
					if(!open(FH,">$check_report_date_txt"))
					{
						$logging_obj->log("EXCEPTION","auto_correct_report_data :: 5 unable to open  : $check_report_date_txt  $!");
						return;
					}
					print FH $current_timestamp;
					close(FH);
				}
			}
		}
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","Error while removing future records from database : $@");
		return;
	}
}

sub update_db_tables_rrd_timestamp
{	
	my($conn,$temp_date) = @_;	
	
	#delete healthreport records from db
	my $delete_health_report = "DELETE FROM healthReport WHERE reportDate > '$temp_date'";								
	$logging_obj->log("EXCEPTION","update_db_tables delete_health_report :: $delete_health_report\n");	
	my $delete_health_report_rs = $conn->sql_query($delete_health_report);
	
	#delete trendingdata records from db
	my $delete_trending_data = "DELETE FROM trendingData WHERE reportHour > '$temp_date'";								
	$logging_obj->log("EXCEPTION","update_db_tables delete_trending_data :: $delete_trending_data\n");	
	my $delete_trending_data_rs = $conn->sql_query($delete_trending_data);
	
	#delete from profiling details
	my $delete_profiling_details = "DELETE FROM profilingDetails WHERE forDate > '$temp_date'";								
	$logging_obj->log("EXCEPTION","update_db_tables delete_profiling_details :: $delete_profiling_details\n");	
	my $delete_profiling_details_sql = $conn->sql_query($delete_profiling_details);
	
	#delete from policy violation events
	my $delete_policy_violation_events_details = "DELETE FROM policyViolationEvents WHERE eventDate > '$temp_date'";

	$logging_obj->log("EXCEPTION","update_db_tables delete_profiling_details :: $delete_profiling_details\n");
	my $delete_policy_violation_events_sql = $conn->sql_query($delete_policy_violation_events_details);
	
	#update last email/trap dispatch time for all users
	my $update_last_email_trap_dispatch_time = "UPDATE users SET last_dispatch_time='0',last_trap_dispatch_time = '0' ";

	$logging_obj->log("EXCEPTION","update_db_tables update_last_email_trap_dispatch_time :: $update_last_email_trap_dispatch_time\n");
	my $update_last_email_trap_dispatch_time_sql = $conn->sql_query($update_last_email_trap_dispatch_time);

	#run change rrd timestamp script to set the timestamp of rrds to current time		
	my $change_rrdtime_scriptpath;
	if(Utilities::isWindows())
	{
		$change_rrdtime_scriptpath = "$cs_installation_path\\home\\svsystems\\bin\\change_rrd_timestamp.pl";
	}
	else
	{
		$change_rrdtime_scriptpath = "/home/svsystems/bin/change_rrd_timestamp.pl";
	}
	
	my $run_rrd_status = `perl "$change_rrdtime_scriptpath"`;
	return;
}

sub get_ps_guids {

	my ($conn)=@_;
	my @ps_guids;
	my $sql = "SELECT id FROM hosts WHERE processServerEnabled='1'";
	my $result = $conn->sql_exec($sql);

	foreach my $sql_ref (@{$result})
	{		
		push(@ps_guids,$sql_ref->{'id'});
	}
	
	return @ps_guids;
}

#
# Returns the current status of the host
#
sub get_host_status
{
	my ($conn, $hostId) = @_;
	my ($sql,$res,$ref);
	my $agent_state = 0;
	eval
	{
		$agent_state = $conn->sql_get_value('hosts', 'agent_state', "id = '$hostId'");
		$logging_obj->log("DEBUG","agent_state::$agent_state");
		
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","get_host_status : $@");
		return 0;
	}
	
	return $agent_state;
}

##
#  
#Function Name: upload_file  
# Description: 
#     This function upload file from PS to CS
# Parameters:
#   Param 1 [IN]:$upload_file
#		   - This holds the file that ps has sent to cx.
#
##
sub upload_file
{
	my ($files, $remote_cx_ip, $remote_cx_port) = @_;
	my $response;
	
	my $cs_path = $cs_installation_path;
	$cs_path =~ s/\\/\\\\/g;

	eval
	{	
		my $ps_cs_ip = $remote_cx_ip eq "" ? $AMETHYST_VARS->{'PS_CS_IP'} : $remote_cx_ip;
		my $ps_cs_port = $remote_cx_port eq "" ? $AMETHYST_VARS->{'PS_CS_PORT'} : $remote_cx_port;
		my $http = ($AMETHYST_VARS->{'CX_TYPE'} == 1)?$AMETHYST_VARS->{'CS_HTTP'}:$AMETHYST_VARS->{'PS_CS_HTTP'};
		my $file_hash;
		my $i = 0;
		
		
		foreach my $src_file (keys %{$files})
		{
			next unless -e  $src_file;
			$file_hash->{"file_".$i}  = [$src_file];
			$file_hash->{"file_name_".$i}  = $src_file;
			$files->{$src_file} =~ s/$cs_path//g;
			$logging_obj->log("DEBUG","upload path:".$files->{$src_file});
			$file_hash->{"file_path_".$i} = $files->{$src_file};			
			$i++;
		}
		
		
		if (defined $file_hash)
		{
			my $http_method = "POST";
			my $https_mode = ($http eq 'https') ? 1 : 0;
			my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $ps_cs_ip, 'PORT' => $ps_cs_port);
			my $param = {'access_method_name' => "upload_file", 'access_file' => "/upload_file.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
			my $content = {'type' => 'multipart/form-data', 'content' => $file_hash};
					
			my $request_http_obj = new RequestHTTP(%args);
			$response = $request_http_obj->call($http_method, $param, $content);
		}
	};	
	if($@)
	{
		$logging_obj->log("EXCEPTION","upload_file : Could not upload : $@");
	}
	
	return $response;
}

sub call_cx_api
{	
	my $xml_output;
	
	eval
	{		
		my $cs_ip = ($AMETHYST_VARS->{'PS_CS_IP'}) ? $AMETHYST_VARS->{'PS_CS_IP'} : $AMETHYST_VARS->{'CS_IP'};
		my $cs_port = ($AMETHYST_VARS->{'PS_CS_PORT'}) ? $AMETHYST_VARS->{'PS_CS_PORT'} : $AMETHYST_VARS->{'CS_PORT'};		
        
        my $php_args = "HOST_GUID##".$AMETHYST_VARS->{'HOST_GUID'}."!##!IP##".$cs_ip."!##!PORT##".$cs_port."!##!PROTOCOL##".$HTTP;
		
		my $extension = "php";
		my $file_path = Utilities::makePath("/home/svsystems/admin/web/get_ps_settings.php");
		if(Utilities::isWindows())
		{
			$file_path = "$cs_installation_path\\home\\svsystems\\admin\\web\\get_ps_settings.php";
			$extension = $AMETHYST_VARS->{'PHP_PATH'};
		}
		my $command = $extension." ".'"'.$file_path.'"'." ". $php_args;
		$logging_obj->log("DEBUG","call_cx_api : Command : $command");
		$xml_output = `$command`;            
		$logging_obj->log("DEBUG","call_cx_api : Exit Code: ".$?);
	
		if($? != 0)
        {
            $logging_obj->log("EXCEPTION","call_cx_api : Failed to get the data");
            $xml_output = undef;
        }
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","call_cx_api : Could not get the cache : $@");
	}
    return $xml_output;	
}

sub call_cx_api_for_data
{	
	my($id, $call_option) = @_;

	my $xml_output;
	my $php_args;
	eval
	{		
		my $cs_ip = ($AMETHYST_VARS->{'PS_CS_IP'}) ? $AMETHYST_VARS->{'PS_CS_IP'} : $AMETHYST_VARS->{'CS_IP'};
		my $cs_port = ($AMETHYST_VARS->{'PS_CS_PORT'}) ? $AMETHYST_VARS->{'PS_CS_PORT'} : $AMETHYST_VARS->{'CS_PORT'};		
        
		if ($call_option eq 'getPsConfiguration')
		{
			$php_args = "HOST_GUID##".$AMETHYST_VARS->{'HOST_GUID'}."!##!IP##".$cs_ip."!##!PORT##".$cs_port."!##!PROTOCOL##".$HTTP."!##!PAIRID##".$id."!##!OPTION##".$call_option;
		}
		
		if ($call_option eq 'getRollbackStatus')
		{
			$php_args = "HOST_GUID##".$AMETHYST_VARS->{'HOST_GUID'}."!##!IP##".$cs_ip."!##!PORT##".$cs_port."!##!PROTOCOL##".$HTTP."!##!SOURCEID##".$id."!##!OPTION##".$call_option;
		}
		
		if($call_option eq 'setResyncFlag')
		{
			$php_args = "HOST_GUID##".$AMETHYST_VARS->{'HOST_GUID'}."!##!IP##".$cs_ip."!##!PORT##".$cs_port."!##!PROTOCOL##".$HTTP."!##!PAIRINFO##".$id."!##!OPTION##".$call_option;
		}
		
		my $extension = "php";
		my $file_path = Utilities::makePath("/home/svsystems/admin/web/get_db_data.php");
		if(Utilities::isWindows())
		{
			$file_path = "$cs_installation_path\\home\\svsystems\\admin\\web\\get_db_data.php";
			$extension = $AMETHYST_VARS->{'PHP_PATH'};
		}
		my $command = $extension." ".'"'.$file_path.'"'." ". $php_args;
            $logging_obj->log("DEBUG","call_cx_api_for_data : Command : $command");
        	$xml_output = `$command`;
	            $logging_obj->log("DEBUG","call_cx_api_for_data : Exit Code: ".$?);
	
		if($? != 0)
        {
            $logging_obj->log("EXCEPTION","call_cx_api_for_data : Command : $command Failed to get the data");
            $xml_output = undef;
        }
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","call_cx_api_for_data : Could not get the data : $@");
		$xml_output = undef;
	}
    return $xml_output;	
}

sub parse_xml_data
{
	my($parsed_data) = @_;
	my %response = %{$parsed_data->{'Body'}};
	my $res = $response{'Function'};	
	
	my @table_list;
    my $only_one_input = 0;
	if($res->{'FunctionResponse'}->{"ParameterGroup"})
	{
		@table_list = @{$res->{'FunctionResponse'}->{"ParameterGroup"}};
	}
    elsif($res->{'FunctionResponse'}->{"Parameter"})
    {
        push(@table_list, $res->{'FunctionResponse'});
        $only_one_input = 1;
    }
	
	my $responses;
	foreach my $table (@table_list)
	{
		my $resp;
		my $table_name = $table->{"Id"};
		my $resp_type = ref($table->{"ParameterGroup"});
		
		my @table_array = $resp_type eq "ARRAY" ? @{$table->{"ParameterGroup"}} : $table->{"ParameterGroup"};
		if($resp_type eq "")
		{
			my $resp_type = ref($table->{"Parameter"});
			my @table_data = $resp_type eq "ARRAY" ?  @{$table->{"Parameter"}} : $table->{"Parameter"};			
			foreach my $table_keys (@table_data)
			{
				$resp->{$table_keys->{"Name"}} = $table_keys->{"Value"};				
			}
            if($only_one_input)
            {
                $responses = $resp;
                return $responses;
            }
		}
		else
		{
			foreach my $table_info (@table_array)
			{			
				my $resp_type = ref($table_info->{"Parameter"});
				my @table_data = $resp_type eq "ARRAY" ?  @{$table_info->{"Parameter"}} : $table_info->{"Parameter"};	
				my $key = $table_info->{"Id"};
				foreach my $table_keys (@table_data)
				{
					$resp->{$key}->{$table_keys->{"Name"}} = $table_keys->{"Value"};				
				}		
			}            		
        }
		$responses->{$table_name} = $resp;		
	}
	return $responses;
}

sub update_cs_db
{
	my($event,$api_output,$policy_data) = @_;
	my @batch_input;
	
	my $cs_ip = ($AMETHYST_VARS->{'PS_CS_IP'}) ? $AMETHYST_VARS->{'PS_CS_IP'} : $AMETHYST_VARS->{'CS_IP'};
	my $cs_port = ($AMETHYST_VARS->{'PS_CS_PORT'}) ? $AMETHYST_VARS->{'PS_CS_PORT'} : $AMETHYST_VARS->{'CS_PORT'};
	
	eval
	{
		my $request_xml = "<Body>
						<FunctionRequest Name='UpdateDB'>"."\n";
		my $count = 0;
		foreach my $keys (keys %{$api_output})
		{
			my $input = $api_output->{$keys}->{'queryType'}.",".$api_output->{$keys}->{'tableName'}.",".$api_output->{$keys}->{'fieldNames'}.",".$api_output->{$keys}->{'condition'}.",".$api_output->{$keys}->{'additionalInfo'};
			
			if(!(grep  $_ eq $input,@batch_input))
			{
				push(@batch_input,$input);
				$api_output->{$keys}->{'queryType'} =~ s/"/&quot;/g;
				$api_output->{$keys}->{'tableName'} =~ s/"/&quot;/g;
				$api_output->{$keys}->{'fieldNames'} =~ s/"/&quot;/g;
				$api_output->{$keys}->{'condition'} =~ s/"/&quot;/g;
				$api_output->{$keys}->{'additionalInfo'} =~ s/"/&quot;/g;
				
				$request_xml .= '<ParameterGroup Id="Id'.$count.'">
							<Parameter Name="queryType" Value="'.$api_output->{$keys}->{'queryType'}.'"/>
							<Parameter Name="tableName" Value="'.$api_output->{$keys}->{'tableName'}.'"/>
							<Parameter Name="fieldNames" Value="'.$api_output->{$keys}->{'fieldNames'}.'"/>
							<Parameter Name="condition" Value="'.$api_output->{$keys}->{'condition'}.'"/>
							<Parameter Name="additionalInfo" Value="'.$api_output->{$keys}->{'additionalInfo'}.'"/>
						</ParameterGroup>'."\n";
				$count++;		
			}
		}
		my $unique_id = 0;
		foreach my $value (@$policy_data)
		{
			my @monitor_rpo_output = split(/\!\!/,$value);
			
			$request_xml .= '<ParameterGroup Id="monitor_pairs'.$unique_id.'">
							<Parameter Name="rpo" Value="'.$monitor_rpo_output[0].'"/>
							<Parameter Name="src_id" Value="'.$monitor_rpo_output[1].'"/>
							<Parameter Name="src_vol" Value="'.$monitor_rpo_output[2].'"/>
							<Parameter Name="dest_vol" Value="'.$monitor_rpo_output[4].'"/>
							<Parameter Name="dest_id" Value="'.$monitor_rpo_output[3].'"/>
							<Parameter Name="rpo_violation" Value="'.$monitor_rpo_output[5].'"/>
						</ParameterGroup>'."\n";
			$unique_id++;			
		}
		$request_xml .= '		</FunctionRequest>
					</Body>';
				
		my $response;
		my $http_method = "POST";
		
		# $HTTP defined in global scope
		my $https_mode = ($HTTP eq 'https') ? 1 : 0;
	
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $cs_ip, 'PORT' => $cs_port);
		my $param = {'access_method_name' => "UpdateDB", 'access_file' => "/ScoutAPI/CXAPI.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'type' => 'text/xml' , 'content' => $request_xml};
			
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);
		
		if (!$response->{'status'}) 
		{
			$logging_obj->log("EXCEPTION","Failed to post the content for ".$event);
		}

		# change the log level to INFO for debugging Request/Response data
		$logging_obj->log("DEBUG", "Event: ".$event." Request: ".$request_xml." Response: ".Dumper($response));
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the ".$event." details to the CS DB. Error : ".$@);
	}
}

#  
# Function Name: cs_db_backup  
# Description: 
#   Subroutein will maintain cs db backup
# Parameters: None 
# Return Value: None
#
sub cs_db_backup
{
	my ($package, $extra_params) = @_;
	
	eval
	{
		database_backup($extra_params);
		cs_db_telemetry($extra_params);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","cs_db_backup :: error while executing - $@");
	}
}

sub database_backup
{
	my ($extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	##############
	#
	#This code part has been added to do pruning the database records for the mentioned intervals in queries. In future if you have any table prunes, we can add here. It prunes the trendingData, #policyViolationEvents now, instead to run these queries every time in API's. I think every day, if we run event there is a chance of 23 hours ready to prune data can exist in table and that #can cause scan cost too with those ready to prune records. Instead, run the event every one hour and so it keeps one hour ready to prune data additionally. So add all these prune queries to #every hour event + the optimized prune queries(the queries use indexes) should help to improve performance with full table scan avoid every time. 
	#
	##############
	my $policy_violation_records_prune_sql = "DELETE FROM policyViolationEvents WHERE eventDate < CURDATE()";
	my $status = $conn->sql_query($policy_violation_records_prune_sql);
	my $trending_data_changes_prune_sql = "DELETE FROM trendingData where reportHour < (NOW() - INTERVAL 31 DAY)";
	my $status = $conn->sql_query($trending_data_changes_prune_sql);
	my $audit_sql = "DELETE FROM auditLog where dateTime < (NOW() - INTERVAL 180 DAY)";
	my $status = $conn->sql_query($audit_sql);
	
	$logging_obj->log("DEBUG","cs_db_backup started");
	my $DB_USER = $AMETHYST_VARS->{'DB_USER'};
	my $DB_PASSWD = $AMETHYST_VARS->{'DB_PASSWD'};
	my $DB_NAME = $AMETHYST_VARS->{'DB_NAME'};
	my $SLASH = "\/";
	my $MYSQL_DUMP = "/usr/bin/mysqldump";
	my $back_up_output;
	my $backup_file_count = $AMETHYST_VARS->{'CS_DB_BACKUP_COUNT'};
	my @file_list;
	my $is_zero_file = 0;
	if(!$backup_file_count){ $backup_file_count = 7; }
	
	my $current_date = HTTP::Date::time2iso();
	my @cur_date = split(/ /,$current_date);
	my $cur_date = $cur_date[0];
	my @cur_time = split(/:/,$cur_date[1]);
	
	if (Utilities::isWindows())
	{
		$SLASH = "\\";		
		#$MYSQL_DUMP = "\"c:\\Program Files (x86)\\MySQL\\MySQL Server 5.1\\bin\\mysqldump\"";
		$MYSQL_DUMP = '"'.$MYSQL_PATH.$SLASH."bin\\mysqldump".'"';
	}
	
	my $backup_dir = $AMETHYST_VARS->{'INSTALLATION_DIR'}.$SLASH."var".$SLASH."cs_db_backup";
	my $backup_dir_alias = '"'.$backup_dir.'"';
	my $backup_cmd = "$MYSQL_DUMP --skip-add-drop-table -u ".$DB_USER." ".$DB_NAME.
				" > ".$backup_dir_alias.$SLASH.$cur_date.".sql -p".$DB_PASSWD;
	

	#Create backup directory if not exists			
	if (!-e $backup_dir) 
	{ 
		my $mkpath_output = mkpath($backup_dir, { mode => 0777 });
		$back_up_output = `$backup_cmd`;
		$logging_obj->log("DEBUG","cs_db_backup :: backup directory created : mkpath_output - $mkpath_output,  and sql created : back_up_output - $back_up_output");
	}
	else
	{
		if(!opendir (DIR, $backup_dir))
		{
			$logging_obj->log("EXCEPTION","cs_db_backup :: error while accessing directory $backup_dir - $!");
			return ;
		}
		else
		{
			#read all sql files from the backup directory
			while (my $file = readdir(DIR)) 
			{
				my @file_parts = split (/\./, $file);
				if($file_parts[1] eq 'sql')
				{
					my $time_stamp = $file_parts[0]." 00:00:00";
					push(@file_list,$file_parts[0]);
				}
			}
			closedir(DIR);
			#print Dumper(\@file_list);
			@file_list = sort(@file_list);				
			#print Dumper(\@file_list);
			my $file_count = @file_list;
			my $latest_date = $file_list[$file_count - 1];
			my $oldest_date = $file_list[0];
			my $filesize;
			
			if (!$file_count)
			{
				$back_up_output = `$backup_cmd`;
				$logging_obj->log("DEBUG","cs_db_backup :: backup sql created back_up_output - $back_up_output");
			}
			else
			{
				#Create backup file for every day between 5 am to 10 am
				if($cur_date ne $latest_date)
				{
					if(int($cur_time[0]) >=5 && int($cur_time[0]) <=10)
					{
						$back_up_output = `$backup_cmd`;
						
						if(-e $backup_dir.$SLASH.$cur_date.".sql")
						{
							$filesize = ((stat($backup_dir.$SLASH.$cur_date.".sql"))[7]);
							if (!$filesize)
							{
								#If zero bytes dump generated, then delete the 0 bytes dump file.
								unlink($backup_dir.$SLASH.$cur_date.".sql");
								$is_zero_file = 1;
								$logging_obj->log("EXCEPTION","cs_db_backup :: Zero file size detected: ".$cur_date.".sql");
							}
						}
						
						$logging_obj->log("DEBUG","cs_db_backup :: dbbackup taken at $cur_time[0] with filesize $filesize");
					}
				}
				
				if($file_count > $backup_file_count)
				{
					#If today database dump size is zero bytes, then stop pruning.
					if ($is_zero_file eq 0)
					{
						my $prune_file = $oldest_date.".sql";
						unlink($backup_dir.$SLASH.$prune_file);
					}
				}
			}
		}
	}
}

sub cs_db_telemetry
{
	my ($extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	my ($logStr, $infralogStr);
	my %log_data;
	
	my %telemetry_metadata = ('RecordType' => 'CSDBTelemetry');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
	my $current_system_date = HTTP::Date::time2iso();
	my @current_date = split(/ /,$current_system_date);
	my $current_date = $current_date[0];
	my @current_system_time = split(/:/,$current_date[1]);
	
	#Collecting DB data every day between 5 am to 10 am. Date module returns time in 24 hours format.
	if(int($current_system_time[0]) >= 5 && int($current_system_time[0]) <= 6)
	{
		my $sql_hosts = "SELECT
						id,
						name,
						hosts.ipaddress as ip,
						sentinel_version,
						from_unixtime(lastHostUpdateTime) as updatetime,
						creationTime,
						biosId,
						GROUP_CONCAT(macAddress) as macAddresses
					FROM
						hosts,
						hostnetworkaddress 
					WHERE
						hosts.id = hostnetworkaddress.hostId
					AND 
						name != 'InMageProfiler' 
					AND 
						ipType = 1
					group by id";
		
		my $result_hosts = $conn->sql_exec($sql_hosts);

		foreach my $row_hosts (@{$result_hosts})
		{
			$logStr .= "\ncs_db_telemetry Host data:: (hostid:".$row_hosts->{'id'}.", name:".$row_hosts->{'name'}.", 
			ipaddress:".$row_hosts->{'ip'}.", Agent Version:".$row_hosts->{'sentinel_version'}.", 
			Agent update time:".$row_hosts->{'updatetime'}.", Agent bios id:".$row_hosts->{'biosId'}.", 
			Agent macAddresses:".$row_hosts->{'macAddresses'}.")";
		}
		
		$log_data{"Message"} = $logStr;
		$logging_obj->log("EXCEPTION",\%log_data,$TELEMETRY);
		
		my $sql_infra = "SELECT
						ii.inventoryId,
						siteName,
						ii.hostIp,
						hostType,
						lastDiscoverySuccessTime,
						creationTime,
						iv.infrastructureHostId,
						infrastructureVMId,
						instanceUuid,
						hostUuid,
						displayName,
						iv.hostName,
						OS,
						hostId,
						macAddressList,
						discoveryType
					FROM
						infrastructureinventory ii,
						infrastructurehosts ih,
						infrastructurevms iv 
					WHERE
						ii.inventoryId = ih.inventoryId
					AND 
						ih.infrastructureHostId = iv.infrastructureHostId
					AND 
						ii.hostIp = ih.hostIp
					AND 
						ih.hostIp = iv.hostIp 
					AND 
						ii.hostType = 'Physical'";
		
		my $result_infra = $conn->sql_exec($sql_infra);

		foreach my $row_infra (@{$result_infra})
		{
			$infralogStr .= "\ncs_db_telemetry Infrastructure data:: (inventoryId:".$row_infra->{'inventoryId'}.", 
			siteName:".$row_infra->{'siteName'}.", hostIp:".$row_infra->{'hostIp'}.", 
			hostType:".$row_infra->{'hostType'}.", lastDiscoverySuccessTime:".$row_infra->{'lastDiscoverySuccessTime'}.",
			creationTime:".$row_infra->{'creationTime'}.", infrastructureHostId:".$row_infra->{'infrastructureHostId'}.", 
			infrastructureVMId:".$row_infra->{'infrastructureVMId'}.", instanceUuid:".$row_infra->{'instanceUuid'}.", 
			hostUuid:".$row_infra->{'hostUuid'}.", displayName:".$row_infra->{'displayName'}.", 
			hostName:".$row_infra->{'hostName'}.", OS:".$row_infra->{'OS'}.", hostId:".$row_infra->{'hostId'}.", 
			macAddressList:".$row_infra->{'macAddressList'}.", discoveryType:".$row_infra->{'discoveryType'}.")";
		}

		$log_data{"Message"} = $infralogStr;
		$logging_obj->log("EXCEPTION",\%log_data,$TELEMETRY);
	}
}

sub add_error_message
{	
	my ($alert_info, $trap_info) = @_;
	$logging_obj->log("DEBUG","add_error_message : alert_info : $alert_info :: trap_info : $trap_info");
	
	my $cs_ip = ($AMETHYST_VARS->{'PS_CS_IP'}) ? $AMETHYST_VARS->{'PS_CS_IP'} : $AMETHYST_VARS->{'CS_IP'};
	my $cs_port = ($AMETHYST_VARS->{'PS_CS_PORT'}) ? $AMETHYST_VARS->{'PS_CS_PORT'} : $AMETHYST_VARS->{'CS_PORT'};
	my $response;		
	eval
	{
		my $param_xml;
		my $header_xml = "<Body>
						<FunctionRequest Name='addErrorMessage'>
						<ParameterGroup Id='addErrorMessage'> \n";
		if($alert_info)
		{
			$param_xml .= "<Parameter Name='alert_info' Value='".$alert_info."'/>";
		}
		
		if($trap_info)
		{
			$param_xml .= "<Parameter Name='trap_info' Value='".$trap_info."'/>";
		}
		
		my $footer_xml .=	"</ParameterGroup>
						</FunctionRequest>
					</Body>"."\n";
		if($param_xml eq "")
		{
			$logging_obj->log("EXCEPTION","add_error_message : Failed to create the request XML params.");
			return;
		}
		
		my $body_xml =  $header_xml." ".$param_xml." ".$footer_xml;
		
		
		my $http_method = "POST";
		my $https_mode = ($HTTP eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $cs_ip, 'PORT' => $cs_port);
		my $param = {'access_method_name' => "addErrorMessage", 'access_file' => "/ScoutAPI/CXAPI.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'type' => 'text/xml' , 'content' => $body_xml};
		
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);	

		if (!$response->{'status'}) 
		{
			$logging_obj->log("EXCEPTION","Failed to post the content");
		}		
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : ".$@);
	}
}	

sub get_cx_role
{	
	my ($standby_ip_address) = @_;
	
	my $cs_ip = $standby_ip_address;
	my $cs_port = ($AMETHYST_VARS->{'PS_CS_PORT'}) ? $AMETHYST_VARS->{'PS_CS_PORT'} : $AMETHYST_VARS->{'CS_PORT'};	
	my $is_primary;
	eval
	{
		my $request_xml = "<Body>
						<FunctionRequest Name='GetCXRole'>
							<ParameterGroup Id='GetCXRole'>
								<Parameter Name='ip_address' Value='".$standby_ip_address."'/>
							</ParameterGroup>
						</FunctionRequest>
					</Body>"."\n";
			
		$logging_obj->log("DEBUG","request_xml ::$request_xml \n");

		my $http_method = "POST";
		my $https_mode = ($HTTP eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $cs_ip, 'PORT' => $cs_port);
		my $param = {'access_method_name' => "GetCXRole", 'access_file' => "/ScoutAPI/CXAPI.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'type' => 'text/xml', 'content' => $request_xml};
		
		my $request_http_obj = new RequestHTTP(%args);
		my $response = $request_http_obj->call($http_method, $param, $content);
		
		$logging_obj->log("DEBUG","Response::".Dumper($response));		
        
        if($response->{'status'})
        {
            my $response_xml = $response->{'content'};

            my $parsed_data;
            if(!$response_xml)
            {
                $logging_obj->log("DEBUG","get_cx_role : Reponse XML HASH : \n ".Dumper($response));
                return $parsed_data;
            }
		
            my $parse = new XML::Simple();
            $parsed_data = $parse->XMLin($response_xml);
            
            $logging_obj->log("DEBUG","get_cx_role : Reponse XML HASH : ".Dumper($parsed_data));
            
            my $xml_output = &parse_xml_data($parsed_data);
            $is_primary = ($xml_output->{'IsPrimary'}) ? "Primary" : "Secondary"; 
        }        		
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","get_cx_role : Error : ".$@);
	}
    return $is_primary;
}

sub update_cache_files
{
    my ($api_output) = @_;
    
    my $intervals_str = $AMETHYST_VARS->{'INTERVALS'};
    $intervals_str =~ s/\s+//g;
    my @intervals = split(/,/, $intervals_str);
    
    foreach my $interval (@intervals)
    {
        my $cache_file = $BASE_DIR."/var/event_data_".$interval.".txt";
        open(INT_FH, ">$cache_file");
        print INT_FH $api_output;
        close(INT_FH);
    }
}

sub insert_host_credentials
{
	my($conn, $esx_ip, $esxUsername, $esxPassword) = @_;
	my $encryption_key= Utilities::get_cs_encryption_key();	
	if(!$encryption_key)
	{
		$logging_obj->log("EXCEPTION","Encryption key not found in insert_host_credentials \n");
		return 0;
	}
	my @esx_ip_arr = ($esx_ip);	
	my $esx_ip_ref = \@esx_ip_arr;
	my $id;
	my $select_sql = "SELECT	
						id
					FROM	
						credentials
					WHERE	
						serverId = ?";
	my $select_sth = $conn->sql_query($select_sql, $esx_ip_ref);
	my @id_array = (\$id);
	$conn->sql_bind_columns($select_sth, @id_array);
	my $numCount = $conn->sql_num_rows($select_sth);
	my @esx_credentials = ($esxUsername, $esxUsername, $esxUsername, $encryption_key, $esxPassword, $esxPassword, $esxPassword, $encryption_key, $esx_ip);
	my $esx_credentials_ref = \@esx_credentials;	
	if($numCount)
	{
		my $update_sql = "UPDATE
						credentials
					SET
						userName = IF(? = '', ?,  HEX(AES_ENCRYPT(?, ?))),
						password = IF(? = '', ?,  HEX(AES_ENCRYPT(?, ?)))
					WHERE
						serverId = ?";
		my $update_sth = $conn->sql_query($update_sql, $esx_credentials_ref);		
	}
	else
	{
		my $insert_sql = "INSERT INTO 
							credentials ( 								
							userName,
							password,
							serverId)
						VALUES(IF(? = '', ?,  HEX(AES_ENCRYPT(?, ?))), IF(? = '', ?,  HEX(AES_ENCRYPT(?, ?))), ?)";
		my $insert_sth = $conn->sql_query($insert_sql, $esx_credentials_ref);
	}
	return 1;
}
#  
# Function Name: monitor_alerts  
# Description: 
#   This Sub routine is used to insert vCon protection/recovery alerts
# Parameters: None 
# Return Value: None
#

sub monitor_alerts
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	eval
	{
		my $api_request_sql = "SELECT
						apiRequestId
					FROM
						apiRequest 
					WHERE
						requestType = 'SendAlerts'
					AND	
						state = 1
					ORDER BY apiRequestId";
					
		$logging_obj->log("DEBUG","monitor_alerts :: $api_request_sql");
		my $result = $conn->sql_exec($api_request_sql);
		my ($error_id, $error_template_id, $error_summary, $error_message, $host_id, $event_code, $error_placeholders, $alertPlaceHolders,$host_name);
		if(defined $result)
		{
			foreach my $row (@{$result})
			{
				my $apiRequestId = $row->{'apiRequestId'};
				my $api_request_detail_sql = "SELECT requestdata FROM apiRequestDetail WHERE apiRequestId = $apiRequestId";
				my $rs_data = $conn->sql_exec($api_request_detail_sql);
				my $requestData;
				if (defined $rs_data)
				{
					my @result_arr = @{$rs_data};
					$requestData = $result_arr[0]{'requestdata'};		
				}
				my $alertDetails = unserialize($requestData);
				my $alertInfo = $alertDetails->{'AlertInfo'};		

				foreach my $entry (keys %{$alertInfo})
				{		
					$alertPlaceHolders 	= unserialize($alertInfo->{$entry}->{'alertPlaceHolders'});	
					$error_id =$host_id = $alertInfo->{$entry}->{'alertId'};
					$error_template_id 	= $alertInfo->{$entry}->{'alertTemplateId'};
					$error_summary 		= $alertInfo->{$entry}->{'alertSummary'};
					$error_message 		= $alertInfo->{$entry}->{'alertMessage'};				
					$event_code 		= $alertInfo->{$entry}->{'eventCode'};
					$error_placeholders	= $alertInfo->{$entry}->{'alertPlaceHolders'};			
					my $error_flag	= 0;
					if($alertPlaceHolders->{'xmlFileName'})
					{
						my $xml_file_name = $alertPlaceHolders->{'xmlFileName'};
						my $alert_type = $alertPlaceHolders->{'type'};
						my($xml_content,$parsed_data);
						if((-e ($xml_file_name)) && (-s ($xml_file_name) > 0))
						{
							open(FH,"<$xml_file_name") or die "Unable to open $xml_file_name file : $!\n";
							#$xml_content = <FH>;
							while(<FH>)
							{
								$xml_content .= $_;
							}						
							close FH;
						}	
						else
						{
							$error_flag == 1;
						}
						if(!$xml_content)
						{
							$logging_obj->log("EXCEPTION","Unable to get xml content from $xml_file_name file \n");
							$error_flag = 1;
						}
						else
						{
							my $parse = new XML::Simple();
							$parsed_data = $parse->XMLin($xml_content);												
							if($parsed_data)
							{	
								my $srchost_data = $parsed_data->{'SRC_ESX'};
								my $placeHolderarr;
								if($srchost_data) 
								{	
									my @source_host_arr = (ref($srchost_data->{'host'}) eq "ARRAY") ? @{$srchost_data->{'host'}} : $srchost_data->{'host'};
									
									foreach my $host(@source_host_arr) 
									{
										$host_id = $error_id = $host->{'inmage_hostid'};
										$host_name = $host->{'hostname'};
										$placeHolderarr->{'HostName'} = $host_name;									
										$error_message = $alert_type." failed before downloading the configuration file for host : $host_name";
										$error_placeholders = serialize($placeHolderarr);								
										&Messenger::add_error_message ($conn, $error_id, $error_template_id, $error_summary, $error_message, $host_id, $event_code, $error_placeholders);
									}																		
								}
								
							}
							else
							{
								$logging_obj->log("EXCEPTION","Unable to parse $xml_file_name file \n");
								$error_flag == 1;
							}
						}
						if($error_flag == 1)
						{						
							$event_code = ($alert_type eq "Protection") ? 'VE0705' : 'VE0706';
							$error_message = "Unable to read $alert_type xml file $xml_file_name";
							&Messenger::add_error_message ($conn, $error_id, $error_template_id, $error_summary, $error_message, $host_id, $event_code, $error_placeholders);
						}
					}
					else
					{
						&Messenger::add_error_message ($conn, $error_id, $error_template_id, $error_summary, $error_message, $host_id, $event_code, $error_placeholders);
					}
				}
				my $api_request_update = $conn->sql_query("UPDATE apiRequest SET state = 3 WHERE apiRequestId = $apiRequestId");
				my $api_request_detail_update = $conn->sql_query("UPDATE apiRequestDetail SET state = 3 WHERE apiRequestId = $apiRequestId");
			}
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","monitor_alerts : Error : ".$@);
		return 0;
	}
	return 1;
}

sub service_restart_periodically
{	
	eval
	{
		if (Utilities::isWindows())
		{
			my $AMETHYST_VARS		= Utilities::get_ametheyst_conf_parameters();
			
			my $TMAN_SERVICE_RESTART_PERIOD	= $AMETHYST_VARS->{"TMAN_SERVICE_RESTART_PERIOD"};
			my $INSTALLATION_DIR       = $AMETHYST_VARS->{"INSTALLATION_DIR"};
			
			if ($TMAN_SERVICE_RESTART_PERIOD > 0)
			{
				my $tman_running = "RUNNING";
				my $tman_stopped = "STOPPED";
				
				my $batch_file_path = Utilities::makePath($INSTALLATION_DIR."/bin/restart_tman_services.bat");
				my $pid_file_path = Utilities::makePath($INSTALLATION_DIR."/var/tmansvc.pid");
				
				#If tmanagerd service stopped state, then exit. That means somebody initiated restart or stop. Don't do any thing further, because we need restart finally.
				my $tman_service_status = tman_status($tman_stopped);
				if ($tman_service_status == 1)
				{
					$logging_obj->log("EXCEPTION","Exit from service_restart_periodically sub routine as  tmansvc service already in stop mode.");
					exit(0);
				}
				
				#If service started, but tmansvc.pid file not yet created.
				unless(-e $pid_file_path)
				{
					$logging_obj->log("EXCEPTION","Exit from service_restart_periodically sub routine as  $pid_file_path file not exists.");
					exit(0);
				}
				
				
				my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,$blksize,$blocks);
				($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat($pid_file_path);
					
				my $cur_time = time;
				
				
				if ( ($cur_time - $mtime) > $TMAN_SERVICE_RESTART_PERIOD)
				{	
				
						my $tman_service_status = tman_status($tman_running);
						
						my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime_alias,$ctime,$blksize,$blocks) = Win32::UTCFileTime::file_alt_stat($pid_file_path);
						#Service is in running mode and it should be old tmansvc pid, then only proceed further. Otherwise somebody started tmansvc in between.
						
						if (($tman_service_status == 1) and ($mtime_alias - $mtime)==0)
						{
							`$batch_file_path`;
						}
					
				}	
			}
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","service_restart_periodically : Error : ".$@);
	}
}

#Function to get the CS and PS SSL certificates expiry date in UTC.
sub monitor_certs
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	my $data_hash = $extra_params->{'data_hash'};
	my $host_data = $data_hash->{'host_data'};
	my $ps_data = $data_hash->{'ps_data'}; 
	my ($cs_cert_expiry_date,$ps_cert_expiry_date,$cs_cert_path,$ps_cert_path, $serialized_data,$cert_expiry_details,$host_guid,$data,$utc_time,$cs_type,$INSTALLATION_DIR,$bin_file_path,$cert_path, $host_name);
	eval
	{	
		my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
		$cs_type = ($AMETHYST_VARS->{'CX_TYPE'}) ? $AMETHYST_VARS->{'CX_TYPE'} : $AMETHYST_VARS->{'CX_TYPE'};
		$host_guid = ($AMETHYST_VARS->{'HOST_GUID'}) ? $AMETHYST_VARS->{'HOST_GUID'} : $AMETHYST_VARS->{'HOST_GUID'};
		$INSTALLATION_DIR       = $AMETHYST_VARS->{"INSTALLATION_DIR"};	
		$bin_file_path = "viewcert.exe";
		$cert_path = $ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\certs\\";
		$cert_path = Utilities::makePath($cert_path);
		$host_name = $host_data->{$host_guid}->{"name"};
		$serialized_data = undef;
		my $currentTime = time();
		#For CS
		if ($cs_type eq 1 || $cs_type eq 3)
		{
			$cs_cert_path = $cert_path."cs.crt";
			$cs_cert_expiry_date = get_cert_expiry($bin_file_path,$cs_cert_path);
			if (defined($cs_cert_expiry_date))
			{
				$utc_time = convert_to_utc($cs_cert_expiry_date); 
				if (defined($utc_time))
				{
					$serialized_data->{"CsCertExpiry"} = $utc_time;
				}
			}
		}
		
		#Serialize and update the value in hosts table in DB.
		if (defined($serialized_data))
		{
			$cert_expiry_details = serialize($serialized_data);
			my $update_query;
			$update_query->{'cleanup_update'}->{"queryType"} = "UPDATE";
			$update_query->{'cleanup_update'}->{"tableName"} = "hosts";
			$update_query->{'cleanup_update'}->{"fieldNames"} = "certExpiryDetails='$cert_expiry_details'";
			$update_query->{'cleanup_update'}->{"condition"} = "id='$host_guid'";	
			&tmanager::update_cs_db("monitor_certs",$update_query);
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","monitor_certs : Could not get the data : $@");
	}	
}

#Function to get the certificate expiry date from the output given from viewcert binary
sub get_cert_expiry
{
	my($bin_file_path,$cert_path) = @_;
	my ($command,$result, @date_time, $cert_expiry_date);
	$cert_expiry_date = undef;
	if (-e $cert_path and -f $cert_path)
	{
		$command = $bin_file_path." '".$cert_path."'";
		my $powershell_path = `where powershell.exe`;
		$result = `$powershell_path -command  $command`;
		if($? != 0)
		{
			$logging_obj->log("INFO","get_cert_expiry : Failed to execute Command : $command, Exit code: $? \n");
		}
		else
		{
			#Not After : Sep  6 14:59:12 2019 GMT(Get string between After and GMT)
			@date_time = $result =~ /After(.*)GMT/g;
			foreach my $data(@date_time) 
			{
				$cert_expiry_date = $data;
				$cert_expiry_date =~ s/^\s+|\s+$//g;
				last;
			}
			$logging_obj->log("INFO","get_cert_expiry : Command Succeeded: $command, Exit code: $? \n");
		}
	}
	else
	{
		$logging_obj->log("INFO","get_cert_expiry : Cert path not exists, cert path: $cert_path \n");
	}
	return $cert_expiry_date;
}


#Function to convert sent time string to UTC time stamp.
sub convert_to_utc
{
	
	my($cert_expiry_date) = @_;
	my ($utc_time,$data,$mon,$mday,$year,$time);
	my %monthtonum = qw(jan 1  feb 2  mar 3  apr 4  may 5  jun 6 jul 7  aug 8  sep 9  oct 10 nov 11 dec 12);
	#Left trim
	$cert_expiry_date =~ s/^\s+//;
	#Right trim
	$cert_expiry_date =~ s/\s+$//;
	#Split the date string(Sep  6 14:59:12 2019) with space
	my @date_data = split ' ', $cert_expiry_date;
	
	$utc_time = undef;
	my $length = @date_data;
	for (my $i = 1; $i < $length; $i++)
	{
		  $data = $date_data[$i];
		  $data =~ s/^\s+//;
		  $data =~ s/\s+$//;
		  if ($i == 1)
		  {
			$data = lc $data;
			$mon = $monthtonum{$data};
		  }
		  if ($i == 2)
		  {
			$mday = $data;
		  }
		  if ($i == 3)
		  {
			$time = $data;
		  }
		  if ($i == 4)
		  {
			$year = $data;
		  }
	}
	my ($hour,$min,$sec) = split(/[\s:]+/, $time);
	#Get UTC time stamp by passing time and date parameters.
	$utc_time = timelocal($sec,$min,$hour,$mday,$mon-1,$year);
	return $utc_time;
}

#CS new SSL certificate generation sub routine
sub cs_cert_regeneration
{
	my ($cert_update_status, $max_attempts) = @_;
	my @list_services = ("INMAGE-AppScheduler", "tmansvc","w3logsvc","W3SVC","WAS","ProcessServer","ProcessServerMonitor");
	my %placeholder_list=();
	my $hostname =  hostname();
	$placeholder_list{CsName}=$hostname;
	my $msg = "";
	eval
	{
		my @files;
		my $command = "\"import-module webadministration;(Get-ChildItem IIS:\SslBindings | ?{\$_.port -eq 443}\").Thumbprint";
		my $Old_Ssl_Thumbprint = `$powershell_path -command  $command`;
		$logging_obj->log("INFO","cs_cert_regeneration: Command: $command, Old certificate thumbprint: $Old_Ssl_Thumbprint, Status is: $?");
		#Getting old certificate thumbprint failed.
		if ($? != 0) 
		{
			$logging_obj->log("INFO","cs_cert_regeneration: Could not get the currently used old certificate thumbprint: $Old_Ssl_Thumbprint, Status is: $?");
			$cert_update_status->{'Response'} = "1";
			$cert_update_status->{'ErrorCode'} = "ECA0162";
			$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
			$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
			$msg = $msg."ECA0162: cs_cert_regeneration: Could not get the currently used old certificate thumbprint: $Old_Ssl_Thumbprint, Status is: $?";
			$cert_update_status->{'logmessage'} = $msg;
		}
		#Getting old certificate thumbprint got success.
		else
		{
			my $return_value = stop_services($cert_update_status,@list_services);		
			if ($return_value == 1)
			{
				return 0;
			}
			
			#Delete the old certificate backup files.
			my $del_bak_file_status = Utilities::del_files_with_pattern($cs_key_file_path,"cs.key.bak*",5,$cert_update_status);
			
			if ($del_bak_file_status == 1)
			{
				start_services_back($cert_update_status,@list_services);
				$msg = $msg."ECA0163: cs.key.bak deletion failed.";
				$cert_update_status->{'Response'} = "1";
				$cert_update_status->{'ErrorCode'} = "ECA0163";
				$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
				$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
				$cert_update_status->{'logmessage'} = $msg;
				return 0;
			}
			my $del_bak_file_status = Utilities::del_files_with_pattern($cs_key_file_path,"cs.keycert.pfx.bak*",5,$cert_update_status);
			if ($del_bak_file_status == 1)
			{
				start_services_back($cert_update_status,@list_services);
				$msg = $msg."ECA0163: cs.keycert.pfx.bak deletion failed.";
				$cert_update_status->{'Response'} = "1";
				$cert_update_status->{'ErrorCode'} = "ECA0163";
				$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
				$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
				$cert_update_status->{'logmessage'} = $msg;
				return 0;
			}
			my $del_bak_file_status = Utilities::del_files_with_pattern($cs_fingerprint_file_path,"cs.fingerprint.bak*",5,$cert_update_status);
			if ($del_bak_file_status == 1)
			{
				start_services_back($cert_update_status,@list_services);
				$msg = $msg."ECA0163: cs.fingerprint.bak deletion failed.";
				$cert_update_status->{'Response'} = "1";
				$cert_update_status->{'ErrorCode'} = "ECA0163";
				$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
				$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
				$cert_update_status->{'logmessage'} = $msg;
				return 0;
			}
			my $del_bak_file_status = Utilities::del_files_with_pattern($cs_crt_file_path,"cs.crt.bak*",5,$cert_update_status);
			if ($del_bak_file_status == 1)
			{
				start_services_back($cert_update_status,@list_services);
				$msg = $msg."ECA0163: cs.crt.bak deletion failed.";
				$cert_update_status->{'Response'} = "1";
				$cert_update_status->{'ErrorCode'} = "ECA0163";
				$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
				$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
				$cert_update_status->{'logmessage'} = $msg;
				return 0;
			}
		
			#Generating the new SSL certificate.
			my $cert_gen_command = $gen_cert_bin_path."gencert.exe -n cs -i";
			my $cert_gen_status;
			$cert_gen_status = `$cert_gen_command 2>&1`;
			$logging_obj->log("INFO","cs_cert_regeneration: Command is:".$cert_gen_command."Status is:".$?);
			#New cert generation failed.
			if ($? != 0) 
			{
				
				undo_cs_cert_files($cert_update_status);
				start_services_back($cert_update_status,@list_services);
				$logging_obj->log("INFO","cs_cert_regeneration: Could not generate CS certificate and Output is:".$cert_gen_status."Status is:".$?);
				$cert_update_status->{'Response'} = "1";
				$cert_update_status->{'ErrorCode'} = "ECA0163";
				$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
				$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
				$msg = $msg."ECA0163: cs_cert_regeneration: Could not generate CS certificate and Status is: $?";
				$cert_update_status->{'logmessage'} = $msg;	
			}
			#New cert generation Succeeded.
			else
			{
				$logging_obj->log("INFO","cs_cert_regeneration: CS certificate generation is success and Output is:".$cert_gen_status."Status is:".$?);
				my $cs_key_file = $cs_key_file_path."cs.key";
				my $cs_crt_file = $cs_crt_file_path."cs.crt";
				my $cs_fingerprint_file = $cs_fingerprint_file_path."cs.fingerprint";
				my $cs_keycert_file = $cs_key_file_path."cs.keycert.pfx";
				#After new cert generation got succeeded, validating once again to check the newly generated cert files exists or not.
				my $key_bak_count = @files = bsd_glob $cs_key_file_path."cs.key.bak*";
				my $keycert_bak_count = @files = bsd_glob $cs_key_file_path."cs.keycert.pfx.bak*";
				my $fp_bak_count = @files = bsd_glob $cs_fingerprint_file_path."cs.fingerprint.bak*";
				my $crt_bak_count = @files = bsd_glob $cs_crt_file_path."cs.crt.bak*";
				
				$logging_obj->log("INFO","cs_cert_regeneration: cert file = $cs_crt_file, keycert file = $cs_keycert_file, key file = $cs_key_file, Fingerprint file = $cs_fingerprint_file, key bak count = $key_bak_count , keycert bak count = $keycert_bak_count , fp bak count = $fp_bak_count, crt bak count = $crt_bak_count");				
				if (-e $cs_key_file and 
					-e $cs_crt_file and 
					-e $cs_fingerprint_file and 
					-e $cs_keycert_file and  
					$key_bak_count and 
					$keycert_bak_count and 
					$fp_bak_count and 
					$crt_bak_count)
				{
					#Getting the newly generated certificate thumbprint.
					$command = "\"(Get-ChildItem -Path cert:\\LocalMachine\\My -DnsName 'Scout' | select Thumbprint, NotAfter | Foreach-Object {\$_.NotAfter = [DateTime]\$_.NotAfter; \$_} | Group-Object Computer | Foreach-Object {\$_.Group | Sort-Object NotAfter | Select-Object -Last 1}).Thumbprint\"";
					my $New_Ssl_Thumbprint = `$powershell_path -command  $command`;
					$logging_obj->log("INFO","cs_cert_regeneration: Command: $command, Newly generated certificate thumbprint: $New_Ssl_Thumbprint, Status is: $?");

					#Getting the newly generated certificate thumbprint failed.
					if ($? != 0) 
					{
						undo_cs_cert_files($cert_update_status);
						start_services_back($cert_update_status,@list_services);
						$logging_obj->log("INFO","cs_cert_regeneration: Could not get the newly generated certificate thumbprint: $New_Ssl_Thumbprint, Status is: $?");
						$cert_update_status->{'Response'} = "1";
						$cert_update_status->{'ErrorCode'} = "ECA0163";
						$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
						$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
						$msg = $msg."ECA0163: cs_cert_regeneration: Could not get the newly generated certificate thumbprint: $New_Ssl_Thumbprint, Status is: $?";
						$cert_update_status->{'logmessage'} = $msg;	
					}
					#Getting the newly generated certificate thumbprint got success.
					else
					{
						#Removing the existing SSL certificate binding on port 443.
						$command = "import-module webadministration; Remove-Item -Path \"IIS:\\SslBindings\\*!443\"";
						my $remove_binding = `$powershell_path -command  $command`;
						$logging_obj->log("INFO","cs_cert_regeneration: Command: $command, Removal of SSL certificate binding:  $remove_binding, Status is: $?");
						
						#Removing the existing SSL certificate binding on port 443 failed.
						if ($? != 0) 
						{
							undo_cs_cert_files($cert_update_status);
							start_services_back($cert_update_status,@list_services);
							$logging_obj->log("INFO","cs_cert_regeneration: Removal of SSL certificate binding failed from 443 port: $remove_binding, Status is: $?");
							$cert_update_status->{'Response'} = "1";
							$cert_update_status->{'ErrorCode'} = "ECA0164";
							$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
							$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
							$msg = $msg."ECA0164: cs_cert_regeneration: Removal of SSL certificate binding failed from 443 port: $remove_binding, Status is: $?";
							$cert_update_status->{'logmessage'} = $msg;
						}
						#Removing the existing SSL certificate binding on port 443 success.
						else
						{
							#Binding the new certificate thumbprint on port 443 with 5 attempts and with sleep 5.
							$command = "\"import-module webadministration; Get-Item \"Cert:\\LocalMachine\\My\\$New_Ssl_Thumbprint\" | New-Item IIS:\\SslBindings\\*!443";
							my $start_counter = 0;
							my $do_binding;						
							while ($start_counter < $max_attempts) 
							{
								$do_binding = `$powershell_path -command  $command`;
								if ($? == 0)
								{
									last;
								}
								$start_counter++;
								sleep(5);
							}
							$logging_obj->log("INFO","cs_cert_regeneration: Command: $command, SSL certificate binding on 443 port with both new thumbprint: $do_binding, $New_Ssl_Thumbprint, Status is: $?");
							#Binding the new certificate thumbprint on port 443 failed.
							if ($? != 0) 
							{
								#Trying to Binding with old certificate thumbprint on port 443 with 5 attempts and sleep 5.
								$command = "\"import-module webadministration; Get-Item \"Cert:\\LocalMachine\\My\\$Old_Ssl_Thumbprint\" | New-Item IIS:\\SslBindings\\*!443";
								my $start_counter = 0;
								my $do_binding;						
								while ($start_counter < $max_attempts) 
								{
									$do_binding = `$powershell_path -command  $command`;
									if ($? == 0)
									{
										last;
									}
									$start_counter++;
									sleep(5);
								}
								$logging_obj->log("INFO","cs_cert_regeneration: Command: $command, SSL certificate binding on 443 port with both old thumbprint: $do_binding, $Old_Ssl_Thumbprint, Status is: $?");
								#Binding with old certificate thumbprint on port 443 failed after max attempts and returning with response and error description.
								if ($? != 0) 
								{
									$logging_obj->log("INFO","cs_cert_regeneration: SSL certificate binding failed on 443 port with both new thumbprint: $New_Ssl_Thumbprint and old thumb print $Old_Ssl_Thumbprint, ouput is: $do_binding, Status is: $?");
									$cert_update_status->{'Response'} = "1";
									$cert_update_status->{'ErrorCode'} = "ECA0165";
									$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
									$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
									$msg = $msg."ECA0165: cs_cert_regeneration: SSL certificate binding failed on 443 port with both new thumbprint: $New_Ssl_Thumbprint and old thumb print $Old_Ssl_Thumbprint, ouput is: $do_binding, Status is: $?";
									$cert_update_status->{'logmessage'} = $msg;
								}
								else
								{
									undo_cs_cert_files($cert_update_status);
									start_services_back($cert_update_status,@list_services);
								}
								return 0;
							}
							else
							{
								#If all above success, then remove the old certificate with old thumbprint. 
								$command = "\"Get-ChildItem -Path \"cert:\\LocalMachine\\My\\$Old_Ssl_Thumbprint\" | Remove-Item\"";
								my $start_counter = 0;
								my $remove_from_cert_store;						
								while ($start_counter < $max_attempts) 
								{
									$remove_from_cert_store = `$powershell_path -command  $command`;
									if ($? == 0)
									{
										last;
									}
									$start_counter++;
									sleep(5);
								}
								$logging_obj->log("INFO","cs_cert_regeneration: Command: $command, Remove Old certificate from cert store with old thumbprint: $remove_from_cert_store, $Old_Ssl_Thumbprint, Status: $? ");
								if ($? != 0) 
								{
									$logging_obj->log("INFO","cs_cert_regeneration: Failed to remove Old certificate from cert store with thumbprint $Old_Ssl_Thumbprint. Ignore this one.");
									$msg = $msg."cs_cert_regeneration: Failed to remove Old certificate from cert store with thumbprint $Old_Ssl_Thumbprint. Ignore this one., Status: $? ";
									$cert_update_status->{'logmessage'} = $msg;
								}
								start_services_back($cert_update_status,@list_services);
							}
						}
					}
				}
				#Newly generated certificate files not found after generating new certificate, hence, stopping further process.
				else
				{
					undo_cs_cert_files($cert_update_status);
					start_services_back($cert_update_status,@list_services);
					$logging_obj->log("INFO","cs_cert_regeneration: CS cert generation passed, but one or some of the files: $cs_key_file,$cs_crt_file,$cs_fingerprint_file,$cs_keycert_file not exists in folder(s).");
					$cert_update_status->{'Response'} = "1";
					$cert_update_status->{'ErrorCode'} = "ECA0167";
					$placeholder_list{FolderName}=$ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\";
					$placeholder_list{FileNames}="private\\cs.key,certs\\cs.crt,fingerprints\\cs.fingerprint,private\\cs.keycert.pfx";
					$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
					$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
					$msg = $msg."ECA0167: cs_cert_regeneration: CS cert generation passed, but one or some of the files: $cs_key_file,$cs_crt_file,$cs_fingerprint_file,$cs_keycert_file not exists in folder(s)";
					$cert_update_status->{'logmessage'} = $msg;
				}
			}
		}
	};
	#For any other generic exception, response code is as below.
	if ($@)
	{
		start_services_back($cert_update_status,@list_services);
		$logging_obj->log("EXCEPTION","cs_cert_regeneration: Exception in CS cert generation: $@");
		$cert_update_status->{'Response'} = "1";
		$cert_update_status->{'ErrorCode'} = "ECA0163";
		$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
		$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
		$msg = $msg."ECA0163: cs_cert_regeneration: Exception in CS $hostname cert generation.";
		$cert_update_status->{'logmessage'} = $msg;
	}
}

#Process server certificate generation renewal.
sub ps_cert_regeneration
{	
	my ($cert_update_status, $max_attempts) = @_;
	my @list_services = ("cxprocessserver");
	my %placeholder_list=();
	my $hostname =  hostname();
	$placeholder_list{PsName}=$hostname;
	my $msg = "";
	eval
	{
		my $ps_dh_file = $cs_key_file_path."ps.dh";
		my $ps_cert_file = $cs_crt_file_path."ps.crt";
		my $ps_fingerprint_file = $cs_fingerprint_file_path."ps.fingerprint";
		my $ps_key_file = $cs_key_file_path."ps.key";
		my @files;
		my $return_value = stop_services($cert_update_status,@list_services);		
		if ($return_value == 1)
		{
			return 0;
		}
		
		#Delete the old certificate backup files.
		my $del_bak_file_status = Utilities::del_files_with_pattern($cs_key_file_path,"ps.key.bak*",5,$cert_update_status);
		
		if ($del_bak_file_status == 1)
		{
			start_services_back($cert_update_status,@list_services);
			$msg = $msg."ECA0169: ps.key.bak deletion failed.";
			$cert_update_status->{'Response'} = "1";
			$cert_update_status->{'ErrorCode'} = "ECA0169";
			$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
			$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
			$cert_update_status->{'logmessage'} = $msg;
			return 0;
		}
		my $del_bak_file_status = Utilities::del_files_with_pattern($cs_key_file_path,"ps.dh.bak*",5,$cert_update_status);
		if ($del_bak_file_status == 1)
		{
			start_services_back($cert_update_status,@list_services);
			$msg = $msg."ECA0169: ps.dh.bak deletion failed.";
			$cert_update_status->{'Response'} = "1";
			$cert_update_status->{'ErrorCode'} = "ECA0169";
			$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
			$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
			$cert_update_status->{'logmessage'} = $msg;
			return 0;
		}
		my $del_bak_file_status = Utilities::del_files_with_pattern($cs_fingerprint_file_path,"ps.fingerprint.bak*",5,$cert_update_status);
		if ($del_bak_file_status == 1)
		{
			start_services_back($cert_update_status,@list_services);
			$msg = $msg."ECA0169: ps.fingerprint.bak deletion failed.";
			$cert_update_status->{'Response'} = "1";
			$cert_update_status->{'ErrorCode'} = "ECA0169";
			$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
			$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
			$cert_update_status->{'logmessage'} = $msg;
			return 0;
		}
		my $del_bak_file_status = Utilities::del_files_with_pattern($cs_crt_file_path,"ps.crt.bak*",5,$cert_update_status);
		if ($del_bak_file_status == 1)
		{
			start_services_back($cert_update_status,@list_services);
			$msg = $msg."ECA0169: ps.crt.bak deletion failed.";
			$cert_update_status->{'Response'} = "1";
			$cert_update_status->{'ErrorCode'} = "ECA0169";
			$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
			$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
			$cert_update_status->{'logmessage'} = $msg;
			return 0;
		}
		sleep(1);
		#Generating the process server new certificate.
		my $cert_gen_command = $gen_cert_bin_path."gencert.exe -n ps --dh";
		my $cert_gen_status;
		$cert_gen_status = `$cert_gen_command 2>&1`;
		$logging_obj->log("INFO","ps_cert_regeneration: Command: $cert_gen_command, Generate PS certificate and Output is:".$cert_gen_status." Status is:".$?);
		#Generating the process server new certificate failed.
		if ($? != 0) 
		{
			undo_ps_cert_files($cert_update_status);
			start_services_back($cert_update_status,@list_services);
			$logging_obj->log("INFO","ps_cert_regeneration: Could not generate PS certificate and Output is:".$cert_gen_status."Status is:".$?);
			$cert_update_status->{'Response'} = "1";
			$cert_update_status->{'ErrorCode'} = "ECA0169";
			$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
			$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
			$msg = $msg."ECA0169: ps_cert_regeneration: Could not generate PS certificate and Status is: $?";
			$cert_update_status->{'logmessage'} = $msg;
		}
		#Generating the process server new certificate got success.
		else
		{
			$logging_obj->log("INFO","ps_cert_regeneration: PS cetificate generation got success: Status:".$?."Output is:".$cert_gen_status);	
			#Validating the newly generated certificate files existence. If files exists, then proceed further.
			my $key_bak_count = @files = bsd_glob $cs_key_file_path."ps.key.bak*";
			my $dh_bak_count = @files = bsd_glob $cs_key_file_path."ps.dh.bak*";
			my $fp_bak_count = @files = bsd_glob $cs_fingerprint_file_path."ps.fingerprint.bak*";
			my $crt_bak_count = @files = bsd_glob $cs_crt_file_path."ps.crt.bak*";	
			$logging_obj->log("INFO","ps_cert_regeneration: cert file = $ps_cert_file, dh file = $ps_dh_file, key file = $ps_key_file, Fingerprint file = $ps_fingerprint_file, key bak count = $key_bak_count , dh bak count = $dh_bak_count , fp bak count = $fp_bak_count, crt bak count = $crt_bak_count");
			if (-e $ps_cert_file and 
			-e $ps_dh_file and 
			-e $ps_key_file and 
			-e $ps_fingerprint_file and 
			$key_bak_count and 
			$dh_bak_count and 
			$fp_bak_count and 
			$crt_bak_count)
			{
				start_services_back($cert_update_status,@list_services);
			}
			#If cert generation succeeded, but one of the cert related files not found.
			else
			{
				undo_ps_cert_files($cert_update_status);
				start_services_back($cert_update_status,@list_services);
				$logging_obj->log("INFO","ps_cert_regeneration: PS cert generation passed, but one or some of the files: $ps_cert_file,$ps_dh_file,$ps_key_file,$ps_fingerprint_file not exists in folder(s).");
				$cert_update_status->{'Response'} = "1";
				$cert_update_status->{'ErrorCode'} = "ECA0170";
				$placeholder_list{FolderName}=$ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\";
				$placeholder_list{FileNames}="private\\ps.key,certs\\ps.crt,fingerprints\\ps.fingerprint,private\\ps.dh";
				$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
				$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
				$msg = $msg."ECA0170: ps_cert_regeneration: PS cert generation passed, but one or some of the files: $ps_cert_file,$ps_dh_file,$ps_key_file,$ps_fingerprint_file not exists in folder(s)";
				$cert_update_status->{'logmessage'} = $msg;
			}
		}	
	};
	#This is for any generic exception, catches here.
	if ($@)
	{
		undo_ps_cert_files($cert_update_status);
		start_services_back($cert_update_status,@list_services);
		$logging_obj->log("EXCEPTION","ps_cert_regeneration: Exception in PS cert generation(ps_cert_regeneration sub routine):$@");
		$cert_update_status->{'Response'} = "1";
		$cert_update_status->{'ErrorCode'} = "ECA0169";
		$cert_update_status->{'PlaceHolders'} = \%placeholder_list;
		$cert_update_status->{'ErrorDescription'} = "SSL certificate renewal failed on server $hostname.";
		$msg = $msg."ECA0169: ps_cert_regeneration: Exception in PS cert generation.";
		$cert_update_status->{'logmessage'} = $msg;
	}	
}

#Sub routine to stop the services and on failure of stop, start the stopped services back.
sub stop_services
{
	my ($cert_update_status, @list_services_to_stop) = @_;
	my @list_of_services_stopped;
	my $return_value = 0;
	foreach my $service_name (@list_services_to_stop)
	{
		push(@list_of_services_stopped, $service_name);				
		stop_service($cert_update_status,$service_name,5,30);
		if ($cert_update_status->{'Response'} eq "0")
		{
			$return_value = 0;
			$logging_obj->log("INFO","Service $service_name is stopped successfully.");	
		}
		else
		{
			$return_value = 1;
			$logging_obj->log("INFO","Service $service_name stop failed. Hence returning without proceeding further just by doing stopped services starting back.");	
			my @reversed_order_of_services_to_start = reverse(@list_of_services_stopped);
			foreach $service_name (@reversed_order_of_services_to_start)
			{
				start_service($cert_update_status,$service_name,10,10);	
			}
			last;
		} 
	}
	return $return_value;
}

#Order of service starts important too.
sub start_services_back
{
	my($cert_update_status,@list_services_to_start) = @_;
	my @reversed_order_of_services_to_start = reverse(@list_services_to_start);
	foreach my $service_name (@reversed_order_of_services_to_start)
	{	
		#Checking the service status.
		my %status;
		Win32::Service::GetStatus('', $service_name, \%status);
		if ($status{"CurrentState"} ne undef)
		{
			#Ifservice is already in stopped mode, just start the service.
			if ($status{"CurrentState"} eq $service_status_code{'Stopped'}) 
			{
				$logging_obj->log("INFO","start_services_back: Service $service_name is in stop mode and starting now.");
				start_service($cert_update_status, $service_name,10,10);
				
			}
			else
			{
				$logging_obj->log("INFO","start_services_back: Service $service_name is is in running mode, hence, stopping and starting now.");
				#If WAS service is in running mode, just stop and start the service.
				stop_service($cert_update_status,$service_name,5,30);
				if ($cert_update_status->{'Response'} eq "0")
				{
					start_service($cert_update_status,$service_name,10,10);
				}
			}
		}
	}
}
#Sub routine to start the service with required attempts and required sleep time in between the attempts.
#Params: Global hash to handle response code and error description, Service name, Number of attempts, sleep time
sub start_service
{
	my($start_service_status, $service_name, $max_attempts, $sleep_time) = @_;
	my %status;
	my $start_counter = 0;
	my %placeholder_list=();
	my $hostname =  hostname();
	$placeholder_list{Name}=$hostname;
	Win32::Service::GetStatus('', $service_name, \%status);
	if ($status{"CurrentState"} ne undef)
	{
		if ($status{"CurrentState"} ne $service_status_code{'Running'}) 
		{
			while ($start_counter < $max_attempts) 
			{
				$logging_obj->log("INFO","start_service: Attempting to star service: $service_name .\n");
				Win32::Service::StartService('', $service_name);
				sleep($sleep_time);
				Win32::Service::GetStatus('', $service_name, \%status);
				if ($status{"CurrentState"} eq $service_status_code{'Running'}) 
				{
					$logging_obj->log("INFO","start_service: Started $service_name in $start_counter attempts.\n");
					last;
				}
				$start_counter++;
			}	
			Win32::Service::GetStatus('', $service_name, \%status);
			if ($status{"CurrentState"} ne $service_status_code{'Running'}) 
			{
				$logging_obj->log("INFO","start_service: Unable to start $service_name in $start_counter attempts.\n");
				$start_service_status->{'Response'} = "1";
				$start_service_status->{'ErrorCode'} = "ECA0166";
				$placeholder_list{ServiceName}=$service_name;
				$start_service_status->{'PlaceHolders'} = \%placeholder_list;
				$start_service_status->{'ErrorDescription'} = "Unable to start $service_name in $start_counter attempts.";
				$start_service_status->{'logmessage'} = "ECA0166: start_service: Unable to stop $service_name in $start_counter attempts.";
			}
		}
	}
}

#Sub routine to stop the service with required attempts and required sleep time in between the attempts.
#Params: Global hash to handle response code and error description, Service name, Number of attempts, sleep time
sub stop_service
{
	my($stop_service_status, $service_name, $max_attempts, $sleep_time) = @_;
	my %status;
	my $start_counter = 0;
	my %placeholder_list=();
	my $hostname =  hostname();
	$placeholder_list{Name}=$hostname;
	Win32::Service::GetStatus('', $service_name, \%status);
	if ($status{"CurrentState"} ne undef)
	{
		if ($status{"CurrentState"} ne $service_status_code{'Stopped'}) 
		{
			while ($start_counter < $max_attempts) 
			{
				$logging_obj->log("INFO","stop_service: Attempting to stop service: $service_name .\n");
				Win32::Service::StopService('', $service_name);
				sleep($sleep_time);
				Win32::Service::GetStatus('', $service_name, \%status);
				if ($status{"CurrentState"} eq $service_status_code{'Stopped'}) 
				{
					$logging_obj->log("INFO","stop_service: Stopped $service_name in $start_counter attempts.\n");
					last;
				}
				$start_counter++;
			}	
			
			Win32::Service::GetStatus('', $service_name, \%status);
			if ($status{"CurrentState"} ne $service_status_code{'Stopped'})
			{
				$logging_obj->log("INFO","stop_service: Unable to stop $service_name in $start_counter attempts.\n");
				$stop_service_status->{'Response'} = "1";
				$stop_service_status->{'ErrorCode'} = "ECA0168";
				$placeholder_list{ServiceName}=$service_name;
				$stop_service_status->{'PlaceHolders'} = \%placeholder_list;
				$stop_service_status->{'ErrorDescription'} = "Unable to stop Service $service_name on server $hostname";
				$stop_service_status->{'logmessage'} = "ECA0168: stop_service: Unable to stop $service_name in $start_counter attempts.";
			}
		}
	}
}

sub undo_cs_cert_files
{
	my($cert_update_status) = @_;
	$logging_obj->log("INFO","undo_cs_cert_files.");
	my $rename_bak_file_status = Utilities::move_files_with_pattern($cs_crt_file_path, "cs.crt.bak*", 5, "cs.crt", $cert_update_status);
	my $rename_bak_file_status = Utilities::move_files_with_pattern($cs_key_file_path, "cs.key.bak*", 5, "cs.key", $cert_update_status);
	my $rename_bak_file_status = Utilities::move_files_with_pattern($cs_key_file_path, "cs.keycert.pfx.bak*", 5, "cs.keycert.pfx", $cert_update_status);
	my $rename_bak_file_status = Utilities::move_files_with_pattern($cs_fingerprint_file_path, "cs.fingerprint.bak*", 5, "cs.fingerprint", $cert_update_status);
	
}

sub undo_ps_cert_files
{		
	my($cert_update_status) = @_;
	$logging_obj->log("INFO","undo_ps_cert_files.");
	my $rename_bak_file_status = Utilities::move_files_with_pattern($cs_crt_file_path, "ps.crt.bak*", 5, "ps.crt", $cert_update_status);
	my $rename_bak_file_status = Utilities::move_files_with_pattern($cs_key_file_path, "ps.key.bak*", 5, "ps.key", $cert_update_status);
	my $rename_bak_file_status = Utilities::move_files_with_pattern($cs_key_file_path, "ps.dh.bak*", 5, "ps.dh", $cert_update_status);
	my $rename_bak_file_status = Utilities::move_files_with_pattern($cs_fingerprint_file_path, "ps.fingerprint.bak*", 5, "ps.fingerprint", $cert_update_status);
	
}

sub tman_status
{
	my($tman_verify) = @_;
	my $tmansvc_service_status = `sc query tmansvc | findstr "STOPPED RUNNING"`;
	my $return_val = 0;
		
	if (index(lc($tmansvc_service_status), lc($tman_verify)) != -1) 
	{
		$return_val = 1;
	}
	
	return $return_val;
}

sub checkCertExpiryAlerts
{
	my($conn, $certExpiryDate, $cx_type, $host_guid, $host_name) = @_;
	my($type, $err_summary, $err_message, $placeHolderarr, $err_placeholders, $err_id, $err_code);
	my $currentTime = time();
	
	# Get the number of days.
	my $certDiffDays = floor(($certExpiryDate - $currentTime) / 86400);		
	my $certDiffWeekly = ($certDiffDays < 30 && $certDiffDays / 7) ? 0 : -1;
	
	$logging_obj->log("INFO","checkCertExpiryAlerts Expiry Days = $certDiffDays, Weekly Value = $certDiffWeekly  \n");
		
	# Generate alerts in the following cases:
	# In case of certExpiryDate is in between 2 months to 1 month generate alert for every 15 days.
	# In case of certExpiryDate less than 1 month generate alert weekly once.
	# In case of certExpiryDate less than 1 week generate alert daily.
	if($certDiffDays == 60 || $certDiffDays == 45 || $certDiffWeekly == 0 || $certDiffDays < 7)
	{
		$err_id = $host_guid;
		$placeHolderarr->{"CsName"} = $host_name;
		
		if($certDiffDays <= 0)
		{
			$err_summary = "SSL Cert Expired";
			$err_message = "The SSL certificate on $host_name has expired.";
			$err_code = "EA0615";
		}
		else
		{
			$err_summary = "SSL Cert Expiry Warning";
			$err_message = "The SSL certificate on $host_name is going to expire in $certDiffDays days.";
			$placeHolderarr->{"NoOfDays"} = $certDiffDays;
			$err_code = "EA0616";
		}
		

		if($certDiffDays < 7)
		{
			# Deleting the old alert as we want to generate new alert every day.
			my $delete_sql = "DELETE FROM dashboardAlerts WHERE errCode IN ('EA0615','EA0616') AND (UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(errStartTime) > 86400)";
			$conn->sql_query($delete_sql);
			$logging_obj->log("INFO","Logging dashboard alert for CS checkCertExpiry function\n sql = $delete_sql");
		}
		
		$err_placeholders = serialize($placeHolderarr);
		$logging_obj->log("INFO","Logging dashboard alert for CS checkCertExpiry function\n");
		&Messenger::add_error_message ($conn, $err_id, "SSL_CERT_ALERTS", $err_summary, $err_message, $host_guid, $err_code, $err_placeholders);
	}
}


sub monitor_data_for_telemetry
{	
	my ($package, $extra_params) = @_;
	
	eval
	{
		my $data_hash = $extra_params->{'data_hash'};
		$logging_obj->log("DEBUG","Entered monitor_data_for_telemetry function");
		
		my $batch = &get_tmid_batch($data_hash);
		my $sys_boot_unix_time = &system_boot_unix_time();
		my $cachefolder_usedvol_size = &get_cache_vol_usage_stats();
		
		foreach my $batch_id (keys %$batch)
		{		
			my @tmids = @{$batch->{$batch_id}};
			$logging_obj->log("DEBUG","Entered monitor_data_for_telemetry function for batch '".$batch."'".Dumper(\@tmids));
            my $tmid_str = join("','",@tmids);
			$logging_obj->log("DEBUG","Entered monitor_data_for_telemetry function for '".$tmid_str."'");
			
            my @updated_pairs_ids;
			my ($keys,$cluster_ids) = &getPSPairDetails($data_hash,@tmids);
			my @keys = @$keys;
			my @cluster_id = @$cluster_ids;
			my @telemetry_log_data = ();
		
			if(scalar @keys)
            {		
                foreach my $key (@keys)
                {
					my @values = split(/\!\&\*\!/, $key);
					my $src_id = $values[0];
                    my $src_vol = $values[1];					
                    my $dest_id = $values[2];
                    my $dest_vol = $values[3];
                    my $pair_id = $values[4];
                    my $is_cluster = $values[5];
					
					if(!(grep  $_ eq $pair_id,@updated_pairs_ids ))
					{
						my $telemetry_log_str = TimeShotManager::monitordatafortelemetry($src_vol,$src_id,$dest_id, $dest_vol,$is_cluster,\@cluster_id,$data_hash,$pair_id, $sys_boot_unix_time, $cachefolder_usedvol_size);
						
						if($telemetry_log_str ne "")
						{
							push(@telemetry_log_data, $telemetry_log_str);
						}
						$logging_obj->log("DEBUG","monitor_data_for_telemetry telemetry_log_str: $telemetry_log_str");
					}					
                }
            }
			if(@telemetry_log_data)
			{
				TimeShotManager::telemetry_log_monitor(@telemetry_log_data);
			}
        }
		$logging_obj->log("DEBUG","End of monitor_data_for_telemetry function");
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","monitor_data_for_telemetry : ".$@);
    }
}

sub system_boot_unix_time
{
	my $system_boot_unix_timestamp = 0;
	
	eval
	{
		my $system_boot_time_output = `wmic path  win32_operatingsystem get LastBootUpTime`;
		$system_boot_time_output  =~ s/^\s+|\s+$//g;
		
		if( $system_boot_time_output ne "" )
		{
			my @system_boot_time_arr = split(/\s+/, $system_boot_time_output);
			if( @system_boot_time_arr && $system_boot_time_arr[1] ne "" )
			{
				my $system_boot_time_str = $system_boot_time_arr[1];
				if($system_boot_time_str =~ m/^[0-9]{14}/)
				{
					my ($year, $month, $day, $hour, $min, $sec) = $system_boot_time_str =~ m{^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})};
					my $formated_system_boot_time_str = sprintf('%04d-%02d-%02d %02d:%02d:%02d', $year, $month, $day, $hour, $min, $sec);
					$system_boot_unix_timestamp = Utilities::datetimeToUnixTimestamp($formated_system_boot_time_str);
					
					if( !$system_boot_unix_timestamp || $system_boot_unix_timestamp eq "" )
					{
						$logging_obj->log("EXCEPTION", "system_boot_unix_time:: system_boot_unix_timestamp is NULL. system_boot_time_output: $system_boot_time_output, formated_system_boot_time_str: $formated_system_boot_time_str");
						$system_boot_unix_timestamp = 0;
					}
				}
				else
				{
					$logging_obj->log("EXCEPTION", "system_boot_unix_time:: system_boot_time_str is not in expected format. system_boot_time_str: $system_boot_time_str and system_boot_time_output: $system_boot_time_output");
				}
				
			}
		}
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","Exception occurred in system_boot_unix_time: ".$@);
	}	
	
	return $system_boot_unix_timestamp;
}

sub get_cache_vol_usage_stats
{
	my %cachefolder_and_usedvol_size = ();
		
	$cachefolder_and_usedvol_size{"CacheFldSize"} = 0;
	$cachefolder_and_usedvol_size{"UsedVolSize"} = 0;
	$cachefolder_and_usedvol_size{"FreeVolSize"} = 0;
		
	# Calculate home folder size
	$home_dir_size = 0; # Global variable to get size of "/home" directory
	$folder_to_query = ''; # Leave empty, since we need to get the size including all the sub-folders
	find(\&get_folder_size, $BASE_DIR);
	$cachefolder_and_usedvol_size{"CacheFldSize"} = $home_dir_size;
	
	# Get used and free size of volume, where home folder resides
	my $usedVolSize = 0;
	my $freeVolSize = 0;
	if (open(PS, "$DF_CMD -k -P $BASE_DIR|"))
	{
		my $i = 0;
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
			my ($filesystem, $OneKblocks, $usedBytes, $availableBytes, $use, $mounted_on) = split (' ', $process);
			
			$usedVolSize = $usedBytes;
			$freeVolSize = $availableBytes;
		}
		
		close PS;
	}
	$cachefolder_and_usedvol_size{"UsedVolSize"} = $usedVolSize;
	$cachefolder_and_usedvol_size{"FreeVolSize"} = $freeVolSize;
	return \%cachefolder_and_usedvol_size;
}

sub get_folder_size
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
		$home_dir_size += $tmp_size;
	};
	if ($@) 
	{		
		$logging_obj->log("EXCEPTION","EXCEPTION occurred in get_folder_size :: ".$@);
	}	
}

sub monitor_cs_telemetry_folders
{
	my($cs_log_directory,$cs_telemetry_directory,$cs_log_directory_count,$cs_telemetry_directory_count);
	my (%log_data);
	my (@file_contents,@cs_log_directory_count,@cs_telemetry_directory_count,@directory_list);
	my (@log_directory_list);
	my($file_path,$dir_path,$log_dir_path,$unlink_status);
	
	$file_path = $cs_installation_path.$TELEMETRY_FOLDER_COUNT_FILE_PATH.$WIN_SLASH.$TELEMETRY_FOLDER_COUNT_FILE;
	$cs_log_directory = $cs_installation_path.$CS_LOGS_PATH;
	$cs_telemetry_directory = $cs_installation_path.$CS_TELEMETRY_PATH;
	@cs_log_directory_count = Utilities::read_directory_contents($cs_log_directory);
	@cs_telemetry_directory_count = Utilities::read_directory_contents($cs_telemetry_directory);
	
	foreach my $log_dir_list (@cs_log_directory_count)
	{
		$log_dir_path = $cs_log_directory.$WIN_SLASH.$log_dir_list;
		if(-d $log_dir_path)
		{
			push(@log_directory_list, $log_dir_list);
		}
	}
	
	foreach my $dir_list (@cs_telemetry_directory_count)
	{
		$dir_path = $cs_telemetry_directory.$WIN_SLASH.$dir_list;
		if(-d $dir_path)
		{
			push(@directory_list, $dir_list);
		}
	}
	
	if((scalar(@directory_list)+scalar(@log_directory_list)) >= $APPINSIGHTS_WRITE_DIR_COUNT_LIMIT)
	{
		if(-e $file_path)
		{
		  open (FILEH, "<$file_path");
		  my @file_contents = <FILEH>;
		  close (FILEH);
		  
		  $log_data{"Message"} = "monitor_cs_telemetry_folders,File exist";
		  $log_data{"FileCreationTime"} = Dumper(@file_contents);
		  $logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		}
		else
		{
			my $current_utc_time = &get_current_utc_time();
			
			$log_data{"Message"} = "monitor_cs_telemetry_folders,File creating";
			$log_data{"FileCreationTime"} = $current_utc_time;
			$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
			
			open(my $fh, '>', $file_path);
			print $fh $current_utc_time;
			close($fh);
		}
	}
	else
	{
		if(-e $file_path)
		{
			my $current_utc_time = &get_current_utc_time();
			
			open (FILEH, "<$file_path");
		    my @file_contents = <FILEH>;
		    close (FILEH);
			
			for(my $i=0; $i<$MAX_RETRY; $i++)
			{
				$unlink_status = unlink($file_path);
				last if($unlink_status);
			}
			
			$log_data{"Message"} = "monitor_cs_telemetry_folders,File exist and removing";
		    $log_data{"FileCreationTime"} = Dumper(@file_contents);
		    $log_data{"FileRemoveTime"} = $current_utc_time;
		    $logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		}
	}
	
}

sub get_current_utc_time
{
	my $dt = time();
	my $year = strftime ('%Y', gmtime($dt));
	my $mon = strftime ('%m', gmtime($dt));
	my $mday = strftime ('%d', gmtime($dt));
	my $hour = strftime ('%H', gmtime($dt));
	my $min = strftime ('%M', gmtime($dt));
	my $sec = strftime ('%S', gmtime($dt));
	
	my $datetime   = $year."-".$mon."-".$mday." ".$hour.":".$min.":".$sec;
	
	return $datetime;
}

1;
