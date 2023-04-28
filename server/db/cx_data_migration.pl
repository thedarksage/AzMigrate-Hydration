#!/usrl/bin/perl
use strict;
use DBI;
use Config;
my $dsn;
my $DB_HOST;
my $DB_NAME;
my $username;
my $password;
my $dbh = 'undef';
my $CX_TYPE;
my $amethyst_filename;
my $amethyst_vars;
my $CLUSTER_IP;
my $HOST_GUID;
my $contents;

eval
{
 	$amethyst_filename = "/home/svsystems/etc/amethyst.conf";
		
	if (isWindows())
	{
		$amethyst_filename = "c:\\home\\svsystems\\etc\\amethyst.conf";
	}
	
	$amethyst_vars = &parse_conf_file($amethyst_filename);
    $DB_HOST = $amethyst_vars->{'DB_HOST'};
    $DB_NAME = $amethyst_vars->{'DB_NAME'};
    $dsn = "DBI:mysql:database=".$DB_NAME.";host=".$DB_HOST;
    $username = $amethyst_vars->{'DB_USER'};
    $password = $amethyst_vars->{'DB_PASSWD'};
	$CX_TYPE = $amethyst_vars->{'CX_TYPE'};
	$CLUSTER_IP = $amethyst_vars->{'CLUSTER_IP'};
	$HOST_GUID = $amethyst_vars->{'HOST_GUID'};
		
	if($CX_TYPE != 2)
	{
		$dbh = DBI->connect($dsn,
							$username,
							$password);
	}						
};
if($@)
{
    &cx_config_data_mig_log("ERROR while performing data migration".$@."\n");
	#print 111;
}

my $cs_version = &get_cs_version();
if($dbh ne 'undef' and $CX_TYPE != 2 and $cs_version lt '7.1')
{
	$dbh->{AutoCommit} = 0;
	$dbh->{RaiseError} = 1;

	eval
	{
		my $remote_cx = `perl is_cx_standby.pl`;

		$remote_cx = int($remote_cx);
		
		#To check whether cx is ha or not, if clusterip is set in amethyst.conf then trating it as HA.
		if($CLUSTER_IP ne "" and $CLUSTER_IP ne undef)
		{
			my $CLUSTER_NAME = "CLUSTER_".$CLUSTER_IP;
			&cx_ha_data_migration($CLUSTER_NAME);
		}

		#If the host is remot cx then update configuration and insert data in standbydetails table.	
		&cx_config_data_mig_log("SELECT remote_cx::$remote_cx\n");

		#If the host is Primary(ACTIVE) we need to insert the data into standbydetails table.
		&remote_cx_data_migration($amethyst_vars);
		
		$dbh->commit;
	};
	if($@)
    {
		#Rollback the transaction 
		&cx_config_data_mig_log("Rollback".$@);
        $dbh->rollback ;
		#print 111;
    }

}

if($dbh ne 'undef' and $CX_TYPE != 2)
{
	$dbh->{AutoCommit} = 0;
	$dbh->{RaiseError} = 1;
	eval
	{
		#To check whether cx is ha or not, if clusterip is set in amethyst.conf then trating it as HA.
		if($CLUSTER_IP ne "" and $CLUSTER_IP ne undef)
		{
			&cx_ha_fx_job_cleanup();
		}		
		$dbh->commit;
	};
	if($@)
    {
		#Rollback the transaction 
		&cx_config_data_mig_log("Rollback cx_ha_fx_job_cleanup: ".$@);
        $dbh->rollback;
    }
}

sub cx_ha_data_migration
{
	my($cluster_name) = @_;
	
	#Fetching the details from cxPair table only if clusteIP is not null, because only for CLuster HA we need to insert the data
		
	my $select_cx_pair_sql = "SELECT 
								* 
							FROM 
								cxPair 
							WHERE 
								(clusterIp !='' AND clusterIp !='NULL' AND clusterIp IS NOT NULL)";
								
	&cx_config_data_mig_log("cx_ha_data_migration select_cx_pair_sql::$select_cx_pair_sql\n"); 
	my $sth_select_cx_pair_sql = $dbh->prepare($select_cx_pair_sql);
	$sth_select_cx_pair_sql->execute();
	
	while(my $ref = $sth_select_cx_pair_sql->fetchrow_hashref())
	{
		my $cluster_ip = $ref->{'clusterIp'};
		my $state = $ref->{'state'};
		my $db_sync_state = 'DISABLED';
		if($state == 1)
		{
			$db_sync_state = 'ENABLED';
		}
		my $node_role = $ref->{'role'};
		my $node_ip = $ref->{'ip_address'};
		my $node_id = $ref->{'hostId'};
		my $db_time_stamp = $ref->{'dbTimeStamp'};
		my $sequence_number = $ref->{'sequenceNumber'};
		
		#Checking any data exists in applianceCluster table or not, if exists updating the table else inserting the data into applianceCluster
		my $select_app_cluster = "SELECT 
									*
								FROM 
									applianceCluster";
									
		&cx_config_data_mig_log("cx_ha_data_migration select_app_cluster::$select_app_cluster\n"); 
		my $sth_select_app_cluster = $dbh->prepare($select_app_cluster);
		$sth_select_app_cluster->execute();
		
		my $insert_app_cluster;

		if($sth_select_app_cluster->rows())
		{
			$insert_app_cluster = "UPDATE 
											applianceCluster 
										SET 
											applianceId='$cluster_name', 
											applianceIp = '$cluster_ip', 
											applianceType='HA', 
											dbSyncJobState='$db_sync_state'
										WHERE 
											applianceIp = '$cluster_ip'";
		}
		else
		{
			$insert_app_cluster = "INSERT 
										INTO 
											applianceCluster  
										values 
										('$cluster_name',
										'$cluster_ip',
										'HA',
										'$db_sync_state')";
		}
		
		&cx_config_data_mig_log("cx_ha_data_migration insert_app_cluster::$insert_app_cluster \n");   
		my $sth_insert_app_cluster = $dbh->prepare($insert_app_cluster);
		$sth_insert_app_cluster->execute();
		
		#Checking with the host_id any entry exists in applianceNodeMap or not.
		my $select_app_node_map = "SELECT 
										* 
									FROM 
										applianceNodeMap 
									WHERE 
										nodeId='$node_id'";
										
		&cx_config_data_mig_log("cx_ha_data_migration applianceNodeMap::$select_app_node_map \n");
		my $sth_select_node_map = $dbh->prepare($select_app_node_map);
		$sth_select_node_map->execute();
		
		my $insert_app_node_map;
		if($sth_select_node_map->rows())
		{
			$insert_app_node_map = "UPDATE 
											applianceNodeMap 
										SET 
											applianceId='$cluster_name', 
											nodeId='$node_id', 
											nodeIp='$node_ip', 
											nodeRole='$node_role', 
											dbTimeStamp='$db_time_stamp', 
											sequenceNumber='$sequence_number'
										WHERE
											nodeId='$node_id'";	
		} else{
			$insert_app_node_map = "INSERT 
										INTO 
											applianceNodeMap 
										values 
										('$cluster_name',
										 '$node_id',
										 '$node_ip',
										 '$node_role',
										 '$db_time_stamp',
										 '$sequence_number')";
		}
		
		&cx_config_data_mig_log("cx_ha_data_migration applianceNodeMap::$insert_app_node_map\n");  
		my $sth_node_map = $dbh->prepare($insert_app_node_map);
		$sth_node_map->execute();
	}
}

sub cx_ha_fx_job_cleanup
{
	#removing ha_db_sync FX jobs for CX HA in the following record deletes in the six tables, 
    #frbStatus, frbJobs, frbOptions, frbJobGroups, frbSchedule, frbJobConfig
    
	my $sql_del_frbStatus = "DELETE FROM frbStatus WHERE jobConfigId IN (SELECT id FROM frbJobConfig WHERE appname = 'HA_DB_SYNC')";
	&cx_config_data_mig_log("cx_ha_fx_job_cleanup :: sql to delete ha_db_sync FX jobs from frbStatus: ".$sql_del_frbStatus);	
	my $rs_del_frbStatus = $dbh->prepare($sql_del_frbStatus);
	$rs_del_frbStatus->execute();
	
	my $sql_del_frbJobs = "DELETE FROM frbJobs WHERE jobConfigId IN (SELECT id FROM frbJobConfig WHERE appname = 'HA_DB_SYNC')";
	&cx_config_data_mig_log("cx_ha_fx_job_cleanup :: sql to delete ha_db_sync FX jobs from frbJobs: ".$sql_del_frbJobs);	
	my $rs_del_frbJobs = $dbh->prepare($sql_del_frbJobs);
	$rs_del_frbJobs->execute();
	
	my $sql_del_frbOptions = "DELETE FROM frbOptions WHERE id IN (SELECT optionsId FROM frbJobConfig WHERE appname = 'HA_DB_SYNC')";
	&cx_config_data_mig_log("cx_ha_fx_job_cleanup :: sql to delete ha_db_sync FX jobs from frbOptions: ".$sql_del_frbOptions);	
	my $rs_del_frbOptions = $dbh->prepare($sql_del_frbOptions);
	$rs_del_frbOptions->execute();

	my $sql_del_frbJobGroups = "DELETE FROM frbJobGroups WHERE id IN (SELECT groupId FROM frbJobConfig WHERE appname = 'HA_DB_SYNC')";
	&cx_config_data_mig_log("cx_ha_fx_job_cleanup :: sql to delete ha_db_sync FX jobs from frbJobGroups: ".$sql_del_frbJobGroups);	
	my $rs_del_frbJobGroups = $dbh->prepare($sql_del_frbJobGroups);
	$rs_del_frbJobGroups->execute();
    
	my $sql_del_frbSchedule = "DELETE FROM frbSchedule WHERE id IN (SELECT scheduleId FROM frbJobConfig WHERE appname = 'HA_DB_SYNC')";
	&cx_config_data_mig_log("cx_ha_fx_job_cleanup :: sql to delete ha_db_sync FX jobs from frbSchedule: ".$sql_del_frbSchedule);	
	my $rs_del_frbSchedule = $dbh->prepare($sql_del_frbSchedule);
	$rs_del_frbSchedule->execute();
    
	my $sql_del_frbJobConfig = "DELETE FROM frbJobConfig WHERE appname = 'HA_DB_SYNC'";
	&cx_config_data_mig_log("cx_ha_fx_job_cleanup :: sql to delete ha_db_sync FX jobs from frbJobConfig: ".$sql_del_frbJobConfig);	
	my $rs_del_frbJobConfig = $dbh->prepare($sql_del_frbJobConfig);
	$rs_del_frbJobConfig->execute();
}

sub remote_cx_data_migration()
{	
	my ($amethyst_vars) = @_;
	my $host_id = $amethyst_vars->{'HOST_GUID'};
	
	#removing db-sync fx jobs for remote cx. From 7.0 onwards Remote CX db-sync is handled by PS
	my $sql_fx_instances = "DELETE FROM frbJobs WHERE jobConfigId IN(SELECT id FROM frbJobConfig WHERE appname = 'RR_DB_SYNC')";
					
	&cx_config_data_mig_log("remote_cx_data_migration :: sql to delete db-sync fxjobs : ".$sql_fx_instances);
	
	my $rs_fx_instances = $dbh->prepare($sql_fx_instances);
	$rs_fx_instances->execute();
	
	my $sql_fx_jobs = "DELETE FROM frbStatus WHERE jobConfigId IN(SELECT id FROM frbJobConfig WHERE  appname = 'RR_DB_SYNC')";
					
	&cx_config_data_mig_log("remote_cx_data_migration :: sql to delete db-sync fxjobs : ".$sql_fx_jobs);
	
	my $rs_fx_jobs = $dbh->prepare($sql_fx_jobs);
	$rs_fx_jobs->execute();
	
	my $sql_fx_config = "DELETE FROM frbJobConfig WHERE appname = 'RR_DB_SYNC'";
					
	&cx_config_data_mig_log("remote_cx_data_migration :: sql to delete db-sync fxjobs : ".$sql_fx_config);
	
	my $rs_fx_config = $dbh->prepare($sql_fx_config);
	$rs_fx_config->execute();

	#----------------#-----------------#--------------------#--------------------#
	
	#migrate data from cx_pair to standby_details
	
	my $sql_select = "SELECT 
					hostId,
					ip_address,
					port_number,
					role,
					timeout,
					nat_ip,
					clusterIp,
					dbTimeStamp 
				FROM 
					cxPair 
				WHERE 
					clusterIp='NULL' 
				OR 
					clusterIp=''
				OR
					clusterIp IS NULL";
					
	&cx_config_data_mig_log("remote_cx_data_migration :: sql to get details from cx-pair : ".$sql_select);
	
	my $rs_select = $dbh->prepare($sql_select);
	$rs_select->execute();
	while (my @row = $rs_select->fetchrow_array)
	{
		my $ip = $row[1];
		my $appliance_cluster_or_hostId = $row[0];
		my $role;
		
		if($row[3] eq "ACTIVE")
		{
			$role = "PRIMARY";
			
			my $sql_select1 = "SELECT 
									* 
								FROM 
									cxPair 
								WHERE 
									(clusterIp <> 'NULL' AND clusterIp <> '' AND clusterIp IS NOT NULL)
								AND 
									hostId='".$row[0]."' AND role='ACTIVE'";
			&cx_config_data_mig_log("remote_cx_data_migration sql_select1::$sql_select1\n");

			my $rs_select1 = $dbh->prepare($sql_select1);
			$rs_select1->execute();
			if ($rs_select1->rows())
			{
				my $row1 = $rs_select1->fetchrow_hashref;
				$ip = $row1->{'clusterIp'};
				$appliance_cluster_or_hostId = "CLUSTER_".$ip;
			}
		}
		else 
		{	if($row[3] eq "PASSIVE")
			{
				$role = "STANDBY";
			}
		}
		
		my $insert_sql = "INSERT 
							INTO 
								standByDetails ( 
								appliacneClusterIdOrHostId,
								ip_address,
								port_number,
								role,
								timeout,
								nat_ip,
								dbTimeStamp)
							VALUES ( 
								'".$appliance_cluster_or_hostId."',
								'".$ip."',
								'".$row[2]."',
								'".$role."',
								'".$row[4]."',
								'".$row[5]."',
								'".$row[7]."')";
		
		&cx_config_data_mig_log("remote_cx_data_migration insert_sql::$insert_sql\n");
		my $rs_insert = $dbh->prepare($insert_sql);
		$rs_insert->execute();
	}
}

sub getOSType()
{
	return $Config{osname};
}

sub isWindows() 
{
  my $os = getOSType();
  $os =~ m/win/ig ? return 1 : return 0;
}

sub parse_conf_file
{
	my ($filename) = @_;
	open (FILEH, "<$filename") || die "Unable to locate $filename:$!\n";
	my @file_contents = <FILEH>;
	close (FILEH);

	my $amethyst_vars;

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

sub cx_config_data_mig_log 
{
	my ($content) = @_;
	my $log_file = "/home/svsystems/var/cx_data_migration.log";

	if (isWindows())
	{
		$log_file = "c:\\home\\svsystems\\var\\cx_data_migration.log";
	}
	open(LOGFILE, ">>$log_file") || print "Could not open fallback global logfile: dpsglobal.log , faulty installation?\n";
	my $entry   = time() . ": " . $content ; # No newline added at end
	print LOGFILE $entry;
	close(LOGFILE);
}

#get cs version
sub get_cs_version()
{
	my $return_val = 0;
	my $version_file = "/home/svsystems/etc/version";
	if (isWindows())
	{
		$version_file = "c:\\home\\svsystems\\etc\\version";
	}
	if( -e $version_file)
	{
		open (FILEH, "<$version_file") || die "Unable to locate $version_file:$!\n";
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
				if($arr[0] eq 'VERSION')
				{
					$return_val =  $arr[1];
				}
			}
		}
	}
	return $return_val;
}
