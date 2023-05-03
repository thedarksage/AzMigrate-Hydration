#================================================================= 
# FILENAME
#   BandwidthTrends.pm 
#
# DESCRIPTION
#    This perl module is responsible for creating/updating
#    bandwidth RRD file for each host downloading/uploading
#    data from a process server
#
# HISTORY
#     11 Dec 2008  hpatel    Created.
#     07 Jan 2009  chpadh   Updated.
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ProcessServer::Trending::BandwidthTrends;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
#use lib qw( /home/svsystems/rrd/lib/perl/ /home/svsystems/pm );
use File::Basename;
use Utilities;
use TimeShotManager;
use tmanager;
use Data::Dumper;
use File::Copy;
use ProcessServer::FTP::ParseLog;
use Common::Log;
use Common::DB::Connection;
use DBI();
use RRDs;
use PHP::Serialization qw(serialize unserialize);

my $logging_obj = new Common::Log();
my $MONITOR_INTERVAL  = 300; # 5 Minutes
my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};

#
# This sub routine used for to initialized the varibles.
#
sub new 
{
	my ($class, %args) = @_;
	my $self = {}; 
      
	$self->{'base_dir'} = $args{'base_dir'};
	$self->{'web_root'} = $args{'web_root'};
	#$self->{'rrd_command'} = $args{'rrd_command'};
	$self->{'log_dir'} = $args{'log_dir'};
	$self->{'diff'} = $args{'diff'};
	$self->{'conn'} = $args{'conn'};

	my $xfer_log_file = basename($args{'xfer_log_file'});

	my %xferlog_args = (
		'log_file'        => $xfer_log_file,
		'log_file_old'    => $xfer_log_file . ".bak",
		'log_file_diff'   => $xfer_log_file . ".diff",
		'log_dir'         => dirname($args{'xfer_log_file'}),
		'diff'		      => $self->{'diff'},
      'cxps_xfer'       => $args{'cxps_xfer'}
	);
	
	$self->{'rrdfilename'} = "";
	$self->{'rrderror'} = "";
	$self->{'xferlog'} = new ProcessServer::FTP::ParseLog(%xferlog_args);
	
	bless ($self, $class);
}

sub update_bandwidth_log
{
	my ($api_output,$args) = @_; 
	my %xfer_logs;
	my %args = %$args;
	
	$xfer_logs{'log_file'} = $args->{'xfer_log_file'};
	$xfer_logs{'log_file_old'} = $args->{'xfer_log_file'}.".bak";
	$xfer_logs{'log_file_diff'} = $args->{'xfer_log_file'}.".diff";
	$xfer_logs{'log_dir'} = dirname($args->{'xfer_log_file'});
	$xfer_logs{'diff'} = $args->{'diff'};
	$xfer_logs{'cxps_xfer'} = $args->{'cxps_xfer'};
	
	my %file_hash = ProcessServer::FTP::ParseLog::parse_xfer_diff(\%xfer_logs);
	$logging_obj->log("DEBUG","update_bandwidth_log : file_hash".Dumper(\%file_hash));
	my @ids = keys (%file_hash);
	
	#
	# dbh ping to check whether valid db resource or not and check the mysql availlable
	#
	my $host_data = $api_output->{'host_data'};
	eval
	{
		foreach my $host_id (@ids)
		{
			my ($tmp_file_size, $tmp_transfer_time, $tmp_avg_bytes) = (0, 0, 0);			
			my ($tot_bytes_in, $avg_bytes_in) = (0, 0);
			
			foreach my $val (@{$file_hash{$host_id}{'incomming'}})
			{
				$tot_bytes_in += $val->{'file_size'};
			}
			
			if($tot_bytes_in != 0)
			{
				$avg_bytes_in = int($tot_bytes_in);
			}
		
			my ($tot_bytes_out, $avg_bytes_out) = (0, 0);
			foreach my $val (@{$file_hash{$host_id}{'outgoing'}})
			{
				$tot_bytes_out += $val->{'file_size'};
			}
			
			if($tot_bytes_out != 0)
			{
				$avg_bytes_out = int($tot_bytes_out);
			}
			
			if(!(exists $host_data->{$host_id})) 
			{
				next;
			}
		
			#
			# Creation of bandwidth.txt, Check if exists, unless create it
			# Process server guid is included with bandwidth.txt
			
			my $bandwidth_txt = "/home/svsystems/".$host_id."/".$host_guid."-bandwidth.txt";
			my $os_flag = $host_data->{$host_id}->{'osFlag'};
			$bandwidth_txt = Utilities::makeLinuxPath($bandwidth_txt,$os_flag);
			open(LOGFILE, ">>$bandwidth_txt") || $logging_obj->log("EXCEPTION","Could not open $bandwidth_txt : $@ ");			

			my $entry   = $avg_bytes_out.':'.$avg_bytes_in."\n" ;
			print LOGFILE $entry;
			close(LOGFILE);
		}
	};
	if ($@)
    {
		$logging_obj->log("EXCEPTION","11: Failed to connect to DB from ProcessServer::Trending::BandwidthTrends: $@");
    }
}

#
# Create and Update the rrd file at specified time inverval 
#
sub update_bandwidth_rrd
{
	my $self = shift;
	
	my @ids = tmanager::get_host_ids($self->{'conn'});
	my @ps_guids = tmanager::get_ps_guids($self->{'conn'});
	
	#
	# dbh ping to check whether valid db resource or not and check the mysql availlable
	#
	eval
	{
        my $conn = $self->{'conn'};
		
		my $rx_ip;
		$rx_ip = $conn->sql_get_value('rxSettings', 'ValueData', "ValueName='IP_ADDRESS'");
		
		foreach my $host_id(@ids)
		{
			my @bandwidth_lines = "";
			my $os_flag = TimeShotManager::get_osFlag($conn,$host_id);
			my $bandwidth_str = '';
			my $bandwidth_in = 0;
			my $bandwidth_out = 0;
			my $host_dir = "/home/svsystems/".$host_id ;
			my $bandwidth_old_txt = "/home/svsystems/".$host_id."/bandwidth.txt";
			
			if(-e $host_dir)
			{
			
				$bandwidth_old_txt = Utilities::makeLinuxPath($bandwidth_old_txt,$os_flag);
				
			#Block added to handle bandwidth.txt, which may be sent by PS(if not upgraded/updated)
				if (-s $bandwidth_old_txt)
				{
					if (!open(LOG, "<$bandwidth_old_txt"))
					{
						print ("Could not open $bandwidth_old_txt for reading\n");
						next;
					}
					else
					{
						@bandwidth_lines = <LOG>;
					}
					close LOG;
					my $num_data_points = @bandwidth_lines;
					
					foreach my $bandwidth_data(@bandwidth_lines)
					{
						chomp $bandwidth_data;
						$bandwidth_data =~ s/\s+/ /g;	# Compress whitespace
						$bandwidth_data =~ s/^\s+//;	# Strip leading whitespace
						$bandwidth_data =~ s/\s+$//;	# Strip trailing whitespace
						$logging_obj->log("DEBUG","update_bandwidth_rrd :: bandwidth_old_txt - bandwidth_old_txt, bandwidth_data - $bandwidth_data ");
						my ($in, $out) = split(/:/, $bandwidth_data);
						
						$bandwidth_in += $in;
						$bandwidth_out += $out;
					}
				}
				
				#Process bandwidth logs from all PS's(this will handle single host using multiple PS)
				foreach my $ps_guid(@ps_guids)
				{
					my $bandwidth_txt = "/home/svsystems/".$host_id."/".$ps_guid."-bandwidth.txt";
					$bandwidth_txt = Utilities::makeLinuxPath($bandwidth_txt,$os_flag);
					#$logging_obj->log("EXCEPTION","update_bandwidth_rrd :: bandwidth_txt - $bandwidth_txt ");
					if (-s $bandwidth_txt)
					{
						if (!open(LOG, "<$bandwidth_txt"))
						{
							print ("Could not open $bandwidth_txt for reading\n");
							next;
						}
						else
						{
							@bandwidth_lines = <LOG>;
						}
						close LOG;
						unlink ($bandwidth_txt);
						my $num_data_points = @bandwidth_lines;
						
						foreach my $bandwidth_data(@bandwidth_lines)
						{
							chomp $bandwidth_data;
							$bandwidth_data =~ s/\s+/ /g;	# Compress whitespace
							$bandwidth_data =~ s/^\s+//;	# Strip leading whitespace
							$bandwidth_data =~ s/\s+$//;	# Strip trailing whitespace
							#$logging_obj->log("DEBUG","update_bandwidth_rrd :: bandwidth_txt - bandwidth_txt, bandwidth_data - $bandwidth_data ");
							my ($in, $out) = split(/:/, $bandwidth_data);
							
							$bandwidth_in += $in;
							$bandwidth_out += $out;
						}
					}
				}
				my $current_time = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
				my $current_date = Utilities::unixTimestampToDate($current_time);
				
				if($bandwidth_in > 0 || $bandwidth_out > 0)
				{
					$bandwidth_str = "$current_time:$bandwidth_in:$bandwidth_out";
				}
				else
				{
					$bandwidth_str = "$current_time:0:0";
				}
			
				#
				# Creation of bandwidth.rrd, Check if exists, unless create it
				#
				my $bandwidth_rrd = "/home/svsystems/".$host_id."/bandwidth.rrd";
				
				my $new_os_flag = TimeShotManager::get_osFlag($conn,$host_id);
				$bandwidth_rrd = Utilities::makeLinuxPath($bandwidth_rrd,$new_os_flag);

				my @rrdcmd = ();
				my $flag = 0;
				unless(-e "$bandwidth_rrd")
				{
					push @rrdcmd, "$bandwidth_rrd";
					push @rrdcmd, "--start=N-600";
					push @rrdcmd, "--step=300";
					push @rrdcmd, "DS:in:GAUGE:86400:U:U";
					push @rrdcmd, "DS:out:GAUGE:86400:U:U";
					push @rrdcmd, "RRA:AVERAGE:0.5:1:105120";
					
					RRDs::create (@rrdcmd);
					my $ERROR = RRDs::error;
					
					if ($ERROR)
					{
						$logging_obj->log("EXCEPTION","Unable to create $bandwidth_rrd: $ERROR ");
						next;
						#return 0;
					}
					$flag = 1;
				}
				if($flag == 1)
				{
				   @rrdcmd = ();
				   $flag = 0;
				}
		
				if($bandwidth_str ne '')
				{
					my $check_sql = "SELECT 
										  reportDate,
										  dataIn,
										  dataOut
									  FROM 
										  bandwidthReport 
									  WHERE 
										  hostId='$host_id'
									  AND 
										  reportDate = '$current_date'";
					
					my $rs = $conn->sql_exec($check_sql);

					my $update_sql;
					
					if(defined $rs)
					{
						my @value = @$rs;

						my $dataIn = $value[0]{'dataIn'};
						my $dataOut = $value[0]{'dataOut'};
						$bandwidth_in = $bandwidth_in+$dataIn;
						$bandwidth_out = $bandwidth_out+$dataOut;
						
						$update_sql = "UPDATE 
											bandwidthReport
										SET
											dataIn ='$bandwidth_in',
											dataOut ='$bandwidth_out' 
										WHERE 
											hostId='$host_id' 
										AND 
											reportDate = '$current_date'";
						
					}
					else
					{
						$update_sql = "INSERT 
									INTO 
										bandwidthReport
										(hostId,
										reportDate,
										dataIn,
										dataOut) 
									VALUES 
										('$host_id',
										'$current_date',
										'$bandwidth_in',
										'$bandwidth_out')";
					}
					#print "update_sql - ".$update_sql;
					my $insert_sth =  $conn->sql_query($update_sql);
					#$logging_obj->log("DEBUG","update_bandwidth_rrd :: bandwidth_rrd - $bandwidth_rrd, bandwidth_str : $bandwidth_str ");
					push @rrdcmd, "$bandwidth_rrd";
					push @rrdcmd, "$bandwidth_str";
					RRDs::update ( @rrdcmd );
					my $ERROR = RRDs::error;

					if($ERROR)
					{
						$logging_obj->log("EXCEPTION","Unable to update $bandwidth_rrd: $ERROR");
						next;
						#return 0;
					}
					
					if($rx_ip ne "")
					{
						my $bandwidth_rx_txt = "/home/svsystems/".$host_id."/bandwidth_rx.txt";
						my $os_flag = TimeShotManager::get_osFlag($conn,$host_id);
						$bandwidth_rx_txt = Utilities::makeLinuxPath($bandwidth_rx_txt,$os_flag);
						
						open(LOGFILE, ">>$bandwidth_rx_txt") || $logging_obj->log("DEBUG","Could not open $bandwidth_rx_txt ");
						print LOGFILE $bandwidth_str."\n" ;
						close(LOGFILE);
					}
				}
				foreach my $ps_guid(@ps_guids)
				{
					my $bandwidth_txt = "/home/svsystems/".$host_id."/".$ps_guid."-bandwidth.txt";
					if(-e $bandwidth_txt)
					{
						unlink ($bandwidth_txt);
					}
				}
			
			}

		}
	};
	if ($@)
    {
		$logging_obj->log("EXCEPTION","12 : Failed to connect to DB from ProcessServer::Trending::BandwidthTrends: $@");
    }
}

1;
