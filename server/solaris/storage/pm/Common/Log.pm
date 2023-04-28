#$Header: /src/server/solaris/storage/pm/Common/Log.pm,v 1.2 2014/02/13 12:13:19 rajella Exp $
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

use lib ("/home/svsystems/pm");
use strict;
use Data::Dumper;
use Time::HiRes qw/ time sleep gettimeofday /;
use File::Copy;
my $global_event_name;
my $global_verbose_mode;

#
# Function name: new
# Description: Constructor used to initialize the object .
# Input : None
# Return Value : Returns the object referance to the package 
#
sub new
{
	my $class = shift; 
	my $self = {};
	
	bless ($self, $class);
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
	my ($self, $log_type, $log_msg) = @_;
	my ($package, $caller_file_name, $line) = caller;
	my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
	my $tmanager_log_level = $amethyst_vars->{'TMANAGER_LOG_LEVEL'};
	my $file_name  =  $self->get_file_name($global_event_name);
	if(($tmanager_log_level =~ /EXCEPTION/i && $log_type =~ /EXCEPTION/i) || ($tmanager_log_level =~ /DEBUG/i && $log_type =~ /DEBUG/i) || ($log_type =~ /INFO/i))   
	{
		if ($file_name ne "") 
		{
			my $file_size = -s $file_name;
			if($file_size > 10485760) 
			{ 
				my $file_name_bak = $file_name."_bak";
				if( -e $file_name_bak)
				{
					unlink($file_name_bak);
					File::Copy::move($file_name,$file_name_bak);
				}						 
			}
			
			open(LOGFILE, ">>$file_name");
			my $entry   = $self->logdate() ." ".$log_type." : ".$global_event_name." : ".$log_msg." at ".$caller_file_name." line ".$line;
			print LOGFILE $entry."\n";
			close(LOGFILE);
		}
	}
	if($global_verbose_mode)
	{
		print $self->logdate() ." ".$log_type." : ".$global_event_name." : ".$log_msg." at ".$caller_file_name." line ".$line."\n";
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
	my ($self, $event_name) = @_;	
	my ($LOG_HOME_DIR, $file_name);	
	if (Utilities::isWindows())
	{
		$LOG_HOME_DIR	= "c:\\home\\svsystems\\var\\";
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

1;
