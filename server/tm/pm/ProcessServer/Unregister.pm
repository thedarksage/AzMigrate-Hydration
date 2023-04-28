#$Header: /src/server/tm/pm/ProcessServer/Unregister.pm,v 1.12 2015/11/25 14:04:02 prgoyal Exp $
#================================================================= 
# FILENAME
#    Unregister.pm
#
# DESCRIPTION
#    This perl module will unregister process server from 
#    configuration server. It checks if there are any replication 
#    pairs attached to PS and alerts the user about replication pairs 
#    being removed upon process server unregistration.
#
#    If the user proceeds to continue:
#
#    i.     Deletes the ps entries from 
#           srcLogicalVolumeDestLogicalVolume tables
#    ii.    Deletes the ps entries from processServer table
#    iii.   Deletes the ps entries from hosts table
#    iv.    Deletes the ps entries from networkAddress table
#    
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ProcessServer::Unregister;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Utilities;
use DBI();
use tmanager;
use Common::Constants;
use Fabric::Constants;
use TimeShotManager;
use Messenger;
use Common::DB::Connection;
use  Common::Log;
use PHP::Serialization qw(serialize unserialize);

my $logging_obj = new Common::Log();
my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
my $cs_installation_path = Utilities::get_cs_installation_path();
#removed process server check to handle source and target nodes unregistration
sub unregister_ps
{
    my ($conn) = @_;
    	
    my $check_sql = "SELECT 
						id,
						name,
						ipaddress,						
						type
					FROM
						hosts
					WHERE 
						agent_state = '".Common::Constants::UNINSTALL_PENDING."'";
						
	my $result = $conn->sql_exec($check_sql);

	foreach my $ref (@{$result})
	{		
		&monitor_ps_un_installation($conn, $ref->{'id'}, $ref->{'name'}, $ref->{'ipaddress'}, $ref->{'type'});        
	}		
}

#
# The following function is responsible for Process Server
# un-registration, and is being called from tmanager::getHosts()
#
sub monitor_ps_un_installation
{
	my ($conn, $psHostId, $psHostName, $psHostIp, $hostType) = @_;    
	my ($ruleId, $srcHostId, $srcDev, $tgtHostId, $tgtDev,$lunId);
	my ($isLun, $isProfiler);
	my ($replicationStatus, $newReplicationStatus);
	my ($lunState, $agentState, $sql);
	my ($newFrbState, $frbState,$compatibilityNo,$solutionType);

	$logging_obj->log("DEBUG","monitor_ps_un_installation");
	$agentState = Common::Constants::UNINSTALL_PENDING;
	$lunState = Fabric::Constants::STOP_PROTECTION_PENDING;
	
	my $sqlCond = "";
	
	$compatibilityNo = $conn->sql_get_value('hosts', 'compatibilityNo', "id = '$psHostId'");
	
	$logging_obj->log("DEBUG","monitor_ps_un_installation::$compatibilityNo");
	$solutionType = "HOST";
	if($compatibilityNo >= 560000)
	{
		$sql = "SELECT 
					ap.solutionType  
			   FROM
					srcLogicalVolumeDestLogicalVolume src,
					applicationPlan ap,
					applicationScenario appS 		
			   WHERE  
					ap.planId = src.planId AND  
					appS.planId = src.planId AND  
					appS.planId = ap.planId AND  
					appS.scenarioId = src.scenarioId AND
					(src.processServerId = '$psHostId' OR 
					src.sourceHostId = '$psHostId' OR
					src.destinationHostId = '$psHostId' OR 
					appS.sourceId LIKE ('%$psHostId%'))";
		my $res = $conn->sql_exec($sql);

		foreach my $ref (@{$res})
		{
			$solutionType = $ref->{'solutionType'};
		}
		
		$logging_obj->log("DEBUG","monitor_ps_un_installation::$solutionType");
		if(($solutionType eq 'PRISM') || ($solutionType eq 'ARRAY'))
		{
			#$sqlCond = "destinationHostId = '$psHostId' OR ";
			$sql = "SELECT 
					src.ruleId,
					src.sourceHostId,
					src.sourceDeviceName,
					src.destinationHostId,
					src.destinationDeviceName,
					src.Phy_Lunid,
					src.replication_status
			   FROM
					srcLogicalVolumeDestLogicalVolume src,
					applicationScenario appS 
			   WHERE  
					(destinationHostId = '$psHostId' OR  
					processServerId = '$psHostId' OR 
					appS.sourceId LIKE ('%$psHostId%')) AND
					src.planId = appS.planId";
			
		}
		else
		{
			$sql = "SELECT 
					ruleId,
					sourceHostId,
					sourceDeviceName,
					destinationHostId,
					destinationDeviceName,
					Phy_Lunid,
					replication_status
			   FROM
					srcLogicalVolumeDestLogicalVolume 
			   WHERE  
					$sqlCond
					processServerId = '$psHostId'";
		
		
		}
	}
	else
	{
		#
		# In case of fabric, PS is coupled with VX
		# and that PS can only be used as Tgt. In that
		# case, consider destinationHostId also for
		# getting the associated pair list.
		#
		if($hostType eq 'fabric')
		{
			$sqlCond = "destinationHostId = '$psHostId' OR ";
		}
		$sql = "SELECT 
					ruleId,
					sourceHostId,
					sourceDeviceName,
					destinationHostId,
					destinationDeviceName,
					Phy_Lunid,
					replication_status
			   FROM
					srcLogicalVolumeDestLogicalVolume 
			   WHERE  
					$sqlCond
					processServerId = '$psHostId'";
			
		
	}
	my $result = $conn->sql_exec($sql);
	
	#
	# Get associated pairs with the PS being un-installed.
	#
	$logging_obj->log("DEBUG","monitor_ps_un_installation::$sql");
	if (defined $result)
	{
		foreach my $ref (@{$result})
		{
			$ruleId = $ref->{'ruleId'};
			$srcHostId = $ref->{'sourceHostId'};
			$srcDev = $ref->{'sourceDeviceName'};
			$tgtHostId = $ref->{'destinationHostId'};
			$tgtDev = $ref->{'destinationDeviceName'};
			$lunId = $ref->{'Phy_Lunid'};
			$replicationStatus = $ref->{'replication_status'};
			
			if($tgtHostId eq Common::Constants::PROFILER_HOST_ID)
			{
				#
				# It's a profiler pair. So no need for TGT side cleanup
				#
				$isProfiler = 1;                
			}
			else
			{
				$isProfiler = 0;
			}
			
			if(($lunId != 0 && $lunId ne "") || ($solutionType eq 'PRISM') || ($solutionType eq 'ARRAY'))
			{
				#
				# It's a LUN pair, so process LUN pairs
				#
				$isLun = 1;
				$logging_obj->log("DEBUG","monitor_ps_un_installation::process_lun_pairs");
				&process_lun_pairs($conn, $tgtHostId, $psHostId, $isProfiler,$replicationStatus,$ruleId);
			}
			else
			{
				#
				# It's a host pair, so process HOST pairs
				#
				$isLun = 0;
				$logging_obj->log("DEBUG","monitor_ps_un_installation::process_host_pairs");
				&process_host_pairs($conn, $srcHostId, $tgtHostId, $psHostId, $isProfiler,$replicationStatus,$ruleId);
			}
		}
	}
	else
	{
		#
		# If no pair is associated with, delete the DB entry
		#
		$logging_obj->log("DEBUG","monitor_ps_un_installation::hostType>>$hostType");
		#
		# Delete appliance ports for fabric PS
		#
		&delete_appliance_ports($conn, $psHostId);
	&delete_physical_ports($conn, $psHostId);
	&delete_from_host_shared($conn, $psHostId);
	&delete_application_information($conn, $psHostId);
#    if($solutionType eq "ARRAY")
 #   {
	&delete_array_association($conn,$psHostId);
  #  }
		if($hostType eq 'fabric')
		{
			
			#
			# Host is fabric, so un-install Vx agent also
			#
			&delete_logical_volumes($conn, $psHostId);
			&delete_fabric_agent($conn, $psHostId);
		}
		else
		{
			#
			# need to check if agent is using for Prism then need to cleanup Prism table
			#
			my $count = $conn->sql_get_value('hostApplianceTargetLunMapping', 'count(sharedDeviceId)', "hostId = '$psHostId' group by sharedDeviceId");
			
			$logging_obj->log("DEBUG","monitor_ps_un_installation else::count>>$count,solutionType>>$solutionType");
			if ($count == 0)
			{
				&delete_logical_volumes($conn, $psHostId);
				&delete_fabric_agent($conn, $psHostId);
				&delete_prism_information($conn, $psHostId);
				
				if (Utilities::isWindows())
				{    
					my $delete_plan = $cs_installation_path."\\home\\svsystems\\admin\\web\\delete_plan_ps.php";
					`$AMETHYST_VARS->{'PHP_PATH'} "$delete_plan" "$psHostId"`;   
				}
				else
				{
					if (-e "/usr/bin/php")
					{
						$logging_obj->log("DEBUG","monitor_ps_un_installation inside php");
						`cd $AMETHYST_VARS->{WEB_ROOT}/; php delete_plan_ps.php  "$psHostId"`;
					}
					elsif(-e "/usr/bin/php5")
					{
						$logging_obj->log("DEBUG","monitor_ps_un_installation inside php5");
						`cd $AMETHYST_VARS->{WEB_ROOT}/; php5 delete_plan_ps.php  "$psHostId"`;
					}
				}
			}
			
			#
			# for Host, un-install only PS
			#
		}
		
		#
		# Delete process server from networkAddress table
		#
		&delete_bpm_policies($conn, $psHostId);
		
		#
		# Delete process server from networkAddress table
		#
		&delete_network_address($conn, $psHostId);
		
		#
		# Delete process server from process server table
		#
		&delete_process_server($conn, $psHostId);

		#
		# Delete process server from hosts table
		#
		&delete_ps_hosts($conn, $psHostId);
	}

	my $errId = $psHostId;
	my $errSummary = "Process Server Uninstalled";
	my $errMessage = "Process server $psHostName of IP $psHostIp has been uninstalled.";
	my $CSHostName = TimeShotManager::getCSHostName($conn);
    my $errCode = "EC0125";
    my %errPlaceholders;
    $errPlaceholders{"HostName"} = $psHostName;
    $errPlaceholders{"IPAddress"} = $psHostIp;
    my $serErrPlaceholders = serialize(\%errPlaceholders);
	my %arguments;
	$arguments{"cs_host_name"} = $CSHostName;
	$arguments{"ps_host_name"} = $psHostName;
	my $params = \%arguments;
	&Messenger::add_error_message ($conn, $errId, "PS_UNINSTALL", $errSummary, $errMessage, $psHostId, $errCode, $serErrPlaceholders);
	&Messenger::send_traps($conn,"PS_UNINSTALL",$errMessage,$params);
	return;
}

#
# Process all the LUN pairs associated
# with the process server
#
sub process_lun_pairs
{
    my ($conn, $tgtHostId, $psHostId, $isProfiler, $replicationStatus,$ruleId) = @_;
    my ($replicationStatesNotOkToSet,$frbStatesNotOkToSet);
    my ($tgtHostStatus);
    my ($frbStatesNotOkToSet);

    my $newReplicationStatus = Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;
    my $lunState = Fabric::Constants::STOP_PROTECTION_PENDING;
    my $newFrbState = Fabric::Constants::DELETE_FR_BINDING_PENDING;

    #
    # For LUN protection, becoz src & ps are same, so 
    # only tgt side clean up is required. if tgt is
    # profiler or tgt is also being un-installed or
    # PS & tgt are in same box, no need 
    # of tgt side clean up
    #

    $tgtHostStatus = tmanager::get_host_status($conn, $psHostId);

	$logging_obj->log("DEBUG","process_lun_pairs::$tgtHostStatus");
    if($replicationStatus == Common::Constants::SOURCE_DELETE_DONE)
    {
        $newReplicationStatus = Common::Constants::PS_DELETE_DONE;
        $replicationStatesNotOkToSet = Common::Constants::PS_DELETE_DONE;
    }
    elsif(($isProfiler == 1) ||
        ($replicationStatus == Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_FAIL) ||
        (($tgtHostStatus == Common::Constants::UNINSTALL_PENDING) && ($tgtHostId eq $psHostId)))
    {
         $newReplicationStatus = Common::Constants::SOURCE_DELETE_PENDING;
         $replicationStatesNotOkToSet = Common::Constants::SOURCE_DELETE_PENDING .
                                  "," . Common::Constants::PS_DELETE_DONE;
    }
    else
    {
         $newReplicationStatus = Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;
         $replicationStatesNotOkToSet = Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING . 
                                    "," . Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_FAIL . 
                                    "," . Common::Constants::SOURCE_DELETE_PENDING .
                                    "," . Common::Constants::PS_DELETE_DONE;
    }

	$logging_obj->log("DEBUG","process_lun_pairs::newReplicationStatus->$newReplicationStatus, replicationStatesNotOkToSet->$replicationStatesNotOkToSet");
    my $cleanupOptions = Common::Constants::INVOLFLT_DRIVER_CLEANUP;
    my $sqlSrcLDestLRepState = "UPDATE
                                    srcLogicalVolumeDestLogicalVolume
                                SET
                                    replication_status = $newReplicationStatus,
                                    replicationCleanupOptions = $cleanupOptions
                                WHERE
                                    ruleId = $ruleId AND 
                                    replication_status NOT IN ($replicationStatesNotOkToSet)";

    my $sqlSrcLDestLLunState = "UPDATE
                                    srcLogicalVolumeDestLogicalVolume
                                SET
                                    lun_state = $lunState
                                WHERE
                                    ruleId = $ruleId AND
                                    lun_state != $lunState";

     my $resRepState = $conn->sql_query($sqlSrcLDestLRepState);
   
     my $resLunState = $conn->sql_query($sqlSrcLDestLLunState);
	 
	my $scenarioId = $conn->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'scenarioId', "ruleId = $ruleId");
	my @par_array = (Common::Constants::SCENARIO_DELETE_PENDING,$scenarioId);
	my $par_array_ref = \@par_array;
	
	my $updateApplicationScenario = $conn->sql_query (
					" UPDATE ".
					" applicationScenario".
					" SET ".
					" executionStatus = ? WHERE  
					scenarioId = ? ",$par_array_ref);
					
	$logging_obj->log("DEBUG","process_lun_pairs:: update updateApplicationScenario>>$scenarioId>>".Common::Constants::SCENARIO_DELETE_PENDING);	 
	 
	 
}


#
# Process all the HOST pairs associated
# with the process server
#
sub process_host_pairs
{
    my ($conn, $srcHostId, $tgtHostId, $psHostId, $isProfiler, $replicationStatus,$ruleId) = @_;
    my ($replicationStatesNotOkToSet);
    my ($newReplicationStatus);

     my $tgtHostStatus = tmanager::get_host_status($conn, $psHostId);
    #
    # For profiler pair or for tgt 
    # is being un-installed, there is
    # no need for TGT side clean up
    #
    if(($isProfiler == 1) || ($tgtHostStatus == Common::Constants::UNINSTALL_PENDING))
    {
         
        $newReplicationStatus = Common::Constants::PS_DELETE_DONE;
        $replicationStatesNotOkToSet = Common::Constants::PS_DELETE_DONE;
    }
    else
    {
        #
        # both src & tgt side clean up is required
        #
        $newReplicationStatus = Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;
        $replicationStatesNotOkToSet = Common::Constants::PS_DELETE_DONE . 
                                   "," . Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING . 
                                   "," . Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_DONE . 
                                   "," . Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_FAIL . 
                                   "," . Common::Constants::SOURCE_DELETE_PENDING .
                                   "," . Common::Constants::SOURCE_DELETE_DONE;
    }
    
    if($replicationStatus == Common::Constants::SOURCE_DELETE_DONE ||
        $replicationStatus == Common::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_FAIL)
    {
        #
        # As the PS is already gone, so PS side 
        # clean up is NOT required.
        #
        $newReplicationStatus = Common::Constants::PS_DELETE_DONE;
        $replicationStatesNotOkToSet = Common::Constants::PS_DELETE_DONE;
    }

    my $cleanupOptions = Common::Constants::INVOLFLT_DRIVER_CLEANUP;
    my $sqlSrcLDestLRepState = "UPDATE
                                    srcLogicalVolumeDestLogicalVolume
                                SET
                                    replication_status = $newReplicationStatus,
                                    replicationCleanupOptions = $cleanupOptions
                                WHERE
                                    ruleId = $ruleId AND
                                    processServerId = '$psHostId' AND
                                    replication_status NOT IN ($replicationStatesNotOkToSet)";
    
  
     my $resRepState = $conn->sql_query($sqlSrcLDestLRepState);
}


#
# Deletes all the BPM Policies  
# associated with the PS
#
sub delete_bpm_policies
{
    #my ($dbh, $psHostId) = @_;
     my ($conn, $psHostId) = @_;


    print "Cleaning up all the associated BPM Policies\n";
  
    my $sqlBpmAlloc = "DELETE FROM 
                            BPMAllocations 
                        WHERE 
                            sourceHostId='$psHostId'";
        
    my $sqlBpmPolicy = "DELETE FROM 
                            BPMPolicies 
                        WHERE 
                            sourceHostId='$psHostId'";
   
    my $resBpmAlloc = $conn->sql_query($sqlBpmAlloc);
   
    my $resAppBpmPolicy = $conn->sql_query($sqlBpmPolicy);

    return;    
}


#
# Deletes all the appliance  
# port information
#
sub delete_appliance_ports
{
    my ($conn, $psHostId) = @_;
	$logging_obj->log("DEBUG","delete_appliance_ports ::$psHostId");
    print "Cleaning up all the associated port entries\n";

    my $sqlAppFrTgt = "DELETE FROM 
                            applianceFrBindingTargets 
                        WHERE 
                            processServerId='$psHostId'";
    
    my $sqlAppFrInit = "DELETE FROM 
                            applianceFrBindingInitiators 
                        WHERE 
                            processServerId='$psHostId'";                    
    
    my $sqlAi = "DELETE FROM 
                        applianceInitiators 
                    WHERE 
                        processServerId='$psHostId'";

    my $sqlAppNports = "DELETE FROM 
                            applianceNports  
                        WHERE 
                            hostId ='$psHostId'";
    my $sqlAppIntTgtLunMap = "DELETE FROM 
                            applianceInitiatorsTargetLunMap   
                        WHERE 
                            hostId ='$psHostId'";
    
    my $sqlhostIscsiInit = "DELETE FROM 
                            hostIscsiInitiator    
                        WHERE 
                            hostId ='$psHostId'";

    my $resAppNports = $conn->sql_query($sqlAppNports);
    my $resAppIntTgtLunMap = $conn->sql_query($sqlAppIntTgtLunMap);
    my $reshostIscsiInit = $conn->sql_query($sqlhostIscsiInit);
   
    return;    
}

#
# Deletes all the ports from nports  
# for Vx unregistration
#
sub delete_physical_ports
{
	my($conn, $vxHostId)=@_;
	$logging_obj->log("DEBUG","delete_physical_ports::$vxHostId");
	my @par_array = ($vxHostId);
	my $par_array_ref = \@par_array;
	my $deleteNports = $conn->sql_query (
						"DELETE ".
						" FROM".
						" nports". 
						" WHERE ".
						" hostId = ?",$par_array_ref);
	
	my $sqlCheck = "SELECT
                        arrayGuid 
                    FROM
                        arrayInfo 
                    WHERE 
                        agentGuid = '$vxHostId'";
    
  
    my $result = $conn->sql_exec($sqlCheck);
  
    if (defined $result)
    {       
		my @value = @$result;

		my $arrayGuid = $value[0]{'arrayGuid'};
		$logging_obj->log("DEBUG","delete_physical_ports::arrayGuid:$arrayGuid");
		
		my @par_array = ($arrayGuid);
		my $par_array_ref = \@par_array;
		my $deleteNports = $conn->sql_query (
							"DELETE ".
							" FROM".
							" nports". 
							" WHERE ".
							" hostId = ?",$par_array_ref);

	}					
}


#
# Deletes Vx entry from hosts  
# for FA un-registration
#
sub delete_fabric_agent
{
    my ($conn, $vxHostId) = @_;
    $logging_obj->log("DEBUG","delete_fabric_agent ::$vxHostId");
    print "Cleaning up hosts table for fabric agent\n";

    my $sql = "UPDATE
                    hosts
                SET
                    outpostAgentEnabled = 0,
                    sentinelEnabled = 0
                WHERE 
                    id = '$vxHostId'";
    
 
    $conn->sql_query($sql);
    return;    
}


#
# Deletes all the volume entries  
# from logicalVolumes table for
# VX un-registration
#
sub delete_logical_volumes
{
   
    my ($conn, $vxHostId) = @_;
    $logging_obj->log("DEBUG","delete_logical_volumes ::$vxHostId");
	print "Cleaning up logicalVolumes table\n";

    my $sql = "DELETE FROM
                    logicalVolumes
                WHERE
                    hostId = '$vxHostId'";
    my $res = $conn->sql_query($sql);
	
	my @par_array = ($vxHostId);
	my $par_array_ref = \@par_array;
	my $deletepartitions = $conn->sql_query (
						" DELETE FROM".
						" partitions".
						" WHERE".
						" hostId = ?",$par_array_ref);
	
	
	my $deleteDiskGroups = $conn->sql_query (
						" DELETE FROM".
						" diskGroups ".
						" WHERE".
						" hostId = ?",$par_array_ref);
	
	my $deleteDiskPartitions = $conn->sql_query (
						" DELETE FROM".
						" diskPartitions ".
						" WHERE".
						" hostId = ?",$par_array_ref);
	
	my $deletePhysicalDisks = $conn->sql_query (
						" DELETE FROM".
						" physicalDisks ".
						" WHERE".
						" hostId = ?",$par_array_ref);	
	
    return;    
}

sub delete_application_information
{
	my ($conn, $vxHostId) = @_;
	$logging_obj->log("DEBUG","delete_application_information::vxHostId::$vxHostId");
	my ($policyId,@policy_id_list);
	my @par_array = ($vxHostId);
	my $par_array_ref = \@par_array;
	my $deleteApplicationHosts = $conn->sql_query (
									"DELETE FROM ".
									" applicationHosts".
									" WHERE".
									" hostId = ?",$par_array_ref);
	
	$logging_obj->log("DEBUG","delete_application_information::after delete from applicationHosts");	
					
	# my $selectPolicy  = $conn->sql_query(
						# "SELECT".
						# " policyId".
						# " FROM ".
						# " policy ".
						# " WHERE".
						# " hostId LIKE '%?%'",$par_array_ref);
	# $selectPolicy->bind_columns(\$policyId);
	# while ($selectPolicy->fetch) 
	# {
		# push(@policy_id_list,$policyId);
	# }
	# my $policy_id_list = join("','",@policy_id_list);
	
# $logging_obj->log("DEBUG","delete_application_information::before delete from policy::$policy_id_list");		
	# my $deletePolicy = $conn->sql_query (
				# "DELETE FROM".
				# " policy".
				# " WHERE".
				# " hostId LIKE '%?%'",$par_array_ref);
	
	# my @par_array2 = ($policy_id_list);
	# my $par_array_ref2 = \@par_array2;	
	# my $deleteRecoveryExecutionSteps = $conn->sql_query (
					# "DELETE FROM".
					# " recoveryExecutionSteps".
					# " WHERE".
					# " policyId IN ('?')",$par_array_ref2);
	# $logging_obj->log("DEBUG","delete_application_information::after delete from recoveryExecutionSteps");				
}

sub delete_prism_information
{
	my ($conn, $hostId) = @_;
	my($appInstanceId,$appInstanceName,$isClustered,$clusterid,$appFlag);
	$logging_obj->log("DEBUG","delete_prism_information::hostId::$hostId");
	my $selectAppHosts = "SELECT 
								applicationInstanceId,
								applicationInstanceName,
								isClustered,
								clusterId
						FROM	
								applicationHosts
						WHERE
								hostId = '$hostId' AND
								applicationType = 'ORACLE'";
	
	$logging_obj->log('DEBUG',"delete_prism_information : $selectAppHosts");
	
	my $result = $conn->sql_exec($selectAppHosts);

	if (defined $result)
	{
		foreach my $row (@{$result})
		{
			$logging_obj->log('DEBUG',"delete_prism_information ::inside while");
			$appInstanceId = $row->{'applicationInstanceId'};
			$appInstanceName = $row->{'applicationInstanceName'};
			$isClustered = $row->{'isClustered'};
			$clusterid = $row->{'clusterId'};
			
			$appFlag = 1;
			&delete_from_oracle_database_failover_details($conn,$hostId,$appInstanceName,$appFlag);
			&delete_from_oracle_nodes($conn,$hostId,$clusterid,$appFlag);
			if (!$isClustered)
			{
				$logging_obj->log('DEBUG',"delete_prism_information ::inside not isclustered");
				&delete_from_oracle_cluster($conn,$clusterid,$hostId,$appFlag);
			}
			else
			{
				$logging_obj->log('DEBUG',"delete_prism_information::inside inside isclustered");
				&delete_from_oracle_cluster($conn,$clusterid,$hostId,$appFlag);
				my $selectClusterIdInsName = "SELECT
													clusterId
												FROM
													applicationHosts
												WHERE
													clusterId = '$clusterid' AND
													applicationInstanceName = '$appInstanceName'";
				$logging_obj->log('DEBUG',"delete_prism_information::inside else11:$selectClusterIdInsName");					
				my $result = $conn->sql_exec($selectClusterIdInsName);
				
				if (defined $result)
				{
					&delete_from_oracle_database_instance($conn,$clusterid,$appInstanceName);
				}
				&delete_from_oracle_devices($conn,$appInstanceId);
				&delete_from_oracle_cluster_devices($conn,$hostId);
			}
		}
	}
	else
	{
		$appFlag = 0;
		&delete_from_oracle_database_failover_details($conn,$hostId,'',$appFlag);
		&delete_from_oracle_cluster($conn,'',$hostId,$appFlag);
		&delete_from_oracle_nodes($conn,$hostId,'',$appFlag);
		&delete_from_oracle_cluster_devices($conn,$hostId);
		
	}
}


sub delete_from_oracle_database_failover_details
{
	my ($conn, $hostId, $appInstanceName,$appFlag) = @_;
	my($deleteOracleDatabaseFailoverDetails);
	$logging_obj->log("DEBUG","delete_from_oracle_database_failover_details>>hostId::$hostId,appInstanceName::$appInstanceName");
	$logging_obj->log("DEBUG","delete_from_oracle_database_failover_details>>appFlag::$appFlag");
	
	if($appFlag)
	{
	$deleteOracleDatabaseFailoverDetails = "DELETE
											FROM
												oracleDatabaseFailoverDetails	
											WHERE
												hostId = '$hostId' AND
												applicationInstanceName = '$appInstanceName'";
	$conn->sql_query($deleteOracleDatabaseFailoverDetails);
	$logging_obj->log('DEBUG',"delete_from_oracle_database_failover_details : $deleteOracleDatabaseFailoverDetails");
	}
	else
	{
		$deleteOracleDatabaseFailoverDetails = "DELETE
											FROM
												oracleDatabaseFailoverDetails	
											WHERE
												hostId = '$hostId'";
	$conn->sql_query($deleteOracleDatabaseFailoverDetails);
	$logging_obj->log('DEBUG',"delete_from_oracle_database_failover_details : $deleteOracleDatabaseFailoverDetails");
	}
}

sub delete_from_oracle_nodes
{
	my ($conn, $hostId,$clusterid,$appFlag) = @_;
	my($updateOracleNodes);
	$logging_obj->log("DEBUG","delete_from_oracle_nodes>>hostId::$hostId,clusterid::$clusterid,appFlag>>$appFlag");
	$updateOracleNodes = "	UPDATE
								oracleNodes	
								SET
									hostId = ''
								WHERE
									hostId = '$hostId'";
		$conn->sql_query($updateOracleNodes);
		$logging_obj->log('DEBUG',"delete_from_oracle_nodes : $updateOracleNodes");
}

sub delete_from_oracle_cluster
{
	my ($conn, $clusterid,$hostId,$appFlag) = @_;
	my($deleteOracleClusters);
	$logging_obj->log("DEBUG","delete_from_oracle_cluster>>$clusterid,$hostId,$appFlag");
	my $selectClusterId = "SELECT
								oracleClusterId 
							FROM
									oracleNodes
							WHERE
									hostId  = '$hostId'";
	$logging_obj->log('DEBUG',"delete_from_oracle_cluster ::$selectClusterId");
	
	my $result = $conn->sql_exec($selectClusterId);
	if(defined $result)
	{
		my @value = @$result;

		my $oracleClusterId = $value[0]{'oracleClusterId'};
		my $selectHostId = "SELECT
									hostId 
								FROM 
										oracleNodes 
								WHERE 
										oracleClusterId  = '$oracleClusterId' 
								AND 
									hostId != ''";
		$logging_obj->log('DEBUG',"delete_from_oracle_cluster ::$selectHostId");
		my $rs = $conn->sql_exec($selectHostId);
		if (defined $rs)
		{
			$deleteOracleClusters = "DELETE
									FROM
										oracleClusters	
									WHERE
										oracleClusterId = '$oracleClusterId'";
			$conn->sql_query($deleteOracleClusters);
			$logging_obj->log('DEBUG',"delete_from_oracle_cluster : $deleteOracleClusters");
		}
	}	
}

sub delete_from_oracle_database_instance
{
	my ($conn, $clusterid,$appInstanceName) = @_;
	my($deleteOracleInstance);
	$logging_obj->log("DEBUG","delete_from_oracle_database_instance>>clusterid::$clusterid,appInstanceName::$appInstanceName");
	$deleteOracleInstance = "DELETE
							 FROM
									oracleDatabaseInstances	
							 WHERE
									oracleClusterId = '$clusterid' AND
									instanceName = '$appInstanceName'";
		$conn->sql_query($deleteOracleInstance);
		$logging_obj->log('DEBUG',"delete_from_oracle_database_instance : $deleteOracleInstance");
}

sub delete_from_oracle_devices
{
	my ($conn, $appInstanceId) = @_;
	my($deleteOracleDevices);
	$logging_obj->log("DEBUG","delete_from_oracle_devices>>appInstanceId::$appInstanceId");
	$deleteOracleDevices = "DELETE
							FROM
								oracleDevices
							WHERE
								applicationInstanceId = '$appInstanceId'";
	$conn->sql_query($deleteOracleDevices);
	$logging_obj->log('DEBUG',"delete_from_oracle_devices : $deleteOracleDevices");	
}

sub delete_from_oracle_cluster_devices
{
	my ($conn, $hostId) = @_;
	my($deleteOracleClusterDevices);
	$logging_obj->log("DEBUG","delete_from_oracle_cluster_devices>>hostId::$hostId");
	$deleteOracleClusterDevices = "DELETE
							FROM
								oracleClusterDevices 
							WHERE
								hostId = '$hostId'";
	$conn->sql_query($deleteOracleClusterDevices);
	$logging_obj->log('DEBUG',"delete_from_oracle_cluster_devices : $deleteOracleClusterDevices");	
}


sub delete_from_host_shared
{
	my ($conn, $hostId) = @_;
	my($deleteHostShared);
	$logging_obj->log('DEBUG',"delete_from_host_shared : $hostId");
	$deleteHostShared = "DELETE
						FROM
							hostSharedDeviceMapping	
						WHERE
							hostId = '$hostId'";
	$logging_obj->log('DEBUG',"delete_from_host_shared : $deleteHostShared");						
	$conn->sql_query($deleteHostShared);						
}

#
# Deletes process server entry  
# from networkAddress table
#
sub delete_network_address
{

    my ($conn, $psHostId) = @_;
    print "Cleaning up network address table\n";

    my $sql = "DELETE FROM
                    networkAddress
                WHERE
                    hostId = '$psHostId'";
    
    
    my $res = $conn->sql_query($sql);

    return;    
}


#
# Deletes process server entry
# from processServer table
#
sub delete_process_server
{
    
    my ($conn, $psHostId) = @_;
    print "Cleaning up processServer table\n";

    my $sql = "DELETE FROM
                    processServer
                WHERE
                    processServerId = '$psHostId'";
    
   
    my $res = $conn->sql_query($sql);

    return;
}

#
# Deletes process server entry  from hosts table
# provided VX/FX is not installed. Otherwise,
# reset PS enabled flag
#
sub delete_ps_hosts
{
    
    my ($conn, $psHostId) = @_;
    my ($sql);

    print "Cleaning up the hosts table for process server\n";

    my $sqlCheck = "SELECT
                        outpostAgentEnabled,
                        sentinelEnabled,
                        filereplicationAgentEnabled 
                    FROM
                        hosts
                    WHERE 
                        id = '$psHostId'";
    
	my $result = $conn->sql_exec($sqlCheck);
	
    if (defined $result)
    {
        my @value = @$result;

		my $outpostAgentEnabled = $value[0]{'outpostAgentEnabled'};
		my $sentinelEnabled = $value[0]{'sentinelEnabled'};
		my $filereplicationAgentEnabled = $value[0]{'filereplicationAgentEnabled'};
		if(($outpostAgentEnabled == 1 || $sentinelEnabled == 1) ||
			$filereplicationAgentEnabled == 1)
		{
			#
			# VX/FX is installed in the box. So, 
			# don't remove entry from hosts table
			#
			$sql = "UPDATE
						hosts
					SET
						processServerEnabled = 0,
						agent_state = 0
					WHERE 
						id = '$psHostId'";
		}
		else
		{
			$sql = "DELETE FROM
						hosts
					WHERE 
						id = '$psHostId'";
		}

	 
		my $res = $conn->sql_query($sql);
	}
    return;
}

# Delete the array association
# Executed only in case of ARRAY solution type

sub delete_array_association
{
	my ($conn, $psHostId) = @_;
    $logging_obj->log('DEBUG',"delete_array_association : $psHostId");

    my $sql = "DELETE FROM
                    arrayAgentMap
                WHERE
                    agentGuid = '$psHostId'";


    my $res = $conn->sql_query($sql);
		
	$sql = "DELETE FROM
                    arrayInfo
                WHERE
                    agentGuid = '$psHostId'";


    $res = $conn->sql_query($sql);
	
	$sql = "DELETE FROM
                    arrayLunMapInfo
                WHERE
                    protectedProcessServerId = '$psHostId'";


    $res = $conn->sql_query($sql);
		
	$sql = "DELETE FROM
                    managedLuns
                WHERE
                    processServerId = '$psHostId'";


    $res = $conn->sql_query($sql);

    return;
	
}

1;
