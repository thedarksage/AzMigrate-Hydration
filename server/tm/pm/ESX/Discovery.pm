package ESX::Discovery;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Cwd;
use DBI();
use HTTP::Request::Common;
use XML::Simple;
use Net::FTP;
use Common::Log;
use Common::Constants;
use Common::DB::Connection;
use Data::Dumper;
use Utilities;
use tmanager;
use PHP::Serialization qw(serialize unserialize);
use File::Basename;
use File::Path;
use RequestHTTP;
use MDSErrors;
use XML::LibXML;

my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
my $BASE_DIR = $AMETHYST_VARS->{"INSTALLATION_DIR"};
my $cs_installation_path = Utilities::get_cs_installation_path();
my $logging_obj  = new Common::Log();

my $DISCOVERY_FAILURE     = 1;
my $DISCOVERY_INVALIDIP    = 2;
my $DISCOVERY_INVALIDCREDENTIALS  = 3;
my $DISCOVERY_EXIST      = 4;
my $DISCOVERY_NOTFOUND    = -1;
my $DISCOVERY_INVALIDARGS   = 5;
my $DISCOVERY_NOVMSFOUND    = 6;

my @discovery_jobs = ("InfrastructureDiscovery", "UpdateInfrastructureDiscovery");

my $TELEMETRY = Common::Constants::LOG_TO_TELEMETRY;
my $MDS_AND_TELEMETRY = Common::Constants::LOG_TO_MDS_AND_TELEMETRY;

sub new
{
	my ($class, %args) = @_;
	my $self = {};
		
	#$self->{'conn'} = $args{'conn'};
	while (my($key, $value) = each %args)
	{
	    $self->{$key} = $value;
	}
	
	bless($self, $class);
}

sub request_discovery_info
{
	# Request XML for the GetCloudDiscovery API
	# Gets the cloud recovery requests for the PS
	my ($response_xml,$response,$https_mode);
	my $parsed_data;
	my %log_data;
	
	my %telemetry_metadata = ('RecordType' => 'VcenterDiscovery');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);

	
	eval
	{
		my $body_xml = "<Body>
									<FunctionRequest Name='GetCloudDiscovery'>
									<Parameter Name='HostId' Value='".$AMETHYST_VARS->{'HOST_GUID'}."' />
									</FunctionRequest>
						</Body>";
			
		my $http_method = "POST";
		$https_mode = ($AMETHYST_VARS->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $AMETHYST_VARS->{'PS_CS_IP'}, 'PORT' => $AMETHYST_VARS->{'PS_CS_PORT'});
		my $param = {'access_method_name' => "GetCloudDiscovery", 'access_file' => "/ScoutAPI/CXAPI.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'type' => 'text/xml' , 'content' => $body_xml};

		# Calling the CX API
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);
		$response_xml = $response->{"content"};
		
        #bug: 5922423  - Do not log credentials even in Debug mode in CS
        # commented the below lines to address  the bug 5922423		
		#$logging_obj->log("DEBUG","request_discovery_info : Reponse ::".Dumper($response));
		
		# Verifying the response of the API
		if (! $response->{'status'})
		{
			$log_data{"Message"} = "Failed to get discovery information";
			$log_data{"Status"} = $response->{'status'};
			$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
			return $parsed_data;
		}
	};
	if($@)
	{
		$log_data{"Status"} = "request_discovery_info Fail";
		$log_data{"Reason"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
		return $parsed_data;
	}
	
	# Parsing the API XML response
	my $xml_output;
	eval
	{
		my $parse = new XML::Simple();
		$parsed_data = $parse->XMLin($response_xml);
		
		#bug: 5922423  - Do not log credentials even in Debug mode in CS
		# do not enable the below debug statement
		#$logging_obj->log("DEBUG","request_discovery_info : Reponse XML HASH : ".Dumper($parsed_data));
		
		$xml_output = &parse_xml_data($parsed_data);
		
		#bug: 5922423  - Do not log credentials even in Debug mode in CS
		# do not enable the below debug statement		
		#$logging_obj->log("DEBUG","request_discovery_info : Parsed XML Output : ".Dumper($xml_output));
	};
	if($@)
	{
		$log_data{"Message"} = "request_discovery_info,Failed to parse the response";
		$log_data{"Reason"} = $@;
		$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
	}
	return $xml_output;
}

sub parse_xml_data
{
	my($parsed_data) = @_;
	my %response = %{$parsed_data->{'Body'}};
	my $res = $response{'Function'};	
	my $responses = &parse_node($res->{'FunctionResponse'});
	return $responses;
}

sub parse_node
{
	my ($node) = @_;
	my $parsed_details;
	
	my $resp_type = ref($node->{"ParameterGroup"});
	my @node_details = $resp_type eq "ARRAY" ? @{$node->{"ParameterGroup"}} : $node->{"ParameterGroup"};
	foreach my $node_detail (@node_details)
	{
		my $key = $node_detail->{"Id"};
		if($node_detail->{"ParameterGroup"})
		{
			my $id = $node_detail->{"ParameterGroup"}->{"Id"};
			$parsed_details->{$id} = &parse_node($node_detail);
		}
		else
		{
			my $parameters = $node_detail->{"Parameter"};
			foreach my $parameter (@$parameters)
			{
				$parsed_details->{$key}->{$parameter->{"Name"}} = $parameter->{"Value"};
			}
		}
	}
	return $parsed_details;
}

sub process_job
{
	my ($self,$result) = @_;
	
	if(exists ($result->{'faultCode'}))
	{
		return;
	}
	
	$self->{'conn'} = new Common::DB::Connection();
	
	my $rx_key = $self->{'conn'}->sql_get_value("rxSettings","ValueData","ValueName = 'RX_KEY'");
	my $rx_ip = $self->{'conn'}->sql_get_value("rxSettings","ValueData","ValueName = 'IP_ADDRESS'");
	my $rx_port = $self->{'conn'}->sql_get_value("rxSettings","ValueData","ValueName = 'HTTP_PORT'");
	my $secured_communication = $self->{'conn'}->sql_get_value("rxSettings","ValueData","ValueName = 'SECURED_COMMUNICATION'");
	my $http = ($secured_communication) ? "https" : "http";
	
	my %data = %{$result};
	
	my @synced_hosts;
	
	foreach my $inventory (values %data)
	{
		my $rxInventoryId = $inventory->{"inventoryId"};
		
		push (@synced_hosts, $rxInventoryId);
		
		my $siteName = $inventory->{"siteName"};
		my $hostType = $inventory->{"hostType"};				
		my $hostIp = $inventory->{"hostIp"};
		my $login = $self->{'conn'}->sql_escape_string($inventory->{"login"});
		my $passwd = $inventory->{"passwd"};
		my $organization = $inventory->{"organization"};
		my $esxAgentId = $inventory->{"esxAgentId"};
		my $discoverNow = $inventory->{"discoverNow"};
		
		my $creationTime = $inventory->{"creationTime"};
		my $infrastructureType = $inventory->{"infrastructureType"};
		my $autoDiscovery = $inventory->{"autoDiscovery"};
		my $discoveryInterval = $inventory->{"discoveryInterval"};
		my $autoLoginVerify = $inventory->{"autoLoginVerify"};
	
		# Yet to Add the CS ID and other conditions required.
		my $inventoryId = $self->{'conn'}->sql_get_value('infrastructureInventory', 'inventoryId', "rxInventoryId = '$rxInventoryId'");
		my $sql = ($inventoryId) ?  "UPDATE" : "INSERT INTO";
		$sql .=	"	infrastructureInventory
						 SET
							siteName = '$siteName',
							hostType = '$hostType',
							hostIp = '$hostIp',
							login = '$login',
							passwd = '$passwd',
							organization = '$organization',
							esxAgentId = '$esxAgentId',
							discoverNow = '$discoverNow',
							creationTime = '$creationTime',
							infrastructureType = '$infrastructureType',
							autoDiscovery = '$autoDiscovery',
							discoveryInterval = '$discoveryInterval',
							autoLoginVerify = '$autoLoginVerify', 
							rxInventoryId = '$rxInventoryId'"; 
		$sql .= ($inventoryId) ?  " WHERE inventoryId = '$inventoryId'" : "";
		
		$self->{'conn'}->sql_query($sql);
	}
	
	my $synced_hosts_str = join("','",@synced_hosts);
	my $del_sql = "DELETE
					FROM
						infrastructureInventory
					WHERE
						rxInventoryId != '0' 
					AND					
						rxInventoryId not in ('$synced_hosts_str')";
				
	$self->{'conn'}->sql_query($del_sql);	

	# Discovery Update to RX
	my $discovery_data_dir = $self->{'BASE_DIR'}."/var/esx_discovery_data";
	my @files = <$discovery_data_dir/*>;

	if(@files)
	{
		foreach my $out_file (@files)
		{
			my $file_name = basename($out_file);
			my @file_name_split = split(/_/,$file_name);
			if($file_name_split[0] eq $rx_key)
			{
				next;
			}
			my $move_file_name = $discovery_data_dir."/".$rx_key."_".$file_name;
			rename $out_file, $move_file_name;
		}
		
		# Send the discovery data to RX	
		my $web_dir  = $BASE_DIR.'/admin/web';
		if(Utilities::isWindows())
		{
			$web_dir =~ s/\//\\\\/g;
		}
		my $curl_php_script = $web_dir."/curl_dispatch_http.php";								
		my $rx_url = $http."://".$rx_ip.":".$rx_port."/uploadDataFile.php";					
		my @files = <$discovery_data_dir/$rx_key*>;
		foreach my $out_file (@files)
		{						
			my $post_data = "data_file##@".$out_file; 
			my $status = `php "$curl_php_script" "$rx_url" "$post_data"`;					
			unlink ( $out_file );
		}
	}
	
	# Update failures to the RX
	my $discovery_failures = $self->{'conn'}->sql_exec("SELECT * FROM infrastructureInventory WHERE loginVerifyTime != '0000-00-00 00:00:00' AND (loginVerifyStatus = '0' OR connectionStatus = '0')","rxInventoryId");
	
	return $discovery_failures;
}

sub discover()
{	
	my ($self) = @_;
	my %log_data;
	my %telemetry_metadata = ('RecordType' => 'VcenterDiscovery');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
	$self->{'conn'} = new Common::DB::Connection();
	
	# Get PS hostname
	my $ps_hostname = `hostname`;
	chomp($ps_hostname);
	
	# Get PS IP Address
	my $ps_ip_address = &Utilities::getIpAddress();
	chomp($ps_ip_address);
	
	my $discovery_files;
	
	# Create required directories
	my $inv_file_path = "/home/svsystems/var/esx_discovery_data";
	my $vcon_log_path = "/home/svsystems/var/vcon/logs";
	if(Utilities::isWindows())
	{
		$inv_file_path = $cs_installation_path."\\home\\svsystems\\var\\esx_discovery_data";
		$vcon_log_path = $cs_installation_path."\\home\\svsystems\\var\\vcon\\logs";
	}
	
	if(!-d $inv_file_path)
	{
		mkpath($inv_file_path, { mode => 0777 });
		if ($? != 0) 
		{
			$log_data{"Message"} = "discover,Could not create directory";
			$log_data{"FilePath"} = $inv_file_path;
			$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
			
			return 0;
		}
	}

	if(!-d $vcon_log_path)
	{	
		mkpath($vcon_log_path, { mode => 0777 });
		if ($? != 0) 
		{
			$log_data{"Message"} = "discover,Could not create directory";
			$log_data{"FilePath"} = $vcon_log_path;
			$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
			
			return 0;
		}
	}
	
	#Fetching infrastructure inventory table details
	my $inv_sql = "SELECT 
						inventoryId,
						hostType,						
						hostIp, 
						hostPort, 
						organization, 
						accountId, 
						loginVerifyStatus, 
						lastDiscoverySuccessTime, 
						now() as currentTime 
					FROM 
						infrastructureInventory 
					WHERE 
						hostType IN ('vSphere','vCenter','vCloud','HyperV')
								AND
								(
									(autoDiscovery = '1' AND 
										(
											((unix_timestamp(now()) - unix_timestamp(loginVerifyTime)) > discoveryInterval) OR 
											(unix_timestamp(now()) < unix_timestamp(loginVerifyTime))
										)
									) OR 
									discoverNow = '1'
								)";
	my $inv_details =  $self->{'conn'}->sql_exec($inv_sql);
		
	
	foreach my $inventory_item (@$inv_details)
	{
		#$logging_obj->log("DEBUG","discover : Processing for inventory ID: ".$inventory_id);
		$log_data{"Message"} = "Processing Discovery for inventory";
		$log_data{"InventoryId"} = $inventory_item->{"inventoryId"};
		$logging_obj->log("INFO",\%log_data,$TELEMETRY);
		
		my $discovery_type = $inventory_item->{"hostType"};
		my $server_ip = $inventory_item->{"hostIp"};
		my $server_port = $inventory_item->{"hostPort"};
        my $server_ip_port = $server_ip.":".$server_port;
		my $organization = $inventory_item->{"organization"};
		my $account_id = $inventory_item->{"accountId"};
        
		%telemetry_metadata = ('RecordType' => 'VcenterDiscovery', 'ServerIp' => $server_ip);
		$logging_obj->set_telemetry_metadata(%telemetry_metadata);
		
        my $discovery_output;
		my $error_log = Utilities::makePath($BASE_DIR."/var/vcon/logs/vContinuum_ESX.log");		
		
        if((Utilities::isLinux()) && (!Utilities::verify_module_installed('VMware::VIRuntime') || !Utilities::verify_module_installed('VMware::VILib') || !Utilities::verify_module_installed('VMware::VIExt')) && ((lc($discovery_type) eq "vsphere") or (lc($discovery_type) eq "vcenter") or (lc($discovery_type) eq "vcloud")))
        {
            my $discovery_error;			
			my $error_placeholders = { 'vCenterIP' => $server_ip, 'vCenterPort' => $server_port, 'ErrorCode' => 'EC0403', 'PsName' => $ps_hostname, 'PsIP' => $ps_ip_address };
            $discovery_error->{'error_code'} = "EC0403";
            $discovery_error->{'error'} = "Discovery of vCenter server ".$server_ip." failed.";
            $discovery_error->{'error_log'} = "Discovery of the vCenter server ".$server_ip.":".$server_port." failed with the error code 1. Either the vSphere CLI is not installed on the process server ".$ps_hostname." (".$ps_ip_address.") or the vSphere CLI version installed is incompatible..";
            $discovery_error->{'error_placeholders'} = $error_placeholders;
            $discovery_output = $discovery_error;
        }
		else
        {
			my $vcli_version;
			if( Utilities::isLinux() && ((lc($discovery_type) eq "vsphere") or (lc($discovery_type) eq "vcenter") or (lc($discovery_type) eq "vcloud")))
			{
				$vcli_version = $self->get_vcli_version();
			}
			if (Utilities::isLinux() && ($vcli_version ne "5.1" && $vcli_version ne "5.5" && ((lc($discovery_type) eq "vsphere") or (lc($discovery_type) eq "vcenter") or (lc($discovery_type) eq "vcloud"))))
			{
				my $discovery_error;			
				my $error_placeholders = { 'vCenterIP' => $server_ip, 'vCenterPort' => $server_port, 'ErrorCode' => 'EC0403', 'PsName' => $ps_hostname, 'PsIP' => $ps_ip_address };
				$discovery_error->{'error_code'} = "EC0403";
				$discovery_error->{'error'} = "Discovery of vCenter server ".$server_ip." failed.";
				$discovery_error->{'error_log'} = "Discovery of the vCenter server ".$server_ip.":".$server_port." failed with the error code 1. Either the vSphere CLI is not installed on the process server ".$ps_hostname." (".$ps_ip_address.") or the vSphere CLI version installed is incompatible..";
				$discovery_error->{'error_placeholders'} = $error_placeholders;
				$discovery_output = $discovery_error;
			}
			else
			{			
				my $discovery_command_args = "";		
				my ($discover_pl_path,$discover_pl);
				my $redirect_log = Utilities::makePath($BASE_DIR."/var/vcon_discovery.log");
				my $perl_cmd = '';
				my $discovery_command = '';
				
				if((lc($discovery_type) eq "vsphere") or (lc($discovery_type) eq "vcenter"))
				{
					if(Utilities::isWindows())
					{
						$discover_pl_path = $BASE_DIR."/bin";
						$discover_pl = "InMageDiscovery.exe";
						$discovery_command_args = " --server $server_ip_port --accountid \"$account_id\" --csip \"$AMETHYST_VARS->{\"PS_CS_IP\"}\" --csport \"$AMETHYST_VARS->{\"PS_CS_PORT\"}\" --ostype all --retrieve hosts!@!@!datastores";
						$discovery_command = $discover_pl." ".$discovery_command_args;
					}
				}
					
				if (Utilities::isLinux())
				{
					$discover_pl_path = Utilities::makePath($discover_pl_path);
					$discover_pl = Utilities::makePath($discover_pl_path."/".$discover_pl);
				}
				elsif(Utilities::isWindows())
				{
					$discover_pl_path =~ s/\//\\\\/g;
					$discover_pl_path = Utilities::makePath($discover_pl_path);
					$discover_pl = Utilities::makePath($discover_pl_path."\\".$discover_pl);
				}
					
				my $prev_path = getcwd;
				$logging_obj->log("DEBUG","discover : Discovery prev_path:".$prev_path);
				chdir($discover_pl_path);	
				
				#$logging_obj->log("DEBUG","discover : Discovery Command : ".$discovery_command);
				$discovery_output = `$discovery_command`;	
				
				my $return_val = $? >> 8;
				$logging_obj->log("DEBUG","discover : Discovery errorCode : ".$return_val);
				my $error_msg;
				if($return_val)
				{
					my $complete_error_log;
					# TO update to the MDS
					# Read the error log file and update the DB
					if( -e $error_log)
					{
						local $/ = undef;
						open(DISCOVERY_LOG_FILE, $error_log);
						$complete_error_log = <DISCOVERY_LOG_FILE>;
						close(DISCOVERY_LOG_FILE);
						local $/ = "\n";
					}
					my $discovery_error;
					my $error_placeholders;
					# NO VMs case to be handled
					if($return_val == $DISCOVERY_NOVMSFOUND)
					{
						$discovery_error->{'error_code'} = "0";
						$discovery_error->{'error'} = "";
						$discovery_error->{'error_placeholders'} = {};
						$discovery_error->{'error_log'} = "No VMs found";
					}
					elsif($return_val == $DISCOVERY_FAILURE)
					{
						$error_placeholders = { 'vCenterIP' => $server_ip, 'vCenterPort' => $server_port, 'ErrorCode' => 'EC0404', 'ErrorString' => $complete_error_log };
						$discovery_error->{'error_code'} = "EC0404";
						$discovery_error->{'error'} = "Discovery of the vCenter server ".$server_ip.":".$server_port." failed with error code  ".$return_val.". Operation failed with an error from vSphere SDK.";
						$discovery_error->{'error_placeholders'} = $error_placeholders;
						$discovery_error->{'error_log'} = $complete_error_log;
					}
					elsif($return_val == $DISCOVERY_INVALIDIP)
					{					
						$error_placeholders = { 'vCenterIP' => $server_ip, 'vCenterPort' => $server_port, 'ErrorCode' => 'EC0401', 'PsName' => $ps_hostname, 'PsIP' => $ps_ip_address };
						$discovery_error->{'error_code'} = "EC0401";
						$discovery_error->{'error'} = "Discovery of the vCenter server ".$server_ip.":".$server_port." failed with error code  ".$return_val.". The vCenter server is not accessible from the process server ".$ps_hostname." (".$ps_ip_address.")";
						$discovery_error->{'error_placeholders'} = $error_placeholders;
						$discovery_error->{'error_log'} = $complete_error_log;
					}
					elsif($return_val == $DISCOVERY_INVALIDCREDENTIALS)
					{					
						$error_placeholders = { 'vCenterIP' => $server_ip, 'vCenterPort' => $server_port, 'ErrorCode' => 'EC0402' };
						$discovery_error->{'error_code'} = "EC0402";
						$discovery_error->{'error'} = "Discovery of the vCenter server ".$server_ip.":".$server_port." failed with the error code  ".$return_val." due to invalid login credentials.";
						$discovery_error->{'error_placeholders'} = $error_placeholders;
						$discovery_error->{'error_log'} = $complete_error_log;
					}
					elsif($return_val == $DISCOVERY_INVALIDARGS || $return_val == $DISCOVERY_EXIST || $return_val == $DISCOVERY_NOTFOUND)
					{
						$error_placeholders = { 'vCenter' => $server_ip, 'hostip' => $server_ip, 'port' => $server_port, 'psname' => $ps_hostname };
						$discovery_error->{'error_code'} = "ECA0003";
						$discovery_error->{'error'} = "Internal Server Error";
						$discovery_error->{'error_placeholders'} = $error_placeholders;
						$discovery_error->{'error_log'} = $complete_error_log;
					}
					else
					{
						$error_placeholders = { 'vCenterIP' => $server_ip, 'vCenterPort' => $server_port, 'ErrorCode' => 'EC0404', 'ErrorString' => $complete_error_log };
						$discovery_error->{'error_code'} = "EC0404";
						$discovery_error->{'error'} = "Discovery of the vCenter server ".$server_ip.":".$server_port." failed with error code  ".$return_val.". Operation failed with an error from vSphere SDK.";
						$discovery_error->{'error_placeholders'} = $error_placeholders;
						$discovery_error->{'error_log'} = $complete_error_log;
					}
					$discovery_output = $discovery_error;
					
					$log_data{"ServerIp"} = $server_ip;
					$log_data{"ServerPort"} = $server_port;
					$log_data{"DiscoveryType"} = $discovery_type;
					$log_data{"ReturnVal"} = "Discovery failed with error code $return_val";
					$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
				}
			
				chdir($prev_path);
				$logging_obj->log("DEBUG","discover : Discovery Output : ".Dumper($discovery_output));
			}
        }
		
		my $disc_file_name = $server_ip."_".$organization."_discovery.txt";
        $disc_file_name =~ s/\s+//g;
		my $disc_file_path = $inv_file_path.'/'.$disc_file_name;
		
		if((Utilities::isWindows()) && ((lc($discovery_type) eq "vsphere") or (lc($discovery_type) eq "vcenter"))
			&& (ref($discovery_output) ne 'HASH'))
		{
			my $xml_file_name = $vcon_log_path. '/' . $server_ip ."_discovery.xml";
			my $returnCode = $self->info_xml_serialization( $xml_file_name, $disc_file_path, "all");
			if($returnCode != 0)
			{
				$log_data{"MESSAGE"} = "discover,Unable to serialize the xml.";
				$log_data{"ServerIp"} = $server_ip;
				$log_data{"ServerPort"} = $server_port;
				$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
				
				my $complete_error_log = "Unable to serialize the xml";
				my $error_placeholders = { 'vCenterIP' => $server_ip, 'vCenterPort' => $server_port, 'ErrorCode' => 'EC0404', 'ErrorString' => $complete_error_log };
				
				my $discovery_error;
				$discovery_error->{'error_code'} = "EC0404";
				$discovery_error->{'error'} = "Discovery of the vCenter server ".$server_ip.":".$server_port." failed with error code  ".$DISCOVERY_FAILURE.". Operation failed with an error from vSphere SDK.";
				$discovery_error->{'error_placeholders'} = $error_placeholders;
				$discovery_error->{'error_log'} = $complete_error_log;
				
				$discovery_output = serialize($discovery_error) if(ref($discovery_error) eq 'HASH' && exists($discovery_error->{'error'}));
				open(FH,">$disc_file_path") || die "Can't open for writing to file:$disc_file_path: $!";
				print FH $discovery_output;
				close(FH);
				next;
			}
		}
		
		if(((lc($discovery_type) ne "vsphere") && (lc($discovery_type) ne "vcenter")) || (ref($discovery_output) eq 'HASH' && exists($discovery_output->{'error'})))
		{
			$discovery_output = serialize($discovery_output) if(ref($discovery_output) eq 'HASH' && exists($discovery_output->{'error'}));
			open(FH,">$disc_file_path") || die "Can't open for writing to file:$disc_file_path: $!";
			print FH $discovery_output;
			close(FH);
		}
		
		$discovery_files->{$disc_file_path} = $inv_file_path;
	}
	
	if (defined $discovery_files)
	{
		$self->upload_discovery_files_to_CS($discovery_files);
	}
}

sub upload_discovery_files_to_CS
{
	my ($self,$upload_files) = @_;
	
	my $cs_ip = $AMETHYST_VARS->{'CS_IP'};		
	my $ps_cs_ip = $AMETHYST_VARS->{'PS_CS_IP'};	
	my $cx_type = $AMETHYST_VARS->{'CX_TYPE'};

	if (($cx_type eq 2) || ($cs_ip ne $ps_cs_ip))
	{
		&tmanager::upload_file($upload_files);
	}
}

sub update_discovery_info
{
	my ($self) = @_;
    my %log_data;
	my %telemetry_metadata = ('RecordType' => 'VcenterDiscovery');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
    my $get_hosts_sql = "SELECT id, ipAddress from hosts where id != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";
    $self->{'hosts_details'} = $self->{'conn'}->sql_get_hash($get_hosts_sql, "ipAddress", "id");
    
    my $get_protected_servers_sql = "SELECT distinct sourceHostId from srcLogicalVolumeDestLogicalVolume UNION SELECT distinct id from hosts where agentRole='MasterTarget' and ( UNIX_TIMESTAMP(now()) - (lastHostUpdateTime))  > 1800";
    my $protected_server_details = $self->{'conn'}->sql_get_hash($get_protected_servers_sql, "sourceHostId", "sourceHostId");
    my @protected_servers = (keys %$protected_server_details);
    $self->{'protected_servers'} = \@protected_servers;
	
	my $get_replica_vms_sql = "SELECT replicaId from infrastructureVMs where replicaId != ''";
    my $replica_vm_details = $self->{'conn'}->sql_get_hash($get_replica_vms_sql, "replicaId", "replicaId");
    my @replica_vms = (keys %$replica_vm_details);
    $self->{'replica_vms'} = \@replica_vms;
	
	my $discovery_data_dir = Utilities::makePath($BASE_DIR."/var/esx_discovery_data");
	#my @files = <$discovery_data_dir/*>;
	my @files = Utilities::read_directory_contents($discovery_data_dir);
	foreach my $out_file (@files)
	{
		my $esx_file_name = basename($out_file);
		my @files_split = split(/_/,$esx_file_name);
		my $server_ip = $files_split[0];
		my $organization = $files_split[1];
		my $get_discovery_type = "SELECT inventoryId, hostType FROM infrastructureInventory WHERE hostIp = '$server_ip' AND organization = '$organization'";
		my $discovery_info = $self->{'conn'}->sql_get_hash($get_discovery_type,"inventoryId","hostType");
		my ($inventory_id, $discovery_type ) = each %$discovery_info;
		
		local $/ = undef;	
		open(DISCOVERY_FILE,$discovery_data_dir."/".$out_file);		
		my $discovery_output = <DISCOVERY_FILE>;
		close(DISCOVERY_FILE);
		local $/ = "\n";
		
		%telemetry_metadata = ('RecordType' => 'VcenterDiscovery', 'ServerIp' => $server_ip, 'InventoryId' => $inventory_id);
		$logging_obj->set_telemetry_metadata(%telemetry_metadata);
		$logging_obj->log("INFO","Updating discovery::$out_file");
		
		eval
		{
            if(!$discovery_output)
            {
                $log_data{"Message"} = "No discovery data available, File content=Empty";
				$log_data{"ServerIp"} = $server_ip;
				$log_data{"InventoryId"} = $inventory_id;
				$log_data{"File"} = $out_file;
				$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		
                $self->update_discovery_intervals($inventory_id);
                unlink($discovery_data_dir."/".$out_file);
                next;
            }
			my $discovery_hash = unserialize($discovery_output);
			$self->{'conn'} = &tmanager::get_db_connection();
			#$logging_obj->log("DEBUG","discover : Discovery Output : ".Dumper($discovery_hash));			
			
			if((ref($discovery_hash) eq "HASH" && exists($discovery_hash->{'error'})) || !$discovery_hash)
			{
				$self->update_discovery_failure($inventory_id, $discovery_hash);
				unlink($discovery_data_dir."/".$out_file);
				next;
			}
			if((lc($discovery_type) eq "vsphere") or (lc($discovery_type) eq "vcenter"))
			{
				$self->process_discovery_info($server_ip,$inventory_id,$discovery_hash);
			}
			elsif((lc($discovery_type) eq "vcloud"))
			{
				if(!$discovery_hash || (ref($discovery_hash) ne "HASH"))
				{
					$logging_obj->log("INFO","No information discovered for vCloud : $server_ip");
					$self->update_discovery_intervals($inventory_id);
					next;
				}
				$self->process_vcloud_discovery_info($server_ip,$inventory_id,$discovery_hash);
			}
			elsif((lc($discovery_type) eq "hyperv"))
			{
				$self->process_hyperv_discovery($server_ip,$inventory_id,$discovery_hash);
			}
			$self->update_discovery_intervals($inventory_id);
		};
		if($@)
		{
			$logging_obj->log("EXCEPTION","Failed to process discovery for Server IP : $server_ip, Inventory ID : $inventory_id,File : $out_file, File content : \n$discovery_output\nFailure reason : $@");
			
			$log_data{"LOGMESSAGE"} = "Failed to process discovery for Server IP - $server_ip, Inventory ID - $inventory_id,File - $out_file \nFailure reason - $@";
			$logging_obj->log("EXCEPTION",\%log_data,$MDS_AND_TELEMETRY);
			
			my $discovery_error_hash = {'error_code' => 'ECA0003', 'error' => 'Internal Server Error', 'error_log' => $@};
			$self->update_discovery_failure($inventory_id, $discovery_error_hash);
		}
		unlink($discovery_data_dir."/".$out_file);
	}
}

sub process_discovery_info
{
	my($self,$server_ip,$inventory_id,$discovery_hash) = @_;
	my(%log_data,%infravm_hash);
	#my $infravm_hash;
	
	my $infravm_query = "SELECT
						infrastructureVMId,
						infrastructureHostId,
						hostIp,
						ipList,
						hostUuid,
						instanceUuid,
						hostName,
						displayName,
						OS,
						isVMPoweredOn,
						isTemplate,
						macIpAddressList,
						isVmDeleted,
						validationErrors,
						diskCount,
						macAddressList
					FROM 
						infrastructureVMs";
	my $infravm_result = $self->{'conn'}->sql_exec($infravm_query);

	foreach my $row (@{$infravm_result})
	{
		$infravm_hash{$row->{'infrastructureVMId'}}{"infrastructureHostId"} = $row->{'infrastructureHostId'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"hostIp"} = $row->{'hostIp'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"ipList"} = $row->{'ipList'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"hostUuid"} = $row->{'hostUuid'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"instanceUuid"} = $row->{'instanceUuid'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"hostName"} = $row->{'hostName'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"displayName"} = $row->{'displayName'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"OS"} = $row->{'OS'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"isVMPoweredOn"} = $row->{'isVMPoweredOn'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"macIpAddressList"} = $row->{'macIpAddressList'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"isVmDeleted"} = $row->{'isVmDeleted'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"isTemplate"} = $row->{'isTemplate'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"validationErrors"} = $row->{'validationErrors'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"diskCount"} = $row->{'diskCount'};
		$infravm_hash{$row->{'infrastructureVMId'}}{"macAddressList"} = $row->{'macAddressList'};
	}
	
	
	
	my $host_discovery_data = $self->parse_host_level_data($discovery_hash);
	my $host_network_info = $host_discovery_data->{"host_network_info"};
	my $host_datastore_info = $host_discovery_data->{"host_datastore_info"};
	my $host_config_info = $host_discovery_data->{"host_config_info"};
	my $host_scsi_disk_info = $host_discovery_data->{"host_scsi_disk_info"};
	my $host_resource_pool_info = $host_discovery_data->{"host_resource_pool_info"};
	my $host_vm_list = $host_discovery_data->{"host_vm_list"};
	my $additional_details = $self->{'conn'}->sql_escape_string(serialize($host_discovery_data->{"additional_details"}));
	
	my $update_additional_details = "UPDATE
							infrastructureInventory
						 SET
							additionalDetails = '$additional_details'
						WHERE 
							inventoryId = '$inventory_id'";
	$log_data{"Message"} = "update_additional_details";
	$log_data{"Sql"} = $update_additional_details;
	$logging_obj->log("INFO",\%log_data,$TELEMETRY);
	$self->{'conn'}->sql_query($update_additional_details);
	
	my @vpshere_hosts = (keys %$host_vm_list);
	my @vms_reported;
	my @esx_hosts_reported;
	my $host_sth;
	
	foreach my $host (@vpshere_hosts)
	{	
		my $network_info = $self->{'conn'}->sql_escape_string(serialize($host_network_info->{$host}));	
		my $datastore_info = $self->{'conn'}->sql_escape_string(serialize($host_datastore_info->{$host}));
		my $config_info = $self->{'conn'}->sql_escape_string(serialize($host_config_info->{$host}));
		my $scsi_disk_info = $self->{'conn'}->sql_escape_string(serialize($host_scsi_disk_info->{$host}));	
		my $resource_pool_info = $self->{'conn'}->sql_escape_string(serialize($host_resource_pool_info->{$host}));
		my $vm_list = $host_vm_list->{$host};
		
		# Yet to Add the CS ID and other conditions required.
		my $inf_host_id = $self->{'conn'}->sql_get_value("infrastructureHosts","infrastructureHostId","inventoryId = '$inventory_id' AND (hostIp = '$host' OR hostName = '$host')");
		my %telemetry_metadata = ('RecordType' => 'VcenterDiscovery', 'ServerIp' => $server_ip, 'InventoryId' => $inventory_id, 'infrastructureHostId' => $inf_host_id, 'InfraHostName' => $host);
		$logging_obj->set_telemetry_metadata(%telemetry_metadata);
		
		my $host_insertion_sql = ($inf_host_id) ?  "UPDATE" : "INSERT INTO";
		$host_insertion_sql .=	"	infrastructureHosts
								 SET
									hostIp = '$host',
									hostName = '$host',
									inventoryId = '$inventory_id',
									datastoreDetails = '$datastore_info',
									resourcepoolDetails = '$resource_pool_info',
									networkDetails = '$network_info',
									configDetails = '$config_info'"; 
		$host_insertion_sql .= ($inf_host_id) ?  " WHERE inventoryId = '$inventory_id' AND infrastructureHostId = '$inf_host_id'" : "";
		$host_sth = $self->{'conn'}->sql_query($host_insertion_sql);
		
		if(!$inf_host_id)
		{
			$inf_host_id = $self->{'conn'}->sql_get_value("infrastructureHosts","infrastructureHostId","inventoryId = '$inventory_id' AND (hostIp = '$host' OR hostName = '$host')");
			
			$log_data{"Sql"} = $host_insertion_sql;
			$log_data{"infrastructureHostId"} = $inf_host_id;
			$log_data{"hostIp"} = $host;
			$log_data{"inventoryId"} = $inventory_id;
			$log_data{"HostName"} = $host;
			$logging_obj->log("INFO",\%log_data,$TELEMETRY)
		}
		else
		{
			# Update block
		}
			
		my $vms_reported_by_host = $self->process_vm_info($inventory_id, $inf_host_id, $vm_list,'vcenter',\%infravm_hash);
		push(@esx_hosts_reported, $inf_host_id);
		push(@vms_reported, @{$vms_reported_by_host});
	}
    
	
    # # Delete the unreported VMs
	# if(@vms_reported)
    # {
        my $vm_ids_str = join("','",@vms_reported);
		my $protected_servers = $self->{'protected_servers'};
		my $replica_vms = $self->{'replica_vms'};
        my $protected_servers_str = join("','", @{$protected_servers});
        my $replica_vms_str = join("','", @{$replica_vms});
        my $esx_servers_str = join("','", @esx_hosts_reported);
        
		my $not_reported_vms_sql_cond = " AND invh.inventoryId = '".$inventory_id."'";
        if($vm_ids_str)
        {            
            $not_reported_vms_sql_cond .= " AND vm.infrastructureVMId NOT IN ('".$vm_ids_str."')";
        }

        if($protected_servers_str)
        {
            $not_reported_vms_sql_cond .= " AND vm.hostId NOT IN ('".$protected_servers_str."')";
        }
		
		if($replica_vms_str)
		{
			$not_reported_vms_sql_cond .= " AND vm.infrastructureVMId NOT IN ('".$replica_vms_str."')";
		}
        
        # Delete unreported VMs        
        my $not_reported_vms_sql = "SELECT 
                                    vm.infrastructureVMId
                                FROM 
                                    infrastructureVMs vm,
                                    infrastructureHosts invh
                                WHERE
                                    vm.infrastructureHostId = invh.infrastructureHostId"
                                    .$not_reported_vms_sql_cond;        
        
		my $vms_to_delete = $self->{'conn'}->sql_get_hash($not_reported_vms_sql, "infrastructureVMId", "infrastructureVMId");
        
		$log_data{"Message"} = "process_discovery_info, Fetch infrastructureVMId which is not reported by discovery";
		$log_data{"Sql"} = $not_reported_vms_sql;
		$logging_obj->log("INFO",\%log_data,$TELEMETRY);
		
		my @vm_ids_to_delete = (keys %$vms_to_delete);
        my $vm_ids_to_delete_str = join("','", @vm_ids_to_delete);
        if($vm_ids_to_delete_str)
        {
            my $delete_unreported_vms_sql = "DELETE FROM infrastructureVMs WHERE infrastructureVMId IN ('".$vm_ids_to_delete_str."')";
            $self->{'conn'}->sql_query($delete_unreported_vms_sql);
			
			$log_data{"Message"} = "process_discovery_info, DELETE INFRASTRUCUTRE VMS not reported by discovery";
			$log_data{"Sql"} = $delete_unreported_vms_sql;
			$logging_obj->log("INFO",\%log_data,$TELEMETRY);
        }
        
        # Delete unreported ESX Servers        
        my $delete_unreported_esx_sql = "DELETE FROM infrastructureHosts WHERE inventoryId = '".$inventory_id."'";
        $delete_unreported_esx_sql .= " AND infrastructureHostId NOT IN ('".$esx_servers_str."')";
        
		if($protected_servers_str) 
		{
			my $reported_esx_sql = "SELECT 
                                            DISTINCT
                                            vm.infrastructureHostId
                                        FROM 
                                            infrastructureVMs vm,
                                            infrastructureHosts invh
                                        WHERE
                                            vm.infrastructureHostId = invh.infrastructureHostId AND
                                            invh.inventoryId = '".$inventory_id."' AND
                                            vm.hostId IN ('".$protected_servers_str."')";
            $log_data{"Message"} = "process_discovery_info : REPORTED ESX";
			$log_data{"Sql"} = $reported_esx_sql;
			$logging_obj->log("INFO",\%log_data,$TELEMETRY);
            my $esx_not_to_delete = $self->{'conn'}->sql_get_hash($reported_esx_sql, "infrastructureHostId", "infrastructureHostId");
            my @esx_ids_not_to_delete = (keys %$esx_not_to_delete);
            my $esx_not_to_delete_str = join("','", @esx_ids_not_to_delete);
            # For not deleting the non-reporting and VM protected ESX
            if($esx_not_to_delete_str)
            {
                $delete_unreported_esx_sql .= " AND infrastructureHostId NOT IN ('".$esx_not_to_delete_str."')";
            }
        }        
        $log_data{"Message"} = "process_discovery_info : DELETE INFRASTRUCUTRE HOSTS";
		$log_data{"Sql"} = $delete_unreported_esx_sql;
		$logging_obj->log("INFO",\%log_data,$TELEMETRY);
        $self->{'conn'}->sql_query($delete_unreported_esx_sql);
		
		my $not_reported_vms_sql_cond = " AND invh.inventoryId = '".$inventory_id."'";
        if($vm_ids_str)
        {            
            $not_reported_vms_sql_cond .= " AND vm.infrastructureVMId NOT IN ('".$vm_ids_str."')";
        }
        if($protected_servers_str)
        {
            $not_reported_vms_sql_cond .= " AND vm.hostId IN ('".$protected_servers_str."')";
        }
        # Update unreported VMs, but those are protected, mark isVmDeleted as 1 in DB.        
        my $not_reported_vms_sql = "SELECT 
                                    hostUuid,
									hostId 
                                FROM 
                                    infrastructureVMs vm,
                                    infrastructureHosts invh
                                WHERE
                                    vm.infrastructureHostId = invh.infrastructureHostId"
                                    .$not_reported_vms_sql_cond;        
        $log_data{"Message"} = "process_discovery_info : REPORTED VMS";
		$log_data{"Sql"} = $not_reported_vms_sql;
		$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
        my $vms_to_update = $self->{'conn'}->sql_get_hash($not_reported_vms_sql, "hostUuid", "hostId");
		
		my ($vm_hostid,$vm_uuid);
		my %vms_without_heartbeats = $self->get_vm_heartbeat($vms_to_update);
		if(%vms_without_heartbeats)
		{
			foreach my $ref (keys %vms_without_heartbeats)
			{
				$vm_hostid = $vms_without_heartbeats{$ref};
				$vm_uuid = $ref;
				my $update_unreported_for_protected_vms_sql = "UPDATE infrastructureVMs set isVmDeleted = '1' WHERE hostUuid = '".$vm_uuid."' AND hostId = '".$vm_hostid."'";
				$log_data{"Message"} = "process_discovery_info : UPDATE INFRASTRUCUTRE VMS SQL TO MARK VM AS NOT DISCOVERED FOR PROTECTED VMS";
				$log_data{"Sql"} = $update_unreported_for_protected_vms_sql;
				$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
				$self->{'conn'}->sql_query($update_unreported_for_protected_vms_sql);
			}
		}
}

 
    # Function Name: get_vm_heartbeat
    #
    # Description: This function is used to returns all the protected VMs
    #   for which heartbeat is older than 15 min
    # 
    # Parameters:
    #     Param [IN]:HASH array with Key as biosId and value as hostid
    #     Param [OUT]:HASH array with Key as biosId and value as hostid
    #
    # Return Value:
    #     Ret Value: Hash Array
	
sub get_vm_heartbeat
{
	my ($self,$not_reported_vm_uuid) = @_;

	my $HEARTBEAT_TIME = Common::Constants::HEARTBEAT_TIME;
	
	my(%log_data,%vm_uuid_without_heartbeats);	
	my($vx_agent_time,$app_agent_time,$current_system_time,$vm_uuid,$vm_hostid);
	my ($not_reported_vm_hostid,$not_reported_biosid,$vm_name,$vm_ipaddress);
	my $agent_hb_time;
	
	foreach my $ref (keys %$not_reported_vm_uuid)
	{
		$vm_hostid = $$not_reported_vm_uuid{$ref};
		$vm_uuid = $ref;
		
		$log_data{"NotReportedVms"} = "get_vm_heartbeat::vm_hostid:$vm_hostid,vm_uuid:$vm_uuid";
		$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		
		my $inv_sql = "SELECT 
						id, 
						name,
						ipaddress,
						biosId, 
						lastHostUpdateTime, 
						unix_timestamp(lastHostUpdateTimeApp) as unix_app_agenttime,
						unix_timestamp(now()) as current_system_time
					FROM 
						hosts  
					WHERE 
						biosId ='".$vm_uuid."' and id='".$vm_hostid."'";
		my $host_details =  $self->{'conn'}->sql_exec($inv_sql);
		
		foreach my $data (@{$host_details})
		{
			$vx_agent_time = $data->{'lastHostUpdateTime'}; 
			$app_agent_time = $data->{'unix_app_agenttime'};
			$current_system_time = $data->{'current_system_time'};
			$not_reported_vm_hostid = $data->{'id'};
			$not_reported_biosid = $data->{'biosId'};
			$vm_name = $data->{'name'};
			$vm_ipaddress = $data->{'ipaddress'};
			
			if($vx_agent_time >= $app_agent_time)
			{
				$agent_hb_time = $vx_agent_time;
			}
			else
			{
				$agent_hb_time = $app_agent_time;
			}
			
			if($current_system_time < $agent_hb_time)
			{
				$log_data{"Message"} = "get_vm_heartbeat::Current System time is lesser than agent time,vm_name:$vm_name,vm_ipaddress:$vm_ipaddress,vm_uuid:$vm_uuid";
				$log_data{"Times"} = "get_vm_heartbeat::Current System time is lesser than agent time,current_system_time:$current_system_time,vx_agent_time:$vx_agent_time,app_agent_time:$app_agent_time";
				$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
				next;
			}
			
			if(($current_system_time - $agent_hb_time) > $HEARTBEAT_TIME)
			{
				# vm is down
				$vm_uuid_without_heartbeats{$not_reported_biosid} = $not_reported_vm_hostid;
			}
			else
			{
				$log_data{"Message"} = "get_vm_heartbeat::Agent heartbeat is healthy hence ignoring to mark VM as NOT DISCOVERED,vm_name:$vm_name,vm_ipaddress:$vm_ipaddress,vm_uuid:$vm_uuid";
				$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
			}
		}
	}
	
	return %vm_uuid_without_heartbeats;
}

sub parse_host_level_data
{
	my ($self,$discovery_hash) = @_;
	
	my $vm_list = $discovery_hash->{"vmList"};
	my $network_info = &normalize($discovery_hash->{"networkInfo"},1);
	my $datastore_info = $discovery_hash->{"datastoreInfo"};
	my $config_info = $discovery_hash->{"configInfo"};
	my $scsi_disk_info = $discovery_hash->{"scsiDiskInfo"};
	my $resource_pool_info = $discovery_hash->{"resourcePoolInfo"};
	my $vc_additional_details = $discovery_hash->{"vcAdditionalDetails"};
	
	my $host_network_info = &parse_hash_vcenter($network_info);
	my $host_datastore_info = &parse_hash_vcenter($datastore_info);
	my $host_config_info = &parse_hash_vcenter($config_info);
	my $host_scsi_disk_info = &parse_hash_vcenter($scsi_disk_info);
	my $host_resource_pool_info = &parse_hash_vcenter($resource_pool_info,"inHost");
	my $host_vm_list = &parse_hash_vcenter($vm_list);
	
	my $return_data = {"host_network_info" => $host_network_info, "host_datastore_info" => $host_datastore_info, "host_config_info" => $host_config_info, "host_scsi_disk_info" => $host_scsi_disk_info, "host_resource_pool_info" => $host_resource_pool_info, "host_vm_list" => $host_vm_list,
	"additional_details" => $vc_additional_details};
	
	return $return_data;
}

sub parse_hash_vcenter
{
	my ($data,$parse_key) = @_;

	my $return_data;	
	if($data)
	{
		my $resp_type = ref($data);	
		
		if($resp_type eq "ARRAY")
		{
			foreach my $hash (@$data)
			{			
				my $host_key = ($parse_key) ? $hash->{$parse_key} : $hash->{'vSphereHostName'};
				push (@{$return_data->{$host_key}},$hash);
			}
		}
		else
		{		
			my $host_key = ($parse_key) ? $data->{$parse_key} : $data->{'vSphereHostName'};
			push (@{$return_data->{$host_key}},$data);
		}
	}
	return $return_data;
}

sub normalize
{
	my ($structure,$level_number) = @_;
	
	my @struct_array = @{$structure};
	my $ret_structure;
	
	for(my $i=0; $i < $level_number; $i++)
	{
		my $new_struct = $struct_array[$i];
		$ret_structure = $new_struct;
	}
	return $ret_structure;
}

sub process_vm_info()
{
	my($self, $inventory_id, $inf_host_id, $vm_list, $type, $infravm_hash) = @_;	
    my (%log_data,%infravm_hash);
	
	my @vms_reported;
	
	foreach my $vm_info (@$vm_list)
	{
		my ($ip_list,$vm_host_name,$vm_disp_name,$vm_os,$vm_powered_on, $validationErrors);
		if($type eq "vcloud")
		{			
			$ip_list = ($vm_info->{"ip_address"} eq "NOT SET") ? "" : $vm_info->{"ip_address"};
			$vm_host_name = ($vm_info->{"hostname"} eq "NOT SET") ? "" : $vm_info->{"hostname"};			
			$vm_disp_name = $vm_info->{"display_name"};
			$vm_os = $vm_info->{"os"};
			$vm_powered_on = ((lc($vm_info->{"powered_status"})) eq "on") ? "1" : "0";
			$validationErrors = $vm_info->{"validationErrors"};
		}
		else
		{
			$ip_list = ($vm_info->{"ipAddress"} eq "NOT SET") ? "" : $vm_info->{"ipAddress"};
			$vm_host_name = ($vm_info->{"hostName"} eq "NOT SET") ? "" : $vm_info->{"hostName"};
			$vm_disp_name = $vm_info->{"displayName"};
			$vm_os = $vm_info->{"os"};
			$vm_powered_on = (((lc($vm_info->{"powerState"})) eq "poweredoff") || ((lc($vm_info->{"powerState"})) eq "off")) ? "0" : "1";
			$validationErrors = $vm_info->{"validationErrors"};
		}
        
        my $is_template = ($vm_info->{"template"} eq "false") ? "0" : "1";

		# Tuned IP Address
        $ip_list =~ s/\s+//g;
		# Loopback address removal
        $ip_list =~ s/127\.0\.0\.1/Loopback/g;
		# For HyperV discovery, it gives a semi-colon on multiple ipAddresses
        $ip_list =~ s/;/,/g;
		# To remove empty elements in the array
        $ip_list =~ s/,+/,/g;
		
        my @ips = split(/,/,$ip_list);
		my $vm_ip = "";
		foreach my $tmp_vm_ip (@ips)
		{
		   if($self->validateIPV4Address($tmp_vm_ip))
		   {
				$vm_ip = $tmp_vm_ip;
				last;
		   }
		}
		  
		$vm_ip = (!$vm_ip) ? $ips[0] : $vm_ip;
        
        # To make the comparison simple adding leading and trailing comma to ipList
        $ip_list = ",".$ip_list.",";
		# To remove empty elements in the array
        $ip_list =~ s/,+/,/g;
        
        # To only consider hostname avoiding the domain name
		$vm_host_name =~ s/(\w+)\.(.*)/$1/;
		
		my $vm_uuid = $vm_info->{"uuid"};
		my $vm_instance_uuid = $vm_info->{"instanceUuid"};
		my $vm_validation_errors = $vm_info->{"validationErrors"};
		my $vm_disk_details = $vm_info->{"diskInfo"};
		my $vm_disk_count = scalar(@$vm_disk_details);
        
        # Prepare the MAC Address list
        my $vm_network_details = $vm_info->{"networkInfo"};
        my $mac_ip_address_list = "";
		my $mac_address_list = "";
        if(ref($vm_network_details) eq "ARRAY")
        {            
            foreach my $nic_info (@$vm_network_details)
            {
                my @ip_addresses = split(/,/, $nic_info->{"macAssociatedIp"});
				$mac_address_list .= $nic_info->{"macAddress"}.",";
                foreach my $mac_ip (@ip_addresses)
                {
                    $mac_ip_address_list .= $nic_info->{"macAddress"}."_".$mac_ip.",";
                }
            }
            $mac_ip_address_list = ",".$mac_ip_address_list;
			$mac_address_list =~ s/,$//;
        }
        
		my $vm_info_str = $self->{'conn'}->sql_escape_string(serialize($vm_info));
		
	
		# Look for the instance uuid duplication
		my ($vm_exists, $inf_vm_id, $vm_inv_id, $vm_instance_exists_info) = 0;
		if($vm_instance_uuid)
		{
			my $get_vm_by_instance_sql = "SELECT 
								vm.infrastructureVMId,
								vhost.inventoryId
							FROM 
								infrastructureVMs vm,
								infrastructureHosts vhost
							WHERE
								vm.infrastructureHostId = vhost.infrastructureHostId AND
								vm.instanceUuid = '".$vm_instance_uuid."'";
			$vm_instance_exists_info = $self->{'conn'}->sql_get_hash($get_vm_by_instance_sql, "infrastructureVMId", "inventoryId");
		}
        
        # If instanceUuid is not associated, then look for the VM UUID
        if(!$vm_instance_exists_info)
        {        
            my $get_vm_by_uuid_sql = "SELECT
                                            vm.infrastructureVMId,
                                            vinv.inventoryId
                                        FROM 
                                            infrastructureVMs vm,
                                            infrastructureHosts vhost,
                                            infrastructureInventory vinv
                                        WHERE
                                            vinv.inventoryId = vhost.inventoryId AND
                                            vinv.hostType NOT IN ('Physical') AND
                                            vm.infrastructureHostId = vhost.infrastructureHostId AND
											vm.hostUuid = '".$vm_uuid."' AND
											vm.displayName = '".$vm_disp_name."'";
            my $vm_exists_info = $self->{'conn'}->sql_get_hash($get_vm_by_uuid_sql, "infrastructureVMId", "inventoryId");
            $vm_instance_exists_info = $vm_exists_info;
	    }
        
        # Handling the addition of VM with the same IP as of a Physical infrastructure
        #  Making the Physical VM owned by the vCenter/ESX.
        if(!$vm_instance_exists_info)
        {
            my $ips_str = join("','", @ips);
            my $get_physical_inventory_sql = "SELECT
                                            vm.infrastructureVMId,
                                            vinv.inventoryId,
											vm.hostIp,
											vm.isPersonaChanged
                                        FROM 
                                            infrastructureVMs vm,
                                            infrastructureHosts vhost,
                                            infrastructureInventory vinv
                                        WHERE
                                            vinv.inventoryId = vhost.inventoryId AND
                                            vinv.hostType = 'Physical' AND
                                            vm.infrastructureHostId = vhost.infrastructureHostId AND
                                            (vm.hostUuid = '$vm_uuid' OR
											 macAddressList REGEXP REPLACE ('$mac_address_list',',','|'))";
			my $physical_vm_data = $self->{'conn'}->sql_exec($get_physical_inventory_sql);	
			
			# Because we will skip if it doesn't belong to this inventory, we are modifying
            #  the inventory id to get the physical VM owned by this vCenter/ESX.
			if (defined $physical_vm_data)
			{
				foreach my $ref (@{$physical_vm_data})
				{
					$logging_obj->log("INFO", "Modifying the inventoryId ".$ref->{"infrastructureVMId"}." to ".$inventory_id." as the IP is matching one of the IP in the list ".$ips_str);
					$vm_instance_exists_info->{$ref->{"infrastructureVMId"}} = $inventory_id;
					
					# In case if physical VM is converted to VMware VM and vmware tools are not installed then let ipaddress be as physical ip address.
					if($ref->{"isPersonaChanged"} eq "1")
					{
						$vm_ip = $ref->{"hostIp"};
					}
				}
			}
        }
        
		if($vm_instance_exists_info)
		{
			$vm_exists = 1;
			($inf_vm_id, $vm_inv_id ) = each (%$vm_instance_exists_info);
			next if($vm_inv_id != $inventory_id);
		}
			
		my $vm_insertion_sql = ($vm_exists) ?  "UPDATE" : "INSERT INTO";
		$vm_insertion_sql .=	"	infrastructureVMs ";
		$vm_insertion_sql .=	"	 SET ";
		$vm_insertion_sql .= ($vm_exists) ?  " " : "infrastructureVMId = UUID(),";
		$vm_insertion_sql .=	"	infrastructureHostId = '$inf_host_id',
									hostIp = '$vm_ip',
                                    ipList = '$ip_list',
									hostUuid = '$vm_uuid',
									instanceUuid = '$vm_instance_uuid',
									hostName = '$vm_host_name',
									hostDetails = '$vm_info_str',
									displayName = '$vm_disp_name',
									OS = '$vm_os',
									isVMPoweredOn = '$vm_powered_on',
                                    isTemplate = '$is_template',
                                    macIpAddressList = '$mac_ip_address_list',
									isVmDeleted = '0',
									validationErrors = '$validationErrors',
									diskCount = '$vm_disk_count',
									macAddressList = '$mac_address_list'"; 
		$vm_insertion_sql .= ($vm_exists) ?  " WHERE infrastructureVMId = '".$inf_vm_id."'" : "";
		$logging_obj->log("DEBUG","process_vm_info : INFRASTRUCUTRE VMS SQL : $vm_insertion_sql");
		my $vm_insert_sth = $self->{'conn'}->sql_query($vm_insertion_sql);
		
		if(($vm_exists) && defined(%$infravm_hash) && (scalar(keys %$infravm_hash) > 0) && exists($infravm_hash->{$inf_vm_id}))
		{
			if(($infravm_hash->{$inf_vm_id}->{"infrastructureHostId"} ne $inf_host_id) || 
				($infravm_hash->{$inf_vm_id}->{"hostIp"} ne $vm_ip) ||
				($infravm_hash->{$inf_vm_id}->{"ipList"} ne $ip_list) ||
				($infravm_hash->{$inf_vm_id}->{"hostUuid"} ne $vm_uuid) ||
				($infravm_hash->{$inf_vm_id}->{"instanceUuid"} ne $vm_instance_uuid) ||
				($infravm_hash->{$inf_vm_id}->{"hostName"} ne $vm_host_name) ||
				($infravm_hash->{$inf_vm_id}->{"displayName"} ne $vm_disp_name) ||
				($infravm_hash->{$inf_vm_id}->{"macIpAddressList"} ne $mac_ip_address_list) ||
				($infravm_hash->{$inf_vm_id}->{"isVmDeleted"} != 0) ||
				($infravm_hash->{$inf_vm_id}->{"macAddressList"} ne $mac_address_list))
			{
				$log_data{"Sql"} = $vm_insertion_sql;
				$log_data{"infrastructureVMId"} = $inf_vm_id;
				$log_data{"infrastructureHostId"} = $infravm_hash->{$inf_vm_id}->{"infrastructureHostId"};
				$log_data{"hostIp"} = $infravm_hash->{$inf_vm_id}->{"hostIp"};
				$log_data{"ipList"} = $infravm_hash->{$inf_vm_id}->{"ipList"};
				$log_data{"hostUuid"} = $infravm_hash->{$inf_vm_id}->{"hostUuid"};
				$log_data{"instanceUuid"} = $infravm_hash->{$inf_vm_id}->{"instanceUuid"};
				$log_data{"hostName"} = $infravm_hash->{$inf_vm_id}->{"hostName"};
				$log_data{"displayName"} = $infravm_hash->{$inf_vm_id}->{"displayName"};
				$log_data{"OS"} = $infravm_hash->{$inf_vm_id}->{"OS"};
				$log_data{"isVMPoweredOn"} = $infravm_hash->{$inf_vm_id}->{"isVMPoweredOn"};
				$log_data{"isTemplate"} = $infravm_hash->{$inf_vm_id}->{"isTemplate"};
				$log_data{"macIpAddressList"} = $infravm_hash->{$inf_vm_id}->{"macIpAddressList"};
				$log_data{"validationErrors"} = $infravm_hash->{$inf_vm_id}->{"validationErrors"};
				$log_data{"diskCount"} = $infravm_hash->{$inf_vm_id}->{"diskCount"};
				$log_data{"macAddressList"} = $infravm_hash->{$inf_vm_id}->{"macAddressList"};
				$logging_obj->log("INFO",\%log_data,$TELEMETRY);
			}
		}
		
		if(!$inf_vm_id)
        {
			if($vm_instance_uuid)
			{
				$inf_vm_id = $self->{'conn'}->sql_get_value("infrastructureVMs", "infrastructureVMId", "infrastructureHostId = '$inf_host_id' AND instanceUuid = '$vm_instance_uuid'");
				
				$log_data{"Sql"} = "INSERT";
				$log_data{"infrastructureVMId"} = $inf_vm_id;
				$log_data{"infrastructureHostId"} = $inf_host_id;
				$log_data{"hostIp"} = $vm_ip;
				$log_data{"ipList"} = $ip_list;
				$log_data{"hostUuid"} = $vm_uuid;
				$log_data{"displayName"} = $vm_disp_name;
				$log_data{"instanceUuid"} = $vm_instance_uuid;
				$log_data{"hostName"} = $vm_host_name;
				$log_data{"OS"} = $vm_os;
				$log_data{"isVMPoweredOn"} = $vm_powered_on;
				$log_data{"isTemplate"} = $is_template;
				$log_data{"macIpAddressList"} = $mac_ip_address_list;
				$log_data{"validationErrors"} = $validationErrors;
				$log_data{"diskCount"} = $vm_disk_count;
				$log_data{"macAddressList"} = $mac_address_list;
				$logging_obj->log("INFO",\%log_data,$TELEMETRY);
			}
			else
			{
				$inf_vm_id = $self->{'conn'}->sql_get_value("infrastructureVMs", "infrastructureVMId", "infrastructureHostId = '$inf_host_id' AND hostUuid = '$vm_uuid' AND displayName = '$vm_disp_name'");
				
				$log_data{"Sql"} = "INSERT";
				$log_data{"infrastructureVMId"} = $inf_vm_id;
				$log_data{"infrastructureHostId"} = $inf_host_id;
				$log_data{"hostIp"} = $vm_ip;
				$log_data{"ipList"} = $ip_list;
				$log_data{"hostUuid"} = $vm_uuid;
				$log_data{"displayName"} = $vm_disp_name;
				$log_data{"instanceUuid"} = $vm_instance_uuid;
				$log_data{"hostName"} = $vm_host_name;
				$log_data{"OS"} = $vm_os;
				$log_data{"isVMPoweredOn"} = $vm_powered_on;
				$log_data{"isTemplate"} = $is_template;
				$log_data{"macIpAddressList"} = $mac_ip_address_list;
				$log_data{"validationErrors"} = $validationErrors;
				$log_data{"diskCount"} = $vm_disk_count;
				$log_data{"macAddressList"} = $mac_address_list;
				$logging_obj->log("INFO",\%log_data,$TELEMETRY);
			}
        }
		
		push(@vms_reported, $inf_vm_id);
	}
	
	return \@vms_reported;
}

sub process_vcloud_discovery_info
{
	my ($self,$server_ip,$inventory_id,$discovery_hash) = @_;
	
	foreach my $organization ( keys %$discovery_hash)
	{
		eval
		{
			my $vm_list;
			if (ref($discovery_hash->{$organization}) ne "HASH")
			{
				next;
			}
			foreach my $vdc (keys %{$discovery_hash->{$organization}})
			{
				if(ref($discovery_hash->{$organization}->{$vdc}) ne "HASH")
				{
					next;
				}
				foreach my $vapp (keys %{$discovery_hash->{$organization}->{$vdc}})
				{													
					if(ref($discovery_hash->{$organization}->{$vdc}->{$vapp}) ne "HASH")
					{
						next;
					}							
					foreach my $vm_key (keys %{$discovery_hash->{$organization}->{$vdc}->{$vapp}})
					{
						if(ref($discovery_hash->{$organization}->{$vdc}->{$vapp}) ne "HASH")
						{
							next;
						}
						push (@$vm_list, $discovery_hash->{$organization}->{$vdc}->{$vapp}->{$vm_key});
					}
				}
			}
					
			# Yet to Add the CS ID and other conditions required.
			my $inf_host_id = $self->{'conn'}->sql_get_value("infrastructureHosts","infrastructureHostId","inventoryId = '$inventory_id' AND (hostIp = '$server_ip' OR hostName = '$server_ip') AND organization = '$organization'");
			my $host_insertion_sql = ($inf_host_id) ?  "UPDATE" : "INSERT INTO";
			$host_insertion_sql .=	"	infrastructureHosts
									 SET
										hostIp = '$server_ip',
										hostName = '$server_ip',
										organization = '$organization',
										inventoryId = '$inventory_id'"; 
			$host_insertion_sql .= ($inf_host_id) ?  " WHERE inventoryId = '$inventory_id' AND infrastructureHostId = '$inf_host_id'" : "";
			$self->{'conn'}->sql_query($host_insertion_sql);
			$logging_obj->log("DEBUG","process_vcloud_discovery_info : INFRASTRUCUTRE HOSTS SQL : $host_insertion_sql");
			
			if(!$inf_host_id)
			{
				$inf_host_id = $self->{'conn'}->sql_get_value("infrastructureHosts","infrastructureHostId","inventoryId = '$inventory_id' AND (hostIp = '$server_ip' OR hostName = '$server_ip') AND organization = '$organization'");
			}
			
			if($vm_list)
			{
				$self->process_vm_info($inf_host_id,$vm_list,'vcloud');
			}
		};
		if($@)
		{
			$logging_obj->log("EXCEPTION","process_vcloud_discovery_info : Failed to process for ORG : $organization ".$@);
		}
	}
}

sub process_hyperv_discovery()
{
	my ($self,$server_ip,$inventory_id,$discovery_hash) = @_;
	
	# Yet to Add the CS ID and other conditions required.
	my $inf_host_id = $self->{'conn'}->sql_get_value("infrastructureHosts","infrastructureHostId","inventoryId = '$inventory_id' AND (hostIp = '$server_ip' OR hostName = '$server_ip')");
	my $host_insertion_sql = ($inf_host_id) ?  "UPDATE" : "INSERT INTO";
	$host_insertion_sql .=	"	infrastructureHosts
							 SET
								hostIp = '$server_ip',
								hostName = '$server_ip',
								inventoryId = '$inventory_id'"; 
	$host_insertion_sql .= ($inf_host_id) ?  " WHERE inventoryId = '$inventory_id' AND infrastructureHostId = '$inf_host_id'" : "";
	$logging_obj->log("DEBUG","process_discovery_info : INFRASTRUCUTRE HOSTS SQL : $host_insertion_sql");
	$self->{'conn'}->sql_query($host_insertion_sql);
	
	if(!$inf_host_id)
	{
		$inf_host_id = $self->{'conn'}->sql_get_value("infrastructureHosts","infrastructureHostId","inventoryId = '$inventory_id' AND (hostIp = '$server_ip' OR hostName = '$server_ip')");
	}
			
	$self->process_vm_info($inf_host_id,$discovery_hash,'hyperv');
}

sub update_discovery_intervals
{
	my ($self, $inventory_id) = @_;
	my %log_data;	
	
	my $update_discovery_time = "UPDATE
							infrastructureInventory
						 SET
							lastDiscoverySuccessTime = now(),
							loginVerifyTime = now(),
							loginVerifyStatus = '1',
							connectionStatus = '1',
							lastDiscoveryErrorMessage = NULL,
							lastDiscoveryErrorLog = NULL,
							errCode = '',
							errPlaceHolders = '',
							discoverNow = '0'
						WHERE 
							inventoryId = '$inventory_id'";
	$log_data{"Message"} = "update_discovery_intervals";
	$log_data{"Sql"} = $update_discovery_time;
	$logging_obj->log("INFO",\%log_data,$TELEMETRY);
	$self->{'conn'}->sql_query($update_discovery_time);
}

sub update_discovery_failure
{
	my ($self, $inventory_id, $error_hash) = @_;
	my %log_data;
	#$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
	my $request_type = 'Discovery';
	my ($error, $error_code, $error_placeholders);
	if(ref($error_hash) eq 'HASH' && exists($error_hash->{'error_code'}))
	{
		$error = $error_hash->{'error'};
		$error_code = $error_hash->{'error_code'};
        if(exists($error_hash->{'error_placeholders'}))
        {
            $error_placeholders = serialize($error_hash->{'error_placeholders'});
            $error_placeholders = $self->{'conn'}->sql_escape_string($error_placeholders);
        }
	}
	else
	{
		$error = $error_hash->{'error'};
		$error = $self->{'conn'}->sql_escape_string($error);
	}
	   
    # Update a success in No VMs case
	if(!$error)
	{
		$self->update_discovery_intervals($inventory_id);
		return;
	}
	
	my $error_log = $error_hash->{'error_log'};
	$error_log = $self->{'conn'}->sql_escape_string($error_log);
	
	################################################################################################################
	# Check if the discovery is being marked failure first time.
	# To avoid multiple flooded entries in MDS for discovery failures.
	# 1. If login was successful earlier.
	# 2. If connection was successful earlier.
	# 3. If last discovery error message was empty.
	# 4. If last discovery error log was empty.
	# 5. If the loginVerifyTime is '0000-00-00 00:00:00' - To address a fresh discovery failure as all above will be set.
	###############################################################################################################
	my $insert_to_mds = $self->{'conn'}->sql_get_value("infrastructureInventory", "inventoryId", "inventoryId = '".$inventory_id."' AND ((loginVerifyStatus = '1' AND connectionStatus = '1' AND lastDiscoveryErrorMessage IS NULL) OR loginVerifyTime = '0000-00-00 00:00:00')");
	
	my $update_discovery_failure = "UPDATE
							infrastructureInventory
						 SET
							discoverNow = '0',
							loginVerifyTime = now(),
							loginVerifyStatus = '0',
							connectionStatus = '0',
							lastDiscoveryErrorMessage = '".$error."',
							lastDiscoveryErrorLog = '".$error_log."',
							errCode = '".$error_code."',
							errPlaceHolders = '".$error_placeholders."'
						WHERE 
							inventoryId = '".$inventory_id."'";
	
	$log_data{"Message"} = "update_discovery_failure";
	$log_data{"Sql"} = $update_discovery_failure;
	$logging_obj->log("INFO",\%log_data,$TELEMETRY);
	
	$self->{'conn'}->sql_query($update_discovery_failure);
	if($insert_to_mds)
	{
		$self->insert_into_mds($inventory_id,$error_log,$request_type);
	}
}

sub validateIPV4Address()
{
	my ($self, $ipaddr) = @_;
	if( $ipaddr =~ m/^(\d\d?\d?)\.(\d\d?\d?)\.(\d\d?\d?)\.(\d\d?\d?)$/ )
	{
		if($1 <= 255 && $2 <= 255 && $3 <= 255 && $4 <= 255)
		{
			return 1;
		}
	}
	return 0;
}

sub get_vcli_version
{
	my ($self) = @_;
	
	require VMware::VIRuntime;
	my $vcli_version	= $VMware::VIRuntime::VERSION;
	my $perl_version	= $];
	my $version		= substr( $vcli_version, 0, rindex( $vcli_version, "." ) );
	if( ( $vcli_version ge "5.5.0" ) && ( $vcli_version lt "6.0.0" ) && ( $perl_version gt "5.008008" ) )
	{
		$version .= "U2";
	}
	
	return $version;
}

sub insert_into_mds
{
	my ($self, $inventory_id,$error_log,$request_type) = @_;
	my ($activity_id,$client_request_id);
	my $error_string = $self->get_discovery_error_data($inventory_id, $request_type);
	
	# If no log return from here
	return if(!$error_string);
	$error_log .= $error_string;
	
	if($inventory_id)
    {
		my $result = $self->get_client_id($inventory_id, @discovery_jobs);
		
		if (defined $result)
		{
			foreach my $ref (@{$result})
			{
				$activity_id = $ref->{'activityId'};
				$client_request_id = $ref->{'clientRequestId'};
			}	
		}
	}
	my $mds_data = {'activityId' => $activity_id, 'clientRequestId' => $client_request_id, 'eventName' => 'Process Server', 'errorCode' => '1000', 'errorCodeName' => 'Discovery Failed', 'errorLog' => $error_log};
	
	my $mdsObj = new MDSErrors();
	$mdsObj->update_data_to_mds($mds_data);
}

sub get_api_request_data
{
	my ($self, $reference_id, $request_type) = @_;
	my ($request_xml,$request_time);
	my $request_data = join("','", @discovery_jobs);
	my $request_data = "'".$request_data."'";
	#Fetching API Request details.
	my $api_request_sql = "SELECT 
								requestXml, 
								requestTime
							FROM 
								apiRequest 
							WHERE 
								referenceId LIKE ('%,$reference_id,%') AND 
								requestType IN ($request_data) ORDER BY apiRequestId DESC LIMIT 1";
	my $result = $self->{'conn'}->sql_exec($api_request_sql);
	my $request = '';
	
	if (defined $result)
	{
		foreach my $ref (@{$result})
		{
			$request_xml = unserialize($ref->{'requestXml'});
			$request_time = $ref->{'requestTime'};
			$request .= "$request_type request was received at $request_time with the input xml as ".$request_xml;
		}	
	}
	return $request;
}

sub get_discovery_error_data
{
	my ($self, $inventory_id, $request_type) = @_;
	my $inventory_data;
	#Fetching API Request details.
	my $request_xml = $self->get_api_request_data($inventory_id, $request_type);
	
	# Return if request XML is empty
	return if($request_xml eq '');
		
	#Fetching infrastructure inventory table details
	my $inv_sql = "SELECT 
						inventoryId, 
						hostIp, 
						lastDiscoveryErrorLog, 
						lastDiscoveryErrorMessage, 
						connectionStatus, 
						loginVerifyStatus, 
						lastDiscoverySuccessTime, 
						now() as currentTime 
					FROM 
						infrastructureInventory 
					WHERE 
						inventoryId = $inventory_id";
	my $inv_details =  $self->{'conn'}->sql_exec($inv_sql);
		
	foreach my $ref (@{$inv_details})
	{
		$inventory_data .= "Inventory Id - ".$ref->{'inventoryId'}." HostIp - ".$ref->{'hostIp'}." DiscoveryLog - ".$ref->{'lastDiscoveryErrorLog'}." lastDiscoveryErrorMessage - ".$ref->{'lastDiscoveryErrorMessage'}." connectionStatus - ".$ref->{'connectionStatus'}." loginVerifyStatus - ".$ref->{'loginVerifyStatus'}." lastLoginVerifyTime - ".$ref->{'lastLoginVerifyTime'}." lastDiscoverySuccessTime - ".$ref->{'lastDiscoverySuccessTime'}." currentTime - ".$ref->{'currentTime'};
	}
	
	my $error_string = $request_xml."\n".$inventory_data;
	return $error_string;
}

sub get_client_id
{
	my ($self,$reference_id, @request_type) = @_;
	my $request_data = join("','", @request_type);
	my $request_data = "'".$request_data."'";
	my $sql = "SELECT activityId,clientRequestId FROM apiRequest  WHERE referenceId Like ('%,$reference_id,%') AND requestType IN($request_data) ORDER BY apiRequestId DESC LIMIT 1"; 
	my $result = $self->{'conn'}->sql_exec($sql);
	return $result;
}

sub info_xml_serialization
{
	my ($self, $infoXmlFile, $disc_file_name, $osType) = @_;
	my $returnCode	= 0;
	
	if( ! open(XML,"<$infoXmlFile"))
	{
		$logging_obj->log("EXCEPTION","discover : Unable to open  $infoXmlFile ");
		return 1;
	}
	
	my $parser = XML::LibXML->new;
	my $parsed_data = $parser->parse_file($infoXmlFile);

	my $root 		= $parsed_data->getDocumentElement;
	
	my %vcAdditionalDetails = ();
	$vcAdditionalDetails{isItvCenter} 	= $root->getAttribute("vCenter");
	$vcAdditionalDetails{version}		= $root->getAttribute("version");
	$vcAdditionalDetails{instanceUuid}	= $root->getAttribute("instanceUuid");
	$vcAdditionalDetails{vmCount}		= $root->getAttribute("vmCount");
	
	my @vmList				= ();
	my @datacenterInfo		= ();
	my @listDataStoreInfo	= ();
	my @networkInfo			= (); 
	my @listConfigInfo		= ();
	my @scsiDiskInfo		= ();
	my @resourcePoolInfo	= ();
	
	my %datastoreTypeMap = ();
	my @datastores	= $root->getElementsByTagName('datastore');
	foreach my $datastore ( @datastores )
	{
		my %mapDatastoreInfo 				= ();
		$mapDatastoreInfo{vSphereHostName} 	= $datastore->getAttribute('vSpherehostname');
		$mapDatastoreInfo{symbolicName} 	= $datastore->getAttribute('datastore_name');
		$mapDatastoreInfo{blockSize}		= $datastore->getAttribute('datastore_blocksize_mb');
		$mapDatastoreInfo{fileSystem}		= $datastore->getAttribute('filesystem_version');
		$mapDatastoreInfo{type}				= $datastore->getAttribute('type');	
		$mapDatastoreInfo{refId}			= $datastore->getAttribute('refid');		
		$mapDatastoreInfo{uuid}				= $datastore->getAttribute('uuid');
		$mapDatastoreInfo{capacity}			= $datastore->getAttribute('total_size');
		$mapDatastoreInfo{freeSpace}		= $datastore->getAttribute('free_space');
		push @listDataStoreInfo , \%mapDatastoreInfo;
		
		$datastoreTypeMap{$mapDatastoreInfo{vSphereHostName}}{$mapDatastoreInfo{symbolicName}} = $mapDatastoreInfo{type};
	}
	
	my @hosts	= $root->getElementsByTagName('host');
	foreach my $host ( @hosts )
	{
		my %vmInfo	= ();
		my @validationErrors	= ();
		$vmInfo{displayName}	= $host->getAttribute('display_name');
		$vmInfo{hostName}		= $host->getAttribute('hostname');
		$vmInfo{ipAddress}		= $host->getAttribute('ip_address');
		$vmInfo{guestId}		= $host->getAttribute('alt_guest_name');
		$vmInfo{uuid}			= $host->getAttribute('source_uuid');
		$vmInfo{toolsStatus}	= $host->getAttribute('vmwaretools');
		$vmInfo{numVirtualDisks}= $host->getAttribute('number_of_disks');
		$vmInfo{operatingSystem}= $host->getAttribute('operatingsystem');
		$vmInfo{vmPathName}		= $host->getAttribute('vmx_path');
		$vmInfo{floppyDevices}	= $host->getAttribute('floppy_device_count');
		$vmInfo{rdm}			= $host->getAttribute('rdm');
		$vmInfo{powerState}		= $host->getAttribute('powered_status');
		$vmInfo{datastore}		= $host->getAttribute('datastore');
		$vmInfo{instanceUuid}	= $host->getAttribute('source_vc_id');
		$vmInfo{efi}			= $host->getAttribute('efi');
		$vmInfo{vSphereHostName}= $host->getAttribute('vSpherehostname');
		$vmInfo{memorySizeMB}	= $host->getAttribute('memsize');
		$vmInfo{cluster}		= $host->getAttribute('cluster');
		$vmInfo{ideCount}		= $host->getAttribute('ide_count');
		$vmInfo{template}		= $host->getAttribute('template');
		$vmInfo{resourcePool}	= $host->getAttribute('resourcepool');
		$vmInfo{vmxVersion}		= $host->getAttribute('vmx_version');
		$vmInfo{hostVersion}	= $host->getAttribute('hostversion');
		$vmInfo{vmConsoleUrl}	= $host->getAttribute('vm_console_url');
		$vmInfo{numCpu}			= $host->getAttribute('cpucount');
		$vmInfo{diskEnableUuid}	= $host->getAttribute('diskenableuuid');
		$vmInfo{dataCenter}		= $host->getAttribute('datacenter');
		$vmInfo{resourcePoolGrp}= $host->getAttribute('resourcepoolgrpname');
		$vmInfo{folderPath}		= $host->getAttribute('folder_path');
		$vmInfo{datacenter_refid} = $host->getAttribute('datacenter_refid');
		$vmInfo{snapshot} 		= lc($host->getAttribute('snapshot'));
		
		$vmInfo{os} = "OTHER";
		if( $vmInfo{operatingSystem} =~ /windows/i )
		{
			$vmInfo{os} = "WINDOWS";
		}
		elsif( ($vmInfo{operatingSystem} =~ /linux/i ) || ($vmInfo{operatingSystem} =~ /CentOS/i))
		{
			$vmInfo{os} = "LINUX";
		}
		elsif($vmInfo{operatingSystem} =~ /solaris/i)
		{
			$vmInfo{os} = "SOLARIS";
		}
		
		# VM state is powered off.
		if ($vmInfo{powerState}	ne "ON")
		{
			push @validationErrors, "ECD001";
		}
		elsif ($vmInfo{ipAddress} eq "NOT SET")
		{
			# VM VMware tools are not running.
			if ($vmInfo{toolsStatus} ne "OK")
			{
				push @validationErrors, "ECD002";
			}
			# VM IP address was not set.
			else
			{
				push @validationErrors, "ECD003";
			}
		}
		
		# VM disk.enable uuid false
		push @validationErrors, "ECD008" if (lc($vmInfo{diskEnableUuid}) eq "no");
		
		push @validationErrors, "ECD011" if (lc($vmInfo{snapshot}) eq "true");

		my @disksInfo = ();
		my @disks	= $host->getElementsByTagName('disk');
		my $isControllerTypeSame = 1;
		my $scsiControllerName = "";
		foreach my $disk ( @disks )
		{
			my %diskInfo = ();
			$diskInfo{diskName} 		= $disk->getAttribute('disk_name');
			$diskInfo{diskUuid} 		= $disk->getAttribute('source_disk_uuid');
			$diskInfo{controllerType} 	= $disk->getAttribute('controller_type');
			$diskInfo{diskSize} 		= $disk->getAttribute('size');
			$diskInfo{diskMode} 		= $disk->getAttribute('disk_mode');
			$diskInfo{capacityInBytes} 	= $disk->getAttribute('capacity_in_bytes');
			$diskInfo{diskType}			= $disk->getAttribute('disk_type');
			$diskInfo{scsiVmx} 			= $disk->getAttribute('scsi_mapping_vmx');
			$diskInfo{scsiHost} 		= $disk->getAttribute('scsi_mapping_host');
			$diskInfo{clusterDisk} 		= $disk->getAttribute('cluster_disk');
			$diskInfo{ideOrScsi} 		= $disk->getAttribute('ide_or_scsi');
			$diskInfo{controllerMode} 	= $disk->getAttribute('controller_mode');
			$diskInfo{independentPersistent} = $disk->getAttribute('independent_persistent');
			$diskInfo{diskObjectId} 	= $disk->getAttribute('disk_object_id');
			
			$scsiControllerName = $diskInfo{controllerType} if ($scsiControllerName eq "");

			$isControllerTypeSame = 0 if ($diskInfo{controllerType} ne $scsiControllerName);
			
			push @disksInfo, \%diskInfo;
		}
		$vmInfo{diskInfo} = \@disksInfo;
		
		# For MT, SCSI controllerType should be of same type
		push @validationErrors, "ECD006" unless ($isControllerTypeSame);
		
		# For Linux MT, SCSI controllerType should be LSI logical
		if (($vmInfo{os} eq "LINUX") && ($scsiControllerName ne "VirtualLsiLogicController"))
		{
			push @validationErrors, "ECD007";
		}
		
		my @nicsInfo = ();
		my @nics = $host->getElementsByTagName('nic');
		foreach my $nic (@nics)
		{
			my %nicInfo = ();
			$nicInfo{networkLabel} 	= $nic->getAttribute('network_label');
			$nicInfo{network} 		= $nic->getAttribute('network_name');
			$nicInfo{macAddress} 	= $nic->getAttribute('nic_mac');
			$nicInfo{macAssociatedIp} = $nic->getAttribute('ip');
			$nicInfo{adapterType} 	= $nic->getAttribute('adapter_type');
			$nicInfo{isDhcp} 		= $nic->getAttribute('dhcp');
			$nicInfo{addressType} 	= $nic->getAttribute('address_type');
			$nicInfo{dnsIp} 		= $nic->getAttribute('dnsip');
			
			push @nicsInfo, \%nicInfo;
		}
		$vmInfo{networkInfo} = \@nicsInfo;
		
		$vmInfo{validationErrors} = join( ",", @validationErrors);
				
		push @vmList, \%vmInfo;
	}
	
	my @datacenters	= $root->getElementsByTagName('datacenter');
	foreach my $datacenter ( @datacenters )
	{
		my %mapDatacenterInfo 				= ();
		$mapDatacenterInfo{datacenter_name} = $datacenter->getAttribute('datacenter_name');
		$mapDatacenterInfo{refid}			= $datacenter->getAttribute('refid');
		push @datacenterInfo, \%mapDatacenterInfo;
	}
	
	my @networks	= $root->getElementsByTagName('network');
	foreach my $network ( @networks )
	{
		my %mapNetworkInfo					= ();
		$mapNetworkInfo{name}				= $network->getAttribute('name');
		$mapNetworkInfo{vSpherehostname}	= $network->getAttribute('vSpherehostname');
		$mapNetworkInfo{portgroupKey}		= $network->getAttribute('portgroupKey');
		$mapNetworkInfo{switchUuid}			= $network->getAttribute('switchUuid');
		push @networkInfo, \%mapNetworkInfo;
	}
	
	my @configs	= $root->getElementsByTagName('config');
	foreach my $config ( @configs )
	{
		my %mapConfigInfo				= ();
		$mapConfigInfo{vSpherehostname} = $config->getAttribute('vSpherehostname');
		$mapConfigInfo{memsize} 		= $config->getAttribute('memsize');
		$mapConfigInfo{cpucount} 		= $config->getAttribute('cpucount');
		push @listConfigInfo, \%mapConfigInfo;
	}
	
	my @resourcepools	= $root->getElementsByTagName('resourcepool');
	foreach my $resourcepool ( @resourcepools )
	{
		my %mapResourcepoolInfo					= ();
		$mapResourcepoolInfo{vSpherehostname}	= $resourcepool->getAttribute('host');
		$mapResourcepoolInfo{name}				= $resourcepool->getAttribute('name');
		$mapResourcepoolInfo{groupId}			= $resourcepool->getAttribute('groupId');
		$mapResourcepoolInfo{owner}				= $resourcepool->getAttribute('owner');
		$mapResourcepoolInfo{owner_type}		= $resourcepool->getAttribute('owner_type');
		$mapResourcepoolInfo{type}				= $resourcepool->getAttribute('type');
		push @resourcePoolInfo, \%mapResourcepoolInfo;
	}
	
	my $return_hash = {vmList => \@vmList ,osType => $osType, vcAdditionalDetails => \%vcAdditionalDetails,
		datastoreInfo => \@listDataStoreInfo , networkInfo => \@networkInfo ,configInfo => \@listConfigInfo ,
		resourcePoolInfo => \@resourcePoolInfo,scsiDiskInfo => "", datacenterInfo => \@datacenterInfo};
	my $hash_str = PHP::Serialization::serialize($return_hash);	
	open(FH,">$disc_file_name");
	print FH $hash_str;
	close(FH);
	return 0;
}
1;
