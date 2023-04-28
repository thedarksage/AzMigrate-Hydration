package EventManager;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use tmanager;
use Messenger;
use Utilities;
use Common::Log;
use Common::DB::Connection;
use Array::ArrayProtector;
use Fabric::VsnapProcessor;
use Prism::PrismProtector;
use ConfigServer::Trending::HealthReport;
use Data::Dumper;
use File::Basename;
use ESX::Discovery;
use PHP::Serialization qw(serialize unserialize);
use File::Find::Rule;

my $BASE_DIR = $tmanager'BASE_DIR;
my $msg = Messenger->new(); 
my $logging_obj = new Common::Log();
my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
my $cs_installation_path = Utilities::get_cs_installation_path();

my $TELEMETRY = Common::Constants::LOG_TO_MDS;
my $MDS_AND_TELEMETRY = Common::Constants::LOG_TO_MDS_AND_TELEMETRY;

#
# This function will monitor the state of apiRequestDetail for SRM and if all the device/scsiids are 
# in success/failure state then will update the same state to table apiRequest.
#
sub monitor_api_request
{	
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
    $logging_obj->log("DEBUG","Entered monitor_api_request \n");
	my ($origAutoCommit,$origRaiseError);
	
	eval
	{
		
	    $origAutoCommit = $conn->get_auto_commit();
        $origRaiseError = $conn->get_raise_error();
		
		$conn->set_auto_commit(0,1);
		
		if(!$conn->sql_ping_test())
        {
			&tmanager::monitor_request_data($conn);
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","monitor_api_request: ERROR -".$@);
		 $conn->sql_rollback();
		 $conn->set_auto_commit($origAutoCommit,$origRaiseError);
		 return;
	}
	$conn->set_auto_commit($origAutoCommit,$origRaiseError);
}

sub logged_error_messages
{	
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	if((&tmanager::isPrimaryActive($conn) != 0) || (&tmanager::isStandAloneCX($conn) == 1))
	{
		$logging_obj->log("DEBUG","Entered logged_error_messages function\n");
				
		Messenger::sendTrapInfo;
		select(STDERR); $| = 1;		# make unbuffered
		select(STDOUT); $| = 1;		# make unbuffered
	}
	# Report any important outstanding errors
}

sub send_error_messages
{	
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	if((&tmanager::isPrimaryActive($conn) != 0) || (&tmanager::isStandAloneCX($conn) == 1))
	{
		$logging_obj->log("DEBUG","Entered send_error_messages function\n");	
		if ($msg->is_valid_email_delivery()) 
		{
			$msg->send_all_messages();
		}
	}
}


##
# Has each filerep job update its progress total, check its RPO, and test exit
# codes. Requires a database handle and a messenger.

sub monitor_filerep
{	
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	if((&tmanager::isPrimaryActive($conn) != 0) || (&tmanager::isStandAloneCX($conn) == 1))
	{
		$logging_obj->log("DEBUG","Entered monitor_filerep function\n");		
		
		# Get list of configs with their RPO thresholds
		my (@fr_configs, $err_message);
		my $run_once_job = 0;
		
		my $CSHostName = &TimeShotManager::getCSHostName($conn);
		my $sql_rpo = "SELECT 
						c.id,
						c.rpoThreshold, 
						s.repeat, 
						c.groupId, 
						hsrc.name as src_name, 
						c.sourcePath, 
						hdest.name, 
						c.destPath,
						hsrc.id	as hostId
					FROM 
						frbJobConfig c, 
						frbSchedule s,
						hosts hsrc, 
						hosts hdest
					WHERE 
						hsrc.id = c.sourceHost AND 
						hdest.id = c.destHost AND 
						c.scheduleId = s.id AND
						c.rpoThreshold > 0 AND
						c.deleted = 0";
		
		my $result = $conn->sql_exec($sql_rpo);

		foreach my $row(@{$result})
		{
			push( @fr_configs,
				{id => $row->{'id'}, rpo_threshold => $row->{'rpoThreshold'}, repeat => $row->{'repeat'}, gid => $row->{'groupId'}, sourceName => $row->{'src_name'}, sourcePath => $row->{'sourcePath'}, destName => $row->{'name'}, destPath => $row->{'destPath'}, hostId => $row->{'hostId'}});
		}
		
		# Get the list of jobs that exceeded their RPO threshold
		my @rpo_exceeded;
		foreach my $config (@fr_configs) 
		{
			my $sql_time = "SELECT 
						UNIX_TIMESTAMP(max(startTime)) as 'start_time',
						UNIX_TIMESTAMP(now()) as 'current_time' 
					FROM 
						frbStatus 
					WHERE 
						jobConfigId = '$config->{id}' AND 
						exitCode = 0";
			
			my $res_time = $conn->sql_exec($sql_time);
			$logging_obj->log("DEBUG","monitor_filerep : Entered rpo_threshold :: ".$config->{rpo_threshold});
			
			if (defined $res_time)
			{
				my @value = @$res_time;

				my $start_time = $value[0]{'start_time'};
				my $current_time = $value[0]{'current_time'};
				my $rpo = $current_time - $start_time;
				$logging_obj->log("DEBUG","monitor_filerep : Entered :: ".$current_time."::".$start_time."::".$rpo);
				
				if($start_time)
				{
					push(@rpo_exceeded, $config) if $rpo > $config->{rpo_threshold};
				}
			}
			
			if( ! $config->{repeat} ) 
			{ 
				# 
				# Raise a flag to set $run_once_job to 1. This is checked below to 
				# dispatch the rpo mail alerts to the user. If the job is a run once job
				# RPO is not applicable. -- BKNATHAN
				#
				$run_once_job = 1;
			}
		}

		# Get the list of running jobs that appear to be stuck
		my @stuck_jobs;
	   
		my $sql_struck = "SELECT 
					j.id,
					c.progressThreshold,
					c.groupId,
					hsrc.name as srcname,
					c.sourcePath,
					hdest.name,
					c.destPath,
					hsrc.id as hostId
				FROM 
					frbJobs j, 
					frbJobConfig c, 
					frbStatus s, 
					hosts hsrc, 
					hosts hdest
				WHERE 
					hsrc.id = c.sourceHost AND
					hdest.id = c.destHost AND 
					j.jobConfigId = c.id AND 
					j.assignedState = 1 AND 
					j.id = s.id AND
					s.jobConfigId = c.id AND
					( ( s.lastProgressTime = 0 and UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(s.startTime) > c.ProgressThreshold ) OR
					( s.lastProgressTime <> 0 and UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(s.lastProgressTime) > c.progressThreshold ) ) AND c.progressThreshold > 0";
		my $res_struck = $conn->sql_exec($sql_struck);

		foreach my $res(@{$res_struck})
		{
			push( @stuck_jobs, { id => $res->{'id'}, limit => $res->{'progressThreshold'},gid => $res->{'groupId'}, sourceName => $res->{'srcname'}, sourcePath => $res->{'sourcePath'},destName => $res->{'name'}, destPath => $res->{'destPath'}, hostId => $res->{'hostId'}} );
		}
		
		# For each config exceeding its RPO, send an alert
		foreach my $config (@rpo_exceeded) 
		{
			my %arguments;
			$arguments{"cs_host_name"} = $CSHostName;
			$arguments{"src_host_name"} = $config->{ sourceName };
			$arguments{"id"} = $config->{ id };
			$arguments{"dest_host_name"} = $config->{ destName };
			$arguments{"src_vol"} = $config->{ sourcePath };
			$arguments{"dest_vol"} = $config->{ destPath };
			my $params = \%arguments;
			my $source = "filerep: $config->{ id }";
			my $msgStr="\n";
			$msgStr .= "[Group Id]: $config->{ gid } \n";
			$msgStr .= "[Job Id]: $config->{ id } \n";
			$msgStr .= "[Source - Path]=>[Target - Path] \n";
			$msgStr .= "[$config->{ sourceName }- $config->{ sourcePath }]=>[$config->{ destName }- $config->{ destPath }] \n";
			my $subject = "TRAP on fr $config->{id}";
			my $err_id = "FILEREP_".$config->{ id }."_".$config->{ gid }."_".$config->{ hostId };
			$err_message = "File Replication RPO has exceeded the configured SLA Threshold for \n $msgStr";
			my $err_summary = "File Replication RPO exceeded SLA Threshold";
			if ($run_once_job != 1)
			{
				&Messenger::add_error_message ($conn, $err_id, "RPOSLA_EXCD", $err_summary, $err_message, '');
			}
			else
			{
				$logging_obj->log("DEBUG","monitor_filerep : Entered trap");
				&Messenger::send_traps($conn, "RPOSLA_EXCD_FX", $err_message, $params);
			}
		} # end outer foreach

		# For each job exceeding its progress threshold, send an alert
		foreach my $job (@stuck_jobs) 
		{
			my %args;
			$args{"cs_host_name"} = $CSHostName;
			$args{"src_host_name"} = $job->{ sourceName };
			$args{"id"} = $job->{ id };
			$args{"dest_host_name"} = $job->{ destName };
			$args{"src_vol"} = $job->{ sourcePath };
			$args{"dest_vol"} = $job->{ destPath };
			my $send_args = \%args;
			my $source = "filerep: $job->{ id }";
			my $msgStr="\n";
			$msgStr .= "[Group Id]: $job->{ gid } \n";
			$msgStr .= "[Job Instance]: $job->{ id } \n";
			$msgStr .= "[Source - Path]=>[Target - Path] \n";
			$msgStr .= "[$job->{ sourceName }- $job->{ sourcePath }]=>[$job->{ destName }- $job->{ destPath }] \n";
			my $subject = "TRAP on fr instance $job->{id}";

			my $err_id = "FILEREP_".$job->{ id }."_".$job->{ gid }."_".$job->{ hostId };
			my $err_message = "File Replication job mentioned below has made no progress within $job->{limit} seconds: \n $msgStr \n";
			my $err_summary = "File Replication Job has not progressed for some time";
			&Messenger::add_error_message ($conn, $err_id, "FXJOB_ERROR", $err_summary, $err_message, '');
			&Messenger::send_traps($conn, "FXJOB_ERROR", $err_message, $send_args);
		}
	}
}

sub gen_traffic_trending
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	if((&tmanager::isPrimaryActive($conn) != 0) || (&tmanager::isStandAloneCX($conn) == 1))
	{            
		my %report_args = ('conn'=>$conn,'basedir'=>$BASE_DIR);
		my $report = new ConfigServer::Trending::HealthReport(%report_args);
		$report->gen_traffic_trending($conn);
	}
}

sub update_health_report_data
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	if((&tmanager::isPrimaryActive($conn) != 0) || (&tmanager::isStandAloneCX($conn) == 1))
	{	
		my %report_args = ('conn'=>$conn,'basedir'=>$BASE_DIR);
		my $report = new ConfigServer::Trending::HealthReport(%report_args);
		$report->updateHealthReportData($conn);
	}
}

sub accumulate_perf_data
{
	my ($package, $extra_params) = @_;
	my $data_hash = $extra_params->{'data_hash'};
	$logging_obj->log("DEBUG","Entered in gentrends::accumulate_perf_data");	
	ConfigServer::Trending::HealthReport::accumulate_perf_data($data_hash);	
	$logging_obj->log("DEBUG","Exiting gentrends::accumulate_perf_data");
}

sub update_network_rrds
{
	#
	# Create a new object of the class and initialize
	# it with some values
	#
	my $ntw_obj = new ProcessServer::Trending::NetworkTrends();

	#
	# Get the device statistics
	#
	$ntw_obj->updateNetworkRrds();	 
}

sub monitor_bandwidth
{
	my ($package, $extra_params) = @_;
	my $data_hash = $extra_params->{'data_hash'};
	my %args;
	$args{'base_dir'} = $BASE_DIR;
	$args{'web_root'} = $AMETHYST_VARS->{'WEB_ROOT'};
	$args{'installation_dir'} = $AMETHYST_VARS->{'INSTALLATION_DIR'};
	$args{'cxps_xfer'} = $AMETHYST_VARS->{'CXPS_XFER'};
	$args{'xfer_log_file'} = $AMETHYST_VARS->{'XFER_LOG_FILE'};
	
	if (Utilities::isWindows())
	{
		my $rrdtool_path = $AMETHYST_VARS->{'RRDTOOL_PATH'};
		$rrdtool_path =~ s/\\/\\\\/g;
		$args{'rrd_command'} = $rrdtool_path.'\\rrdtool\\Release\\rrdtool.exe';
		$args{'log_dir'}    = $cs_installation_path."\\home\\svsystems\\var\\log";
		$args{'diff'}       = "diff.exe -a";
	}
	else
	{
		$args{'rrd_command'} = 'rrdtool';
		$args{'log_dir'}    = "/var/log";
		$args{'diff'}       = "/usr/bin/diff -a";
	}
	
	if($AMETHYST_VARS->{'CX_TYPE'} != 1)
	{
		ProcessServer::Trending::BandwidthTrends::update_bandwidth_log($data_hash,\%args);
	}
	return 1;	
}

sub update_bandwidth_rrd
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
	my %args = (
					'base_dir' => $BASE_DIR,
					'web_root' => $AMETHYST_VARS->{'WEB_ROOT'},
					'installation_dir' => $AMETHYST_VARS->{'INSTALLATION_DIR'},
					'rrd_command' => "",
					'log_dir' => "",
					'conn'	=> $conn
				);

	if (Utilities::isWindows())
	{
		my $rrdtool_path = $AMETHYST_VARS->{'RRDTOOL_PATH'};
		$rrdtool_path =~ s/\\/\\\\/g;
		$args{'rrd_command'} = $rrdtool_path.'\\rrdtool\\Release\\rrdtool.exe';
		$args{'log_dir'}    = $cs_installation_path."\\home\\svsystems\\var\\log";
		$args{'diff'}       = "diff.exe -a";
	}
	else
	{
		$args{'rrd_command'} = 'rrdtool';
		$args{'log_dir'}    = "/var/log";
		$args{'diff'}       = "/usr/bin/diff -a";
	}

	$args{'cxps_xfer'} = $AMETHYST_VARS->{'CXPS_XFER'};
	$args{'xfer_log_file'} = $AMETHYST_VARS->{'XFER_LOG_FILE'};

	my $monitorbw_obj = new ProcessServer::Trending::BandwidthTrends(%args);

	my $is_primary_active = &tmanager::isPrimaryActive($conn);
	my $is_not_ha = &tmanager::isStandAloneCX($conn);
	if(($AMETHYST_VARS->{'CX_TYPE'} != 2) && (($is_primary_active != 0) || ($is_not_ha == 1)))
	{	
		$monitorbw_obj->update_bandwidth_rrd();
	}
	
	return 1;	
}

sub array_protector
{
	&Array::ArrayProtector::pair_process_requests();
}

sub vsnap_processor
{
	&Fabric::VsnapProcessor::processVsnapComponent();
}

sub prism_protector
{
	&Prism::PrismProtector::prism_requests();
}

# 
# Function Name: monitor_services
#
# Description: 
#	 Used to check the status of the services and restart the services if they are stopped. 
#
# Parameters: None
#
# Return Value: None
#
sub monitor_services
{
	my ($package, $extra_params) = @_;
	my $data_hash = $extra_params->{'data_hash'};
	$logging_obj->log("DEBUG","Entered monitor_services");
	eval
	{
		select(STDERR); $| = 1;		# make unbuffered
		select(STDOUT); $| = 1;		# make unbuffered	
		
		my $services = Common::Constants::MON_SERVICES;
		
		my $custom_build_type = $AMETHYST_VARS->{'CUSTOM_BUILD_TYPE'};
		my $cx_type = $AMETHYST_VARS->{'CX_TYPE'};
		my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
		
		if(Utilities::isLinux())
		{
			# Verify Only on Config Server
			if ($cx_type != 2)
			{				
				if(exists($data_hash->{'appliance_node_data'}->{$host_guid}->{'nodeRole'}) && $data_hash->{'appliance_node_data'}->{$host_guid}->{'nodeRole'} eq "PASSIVE")
				{	
					&TimeShotManager::monitorScheduler('STOP_SCHEDULER');
				}
				else
				{				
					&TimeShotManager::monitorScheduler('START_SCHEDULER');
				}
				
				# Cleanup old incomplete uploads from the hostagents
				&tmanager::cleanupPartialHostUploads($BASE_DIR);				
			}
		
			foreach my $service (@$services)
			{
				my $check_service_flag = 0;

				if($service eq "cxpsctl")
				{
					my $check_status;
					$check_status = `/etc/init.d/cxpsctl status`;
					chomp $check_status;
					if($check_status =~ /stopped/)
					{
						$check_service_flag = 1;
					}	
				}	
				elsif($service eq "tgtd")
				{	
					if($custom_build_type eq "2") # Pillar Case
					{
						my $check_status = `/etc/init.d/tgtd status`;
						chomp $check_status;
						if($check_status =~ /stopped/)
						{
							$check_service_flag = 1;
						}
					}	
				}
				elsif($service eq "vxagent")
				{
					if($custom_build_type eq "2") # Pillar Case
					{
						my $check_status = `/etc/init.d/vxagent status`;
						chomp $check_status;
						if($check_status =~ /appservice\s+daemon\s+is\s+not\s+running/g || $check_status =~ /VolumeAgent\s+daemon\s+is\s+not\s+running/g)
						{
							$check_service_flag = 1;
						}
					}
				}
				elsif($service eq "multipath")
				{
					my $multipath_service_check = system ("service multipathd status");
					if ($? != 0) 
					{
						$logging_obj->log("EXCEPTION","multipath service not running");
						my $unlink_multipath_id = unlink ("/var/run/multipathd.pid");
		
						if ($unlink_multipath_id != 0) 
						{
							$logging_obj->log("EXCEPTION","Error removing multipath pid file");							
						}
						else
						{
							my $multipath_start = system ("service multipathd start");
							if ($multipath_start != 0) 
							{
								$logging_obj->log("EXCEPTION","Error starting multipath service: $@");
							}
							else
							{
								$logging_obj->log("DEBUG","multipath service started successfully");
							}
						}	
					}
				}
				
				if($check_service_flag == 1)
				{
					$logging_obj->log("EXCEPTION", "Found $service service stopped. Starting $service service");
					my $start_cmd = "/etc/init.d/".$service." restart";
					my $run_cmd = `$start_cmd`;
					my $return_cmd = `echo $?`;
					$logging_obj->log("DEBUG" ,"Restarted $service service: returncode - $return_cmd");
				}
			}	
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error in checking the service status : ERROR : $@");
	}
}

# 
# Function Name: cleanup_job
#
# Description: 
#	 Used to remove stale data from cx; be it at the DB level or at the directory level 
#
# Parameters: None
#
# Return Value: None
#
sub cleanup_job
{
	$logging_obj->log("DEBUG","Entered cleanup_job");
	eval
	{
        my $cx_type = $AMETHYST_VARS->{'CX_TYPE'};
        if ($cx_type != 2)
        {
            # Clean old records from the dashboardAlerts table that are older than 24 hours
            &TimeShotManager::cleanupDashboardAlerts();
        }
    };	
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error in cleanup_job: $@");
	}
}

# 
# Function Name: process_esx_discovery
#
# Description: 
#	 Used to discover vms from ESX on PS
#
# Parameters: None
#
# Return Value: None
#
sub process_esx_discovery
{
	my ($package, $extra_params) = @_;
	my %log_data;
	my %telemetry_metadata = ('RecordType' => 'ProcessESXDiscovery');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
	$logging_obj->log("DEBUG","Entered process_esx_discovery");
	eval
	{		
		$logging_obj->log("INFO","Discovering the VMs");
		my $esx_discovery_obj = new ESX::Discovery();	
		$esx_discovery_obj->discover();
    };
	if($@)
	{
		$log_data{"RecordType"} = "ProcessESXDiscovery";
		$log_data{"ErrorMessage"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
	}
}

# 
# Function Name: update_discovery_data
#
# Description: 
#	 Used to update ESX discovery information on CS
#
# Parameters: None
#
# Return Value: None
#
sub update_discovery_data
{
	my ($package, $extra_params) = @_;
	my %log_data;
	
	my %telemetry_metadata = ('RecordType' => 'UpdateDiscoveryData');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	$logging_obj->log("DEBUG","Entered update_discovery_data");
	eval
	{
		$logging_obj->log("INFO","Updating discovery information to DB");
		
		my $conn = &tmanager::get_db_connection();		
		my %args = ('conn' => $conn);
		my $esx_discovery_obj = new ESX::Discovery(%args);				
		$esx_discovery_obj->update_discovery_info();
    };
	if($@)
	{
		$log_data{"RecordType"} = "UpdateDiscoveryData";
		$log_data{"ErrorMessage"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
	}
}

sub registerToRx
{
    my $rx_conf_file = "/home/svsystems/var/cx_register_conf_file.txt";
    
	# Get RX details for registration
	open(CONF_FH,$rx_conf_file);
	my $rx_config = <CONF_FH>;
	close(CONF_FH);
    
    my ($rx_ip,$rx_port,$rx_secured_communication,$customer_account_id) = split(/:/,$rx_config);
	
    # Register sequence:
	# CS_IP_ADDREES, CS_PORT, CS_HOST_GUID, CS_HOSTANME, SYNC_INTERVAL, SYNC_METHOD, ALIAS, CS_COMMUNICATION_TYPE, RX_KEY, CUSTOMER_ACCOUNT_ID, CUSTOMER_REFERENCE_ID

    # Get RX hostname
	my $cs_host_name = `hostname`;
    chomp($cs_host_name);
    
	my @xmlrpcval_array = ($AMETHYST_VARS->{'PS_CS_IP'},$AMETHYST_VARS->{'PS_CS_PORT'},$AMETHYST_VARS->{'HOST_GUID'},$cs_host_name,'300','PUSH','','0','',$customer_account_id,'');

	my $rx_protocol = ($rx_secured_communication == 0) ? "http" : "https";
	my $rpc_server = $rx_protocol.'://'.$rx_ip.':'.$rx_port.'/xml_rpc/server.php';	
    $logging_obj->log("DEBUG","registerCx : rpc_server : $rpc_server\nxmlrpcval_array\n".Dumper(\@xmlrpcval_array));
	my $xmlrpc = XML::RPC->new($rpc_server);
	my $result = $xmlrpc->call('report.registerCx',\@xmlrpcval_array);
	
	if(ref($result) eq "HASH")
	{
        if($result->{'faultCode'})
        {
            $logging_obj->log("EXCEPTION","registerCx : CS Registration Failed. RX : $rx_ip, Port : $rx_port, Protocol : $rx_protocol, Account ID : $customer_account_id");
            return 0;
        }
        my $rx_host_name = $result->{'HOST_NAME'};
        my $rx_key = $result->{'RX_KEY'};
        my $rx_version_tag = $result->{'VERSION_TAG'};
        
        my $conn_obj = new Common::DB::Connection();
        
		my $rx_update_values = {'IP_ADDRESS' => $rx_ip, 'SECURED_COMMUNICATION' => $rx_secured_communication, 'HTTP_PORT' => $rx_port, 'RX_CS_IP' => $AMETHYST_VARS->{'PS_CS_IP'}, 'RX_CS_SECURED_COMMUNICATION' => '0', 'RX_CS_PORT' => $AMETHYST_VARS->{'PS_CS_PORT'}, 'RESPONSE_INTERVAL' => '300', 'RESPONSE_TYPE' => 'PUSH', 'HOST_NAME' => $rx_host_name, 'RX_KEY' => $rx_key, 'VERSION_TAG' => $rx_version_tag, 'CUSTOMER_ACCOUNT_ID' => $customer_account_id};
       foreach my $key (keys %$rx_update_values)
        {
            my $sql = "UPDATE rxSettings set ValueData='".$rx_update_values->{$key}."' WHERE ValueName='".$key."'";
            $conn_obj->sql_query($sql);
        }

		$logging_obj->log("INFO","registerCx : CS Registration completed. RX : $rx_ip, Port : $rx_port, Protocol : $rx_protocol, Account ID : $customer_account_id");
        
        return 1;
    }
    return 0;
}

sub monitor_timeout
{
	my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
    
	my %log_data;
	my %telemetry_metadata = ('RecordType' => 'MonitorTimeOut');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
    my $PUSH_INSTALL_JOB_TIMEOUT = ($AMETHYST_VARS->{"PUSH_INSTALL_JOB_TIMEOUT"}) ? $AMETHYST_VARS->{"PUSH_INSTALL_JOB_TIMEOUT"} : "5400";
	my $PUSH_HEART_BEAT_DOWN = ($AMETHYST_VARS->{"DEFAULT_VX_AGENT_TIMEOUT"}) ? $AMETHYST_VARS->{"DEFAULT_VX_AGENT_TIMEOUT"} : "900";
	
	my $push_server_heartbeats;
	my $push_jobs;
	my @infrastructure_list;
	my @push_server_list;
	my $infrastructure_names;
	my $push_server_names;
	my $timeout_details;
	
	
	#Get push server heartbeats
	my $sql = "SELECT 
					id,
					unix_timestamp(now()) - unix_timestamp(lastHostUpdateTime) as heartbeat  
				FROM 
					pushProxyHosts";
	my $push_heartbeat_sth = $conn->sql_query($sql);
	if ($conn->sql_num_rows($push_heartbeat_sth) > 0) 
	{
		while (my $row = $conn->sql_fetchrow_hash_ref($push_heartbeat_sth))
		{
			my $push_server_id = $row->{'id'};
			my $push_server_heartbeat = $row->{'heartbeat'};
			$push_server_heartbeats->{$push_server_id} = $push_server_heartbeat;
		}
	}
	
	if (defined $push_server_heartbeats)
	{
	
		# Check for PushInstall jobs to be timed out
		my $sql = "SELECT 
										aid.infrastructureVmId, 
										ai.pushServerId, 
										aid.jobId,
										aid.upgradeRequest 		
									FROM 
										agentInstallers ai, 
										agentInstallerDetails aid 
									WHERE 
										aid.state IN ('1','2') 
									AND 
										((UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(aid.startTime)) > ".$PUSH_INSTALL_JOB_TIMEOUT.") 
									AND 
										ai.infrastructureVmId=aid.infrastructureVmId 
									AND 
										ai.ipAddress = aid.ipAddress 
									AND 
										aid.infrastructureVmId!=''";
		
		my $sth = $conn->sql_query($sql);
		
		if ($conn->sql_num_rows($sth) > 0) 
		{
			while (my $row = $conn->sql_fetchrow_hash_ref($sth))
			{
				my $job_id = $row->{'jobId'};	
				$push_jobs->{$job_id}->{infrastructureVmId} = $row->{'infrastructureVmId'};
				$push_jobs->{$job_id}->{pushServerId} = $row->{'pushServerId'};	
				$push_jobs->{$job_id}->{upgradeRequest} = $row->{'upgradeRequest'};	
				push @infrastructure_list, $row->{'infrastructureVmId'};
				push @push_server_list, $row->{'pushServerId'};				
			}
			
			if(scalar(@infrastructure_list) > 0)
			{
				
				@infrastructure_list = Utilities::arrayuniq(@infrastructure_list);
				@push_server_list = Utilities::arrayuniq(@push_server_list);
				
				my $infrastructure_data = join("','", @infrastructure_list);
				my $push_server_string = join("','", @push_server_list);
				my $sql = "SELECT 
								infrastructureVMId,
								hostName 
							FROM 
								infrastructurevms 
							WHERE 
								infrastructureVMId IN ('".$infrastructure_data."')";
								 
				my $sth = $conn->sql_query($sql);
				if ($conn->sql_num_rows($sth) > 0) 
				{
					while(my $row = $conn->sql_fetchrow_hash_ref($sth))
					{
						$infrastructure_names->{$row->{"infrastructureVMId"}} = $row->{'hostName'};
					}
				}
				
				my $sql = "SELECT 
												id,
												name 
											FROM 
												hosts
											WHERE 
												processServerEnabled = 1
											AND	
												id IN ('".$push_server_string."')";
				my $sth = $conn->sql_query($sql);
				if($conn->sql_num_rows($sth) > 0) 
				{
					while (my $row = $conn->sql_fetchrow_hash_ref($sth))
					{
						$push_server_names->{$row->{'id'}} = $row->{'name'};
					}
				}
				
				
				foreach my $jobid (keys %$push_jobs)
				{
					my $error_code;
					my $push_server_id = $push_jobs->{$jobid}->{'pushServerId'};
					my $infrastructure_vm_id = $push_jobs->{$jobid}->{'infrastructureVmId'};
					my $upgrade_request = $push_jobs->{$jobid}->{'upgradeRequest'};
					if ($push_server_heartbeats->{$push_server_id} > $PUSH_HEART_BEAT_DOWN)
					{
						if ($upgrade_request == 1)
						{
							$error_code =  Common::Constants::EC_PUSH_UPGRADE_TIMED_OUT_WITHOUT_HEARTBEAT;
						}
						else
						{
							$error_code = Common::Constants::EC_PUSH_INSTALL_TIMED_OUT_WITHOUT_HEARTBEAT;
						}
					}
					else
					{
						if ($upgrade_request == 1)
						{
							$error_code = Common::Constants::EC_PUSH_UPGRADE_TIMED_OUT_WITH_HEARTBEAT;
						}
						else
						{
							$error_code = Common::Constants::EC_PUSH_INSTALL_TIMED_OUT_WITH_HEARTBEAT;
						}
					}
						
					$timeout_details->{"ErrorCode"} = $error_code;
					
					if (exists $infrastructure_names->{$infrastructure_vm_id}) 
					{
						$timeout_details->{"SourceIp"} = $infrastructure_names->{$infrastructure_vm_id};
					}
					else
					{
						$timeout_details->{"SourceIp"} = "";
					}
					
					if (exists $push_server_names->{$push_server_id})
					{
						$timeout_details->{"PsName"} = $push_server_names->{$push_server_id};
					}
					else
					{
						$timeout_details->{"PsName"} = "";
					}
					
					my $place_holder_data = serialize($timeout_details);
					
					my $sql = "UPDATE 
									agentInstallerDEtails 
								SET 
									errCode = '$error_code', 
									errPlaceHolders = '$place_holder_data',
									state = '-3', 
									statusMessage = 'Timed out at CS' 	
								WHERE 
									jobId = $jobid
								AND	
									infrastructureVmId = '$infrastructure_vm_id'
								AND state IN ('1','2')";
					$conn->sql_query($sql);
				
					$log_data{"jobId"} = $jobid;
					$log_data{"infrastructureVmId"} = $infrastructure_vm_id;
					$log_data{"placeHolders"} = $place_holder_data;
					$log_data{"errCode"} = $error_code;
					$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
				}
			}
		}
	}
	
	# StopProtection Timeout
	my $STOP_PROTECTION_JOB_TIMEOUT = ($AMETHYST_VARS->{"STOP_PROTECTION_JOB_TIMEOUT"}) ? $AMETHYST_VARS->{"STOP_PROTECTION_JOB_TIMEOUT"} : "1800";
	
	my $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING = 24;
	my $RELEASE_DRIVE_ON_TARGET_DELETE_FAIL = -25;
	my $MT_DELETE_DONE = 30;
	my $PS_DELETE_DONE = 31;
	
	my $sql = "SELECT b.requestData as requestData FROM apiRequest a, apiRequestDetail b WHERE a.requestType IN ('StopProtection', 'DeletePairs') AND ((UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(a.requestTime)) > ".$STOP_PROTECTION_JOB_TIMEOUT.") AND a.apiRequestId=b.apiRequestId";
	my $sth = $conn->sql_query($sql);
	if($conn->sql_num_rows($sth) > 0) {
		while (my $row = $conn->sql_fetchrow_hash_ref($sth))
		{
			my $data = unserialize($row->{'requestData'});
			my $scenarioIds;
			my $planId = $data->{'planId'};
			
			if((exists $data->{'sourceHostIdsScenarioIds'}) && (defined $data->{'sourceHostIdsScenarioIds'}) && ((ref $data->{'sourceHostIdsScenarioIds'}) ne 'Array')) {
				my %scenarioDetails = %{$data->{'sourceHostIdsScenarioIds'}};
				foreach my $k (keys %scenarioDetails) {
					my $sourceId = $k;
					my $scenarioId = $data->{'sourceHostIdsScenarioIds'}->{$k};
					
					my $sql1 = "SELECT count(*) as count FROM srcLogicalVolumeDestLogicalVolume WHERE planId = '$planId' AND sourceHostId = '$sourceId' AND scenarioId = '$scenarioId' AND replication_status IN ('$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING','$RELEASE_DRIVE_ON_TARGET_DELETE_FAIL','$MT_DELETE_DONE')";
					my $rs1 = $conn->sql_exec($sql1);
					if(($rs1->[0]->{'count'})>0) {
						my $sql2 = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status='$PS_DELETE_DONE' WHERE planId = '$planId' AND sourceHostId = '$sourceId' AND scenarioId = '$scenarioId' AND replication_status IN ('$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING','$RELEASE_DRIVE_ON_TARGET_DELETE_FAIL','$MT_DELETE_DONE')";
						$conn->sql_query($sql2);
						
						$log_data{"Sql"} = $sql2;
						$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
					}
				}
			}else {
				my $sql1 = "SELECT count(*) as count FROM srcLogicalVolumeDestLogicalVolume WHERE planId = '$planId' AND replication_status IN ('$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING','$RELEASE_DRIVE_ON_TARGET_DELETE_FAIL','$MT_DELETE_DONE')";
				my $rs1 = $conn->sql_exec($sql1);
				if(($rs1->[0]->{'count'})>0) {
					my $sql2 = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status='$PS_DELETE_DONE' WHERE planId = '$planId' AND replication_status IN ('$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING','$RELEASE_DRIVE_ON_TARGET_DELETE_FAIL','$MT_DELETE_DONE')";
					$conn->sql_query($sql2);
					
					$log_data{"Sql"} = $sql2;
					$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
				}
				
			}
		}
	}
    
    # Check for Discovery jobs for time out
	#################################################################
	# 1. Job is vSphere / vCenter / vCloud
	# 2. login verify time is more than JOB_TIMEOUT seconds from now
	# 3. creationTime time is more than JOB_TIMEOUT seconds from now
	# 4. Did not already fail because of a different failure
	################################################################
	my $DISCOVERY_JOB_TIMEOUT = ($AMETHYST_VARS->{"DISCOVERY_JOB_TIMEOUT"}) ? $AMETHYST_VARS->{"DISCOVERY_JOB_TIMEOUT"} : "7200";

    my $get_discovery_timeout_jobs = "SELECT inventoryId, esxAgentId FROM infrastructureInventory WHERE hostType IN ('vSphere', 'vCenter', 'vCloud') AND ((UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(loginVerifyTime)) > ".$DISCOVERY_JOB_TIMEOUT.") AND ((UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(creationTime)) > ".$DISCOVERY_JOB_TIMEOUT.") AND lastDiscoveryErrorMessage IS NULL";
    my $discovery_timeout_jobs = $conn->sql_get_hash($get_discovery_timeout_jobs, "inventoryId", "esxAgentId");
    if(defined $discovery_timeout_jobs)
    {
		$log_data{"DiscoverySql"} = $discovery_timeout_jobs;
		$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		
        my %args = ('conn' => $conn);
		my $error_hash = {"error" => "Timed Out at CS", "error_log" => "Timed Out at CS"};
		my $esx_discovery_obj = new ESX::Discovery(%args);
		foreach my $inventory_id (keys %$discovery_timeout_jobs)
		{
			$esx_discovery_obj->update_discovery_failure($inventory_id, $error_hash);
		}
    }
	
	 # Check for Renew cert jobs for time out
    my $get_renewcert_timeout_jobs = "SELECT
											p.hostId, 
											p.policyId,
											name,
											UNIX_TIMESTAMP(policyTimestamp)as policyStartTime,
											UNIX_TIMESTAMP(now()) as currentTime,
											policyType,
											UNIX_TIMESTAMP(lastHostUpdateTimePs)as psLastHeartbeat
										FROM 
											policy  p,
											policyInstance pi,
											hosts h
										WHERE
											h.id = p.hostId
										AND 
											pi.hostId = h.id
										AND
											p.policyId = pi.policyId
										AND
											p.policyType IN ('50', '51')
										AND 
											pi.policyState IN ('3', '4')";
	my $renew_cert_sth = $conn->sql_query($get_renewcert_timeout_jobs);
	if($conn->sql_num_rows($renew_cert_sth) > 0) 
	{
		my ($error_details, $execution_log, $policy_diff_time, $ps_last_heart_beat);
		while (my $renew_cert_data = $conn->sql_fetchrow_hash_ref($renew_cert_sth))
		{
			my $policy_time_out = 0;
			$policy_diff_time = $renew_cert_data->{"currentTime"} - $renew_cert_data->{"policyStartTime"};
			$ps_last_heart_beat = $renew_cert_data->{"currentTime"} - $renew_cert_data->{"psLastHeartbeat"};
			
			# In case if ps heartbeat is more than 15 mins and 
			# policy start time is more than 5 mins timing out ps renew cert.
			if($renew_cert_data->{"policyType"} == "51" && ($policy_diff_time >= 600 || $ps_last_heart_beat >= 900))
			{
				$policy_time_out = 1;
				
				$log_data{"PolicyDiffTime"} = $policy_diff_time;
				$log_data{"PsLastHeartBeat"} = $ps_last_heart_beat;
				$log_data{"PolicyType"} = $renew_cert_data->{"policyType"};
				$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
			}
			elsif($policy_diff_time >= 1800)
			{
				$policy_time_out = 1;
				$log_data{"PsLastHeartBeat"} = $ps_last_heart_beat;
				$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
			}
			
			if($policy_time_out == 1)
			{
				$log_data{"PolicyDiffTime"} = $discovery_timeout_jobs;
				$log_data{"RenewCertData"} = Dumper($renew_cert_data);
				$log_data{"PSLastHeartbeat"} = $ps_last_heart_beat;
				$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
				
				$error_details->{"ErrorCode"} = "EC0161";
				$error_details->{"PlaceHolders"} = $renew_cert_data->{"name"};
				$error_details->{'ErrorDescription'} = "Renew cert timed out at CS on ".$renew_cert_data->{"name"};
				
				$execution_log = serialize($error_details);
				
				my $policy_instance_sql = "UPDATE 
												policyInstance 
											SET 
												policyState = '2', 
												executionLog = '$execution_log' 
											WHERE 
												policyId = ".$renew_cert_data->{"policyId"}." 
											AND 
												policyState IN (3,4)";
				$conn->sql_query($policy_instance_sql);
				
				$log_data{"RenewCertSql"} = $policy_instance_sql;
				$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);	
			}
		}
	}
	
    # Migrate ACS to AAD Timeout
    my $MIGRATE_ACS_TO_AAD_TIMEOUT = '1800';
    my $pending_agent_jobs_sql = "SELECT * FROM agentJobs WHERE jobStatus IN ('Pending', 'InProgress') AND (UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(jobUpdateTime) > $MIGRATE_ACS_TO_AAD_TIMEOUT OR UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(jobCreationTime) > $MIGRATE_ACS_TO_AAD_TIMEOUT)";
    my $pending_agent_job_data = $conn->sql_exec($pending_agent_jobs_sql);
    if($pending_agent_job_data && scalar(@$pending_agent_job_data) > 0)
    {
        $log_data{"PendingAgentJobsSql"} = $pending_agent_jobs_sql;
		$log_data{"PendingAgentJobData"} = Dumper($pending_agent_job_data);
		$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
        
		my @error_details;
        $error_details[0][1] = "Timed out at CS";
        $error_details[0][2] = "Job has timed out at CS after $MIGRATE_ACS_TO_AAD_TIMEOUT minutes";
        my $error_log = serialize(\@error_details);
        my $update_pending_jobs_sql = "UPDATE agentJobs SET jobStatus = 'Failed', errorDetails = '".$error_log."' WHERE jobStatus IN ('Pending', 'InProgress') AND (UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(jobUpdateTime) > $MIGRATE_ACS_TO_AAD_TIMEOUT OR UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(jobCreationTime) > $MIGRATE_ACS_TO_AAD_TIMEOUT)";
        
		$log_data{"UpdatePendingJobsSql"} = $update_pending_jobs_sql;
		$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		$conn->sql_exec($update_pending_jobs_sql);
	}
    
	# Create a registry file to be consumed by MT on execution of MigrateAcsToAad API.
	my $CLOUD_CONTAINER_REGISTRY_HIVE_FILE_PATH = $AMETHYST_VARS->{'INSTALLATION_DIR'}."/var/cloud_container_registry.reg";
	my $get_agent_jobs_sql = "SELECT count(jobId) as jobCount FROM agentJobs WHERE jobType = 'MTPrepareForAadMigrationJob' AND jobStatus = 'Pending'";
    my $agent_jobs = $conn->sql_exec($get_agent_jobs_sql);
    my $registry_data_str;
	if($agent_jobs->[0]->{'jobCount'} > 0)
	{
		my $cert_cmd = 'perl "'.$AMETHYST_VARS->{'INSTALLATION_DIR'}.'/bin/get_cloud_container_reg.pl"';
		$registry_data_str = `$cert_cmd`;
		$log_data{"CertCmd"} = $cert_cmd;
		$log_data{"RegistryDataStr"} = $registry_data_str;
		$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
	}
		
	if(-e $CLOUD_CONTAINER_REGISTRY_HIVE_FILE_PATH && -s $CLOUD_CONTAINER_REGISTRY_HIVE_FILE_PATH > 0 && $agent_jobs->[0]->{'jobCount'} > 0)
	{
		my $update_agent_jobs_sql = "UPDATE agentJobs SET jobUpdateTime = now(), inputPayload = '".$registry_data_str."', jobStatus = 'InProgress' WHERE jobType = 'MTPrepareForAadMigrationJob' AND jobStatus = 'Pending'";
		
		$log_data{"UpdateAgentJobsSql"} = $update_agent_jobs_sql;
		$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		$conn->sql_query($update_agent_jobs_sql);
	}
}

sub cleanup_data
{
    my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
    
    my $settings_sql = "SELECT ValueName, ValueData FROM transbandSettings WHERE valueType = 'CLEANUP_DATA'";
    my $settings = $conn->sql_get_hash($settings_sql, "ValueName", "ValueData");
	
	my $max_token_sql = "SELECT ValueName, ValueData FROM transbandSettings WHERE ValueName = 'ALERTS_MAX_TOKEN_ID'";
    my $max_token_obj = $conn->sql_get_hash($max_token_sql, "ValueName", "ValueData");
	my $max_token_value = ($max_token_obj->{'ALERTS_MAX_TOKEN_ID'})?$max_token_obj->{'ALERTS_MAX_TOKEN_ID'}:0;
    
    # Set the time to check for clean-up
    my $cleanup_time = ($settings->{'CLEANUP_TIME_LIMIT'}) ? ($settings->{'CLEANUP_TIME_LIMIT'}) : 90 * 24 * 60 * 60; # 90 days by default
    # Set the size to check for clean-up
    my $cleanup_size = ($settings->{'CLEANUP_RECORD_LIMIT'}) ? ($settings->{'CLEANUP_RECORD_LIMIT'}) : 25000; # twenty five thousand records by default
    
    # Tables to clean
    my $tables_to_clean = {
                            "MDSLogging"         => {
                                                    "time" => "logCreationDate",
                                                    "record" => "logId"
                                                   },    
                           "apiRequest"         => {
                                                    "time" => "requestTime",
                                                    "record" => "apiRequestId"
                                                   },    
                           "apiRequestDetail"   => {
                                                    "time" => "lastUpdateTime",
                                                    "record" => "apiRequestDetailId"
                                                   },
							"dashboardAlerts"	=> {
													"time" => "errStartTime",
													"record" => "alertId"
												   }
                          };
    foreach my $table (keys %$tables_to_clean)
    {
		my $table_cleanup_time;
		
		# For dashboard alerts the max time of records to be held is 1 day.
		if($table eq "dashboardAlerts")
		{
			#1 day pruning
			$table_cleanup_time = "86400";
		}
		elsif ($table eq "MDSLogging")
		{
			#30 days pruning
			$table_cleanup_time = "2592000";
		}
		else
		{
			#90 days pruning for apirequests
			$table_cleanup_time = $cleanup_time;
		}
		
        my $column_for_time_check = $tables_to_clean->{$table}->{'time'};
        my $column_to_record_check = $tables_to_clean->{$table}->{'record'};
        my $table_test_sql = "SELECT count(*) as totalRecords, (SELECT count(*) FROM ".$table." WHERE UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(".$column_for_time_check.") >= ".$table_cleanup_time.") as timeupRecords, MAX(".$column_to_record_check.") as maxRecord FROM ".$table;
		
        my $table_data = $conn->sql_exec($table_test_sql);
		$table_data = $table_data->[0];
        
        # Record limit reached
        if($table_data->{'totalRecords'} > $cleanup_size)
        {
            my $limit_records = $table_data->{'totalRecords'} - $cleanup_size;
            my $record_cleanup_sql = "DELETE FROM ".$table." ORDER BY ".$column_to_record_check." LIMIT ".$limit_records;
			$logging_obj->log("INFO","cleanup_data : Delete records as record limit reached : ".$record_cleanup_sql);
            $conn->sql_query($record_cleanup_sql);
        }
        # If reaches the column length limit
        elsif($table_data->{'maxRecord'} >= 99999999900)
        {
            # Truncate table to reset the auto increment value
            my $record_cleanup_sql = "TRUNCATE ".$table;
			$logging_obj->log("INFO","cleanup_data : Truncate table as auto increment value almost reached : ".$record_cleanup_sql);
            $conn->sql_query($record_cleanup_sql);
        }        
        # Time limit reached
        elsif($table_data->{'timeupRecords'})
        {
            my $record_cleanup_sql = "DELETE FROM ".$table." WHERE UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(".$column_for_time_check.") >= ".$table_cleanup_time;
			 $record_cleanup_sql = $record_cleanup_sql." AND alertId<= $max_token_value"  if($table eq "dashboardAlerts");
			$logging_obj->log("INFO","cleanup_data : Delete records as there are records older than 7 days : ".$record_cleanup_sql);
            $conn->sql_query($record_cleanup_sql);
        }
    }
	
	#Folder to monitor to clean temporary files.
	#Time to clean all temporary files older than a day
	&cleanup_temporary_files('C:\thirdparty\php5nts\uploadtemp', 1);
}

#Clean any folder files for older than specified number of days.
sub cleanup_temporary_files
{
	my ($directory_path, $no_of_days) = @_;

	#Depth 1 indicates non recursive directory scanning. 
	#Depth 0 indicates recursive scanning.
	my @files = File::Find::Rule->file()
								->maxdepth(1)
								->in($directory_path);

	for my $file (@files)
	{
		if (-M $file > $no_of_days)
		{
			#print "deleting $file\n";
			unlink $file;
		}
	}
}

sub cleanup_alerts
{
    my ($package, $extra_params) = @_;
	my $conn = $extra_params->{'conn'};
    
    # Get alerts that are existing for less than 24 hours.
    # Less than 24 hours because the cleanup_data event cleans the data if exists for more than 24 hours.
    my $alerts_data_sql = "SELECT errCode, alertId, UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(errStartTime) as alertTime FROM dashboardAlerts WHERE UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(errStartTime) < 86400";
    my $alerts_data = $conn->sql_exec($alerts_data_sql, "errCode");
    $logging_obj->log("DEBUG","cleanup_alerts : Current alerts. SQL : ".$alerts_data_sql."\nData : ".Dumper($alerts_data));
	
    my @dashboard_alert_codes = keys (%$alerts_data);
    my $alert_codes_str = join("','", @dashboard_alert_codes);
    
    # Return if there are no alerts.
    if(scalar(@dashboard_alert_codes) <= 0) 
    {
        return;
    }
    
    # Get the suppression times for the alert codes from eventCodes table.
	my $get_suppress_time_sql = "SELECT errCode, suppressTime FROM eventCodes WHERE category = 'Alerts/Events' AND errCode IN ('".$alert_codes_str."')";
    my $supress_time_data = $conn->sql_get_hash($get_suppress_time_sql, "errCode", "suppressTime");
    $logging_obj->log("DEBUG","cleanup_alerts : Suppress Times. SQL : ".$get_suppress_time_sql."\nData : ".Dumper($supress_time_data));
    
    my $max_alert_id = $conn->sql_get_value('transbandSettings', 'ValueData', "ValueName = 'ALERTS_MAX_TOKEN_ID'");
    
    my @alerts_to_be_deleted;
    
    # Get the logic running for the alerts and collect those are to be deleted.
    foreach my $err_code (keys %$alerts_data)
    {
        my $alert_id = $alerts_data->{$err_code}->{'alertId'};
        my $alert_time = $alerts_data->{$err_code}->{'alertTime'};
        
        # Delete the alert IF :
        # 1. Suppression time is more than 0.
        # 2. Alert existence time is greater than the Alert Suppression Time.
        # 3. If the alert id is less than the max alert id. This ensure that the alert has reached SRS atleast once.
        if($supress_time_data->{$err_code} > 0 && $alert_time > $supress_time_data->{$err_code} && $alert_id < $max_alert_id)
        {
            push(@alerts_to_be_deleted, $alert_id);
        }
    }
    
    # Only if the count of alerts to be deleted is more than 0.
    if(scalar(@alerts_to_be_deleted) > 0)
    {
        # Delete the alerts by their alert ids.
        my $alerts_to_be_deleted_str = join("','", @alerts_to_be_deleted);
        my $delete_alerts_sql = "DELETE FROM dashboardAlerts WHERE alertId IN ('".$alerts_to_be_deleted_str."')";
        $conn->sql_query($delete_alerts_sql);
        $logging_obj->log("INFO","cleanup_alerts : Delete alerts according to their pruning logic. Alerts hash : ".Dumper($alerts_data)."\nSQL : ".$delete_alerts_sql);
    }
}

# 
# Function Name: clean_unprotected_hosts
#
# Description: 
#	 Used to cleanup unprotected and no heartbeat hosts.
#	 Used to reset the hostId for VM in discovery.
#
# Parameters: None
#
# Return Value: None
#
sub clean_unprotected_hosts
{
	my ($package, $extra_params) = @_;
	my %log_data;
	my %telemetry_metadata = ('RecordType' => 'CleanUnprotectedHosts');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
	$logging_obj->log("INFO","Entered clean_unprotected_hosts");
	eval
	{		
        my $conn = $extra_params->{'conn'};
		
		# Fetching records for those hosts who does not have heartbeat for more than 7 days 
		# and no protection exists.
		my $stale_vm_sql = "SELECT 
									id 
								FROM 
									hosts 
								WHERE 
									id NOT IN (SELECT sourceHostId FROM srcLogicalVolumeDestLogicalVolume)
								AND	
									id NOT IN (SELECT sourceId FROM applicationScenario) 
								AND 
									((UNIX_TIMESTAMP(now()) - lastHostUpdateTime) > 604800) 
								AND
									agentRole = 'Agent'";		
		my $stale_vm_data = $conn->sql_exec($stale_vm_sql);
		
		if (defined $stale_vm_data)
		{
			foreach my $ref (@{$stale_vm_data})
			{
				my $hostId = $ref->{"id"};
				
				my $update_host_sql = "UPDATE infrastructureVMs SET hostId = '' WHERE hostId = '$hostId'";
				$conn->sql_query($update_host_sql);
				
				my $delete_host_sql = "DELETE FROM hosts WHERE id = '$hostId'";
				$conn->sql_query($delete_host_sql);
				
				$logging_obj->log("INFO","clean_unprotected_hosts VM data hostId :$hostId \n update_host_sql::$update_host_sql \n delete_host_sql::$delete_host_sql");
			}
		}
    };
	if($@)
	{
		$log_data{"RecordType"} = "CleanUnprotectedHosts";
		$log_data{"ErrorMessage"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
	}
}

1;