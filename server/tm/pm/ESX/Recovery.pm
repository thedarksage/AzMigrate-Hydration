package ESX::Recovery;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use DBI();
use File::Copy;
use LWP::UserAgent;
use HTTP::Request::Common;
use Utilities;
use XML::RPC;
use XML::TreePP;
use Net::FTP;
use Common::Log;
use Common::DB::Connection;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use Cwd;
use tmanager;

my $logging_obj  = new Common::Log();

sub new
{
	my ($class) = @_;
	my $self = {};
		
	$self->{'conn'} = new Common::DB::Connection();	
	$self->{'common'} = new RX::CommonFunctions();	
	
	my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
	$self->{'cx_install_path'} = $AMETHYST_VARS->{'INSTALLATION_DIR'};
	
	bless($self, $class);	
}

sub process_job
{
	my ($self, $result) = @_;
	
	if(($result && $result->{'faultCode'}) || $result eq undef) 
	{		
		$logging_obj->log("EXCEPTION","XML RPC call - getRecoveryJobs failed.");
		return;
	}
	
	my $job_status;
	foreach my $plan_id (keys %$result)
	{
		my $recoveryUpdate;						
		my $plan_data = $self->{'common'}->format_plan_data(%{$result->{$plan_id}});
        $self->{'recovery_data'} = $plan_data;

		my %data = %{$plan_data};
		my $recovery_status = $data{'recovery_status'};
		
		my $recovery_update;
		
		if($recovery_status eq 'READINESS_CHECK')
		{
			$recovery_update = $self->{'common'}->perform_readiness_check($plan_data);
		}
		elsif($recovery_status eq 'RECOVERY_INITIATED')
		{
			$recovery_update = $self->create_recovery_job($plan_data);
		}
		elsif($recovery_status eq 'RECOVERY_IN_PROGRESS')
		{
			$recovery_update = $self->get_recovery_status($plan_data);
		}
		$recovery_update->{'recoveryId'} = $plan_data->{'recovery_id'};
		$recovery_update->{'recoveryStep'} = $plan_data->{'recovery_status'};
        
		$job_status->{$plan_id} = $recovery_update;
	}	
	return $job_status;
}

sub create_recovery_job
{
	my ($self, $plan_data) = @_;
	
	my %data = %{$plan_data};
	
	my $recovery_status = $data{'recovery_status'};
	my $recovery_id = $data{'recovery_id'};
	my $rec_plan_name = $data{"recovery_plan_name"};
	my $srchostname = $data{'fx_src'};		
	my $desthostname = $data{'fx_tgt'};
	my $recovery_plan_id = $data{'recovery_plan_id'};
	
	my $xml = '';
	my $srchost;		
	my $desthost;
	my $recovery_update;
	if($srchostname && $desthostname)
	{			
		my %src_details = $self->get_agent_details($srchostname);
		my %tgt_details = $self->get_agent_details($desthostname);	
			
		$srchost = $src_details{"id"};
		$desthost = $tgt_details{"id"};
		
		my $src_os_type = $src_details{"osFlag"};
		my $src_install_path = $src_details{"vxAgentPath"};
		my $tgt_install_path = $tgt_details{"fxAgentPath"};
		
		if(!$src_install_path)
		{			
			$recovery_update = $self->{'common'}->log_recovery_error("create_recovery_job","Source Install Path for \"$srchostname\" was not found in CS",$recovery_update,"recovery");
			return $recovery_update;
		}
		elsif(!$tgt_install_path)
		{
			$recovery_update = $self->{'common'}->log_recovery_error("create_recovery_job","Target Install Path for \"$desthostname\" was not found in CS",$recovery_update,"recovery");
			return $recovery_update;
		}
		
		if(!$srchost)
		{			
			$recovery_update = $self->{'common'}->log_recovery_error("create_recovery_job","Source Host information related to \"$srchostname\" was not found in CS",$recovery_update,"recovery");
			return $recovery_update;
		}
		elsif(!$desthost)
		{
			$recovery_update = $self->{'common'}->log_recovery_error("create_recovery_job","Target Host information related to \"$desthostname\" was not found in CS",$recovery_update,"recovery");
			return $recovery_update;
		}
		
		# Additional checks required to verify agent heartbeat
	
		my $cx_install_path = $self->{'cx_install_path'};
		my $random_number = int(rand(0..99999)) + time;	
		my $rec_plan_dir_name = $rec_plan_name."_".$random_number;
		my $cx_rec_plan_path = $cx_install_path."/vcon/".$rec_plan_dir_name;
		$self->{"cx_rec_plan_path"} = $cx_rec_plan_path;
		
		if(!-d $cx_rec_plan_path)
		{
			my $mkdir_res = mkdir($cx_rec_plan_path, 0777);
			if(!$mkdir_res)
			{
				$recovery_update = $self->{'common'}->log_recovery_error("create_recovery_job","Make directory failed : $cx_rec_plan_path",$recovery_update,"recovery");
				return $recovery_update;
			}
		}
		
		if($recovery_plan_id)
		{
			$self->clean_recovery_execution_steps($recovery_plan_id);
		}
		
		# Create ESX recovery related files		
		my $creation_of_xml =  $self->create_recovery_xml($cx_rec_plan_path,$rec_plan_dir_name);		
		my $creation_of_batch_script = $self->create_batch_script($rec_plan_name, $cx_rec_plan_path, $random_number,$tgt_install_path);
		my $creation_of_input_txt = $self->create_input_txt($cx_rec_plan_path);
		
		if(!$creation_of_xml || !$creation_of_batch_script || !$creation_of_input_txt)
		{
			$recovery_update = $self->{'common'}->log_recovery_error("create_recovery_job", "Creation of recovery configuration files at $cx_rec_plan_path failed", $recovery_update,"recovery");
			return $recovery_update;
		}		
		
		# Set File/Folder permission to 777
		`chmod -R 777 "$cx_rec_plan_path"`;	
		
		my $src_dir = $src_install_path;
		$src_dir .= ($src_os_type == 1) ? "\\Failover\\Data\\" : "failover_data/";
		$src_dir .= $rec_plan_name."_".$random_number;
		
		my $tgt_dir = $tgt_install_path."Failover\\Data\\".$rec_plan_name."_".$random_number;
		
		my $pre_script = "'".$src_install_path;
		$pre_script .= ($src_os_type == 1) ? "\\EsxUtil.exe" : "/bin/EsxUtil";
		$pre_script .= "' -rollback -d '".$src_dir."' -cxpath '".$cx_install_path."/vcon/".$rec_plan_name."_".$random_number."'";
		my $post_script = "'".$tgt_install_path."Failover\\Data\\".$rec_plan_name."_".$random_number."\\InMage_Script.bat'";
        
		$xml = '<FunctionRequest Id="'.$recovery_id.'" Name="ProtectESX" Include="N">
					<Parameter Name="HostIdentification" Value="'.$srchost.'" />
					<Parameter Name="PlanName" Value="'.$rec_plan_name.'" />
					<Parameter Name="JobType" Value="Recovery" />
					<Parameter Name="ConfigureFXJob" Value="Yes" />
					<Parameter Name="ScheduleType" Value="run_now" />
					<ParameterGroup Id="JobOptions">
						<Parameter Name="JobName" Value="Master Target - vContinuum" />
						<Parameter Name="SourceHostId" Value="'.$srchost.'" />
						<Parameter Name="TargetHostId" Value="'.$desthost.'" />
						<Parameter Name="SourceDir" Value="'.$src_dir.'" />
						<Parameter Name="TargetDir" Value="'.$tgt_dir.'" />
						<Parameter Name="ExcludeFiles" Value="" />
						<Parameter Name="IncludeFiles" Value="" />
						<Parameter Name="TransferMode" Value="Push" />
						<Parameter Name="SourcePreScript" Value="'.$pre_script.'" />								
						<Parameter Name="TargetPostScript" Value="'.$post_script.'" />
					</ParameterGroup>
				</FunctionRequest>';		
	}

	my $request_xml = ($xml) ? $self->get_cx_api_header() . $xml . $self->get_cx_api_footer() : "";

	if(!$request_xml)
	{
		$recovery_update = $self->{'common'}->log_recovery_error("create_recovery_job","Necessary Information for recovery not received",$recovery_update,"recovery");
	}
	else
	{
		$logging_obj->log("DEBUG", "create_recovery_job : Request XML sent as \n".$request_xml);
		my $parsed_data = $self->{'common'}->call_cx_api($request_xml, $recovery_status);
		$recovery_update = $self->parse_ProtectESX_response($parsed_data);
	}

	return $recovery_update;
}

sub get_recovery_status
{
	my ($self, $plan_data) = @_;
	
	my %data = %{$plan_data};
	
	my $recovery_status = $data{'recovery_status'};
	my $recovery_id = $data{'recovery_id'};
	my $rec_plan_id = $data{"recovery_plan_id"};
	my $host_data = $data{"host_data"};
	
	my $xml = '';
	if($rec_plan_id)
	{
		$xml = '<FunctionRequest Id="'.$recovery_id.'" Name="MonitorESXProtectionStatus" Include="N">					
					<Parameter Name="PlanId" Value="'.$rec_plan_id.'" />
					<Parameter Name="StepName" Value="Recovery" />
				</FunctionRequest>';
	}
	
	my $request_xml = ($xml) ? $self->get_cx_api_header() . $xml . $self->get_cx_api_footer() : "";

	my $recovery_update;
	if(!$request_xml)
	{
		$logging_obj->log("EXCEPTION", "get_recovery_status : Error creating XML");
		$recovery_update->{"recoveryError"} = 'Required information not received';
	}
	else
	{
		$logging_obj->log("DEBUG", "get_recovery_status : Request XML sent as \n".$request_xml);
		my $parsed_data = $self->{'common'}->call_cx_api($request_xml);
		$recovery_update = $self->parse_MonitorESXProtectionStatus_response($parsed_data);
	}

	return $recovery_update;
}

sub parse_ProtectESX_response
{
	my ($self, $parsed_data) = @_;

	my $return_data;
	my %response = %{$parsed_data->{'Response'}->{'Body'}};	
	my $res = $response{'Function'};
	my $id = $res->{'-Id'};
	
	if($res->{'-Message'} ne "Success")
	{
		$return_data = $self->{'common'}->log_recovery_error("create_recovery_job",$res->{'-Message'},$return_data,"recovery");
	}
	else
	{
		$return_data->{'recoveryPlanId'} = $res->{'FunctionResponse'}->{'Parameter'}->{'-Value'};
	}

	return $return_data;
}

sub parse_MonitorESXProtectionStatus_response
{
	my ($self, $parsed_data) = @_;

	my $return_data;
	my $response = $parsed_data->{'Response'}->{'Body'};
	my $res = $response->{'Function'};
   	my $id = $res->{'-Id'};
	
	if($res->{'-Message'} ne "Success")
	{
		$return_data = $self->{'common'}->log_recovery_error("get_recovery_status",$res->{'-Message'},$return_data,"recovery");
		#my $keerthy = 'a:4:{i:0;a:6:{s:8:"TaskName";s:26:"Initializing Recovery Plan";s:11:"Description";s:86:"This will initializes the Recovery Plan. It starts the EsxUtil.exe binary for Recovery";s:10:"TaskStatus";s:6:"Failed";s:12:"ErrorMessage";s:17:"Failed by Keerthy";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:1;a:6:{s:8:"TaskName";s:31:"Downloading Configuration Files";s:11:"Description";s:65:"The files which are going to download from CX are 1. Recovery.xml";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:2;a:6:{s:8:"TaskName";s:36:"Starting Recovery For Selected VM(s)";s:11:"Description";s:225:"The following operations going to perform in this task: 1. Remove pairs for all the selected VMs 2. Completes network related changes for all VMs 3. Deploys the source disk layout on respective target disk(in case of windows)";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:3;a:6:{s:8:"TaskName";s:31:"Powering on the recovered VM(s)";s:11:"Description";s:0:"";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}}';
		#$return_data->{'recoveryStepProgress'} = $keerthy;
	}
	else
	{
		if($res->{'FunctionResponse'}->{"ParameterGroup"}->{"ParameterGroup"}->{"ParameterGroup"})
		{
			my @tasks_list = @{$res->{'FunctionResponse'}->{"ParameterGroup"}->{"ParameterGroup"}->{"ParameterGroup"}};
			my $responses;
			foreach my $task (@tasks_list)
			{
				my $resp;		
				my @xs = @{$task->{"Parameter"}};
				foreach my $x (@xs)
				{
					$resp->{$x->{"-Name"}} = $x->{"-Value"};
				}		
				push(@{$responses},$resp);
			}		
			
			my $file_content = "";
			foreach my $resp (@{$responses})
			{
				if($resp->{"TaskStatus"} eq "Failed" or $resp->{"TaskStatus"} eq "Warning")
				{
					my $log_path = $resp->{"LogPath"};
					
					if(-f $log_path)
					{
						local $/ = undef;
						open(LOG_DETAILS,"$log_path");
						$file_content = <LOG_DETAILS>;
						close(LOG_DETAILS);
						local $/ = "\n";
					}
					else
					{
						$file_content = "Log file not available at $log_path";
					}
							
					last;
				}
			}
			$return_data->{'recoveryError'} = $file_content;	
			$return_data->{'recoveryStepProgress'} = serialize($responses);
		}
		elsif($res->{'-Message'} eq "Success")
		{
			$return_data = $self->{'common'}->log_recovery_error("get_recovery_status","Pending",$return_data,"recovery");
		}
		else
		{
			$return_data = $self->{'common'}->log_recovery_error("get_recovery_status","Failed to get the status of the recovery job",$return_data,"recovery");
		}
	}
	
	return $return_data;
}

sub get_agent_details
{
	my($self, $hostname) = @_;
	my %agent_details;
	my $sql = "SELECT id,vxAgentPath,fxAgentPath,osFlag FROM hosts WHERE name = '$hostname' OR ipAddress = '$hostname'";
	my $result = $self->{'conn'}->sql_exec($sql);

	foreach my $ref (@{$result})
	{
		$agent_details{"id"} = $ref->{"id"};
		$agent_details{"vxAgentPath"} = $ref->{"vxAgentPath"};
		$agent_details{"fxAgentPath"} = $ref->{"fxAgentPath"};
		$agent_details{"osFlag"} = $ref->{"osFlag"};
	}
	return %agent_details;
}

sub create_recovery_xml
{
	my ($self, $cx_rec_plan_path, $rec_plan_dir_name) = @_;
	
	my %data = %{$self->{"recovery_data"}};
	my @recovery_hosts = keys(%{$data{'host_data'}});	
	my $plan_name = $data{"plan_name"};	
	my $batch_count = $data{'batch_count'};
	my $cx_install_path = $self->{'cx_install_path'};
	my %host_data = %{$data{'host_data'}};
	
	my $params;
	foreach my $src_host_id (@recovery_hosts)
	{
		my ($tag,$tag_type,$host_tag);		
		my $tmp_tag;		
		my $recovery_option = $host_data{$src_host_id}{"recovery_option"};	
		if($recovery_option eq "LATEST_TAG")
		{
			$tag = "LATEST";
			$tag_type = "FS";
		}
		elsif($recovery_option eq "LATEST_TIME")
		{
			$tag = "LATESTTIME";
			$tag_type = "FS";
		}
		elsif($recovery_option eq "TAG_AT_SPECIFIC_TIME")
		{
			$tag = "";
			$tag_type = "FS";
		}
		elsif($recovery_option eq "SPECIFIC_TIME")
		{
			$tag = $host_data{$src_host_id}{"recovery_specific_time"};
            $tag =~ s/-/\//g; # Required Time format: yyyy/mm/dd [hr][:min][:sec][:millisec][:microsec][:hundrednanosec]
			$tag_type = "TIME";
		}
		
		if($tag eq "")
		{
			$host_tag = $host_data{$src_host_id}{'recovery_tag'};
		}
		
		$tmp_tag = ($tag) ? $tag : $host_tag;
		$params->{$plan_name}->{"batch_count"} = $batch_count;
		$params->{$plan_name}->{"xmlDirectoryName"} = $rec_plan_dir_name;
		my $power_on_vm = $host_data{$src_host_id}{"hardware_config_hosts"}{"poweronvm"};
		
		# Structure for Recovery.XML
		# Hardware Configuration
		$params->{$plan_name}->{"hosts_details"}->{$src_host_id}->{"config"}->{"cpucount"} = $host_data{$src_host_id}{"hardware_config_hosts"}{"cpucount"};
		$params->{$plan_name}->{"hosts_details"}->{$src_host_id}->{"config"}->{"memsize"} = $host_data{$src_host_id}{"hardware_config_hosts"}{"memsize"};
		
		# Network Configuration
		$params->{$plan_name}->{"hosts_details"}->{$src_host_id}->{"host_network_details"} = $host_data{$src_host_id}{"network_config_hosts"};
		
		# Other Configuration					
		$params->{$plan_name}->{"hosts_details"}->{$src_host_id}->{"host_tag_details"} = { "recoveryOrder" => $host_data{$src_host_id}{"recovery_order"},		
		"tag" => $tmp_tag,		
		"tagType" => $tag_type,
		"power_on_vm" => ( $power_on_vm ) ? "true" : "false" };		
	}
	
	$logging_obj->log("DEBUG","create_recovery_xml : sending params\n".Dumper($params));
	
	my $args_str = serialize($params);
    my $recovery_args_file = $cx_rec_plan_path."/Recovery_Args.txt";
	open(RECOVERY_ARGS, ">$recovery_args_file");	
	print RECOVERY_ARGS $args_str;
	close(RECOVERY_ARGS);
	
    my $cwd_current = getcwd;
    my $cx_admin_path = $cx_install_path."/admin/web/";
    chdir($cx_admin_path);
	my $call = "php ".$cx_install_path."/admin/web/create_esx_recovery_xml.php ".$rec_plan_dir_name;
	$logging_obj->log("DEBUG","create_recovery_xml : Command : $call");	
	#print $call."\n";	
	my $recovery_xml = `$call`;
    chdir($cwd_current);
	my $recovery_xml_file = $cx_rec_plan_path."/Recovery.xml";
	if($? == 0)
	{		
		open(RECOVERY_XML, ">$recovery_xml_file");	
		print RECOVERY_XML $recovery_xml;
		close(RECOVERY_XML);
	}
	else
	{
		return 0;
	}
	
	if(-f $recovery_xml_file)
	{
        unlink($recovery_args_file);
		return 1;
	}
	return 0;
}

sub create_batch_script
{
	my ($self, $rec_plan_name, $cx_rec_plan_path, $random_number,$dir) = @_;
	
	$dir =~ s/\\/\\\\/g;
	my %data = %{$self->{"recovery_data"}};
		
	my $esxIp = $data{'esx_ip'};
	my $esxUsername = $data{'esx_username'};
	my $esxPassword = $data{'esx_password'};	
	my $result = tmanager::insert_host_credentials($self->{'conn'}, $esxIp, $esxUsername, $esxPassword);
	
	if(!$result)
	{
		return 0;
	}
	
	my $script_data = 'echo off
SET PLANID=%2
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
set REG_OS=SOFTWARE
) else (
set REG_OS=SOFTWARE\Wow6432Node
)
ver | find "XP" > nul
if %ERRORLEVEL% == 0 (
FOR /F "tokens=2* " %%A IN (\'REG QUERY "HKLM\%REG_OS%\SV Systems\vContinuum" /v vSphereCLI_Path\') DO SET vmpath=%%B
) else (
FOR /F "tokens=2* delims= " %%A IN (\'REG QUERY "HKLM\%REG_OS%\SV Systems\vContinuum" /v vSphereCLI_Path\') DO SET vmpath=%%B
)
set PATH=%vmpath%;%path%;%windir%;%windir%\system32;%windir%\system32\wbem;
echo PATH = %PATH%
cd "'.$dir.'vContinuum\\Scripts"
"'.$dir.'\\vContinuum\\Scripts\\Recovery.pl" --server '.$esxIp.' --directory "'.$dir.'\\Failover\\Data\\'.$rec_plan_name.'_'.$random_number.'" --recovery yes --cxpath "'.$cx_rec_plan_path.'" --planid %PLANID%';

	my $file_path = $cx_rec_plan_path."/InMage_Script.Bat";
	open(BATCH_SCRIPT, ">$file_path");
	print BATCH_SCRIPT $script_data;
	close(BATCH_SCRIPT);
	if(-f $file_path)
	{
		return 1;
	}
	return 0;
}

sub create_input_txt
{
	my ($self, $cx_rec_plan_path) = @_;

	my $txt_data = "Recovery.xml" . "\n" . "InMage_Script.Bat";
	
	my $file_path = $cx_rec_plan_path."/input.txt";
	open(INPUT_TEXT, ">$file_path");
	print INPUT_TEXT $txt_data;
	close(INPUT_TEXT);
	if(-f $file_path)
	{
		return 1;
	}
	return 0;
}

sub get_cx_api_header
{
	my ($self) = @_;
	
	my $xml_header = '<Request Id="0001" Version="1.0">
				<Header>
					<Authentication> 
						<AccessKeyID>DDC9525FF275C104EFA1DFFD528BD0145F903CB1</AccessKeyID>
						<AccessSignature></AccessSignature>
					</Authentication> 
				</Header>
				<Body>'; 
				
	return $xml_header;
}

sub get_cx_api_footer
{
	my ($self) = @_;
	
	my $xml_footer = '</Body> 
				</Request>';
				
	return $xml_footer;
}

sub clean_recovery_execution_steps
{
	my ($self, $recovery_plan_id) = @_;
	my $sql;
	my $exe_step_id = $self->{'conn'}->sql_get_value('executionSteps', 'executionStepId', "planId = '$recovery_plan_id'");	
	
	$sql = "DELETE FROM executionStepTasks WHERE executionStepId = '$exe_step_id'";
	$self->{'conn'}->sql_query($sql);
	
	$sql = "DELETE FROM executionSteps WHERE planId = '$recovery_plan_id'";
	$self->{'conn'}->sql_query($sql);		
}

1;