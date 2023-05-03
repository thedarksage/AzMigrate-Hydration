# Package for MDSErrors
#
############## HEADER #########################################################
# Description :  This module is responsible for collecting the various logs 
#                which will be captured in to the MDS logs.
#                 1. unmarshals - unmarshal.txt
#                 2. Stub Call Auth Failures - Authentication.log
#                 3. API auth failures and Invalid API Response - api_error.log
#                 4. 500 Response - php_error.log
#                 5. Agent Heartbeat error
#                 6. Server space
#                 7. Service Status
#################End of the Header#############################################

package MDSErrors;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use bytes;
use POSIX;
use Common::Constants;
use Data::Dumper;
use Utilities;
use Common::Log;
my $heartbeat_threshold_time = 1800;
my $max_chunk_size = 8 * 1024;
my $max_batch_size = 7340032; # 7MB
my $MDS_BACKEND_ERRORS_ID = Common::Constants::MDS_BACKEND_ERRORS;
my $MDS_BACKEND_ERROR_CODE = Common::Constants::MDS_BACKEND_ERROR_CODE;
my $cs_installation_path = Utilities::get_cs_installation_path();
my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
my $MDS = Common::Constants::LOG_TO_MDS;
my $TELEMETRY = Common::Constants::LOG_TO_TELEMETRY;
my $MDS_AND_TELEMETRY = Common::Constants::LOG_TO_MDS_AND_TELEMETRY;
my $METADATA_JSON = Common::Constants::METADATA_JSON;
my $SLASH = Common::Constants::SLASH;
my $CS_TELEMETRY_PATH = Common::Constants::CS_TELEMETRY_PATH;
my $CS_LOGS_PATH = Common::Constants::CS_LOGS_PATH;
my $cs_telemetry_directory = $cs_installation_path.$CS_TELEMETRY_PATH;
my $cs_log_directory = $cs_installation_path.$CS_LOGS_PATH;
my $logging_obj = new Common::Log();
our $additional_info = undef;

sub new
{
    my ($class) = shift;
    
    my $self = {};
    
    bless($self, $class);
}

sub update_backend_errors_to_mds
{
	my ($self, $extra_params) = @_;
	my %log_data;
	my $conn = $extra_params->{'conn'};
	
	my %telemetry_metadata = ('RecordType' => 'LogMonitor');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
		
	$self->get_error_files_data();
	$self->check_agent_heartbeat($conn);
	$self->get_ps_errors($conn);
	$self->get_version_mismatch($conn);
	$self->get_dashboard_alerts($conn);
	$self->get_agents_error_files_data($conn);
	$self->validate_telemetry_directory_existence();
	$self->generate_metadata_file();
	$self->move_telemetry_log_file();
	return;
}

sub get_error_files_data
{
	my ($self) = @_;
	
	my @var_error_log_files = ("unmarshal.txt", "api_error.log", "php_error.log", "php_sql_error.log", "perl_sql_error.log", "tmansvc.log","perldebug.log","VCenter.log","clear_replication.log");
	
	foreach my $log (@var_error_log_files)
	{
		my $var_log_file_path = $cs_installation_path."\\home\\svsystems\\var\\";
		$self->push_error_files_data($var_log_file_path, $log);
	}
}

sub get_agents_error_files_data
{
	my ($self, $conn) = @_;
	my ($agent_hardware_type, $agent_infra_host_id, $agent_infra_id);
	my ($infra_host_ids, $infra_inventory_ids);
	
	my $agent_file_path = $cs_installation_path."\\home\\svsystems\\var\\hosts\\";
	
	my $get_agent_guids_sql = "SELECT 
									id, 
									ipAddress, 
									name, 
									agentRole, 
									sentinel_version, 
									osType 
								FROM 
									hosts 
								WHERE 
									sentinelEnabled = '1'";
	my $agent_guids_hash = $conn->sql_exec($get_agent_guids_sql);
	
	my $get_infra_vm_ids_sql = "SELECT infrastructureHostId, hostId FROM infrastructurevms WHERE hostId !='' ";
	my $infra_vm_ids = $conn->sql_get_hash($get_infra_vm_ids_sql, "hostId", "infrastructureHostId");
	my $size = scalar keys %$infra_vm_ids;
	
	if($size)
	{
		my @infra_vm_array = values %$infra_vm_ids;
		my $infra_host_str = join("','", @infra_vm_array);
		
		my $get_infra_host_ids_sql = "SELECT 
										infrastructureHostId, 
										inventoryId 
									FROM 
										infrastructurehosts 
									WHERE 
										infrastructureHostId in ('".$infra_host_str."')";
		$infra_host_ids = $conn->sql_get_hash($get_infra_host_ids_sql, "infrastructureHostId", "inventoryId");
		
		my $infra_host_id_size = scalar keys %$infra_host_ids;
		if($infra_host_id_size)
		{
			my @infra_host_array = values %$infra_host_ids;
			my $infra_inventory_str = join("','", @infra_host_array);
			
			my $get_infra_inventory_ids_sql = "SELECT 
												inventoryId, 
												hostType 
											 FROM 
												infrastructureinventory 
											 WHERE 
												inventoryId in ('".$infra_inventory_str."')";
			$infra_inventory_ids = $conn->sql_get_hash($get_infra_inventory_ids_sql, "inventoryId", "hostType");
		}
	}
	
	foreach my $data (@$agent_guids_hash)
	{
		$additional_info = undef;
		my $agent_id = $data->{'id'};
		my $ipAddress = $data->{'ipAddress'};
		my $host_name = $data->{'name'};
		my $agent_role = $data->{'agentRole'};
		my $agent_version = $data->{'sentinel_version'};
		my $osType = $data->{'osType'};
		my $agent_file = $agent_file_path.$agent_id.".log";
		
		if(exists ($infra_vm_ids->{$agent_id}))
		{
			$agent_infra_host_id = $infra_vm_ids->{$agent_id};
			if(exists ($infra_host_ids->{$agent_infra_host_id}))
			{
				$agent_infra_id = $infra_host_ids->{$agent_infra_host_id};
				if(exists ($infra_inventory_ids->{$agent_infra_id}))
				{
					$agent_hardware_type = $infra_inventory_ids->{$agent_infra_id};
				}				
			}
		}
		
		if($agent_hardware_type eq '')
		{
			$agent_hardware_type = "N/A";
		}
		
		if(-e $agent_file)
		{
			my $file_data = &file_get_contents($agent_file);
			
			if($file_data)
			{
				$additional_info = "Component Role: ".$agent_role.", HostId - ".$agent_id.", Name - ".$host_name.", IP Address - ".$ipAddress.", agent_bits_version - ".$agent_version.", AgentOS - ".$osType.", hardware_type - ".$agent_hardware_type.", \n";
				my $mds_data = {'errorCodeName' => 'Agent error log', 'eventName' => 'AGENT_ERROR', 'errorLog' => $file_data};
				$self->update_data_to_mds($mds_data);
				truncate(open(ERR_FH, ">$agent_file"), eof ERR_FH);
			}
		}
	}
	$additional_info = undef;
}

sub file_get_contents
{
	my ($file_name) = @_;
	
	my $file_content;
	if(-e $file_name && -s $file_name)
	{
		local $/ = undef;
		open(ERR_FH, $file_name);
		$file_content = <ERR_FH>;
		close(ERR_FH);
		local $/ = "\n";
	}
	
	return $file_content;
}

sub check_agent_heartbeat
{
    my ($self, $conn) = @_;
    my $mds_data;
	my (%infra_ids,%log_data);
    my $check_heartbeat_sql = "SELECT 
                                    id, 
                                    name, 
                                    ipAddress, 
                                    sentinelEnabled, 
                                    outpostAgentEnabled, 
                                    processServerEnabled,
									appAgentEnabled,
                                    agentRole,
                                    lastHostUpdateTime as agentHeartBeatTimestamp,
                                    FROM_UNIXTIME(lastHostUpdateTime) as agentHeartBeat,
                                    UNIX_TIMESTAMP(lastHostUpdateTimePs) as psHeartBeatTimestamp,
                                    lastHostUpdateTimePs as psHeartBeat,
									UNIX_TIMESTAMP(lastHostUpdateTimeApp) as appHeartBeatTimestamp,
									lastHostUpdateTimeApp as appHeartBeat,
                                    prod_version,
                                    psVersion,
                                    psPatchVersion,
                                    patchInstallTime,
                                    PatchHistoryVX,
									UNIX_TIMESTAMP(now()) currentTimestamp
                            FROM
                                    hosts
                            WHERE
                                    id != '".Common::Constants::PROFILER_HOST_ID."' AND
                                    (((sentinelEnabled = 1 OR outpostAgentEnabled = 1) AND (UNIX_TIMESTAMP(now()) - lastHostUpdateTime) >= '".$heartbeat_threshold_time."') OR
                                    ( processServerEnabled = '1' AND (UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(lastHostUpdateTimePs)) >= '".$heartbeat_threshold_time."')OR
                                    ( appAgentEnabled = '1' AND (UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(lastHostUpdateTimeApp)) >= '".$heartbeat_threshold_time."')
									)";
    my $host_data = $conn->sql_exec($check_heartbeat_sql);
	my ($structured_host_data,$structured_data);
		
	my $get_infra_ids_sql = "select hostId,infrastructureVMId from infrastructureVms where hostId!=''";
	my $infra_ids = $conn->sql_get_hash($get_infra_ids_sql, "hostId", "infrastructureVMId");
	
	foreach my $data (@$host_data)
	{
        my ($component, $heartbeat, $heartbeat_timestamp, $heart_beat_interval, $version, $patch_details,$infra_vm_id);
        $structured_data = "";
		
		my $host_id = $data->{'id'};
		if (exists $infra_ids->{$host_id}) 
		{
			$infra_vm_id = $infra_ids->{$host_id};
		} 
		else 
		{
			$infra_vm_id = "";
		}
		
		if(($data->{'processServerEnabled'}) && (($data->{'currentTimestamp'} - $data->{'psHeartBeatTimestamp'}) > $heartbeat_threshold_time))
        {
            $component = "ProcessServer";
            $heartbeat = $data->{'psHeartBeat'};
            $heartbeat_timestamp = $data->{'psHeartBeatTimestamp'};
            $version = $data->{'psVersion'};
            $patch_details = $data->{'psPatchVersion'};
			$heart_beat_interval = $data->{'currentTimestamp'} - $data->{'psHeartBeatTimestamp'};
			
			$structured_data .= " component: ".$component.", Heartbeat Timestamp : ".$heartbeat_timestamp.", Heartbeat Time - ".$heartbeat.", Last HeartBeat Since - ".$heart_beat_interval."(seconds), Version - ".$version.", Patch Details - ".$patch_details."\n";	
        }
		
        if(($data->{'sentinelEnabled'} eq "1" || $data->{'outpostAgentEnabled'} eq "1") && (($data->{'currentTimestamp'} - $data->{'agentHeartBeatTimestamp'}) > $heartbeat_threshold_time))
		{
            $component = ($data->{'agentRole'} eq "MasterTarget") ? "MasterTarget(VxAgent)" : "ScoutAgent(VxAgent)";
			$version = $data->{'prod_version'};
			$patch_details = $data->{'PatchHistoryVX'};
			$heartbeat = $data->{'agentHeartBeat'};
			$heartbeat_timestamp = $data->{'agentHeartBeatTimestamp'};
			$heart_beat_interval = $data->{'currentTimestamp'} - $data->{'agentHeartBeatTimestamp'};
			
			$structured_data .= " component: ".$component.", Heartbeat Timestamp : ".$heartbeat_timestamp.", Heartbeat Time - ".$heartbeat.", Last HeartBeat Since - ".$heart_beat_interval."(seconds), Version - ".$version.", Patch Details - ".$patch_details."\n";	
		}
		
		if(($data->{'appAgentEnabled'} eq "1") && (($data->{'currentTimestamp'} - $data->{'appHeartBeatTimestamp'}) > $heartbeat_threshold_time))
		{
			$component = ($data->{'agentRole'} eq "MasterTarget") ? "MasterTarget(AppAgent)" : "ScoutAgent(AppAgent)";
			$version = $data->{'prod_version'};
			$patch_details = $data->{'PatchHistoryVX'};
			$heartbeat = $data->{'appHeartBeat'};
			$heartbeat_timestamp = $data->{'appHeartBeatTimestamp'};
			$heart_beat_interval = $data->{'currentTimestamp'} - $data->{'appHeartBeatTimestamp'};
			
			$structured_data .= " component: ".$component.", Heartbeat Timestamp : ".$heartbeat_timestamp.", Heartbeat Time - ".$heartbeat.", Last HeartBeat Since - ".$heart_beat_interval."(seconds), Version - ".$version.", Patch Details - ".$patch_details."\n";	
		}
		
		$structured_host_data .= "HostId - ".$data->{'id'}.", VmId - ".$infra_vm_id.", Name - ".$data->{'name'}.", IP Address - ".$data->{'ipAddress'}.",".$structured_data."\n";
	}
	
    if($structured_host_data)
	{
		$mds_data = {'errorCodeName' => 'Agent-CS Communication', 'errorLog' => $structured_host_data};
		$self->update_data_to_mds($mds_data);
		
		my %telemetry_metadata = ('RecordType' => 'LogMonitor');
		$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
		$log_data{"Message"} = "Agent-CS Communication";
		$log_data{"Heartbeat"} = $structured_host_data;
		$logging_obj->log("INFO",\%log_data,$TELEMETRY);
	}
}

sub update_data_to_mds
{
	my ($self, $mds_data) = @_;
	
	# Get CS DB connection
	my $conn = &tmanager::get_db_connection();
	
	# Remove non-ascii characters from the error_log
	$mds_data->{'errorLog'} =~ s/[^\x0A\x20-\x7E]//g;
	
	# Get chunked data
	my $data_chunked = $self->chunk_error_data($mds_data->{'errorLog'});	
	
	my $num_chunks = ceil(bytes::length($mds_data->{'errorLog'})/$max_chunk_size);
	
	my $activity_id = $MDS_BACKEND_ERRORS_ID;
	my $client_request_id = '';
	my $event_name = 'CS_BACKEND_ERRORS';
	my $error_code = $MDS_BACKEND_ERROR_CODE;
	my $error_type = 'ERROR';
	
	$activity_id = (defined ($mds_data->{'activityId'})) ? $mds_data->{'activityId'} : $MDS_BACKEND_ERRORS_ID;		
	$client_request_id = (defined ($mds_data->{'clientRequestId'})) ? $mds_data->{'clientRequestId'} : $client_request_id;	
	$event_name = (defined ($mds_data->{'eventName'})) ? $mds_data->{'eventName'} : $event_name;
	$error_code = (defined ($mds_data->{'errorCode'})) ? $mds_data->{'errorCode'} : $error_code;
	
	if ($mds_data->{'errorCode'} eq '1000' && $num_chunks > 0)
	{
		my $del_sql = "DELETE FROM MDSLogging WHERE activityId = '$mds_data->{'activityId'}'";
		$conn->sql_query($del_sql);
	}
	
	# Insert each one to the MDSLogging table
	my $error_upload_sql = "INSERT 
							INTO
								MDSLogging
								(activityId, clientRequestId, eventName, errorCode, errorCodeName, errorType, errorDetails, logCreationDate)
							VALUES
							";
	my $error_db_values;
	foreach my $chunk (@$data_chunked)
	{
		$chunk = $conn->sql_escape_string($chunk);
		$error_db_values .= ($error_db_values) ? "," : "";
		$error_db_values .= "('$activity_id', '$client_request_id', '$event_name', '$error_code', '$mds_data->{'errorCodeName'}', '$error_type', '$chunk', now())";
	
		# Get the size of batch sql and if it is >= 7 MB then push the batch to database.
		my $mysql_length = ceil(bytes::length($error_db_values));
		if($mysql_length >= $max_batch_size)
		{
			my $error_sql = $error_upload_sql;
			if($error_db_values)
			{
				$error_sql .= $error_db_values;
				$conn->sql_query($error_sql);
				$error_db_values = "";
				$error_sql = "";
			};
		}
	}

	if($error_db_values)
	{
		$error_upload_sql .= $error_db_values;
		$conn->sql_query($error_upload_sql);
	};
}

sub chunk_error_data
{
	my ($self, $error_data) = @_;

    my @chunks;
	
	my $CS_IP = $AMETHYST_VARS->{CS_IP};
	my $CS_GUID = $AMETHYST_VARS->{HOST_GUID};
	substr($CS_GUID, -5, 5) = "#####";
	
	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime(time);
	$year = $year + 1900;
	$mon += 1;
	
	my $datetime   = "( ".$year."-".$mon."-".$mday." ".$hour.":".$min.":".$sec . " ),";
	
	my $version_data = &file_get_contents($cs_installation_path."\\home\\svsystems\\etc\\version");
	$version_data =~ s/^\s+//;
	my @CS_VERSION = split(/\n/, $version_data);
	
	# Get number of chunks to be made
    my $num_chunks = ceil(bytes::length($error_data)/$max_chunk_size);
	
	# Chunk the data and push to an array
	for(my $di = 0; $di < $num_chunks; $di++)
	{
		my $chunk_string = "Date: ".$datetime." CS_IP: ".$CS_IP.", CS_GUID - ".$CS_GUID.", CS_VERSION - ".$CS_VERSION[0].", ";
		my $chunk_data = bytes::substr($error_data, $di*$max_chunk_size, $max_chunk_size);
		
		if(defined($additional_info))
		{
			$chunk_string .= $additional_info;
		}
		$chunk_string .= $chunk_data;
		push(@chunks, "Sequence:" .($di+1). "\n" .$chunk_string);
	}
    
	# Return the array ref
    return \@chunks;
}

sub get_ps_errors
{
	my ($self, $conn) = @_;
	my %log_data;
	my ($error_content,$id,$ipAddress,$psVersion,$service_file_str,$ps_error_content);
	my $PS_FILE_PATH = $cs_installation_path."\\home\\svsystems\\SystemMonitorRrds";
	
	my $get_ps_guids_sql = "SELECT id, ipAddress, psVersion FROM hosts WHERE processServerEnabled = '1'";
	my $ps_guids_hash = $conn->sql_exec($get_ps_guids_sql);
	
	foreach my $ps_id (@$ps_guids_hash)
	{
		$ps_error_content ="";
		$id = $ps_id->{'id'};
		$ipAddress = $ps_id->{'ipAddress'};
		$psVersion = $ps_id->{'psVersion'};
		# Statistics File
		
		my $perf_file = $PS_FILE_PATH."/".$id."-perf.txt";
		if(-e $perf_file)
		{
			my $perf_content = &file_get_contents($perf_file);
			my @perf_contents = split(/:/, $perf_content);
			my $free_space = $perf_contents[3];
			my $tot_free_space = $perf_contents[4];
			my $used_free_space = $perf_contents[5];
			my $avail_free_space = $tot_free_space - $used_free_space;
			# If freespace more than 90%
			my $freespace_percent = ($avail_free_space/$tot_free_space) * 100;
			if($freespace_percent > 90)
			{
				$ps_error_content .= "used disk size is more than 90%. Total Size - $tot_free_space, Available Space - avail_free_space, UI Data  - $free_space\n";
			}
		}
		
		# Services File
		my $service_file = $PS_FILE_PATH."/".$id."-service.txt";
		if(-e $service_file)
		{
			my $service_content = &file_get_contents($service_file);
			my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$fsize,$atime,$mtime,$ctime,$blksize,$blocks) = stat($service_file);
			my $local_time	= localtime($mtime);
			my @service_arr = split(/:/, $service_content);
			
			# If any service is in stopped state
			$ps_error_content .= "Service : monitor is stopped\n"  if($service_arr[1] != 1);
			$ps_error_content .= "Service : monitor_disks is stopped\n" if($service_arr[3] != 1);
			$ps_error_content .= "Service : Volsync Count : ".$service_arr[4]." running \n" if($service_arr[4] < 12);
			$ps_error_content .= "Service : cxps is stopped\n" if($service_arr[12] != 1);
			$ps_error_content .= "Service : monitor_ps is stopped\n" if($service_arr[2] != 1);
			$ps_error_content .= "Service : pushinstalld is stopped\n" if($service_arr[7] != 1);
			$service_file_str = "Service file $service_file, last modified timestamp:$mtime, local time:$local_time";
			$ps_error_content = "\n On the PS : ps_id (".$id." ),ipAddress (".$ipAddress." ),psVersion (".$psVersion." ) ".$ps_error_content.",".$service_file_str if($ps_error_content);
		}
		
		$error_content .= $ps_error_content;
	}
	
	my $mds_data = {'errorCodeName' => 'PS-ERRORS', 'errorLog' => $error_content};
	$self->update_data_to_mds($mds_data);
	
	if($error_content)
	{
		my %telemetry_metadata = ('RecordType' => 'LogMonitor');
		$logging_obj->set_telemetry_metadata(%telemetry_metadata);
		
		$log_data{"Message"} = "PS-ERRORS";
		$log_data{"Errors"} = $error_content;
		$logging_obj->log("INFO",\%log_data,$TELEMETRY);
	}
}

sub get_version_mismatch
{
	my ($self, $conn) = @_;
	
	my $query = "SELECT
						name,
						ipaddress,
						id,
						sentinel_version,
						fr_version
						sentinelEnabled,
						outpostAgentEnabled,
						processServerEnabled,
						psVersion,
						agentRole,
						PatchHistoryVX,
						psPatchVersion 
					FROM 
						hosts 
					WHERE 
						name <> 'InMageProfiler'
					AND (sentinelEnabled = 1 OR outpostAgentEnabled = 1 OR processServerEnabled = 1)
					ORDER BY 
						PatchHistoryVX,PatchHistoryFX 
					DESC";
	my $result = $conn->sql_exec($query);

	my $raw_data = &file_get_contents($cs_installation_path."\\home\\svsystems\\etc\\version");
	$raw_data =~ s/^\s+//;
	my @str_parts = split(/_/, $raw_data);
	my $structured_host_data;
	
	#sample raw_date - RELEASE_6-00-1_GA_2368_Jun_27_2011_InMage
	#$str_parts[1] will contain the version number 6-00-1
	#$str_parts[2] will contain the build release GA
	
	my $cx_version = $str_parts[1];
	my $cs_patch_version = &get_cs_patch_version();
	
	foreach my $row (@{$result})
	{
		my @version_data;
		my ($component, $patch_version);
		
		if($row->{'sentinelEnabled'} eq "1" || $row->{'outpostAgentEnabled'} eq "1")
		{
			@version_data = ($row->{'sentinel_version'}) ? split(/_/, $row->{'sentinel_version'}) : split(/_/, $row->{'fr_version'});
			$component = ($row->{'agentRole'} eq "MasterTarget") ? "MasterTarget" : "ScoutAgent";
			$patch_version = $row->{'PatchHistoryVX'};
		}
		if($row->{'processServerEnabled'} eq "1")
		{	
			@version_data = split(/_/, $row->{'psVersion'});
			$component = "ProcessServer";
			$patch_version = $row->{'psPatchVersion'};
		}	
		
		if($version_data[1] ne $cx_version || ($cs_patch_version && ($version_data[1] ne $cs_patch_version)))
		{
			$structured_host_data .= "Component : ".$component.", HostId - ".$row->{'id'}.", Name - ".$row->{'name'}.", IP Address - ".$row->{'ipaddress'}.", Current CX Version - ".$cx_version.", Current ".$component." version - ".$version_data[1]." Patch Version - ".$patch_version." Latest CX Update Version - ".$cs_patch_version." \n";
		}	
	}
	
	if($structured_host_data)
	{
		my $mds_data = {'errorCodeName' => 'Agent/CS/PS Version Mismatch', 'errorLog' => $structured_host_data};
		$self->update_data_to_mds($mds_data);
	}
}

sub get_cs_patch_version 
{
	my $file_contents = &file_get_contents("$cs_installation_path\\home\\svsystems\\patch.log");
	my $cs_version = 0;
	
	my @file_data = split(/\n/, $file_contents);
	foreach my $line (@file_data) 
	{
		my @data = split(/,/,$line);
		my @cs_patch_version = split(/_/,$data[0]);

		if($cs_patch_version[2] > $cs_version)
		{
			$cs_version = $cs_patch_version[2];
		}
	}
	return $cs_version;	
}

sub get_dashboard_alerts
{
	my ($self, $conn) = @_;
	my ($structured_alert_data, $max_alert_id);
	my $alert_id = $conn->sql_get_value("transbandSettings","ValueData","ValueName = 'MDSAlertSyncCounter'");
	
	my $sql_alert_id = "SELECT 
							max(alertId) as 'alert_id'
						FROM 
							dashboardAlerts";
			
	my $res_alert_id = $conn->sql_exec($sql_alert_id);
	
	if (defined $res_alert_id)
	{
		my @value = @$res_alert_id;
		$max_alert_id = $value[0]{'alert_id'};
	}
	
    # No-op when no alerts are found and max alert is '0'
    return if(!$max_alert_id);
    
	my $query = "SELECT
						errSummary,
						errMessage,
						errCode,
						convert_tz(errStartTime, \@\@global\.time_zone, '+00:00') as errStartTime,
						convert_tz(errTime, \@\@global\.time_zone, '+00:00') as errTime,
						errCount,
						hostId
					FROM 
						dashboardAlerts 
					WHERE 
						alertId > '".$alert_id."' AND alertId <= '".$max_alert_id."'";
	my $result = $conn->sql_exec($query);

	foreach my $row (@{$result})
	{
		$structured_alert_data .= "HostId - ".$row->{'hostId'}.", ErrorCode - ".$row->{'errCode'}.", Alert generated time (UTC) - ".$row->{'errStartTime'}.", Alert latest reported time (UTC) - ".$row->{'errTime'}.", No of occurrences - ".$row->{'errCount'}.", Summary - ".$row->{'errSummary'}.", ErrorMessage - ".$row->{'errMessage'}."\n";
	}
	
	if($structured_alert_data)
	{
		my $mds_data = {'errorCodeName' => 'Dashboard Alerts', 'errorLog' => $structured_alert_data};
		$self->update_data_to_mds($mds_data);
		$self->update_alert_id($max_alert_id);
	}
}

sub update_alert_id
{
	my ($self, $alert_id) = @_;
	
	# Get CS DB connection
	my $conn = &tmanager::get_db_connection();
	
	if($alert_id)
	{
		my $update_alert_id = "UPDATE
							transbandSettings
						 SET
							ValueData = '".$alert_id."'
						WHERE 
							ValueName = 'MDSAlertSyncCounter'";
		$conn->sql_query($update_alert_id);
	}
}

sub push_error_files_data
{
	my ($self, $filePath, $fileName) = @_;
	
	my $log_file = $filePath.$fileName;
	
	my $err_file_content = &file_get_contents($log_file);
	
	if($err_file_content)
	{
		my $mds_data = {'errorCodeName' => $fileName,'errorLog' => $err_file_content};
		$self->update_data_to_mds($mds_data);
		truncate(open(ERR_FH, ">$log_file"), eof ERR_FH);
	}
}

sub generate_metadata_file
{
	my ($self) = @_;
	my $current_directory_name = Common::Log::get_current_telemetry_log_directory();
	my $json_path;
	
	my @dir_list = Utilities::read_directory_contents($cs_log_directory);
	foreach my $each_dir(@dir_list)
	{
		if((-d $cs_log_directory.$SLASH.$each_dir) && ($each_dir ne $current_directory_name))
		{
			if(!-e $cs_log_directory.$SLASH.$each_dir.$SLASH.$METADATA_JSON)
			{
			  $json_path = $cs_log_directory.$SLASH.$each_dir.$SLASH.$METADATA_JSON;
			  open(my $FH, '>', $json_path);
			  close $FH;	
			}
		}
	}
}

sub validate_telemetry_directory_existence
{
	my ($self) = @_;
	my %log_data;
	
	my $mkdir_output = mkdir "$cs_telemetry_directory" unless -d "$cs_telemetry_directory";
	if ($? != 0) 
	{
		$log_data{"directory_status"} = "validate_telemetry_directory_existence:: Could not create cs_telemetry_directory: $cs_telemetry_directory directory returng exception: $? , mkdir_output: $mkdir_output, MKDIR $cs_telemetry_directory unless -d $cs_telemetry_directory \n";
		$logging_obj->log("EXCEPTION",\%log_data,$TELEMETRY); 
		return 0;
	}
}

sub move_telemetry_log_file
{
	my ($self) = @_;
	my ($source_path,$destination_path,$return_val);
	my %log_data;
	my $current_directory_name = Common::Log::get_current_telemetry_log_directory();
	my $json_path;
	
	if ((-e $cs_log_directory and -d $cs_log_directory) && (-e $cs_telemetry_directory and -d $cs_telemetry_directory))
	{
		my @dir_list = Utilities::read_directory_contents($cs_log_directory);
		foreach my $each_dir(@dir_list)
		{
			$source_path = $cs_log_directory.$SLASH.$each_dir;
			$destination_path = $cs_telemetry_directory.$SLASH.$each_dir;
			if((-d $cs_log_directory.$SLASH.$each_dir) && ($each_dir ne $current_directory_name) && (-e $cs_log_directory.$SLASH.$each_dir.$SLASH.$METADATA_JSON))
			{
				$return_val = Utilities::move_file($source_path,$destination_path);
				if(!$return_val)
				{
					$log_data{"MoveTelemetryLogFile"} = " Move log file failed with Errorcode ".$return_val;
					$log_data{"SourcePath"} = $source_path;
					$log_data{"DestinationPath"} = $destination_path;
					$logging_obj->log("EXCEPTION",\%log_data,$TELEMETRY); 	
				}
			}
		}
	}
}

1;