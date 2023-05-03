#$Header: /src/server/tm/pm/Common/Log.pm,v 1.7.8.5 2018/08/27 04:49:11 vipenmet Exp $
#========================================================================== 
# FILENAME
#   Logging.pm 
#
# DESCRIPTION
#    This perl module is responsible to get exeception & debug logs.
#    
# HISTORY
#     19 March 2009  Pawan    Created.
#
#=========================================================================
#                 Copyright (c) InMage Systems                    
#=========================================================================
package Common::Log;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Config;
use POSIX;
use POSIX qw(strftime);
use Data::Dumper;
use Time::HiRes qw/ time sleep gettimeofday /;
use Time::Local;
use Common::Constants;
use win32;
require Utilities;
my $global_event_name;
my $global_verbose_mode;

my $MAX_CHUNK_SIZE = Common::Constants::MAX_CHUNK_SIZE;
my $MDS = Common::Constants::LOG_TO_MDS;
my $TELEMETRY = Common::Constants::LOG_TO_TELEMETRY;
my $MDS_AND_TELEMETRY = Common::Constants::LOG_TO_MDS_AND_TELEMETRY;
my $JSON_EXTENTION = Common::Constants::JSON_EXTENTION;
my $LOG_EXTENTION = Common::Constants::LOG_EXTENTION;
my $CS_LOGS_PATH = Common::Constants::CS_LOGS_PATH;
my $CS_TELEMETRY_PATH = Common::Constants::CS_TELEMETRY_PATH;
my $SLASH = Common::Constants::SLASH;
my $RECORD_SEPARATOR = Common::Constants::RECORD_SEPARATOR;
my $APPINSIGHTS_WRITE_DIR_COUNT_LIMIT = Common::Constants::APPINSIGHTS_WRITE_DIR_COUNT_LIMIT;
my $TELEMETRY_FOLDER_COUNT_FILE_PATH = Common::Constants::TELEMETRY_FOLDER_COUNT_FILE_PATH;
my $TELEMETRY_FOLDER_COUNT_FILE = Common::Constants::TELEMETRY_FOLDER_COUNT_FILE;
my %global_telemetry_metadata;

#
# Function name: new
# Description: Constructor used to initialize the object .
# Input : None
# Return Value : Returns the object referance to the package 
#

sub new
{
    my ($class) = shift;    
	my $self = {};
    bless($self, $class);
	$self->{'cs_installation_path'} = Utilities::get_cs_installation_path();
    $self->{'tmanager_log_level'} = $self->get_logging_level();
	
    return $self;
}

# 
# Function name : log
# Description:
# Function used to write debug & exception messages based upon log level 
# configurd in amethyst.conf file.
# Input : Logtype :Exception/Debug
#         Logmessage : Message that write into the log file
# Output : None
#
sub log
{ 
	my ($self, $log_type, $log_msg, $telemetry_flag) = @_;
	my ($package, $caller_file_name, $line) = caller;
	my $tmanager_log_level = $self->{'tmanager_log_level'};
	my ($file_name, $appinsights_log_file_name, $file_rename_status, $log_json_str, $matadata_json_str);
	my ($log_data, $logs_to_write,$total_json_length,$correlation_id);
	my ($cs_log_directory_count,$cs_telemetry_directory_count,$cs_log_directory,$cs_telemetry_directory,$telemetry_folder_exhausted_file_path);
	my ($log_telemetry_flag,$cs_log_flag);
	
	my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
	my $IS_MDS_LOGGING_ENABLED = $AMETHYST_VARS->{IS_MDS_LOGGING_ENABLED};
	my $IS_APPINSIGHTS_LOGGING_ENABLED = $AMETHYST_VARS->{IS_APPINSIGHTS_LOGGING_ENABLED};
	if (ref($log_msg) eq "HASH") 
	{
		$log_data = Dumper(%{$log_msg});
	}
	unless (ref($log_msg)) 
	{
		$log_data = $log_msg;
	}
	
	if((($telemetry_flag eq $TELEMETRY) || ($telemetry_flag eq $MDS_AND_TELEMETRY)) && ($IS_APPINSIGHTS_LOGGING_ENABLED == 1))
	{
		$telemetry_folder_exhausted_file_path = $self->{'cs_installation_path'}.$TELEMETRY_FOLDER_COUNT_FILE_PATH.$SLASH.$TELEMETRY_FOLDER_COUNT_FILE;
		$cs_log_directory = $self->{'cs_installation_path'}.$CS_LOGS_PATH;
		$cs_telemetry_directory = $self->{'cs_installation_path'}.$CS_TELEMETRY_PATH;
		$cs_log_directory_count = Utilities::read_directory_contents($cs_log_directory);
		$cs_telemetry_directory_count = Utilities::read_directory_contents($cs_telemetry_directory);
		$appinsights_log_file_name  =  $self->get_file_name($global_event_name,$telemetry_flag);
	}
	
	if(((!defined $telemetry_flag) || ($telemetry_flag eq $MDS) || ($telemetry_flag eq $MDS_AND_TELEMETRY)) && ($IS_MDS_LOGGING_ENABLED == 1))
	{
		$file_name  =  $self->get_file_name($global_event_name);
	}
	
	if(($tmanager_log_level =~ /EXCEPTION/i && $log_type =~ /EXCEPTION/i) || ($tmanager_log_level =~ /DEBUG/i && $log_type =~ /DEBUG/i) || ($log_type =~ /INFO/i))   
	{
		if ($appinsights_log_file_name ne "" && (($telemetry_flag eq $TELEMETRY) || ($telemetry_flag eq $MDS_AND_TELEMETRY)) && (! -e $telemetry_folder_exhausted_file_path)) 
		{
			$log_telemetry_flag = $self->validate_cs_log_telemetry_directory_existence();
			$cs_log_flag = $self->validate_cs_log_telemetry_directory_existence(1);
			#$cs_log_flag = $self->validate_log_directory_existence();
			if($log_telemetry_flag && $cs_log_flag)
			{
				$log_json_str  =  $self->convertArrToJsonString(\%{$log_msg});
				$matadata_json_str  =  $self->convertArrToJsonString(\%global_telemetry_metadata, 1);
				
				$total_json_length = length($matadata_json_str) + length($log_json_str) + length($RECORD_SEPARATOR);
				
				if($total_json_length <= $MAX_CHUNK_SIZE)
				{
					$correlation_id = Win32::GuidGen();
					$correlation_id =~ s/^(.)(.*)(.)$/$2/; 
					$matadata_json_str =~ s/"CorrelationId":"",//;
					
					$logs_to_write = $matadata_json_str." ".$log_json_str;
					$file_rename_status  =  $self->push_telemetry_data($appinsights_log_file_name,$logs_to_write,$JSON_EXTENTION);
				}
				else
				{
					$correlation_id = Win32::GuidGen();
					$correlation_id =~ s/^(.)(.*)(.)$/$2/; 
					$matadata_json_str =~ s/"CorrelationId":""/\"CorrelationId\":\"$correlation_id\"/;
					
					$logs_to_write = $matadata_json_str." ".$log_json_str;
					$file_rename_status  =  $self->push_telemetry_data($appinsights_log_file_name,$matadata_json_str,$JSON_EXTENTION);
					
					my $event_name = lc($global_event_name);
					$appinsights_log_file_name =~ s/$event_name/$correlation_id/;
					$file_rename_status  =  $self->push_telemetry_data($appinsights_log_file_name,$logs_to_write,$LOG_EXTENTION);
				}
				
				undef %{$log_msg};
			}
		}
		
		if ($file_name ne "") 
		{
			open(LOGFILE, ">>$file_name");
			my $entry   = $self->logdate() ." ".$log_type." : ".$global_event_name." ".$$." : ".$log_data." at ".$caller_file_name." line ".$line;
			print LOGFILE $entry."\n";
			close(LOGFILE);
		}
	}
	if($global_verbose_mode)
	{
		print $self->logdate() ." ".$log_type." : ".$global_event_name." ".$$." : ".$log_data." at ".$caller_file_name." line ".$line."\n";
	}   
}

#
# Function name : logdate
# Description : 
# Function used to get the time in YYYY-MM-DD hh:mm:ss format.
# Input  : None
# Output : Formated time 
#
sub logdate
{
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
	my ($sec1, $micro_sec) = gettimeofday();
    my $formatted_time = sprintf "%4d-%02d-%02d %02d:%02d:%02d:%06d",$year+1900,$mon+1,$mday,$hour,$min,$sec,$micro_sec;
    return $formatted_time;
}

#
# Function name : get_file_name
# Description :
# Function used to get file name for corresponding event.
# Input  : Event name
# Output : Returns the logfile name 
#
sub get_file_name
{	
	my ($self, $event_name, $telemetry_flag) = @_;	
	my ($LOG_HOME_DIR, $file_name);	
	
	if(!defined $telemetry_flag)
	{
		$telemetry_flag = 0;
	}
	
	if(($telemetry_flag eq $TELEMETRY) || ($telemetry_flag eq $MDS_AND_TELEMETRY))
	{
		if ($Config{osname} !~ /linux/i)
		{
			
			my $current_directory_name = $self->get_current_telemetry_log_directory();
			$LOG_HOME_DIR	= $self->{'cs_installation_path'}.$CS_LOGS_PATH.$SLASH.$current_directory_name.$SLASH;
		}
		else
		{
			$LOG_HOME_DIR	= "/home/svsystems/var/";
		}
		
		$event_name = lc($event_name);
		if($event_name)
		{		
			$file_name = $LOG_HOME_DIR.$event_name;	
		}
		else
		{
			$file_name = $LOG_HOME_DIR.'perldebug.log';
		}
	}
	else
	{
		if ($Config{osname} !~ /linux/i)
		{
			$LOG_HOME_DIR	= $self->{'cs_installation_path'}."\\home\\svsystems\\var\\";
		}
		else
		{
			$LOG_HOME_DIR	= "/home/svsystems/var/";
		}
		
		$event_name = lc($event_name);
		if($event_name)
		{		
			$file_name = $LOG_HOME_DIR.$event_name.'.log';	
		}
		else
		{
			$file_name = $LOG_HOME_DIR.'perldebug.log';
		}
	}
	return $file_name;
}


#
# Function name : set_event_name
# Description :
# Function used to set the event_name.
# Input  : Event name
# Output : None
#
sub set_event_name
{
	my ($self, $event_name) = @_;
	$global_event_name = $event_name;
}

#
# Function name : set_verbose
# Description :
# Function used to set the verbose mode.
# Input  : verbose mode (0/1)
# Output : None
#
sub set_verbose
{
	my ($self, $verbose_mode) = @_;	
	$global_verbose_mode = $verbose_mode;
}


sub get_logging_level
{
	#my $conf_file = "/home/svsystems/etc/amethyst.conf";
	my $self = shift;
	my $conf_file;
	if ($Config{osname} !~ /linux/i)
	{
		$conf_file	= $self->{'cs_installation_path'}."\\home\\svsystems\\etc\\amethyst.conf";
	}
	else
	{
		$conf_file	= "/home/svsystems/etc/amethyst.conf";
	}	
	my ($key, $output);
    if(-f $conf_file)
    {
        my $open_status = open (FILE, '<', $conf_file);
        #print "open status::".$open_status;
        if ($open_status)
        {
          	#print "entered";
        	while (<FILE>) 
			{
               		next if (! /^TMANAGER_LOG_LEVEL/ );
                       	($key, $output) = split /=/, $_, 2;
                      	$output =~ s/\"//g;
			}
			close FILE;
         }
     }
     my $tmanager_log_level  = ($output) ? $output : "EXCEPTION";
 	return $tmanager_log_level;
}

#
# Function name : get_current_telemetry_log_directory
# Description :
# Function used to return date and time format name.
# Input  : None
# Output : name in date and time format.
#
sub get_current_telemetry_log_directory
{
	my ($self) = @_;
	
	my $dt = time();
	my $year = strftime ('%Y', gmtime($dt));
	my $mon = strftime ('%m', gmtime($dt));
	my $mday = strftime ('%d', gmtime($dt));
	my $hour = strftime ('%H', gmtime($dt));
	my $min = strftime ('%M', gmtime($dt));
	my $sec = strftime ('%S', gmtime($dt));
	my $next_hour = sprintf("%02d", $hour+1);
	
	my $directory_name = $year."_".$mon."_".$mday."_".$hour."_".$next_hour;
	return $directory_name;
}

#
# Function name : push_telemetry_data
# Description :
# Function used to check file size and if size is more than 18KB then rename it.
# Input  : file name
# Output : true or false
#
sub push_telemetry_data
{
	my ($self, $appinsights_log_file_name,$log_chunked_data,$file_extention) = @_;
	my ($status,$file_name);
	
	my $push_data   = $log_chunked_data." ".$RECORD_SEPARATOR;
	my $log_chunk_length = length($push_data);
	my $file_size = -s 	$appinsights_log_file_name.$file_extention; # returns file size in bytes.
	
	if((($file_size+$log_chunk_length) > $MAX_CHUNK_SIZE) && ($file_extention eq "$JSON_EXTENTION")) # check if file size is more than 18 KB then rename file name.
	{ 
		my $random = int(rand(999999)); # generating 6 digit random number for file name
		my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
		$year += 1900; 
		$mon += 1;
		my $utc_time = timelocal($sec,$min,$hour,$mday,$mon-1,$year);
		my $telemetry_new_file_name = $appinsights_log_file_name."_".$utc_time."_".$random.$file_extention;
		$status = rename $appinsights_log_file_name.$file_extention,$telemetry_new_file_name;
	}
	
	$file_name = $appinsights_log_file_name.$file_extention;
	open(LOGFILE, ">>$file_name");
	print LOGFILE $push_data."\n";
	close(LOGFILE);	
	
	return $status;
}


sub convertArrToJsonString
{
	my ($self, $json, $is_metadata ) = @_;
	my $json_str = "";
	my $incr_num = 1;
	my $num = 1;
	my %json_hash = %{$json};
	#my %header_hash = %{$header};
	
	if((defined $is_metadata) && $is_metadata == 1)
	{
		my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
		my $CS_IP = $AMETHYST_VARS->{CS_IP};
		my $CS_GUID = $AMETHYST_VARS->{HOST_GUID};
		my $version_data = &file_get_contents($self->{'cs_installation_path'}."\\home\\svsystems\\etc\\version");
		$version_data =~ s/^\s+//;
		my @CS_VERSION = split(/\n/, $version_data);
		
		$json_hash{'DateTime'} = $self->logdate();
		$json_hash{'CSIP'} = $CS_IP;
		$json_hash{'CSGUID'} = $CS_GUID;
		$json_hash{'CSVERSION'} = $CS_VERSION[0];
		$json_hash{'CorrelationId'} = "";
	}
	
	my $hash_keys_count = keys %json_hash;
	#my $metadata_keys_count = keys %header_hash;
	
	foreach my $tmpkey (keys %json_hash)
	{		
		if($incr_num == 1)
		{
			$json_str .= '{';
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
			 if($is_metadata == 1)
			 {
				 $json_str .= "}\n";
			 }
			 else
			 {
				$json_str .= "}\n";
			 }
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

#
# Function name : set_telemetry_metadata
# Description :
# Function used to set the global telemetry meta data.
# Input  : Event name
# Output : None
#
sub set_telemetry_metadata
{
	my ($self, %telemetry_metadata) = @_;
	%global_telemetry_metadata = %telemetry_metadata;
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
		#local $/ = "\n";
	}
	
	return $file_content;
}

sub validate_cs_log_telemetry_directory_existence
{
	my ($self,$directoryFlag) = @_;
	my %log_data;
	my ($cs_log_directory,$current_log_directory_path);
	
	my $telemetry_directory = $self->{'cs_installation_path'}.$CS_LOGS_PATH;
	my $current_log_directory_path = $telemetry_directory;
	
	if($directoryFlag == 1)
	{
		$cs_log_directory = $self->get_current_telemetry_log_directory();
		$current_log_directory_path = $telemetry_directory."\\$cs_log_directory";
	}
		
	my $mkdir_output = mkdir "$current_log_directory_path" unless -d "$current_log_directory_path";
	if ($? != 0 && (! -e $current_log_directory_path)) 
	{
		return 0;
	}
	return 1;
}

1;
