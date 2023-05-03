#$Header: /src/server/tm/pm/ProcessServer/Register.pm,v 1.47.8.2 2017/12/07 09:15:39 raambala Exp $ 
#================================================================= 
# FILENAME
#   Register.pm 
#
# DESCRIPTION
#    This perl module is responsible for Process server 
#    Registration and monitoring.
#    
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ProcessServer::Register;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use tmanager;
use Config;
use Utilities;
use Sys::Hostname;
use TimeShotManager;
use Common::Constants;
use ProcessServer::Trending::NetworkTrends;
use Common::Log;
use Common::DB::Connection;
use Data::Dumper;
use Messenger;
use PHP::Serialization qw(serialize unserialize);
use RequestHTTP;

my $logging_obj = new Common::Log();
my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
#
# Constructor for the Register class.
#
sub new
{
    my ($class, %args) = @_; 
    my $self = {};

    #
    # Read amethyst conf file and parse all values.
    #
    #$self->{'dbh'} = $args{'dbh'};
    $self->{'conn'} = $args{'conn'};
    $self->{'host_guid'} = $args{'host_guid'};    
    $self->{'cs_ip'} = $args{'cs_ip'};
    $self->{'cx_type'} = $args{'cx_type'};
    $self->{'base_dir'} = $args{'base_dir'};
    $self->{'bpm_ntw_device'} = $AMETHYST_VARS->{'BPM_NTW_DEVICE'};
    $self->{'CX_MODE'} = $AMETHYST_VARS->{'CX_MODE'};
	$self->{'amethyst_vars'} = $AMETHYST_VARS;

    my $host = `hostname`;
    chomp $host;
    $self->{'hostname'} = $host;
	$self->{'cs_enabled'} = 0;
	if($self->{'cx_type'} == Common::Constants::CONFIG_PROCESS_SERVER) {
		$self->{'cs_enabled'} = 1;
	}

     bless ($self, $class);
}

#
# Upload the perf and rpo log files from CS to PS
#

sub upload_log_files
{
	eval
	{
		my($output, $base_dir) = @_;
		my @all_source_details;
		my @all_target_details;
		my %dest_entry;
		
		my $cs_ip = $AMETHYST_VARS->{'CS_IP'};		
        my $ps_cs_ip = $AMETHYST_VARS->{'PS_CS_IP'};		
		my $host_id = $AMETHYST_VARS->{'HOST_GUID'};
		my $cx_type = $AMETHYST_VARS->{'CX_TYPE'};
		my $is_secondary_passive = 0;
	
		my $pair_data = $output->{'pair_data'};
				
		foreach my $pair_id (keys %{$pair_data}) {	
			my $src_host_details;
			$src_host_details->{'sourceHostId'} = $pair_data->{$pair_id}->{'sourceHostId'};
			$src_host_details->{'srcDevName'} = $pair_data->{$pair_id}->{'sourceDeviceName'};
			my $destination_host_id = $pair_data->{$pair_id}->{'destinationHostId'};

			push(@all_source_details,$src_host_details);
			push(@all_target_details,$destination_host_id);			
		}

		@all_target_details = grep { ! $dest_entry{ $_ }++ } @all_target_details;	
		
		if(exists($AMETHYST_VARS->{'CLUSTER_IP'}))	{
			my $appliance_node_data = $output->{'appliance_node_data'};			
			foreach my $node_id (keys %{$appliance_node_data}) {
				if($node_id == $host_id && $appliance_node_data->{$node_id}->{'nodeRole'} == 'PASSIVE') 
				{
					$is_secondary_passive = 1;
				}
			}
		}
		
		if (($cs_ip ne $ps_cs_ip) || ($cx_type eq 2) || ($is_secondary_passive && ($cx_type ne 1)))
        {
			$logging_obj->log("DEBUG","Dump of the src details ".Dumper(@all_source_details));
			my $slash = "\/";
			my %src_path;
			foreach my $source (@all_source_details) {	
			
				if (Utilities::isWindows())	{
					$source->{'srcDevName'} =~ s/\//\\/g;
				} else {
					$source->{'srcDevName'} =~ s/\\/\//g;
				}
				
				if($source->{'srcDevName'} =~ /:/) {
				   $source->{'srcDevName'} =~ s/://g;
				   $source->{'srcDevName'} =~ s/\\/\//g;
				}
				
				my $src_perf_node_ps_path =  $base_dir.$slash.$source->{'sourceHostId'}.$slash.$source->{'srcDevName'}.$slash.'perf_ps.txt';
				if(-e ($src_perf_node_ps_path))	{				
					$src_path{$src_perf_node_ps_path} = $base_dir.$slash.$source->{'sourceHostId'}.$slash.$source->{'srcDevName'}.$slash;
				}
				
				if($AMETHYST_VARS->{'CUSTOM_BUILD_TYPE'} != "3")
				{
					my $src_rpo_path = $base_dir.$slash.$source->{'sourceHostId'}.$slash.$source->{'srcDevName'}.$slash.'rpo.txt';
					if(-e ($src_rpo_path)) {				
						$src_path{$src_rpo_path} = $base_dir.$slash.$source->{'sourceHostId'}.$slash.$source->{'srcDevName'}.$slash;
					}
					
					my $src_bw_path = $base_dir.$slash.$source->{'sourceHostId'}.$slash.$host_id.'-bandwidth.txt';
					if(-e ($src_bw_path)) {				
						$src_path{$src_bw_path} = $base_dir.$slash.$source->{'sourceHostId'}.$slash;
					}
				}
			}
			
			if($AMETHYST_VARS->{'CUSTOM_BUILD_TYPE'} != "3")
			{
				foreach my $dest_host_id (@all_target_details) {
					my $tgt_bw_path = $base_dir.$slash.$dest_host_id.$slash.$host_id.'-bandwidth.txt';				
					$logging_obj->log("DEBUG","upload_log_files::tgt_bw_path::$tgt_bw_path");		
					if(-e ($tgt_bw_path))
					{				
						$src_path{$tgt_bw_path} = $base_dir.$slash.$dest_host_id.$slash;
					}
				}
			}
			
			my $batch_count_limit = 25;
			my $batch_cnt = 0;
			my %batch;
			
			my @src_files = (keys %src_path);
			foreach my $src_file (keys %src_path)
			{
				$batch_cnt++;
				
				next if(! -e ($src_file));
				$batch{$src_file} = $src_path{$src_file};
				
                # When the counter reached the last index or
                # when the counter modulus is '0' upload the files and reset the batch
				if(($batch_cnt%$batch_count_limit == 0) || ($batch_cnt == $#src_files+1))
				{
					&tmanager::upload_file(\%batch);
					# Reset batch
					undef %batch;					
				}
			}
			
			#&tmanager::upload_file(\%src_path);
			
			foreach my $src_file (keys %src_path)
			{
				$logging_obj->log("DEBUG","Unlinking $src_file");				
				unlink($src_file);
			}		
		}
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","Error syncing files to CS: $@");
	}	
}

#
# Upload the perf log files from PS to CS
#
sub upload_log_files_perf
{	
	eval
	{
		my($output,$base_dir) = @_;
		my(@perf_logs,@all_source_details);
		my $cs_ip = $AMETHYST_VARS->{'CS_IP'};
        my $ps_cs_ip = $AMETHYST_VARS->{'PS_CS_IP'};
		my $host_id = $AMETHYST_VARS->{'HOST_GUID'};
		my $cx_type = $AMETHYST_VARS->{'CX_TYPE'};
		my $is_secondary_passive = 0;
		my $os_flag = 0;		
		
		my $pair_data = $output->{'pair_data'};
		
		foreach my $pair_id (keys %{$pair_data}) {	
			my $src_host_details;
			$src_host_details->{'sourceHostId'} = $pair_data->{$pair_id}->{'sourceHostId'};
			$src_host_details->{'srcDevName'} = $pair_data->{$pair_id}->{'sourceDeviceName'};
			push(@all_source_details,$src_host_details);
		}
		
		if(exists($AMETHYST_VARS->{'CLUSTER_IP'}))	
		{
			my $appliance_node_data = $output->{'appliance_node_data'};			
			if($appliance_node_data->{$host_id}->{'nodeRole'} == 'PASSIVE') 
			{
				$is_secondary_passive = 1;
			}			
		}

        if (($cs_ip eq $ps_cs_ip) || ($cx_type eq 2) || ($is_secondary_passive && ($cx_type ne 1)))
        {
			$logging_obj->log("DEBUG","Dump of the src details ".Dumper(@all_source_details));
			my $slash = "\/";
			my %src_path;
			
			foreach my $source (@all_source_details) 
			{				
				if (Utilities::isWindows()) {
					$source->{'srcDevName'} =~ s/\//\\/g;					
				} else {
					$source->{'srcDevName'} =~ s/\\/\//g;					
				}
				
				if($source->{'srcDevName'} =~ /:/)
				{
				   $source->{'srcDevName'} =~ s/://g;
				   $source->{'srcDevName'} =~ s/\\/\//g;
				}
				
				my $src_perf_log_path = $base_dir.$slash.$source->{'sourceHostId'}.$slash.$source->{'srcDevName'}.$slash.'perf.log';
				if(-e ($src_perf_log_path)) {
					$src_path{$src_perf_log_path} = $base_dir.$slash.$source->{'sourceHostId'}.$slash.$source->{'srcDevName'}.$slash;	
				}	
			}			
			$logging_obj->log("DEBUG","Syncing perf logs".Dumper(\%src_path));
			&tmanager::upload_file(\%src_path);
		}
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","Error syncing files to CS: $@");
	}	
}

#
# Update the HBA POrts of the Process Server in CS
#
sub update_hba_ports
{
  my $self = shift;
  my $cfg = 1;
  my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
  my $host_id = $AMETHYST_VARS->{'HOST_GUID'};
  my ($cx_nodewwn_output,@nodes_wwn,$cx_portwwn_output,@ports_wwn,$model_name_ouput,@model_names,$model_desc_output,@model_descs,$serial_num_output);
  my (@serial_nums,$target_mode_output,@target_modes,$path_state_output,@path_states,$count_hba_ports,$i);
  
  if(-d "/sys/class/fc_host/") 
  {
	my $osRelease = `grep 6.2 /etc/redhat-release`;
    $cx_nodewwn_output = `cat /sys/class/fc_host/host*/node_name`;
    @nodes_wwn = split(/\n/,$cx_nodewwn_output);

    $cx_portwwn_output = `cat /sys/class/fc_host/host*/port_name`;
    @ports_wwn = split(/\n/,$cx_portwwn_output);

	if($osRelease eq "")
	{
		$target_mode_output =`cat /sys/class/scsi_host/host*/target_mode_enabled`;
		@target_modes = split(/\n/,$target_mode_output); 
		
		$model_name_ouput = `cat /sys/class/fc_host/host*/device/scsi_host\:host*/model_name`;
		@model_names = split(/\n/,$model_name_ouput);

		$model_desc_output = `cat /sys/class/fc_host/host*/device/scsi_host\:host*/model_desc`;
		@model_descs = split(/\n/,$model_desc_output);

		$serial_num_output = `cat /sys/class/fc_host/host*/device/scsi_host\:host*/serial_num`;
		@serial_nums = split(/\n/,$serial_num_output);

		$path_state_output = `cat /sys/class/fc_host/host*/device/scsi_host\:host*/state`;
		@path_states = split(/\n/,$path_state_output);
		
	}
	else
	{
		# from 6.2 release onwards use below path.
		
		$target_mode_output =`cat /sys/class/scsi_host/host*/target_mode_enabled`;
		@target_modes = split(/\n/,$target_mode_output); 
		
		$model_name_ouput = `cat /sys/class/fc_host/host*/device/scsi_host/host*/model_name`;
		@model_names = split(/\n/,$model_name_ouput);

		$model_desc_output = `cat /sys/class/fc_host/host*/device/scsi_host/host*/model_desc`;
		@model_descs = split(/\n/,$model_desc_output);

		$serial_num_output = `cat /sys/class/fc_host/host*/device/scsi_host/host*/serial_num`;
		@serial_nums = split(/\n/,$serial_num_output);

		$path_state_output = `cat /sys/class/fc_host/host*/device/scsi_host/host*/state`;
		@path_states = split(/\n/,$path_state_output);
	}

    $count_hba_ports = $#nodes_wwn + 1;

    for($i=0;$i<$count_hba_ports;$i++) {
      if($path_states[$i] =~ /Link Up/i) {
        $path_states[$i] = 1;
      }
      else {
        $path_states[$i] = 0;
      }


		my $hba_params = $cfg.":".$nodes_wwn[$i].":".$ports_wwn[$i].":".$model_names[$i].":".$model_descs[$i].":".$serial_nums[$i].":".$target_modes[$i].":".$path_states[$i].":".$host_id;
		eval
		{
			my $response;
			my $http_method = "POST";
			my $https_mode = ($AMETHYST_VARS->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;
			my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $AMETHYST_VARS->{'PS_CS_IP'}, 'PORT' => $AMETHYST_VARS->{'PS_CS_PORT'});
			my $param = {'access_method_name' => "update_hba_info", 'access_file' => "/update_hba_info.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
			my %perl_args = ('hba_params' => $hba_params);
			my $content = {'content' => \%perl_args};
			
			my $request_http_obj = new RequestHTTP(%args);
			$response = $request_http_obj->call($http_method, $param, $content);
			
			$logging_obj->log("DEBUG","Response::".Dumper($response));
			if (! $response->{'status'})
			{
				$logging_obj->log("EXCEPTION","Failed to post the details for update_hba_ports");
			}
		};
		
		if($@)
		{
			$logging_obj->log("EXCEPTION","Error : " . $@);
		}	
	}
  }   
}

#
# Register the list of valid network addresses
# obtained from the host
#
sub register_network_devices
{
	my $self = shift;
	my $nic_speed;
	my $ip_list;
	my %bond_details;
	$logging_obj->log("DEBUG","Entered register_network_devices"); 
	if (Utilities::isWindows())
	{
		my $ip = Utilities::getIpAddress();
		my $nic;
		my $get_from_ip = `wmic path Win32_NetworkAdapterConfiguration get IpAddress,description`;
        my @mac_values = split(/(\s{2})+/,$get_from_ip);
        my $mac_count = $#mac_values + 1;
        for(my $i=0;$i<$mac_count;$i++) {
		  if($mac_values[$i] =~ /\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/) {
		    my $ip_check = $&;
            if($ip_check eq $ip) {
 	          $nic = $mac_values[$i-2];
			  $nic =~ s/\n//g;
	          last;
            }
		  }
        }
		$ip_list->{$nic} = $ip;
	}
	else
	{
		my $ntw_obj = new ProcessServer::Trending::NetworkTrends();
		$ntw_obj->get_device_statistics();
		$ip_list = $ntw_obj->get_ip_address();
	}
	
	my @ips = keys %{$ip_list};
    foreach my $nic (@ips)
    {
		my $device_name = $nic;
		my $ip_address = $ip_list->{$nic};
		chomp $ip_address;
		if (Utilities::isWindows())
		{
			$nic_speed  = "";
			
		}
		else
		{
			my $bond_vars;
			my $nic_speed_temp = `ethtool $device_name | grep -i speed`;
			my @nic_values = split(':', $nic_speed_temp);
			$nic_speed  =  $nic_values[1];
			my $bond_filename = Utilities::makePath("/etc/sysconfig/network-scripts/ifcfg-".$device_name);
			if(-e $bond_filename)
			{
				open (FILEH, "<$bond_filename") || die "Unable to locate $bond_filename:$!\n";
				my @file_contents = <FILEH>;
				close (FILEH);
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
						$bond_vars->{$arr[0]} = $arr[1];
					}
				}
				
				my $bond_name = $bond_vars->{"MASTER"};
				if($bond_name ne "")
				{
					#$bond_details{ $bond_name } =   { $nic_name => $nic_speed };
					push @{ $bond_details{$bond_name} }, $nic_speed;
				}
			}
		}
		eval
		{

		if($ip_address ne "")
		{
			my $network_addr = "INSERT INTO 
									networkAddress
									(hostId,
									deviceName,
									ipAddress,
									natipAddress,
									deviceType,
									nicSpeed)
								VALUES
									(\'$self->{'host_guid'}\',
									'$device_name',
									'$ip_address',
									'',
									1,
									'$nic_speed')";

			
			my $sth = $self->{'conn'}->sql_query($network_addr);
			
			my $count = $self->{'conn'}->sql_num_rows($sth);
		

		
			if( $count != 0)
			{
				$logging_obj->log("DEBUG","Registering $self->{'host_guid'}, $ip_address into network address");
			}
			else
			{
				$logging_obj->log("EXCEPTION","Error registering $self->{'host_guid'}, $ip_address in network address");
			}
		}
		else
		{
			my $delete_network_addr = "DELETE FROM
													networkAddress
									   WHERE
													hostId = \'$self->{'host_guid'}\' 
									   AND
													deviceName = '$device_name'";

			$self->{'conn'}->sql_query($delete_network_addr);							            
												

		}
		};
		if($@)
		{
			$logging_obj->log("EXCEPTION","Failed to execute a loop here. ERROR: $@ $!");
		}
	}
	
	while ( my ($key, $list) = each(%bond_details) ) 
	{
		
		my $bond_speed = 0;
		my $count  = 0;
		my $ipAddress = $ip_list->{$key};
		my @nic_details = @$list;
		my $total_number_nics = scalar(@nic_details);
		while($count < $total_number_nics)
		{
			$bond_speed = $bond_speed + $nic_details[$count];
			$count++;
		}
		$bond_speed = int($bond_speed/$count);
		$bond_speed = $bond_speed."Mb/s";
		my $sql = "UPDATE networkAddress SET nicSpeed = '$bond_speed' WHERE deviceName = '$key' AND hostId = '$self->{'host_guid'}'";
		$logging_obj->log("DEBUG","Entered into the loop update".$sql);
		$self->{'conn'}->sql_query($sql);
    }
   return;
}

#
# Validate the supplied IP address from the ifconfig/ipconfig
#
sub validate_ip_address
{
   my $ipaddress = shift;

    if(! $ipaddress)
    {
        return 0;
    }

    my $check_ip;
    my ($err_id, $err_message, $err_summary);
    my ($ip_command, $ip_match_char, $pattern);
    my ($available_ip, $available_ip_list);
    my $host = `hostname`;
    chomp $host;
    my $hostname =  $host;
   
    if (Utilities::isWindows)
    {
        $ip_command = "ipconfig";
        $ip_match_char = "IP Address";
        $pattern = 'IP Address.*?:\s?(.*?)$';
    }
    else
    {
        if($AMETHYST_VARS->{'BPM_NTW_DEVICE'} ne "")
        {
            $ip_command = "ifconfig -a $AMETHYST_VARS->{'BPM_NTW_DEVICE'}";
        }
        else
        {
            $ip_command = "ifconfig -a eth0 ";
        }
        $ip_match_char = "inet addr";
        $pattern = 'inet addr:(.*?)\s+(.*)$';
    }

    $check_ip = `$ip_command | grep $ipaddress`;

    if((! is_ip_loopback($ipaddress)) && ($check_ip ne ""))
    {
        #
        # Valid IP address
        #
        return 1;
    }

    #
    # Get the available IP list
    #
    my @val = `$ip_command 2>&1`;
    
    foreach my $line(@val)
    {
        chomp $line;
        if ($line =~ /$pattern/)
        {
            $available_ip = $1;
            chomp $available_ip;
            
            if(! is_ip_loopback($available_ip))
            {
                $available_ip_list .= $available_ip.",";
            }
        }   
    }
    chop $available_ip_list;

    $logging_obj->log("EXCEPTION","Process server registration failed.". "Invalid IP ".$ipaddress." for ".$AMETHYST_VARS->{'HOST_GUID'}."[".$hostname."]");

    return 0;
}

#
# Verify if the ip is a loopback address IP
#
sub is_ip_loopback
{
    my $ip_address = shift;

    if ($ip_address eq '127.0.0.1')
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

sub update_cs_network_devices
{
	
	my $self = shift;    
    my @nic_name_list;
	my $ip_list;
	my %network_details;
	my %slaves;
	$logging_obj->log("DEBUG","Entered update_cs_network_devices\n");
	if (Utilities::isWindows())
	{
		###
		#  Get the IPaddress Details for Windows
		###
		$ip_list = Utilities::getWinIPaddress();		
	}
	else
	{
		my $ntw = new ProcessServer::Trending::NetworkTrends;
		$ntw->get_device_statistics();
		$ip_list = $ntw->get_ip_address();
	}
    my %nic_ip_hash = %$ip_list;
	       
	foreach my $nic_name (keys %nic_ip_hash)
	{
		push (@nic_name_list,$nic_name);
		my $nic_ip_address = $nic_ip_hash{$nic_name};
		chomp $nic_ip_address;
		$network_details{$nic_name}{'ip_address'} =  $nic_ip_address;		
		if (Utilities::isWindows())
		{
			$network_details{$nic_name}{'speed'} = "";
			$network_details{$nic_name}{'device_type'} = 1;
			$network_details{$nic_name}{'ip_address'} = $nic_ip_hash{$nic_name}->[0];
			$network_details{$nic_name}{'nic_dns'} = $nic_ip_hash{$nic_name}->[1];
			$network_details{$nic_name}{'nic_gateway'} = $nic_ip_hash{$nic_name}->[2];
			$network_details{$nic_name}{'nic_netmask'} = $nic_ip_hash{$nic_name}->[3];
		}
		else
		{		
			my $network_config;
			
			# Get the speed of the interface
			my $nic_speed_temp = `ethtool $nic_name | grep -i speed`;
			my @nic_values = split(':', $nic_speed_temp);
			$network_details{$nic_name}{'speed'}  =  $nic_values[1];
			
			# Get network details for the interface
			my $int_conf_file = Utilities::makePath("/etc/sysconfig/network-scripts/ifcfg-".$nic_name);
			if(-e $int_conf_file)
			{
				open (FILEH, "<$int_conf_file") || die "Unable to locate $int_conf_file:$!\n";
				my @file_contents = <FILEH>;
				close (FILEH);
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
						$network_config->{$arr[0]} = $arr[1];
					}
				}
				
				my $bond_name = $network_config->{"MASTER"};
				if($bond_name ne "")
				{
					# Device Type defines the interface type
					# 1 - Normal Interfaces
					# 2 - Bonding Masters
					# 3 - Bonding Slaves
					
					$network_details{$bond_name}{'device_type'} = 2;
					$network_details{$nic_name}{'device_type'} = 3;					
					push @{ $slaves{$bond_name} },$nic_name;	
				}
				$network_details{$nic_name}{'netmask'} = $network_config->{"NETMASK"};
				$network_details{$nic_name}{'gateway'} = $network_config->{"GATEWAY"};
				$network_details{$nic_name}{'broadcast'} = $network_config->{"BROADCAST"};
				
				my $nic_dns;
				for(my $i=1;$i<4;$i++)
				{
					my $dns_check = "DNS".$i;
					if(!$network_config->{$dns_check})
					{
						last;
					}
					if($network_config->{$dns_check})
					{
						$nic_dns .= ($nic_dns ne "") ? ",".$network_config->{$dns_check} : $network_config->{$dns_check};
					}					
				}
				$network_details{$nic_name}{'dns'} = $nic_dns;
			}
		
		}
	}
		
	foreach my $bond (keys %slaves)
	{
		my @tmp_slaves = @{$slaves{$bond}};
		$network_details{$bond}{'slaves'} = join(",", @tmp_slaves);
	}
		
	foreach my $nic_name (keys %network_details)
	{	
		# Default device type
		my $device_type = $network_details{$nic_name}{'device_type'};
		$device_type = (!$device_type) ? '1' : $device_type;
		
		# Default Nic Speed
		my $nic_speed = $network_details{$nic_name}{'speed'};	
			
		my $query = "SELECT 
				ipAddress 
			FROM 
				networkAddress 
			WHERE
				hostId = \'$self->{'host_guid'}\' AND
				deviceName = \'$nic_name\'";
		
        my $sth = $self->{'conn'}->sql_query($query);		
		my $count = $self->{'conn'}->sql_num_rows($sth);
		
		if (Utilities::isWindows()) 
		{
			###
			#  In Case of Windows Same NIC can have multiple IP's
			#  1) Insert the newly created IP's for a NIC
			#  2) Delete the IP's which does not belong to a NIC
			#  3) Update the IP's if no Change.
			###
			
			my (%new_ip_list,%deleted_ip_list,@registered_ip_list,@nic_ip_list);
			
			while(my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($sth)) {
				if($ref->{'ipAddress'}) {
					push @registered_ip_list,$ref->{'ipAddress'};
				}
			}
			
			my $nic_dns = $network_details{$nic_name}{'nic_dns'};
			my $nic_gateway = $network_details{$nic_name}{'nic_gateway'};
			
			# Get the NEW IP List for this NIC
			@nic_ip_list = @{$network_details{$nic_name}{'ip_address'}};
			@new_ip_list {@nic_ip_list} =  @nic_ip_list;
			delete @new_ip_list {@registered_ip_list};
			my @new_list = (keys %new_ip_list);
			if((scalar @new_list) > 0) {
				foreach my $ip (@new_list) {
					my $nic_netmask = $network_details{$nic_name}{'nic_netmask'}->{$ip};
					my $network_addr = "INSERT INTO 
											networkAddress
											(hostId,
											deviceName,
											ipAddress,
											natipAddress,
											deviceType,
											nicSpeed,
											gateway,
											dns,
											netmask)
										VALUES
											(\'$self->{'host_guid'}\',
											'$nic_name',
											'$ip',
											'',
											$device_type,
											'$nic_speed',
											'$nic_gateway',
											'$nic_dns',
											'$nic_netmask')";

					my $sth = $self->{'conn'}->sql_query($network_addr);
				}
			}
			
			if ($count > 0)
			{
				# Get the IP address that are no more valid for this NIC and which needs to be deleted from DB
				@deleted_ip_list{@registered_ip_list} = @registered_ip_list;
				delete @deleted_ip_list{@nic_ip_list};
				my @delete_list = (keys %deleted_ip_list);
				if((scalar @delete_list) > 0) {
					my $delete_ip_list = join("','", @delete_list);
					my $sql = 	"DELETE FROM
									networkAddress
								 WHERE 
									hostId = \'$self->{'host_guid'}\' AND
									deviceName = '$nic_name' AND
									ipAddress IN ('$delete_ip_list')";

					my $sth = $self->{'conn'}->sql_query($sql);
				}
				
				#Update the existing IP Info
				my %in_array2 = map { $_ => 1 } @registered_ip_list;
				my @common_ip_list = grep { $in_array2{$_} } @nic_ip_list;
				if((scalar @common_ip_list) > 0) {
					foreach my $ip (@common_ip_list) {
						my $nic_netmask = $network_details{$nic_name}{'nic_netmask'}->{$ip};
						my $network_addr = "UPDATE
										networkAddress
								   SET
									   ipAddress = \'$ip\',
									   netmask = \'$nic_netmask\',
									   gateway = \'$nic_gateway\',
									   dns = \'$nic_dns\'
								   WHERE
									  hostId = \'$self->{'host_guid'}\' AND
									  deviceName = \'$nic_name\' AND
									  ipAddress = \'$ip\'";
						my $sth = $self->{'conn'}->sql_query($network_addr);
					}
				}
			}
		}
		else
		{
			my %network_detail = %{$network_details{$nic_name}};	
					
			# In case of bonds the speed will be average speed of all hosts
			if($device_type == 2)
			{
				my @slaves = $network_details{$nic_name}{'slaves'};
				my $total_number_nics = scalar(@slaves);
				foreach my $slave (@slaves)
				{
					$nic_speed = $nic_speed + $network_details{$slave}{'speed'};				
				}
				$nic_speed = int($nic_speed/$total_number_nics);
				$nic_speed = $nic_speed."Mb/s";
			}
			
			if ($count == 0)
			{
				my $network_addr = "INSERT INTO 
										networkAddress
										(hostId,
										deviceName,
										ipAddress,
										netmask,
										gateway,
										bcast,
										dns,
										natipAddress,
										deviceType,
										nicSpeed,
										slaves)
									VALUES
										(\'$self->{'host_guid'}\',
										'$nic_name',
										\'$network_detail{'ip_address'}\',
										\'$network_detail{'netmask'}\',
										\'$network_detail{'gateway'}\',
										\'$network_detail{'broadcast'}\',
										\'$network_detail{'dns'}\',
										'',
										'$device_type',
										\'$nic_speed\',
										\'$network_detail{'slaves'}\')";						
				my $sth = $self->{'conn'}->sql_query($network_addr);					
				my $count = $self->{'conn'}->sql_num_rows($sth);					
				if ($count == 0)
				{
					$logging_obj->log("EXCEPTION","Error registering ".$self->{'host_guid'}.", 
					".$network_detail{'ip_address'}." in network address");
				}
			}
			else 
			{				
				my $network_addr = "UPDATE
										networkAddress
								   SET
									   ipAddress = \'$network_detail{'ip_address'}\',
									   netmask = \'$network_detail{'netmask'}\',
									   gateway = \'$network_detail{'gateway'}\',
									   bcast = \'$network_detail{'broadcast'}\',
									   dns = \'$network_detail{'dns'}\',
									   nicSpeed = \'$nic_speed\',
									   deviceType = \'$device_type'\,
									   slaves = \'$network_detail{'slaves'}\'
								   WHERE
									  hostId = \'$self->{'host_guid'}\' AND
									  deviceName = \'$nic_name\'";			   
				my $sth = $self->{'conn'}->sql_query($network_addr);			   
				my $count = $self->{'conn'}->sql_num_rows($sth);
				if ($count == 0)
				{
					$logging_obj->log("EXCEPTION","Error registering ".$self->{'host_guid'}.",".$network_detail{'ip_address'}." for ".$nic_name." interface in network address");
				}
			}
		}		
    }
	my $nic_count = scalar(@nic_name_list);
	if ($nic_count > 0) 
	{
	    my $check_sql;
		my $nic_names = join("','", @nic_name_list);
		
		#
		# Delete from the networkAddressMap
		#
		my $sql_nw_map = "DELETE 
						  FROM 
							   networkAddressMap 
						  WHERE
							   processServerId = \'$self->{'host_guid'}\'
						  AND
							   nicDeviceName IN ('$nic_names')";
		$self->{'conn'}->sql_query($sql_nw_map);
		
		#
		# Select policyId from the BPMPolicies
		#
		$check_sql = "UPDATE
							BPMPolicies
					  SET 
							isBPMEnabled = 2
					  WHERE 
							sourceHostId=\'$self->{'host_guid'}\'
					  AND
							networkDeviceName NOT IN ('$nic_names')";		
		$self->{'conn'}->sql_query($check_sql);
				
		my $delete_sql = 	"DELETE 
							FROM
								networkAddress
							WHERE 
								hostId = \'$self->{'host_guid'}\' AND
								deviceName NOT IN ('$nic_names')";		
		my $delete_sth = $self->{'conn'}->sql_query($delete_sql);
		
		my $delete_count = $self->{'conn'}->sql_num_rows($delete_sth);

		if ($delete_count != 0)
		{
			$logging_obj->log("DEBUG","update_network_devices :: Deleted some of the nics from $nic_names for ".$self->{'host_guid'}." from network address table");	
		}
	}	
}

sub isCsWindows
{
    my $self=shift;
	my $ps_cs_ip = $AMETHYST_VARS->{'PS_CS_IP'};
	my $os_flag;
	if(exists($AMETHYST_VARS->{'CLUSTER_IP'}))
	{
		my $get_src_sql = "SELECT 
								nodemap.nodeId as host_id
							FROM 
								applianceNodeMap AS nodemap, applianceCluster AS clus 
							WHERE 
								nodemap.applianceId=clus.applianceId 
							AND 
								nodemap.nodeRole='ACTIVE'";

		my $result = $self->{'conn'}->sql_exec($get_src_sql);

		my @value = @$result;

		my $cs_guid = $value[0]{'host_id'};
					
		$os_flag = $self->{'conn'}->sql_get_value('hosts', 'osFlag', "id='$cs_guid'");	
		if($os_flag == 1)
		{
			return 1;
		}		
		
	}
	else
	{
		$os_flag = $self->{'conn'}->sql_get_value('hosts', 'osFlag', "csEnabled=1");	
		if($os_flag == 1)
		{
			return 1;
		}
	}
	
	return 0;
}

sub getHostInfo
{
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
	my $ipaddress = Utilities::getIpAddress();
	
=head
    if(!&validate_ip_address($ipaddress))
    {
        return;
    }	
=cut	

	my $os_flag = 0;
	my $os_type = "";

	my (@osname, @osversion,@build, $osname, $osversion, $buildnumber, $host_data);
	my $host = `hostname`;
    chomp $host;
    my $hostname =  $host;
	
	#
	# Use the Win32 API for GetOSName, GetOSVersion 
	# to register uname info in CS 
	#
	if (Utilities::isWindows())
	{
		use if Utilities::isWindows(), "Win32";

		eval 
		{
			@osname = Win32::GetOSName();
			@osversion = Win32::GetOSVersion();
			#@build = Win32::BuildNumber();

		};
		if ($@)
		{
			$logging_obj->log("EXCEPTION","Unable to determine OS name/version for $hostname . $@");     
		}

		$osname = join(" ", @osname);
		$osversion = " Version ".join ("", @osversion);
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

		#
		# Update OS flag to PS_LINUX is the osType
		# is linux.
		#
		$os_flag = 2;
	}
	chomp $os_type;
	
	my $cs_enabled = 0;
	if($AMETHYST_VARS->{'CX_TYPE'} == Common::Constants::CONFIG_PROCESS_SERVER) {
		$cs_enabled = 1;
	}
	
	#Fix for multi-tenancy
	my $account_id = $AMETHYST_VARS->{'ACCOUNT_ID'};
	my $account_type = $AMETHYST_VARS->{'ACCOUNT_TYPE'};
	if($account_id == '')
	{
		$account_id = Common::Constants::INMAGE_ACCOUNT_ID;
		$account_type = Common::Constants::INMAGE_ACCOUNT_TYPE;
	}
	$logging_obj->log("DEBUG","getHostInfo:: account_id:".$AMETHYST_VARS->{'ACCOUNT_ID'}.", account_type: ".$AMETHYST_VARS->{'ACCOUNT_TYPE'}.", account_id: $account_id, account_type: $account_type");	
	
	my @version = &tmanager::get_inmage_version_array();
    my @ps_patch_version = &tmanager::get_ps_patch_version();
	my ($patchInstallTime, $psPatchVersion);
		 
	for(my $i = 0 ; $i < scalar(@ps_patch_version) ; $i++ ) 
	{
		$psPatchVersion .= $ps_patch_version[$i]{'ps_patch_version'}.",";
		$patchInstallTime .= $ps_patch_version[$i]{'patch_install_time'}.",";		
	}
	chop($psPatchVersion);
	chop($patchInstallTime);
    my $ps_version = $version[ 0 ];
    my $ps_install_time = $version[ 2 ];	
	
	my $transport_params = Utilities::get_transport_conf_parameters();
	my $port = $transport_params->{'port'};
	my $ssl_port = $transport_params->{'ssl_port'};
	$host_data->{'osType'} = $os_type;
	$host_data->{'osFlag'} = $os_flag;
	$host_data->{'id'} = $host_guid;
	$host_data->{'name'} = $hostname;
	$host_data->{'accountId'} = $account_id;
	$host_data->{'csEnabled'} = $cs_enabled;
	$host_data->{'ipaddress'} = $ipaddress;
	$host_data->{'accountType'} = $account_type;
	$host_data->{'hypervisorState'} = 0;
	$host_data->{'hypervisorName'} = "";
	$host_data->{'processServerEnabled'} = 1;
	$host_data->{'psVersion'} = $ps_version;
	$host_data->{'psInstallTime'} = $ps_install_time;
	$host_data->{'psPatchVersion'} = $psPatchVersion;
	$host_data->{'patchInstallTime'} = $patchInstallTime;
	$host_data->{'port'} = $port;
	$host_data->{'sslPort'} = $ssl_port;	
	$host_data = serialize($host_data);
	
	return $host_data;	
}

sub getNetworkInfo
{
	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
	my @nic_name_list;
	my $ip_list;
	my $network_details;
	my %slaves;
	$logging_obj->log("DEBUG","Entered getNetworkInfo\n");
	
	if (Utilities::isWindows())
	{
		###
		#  Get the IPaddress Details for Windows
		###
		$ip_list = Utilities::getWinIPaddress();		
	}
	else
	{
		my $ntw = new ProcessServer::Trending::NetworkTrends;
		$ntw->get_device_statistics();
		$ip_list = $ntw->get_ip_address();
	}
    my %nic_ip_hash = %$ip_list;

	foreach my $nic_name (keys %nic_ip_hash)
	{
		push (@nic_name_list,$nic_name);
		$network_details->{$nic_name}->{'hostId'} = $host_guid;
		$network_details->{$nic_name}->{'deviceName'} = $nic_name;
		$network_details->{$nic_name}->{'deviceType'} = 1;
		if (Utilities::isWindows())
		{
			$network_details->{$nic_name}->{'nicSpeed'} = "";
			$network_details->{$nic_name}->{'ipAddress'} = $nic_ip_hash{$nic_name}->[0];
 	  	  	$network_details->{$nic_name}->{'dns'} = $nic_ip_hash{$nic_name}->[1];
 	  	  	$network_details->{$nic_name}->{'gateway'} = $nic_ip_hash{$nic_name}->[2];
 	  	  	$network_details->{$nic_name}->{'netmask'} = $nic_ip_hash{$nic_name}->[3];			
		}
		else
		{		
			my $network_config;
			my $nic_ip_address = $nic_ip_hash{$nic_name};
			chomp $nic_ip_address;
			$network_details->{$nic_name}->{'ipAddress'}->{0} =  $nic_ip_address;
						
			# Get the speed of the interface
			my $nic_speed_temp = `ethtool $nic_name | grep -i speed`;
			my @nic_values = split(':', $nic_speed_temp);
			$network_details->{$nic_name}->{'nicSpeed'}  =  $nic_values[1];
			
			# Get network details for the interface
			my $int_conf_file = Utilities::makePath("/etc/sysconfig/network-scripts/ifcfg-".$nic_name);
			if(-e $int_conf_file)
			{
				open (FILEH, "<$int_conf_file") || die "Unable to locate $int_conf_file:$!\n";
				my @file_contents = <FILEH>;
				close (FILEH);
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
						$network_config->{$arr[0]} = $arr[1];
					}
				}
				
				my $bond_name = $network_config->{"MASTER"};
				if($bond_name ne "")
				{
					# Device Type defines the interface type
					# 1 - Normal Interfaces
					# 2 - Bonding Masters
					# 3 - Bonding Slaves
					
					$network_details->{$bond_name}->{'deviceType'} = 2;
					$network_details->{$nic_name}->{'deviceType'} = 3;					
					push @{ $slaves{$bond_name} },$nic_name;
				}
				$network_details->{$nic_name}->{'netmask'}->{$nic_ip_address} = $network_config->{"NETMASK"};
				$network_details->{$nic_name}->{'gateway'} = $network_config->{"GATEWAY"};
				$network_details->{$nic_name}->{'bcast'} = $network_config->{"BROADCAST"};
				
				my $nic_dns;
				for(my $i=1;$i<4;$i++)
				{
					my $dns_check = "DNS".$i;
					if(!$network_config->{$dns_check})
					{
						last;
					}
					if($network_config->{$dns_check})
					{
						$nic_dns .= ($nic_dns ne "") ? ",".$network_config->{$dns_check} : $network_config->{$dns_check};
					}					
				}
				$network_details->{$nic_name}->{'dns'} = $nic_dns;
			}		
		}
	}
	
	foreach my $bond (keys %slaves)
	{
		my @tmp_slaves = @{$slaves{$bond}};
		$network_details->{$bond}->{'slaves'} = join(",", @tmp_slaves);
	}
	$network_details = serialize($network_details);
	
	return $network_details;
}

sub registerHost
{
	eval
	{
		my ($host_info, $network_info);
		
		$host_info = &getHostInfo();
		$network_info = &getNetworkInfo();
		my $body_xml = "<Body>
						<FunctionRequest Name='RegisterHost'>
							<ParameterGroup Id='RegisterHost'>
								<Parameter Name='host_info' Value='".$host_info."'/>
								<Parameter Name='network_info' Value='".$network_info."'/>
							</ParameterGroup>
						</FunctionRequest>
					</Body>";
			
		my $response;
		my $http_method = "POST";
		my $https_mode = ($AMETHYST_VARS->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $AMETHYST_VARS->{'PS_CS_IP'}, 'PORT' => $AMETHYST_VARS->{'PS_CS_PORT'});
		my $param = {'access_method_name' => "RegisterHost", 'access_file' => "/ScoutAPI/CXAPI.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'type' => 'text/xml' , 'content' => $body_xml};
		
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);
		
		$logging_obj->log("DEBUG","Response::".Dumper($response));
		if (!$response->{'status'})
		{
			$logging_obj->log("EXCEPTION","Failed to post the details for registerHost");
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : ".$@);
	}		
}


1;
