package RX::PushInstall;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Common::Log;
use Data::Dumper;
use Utilities;
use File::Basename;
use File::Copy;
use PHP::Serialization qw /serialize unserialize/;

my $logging_obj = new Common::Log;
my $status = { "1" => "Pending", "2" => "In-Progress", "3" => "Completed", "-3" => "Failed" };

sub new
{
	my ($class) = @_;
	my $self = {};
		
	$self->{'conn'} = new Common::DB::Connection();	
	
	my $sql = "SELECT ValueName, ValueData FROM rxSettings";
	my $rx_data = $self->{'conn'}->sql_get_hash($sql,'ValueName','ValueData');	

	my $rx_ip = $rx_data->{"IP_ADDRESS"};
	my $rx_port = $rx_data->{"HTTP_PORT"};
	my $secured_communication = $rx_data->{"SECURED_COMMUNICATION"};	
	my $rx_cs_ip = $rx_data->{"RX_CS_IP"};	
	my $http = ($secured_communication) ? "https" : "http";
	$self->{'WINDOWS_AGENT_INSTALL_PATH'} = "C:\\Program Files\\InMage Systems";
	$self->{'LINUX_AGENT_INSTALL_PATH'} = "/usr/local/InMage/";
	$self->{'parallelPushCount'} = "5";
	$self->{'rxKey'} = $rx_data->{"RX_KEY"};
	$self->{'rpc_server'} = $http.'://'.$rx_ip.':'.$rx_port.'/xml_rpc/server.php';
	$self->{'rx_file_upload_url'} = $http.'://'.$rx_ip.':'.$rx_port.'/uploadDataFile.php';
	my $web_dir = '/home/svsystems/admin/web';
	$self->{'curl_php_script'} = $web_dir."/curl_dispatch_http.php";
	bless($self, $class);	
}

sub process_job
{	
	my ($self,$result) = @_;
	
	my $install_status;
	eval
	{			
		if(!$result)
		{
            $logging_obj->log("DEBUG","process_job : No jobs to process");
            return;
        }
        #$logging_obj->log("INFO","process_job : ".Dumper($result));
        my $result_ref = ref($result);
        if($result_ref eq 'HASH')
        {
            if($result->{'faultCode'})
            {
                $logging_obj->log("EXCEPTION","process_push_install_jobs : Failed : ".$result->{'faultString'});
                return;
            }
        }
		$install_status = $self->add_agent_installers($result);
		$logging_obj->log("INFO","process_push_install_jobs : updating the status as ".Dumper($install_status));
		#my $update_resp = $xmlrpc->call( 'report.updatePushInstallJob', {"cxKey" => $self->{'rxKey'} , "install_status" => serialize($install_status)});
	};
	if($@)
	{		
		$logging_obj->log("EXCEPTION","process_push_install_jobs : FAILED : ".$@);
	}
	return $install_status;
}

sub add_agent_installers
{
	my ($self, $details_hash) = @_;
	my $result;
	my $amethyst_values = Utilities::get_ametheyst_conf_parameters();
	my @synced_jobs;
	foreach my $agent_details (@$details_hash)
	{
		my $ipAddress = $agent_details->{"ipaddress"};
		$logging_obj->log("DEBUG","add_agent_installers : Processing the host $ipAddress");
		my $userName = $agent_details->{"userName"};
		my $pwd = $agent_details->{"password"};
		my $domain = $agent_details->{"domain"};
		my $hostName = $agent_details->{"hostName"};
		my $osType = $agent_details->{"osType"};
		my $cs_ip = $amethyst_values->{"CS_IP"};		
		my $port = $amethyst_values->{"CS_PORT"};
		my $osDetailType = $agent_details->{"osDetailType"};
		my $agentType = $agent_details->{"agentType"};
		my $agentFeature = $agent_details->{"agentFeature"};
		my $pushServerId = $agent_details->{"pushServerId"};
		my $agentInstallersId = $agent_details->{"agentInstallersId"};
		my $state = $agent_details->{"state"};
		my $communicationMode = uc($amethyst_values->{"CS_HTTP"});
		my $statusMessage;
		my $state_flag;
		push (@synced_jobs, $agentInstallersId);
		
		if($state == -4 || $state == 4)
		{
			$statusMessage = "UnInstallation Pending";
			$state_flag = "4";
		}
		elsif($state == 0 || $state == 1)
		{
			$statusMessage = "Installation Pending";
			$state_flag = "1";
		}
		# not to process completed (success/failed) jobs
		if($state == 3 || $state == -3 || $state == 6 || $state == -6) { next; }
			
		my $data = $self->get_job_details($agentInstallersId);
		if($data) 
		{
			my $agent_role = $self->{'conn'}->sql_get_value("hosts","agentRole","id = '".$data->{$agentInstallersId}->{'hostId'}."'");
			$data->{$agentInstallersId}->{'agentFeature'} = ($agent_role eq "Agent") ? "ScoutAgent" : $agent_role;
			$logging_obj->log("DEBUG","add_agent_installers : agent_role :: $agent_role, state :: $state\n");
			if(($state == 0) || ($state == -4))
			{										
				my $update_installers_sql = "UPDATE
									agentInstallers
								SET
									`userName` = '$userName',
									`password` = AES_ENCRYPT(AES_DECRYPT(UNHEX('$pwd'), '".$self->{'rxKey'}."'), '".$amethyst_values->{'HOST_GUID'}."'),
									`osType` = '$osType',
									`Domain` = '$domain',
									`hostName` = '$hostName',
									`osDetailType` = '$osDetailType',
									`cs_ip` = '$cs_ip',
									`port` = '$port',
									`agentType` = '$agentType',
									`agentFeature` = '$agentFeature',
									`pushServerId` = '$pushServerId',
									`communicationMode` = '$communicationMode'
								WHERE
									rxAgentInstallersId='$agentInstallersId'";
				$self->{'conn'}->sql_query($update_installers_sql);
				#$logging_obj->log("INFO","add_agent_installers : update_installers_sql :: $update_installers_sql");
				
				my $update_sql = "UPDATE
									agentInstallerDetails
								SET
									startTime = now(),
									endTime = '0000-00-00 00:00:00',
									lastUpdatedTime = now(),
									logFileLocation = '',
									statusMessage = '',									
									state = '$state_flag'
								WHERE				
									rxAgentInstallersId='$agentInstallersId'";
				$self->{'conn'}->sql_query($update_sql);
				$logging_obj->log("INFO","add_agent_installers : update_sql :: $update_sql");
				$result->{$agentInstallersId}->{'statusMessage'} = $statusMessage;
				$result->{$agentInstallersId}->{'state'} = $state_flag;
				$result->{$agentInstallersId}->{'agentFeature'} = $agentFeature;	
			}
			else
			{
				my $log_file_location = $data->{$agentInstallersId}->{'logFileLocation'};
				if(-f $log_file_location)
				{
					$data->{$agentInstallersId}->{'log'} = $agentInstallersId."_".basename($log_file_location);
					$self->upload_pushinstall_log_file($log_file_location,$data->{$agentInstallersId}->{'log'});
				}
				
				$result->{$agentInstallersId} = $data->{$agentInstallersId};
			}
		}
		else
		{
			my $sql_update =	"INSERT INTO `agentInstallers`
									SET
										ipaddress = '$ipAddress',
										`userName` = '$userName',
										`password` = AES_ENCRYPT(AES_DECRYPT(UNHEX('$pwd'),'".$self->{'rxKey'}."'), '".$amethyst_values->{'HOST_GUID'}."'),
										`osType` = '$osType',
										`Domain` = '$domain',
										`hostName` = '$hostName',
										`osDetailType` = '$osDetailType',
										`cs_ip` = '$cs_ip',
										`port` = '$port',
										`agentType` = '$agentType',
										`agentFeature` = '$agentFeature',
										`parllelPushCount` = '".$self->{'parallelPushCount'}."',
										`pushServerId` = '$pushServerId',
										`rxAgentInstallersId` = '$agentInstallersId',
										`communicationMode` = '$communicationMode'";
			#$logging_obj->log("INFO","add_agent_installers : SQL - $sql_update");
			my $update_sth = $self->{'conn'}->sql_query($sql_update);
			
			my $agent_installer_path = "/home/svsystems/admin/web/sw/";
			if (Utilities::isWindows())
			{
				$agent_installer_path = "C:\\home\\svsystems\\admin\\web\\sw\\";
			}
			$agent_installer_path = $self->{'conn'}->sql_escape_string($agent_installer_path);
			
			my $installationPath = ($osType == 1) ? $self->{'WINDOWS_AGENT_INSTALL_PATH'} : $self->{'LINUX_AGENT_INSTALL_PATH'};	
			$installationPath = $self->{'conn'}->sql_escape_string($installationPath);
			
			my $insert_details_sql = "INSERT
					  INTO
					  `agentInstallerDetails`
					   (
						`ipaddress` ,
						`agentInstallerLocation`,
						`startTime`,
						`lastUpdatedTime`,
						`agentinstallationPath`,
						`rebootRequired`,
						`upgradeRequest`,
						`rxAgentInstallersId`,
						`state`
					   )
					  VALUES
					   (
						'$ipAddress',
						'$agent_installer_path',
						now(),
						now(),
						'$installationPath',
						'0',
						'0',
						'$agentInstallersId',
						'$state_flag'
					   )";
			my $details_sth = $self->{'conn'}->sql_query($insert_details_sql);
			$logging_obj->log("DEBUG","add_agent_installers : INSERT SQL - $insert_details_sql");
			$result->{$agentInstallersId}->{'statusMessage'} = $statusMessage;
			$result->{$agentInstallersId}->{'state'} = $state_flag;
			$result->{$agentInstallersId}->{'agentFeature'} = $agentFeature;
		}
	}
	
	my $synced_jobs_str = join("','",@synced_jobs);
	my $del_sql = "DELETE
					FROM
						agentInstallers
					WHERE
						rxAgentInstallersId != '0' 
					AND					
						rxAgentInstallersId NOT IN ('$synced_jobs_str')";
	$self->{'conn'}->sql_query($del_sql);	
	
	my $del_sql = "DELETE
					FROM
						agentInstallerDetails
					WHERE
						rxAgentInstallersId != '0' 
					AND					
						rxAgentInstallersId NOT IN ('$synced_jobs_str')";
	$self->{'conn'}->sql_query($del_sql);	
	
    return $result;
}
   
sub get_job_details() 
{
	my ($self,$agentInstallersId) = @_;
	
	# 1  - pending
	# 2  - inprogress
	# 3  - completed
	# -3 - failed	
	my $sql = "SELECT				
				state,
				logFileLocation,
				statusMessage,
				ipaddress,
				hostId,
				rxAgentInstallersId
			FROM
				agentInstallerDetails
			WHERE 
				rxAgentInstallersId = '$agentInstallersId'";
	my $data = $self->{'conn'}->sql_exec($sql,"rxAgentInstallersId");	
	return $data;
}

sub upload_pushinstall_log_file
{
	my ($self,$original_location,$log_file_name) = @_;
	
	my $rx_url = $self->{'rx_file_upload_url'};
	my $curl_php_script = $self->{'curl_php_script'};
	
	my $copy_file_location = "/home/svsystems/var/".$log_file_name;
	copy($original_location,$copy_file_location);
	
	my $post_data = "data_file##@".$copy_file_location; 
		
	my $status = `php "$curl_php_script" "$rx_url" "$post_data"`;
	
	if(!$status)
	{
		unlink($copy_file_location);
	}
}
1;