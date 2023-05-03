#
# This is a common package containing utility functions used by
# tmanager.pm and TimeShotManager.pm. All common functions should be 
# included in this package.
#
package Utilities;

use lib("/home/svsystems/pm");

use strict;
use Config;
use POSIX;
use Common::Log;
use Data::Dumper;

my $amethyst_vars = {};
my $transport_vars={};
my $tmanager_events = {};
my $logging_obj = new Common::Log();

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
    my $windows_path = $path;
    if (isWindows()) # WINDOWS CX
    {
        # If the path doesn't contain the "c:", then it allows to execute the block.
        if ($path !~ m/^c\:/) # ONLY FOR LINUX AGENT
        {
            if ($osFlag == 3)
            {
                $windows_path = "c:$path";
            }
            #BUG 7894
            else
            {
            # If the path contains the :, it removes that from the path.
                $path =~ s/[:]//;
                $windows_path = "c:$path";
            }
        }
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
  $os =~ m/linux|solaris/ig ? return 1 : return 0;
}

#
# Read all the parameters from amethyst.conf 
#
sub get_ametheyst_conf_parameters
{
   #my $amethyst_filename = makePath("/home/svsystems/etc/amethyst.conf");
   my $amethyst_filename = makePath("/home/svsystems/etc/storage.conf");
   my $amethyst_vars = &parse_conf_file($amethyst_filename);   
   return $amethyst_vars;
}

sub parse_conf_file
{
	my ($filename) = @_;
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
			$amethyst_vars->{$arr[0]} = $arr[1];
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
	my $event_hash;
	my $service_hash;
	my $amethyst_params = &get_ametheyst_conf_parameters();
	my $event_conf_file = makePath("/home/svsystems/etc/tman_events.conf");
	my $service_conf_file = makePath("/home/svsystems/etc/tman_services.conf");
	  
	open (FILEH, "<$event_conf_file") || die "Unable to locate $event_conf_file:$!\n";
	my @event_file_contents = <FILEH>;
	close (FILEH);
   
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
			my $storage_node = $amethyst_params->{'STORAGE_NODE'};
			
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
						($comp eq "SN" && $storage_node eq "1") || 
						($comp eq "BOTH"))
						{						
							push(@{$event_hash->{$interval}},{  "event"=>$event,
																"solution"=>$solution,
																"component"=>$comp,
																"package"=>$package,
																"thread"=>$thread																
																});
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
	my $event_conf_file = makePath("/home/svsystems/etc/tman_events.conf");
	my $service_conf_file = makePath("/home/svsystems/etc/tman_services.conf");
	  
	open (FILEH, "<$event_conf_file") || die "Unable to locate $event_conf_file:$!\n";
	my @event_file_contents = <FILEH>;
	close (FILEH);
   
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
	
	#
	# Read all the config parameters in tman_events.conf and 
	# put them in a hash.
	#
	foreach my $line (@event_file_contents)
	{		
		if($line =~ /^\b$event\b/)
		{
			$line =~ s/\s+//g;
			$line =~ s/=/,/g;
			my @event_conf = split (/,/, $line);
			$logging_obj->log("INFO","Split Content : ".Dumper(@event_conf));
			my($event_name,$event_state,$solution,$comp,$package,$interval,$thread) = split (/,/, $line);
			
			my $cx_type = $amethyst_params->{'CX_TYPE'};
			my $cx_mode = $amethyst_params->{'CX_MODE'};
			my $storage_node = $amethyst_params->{'STORAGE_NODE'};
						
			# Reading the value as ON/OFF,HOST/FABRIC/ALL,CS/PS/BOTH,PACKAGE,INTERVAL,THREAD
			if($event_state eq "ON" && $service_hash->{$thread})
			{
				if(( $solution eq "FABRIC" && $cx_mode eq "FABRIC") ||
					($solution eq "HOST" && ($cx_mode eq "" || $cx_mode eq "HOST")) ||
					($solution eq "ALL"))					
				{
					if(( $comp eq "CS" && ($cx_type eq "1" || $cx_type eq "3")) || 
					($comp eq "PS" && ($cx_type eq "2" || $cx_type eq "3")) || 
					($comp eq "SN" && $storage_node eq "1") || 
					($comp eq "BOTH"))
					{						
						return 	({  	"event"=>$event,
									"solution"=>$solution,
									"interval"=>$interval,
									"component"=>$comp,
									"package"=>$package,
									"thread"=>$thread																
									});						
					}					
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

1;
