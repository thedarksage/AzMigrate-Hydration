#================================================================= 
# FILENAME
#    Failover.pm
#
# DESCRIPTION
#    This module is used by cxfailover.pl responsible for
#    Ha failover actions and performs the following actions.
#
#    1. To process srcLogicalDestLogicalVolume table after failover.
#    2. process pair info according the current Active node info.
#
# HISTORY
#     <19/09/2009>  Prakash Goyal    Created.
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ConfigServer::HighAvailability::Failover;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Utilities;
use TimeShotManager;
use Common::Constants;
use Common::Log;
use Fabric::Constants;
use Common::DB::Connection;
use Data::Dumper;
use Messenger;
use RequestHTTP;
#
# Flush STDERR and STDOUT upon every print
#
select (STDERR); $|=1;
select (STDOUT); $|=1;
my $AMETHYST_VARS;
my @PAIR_IDS_NEW_PS = ();
sub new
{
    my $class = shift;
    my $self = {};
    $AMETHYST_VARS       = Utilities::get_ametheyst_conf_parameters();
	$self->{'DB_TYPE'}      = $AMETHYST_VARS->{"DB_TYPE"};
	$self->{'DB_NAME'}      = $AMETHYST_VARS->{"DB_NAME"};
	$self->{'DB_USER'}      = $AMETHYST_VARS->{"DB_USER"};
	$self->{'DB_PASSWD'}    = $AMETHYST_VARS->{"DB_PASSWD"};
	$self->{'SVSYSTEM_BIN'} = $AMETHYST_VARS->{"SVSYSTEM_BIN"};
	$self->{'INSTALLATION_DIR'} = $AMETHYST_VARS->{"INSTALLATION_DIR"};
	$self->{'GUID_FILE'} 	= $AMETHYST_VARS->{"GUID_FILE"};
	$self->{'WEB_ROOT'} 	= $AMETHYST_VARS->{"WEB_ROOT"};
	$self->{'GUIDFILE'} 	= $self->{'INSTALLATION_DIR'}."/".$self->{'GUID_FILE'};
	
	#
	# Failover script should only talk
	# to the DB which is owned by it
	#
	$self->{'DB_HOST'}        = $AMETHYST_VARS->{"DB_HOST"};
	
	$self->{'dbSyncRpoInSec'} = 1800;    # 30 minutes lag of db-sync job assumption.
	$self->{'maxIoPerSec'} 	  = 500000;  # Considered max IO peak per second is 500K count.
	$self->{'logFile'} 		  = Utilities::makePath(Fabric::Constants::BASE_DIR."/var/ha_failover.log");
	$self->{'logging_obj'} 	  = new Common::Log();
	bless ($self, $class);
	if (!$self->check_log_writable) 
	{
	   return undef;
	}
	return $self;
}

sub check_log_writable 
{
   my ($self) = shift;
   
   if (!open(HA_FAILOVER, ">>$self->{'logFile'}"))
   {
		print HA_FAILOVER "Could not open $self->{'logFile'} for writing:$!\n";
		return Common::Constants::FILE_IO_FAILURE;
   }
   else
   {
		return 1;
   }
	
}

sub failover
{
	my ($self, $conn, $cxHostId) = @_;
	
	$self->trace_log("failover process: $conn \n");
	eval
    {
		#
		# Process lun pairs
		# 
		my $old_process_id = $self->get_old_process_serverid($conn, $cxHostId);
		my $scenario_ref = $self->get_old_ps_before_ha_failover($conn, $cxHostId, $old_process_id);
		$self->process_app_ps_for_host_pair_on_failover($scenario_ref);
		$conn->sql_commit();
		
		$self->process_src_logical_volume_dest_logical_volume_set_flag($conn, $cxHostId, $old_process_id);
		$self->update_retention_tag($conn, $cxHostId, $old_process_id);
		$self->process_array_info($conn, $cxHostId, $old_process_id);
		$conn->sql_commit();
		#
		# Send email upon failover
		#
		$self->send_mail_alert_upon_failover($conn, $cxHostId, $old_process_id);
		$self->process_iscsi_login($conn, $cxHostId, $old_process_id);
		$self->process_src_logical_dest_logical_and_logical_volumes_update($conn, $cxHostId, $old_process_id);
		$self->process_direct_sync_pair($conn, $old_process_id);
		$self->process_driver_sequence_number_on_failover($conn, $cxHostId,$old_process_id);
		$conn->sql_commit();

		#
		# Process host/lun common pairs
		#
		
		
		
		$self->process_ps_for_host_pair_on_failover($conn, $cxHostId, $old_process_id);
		$conn->sql_commit();
		
		$self->process_transProtocol_on_failover($conn, $cxHostId);
		$conn->sql_commit();
		
		$self->process_cx_stale_file_cleanup_on_failover($conn, $cxHostId);
		$conn->sql_commit();
		
		#$self->process_remote_recovery_on_failover($conn, $cxHostId);
		#$conn->sql_commit();
		
		$self->process_replication_on_failover($conn, $cxHostId);
		$conn->sql_commit();

	};
	if($@)
	{
		$self->trace_log("ERROR failover- Roll Back: ".$@."\n");
		#
		# ERROR - Roll Back
		# 
		eval 
		{
			$conn->sql_rollback();
		};
	}
}

#
# Debug-log function
#
sub trace_log
{
    my $TRACE_VAR = 1;
    my($self,$message) = @_;
	if ($TRACE_VAR) 
    {
        print HA_FAILOVER localtime()." :  $message\n";
    }
}

#
# This functions returns the host Id from GUID file
#
sub get_my_host_id()
{
     my($self) = @_;
	if (!open (GUID_FH, "< $self->{'GUIDFILE'}")) 
    {    
        print  "Could not open $self->{'GUIDFILE'} for reading:$!\n";
        return Common::Constants::FILE_IO_FAILURE;
    }

    my $cxHostId = <GUID_FH>;
    close(GUID_FH);
    chomp $cxHostId; 
    return $cxHostId;
}

sub process_array_info
{
	my ($self, $conn, $newCsHostId, $old_process_id) = @_;
	$self->trace_log("process_array_info status update newCsHostId:$newCsHostId,old_process_id:$old_process_id");
	my @par_array = (
					$newCsHostId,
					$newCsHostId,
					$old_process_id
				);
    my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "UPDATE ".
        "  arrayInfo ".
        "SET ".
        "  agentGuid = ?, status = 'REG-PENDING' ".
        "WHERE ".
        "  (agentGuid = ? OR agentGuid = ?) ",$par_array_ref
        );
	$self->trace_log("process_array_info status update arrayInfo:: $sqlStmnt");
	
	my @par_array = (
					$newCsHostId,
					$old_process_id
				);
    my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "UPDATE ".
        "  arrayAgentMap ".
        "SET ".
        "  agentGuid = ? ".
        "WHERE ".
        "  agentGuid = ? ",$par_array_ref
        );
	$self->trace_log("process_array_info status update arrayAgentMap:: $sqlStmnt");
}

sub process_iscsi_login
{
	my ($self, $conn, $newCsHostId, $old_process_id) = @_;
	$self->trace_log("process_iscsi_login newCsHostId:$newCsHostId,old_process_id:$old_process_id");

	my $arrayGuid;
	my @par_array = ($newCsHostId);
	my $par_arrayref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
            "SELECT ".
            "  arrayGuid ".
            "FROM  ".
            "  arrayInfo ".
            "WHERE  ".
            "  agentGuid = ? ",$par_arrayref    
            );

    
    my @array = (\$arrayGuid);
	$conn->sql_bind_columns($sqlStmnt,@array);
    
	if($conn->sql_num_rows($sqlStmnt))
	{
		while ($conn->sql_fetch($sqlStmnt))
		{
			#$arrayGuid = $ref->{"arrayGuid"};
			$self->trace_log("process_iscsi_login cd $AMETHYST_VARS->{WEB_ROOT}/ui/; php insert_policy.php  '$arrayGuid'");
			`cd $AMETHYST_VARS->{WEB_ROOT}/ui/; php insert_policy.php  "$arrayGuid"`;
		}
	}
}


#
# Send email alert after failover
#
sub send_mail_alert_upon_failover
{
    my ($self, $conn, $newCsHostId, $old_process_id) = @_;
    my ($hostName,$hostIp,$affJobs,$srcName,$srcPath,$destName,$destPath);

    $self->trace_log("send_mail_alert_upon_failover");
    $self->trace_log("send_mail_alert_upon_failover : newID : $newCsHostId; oldId : $old_process_id;");
    
    #
    # Flush the error_message table
    #
    my $sqlDel = $conn->sql_query("DELETE FROM error_message");
	my $sqlUpdate = $conn->sql_query("UPDATE users SET last_dispatch_time =0");

    #$sqlDel->execute() or die $db->errstr;

    my @par_array = ($newCsHostId);
	my $par_arrayref = \@par_array;
    my $sqlStmntHosts = $conn->sql_query(
            "SELECT ".
            "  name, ".
            "  ipaddress ".
            "FROM  ".
            "  hosts ".
            "WHERE  ".
            "  id = ? ",$par_arrayref    
            );

    
      my @array = (\$hostName,\$hostIp);
	  $conn->sql_bind_columns($sqlStmntHosts,@array);
    $conn->sql_fetch($sqlStmntHosts);

    my ($ps_hostName,$ps_hostIp);
    my @ps_par_array = ($old_process_id);
	my $ps_par_arrayref = \@ps_par_array;
    my $ps_sqlStmntHosts = $conn->sql_query(
            "SELECT ".
            "  name, ".
            "  ipaddress ".
            "FROM  ".
            "  hosts ".
            "WHERE  ".
            "  id = ? ",$ps_par_arrayref    
            );

      my @ps_array = (\$ps_hostName,\$ps_hostIp);
	  $conn->sql_bind_columns($ps_sqlStmntHosts,@ps_array);
    $conn->sql_fetch($ps_sqlStmntHosts);
    $self->trace_log("send_mail_alert_upon_failover : oldHostName : $ps_hostName; oldHostId: $ps_hostIp for ID : $old_process_id");
    
    #
    # Find the pairs affected due to the failover
    # Lun Pair: FAILOVER_PRE_PROCESSING_PENDING is set
    # Host Pair: PS is different & participating in HA
    #

    @par_array = (
                       Fabric::Constants::FAILOVER_PRE_PROCESSING_PENDING,
                       $old_process_id
                           );
	my $par_arrayref = \@par_array;
    my $sqlStmntSldl = $conn->sql_query(
            "SELECT ".
            "  sourceHostId, ".
            "  sourceDeviceName, ".
            "  destinationHostId, ".
            "  destinationDeviceName ".
            "FROM  ".
            "  srcLogicalVolumeDestLogicalVolume ".
            "WHERE  ".
            "  lun_state = ? OR ".
            "  ((Phy_Lunid = 0 OR Phy_Lunid = '') AND ".
            "    processServerId = ? ) ",$par_arrayref);
            
		$affJobs = "";
		my @array = (\$srcName, \$srcPath, \$destName, \$destPath);
		$conn->sql_bind_columns($sqlStmntSldl,@array);
		
	while($conn->sql_fetch($sqlStmntSldl))
    {
        $affJobs = $affJobs."\n\t<br>[Source:Path]=>[Target:Path]:<br>\n\t".
            $srcName."-".$srcPath."]=>[".$destName."-".$destPath."]\n\n";
    }

    my $err_summary = "CX High Availability Cluster Node Failover \n<br>";
    $err_summary = $err_summary."New Active CX: ".$hostName ."[Node IP - $hostIp]";

    my $err_message = "";

	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
	my @abbr = qw( Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec );
	$year += 1900;
	my $time = $mday." ".$abbr[$mon]." ".$year." ".$hour.":".$min.":".$sec;
    if (length($affJobs)>0)
    {
        $err_message = "A CX High Availability cluster node failover has occurred at ".$time." from $ps_hostName of IP $ps_hostIp to node $hostName of IP $hostIp.\n\n<br><br>Following jobs were affected:\n\n<br><br>$affJobs";
    }
    else
    {
        $err_message = "A CX High Availability cluster node failover has occurred at ".$time." from $ps_hostName of IP $ps_hostIp to node $hostName of IP $hostIp.\n\n<br><br>No jobs were affected.";
    }

    &Messenger::add_error_message ($conn, $newCsHostId, "CS_FAILOVER", $err_summary, $err_message, $old_process_id);
	if(length($err_message) > 1024)
	{
		$err_message = substr($err_message, 0, 1024);
	}
    &Messenger::send_traps($conn,"CS_FAILOVER",$err_message);
    return 0;
}


#
# This functions marks the pair for processing upon failover
#
sub process_src_logical_volume_dest_logical_volume_set_flag
{
    my ($self, $conn, $newSrcHostId, $old_process_id) = @_;

    $self->trace_log("process_src_logical_volume_dest_logical_volume_set_flag, my host id = $newSrcHostId ");
    
    #
    # Set the flag as FAILOVER_PRE_PROCESSING_PENDING provided 
    # the pair is NOT owned by the current ACTIVE node already
    # 
	my @par_array = (
                            Fabric::Constants::FAILOVER_PRE_PROCESSING_PENDING,
                            $newSrcHostId,
							$old_process_id,
                            Fabric::Constants::DELETE_PENDING,
							Fabric::Constants::DELETE_FR_BINDING_PENDING,
                            Fabric::Constants::DELETE_FR_BINDING_DONE,
                            Fabric::Constants::DELETE_AT_LUN_PENDING, 
                            Fabric::Constants::DELETE_AT_LUN_DONE,
                            Fabric::Constants::DELETE_AT_LUN_BINDING_DEVICE_PENDING,
                            Fabric::Constants::DELETE_AT_LUN_BINDING_DEVICE_DONE,
							Fabric::Constants::STOP_PROTECTION_PENDING
                        );
	my $par_array_ref = \@par_array;

    my $sqlStmnt = $conn->sql_query(
            "UPDATE ".
            "  srcLogicalVolumeDestLogicalVolume ".
            "SET ".
            "  lun_state = ? ".
            "WHERE ".
            "  (sourceHostId = ? OR sourceHostId = ?) AND ".
            " (Phy_Lunid != 0 AND ".
            "  Phy_Lunid != '') AND ".
            "  sourceHostId IN (select a.nodeId from applianceNodeMap a , applianceCluster b where a.applianceId = b.applianceId and b.dbSyncJobState = 'ENABLED') AND ".
			"  NOT lun_state IN (?, ?, ?, ?, ?, ?, ?, ?) ",$par_array_ref
            );
    
	#
    # Set the flag as FAILOVER_DRIVER_DATA_CLEANUP_PENDING provided 
    # the pair is ALREADY owned by the current ACTIVE node as in this
    # scenario, no need to perform the additional database operations
    #

	# @par_array = (
                            # Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_PENDING,
                            # $newSrcHostId,
							# $old_process_id,
                            # Fabric::Constants::DELETE_PENDING,
                            # Fabric::Constants::DELETE_FR_BINDING_PENDING,
                            # Fabric::Constants::DELETE_FR_BINDING_DONE,
                            # Fabric::Constants::DELETE_AT_LUN_PENDING,
                            # Fabric::Constants::DELETE_AT_LUN_DONE,
                            # Fabric::Constants::DELETE_AT_LUN_BINDING_DEVICE_PENDING,
                            # Fabric::Constants::DELETE_AT_LUN_BINDING_DEVICE_DONE,
							# Fabric::Constants::STOP_PROTECTION_PENDING
                        # );
	# $par_array_ref = \@par_array;

    # my $sqlStmntOwnPair = $conn->sql_query(
            # "UPDATE ".
            # "  srcLogicalVolumeDestLogicalVolume ".
            # "SET ".
            # "  lun_state = ? ".
            # "WHERE ".
            # "  (sourceHostId = ? OR sourceHostId = ? )AND ".
            # " (Phy_Lunid != 0 AND ".
            # "  Phy_Lunid != '') AND ".
            # "  sourceHostId IN (select a.nodeId from applianceNodeMap a , applianceCluster b where a.applianceId = b.applianceId and b.dbSyncJobState = 'ENABLED') AND ".
			# "  NOT lun_state IN (?, ?, ?, ?, ?, ?, ?, ?) ",$par_array_ref
            # );
}


#
# This function changes the passive hostId to active 
# on node failover. It acts upon logicalVolumes
# And srcLogicalVolumeDestLogicalVolume tables
#
sub process_src_logical_dest_logical_and_logical_volumes_update
{
    my ($self, $conn, $newSrcHostId, $old_process_id) = @_;

    my ($sourceHostId,$sourceDeviceName,$destHost,$scenarioId,$phy_lun_id);
    my ($destinationHostId,$destinationDeviceName,$pairSourceHostId);
    my ($insertStringPartOne,$insertStringPartTwo,$sqlInsertString);

	$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update, new host id = $newSrcHostId, old host id= $old_process_id ");
		
	
	#
	# Target side processing
	#
	my @par_array = (
					$old_process_id,
					$newSrcHostId
					);
	my $par_array_ref = \@par_array;
	
	
	my $sqlStmntLun = $conn->sql_query(
        "SELECT ".
        "  DISTINCT ".
        "  sourceHostId, ".
        "  destinationHostId, ".
        "  scenarioId ".
        "FROM  ".
        "  srcLogicalVolumeDestLogicalVolume sldlv, ".
        "  applicationPlan ap ".
        "WHERE  ".
        "  (ap.planId=sldlv.planId) AND ap.solutionType = 'ARRAY' AND " .
        "  (sldlv.destinationHostId = ? OR sldlv.destinationHostId = ?) ",$par_array_ref
        );

	my @array = (\$pairSourceHostId, \$destinationHostId, \$scenarioId);
	$conn->sql_bind_columns($sqlStmntLun,@array);
	
	while ($conn->sql_fetch($sqlStmntLun))
    {
        
		$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update::checking Target failover ,destinationHostId:$destinationHostId,scenarioId:$scenarioId,pairSourceHostId:$pairSourceHostId");
		
		$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update::cd $AMETHYST_VARS->{WEB_ROOT}/ui/; php insert_policy.php  '$scenarioId' '$old_process_id'");
		
		# IN CASE OF ARRAY
		
		#
		# delete the policy with old PS id and insert new policy.
		#
				
		@par_array = (
					$newSrcHostId,
					Fabric::Constants::MAP_DISCOVERY_PENDING,
					$old_process_id,
					$newSrcHostId
				  );
		my $par_array_ref = \@par_array;
		my $sqlStmnt = $conn->sql_query(
			"UPDATE ".
			"  managedLuns ".
			"SET ".
			"  processServerId = ? ,".
			"  state = ? ".
			"WHERE ".
			"  (processServerId = ? OR processServerId = ?) ",$par_array_ref
			);

		`cd $AMETHYST_VARS->{WEB_ROOT}/ui/; php insert_policy.php  "$scenarioId" "$old_process_id"`;		
	
		@par_array = (
					$newSrcHostId,
					$old_process_id
				  );
		my $par_array_ref = \@par_array;
		my $sqlStmnt = $conn->sql_query(
			"UPDATE ".
			"  srcLogicalVolumeDestLogicalVolume ".
			"SET ".
			"  destinationHostId = ? ".
			"WHERE ".
			"  destinationHostId = ? ",$par_array_ref
			);
		$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update::update target guid srcLogicalVolumeDestLogicalVolume");
		
		if($pairSourceHostId ne $destinationHostId)
		{
				$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update::source and target are not same.");
				my @par_array = (
					Fabric::Constants::FAILOVER_TARGET_RECONFIGURATION_PENDING,
					$pairSourceHostId,
					$newSrcHostId
				  );
				my $par_array_ref = \@par_array;
				my $sqlStmnt = $conn->sql_query(
					"UPDATE ".
					"  srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  lun_state = ? ".
					"WHERE ".
					"  sourceHostId = ? AND ".
					"  destinationHostId = ? ",$par_array_ref
					);

		}
	}	
	
	
	my @par_array = (
					$newSrcHostId,
					$old_process_id,
					Fabric::Constants::FAILOVER_PRE_PROCESSING_PENDING
					);
	my $par_array_ref = \@par_array;
	
	
    my $sqlStmntLun = $conn->sql_query(
        "SELECT ".
        "  sourceHostId, ".
        "  sourceDeviceName, ".
        "  scenarioId, ".
        "  Phy_Lunid ".
        "FROM  ".
        "  srcLogicalVolumeDestLogicalVolume ".
        "WHERE  ".
        "  (processServerId = ? OR processServerId = ? ) AND ".
        "  lun_state = ? ",$par_array_ref  
        );

	my @array = (\$sourceHostId, \$sourceDeviceName, \$scenarioId, \$phy_lun_id);
	$conn->sql_bind_columns($sqlStmntLun,@array);
	
	while ($conn->sql_fetch($sqlStmntLun))
    {
        
		$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update::sourceHostId:$sourceHostId,sourceDeviceName:$sourceDeviceName,scenarioId:$scenarioId,phy_lun_id:$phy_lun_id");
		
		#
        # Delete stale entry with newHostId (If Any)
        # And then insert new entry with new HostId
        #
        @par_array = (
                        $newSrcHostId,
                        $sourceDeviceName
                        );
		$par_array_ref = \@par_array;
        my $sqlStmntLv = $conn->sql_query(
            "SELECT ".
                " hostId ".
                "FROM ".
                "  logicalVolumes ".
                "WHERE ".
                "  hostId = ? AND ".
                "  deviceName = ? ",$par_array_ref
                );
        
	    if(!$conn->sql_num_rows($sqlStmntLv))
        {
            #
            # No record is present with active host id in the DB.
            # Need to insert a new rec-set with active host id
            #
            @par_array = (
                                $sourceHostId,
                                $sourceDeviceName
                                );
			$par_array_ref = \@par_array;
            my $sqlStmntLVSel = $conn->sql_query(
                    "SELECT ".
                    " * ".
                    "FROM ".
                    "  logicalVolumes ".
                    "WHERE ".
                    "  hostId = ? AND ".
                    "  deviceName = ? ",$par_array_ref
                    );
            
	        my $ref = $conn->sql_fetchrow_hash_ref($sqlStmntLVSel);

            #
            # Need to insert a new rec-set with active host id
            # So, assigning the active host id
            #
            $ref->{hostId} = $newSrcHostId;

            $insertStringPartOne ="";
            $insertStringPartTwo = "";
            $sqlInsertString = "";

            #
            # Construct the INSERT statement
            #
            foreach my $colName (keys(%$ref)) 
            {
                chomp $colName;
                chomp $ref->{$colName};
    
				$insertStringPartOne = $insertStringPartOne.$colName." ,";
                $insertStringPartTwo = $insertStringPartTwo."'".$ref->{$colName}."' ,";
            }
            chop $insertStringPartOne;
            chop $insertStringPartTwo;

            $insertStringPartOne = "INSERT INTO logicalVolumes (".$insertStringPartOne.")";
            $insertStringPartTwo =  " VALUES (".$insertStringPartTwo.")";

            $sqlInsertString = $insertStringPartOne.$insertStringPartTwo;

            my $sqlStmntLvInsert = $conn->sql_query($sqlInsertString);
	    }
		else
		{
			#
			# got the existing record need to update the logicalVolume
			#  from active node to passive in HA.
			#
			
			my @par_array = (
                                $sourceHostId,
                                $sourceDeviceName
                                );
            my $par_array_ref = \@par_array;
            my $sqlStmntLVSel = $conn->sql_query(
										"SELECT ".
										" * ".
										"FROM ".
										"  logicalVolumes ".
										"WHERE ".
										"  hostId = ? AND ".
										"  deviceName = ? ",$par_array_ref
										);

            my $ref = $conn->sql_fetchrow_hash_ref($sqlStmntLVSel);

 
			my $updateSql;

			foreach my $colName (keys(%$ref))
			{
			  chomp $colName;
			  chomp $ref->{$colName};
			   if($colName ne 'hostId')
			   {
					$updateSql = $updateSql.$colName."="."'".$ref->{$colName}."'"." ,";
			   } 	
			}
			@par_array = (
							$newSrcHostId,
							$sourceDeviceName
						  );
			my $par_array_data = \@par_array;
			chop($updateSql);
			my $updateSqlStmnt = "UPDATE logicalVolumes SET ".$updateSql."WHERE  hostId = ? AND deviceName = ?";
			my $sqlStmntLvUpdate = $conn->sql_query($updateSqlStmnt,$par_array_data);
			$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update::$newSrcHostId");
			$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update::$sourceDeviceName");
			$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update::$updateSqlStmnt");
			$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update status:: $sqlStmntLvUpdate");
		}
    }

	#
	# To update the source host id for volume resize history on
	# fail over from active node to passive in HA.
	#
   @par_array = ($newSrcHostId);
   $par_array_ref = \@par_array;
	my $sql = "UPDATE".  
		"	volumeResizeHistory ".
		"SET ".
		"	hostId = ? ".
		"WHERE ".
			"hostId in (SELECT ".  
            "   slvl.sourceHostId ".
            "FROM  ".
            "   applianceCluster ac, srcLogicalVolumeDestLogicalVolume slvl, applianceNodeMap anm ". 
            "WHERE ".
            "   slvl.sourceHostId = anm.nodeId AND ".
            "   LENGTH(ac.applianceIp)!=0 AND ".
            "   slvl.Phy_Lunid != 0 AND ".
            "   slvl.Phy_Lunid != '')";
	my $sqlStmnt = $conn->sql_query($sql,$par_array_ref);
	
	
	@par_array = (
                        $newSrcHostId,
                        $newSrcHostId,
                        Fabric::Constants::FAILOVER_PRE_PROCESSING_DONE,
                        $newSrcHostId,
						$old_process_id,
                        Fabric::Constants::FAILOVER_PRE_PROCESSING_PENDING
                      );
    $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "UPDATE ".
        "  srcLogicalVolumeDestLogicalVolume ".
        "SET ".
        "  sourceHostId = ?, ".
        "  processServerId = ?, ".
        "  lun_state = ? ".
        "WHERE ".
        "  (processServerId = ? OR processServerId = ?) AND ".
        "  lun_state = ?",$par_array_ref
        );

	$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update status update srcLogical guid:: $sqlStmnt,newSrcHostId::$newSrcHostId,sourceHostId::$sourceHostId,old_process_id::$old_process_id");	
	@par_array = (
					$newSrcHostId,
					$old_process_id
				);
    $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "UPDATE ".
        "  arrayLunMapInfo ".
        "SET ".
        "  protectedProcessServerId = ? ".
        "WHERE ".
        "  protectedProcessServerId = ? ",$par_array_ref
        );
	$self->trace_log("process_src_logical_dest_logical_and_logical_volumes_update status update arrayLunMapInfo:: $sqlStmnt");
	
	return 0;
}


sub change_array_discovery_ownership_node
{
    my ($self, $conn, $my_host_id) = @_;
	my $update_status = 0;
	my ($query,$other_node_hostid);
	$query =	"SELECT ".
					"	hostId ".
				"FROM ".
					"cxPair ".
				"WHERE ".
					"hostId!='$my_host_id'";

	$self->trace_log("change_array_discovery_ownership_node update status:: $query \n");
	my $result_set = $conn->sql_query($query);
	
	if($conn->sql_num_rows($result_set))
	{
		while (my $ref = $conn->sql_fetchrow_hash_ref($result_set))
		{
			$other_node_hostid = $ref->{"hostId"};
		}
	
		$query = "UPDATE ".
                "    arrayInfo ".
                "SET  ".
                "    agentGuid = '$my_host_id' ".
                "WHERE  ".
                "    agentGuid = '$other_node_hostid'";
		$update_status = $conn->sql_query($query);
	}
	$self->trace_log("change_array_discovery_ownership_node update status:: $update_status, query = $query \n");
}

#
# This function returns the DB handler only when 
# the HA role is ACTIVE
#
sub process_active_db_handler
{
    #
    # Try to connect to local DB, If unable to do, will keep trying
    # will give DB handler only the role in DB is Active.
    # 
    
	my ($self, $cxHostId) = @_;
	
    my $conn = undef;
    
    my %args;
	my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
	my $db_name = $amethyst_vars->{'DB_NAME'};
	my $db_passwd = $amethyst_vars->{'DB_PASSWD'};
	my $db_host = $amethyst_vars->{'DB_HOST'};


	%args = ( 'DB_NAME' => $db_name,
	          'DB_PASSWD'=> $db_passwd,
		      'DB_HOST' => $db_host
	        );
	my $args_ref = \%args;
	while($conn eq undef)
    {
        $conn = new Common::DB::Connection($args_ref);
        if ($conn->sql_ping_test()) 
        {
            print "Failed to connect to database\n";
            sleep 5;
        }
        else
        {
			my @par_array = ($cxHostId,Fabric::Constants::ACTIVE_ROLE);
			my $par_arry_ref = \@par_array;
    
			my $sqlStmnt = $conn->sql_query(
                    "SELECT ".
                    "    anm.nodeRole,".
					"    ac.applianceIp ".
                    "FROM  ".
                    "    applianceNodeMap anm, ".
					"    applianceCluster ac ".
                    "WHERE  ".
                    "    anm.nodeId = ? AND anm.nodeRole = ? AND ac.applianceId = anm.applianceId",$par_arry_ref
                );

            if (!$conn->sql_num_rows($sqlStmnt))
			{
                print " Role is NOT Active, so disconnect and try again\n";
                $conn->sql_disconnect();
                $conn = undef;
                sleep 5;
            }
            else
            {
				my $role;
				my $clusterIp;
				my @array = (\$role, \$clusterIp);
				$conn->sql_bind_columns($sqlStmnt,@array);
				my $ref = $conn->sql_fetchrow_hash_ref($sqlStmnt);
				my $clusterIp = $ref->{'clusterIp'};
			
				my @param_array = (Fabric::Constants::PASSIVE_ROLE,$cxHostId,$clusterIp);
			    my $param_arry_ref = \@param_array;
				my $applNodeUpdate = $conn->sql_query(
                    "UPDATE ".
                    "    applianceNodeMap ".
                    "SET  ".
                    "    nodeRole = ?".
                    "WHERE  ".
                    "    nodeId != ? AND ".
					"    nodeIp = ?",$par_arry_ref
                );
			}
        }
    }
    return $conn;
}


#
# This function cleans up all the stale files from 
# CX on failover
#
sub process_cx_stale_file_cleanup_on_failover
{
    my ($self, $conn, $activeCxHostId) = @_;
    my $list;    
    my ($cmdRetVal, $srcHostId, $destHostId);
    my ($srcVol, $destVol, $srcVolPath, $destVolPath);

    #
    # This function is specifically for host pairs
    # But, it's being made general for both host and fabric
    # as it will remove stale entry display in the UI
    #
    $self->trace_log("process_cx_stale_file_cleanup_on_failover");
    my @par_array = ($activeCxHostId);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "SELECT ".
        "   sourceHostId, ".
        "   sourceDeviceName, ".
        "   destinationHostId, ".
        "   destinationDeviceName ".
        "FROM ".
        "    srcLogicalVolumeDestLogicalVolume ".
        "WHERE ".
        "    processServerId = ? ",$par_array_ref);

	my @array = (
                    \$srcHostId, 
                    \$srcVol, 
                    \$destHostId, 
                    \$destVol,
                    );
	$conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt))
    {
        #
        # Delete stale source files
        # 

        $srcVolPath = $self->convert_windows_path_to_unix_format($srcVol);
        $destVolPath = $self->convert_windows_path_to_unix_format($destVol);

        if(-e "$self->{'INSTALLATION_DIR'}/$srcHostId/$srcVolPath")
        {
            
			$cmdRetVal = $self->execute_command("rmtree($self->{'INSTALLATION_DIR'}/$srcHostId/$srcVolPath, {result => \$list})");
     
            if($cmdRetVal != Common::Constants::COMMAND_EXEC_SUCCESS)
            {
                print "Could not delete source stale files: $!\n";
                return Common::Constants::SOURCE_STALE_REMOVAL_FAIL;
            }
        }
        else
        {
            # Log message
            $self->trace_log("FAIL to delete source stale file - " . 
            "$self->{'INSTALLATION_DIR'}/$srcHostId/$srcVolPath " . 
            "does NOT exist");
        }
        
        #
        # Delete stale target files
        # 
        if(-e "$self->{'INSTALLATION_DIR'}/$destHostId/$destVolPath")
        {
            $cmdRetVal = $self->execute_command("rm -rf $self->{'INSTALLATION_DIR'}/$destHostId/$destVolPath");
     
            if($cmdRetVal != Common::Constants::COMMAND_EXEC_SUCCESS)
            {
                print "Could not delete target stale file: $!\n";
                return Common::Constants::TARGET_STALE_REMOVAL_FAIL;
            }
        }
        else
        {
            # Log message
            $self->trace_log("FAIL to delete target stale file - " . 
            "$self->{'INSTALLATION_DIR'}/$srcHostId/$destVolPath " . 
            "does NOT exist");
        }
    }
    return 0;
}


#
# After failover, set transProtocol as FtpTal where Host Id
# of a pair is NOT same as failed-over PS Id (New PS Id). If 
# it is same, set transProtocol as FileTal
#
sub process_transProtocol_on_failover
{
    my ($self, $conn, $activeCxHostId) = @_;
    my ($srcHostId, $destHostId, $srcVol, $destVol, $psId);
    my @par_array = ($activeCxHostId);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "SELECT ".
        "    sourceHostId, ".
        "    sourceDeviceName, ".
        "    destinationHostId, ".
        "    destinationDeviceName ".
        "FROM ".
        "    srcLogicalVolumeDestLogicalVolume ".
        "WHERE ".
        "    processServerId = ? ",$par_array_ref);

	#    $sqlStmnt->execute($activeCxHostId) or die $db->errstr;
    my @array = (
                    \$srcHostId, 
                    \$srcVol, 
                    \$destHostId, 
                    \$destVol);
	$conn->sql_bind_columns($sqlStmnt,@array);
	
    while ($conn->sql_fetch($sqlStmnt))
    {
        #
        # Process for source host
        #
        $self->trace_log("set transProtocol for source: [$srcHostId : $srcVol]");
        $self->set_transProtocol_on_failover($conn, $srcHostId, $srcVol, $activeCxHostId);

        #
        # Process for target host
        #
        $self->trace_log("set transProtocol for target: [$destHostId : $destVol]");
        $self->set_transProtocol_on_failover($conn, $destHostId, $destVol, $activeCxHostId);
    }
    return 0;
}


#
# Set the transProtocol Flag upon failover
#
sub set_transProtocol_on_failover
{
    my ($self, $conn, $hostId, $deviceName, $PsHostId) = @_;
    my $transProtocol;
    $self->trace_log("set_transProtocol_on_failover: $hostId  $deviceName  $PsHostId");

    if($PsHostId eq $hostId)
    {
        $transProtocol = Common::Constants::FILE_TAL;
    }
    else
    {
		#15172 Fix
		my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
		$transProtocol = $amethyst_vars->{'CXPS_XFER'};
		$self->trace_log("set_transProtocol_on_failover:: transProtocol : $transProtocol");
				
        #$transProtocol = Common::Constants::FTP_TAL;
    }
    my @par_array = (
                        $transProtocol,
                        $hostId,
                        $deviceName
                       );
	my $par_array_ref = \@par_array;
    my $sqlUpdate = $conn->sql_query(
                "UPDATE ".
                "    logicalVolumes ".
                "SET  ".
                "    transProtocol = ? ".
                "WHERE  ".
                "    hostId = ? AND ".
                "    deviceName = ? ",$par_array_ref

                );
    $self->trace_log("set_transProtocol_on_failover:: sqlUpdate:$sqlUpdate setting transProtocol as $transProtocol for [$hostId : $deviceName]");

	return 0;
}


#
# Change the process server Id with the 
# failover process server Id 
# (Scope for this function is NON LUN pair)
#
sub process_ps_for_host_pair_on_failover
{
    my ($self, $conn, $newPsId, $old_process_id) = @_;

    #find out pairIds of all the pairs using the newPsId
    #my @ps_array = ($newPsId);
#	my $newPsId_ref = \@ps_array;
 #   my $sqlSelectPairsUsingNewPs = $conn->sql_query("SELECT pairId FROM srcLogicalVolumeDestLogicalVolume 
  #                                                  WHERE processServerId = ?", $newPsId_ref);
   # @PAIR_IDS_NEW_PS = ();
    #while (my $ref = $conn->sql_fetchrow_hash_ref($sqlSelectPairsUsingNewPs)) {
     #   push(@PAIR_IDS_NEW_PS, $ref->{'pairId'});
    #}
    
    my @par_array = (
                        $newPsId,
		                $old_process_id
                       );
	my $par_array_ref = \@par_array;
	my $sqlUpdatePs = $conn->sql_query(
                "UPDATE ".
                "    srcLogicalVolumeDestLogicalVolume ".
                "SET ".
                "    processServerId = ? ".
                "WHERE  ".
		        "    processServerId = ? AND ".
                "    (Phy_Lunid = '0' OR Phy_Lunid = '') AND ".
                "    processServerId IN (select a.nodeId from applianceNodeMap a , applianceCluster b where a.applianceId = b.applianceId and b.dbSyncJobState = 'ENABLED') ",$par_array_ref
                );
	return 0;
}

#
# After failover, change all the direct sync to new fast sync
# If source and target hosts are different 
#
sub process_direct_sync_pair
{
    my ($self, $conn, $old_process_id) = @_;
    my @par_array = (
                        Common::Constants::FAST_SYNC,
						$old_process_id,
                        Common::Constants::DIRECT_SYNC
                       );
	my $par_array_ref = \@par_array;
    my $sqlUpdate = $conn->sql_query(
                "UPDATE ".
                "    srcLogicalVolumeDestLogicalVolume ".
                "SET  ".
                "    resyncVersion = ?".
                "WHERE  ".
                "    sourceHostId != destinationHostId AND ".
				"    processServerId != ? AND ".
                "    resyncVersion = ? ",$par_array_ref

                );
	return 0;
}

#
# This function writes the sequence number for driver 
# after adding some delta value
#
sub process_driver_sequence_number_on_failover
{
    my ($self, $conn, $hostId,$old_process_id) = @_;
	my $procDir = "/etc/vxagent/bin/inm_dmit";
    my ($exceCmd, $seqNo, $cmdRetVal);

    $self->trace_log("process_driver_sequence_number_on_failover");
    my @par_array = ($old_process_id);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "SELECT ".
        "   sequenceNumber ".
        "FROM  ".
        "   applianceNodeMap ".
        "WHERE ".
        "   nodeId = ?",$par_array_ref);

	
    my @array = (\$seqNo);
	$conn->sql_bind_columns($sqlStmnt,@array);
	
     $conn->sql_fetch($sqlStmnt);
    
    #
    # Assuming $self->{'maxIoPerSec'} IO per second, and
    # DB is not synced for $self->{'dbSyncRpoInSec'}
    #
    my $delta = $self->{'dbSyncRpoInSec'}*$self->{'maxIoPerSec'};
    $seqNo = $seqNo + $delta; 
    if (-e "$procDir")
    {
        my $status = `$procDir --set_attr SequenceNumber $seqNo`;
        $self->trace_log("New seq no $seqNo set to involflt with status $status. \n");
    }
    else
    {
        $self->trace_log("Sequence number file $procDir does not exist. \n");
    }

    return 0;
}


#
# This function changes the active node id in cxPair   
# table and for remote recovery DB sync job 
#
sub process_remote_recovery_on_failover
{
    my ($self, $conn, $newActiveHostId) = @_;
    my ($newActiveIp,$oldActiveHostId);

    $self->trace_log("process_remote_recovery_on_failover");
    
    #
    # get the local ip address for the new active node
    #
	my @par_array = ($newActiveHostId);
	my $par_array_ref = \@par_array;
    my $sqlStmntHosts = $conn->sql_query(
            "SELECT ".
            "  ipaddress ".
            "FROM  ".
            "  hosts ".
            "WHERE  ".
            "  id = ? ",$par_array_ref  
            );

	if(!$conn->sql_num_rows($sqlStmntHosts))
    {
        return 1;
    }
    my @array = (\$newActiveIp);
	$conn->sql_bind_columns($sqlStmntHosts,@array);
	
    $conn->sql_fetch($sqlStmntHosts);
    
    #
    # Change the active host Id & Ip for remote recovery pair
    # by fetching the passive cluster node Ip
    #
    @par_array = ($newActiveHostId);
    $par_array_ref = \ @par_array;
    my $sqlStmnt = $conn->sql_query(
                    "       SELECT ".
                    "           appliacneClusterIdOrHostId ".
                    "       FROM ".
                    "           standByDetails ".
                    "       WHERE ".
                    "           appliacneClusterIdOrHostId != ?",$par_array_ref
                    );

	@array = (\$oldActiveHostId);
    $conn->sql_bind_columns($sqlStmnt,@array);
	$conn->sql_fetch($sqlStmnt);

    @par_array =(
                        $newActiveHostId,
                        $newActiveIp,
                        $oldActiveHostId
                      );
    $par_array_ref = \ @par_array;
    my $sqlStmnt = $conn->sql_query(
                "UPDATE ".
                "  standByDetails ".
                "SET ".
                "  appliacneClusterIdOrHostId = ?, ".
                "  ip_address = ? ".
                "WHERE ".
                "   appliacneClusterIdOrHostId = ? ",$par_array_ref
                );

	#
    # Change the source host id & ip for the fx job 
    # particular to the remote recovery pair
    #
    @par_array =(
                        $newActiveHostId,
                        Common::Constants::RR_DB_SYNC_JOB
                      );
    $par_array_ref = \ @par_array;

    my $sqlStmntFrb = $conn->sql_query(
                "UPDATE ".
                "  frbJobConfig ".
                "SET ".
                "  sourceHost = ? ".
                "WHERE ".
                "  appName = ?",$par_array_ref
                );

	return 0;

}


#
# This function restarts the resync for pair is in resync 
# and sets the tesync required flag for pairs in diffs
#
sub process_replication_on_failover
{

	my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
    my ($self, $conn, $newPsId) = @_;
    my ($resyncing, $pairId, $srcHostId, $srcVol, $destHostId, $destVol, $pauseState, $executionState);
    
    $self->trace_log("process_replication_on_failover");
	eval
	{
		my $pair_ids_new_ps = (@PAIR_IDS_NEW_PS) ? join("','",@PAIR_IDS_NEW_PS) : '';
		my @par_array =(
						  $newPsId
						);
		my $par_array_ref = \ @par_array; 
		my $sqlStmnt = $conn->sql_query(
			"SELECT ".
			"   ruleId, ".
			"   isResync, ".
			"   sourceHostId, ".
			"   sourceDeviceName, ".
			"   destinationHostId, ".
			"   destinationDeviceName, ".
			"   replication_status, ".
			"   executionState  ".
			"FROM  ".
			"   srcLogicalVolumeDestLogicalVolume ".
			" WHERE processServerId = ? AND pairId NOT IN ('$pair_ids_new_ps')",$par_array_ref);
		#    $sqlStmnt->execute() or die $db->errstr;
		   $self->trace_log("process_replication_on_failover:$newPsId,$pair_ids_new_ps");
		my @array =  (
						\$pairId,
						\$resyncing, 
						\$srcHostId, 
						\$srcVol, 
						\$destHostId, 
						\$destVol,
						\$pauseState,
						\$executionState
						);
		$conn->sql_bind_columns($sqlStmnt,@array);

		my $http_method = "POST";
		my $https_mode = ($AMETHYST_VARS->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;		
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $AMETHYST_VARS->{'PS_CS_IP'}, 'PORT' => $AMETHYST_VARS->{'PS_CS_PORT'});
		my $param = {'access_method_name' => "resync_now", 'access_file' => "/resync_now.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $request_http_obj = new RequestHTTP(%args);
		
		while ($conn->sql_fetch($sqlStmnt))
		{
			$self->trace_log("process_replication_on_failover:: destVol: $destVol, resyncing: $resyncing & executionState: $executionState ");
			if($resyncing == 1 && 
					(Common::Constants::PAUSED_PENDING != $pauseState &&
					 Common::Constants::PAUSE_COMPLETED != $pauseState) && ($executionState != 2) && ($executionState != 3))
			{
				#
				# Restart the resync if the pair is in resync
				# and pair is not in pause state.
				# If pair is in diff sync set the resync required 
				# flag as YES for pairs  - Provided pair is NOT PROFILER
				# 
				if(($destHostId ne Fabric::Constants::PROFILER_HOST_ID) &&
						($destVol ne Fabric::Constants::PROFILER_DEVICE_NAME))
				{
					$srcVol = $self->format_device_name($srcVol);
					$destVol = $self->format_device_name($destVol);
					
					#
					# Pair is in resync, so restart the resync
					# 
					
					my %perl_args = ('sourceHostId' => $srcHostId,'sourceDeviceName' => $srcVol ,'destinationHostId' => $destHostId ,'destinationDeviceName' => $destVol);
					my $content = {'content' => \%perl_args};					
					
					my $response = $request_http_obj->call($http_method, $param, $content);
			
					if (! $response->{'status'}) 
					{
						$self->{'logging_obj'}->log("DEBUG","Response::".Dumper($response));
						$self->{'logging_obj'}->log("EXCEPTION","Failed to post the details for process_replication_on_failover");
					}
				}
			}
			elsif($resyncing == 0)
			{
				#
				# Pair is in diff sync, set the resync require flag as YES
				# 
				my @par_array = (time,$pairId);
				my $par_array_ref = \@par_array;
				my $profiler_hostid = Fabric::Constants::PROFILER_HOST_ID;
				my $sqlStmnt = $conn->sql_query(
					"UPDATE ".
					"  srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  shouldResync = 1, ".
					"  ShouldResyncSetFrom = 1, ".
					"  resyncSetCxtimestamp = ? ".
					"WHERE ".
					"  destinationHostId != '$profiler_hostid' ".
					" AND ".
					"  ruleId = ?",$par_array_ref
					);
			}
		}
	};
	if($@)
	{
		$self->{'logging_obj'}->log("EXCEPTION","Error : " . $@);
	}		
    return 0;
}


#
# Checks whether HA is already enabled
# in the current node or not
#
sub is_ha_enabled
{
    my ($self, $conn) = @_;
    
    $self->trace_log("is_ha_enabled");

    my $sqlStmnt = $conn->sql_query(
                    "SELECT ".
                    "    nodeId ".
                    "FROM  ".
                    "    applianceNodeMap"
                );

    if($conn->sql_num_rows($sqlStmnt) > 1)
    {
        #
        # Two HA nodes are present
        #
        $self->trace_log("Both high-availability nodes are present");
        return 1;
    }
    else
    {
        $self->trace_log("High-availability nodes are NOT present");
        return;
    }
}

#
# Formats the data path name for NON UNIX OS
#
sub format_device_name
{
    my ($self, $device_name) = @_;
    if ($device_name ne "")
    {
         $device_name =~ s/\\\\/\\/g;
         $device_name =~ s/\\/\\\\/g;
    }
    return $device_name;
}


#
# Converts the NON UNIX path to UNIX format as
# in RS, the OS is RHEL
#
sub convert_windows_path_to_unix_format
{
    my ($self, $device_path) = @_;
    if ($device_path ne "")
    {
         $device_path =~ s/\\\\/\\/g;
         $device_path =~ s/\\/\\\\/g;
         $device_path =~ s/\\/\//g;
         $device_path =~ s/://g;
    }
    return $device_path;
}


#
# This will execute the system command.
# Also returns the command execution status
#
sub execute_command
{
    my ($self, $command) = @_;
    $self->trace_log("Command to be Executed: ".$command."\n");
    eval 
    {
        my $retVal = system($command);
    };
    if ($@)
    {
        print "Error executing $command: $@\n";
        return Common::Constants::COMMAND_EXEC_FAILURE;
    }
    else
    {
        return Common::Constants::COMMAND_EXEC_SUCCESS;
    }
}

sub get_old_ps_before_ha_failover
{
	my ($self, $conn, $newPsId,  $old_process_id) = @_;
	$self->trace_log("get_old_ps_before_ha_failover::$newPsId");
	my @scenario_details_array1;
	my @scenario_details_array2;
	my @scenario_details_array3;
	my $tgt_ps_id;
	
	my $sql =  "SELECT 
				b.nodeId  as nodeId
			FROM applianceNodeMap a , 
				 applianceNodeMap b 
			WHERE a.nodeId = '$newPsId' 
			AND   a.applianceId = b.applianceId 
			AND 
				  b.nodeId != '$newPsId'";
	$self->trace_log("get_old_ps_before_ha_failover sql1::$sql");
	my $sth = $conn->sql_query($sql);
	my $ref = $conn->sql_fetchrow_hash_ref($sth);
	my $old_id = $ref->{nodeId};

	my $sql = "SELECT
					DISTINCT
					planId,
					scenarioId,
					processServerId,
					destinationHostId 
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					(processServerId = '".$old_id."' OR processServerId = '".$newPsId."')
				AND
					processServerId IN (select a.nodeId from applianceNodeMap a , applianceCluster b where a.applianceId = b.applianceId and b.dbSyncJobState = 'ENABLED')";	

	$self->trace_log("get_old_ps_before_ha_failover sql2::$sql");
	my $rs = $conn->sql_query($sql);
	
	if($conn->sql_num_rows($rs))
	{
		$self->trace_log("get_old_ps_before_ha_failover::for source failover");
		while (my $ref = $conn->sql_fetchrow_hash_ref($rs))
		{
			$self->trace_log("get_old_ps_before_ha_failover::".$ref->{"processServerId"}."destinationHostId::".$ref->{"destinationHostId"});
			
			$tgt_ps_id ='';
			$self->trace_log("get_old_ps_before_ha_failover::newPsId:$newPsId,tgt_ps_id:$tgt_ps_id");
			@scenario_details_array1 = {'scenario_id' => $ref->{"scenarioId"}, 
										 'plan_id' => $ref->{"planId"}, 
										 'old_ps_id' => $ref->{"processServerId"},
										 'new_ps_id' => $newPsId,
										 'new_tgt_ps_id' => $tgt_ps_id
										};
			
			#$self->trace_log("get_old_ps_before_ha_failover::for source failover".@scenario_details_array1);
			$self->trace_log("get_old_ps_before_ha_failover::for source failover>>".Dumper(@scenario_details_array1));
			push(@scenario_details_array2, @scenario_details_array1);			
		}
	}
	#else
	#{
	#
	# block will execute if target failover happened.
	#
	$self->trace_log("get_old_ps_before_ha_failover::checking target failover");
	
		my ($insertStringPartOne,$insertStringPartTwo,$sqlInsertString,$scenarioId,$phy_lun_id);
		my ($destinationHostId,$destinationDeviceName,$pairSourceHostId);
		my $newSrcHostId = $newPsId;
		my @par_array = (
			$old_process_id, $newPsId
			);
		my $par_array_ref = \@par_array;

		
		my $sqlStmntLun = $conn->sql_query(
			"SELECT ".
			"  sourceHostId, ".
			"  destinationHostId, ".
			"  destinationDeviceName, ".
			"  scenarioId, ".
			"  Phy_Lunid ".
			"FROM  ".
			"  srcLogicalVolumeDestLogicalVolume ".
			"WHERE  ".
			"  (destinationHostId = ? OR destinationHostId = ?)",$par_array_ref
			);

		my @array = (\$pairSourceHostId, \$destinationHostId, \$destinationDeviceName, \$scenarioId, \$phy_lun_id);
		$conn->sql_bind_columns($sqlStmntLun,@array);
		
		while ($conn->sql_fetch($sqlStmntLun))
		{
				
			#
			# updating target device ownership in logicalVolume.
			#
			
			$self->trace_log("get_old_ps_before_ha_failover::destinationHostId:$destinationHostId,destinationDeviceName:$destinationDeviceName");
			
			@par_array = (
							$newSrcHostId,
							$destinationDeviceName
							);
			$par_array_ref = \@par_array;
			my $sqlStmntLv = $conn->sql_query(
				"SELECT ".
					" hostId ".
					"FROM ".
					"  logicalVolumes ".
					"WHERE ".
					"  hostId = ? AND ".
					"  deviceName = ? ",$par_array_ref
					);
			$self->trace_log("get_old_ps_before_ha_failover::1");
			if(!$conn->sql_num_rows($sqlStmntLv))
			{
				#
				# No record is present with active host id in the DB.
				# Need to insert a new rec-set with active host id
				#
				@par_array = (
									$destinationHostId,
									$destinationDeviceName
									);
				$par_array_ref = \@par_array;
				my $sqlStmntLVSel = $conn->sql_query(
						"SELECT ".
						" * ".
						"FROM ".
						"  logicalVolumes ".
						"WHERE ".
						"  hostId = ? AND ".
						"  deviceName = ? ",$par_array_ref
						);
				
				my $ref = $conn->sql_fetchrow_hash_ref($sqlStmntLVSel);

				#
				# Need to insert a new rec-set with active host id
				# So, assigning the active host id
				#
				$ref->{hostId} = $newSrcHostId;

				$insertStringPartOne ="";
				$insertStringPartTwo = "";
				$sqlInsertString = "";

				#
				# Construct the INSERT statement
				#
				foreach my $colName (keys(%$ref)) 
				{
					chomp $colName;
					chomp $ref->{$colName};
		
					$insertStringPartOne = $insertStringPartOne.$colName." ,";
					$insertStringPartTwo = $insertStringPartTwo."'".$ref->{$colName}."' ,";
				}
				chop $insertStringPartOne;
				chop $insertStringPartTwo;

				$insertStringPartOne = "INSERT INTO logicalVolumes (".$insertStringPartOne.")";
				$insertStringPartTwo =  " VALUES (".$insertStringPartTwo.")";

				$sqlInsertString = $insertStringPartOne.$insertStringPartTwo;

				my $sqlStmntLvInsert = $conn->sql_query($sqlInsertString);
				$self->trace_log("get_old_ps_before_ha_failover::$sqlInsertString");
			}
			else
			{
				#
				# got the existing record need to update the logicalVolume
				#  from active node to passive in HA.
				#
			$self->trace_log("get_old_ps_before_ha_failover::2");	
				my @par_array = (
									$newSrcHostId,
									$destinationDeviceName
									);
				my $par_array_ref = \@par_array;
				my $sqlStmntLVSel = $conn->sql_query(
											"SELECT ".
											" * ".
											"FROM ".
											"  logicalVolumes ".
											"WHERE ".
											"  hostId = ? AND ".
											"  deviceName = ? ",$par_array_ref
											);

				my $ref = $conn->sql_fetchrow_hash_ref($sqlStmntLVSel);

	 
				my $updateSql;

				foreach my $colName (keys(%$ref))
				{
				  chomp $colName;
				  chomp $ref->{$colName};
				   if($colName ne 'hostId')
				   {
						$updateSql = $updateSql.$colName."="."'".$ref->{$colName}."'"." ,";
				   } 	
				}
				@par_array = (
								$newSrcHostId,
								$destinationDeviceName
							  );
				my $par_array_data = \@par_array;
				chop($updateSql);
				$self->trace_log("get_old_ps_before_ha_failover::3,$updateSql");
				my $updateSqlStmnt = "UPDATE logicalVolumes SET ".$updateSql."WHERE  hostId = ? AND deviceName = ?";
				my $sqlStmntLvUpdate = $conn->sql_query($updateSqlStmnt,$par_array_data);
				$self->trace_log("get_old_ps_before_ha_failover,Target device::hostId:$newSrcHostId");
				$self->trace_log("get_old_ps_before_ha_failover,Target device::deviceName:$destinationDeviceName");
				$self->trace_log("get_old_ps_before_ha_failover,Target device::$updateSqlStmnt");
				$self->trace_log("get_old_ps_before_ha_failover,Target device status:: $sqlStmntLvUpdate");
			}
				
		}			
	
	my $sql = "SELECT
				DISTINCT
				planId,
				scenarioId,
				processServerId
			FROM
				srcLogicalVolumeDestLogicalVolume sldl,
				arrayLunMapInfo almi,
				logicalVolumes lv  	
			WHERE
				(almi.protectedProcessServerId = '".$old_id."' OR
				almi.protectedProcessServerId = '".$newPsId."') AND
				almi.lunType = 'TARGET'	AND
				sldl.Phy_Lunid = almi.sourceSharedDeviceId AND 
				sldl.destinationDeviceName = lv.deviceName	AND 
				lv.Phy_Lunid = almi.sharedDeviceId ";	

	$self->trace_log("get_old_ps_before_ha_failover::checking target failover:".$sql);			
	my $rs = $conn->sql_query($sql);
	if($conn->sql_num_rows($rs))
	{
		while (my $ref = $conn->sql_fetchrow_hash_ref($rs))
		{
			my $old_ps_id = '';
			my $new_ps_id = '';
			@scenario_details_array3 = {'scenario_id' => $ref->{"scenarioId"}, 
										 'plan_id' => $ref->{"planId"}, 
										 'old_ps_id' => $old_ps_id,
										 'new_ps_id' => $new_ps_id,
										 'new_tgt_ps_id' => $newPsId
										};
			
			$self->trace_log("get_old_ps_before_ha_failover::target failover".$ref->{"processServerId"},$newPsId);
			$self->trace_log("get_old_ps_before_ha_failover::target failover>>".Dumper(@scenario_details_array3));
			
			push(@scenario_details_array2, @scenario_details_array3);			
		}
	}
		
	#}	
	return \@scenario_details_array2;
}

sub process_app_ps_for_host_pair_on_failover
{
	my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
	my $self = shift;
	my $scenario_details_array_ref = shift;
	$self->trace_log("process_app_ps_for_host_pair_on_failover");

	#$self->trace_log("process_app_ps_for_host_pair_on_failover".@$scenario_details_array_ref);	
	$self->trace_log("process_app_ps_for_host_pair_on_failover>>".Dumper(@$scenario_details_array_ref));
	my @scenario_details_array = @$scenario_details_array_ref;
	my $i;
	eval
	{
		my $http_method = "POST";	
		my $https_mode = ($amethyst_vars->{'PS_CS_HTTP'} eq 'https') ? 1: 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $amethyst_vars->{'PS_CS_IP'}, 'PORT' => $amethyst_vars->{'PS_CS_PORT'});
		my $request_http_obj = new RequestHTTP(%args);
		
		for($i = 0 ; $i < scalar(@scenario_details_array) ; $i++ ) 
		{
			my $scenario_id = $scenario_details_array[$i]{'scenario_id'};
			my $plan_id = $scenario_details_array[$i]{'plan_id'};
			my $old_ps_id = $scenario_details_array[$i]{'old_ps_id'};
			my $new_ps_id = $scenario_details_array[$i]{'new_ps_id'};
			my $new_tgt_ps_id = $scenario_details_array[$i]{'new_tgt_ps_id'};
			
			my $param = {'access_method_name' => "update_scenario_ps", 'access_file' => "/update_scenario_ps.php", 'access_key' => $amethyst_vars->{'HOST_GUID'}};
			my %perl_args = ('scenario_id' => $scenario_id,'plan_id' => $plan_id ,'old_ps_id' => $old_ps_id ,'new_ps_id' => $new_ps_id,'new_tgt_ps_id' => $new_tgt_ps_id);
			my $content = {'content' => \%perl_args};
			
			my $response = $request_http_obj->call($http_method, $param, $content);
								
			$self->trace_log("process_app_ps_for_host_pair_on_failover::response:$response");
			
			if (!$response->{'status'}) 
			{
				$self->{'logging_obj'}->log("DEBUG","Response::".Dumper($response));	
				$self->{'logging_obj'}->log("EXCEPTION","Failed to post the details for process_app_ps_for_host_pair_on_failover");
			}
		}
	};
	if($@)
	{
		$self->{'logging_obj'}->log("EXCEPTION","Error : " . $@);
	}	
}

sub get_old_process_serverid
{
	my ($self, $conn, $newSrcHostId) = @_;
	$self->trace_log("get_old_process_serverid");

	 #find out pairIds of all the pairs using the newPsId
        my @ps_array = ($newSrcHostId);
        my $newPsId_ref = \@ps_array;
        my $sqlSelectPairsUsingNewPs = $conn->sql_query("SELECT pairId FROM srcLogicalVolumeDestLogicalVolume
        WHERE processServerId = ?", $newPsId_ref);
        @PAIR_IDS_NEW_PS = ();
        while (my $ref = $conn->sql_fetchrow_hash_ref($sqlSelectPairsUsingNewPs)) {
        push(@PAIR_IDS_NEW_PS, $ref->{'pairId'});
        }

        $self->trace_log("get_old_process_serverid:".@PAIR_IDS_NEW_PS);

	
	my $sql =  "SELECT 
				b.nodeId  as nodeId
			FROM applianceNodeMap a , 
				 applianceNodeMap b 
			WHERE a.nodeId = '$newSrcHostId' 
			AND   a.applianceId = b.applianceId 
			AND 
				  b.nodeId != '$newSrcHostId'";

	my $sth = $conn->sql_query($sql);
	my $ref = $conn->sql_fetchrow_hash_ref($sth);
	my $old_id = $ref->{nodeId};
	return $old_id;
}

sub update_retention_tag
{
    my ($self, $conn, $newSrcHostId, $old_process_id) = @_;
	$self->trace_log("update_retention_tag");
	my $sql =  "UPDATE 
					retentionTag
				SET 
					HostId = '$newSrcHostId'
				WHERE 
					HostId = '$old_process_id'";
	$self->trace_log("update_retention_tag $sql");
	my $sth = $conn->sql_query($sql);
}


1;
