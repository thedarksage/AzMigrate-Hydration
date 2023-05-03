package RX::CommonFunctions;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use DBI;
use File::Copy;
use LWP::UserAgent;
use Utilities;
use XML::RPC;
use XML::TreePP;
use Net::FTP;
use Common::Log;
use Data::Dumper;
use HTTP::Request::Common;
use Common::DB::Connection;
use Time::Local;
use PHP::Serialization qw(serialize unserialize);

my $logging_obj = new Common::Log();
$logging_obj->set_event_name("ESX");

sub new
{
	my ($class) = @_;
	my $self = {};
		
	$self->{'conn'} = new Common::DB::Connection();	
		
	my $sql = "SELECT ValueName, ValueData FROM rxSettings";
	my $rx_data = $self->{'conn'}->sql_get_hash($sql,"ValueName","ValueData");
	
	my $rx_ip = $rx_data->{"IP_ADDRESS"};
	my $rx_port = $rx_data->{"HTTP_PORT"};
	my $rx_cs_ip = $rx_data->{"RX_CS_IP"};
	my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
	my $secured_communication = $rx_data->{"SECURED_COMMUNICATION"};	
	my $http = ($secured_communication) ? "https" : "http";
	my $cs_secured_communication = $AMETHYST_VARS->{'CS_SECURED_COMMUNICATION'};	
	my $cs_http = ($cs_secured_communication) ? "https" : "http";
	
	$self->{'rpc_server'} = $http.'://'.$rx_ip.':'.$rx_port.'/xml_rpc/server.php';

	$self->{'cx_install_path'} = $AMETHYST_VARS->{'INSTALLATION_DIR'};
	$self->{'cs_ip'} = $AMETHYST_VARS->{'CS_IP'};
	$self->{'rx_cs_ip'} = $rx_cs_ip;
	$self->{'cs_port'} = $AMETHYST_VARS->{'CS_PORT'};
	
	$self->{'cs_api_url'} = $cs_http."://".$self->{'cs_ip'}.":".$self->{'cs_port'}."/ScoutAPI/CXRXAPI.php";
	
	bless($self, $class);	
}

sub make_xml_rpc_call
{
	my $self = shift;
	my $rpc_method = shift;
	my $extra_params = shift;
	
	$logging_obj->log("DEBUG","make_xml_rpc_call : rpc_method = ".$rpc_method);
	
	my $response;
	eval
	{
		my $xmlrpc = XML::RPC->new($self->{'rpc_server'});
		$response = $xmlrpc->call( $rpc_method, $extra_params);
		
		#$logging_obj->log("DEBUG","make_xml_rpc_call : ".Dumper($response));
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","make_xml_rpc_call : $rpc_method Failed : ".$@);
	}
		
	return $response;
}

sub verify_plan_uniqueness
{
	my($self, $plan_name, $plan_id) = @_;
	
	my $cond = '';
	if($plan_id)
	{
		$cond = " AND planId != '$plan_id'";
	}
	
	my $sql = "SELECT planId FROM applicationPlan WHERE planName = '$plan_name' $cond";
	my $planExist = $self->{'conn'}->sql_get_num_rows($sql);
	
	return $planExist;
}

sub perform_readiness_check
{
	my ($self, $readiness_check_data, $job_type) = @_;
	
	my $recovery_update;	
	my %data = %{$readiness_check_data};
	
	my $recovery_id = $data{"recovery_id"};
	my $recovery_plan_name = $data{"recovery_plan_name"};
	my $recovery_plan_id = $data{"recovery_plan_id"};		
	my $recovery_status = $data{'recovery_status'};	
	my $fx_src = $data{'fx_src'};		
	my $fx_tgt = $data{'fx_tgt'};
	my $host_data = $data{"host_data"};
	
	my $recovery_update;
	
	my $plan_exists = $self->verify_plan_uniqueness($recovery_plan_name, $recovery_plan_id);
	if($plan_exists)
	{			
		$recovery_update = $self->log_recovery_error("perform_readiness_check","Recovery plan name \"$recovery_plan_name\" already exists in CS",$recovery_update);
		return $recovery_update;
	}	
	
	my $src_details = $self->get_agent_details($fx_src);
	my $tgt_details = $self->get_agent_details($fx_tgt);	
		
	my $srchost = $src_details->{"id"};
	my $desthost = $tgt_details->{"id"};
	if(!$srchost)
	{			
		$recovery_update = $self->log_recovery_error("perform_readiness_check","Source Host (Master Target) information related to \"$fx_src\" was not found in CS",$recovery_update);
		return $recovery_update;
	}
	elsif(!$desthost)
	{
		$recovery_update = $self->log_recovery_error("perform_readiness_check","Target Host (vContinuum) information related to \"$fx_tgt\" was not found in CS",$recovery_update);
		return $recovery_update;
	}
		
	my $xml = '';
	
	my @hosts = keys(%{$data{"host_data"}});			
	foreach my $src_host_id (@hosts)
	{
		my $tgt_host_id = $data{"host_data"}{$src_host_id}{"target_host_id"};
		my $recovery_option = $data{"host_data"}{$src_host_id}{"recovery_option"};
		my $recovery_specific_time = $data{"host_data"}{$src_host_id}{"recovery_specific_time"};
	
		if($recovery_specific_time)
		{
			my ($year,$mon,$mday,$hour,$min,$sec) = split(/[\s-:]+/, $recovery_specific_time);		
			$recovery_specific_time = timegm($sec,$min,$hour,$mday,$mon-1,$year);
		}
		
		$xml .= '<FunctionRequest Id="'.$src_host_id.'" Name="GetCommonRecoveryPoint" Include="N">
					<Parameter Name="SourceHostID"  Value="'.$src_host_id.'" />
					<Parameter Name="TargetHostID"  Value="'.$tgt_host_id.'" />
					<Parameter Name="Option" Value="'.$recovery_option.'" />';
		if($recovery_specific_time)
		{			
			$xml .='<Parameter Name="Time" Value= "'.$recovery_specific_time.'" />';
		}
		$xml .= '</FunctionRequest>';
	}	
				
	my $request_xml = ($xml) ? $self->get_cx_api_header() . $xml . $self->get_cx_api_footer() : "";
		
	if(!$request_xml)
	{
		$logging_obj->log("EXCEPTION", "perform_readiness_check : Error creating XML");
		$recovery_update->{"recoveryError"} = 'Required information not received';
	}
	else
	{
		$logging_obj->log("DEBUG", "perform_readiness_check : Request XML sent as \n".$request_xml);
		my $parsed_data = $self->call_cx_api($request_xml);
        if(!$parsed_data)
        {
            $logging_obj->log("EXCEPTION", "perform_readiness_check : Error getting the respones of GetCommonRecoveryPoint API Call");
            $recovery_update->{"recoveryError"} = 'Error getting the respones of GetCommonRecoveryPoint API Call';
        }
        else
        {
            $recovery_update = $self->parse_GetCommonRecoveryPoint_response($readiness_check_data,$parsed_data);
        }
	}

	$recovery_update->{"recoveryId"} = $recovery_id;
	$recovery_update->{"recoveryStep"} = $recovery_status;
	return $recovery_update;
}

sub get_agent_name
{
	my($self, $id) = @_;
	
	my $host_name = $self->{'conn'}->sql_get_value("hosts","name","id = '$id'");	
	return $host_name;
}

sub get_agent_details
{
	my($self, $hostname) = @_;
	my %agent_details;
	my $sql = "SELECT id,vxAgentPath,fxAgentPath,osFlag FROM hosts WHERE name = '$hostname' OR ipAddress = '$hostname'";
	my $agent_details = $self->{'conn'}->sql_exec($sql);
	return $agent_details->[0];
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

sub call_cx_api
{
	my ($self, $request_xml) = @_;
	
	my $userAgent = LWP::UserAgent->new();	
	my $response = $userAgent->post($self->{'cs_api_url'}, Content_Type => 'text/xml', Content => $request_xml);

	#my $reponse = $response->error_as_HTML unless $response->is_success;
	$logging_obj->log("DEBUG","call_cx_api : Reponse : ".Dumper($response));
	#my $response_str = $response->as_string;	
	
	my $response_xml = $response->{"_content"};
	#print "Response = $response_xml\n";
	
	my $parsed_data;
	if(!$response_xml)
	{
		$parsed_data = $self->log_recovery_error("call_cx_api","Response for the URL : ".$self->{'cs_api_url'}." is improper",$parsed_data);
		return $parsed_data;
	}
	
	my $tpp = XML::TreePP->new();
	$parsed_data = $tpp->parse( $response_xml );
	$logging_obj->log("DEBUG","call_cx_api : Reponse XML HASH : ".Dumper($parsed_data));
	
	return $parsed_data;
}

sub parse_GetCommonRecoveryPoint_response
{
	my ($self, $readiness_check_data, $parsed_data) = @_;

	my ($return_data, $recoveryTag);
	my %response = %{$parsed_data->{'Response'}->{'Body'}};
	my $res = $response{'Function'};
	my $resp_type = ref($res);
	
	my %data = %$readiness_check_data;
	
	my $response_list;
	
	if($resp_type eq 'ARRAY')
	{
		foreach my $fresp (@{$res})
		{
			my $host_id = $fresp->{"-Id"};
			$response_list->{$host_id} = $fresp;
		}
	}
	else
	{
		my $host_id = $res->{"-Id"};
		if($res->{"-Message"} ne "Success")
		{
			$return_data->{'readinessCheckLog'} = $res->{"-Message"};
			$return_data->{'readinessCheckStatus'} = "RC_FAILED";
			return $return_data;
		}
		$response_list->{$host_id} = $res;
	}
	
	my $response_parsed;
	foreach my $host_id (keys %{$response_list})
	{
		my $resp_params  = $response_list->{$host_id}->{'FunctionResponse'}->{'Parameter'};
		my $resp_params_type = ref($resp_params);
		
		if($resp_params_type eq 'ARRAY')
		{
			foreach my $resp_param (@{$resp_params})
			{
				$response_parsed->{$host_id}->{$resp_param->{'-Name'}} = $resp_param->{'-Value'};
			}
		}
		else
		{
			$response_parsed->{$host_id}->{$resp_params->{'-Name'}} = $resp_params->{'-Value'};
		}		
	}
		
	$return_data->{'readinessCheckStatus'} = 'RC_PASSED';	
	foreach my $host_id (keys %{$response_parsed})
	{
		my $host_name = $self->get_agent_name($host_id);
		my $recovery_option = $data{"host_data"}{$host_id}{"recovery_option"};
		$return_data->{'readinessCheckLog'} .= "Recovery point corresponding to the recovery option ".$recovery_option." for host ".$host_name." is available. ";
		if($recovery_option eq "LATEST_TIME" or $recovery_option eq "SPECIFIC_TIME")
		{				
			if($response_parsed->{$host_id}->{"CommonTimeExists"} ne "True")
			{
				$return_data->{'readinessCheckStatus'} = 'RC_FAILED';					
			}			
		}
		elsif($recovery_option eq "LATEST_TAG")
		{
			if($response_parsed->{$host_id}->{"CommonTagExists"} ne "True")
			{
				$return_data->{'readinessCheckStatus'} = 'RC_FAILED';				
			}
		}
		elsif($recovery_option eq "TAG_AT_SPECIFIC_TIME")
		{
			if($response_parsed->{$host_id}->{"CommonTagExists"} ne "True")
			{
				$return_data->{'readinessCheckStatus'} = 'RC_FAILED';				
			}
			else
			{
				#$return_data->{'recoveryTag'} = $response_parsed->{$host_id}->{"BeforeTagName"};
				my $user_given_time = $data{'recoveryTimeStamp'};
				my $b_tag_time = $response_parsed->{$host_id}->{"BeforeTagTime"};
				$recoveryTag->{$host_id} = $response_parsed->{$host_id}->{"BeforeTagName"};
				
				my $b_tag_time_formatted_date = $self->get_formatted_time("date", $b_tag_time);
				
				if($user_given_time - $b_tag_time >= 86400)
				{
					$return_data->{'readinessCheckStatus'} = 'RC_PASSED_WITH_WARNING';
					$return_data->{'readinessCheckLog'} .= "Recovery point for server ".$host_name." is not available at the choosen time. The latest available recovery time is ".$b_tag_time_formatted_date." (".$recoveryTag->{$host_id}."). ";
				}
				else
				{
					$return_data->{'readinessCheckLog'} .= "The latest available recovery time for server ".$host_name." is ".$b_tag_time_formatted_date." (".$recoveryTag->{$host_id}."). ";			
				}
			}
		}
		if($return_data->{'readinessCheckStatus'} eq 'RC_FAILED')
		{
			$return_data->{'readinessCheckLog'} = "Recovery point corresponding to the recovery option ".$recovery_option." for host ".$host_name." is not available";
			return $return_data;
		}
	}

	if($recoveryTag)
	{
		$return_data->{'recoveryTag'} = serialize($recoveryTag);
	}
	
	return $return_data;	
}

sub log_recovery_error
{
	my ($self,$function_name, $error_string,$recovery_update, $job_type) = @_;
	
	if($function_name eq "perform_readiness_check")
	{
		$recovery_update->{'readinessCheckLog'} = $error_string;	
		$recovery_update->{'readinessCheckStatus'} = 'RC_FAILED';
	}
	else
	{
		if($function_name eq "create_recovery_job" || $function_name eq "get_recovery_status")
		{
			$recovery_update->{'recoveryStepProgress'} = $self->add_error_string($job_type, $error_string);
		}	
		$recovery_update->{'recoveryError'} = $error_string;		
        if(($function_name eq "get_recovery_status") && ($error_string eq "Pending"))
        {
            delete($recovery_update->{'recoveryError'});
        }
	}

	$logging_obj->log("EXCEPTION","$function_name : $error_string");

	return $recovery_update;	
}

sub add_error_string
{
	my ($self, $job_type, $status) = @_;
	my $fx_creation_error;
    
    if($status eq "Pending")
	{
		$fx_creation_error =    'a:4:{i:0;a:6:{s:8:"TaskName";s:26:"Initializing Recovery Plan";s:11:"Description";s:86:"This will initializes the Recovery Plan. It starts the EsxUtil.exe binary for Recovery";s:10:"TaskStatus";s:11:"InProgress";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:1;a:6:{s:8:"TaskName";s:31:"Downloading Configuration Files";s:11:"Description";s:65:"The files which are going to download from CX are 1. Recovery.xml";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:2;a:6:{s:8:"TaskName";s:36:"Starting Recovery For Selected VM(s)";s:11:"Description";s:225:"The following operations going to perform in this task: 1. Remove pairs for all the selected VMs 2. Completes network related changes for all VMs 3. Deploys the source disk layout on respective target disk(in case of windows)";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:3;a:6:{s:8:"TaskName";s:31:"Powering on the recovered VM(s)";s:11:"Description";s:0:"";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}}';		
	}
	elsif($job_type eq "recovery")
	{	
		$fx_creation_error =	'a:4:{i:0;a:6:{s:8:"TaskName";s:26:"Initializing Recovery Plan";s:11:"Description";s:86:"This will initializes the Recovery Plan. It starts the EsxUtil.exe binary for Recovery";s:10:"TaskStatus";s:6:"Failed";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:1;a:6:{s:8:"TaskName";s:31:"Downloading Configuration Files";s:11:"Description";s:65:"The files which are going to download from CX are 1. Recovery.xml";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:2;a:6:{s:8:"TaskName";s:36:"Starting Recovery For Selected VM(s)";s:11:"Description";s:225:"The following operations going to perform in this task: 1. Remove pairs for all the selected VMs 2. Completes network related changes for all VMs 3. Deploys the source disk layout on respective target disk(in case of windows)";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}i:3;a:6:{s:8:"TaskName";s:31:"Powering on the recovered VM(s)";s:11:"Description";s:0:"";s:10:"TaskStatus";s:6:"Queued";s:12:"ErrorMessage";s:0:"";s:8:"FixSteps";s:0:"";s:7:"LogPath";s:0:"";}}';	
	}
	elsif($job_type eq "snapshot")
	{
		$fx_creation_error =	'a:7:{i:0;a:6:{s:8:"FixSteps";s:0:"";s:10:"TaskStatus";s:6:"Failed";s:8:"TaskName";s:26:"Initializing Dr-Drill Plan";s:12:"ErrorMessage";s:13:"Invalid input";s:11:"Description";s:115:"This will initializes the Dr-Drill Plan.           It starts the EsxUtilWin.exe binary for taking physical snapshot";s:7:"LogPath";s:0:"";}i:1;a:6:{s:8:"FixSteps";s:0:"";s:10:"TaskStatus";s:6:"Queued";s:8:"TaskName";s:31:"Downloading Configuration Files";s:12:"ErrorMessage";s:0:"";s:11:"Description";s:117:"The files which are going to download from CX are            1. snapshot.xml            2. Inmage_scsi_unit_disks.txt";s:7:"LogPath";s:0:"";}i:2;a:6:{s:8:"FixSteps";s:0:"";s:10:"TaskStatus";s:6:"Queued";s:8:"TaskName";s:29:"Preparing Master Target Disks";s:12:"ErrorMessage";s:0:"";s:11:"Description";s:210:"The following operations going to perform in this task:           1. Discovers all the source respective target disks in MT            2. Offline and clean the disks           3. initialise and online the disks";s:7:"LogPath";s:0:"";}i:3;a:6:{s:8:"FixSteps";s:152:"Check the MT disks are in proper state or not Check the volume layouts created on teh disksRerun the job. If still fails contact inmage customer support";s:10:"TaskStatus";s:6:"Queued";s:8:"TaskName";s:21:"Updating Disk Layouts";s:12:"ErrorMessage";s:125:"Failed to assign mount ponits to the newly created Volumes.For extended Error information download EsxUtilWin.log in vCon Wiz";s:11:"Description";s:182:"The following operations going to perform in this task:           1. Deploys the source disk layout on respective target disk           2. Creates the Source volumes at Target disks.";s:7:"LogPath";s:0:"";}i:4;a:6:{s:8:"FixSteps";s:0:"";s:10:"TaskStatus";s:6:"Queued";s:8:"TaskName";s:40:"Initializing Physical Snapshot Operation";s:12:"ErrorMessage";s:0:"";s:11:"Description";s:116:"This will initializes the Physical snapshot.           It starts the EsxUtil.exe binary for taking physical snapshot";s:7:"LogPath";s:0:"";}i:5;a:6:{s:8:"FixSteps";s:0:"";s:10:"TaskStatus";s:6:"Queued";s:8:"TaskName";s:45:"Starting Physical Snapshot For Selected VM(s)";s:12:"ErrorMessage";s:0:"";s:11:"Description";s:268:"The following operations going to perform in this task:           1. takes physical snapshot for all the selected VMs            2. Completes network related changes for all VMs            3. Deploys the source disk layout on respective target disk(in case of windows)";s:7:"LogPath";s:0:"";}i:6;a:6:{s:8:"FixSteps";s:0:"";s:10:"TaskStatus";s:6:"Queued";s:8:"TaskName";s:30:"Powering on the dr drill VM(s)";s:12:"ErrorMessage";s:0:"";s:11:"Description";s:143:"This will power-on all the dr-drilled VMs            1. It will detach all the snapshot disks from MT            2. Power-on the Dr-driiled VMs";s:7:"LogPath";s:0:"";}}';
	}	
	return $fx_creation_error;
}

sub get_formatted_time
{
	my ($self, $format, $date) = @_;
	
	my $sql_patch;
	if($format eq "date")
	{
		my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst,$time);
		($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime($date);
		$year += 1900;
		$mon += 1;
		
		$mon = sprintf("%02d", $mon);
		$mday = sprintf("%02d", $mday);
		$hour = sprintf("%02d", $hour);
		$min = sprintf("%02d", $min);
		$sec = sprintf("%02d", $sec);
		
		$time = $year."-".$mon."-".$mday." ".$hour.":".$min.":".$sec;
		return $time;
	}
	elsif($format eq "timestamp")
	{
		$sql_patch = " UNIX_TIMESTAMP($date) ";
	}
	
	my $sql = "SELECT ".$sql_patch." as user_defined_time";
	my $sth = $self->{'conn'}->sql_query($sql);
	my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($sth);
	my $user_time = $ref->{"user_defined_time"};
	
	return $user_time;
}

#####Connect#####
##Description 		:: It makes a connection with vCenter/vSphere after validating ESX IP passed.
##Input 			:: ESX_IP, Username and Password.
##Output 			:: Returns 2 on invalid IP, returns 3 if failed to establish connection else returns 1 on success.
#####Connect#####
sub Connect
{
	my %Connect_args 	= @_;
	
	chomp ( $Connect_args{Server_IP} );
	my $iretVal 		= is_a_valid_ip_address( ip => $Connect_args{Server_IP} );
	if ( $iretVal == 0 )
	{
		$logging_obj->log("EXCEPTION"," :: Invalid IP address entered for ESX IP.Please provide a valid ESX IP.");
		return ( 0 );
	}
	
	$Connect_args{Server_IP} = $iretVal;
	eval
	{
		my $server = $Connect_args{Server_IP};
		my $username = $Connect_args{UserName};
		my $password = $Connect_args{Password};		
		my $url 		= "https://$server/sdk/webService";
		
		my $return_code 	= Vim::login(service_url => $url, user_name => $username, password => $password );
		$logging_obj->log("DEBUG","Successfully Connected to vCenter/vSphere Server : $server .");		
	};
	if ( $@ )
	{
		$logging_obj->log("DEBUG","$@.");
		$logging_obj->log("EXCEPTION"," :: Failed to connect with vCenter/vSphere server $Connect_args{ESX_IP}.");
		return ( 0 );
	}
	return ( 1 );
}

#####is_a_valid_ip_address#####
##Description 		:: validates whether the given IP is valid or not.
##Input 			:: ip value.
##Output 			:: if valid returns 1 else returns IP address by triming  all leading zero's.
#####is_a_valid_ip_address#####
sub is_a_valid_ip_address
{
	my %args			= @_;
	my $new_ip 			= "";
	
	$logging_obj->log("DEBUG","IP address received for validation : $args{ip}.",2);
	##some times we might receive host name instead of IP. So we need to check whether it contains a 
	if ( substr( $args{ip}, length($args{ip})-1, length($args{ip} ) ) !~ /\d/ )
	{
		$logging_obj->log("EXCEPTION"," :: Invalid input : \"$args{ip}\".Please enter a valid IPv4 address.",3);
		return 0;
	}
	
	my @subparts 		= split( '\.',$args{ip} );
	if ( 4 == ($#subparts + 1) )
	{
		for ( my $i = 0 ; $i <= $#subparts; $i++ )
		{
			if ( $subparts[$i] =~ /^[0-9]{1,3}$/ )
			{
				if ( ( $subparts[$i] > 255 ) || ( $subparts[$i] < 0 ) )
				{
					$logging_obj->log("EXCEPTION"," :: Invalid input : \"$args{ip}\".Please enter a valid IPv4 address.",3);
					return 0;
				}
				$subparts[$i] =~ s/^(0+)//g;
				
				if ( !length ( $subparts[$i] ) )
				{
					$subparts[$i] = 0;
				}
				
				if ( !length ( $new_ip ) )
				{
					$new_ip = $subparts[$i];
					next;					
				}
				$new_ip = $new_ip.".".$subparts[$i];
			}
			else
			{
				$logging_obj->log("EXCEPTION"," :: Invalid input : \"$args{ip}\".Please enter a valid IPv4 address.",3);
				return 0;
			}
		}
	}
	else
	{
		$logging_obj->log("EXCEPTION"," :: Invalid input : \"$args{ip}\".Please enter a valid IPv4 address.",3);
		return 0;
	}
	
	$logging_obj->log("DEBUG","Returning IP address $new_ip.",2);
	return $new_ip;
}

sub format_plan_data
{
	my($self,%plan_data) = @_;   

	my %formatted_plan_data;
	my $recovery_update;
	
	$formatted_plan_data{"recovery_id"} = $plan_data{"recoveryId"};
	$formatted_plan_data{"plan_name"} = $plan_data{"planName"};
	$formatted_plan_data{"recovery_plan_name"} = $plan_data{"recoveryPlanName"};
	$formatted_plan_data{"batch_count"} = $plan_data{"batchCount"};
	$formatted_plan_data{"recovery_plan_id"} = $plan_data{"recoveryPlanId"};
	$formatted_plan_data{"recovery_status"} = $plan_data{"recoveryStatus"};
	$formatted_plan_data{"readiness_check_status"} = $plan_data{"readinessCheckStatus"};
	$formatted_plan_data{"fx_src"} = $plan_data{"srcHost"};
	$formatted_plan_data{"fx_tgt"} = $plan_data{"destHost"};
	$formatted_plan_data{"esx_ip"} = $plan_data{"esxIp"};
	$formatted_plan_data{"esx_username"} = $plan_data{"esxUsername"};
	$formatted_plan_data{"esx_password"} = $plan_data{"esxPassword"};
	
	my $hosts = unserialize($plan_data{"hosts"});
	my $recovery_order = unserialize($plan_data{"recoveryOrder"});	
	my $recovery_option = unserialize($plan_data{"recoveryOption"});	
	my $global_recovery_option = $recovery_option->{"all"}->{"recovery_option"};	
	my $global_recovery_time = $recovery_option->{"all"}->{"recovery_specific_time"};	
	my $network_config_hosts = unserialize($plan_data{"networkConfig_hosts"});
	my $hardware_config_hosts = unserialize($plan_data{"hardwareConfig_hosts"});	
	my $recovery_tag;
	if($plan_data{"recoveryTag"})
	{
		$recovery_tag = unserialize($plan_data{"recoveryTag"});
	}
	
	my @recovery_hosts = keys(%{$hosts});
	
	foreach my $host_id (@recovery_hosts)
	{
		my $host_data;
		$host_data->{"target_host_id"} = $hosts->{$host_id};
		$host_data->{"recovery_order"} = $recovery_order->{$host_id};
		if($global_recovery_option)
		{
			$host_data->{"recovery_option"} = $global_recovery_option;#) ? $global_recovery_option : 
			$host_data->{"recovery_specific_time"} = $global_recovery_time;#) ? $global_recovery_time : 
		}
		else
		{
			if(exists($recovery_option->{"server"}->{$host_id}))
			{
				$host_data->{"recovery_option"} = $recovery_option->{"server"}->{$host_id}->{"recovery_option"};
				$host_data->{"recovery_specific_time"} = $recovery_option->{"server"}->{$host_id}->{"recovery_specific_time"};
			}
			else
			{
				
			}
		}
		$host_data->{"network_config_hosts"} = $network_config_hosts->{$host_id};	
		$host_data->{"hardware_config_hosts"} = $hardware_config_hosts->{$host_id};	
		$host_data->{"recovery_tag"} = $recovery_tag->{$host_id};	
		$host_data->{"host_name"} = $self->get_agent_name($host_id);	
		$formatted_plan_data{"host_data"}->{$host_id} = $host_data;
	}
	return \%formatted_plan_data;
}

1;
