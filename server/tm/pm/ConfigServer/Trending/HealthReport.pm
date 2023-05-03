#$Header: /src/server/tm/pm/ConfigServer/Trending/HealthReport.pm,v 1.29 2015/12/19 11:38:40 prgoyal Exp $
#================================================================= 
# FILENAME
#   Reports.pm 
#
# DESCRIPTION
#    This perl module is responsible for the reporting 
#    section of trending. 
#    
# HISTORY
#     16 June 2008  chpadh    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ConfigServer::Trending::HealthReport;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
#use lib qw( /home/svsystems/rrd/lib/perl/ /home/svsystems/pm );
use DBI();
use TimeShotManager;
use Utilities;
use POSIX;
use HTTP::Date;
use Common::Log;
use List::Util qw(max);
use List::Util qw(min);
use Data::Dumper;
use tmanager;
use RRDs;

my $logging_obj = new Common::Log();

my $LV_TBL                           = "logicalVolumes";
my $LV_deviceName                    = "deviceName";
my $LV_dpsId                         = "dpsId";
my $LV_farLineProtected              = "farLineProtected";


my $SRC_DST_LV_TBL                   ="srcLogicalVolumeDestLogicalVolume";
my $SRC_DST_LV_sourceHostId          = "sourceHostId";
my $SRC_DST_LV_sourceDeviceName      = "sourceDeviceName";
my $SRC_DST_LV_destinationHostId     = "destinationHostId";
my $SRC_DST_LV_destinationDeviceName = "destinationDeviceName";

my $DPS_deviceName                   = "deviceName";
my $DPS_LV_TBL                       = "dpsLogicalVolumes";
my $DPS_id                           = "id";

my $HOSTS_id                         = "id";

my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();

my $BASE_DIR = "$lib_path/home/svsystems";
my $DB_TYPE = $AMETHYST_VARS->{'DB_TYPE'};
my $DB_NAME = $AMETHYST_VARS->{"DB_NAME"};
my $DB_HOST = $AMETHYST_VARS->{"PS_CS_IP"};
my $DB_USER = $AMETHYST_VARS->{"DB_USER"};
my $DB_PASSWD = $AMETHYST_VARS->{"DB_PASSWD"};
my $CS_IP = $AMETHYST_VARS->{"CS_IP"};
my $cx_type = $AMETHYST_VARS->{"CX_TYPE"};
my $baseDir = $AMETHYST_VARS->{"INSTALLATION_DIR"};

sub new 
{
    my ($class, %args) = @_; 
    my $self = {};

    #
    # Initialize all the required variables 
    #
    my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
    $self->{'DPSO_srchostid'} = $args{'srchostid'};
    $self->{'DPSO_dsthostid'} = $args{'dsthostid'};
    $self->{'DPSO_srcvol'} = $args{'srcvol'};
    $self->{'DPSO_dstvol'} = $args{'dstvol'};
    $self->{'DPSO_conn'} = $args{'conn'};
    $self->{'DPSO_basedir'} = $args{'basedir'};
	$self->{'source_failure_threshold'} = $AMETHYST_VARS->{"SOURCE_FAILURE"};

    bless ($self, $class);
}

#
# Populates "policyViolationEvents" table based upon inputs
#
sub policyViolation
{
    my($obj, $eventType, $eventFlag) = @_;
	my ($sql, $sth, $ref);
	
    if(!$obj->checkPrimaryPair()) { return 0; }
	
	$sql = "SELECT now() as eventTime";

    $sth = $obj->{'DPSO_conn'}->sql_query($sql);
	
	$ref = $obj->{'DPSO_conn'}->sql_fetchrow_hash_ref($sth);
	my $eventTime = $ref->{'eventTime'};
    
	my $list = $obj->getProtectionHosts();
	
	while ( my ($srchostid, $srcvol) = each(%{$list}) )
	{	
	    my $source_vol = TimeShotManager::formatDeviceName($srcvol);
	    my $dest_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_dstvol'});
	    
	    $sql = "SELECT 
	                startTime 
	            FROM 
	                policyViolationEvents 
	            WHERE
	                sourceHostId='$srchostid' AND
	                 sourceDeviceName='$source_vol' AND
	                destinationHostId='$obj->{DPSO_dsthostid}' AND
	                 destinationDeviceName='$dest_vol' AND
	                startTime!='0000-00-00 00:00:00' AND
	                eventType='$eventType'";
	
		$sth = $obj->{'DPSO_conn'}->sql_query($sql);
	  

	    $ref = $obj->{'DPSO_conn'}->sql_fetchrow_hash_ref($sth);
	    #
	    # Check if the event exists
	    #
	    if($eventFlag == 1)
	    {
	   
            if($obj->{'DPSO_conn'}->sql_num_rows($sth) == 0)
	        {
	            $sql = "SELECT 
	                        startTime 
	                    FROM
	                        policyViolationEvents 
	                    WHERE
	                        sourceHostId='$srchostid' AND
	                         sourceDeviceName='$source_vol' AND
	                        destinationHostId='$obj->{DPSO_dsthostid}' AND
	                         destinationDeviceName='$dest_vol' AND
	                        eventDate = CURDATE() AND
	                        startTime='0000-00-00 00:00:00' AND
	                        eventType='$eventType'";
	            
                $sth = $obj->{'DPSO_conn'}->sql_query($sql);
	            $ref = $obj->{'DPSO_conn'}->sql_fetchrow_hash_ref($sth);
	            if($obj->{'DPSO_conn'}->sql_num_rows($sth) == 0)
	            {
	                $sql = "INSERT 
	                        INTO 
	                            policyViolationEvents (
	                            sourceHostId,
	                            sourceDeviceName,
	                            destinationHostId,
	                            destinationDeviceName,
	                            eventDate,startTime,
	                            eventType,
	                            numOccurrence)
	                        values 
	                            ('$srchostid', 
	                            '$source_vol', 
	                            '$obj->{DPSO_dsthostid}', 
	                            '$dest_vol', 
	                            CURDATE(), 
	                            now(), 
	                            '$eventType',
	                            '1')" ;
	              
                    $sth = $obj->{'DPSO_conn'}->sql_query($sql);
	            }
	            else
	            {
	                $sql = "UPDATE 
	                            policyViolationEvents 
	                        SET 
	                            startTime=now(),
	                            numOccurrence=numOccurrence+1 
	                        WHERE 
	                            sourceHostId='$srchostid' AND
	                             sourceDeviceName='$source_vol' AND
	                            destinationHostId='$obj->{DPSO_dsthostid}' AND
	                             destinationDeviceName='$dest_vol' AND
	                            eventDate = CURDATE() AND
	                            startTime='0000-00-00 00:00:00' AND
	                            eventType='$eventType'";
	              
					$sth = $obj->{'DPSO_conn'}->sql_query($sql);
	            }
	        }
			else
			{
				#
				# Check if the event crosses the day boundary
				# If so, complete the previous record and create new open record for current date
				#	
				
				my $event_date = Utilities::datetimeToDate($ref->{'startTime'});
				my $current_date = Utilities::datetimeToDate($eventTime);
				
				if($event_date ne $current_date)
				{
		            my $days = &getDays($ref->{'startTime'}, $eventTime);
		            
		            while ( my ($tmp_st_time, $tmp_en_time) = each(%{$days}) ) 
		            {        
		                
						my ($event_start, $event_duration);
						$event_date = Utilities::datetimeToDate($tmp_st_time);
						
						if($event_date eq $current_date)
						{
							$event_start = $event_date.' 00:00:00';
							$event_duration = 0;
						}
						else
						{
							$event_start = '0000-00-00 00:00:00';
							$event_duration = "(unix_timestamp('$tmp_en_time') - unix_timestamp('$tmp_st_time'))";
						}
							
						$sql = "SELECT 
		                            count(*) as num_entries 
		                        FROM 
		                            policyViolationEvents 
		                        WHERE
		                            sourceHostId='$srchostid' AND
		                             sourceDeviceName='$source_vol' AND
		                            destinationHostId='$obj->{DPSO_dsthostid}' AND
		                             destinationDeviceName='$dest_vol' AND
		                            eventDate='$event_date' AND
		                            eventType='$eventType'" ;
						
                        $sth = $obj->{'DPSO_conn'}->sql_query($sql);
		                $ref = $obj->{'DPSO_conn'}->sql_fetchrow_hash_ref($sth);        
		                if($ref->{'num_entries'} > 0)
		                {
		                    $sql = "UPDATE 
		                                policyViolationEvents 
		                            SET 
		                                duration=(duration + $event_duration), 
		                                startTime='$event_start' 
		                            WHERE
		                                sourceHostId='$srchostid' AND
		                                 sourceDeviceName='$source_vol' AND
		                                destinationHostId='$obj->{DPSO_dsthostid}' AND
		                                 destinationDeviceName='$dest_vol' AND
		                                eventDate='$event_date' AND
		                                eventType='$eventType'";
		                }
		                else
		                {							
							$sql = "INSERT 
		                            INTO 
		                                policyViolationEvents (
		                                sourceHostId,
		                                sourceDeviceName,
		                                destinationHostId,
		                                destinationDeviceName,
		                                eventDate,
		                                startTime,
		                                duration,
		                                eventType,
		                                numOccurrence) 
		                            VALUES 
		                                ('$srchostid',
		                                '$source_vol',
		                                '$obj->{DPSO_dsthostid}',
		                                '$dest_vol',
		                                '$event_date',
		                                '$event_start', 
		                                $event_duration,
		                                '$eventType',
		                                '1')";
		                }
		                
						
						$sth = $obj->{'DPSO_conn'}->sql_query($sql);
		            }
				}
			}			
	    }
	    else
	    {
	      
            if($obj->{'DPSO_conn'}->sql_num_rows($sth) != 0)
	        {
	            my $event_date;
				my $days = &getDays($ref->{'startTime'}, $eventTime);
	            
	            while ( my ($tmp_st_time, $tmp_en_time) = each(%{$days}) ) 
	            {        
	                $event_date = Utilities::datetimeToDate($tmp_st_time);
					$sql = "SELECT 
	                            count(*) as num_entries 
	                        FROM 
	                            policyViolationEvents 
	                        WHERE
	                            sourceHostId='$srchostid' AND
	                             sourceDeviceName='$source_vol' AND
	                            destinationHostId='$obj->{DPSO_dsthostid}' AND
	                             destinationDeviceName='$dest_vol' AND
	                            eventDate='$event_date' AND
	                            eventType='$eventType'" ;
	              
                    $sth = $obj->{'DPSO_conn'}->sql_query($sql);
	                      
	                $ref = $obj->{'DPSO_conn'}->sql_fetchrow_hash_ref($sth); 
	                
	                if($ref->{'num_entries'} > 0)
	                {
	                    $sql = "UPDATE 
	                                policyViolationEvents 
	                            SET 
	                                duration=(duration+(unix_timestamp('$tmp_en_time') - 
	                                unix_timestamp('$tmp_st_time'))), 
	                                startTime='0000-00-00 00:00:00' 
	                            WHERE
	                                sourceHostId='$srchostid' AND
	                                 sourceDeviceName='$source_vol' AND
	                                destinationHostId='$obj->{DPSO_dsthostid}' AND
	                                 destinationDeviceName='$dest_vol' AND
	                                eventDate='$event_date' AND
	                                eventType='$eventType'";
	                }
	                else
	                {
	                    $sql = "INSERT 
	                            INTO 
	                                policyViolationEvents (
	                                sourceHostId,
	                                sourceDeviceName,
	                                destinationHostId,
	                                destinationDeviceName,
	                                eventDate,
	                                startTime,
	                                duration,
	                                eventType,
	                                numOccurrence) 
	                            VALUES 
	                                ('$srchostid',
	                                '$source_vol',
	                                '$obj->{DPSO_dsthostid}',
	                                '$dest_vol',
	                                '$event_date',
	                                '0000-00-00 00:00:00', 
	                                (unix_timestamp('$tmp_en_time') - 
	                                unix_timestamp('$tmp_st_time')), 
	                                '$eventType',
	                                '1')";
	                }
	                
	               
					$sth = $obj->{'DPSO_conn'}->sql_query($sql);
	            }            
	        }
	    }
	}
	
	#my $delete_policy_violation_sql = "DELETE FROM policyViolationEvents WHERE eventDate < CURDATE()";
	#$obj->{'DPSO_conn'}->sql_query($delete_policy_violation_sql);
	
	return 1;
}

#
# This subroutine creates and maintains the retention rrd file
#
sub updateRetentionRrd
{
    my($obj) = @_;
    if(!$obj->checkPrimaryPair()) { return 0; }
    
    my $source_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_srcvol'});
    my $sql;
    $sql = "SELECT 
                sourceHostId,
                sourceDeviceName,
                destinationHostId,
                destinationDeviceName,
                logsFrom,
                logsTo,
                pairConfigureTime
            FROM 
                srcLogicalVolumeDestLogicalVolume 
            WHERE 
                sourceHostId='$obj->{DPSO_srchostid}' AND 
                 sourceDeviceName='$source_vol'
            LIMIT 1";
  
	my $result = $obj->{'DPSO_conn'}->sql_exec($sql);

	my @value = @$result;

	my $pairConfigureTime = $value[0]{'pairConfigureTime'};
	my $logsFrom = $value[0]{'logsFrom'};
	my $logsTo = $value[0]{'logsTo'};
	if($pairConfigureTime eq '0000-00-00 00:00:00')
	{
		$sql = "UPDATE  
					srcLogicalVolumeDestLogicalVolume
				SET 
					pairConfigureTime=now()
				WHERE 
					sourceHostId='$obj->{DPSO_srchostid}' AND 
					 sourceDeviceName='$source_vol'";
	 
		my $sth = $obj->{'DPSO_conn'}->sql_query($sql);
	}
		
	my @arrFrom = split(/,/,$logsFrom);        
	my @arrTo = split(/,/,$logsTo);

	my $unixtime_to = mktime ($arrTo[5], $arrTo[4], $arrTo[3], $arrTo[2], 
	$arrTo[1]-1, $arrTo[0]-1900);
	my $unixtime_from = mktime ($arrFrom[5], $arrFrom[4], $arrFrom[3], 
	$arrFrom[2], $arrFrom[1]-1, $arrFrom[0]-1900);
	
	my $depth = $unixtime_to - $unixtime_from;

	my $command;
	
	my $host = $obj->{'DPSO_srchostid'};
	my $device = $obj->{'DPSO_srcvol'};
	
	#
	# Creation of retention.rrd
	#
	my $retention_rrd = $obj->{'DPSO_basedir'}."/".$host."/".$device."/retention.rrd";
	my $os_flag = TimeShotManager::get_osFlag($obj->{'DPSO_conn'},$host);
	$retention_rrd = Utilities::makePath($retention_rrd);
	#
	# Check if exists, unless create it
	#
	unless(-e "$retention_rrd")
	{
		my @rrd_cmd = ();
		
		push @rrd_cmd, $retention_rrd;
		push @rrd_cmd, "--start=N";
		push @rrd_cmd, "--step=60";
		push @rrd_cmd, "DS:retention:GAUGE:86400:U:U";
		push @rrd_cmd, "RRA:AVERAGE:0.5:1:525600";
		
		RRDs::create (@rrd_cmd);
		my $ERROR = RRDs::error;
		
		if($ERROR)
		{
		   $logging_obj->log("EXCEPTION","unable to create $retention_rrd: $ERROR\n"); 
		   return 0;			   
		}
					
	}
	
	#
	# Update rrd with values
	#
	my @rrdcmd = ();
	push @rrdcmd, $retention_rrd;
	push @rrdcmd, "N:$depth";
	RRDs::update ( @rrdcmd );
	my $ERROR = RRDs::error;
	
	if ($ERROR) 
	{ 
		$logging_obj->log("EXCEPTION","unable to update $retention_rrd: $ERROR\n");
		return 0;
	}
	return 1;
}

#
# This subroutine creates and maintains the Perf txt 
#
sub updatePerfTxt
{
	my($obj, $uncompressed_size, $compressed_size, $sourceHostId, $sourceDeviceName, $destinationHostId, $destinationDeviceName, $src_os_flag, $one_to_many) = @_;
	
	if(!$one_to_many) { return 0; }

	# Creation of perf.txt
	my $perf_txt = $obj->{'DPSO_basedir'}."/".$sourceHostId."/".$sourceDeviceName."/perf.txt";
	#$perf_txt = Utilities::makeLinuxPath($perf_txt,$src_os_flag);
	$perf_txt = Utilities::makePath($perf_txt);
	
	open(LOGFILE, ">>$perf_txt") || $logging_obj->log("EXCEPTION","Could not open $perf_txt ");
	my $entry   = $uncompressed_size.",".$compressed_size."\n" ;
	print LOGFILE $entry;
	close(LOGFILE);
    
    return 1;    
}

sub updatePerfPsTxt()
{
    my($pair_array) = @_;
    
	my $DELIMITER = ",";
	foreach my $key (keys %{$pair_array})
	{
		my $one_to_many = $pair_array->{$key}->{'one_to_many'};
		my $os_flag = $pair_array->{$key}->{'os_flag'};
		my $host = $pair_array->{$key}->{'srchostid'};
		my $device = $pair_array->{$key}->{'src_vol'};
		my $unix_time = $pair_array->{$key}->{'unix_time'};
		
		if(!$one_to_many) { next; }		
		
		# Creation of perf.txt
		my $perf_txt = $BASE_DIR."/".$host."/".$device."/perf.txt";
		$perf_txt = Utilities::makePath($perf_txt);
		my(@perf_log_lines, $entry);

		if (!-s $perf_txt) 
		{ 
			next;
		}
		if (!open (INPUT, "<$perf_txt" )) 
		{
			$logging_obj->log("EXCEPTION","Could not open $perf_txt ");
			next;
		}
		else
		{
			@perf_log_lines = <INPUT>;
			close INPUT;
			unlink ($perf_txt);
			
			my $uncompressed_total = 0;
			my $compressed_total = 0;
			foreach my $line(@perf_log_lines)
			{
				my ($uncompressed, $compressed) = split(/$DELIMITER/, $line);

				$uncompressed =~ s/\s+/ /g;	# Compress whitespace
				$uncompressed =~ s/^\s+//;	# Strip leading whitespace
				$uncompressed =~ s/\s+$//;	# Strip trailing whitespace

				$compressed =~ s/\s+/ /g; # Compress whitespace
				$compressed =~ s/^\s+//;	# Strip leading whitespace
				$compressed =~ s/\s+$//;	# Strip trailing whitespace

				$uncompressed_total += $uncompressed;
				$compressed_total += $compressed;
			}
			
			my $perf_ps_txt = $BASE_DIR."/".$host."/".$device."/perf_ps.txt";			
			$perf_ps_txt = Utilities::makePath($perf_ps_txt);
			open (OUTPUT, ">>$perf_ps_txt") || $logging_obj->log("EXCEPTION","Could not open $perf_ps_txt ");
			
			# Store the CS Timestamp in perf_ps.txt file
			$entry = "$unix_time:$uncompressed_total,$compressed_total\n" ;
			print OUTPUT $entry;
			close OUTPUT;
		} 
	}
    return 1;    
}

sub updateRpoTxt
{
    my($current_rpo,$src_id,$srcvol,$os_flag, $data_hash, $pair_id) = @_;
	my $one_to_many = $data_hash->{'pair_data'}->{$pair_id}->{'oneToManySource'};

    if(!$one_to_many) { return 0; }
    
	# Creation of rpo.txt
	my $rpo_txt = $BASE_DIR."/".$src_id."/".$srcvol."/rpo.txt";
	$rpo_txt = Utilities::makePath($rpo_txt,$os_flag);
	
	open(LOGFILE, ">>$rpo_txt") || $logging_obj->log("EXCEPTION","Could not open $rpo_txt ");
	my $entry   = $current_rpo."\n" ;
	print LOGFILE $entry;
	close(LOGFILE);
	
    return 1; 
}

#
# This subroutine creates and maintains the Perf_cs_txt file
#
sub updatePerfCsTxt
{
	my($obj) = @_;
    my $os_flag;
    if(!$obj->checkPrimaryPair()) { return 0; }
	$logging_obj->log("DEBUG","updatePerfCsTxt entered");
	my $command;
	my $DELIMITER = ",";
	my $host = $obj->{'DPSO_srchostid'};
	my $device = $obj->{'DPSO_srcvol'};

		my @perf_log_lines = "";
		my $last_update_time = 0;
		my $perf_ps_txt = $obj->{'DPSO_basedir'}."/".$host."/".$device."/perf_ps.txt";
		my $perf_cs_txt = $obj->{'DPSO_basedir'}."/".$host."/".$device."/perf_cs.txt";
		$os_flag = TimeShotManager::get_osFlag($obj->{'DPSO_conn'},$host);
        
        $perf_ps_txt = Utilities::makePath($perf_ps_txt);
		$perf_cs_txt = Utilities::makePath($perf_cs_txt);
        
		$logging_obj->log("DEBUG","updatePerfCsTxt perf_ps_txt - $perf_ps_txt, perf_cs_txt - $perf_cs_txt");
		
		if (!-s $perf_ps_txt)
		{
			next;
		}
		if (!open(INPUT, "<$perf_ps_txt"))
		{
			#print ("Could not open $perf_ps_txt for reading\n");
			$logging_obj->log("EXCEPTION","Could not open $perf_ps_txt for reading so exit from updatePerfRrd");
			return;
		}
		else
		{
			@perf_log_lines = <INPUT>;
			open(OUTPUT, ">>$perf_cs_txt");

			foreach my $line(@perf_log_lines)
			{
				$logging_obj->log("DEBUG","updatePerfCsTxt :: perf_ps_lines - $line");
				print OUTPUT $line;
			}
			close OUTPUT;
		}
		close INPUT;
		
		unlink ($perf_ps_txt);
    
    return 1; 

}

#
# This subroutine creates and maintains the RPO rrd file
#
sub updateRpoRrd
{
    my($obj) = @_;
    
    if(!$obj->checkPrimaryPair()) { return 0; }
    
	my ($command,$os_flag);
	my $host = $obj->{'DPSO_srchostid'};
	my $device = $obj->{'DPSO_srcvol'};
	
		my @rpo_lines = "";
		my $last_update_time = 0;
		my $rpo_rrd;
		my $rpo_txt = $obj->{'DPSO_basedir'}."/".$host."/".$device."/rpo.txt";

		$os_flag = TimeShotManager::get_osFlag($obj->{'DPSO_conn'},$host);

	    $rpo_txt = Utilities::makePath($rpo_txt);

		if (!-s $rpo_txt) 
		{ 
			return 0;
		}
		if (!open(LOG, "<$rpo_txt")) 
		{
			#print ("Could not open $rpo_txt for reading\n");
			$logging_obj->log("DEBUG","Could not open $rpo_txt for reading");
			return;
		}
		else
		{
			@rpo_lines = <LOG>;
		}
		close LOG;
		unlink ($rpo_txt);
		
		my $num_data_points = @rpo_lines;
		my $rpo_str = '';
		my $cnt = 1;
		my $dp_time;
		my $current_time = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
		#
	    # Creation of rpo.rrd
	    #
		
	    $rpo_rrd = $obj->{'DPSO_basedir'}."/".$host."/".$device."/rpo.rrd";
		$os_flag = TimeShotManager::get_osFlag($obj->{'DPSO_conn'},$host);
	    $rpo_rrd = Utilities::makePath($rpo_rrd,$os_flag);
		
		if (-e "$rpo_rrd")
		{
			$last_update_time = &get_rrd_info($rpo_rrd);
		}
		my @rrdcmd = ();
		push @rrdcmd, "$rpo_rrd";
		foreach my $current_rpo(@rpo_lines)
		{
			chomp $current_rpo;
			$current_rpo =~ s/\s+/ /g;	# Compress whitespace
			$current_rpo =~ s/^\s+//;	# Strip leading whitespace
			$current_rpo =~ s/\s+$//;	# Strip trailing whitespace
			
			$dp_time = $current_time - (($num_data_points - $cnt) * 60);
			if($dp_time gt $last_update_time)
			{
				$rpo_str = "$dp_time:$current_rpo";
				push @rrdcmd,$rpo_str;
			}
			
			$cnt++;
		}		

	    #
	    # Check if exists, unless create it
	    #
	    unless(-e "$rpo_rrd")
	    {
		    my @rrd_cmd = ();
		    push @rrd_cmd, "$rpo_rrd";
			push @rrd_cmd, "--start=N";
			push @rrd_cmd, "--step=60";
			push @rrd_cmd, "DS:rpo:GAUGE:86400:U:U";
			push @rrd_cmd, "RRA:AVERAGE:0.5:1:525600";
			push @rrd_cmd, "RRA:MAX:0.5:5:525600";
			
			RRDs::create (@rrd_cmd);
		    my $ERROR = RRDs::error;
	        if ($ERROR)
	        {
				$logging_obj->log("EXCEPTION","Unable to create $rpo_rrd: $ERROR ");
	            return 0;
	        }
	    }
	    # 
	    # Update rrd with values
	    #
		if(@rrdcmd)
		{
			RRDs::update ( @rrdcmd );
			my $ERROR = RRDs::error;
			if ($ERROR) 
			{
				$logging_obj->log("EXCEPTION","Unable to update $rpo_rrd: $ERROR ");
				return 0;
			}
		}
    
    return 1;    
}

#
# This subroutine creates and maintains the health rrd file
#
sub updateHealthRrd
{
    my($obj) = @_;
    
    if(!$obj->checkPrimaryPair()) { return 0; }
	my $sth;
    my $health = 0;
    my $rpo_flag = 0;
    my $throttle_flag = 0;
    my $retention_flag = 0;
    my $resync_flag = 0;
    my $datamode_flag = $obj->getDataMode();

    my $source_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_srcvol'});
    my $dest_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_dstvol'});
    
    my $sql;
	$sql = "SELECT 
                eventType 
            FROM 
                policyViolationEvents 
            WHERE
                sourceHostId='$obj->{DPSO_srchostid}' AND
                 sourceDeviceName='$source_vol' AND
                destinationHostId='$obj->{DPSO_dsthostid}' AND
                 destinationDeviceName='$dest_vol' AND
                startTime!='0000-00-00 00:00:00' AND
                eventDate = CURDATE()";
   
    my $result = $obj->{'DPSO_conn'}->sql_exec($sql);

	foreach my $ref (@{$result})
	{
        if($ref->{eventType} eq 'rpo') 
        { 
            $rpo_flag = 1; 
        }
        elsif($ref->{eventType} eq 'throttle')
        {
            $throttle_flag = 1; 
        }
        elsif($ref->{eventType} eq 'resync') 
        { 
            $resync_flag = 1;
        }
    }
	
    $sql = "SELECT 
                isCommunicationfromSource,
				resyncEndTime,
				restartResyncCounter
            FROM 
                srcLogicalVolumeDestLogicalVolume 
            WHERE 
                sourceHostId='$obj->{DPSO_srchostid}' AND 
                 sourceDeviceName='$source_vol' AND 
                destinationHostId='$obj->{DPSO_dsthostid}' AND 
                 destinationDeviceName='$dest_vol'";
    
    my $result1 = $obj->{'DPSO_conn'}->sql_exec($sql);

	my @value = @$result1;

	my $isCommunicationfromSource = $value[0]{'isCommunicationfromSource'};
	my $resyncEndTime = $value[0]{'resyncEndTime'};
	my $restartResyncCounter = $value[0]{'restartResyncCounter'};
	if($isCommunicationfromSource == 1) 
	{ 
		$rpo_flag = 1; 
	}
	if($resyncEndTime == '0000-00-00 00:00:00' && $restartResyncCounter > 0) 
	{
		$resync_flag = 1; 
	}
	
	
	#
	# For Profiler Pair, resync issue is not applicable
	#

	if($obj->{DPSO_dsthostid} eq '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' && $dest_vol eq 'P')
	{
		$resync_flag = 2;
	}	
	$sql = "SELECT 
                b.retId, 
                b.retPolicyType 
            FROM 
                srcLogicalVolumeDestLogicalVolume a, 
                retLogPolicy b 
            WHERE 
                a.sourceHostId='$obj->{DPSO_srchostid}' AND 
                 a.sourceDeviceName='$source_vol' AND 
                a.destinationHostId='$obj->{DPSO_dsthostid}' AND 
                 a.destinationDeviceName='$dest_vol' AND
                a.ruleId = b.ruleId";
    my $rs = $obj->{'DPSO_conn'}->sql_exec($sql);

	my $ret_window_policy = 0;
    if(defined $rs)
	{
		my @value1 = @$rs;

		my $policy_type = $value1[0]{'retPolicyType'};
		my $retId = $value1[0]{'retId'};
		
		if(($policy_type == 1) || ($policy_type == 2) )
		{
			my $ret_window ="SELECT 
								startTime,
								endTime
							FROM
								retentionWindow
							WHERE 
								retId = '$retId'";
			my $rs1 = $obj->{'DPSO_conn'}->sql_exec($ret_window);

			foreach my $ret_window_ref (@{$rs1})
			{
				my $start_time = $ret_window_ref->{'startTime'};
				my $end_time   = $ret_window_ref->{'endTime'};
				my $time       = ($end_time - $start_time);
				$ret_window_policy = ($ret_window_policy) + ($time*60*60);
			}
		}
		else
		{
			$ret_window_policy = 0;
		}
		
	    # my $tmp_log_days = $ref->{'ret_logupto_days'};
	    # my $tmp_log_hours = $ref->{'ret_logupto_hrs'};
	    # my $ret_window_policy = ($tmp_log_days*24*60*60) + ($tmp_log_hours*60*60);
	    
	    if($ret_window_policy > 0)
	    {
	        $sql = "SELECT 
	                    sourceHostId,
	                    sourceDeviceName,
	                    destinationHostId,
	                    destinationDeviceName,
	                    (unix_timestamp(now()) - unix_timestamp(quasidiffEndtime)) as pairDuration,
	                    logsFrom,
	                    logsTo
	                FROM 
	                    srcLogicalVolumeDestLogicalVolume 
	                WHERE 
	                    sourceHostId='$obj->{DPSO_srchostid}' AND
	                     sourceDeviceName='$source_vol'
	                LIMIT 1";
	       
            my $result2 = $obj->{'DPSO_conn'}->sql_exec($sql);
			
			my @value = @$result2;

			my $logsFrom = $value[0]{'logsFrom'};
			my $logsTo = $value[0]{'logsTo'};
			my @arrFrom = split(/,/,$logsFrom);        
			my @arrTo = split(/,/,$logsTo);
			my $pairDuration = $value[0]{'pairDuration'};
			
			if(@arrFrom)
			{
	       		 my $unixtime_to = mktime ($arrTo[5], $arrTo[4], $arrTo[3], $arrTo[2],
	        	$arrTo[1]-1, $arrTo[0]-1900);
	        	my $unixtime_from = mktime ($arrFrom[5], $arrFrom[4], $arrFrom[3],
	        	$arrFrom[2], $arrFrom[1]-1, $arrFrom[0]-1900);
	       	 	my $depth = $unixtime_to - $unixtime_from;
	        	
			
				my $degraded_policy_duration = (($ret_window_policy)*60)/100;  
		
				if($pairDuration >= $ret_window_policy)
				{ 
					if($depth lt $degraded_policy_duration)  #Available retention window is less than 60% of configured retention window
					{
					 $retention_flag = 1; # Retention flag 1 for degraded.
					}
					else
					{
						$retention_flag = 0; # Retention flag 0 for healthy.
					}
				}
				else
				{
				   $retention_flag = 0;
				}
			}
			else
			{
				$retention_flag = 2; # Retention flag 2 for not applicable.
			}
		}
		else
		{
			$retention_flag = 2; # Retention flag 2 for not applicable.
		}
	}
	else
	{
		$retention_flag = 2;
	}
    
    if($rpo_flag == 1 || $throttle_flag == 1 || $retention_flag == 1 || 
    $resync_flag == 1 || $datamode_flag == 1 || $datamode_flag == 2) { $health = 1; }

	my $command;

	my $host = $obj->{'DPSO_srchostid'};
	my $device = $obj->{'DPSO_srcvol'};
	
		#
	    # Creation of health.rrd
	    #
	    my $health_rrd = $obj->{'DPSO_basedir'}."/".$host."/".$device."/health.rrd";

		my $os_flag = TimeShotManager::get_osFlag($obj->{'DPSO_conn'},$host);

		$health_rrd = Utilities::makePath($health_rrd);
	    #
	    # Check if exists, unless create it
	    #
	    unless(-e "$health_rrd")
	    {
		    my @rrd_cmd = ();
		    push @rrd_cmd, "$health_rrd";
			push @rrd_cmd, "--start=N";
			push @rrd_cmd, "--step=60";
			push @rrd_cmd, "DS:health:GAUGE:86400:U:U";
			push @rrd_cmd, "DS:rpo:GAUGE:86400:U:U";
			push @rrd_cmd, "DS:throttle:GAUGE:86400:U:U";
			push @rrd_cmd, "DS:retention:GAUGE:86400:U:U";
			push @rrd_cmd, "DS:resync:GAUGE:86400:U:U";
			push @rrd_cmd, "DS:datamode:GAUGE:86400:U:U";
			push @rrd_cmd, "RRA:AVERAGE:0.5:1:525600";
			
			RRDs::create (@rrd_cmd);
		    my $ERROR = RRDs::error;
	        
	        if($ERROR)
	        { 
				$logging_obj->log("EXCEPTION","Unable to create $health_rrd: $ERROR ");
	            return 0;
	        }   			
	    }
		
		
	    #
	    # Update rrd with values
	    #
		my @rrdcmd = ();
		my $update_time = 'N:'.$health.':'.$rpo_flag.':'.$throttle_flag.':'.$retention_flag.':'.$resync_flag.':'.$datamode_flag;
		push @rrdcmd, "$health_rrd";
		push @rrdcmd, "$update_time";
		RRDs::update ( @rrdcmd );
	    my $ERROR = RRDs::error;

	    if ($ERROR) 
	    {
	        
			$logging_obj->log("EXCEPTION","Unable to update $health_rrd: $ERROR ");
	        return 0;
	    }
    
    return 1;    
}

#
# This subroutine returns the pairId
#
sub getPairId
{
    my($obj) = @_;
	
	my $source_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_srcvol'});
    my $dest_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_dstvol'});
	
    my $pair_id = $obj->{'DPSO_conn'}->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'pairId', "sourceHostId='$obj->{DPSO_srchostid}' AND sourceDeviceName='$source_vol' AND destinationHostId='$obj->{DPSO_dsthostid}' AND destinationDeviceName='$dest_vol'");
	return $pair_id;
}


#
# This subroutine checks if the pair is throttled and update PolicyViolationEvents table
#
sub checkThrottle
{
	my($obj) = @_;
	
	my ($sql, $sth, $row);

    my $source_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_srcvol'});
    my $dest_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_dstvol'});
	
	#
	# Check resync/diffsync throttle
	#
    $sql = "SELECT 
                throttleresync,
				throttleDifferentials,
				processServerId
            FROM 
                srcLogicalVolumeDestLogicalVolume 
            WHERE 
                sourceHostId='$obj->{DPSO_srchostid}' AND
                 sourceDeviceName='$source_vol' AND
                destinationHostId='$obj->{DPSO_dsthostid}' AND
                 destinationDeviceName='$dest_vol'";
	
    my $result = $obj->{'DPSO_conn'}->sql_exec($sql);

	my $throttle_resync = 0;
	my $throttle_differentials = 0;
	my $process_server_id = '';
	if (defined $result)
	{
		my @values = @$result;

		$throttle_resync = $values[0]{'throttleresync'};
		$throttle_differentials = $values[0]{'throttleDifferentials'};
		$process_server_id = $values[0]{'processServerId'};
	}
	
	#
	# Check Space Constarint
	#
	
	my $space_constraint = 0;
	$space_constraint = $obj->{'DPSO_conn'}->sql_get_value('processServer', 'cummulativeThrottle', "processServerId ='$process_server_id'");
	
	my $throttleFlag = ($throttle_resync || $throttle_differentials || $space_constraint) ? 1 : 0;	
	$obj->policyViolation('throttle', $throttleFlag);
}

#
# This subroutine checks if the pair is throttled and update PolicyViolationEvents table
#
sub getDataMode
{
	my($obj) = @_;
	
	my $datamode_flag = 0;

	my $datamode_file = $obj->{'DPSO_basedir'}."/".$obj->{DPSO_srchostid}."/".$obj->{'DPSO_srcvol'}."/datamode.txt";
	my $os_flag = TimeShotManager::get_osFlag($obj->{'DPSO_conn'},$obj->{DPSO_srchostid});
	$datamode_file = Utilities::makeLinuxPath($datamode_file,$os_flag);
	if (-e $datamode_file)
	{
		open(FH,"$datamode_file");
		my @file_content = <FH>;
		close (FH);
		
		my @data = split(/:/,$file_content[0]);
		
		my $agent_time = $data[0];
		my $cx_time = $data[1];
		$datamode_flag = $data[2];
		my $current_time = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
		
		#
		# It's overwrite the flag, if communication from Source is disconnected
		#
		if($current_time - $cx_time > 600)
		{
			$datamode_flag = -1;
		}
	}
	else
	{
		$datamode_flag = -1;
	}
		
	return $datamode_flag;
}

#
# Check if a pair is primary
#
sub checkPrimaryPair
{
    my($obj) = @_;
    
    my $source_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_srcvol'});
    my $dest_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_dstvol'});
    
	my $one_to_many = $obj->{'DPSO_conn'}->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'oneToManySource', "sourceHostId='$obj->{DPSO_srchostid}' AND sourceDeviceName='$source_vol' AND destinationHostId='$obj->{DPSO_dsthostid}' AND destinationDeviceName='$dest_vol' LIMIT 1");
	 
    return $one_to_many;
}

#
# Check if the host is a part of cluster, and 
# returns all the node information
#
sub getProtectionHosts
{
	my($obj) = @_;
	
	my $source_vol = TimeShotManager::formatDeviceName($obj->{'DPSO_srcvol'});

	my ($sql, $sth, $ref);
	my $host_list = {};

	my $num_entries = $obj->{'DPSO_conn'}->sql_get_value('clusters', 'count(*)', "hostId='$obj->{DPSO_srchostid}' AND deviceName='$source_vol'");
    
	if($num_entries > 0)
	{
		my $clusterId = $obj->{'DPSO_conn'}->sql_get_value('clusters', 'clusterId', "hostId='$obj->{DPSO_srchostid}' AND deviceName='$source_vol'");
		
		$sql = "SELECT 
					hostId,
					deviceName 
				FROM
					clusters
				WHERE
					 deviceName='$source_vol' AND
					clusterId = '$clusterId'";

		
        my $result = $obj->{'DPSO_conn'}->sql_exec($sql);

		foreach my $ref (@{$result})
		{
	        $host_list->{$ref->{hostId}} = $ref->{'deviceName'};
		}
	}
	else
	{
		$host_list->{$obj->{DPSO_srchostid}} = $obj->{'DPSO_srcvol'};
	}

	return $host_list;
}

#
# This subroutine returns the days between the supplied date range
#
sub getDays
{
    my ($dt1, $dt2) = @_;
    
    my $st_dt = Utilities::datetimeToUnixTimestamp($dt1);
    my $en_dt = Utilities::datetimeToUnixTimestamp($dt2);
    
    my $tmp_dt;
    
    #
    # If start date is greater than end date, swap the values
    #
    if($en_dt < $st_dt)
    {
        $tmp_dt = $en_dt;
        $en_dt = $st_dt;
        $st_dt = $tmp_dt;
    }

    my $dates = {};
    my $i;
    for($i = $st_dt; $i <= $en_dt; $i += 43200)
    {
        $tmp_dt = Utilities::unixTimestampToDate($i);
        $dates->{$tmp_dt} = 1;
    }
    
    $tmp_dt = Utilities::unixTimestampToDate($en_dt);
    $dates->{$tmp_dt} = 1;
    
    my @tmp_dates = keys(%{$dates});

    my $new_dt = {};
    my ($start, $end);
    foreach my $date (@tmp_dates)
    {
        if(Utilities::unixTimestampToDate($st_dt) eq $date &&
        Utilities::unixTimestampToDate($en_dt) eq $date)
        {
            $start = Utilities::unixTimestampToDatetime($st_dt);
            $end = Utilities::unixTimestampToDatetime($en_dt);
        }        
        elsif(Utilities::unixTimestampToDate($st_dt) eq $date)
        {
            $start = Utilities::unixTimestampToDatetime($st_dt);
            $end = Utilities::unixTimestampToDate($st_dt).' 23:59:59';
        }
        elsif(Utilities::unixTimestampToDate($en_dt) eq $date)
        {
            $start = Utilities::unixTimestampToDate($en_dt).' 00:00:00';
            $end = Utilities::unixTimestampToDatetime($en_dt);
        }
        else
        {
            $start = $date.' 00:00:00';
            $end = $date.' 23:59:59';
        }
        
        $new_dt->{$start} = $end;
    }        

    return $new_dt;
}
#
# This function fetch the data from rrd file and 
# and store it in  DB
#
sub updateHealthReportData 
{    
	eval
	{
		my ($obj,$conn1) = @_;
		my $conn = $obj->{'DPSO_conn'};
		my ($query_type,$date,@only_date,$internal_date_value,$count,$key,$value,$start_time,$end_time,$sourceDeviceName) ;
		my (%result_hour,%result,@pair_details);
		
		#
		# Fetch the all Source info from srcLogicalVolumeDestLogicalVolume
		#
		my $sql = "SELECT 
					   sourceHostId ,
					   sourceDeviceName ,
					   destinationHostId ,
					   destinationDeviceName,
					   DATE_FORMAT(pairConfigureTime,'%Y-%m-%d %H:00:00') as pairConfigureTime,
					   pairId,
					   planId,
					   oneToManySource,
					   isResync,
					   isQuasiFlag,
					   restartResyncCounter
					FROM
						 srcLogicalVolumeDestLogicalVolume
					ORDER BY oneToManySource,sourceHostId";
		my $result1 = $conn->sql_exec($sql);

		foreach my $ref (@{$result1})
		{
			$sourceDeviceName = TimeShotManager::formatDeviceName($ref->{'sourceDeviceName'});
			
			my $reportDate = $conn->sql_get_value('healthReport', 'max(reportDate)', "sourceHostId=\"$ref->{'sourceHostId'}\" AND sourceDeviceName='$sourceDeviceName'");
			push @pair_details, { destinationHostId => "$ref->{'destinationHostId'}", 
								  destinationDeviceName => "$ref->{'destinationDeviceName'}", 
								  sourceHostId => "$ref->{'sourceHostId'}", 
								  sourceDeviceName => "$sourceDeviceName", 
								  pairConfigureTime => "$ref->{'pairConfigureTime'}",
								  pairId => "$ref->{'pairId'}",
								  planId => "$ref->{'planId'}",
								  oneToManySource => "$ref->{'oneToManySource'}",
								  isResync => "$ref->{'isResync'}",
								  isQuasiFlag => "$ref->{'isQuasiFlag'}",
								  restartResyncCounter => "$ref->{'restartResyncCounter'}",
								  reportDate =>  "$reportDate"};
		   
		}  
		
		my ($perf_command, $pair_key, $pair_value,$rpo_command,$retention_command,$health_command,$pair_creation_timestamp,$os_flag);
		for  $pair_key ( @pair_details ) 
		{ 
			%result =();
			%result_hour =();

			my ($total_uncompress_hours,$total_compress_hours,@current_hour,$today_date);
			$total_uncompress_hours =$total_compress_hours =0;
			my $source_hostId = $pair_key->{sourceHostId};
			my $source_deviceName =  $pair_key->{sourceDeviceName};
			my $tgt_deviceName = $pair_key->{destinationDeviceName};
			my $tgt_hostId = $pair_key->{destinationHostId};
			my $pair_creation_time = $pair_key->{pairConfigureTime};
			my $pair_id = $pair_key->{pairId};
			my $plan_id = $pair_key->{planId};
			my $one_to_many_source = $pair_key->{oneToManySource};
			my $is_resync = $pair_key->{isResync};
			my $is_quasi_flag = $pair_key->{isQuasiFlag};
			my $restart_resync_counter = $pair_key->{restartResyncCounter};
			my $reportDate = $pair_key->{reportDate};

			$os_flag = TimeShotManager::get_osFlag($obj->{'DPSO_conn'},$source_hostId);
			$pair_creation_timestamp = Utilities::datetimeToUnixTimestamp($pair_creation_time);
			$tgt_deviceName = TimeShotManager::formatDeviceName($tgt_deviceName);
			my $current_time = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
			my $current_time1 = Utilities::unixTimestampToDatetime($current_time);
			my @current_date = split(' ',$current_time1);
			my $last_updated_time = 0;

			if($reportDate)
			{ 
				$last_updated_time = $reportDate;
				my $time_from_db = $reportDate.' 00:00:00';
				$time_from_db = Utilities::datetimeToUnixTimestamp($time_from_db);
				$start_time = $time_from_db;
				$end_time = $current_time;  
			}
			else
			{
				$last_updated_time = 0;
				$start_time = $pair_creation_timestamp;
				$end_time = $current_time;   
			}
			my $diff_time = $end_time - $start_time;
			my $limit = 432000; # 5*86400 (86400 is time stamp diff b/w 2 days max difference should be of 5days)
			
			if($diff_time > $limit)
			{
			  $start_time = $end_time - $limit;
			}
			my $last_updated_hour = 0;
		 
			#
			# Fetch data from perf.rrd
			#
			my $perf_rrd_result = $obj->{'DPSO_basedir'}."/".'perf.txt';
			$perf_rrd_result = Utilities::makePath($perf_rrd_result);
			my $is_cluster = $obj->checkCluster($source_hostId);
			my $perf_rrd_path;
			#$perf_rrd_path = $obj->{'DPSO_basedir'}."/".$source_hostId."/".$source_deviceName."/perf.rrd";
			my $perf_cs_txt = $obj->{'DPSO_basedir'}."/".$source_hostId."/".$source_deviceName."/perf_cs.txt";
			
			$logging_obj->log("DEBUG","updateHealthReportData :: perf_cs_txt1 - $perf_cs_txt");
			$perf_cs_txt = Utilities::makePath($perf_cs_txt);			
			$logging_obj->log("DEBUG","updateHealthReportData :: perf_cs_txt2 - $perf_cs_txt");

			#$perf_rrd_path = Utilities::makeLinuxPath($perf_rrd_path,$os_flag);
			if(-e "$perf_cs_txt")
			{
				#&fetchRrd($perf_rrd_path,$perf_rrd_result,$start_time,$end_time);
				
				if (!open(LOG, "<".$perf_cs_txt)) 
				{
					$logging_obj->log("EXCEPTION","updateHealthReportData::Unable to open the perf .txt $! "); 
				}
				else
				{
					my $total_uncompress = 0;
					my $total_compress = 0;
					my $internal_hour_value = 0;
					my ($uncompress,$compress);
					my $DELIMITER = ",";

					while (my $line = <LOG>) 
					{
						chomp $line;
						#($date, $uncompress, $compress) = split(' ', $line);
						#($date) = split(':', $date);
						
						my ($date,$line) = split(":", $line);			
						my ($uncompress, $compress) = split(/$DELIMITER/, $line);

						$date = Utilities::unixTimestampToDatetime($date);
						my $current_time_hr = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
						$current_time_hr = Utilities::unixTimestampToDatetime($current_time);
						my @current_time_arr = split(' ',$current_time_hr); 
						@only_date = split(' ',$date);
						#if(@only_date[0] gt $internal_date_value)
						$logging_obj->log("DEBUG","updateHealthReportData::only_date - ".$only_date[0].", internal_date_value - $internal_date_value"); 
						if($only_date[0] gt $internal_date_value)
						{
							$result{$internal_date_value}{'uncompress'} = $total_uncompress;
							$result{$internal_date_value}{'compress'}  = $total_compress;
							$total_uncompress = 0;
							$total_compress =0;                      
						}
						if($uncompress > 0)
						{
							$total_uncompress += $uncompress;    
						}
						if($compress > 0)
						{
							$total_compress += $compress;
						}
						$internal_date_value = $only_date[0];
						
						if($only_date[0] eq $current_time_arr[0])
						{
							$today_date = $only_date[0];
							@current_hour = split(':',$only_date[1]);
							if($current_hour[0] gt $internal_hour_value)
							{
								$result_hour{$today_date}{$internal_hour_value}{'uncompress'} = $total_uncompress_hours;
								$result_hour{$today_date}{$internal_hour_value}{'compress'} = $total_compress_hours;
								$total_compress_hours = 0;
								$total_uncompress_hours =0;
							}
							if($uncompress > 0)
							{
								$total_uncompress_hours +=  $uncompress;
							}
							if($compress > 0)
							{
								$total_compress_hours += $compress;
							}
							$internal_hour_value = $current_hour[0];
							#PERLLOG("Compress data::$compress and uncompressed :$uncompress and total_com:$total_compress_hours and total uncom:$total_uncompress_hours\n   ")
						}
									   
					}
					
					#
					# Storing Data in hash
					#
					$result{$only_date[0]}{'uncompress'} = $total_uncompress;
					$result{$only_date[0]}{'compress'}  = $total_compress; 
					$result_hour{$today_date}{$current_hour[0]}{'uncompress'} = $total_uncompress_hours;
					$result_hour{$today_date}{$current_hour[0]}{'compress'} = $total_compress_hours;
					close LOG;
					if($one_to_many_source)
					{
						unlink($perf_cs_txt);
					}
				}
			}    
			#
			# Fetch data from rpo.rrd
			#
			my $rpo_rrd_result = $obj->{'DPSO_basedir'}."/".'rpo.txt';
			$rpo_rrd_result = Utilities::makePath($rpo_rrd_result);
			
			my $rpo_rrd_path = $obj->{'DPSO_basedir'}."/".$source_hostId."/".$source_deviceName."/rpo.rrd";
			$rpo_rrd_path = Utilities::makePath($rpo_rrd_path);        
			
			&fetchRrd($rpo_rrd_path,$rpo_rrd_result,$start_time,$end_time);
			if(-e "$rpo_rrd_result")
			{
				if (!open(LOG, "<".$rpo_rrd_result)) 
				{
				   
				  $logging_obj->log("EXCEPTION","updateHealthReportData::Unable to open the rpo.txt $!"); 
				}
				else
				{
					my ($rpo,$total_rpo,$total_rpo_hour);
					my $internal_hour_value = 0;
					$total_rpo = $total_rpo_hour = 0;
					
					while (my $line = <LOG>) 
					{
						chomp $line;
						($date, $rpo) = split(' ', $line);
						($date) = split(':', $date);
						my $current_time_hr = Utilities::datetimeToUnixTimestamp(HTTP::Date::time2iso());
						$current_time_hr = Utilities::unixTimestampToDatetime($current_time);
						my @current_time_arr = split(' ',$current_time_hr); 

						$date = Utilities::unixTimestampToDatetime($date);
						@only_date = split(' ',$date);
						if(( $rpo !~ 'nan') && ($rpo ne '-1.#IND000000e+000'))
						{
							if($rpo > $total_rpo)
							{
								$total_rpo = $rpo;
							}
						}
						if($only_date[0] gt $internal_date_value)
						{
							$result{$internal_date_value}{'rpo'} = $total_rpo;
							$total_rpo = 0;                    
						}
						$internal_date_value = $only_date[0];

						if($only_date[0] eq $current_time_arr[0])
						{
							$today_date = $only_date[0];
							@current_hour = split(':',$only_date[1]);

							if(( $rpo !~ 'nan') && ($rpo ne '-1.#IND000000e+000'))
							{
								if($rpo > $total_rpo_hour)
								{
									$total_rpo_hour = $rpo;
								}
							}

							if($current_hour[0] gt $internal_hour_value)
							{
								$result_hour{$today_date}{$internal_hour_value}{'rpo'} = $total_rpo_hour;
								$total_rpo_hour =0;
							}
							$internal_hour_value = $current_hour[0];
						}
					}
					$result{$only_date[0]}{'rpo'} = $total_rpo;
					$result_hour{$today_date}{$current_hour[0]}{'rpo'} = $total_rpo_hour;
					
					close LOG;
					unlink($rpo_rrd_result);
				}
			}	
			

			#
			# Fetch data from retention.rrd
			#        
			my $retention_rrd_result = $obj->{'DPSO_basedir'}."/".'retention.txt';
			$retention_rrd_result = Utilities::makePath($retention_rrd_result);
			
			my $retention_rrd_path = $obj->{'DPSO_basedir'}."/".$source_hostId."/".$source_deviceName."/retention.rrd";
			$retention_rrd_path = Utilities::makePath($retention_rrd_path);
			
			&fetchRrd($retention_rrd_path,$retention_rrd_result,$start_time,$end_time);
			
			if(-e "$retention_rrd_result")
			{
				if (!open(LOG, "<".$retention_rrd_result)) 
				{
				  
				  $logging_obj->log("EXCEPTION","updateHealthReportData::Unable to open the retention.txt $! "); 
				}
				else
				{
					my $total_retention = 0;
					$count = 0;
					$internal_date_value =0;
					my ($retention);
					while (my $line = <LOG>) 
					{
						chomp $line;
						($date, $retention) = split(' ', $line);
						($date) = split(':', $date);
						$date = Utilities::unixTimestampToDatetime($date);
						@only_date = split(' ',$date);
						if(( $retention !~ 'nan') && ($retention ne '-1.#IND000000e+000'))
						{
							$total_retention = $total_retention+$retention;
							$count++;						
						}
						if(($only_date[0] gt $internal_date_value) && ($internal_date_value != 0))
						{   
							if($count !=0)
							{
								$result{$internal_date_value}{'retention'} = ($total_retention/$count);
							}
							$total_retention = 0;
							$count = 0;                    
						}
						$internal_date_value = $only_date[0];    
					}
					if($count !=0)
					{
						$result{$only_date[0]}{'retention'} = ($total_retention/$count);
					}
					close LOG;
					unlink($retention_rrd_result);
				}
			}

			#
			# Fetch data from health.rrd
			#
			my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
			my $RPO_IN_PROTECTION_COVERAGE = $AMETHYST_VARS->{"RPO_IN_PROTECTION_COVERAGE"};
			my $RETENTION_IN_PROTECTION_COVERAGE = $AMETHYST_VARS->{"RETENTION_IN_PROTECTION_COVERAGE"};
			my $THROTTLE_IN_PROTECTION_COVERAGE = $AMETHYST_VARS->{"THROTTLE_IN_PROTECTION_COVERAGE"};
			my $RESYNC_IN_PROTECTION_COVERAGE = $AMETHYST_VARS->{"RESYNC_IN_PROTECTION_COVERAGE"};
			my $DATA_MODE_IN_PROTECTION_COVERAGE = $AMETHYST_VARS->{"DATA_MODE_IN_PROTECTION_COVERAGE"};
			
			my $health_rrd_path = $obj->{'DPSO_basedir'}."/".$source_hostId."/".$source_deviceName."/health.rrd";
			$health_rrd_path = Utilities::makePath($health_rrd_path);
			
			my $health_rrd_result = $obj->{'DPSO_basedir'}."/".'health.txt';
			$health_rrd_result = Utilities::makePath($health_rrd_result);
			
			&fetchRrd($health_rrd_path,$health_rrd_result,$start_time,$end_time);
			if(-e "$health_rrd_result")
			{
				if (!open(LOG, "<".$health_rrd_result)) 
				{
				   
				   $logging_obj->log("EXCEPTION","updateHealthReportData::Unable to open the health.txt $! ");  
				}
				else
				{
					my $total_retention = 0;
					$count = 0;
					my ($rpo_empty_flag,$rpo_value_flag,$throttle_empty_flag,$throttle_value_flag, $ret_empty_flag ,$ret_value_flag,$data_empty_flag,$data_value_flag,$health_flag,$rpo_flag,$throttle_flag,$ret_flag,$resync_flag,$data_mode,$total_protetcion,$health_rpo,$health_throttle,$health_ret,$health_data,$health_resync,$resync_empty_flag,$resync_value_flag,$health_rpo1,$health_throttle1,$health_ret1,$health_data1,$health_resync1);
					$rpo_empty_flag = $rpo_value_flag = $throttle_empty_flag = $throttle_value_flag = $ret_empty_flag = $ret_value_flag = $data_empty_flag = $data_value_flag = $health_rpo1 = $health_throttle1 = $health_ret1 = $health_data1 = $health_resync1 = $health_rpo = $health_throttle = $health_ret = $health_data = $health_resync = 0;
					while (my $line = <LOG>) 
					{
						chomp $line;
						($date, $health_flag,$rpo_flag,$throttle_flag,$ret_flag,$resync_flag,$data_mode) = split(' ', $line);
						
						($date) = split(':', $date);
						$date = Utilities::unixTimestampToDatetime($date);
						@only_date = split(' ',$date);
						
						if(( $rpo_flag !~ 'nan') && ($rpo_flag ne '-1.#IND000000e+000'))
						{
							if($rpo_flag == 0)
							{
								$rpo_empty_flag++;
							}
							else
							{
								$rpo_value_flag++;
							}
						}
						if(( $throttle_flag !~ 'nan') && ($throttle_flag ne '-1.#IND000000e+000'))
						{
							if($throttle_flag == 0)
							{
								$throttle_empty_flag++;
							}
							else
							{
								$throttle_value_flag++;
							}
						}
						if(( $ret_flag !~ 'nan') && ($ret_flag ne '-1.#IND000000e+000'))
						{
							if($ret_flag == 0 || $ret_flag == 2)
							{
								$ret_empty_flag++;
							}
							else
							{
								$ret_value_flag++;
							}
						}
						if(( $resync_flag !~ 'nan') && ($resync_flag ne '-1.#IND000000e+000'))
						{
							if($resync_flag == 0 || $resync_flag == 2)
							{
								$resync_empty_flag++;
							}
							else
							{
								$resync_value_flag++;
							}
						}
						if(( $data_mode !~ 'nan') && ($data_mode ne '-1.#IND000000e+000'))
						{
							if($data_mode == 0 || $data_mode == -1)
							{
								$data_empty_flag++;
							}
							else
							{
								$data_value_flag++;
							}
						}
						
						
							if(!($rpo_empty_flag == 0 &&  $rpo_value_flag == 0))
							{
							   $health_rpo = $rpo_empty_flag / ($rpo_empty_flag + $rpo_value_flag);
							}
							if(!($throttle_empty_flag == 0 &&  $throttle_value_flag == 0))
							{
								$health_throttle = $throttle_empty_flag / ($throttle_empty_flag + $throttle_value_flag);
							}
							if(!($ret_empty_flag == 0 &&  $ret_value_flag == 0))
							{
								$health_ret = $ret_empty_flag / ($ret_empty_flag + $ret_value_flag);
							}
							if(!($data_empty_flag == 0 &&  $data_value_flag == 0))
							{
								$health_data = $data_empty_flag / ($data_empty_flag + $data_value_flag);
							}
							if(!($resync_empty_flag == 0 &&  $resync_value_flag == 0))
							{
								 $health_resync = $resync_empty_flag /($resync_empty_flag + $resync_value_flag);
							}
							# Calculating the total Protection coverage including the rpo, retention,resync,data mode 
							# throttle with the given ratio.
							$health_rpo1 = $health_rpo * $RPO_IN_PROTECTION_COVERAGE;
							$health_throttle1 = $health_throttle * $THROTTLE_IN_PROTECTION_COVERAGE;
							$health_ret1 = $health_ret * $RETENTION_IN_PROTECTION_COVERAGE;
							$health_data1 = $health_data* $DATA_MODE_IN_PROTECTION_COVERAGE;
							$health_resync1 = $health_resync* $RESYNC_IN_PROTECTION_COVERAGE;
							$total_protetcion = ($health_rpo1 + $health_throttle1 + $health_ret1 + $health_data1 + $health_resync1);
							
						
						if($only_date[0] gt $internal_date_value)
						{
							$result{$internal_date_value}{'protection_coverage'} = ($total_protetcion);
							$result{$internal_date_value}{'health_rpo'} = ($health_rpo1);
							$result{$internal_date_value}{'health_throttle'} = ($health_throttle1);
							$result{$internal_date_value}{'health_ret'} = ($health_ret1);
							$result{$internal_date_value}{'health_data'} = ($health_data1);
							$result{$internal_date_value}{'health_resync'} = ($health_resync1);
							$rpo_empty_flag = $rpo_value_flag = $throttle_empty_flag = $throttle_value_flag = $ret_empty_flag = $ret_value_flag = $data_empty_flag = $data_value_flag =0;
						}
						$internal_date_value = $only_date[0];    

					}
					
					$result{$only_date[0]}{'protection_coverage'} = ($total_protetcion);
					$result{$only_date[0]}{'health_rpo'} = ($health_rpo1);
					$result{$only_date[0]}{'health_throttle'} = ($health_throttle1);
					$result{$only_date[0]}{'health_ret'} = ($health_ret1);
					$result{$only_date[0]}{'health_data'} = ($health_data1);
					$result{$only_date[0]}{'health_resync'} = ($health_resync1);
					close LOG;
					unlink($health_rrd_result);
				}
			}	
			
			#
			# To Clean the txt file , who comtains the rrd data
			#
			
			#To insert data into database (insert or update)
			my $dbh = $conn->get_database_handle();	
			$dbh->{'mysql_auto_reconnect'} = 1;
			$logging_obj->log("DEBUG","result_day  :: ".Dumper(\%result));
			while( ($key,$value) = each(%result))
			{
				if($key == '') { next; }
				my  $insert_sql;
				my $report_date = $key;
				my ($maxRpo,$compressed,$uncompressed,$retention,$protection_coverage,$health_rpo2,$health_throttle2,$health_ret2,$health_data2,$health_resync2);
				
				while(my ($ikey,$ival) = each (%$value))
				{
					if($ikey eq 'rpo')
					{
						$maxRpo = $ival;
					}
					elsif($ikey eq 'compress')
					{
						 $compressed = $ival;
					}
					elsif($ikey eq 'uncompress')
					{
						 $uncompressed = $ival;
					}
					elsif($ikey eq 'retention')
					{
						 $retention = $ival;
					}
					elsif($ikey eq 'health_rpo')
					{
						 $health_rpo2 = $ival;
					}
					elsif($ikey eq 'health_throttle')
					{
						 $health_throttle2 = $ival;
					}
					elsif($ikey eq 'health_ret')
					{
						 $health_ret2 = $ival;
					}
					elsif($ikey eq 'health_data')
					{
						 $health_data2 = $ival;
					}
					elsif($ikey eq 'health_resync')
					{
						 $health_resync2 = $ival;
					}
					elsif($ikey eq 'protection_coverage')
					{
						 $protection_coverage = $ival;
					}
				}
				 my $check_sql = "SELECT 
									  reportDate,
									  dataChangeWithCompression,
									  dataChangeWithoutCompression	 
								  FROM 
									  healthReport 
								  WHERE 
									  pairId='$pair_id'
								  AND 
									  reportDate = '$report_date'";

				my $result2 = $conn->sql_exec($check_sql);
				$logging_obj->log("DEBUG","updateHealthReportData :: check_sql - $check_sql");
				my $avaliable_record = '';
				
				if (defined $result2)
				{
					my @values = @$result2;
					$logging_obj->log("DEBUG","updateHealthReportData :: values - ".Dumper(@values));
					$avaliable_record =  $values[0]{'reportDate'};
					$compressed += $values[0]{'dataChangeWithCompression'};
					$uncompressed += $values[0]{'dataChangeWithoutCompression'};
				}
				if($avaliable_record ne '')
				{
					$query_type = 'update';
				}
				else
				{
					$query_type = 'insert';
				}
				
				my $tag_count;
				my $rpo_threshold;
				my $ret_id;
				my $strRetId;
				my $policy_type;
				$tag_count = 0;
							

				if(($report_date ne '') &&  ($report_date ne '1970-01-01'))
				{
					
					my $report_start_date = $report_date.'-00-00-00-000-000-000';
					my $report_end_date = $report_date.'-23-59-59-000-000-000';
					$report_start_date =~ tr/0-9/,/c;
					$report_end_date =~ tr/0-9/,/c;
					
					$tag_count = $conn->sql_get_value('retentionTag', 'COUNT(*)', "HostId='$tgt_hostId' and deviceName = '$tgt_deviceName' and paddedTagTimeStamp >= '$report_start_date' and paddedTagTimeStamp < '$report_end_date'");
					
				}
				$insert_sql = '';
							
				$rpo_threshold = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'rpoSLAThreshold', "sourceDeviceName = '$source_deviceName' and destinationDeviceName='$tgt_deviceName' and sourceHostId = '$source_hostId' and destinationHostId = '$tgt_hostId'");
				
				my $select_sql1 =  "SELECT
							ret.retId
						FROM
							retLogPolicy ret,
							srcLogicalVolumeDestLogicalVolume src
						WHERE
							ret.ruleid = src.ruleId
						AND
							src.destinationHostId = '$tgt_hostId'
						AND
							src.destinationDeviceName='$tgt_deviceName'";
				my $rs = $conn->sql_exec($select_sql1);
				my $x = 0;
				foreach my $check_ref1 (@{$rs})
				{
					if ($x == 0)
					{
						$strRetId = $check_ref1->{'retId'};
					}
					else
					{
						$strRetId = $strRetId.",".$check_ref1->{'retId'};
					}
					 
				   $x++;
					
				}
				
				$policy_type = $conn->sql_get_value('retLogPolicy', 'retPolicyType', "retId IN ('$strRetId')");
				my $startTime = 0;
					my $total_duration = 0;
					my $duration;
				if(($policy_type == 1) || ($policy_type == 2))
				{
					my $select_sql3 ="SELECT
								retwindow.Id,
								retwindow.startTime,
								retwindow.endTime
							FROM
								retentionWindow retwindow
							WHERE 
								retwindow.retId IN ($strRetId)";
					 
					my $result3 = $conn->sql_exec($select_sql3);

					my @value = @$result3;
					$startTime = $value[0]{'startTime'};
					my $endTime = $value[0]{'endTime'};
					$duration = $endTime-$startTime;
					$total_duration = ($total_duration + $duration);        
					
				}
				else
				{
					$total_duration = 0;
				}
				$total_duration = $total_duration / 24;
				
				
				if(($query_type eq 'insert') && (($report_date ne '')&&  ($report_date ne '1970-01-01')&& ($report_date ne '0000-00-00')&& ($report_date ne '0')))
				{
					$insert_sql = "INSERT 
									INTO 
										healthReport                    
										(sourceHostId,
										sourceDeviceName,
										destinationHostId,
										destinationDeviceName,
										reportDate,
										dataChangeWithCompression,
										dataChangeWithoutCompression,
										maxRPO,
										retentionWindow,
										protectionCoverage,
										rpoHealth,
										throttleHealth,
										retentionHealth,
										resyncHealth,
										replicationHealth,
										rpoTreshold,
										retention_policy_duration,
										availableConsistencyPoints,
										pairId,
										planId)
									VALUES
										('$source_hostId',
										'$source_deviceName',
										'$tgt_hostId',
										'$tgt_deviceName',
										'$report_date',
										'$compressed',
										'$uncompressed',
										'$maxRpo',
										'$retention',
										'$protection_coverage',
										'$health_rpo2',
										'$health_throttle2',
										'$health_ret2',
										'$health_resync2',
										'$health_data2',
										'$rpo_threshold',
										'$total_duration',
										'$tag_count',
										'$pair_id',
										'$plan_id')";
			   }
			   elsif($query_type eq 'update')
			   {
			   
					$insert_sql = "UPDATE
											healthReport
									SET
											dataChangeWithCompression = '$compressed',
											dataChangeWithoutCompression = '$uncompressed',
											retention_policy_duration = '$total_duration',
											availableConsistencyPoints = '$tag_count'";

						if($maxRpo > 0){ $insert_sql .= ",maxRPO ='$maxRpo'";}
						if($retention > 0) { $insert_sql .= ",retentionWindow ='$retention'";}
						if($protection_coverage > 0) { $insert_sql .= ",protectionCoverage ='$protection_coverage'";}
						if($health_rpo2 > 0) { $insert_sql .= ",rpoHealth ='$health_rpo2'"; }
						if($health_throttle2 > 0) { $insert_sql .= ",throttleHealth ='$health_throttle2'";}
						if($health_ret2 > 0) { $insert_sql .= ",retentionHealth ='$health_ret2'";}
						if($health_resync2 > 0) { $insert_sql .= ",resyncHealth ='$health_resync2'";}
						if($health_data2 > 0) { $insert_sql .= ",replicationHealth ='$health_data2'";}
						if($rpo_threshold > 0) { $insert_sql .= ",rpoTreshold ='$rpo_threshold'";}
										
						$insert_sql .= " WHERE
												pairId = '$pair_id'
										AND
												reportDate ='$report_date'";
			   }
			   if($insert_sql ne '')
			   {
					if($one_to_many_source)
					{
						$logging_obj->log("DEBUG","updateHealthReportData :: insert_sql - $insert_sql");
						my $insert_sth =  $conn->sql_query($insert_sql);
					}
					elsif(($is_resync eq 0 and $is_quasi_flag eq 2) or ($restart_resync_counter gt 0))
					{
						$logging_obj->log("DEBUG","updateHealthReportData :: insert_sql - $insert_sql");
						my $insert_sth =  $conn->sql_query($insert_sql);
					}
			   }
			}

				#
				# This is For trendingData table.
				#
				$logging_obj->log("DEBUG","result_hour  :: ".Dumper(\%result_hour)); 
				while(($key,$value)= each(%result_hour))
				{
					while( my ($key_2,$value_2) = each(%$value))
					{
						my  $insert_sql1;
						my  $query_type1;
						$last_updated_hour = 0;
						my  $report_hour = $key.' '.$key_2.':'.'00'.':'.'00';
						my ($compressed,$uncompressed,$maxrpo);
						
						while(my ($ikey,$ival) = each (%$value_2))
						{
							
							if($ikey eq 'compress')
							{
								 $compressed = $ival;
							}
							elsif($ikey eq 'uncompress')
							{
								 $uncompressed = $ival;
							}
							elsif($ikey eq 'rpo')
							{
								 $maxrpo = $ival;
							}
							
						}
						
						my $check_sql = "SELECT 
										  	reportHour,
											compressed,
											unCompressed 
									  FROM 
										  trendingData 
									  WHERE 
										  pairId = '$pair_id'
									  AND 
										  reportHour = '$report_hour'";

						my $avaliable_record = '';
					
						my $result4 = $conn->sql_exec($check_sql);

						foreach my $check_ref (@{$result4})
						{
							$avaliable_record =  $check_ref->{'reportHour'};
							$compressed += $check_ref->{'compressed'};
							$uncompressed += $check_ref->{'unCompressed'};
						}
						if($avaliable_record ne '')
						{
							$query_type1 = 'update';
						}
						else
						{
							$query_type1 = 'insert';
						}
						if(($query_type1 eq 'insert') && (($report_hour ne ''))&& ($report_hour ne '0000-00-00 00:00:00')&& ($report_hour ne '0000-00-00')&& ($report_hour ne '0'))
						{
						   
							$insert_sql1 = "INSERT 
											INTO 
												trendingData                    
												(
												sourceHostId,
												sourceDeviceName,
												destinationHostId,
												destinationDeviceName,
												reportHour,
												compressed,
												unCompressed,
												maxRPO,
												pairId
												)
											VALUES
												('$source_hostId',
												'$source_deviceName',
												'$tgt_hostId',
												'$tgt_deviceName',
												'$report_hour',
												'$compressed',
												'$uncompressed',
												'$maxrpo',
												'$pair_id'
												)";
						 

					   }
					   elsif($query_type1 eq 'update')
					   {

						 $insert_sql1 = "UPDATE 
											trendingData
										SET
											compressed = '$compressed',
											unCompressed = '$uncompressed',
											maxRPO = '$maxrpo'
										WHERE
											pairId = '$pair_id'
										AND 
											reportHour ='$report_hour'
										AND 
											reportHour >= '$pair_creation_time'";
					   
					   }
					   
					   
					   if($one_to_many_source)
						{
							$logging_obj->log("DEBUG","updateHealthReportData :: insert_sql1 - $insert_sql1");
							my $insert_sth1 =  $conn->sql_query($insert_sql1);
						}
						elsif((($is_resync eq 0) and ($is_quasi_flag eq 2)) or ($restart_resync_counter gt 0))
						{
							$logging_obj->log("DEBUG","updateHealthReportData :: insert_sql1 - $insert_sql1");
							my $insert_sth1 =  $conn->sql_query($insert_sql1);
						}
					}
				}
		  }
		#
		# To Delete the code for if data is old
		#
		
		#my $delete_sql = "DELETE FROM trendingData WHERE  DATEDIFF(NOW(),reportHour) > 31";
		#my $delete_sth = $conn->sql_query($delete_sql);
	};
	if($@)
	{    
	   
		$logging_obj->log("EXCEPTION","exception in health rrd  :: $@");
	}
    
}

sub checkCluster
{
	
	my($obj,$srchostid) = @_;
	my $result = 0;
	my $num_entries = $obj->{'DPSO_conn'}->sql_get_value('clusters', 'count(*)', "hostId='$srchostid'");
	
	if($num_entries > 0)
	{
		$result = 1;
	}
	return $result;
}


sub gen_traffic_trending
{
	my ($obj) = @_;
	my $conn = $obj->{'DPSO_conn'};
	my %processed;
	eval
	{					
		my @pair_details;
		my $event_start_time = tmanager::getCurrentTime();

		my $sql = "SELECT 
								sourceHostId ,
								sourceDeviceName ,
								destinationHostId ,
								destinationDeviceName,
								profilingId
								FROM
								srcLogicalVolumeDestLogicalVolume 
								WHERE
								oneToManySource = 1";
		my $result1 = $conn->sql_exec($sql);

		foreach my $ref (@{$result1})
		{
			my $sourceDeviceName = TimeShotManager::formatDeviceName($ref->{'sourceDeviceName'});
			my $key = $ref->{'sourceHostId'}.$sourceDeviceName.$ref->{'destinationHostId'}.$ref->{'destinationDeviceName'};
			if(!$processed{$key})
			{
				my @clusters = tmanager::getAssociatedClusterIds($ref->{'sourceHostId'}, $sourceDeviceName,$conn);
				foreach my $clustid (@clusters) {
					my $clustKey = $clustid.$sourceDeviceName.$ref->{'destinationHostId'}.$ref->{'destinationDeviceName'};
					$processed{$clustKey} = 1;
					$logging_obj->log("DEBUG","Marking 1 for process key $clustKey\n");
				}
				push @pair_details, { destinationHostId => "$ref->{'destinationHostId'}", 
				destinationDeviceName => "$ref->{'destinationDeviceName'}", 
				sourceHostId => "$ref->{'sourceHostId'}", 
				sourceDeviceName => "$sourceDeviceName",
				profilingId => "$ref->{'profilingId'}"
				};
				$processed{$key} = 1;
			}
		}
		$logging_obj->log("DEBUG","pair_details : ".Dumper(@pair_details));

		foreach my $pair_key (@pair_details) 
		{ 	
			my $source_hostId = $pair_key->{sourceHostId};
			my $source_deviceName =  $pair_key->{sourceDeviceName};
			my $tgt_deviceName = $pair_key->{destinationDeviceName};
			my $tgt_hostId = $pair_key->{destinationHostId};
			my $profiling_id = $pair_key->{profilingId};
			my $is_cluster = $obj->checkCluster($source_hostId);
			my $host_ids = '';
			my $clusterId;
			if($is_cluster)
			{
				$clusterId = $conn->sql_get_value('clusters', 'clusterId', "hostId = '$source_hostId' limit 1");
				
				if($clusterId)
				{
					my $host_sql = "SELECT 
									distinct  hostId 
							   FROM 
									clusters 
								WHERE 
									clusterId = '$clusterId'";
					my $res1 = $conn->sql_exec($host_sql);

					foreach my $host_row (@{$res1})
					{
						if($host_ids ne '')
						{
							$host_ids .= ",";
						}
						$host_ids .= "'".$host_row->{'hostId'}."'";
					}
				}
				else
				{
					$host_ids .= "'".$source_hostId."'";
				}
			}
			else
			{
				$host_ids .= "'".$source_hostId."'";
			}

			my @report_details;
			my $report_sql = "SELECT 
									sum(dataChangeWithCompression) as dataChangeWithCompression,
									sum(dataChangeWithoutCompression) as dataChangeWithoutCompression,
									reportDate
								FROM 
									healthReport
								WHERE 
									sourceDeviceName = '$source_deviceName' AND
									sourceHostId IN($host_ids) AND
									to_days(now()) - to_days(reportDate) < 7 
								GROUP BY 
									reportDate
								";

			$logging_obj->log("DEBUG","SQL :: $report_sql\n");
			my $res = $conn->sql_exec($report_sql);
			foreach my $ref (@{$res})
			{
				push @report_details, { dataChangeWithCompression => "$ref->{'dataChangeWithCompression'}",
				dataChangeWithoutCompression => "$ref->{'dataChangeWithoutCompression'}",
				reportDate => "$ref->{'reportDate'}"
				};
				$logging_obj->log("DEBUG","report_details :: ".Dumper(@report_details));
			}
			my $CallCounter = 0;
			foreach  my $report_key (@report_details)
			{
				my $comp_in_mb = $report_key->{dataChangeWithCompression}/(1024*1024);
				my $uncomp_in_mb =  $report_key->{dataChangeWithoutCompression}/(1024*1024);
				my $forDay = $report_key->{reportDate};

				my $sel_date = "SELECT cumulativeCompression,cumulativeUnCompression from profilingDetails where profilingId = '$profiling_id' AND forDate = '$forDay'"; 
				$logging_obj->log("DEBUG","SQL :: $sel_date\n");
				my $rs = $conn->sql_exec($sel_date);
				my $query_update;
				
				if(defined $rs)
				{
					$query_update = "UPDATE 
					profilingDetails 
					SET 
					cumulativeCompression = '".$comp_in_mb."', 
					cumulativeUnCompression = '".$uncomp_in_mb."'
					WHERE  
					profilingId = '$profiling_id' and fordate = '".$forDay."'";
				}
				else
				{
					$query_update = "INSERT
					INTO 
					profilingDetails 
					(profilingId,cumulativeCompression,cumulativeUnCompression,forDate,fromUpdate) 	
					VALUES 
					('$profiling_id','$comp_in_mb','$uncomp_in_mb','".$forDay."',1)";
				}
				
				$logging_obj->log("DEBUG","SQL :: $query_update\n");
				my $ins_sth = $conn->sql_query($query_update);
				$conn->sql_finish($ins_sth);
				
				$CallCounter = 1;
			}
			
			my $sql = "SELECT 
			max(compressed) as compressed,
			max(unCompressed) as unCompressed,
			max(reportHour) as reportHour
			FROM 
			trendingData
			WHERE 
			sourceDeviceName = '$source_deviceName' 
			AND 
			sourceHostId IN($host_ids)";
			
			my $result = $conn->sql_exec($sql);

			foreach my $ref (@{$result})
			{
				my $hourly_comp = $ref->{'compressed'}/(1024*1024);
				my $hourly_uncomp = $ref->{'unCompressed'}/(1024*1024);
				my $for_hour = $ref->{'reportHour'};
				my @hour_arr = split(' ',$for_hour);
				
				my $hourly_cumulative_peak_uncomp = $conn->sql_get_value('profilingDetails', 'hourlyCumulativePeakUnCompression', "profilingId = '$profiling_id' and fordate = '".$hour_arr[0]."'");
				
				my $query_update;
				if($hourly_cumulative_peak_uncomp < $hourly_uncomp)
				{
					$query_update = "UPDATE 
						profilingDetails 
						SET 
						hourlyCumulativePeakCompression = '".$hourly_comp."',
						hourlyCumulativePeakUnCompression = '".$hourly_uncomp."'
						WHERE  
						profilingId = '$profiling_id' and fordate = '".$hour_arr[0]."'";
					
					my $ins_sth = $conn->sql_query($query_update);	
				}
				
				$obj->updateMaxCompUnComp($source_hostId,$source_deviceName);
			}
		}
		my $sqlDelRecord = "delete from profilingDetails where to_days(now()) - to_days(forDate) >= 7";
		my $sthRec = $conn->sql_query($sqlDelRecord);
		tmanager::tmanager_event_log('gentrends::gen_traffic_trending', $event_start_time);
	};	
	if($@)
	{
		$logging_obj->log("EXCEPTION","Execution loop for gen_traffic_trending failed due to the ERROR: ".$@);
	}
}

sub updateMaxCompUnComp
{
	my ($obj, $id, $vol) = @_;
	
	my $conn = $obj->{'DPSO_conn'};

    my $sql;
	my $sql2;
	
	$vol = TimeShotManager::formatDeviceName($vol);

	my $profiling_id = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'profilingId', "sourceHostId = '$id' and  sourceDeviceName = '$vol'");
		
	$sql = "select max(hourlyCumulativePeakUnCompression) as uncomp_value, max(hourlyCumulativePeakCompression) as comp_value from profilingDetails where profilingId = '$profiling_id' ";
	
	my $result = $conn->sql_exec($sql);
	
	my @value = @$result;

	my $uncomp_value = $value[0]{'uncomp_value'};
	my $comp_value = $value[0]{'comp_value'};
	
	my $isCluster = tmanager::is_dev_clustered($conn, $id, $vol);
	 
	#if the device is clustered
	if ($isCluster == 1)
	{ 
		$sql2 = "SELECT distinct(cs.hostId) as sourceHostId FROM clusters cs WHERE cs.clusterId = (SELECT distinct(clusterId) FROM   clusters WHERE hostId = '$id')";
		my $rs = $conn->sql_exec($sql2);

		foreach my $ref1 (@{$rs})	
		{
			my $srcid = $ref1->{sourceHostId};
			$sql = "update srcLogicalVolumeDestLogicalVolume set maxUnCompression='$uncomp_value', maxCompression='$comp_value' where sourceHostId = '$srcid' and  sourceDeviceName = '$vol'";

			my $sth1 = $conn->sql_query($sql);
			$conn->sql_finish($sth1);
		}
	}
	else
	{
		$sql = "update srcLogicalVolumeDestLogicalVolume set maxUnCompression='$uncomp_value', maxCompression='$comp_value' where sourceHostId = '$id' and  sourceDeviceName = '$vol'";

		my $sth1 = $conn->sql_query($sql);
		$conn->sql_finish($sth1);
	}
}


sub getProtectionHostsDBDown
{
	my $self = shift;
	my $srcvol  = shift;
	my $srchostid  = shift;
	
	my $source_vol = TimeShotManager::formatDeviceName($srcvol);

	#TimeShotManager::pplog("getProtectionHostsDBDown:: gettg params srcvol & srchostid are $srcvol, $srchostid\n");
	my $sql;

	my %host_list;
	
	my $BASE_DIR			     = '/home/svsystems';
	my $HOME_DIR;
	my $cluster_file_name;

	if (Utilities::isWindows())
	{
		$HOME_DIR                = $BASE_DIR."\\cache";
		$cluster_file_name       = $HOME_DIR."\\cluster";
	}
	else
	{
		$HOME_DIR                 = $BASE_DIR."/cache";
		$cluster_file_name        = $HOME_DIR."/cluster";
	}
	
	my @cluster_cache_hostids_array;
	my @cluster_cache_volumes_array;
	
	
	if (-e $cluster_file_name)
	{
		open(CLUSTER_CACHE_FILE , $cluster_file_name) or die("Could not open cluster cache file $cluster_file_name !!! .");
		foreach my $line (<CLUSTER_CACHE_FILE>) 
		{
			my ($clusterId, $all_hostId, $all_cluster_devices) = split('####',$line);
			
			@cluster_cache_hostids_array = split(',',$all_hostId);								
			
			
			my $cluster_hostvol_match = 0;
			
			for ( my $i=0 ; $i < scalar(@cluster_cache_hostids_array) ;  $i++) 
			{
				$host_list{$cluster_cache_hostids_array[$i]} = $srcvol;
				if( $srchostid eq $cluster_cache_hostids_array[$i])
				{
					@cluster_cache_volumes_array = split(',',$all_cluster_devices);
					
						for ( my $cnt = 0 ; $cnt < scalar(@cluster_cache_volumes_array) ; $cnt++) 
						{
							$cluster_cache_volumes_array[$cnt] = Utilities::makePath($cluster_cache_volumes_array[$cnt]);
							
							$cluster_cache_volumes_array[$cnt] =~ s|\/\/|\/|g;
							chomp($cluster_cache_volumes_array[$cnt]);
							
							if( $srcvol eq $cluster_cache_volumes_array[$cnt])
							{
									$cluster_hostvol_match = 1;
							}
						}
				}
			}
			if($cluster_hostvol_match == 0)
			{
				%host_list = ();
				$host_list{$srchostid} = $srcvol;
				
			}
			@cluster_cache_volumes_array = split(',',$all_cluster_devices);
		}
		close(CLUSTER_CACHE_FILE);
		if(%host_list == '')
		{
			$host_list{$srchostid} = $srcvol;
			
		}
	}
	else
	{
		$host_list{$srchostid} = $srcvol;

	}	
	

	return \%host_list;
}

##
# Returns a hash of farLineProtected volumes for a given host
# Notes: Uses the database handle ($dbh) global to this package
#        Establishing connection is the responsibility of the caller
#
# Inputs: hostid
##
sub getFarlineProtectedVolumes {
	my ($hostid,$conn) = @_;
	my %fpVolumes;

	my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};

	my $sql = "SELECT
				a.deviceName
			FROM
				$LV_TBL a,
				$SRC_DST_LV_TBL b
			WHERE
				a.hostid = b.sourceHostId AND
				a.deviceName = b.sourceDeviceName AND
				a.hostid = '$hostid' AND
				a.farLineProtected = 1 AND
				a.dpsId != '' AND
				b.processServerId = '$host_guid'";
	
	my $result = $conn->sql_get_hash($sql,'deviceName','deviceName');	
	%fpVolumes = %$result;

	return %fpVolumes;
}

sub accumulate_perf_data
{      
	my ($data_hash) = @_;	
	my $pair_data = $data_hash->{'pair_data'};
	my $host_data = $data_hash->{'host_data'};
	
	my $event_start_time = tmanager::getCurrentTime();
	$logging_obj->log("DEBUG","Entered accumulate_perf_data function\n");  
	eval
	{		
		# List for holding pending Initial Sync DPSO's
		my %pendingMonitorDpsoList;
		my $i = 0;
		my $pair_array;
		foreach my $pair_id (keys %{$pair_data})
		{
			my $src_host_id = $pair_data->{$pair_id}->{'sourceHostId'};
						
			if (!defined $pendingMonitorDpsoList{$pair_id}) 
			{				
				$pair_array->{$i}->{'srchostid'} = $src_host_id;
				$pair_array->{$i}->{'src_vol'} = $pair_data->{$pair_id}->{'sourceDeviceName'};
				$pair_array->{$i}->{'one_to_many'} = $pair_data->{$pair_id}->{'oneToManySource'};
				$pair_array->{$i}->{'os_flag'} = $host_data->{$src_host_id}->{'osFlag'};
				$pair_array->{$i}->{'unix_time'} = $pair_data->{$pair_id}->{'unix_time'};
				$i++;
				$pendingMonitorDpsoList{$pair_id} = 1;								
			}
		}
		&updatePerfPsTxt($pair_array);
		tmanager::tmanager_event_log('accumualte_perf_data', $event_start_time);
	};
	if ($@)
	{
		$logging_obj->log("EXCEPTION","Failed to connect to DB:accumualte_perf_data: $@");
	}
}
sub fetchRrd 
{
	my ($rrd_name,$file_name,$start_time,$end_time) = @_;
	if($AMETHYST_VARS->{'CUSTOM_BUILD_TYPE'} == 3){ return; }
	my ($start,$step,$names,$data) = RRDs::fetch ($rrd_name, "AVERAGE", "--start=$start_time", "--end=$end_time");
	my $ERROR = RRDs::error;
	
	if($ERROR)
	{
	    $logging_obj->log("EXCEPTION","fetchRrd::Unable to fetch $rrd_name :$ERROR ");
	}
	
	if(!open(FH, ">".$file_name))
	{
	   $logging_obj->log("EXCEPTION","fetchRrd::Unable to open the $file_name $! ");
	}
	else
	{
		foreach my $line (@$data)
		{		
			print FH scalar ($start), ": ";
			$start += $step;
			foreach my $val (@$line) 
			{
				print FH "$val ";
			}
			print FH "\n";
		}
		close FH;
	}
	
}
sub get_rrd_info
{
  my ($rrd_file) = @_;
  my $info_result = RRDs::info "$rrd_file";
  my $ERROR = RRDs::error;
  $logging_obj->log("EXCEPTION","unable to get info from $rrd_file: $ERROR \n") if $ERROR;
  my %info_result = %$info_result;
  return $info_result{'last_update'};
}


1;
