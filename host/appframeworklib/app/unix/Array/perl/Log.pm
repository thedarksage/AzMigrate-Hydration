#==========================================================================
# FILENAME
#   Log.pm
#
# DESCRIPTION
#    This perl module is responsible to get exeception & debug logs.
#
# HISTORY
#     25 February 2011  Keerthy    Created.
#
#=========================================================================
#                 Copyright (c) InMage Systems
#=========================================================================
package Log;
use strict;
use Data::Dumper;

#
# Function name: new
# Description: Constructor used to initialize the object .
# Input : None
# Return Value : Returns the object referance to the package
#

sub new
{
   	my $class = shift;
	my %CONF_PARAM;
	my $conf_file = "/usr/local/drscout.conf";
	if(!-e $conf_file)
	{
    	# Not to do anything if agent configuration file is not available
	    die "Configuration file NOT FOUND\n";
	}

	# Read the configuration file
	open(CONF,$conf_file);
	my @file_contents = <CONF>;
	close(CONF);

	# Constructing a hash of configuration parameters
	foreach my $attr (@file_contents)
	{
    	my @attrs = split("=",$attr);
	    $attrs[0] =~ s/\s+//g;
    	$attrs[1] =~ s/\s+//g;
	    $CONF_PARAM{$attrs[0]} = $attrs[1];
	}

   	my ($filename,$logOption,$logLevel,$intl_path);
	$filename  = $CONF_PARAM{"LogPathname"}."axiom_pcli.log";	
	$logOption = $CONF_PARAM{"LogOption"};
	$logLevel  = $CONF_PARAM{"LogLevel"};
	$intl_path = $CONF_PARAM{"InstallDirectory"};
   	my $self = {
				"filename"  => $filename,
				"logOption" => $logOption,
				"logLevel"  => $logLevel,
				"installPath" => $intl_path
			  };
   	bless ($self, $class);
   	return $self;
}

sub getInstallPath
{
	my $class = shift;
	return $class->{installPath};	
}

sub log
{
   my $self = shift;
   my $log_type = shift;
   my $log_msg = shift;

      if ($log_type eq "")
      {
              $log_type = "DEBUG";
      }

   my ($package, $caller_file_name, $line) = caller;
   my $log_level = $self->{'logLevel'};
   my @log_level_ary = split(',', $log_level);
   my $file_name  = $self->{"filename"};
   my $log_option = $self->{"logOption"};

   if ( (($log_option == 0) && ($log_type eq "EXCEPTION")) or
                 (($log_option == 1) && ($log_level_ary[1] gt "6") && ($log_type eq "DEBUG")) or
                 (($log_option == 1) && ($log_level_ary[0] gt "6") && ($log_type eq "DEBUG")) or
                 (($log_option == 1) && ($log_type eq "EXCEPTION"))
           )
   {
                if ($file_name ne "")
                {
                        open(LOGFILE, ">>$file_name");
                        my $entry   = $self->logdate() ." ".$log_type." : ARRAY_PCLI : ".$log_msg." at ".$caller_file_name." line ".$line;
                        print LOGFILE $entry."\n";
                        close(LOGFILE);
                }
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
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst)=localtime();
    my $formatted_time = sprintf "%4d-%02d-%02d %02d:%02d:%02d",$year+1900,$mon+1,$mday,$hour,$min,$sec;
    return $formatted_time;
}
1;
