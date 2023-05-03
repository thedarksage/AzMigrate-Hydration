#
# This is a common package containing utility functions used by
# tmanager.pm and TimeShotManager.pm. All common functions should be 
# included in this package.
#
package Utilities;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");

use Config;
use File::Copy;
use POSIX;
use HTTP::Date;
use Fabric::Constants;
use Common::Log;
use Data::Dumper;
use File::Find;
use File::Basename;
use File::Glob ':glob';
use Common::Constants;

# Used these constants for getting the WIN NIC IPs
use constant wbemFlagReturnImmediately => 0x10;
use constant wbemFlagForwardOnly => 0x20;
my $MDS_AND_TELEMETRY = Common::Constants::LOG_TO_MDS_AND_TELEMETRY;

my $amethyst_vars = {};
my $transport_vars={};
my $tmanager_events = {};
my $logging_obj = new Common::Log();

sub get_cs_installation_path
{
	my $csInstallDrivePath;
	my $csInstallPath = '';
	
	if (isWindows()) 
	{
		my $systemDrive = $ENV{'SystemDrive'};
		$csInstallDrivePath = join('\\', ($systemDrive, 'ProgramData', 'Microsoft Azure Site Recovery', "Config", "App.conf"));
		if (-e $csInstallDrivePath) 
		{		
			my $file_contents = &parse_conf_file($csInstallDrivePath);
		
			if(!$file_contents->{'INSTALLATION_PATH'}) 
			{
				$logging_obj->log("EXCEPTION", "get_cs_installation_path: cs install path is empty \n");
			}
			else
			{
				$csInstallPath = $file_contents->{'INSTALLATION_PATH'};
			}
		}
		else
		{
			$logging_obj->log("EXCEPTION", "get_cs_installation_path: Unable to fetch installation_directory.txt path \n");
		}
	} 
    
	return $csInstallPath;
}

##
#  
#Function Name: get_transport_conf_parameters  
# Description: 
#     This function is used to read all the parameters from cxps.conf file
# Return Value:
#     Returns the transport variables  
#
##
sub get_transport_conf_parameters
{
   my $amethyst_vars = get_ametheyst_conf_parameters();
   my $base_dir = $amethyst_vars->{"INSTALLATION_DIR"};
   my $trans_path = $base_dir."/transport/cxps.conf";
   my $transport_filename = makePath("$trans_path");
   open (FILEH, "<$transport_filename") || $logging_obj->log("DEBUG","Unable to locate $transport_filename:$!\n");
   my @file_contents = <FILEH>;
   close (FILEH);

   #
   # Read all the config parameters in cxps.conf and
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
          $transport_vars->{$arr[0]} = $arr[1];
      }
   }
   return $transport_vars;
}

##
#  
#Function Name: cxps_put_file  
# Description: 
#   This function is invoked from Register.pm and is used to transfer files from ps to cs using cxpsclient
# Parameters:
#   Param 1 [IN]:$send_file
#		   - This holds the file that ps has sent to cx.
#	Param 2 [IN]:$remote_dir
#          - This holds the destination directory name
# Return Value:
#   Returns 0 on success 
#
##
sub cxps_put_file
{
    my($send_file,$remote_dir,$is_cs_windows)=@_;
	my($user,$passwd);
	my $amethyst_vars = get_ametheyst_conf_parameters();
	my $ip = $amethyst_vars->{'PS_CS_IP'};
    my $base_dir = $amethyst_vars->{"INSTALLATION_DIR"};
	my $cxps_cmd = $base_dir."/bin/cxpsclient";
	my $transport_vars = get_transport_conf_parameters();
    my $port = $transport_vars->{'port'};
    if($is_cs_windows)
	{
		$user = $amethyst_vars->{'CXPS_USER_WINDOWS'};
		$passwd = $amethyst_vars->{'CXPS_PASSWORD_WINDOWS'};
	}
    else        
    {
		$user = $amethyst_vars->{'CXPS_USER_LINUX'};
		$passwd = $amethyst_vars->{'CXPS_PASSWORD_LINUX'};
	}     
    my $cmd = "$cxps_cmd -i $ip  -p $port -u $user -w $passwd --put \"$send_file\" -d \"$remote_dir\"";                    
	my $status = open(CMD_RESULT, "-|", $cmd);	
	if(<CMD_RESULT>)
    {		
	$logging_obj->log("EXCEPTION","Error while syncing file: Failed to execute 
		  	             $cmd Command,Open status is $status ,cxpsclient status is $?: $!\n");
	}
	close(CMD_RESULT);
}

##
#  
#Function Name: cxps_get_file
# Description: 
#   This function will fetch a file from the remote machine and
#   place it in the local machine's destination path provided
#
# Parameters:
#   Param 1 [IN]:$remote_file
#       - This holds the file with path that is to be picked from the remote machine
#	Param 2 [IN]:$local_file
#       - This holds the local machine's destination path with file name
# Return Value:
#
##
sub cxps_get_file
{
    my($remote_file,$local_file) = @_;
    my($user,$passwd);
    my $amethyst_vars = get_ametheyst_conf_parameters();
    my $ip = $amethyst_vars->{'PS_CS_IP'};
    my $base_dir = $amethyst_vars->{"INSTALLATION_DIR"};
    my $cxps_cmd = $base_dir."/bin/cxpsclient";
    my $transport_vars = get_transport_conf_parameters();
    my $port = $transport_vars->{'port'};
    if(isLinux())
    {
        $user = $amethyst_vars->{'CXPS_USER_LINUX'};
        $passwd = $amethyst_vars->{'CXPS_PASSWORD_LINUX'};
    }
    else
    {
        $user = $amethyst_vars->{'CXPS_USER_WINDOWS'};
        $passwd = $amethyst_vars->{'CXPS_PASSWORD_WINDOWS'};
    }
    my $cmd = "$cxps_cmd -i $ip -p $port -u $user -w $passwd --get $remote_file -L $local_file";
    my $status = open(CMD_RESULT, "-|", $cmd);
    if(<CMD_RESULT>)
    {
        $logging_obj->log("EXCEPTION","Error while syncing file: Failed to execute 
        $cmd Command,Open status is $status ,cxpsclient status is $?: $!\n");
    }
    close(CMD_RESULT);
}
	 
#
# Return 1 if host OS is windows
#
sub isWindows {
  my $os = getOsType();
  $os =~ m/win/ig ? return 1 : return 0;
}

#
# Convert a linux path to its windows equivalent if Windows host
#
sub makePath
{
    my $logging_obj = new Common::Log();
    my $path = $_[0];
    
    my $osFlag;
    if (defined $_[1]) 
    {
        $osFlag = $_[1];
    }
    else
    {
        $osFlag = -1;
    }
    my $cs_installation_path = get_cs_installation_path();
    my $windows_path = $path;
    if (isWindows()) # WINDOWS CX
    {
        #$path =~ s/[\\]/\\\\/g;  
		$path =~ s/[\/]/\\\\/g;  
        substr( $path, 1+index( $path, ':' ) ) =~ s/[:]//;     
        
        # If the path starts with windows slash, prepend with cs installation path        
        if($path =~ /^\\/)
        { 
            $windows_path = $cs_installation_path."$path";
        } 
        else
        {
            $windows_path = $path;
        }
        #$logging_obj->log("EXCEPTION","windows_path:: $windows_path \n");
        return $windows_path;
    }
    else # LINUX CX
    {
        if ($osFlag == 3)
        {
            $path =~ s|\\|\/|g; # FOR SOLARIS AGENT
        }
        else  # OTHER THAN SOLARIS AGENT
        {
            $path =~ s/[:]// and $path =~ s|\\|\/|g;
            $path =~ s|\\|\/|g; 
        }
        return $path;
    }
}

#
# Returns linux or MSWin32
#
sub getOsType() 
{
  return $Config{osname};
}

#
# Return 1 if host OS is linux
#
sub isLinux() 
{
  my $os = getOsType();
  $os =~ m/linux/ig ? return 1 : return 0;
}

#
# Read all the parameters from amethyst.conf 
#
sub get_ametheyst_conf_parameters
{
    my $amethyst_filename = makePath("/home/svsystems/etc/amethyst.conf");
    my $amethyst_vars = &parse_conf_file($amethyst_filename);
    
	if($amethyst_vars->{'CX_TYPE'} eq "3" || $amethyst_vars->{'CX_TYPE'} eq "2")
    {
        $amethyst_vars->{'PS_CS_HTTP'} = ($amethyst_vars->{'PS_CS_SECURED_COMMUNICATION'}) ? "https" : "http";
    }
	if($amethyst_vars->{'CX_TYPE'} eq "3" || $amethyst_vars->{'CX_TYPE'} eq "1")
	{
		$amethyst_vars->{'CS_HTTP'} = ($amethyst_vars->{'CS_SECURED_COMMUNICATION'}) ? "https" : "http";
	}
	if(isWindows())
	{
		$amethyst_vars->{'PHP_PATH'} = (defined $amethyst_vars->{'PHP_PATH'} && $amethyst_vars->{'PHP_PATH'} ne "") ? $amethyst_vars->{'PHP_PATH'}."\\php.exe" : "php.exe";
	}
	return $amethyst_vars;
}

sub parse_conf_file
{
	my ($filename) = @_;
	my $amethyst_vars;
	open (FILEH, "<$filename") || die "Unable to locate $filename:$!\n";
	my @file_contents = <FILEH>;
	close (FILEH);

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
			if($arr[0])
			{
				$amethyst_vars->{$arr[0]} = $arr[1];
			}
		}
	}
	return $amethyst_vars;
}

# 
# Find a list of files based on some pattern.
# 
sub read_directory_contents 
{
    my ($directory_name, $pattern) = @_;
    
    opendir (DIRH, $directory_name) || warn ("Cannot read directory contents. Directory name: $directory_name. Error: $!\n");
    my @dir_elements = readdir(DIRH);
    closedir (DIRH);

    if ($pattern ne "") 
    {
        @dir_elements = grep (!(/^\./) && (/$pattern/), @dir_elements);	    
    }
    else
    {
        @dir_elements = grep (!(/^\./), @dir_elements);
    }
    return @dir_elements;
}

#
# Perl function to move a file. 
#
sub move_file 
{
     my ($source, $destination) = @_;
     
     my $return_val = "";

     if (($source ne "") && ($destination ne ""))
     {
         $return_val = move ($source, $destination);
     }
     
     return $return_val;
}

sub datetimeToUnixTimestamp
{
	my ($datetime) = @_;
	
	my $unixtime = HTTP::Date::str2time ($datetime);
	
	return $unixtime;
}

sub unixTimestampToDatetime
{
	my ($unixtime) = @_;
	
	my $datetime = HTTP::Date::time2iso($unixtime);
	
	return $datetime;
}

sub unixTimestampToDate
{
	my ($unixtime) = @_;
	
	my ($date, $time) = split(" ", HTTP::Date::time2iso($unixtime)); 

	return $date;
}

sub datetimeToDate
{
	my ($datetime) = @_;
	
	my @arr = split(/ /, $datetime);
	
	return $arr[0];
}

sub makeLinuxPath
{
	my $path = $_[0];
	my $osFlag;
	if (defined $_[1]) 
	{
		$osFlag = $_[1];
	}
	else
	{
		$osFlag = -1;
	}
	if ($osFlag == 3)
	{
		$path =~ s|\\|\/|g;
		$path =~ s|\/\/|\/|g;		
	}
	else
	{
		$path =~ s/[:]//;
		$path =~ s|\\|\/|g;
		$path =~ s|\/\/|\/|g;
	}
  return $path;

}


#
# Returns the IP address of the host OS
#
sub getIpAddress
{
   #my $hostname = pop (@_);
   my $hostname = `hostname`;
   chomp $hostname;

   my $ipAddress = "";
   my ($packedIP, $ip_command, $pattern);
   my (@val, $line);
   my $ntw;

   my $amethyst_vars        = get_ametheyst_conf_parameters();
   my $bpm_ntw_device       = $amethyst_vars->{"BPM_NTW_DEVICE"};

   use Socket;
   my $library_path;
   BEGIN {$library_path = InstallationPath::getInstallationPath();}
   use lib ("$library_path/home/svsystems/pm");
   #use lib ("/home/svsystems/pm");
   use ProcessServer::Trending::NetworkTrends;

	if(isWindows())
	{
		use Win32::OLE('in');
		
        # Get PS_IP in the amethyst
        my $amethyst_ip = $amethyst_vars->{"PS_IP"};
        
        # Get the NIC details of BPM_NTW_DEVICE
        my $objWMIService = Win32::OLE->GetObject("winmgmts:\\\\localhost\\root\\CIMV2") or die "WMI connection failed.\n";
        my $colItems = $objWMIService->ExecQuery("SELECT * FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled=1 AND Description = '".$bpm_ntw_device."'", "WQL", wbemFlagReturnImmediately);		
		
		# If there is no return for the given BPM device, get all the NICS details
		if(! $colItems->{Count})
		{
			$logging_obj->log("EXCEPTION", "Could not get the information on the $bpm_ntw_device NIC, so picking any IP Address from the available ones");
			$colItems = $objWMIService->ExecQuery("SELECT * FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled=1", "WQL", wbemFlagReturnImmediately | wbemFlagForwardOnly);
		}
		
        foreach my $objItem (in $colItems)
        {
            foreach my $ip (in $objItem->{IPAddress}) 
            {
                # To pick only the first IP assigned to the NIC
                $ipAddress = (!$ipAddress) ? $ip : $ipAddress;
                if($ip !~ /:/) 
                {
                    # If the NIC IP matches with that of amethyst use the same
                    if($ip eq $amethyst_ip)
                    {
                        $ipAddress = $amethyst_ip;
                        last;
                    }
                }
            }	
        }
	}
	else
	{
       $ntw = ProcessServer::Trending::NetworkTrends->new();
       $ntw->get_device_statistics();
       my $ip = $ntw->get_ip_address();
       
       if($bpm_ntw_device ne "")
       {
            $ipAddress = $ip->{$bpm_ntw_device};       
       }
       else
       {
	    my ($package, $filename, $line) = caller;
	    $logging_obj->log("DEBUG","getIpAddress : Package: $package, File: $filename, Line no: $line - ". 
		    		      "BPM_NTW_DEVICE in amethyst.conf does not contain a valid NIC name. ". 
				          "Defaulting to eth0\n");
            $ipAddress = $ip->{"eth0"};   
       }

       chomp $ipAddress;
	}
   
   return $ipAddress;
}
#
# This function will write the log in sentinel.txt
#  this function will call to write log in agent log file.
# 
sub update_agent_log
{
    my ($hostId, $volume, $webRoot, $errorString) = @_;
	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
 
    my @weekdays = qw(Sunday Monday Tuesday
                      Wednesday Thursday Friday Saturday);

    my @months = ( "January", "February", "March", "April",
                   "May", "June", "July", "August", "September",
                   "October", "November", "December" );
	my $currentYear = 1900 + $year;
	my $currentTime = $hour.':'.$min.':'.$sec;
    #print "Date is: $weekdays[$wday],$months[$mon],$mday,$currentYear,$hour:$min:$sec\n";
	$errorString = $weekdays[$wday].' '.$months[$mon].' '.$mday.' '.$currentTime.' '.$currentYear.' '.$errorString;
	   
	# this regulaar expression will replace // in to /
	$volume =~ s/\/\//\//g;
	#  this regulaar expression will replace : in to 
	$volume =~ s/://g;
	
	
	my $filename = Fabric::Constants::BASE_DIR.'/'.$hostId.'/'.$volume.'/sentinel.txt';
	
	#opening the log file
	open(WRITELOGFILE, ">>$filename") or return -1;
	print WRITELOGFILE "$errorString \n";	
	
	$volume =~ s/\//_/g;
	my $log_link = $webRoot.'/trends/logs/Sentinel_'.$hostId.'_'.
                            $volume.'.txt';
                            
    # this command to check file is exist or not.
	if(!-e $log_link)
    {
        #Creating the soft link on unix flavours 
       my $cmd = 'ln -s '.$filename.' '.$log_link;
       my $output = system($cmd);
            
    }
	
	#closing the log file
	close(WRITELOGFILE);
    return 0;
}
#
# Get all the tmanagerd events
#
sub get_tmanager_events
{
   my $path = makePath("/home/svsystems/etc/tman_events.conf");
   my $tmanager_events = &parse_conf_file($path);   
   return $tmanager_events;
}

#this is the perl equivalent of php's str_replace. but won't replace the needle at last position of the string, if present.
sub str_replace {
	my $replace_this = shift;
	my $with_this  = shift; 
	my $string   = shift;
	
	my $length = length($string);
	my $target = length($replace_this);
	
	for(my $i=0; $i<$length - $target + 1; $i++) {
		if(substr($string,$i,$target) eq $replace_this) {
			$string = substr($string,0,$i) . $with_this . substr($string,$i+$target);
			# return $string; #Comment this if you what a global replace
		}
	}
	return $string;
}
sub uniq 
{
    return keys %{{ map { $_ => 1 } @_ }};
}


sub get_event_configuration
{
	my ($get_interval) = @_;
	my ($event_hash, $service_hash);
	
	my $amethyst_params = &get_ametheyst_conf_parameters();
	my $service_conf_file = makePath("/home/svsystems/etc/tman_services.conf");
	my $script_conf_file = makePath("/home/svsystems/etc/eventmanager.conf");
	my $config_hash = &parse_conf_file($script_conf_file);
	
	open (FILEH, "<$service_conf_file") || die "Unable to locate $service_conf_file:$!\n";
	my @service_file_contents = <FILEH>;
	close (FILEH);
	
	#
	# Read all the config parameters in tman_events.conf and 
	# put them in a hash.
	#
	foreach my $line (@service_file_contents)
	{
		chomp $line;		
		if (($line ne "") && ($line !~ /^#/))
		{
			my @arr = split (/=/, $line);
			$arr[0] =~ s/\s+$//g;
			$arr[1] =~ s/^\s+//g;
			$arr[1] =~ s/\"//g;
			$arr[1] =~ s/\s+$//g;
			if($arr[1] eq "ON")
			{
				$service_hash->{$arr[0]} = $arr[1];
			}
			else
			{
				my $service_file = $amethyst_params->{"INSTALLATION_DIR"}."/var/services/".$arr[0];
				unlink($service_file);
			}
		}
	}
	
	open (FILEH, "<$script_conf_file") || die "Unable to locate $script_conf_file:$!\n";
	my @script_file_contents = <FILEH>;
	close (FILEH);
	
	
	foreach my $conf_file (keys %$config_hash)
	{	
		my $config_file = makePath("/home/svsystems/etc/".$conf_file);
		
		open (FILEH, "<$config_file") || die "Unable to locate $config_file:$!\n";
		my @event_file_contents = <FILEH>;
		close (FILEH);
	
		foreach my $line (@event_file_contents)
		{
			chomp $line;
			if (($line ne "") && ($line !~ /^#/))
			{
				my @arr = split (/=/, $line);
				$arr[0] =~ s/\s+$//g;			
				$arr[1] =~ s/\"//g;			
				$arr[1] =~ s/\s+//g;
				my $event = $arr[0];
				my @event_conf = split (/,/, $arr[1]);
				my $event_state = $event_conf[0];
				my $solution = $event_conf[1];
				my $comp = $event_conf[2];
				my $package = $event_conf[3];
				my $interval = $event_conf[4];
				my $thread = $event_conf[5];			
				my $script_file = $event_conf[6];			
				my $cx_type = $amethyst_params->{'CX_TYPE'};
				my $cx_mode = $amethyst_params->{'CX_MODE'};
				
				# Reading the value as ON/OFF,HOST/FABRIC/ALL,CS/PS/BOTH,PACKAGE,INTERVAL,THREAD
				if($event_state eq "ON" && $service_hash->{$thread})
				{
					if(($get_interval && $get_interval eq $interval) || (!$get_interval))
					{
						if(( $solution eq "FABRIC" && $cx_mode eq "FABRIC") ||
								($solution eq "HOST" && ($cx_mode eq "" || $cx_mode eq "HOST")) ||
								($solution eq "ALL"))					
						{
							if(( $comp eq "CS" && ($cx_type eq "1" || $cx_type eq "3")) || 
							($comp eq "PS" && ($cx_type eq "2" || $cx_type eq "3")) || 
							($comp eq "BOTH"))
							{						
								push(@{$event_hash->{$interval}},{  "event"=>$event,
																	"solution"=>$solution,
																	"component"=>$comp,
																	"package"=>$package,
																	"thread"=>$thread,
																	"script"=>$script_file
																	});
							}
						}						
					}
				}
			}
		}
	}
	return $event_hash;
}

sub get_event_details
{
	my ($event) = @_;
	my $event_hash;
	my $service_hash;
	my $amethyst_params = &get_ametheyst_conf_parameters();
	my $service_conf_file = makePath("/home/svsystems/etc/tman_services.conf");
	my $script_conf_file = makePath("/home/svsystems/etc/eventmanager.conf");
	my $config_hash = &parse_conf_file($script_conf_file);
	
	open (FILEH, "<$service_conf_file") || die "Unable to locate $service_conf_file:$!\n";
	my @service_file_contents = <FILEH>;
	close (FILEH);
	
	#
	# Read all the config parameters in tman_events.conf and 
	# put them in a hash.
	#
	foreach my $line (@service_file_contents)
	{
		chomp $line;		
		if (($line ne "") && ($line !~ /^#/))
		{
			my @arr = split (/=/, $line);
			$arr[0] =~ s/\s+$//g;
			$arr[1] =~ s/^\s+//g;
			$arr[1] =~ s/\"//g;
			$arr[1] =~ s/\s+$//g;
			if($arr[1] eq "ON")
			{
				$service_hash->{$arr[0]} = $arr[1];
			}
			else
			{
				my $service_file = $amethyst_params->{"INSTALLATION_DIR"}."/var/services/".$arr[0];
				unlink($service_file);
			}
		}
	}
	
	open (FILEH, "<$script_conf_file") || die "Unable to locate $script_conf_file:$!\n";
	my @script_file_contents = <FILEH>;
	close (FILEH);
	
	#
	# Read all the config parameters in tman_events.conf and 
	# put them in a hash.
	#
	
	foreach my $conf_file (keys %$config_hash)
	{	
		my $config_file = makePath("/home/svsystems/etc/".$conf_file);
		
		my $event_file_contents = &parse_conf_file($config_file);
		#
		# Read all the config parameters in tman_events.conf and 
		# put them in a hash.
		#
		my $line = $event_file_contents->{$event};
		$line =~ s/\s+//g;
		$line =~ s/=/,/g;
		my($event_state,$solution,$comp,$package,$interval,$thread,$script_file,$param_script) = split (/,/, $line);
			
		my $cx_type = $amethyst_params->{'CX_TYPE'};
		my $cx_mode = $amethyst_params->{'CX_MODE'};
		
		# Reading the value as ON/OFF,HOST/FABRIC/ALL,CS/PS/BOTH,PACKAGE,INTERVAL,THREAD
		if($event_state eq "ON" && $service_hash->{$thread})
		{
			if(( $solution eq "FABRIC" && $cx_mode eq "FABRIC") ||
				($solution eq "HOST" && ($cx_mode eq "" || $cx_mode eq "HOST")) ||
				($solution eq "ALL"))					
			{
				if(( $comp eq "CS" && ($cx_type eq "1" || $cx_type eq "3")) || 
				($comp eq "PS" && ($cx_type eq "2" || $cx_type eq "3")) || 
				($comp eq "BOTH"))
				{						
					return 	({ 	"event"=>$event,
								"solution"=>$solution,
								"interval"=>$interval,
								"component"=>$comp,
								"package"=>$package,
								"thread"=>$thread,	
								"script_file"=>$script_file,			
								"param_script"=>$param_script								
								});						
				}					
			}
		}
	}
	return undef;
}

sub get_service_lowest_interval
{
	my ($service) = @_;
	my $event_hash;
	my $lowest_interval = 0;
	my $event_conf_file = makePath("/home/svsystems/etc/tman_events.conf");
	my $amethyst_params = &get_ametheyst_conf_parameters();
   
	open (FILEH, "<$event_conf_file") || die "Unable to locate $event_conf_file:$!\n";
	my @event_file_contents = <FILEH>;
	close (FILEH);
   
	#
	# Read all the config parameters in tman_events.conf and 
	# put them in a hash.
	#
	foreach my $line (@event_file_contents)
	{
		chomp $line;
		if (($line ne "") && ($line !~ /^#/))
		{
			my @arr = split (/=/, $line);
			$arr[0] =~ s/\s+$//g;			
			$arr[1] =~ s/\"//g;			
			$arr[1] =~ s/\s+//g;
			my $event = $arr[0];
			my @event_conf = split (/,/, $arr[1]);
			my $event_state = $event_conf[0];
			my $solution = $event_conf[1];
			my $comp = $event_conf[2];
			my $package = $event_conf[3];
			my $interval = $event_conf[4];
			my $thread = $event_conf[5];			
			my $cx_type = $amethyst_params->{'CX_TYPE'};
			my $cx_mode = $amethyst_params->{'CX_MODE'};
			
			# Reading the value as ON/OFF,HOST/FABRIC/ALL,CS/PS/BOTH,PACKAGE,INTERVAL,THREAD
			if($thread eq $service)
			{
				if(($lowest_interval && ($lowest_interval > $interval)) || (!$lowest_interval))
				{
					$lowest_interval = $interval;
				}
			}
		}
	}
	return $lowest_interval;
}

sub getWinIPaddress {
	my %nicDetails = ();
	use if $^O eq 'MSWin32', 'Win32::OLE';
	use constant wbemFlagReturnImmediately => 0x10;
	use constant wbemFlagForwardOnly => 0x20;

	eval {
		my $computer = "localhost";
		my $objWMIService = Win32::OLE->GetObject("winmgmts:\\\\$computer\\root\\CIMV2");
		my $colItems = $objWMIService->ExecQuery("SELECT * FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled=1", "WQL",
					  wbemFlagReturnImmediately | wbemFlagForwardOnly);
		foreach my $objItem (Win32::OLE::in $colItems) {
			my $nic = $objItem->{Description};
			my $dns = join(",", (Win32::OLE::in $objItem->{DNSServerSearchOrder}));
			my $gateWay = join(",", (Win32::OLE::in $objItem->{DefaultIPGateway}));
			my @ip_arr = ();
			my @tmp_ip_arr = ();
			foreach my $ip (Win32::OLE::in $objItem->{IPAddress}) {
				push(@tmp_ip_arr,$ip);
				if($ip !~ /:/) {
					push(@ip_arr,$ip);
				}
			}
			my %IPSubnet = ();
			@IPSubnet{@tmp_ip_arr} = (Win32::OLE::in $objItem->{IPSubnet});
			$nicDetails{$nic} = [\@ip_arr,$dns,$gateWay,\%IPSubnet];
		}
	};
	if($@) {
		$logging_obj->log("EXCEPTION","getWinIPaddress: WMI connection failed $@\n");
	}
	
	return \%nicDetails;
}

sub get_event_config
{	
	my (%event_hash, $service_hash, %config_hash);
	my $amethyst_params = &get_ametheyst_conf_parameters();
	my $service_conf_file = makePath("/home/svsystems/etc/tman_services.conf");
	my $script_conf_file = makePath("/home/svsystems/etc/eventmanager.conf");
	my $config_hash = &parse_conf_file($script_conf_file);
	
	open (FILEH, "<$service_conf_file") || die "Unable to locate $service_conf_file:$!\n";
	my @service_file_contents = <FILEH>;
	close (FILEH);
	
	#
	# Read all the config parameters in tman_events.conf and 
	# put them in a hash.
	#
	foreach my $line (@service_file_contents)
	{
		chomp $line;		
		if (($line ne "") && ($line !~ /^#/))
		{
			my @arr = split (/=/, $line);
			$arr[0] =~ s/\s+$//g;
			$arr[1] =~ s/^\s+//g;
			$arr[1] =~ s/\"//g;
			$arr[1] =~ s/\s+$//g;
			if($arr[1] eq "ON")
			{
				$service_hash->{$arr[0]} = $arr[1];
			}
			else
			{
				my $service_file = $amethyst_params->{"INSTALLATION_DIR"}."/var/services/".$arr[0];
				unlink($service_file);
			}
		}
	}
	
	foreach my $conf_file (keys %$config_hash)
	{	
		my $config_file = makePath("/home/svsystems/etc/".$conf_file);
		
		open (FILEH, "<$config_file") || die "Unable to locate $config_file:$!\n";
		my @event_file_contents = <FILEH>;
		close (FILEH);
	
		foreach my $line (@event_file_contents)
		{
			chomp $line;
			if (($line ne "") && ($line !~ /^#/))
			{
				my @arr = split (/=/, $line);
				$arr[0] =~ s/\s+$//g;			
				$arr[1] =~ s/\"//g;			
				$arr[1] =~ s/\s+//g;
				my $event = $arr[0];
				my @event_conf = split (/,/, $arr[1]);
				my $event_state = $event_conf[0];
				my $solution = $event_conf[1];
				my $comp = $event_conf[2];
				my $package = $event_conf[3];
				my $interval = $event_conf[4];
				my $thread = $event_conf[5];			
				my $script = $event_conf[6];			
				my $param_script = $event_conf[6];			
				my $cx_type = $amethyst_params->{'CX_TYPE'};
				my $cx_mode = $amethyst_params->{'CX_MODE'};
				
				# Reading the value as ON/OFF,HOST/FABRIC/ALL,CS/PS/BOTH,PACKAGE,INTERVAL,THREAD
				if($event_state eq "ON" && $service_hash->{$thread})
				{
					if(( $solution eq "FABRIC" && $cx_mode eq "FABRIC") ||
							($solution eq "HOST" && ($cx_mode eq "" || $cx_mode eq "HOST")) ||
							($solution eq "ALL"))					
					{
						if(( $comp eq "CS" && ($cx_type eq "1" || $cx_type eq "3")) || 
						($comp eq "PS" && ($cx_type eq "2" || $cx_type eq "3")) || 
						($comp eq "BOTH"))
						{						
							$event_hash{$event}{"interval"} = $interval;
							$event_hash{$event}{"package"} = $package;
							$event_hash{$event}{"thread"} = $thread;					
							$event_hash{$event}{"script"} = $script;					
							$event_hash{$event}{"param_script"} = $param_script;										
						}
					}						
					
				}
			}
		}
	}
	return \%event_hash;
}

sub verify_module_installed
{
    my ($module) = @_;
    my @module_path = split(/::/,$module);
    my $module_dir = $module_path[$#module_path-1];
    my $actual_module = $module_path[$#module_path];
    my $module_exists = 0;
	find({ wanted => sub { if($module_exists == 0) { if (-f $_ && $_ =~ /(.*)\/$module_dir\/$actual_module\.pm$/) { $module_exists = 1; }}}, no_chdir => 1 } , @INC);
    return $module_exists;
}

sub get_cert_root_dir
{
	my $dirpath;
	if (isWindows()) 
	{
		$dirpath = $ENV{'ProgramData'};
		if ($dirpath)
		{
			$dirpath = join('\\', ($dirpath, 'Microsoft Azure Site Recovery'));
			if (-d $dirpath) {	return $dirpath; }
			$logging_obj->log("EXCEPTION", "get_cert_root_dir: Unable to fetch Microsoft Azure Site Recovery under :   $dirpath\n");
		}
		else
		{
			$logging_obj->log("EXCEPTION", "get_cert_root_dir: ProgramData path:  $dirpath does not exists \n");
		}
	 } 
	 else
	 {
		$dirpath = join('/', ('/usr', 'local', 'InMage'));
		if (-d $dirpath) {	return $dirpath; }
		
		$logging_obj->log("EXCEPTION", "get_cert_root_dir: Unable to fetch the path:  $dirpath\n");
	}
	
	return '';
}

sub get_cs_encryption_key
{
	my $encryption_key = '';
	eval
	{
		my $file_path;
		
		my $system_directory = &get_cert_root_dir();
		
		if (! $system_directory) {	return ''; }
		
		if(isWindows())
		{
			$file_path = $system_directory . '\\private\\encryption.key';
		}
		else
		{
			$file_path = $system_directory . '/private/encryption.key';
		}
		
		$encryption_key = &read_file($file_path);
	};
	
	if ($@)
	{
		$logging_obj->log("EXCEPTION", "get_cs_encryption_key : $@");
	}
	
	return $encryption_key;	
}

# Reads 512 bytes from the given file.
# Throws
#	DIE exception
sub read_file
{
	my ($file_path) = @_;
	my $file_contents;
	
	open (FILEH, "<", $file_path) || die "Unable to open $file_path : $!\n";
	binmode FILEH;
	read FILEH, $file_contents, 512;
	close (FILEH);

	# Chomp if its a text file
	chomp $file_contents if -T $file_path;
	return  $file_contents;
}

# Clean stale perl process based on given process.
# Search the given process and kill
# returns
#   0 - on success
#   non-zero - on failure
sub clean_stale_process
{
	my ($current_process, $current_pid) = @_;
	my $command;
	#$logging_obj->set_verbose(1) ;
	$logging_obj->log("DEBUG", "Entered clean_stale_process");
	
	# "C:\\home\\svsystems\\bin\\volsync.pl volsync" will return volsync.pl volsync
	$current_process = basename($current_process);	
	my ($process_file_name, $process_args) = split / /, $current_process if(Utilities::isWindows());
    $command = 'wmic process where "commandline like \'%%'.$process_file_name.'%%\'" get processid, commandline | findstr /C:"'.$process_file_name.'" | findstr /v "wmic" | findstr /v "findstr"' if(Utilities::isWindows());
	$command = 'ps -o pid,cmd -e | grep "' . $current_process . '" | grep -v grep' if(Utilities::isLinux());
	
    my $process_op = `$command 2>&1`;
    my $status = $?;
    if ($status != 0)
    {
        $logging_obj->log("EXCEPTION",": clean_stale_process: failed: cmd $command : status ($status:$process_op)\n\n");
        return $status;
    }
    else
    {
        $logging_obj->log("DEBUG", "clean_stale_process: command : $command :: process_op \n $process_op");
    }
	
    # the output should contain the current process details at the least
    my $found_current_pid = 0;
	if($process_op)
	{
		my ($stale_process, $stale_pid);
		
		foreach my $line (split /^/, $process_op)
		{
			# trim spaces
			$line =~ s/^\s+|\s+$//g;
			
			if (Utilities::isWindows())
			{
				# If not Perl executable
				# Execution commands can be => 
				# perl .\eventmanager.pl                                            3708
				# C:\strawberry\perl\bin\perl .\eventmanager.pl                     3708
				# C:\strawberry\perl\bin\perl" .\eventmanager.pl                                            3708
				# C:\strawberry\perl\bin\perl.exe .\eventmanager.pl				                            3708
				# "C:\Program Files (x86)\VMware\VMware vSphere CLI\Perl\bin\perl.exe" .\eventmanager.pl	3708			
				next unless $line =~ /perl(\.exe)?[",']?\s(.*)\s([0-9]+)$/;
				
				$stale_pid = $3;
				$stale_process = $2;
				$stale_process =~ s/^\s+|(")|\s+$//g;
			}
			else
			{
				# Command: 2791 /usr/bin/perl /home/svsystems/bin/volsync.pl volsync_child 24
				$line =~ /^(\d+)\s+(.+?)\s+(.+)$/;
				
				# Assign the callback values before checking perl executable, as they vanish in the next regular expression match
				$stale_pid = $1;
				$stale_process = $3;
				$stale_process =~ s/^\s+|\s+$//g;
				
				#If not Perl executable
				next unless $2 =~ /perl$/;
			}
			
			$logging_obj->log("DEBUG", "stale_process : ".basename($stale_process)." :: stale_pid : $stale_pid");
			$logging_obj->log("DEBUG", "current_process : $current_process :: current_pid : $current_pid");
			
            if ($current_pid == $stale_pid)
            {
                $found_current_pid = 1;
            }

			if((basename($stale_process) eq $current_process) && ($stale_pid =~ /^\d+$/) && ($stale_pid != $current_pid))
			{
				# Verify if the PID is running and If not, ignore the record
				#next unless kill 0, $stale_pid;	
  			
				$logging_obj->log("EXCEPTION", "Process - $stale_process is already runinng with PID : ".$stale_pid);

				my $kill_cmd;
				$kill_cmd = "taskkill /F /PID ".$stale_pid if(Utilities::isWindows());
				$kill_cmd = "kill -9 ".$stale_pid if (Utilities::isLinux());
				$logging_obj->log("EXCEPTION", "Kill Command : ".$kill_cmd);	

				my $kill_op = `$kill_cmd`;

				if($? != 0)
				{
					$logging_obj->log("EXCEPTION", "Failed to kill stale_process : $stale_process :: kill_op : $kill_op");
				}
				sleep 1;
			}
		}
	}

	$logging_obj->log("DEBUG", "Coming out from clean_stale_process");

    if ($found_current_pid == 0)
    {
        $logging_obj->log("EXCEPTION",": clean_stale_process: current pid $current_pid not found in process list\n\n");
        $logging_obj->log("EXCEPTION",": clean_stale_process: command $command : status ($status:$process_op)\n\n");
        return 1;
    }
    else
    {
        return 0;
    }
}

##
#  
#Function Name: get_process_list  
# Description: 
#   This function is used to get parent process list
# Parameters: None
# Return Value:
#   Returns hash of all the processes
#
##
sub get_process_list
{
	$logging_obj->log("DEBUG", "Entered get_process_list");
	my $process_list = {
				'bpm' => {
				     'command' => 'bpm.pl',
				     'status' => 'ON'
				   },
				'eventmanager' => {
				     'command' => 'eventmanager.pl',
				     'status' => 'ON'
				   },
				'systemjobs' => {
				     'command' => 'systemjobs.pl run_jobs',
				     'status' => 'ON'
				   },
				'systemmonitor' => {
				     'command' => 'systemmonitor.pl',
				     'status' => 'ON'
				   },
				'volsync' => {
				     'command' => 'volsync.pl volsync',
				     'status' => 'ON'
				   }   
			};
			
	$logging_obj->log("DEBUG", "PROCESS HASH : ".Dumper($process_list));
	
	my $amethyst_vars = get_ametheyst_conf_parameters();		
	my $base_dir = $amethyst_vars->{"INSTALLATION_DIR"};
	my $service_conf_file = makePath($base_dir."/etc/tman_services.conf");	
	$logging_obj->log("DEBUG", "get_process_list : service_conf_file : $service_conf_file");
	
	my $service_vars = &parse_conf_file($service_conf_file);
	$logging_obj->log("DEBUG", "get_process_list : ".Dumper($service_vars));
	
	foreach my $process (keys %$process_list)
	{
		my $status = (exists $service_vars->{$process} && defined $service_vars->{$process}) ? $service_vars->{$process} : $process_list->{$process}->{"status"};
		$process_list->{$process}->{"status"} = $status;
	}
	
	$logging_obj->log("DEBUG", "Returning from get_process_list");
	
	return $process_list;
}

sub del_files_with_pattern($$$;$)
{
	#$cert_update_status is optional argument.
	my ($directory_path, $pattern, $max_attempts,$cert_update_status) = @_;
	my $return_value = 0;
	my @files = bsd_glob $directory_path.$pattern;
	my $unlink_status;
	my $file_name;
	foreach $file_name (@files)
	{
		my $start_counter = 0;
		while ($start_counter < $max_attempts) 
		{
			#On failure, it returns false and sets $! (errno)
			$unlink_status = unlink($file_name);

			if( !$unlink_status )
			{
				$return_value = 1;
				$logging_obj->log("INFO", "del_files_with_pattern: Deleted file $file_name failed with status $unlink_status and error no is:".$?);	
			}
			else
			{
				$return_value = 0;
				$logging_obj->log("INFO", "del_files_with_pattern: Deleted file $file_name with status $unlink_status");
				last;
			}
			$start_counter++;
			sleep(1);
		}	
	}
	return $return_value;
}

sub move_files_with_pattern($$$$;$)
{
	#$cert_update_status is optional argument.
	my ($directory_path, $pattern, $max_attempts, $file_name_for_rename, $cert_update_status) = @_;
	my $return_value = 0;
	my @files = bsd_glob $directory_path.$pattern;
	my $file_name;
	foreach $file_name (@files)
	{
		my $start_counter = 0;
		while ($start_counter < $max_attempts) 
		{
			my $move_status = move_file($file_name , $directory_path.$file_name_for_rename);
			if( !$move_status )
			{
				$return_value = 1;
				$logging_obj->log("INFO", "move_files_with_pattern: Move file $file_name to $file_name_for_rename failed with status $move_status and error no is:".$?);	
			}
			else
			{
				$return_value = 0;
				$logging_obj->log("INFO", "move_files_with_pattern: Move file $file_name to $file_name_for_rename with status $move_status .");
				last;
			}
			$start_counter++;
			sleep(1);
		}	
	}
	return $return_value;
}

sub readDirContentsInSortOrder
{
	my ($dir, $pattern) = @_;
	my @files_list = ();
				
	opendir(DEST_DIR, $dir) || $logging_obj->log("EXCEPTION","readDirContentsInSortOrder Failed to opendir $dir") || return @files_list;
	my @files = sort { $b <=> $a } readdir(DEST_DIR);
	closedir(DEST_DIR);
	
	$logging_obj->log("DEBUG","readDirContentsInSortOrder dir: $dir, files: ".Dumper(\@files));
	
	if(@files)
	{
		@files_list = grep (!(/^\./) && (/$pattern/), @files);
	}
	$logging_obj->log("DEBUG","readDirContentsInSortOrder after applying pattern dir: $dir, files_list: ".Dumper(\@files_list));
	return @files_list;
}

sub convertToJsonString
{
	my (%json_hash) = @_;
	my $json_str = "";
	my $incr_num = 1;
	my $hash_keys_count = keys %json_hash;
	
	foreach my $tmpkey (keys %json_hash)
	{		
		if($incr_num == 1)
		{
			$json_str .= '{Map: {';
		}
		
		$tmpkey =~ s/^\s+|\s+$//g;
		$json_str .= '"'.$tmpkey.'":';
		$json_hash{$tmpkey} =~ s/^\s+|\s+$//g;
		#Check whether the value is number or string
		if($json_hash{$tmpkey} =~ /^-?(0|([1-9][0-9]*))(\.[0-9]+)?([eE][-+]?[0-9]+)?$/)
		{
			$json_str .= $json_hash{$tmpkey};
		}
		else
		{
			$json_str .= '"'.$json_hash{$tmpkey}.'"';
		}
		
		if($hash_keys_count == $incr_num)
		{
			$json_str .= '}}';
		}
		else
		{
			$json_str .= ', ';
		}
		$incr_num++;
		chomp $json_str;
	}
	return $json_str;
}

sub arrayuniq 
{
    my %seen;
    grep !$seen{$_}++, @_;
}

sub start_windows_service
{
	my($service_name, $max_attempts, $sleep_time) = @_;
	my (%status,%log_data);
	my($startCmd,$startResult,$statusCmd,$statusResult);
	my(@splitData);
	my $start_counter = 1;
	my $isServiceStarted = 0;
	
	my %telemetry_metadata = ('RecordType' => 'StartWindowsService');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	
	$startCmd = 'net start "'.$service_name.'"';
	$startResult = `$startCmd`;
	$log_data{"startCmd"} = $startCmd;
	$log_data{"startResult"} = $startResult;
	$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
	if($? == 0)
	{
		OUTER: while($start_counter <= $max_attempts)
		{
			$statusCmd = 'sc query "'.$service_name.'"';
			$statusResult = `$statusCmd`;

			@splitData = split(/\n/,$statusResult);
			INNER: foreach my $value (@splitData)
			{
				if ((uc($value) =~ /STATE/g)) 
				{
					if(uc($value) =~ /STOPPED/g)
					{
					   $log_data{"Message"} = "Service $service_name is not started, will recheck after $sleep_time seconds";
					   $logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
					   sleep($sleep_time);
					}
					else
					{
						$isServiceStarted = 1;
						$log_data{"Message"} = "Started $service_name in $start_counter attempts";
					    $logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
						last OUTER;
					}
				}
			}
			$start_counter++;
		}
		if(!$isServiceStarted)
		{
			$log_data{"Message"} = "Unable to start $service_name in $start_counter attempts";
			$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		}
	}
	else
	{
		$log_data{"Message"} = "Unable to start $service_name";
		$log_data{"CommandOutput"} = $startResult;
		$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
		return 0;
	}
	return 1;
}
1;
