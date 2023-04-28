# File       : PrismProtector.pm
#
# Description: this file will handle protection pair 
# request and manage state for pair.
#

package Prism::PrismProtector;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Utilities;
use DBI;
use Fabric::Constants;
use TimeShotManager;
use Common::Log;
use Common::DB::Connection;
use Data::Dumper;
use Array::CommonProcessor;

my $STATES_OK_TO_SET_DELETE = Fabric::Constants::PROTECTED .
"," . Fabric::Constants::CREATE_SPLIT_MIRROR_PENDING .
"," . Fabric::Constants::CREATE_SPLIT_MIRROR_DONE .
"," . Fabric::Constants::CREATE_AT_LUN_PENDING .
"," . Fabric::Constants::CREATE_AT_LUN_DONE .
"," . Fabric::Constants::MIRROR_SETUP_PENDING_RESYNC_CLEARED .
"," . Fabric::Constants::RESYNC_PENDING .
"," . Fabric::Constants::RESYNC_ACKED .
"," . Fabric::Constants::RESYNC_ACKED_FAILED .
"," . Fabric::Constants::REBOOT_PENDING;




# mysql errors that we might need to know about
my $MYSQL_ER_DUP_KEY = 1002;
my $MYSQL_ER_DUP_ENTRY = 1062;
my $MYSQL_ER_DUP_UNIQUE = 1169;
my $MYSQL_ER_ROW_IS_REFERENCED = 1217;
my $MYSQL_ER_ROW_IS_REFERENCED_2 = 1451;

my $logFileSize = 0;



my $AMETHYST_VARS;
our $WEB_ROOT;
my $logging_obj = new Common::Log();
our $db;
our $conn;
BEGIN
{
    $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
	$WEB_ROOT = $AMETHYST_VARS->{"WEB_ROOT"};
}

sub process_src_logical_volume_dest_logical_volume_requests
{
	my ($conn) = @_;
	$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_requests");
	my $solutionType= Fabric::Constants::PRISM_SOLUTION;
	my @par_array = ($solutionType);
    my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "select distinct sharedDeviceId, ".
                "sldlv.sourceDeviceName, ".
                "sldlv.processServerId ".
				"from hostSharedDeviceMapping hsdm, ".
				"srcLogicalVolumeDestLogicalVolume sldlv,
				applicationPlan ap		
			WHERE 
				ap.solutionType = ? 
			AND 
				ap.planId=sldlv.planId 
			AND 	
				hsdm.sharedDeviceId = sldlv.Phy_Lunid ",$par_array_ref);

    my $sharedDeviceId;
    my $processServerId;
    my $sourceDeviceName;
    my $nodeHostId;

    my @array = (\$sharedDeviceId, \$sourceDeviceName, \$processServerId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt))
    {
		$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_requests:$sharedDeviceId, $processServerId, $sourceDeviceName");
		my @lv_array = ($sharedDeviceId, $processServerId, $sourceDeviceName);
		my $lv_array_ref = \@lv_array;
		my $lvSqlStmnt = $conn->sql_query(
			"select Phy_Lunid ".
					"from logicalVolumes 
				WHERE 
					Phy_Lunid = ? 
				AND 
					hostId = ? 
				AND 	
					deviceName = ? ",$lv_array_ref);
		if($conn->sql_num_rows($lvSqlStmnt) > 0)
		{
			$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_requests:sync device is available");
			Array::CommonProcessor::process_src_logical_volume_dest_logical_volume_start_protection_requests($conn,$solutionType,$sharedDeviceId);
		}
		else
		{
			$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_requests:sync device not found");
		}
	}
	&process_src_logical_volume_dest_logical_volume_stop_protection_requests($conn);
	
}


sub process_src_logical_volume_dest_logical_volume_stop_protection_requests
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_stop_protection_requests");

    my $processServerId;
    my $state;
    my $initiator;
    my $target;
    my $sharedDeviceId;
    my $srcDeviceName;
    my $fabricId;
    my $sourceHostId;
    my $nodeHostId;
    my $sdLunId;
    my @par_array = (
							Fabric::Constants::PRISM_SOLUTION,
                            Fabric::Constants::STOP_PROTECTION_PENDING,
                            Fabric::Constants::SOURCE_DELETE_PENDING
					);
	my $par_array_ref = \@par_array;

    my $pairSqlStmnt = $conn->sql_query(
        "select distinct ".
        " sd.sourceHostId, ".
        " sd.Phy_Lunid, ".
        " hatlm.processServerId ".
        "from   ".
		" applicationPlan ap, ".
        " srcLogicalVolumeDestLogicalVolume sd ".		
        " left join hostApplianceTargetLunMapping hatlm on (sd.Phy_Lunid = hatlm.sharedDeviceId) ".
        "where  ".		
		" ap.solutionType = ? ".
		" AND ".
		" ap.planId = sd.planId ".
		" AND ".
        " sd.lun_state in (?) ".
        "and sd.replication_status = ?".
		"order by ".
        " hatlm.processServerId, hatlm.sharedDeviceId",$par_array_ref);


    my @array = (\$sourceHostId, \$sdLunId, \$processServerId);
    $conn->sql_bind_columns($pairSqlStmnt, @array);
    my @applianceInitiatorInfos;
    my @applianceTargetInfos;
    while ($conn->sql_fetch($pairSqlStmnt)) 
	{
        if (Array::CommonProcessor::san_lun_one_to_n($conn, $sdLunId)) 
		{
            &delete_protection_job($conn, $sdLunId, $processServerId);
        } 
		else 
		{

			my @par_array = (1, $sdLunId);
			my $par_array_ref = \@par_array;
            my $sqlStmnt = $conn->sql_query(
                "SELECT distinct ".
                "    sanInitiatorPortWwn, ".
                "    srcDeviceName, ".
                "    hostId ".
                "FROM   ".
                "    hostSharedDeviceMapping ".
                "WHERE  ".
                "deleteState != ? AND ".
                "  sharedDeviceId  = ? ",$par_array_ref);

		    my @array = (\$initiator, \$srcDeviceName,
                                    \$nodeHostId);
			$conn->sql_bind_columns( $sqlStmnt,@array);

			if($conn->sql_num_rows($sqlStmnt) > 0)
			{
			
				while ($conn->sql_fetch($sqlStmnt)) {
					if($processServerId ne "" && ($processServerId ne $sourceHostId))
					{
						# For passive node, directly move to AT Lun done
						&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_DONE, $initiator, $srcDeviceName, $sdLunId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;
					}
					else
					{
						# Call this if it's an active one
							&process_stop_protection(
								$conn, $initiator, $srcDeviceName, $sdLunId, $nodeHostId, $sourceHostId);
					}
				}
			}
			else
			{
				$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_stop_protection_requests::AT Port configuration failed.");
				&delete_protection_job($conn, $sdLunId, $sourceHostId);
			}
        }
    }
}

sub process_stop_protection
{
    my ($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId) = @_;

    $logging_obj->log("DEBUG","process_stop_protection($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId)");

    print "coming into process_stop_protection\n";

    my $applianceTargetLunMappingId;
    my $applianceTargetLunMappingNumber;
    my $sdLunId;
    
	my $hostIds = &node_being_uninstalled($conn);
	my @par_array = ($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId);
	my $par_array_ref = \@par_array;

    my $sqlStmnt = $conn->sql_query(
        "select distinct ".
        " applianceTargetLunMappingId, ".
        " applianceTargetLunMappingNumber ".
        "from hostApplianceTargetLunMapping ".
        "where exportedInitiatorPortWwn = ? and ".
        " srcDeviceName = ? and ".
        " sharedDeviceId = ? and " .
        " hostId = ? and " .
        " processServerId = ?",$par_array_ref);
     $logging_obj->log("DEBUG","exportedInitiatorPortWwn = $initiator, srcDeviceName = $srcDeviceName, sharedDeviceId = $sharedDeviceId, nodeHostId = $nodeHostId, processServerId = $processServerId\n");

    my @array = (\$applianceTargetLunMappingId, \$applianceTargetLunMappingNumber);
    $conn->sql_bind_columns($sqlStmnt,@array);

	if($conn->sql_num_rows($sqlStmnt) > 0)
	{

		while ($conn->sql_fetch($sqlStmnt)) 
		{
	print "coming inside while loop in stop protection\n";
	 $logging_obj->log("DEBUG","applianceTargetLunMappingId= $applianceTargetLunMappingId applianceTargetLunMappingNumber = $applianceTargetLunMappingNumber\n");
	
			my $count = scalar(@{$hostIds});
			$logging_obj->log("DEBUG","process_stop_protection - $count");
						
			if ($count > 0)
			{
				$logging_obj->log("DEBUG","process_stop_protection - ");
						
				if (grep $_ == $processServerId, @{$hostIds})
				{
					$logging_obj->log("DEBUG","process_stop_protection ProcessServer- $processServerId");
					my $hostStatus = &get_unregistered_host_status($processServerId);
					#
					# hostStatus 1=PS, 2=Target, 3=Source
					#
					
					if($hostStatus == 1) # if nodeType is PS then only we need to skip At lun deletion.
					{		
						&update_node_unregister_state($conn, Fabric::Constants::DELETE_AT_LUN_DONE, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;
						#&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_AT_LUN_DONE, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;		
					}
					else
					{
						&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_AT_LUN_PENDING, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;
					}
				}
				else
				{
					&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_AT_LUN_PENDING, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;
				}
			}
			else
			{
				&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_AT_LUN_PENDING, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;								   
			}

	    }
	}
	else
	{
		$logging_obj->log("DEBUG","process_stop_protection:: got zero rows in hostApplianceTargetLunMapping.\n");
		&delete_protection_job($conn, $sdLunId, $processServerId);
		&process_host_shared_device_mapping_delete_pending($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId);
	}
}


sub process_host_shared_device_mapping_delete_pending
{
    my ($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId) = @_;

    $logging_obj->log("DEBUG","process_host_shared_device_mapping_delete_pending($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId)");
    my @par_array = ($initiator, $srcDeviceName,  $sharedDeviceId, $nodeHostId);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "delete from  hostSharedDeviceMapping ".
        "where ".
        " sanInitiatorPortWwn = ? and ".
        " srcDeviceName = ? and ".
        " sharedDeviceId = ? and ".
        " hostId = ?",$par_array_ref);

    if (!$sqlStmnt
		&& !($conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED
             || $conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED_2)) 
	{
		$logging_obj->log("EXCEPTION","ERROR - process_host_shared_device_mapping_delete_pending ".$conn->sql_error());	     
		die $conn->sql_error();
    }

    &delete_shared_devices($conn, $sharedDeviceId);
}


sub process_host_shared_device_mapping_delete_pending_on_node_reboot
{
    my ($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId) = @_;

    $logging_obj->log("DEBUG","process_host_shared_device_mapping_delete_pending_on_node_reboot($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId)");
    my @par_array = (Fabric::Constants::DELETE_ENTRY_PENDING,
                            $initiator, $srcDeviceName,  $sharedDeviceId,
                            $nodeHostId);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "delete from  hostSharedDeviceMapping ".
        "where state = ? and ".
        " sanInitiatorPortWwn = ? and ".
        " srcDeviceName = ? and ".
        " sharedDeviceId = ? and ".
        " hostId = ?",$par_array_ref);

    if (!$sqlStmnt
		&& !($conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED
             || $conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED_2)) 
	{
		$logging_obj->log("EXCEPTION","ERROR - process_host_shared_device_mapping_delete_pending_on_node_reboot ".$conn->sql_error());	     
		die $conn->sql_error();
    }
}

sub delete_shared_devices
{
    my ($conn, $sharedDeviceId) = @_;

    $logging_obj->log("DEBUG","delete_shared_devices($sharedDeviceId)");
    my @par_array = ($sharedDeviceId);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "delete from sharedDevices ".
        "where sharedDeviceId = ?",$par_array_ref);

    # OK if delete failed because of foreign key constraints
    # as we try to avoid selects to make sure
    # things are OK to delete. we let the constraint
    # prevent us from doing the wrong thing
    if (!$sqlStmnt
		&& !($conn->sql_err()== $MYSQL_ER_ROW_IS_REFERENCED
             || $conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED_2))
	{
        $logging_obj->log("EXCEPTION","ERROR - delete_shared_devices ".$conn->sql_error());
		die $conn->sql_error();
    }
}


sub node_being_uninstalled
{
    my ($conn) = @_;
    $logging_obj->log("DEBUG","node_being_uninstalled");	
	my @hostIds;
	my $id;
    my @par_array = (Fabric::Constants::UNINSTALL_PENDING);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "SELECT ".
        "   id ".
        "FROM  ".
        "   hosts ".
        "WHERE  ".
        "   agent_state = ?",$par_array_ref);
		
    if ($conn->sql_num_rows($sqlStmnt) > 0)
	{
		my @array = (\$id);
		$conn->sql_bind_columns($sqlStmnt,@array);
        while($conn->sql_fetch($sqlStmnt))
		{
			push( @hostIds,$id);
		}
	}	
    $logging_obj->log("DEBUG","node_being_uninstalled::".scalar(@hostIds));	
    return \@hostIds;
}


sub update_host_appliance_target_lun_mapping_delete_state
{
    my ($conn, $nextState, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, @inStates) = @_;

    $logging_obj->log("DEBUG","update_host_appliance_target_lun_mapping_delete_state($nextState, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, @inStates)");

    my $inStates = join(',', @inStates);

    $logging_obj->log("DEBUG","instates: $inStates");	
    my @par_array = ($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query(
        "select sharedDeviceId ".
        " from hostApplianceTargetLunMapping ".
        " where ".
        "  exportedInitiatorPortWwn = ? and ".
        "  srcDeviceName = ? and ".
        "  sharedDeviceId = ? and ".
        "  hostId = ? and ".
        "  processServerId = ?",$par_array_ref);


      if (0 == $conn->sql_num_rows($sqlStmnt)) {
		return 0;
    }

    # TODO: for now just go forward on the deletion even if there
    # are reported errors
    my @par_array = ($nextState, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId);
	my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ?, ".
        " status = 0 ".
        " where ".
        "  (state in ($inStates) or state < 0) and ".
        "  exportedInitiatorPortWwn = ? and ".
        "  srcDeviceName = ? and ".
        "  sharedDeviceId = ? and ".
        "  hostId = ? and ".
        "  processServerId = ?",$par_array_ref);


    return 1;
}

sub update_node_unregister_state
{
    my ($conn, $nextState, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, @inStates) = @_;

    $logging_obj->log("DEBUG","update_node_unregister_state($nextState, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, @inStates)");

	my $inStates = join(',', @inStates);

    $logging_obj->log("DEBUG","instates: $inStates");	
    my @par_array = ($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query(
        "select sharedDeviceId ".
        " from hostApplianceTargetLunMapping ".
        " where ".
        "  exportedInitiatorPortWwn = ? and ".
        "  srcDeviceName = ? and ".
        "  sharedDeviceId = ? and ".
        "  hostId = ? and ".
        "  processServerId = ?",$par_array_ref);


      if (0 == $conn->sql_num_rows($sqlStmnt)) {
		return 0;
    }

    # TODO: for now just go forward on the deletion even if there
    # are reported errors
    my @par_array = ($nextState, $sharedDeviceId);
	my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ?, ".
        " status = 0 ".
        " where ".
        "  (state in ($inStates) or state < 0) and ".
        "  sharedDeviceId = ? ",$par_array_ref);


    return 1;
	
}

sub delete_protection_job
{

    my ($conn, $sdLunId, $processServerId) = @_;
    $logging_obj->log("DEBUG","delete_protection_job($sdLunId, $processServerId)");

    my $srcDev;
    my $dstHostId;
    my $dstDev;

    if (&ok_to_delete_protection_job($conn, $sdLunId) || !$processServerId) 
	{


        my @par_array = ($sdLunId, Fabric::Constants::STOP_PROTECTION_PENDING, Fabric::Constants::SOURCE_DELETE_PENDING, $processServerId);
		my $par_array_ref = \@par_array;

		my $sqlStmnt = $conn->sql_query(
            "select sourceDeviceName, destinationHostId, destinationDeviceName ".
            "from srcLogicalVolumeDestLogicalVolume ".
            "where Phy_lunid = ? ".
            " and lun_state = ? ".
			" and replication_status = ? and ".
            " sourceHostId = ?",$par_array_ref);


        if ($conn->sql_num_rows($sqlStmnt) > 0) 
		{

            my @array = (\$srcDev, \$dstHostId, \$dstDev );
            $conn->sql_bind_columns($sqlStmnt,@array);

            $conn->sql_fetch($sqlStmnt);

           # itl_trace("stop_replication.php  $processServerId, $srcDev, $dstHostId, $dstDev");
            `cd $AMETHYST_VARS->{WEB_ROOT}/ui/; php stop_replication.php  "$processServerId" "$srcDev" "$dstHostId" "$dstDev"`;

            #&validate_protection_job_deleted($conn, $sdLunId, $processServerId, $srcDev, $dstHostId, $dstDev);
        }


	#my @par_array = ($sdLunId);
	#my $par_array_ref = \@par_array;
	#$sqlStmnt = $conn->sql_query(
	#	"select Phy_Lunid  ".
	#	"from srcLogicalVolumeDestLogicalVolume ".
	#	"where Phy_lunid = ? ",$par_array_ref);

	#	if (0 == $conn->sql_num_rows($sqlStmnt)) 
	#	{

	#		my @par_array = ($sdLunId);
	#		my $par_array_ref = \@par_array;
	#		$sqlStmnt = $conn->sql_query(
	#				"delete from logicalVolumes ".
	#				"where Phy_lunid = ? " ,$par_array_ref);
        #}
    }
}


sub validate_protection_job_deleted
{

    my ($conn, $sdLunId, $processServerId, $srcDev, $dstHostId, $dstDev) = @_;
    $logging_obj->log("DEBUG","validate_protection_job_deleted($sdLunId, $processServerId, $srcDev, $dstHostId, $dstDev)");

	 my @par_array = ($sdLunId, Fabric::Constants::STOP_PROTECTION_PENDING,Fabric::Constants::SOURCE_DELETE_PENDING,$processServerId, $srcDev, $dstHostId, $dstDev);
	 my $par_array_ref = \@par_array;
	 my $sqlStmnt = $conn->sql_query(
        "select Phy_Lunid ".
        "from srcLogicalVolumeDestLogicalVolume ".
        "where Phy_lunid = ? and ".
        " lun_state = ? and ".
        " replication_status = ? and ".
		" sourceHostId = ? and ".
        " sourceDeviceName = ? and ".
        " destinationHostId = ? and ".
        " destinationDeviceName = ?",$par_array_ref);

	die "delete from srcLogicalVolumeDestLogicalVolume failed" unless (0 == $conn->sql_num_rows($sqlStmnt));

}


sub ok_to_delete_protection_job
 {

    my ($conn, $sdLunId) = @_;
    $logging_obj->log("DEBUG","ok_to_delete_protection_job($sdLunId)");

    my $state;


    my @par_array = ($sdLunId);
	my $par_array_ref = \@par_array;
	  my $sqlStmnt = $conn->sql_query(
        "select sharedDeviceId ".
        "from hostApplianceTargetLunMapping ".
        "where sharedDeviceId = ?",$par_array_ref);

    if ($conn->sql_num_rows($sqlStmnt) > 0) 
	{
		if (Array::CommonProcessor::san_lun_one_to_n($conn, $sdLunId)) 
		{
            # OK to delete assumes that one of the 1-to-n pairs is being deleted
            # so nothing was removed from hostApplianceTargetLunMapping in that case
            return 1;
        }
        # don't delete it yet other ITL for this lun still in use
        return 0;
    }


    my @par_array = ($sdLunId, Fabric::Constants::STOP_PROTECTION_PENDING);
	my $par_array_ref = \@par_array;
	$sqlStmnt = $conn->sql_query(
        "select lun_state ".
        "from srcLogicalVolumeDestLogicalVolume ".
        "where Phy_Lunid = ?".
        " and lun_state != ?",$par_array_ref);

	if ($conn->sql_num_rows($sqlStmnt) > 0) 
	{
		my @array = (\$state);
		$conn->sql_bind_columns($sqlStmnt, @array);
		$conn->sql_fetch($sqlStmnt);
        # finally check if we need to update the protection job state to reflect no

		my $sqlStmnt = $conn->sql_query(
            "select applianceFrBindingTargetPortWwn ".
            "from applianceFrBindingTargets ".
            " where state = 'RebootReconfigureFailed'");

        if ($conn->sql_num_rows($sqlStmnt) > 0) 
		{
			Array::CommonProcessor::set_protection_job_state_to_error($conn, $sdLunId, $state, Fabric::Constants::APPLIANCE_TARGETS_NOT_CONFIGURED);
            return 0; # don't delete it yet
        }
    }

    # note at this point the protection may still not get deleted but it is ok to try
    return 1;
}




# sub process_src_logical_volume_dest_logical_volume_start_protection_requests
# {
	# my ($conn) = @_;
	# $logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_start_protection_requests::$conn");
	
	# my @par_array = (
					# Fabric::Constants::START_PROTECTION_PENDING,
					# Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING,
					# Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING,
					# Fabric::Constants::REBOOT_PENDING,
					# 0
				  # );
	# my $fabricId = Fabric::Constants::PRISM_FABRIC;			  
				  
	# my $processServerId;
	# my $state;
	# my $sharedDeviceId;
	# my $initiator;
	# my $srcDeviceName;
	# my $nodeHostId;

	
	# my $par_array_ref = \@par_array;
	# my $sqlStmnt = $conn->sql_query(
						# "SELECT DISTINCT ".
						# " sd.sourceHostId, ".
						# " sd.lun_state, ".
						# " hsdm.sharedDeviceId, ".
						# " hsdm.sanInitiatorPortWwn, ".
						# " hsdm.srcDeviceName, ".
						# " hsdm.hostId ".
						# " FROM ".
						# " srcLogicalVolumeDestLogicalVolume sd, ".
						# " hostSharedDeviceMapping hsdm ".
						# " WHERE ".
						# " lun_state in (?,?,?,?) ".
						# " AND sd.Phy_Lunid = hsdm.sharedDeviceId ".
						# " AND sd.replication_status = ? ".
						# " ORDER BY sharedDeviceId", $par_array_ref);
	# my @array = (\$processServerId, \$state, \$sharedDeviceId, \$initiator, \$srcDeviceName, \$nodeHostId);
	# $conn->sql_bind_columns($sqlStmnt,@array);
	
	# print "$processServerId,  $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId";	

	# while($conn->sql_fetch($sqlStmnt))
	# {
		# &protect_device($conn, $processServerId, $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId);
	
	# }
# }

# sub protect_device
# {
	# my ($conn, $processServerId, $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId) = @_;
	# $logging_obj->log("DEBUG","protect_device:: $processServerId, $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId");
	# my $protectAction = &get_one_to_n_protection_action($conn, $sharedDeviceId);
	# my $pathExists = &device_exists_in_host_appliance_target_lun_mapping($conn, $processServerId, $sharedDeviceId, $initiator, $srcDeviceName);
	
	# if (ITL_PROTECTOR_UPDATE_ONLY == $protectAction && $pathExists) 
	# {
        # &finish_start_protection($conn, $sharedDeviceId, $processServerId, $nodeHostId, Fabric::Constants::NO_WORK);
   	# }
	# elsif (ITL_PROTECTOR_SKIP != $protectAction) 
	# {
        # if (!$pathExists) 
		# {
            # we could limit the number of times get_appliance_initiator_infos and get_appliance_target_info
            # are called by tracking fabricId, processServerId and lunId, but since setting up jobs doesn't
            # happen that often, we'll keep things simple and always get them
            # my @applianceInitiatorInfos;
            # &get_appliance_initiator_infos($conn, $sharedDeviceId, $fabricId, $processServerId, \@applianceInitiatorInfos);

            # my @applianceTargetInfos;
            # &get_appliance_target_infos($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId, \@applianceTargetInfos);

            # &process_start_protection(
                # $conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
                # $processServerId, \@applianceInitiatorInfos, \@applianceTargetInfos);
        # }
    # }
# }

# sub process_start_protection
# {
    # my ($conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
        # $processServerId, $applianceInitiators, $applianceTargets) = @_;

	# my $syncDeviceName;	
    # $logging_obj->log("DEBUG","process_start_protection($initiator, $sharedDeviceId, $fabricId, $processServerId, $applianceInitiators, $applianceTargets)");
	# $syncDeviceName = &get_sync_device_name($conn, $sharedDeviceId, $processServerId);
	# if(!$syncDeviceName)
	# {
		# die "Sync device not avaiable for sharedDevice=$sharedDeviceId and processServerId=$processServerId";
	# }
 # $logging_obj->log("DEBUG","process_start_protection::: $syncDeviceName");	
    # foreach my $aiInfo (@{$applianceInitiators}) 
	# {
       # foreach my $atInfo (@{$applianceTargets}) 
		# {
		
			# my @par_array = ($nodeHostId, $sharedDeviceId, $atInfo->{lunId}, $atInfo->{lunNumber}, 
							  # $initiator, $atInfo->{acgId}, $processServerId, $aiInfo, $atInfo->{portWwn},  
							 # $fabricId, $srcDeviceName, $syncDeviceName, Fabric::Constants::CREATE_AT_LUN_PENDING);
			
			# my $par_array_ref = \@par_array;
			# my $sqlStmnt = $conn->sql_query(
					 # "insert into hostApplianceTargetLunMapping ".
					 # "(hostId,".
					 # "sharedDeviceId,".
					 # "applianceTargetLunMappingId,".
					 # "applianceTargetLunMappingNumber,".
					 # "exportedInitiatorPortWwn,".
					 # "accessControlGroupId,".
					 # "processServerId,".
					 # "applianceInitiatorPortWwn,".
					 # "applianceTargetPortWwn,".
					 # "fabricId,".
					 # "srcDeviceName,".
					 # "syncDeviceName,".
					 # "state) ".
					 # " values(?,?,?,?,?,?,?,?,?,?,?,?,?)",$par_array_ref);
			
				 # $logging_obj->log("DEBUG","process_start_protection 4");

				# if (!$sqlStmnt
					# && !($conn->sql_err() == $MYSQL_ER_DUP_KEY
						 # || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
						 # || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) 
				# {
							# $logging_obj->log("EXCEPTION","ERROR - process_start_protection ".$conn->sql_error());
							# die $conn->sql_error();
						# }
        # }
    # }
# }

# sub get_sync_device_name
# {
	# my($conn, $sharedDeviceId, $processServerId) = @_;
	# my $syncDeviceName = Fabric::Constants::INITIAL_LUN_DEVICE_NAME.$sharedDeviceId;
	# $logging_obj->log("DEBUG","get_sync_device_name::  $sharedDeviceId, $processServerId, $syncDeviceName");
	
	# my @par_array = ($sharedDeviceId, $processServerId, $syncDeviceName);
	# my $par_array_ref = \@par_array;

	# my $sql = "select ".
        # " deviceName ".
        # "from ".
        # " logicalVolumes ".
        # "where ".
        # " hostId = '$processServerId' and ".
        # " Phy_Lunid = '$sharedDeviceId' and ".
		# " deviceName = '$syncDeviceName'";
	# $logging_obj->log("DEBUG","get_sync_device_name syncDeviceName2222:: $sql");	
	
	# my $sqlStmnt = $conn->sql_query($sql);
	
	# $logging_obj->log("DEBUG","get_sync_device_name syncDeviceName111::". $conn->sql_num_rows($sqlStmnt));	
	# if ($conn->sql_num_rows($sqlStmnt) > 0) 
	# {
		
		# $logging_obj->log("DEBUG","get_sync_device_name syncDeviceName::  $syncDeviceName");
		# return $syncDeviceName;
	# }

# }


# sub get_appliance_target_infos
# {

    # my ($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId, $applianceTargets) = @_;

    # $logging_obj->log("DEBUG","get_appliance_target_infos($initiator, $sharedDeviceId, $fabricId, $processServerId, $applianceTargets)");

    # my $at_state_to_error = Fabric::Constants::START_PROTECTION_PENDING .
    # "," . Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING .
    # "," . Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING;

    # my $applianceTargetLunId;
    # my $applianceTarget;
    # my $applianceTargetLunNumber;

    # &create_appliance_target_lun_mapping_info($conn, $sharedDeviceId, $fabricId, $processServerId);
    # my $accessControlGroupId = &create_access_control_group_info($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId);
	
    # my $sqlStmnt = &get_appliance_target_lun_info($conn, $sharedDeviceId, $fabricId, $processServerId);

    # if (0 == $conn->sql_num_rows($sqlStmnt)) 
	# {
        # &set_protection_job_state_to_error($conn, $sharedDeviceId, $at_state_to_error,
                                           # Fabric::Constants::APPLIANCE_TARGETS_NOT_CONFIGURED);

        # $logging_obj->log("DEBUG","ERROR - Appliance targets for fr binding are not configured");
		#die "Appliance targets for fr binding are not configured";
    # }


    # my @array = (\$applianceTargetLunId, \$applianceTarget, \$applianceTargetLunNumber);
    # $conn->sql_bind_columns($sqlStmnt,@array);
    # while ($conn->sql_fetch($sqlStmnt)) 
	# {
        # push (@{$applianceTargets},
              # {portWwn => $applianceTarget, lunId => $applianceTargetLunId, 
			  # lunNumber => $applianceTargetLunNumber, acgId => $accessControlGroupId});
    # }
# }

# sub create_access_control_group_info
# {
	# my ($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId) = @_;
    # $logging_obj->log("DEBUG","create_access_control_group_info($initiator, $sharedDeviceId, $fabricId, $processServerId)");
	
	# my $insertState = Fabric::Constants::CREATE_ACG_DONE;
	
	# my @par_array = ($initiator);
	# my $par_array_ref = \@par_array;

	# my $sqlStmnt = $conn->sql_query(
        # "select ".
        # " acg.accessControlGroupId ".
        # "from ".
        # " accessControlGroup acg, ".
        # " accessControlGroupPorts acgp ".
        # "where ".
        # " acgp.exportedInitiatorportWwn = ? and ".
        # " acg.accessControlGroupId = acgp.accessControlGroupId ",$par_array_ref);
    # if (0 == $conn->sql_num_rows($sqlStmnt)) 
	# {
		
		# my $accessControlGroupId = `uuidgen`;
		 # chomp($accessControlGroupId);	
		# my $accessControlGroupName = $initiator;
		
		# $logging_obj->log("DEBUG","create_access_control_group_info::accessControlGroupId=$accessControlGroupId,accessControlGroupName=$accessControlGroupName");
		# my @par_array = ($accessControlGroupId, $accessControlGroupName, $insertState);
		# my $par_array_ref = \@par_array;
		
		# my $insertSqlStmnt = $conn->sql_query(
			# "insert into ".
			# " accessControlGroup ".
			# "(accessControlGroupId,".
			# " accessControlGroupName,".
			# " state) ".
			# "values (?, ?, ?)",$par_array_ref);


		# if (!$insertSqlStmnt
		# && !($conn->sql_err() == $MYSQL_ER_DUP_KEY
			 # || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
			 # || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) 
		# {
            # $logging_obj->log("EXCEPTION","ERROR - create_access_control_group_info ".$conn->sql_error());
			# die $conn->sql_error();
        # }
		
		# my @par_array = ($initiator, $accessControlGroupId);
		# my $par_array_ref = \@par_array;
		
		# my $sqlStmnt1 = $conn->sql_query(
			# "insert into ".
			# " accessControlGroupPorts ".
			# "(exportedInitiatorportWwn,".
			# " accessControlGroupId) ".
			# "values (?, ?)",$par_array_ref);


		# if (!$sqlStmnt1
		# && !($conn->sql_err() == $MYSQL_ER_DUP_KEY
			 # || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
			 # || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) 
		# {
            # $logging_obj->log("EXCEPTION","ERROR - create_access_control_group_info ".$conn->sql_error());
			# die $conn->sql_error();
        # }
		
		# return $accessControlGroupId;
	# }
	# else
	# {
		# my $accessControlGroupId;
		# my @array = (\$accessControlGroupId);
		# $conn->sql_bind_columns($sqlStmnt,@array);
		# $conn->sql_fetch($sqlStmnt); 
		# $logging_obj->log("DEBUG","create_access_control_group_info::existing acgID :$accessControlGroupId");
		# return $accessControlGroupId;
	# }
	
# }



# sub create_appliance_target_lun_mapping_info
# {

    # my ($conn, $sharedDeviceId, $fabricId, $processServerId) = @_;
    # $logging_obj->log("DEBUG","create_appliance_target_lun_mapping_info($sharedDeviceId, $fabricId, $processServerId)");

    # my $applianceTarget;
    # my $rc = 0;
    # my @par_array = ($fabricId, $processServerId);
	# my $par_array_ref = \@par_array;

	  # my $sqlStmnt = $conn->sql_query(
        # "select ".
        # " applianceFrBindingTargetPortWwn ".
        # "from ".
        # " applianceFrBindingTargets ".
        # "where ".
        # " fabricId = ? and ".
        # " processServerId = ? and ".
        # " state = 'stable' and ".
        # " error = 0",$par_array_ref);
    # my @array = (\$applianceTarget);
    # $conn->sql_bind_columns($sqlStmnt,@array);
    # while ($conn->sql_fetch($sqlStmnt)) 
	# {
        # my ($atLunId, $atLunNumber) = get_appliance_lun_info_for_shared_devices($conn, $sharedDeviceId, $processServerId);
        # if (!$atLunId)
		# {
            # $atLunId = &get_next_appliance_target_lun_id($conn);
            # $atLunNumber = &get_next_appliance_target_lun_number($conn, $processServerId);
        # }

         # my @par_array = ($atLunId, $sharedDeviceId, $atLunNumber, $processServerId);
         # my $par_array_ref = \@par_array;

        # my $insertSqlStmnt = $conn->sql_query(
        # "insert into ".
        # " applianceTargetLunMapping ".
        # "(applianceTargetLunMappingId,".
        # " sharedDeviceId,".
        # " applianceTargetLunMappingNumber, ".
        # " processServerId) ".
        # "values (?, ?, ?, ?)",$par_array_ref);


            # if (!$insertSqlStmnt
            # && !($conn->sql_err() == $MYSQL_ER_DUP_KEY
                 # || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
                 # || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) {
            # $logging_obj->log("EXCEPTION","ERROR - create_appliance_target_lun_mapping_info ".$conn->sql_error());
			# die $conn->sql_error();
        # }
    # }
# }

# sub get_next_appliance_target_lun_number
# {

    # my ($conn, $processServerId) = @_;
    # $logging_obj->log("DEBUG","get_next_appliance_target_lun_number($processServerId)");

     # my $nextLunNumber;

     # my @par_array = ($processServerId, $processServerId, $processServerId);
	 # my $par_array_ref = \@par_array;

	  # my $sqlStmnt = $conn->sql_query(
         # "(select distinct applianceTargetLunNumber as nextLunNumber ".
         # "from applianceTLNexus ".
         # "where processServerId = ?) ".
         # "union ".
         # "(select distinct exportedLunNumber as nextLunNumber ".
         # "from exportedTLNexus ".
         # "where hostId = ?) ".
		 # "union ".
         # "(select distinct applianceTargetLunMappingNumber as nextLunNumber ".
         # "from applianceTargetLunMapping ".
         # "where processServerId = ?) ".
         # "order by nextLunNumber",$par_array_ref);

     # my $currentLunNumber = 0;


     # my @array = (\$nextLunNumber);
     # $conn->sql_bind_columns($sqlStmnt,@array);
	# while ($conn->sql_fetch($sqlStmnt)) 
	# {
        	# if ($nextLunNumber - $currentLunNumber > 1) 
		# {
             	# return $currentLunNumber + 1;
         	# }
         # $currentLunNumber = $nextLunNumber;
	# }

     # return $currentLunNumber + 1;
# }


# sub get_next_appliance_target_lun_id
# {
    # my ($conn) = @_;

    # $logging_obj->log("DEBUG","get_next_appliance_target_lun_id");

    # my $nextLunId;
    # my $lunIdPrefix;
    # my $sqlStmnt;

    # $sqlStmnt = $conn->sql_query(
        # "update nextApplianceLunIdLunNumber ".
        # " set nextLunId = nextLunId + 1");

    # $sqlStmnt = $conn->sql_query(
        # "select nextLunId, lunIdPrefix ".
        # "from nextApplianceLunIdLunNumber");

    # my @array = (\$nextLunId,\$lunIdPrefix);
    # $conn->sql_bind_columns($sqlStmnt,@array);

     # if ($conn->sql_num_rows($sqlStmnt) == 0) {
        # die "no entries found in nextApplianceLunIdLunNumber.";
    # }

    # $conn->sql_fetch($sqlStmnt);
    # return sprintf("%s%010d", $lunIdPrefix, $nextLunId);
# }



# sub get_appliance_lun_info_for_shared_devices
# {
    # my ($conn, $sharedDeviceId, $processServerId) = @_;

    # my $sqlStmnt;
	# $logging_obj->log("DEBUG","get_appliance_lun_info_for_shared_devices($sharedDeviceId, $processServerId)");
	
	# my @arrayInfo = ($sharedDeviceId, $processServerId,Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING);
   	# my $arrayInfoRef = \@arrayInfo;
	# my $srcSqlStmnt = $conn->sql_query("
								# select 
								# lun_state  
								# from 
								# srcLogicalVolumeDestLogicalVolume 
								# where 
								# Phy_Lunid = ? and 
								# sourceHostId = ? and 
								# lun_state = ?",$arrayInfoRef);
	# if ($conn->sql_num_rows($srcSqlStmnt) > 0)
	# {
		# my @par_array = ($sharedDeviceId);
		# my $par_array_ref = \@par_array;

		# $sqlStmnt = $conn->sql_query(
			# "select ".
			# " applianceTargetLunMappingId,".
			# " applianceTargetLunMappingNumber ".
			# "from ".
			# " applianceTargetLunMapping ".
			# "where ".
			# " sharedDeviceId = ? ".
			# "limit 1",$par_array_ref);
	# }
	# else
	# {
		# my @par_array = ($sharedDeviceId, $processServerId);
		# my $par_array_ref = \@par_array;

		# $sqlStmnt = $conn->sql_query(
			# "select ".
			# " applianceTargetLunMappingId,".
			# " applianceTargetLunMappingNumber ".
			# "from ".
			# " applianceTargetLunMapping ".
			# "where ".
			# " sharedDeviceId = ? and processServerId = ? ".
			# "limit 1",$par_array_ref);
	# }
	
	# my ($atLunId, $atLunNumber);
	# my @array = (\$atLunId, \$atLunNumber);
	   
	# $conn->sql_bind_columns($sqlStmnt, @array);

	# $conn->sql_fetch($sqlStmnt);
      
    # return ($atLunId, $atLunNumber);
# }

# sub get_appliance_target_lun_info
# {

    # my ($conn, $sharedDeviceId, $fabricId, $processServerId) = @_;

    # $logging_obj->log("DEBUG","get_appliance_target_lun_info($fabricId, $sharedDeviceId, $processServerId)");

    # my @par_array = ($fabricId, $sharedDeviceId, $processServerId);
	# my $par_array_ref = \@par_array;

	# my $sqlStmnt = $conn->sql_query(
        # "select ".
        # " atlm.applianceTargetLunMappingId,".
        # " afbt.applianceFrBindingTargetPortWwn, ".
		# " atlm.applianceTargetLunMappingNumber ".
        # "from ".
        # " applianceFrBindingTargets afbt, ".
        # " applianceTargetLunMapping atlm ".
        # "where ".
        # " afbt.fabricId = ? and ".
        # " afbt.processServerId  = atlm.processServerId and ".
        # " atlm.sharedDeviceId  = ? and ".
        # " afbt.processServerId = ? ",$par_array_ref);
	
	
	# return $sqlStmnt;
# }

# sub get_appliance_initiator_infos
# {
    # my ($conn, $sharedDeviceId, $fabricId, $processServerId, $applianceInitiators) = @_;


    # my $ai_state_to_error = Fabric::Constants::START_PROTECTION_PENDING .
    # "," . Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING .
    # "," . Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING;

    # $logging_obj->log("DEBUG","get_appliance_initiator_infos($sharedDeviceId $fabricId, $processServerId, $applianceInitiators)");

    # my $applianceInitiator;

    # my @par_array = ($fabricId, $processServerId);
	# my $par_array_ref = \@par_array;
	  # my $sqlStmnt = $conn->sql_query(
        # "select ".
        # " applianceFrBindingInitiatorPortWwn ".
        # "from ".
        # " applianceFrBindingInitiators ".
        # "where ".
        # " fabricId = ? and".
        # " processServerId = ? and ".
        # " state = 'stable' and ".
        # " error = 0",$par_array_ref);


	# if (0 == $conn->sql_num_rows($sqlStmnt)) {
        # &set_protection_job_state_to_error($conn, $sharedDeviceId, $ai_state_to_error,
                                           # Fabric::Constants::APPLIANCE_INITIATORS_NOT_CONFIGURED);

        # $logging_obj->log("DEBUG","Appliance initiators for fr binding are not configured");
		#die "Appliance initiators for fr binding are not configured";
    # }


    # my @array = (\$applianceInitiator);
    # $conn->sql_bind_columns($sqlStmnt,@array);
    # while ($conn->sql_fetch($sqlStmnt)) {
        # push (@{$applianceInitiators}, $applianceInitiator);
    # }
# }

# sub set_protection_job_state_to_error
# {

    # my ($conn, $sharedDeviceId, $currentState, $errorState) = @_;
    # $logging_obj->log("DEBUG","set_protection_job_state_to_error($sharedDeviceId, $currentState, $errorState)");

    # my @par_array = ($errorState, $sharedDeviceId, $currentState);
	# my $par_array_ref = \@par_array;
	   # my $sqlStmnt = $conn->sql_query(
        # "update srcLogicalVolumeDestLogicalVolume ".
        # " set lun_state = ? ".
        # "where Phy_lunid = ? ".
        # " and lun_state in (?)",$par_array_ref);

# }


# sub finish_start_protection
# {
	# my ($conn, $sharedDeviceId, $processServerId,  $nodeHostId, $currentState) = @_;
    # $logging_obj->log("DEBUG","finish_start_protection($sharedDeviceId, $processServerId, $nodeHostId, $currentState)");

    # if (Fabric::Constants::NO_WORK != $currentState) 
	# {
       # entries were added so we can now set them to protected
        # &update_host_appliance_target_lun_mapping_state_for_lun($conn, $sharedDeviceId, $processServerId, $nodeHostId, $currentState, Fabric::Constants::PROTECTED);
    # }

    # if (!&all_host_appliance_target_lun_mapping_lun_ready_for_protection($conn, $sharedDeviceId, $processServerId)) 
	# {
        # return;
    # }
	
	# my $pairState;	
	# my @info_array = ($sharedDeviceId, $processServerId);
 	# my $par_array_ref = \@info_array;
	# my $sqlStmnt = $conn->sql_query("select lun_state from srcLogicalVolumeDestLogicalVolume ".
	# "where Phy_Lunid = ? and sourceHostId = ? ",$par_array_ref);
	# my @array=(\$pairState);
	# $conn->sql_bind_columns($sqlStmnt, @array);
	# $conn->sql_fetch($sqlStmnt);
	# if($pairState != Fabric::Constants::NODE_REBOOT_OR_PATH_ADDITION_PENDING)
	# {
		# $logging_obj->log("DEBUG","finish_start_protection::pairState::$pairState");
		# my @par_array = (&get_should_resync_value($conn, $sharedDeviceId, $processServerId), time, 0, $sharedDeviceId, $processServerId,
							  # Fabric::Constants::START_PROTECTION_PENDING,
							  # Fabric::Constants::PROTECTED,
							  # Fabric::Constants::REBOOT_PENDING,
							  # Fabric::Constants::PROFILER_HOST_ID,
							  # Fabric::Constants::PROFILER_DEVICE_NAME);
		# my $par_array_ref = \@par_array;

		# my $sdvSqlStmnt = $conn->sql_query(
			# "update srcLogicalVolumeDestLogicalVolume ".
			# " set ".
			# "  shouldResync = ?, ".
			# "  resyncSetCxtimestamp = ?, ".
			# "  ShouldResyncSetFrom = ? ".
			# "where Phy_lunid = ? and sourceHostId = ?" .
			# " and lun_state in(?,?,?) ".
			# " and destinationHostId != ? " .
			# " and destinationDeviceName != ?",$par_array_ref);

	# }
	# else
	# {
		# $logging_obj->log("DEBUG","finish_start_protection else::pairState::$pairState");
	# }
    
    # For LUN resize, final confirmation mail should be set
    # from here based on lun_state before it became PROTECTED
    

	# &update_and_send_email_alerts_for_lun_resize($conn, $sharedDeviceId);
    # my @par_array = (Fabric::Constants::PROTECTED, $sharedDeviceId, $processServerId, Fabric::Constants::STOP_PROTECTION_PENDING);
	# my $par_array_ref = \@par_array;

	# my $sdvSqlUpdateStateStmnt = $conn->sql_query(
        # "update srcLogicalVolumeDestLogicalVolume ".
        # "set lun_state = ? ".
        # "where Phy_lunid = ? and sourceHostId = ? and
		# lun_state != ?",$par_array_ref);

# }

# sub update_and_send_email_alerts_for_lun_resize
# {

    # my ($conn, $sharedDeviceId) = @_;
	# $logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize");
    # my ($sourceName, $sourceIp, $sourceDevice, $sourceHostId,$nodeHostId);
    # my ($targetName, $targetIp, $targetDevice,$targetId);
	# my @idList;

    # my @par_array = ($sharedDeviceId,Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING);
	# my $par_array_ref = \@par_array;

	# my $sqlStmnt = $conn->sql_query(
				   # "SELECT ".
					# "app.sourceId as nodeId, ".
					# "sd.sourceHostId , ".
					# "sd.destinationHostId as tgtId, ".
					# "sd.destinationDeviceName as tgtName ".	
					# "FROM ".
					# "applicationScenario app, ".
					# "srcLogicalVolumeDestLogicalVolume sd  ".
					# "WHERE ".
					# "app.scenarioId=sd.scenarioId AND  ".
					# "sd.Phy_Lunid = ? AND ".
					# " sd.lun_state = ?",$par_array_ref);
		

    # if ($conn->sql_num_rows($sqlStmnt) > 0)
	# {
        
       # Send mail with reconfiguration success
        

       # my @array = (\$nodeHostId, \$sourceHostId, \$targetId, \$targetDevice);
       # $conn->sql_bind_columns($sqlStmnt,@array);

       # my $errId = $sourceHostId;
        
		# my (@source_host_name_list,@source_ip_address_list);
		# my ($source_host_name,$source_ip_address,$nodeId);
		# my ($source_host,$source_ip);
        # while ($conn->sql_fetch($sqlStmnt))
        # {
            # $logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::$nodeHostId, $sourceHostId, $targetId, $targetDevice");
			# @idList = split(",",$nodeHostId);
			# $targetName = &getHostName($conn,$targetId);
			# $targetIp = &getHostIpaddress($conn,$targetId);
			# $logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::$targetIp, $targetName");
			# @source_ip_address_list = ();
			# @source_host_name_list = ();
			# $source_host_name = "";
			# $source_ip_address = "";
			# foreach $nodeId (@idList)
			# {
				# $source_host = &getHostName($conn,$nodeId);
				# push(@source_host_name_list,$source_host);
				# $source_ip = &getHostIpaddress($conn,$nodeId);
				# push(@source_ip_address_list,$source_ip);
			# }
			# $source_host_name = join(",",@source_host_name_list);
			# $source_ip_address = join(",",@source_ip_address_list);
			
			# $sourceDevice = &getNodeDeviceName($conn,$sharedDeviceId);
			# $logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::source_host_name:$source_host_name,source_ip_address:$source_ip_address");
			# $logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::getNodeDeviceName:$sourceDevice");
		# }	
			
			# my $errSummary = "Source LUN Resize Warning";
			# my $message = "The source Device(s)($sourceDevice) has been reconfigured upon resize and the following replication pair is paused. Please resize your target LUN to greater than or equal to source LUN and then resume the replication pair (Not applicable for profiler targets).";
			# $message = $message."<br>Pair Details: <br><br>";
            # $message = $message."Source Host                  : $source_host_name <br>";
            # $message = $message."Source IP                    : $source_ip_address <br>";
            # $message = $message."Source Volume                : $sourceDevice <br>";
            # $message = $message."Destination Host             : $targetName <br>";
            # $message = $message."Destination IP               : $targetIp <br>";
            # $message = $message."Destination Volume           : $targetDevice <br>";

            # $message = $message."===============================================<br>";
        

		# $logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::add_error_message- $sourceHostId, $errSummary, $message ");
		# TimeShotManager::add_error_message ($conn, $sourceHostId, "VOL_RESIZE", $errSummary, $message);
	
		# my $sourceDeviceName = Fabric::Constants::INITIAL_LUN_DEVICE_NAME.$sharedDeviceId;
		# $logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::update_agent_log- $sourceHostId, $sourceDeviceName, $WEB_ROOT, $message ");
        # Utilities::update_agent_log($sourceHostId, $sourceDeviceName, $WEB_ROOT, $message);
    # }


    # my @par_array = (Fabric::Constants::CAPACITY_CHANGE_RESET,$sharedDeviceId,Fabric::Constants::CAPACITY_CHANGE_PROCESSING);
	# my $par_array_ref = \@par_array;
    # my $sdvSqlUpdateStateStmnt = $conn->sql_query(
        # "update sharedDevices ".
        # "set volumeResize = ? ".
        # "where sharedDeviceId = ? and volumeResize = ?",$par_array_ref);
# }



# sub get_should_resync_value
# {
    # my ($conn, $sharedDeviceId, $processServerId) = @_;
    # my ($sourceDeviceName,$agentLogString);

    # my ($destinationHostId);
    # my @par_array = ($sharedDeviceId, $processServerId, 0);
    # my $par_array_ref = \@par_array;
	# my $sqlStmnt = $conn->sql_query("select distinct destinationHostId from srcLogicalVolumeDestLogicalVolume ".
	# "where Phy_lunid = ? and sourceHostId = ? and resyncStartTagtime = ?",$par_array_ref);
	
	# if ($conn->sql_num_rows($sqlStmnt) > 0)
	# {

		# return 1;
	# }
	# else
	# {
	    # my @array = (\$destinationHostId );
	    # $conn->sql_bind_columns($sqlStmnt,@array);
		# $conn->sql_fetch($sqlStmnt);
		# if($destinationHostId ne Fabric::Constants::PROFILER_HOST_ID)
		# {
			
			# Need to log this, as once pair moves to diff sync
			# resync required flag will be set
			
			# $sourceDeviceName = Fabric::Constants::INITIAL_LUN_DEVICE_NAME.$sharedDeviceId;
			# $agentLogString = "Resync required flag is being set";
			# Utilities::update_agent_log($processServerId, $sourceDeviceName, $WEB_ROOT, $agentLogString);

			# return 2;
		# }
		# else
		# {
			# return 1;
		# }

	# }
# }


# sub all_host_appliance_target_lun_mapping_lun_ready_for_protection
# {

    # my ($conn, $sharedDeviceId, $processServerId) = @_;

    # $logging_obj->log("DEBUG","all_host_appliance_target_lun_mapping_lun_ready_for_protection($sharedDeviceId, $processServerId)");

    # my @par_array = ($sharedDeviceId, $processServerId, Fabric::Constants::PROTECTED);
	# my $par_array_ref = \@par_array;
	# my $sqlStmnt = $conn->sql_query(
        # "select sharedDeviceId ".
        # "from hostApplianceTargetLunMapping ".
        # "where sharedDeviceId = ? and processServerId = ? and ".
        # " state != ?",$par_array_ref);

     # return ($conn->sql_num_rows($sqlStmnt) == 0);
# }

# sub update_host_appliance_target_lun_mapping_state_for_lun
# {

    # my ($conn, $sharedDeviceId, $processServerId, $nodeHostId, $currentState, $nextState) = @_;

    # $logging_obj->log("DEBUG"," update_host_appliance_target_lun_mapping_state_for_lun($sharedDeviceId, $nodeHostId, $currentState, $nextState)");

    # my @par_array = ($nextState, $sharedDeviceId, $processServerId, $nodeHostId, $currentState);
	# my $par_array_ref = \@par_array;

	 # my $sqlStmnt = $conn->sql_query(
        # "update hostApplianceTargetLunMapping ".
        # " set state = ? ".
        # "where sharedDeviceId = ? and processServerId = ? ".
        # " and hostId = ? and state = ?",$par_array_ref);
# }



# sub device_exists_in_host_appliance_target_lun_mapping
# {
    # my ($conn, $processServerId, $sharedDeviceId, $initiator, $srcDeviceName) = @_;


    # $logging_obj->log("DEBUG","device_exists_in_host_appliance_target_lun_mapping($processServerId, $sharedDeviceId, $initiator, $srcDeviceName)");

    # my @par_array = ($processServerId, $sharedDeviceId, $initiator, $srcDeviceName);
	# my $par_array_ref = \@par_array;

    # my $sqlStmnt = $conn->sql_query(
        # "select distinct ".
        # " processServerId, ".
        # " sharedDeviceId, ".
        # " exportedInitiatorPortWwn, ".
        # " applianceTargetLunMappingId ".
        # "from   ".
        # " hostApplianceTargetLunMapping ".
        # "where  ".
        # " processServerId = ? and ".
        # " sharedDeviceId = ? and  ".
        # " exportedInitiatorPortWwn = ? and ".
		# " srcDeviceName = ? ",$par_array_ref);

    # return $conn->sql_num_rows($sqlStmnt);
# }

# sub get_one_to_n_protection_action
# {
	# my($conn, $sharedDeviceId) = @_;
	# my $destHostId;
	# my $destDeviceName;
	# my $state;
	# $logging_obj->log("DEBUG","get_one_to_n_protection_action :: sharedDeviceId ::  $sharedDeviceId ");
	# my @par_array = ($sharedDeviceId, Fabric::Constants::START_PROTECTION_PENDING);
	# my $par_array_ref = \@par_array;
	# my $sqlStmnt = $conn->sql_query(
						# "SELECT DISTINCT ".
						# " destinationHostId, ".
						# " destinationDeviceName, ".
						# " lun_state ".
						# " FROM ".
						# " srcLogicalVolumeDestLogicalVolume ".
						# " WHERE ".
						# " Phy_Lunid = ? ".
						# " AND ".
						# " lun_state != ?", $par_array_ref);
	
	# my @array=(\$destHostId, \$destDeviceName, \$state);
	# $conn->sql_bind_columns($sqlStmnt, @array);
	# while($conn->sql_fetch($sqlStmnt))
	# {
		# if($state == Fabric::Constants::STOP_PROTECTION_PENDING)
		# {
			# skip this until there are no stop protection pendings
			# return ITL_PROTECTOR_SKIP;
		# }
	
	# }
	
	# OK no stop protections, want to update if 1-to-n otherwise add
    # return (&san_lun_one_to_n($conn, $sharedDeviceId) ? ITL_PROTECTOR_UPDATE_ONLY : ITL_PROTECTOR_ADD);
	
# }

# sub san_lun_one_to_n
# {
	# my($conn, $sharedDeviceId) = @_;
	# $logging_obj->log("DEBUG", "san_lun_one_to_n");
	# my @par_array = ($sharedDeviceId);
	# my $par_array_ref = \@par_array;
	# my $StatesNotOkToProcessDeletion = Fabric::Constants::SOURCE_DELETE_DONE .
        # "," . Fabric::Constants::PS_DELETE_DONE;
	# my $sqlStmnt = $conn->sql_query(
						# "SELECT DISTINCT ".
						# " Phy_Lunid, destinationHostId, destinationDeviceName ".
						# " FROM ".
						# " srcLogicalVolumeDestLogicalVolume  ".
						# " WHERE ".
						# " Phy_Lunid = ? AND ".
						# " replication_status ".
						# " NOT IN($StatesNotOkToProcessDeletion)",$par_array_ref);
	
    #1-to-n if more then one row returned
    # return $conn->sql_num_rows($sqlStmnt) > 1;					
# }

sub process_delete_at_lun_done
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_delete_at_lun_done");

    my $srcDeviceName;
    my $sharedDeviceId;
    my $initiator;
    my $applianceTargetLunMappingId;
    my $applianceTargetLunMappingNumber;
    my $nodeHostId;
    my $accessControlGroupId;
    my $processServerId;
    my ($lunState,$lunCapacity);
	my $successValue;
    

    my @par_array = (Fabric::Constants::DELETE_AT_LUN_DONE,Fabric::Constants::PRISM_SOLUTION);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query(
        "select distinct ".
        " hatlm.exportedInitiatorPortWwn,".
        " hatlm.srcDeviceName,".
        " hatlm.sharedDeviceId,".
        " hatlm.applianceTargetLunMappingId,".
        " hatlm.applianceTargetLunMappingNumber,".
        " hatlm.hostId,".
        " hatlm.accessControlGroupId,".
        " hatlm.processServerId ".
        "from hostApplianceTargetLunMapping hatlm, ".
		" applicationPlan ap ,".
		" srcLogicalVolumeDestLogicalVolume sldl ".
		"where hatlm.state = ? ".
		" and sldl.Phy_LunId = hatlm.sharedDeviceId".
		" and sldl.planId = ap.planId ".
		" and ap.solutionType = ?",$par_array_ref);


    if ($conn->sql_num_rows($sqlStmnt) == 0)
	{
		return;
    }
	my $hostIds = &node_being_uninstalled($conn);
    my @array = (
        \$initiator, \$srcDeviceName, \$sharedDeviceId, \$applianceTargetLunMappingId,
        \$applianceTargetLunMappingNumber, \$nodeHostId,
        \$accessControlGroupId, \$processServerId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) 
	{
        
		#
		# This is the general stop protection flow
		#
		
		my $count = scalar(@{$hostIds});
		$logging_obj->log("DEBUG","process_delete_at_lun_done - $count");
					
		if ($count > 0)
		{
			$logging_obj->log("DEBUG","process_delete_at_lun_done - $nodeHostId");
					
			if (grep $_ == $nodeHostId, @{$hostIds})
			{
				#$logging_obj->log("DEBUG","process_delete_at_lun_done - $nodeHostId");
				my $hostStatus = &get_unregistered_host_status($nodeHostId);
				#
				# hostStatus 1=PS, 2=Target, 3=Source
				#
				
				if($hostStatus == 3) # if nodeType is Source then only we need to skip Mirror deletion.
				{		
					$successValue = &update_node_unregister_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_DONE,
				$initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_AT_LUN_DONE);	
				
				#$successValue = &update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_DONE,
				#$initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_AT_LUN_DONE);
				}
				else
				{
					$successValue = &update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_AT_LUN_DONE);
				}
			}
			else
			{
				$successValue = &update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_AT_LUN_DONE);
			}	
		}
		else
		{
			$successValue = &update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING,
				$initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_AT_LUN_DONE);
		
		# if (0 == &update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING,
				# $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_AT_LUN_DONE)) 
		# {
			 # &delete_protection_job($conn, $sharedDeviceId, $processServerId);
			 # &process_host_shared_device_mapping_delete_pending($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId);
		# }
		}
    	
	}

}

sub process_host_shared_device_mapping_requests
{
    # TODO: should only proces NEW_ETNRY_PENDING
    # for lunIds that are not in frBindingNexus
    # or if in frBindIngNexus all set to PROTECTED
    my ($conn) = @_;
	my ($tempSharedDeviceId, @sharedDeviceList,$scsiId);
	my $fabricId = Fabric::Constants::PRISM_FABRIC;
    $logging_obj->log("DEBUG","process_host_shared_device_mapping_requests");
    my @par_array = (Fabric::Constants::NEW_ENTRY_PENDING,Fabric::Constants::DELETE_ENTRY_PENDING);
    my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "select ".
        " hsdm.state,".
        " hsdm.sanInitiatorPortWwn,".
        " hsdm.hostId,".
        " hsdm.sharedDeviceId,".
        " hsdm.srcDeviceName, ".
		" sd.sourceHostId ".
		" FROM ".
        " srcLogicalVolumeDestLogicalVolume sd, ".
		" hostSharedDeviceMapping hsdm ".
        "where ".
		"sd.phy_LunId = hsdm.sharedDeviceId AND".
        " hsdm.state in(?, ?) order by hsdm.state,hsdm.sharedDeviceId",$par_array_ref);
	
    my ($state, $initiator, $nodeHostId, $sharedDeviceId, $srcDeviceName,$processServerId);
    my @array = (\$state, \$initiator, \$nodeHostId, \$sharedDeviceId,\$srcDeviceName,\$processServerId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) 
	{
        my $src_state = &is_lun_protected($conn,$sharedDeviceId);
		# if($tempSharedDeviceId != $sharedDeviceId)
		# {
			
			if ($state == Fabric::Constants::NEW_ENTRY_PENDING)
			{
				
				
				my @applianceInitiatorInfos;
				Array::CommonProcessor::get_appliance_initiator_infos($conn, $sharedDeviceId, $fabricId, $processServerId, \@applianceInitiatorInfos);

				my @applianceTargetInfos;
				Array::CommonProcessor::get_appliance_target_infos($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId, \@applianceTargetInfos);

				
				$logging_obj->log("DEBUG","process_host_shared_device_mapping_requests state:::;$src_state");
				&process_start_protection_on_node_reboot($conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
				$processServerId, \@applianceInitiatorInfos, \@applianceTargetInfos,$src_state);
				
				if($src_state != Fabric::Constants::STOP_PROTECTION_PENDING)
				{
					&update_state_in_src($conn,$sharedDeviceId,Fabric::Constants::NODE_REBOOT_OR_PATH_ADDITION_PENDING);
				}
				
				&clear_new_entry_pending($conn, $initiator, $srcDeviceName,$sharedDeviceId, $nodeHostId);
			}
			elsif ($state == Fabric::Constants::DELETE_ENTRY_PENDING) 
			{
				$logging_obj->log("DEBUG","delete entry state:::;$state");
				&process_delete_pending($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId,$processServerId);
			} 
		#}	
		
		
		@sharedDeviceList=$sharedDeviceId;
		
    }
}


sub process_start_protection_on_node_reboot
{
    $logging_obj->log("DEBUG","process_start_protection_on_node_reboot");
	my ($conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
        $processServerId, $applianceInitiators, $applianceTargets,$src_state) = @_;

	my $syncDeviceName;
		
    
	$syncDeviceName = Array::CommonProcessor::get_sync_device_name($conn, $sharedDeviceId, $processServerId);
	if(!$syncDeviceName)
	{
		die "Sync device not avaiable for sharedDevice=$sharedDeviceId and processServerId=$processServerId";
	}
	
	my @par_array = ($sharedDeviceId);
    my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "select ".
        " state ".
        " FROM ".
        " hostApplianceTargetLunMapping ".
		" where ".
		"sharedDeviceId = ?",$par_array_ref);
	
    my ($hstate);
    my @array = (\$hstate);
    $conn->sql_bind_columns($sqlStmnt,@array);
	my $numRows = $conn->sql_num_rows($sqlStmnt);
	if ($numRows)
	{
		my $hostState;
		$conn->sql_fetch($sqlStmnt);
		$logging_obj->log("DEBUG","process_start_protection_on_node_reboot state:::;$src_state");
		foreach my $aiInfo (@{$applianceInitiators}) 
		{
		   foreach my $atInfo (@{$applianceTargets}) 
			{
				if ($src_state == Fabric::Constants::START_PROTECTION_PENDING)
				{
					$hostState = Fabric::Constants::CREATE_AT_LUN_PENDING;
				
				}
				elsif ($src_state == Fabric::Constants::PROTECTED || $src_state == Fabric::Constants::NODE_REBOOT_OR_PATH_ADDITION_PENDING)
				{
					$hostState = Fabric::Constants::MIRROR_SETUP_PENDING_RESYNC_CLEARED;
				
				}
				elsif ($src_state == Fabric::Constants::STOP_PROTECTION_PENDING)
				{
					if ($hstate == Fabric::Constants::DELETE_AT_LUN_PENDING || 
					$hstate == Fabric::Constants::DELETE_AT_LUN_DONE || 
					$hstate == Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING ||
					$hstate == Fabric::Constants::PROTECTED)
					{
						$hostState = $hstate;
					}	
				
				}
				$logging_obj->log("DEBUG","hoststatestate:::;$hostState");
				if ($hostState)
				{
					my @sel_par_array = ($nodeHostId, $sharedDeviceId, $atInfo->{lunId}, $atInfo->{lunNumber}, 
							  $initiator, $atInfo->{acgId}, $processServerId, $aiInfo, $atInfo->{portWwn},  
							 $fabricId, $srcDeviceName, $syncDeviceName);
					my $sel_par_array_ref = \@sel_par_array;

					my $sqlSelectStmnt = $conn->sql_query(
					"select ".
					" sharedDeviceId ".
					"from ".
					" hostApplianceTargetLunMapping ".
					"where ".
					" hostId = ? and ".
					" sharedDeviceId = ? and ".
					" applianceTargetLunMappingId = ? and ".
					" applianceTargetLunMappingNumber = ? and ".
					" exportedInitiatorPortWwn = ? and ".
					" accessControlGroupId = ? and ".
					" processServerId = ? and ".
					" applianceInitiatorPortWwn = ? and ".
					" applianceTargetPortWwn = ? and ".
					" fabricId = ? and ".
					" srcDeviceName = ? and ".
					" syncDeviceName = ? ",$sel_par_array_ref);
					$logging_obj->log("DEBUG","process_start_protection_on_node_reboot - (checking entry existance)");
					if ($conn->sql_num_rows($sqlSelectStmnt) == 0)
					{
						my @par_array = ($nodeHostId, $sharedDeviceId, $atInfo->{lunId}, $atInfo->{lunNumber}, 
										  $initiator, $atInfo->{acgId}, $processServerId, $aiInfo, $atInfo->{portWwn},  
										 $fabricId, $srcDeviceName, $syncDeviceName, $hostState);
						
						my $par_array_ref = \@par_array;
						my $sqlStmnt = $conn->sql_query(
								 "insert into hostApplianceTargetLunMapping ".
								 "(hostId,".
								 "sharedDeviceId,".
								 "applianceTargetLunMappingId,".
								 "applianceTargetLunMappingNumber,".
								 "exportedInitiatorPortWwn,".
								 "accessControlGroupId,".
								 "processServerId,".
								 "applianceInitiatorPortWwn,".
								 "applianceTargetPortWwn,".
								 "fabricId,".
								 "srcDeviceName,".
								 "syncDeviceName,".
								 "state) ".
								 " values(?,?,?,?,?,?,?,?,?,?,?,?,?)",$par_array_ref);
						
							 $logging_obj->log("DEBUG","process_start_protection 4");

						if (!$sqlStmnt
							&& !($conn->sql_err() == $MYSQL_ER_DUP_KEY
								 || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
								 || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) 
						{
									$logging_obj->log("EXCEPTION","ERROR - process_start_protection ".$conn->sql_error());
									die $conn->sql_error();
						}
					}
						
					my @par_array = ($hostState, $sharedDeviceId, $nodeHostId);
					my $array_ref = \@par_array;
					my $sqlStmnt = $conn->sql_query("update hostApplianceTargetLunMapping ".
					" set state = ? ".
					"where sharedDeviceId = ? AND ".
					"hostId = ? ",$array_ref);
				}		
			}
		}
	}	
}



sub is_lun_protected
{
	my ($conn,$sharedDeviceId) = @_;
	my @par_array = ($sharedDeviceId);
    my $par_array_ref = \@par_array;
	my $state;


	my $sqlStmnt = $conn->sql_query(
        "SELECT lun_state FROM srcLogicalVolumeDestLogicalVolume ".
        "where Phy_Lunid = ? ",$par_array_ref);
	

	my @array = (\$state);
    $conn->sql_bind_columns($sqlStmnt,@array);
	$conn->sql_fetch($sqlStmnt);
	$logging_obj->log("DEBUG","is_lun_protected ::$state");
	return $state;
}	
sub update_state_in_src
{
	my ($conn,$sharedDeviceId,$state) = @_;
	$logging_obj->log("DEBUG","update_state_in_src($sharedDeviceId)");
    my @par_array = ($state,$sharedDeviceId);
    my $par_array_ref = \@par_array;


	my $sqlStmnt = $conn->sql_query(
        "update srcLogicalVolumeDestLogicalVolume ".
        " set lun_state = ? ".
        "where Phy_LunId = ? "
        ,$par_array_ref);

}
sub clear_new_entry_pending
{
    my ($conn, $initiator, $srcDeviceName, $sharedDeviceId,$nodeHostId) = @_;

    $logging_obj->log("DEBUG","clear_new_entry_pending($initiator, $srcDeviceName, $sharedDeviceId)");
    my @par_array = (Fabric::Constants::NO_WORK, $initiator, $srcDeviceName, $sharedDeviceId,$nodeHostId);
    my $par_array_ref = \@par_array;


	my $sqlStmnt = $conn->sql_query(
        "update hostSharedDeviceMapping ".
        " set state = ? ".
        "where sanInitiatorPortWwn = ? and ".
        " srcDeviceName = ? and ".
        " sharedDeviceId = ? and".
		" hostId = ? " ,$par_array_ref);

}

sub set_all_luns_for_itl_to_restart
{
    my ($conn, $sharedDeviceId) = @_;

    $logging_obj->log("DEBUG","set_all_luns_for_itl_to_restart($sharedDeviceId)");
    my @par_array = (Fabric::Constants::CREATE_AT_LUN_PENDING, $sharedDeviceId);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ? ".
        "where sharedDeviceId = ? ",$par_array_ref);

}

sub process_delete_pending
{
    my ($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId,$processServerId) = @_;

    $logging_obj->log("DEBUG","process_delete_pending($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId)");

    &process_host_appliance_target_lun_mapping_delete_pending(
            $conn, $sharedDeviceId, $nodeHostId, $srcDeviceName,$initiator,$processServerId);
}

sub process_host_appliance_target_lun_mapping_delete_pending
{
    my ($conn, $sharedDeviceId, $hostId, $srcDeviceName,$sanInitiatorPortWwn,$processServerId) = @_;

    #&itl_trace("process_san_itl_nexus_delete_pending($initiator, $target, $sanLunId, $sanLunNumber, $fabricId)");
    my @par_array = ($sharedDeviceId,$hostId, $srcDeviceName,  $sanInitiatorPortWwn,$processServerId);
	my $par_array_ref = \@par_array; 
    my $sqlStmnt = $conn->sql_query(
        "delete from  hostApplianceTargetLunMapping ".
        "where sharedDeviceId = ? and ".
        " hostId = ? and ".
        " srcDeviceName = ? and ".
        " exportedInitiatorPortWwn = ? and ".
		" processServerId = ? "
       ,$par_array_ref);

    if (!$sqlStmnt
		&& !($conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED
             || $conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED_2)) {
        die $conn->sql_error();
    }
	&process_host_shared_device_mapping_delete_pending_on_node_reboot(
            $conn, $sanInitiatorPortWwn, $srcDeviceName, $sharedDeviceId, $hostId);
}

sub itl_lun_protected
{
    my ($conn, $sharedDeviceId) = @_;

    $logging_obj->log("DEBUG","itl_lun_protected($sharedDeviceId)");
    my @par_array = ($sharedDeviceId);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "select sharedDeviceId ".
        "from hostApplianceTargetLunMapping ".
        "where sharedDeviceId = ?",$par_array_ref);

    return $conn->sql_num_rows($sqlStmnt) > 0;
}


 sub process_create_at_lun_done
 {
     my ($conn) = @_;

    $logging_obj->log("DEBUG","process_create_at_lun_done");
	Array::CommonProcessor::process_create_appliance_lun_done($conn);
}

sub process_delete_split_mirror_done
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_delete_split_mirror_done");
    
	my $initiator;
	my $srcDeviceName;
	my $sharedDeviceId;
	my $applianceTargetLunMappingId;
	my $applianceTargetLunMappingNumber;
	my $accessControlGroupId;
	my $nodeHostId;
	my $processServerId;
	my ($applianceInitiatorPortWwn, $applianceTargetPortWwn);
	
	my $StatesNotOkToProcessLunResize = Fabric::Constants::DELETE_PENDING .
        "," . Fabric::Constants::DELETE_AT_LUN_PENDING .
        "," . Fabric::Constants::DELETE_AT_LUN_DONE .
        "," . Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING .
        "," . Fabric::Constants::DELETE_SPLIT_MIRROR_DONE .
        "," . Fabric::Constants::STOP_PROTECTION_PENDING .
        "," . Fabric::Constants::FAILOVER_PRE_PROCESSING_PENDING .
        "," . Fabric::Constants::FAILOVER_PRE_PROCESSING_DONE .
        "," . Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_PENDING .
        "," . Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_DONE .
        "," . Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_PENDING .
        "," . Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_DONE .
        "," . Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING .
        "," . Fabric::Constants::FAILOVER_RECONFIGURATION_DONE;
	
	my ($lunState,$lunCapacity,$lunState);
    my @par_array = (Fabric::Constants::DELETE_SPLIT_MIRROR_DONE,Fabric::Constants::PRISM_SOLUTION);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "select distinct ".
        " hatlm.exportedInitiatorPortWwn,".
        " hatlm.srcDeviceName,".
        " hatlm.sharedDeviceId,".
        " hatlm.applianceTargetLunMappingId,".
        " hatlm.applianceTargetLunMappingNumber,".
        " hatlm.accessControlGroupId,".
        " hatlm.hostId,".
        " hatlm.processServerId, ".
        " hatlm.applianceInitiatorPortWwn, ".
        " hatlm.applianceTargetPortWwn ".
		"from hostApplianceTargetLunMapping hatlm,".
		" applicationPlan ap, ".
		" srcLogicalVolumeDestLogicalVolume sldl ".
		"where hatlm.state = ? ".
		" and sldl.Phy_LunId = hatlm.sharedDeviceId".
		" and sldl.planId = ap.planId ".
		" and ap.solutionType = ?",$par_array_ref);


    if ($conn->sql_num_rows($sqlStmnt) == 0)
	{
        return;
    }


    my @array = (\$initiator, \$srcDeviceName, \$sharedDeviceId,
        \$applianceTargetLunMappingId, \$applianceTargetLunMappingNumber,
        \$accessControlGroupId, \$nodeHostId, \$processServerId, \$applianceInitiatorPortWwn, \$applianceTargetPortWwn);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) 
	{
	
		#
        # In case of lun resize, once the mirror is deleted.
        # the lun_state flag should be changed to LUN_RESIZE_PRE_PROCESSING_DONE,
        # which eventually changed to LUN_RESIZE_RECONFIGURATION_PENDING
        # in order to re-create the AT lun with the new size.
        #

       my @par_array = ($sharedDeviceId,$sharedDeviceId,Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_PENDING,Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_DONE,Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING,Fabric::Constants::LUN_RESIZE_RECONFIGURATION_DONE);
	   my $par_array_ref = \@par_array;

       my $sqlStmntSldl = $conn->sql_query(
                        "SELECT DISTINCT ".
                        "   lun_state, ".
                        "   capacity ".
                        "FROM  ".
                        "   srcLogicalVolumeDestLogicalVolume, ".
                        "   sharedDevices ".
                        "WHERE ".
                        "   sharedDeviceId = ? AND ".
                        "   Phy_Lunid = ? AND ".
                        "   lun_state IN (?,?,?,?)",$par_array_ref);
	
		
		if ($conn->sql_num_rows($sqlStmntSldl) > 0)
		{
		    $logging_obj->log("DEBUG","process_delete_split_mirror_done lun resize - Entry found in srcLogicalVolumeDestLogicalVolume");

            $sqlStmntSldl->bind_columns(\$lunState,\$lunCapacity);
            while($sqlStmntSldl->fetch)
            {
                if(Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_PENDING == $lunState)
                {
                    #
                    # update lun_state as LUN_RESIZE_PRE_PROCESSING_DONE
                    # in srcLogicalVolumeDestLogicalVolume
                    #
                    my @par_arary = (Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_DONE,$sharedDeviceId,Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_PENDING);
					my $par_array_ref = \@par_arary;

					my $sqlUpdate = $conn->sql_query(
                            "UPDATE ".
                            "   srcLogicalVolumeDestLogicalVolume ".
                            "SET ".
                            "   lun_state = ? ".
                            "WHERE ".
                            "   Phy_Lunid = ? AND" .
                            "   lun_state = ? AND" .
                            "   lun_state NOT IN ($StatesNotOkToProcessLunResize) ",$par_array_ref);


					$logging_obj->log("DEBUG","process_delete_split_mirror_done lun resize - updated to LUN_RESIZE_PRE_PROCESSING_DONE");
                }
            }
        }
		else
		{
			&delete_host_appliance_target_lun_mapping(
            $conn,  Fabric::Constants::DELETE_SPLIT_MIRROR_DONE,
            $initiator, $srcDeviceName,  $sharedDeviceId, $applianceTargetLunMappingId, 
            $applianceTargetLunMappingNumber, $accessControlGroupId, 
			$nodeHostId, $processServerId, $applianceInitiatorPortWwn, $applianceTargetPortWwn);
		
		}
		
    }
}

sub delete_appliance_target_lun_mapping
{
    my ($conn, $applianceTargetLunMappingId, $applianceTargetLunMappingNumber, $sharedDeviceId) = @_;

    $logging_obj->log("DEBUG","delete_appliance_target_lun_mapping($applianceTargetLunMappingId, $applianceTargetLunMappingNumber, $sharedDeviceId)");
    my @par_array = ($applianceTargetLunMappingId, $applianceTargetLunMappingNumber, $sharedDeviceId);
	my $par_arary_ref = \@par_array;
    my $deleteSqlStmnt = $conn->sql_query(
        "delete from applianceTargetLunMapping  ".
        "where applianceTargetLunMappingId = ? and ".
        "applianceTargetLunMappingNumber = ? AND ".
	"sharedDeviceId = ?",$par_arary_ref);

    if (!$deleteSqlStmnt
        && !($conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED
             || $conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED_2)) {
        $logging_obj->log("EXCEPTION","ERROR - delete_appliance_target_lun_mapping ".$conn->sql_error());
		die $conn->sql_error();
    }
}



sub delete_host_appliance_target_lun_mapping
{
    my ($conn, $state, $initiator, $srcDeviceName,  
	$sharedDeviceId, $applianceTargetLunMappingId, 
            $applianceTargetLunMappingNumber, $accessControlGroupId, 
			$nodeHostId, $processServerId, $applianceInitiatorPortWwn, 
			$applianceTargetPortWwn) = @_;

    $logging_obj->log("DEBUG","delete_host_appliance_target_lun_mapping($state, $initiator, $srcDeviceName,  
	$sharedDeviceId, $applianceTargetLunMappingId, 
            $applianceTargetLunMappingNumber, $accessControlGroupId, 
			$nodeHostId, $processServerId, $applianceInitiatorPortWwn, $applianceTargetPortWwn)");

    my @par_array = ( $initiator, $srcDeviceName, $sharedDeviceId, 
	$applianceTargetLunMappingId, $accessControlGroupId, $applianceInitiatorPortWwn,
$applianceTargetPortWwn );
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "delete from  hostApplianceTargetLunMapping ".
        "where ".
        " exportedInitiatorPortWwn = ? and ".
        " srcDeviceName = ? and ".
        " sharedDeviceId = ? and ".
        " applianceTargetLunMappingId = ? and ".
        " accessControlGroupId = ? and ".
        " applianceInitiatorPortWwn = ? and ".
        " applianceTargetPortWwn = ? ",$par_array_ref);
	
	if (!$sqlStmnt
        && !($conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED
             || $conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED_2)) {
        $logging_obj->log("EXCEPTION","ERROR - delete_host_appliance_target_lun_mapping ".$conn->sql_error());
		die $conn->sql_error();
    }	
		
    if ($conn->sql_num_rows($sqlStmnt) > 0) 
	{
		&finish_delete_protection(
            $conn, $initiator, $srcDeviceName,  
	$sharedDeviceId, $applianceTargetLunMappingId, 
            $applianceTargetLunMappingNumber, $accessControlGroupId, 
			$nodeHostId, $processServerId);
    }
}

sub finish_delete_protection
{
    my ($conn, $initiator, $srcDeviceName,  
	$sharedDeviceId, $applianceTargetLunMappingId, 
            $applianceTargetLunMappingNumber, $accessControlGroupId, 
			$nodeHostId, $processServerId) = @_;

    $logging_obj->log("DEBUG","finish_delete_protection($initiator, $srcDeviceName,
        $sharedDeviceId, $applianceTargetLunMappingId,
            $applianceTargetLunMappingNumber, $accessControlGroupId,
                        $nodeHostId, $processServerId)");
   
	my @update_par_array = (1, $sharedDeviceId, $nodeHostId, $initiator, $srcDeviceName);
	my $update_par_array_ref = \@update_par_array;
	my $updateSqlStmnt = $conn->sql_query(
	"update hostSharedDeviceMapping ".
	" set deleteState = ? ".
	" where ".
	"  sharedDeviceId = ? and ".
	"  hostId = ? and ".
	"  sanInitiatorPortWwn = ? and ".
	"  srcDeviceName = ? ",$update_par_array_ref);					
					
    &delete_access_control_group($conn, $initiator, $accessControlGroupId,$nodeHostId);
	
	my @par_array = ($sharedDeviceId);
	my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "select sharedDeviceId ".
        "from hostApplianceTargetLunMapping ".
        "where sharedDeviceId = ?",$par_array_ref);
	my $count = $conn->sql_num_rows($sqlStmnt);
    $logging_obj->log("DEBUG","finish_delete_protection:: hostApplianceTargetLunMapping count for sharedDeviceId $sharedDeviceId :: $count");
	if ($count == 0) 
	{
		&delete_shared_devices($conn, $sharedDeviceId);
		&delete_appliance_target_lun_mapping($conn, $applianceTargetLunMappingId, $applianceTargetLunMappingNumber, $sharedDeviceId);
		&process_host_shared_device_mapping_delete_pending($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId);
		&delete_protection_job($conn, $sharedDeviceId, $processServerId);
	}
}


sub delete_access_control_group
{
    my ($conn, $initiator, $accessControlGroupId,$nodeHostId) = @_;

    $logging_obj->log("DEBUG","delete_access_control_group($initiator, $accessControlGroupId,$nodeHostId)");
    
	my @par_array = ($nodeHostId);
	my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "select distinct sharedDeviceId ".
        "from hostApplianceTargetLunMapping ".
        "where hostId = ?",$par_array_ref);
	my $count = $conn->sql_num_rows($sqlStmnt);
    $logging_obj->log("DEBUG","delete_access_control_group:: hostApplianceTargetLunMapping count for node $nodeHostId :: $count");
	# this condition will check if we have only one pair with protected node then 
	# only we can remove the accessControl port otherwise leave it.
	if ($count == 0) 
	{
	
		my @par_array = ($initiator, $accessControlGroupId);
		my $par_array_ref = \@par_array;

		my $sqlStmnt = $conn->sql_query(
			"delete from  accessControlGroupPorts ".
			"where exportedInitiatorportWwn = ? AND ".
			"accessControlGroupId = ?",$par_array_ref);

		# OK if delete failed because of foreign key constraints
		# as we try to avoid selects to make sure
		# things are OK to delete. we let the constraint
		# prevent us from doing the wrong thing

		if (!$sqlStmnt
			&& !($conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED
				 || $conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED_2)) 
		{
			$logging_obj->log("EXCEPTION","ERROR - delete_access_control_group ".$conn->sql_error());
			die $conn->sql_error();
		}

		if(!($conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED ||
			 $conn->sql_err() == $MYSQL_ER_ROW_IS_REFERENCED_2))

		{

			my @par_array = ($accessControlGroupId, $initiator);
			my $par_array_ref = \@par_array;
			my $sqlStmnt = $conn->sql_query(
			"delete from  accessControlGroup ".
			"where accessControlGroupId = ? AND ".
			" accessControlGroupName = ? ",$par_array_ref);

			# OK if delete failed because of foreign key constraints
			# as we try to avoid selects to make sure
			# things are OK to delete. we let the constraint
			# prevent us from doing the wrong thing
			if (!$sqlStmnt) 
			{
				$logging_obj->log("EXCEPTION","ERROR - delete_access_control_group ".$conn->sql_error());
				die $conn->sql_error();
			}
		}
	}
}

sub process_reboot_pending
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_reboot_pending");

    my ($processServerId,$sharedDeviceId);
    my $fabricId = Fabric::Constants::PRISM_FABRIC;
    my @par_array = (Fabric::Constants::PRISM_SOLUTION,Fabric::Constants::REBOOT_PENDING,Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING,Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING);
    my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "SELECT DISTINCT ".
        "   hatlm.processServerId, ".
        "   hatlm.sharedDeviceId ".
        "FROM ".
        "   hostApplianceTargetLunMapping hatlm, ".
		" srcLogicalVolumeDestLogicalVolume sldl, ".
		" applicationPlan ap ".
        "WHERE ".
		"ap.solutionType = ? AND ".
        "ap.planId=sldl.planId AND ".
	    "   hatlm.processServerId = sldl.sourceHostId AND ".
		"   sldl.Phy_Lunid = hatlm.sharedDeviceId AND ".
        "   hatlm.state in (?,?,?)",$par_array_ref);
    my @array = (\$processServerId,\$sharedDeviceId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) 
	{
        if (&appliance_fr_binding_targets_ok($conn, $processServerId, $fabricId)
            && &appliance_fr_binding_initiators_ok($conn, $processServerId, $fabricId)) 
		{
            &update_reboot_host_appliance_target_lun_mappings_state($conn, $processServerId, $fabricId,$sharedDeviceId);
        }
    }
}

sub update_reboot_host_appliance_target_lun_mappings_state
{
    my ($conn, $processServerId, $fabricId,$sharedDeviceId) = @_;

    $logging_obj->log("DEBUG","update_reboot_host_appliance_target_lun_mappings_state($processServerId, $fabricId,$sharedDeviceId)");
    my @par_array = (Fabric::Constants::CREATE_AT_LUN_PENDING,0,Fabric::Constants::REBOOT_PENDING,Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING,Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING,$processServerId, $fabricId,$sharedDeviceId);
    my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ?, ".
        " status = ? ".
        " where state in (?,?,?) and ".
        " processServerId = ? and ".
        " fabricId = ? and ".
		" sharedDeviceId = ? ",$par_array_ref);

}


sub appliance_fr_binding_targets_ok
{
    my ($conn, $processServerId, $fabricId) = @_;

    $logging_obj->log("DEBUG","appliance_fr_binding_targets_ok($processServerId, $fabricId)");
    my @par_array = ($processServerId, $fabricId);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "select applianceFrBindingTargetPortWwn ".
        "from applianceFrBindingTargets ".
        "where (state != 'stable' or ".
        " error != 0) and ".
        " processServerId = ? and ".
        " fabricId = ?",$par_array_ref);

    return (0 == $conn->sql_num_rows($sqlStmnt));
}

sub appliance_fr_binding_initiators_ok
{
    my ($conn, $processServerId, $fabricId) = @_;

    $logging_obj->log("DEBUG"," appliance_fr_binding_initiators_ok($processServerId, $fabricId)");
    my @par_array = ($processServerId, $fabricId);
	my $par_array_ref = \@par_array;

    my $sqlStmnt = $conn->sql_query(
        "select applianceFrBindingInitiatorPortWwn ".
        "from applianceFrBindingInitiators ".
        "where (state != 'stable' or ".
        " error != 0) and ".
        " processServerId = ? and ".
        " fabricId = ?",$par_array_ref);


    return (0 == $conn->sql_num_rows($sqlStmnt));
}

sub process_reboot_failures
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_reboot_failures");

    my $sqlStmnt = $conn->sql_query(
        "select applianceFrBindingTargetPortWwn, ".
        " processServerId, ".
        " fabricId ".
        "from applianceFrBindingTargets ".
        " where state = 'RebootReconfigureFailed'");

    my $applianceFrBindingTargetPortWwn;
    my $processServerId;
    my $fabricId;

    $sqlStmnt->bind_columns(
        \$applianceFrBindingTargetPortWwn, \$processServerId , \$fabricId);
    my @par_array = (Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING, $applianceFrBindingTargetPortWwn,$processServerId , $fabricId);
	my $par_array_ref = \@par_array;


    while ($sqlStmnt->fetch) 
	{
		my $updateSqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ? ".
        "where applianceFrBindingTargetPortWwn = ? ".
        " and processServerId = ? ".
        " and fabricId = ? ".
        " and (state in ($STATES_OK_TO_SET_DELETE) or state < 0)",$par_array_ref);

    }
}

sub process_lun_resize
{
     my ($conn) = @_;

     $logging_obj->log("DEBUG","process_lun_resize");

     &process_lun_resize_updation($conn);
     &process_delete_path_on_lun_resize($conn);
     &process_reconfiguration_on_lun_resize($conn);
}

#
# Process the luns which are acknowledged by
# SA with new size after rescan is done
#
sub process_lun_resize_updation
{
    my ($conn) = @_;
    my ($sharedDeviceId, $capacityChangeFlag);

    my $StatesNotOkToProcessLunResize = Fabric::Constants::DELETE_PENDING .
        "," . Fabric::Constants::DELETE_AT_LUN_PENDING .
        "," . Fabric::Constants::DELETE_AT_LUN_DONE .
        "," . Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING .
        "," . Fabric::Constants::DELETE_SPLIT_MIRROR_DONE .
        "," . Fabric::Constants::STOP_PROTECTION_PENDING .
        "," . Fabric::Constants::FAILOVER_PRE_PROCESSING_PENDING .
        "," . Fabric::Constants::FAILOVER_PRE_PROCESSING_DONE .
        "," . Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_PENDING .
        "," . Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_DONE .
        "," . Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_PENDING .
        "," . Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_DONE .
        "," . Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING .
        "," . Fabric::Constants::FAILOVER_RECONFIGURATION_DONE;

    $logging_obj->log("DEBUG","process_lun_resize_updation");
    my @par_array = (Fabric::Constants::CAPACITY_CHANGE_UPDATED);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query(
                "SELECT ".
                "   sharedDeviceId ".
                "FROM  ".
                "   sharedDevices ".
                "WHERE ".
                "   volumeResize = ? ",$par_array_ref);


    if ($conn->sql_num_rows($sqlStmnt) > 0)
	{

        my @array = (\$sharedDeviceId);
        $conn->sql_bind_columns($sqlStmnt,@array);

        while ($conn->sql_fetch($sqlStmnt))
        {
            #
            # Check for associated pair
            #

			@par_array = ($sharedDeviceId);
			$par_array_ref = \@par_array;
            my $sqlStmntSldl = $conn->sql_query(
                            "SELECT ".
                            "   Phy_Lunid ".
                            "FROM  ".
                            "   srcLogicalVolumeDestLogicalVolume ".
                            "WHERE ".
                            "   Phy_Lunid = ? ",$par_array_ref);


            if ($conn->sql_num_rows($sqlStmntSldl) > 0)
			{
                #
                # Set lun_state as LUN_RESIZE_PRE_PROCESSING_PENDING
                # for all the associated pairs.
                #
				@par_array = (Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_PENDING,$sharedDeviceId);
				$par_array_ref = \@par_array;

                my $sqlUpdate = $conn->sql_query(
                        "UPDATE ".
                        "   srcLogicalVolumeDestLogicalVolume ".
                        "SET ".
                        "   lun_state = ? ".
                        "WHERE ".
                        "   Phy_Lunid = ? AND" .
                        "   lun_state NOT IN ($StatesNotOkToProcessLunResize) ",$par_array_ref);



                $capacityChangeFlag = Fabric::Constants::CAPACITY_CHANGE_PROCESSING;
            }
			else
			{
                #
                # Pair is deleted in after new LUN size is discovered
                # and before FS can act upon. In this case, instead of setting
                # capacityChanged as CAPACITY_CHANGE_PROCESSING, reset to
                # CAPACITY_CHANGE_RESET
                #

                $capacityChangeFlag = Fabric::Constants::CAPACITY_CHANGE_RESET;
            }

            #
            # Reset the volumeResize flag
            # (Not sure we should do it now, as we may need to
            # process the pairs, if any after HA failover is complete)
            #
            @par_array = (Fabric::Constants::CAPACITY_CHANGE_PROCESSING, $sharedDeviceId, Fabric::Constants::CAPACITY_CHANGE_UPDATED);
			$par_array_ref = \@par_array;

			my $sqlUpdate = $conn->sql_query(
                    "UPDATE ".
                    "   sharedDevices ".
                    "SET ".
                    "   volumeResize = ?".
                    "WHERE ".
                    "   sharedDeviceId = ? AND volumeResize = ?",$par_array_ref);

        }
    }
}


#
# Process pairs which are marked
# by LUN re-size event
#
sub process_delete_path_on_lun_resize
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_delete_vtb_on_lun_resize");

    my $processServerId;
    my $initiator;
    my $srcDeviceName;
    my $sharedDeviceId;
    my $nodeHostId;
    my $fabricId;

    my $solutionType = Fabric::Constants::PRISM_SOLUTION;
    my @par_arary = ($solutionType,Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_PENDING);
	my $par_array_ref = \@par_arary;
     my $sqlStmnt = $conn->sql_query(
        "SELECT DISTINCT ".
        " hatlm.exportedInitiatorPortWwn, ".
        " hatlm.srcDeviceName, ".
        " hatlm.hostId, ".
        " hatlm.sharedDeviceId, ".
        " hatlm.processServerId ".
        "FROM   ".
        " srcLogicalVolumeDestLogicalVolume sd, ".
        " hostApplianceTargetLunMapping hatlm,
		  applicationPlan ap ".
        "WHERE  ".
        "ap.solutionType = ? AND ".
        "ap.planId = sd.planId AND ".
        "sd.Phy_Lunid = hatlm.sharedDeviceId AND ".
        "sd.lun_state = ? ".
        "order by ".
        " hatlm.processServerId, hatlm.sharedDeviceId",$par_array_ref);

    if ($conn->sql_num_rows($sqlStmnt) == 0)
	{
         return;
    }


    my @array = (\$initiator, \$srcDeviceName, \$nodeHostId,\$sharedDeviceId, \$processServerId);
    $conn->sql_bind_columns($sqlStmnt,@array);
     while ($conn->sql_fetch($sqlStmnt))
	{
        $logging_obj->log("DEBUG","process_delete_path_on_lun_resize set DELETE_AT_LUN_PENDING state in hostApplianceTargetLunMapping");
	
	&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_AT_LUN_PENDING, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;	
												   
	}
}

#
# Process the pairs once stale vtb
# is deleted after lun re-size
#
sub process_reconfiguration_on_lun_resize
{
    my ($conn) = @_;
    my ($lunId, $hostId);

    $logging_obj->log("DEBUG","process_reconfiguration_on_lun_resize");
     my @par_array = (Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_DONE);
	 my $par_array_ref = \@par_array;

	 my $sqlUpdateSlDlThree = $conn->sql_query(
            "SELECT DISTINCT ".
            "   sourceHostId, Phy_Lunid  ".
            "FROM ".
            "   srcLogicalVolumeDestLogicalVolume ".
            "WHERE ".
            "   lun_state = ? AND Phy_Lunid != ''",$par_array_ref);

    $sqlUpdateSlDlThree->bind_columns(\$hostId, \$lunId);


	while ($sqlUpdateSlDlThree->fetch) {

        my $pair_type = TimeShotManager::get_pair_type($conn,$lunId); # pair_type 0=host,1=fabric,2=prism

		$logging_obj->log("DEBUG","process_reconfiguration_on_lun_resize::$lunId,$pair_type");
		if($pair_type == 2)
		{
			my @par_arary1 = (Fabric::Constants::REBOOT_PENDING,$lunId, $hostId);
			my $par_array_ref1 = \@par_arary1;

			my @par_array2 = (Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING, $lunId, Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_DONE);
			my $par_array_ref2 = \@par_array2;

			 my $sqlUpdateHatlm = $conn->sql_query(
				"UPDATE ".
				"   hostApplianceTargetLunMapping ".
				"SET ".
				"  state = ?".
				"WHERE ".
				" sharedDeviceId = ? AND processServerId = ?",$par_array_ref1);
	$logging_obj->log("DEBUG","process_reconfiguration_on_lun_resize::$lunId,$hostId");

			my $sqlUpdate = $conn->sql_query(
									"UPDATE ".
									"   srcLogicalVolumeDestLogicalVolume ".
									"SET ".
									"   lun_state = ? ".
									"WHERE ".
									"   Phy_Lunid = ? AND" .
									"   lun_state = ? ",$par_array_ref2);
									
			$logging_obj->log("DEBUG","process_reconfiguration_on_lun_resize");
		}
	}
}


sub process_reset_ai_at_error_state_in_src_logical_volume_dest_logical_volume
{
	my ($conn) = @_;
	my ($processServerId, $state,$sharedDeviceId,$initiator,$srcDeviceName,$nodeHostId);

	

	$logging_obj->log("DEBUG","process_reset_ai_at_error_state_in_src_logical_volume_dest_logical_volume");
	my @par_array = (
			Fabric::Constants::APPLIANCE_INITIATORS_NOT_CONFIGURED,
			Fabric::Constants::APPLIANCE_TARGETS_NOT_CONFIGURED,
			 Fabric::Constants::PRISM_FABRIC
			);
	my $fabricId = Fabric::Constants::PRISM_FABRIC;
	my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
					"SELECT DISTINCT ".
					" sd.sourceHostId, ".
					" sd.lun_state, ".
					" hsdm.sharedDeviceId, ".
					" hsdm.sanInitiatorPortWwn, ".
					" hsdm.srcDeviceName, ".
					" hsdm.hostId ".
					" FROM ".
					" srcLogicalVolumeDestLogicalVolume sd, ".
					" hostSharedDeviceMapping hsdm, ".
					" applianceFrBindingTargets afbt".
					" WHERE ".
					" lun_state in (?,?) ".
					" AND sd.Phy_Lunid = hsdm.sharedDeviceId ".
					" AND afbt.processServerId = sd.sourceHostId ".
					" AND afbt.fabricId = ? ".
					" ORDER BY sharedDeviceId", $par_array_ref);
	my @array = (\$processServerId, \$state, \$sharedDeviceId, \$initiator, \$srcDeviceName, \$nodeHostId);
	$conn->sql_bind_columns($sqlStmnt,@array);

	#$conn->sql_fetch($sqlStmnt);	
	#my $sqlStmnt1 = &get_appliance_target_lun_info($conn, $sharedDeviceId, $fabricId, $processServerId);

	$logging_obj->log("DEBUG","process_reset_ai_at_error_state_in_src_logical_volume_dest_logical_volume::".$conn->sql_num_rows($sqlStmnt));
	if ($conn->sql_num_rows($sqlStmnt)> 0) 
	{
		while($conn->sql_fetch($sqlStmnt))	
		{
			if($state == Fabric::Constants::APPLIANCE_INITIATORS_NOT_CONFIGURED)
			{
			Array::CommonProcessor::set_protection_job_state_to_error($conn, $sharedDeviceId, Fabric::Constants::APPLIANCE_INITIATORS_NOT_CONFIGURED,
                                           Fabric::Constants::START_PROTECTION_PENDING);
			}
			elsif ($state == Fabric::Constants::APPLIANCE_TARGETS_NOT_CONFIGURED)
			{
			Array::CommonProcessor::set_protection_job_state_to_error($conn, $sharedDeviceId, Fabric::Constants::APPLIANCE_TARGETS_NOT_CONFIGURED,
                                           Fabric::Constants::START_PROTECTION_PENDING);
			}
		}
	}
}

sub process_mirror_device_done
{   
    my ($conn) = @_;
    $logging_obj->log("DEBUG","process_mirror_device_done");
    my @par_array = (Fabric::Constants::CREATE_SPLIT_MIRROR_DONE);
    my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "select distinct sharedDeviceId, 
				hostId ".
				"from hostApplianceTargetLunMapping hatlm, ".
				"srcLogicalVolumeDestLogicalVolume sldlv,
				applicationPlan ap		
			WHERE 
				ap.solutionType = '".Fabric::Constants::PRISM_SOLUTION."' 
			AND 
				ap.planId=sldlv.planId 
			AND 	
				hatlm.sharedDeviceId = sldlv.Phy_Lunid
			AND
				hatlm.processServerId = sldlv.sourceHostId ".
			"AND state = ?",$par_array_ref);

    my $sharedDeviceId;
    my $processServerId;
    my $nodeHostId;

    my @array = (\$sharedDeviceId, \$nodeHostId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt))
    {
    	my @par_array = ($sharedDeviceId, $nodeHostId, Fabric::Constants::CREATE_SPLIT_MIRROR_DONE);
		my $par_array_ref = \@par_array;

    	my $sqlStmntPS = $conn->sql_query(
        "select distinct processServerId ".
        "from hostApplianceTargetLunMapping".
        " where sharedDeviceId = ? and ".
	" hostId = ? and state = ?",$par_array_ref);

    	$sqlStmntPS->bind_columns(\$processServerId);

    	while ($sqlStmntPS->fetch)
		{

				Array::CommonProcessor::finish_start_protection($conn, $sharedDeviceId, $processServerId, $nodeHostId, Fabric::Constants::CREATE_SPLIT_MIRROR_DONE);

		}
    }	

}

sub process_resync_cleared_request
{
	my ($conn) = @_;
    $logging_obj->log("DEBUG","process_resync_cleared");
	my ($nodeHostId,$sharedDeviceId,$currentState,$nextState);
	
    my @par_array = (Fabric::Constants::START_PROTECTION_PENDING,0,Fabric::Constants::RESYNC_PENDING,Fabric::Constants::RESYNC_NEEDED);
    my $par_array_ref = \@par_array;
	my $sqlQuery = "select distinct ".
        " hatlm.hostId, ".
        " hatlm.sharedDeviceId, ".
        " hatlm.state ".
        "from   ".
        " srcLogicalVolumeDestLogicalVolume sd ".
        " left join hostApplianceTargetLunMapping hatlm on (sd.Phy_Lunid = hatlm.sharedDeviceId) ".
        "where  ".
        " sd.lun_state = ? ".
        "and sd.replication_status  = ? ".
        "and hatlm.state  = ? ".
        "and sd.shouldResync  = ? ".
		"order by ".
        "hatlm.sharedDeviceId";
	my $sqlStmnt = $conn->sql_query($sqlQuery,$par_array_ref);
	if ($conn->sql_num_rows($sqlStmnt) > 0)
	{
		my @array = (\$nodeHostId, \$sharedDeviceId, \$currentState);
		$conn->sql_bind_columns($sqlStmnt,@array);
			
		while ($conn->sql_fetch($sqlStmnt)) 
		{
			$nextState = Fabric::Constants::MIRROR_SETUP_PENDING_RESYNC_CLEARED;
			my @par_array = ($nextState, $sharedDeviceId, $nodeHostId, $currentState);
			my $par_array_ref = \@par_array;

			$logging_obj->log("DEBUG","process_resync_cleared Count::".$conn->sql_num_rows($sqlStmnt));
			$logging_obj->log("DEBUG","process_resync_cleared Update:: $nextState, $sharedDeviceId, $nodeHostId, $currentState");
			my $sqlStmnt = $conn->sql_query(
				"update ".
				"hostApplianceTargetLunMapping ".
				" set state = ? ".
				"where ".
				"sharedDeviceId = ? and ".
				"hostId = ? and ".
				"state = ?",$par_array_ref);
		
		}
	}
}

sub start_resync
{
    # Resync will actually be started provided the shouldResync is set
    # as RESYNC_MUST_REQUIRED in srcLogicalVolumeDestLogicalVolume
    my ($conn) = @_;

    my $src_hostid;
    my $src_vol;
    my $dest_hostid;
    my $dest_vol;
    my $sanPhyLunId;
    my $resyncRequired;

    $logging_obj->log("DEBUG","start_resync - provided pair tgt is not PROFILER");
	my $RESYNC_STATES = Fabric::Constants::RESYNC_MUST_REQUIRED;
    my @par_array = (Fabric::Constants::PRISM_SOLUTION,Fabric::Constants::PROFILER_HOST_ID,Fabric::Constants::PROTECTED,Fabric::Constants::RESYNC_NEEDED,$RESYNC_STATES);
	
	my $prof =Fabric::Constants::PROFILER_HOST_ID;
	my $protected = Fabric::Constants::PROTECTED;
	my $resyncNeeded = Fabric::Constants::RESYNC_NEEDED;
	
    my $par_array_ref = \@par_array;
	my $sqlQuery = "SELECT DISTINCT ".
				"   sldl.sourceHostId, ".
				"   sldl.sourceDeviceName, ".
				"   sldl.destinationHostId, ".
				"   sldl.destinationDeviceName, ".
				"   sldl.Phy_Lunid, ".
				"   hsdm.resyncRequired ".
				"FROM  ".
				" applicationPlan ap, ".
				"   srcLogicalVolumeDestLogicalVolume sldl ".
				" left join hostSharedDeviceMapping hsdm on (sldl.Phy_Lunid = hsdm.sharedDeviceId) ".
				"WHERE  ".
				" ap.solutionType = ? AND ".	
				" ap.planId = sldl.planId AND ".
				"   sldl.destinationHostId != ? AND ".
				"   sldl.lun_state = ? AND ".
				"   sldl.shouldResync = ? AND ".
				"   hsdm.sharedDeviceId = sldl.Phy_Lunid AND ".
				"   hsdm.resyncRequired in(?)";
	my $myQuery = "SELECT DISTINCT ".
				"   sldl.sourceHostId, ".
				"   sldl.sourceDeviceName, ".
				"   sldl.destinationHostId, ".
				"   sldl.destinationDeviceName, ".
				"   sldl.Phy_Lunid, ".
				"   hsdm.resyncRequired ".
				"FROM  ".
				" applicationPlan ap, ".
				"   srcLogicalVolumeDestLogicalVolume sldl ".
				" left join hostSharedDeviceMapping hsdm on (sldl.Phy_Lunid = hsdm.sharedDeviceId) ".
				"WHERE  ".
				" ap.solutionType = 'PRISM' AND ".	
				" ap.planId = sldl.planId AND ".
				"   sldl.destinationHostId != '$prof' AND ".
				"   sldl.lun_state = '$protected' AND ".
				"   sldl.shouldResync = '$resyncNeeded' AND ".
				"   hsdm.sharedDeviceId = sldl.Phy_Lunid AND ".
				"   hsdm.resyncRequired in('$RESYNC_STATES')";
	
	$logging_obj->log("DEBUG","start_resync - $myQuery");			
	my $sqlStmnt = $conn->sql_query($sqlQuery,$par_array_ref);
	my $count = $conn->sql_num_rows($sqlStmnt);
	$logging_obj->log("DEBUG","start_resync - $count");			
    
	
	if ($conn->sql_num_rows($sqlStmnt) > 0)
	{
		my @array = (\$src_hostid, \$src_vol, \$dest_hostid, \$dest_vol, \$sanPhyLunId, \$resyncRequired);
		$conn->sql_bind_columns($sqlStmnt,@array);
		while ($conn->sql_fetch($sqlStmnt)) 
		{
			 my $src_dev = &format_device_name($src_vol);
			 my $dest_dev = &format_device_name($dest_vol);

			if($resyncRequired == Fabric::Constants::RESYNC_MUST_REQUIRED)
			{
				$logging_obj->log("DEBUG","start_resync - from UI  $src_hostid $src_dev $dest_hostid $dest_dev");
				my @par_array = ($src_hostid,$src_dev,$dest_hostid,$dest_dev);
				my $par_array_ref = \@par_array;

				my $sqlUpdate = $conn->sql_query(
				"UPDATE ".
				"   srcLogicalVolumeDestLogicalVolume ".
				"SET ".
				"isQuasiflag = 0,".
				"remainingQuasiDiffBytes = 0, ".
				"resyncStartTagtime = 0,".
				"resyncEndTagtime = 0,".
				"quasidiffEndtime = 0,".
				"shouldResync = 1,".
				"isResync = 1 ".
				"WHERE ".
				"sourceHostId = ? AND ".
				"sourceDeviceName = ? AND ".
				"destinationHostId = ? AND ".
				"destinationDeviceName = ?",$par_array_ref);
				
				# Reset resyncRequired to Zero if it is set to RESYNC_MUST_REQUIRED.
				my @par_array = (Fabric::Constants::RESYNC_MUST_REQUIRED,$sanPhyLunId);
				my $par_array_ref = \@par_array;

				my $sqlUpdateResetresyncRequired = $conn->sql_query(
				"UPDATE ".
				"   hostSharedDeviceMapping ".
				"SET ".
				"  resyncRequired = 0 ".
				"WHERE ".
				"  resyncRequired = ? ".
				" AND sharedDeviceId = ?",$par_array_ref);
						
		
			}
			else
			{
			
				$logging_obj->log("DEBUG","start_resync - resync_now.php  $src_hostid $src_dev $dest_hostid $dest_dev");
				
				if ((-e "$AMETHYST_VARS->{WEB_ROOT}/resync_now.php")&&(-e "/usr/bin/php"))
				{
				   `php $AMETHYST_VARS->{WEB_ROOT}/resync_now.php $src_hostid "$src_dev" $dest_hostid "$dest_dev"`;
				}
				elsif ((-e "$AMETHYST_VARS->{WEB_ROOT}/resync_now.php")&&(-e "/usr/bin/php5"))
				{
				   `php5 $AMETHYST_VARS->{WEB_ROOT}/resync_now.php $src_hostid "$src_dev" $dest_hostid "$dest_dev"`;
				}
			} 
		}

		
		# Reset shouldResync, as it is NOT applicable for PROFILER tgt
		$logging_obj->log("DEBUG","start_resync - update shouldResync flag - pair tgt is profiler");
		
		 my $sqlUpdate = $conn->sql_query(
				"UPDATE ".
				"   srcLogicalVolumeDestLogicalVolume ".
				"SET ".
				"  shouldResync = 0 ".
				"WHERE ".
				"   destinationHostId IN ".
				"      (SELECT ".
				"           DISTINCT (id) ".
				"       FROM ".
				"           hosts ".
				"       WHERE ".
				"           name = 'InMageProfiler')");
	}		
}

sub format_device_name
{
    my ($device_name) = @_;
    if ($device_name ne "")
    {
         $device_name =~ s/\\\\/\\/g;
         $device_name =~ s/\\/\\\\/g;
    }
    return $device_name;
}

# The following functions are specific to HA

sub process_change_db_state_flag_on_failover
{
	# Change the DB state flag
     my ($conn) = @_;

	my $lun_id;	
	my $sqlSlDl = $conn->sql_query("SELECT DISTINCT Phy_Lunid ".
	"from srcLogicalVolumeDestLogicalVolume ");
	my @array = (\$lun_id);
	$conn->sql_bind_columns($sqlSlDl,@array);
	
	 while ($conn->sql_fetch($sqlSlDl))
	 {	
		my $pair_type = TimeShotManager::get_pair_type($conn,$lun_id); # pair_type 0=host,1=fabric,2=prism

		$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover($lun_id,$pair_type)");
		if($pair_type == 2)
		{
			$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover");     
			my $hostName;
			my $hostIp;
			my $sourceDeviceName;
			my $from_process_server_info;
			my $to_process_server_info;
			my $hostGuId;
			my $oldHostId;
			my @par_array = (Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_PENDING, 
							 Fabric::Constants::FAILOVER_PRE_PROCESSING_DONE,$lun_id);
			my $par_array_ref = \@par_array;
			my $sqlUpdateSlDlOne = $conn->sql_query(
					"UPDATE ".
					"   srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  lun_state = ?".
					"WHERE ".
					" lun_state = ? AND Phy_Lunid =? ",$par_array_ref);
			
			@par_array = (Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_PENDING, 
					  Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_DONE,$lun_id);
				
			$par_array_ref = \@par_array;
			my $sqlUpdateSlDlTwo = $conn->sql_query(
					"UPDATE ".
					"   srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  lun_state = ?".
					"WHERE ".
					" lun_state = ? AND Phy_Lunid =? ",$par_array_ref);
		  
			 my $hostId;
			 @par_array = (Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_DONE,$lun_id);
			 $par_array_ref = \@par_array;
			 my $sqlUpdateSlDlThree = $conn->sql_query(
					"SELECT DISTINCT sourceHostId, Phy_Lunid, sourceDeviceName  ".
					"from   srcLogicalVolumeDestLogicalVolume ".
					"WHERE ".
					" lun_state = ? AND Phy_Lunid =? ",$par_array_ref);

			 my @array = (\$hostId, \$lun_id, \$sourceDeviceName);
			 $conn->sql_bind_columns($sqlUpdateSlDlThree,@array);
			my $numCount = $conn->sql_num_rows($sqlUpdateSlDlThree);	
			$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover::$numCount==".Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_DONE); 
			while ($conn->sql_fetch($sqlUpdateSlDlThree)) 
			{	
				 @par_array = (Fabric::Constants::REBOOT_PENDING, $lun_id, $hostId);
				 $par_array_ref = \@par_array;
				$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover::$hostId,$lun_id,$sourceDeviceName");
				my $sqlUpdateFrB = $conn->sql_query(
					"UPDATE ".
					"   hostApplianceTargetLunMapping ".
					"SET ".
					"  state = ? ".
					"WHERE ".
					" sharedDeviceId = ? and processServerId = ?",$par_array_ref);

				# my $sqlSelectHost = $conn->sql_query(
					# " SELECT ".
					# " 	   h.id, ".
					# " 	   h.name, ".
					# "	   h.ipaddress ".
					# " FROM".
					# "	   hosts h, ".
					# "	   cxPair cxp ".
					# " WHERE".
					# "	   cxp.hostId = h.id");
			
				# my @array = (\$hostGuId, \$hostName, \$hostIp);
				# $conn->sql_bind_columns($sqlSelectHost,@array);

				# $logging_obj->log("DEBUG","process_change_db_state_flag_on_failover Host Info  $hostName $hostIp");
				
				# while ($conn->sql_fetch($sqlSelectHost)) 
				# {
					# if($hostGuId eq $hostId)
					# {
						# $to_process_server_info = $hostName."[IP".$hostIp."]";
					# }
					# else
					# {
						# $from_process_server_info = $hostName."[IP".$hostIp."]";
					# }
					
					# $logging_obj->log("DEBUG","process_change_db_state_flag_on_failover Host Info  $hostId");
				# }
				@par_array = ($hostId,$hostId);
				$par_array_ref = \@par_array;				
				my $sqlSelectHost = $conn->sql_query(
									" SELECT ".
									" b.nodeId as nodeId".
									" FROM applianceNodeMap a ,applianceNodeMap b ".
									" WHERE ".
									" a.nodeId = ? ".
									" AND   a.applianceId = b.applianceId ".
									" AND ".
									" b.nodeId != ? ",$par_array_ref);	
				
				my @array = (\$oldHostId);
				$conn->sql_bind_columns($sqlSelectHost,@array);
                $conn->sql_fetch($sqlSelectHost);
				$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover Host Info ::$oldHostId");

				@par_array = ($hostId ,$oldHostId);
				$par_array_ref = \@par_array;
				my $sqlSelectHost = $conn->sql_query(
					" SELECT ".
					" 	   id, ".
					" 	   name, ".
					"	   ipaddress ".
					" FROM".
					"	   hosts  ".
					" WHERE".
					"	  id IN (?,?)" ,$par_array_ref);
			
				my @array = (\$hostGuId, \$hostName, \$hostIp);
				$conn->sql_bind_columns($sqlSelectHost,@array);

				$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover Host Info  $hostName $hostIp");
				
				while ($conn->sql_fetch($sqlSelectHost)) 
				{
					if($hostGuId eq $hostId)
					{
						$to_process_server_info = $hostName."[IP".$hostIp."]";
					}
					else
					{
						$from_process_server_info = $hostName."[IP".$hostIp."]";
					}
					
					$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover Host Info  $hostId");
				}
				
				my $agentLogString = "Failover event occurred from Process server $from_process_server_info to $to_process_server_info";
				Utilities::update_agent_log($hostId, $sourceDeviceName, $WEB_ROOT, $agentLogString);
			}
			@par_array = (Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING, Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_DONE,$lun_id);
			$par_array_ref = \@par_array;

		   
			  my $sqlUpdateSlDlFour = $conn->sql_query(
					"UPDATE ".
					"   srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  lun_state = ?".
					"WHERE ".
					" lun_state = ? AND Phy_Lunid = ? ",$par_array_ref); 
		}	
	}	
}

sub process_driver_cleanup_on_failover
{
	# Clean all the driver side stale info
    my ($conn) = @_;

	my $src_hostid;
    my $lun_id;
	my $rule_id;
	my $atlun_id;

	my $proc_dir = "/proc/involflt/";
	my $proc_cmd = "echo \"clear_diffs\" >";
	my $exce_cmd;

	my $sqlSlDl = $conn->sql_query("SELECT DISTINCT Phy_Lunid ".
	"from srcLogicalVolumeDestLogicalVolume ");
	my @array = (\$lun_id);
	$conn->sql_bind_columns($sqlSlDl,@array);
	while ($conn->sql_fetch($sqlSlDl))
	{	
	
		my $pair_type = TimeShotManager::get_pair_type($conn,$lun_id); # pair_type 0=host,1=fabric,2=prism

		$logging_obj->log("DEBUG","process_driver_cleanup_on_failover($lun_id,$pair_type)");
		if($pair_type == 2)
		{
			$logging_obj->log("DEBUG","process_driver_cleanup_on_failover");
			my @par_array = (Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_PENDING,$lun_id);
			my $par_array_ref = \@par_array;
			
			my $sqlStmnt = $conn->sql_query(
				"SELECT ".
				"   ruleId, ".
				"   sourceHostId, ".
				"   Phy_Lunid ".
				"FROM  ".
				"   srcLogicalVolumeDestLogicalVolume ".
				"WHERE  ".
				"   lun_state = ? AND Phy_Lunid = ? ",$par_array_ref);
		   
			 my @array = (\$rule_id, \$src_hostid, \$lun_id);
			 $conn->sql_bind_columns($sqlStmnt,@array);
			
				 while ($conn->sql_fetch($sqlStmnt)) {
				@par_array = ($lun_id, $src_hostid);
				$par_array_ref = \@par_array;
				my $sqlStmntAtl = $conn->sql_query(
					"SELECT ".
					"   applianceTargetLunMappingId ".
					"FROM  ".
					"   applianceTargetLunMapping ".
					"WHERE  ".
					"   sharedDeviceId = ? AND  ".
					"   processServerId = ? ",$par_array_ref);

			
				my @array = (\$atlun_id);
				$conn->sql_bind_columns($sqlStmntAtl,@array);
				while ($conn->sql_fetch($sqlStmntAtl)) 
				{
					$exce_cmd = $proc_cmd.$proc_dir.$atlun_id;
					system($exce_cmd);
				}
				@par_array = (Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_DONE, $rule_id);
				$par_array_ref = \@par_array;
				
				my $sqlUpdateSlDl = $conn->sql_query(
					"UPDATE ".
					"   srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  lun_state = ?".
					"WHERE ".
					" ruleId = ?",$par_array_ref);
				
			}
		}
	}
}



sub process_cx_cleanup_on_failover
{
	# Clean all the CCX side state resync/diffs file from CX
    my ($conn) = @_;

    my $rule_id;
	my $src_hostid;
    my $src_vol;
    my $dest_hostid;
    my $dest_vol;
    my $lun_id;

    my $sqlSlDl = $conn->sql_query("SELECT DISTINCT Phy_Lunid ".
	"from srcLogicalVolumeDestLogicalVolume ");
	my @array = (\$lun_id);
	$conn->sql_bind_columns($sqlSlDl,@array);
	while ($conn->sql_fetch($sqlSlDl))
	{
	
		my $pair_type = TimeShotManager::get_pair_type($conn,$lun_id); # pair_type 0=host,1=fabric,2=prism

		$logging_obj->log("DEBUG","process_cx_cleanup_on_failover($lun_id,$pair_type)");
		if($pair_type == 2)
		{	
			$logging_obj->log("DEBUG","process_cx_cleanup_on_failover");
			my @par_array = (Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_PENDING,$lun_id);
			my $par_array_ref = \@par_array;
			
			my $sqlStmnt = $conn->sql_query(
				"SELECT ".
				"   ruleId, ".
				"   sourceHostId, ".
				"   sourceDeviceName, ".
				"   destinationHostId, ".
				"   destinationDeviceName ".
				"FROM  ".
				"   srcLogicalVolumeDestLogicalVolume ".
				"WHERE  ".
				"   lun_state = ? AND Phy_Lunid = ? ",$par_array_ref);
			 $sqlStmnt->bind_columns(\$rule_id, \$src_hostid, \$src_vol, \$dest_hostid, \$dest_vol);

			 while ($sqlStmnt->fetch) {
				@par_array = (Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_DONE, $rule_id);
				$par_array_ref = \@par_array;
				if(-e "$AMETHYST_VARS->{INSTALLATION_DIR}/$src_hostid/$src_vol")
				{
					`rm -rf $AMETHYST_VARS->{INSTALLATION_DIR}/$src_hostid/$src_vol`;
				}
				if(-e "$AMETHYST_VARS->{INSTALLATION_DIR}/$dest_hostid/$dest_vol")
				{
					`rm -rf $AMETHYST_VARS->{INSTALLATION_DIR}/$dest_hostid/$dest_vol`;
				}
				
				my $sqlUpdateSlDl = $conn->sql_query(
					"UPDATE ".
					"   srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  lun_state = ?".
					"WHERE ".
					" ruleId = ?",$par_array_ref);

			}
		}
	}
}

sub get_unregistered_host_status
{
	my($hid) = @_;
	$logging_obj->log("DEBUG","get_unregistered_host_status::$hid");
	my @par_array = ($hid);
	my $par_array_ref = \@par_array;
    my($Phy_Lunid);
	
	my $sqlStmnt = $conn->sql_query(
        "SELECT ".
        "   Phy_Lunid ".
        "FROM  ".
        "   srcLogicalVolumeDestLogicalVolume ".
        "WHERE  ".
        "   processServerId = ? ",$par_array_ref);
    $sqlStmnt->bind_columns(\$Phy_Lunid);
	my $count = $conn->sql_num_rows($sqlStmnt);
	$logging_obj->log("DEBUG","get_unregistered_host_status PS::$count");
	if ($count > 0)
	{
		return 1;
	}
	else
	{
		my $sqlStmnt = $conn->sql_query(
        "SELECT ".
        "   Phy_Lunid ".
        "FROM  ".
        "   srcLogicalVolumeDestLogicalVolume ".
        "WHERE  ".
        "   destinationHostId = ? ",$par_array_ref);
		$sqlStmnt->bind_columns(\$Phy_Lunid);
		my $count = $conn->sql_num_rows($sqlStmnt);
		$logging_obj->log("DEBUG","get_unregistered_host_status Target::$count");
		if ($count > 0)
		{
			return 2;
		}
		else
		{
			my $sqlStmnt = $conn->sql_query("SELECT 
                        src.Phy_Lunid
					FROM
                        srcLogicalVolumeDestLogicalVolume src,
						applicationScenario appS 
                   WHERE  
                        appS.sourceId LIKE ('%$hid%') AND
						src.planId = appS.planId AND 
						src.scenarioId = appS.scenarioId");
			#$sqlStmnt->bind_columns(\$Phy_Lunid);
			my $count = $conn->sql_num_rows($sqlStmnt);
			$logging_obj->log("DEBUG","get_unregistered_host_status Source::$count");
			if ($count > 0)
			{
				return 3;
			}	
		
		}
	}
}

# sub getHostName
# {
	
	# my ($conn, $hostId) = @_;
	# my ($name);
	# $logging_obj->log("DEBUG","getHostName($hostId)");

	# my @par_array = ($hostId);
	# my $par_array_ref = \@par_array;

	# my $sqlStmnt = $conn->sql_query("SELECT 
				# distinct name 
			# FROM 
				# hosts 
			# WHERE 
				# id=?",$par_array_ref);
	
	#my $rs = $conn->sql_query($sqlStmnt);

	# if ($conn->sql_num_rows($sqlStmnt) == 0)
	# {
		# return 0;
    # }
	# my @array = (\$name );			
	# $conn->sql_bind_columns($sqlStmnt,@array); 			
	# $conn->sql_fetch($sqlStmnt);
	# return $name;
# }
 

# sub getHostIpaddress
# {
	
	# my ($conn,$hostId) = @_;
	# my ($ipaddress);
	# $logging_obj->log("DEBUG","getHostIpaddress($hostId)");

	# my @par_array = ($hostId);
	# my $par_array_ref = \@par_array;

	# my $sqlStmnt = "SELECT 
				# distinct ipaddress 
			# FROM 
				# hosts 
			# WHERE 
				# id='$hostId'";
				
	# my $rs = $conn->sql_query($sqlStmnt);

	#$logging_obj->log("DEBUG","getHostIpaddress($sqlStmnt)");

	# if (my $ref = $conn->sql_fetchrow_hash_ref($rs))
	# {
		# return $ipaddress = $ref->{'ipaddress'};
	# }
	# else
	# {
		# return 0;
	# }	
# }

# sub getNodeDeviceName
# {
	
	# my ($conn,$sharedDeviceId) = @_;
	# my ($hostId,$srcDeviceName,$hostName,$deviceNameStr);
	# my @str;
	# $logging_obj->log("DEBUG","getNodeDeviceName($sharedDeviceId)");
	
	# my @par_array = ($sharedDeviceId);
	# my $par_array_ref = \@par_array;

	# my $sqlStmnt = "SELECT
                                    # distinct  hostId,
                                    # srcDeviceName  
									# FROM
                                    # hostApplianceTargetLunMapping  
									# WHERE
                                    # sharedDeviceId ='$sharedDeviceId'";
	#&fabric_util_trace("getNodeDeviceName($sqlStmnt)");			
	# my $rs = $conn->sql_query($sqlStmnt);

	# if($conn->sql_num_rows($rs)>0)
	# {
		# while (my $ref = $conn->sql_fetchrow_hash_ref($rs))
		# {
			# $hostId = $ref->{'hostId'};
			# $srcDeviceName = $ref->{'srcDeviceName'};
			# $hostName = &getHostName($conn,$hostId);
			# $deviceNameStr = $deviceNameStr.$srcDeviceName ." on ". $hostName.",";
		# }
		# $logging_obj->log("DEBUG","getNodeDeviceName($deviceNameStr)");
	# }
	# else
	# {
		# $deviceNameStr = 0;
	# }
		# return $deviceNameStr;
# }



sub prism_requests
{  
	my %args;
	$logging_obj->log("DEBUG","Prism Process request");
	
	my $db_name = $AMETHYST_VARS->{'DB_NAME'};
	my $db_passwd = $AMETHYST_VARS->{'DB_PASSWD'};
	my $db_host = "localhost";

	%args = ( 'DB_NAME' => $db_name,
	          'DB_PASSWD'=> $db_passwd,
		      'DB_HOST' => $db_host
	        );
	my $args_ref = \%args;
	
	my $origAutoCommit;
    my $origRaiseError;
    eval 
	{
		$conn = new Common::DB::Connection($args_ref);
	        $origAutoCommit = $conn->get_auto_commit();
        	$origRaiseError = $conn->get_raise_error();

        $conn->set_auto_commit(0,0);
        
	#print $conn;
	
		if(!$conn->sql_ping_test())
		{
			&process_src_logical_volume_dest_logical_volume_requests($conn);
			$conn->sql_commit();
			
			&process_delete_split_mirror_done($conn);
			$conn->sql_commit();
			
			&process_host_shared_device_mapping_requests($conn);
			$conn->sql_commit();
	
			&process_create_at_lun_done($conn);
			$conn->sql_commit();

			&process_delete_at_lun_done($conn);
	         	$conn->sql_commit();
			
			&process_reset_ai_at_error_state_in_src_logical_volume_dest_logical_volume($conn);
            		$conn->sql_commit();
		
			&process_mirror_device_done($conn); 
		        $conn->sql_commit();
			
			&process_resync_cleared_request($conn);
			$conn->sql_commit();		

			&start_resync($conn);
			$conn->sql_commit();

			&process_change_db_state_flag_on_failover($conn);
		        $conn->sql_commit();
	        
			&process_driver_cleanup_on_failover($conn);
			$conn->sql_commit();
	        
			&process_cx_cleanup_on_failover($conn);
			$conn->sql_commit();
			
			&process_reboot_pending($conn);
			$conn->sql_commit();
			
			&process_reboot_failures($conn);
			$conn->sql_commit();
			
			&process_lun_resize($conn);
			$conn->sql_commit();
			
		}
	};
	if ($@) 
	{
         $logging_obj->log("EXCEPTION","ERROR -".$@);
        eval 
		{
            $conn->sql_rollback();
	    
        };
   
        $conn->set_auto_commit($origAutoCommit,$origRaiseError);
        return;
    }

    $conn->set_auto_commit($origAutoCommit,$origRaiseError);
    
	Fabric::FabricUtilities::checkForQuitEvent();
	$logging_obj->log("DEBUG","afte calling checkForQuitEvent()");
}
1;
