package RX::Sync;

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
use File::Basename;

my $logging_obj  = new Common::Log();

sub new
{
	my ($class) = @_;
	my $self = {};
		
	$self->{'conn'} = new Common::DB::Connection();	
	
	 	 
	# To add "LAST_AUTO_UPDATE_CHECK_TIME" and "AUTO_UPDATE_CHECK_INTERVAL" 	 
	my $num_rows = $self->{'conn'}->sql_get_value('rxSettings', 'count(*)', "ValueName IN ('LAST_AUTO_UPDATE_CHECK_TIME','AUTO_UPDATE_CHECK_INTERVAL')");	
		 
	if($num_rows == 0) 	 
	{ 	 
		my $sql = "INSERT INTO `rxSettings` (`ValueName`, `ValueData`) VALUES ('AUTO_UPDATE_CHECK_INTERVAL', '3600'), ('LAST_AUTO_UPDATE_CHECK_TIME', '')"; 	 
	    $self->{'conn'}->sql_query($sql); 	 
	}

	my $rx_data = ();
	my $sql = "SELECT ValueName, ValueData FROM rxSettings";
	$rx_data = $self->{'conn'}->sql_get_hash($sql,'ValueName','ValueData');	

	$self->{'https_port'} = $self->{'conn'}->sql_get_value('rxSettings', 'ValueData', "ValueName='HTTPS_PORT'");
	
	my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
	$self->{'BASE_DIR'}      = "/home/svsystems";	
	$self->{'rx_ip'} = $rx_data->{"IP_ADDRESS"};
	$self->{'rx_port'} = $rx_data->{"HTTP_PORT"};
	$self->{'rx_cs_port'} = $rx_data->{"RX_CS_PORT"};
	$self->{'secured_communication'} = $rx_data->{"SECURED_COMMUNICATION"};
	$self->{'rx_cs_secured_communication'} = $rx_data->{"RX_CS_SECURED_COMMUNICATION"};
	$self->{'update_status'} = $rx_data->{"UPDATE_STATUS"};
	$self->{'auto_update_cx'} = $rx_data->{"AUTO_UPDATE_CX"};
	$self->{'auto_update_check_interval'} = $rx_data->{"AUTO_UPDATE_CHECK_INTERVAL"};
	$self->{'last_auto_update_check_time'} = $rx_data->{"LAST_AUTO_UPDATE_CHECK_TIME"};
	$self->{'cs_ip'} = $AMETHYST_VARS->{'CS_IP'};
	$self->{'rx_cs_ip'} = $rx_data->{'RX_CS_IP'};
	$self->{'http'} = ($self->{'secured_communication'}) ? "https" : "http";
	$self->{'cs_port'} = $self->{'rx_cs_port'};	
	$self->{'response_interval'} = $rx_data->{"RESPONSE_INTERVAL"};
	$self->{'last_response_time'} = $rx_data->{"LAST_RESPONSE_TIME"};
	$self->{'response_type'} = $rx_data->{"RESPONSE_TYPE"};				
	$self->{'rx_key'} = $rx_data->{"RX_KEY"};	
	$self->{'cx_compatibility_number'} = $rx_data->{"CX_COMPATIBILITY_NUMBER"};	
	$self->{'host_guid'} = $AMETHYST_VARS->{'HOST_GUID'};
	$self->{'sync_with_rx_now'} = $rx_data->{'SYNC_WITH_RX_NOW'};
	
	my $web_dir = '/home/svsystems/admin/web';
	
	$self->{'file_path'} = $web_dir."/rx_update/";
	$self->{'update_download_script'} = $web_dir."/rx_update_download_request.php";
	$self->{'curl_php_script'} = $web_dir."/curl_dispatch_http.php";
	
	$self->{'out_file'} = $AMETHYST_VARS->{'INSTALLATION_DIR'}.'/'.$rx_data->{"RX_KEY"}.'_out.xml';
	$self->{'update_log_path'} = $AMETHYST_VARS->{'INSTALLATION_DIR'}."/var/";
	$self->{'cx_install_path'} = $AMETHYST_VARS->{'INSTALLATION_DIR'};
	$self->{'cx_type'} = $AMETHYST_VARS->{'CX_TYPE'};
	
	bless($self, $class);	
}

sub push_data_rx
{
	my ($self) = @_;	
	my $current_time = time();
	my $cx_compatibility_number = $self->{'cx_compatibility_number'};
	my $response_type = $self->{'response_type'};
	my $rx_ip = $self->{'rx_ip'};
	my $rx_port = $self->{'rx_port'};
	my $last_response_time = $self->get_unix_timestamp($self->{'last_response_time'});	
	my $response_interval = $self->{'response_interval'};
	my $http = ($self->{'rx_cs_secured_communication'}) ? "https" : "http";
	my $rx_http = $self->{'http'};
	my $cs_ip = $self->{'cs_ip'};
	my $cs_port = $self->{'cs_port'};
	my $BASE_DIR = $self->{'BASE_DIR'};	
	my $out_file = $self->{'out_file'};	
		
	if($rx_ip eq "" || $rx_port eq "")
	{		
		$logging_obj->log("DEBUG","Not registered to RX");
		return;
	}		
	
	if($response_type ne "PUSH")
	{
		$logging_obj->log("DEBUG","Not registered in PUSH mode with RX");
		return;
	}
	
	if (!$last_response_time || $self->{'sync_with_rx_now'} || ($current_time > $last_response_time + $response_interval))
	{		
		my $rpc_server = $http.'://'.$cs_ip.':'.$cs_port.'/xml_rpc/server.php';
		my $xmlrpc = XML::RPC->new($rpc_server);			
		my @rx_secure_key = ($self->{'rx_key'});
		my $result = $xmlrpc->call( 'report.getReportDetails' , \@rx_secure_key);	
		
		my $serialized_str = $result->{'serialized_data'};
		$logging_obj->log("INFO","serialized_str : $serialized_str\n");
		open(TRANSFER_FILE,">$out_file");
		print TRANSFER_FILE $serialized_str;
		close(TRANSFER_FILE);		
		
		my $rx_url = $rx_http."://".$rx_ip.":".$rx_port."/uploadDataFile.php";
		my $curl_php_script = $self->{'curl_php_script'};#Utilities::makePath($BASE_DIR."/admin/web/curl_dispatch_http.php");
		my $post_data = "data_file##@".$out_file; 
		
		my $status = `php "$curl_php_script" "$rx_url" "$post_data"`;
		
		unlink ( $out_file );
		
		$self->{'conn'} = new Common::DB::Connection();
		my $sql = "UPDATE rxSettings SET ValueData = NOW() WHERE ValueName = 'LAST_RESPONSE_TIME'";
		$self->{'conn'}->sql_query($sql);

		my $sql = "UPDATE rxSettings SET ValueData = '0' WHERE ValueName = 'SYNC_WITH_RX_NOW'";
		$self->{'conn'}->sql_query($sql);		
	}		
}

sub auto_update
{
	my ($self) = @_;
	my $cx_type = $self->{'cx_type'};
	
	if (($cx_type == 1) || ($cx_type == 3)) 
	{
		# Only applicable for Linux CS and if auto_update_cx is enabled
		if(!Utilities::isLinux() || !$self->{'auto_update_cx'}) 
		{
			$logging_obj->log("DEBUG","Auto update not enabled");
			return;	
		}
				
		if($self->{'rx_ip'} eq "" || $self->{'rx_port'} eq "")
		{		
			$logging_obj->log("DEBUG","Not registered to RX");
			return;
		}	
		
		my $current_time = time();
		my $last_auto_update_check_time = $self->{'last_auto_update_check_time'};
		my $auto_update_check_interval = $self->{'auto_update_check_interval'};
		
		if (!$last_auto_update_check_time || ($current_time > $last_auto_update_check_time + $auto_update_check_interval))
		{
			my $cx_compatibility_number = $self->get_cx_compatibility();		
			my $rx_compatibility_number = $self->get_rx_compatibility();		
			if($rx_compatibility_number && ($rx_compatibility_number gt $cx_compatibility_number))
			{
				$self->update_cx();
			}
			else
			{
				my $sql = "UPDATE rxSettings SET ValueData = NOW() WHERE ValueName = 'LAST_AUTO_UPDATE_CHECK_TIME'";
				$self->{'conn'}->sql_query($sql);
			}
		}
	}
}

sub update_cx
{
	my ($self) = @_;
	
	my $update_status = $self->get_param("UPDATE_STATUS");
	my $rx_ip = $self->{'rx_ip'};
	my $rx_port = $self->{'rx_port'};
	my $file_path = $self->{'file_path'};
	my $http = $self->{'http'};
	my $update_download_script = $self->{'update_download_script'};
		
	$logging_obj->log("DEBUG","update_cx : update_status : $update_status");		
	if($update_status <= 0)
	{			
		my @update_params = ($self->{'rx_key'});
		my $rpc_server = $http.'://'.$rx_ip.':'.$rx_port.'/xml_rpc/server.php';
		my $xmlrpc = XML::RPC->new($rpc_server);				
		my $result = $xmlrpc->call( 'report.updateCx', \@update_params);			
		if($result && !$result->{'faultCode'})
		{
			$logging_obj->log("DEBUG","update_cx : ".Dumper($result));
			my $download_url = $result->{'DOWNLOAD_URL'};
			my $download_file_name = $result->{'DOWNLOAD_FILE_NAME'};
			my $download_file_checksum = $result->{'DOWNLOAD_FILE_CHECKSUM'};
			
			my $sql = "UPDATE rxSettings SET ValueData = '$download_url' WHERE ValueName = 'UPDATE_DOWNLOAD_URL'";
			$self->{'conn'}->sql_query($sql);
			
			$sql = "UPDATE rxSettings SET ValueData = '$download_file_name' WHERE ValueName = 'DOWNLOAD_FILE_NAME'";
			$self->{'conn'}->sql_query($sql);
			
			$sql = "UPDATE rxSettings SET ValueData = '$download_file_checksum' WHERE ValueName = 'DOWNLOAD_FILE_CHECKSUM'";
			$self->{'conn'}->sql_query($sql);
			$self->update_status(1);
		}
		else
		{
			$self->update_status(-1);
			$logging_obj->log("EXCEPTION","update_cx : XML RPC call updateCx failed".Dumper($result));
			return;
		}
		$update_status = $self->get_param("UPDATE_STATUS");
	}
	if($update_status == 1)
	{
		my $update_download_url = $self->get_param("UPDATE_DOWNLOAD_URL");
		my $download_file_name = $self->get_param("DOWNLOAD_FILE_NAME");
		my $download_file_checksum = $self->get_param("DOWNLOAD_FILE_CHECKSUM");
		my $download_update_url = $http."://".$rx_ip.":".$rx_port."/".$update_download_url;
		
		# Downloading the file					
		my $post_data = "FILE_NAME=".$download_file_name."&FILE_CHECKSUM=".$download_file_checksum;			
		my $data = `php "$update_download_script" "$download_update_url" "$post_data"`;
		$logging_obj->log("INFO","Calling the download script as php ".$update_download_script." ".$download_update_url." ".$post_data);			
		my $full_file_path = $file_path."/".$download_file_name;
		if(-f($full_file_path))
		{
			unlink($full_file_path);
		}
		if(!-d($file_path))
		{
			mkdir($file_path,777);		
		}
		if($data)
		{			
			open(DFH,">$full_file_path");
			print DFH $data;
			close(DFH); 
		}
		$self->update_status(2);
		$update_status = $self->get_param("UPDATE_STATUS");
	}
	if($update_status == 2)
	{
		my $download_file_name = $self->get_param("DOWNLOAD_FILE_NAME");
		my $download_file_checksum = $self->get_param("DOWNLOAD_FILE_CHECKSUM");
		my $full_file_path = $file_path."/".$download_file_name;
		$logging_obj->log("DEBUG","update_cx : update_status : $update_status,full_file_path = $full_file_path");
		my $checksum = $self->md5_file($full_file_path);
		#	Update Status
		#	0  - Nothing to update
		#	1  - Marked to initiate Update process
		#	2  - Download suceeded
		#	-2 - Download failed
		#	3  - verify download (checksum matches)
		#	-3 - checksum match failed
		#	4  - Installation suceeded	
		#	-4 - Installation failed					
				
		# Verifying checksum of the downloaded file		
		$logging_obj->log("DEBUG","Full File Path: $full_file_path");
		$logging_obj->log("INFO","Checksum : $download_file_checksum, Calculated checksum = $checksum");
		if((-e($full_file_path)) && ($download_file_checksum == $checksum))
		{
			$self->update_status(3);
			$self->install_update($download_file_name);
		}			
		else
		{		
			$self->update_status(-3);
		}
		$update_status = $self->get_param("UPDATE_STATUS");			
	}
	if($update_status == 4)
	{
		my $rx_compatibility_number = $self->get_param("RX_COMPATIBILITY_NUMBER");
		my $sql = "UPDATE rxSettings SET ValueData = '$rx_compatibility_number' WHERE ValueName = 'CX_COMPATIBILITY_NUMBER'";
		$self->{'conn'}->sql_query($sql);
		
		my $sql = "UPDATE rxSettings SET ValueData = NOW() WHERE ValueName = 'LAST_AUTO_UPDATE_CHECK_TIME'";
		$self->{'conn'}->sql_query($sql);
		
		$self->update_status(0);
	}
}

sub md5_file
{
	my ($self,$File) = @_;
	use Digest::MD5;
	my $md5 = Digest::MD5->new;
	
	open(FILE, $File) or $logging_obj->log("EXCEPTION","Error: Could not open $File for MD5 checksum, please refer to the usage...");
	binmode(FILE);
	my $md5sum = $md5->addfile(*FILE)->hexdigest;
	close FILE;

	return $md5sum;	
}

sub update_status
{
	my ($self,$status) = @_;
	
	my $sql;
	if(!$status)
	{
		$sql = "UPDATE rxSettings SET ValueData='' WHERE ValueName IN ('UPDATE_DOWNLOAD_URL','DOWNLOAD_FILE_NAME','DOWNLOAD_FILE_CHECKSUM')";
		$logging_obj->log("DEBUG","update_status : Executing SQL : $sql");
		$self->{'conn'}->sql_query($sql);
	}
	$sql = "UPDATE rxSettings SET ValueData = '$status' WHERE ValueName = 'UPDATE_STATUS'";	
	$logging_obj->log("DEBUG","update_status : Executing SQL : $sql");
	$self->{'conn'}->sql_query($sql);
}

sub install_update
{
	my($self,$file_name) = @_;
	
	my $file_path = $self->{'file_path'};
	print "file_path = $file_path, file = $file_name\n";
	my @file_details = split(/\./,$file_name);
	my $extension = $file_details[1];
	my $file = $file_details[0];
	
	print "file = $file, extension = $extension\n";
	if (Utilities::isLinux())
	{			
		if($extension eq "tgz" || $extension eq "tar")
		{				
			my $untar_path = $file_path.$file;
			my $install_file = "install.sh";
			my $executable_path = $untar_path."/".$install_file;
			my $install_cmd;
			
			if(-d($untar_path))
			{
				`rm -rf $untar_path`;
			}
			my $ret_dir = mkdir($untar_path);
			if(!$ret_dir)
			{
				$logging_obj->log("EXCEPTION","Failed to create directory $untar_path.\n");
				$self->update_status(-4);
			}
			
			my $untar_cmd	= "tar -xzvf ".$file_path.$file_name." -C ".$untar_path;		
			my $output = `$untar_cmd`;
			$logging_obj->log("DEBUG","Untar command : $untar_cmd");								
							
			if(!(-e($executable_path)))
			{				
				$logging_obj->log("EXCEPTION","Install not found at $executable_path");
				`rm -rf $untar_path`;
				$self->update_status(-4);
			}
			
			$install_cmd = $executable_path." > ".$self->{'update_log_path'}.$file."_update.log";				
			$logging_obj->log("EXCEPTION","Install command : $install_cmd");
			$self->update_status(4);			
			my $install_ret = `$install_cmd`;
			$logging_obj->log("EXCEPTION","Install command Returned : $install_ret");	
			if($? == 0)
			{
				$logging_obj->log("DEBUG","Update Installation Successful\n");
			}
			else
			{
				$logging_obj->log("DEBUG","Update Installation UnSuccessful\n");
				$self->update_status(-4);
			}
		}
		else 
		{			
			$logging_obj->log("EXCEPTION","Error: Not a valid Installer.\n");
			$self->update_status(-4);
		}		
	}
}

sub get_rx_compatibility
{
	my ($self) = @_;
		
	my $rx_ip = $self->{'rx_ip'};
	my $rx_port = $self->{'rx_port'};
	my $http = $self->{'http'};
	
	my $db_rx_compatibility_number = $self->get_param("RX_COMPATIBILITY_NUMBER");
		
	my $rpc_server = $http.'://'.$rx_ip.':'.$rx_port.'/xml_rpc/server.php';	
	my $xmlrpc = XML::RPC->new($rpc_server);			
	$logging_obj->log("DEBUG","get_rx_compatibility: XML RPC call to server : ".$rpc_server." Call name : getRxCompatibilityNumber");	
	my $result = $xmlrpc->call( 'report.getRxCompatibilityNumber');
	$logging_obj->log("DEBUG","get_rx_compatibility: ".Dumper($result));	
	my $rx_compatibility_number;
	
	if(!$result->{'faultCode'})
	{	
		$rx_compatibility_number = $result->{'RX_COMPATIBILITY_NUMBER'};
		if($db_rx_compatibility_number ne $rx_compatibility_number)
		{
			$logging_obj->log("DEBUG","get_rx_compatibility : Updating RX_COMPATIBILITY_NUMBER as $rx_compatibility_number");
			my $sql = "UPDATE rxSettings SET ValueData = '$rx_compatibility_number' WHERE ValueName = 'RX_COMPATIBILITY_NUMBER'";
			$logging_obj->log("DEBUG","get_rx_compatibility : $sql");
			$self->{'conn'}->sql_query($sql);
		}
	}
	else
	{		
		$logging_obj->log("EXCEPTION","XML RPC call getRxCompatibilityNumber failed :\n".Dumper($result));
	}
		
	return ($rx_compatibility_number) ? $rx_compatibility_number : 0;
}

sub get_param
{
	my($self,$param) = @_;
		
	my $data = $self->{'conn'}->sql_get_value('rxSettings', 'ValueData', "ValueName='$param'");	
	return $data;
}

sub get_cx_compatibility()
{
    my ($self) = @_;

   	my $compatible_number = $self->{'conn'}->sql_get_value('rxSettings', 'ValueData', "ValueName='CX_COMPATIBILITY_NUMBER'");	
    if(!$compatible_number)
    {
        my $version_file = "/home/svsystems/etc/version";
        open(VERSION_FILE,"$version_file");
        my $cx_version_str = <VERSION_FILE>;
        chomp($cx_version_str);
        my @version = split(/_/,$cx_version_str);
        my @version_arr = split(/\./,$version[1]);
        for(my $i=1;$i<($#version_arr+1);$i++)
        {
            if(length($version_arr[$i]) < 2)
            {
                $version_arr[$i] = sprintf("%02d",$version_arr[$i]);
            }
        }
        $compatible_number = join("",@version_arr);
        my $sql = "UPDATE rxSettings SET ValueData = '$compatible_number' WHERE ValueName='CX_COMPATIBILITY_NUMBER'";
        my $result = $self->{'conn'}->sql_query ($sql);
    }
    $logging_obj->log("DEBUG","CX_COMPATIBILITY_NUMBER = $compatible_number");
    return $compatible_number;
}

sub get_unix_timestamp
{
	my ($self,$time) = @_;
	my $sql = "SELECT UNIX_TIMESTAMP(\"$time\") as time";
	my $result = $self->{'conn'}->sql_query ($sql);
	my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($result);
	my $converted_unix_time = $ref->{'time'};
	return $converted_unix_time;
}

1;
