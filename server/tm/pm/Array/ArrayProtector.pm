# File       : PrismProtector.pm
#
# Description: this file will handle protection pair 
# request and manage state for pair.
#

package Array::ArrayProtector;

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
use PHP::Serialization qw(serialize unserialize);
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
	
	my $solutionType = Fabric::Constants::ARRAY_SOLUTION;
	my @par_array = ($solutionType);
    my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "select distinct sharedDeviceId ".
				"from hostSharedDeviceMapping hsdm, ".
				"srcLogicalVolumeDestLogicalVolume sldlv,
				applicationPlan ap		
			WHERE 
				ap.solutionType = ? 
			AND 
				ap.planId=sldlv.planId 
			AND 	
				hsdm.sharedDeviceId = sldlv.Phy_Lunid",$par_array_ref);

    my $sharedDeviceId;
    my @array = (\$sharedDeviceId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt))
    {
		Array::CommonProcessor::process_src_logical_volume_dest_logical_volume_start_protection_requests($conn,$solutionType,$sharedDeviceId);
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
                            Fabric::Constants::ARRAY_SOLUTION,
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
		" sd.lun_state = ? ".
        "and sd.replication_status  = ? ".
		"order by ".
        " hatlm.processServerId, hatlm.sharedDeviceId",$par_array_ref);

    my @array = (\$sourceHostId, \$sdLunId, \$processServerId);
    $conn->sql_bind_columns($pairSqlStmnt, @array);
    my @applianceInitiatorInfos;
    my @applianceTargetInfos;
    while ($conn->sql_fetch($pairSqlStmnt)) 
	{        
        if (&san_lun_one_to_n($conn, $sdLunId)) 
		{
			$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_stop_protection_requests:: san_lun_one_to_n");
            &delete_protection_job($conn, $sdLunId, $processServerId);
        } 
		else 
		{

			my @par_array = ($sdLunId);
			my $par_array_ref = \@par_array;
            my $sqlStmnt = $conn->sql_query(
                "SELECT distinct ".
                "    sanInitiatorPortWwn, ".
                "    srcDeviceName, ".
                "    hostId ".
                "FROM   ".
                "    hostSharedDeviceMapping ".
                "WHERE  ".
                "    	sharedDeviceId  = ? ",$par_array_ref);

		    my @array = (\$initiator, \$srcDeviceName,
                                    \$nodeHostId);
			$conn->sql_bind_columns( $sqlStmnt,@array);

			if($conn->sql_num_rows($sqlStmnt) > 0)
			{
			
				while ($conn->sql_fetch($sqlStmnt)) {
					#if($processServerId ne "" && ($processServerId ne $sourceHostId))
					#{
						# For passive node, directly move to AT Lun done
						#&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_AT_LUN_DONE, $initiator, $srcDeviceName, $sdLunId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;
					#}
					#else
					#{
						# Call this if it's an active one
							&process_stop_protection(
								$conn, $initiator, $srcDeviceName, $sdLunId, $nodeHostId, $sourceHostId);
					#}
				}
			}
			else
			{
				$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_stop_protection_requests::MirrorDeletion failed.");
				&delete_protection_job($conn, $sdLunId, $sourceHostId);
			}
        }
    }
}

sub process_stop_protection
{
    my ($conn, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId) = @_;

    $logging_obj->log("DEBUG","process_stop_protection($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId)");    

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
				&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;								   
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

    #$logging_obj->log("DEBUG","update_host_appliance_target_lun_mapping_delete_state($nextState, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId, @inStates)");

    my $inStates = join(',', @inStates);

    $logging_obj->log("DEBUG","instates: $inStates");	
    my @par_array = ($initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId);
	my $par_array_ref = \@par_array;

	my $sql = "select sharedDeviceId ".
        " from hostApplianceTargetLunMapping ".
        " where ".
        "  exportedInitiatorPortWwn = ? and ".
        "  srcDeviceName = ? and ".
        "  sharedDeviceId = ? and ".
        "  hostId = ? and ".
        "  processServerId = ?";
	my $sqlStmnt = $conn->sql_query($sql,$par_array_ref);
    $logging_obj->log("DEBUG","update_host_appliance_target_lun_mapping_delete_state :: select SQL :: $sql\nValues to : $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId \n");


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

sub update_array_lun_map_info_delete_state
{
        my ($conn, $nextState, $sharedDeviceId, $nodeHostId, $processServerId, @inStates) = @_;
        my $inStates = join(',', @inStates);
        $logging_obj->log("DEBUG","instates: $inStates");
        my @par_array = ($sharedDeviceId, $processServerId);
        my $par_array_ref = \@par_array;
        my $sql = "select sharedDeviceId ".
                  " from arrayLunMapInfo ".
                  " where ".
                  "  sharedDeviceId = ? and ".
                  "  protectedProcessServerId = ?";
        my $sqlStmnt = $conn->sql_query($sql,$par_array_ref);
        $logging_obj->log("DEBUG","update_array_lun_map_info_delete_state :: select SQL :: $sql\nValues to : $sharedDeviceId,  $processServerId \n");
        if (0 == $conn->sql_num_rows($sqlStmnt)) {
           return 0;
        }

        # TODO: for now just go forward on the deletion even if there
        # are reported errors
        my @par_array = ($nextState, $sharedDeviceId, $processServerId);
        my $par_array_ref = \@par_array;
        my $sqlStmnt = $conn->sql_query("update arrayLunMapInfo ".
                                        " set state = ? ".
                                        " where ".
                                        "  sharedDeviceId = ? and ".
                                        "  protectedProcessServerId = ?",$par_array_ref);
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


        my @par_array = ($sdLunId, Fabric::Constants::STOP_PROTECTION_PENDING,Fabric::Constants::SOURCE_DELETE_DONE, $processServerId);
		my $par_array_ref = \@par_array;

		my $sqlStmnt = $conn->sql_query(
            "select sourceDeviceName, destinationHostId, destinationDeviceName ".
            "from srcLogicalVolumeDestLogicalVolume ".
            "where Phy_lunid = ? ".
            " and lun_state = ? ".
			" and replication_status not in (?) and ".
            " sourceHostId = ?",$par_array_ref);


        if ($conn->sql_num_rows($sqlStmnt) > 0) 
		{

            my @array = (\$srcDev, \$dstHostId, \$dstDev );
            $conn->sql_bind_columns($sqlStmnt,@array);

            $conn->sql_fetch($sqlStmnt);

           # itl_trace("stop_replication.php  $processServerId, $srcDev, $dstHostId, $dstDev");
            `cd $AMETHYST_VARS->{WEB_ROOT}/ui/; php stop_replication.php  "$processServerId" "$srcDev" "$dstHostId" "$dstDev"`;
			$logging_obj->log("DEBUG","delete_protection_job:  Num rows ".$conn->sql_num_rows($sqlStmnt)." for delete protection job");
			$logging_obj->log("DEBUG","delete_protection_job( setting 30 for the pair with srcDev as $sdLunId -- dest $dstDev");

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
		if (&san_lun_one_to_n($conn, $sdLunId)) 
		{
            # OK to delete assumes that one of the 1-to-n pairs is being deleted
            # so nothing was removed from hostApplianceTargetLunMapping in that case
            return 1;
        }
        # don't delete it yet other ITL for this lun still in use
        return 0;
    }
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
        #entries were added so we can now set them to protected
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
        
        #Send mail with reconfiguration success
        

       # my @array = (\$nodeHostId, \$sourceHostId, \$targetId, \$targetDevice);
       # $conn->sql_bind_columns($sqlStmnt,@array);

       # my $errId = $sourceHostId;
        
		# my (@source_host_name_list,@source_ip_address_list);
		# my ($source_host_name,$source_ip_address,$nodeId);
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
				# @source_host_name_list = &getHostName($conn,$nodeId);
				# @source_ip_address_list = &getHostIpaddress($conn,$nodeId);
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

 sub san_lun_one_to_n
 {
	 my($conn, $sharedDeviceId) = @_;
	 $logging_obj->log("DEBUG", "san_lun_one_to_n");
	 my @par_array = ($sharedDeviceId);
	 my $par_array_ref = \@par_array;
	 my $StatesNotOkToProcessDeletion = Fabric::Constants::SOURCE_DELETE_DONE .
        "," . Fabric::Constants::PS_DELETE_DONE;
	 my $sqlStmnt = $conn->sql_query(
						 "SELECT DISTINCT ".
						 " Phy_Lunid, destinationHostId, destinationDeviceName ".
						 " FROM ".
						 " srcLogicalVolumeDestLogicalVolume  ".
						 " WHERE ".
						 " Phy_Lunid = ? AND ".
						 " replication_status ".
						 " NOT IN($StatesNotOkToProcessDeletion)",$par_array_ref);
	
    #1-to-n if more then one row returned
     $logging_obj->log("DEBUG", "san_lun_one_to_n :: ".$conn->sql_num_rows($sqlStmnt)." rows");
     return $conn->sql_num_rows($sqlStmnt) > 1;					
}

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
	
    my @par_array = (Fabric::Constants::DELETE_AT_LUN_DONE);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query(
        "select distinct ".
        " exportedInitiatorPortWwn,".
        " srcDeviceName,".
        " sharedDeviceId,".
        " applianceTargetLunMappingId,".
        " applianceTargetLunMappingNumber,".
        " hostId,".
        " accessControlGroupId,".
        " processServerId ".
        "from hostApplianceTargetLunMapping ".
        "where state = ?",$par_array_ref);


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
    $logging_obj->log("DEBUG","process_delete_at_lun_done");
    while ($conn->sql_fetch($sqlStmnt)) 
	{
        
		#
		# This is the general stop protection flow
		#
		
		my $count = scalar(@{$hostIds});
		$logging_obj->log("DEBUG","process_delete_at_lun_done - $count");
		
		if(&check_array_type($conn,$sharedDeviceId))
		{
			my $inState = Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_PENDING.
					",".Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_DONE.
					",".Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING.
					",".Fabric::Constants::STOP_PROTECTION_PENDING.
					",".Fabric::Constants::LUN_RESIZE_RECONFIGURATION_DONE;
			#my @par_array = (
                        #    Fabric::Constants::STOP_PROTECTION_PENDING,
                         #   Fabric::Constants::SOURCE_DELETE_PENDING,
                          #  $sharedDeviceId
			#				);
			my @par_array = ($sharedDeviceId);
			my $par_array_ref = \@par_array;
			my($sourceHostId, $srcDevName, $destHostId, $destDevName, $sdLunId, $rep_options,$lunState);
            my @pair_array = (\$sourceHostId, \$srcDevName, \$destHostId, \$destDevName, \$sdLunId, \$rep_options, \$lunState);
			my $sql = "SELECT DISTINCT ".
										"	sourceHostId, ".
										"	sourceDeviceName, ".
										"	destinationHostId, ".
										"	destinationDeviceName, ".
										"	Phy_Lunid, ". 
										"	replicationCleanupOptions, ".
										"       lun_state ".
										"FROM". 
										"	srcLogicalVolumeDestLogicalVolume  ".
										"WHERE ".
										"	lun_state IN ($inState) AND ".
										"	Phy_LunId = ?";
			my $pairSqlStmnt = $conn->sql_query($sql,$par_array_ref);
            $conn->sql_bind_columns($pairSqlStmnt,@pair_array);
            while ($conn->sql_fetch($pairSqlStmnt))
            {
                $logging_obj->log("DEBUG","process_delete_at_lun_done SQL :: fetched values = $sourceHostId, $srcDevName, $destHostId, $destDevName, $sdLunId, $rep_options,$lunState\n");
			
		if(Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_PENDING == $lunState)
           {
                    #
                    # update lun_state as LUN_RESIZE_PRE_PROCESSING_DONE
                    # in srcLogicalVolumeDestLogicalVolume
                    #
                    my @par_arary = (Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_DONE,$sdLunId,Fabric::Constants::LUN_RESIZE_PRE_PROCESSING_PENDING);
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


				$logging_obj->log("DEBUG","process_delete_at_lun_done lun resize - updated to LUN_RESIZE_PRE_PROCESSING_DONE");
            }		
			elsif($rep_options eq "0010101000" or $rep_options eq "0010101010"  or $rep_options eq "1010101000" or $rep_options eq "1010101010")
    		{
	    			&update_host_appliance_target_lun_mapping_delete_state(
		    			$conn,  Fabric::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING,
			    		$initiator, $srcDeviceName,  $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_AT_LUN_DONE);
                    my @alm_sh_devs = split("/dev/mapper/",$destDevName);
                    my $alm_sh_dev_id = $alm_sh_devs[1];
                    &update_array_lun_map_info_delete_state($conn,  Fabric::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING,
                     $alm_sh_dev_id, $nodeHostId, $destHostId, Fabric::Constants::DELETE_AT_LUN_DONE);
				    &invoke_delete_lun_map($conn);
    		    		$logging_obj->log("DEBUG","process_delete_at_lun_done updated to
		    	    	Fabric::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING\n");
	    	}
            elsif($destHostId eq Fabric::Constants::PROFILER_HOST_ID && $destDevName eq Fabric::Constants::PROFILER_DEVICE_NAME)
            {
				&delete_from_hatlm($conn,$sharedDeviceId);
				&delete_from_alm($conn,$sharedDeviceId);
              	&update_delete_state_in_src($conn,$sharedDeviceId,$destDevName,Fabric::Constants::SOURCE_DELETE_DONE);
            }
    		else
	    	{
		    		&update_host_appliance_target_lun_mapping_delete_state(
			    		$conn,  Fabric::Constants::SOURCE_DELETE_DONE,
				    	$initiator, $srcDeviceName,  $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_AT_LUN_DONE);				
		               	if(Fabric::Constants::STOP_PROTECTION_PENDING == $lunState)
                    	{
							&update_delete_state_in_src($conn,$sharedDeviceId,$destDevName,Fabric::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING);
		    			}	
    		}   
	    }
        }        
		elsif ($count > 0)
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
	# foreach my $scsiId (@sharedDeviceList)
	# {
		# $logging_obj->log("DEBUG","scsiId==>$scsiId");
		# my $src_state = &is_lun_protected($conn,$scsiId);
		# if($src_state != Fabric::Constants::STOP_PROTECTION_PENDING)
		# {
			# &update_state_in_src($conn,$scsiId,Fabric::Constants::NODE_REBOOT_OR_PATH_ADDITION_PENDING);
		# }
	# }
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

sub update_delete_state_in_src
{
	my ($conn,$sharedDeviceId,$dest_dev,$rep_state) = @_;
	$logging_obj->log("DEBUG","update_delete_state_in_src($sharedDeviceId) and destDevname = $dest_dev");
    my @par_array = ($rep_state,$sharedDeviceId,$dest_dev);
    my $par_array_ref = \@par_array;


	my $sqlStmnt = $conn->sql_query(
        "update srcLogicalVolumeDestLogicalVolume ".
        " set  replication_status = ? ".
        " where Phy_LunId = ? ".
		" AND destinationDeviceName = ?"
        ,$par_array_ref);

}

sub delete_from_hatlm
{
    my ($conn,$sharedDeviceId) = @_;
    $logging_obj->log("DEBUG","delete_from_hatlm ($sharedDeviceId)\n");
    my @par_array = ($sharedDeviceId);
    my $par_array_ref = \@par_array;


    my $sqlStmnt = $conn->sql_query(
        "delete from hostApplianceTargetLunMapping ".
        " where sharedDeviceId = ? ",$par_array_ref);
}

sub delete_from_alm
{
    my ($conn,$sharedDeviceId) = @_;
    $logging_obj->log("DEBUG","delete_from_alm ($sharedDeviceId)\n");
    my @par_array = ($sharedDeviceId);
    my $par_array_ref = \@par_array;


    my $sqlStmnt = $conn->sql_query(
        "delete from arrayLunMapInfo ".
        " where sharedDeviceId = ? ",$par_array_ref);
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

sub protect_itl_if_needed
{
    my ($conn, $state, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId) = @_;

	my $fabricId = Fabric::Constants::PRISM_FABRIC;
    $logging_obj->log("DEBUG","protect_itl_if_needed($state, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId)");

    my $processServerId;
 
    # we need the process server id if we find that the lun is being protected

   my @par_array = ($sharedDeviceId);
   my $par_array_ref = \@par_array;
   my $sqlStmnt = $conn->sql_query(
        "SELECT DISTINCT hatlm.processServerId FROM  hostApplianceTargetLunMapping hatlm, srcLogicalVolumeDestLogicalVolume sldl ".
        "WHERE hatlm.sharedDeviceId = ? AND sldl.Phy_Lunid = hatlm.sharedDeviceId AND hatlm.processServerId = sldl.sourceHostId ",$par_array_ref);

    if ($conn->sql_num_rows($sqlStmnt) == 0) 
	{
        return 1;
    }

    my @array = (\$processServerId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    # make sure to get the process server id in case we need it
    $conn->sql_fetch($sqlStmnt);
    # lun is in hostApplianceTargetLunMapping, but we don't want to start this one
    # if there are any entries that are not in the protected state
    # that way we avoid any complications with having to make sure
    # this is to avoid any complications when entries are in different
    # stages of completion
    my @par_array = ($sharedDeviceId, Fabric::Constants::PROTECTED);
	my $par_array_ref = \@par_array;
	$sqlStmnt = $conn->sql_query(
        "select sharedDeviceId from hostApplianceTargetLunMapping ".
        "where sharedDeviceId = ? and ".
        " not state = ?",$par_array_ref);

    if ($conn->sql_num_rows($sqlStmnt) == 0) 
	{
        &set_all_luns_for_itl_to_restart($conn, $sharedDeviceId);
        Array::CommonProcessor::protect_device($conn, $processServerId, $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId);
        return 1;
    }

    return 0;
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
	&invoke_mirror_configuration_policy($conn);
	&invoke_mirror_monitor_policy($conn);
}

##
#function: This function inserts mirror configuration policy after creation of ATLUN done in Array Case
#Date: Jan 6,2011
##

sub invoke_mirror_configuration_policy
{
	my ($conn) = @_;
	$logging_obj->log("DEBUG","inside invoke_mirror_configuration_policy \n");
	
	my $sql = 	"SELECT  
					appsc.scenarioId,                                         
					appsc.sourceId,                                         
					appsc.sourceVolumes,                                 
					appsc.sourceArrayGuid,
					appsc.targetArrayGuid,
					appsc.protectionDirection
				FROM
					applicationScenario appsc, 
					applicationPlan ap 
				WHERE
					appsc.executionStatus='92'
				AND                                         
					appsc.applicationType='BULK' 
				AND 
					ap.solutionType= '".Fabric::Constants::ARRAY_SOLUTION."' 
				AND 
					ap.planId=appsc.planId";

	$logging_obj->log("DEBUG","insert_mirror_configuration_policy sql1 :: $sql \n");
	
    my $pairSqlStmnt = $conn->sql_query($sql);
	my ($scenario_id, $process_server_id, $source_volumes,$source_array_guid,$hash_key, $hash_value,$target_array_guid,$protection_direction);
	my @array = (\$scenario_id, \$process_server_id, \$source_volumes,\$source_array_guid,\$target_array_guid,\$protection_direction);
    $conn->sql_bind_columns($pairSqlStmnt, @array);
    while ($conn->sql_fetch($pairSqlStmnt)) 
	{
			
		my $sql = 	"SELECT 
							distinct 
							hatlm.sharedDeviceId,
							sldlv.restartResyncCounter,
							sldlv.shouldResync,
							sldlv.isQuasiflag,
							sldlv.sourceHostId,
							sldlv.sourceDeviceName,
							sldlv.destinationHostId,
							sldlv.destinationDeviceName
						FROM
							 hostApplianceTargetLunMapping hatlm, srcLogicalVolumeDestLogicalVolume sldlv
						WHERE
							hatlm.sharedDeviceId = sldlv.Phy_Lunid
						AND
							hatlm.processServerId = sldlv.sourceHostId
						AND
							sldlv.scenarioId = ?
						AND
							(hatlm.state = ? OR  hatlm.state = ?)";
		
		 $logging_obj->log("DEBUG","insert_mirror_configuration_policy sql1 :: $sql");
		
		  my @par_array = ($scenario_id, Fabric::Constants::CREATE_SPLIT_MIRROR_PENDING,  Fabric::Constants::MIRROR_SETUP_PENDING_RESYNC_CLEARED);

		my $par_array_ref = \@par_array;
		my $sql_stmt = $conn->sql_query($sql,$par_array_ref);

		my ($shared_device_id, 
			$restart_resync_counter,
			$should_resync,
			$is_quasiflag, 
			$protected_process_server_id, 			$source_device_name, 
			$destination_host_id, 
			$destination_device_name,
			@first_resync_shared_device_list,
			@restart_resync_shared_device_list, $policy_params);


		my @array = (\$shared_device_id, \$restart_resync_counter,
		\$should_resync,
		\$is_quasiflag, \$protected_process_server_id,
		\$source_device_name,
		\$destination_host_id,
		\$destination_device_name);
		$conn->sql_bind_columns($sql_stmt, @array);
		
		if($protection_direction == 'reverse')
		{
			$source_array_guid = $target_array_guid;
		}
		my @source_guid_alias = split(",",$source_array_guid);
		$source_array_guid = $source_guid_alias[0];

		while ($conn->sql_fetch($sql_stmt)) 
		{						
			&insert_policy_configuration($conn, $protected_process_server_id, $scenario_id, $shared_device_id,$source_array_guid,"source");
		}
	}
}


sub insert_policy_configuration
{
		my ($conn, $process_server_id, $scenario_id, $policy_params,$source_array_guid,$reference) = @_;
		
		#'1' -- Success
		#'2' -- Failed
		#'3' -- Pending
		#'4' -- Inprogress
		#'5' -- Skipped
		my ($policy_name,$policy_no);
		if ($reference eq "source") 
		{
			$policy_name = "Mirror Configuration";
			$policy_no = 19;
		}
		elsif ($reference eq "target") 
		{
			$policy_name = "Target LunMap";
			$policy_no = 20;
		}

					
		my $sql = "SELECT
						p.policyId
					FROM
						policy p,
						policyInstance pi
					WHERE
						p.policyId=pi.policyId
					AND
						p.hostId = '$process_server_id'
					AND
						p.policyType = '$policy_no'
					AND
						p.policyScheduledType = '2'
					AND
						p.policyName = '$policy_name'
					AND
						p.policyParameters = '$policy_params'";
		
		# Success, pending and in progress
		my $cond = " AND pi.policyState in ('1','2','3','4')";
		$sql = $sql.$cond;
		my $sqlStmnt = $conn->sql_query($sql);
		my $num_rows = $conn->sql_num_rows($sqlStmnt);
		$logging_obj->log("DEBUG","insert_policy_configuration $sql executed and resulted $num_rows");

		if ($conn->sql_num_rows($sqlStmnt) == 0 ) 
		{			
			my $sql = "INSERT INTO 
			   policy
			  (
			   hostId,
			   policyType,
			   policyScheduledType,
			   policyRunEvery,
			   scenarioId,
			   policyExcludeFrom,
			   policyExcludeTo,
			   policyName,
			   policyTimestamp,
			policyParameters
			  ) 
			 VALUES 
			  (
			   '$process_server_id',
			   '$policy_no',
			   '2',
			   '0',
			   '$scenario_id',
			   '0',
			   '0',
			   '$policy_name',
			   now(),
			'$policy_params'
			  )";
			my  $insert_stmt =  $conn->sql_query($sql);			


			$logging_obj->log("DEBUG","insert_policy_configuration sql3 :: $sql");	
			#my $policy_id = $conn->sql_insert_id('policy','policyId');
			my $dbh = $conn->get_database_handle();
			my $policy_id = $dbh->last_insert_id(undef, undef, "policy","policyId");
			
			$sql = "INSERT INTO
							policyInstance
							(
								policyId,
								lastUpdatedTime,
								policyState,
								executionLog
							)
						VALUES
							(
								'$policy_id',
								now(),
								'3',
								''
							)";
			$logging_obj->log("DEBUG","insert_policy_configuration sql5 :: $sql");	
			$conn->sql_query($sql);
			&update_array_lun_map_info($policy_params, $process_server_id, $source_array_guid);
		
			$logging_obj->log("DEBUG","insert_policy_configuration $policy_id");	
		}
		else
		{
			my $policy = $conn->sql_fetchrow_hash_ref($sqlStmnt);
            my $policy_id = $policy->{'policyId'};
            my $sql = "UPDATE
		          		   policy
              			SET
		                   policyTimestamp = now()
                        WHERE
                           policyId = '$policy_id' AND
        		           hostId = '$process_server_id' AND
			               policyType = '$policy_no' AND
            	           policyScheduledType = '2' AND
              			   policyRunEvery = '0' AND
                           scenarioId = '$scenario_id' AND
   			               policyExcludeFrom = '0' AND
               			   policyExcludeTo = '0' AND
                           policyName = '$policy_name' AND
  			               policyParameters = '$policy_params'";
            #my  $updt_stmt =  $conn->sql_query($sql);
            $logging_obj->log("DEBUG","insert_policy_configuration update existing policy :: $sql");
			my $check_sql = "SELECT
								policyInstanceId
							 FROM
								policyInstance
							 WHERE
								policyId = '$policy_id' AND
			                    policyState in ('3','4')";
            my $checkSth = $conn->sql_query($check_sql);
            my $pol_inst = $conn->sql_num_rows($checkSth);
            $logging_obj->log("DEBUG","insert_policy_configuration Check instances for deletemap :: $check_sql");
            $logging_obj->log("DEBUG","insert_policy_configuration check_sql returned :: $pol_inst number of instances already exist\n");
            if($pol_inst == 0)
            {
            	my $sql = "INSERT INTO
                           policyInstance
                           (
                             policyId,
                             lastUpdatedTime,
	                         policyState,
                             executionLog
                           )
                    VALUES
                           (
                            '$policy_id',
                             now(),
                             '3',
                             ''
                           )";
                        $logging_obj->log("DEBUG","insert_policy_configuration Inserted policy for deletemap :: $sql");
                        $conn->sql_query($sql);
			}
            $logging_obj->log("DEBUG","insert_policy_configuration $policy_id Done");
		}	
}

sub update_array_lun_map_info
{
	my($policy_params, $process_server_id, $source_array_guid) = @_;
	my $shared_device_id;
	$logging_obj->log("DEBUG","update_array_lun_map_info $policy_params");	

	my @policy_param_list = split(/,/, $policy_params);
	my $element;
	for $element (@policy_param_list) 
	{
		$shared_device_id  = $element;
		my $sql = "SELECT ".
				"sharedDeviceId ".
			  "FROM ".
				"arrayLunMapInfo ".
			  " WHERE ".
			  " sharedDeviceId in ('$element')";

		$logging_obj->log("DEBUG","update_array_lun_map_info sql1 :: $sql");
		my $stmt_obj = $conn->sql_query($sql);
		
		my $map_discovery_state = Fabric::Constants::MAP_DISCOVERY_PENDING;
		if ($conn->sql_num_rows($stmt_obj) == 0)
		{
		#my $index = index($policy_params,$shared_device_id)
		#if ($index == -1) 
			$sql =  "INSERT ".
					"INTO ".
					"	arrayLunMapInfo ".
					"	( ".
					"	sourceSharedDeviceId,	".
					"	sharedDeviceId, ".
					"	arrayGuid  , ".
					"	protectedProcessServerId ,".
					"	fabricId , ".
					"	state , ".
					"	lunType ".
					"	)	".
					"VALUES ".
					"	( ".
					"	'$shared_device_id', ".
					"	'$shared_device_id', ".
					"	'$source_array_guid', ".
					"	'$process_server_id', ".
					"	1,	".
					"	$map_discovery_state, ".
					"	'SOURCE' ".								
					"	)";	
		}
		else
		{
			$sql = "UPDATE ".
					"	arrayLunMapInfo ".	
					"SET ".
					"	state = $map_discovery_state,	".
					"	lunType = 'SOURCE',	".
					"	protectedProcessServerId = '$process_server_id',	".
					"	arrayGuid = '$source_array_guid',	".
					"	fabricId = 1	".
					"WHERE	".
					"	sharedDeviceId = '$shared_device_id'";
		}
		$logging_obj->log("DEBUG","update_array_lun_map_info_configuration:: $sql");	
		$conn->sql_query($sql);
	}		
}

sub process_delete_split_mirror_done
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_delete_split_mirror_done");
    
	my $initiator;
	my $srcDeviceName;
	my $sharedDeviceId;	
	my $nodeHostId;
	my $processServerId;
	
    my @par_array = (Fabric::Constants::DELETE_SPLIT_MIRROR_DONE);
	my $par_array_ref = \@par_array;
    my $sqlStmnt = $conn->sql_query(
        "select distinct ".
        " exportedInitiatorPortWwn,".
        " srcDeviceName,".
        " sharedDeviceId,".
		" hostId,".
        " processServerId ".        
		"from hostApplianceTargetLunMapping ".
        "where state = ?",$par_array_ref);
    if ($conn->sql_num_rows($sqlStmnt) == 0)
	{
        return;
    }
	
    my @array = (\$initiator, \$srcDeviceName, \$sharedDeviceId,
				 \$nodeHostId, \$processServerId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) 
	{
	
		#
        # In case of lun resize, once the mirror is deleted.
        # the lun_state flag should be changed to LUN_RESIZE_PRE_PROCESSING_DONE,
        # which eventually changed to LUN_RESIZE_RECONFIGURATION_PENDING
        # in order to re-create the AT lun with the new size.
        #
		if(&check_array_type($conn,$sharedDeviceId))
		{
			my $update_lun_info = $conn->sql_query("UPDATE ".
									" arrayLuns ".
								"SET ".
									" privateData = '', ".
									" mirrored = 'MIRRORED_BY_NONE' ".
								" WHERE ".
									" sanLunId = '$sharedDeviceId'");
			$logging_obj->log("DEBUG","process_delete_split_mirror_done :: updated arrayLuns table");

			&update_host_appliance_target_lun_mapping_delete_state(
				$conn,  Fabric::Constants::DELETE_AT_LUN_PENDING,
				$initiator, $srcDeviceName,  $sharedDeviceId, $nodeHostId, $processServerId, Fabric::Constants::DELETE_SPLIT_MIRROR_DONE);
			$logging_obj->log("DEBUG","process_delete_split_mirror_done updated to Fabric::Constants::DELETE_AT_LUN_PENDING\n");
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

    my ($processServerId,$sharedDeviceId,$destinationHostId);
	my $lun_state,$sharedDeviceId;
    my $fabricId = Fabric::Constants::PRISM_FABRIC;
	my @par_array = (Fabric::Constants::ARRAY_SOLUTION,Fabric::Constants::REBOOT_PENDING,Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING,Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING);
    my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "SELECT DISTINCT ".
        "   hatlm.processServerId, ".
        "   hatlm.sharedDeviceId, ".
        "   sldl.lun_state, ".
        "   sldl.destinationHostId ".
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
    my @array = (\$processServerId,\$sharedDeviceId,\$lun_state, \$destinationHostId);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) 
	{
        $logging_obj->log("DEBUG","process_reboot_pending::$sharedDeviceId,$processServerId,$lun_state,$destinationHostId");
		if (&appliance_fr_binding_targets_ok($conn, $processServerId, $fabricId)
            && &appliance_fr_binding_initiators_ok($conn, $processServerId, $fabricId)) 
		{
            if($lun_state == Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING)
			{
				$logging_obj->log("DEBUG","process_reboot_pending::got the Failover state.");
				my @par_array = ('TARGET',$sharedDeviceId,$processServerId,Fabric::Constants::MAP_DISCOVERY_PENDING);
				my $par_array_ref = \@par_array;
				my $sqlStmnt = $conn->sql_query(
					"SELECT DISTINCT ".
					"   sharedDeviceId ".
					"FROM ".
					"   arrayLunMapInfo  ".
					"WHERE ".
					"lunType = ? AND ".
					"sourceSharedDeviceId = ? AND ".
					"protectedProcessServerId = ? AND ".
					" state in (?)",$par_array_ref);
				if(!$conn->sql_num_rows($sqlStmnt))
				{
					$logging_obj->log("DEBUG","process_reboot_pending::arrayLumMapInfo is not in prepare target.");
					&update_reboot_host_appliance_target_lun_mappings_state($conn, $processServerId, $fabricId,$sharedDeviceId);
				}	

			}
			else
			{
				$logging_obj->log("DEBUG","process_reboot_pending::got the reboot state.");
				# pair_type 0=host,1=fabric,2=prism,3=Array
				my $pair_type = TimeShotManager::get_pair_type($conn,$sharedDeviceId); 
				$logging_obj->log("DEBUG","process_reboot_pending::$sharedDeviceId,$pair_type,$destinationHostId");
				if($pair_type == 3)
				{
					my @par_array = ('RETENTION',$destinationHostId,Fabric::Constants::MAP_DISCOVERY_PENDING);
					my $par_array_ref = \@par_array;
					my $sqlStmnt = $conn->sql_query(
						"SELECT DISTINCT ".
						"   sanLunId ".
						"FROM ".
						"   managedLuns  ".
						"WHERE ".
						"lunType = ? AND ".
						"processServerId = ? AND ".
						" state in (?)",$par_array_ref);
					if(!$conn->sql_num_rows($sqlStmnt))
					{
						$logging_obj->log("DEBUG","process_reboot_pending::managedLun is not in pending state.");
						&update_reboot_host_appliance_target_lun_mappings_state($conn, $processServerId, $fabricId,$sharedDeviceId);
					}
				}
				
			}
			# else
			# {
				# $logging_obj->log("DEBUG","process_reboot_pending::update state.");
				# &update_reboot_host_appliance_target_lun_mappings_state($conn, $processServerId, $fabricId,$sharedDeviceId);
			# }
			
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

	my $solutionType = Fabric::Constants::ARRAY_SOLUTION;
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
        "ap.planId=sd.planId AND ".
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
        $logging_obj->log("DEBUG","process_delete_path_on_lun_resize set DELETE_SPLIT_MIRROR_PENDING state in hostApplianceTargetLunMapping");
	
	&update_host_appliance_target_lun_mapping_delete_state($conn, Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING, $initiator, $srcDeviceName, $sharedDeviceId, $nodeHostId, $processServerId,  $STATES_OK_TO_SET_DELETE) ;	
												   
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
		if($pair_type == 3)
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

    # frBindingNexus.state
#    DISCOVER_BINDING_DEVICE_PENDING => 9,

    # frBindingNexus.state
 #   DISCOVER_BINDING_DEVICE_DONE => 10,

# sub process_mirror_device_done
# {
    # my ($conn) = @_;

    # $logging_obj->log("DEBUG","process_mirror_device_done");
	
	# my @par_array = (Fabric::Constants::CREATE_SPLIT_MIRROR_DONE);
    # my $par_array_ref = \@par_array;
	# my $sqlStmnt = $conn->sql_query(
        # "select distinct sharedDeviceId, 
				# hostId ".
				# "from hostApplianceTargetLunMapping hatlm, ".
				# "srcLogicalVolumeDestLogicalVolume sldlv,
				# applicationPlan ap		
			# WHERE 
				# ap.solutionType = 'ARRAY' 
			# AND 
				# ap.planId=sldlv.planId 
			# AND 	
				# hatlm.sharedDeviceId = sldlv.Phy_Lunid
			# AND
				# hatlm.processServerId = sldlv.sourceHostId ".
			# "AND state = ?",$par_array_ref);

    # my $sharedDeviceId;
    # my $processServerId;
    # my $nodeHostId;

    # my @array = (\$sharedDeviceId, \$nodeHostId);
    # $conn->sql_bind_columns($sqlStmnt,@array);
    # while ($conn->sql_fetch($sqlStmnt))
    # {
		# my @par_array = (Fabric::Constants::DISCOVER_BINDING_DEVICE_PENDING,Fabric::Constants::CREATE_SPLIT_MIRROR_DONE,$sharedDeviceId,$nodeHostId);
		# my $par_array_ref = \@par_array;
        # my $sqlStmnt = $conn->sql_query(
        # "update hostApplianceTargetLunMapping ".
        # " set state = ? ".
        # " where state = ? 
		# AND 
			# sharedDeviceId = ? 
		# AND 
			# hostId = ?",$par_array_ref);
	# }
	
	
# }


#For prism case this would change the TARGET_DEVICE_DISCOVERY_DONE.
sub process_mirror_binding_device_pending
{
	my ($conn) = @_;

    $logging_obj->log("DEBUG","process_mirror_binding_device_pending");
	
	my @par_array = (Fabric::Constants::DISCOVER_BINDING_DEVICE_PENDING);
    my $par_array_ref = \@par_array;
	my @shared_device_list;

	my $sql =	"SELECT ".
					"distinct ".
					"hatlm.sharedDeviceId ".
				"FROM ".
					"hostApplianceTargetLunMapping hatlm, ".
					"srcLogicalVolumeDestLogicalVolume slvdlv ".
				"WHERE ".
					"slvdlv.processServerId = hatlm.processServerId ".
				"AND ".
					"slvdlv.applicationType!='Array' ".
				"AND  ".
					"hatlm.sharedDeviceId=slvdlv.Phy_Lunid ".
				"AND ".
					"hatlm.state=?";

	my $sqlStmnt = $conn->sql_query($sql,$par_array_ref);
	
	my ($shared_device_id);
	my @array = (\$shared_device_id);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt))
    {
		push @shared_device_list , $shared_device_id ;
	}
	
	my $shared_device_list_count = @shared_device_list;
	if ($shared_device_list_count > 0) 
	{
		my $shared_device_data = join("','",@shared_device_list);
		$shared_device_data = "'".$shared_device_data."'";

		my $sql = "UPDAE ". 
				"hostApplianceTargetLunMapping ".
		   "SET ".
				"state = Fabric::Constants::TARGET_DEVICE_DISCOVERY_DONE ".
			"WHERE ".
				"state = Fabric::Constants::DISCOVER_BINDING_DEVICE_PENDING ".
			"AND ".
				"sharedDeviceId in ($shared_device_data)";

		my $update_stmt = $conn->sql_query($sql);
	}


}

sub process_discovery_binding_done
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_discovery_binding_done");

	my @par_array = (Fabric::Constants::TARGET_LUN_MAP_PENDING,Fabric::Constants::DISCOVER_BINDING_DEVICE_DONE);
	my $par_array_ref = \@par_array;
        my $sqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ? ".
        " where state = ?",$par_array_ref);
		&invoke_target_lun_policy($conn);
}


sub invoke_target_lun_policy
{
	my ($conn) = @_;
	$logging_obj->log("DEBUG","inside invoke_target_lun_policy \n");
	
	my $sql = 	"SELECT  
					scenarioId, 
					targetId,
					targetVolumes
				FROM
					applicationScenario
				WHERE
					executionStatus='92'
				AND
					applicationType='Array'";
	$logging_obj->log("DEBUG","invoke_target_lun_policy sql1 :: $sql");
	
    my $pairSqlStmnt = $conn->sql_query($sql);
	my ($scenario_id, $target_id, $target_volumes,$hash_key, $hash_value);
	my @array = (\$scenario_id, \$target_id, \$target_volumes);
    $conn->sql_bind_columns($pairSqlStmnt, @array);
    while ($conn->sql_fetch($pairSqlStmnt)) 
	{
	
		my @target_volumes = split(",",$target_volumes);
		my $target_volumes_count = @target_volumes;
		
		my $sql = 	"SELECT 
							distinct 
							hatlm.sharedDeviceId,
							sldlv.restartResyncCounter,
							sldlv.shouldResync,
							sldlv.isQuasiflag,
							sldlv.sourceHostId,
							sldlv.sourceDeviceName,
							sldlv.destinationHostId,
							sldlv.destinationDeviceName
						FROM
							 hostApplianceTargetLunMapping hatlm, srcLogicalVolumeDestLogicalVolume sldlv
						WHERE
							hatlm.sharedDeviceId = sldlv.Phy_Lunid
						AND
							hatlm.processServerId = sldlv.sourceHostId
						AND
							sldlv.scenarioId = ?
						AND
							hatlm.state = ?";
		
		 $logging_obj->log("DEBUG","invoke_target_lun_policy sql1 :: $sql");
		#my @par_array = ($scenario_id, Fabric::Constants::CREATE_SPLIT_MIRROR_PENDING);
		  my @par_array = ($scenario_id, Fabric::Constants::TARGET_LUN_MAP_PENDING);

		my $par_array_ref = \@par_array;
		my $sql_stmt = $conn->sql_query($sql,$par_array_ref);

		my ($shared_device_id, 
			$restart_resync_counter,
			$should_resync,
			$is_quasiflag, 
			$protected_process_server_id, 			
			$source_device_name, 
			$destination_host_id, 
			$destination_device_name,
			@first_resync_shared_device_list,
			@restart_resync_shared_device_list, $policy_params);


		my @array = (\$shared_device_id, \$restart_resync_counter,
		\$should_resync,
		\$is_quasiflag, \$protected_process_server_id,
		\$source_device_name,
		\$destination_host_id,
		\$destination_device_name);
		$conn->sql_bind_columns($sql_stmt, @array);


		while ($conn->sql_fetch($sql_stmt)) 
		{
			my @device_details =   split(/\/dev\/mapper\//, $destination_device_name);
			
			my $target_lun = $device_details[1];
			
			if ($restart_resync_counter == 0 && 
				$is_quasiflag == 0)
			{
				 push @first_resync_shared_device_list, $target_lun;
			
			}
			elsif (($restart_resync_counter > 0) && 
					($is_quasiflag == 0) && 
					($should_resync != 0))
			{
				push @restart_resync_shared_device_list , $target_lun;

				$logging_obj->log("DEBUG","invoke_target_lun_policy restart resync:: $shared_device_id \n");
			}
		}

		my $first_resync_shared_device_list_count = @first_resync_shared_device_list;

		print "counts:: $first_resync_shared_device_list_count, $target_volumes_count";
			if ($first_resync_shared_device_list_count == $target_volumes_count)
			{
				$policy_params = join(',',@first_resync_shared_device_list);
				
				my $policy_params_value = $policy_params;
				$policy_params_value =~ s/,/\',\'/g; 
		
				my %result_data = getArrayManager($conn, $policy_params_value);

				while (($hash_key, $hash_value) = each(%result_data))
				{
					my @luns = @{$hash_value};
					$policy_params = join(',',@luns);
					$protected_process_server_id = $hash_key;
					&insert_policy_configuration($conn, $protected_process_server_id, $scenario_id, $policy_params,"no","target");
				}
			}
			else
			{			
				foreach my $shared_device (@restart_resync_shared_device_list)
				{
					$policy_params = $shared_device;
					my %result_data = getArrayManager($conn, $policy_params);
					my $hash_count = scalar keys %result_data;
					if ($hash_count > 0) 
					{					
						while (($hash_key, $hash_value) = each(%result_data))
						{
							my @luns = @{$hash_value};
							$policy_params = join(',',@luns);
							$protected_process_server_id = $hash_key;
						}
						&insert_policy_configuration($conn, $protected_process_server_id, $scenario_id, $policy_params,"yes","target");
					}
				}
			}
	}
}


sub getArrayManager
{
	my ($conn, $policy_params_value) = @_;
    $logging_obj->log("DEBUG","getArrayManager \n");

	my $sql = "SELECT
						ai.agentGuid,
						al.sanLunId
					FROM
						arrayInfo ai,
						arrayLuns al
					WHERE
						ai.arrayGuid = al.arrayGuid
					AND
						al.sanLunId in ('$policy_params_value')";
				my $stmt_obj = $conn->sql_query($sql);
    $logging_obj->log("DEBUG","getArrayManager SQL :: $sql\n");
				my %result_data = ();
				while (my $ref = $conn->sql_fetchrow_hash_ref($stmt_obj))
				{	
					my $agent_guid = $ref->{agentGuid};
					push(@{$result_data{$agent_guid}}, $ref->{sanLunId});
				}
		return %result_data;

}


sub process_target_device_discovery_pending
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_target_device_discovery_pending");

	my @par_array = (Fabric::Constants::TARGET_DEVICE_DISCOVERY_PENDING,Fabric::Constants::TARGET_LUN_MAP_DONE);
	my $par_array_ref = \@par_array;
        my $sqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ? ".
        " where state = ?",$par_array_ref);
}


#This has to be called finally.
sub process_mirror_device_done
{   
    my ($conn) = @_;
    $logging_obj->log("DEBUG","process_target_device_discovery_done");
    my @par_array = (Fabric::Constants::CREATE_SPLIT_MIRROR_DONE);
    my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "select distinct sharedDeviceId, 
		hostId ".
				"from hostApplianceTargetLunMapping hatlm, ".
				"srcLogicalVolumeDestLogicalVolume sldlv,
				applicationPlan ap		
			WHERE 
				ap.solutionType = '".Fabric::Constants::ARRAY_SOLUTION."' 
			AND 
				ap.planId=sldlv.planId 
			AND 	
				hatlm.sharedDeviceId = sldlv.Phy_Lunid
			AND
				hatlm.processServerId = sldlv.sourceHostId ".
        " AND state = ?",$par_array_ref);

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
    my @par_array = (Fabric::Constants::ARRAY_SOLUTION,Fabric::Constants::PROFILER_HOST_ID,Fabric::Constants::PROTECTED,Fabric::Constants::RESYNC_NEEDED,$RESYNC_STATES);
	
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
				" ap.solutionType = 'ARRAY' AND ".	
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
				"quasidiffStarttime = 0,".
				"quasidiffEndtime = 0,".
				"resyncStartTime = now(),".
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

	my ($lun_id,$sourceHostId);	
	my $sqlSlDl = $conn->sql_query("SELECT DISTINCT Phy_Lunid,sourceHostId ".
	"from srcLogicalVolumeDestLogicalVolume ");
	my @array = (\$lun_id, \$sourceHostId);
	$conn->sql_bind_columns($sqlSlDl,@array);
	
	 while ($conn->sql_fetch($sqlSlDl))
	 {	
		my $pair_type = TimeShotManager::get_pair_type($conn,$lun_id); # pair_type 0=host,1=fabric,2=prism
		my $write_log = 0;
		my $shouldResync = Array::CommonProcessor::get_should_resync_value($conn, $lun_id, $sourceHostId,$write_log);
		$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover($lun_id,$pair_type,$sourceHostId)");
		if($pair_type == 3)
		{
			$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover");     
			my $hostName;
			my $hostIp;
			my $sourceDeviceName;
			my $scenario_id;
			my $from_process_server_info;
			my $to_process_server_info;
			my $hostGuId;
			my $oldHostId;
			my ($destinationHostId,$destinationDeviceName,$oldTgtHostId,$tgtHostGuId);
			my ($tgtHostName,$tgtHostIp,$to_tgt_process_server_info,$from_tgt_process_server_info);
			
			#
			# writing into the sentinle.txt for target failover event
			#
			my @par_array = (Fabric::Constants::FAILOVER_TARGET_RECONFIGURATION_PENDING,'TARGET', Fabric::Constants::MAP_DISCOVERY_DONE);
			my $par_array_ref = \@par_array;
			
			my $sqlUpdateSlDlTgt = $conn->sql_query(
			"SELECT distinct sldl.Phy_Lunid,sldl.destinationHostId, sldl.destinationDeviceName ".
			"from srcLogicalVolumeDestLogicalVolume sldl, arrayLunMapInfo almi, ".
			"logicalVolumes lv WHERE ".
			"sldl.lun_state = ? AND almi.lunType = ? ".
			"AND almi.sourceSharedDeviceId = sldl.Phy_Lunid ".
			"AND sldl.destinationDeviceName = lv.deviceName ".
			"AND lv.Phy_Lunid = almi.sharedDeviceId ".
			"AND almi.state = ?",$par_array_ref);

			 my @array = (\$lun_id, \$destinationHostId ,\$destinationDeviceName);
			 $conn->sql_bind_columns($sqlUpdateSlDlTgt,@array);
			my $numCountTgt = $conn->sql_num_rows($sqlUpdateSlDlTgt);	
			$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover::tgt $numCountTgt==".Fabric::Constants::FAILOVER_TARGET_RECONFIGURATION_PENDING); 
			while ($conn->sql_fetch($sqlUpdateSlDlTgt)) 
			{	
				@par_array = ($destinationHostId,$destinationHostId);
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
				
				my @array = (\$oldTgtHostId);
				$conn->sql_bind_columns($sqlSelectHost,@array);
                $conn->sql_fetch($sqlSelectHost);
				$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover tgt Host Info ::$oldTgtHostId");

				@par_array = ($destinationHostId ,$oldTgtHostId);
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
			
				my @array = (\$tgtHostGuId, \$tgtHostName, \$tgtHostIp);
				$conn->sql_bind_columns($sqlSelectHost,@array);

				$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover tgt Host Info  $tgtHostName $tgtHostIp");
				
				while ($conn->sql_fetch($sqlSelectHost)) 
				{
					if($tgtHostGuId eq $destinationHostId)
					{
						$to_tgt_process_server_info = $tgtHostName."[IP".$tgtHostIp."]";
					}
					else
					{
						$from_tgt_process_server_info = $tgtHostName."[IP".$tgtHostIp."]";
					}
					
					$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover tgt Host Info  $tgtHostGuId");
				}
				
				my $agentLogString = "Target Failover event occurred from Process server $from_tgt_process_server_info to $to_tgt_process_server_info";
				Utilities::update_agent_log($destinationHostId, $destinationDeviceName, $WEB_ROOT, $agentLogString);
			}
			
			#
			# update srcLogicalVolumeDestLogicalVolume to prtocted state
			# if only target side failover happened and prepare target is succeed.
			#

			my @par_array = (Fabric::Constants::PROTECTED,$shouldResync, 
			 Fabric::Constants::FAILOVER_TARGET_RECONFIGURATION_PENDING,
			 'TARGET', Fabric::Constants::MAP_DISCOVERY_DONE);
				
			my $par_array_ref = \@par_array;
			my $sqlUpdateSlDlTarget = $conn->sql_query(
					"UPDATE ".
					"   srcLogicalVolumeDestLogicalVolume sldl, arrayLunMapInfo almi,  logicalVolumes lv ".
					"SET ".
					"  sldl.lun_state = ?, ".
					"  sldl.shouldResync = ? ".
					"WHERE ".
					" sldl.lun_state = ? AND almi.lunType = ? ".
					" AND almi.sourceSharedDeviceId = sldl.Phy_Lunid ".
					" AND sldl.destinationDeviceName = lv.deviceName ".
					" AND lv.Phy_Lunid = almi.sharedDeviceId ".
					" AND almi.state = ? ",$par_array_ref);
			$logging_obj->log("DEBUG","process_change_db_state_flag_on_failover::Target Failover,$sqlUpdateSlDlTarget");
			
			my @par_array = (Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_PENDING, 
							 Fabric::Constants::FAILOVER_PRE_PROCESSING_DONE,$lun_id);
			my $par_array_ref = \@par_array;
			my $sqlUpdateSlDlOne = $conn->sql_query(
					"UPDATE ".
					"   srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  lun_state = ? ".
					"WHERE ".
					" lun_state = ? AND Phy_Lunid =? ",$par_array_ref);
			
			@par_array = (Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_PENDING, 
					  Fabric::Constants::FAILOVER_DRIVER_DATA_CLEANUP_DONE,$lun_id);
				
			$par_array_ref = \@par_array;
			my $sqlUpdateSlDlTwo = $conn->sql_query(
					"UPDATE ".
					"   srcLogicalVolumeDestLogicalVolume ".
					"SET ".
					"  lun_state = ? ".
					"WHERE ".
					" lun_state = ? AND Phy_Lunid =? ",$par_array_ref);
		  
			 my $hostId;
			 @par_array = (Fabric::Constants::FAILOVER_CX_FILE_CLEANUP_DONE,$lun_id);
			 $par_array_ref = \@par_array;
			 my $sqlUpdateSlDlThree = $conn->sql_query(
					"SELECT DISTINCT sourceHostId, Phy_Lunid, sourceDeviceName ".
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
		if($pair_type == 3)
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
		if($pair_type == 3)
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

sub process_delete_split_mirror_pending
{
	my ($conn) = @_;
	$logging_obj->log("DEBUG","inside process_delete_split_mirror_pending \n");
	
	my $sql = 	"SELECT  
					appsc.scenarioId,                                         
					appsc.sourceId,                                         
					appsc.sourceVolumes,                                 
					appsc.sourceArrayGuid
				FROM
					applicationScenario appsc, 
					applicationPlan ap 
				WHERE					                                        
					appsc.applicationType='BULK' 
				AND 
					ap.solutionType= '".Fabric::Constants::ARRAY_SOLUTION."' 
				AND 
					ap.planId=appsc.planId";
	$logging_obj->log("DEBUG","process_delete_split_mirror_pending sql for getting the plan deletion request :: $sql");
	
    my $pairSqlStmnt = $conn->sql_query($sql);
	my ($scenario_id, $source_id, $source_volumes, $source_array_guid);
	my @array = (\$scenario_id, \$source_id, \$source_volumes, \$source_array_guid);
    $conn->sql_bind_columns($pairSqlStmnt, @array);
    while ($conn->sql_fetch($pairSqlStmnt)) 
	{
	
		my @source_volumes = split(",",$source_volumes);
		my $source_volumes_count = @source_volumes;
		
		my $sql = 	"SELECT 
							distinct 
							hatlm.sharedDeviceId,
							sldlv.isQuasiflag,
							sldlv.sourceHostId
						FROM
							 hostApplianceTargetLunMapping hatlm, srcLogicalVolumeDestLogicalVolume sldlv
						WHERE
							hatlm.sharedDeviceId = sldlv.Phy_Lunid
						AND
							hatlm.processServerId = sldlv.sourceHostId
						AND
							sldlv.scenarioId = ?
						AND
							hatlm.state = ?";
		
		$logging_obj->log("DEBUG","process_delete_split_mirror_pending get the hatlm details for the state :: $sql");
		my @par_array = ($scenario_id, Fabric::Constants::DELETE_SPLIT_MIRROR_PENDING);
		my $par_array_ref = \@par_array;
		my $sql_stmt = $conn->sql_query($sql,$par_array_ref);

		my ($shared_device_id, $is_quasiflag, $porcess_server_id, @shared_device_list, $policy_params);
		my @array = (\$shared_device_id, \$is_quasiflag, \$porcess_server_id);
		
		$conn->sql_bind_columns($sql_stmt, @array);
		while ($conn->sql_fetch($sql_stmt)) 
		{
			$policy_params = $shared_device_id;
			$logging_obj->log("DEBUG","process_delete_split_mirror_pending inserting policy and policyInstance");
			&insert_policy_configuration($conn, $porcess_server_id, $scenario_id, $policy_params,$source_array_guid,"source");			
		}
		# while ($conn->sql_fetch($sql_stmt)) 
		# {
			# push @shared_device_list, $shared_device_id;			
		# }
		# foreach my $shared_device (@shared_device_list)
		# {
				# $policy_params = $shared_device;
				# $logging_obj->log("DEBUG","process_delete_split_mirror_pending inserting policy and policyInstance");
				# &insert_policy_configuration($conn, $source_id, $scenario_id, $policy_params,$source_array_guid,"source");				
		# }
	}
}

sub invoke_delete_lun_map
{
	my ($conn) = @_;
	$logging_obj->log("DEBUG","inside invoke_delete_lun_map \n");
	
	my $sql = 	"SELECT  
					appsc.scenarioId,                                         
					appsc.targetId,                                         
					appsc.targetVolumes,                                 
					appsc.sourceArrayGuid
				FROM
					applicationScenario appsc, 
					applicationPlan ap 
				WHERE
					appsc.applicationType='BULK' 
				AND 
					ap.solutionType= '".Fabric::Constants::ARRAY_SOLUTION."' 
				AND 
					ap.planId=appsc.planId";
	$logging_obj->log("DEBUG","process_delete_split_mirror_pending sql for getting the plan deletion request :: $sql");
	
    my $pairSqlStmnt = $conn->sql_query($sql);
	my ($scenario_id, $source_id, $source_volumes, $source_array_guid);
	my @array = (\$scenario_id, \$source_id, \$source_volumes, \$source_array_guid);
    $conn->sql_bind_columns($pairSqlStmnt, @array);
    while ($conn->sql_fetch($pairSqlStmnt)) 
	{
	
		my @source_volumes = split(",",$source_volumes);
		my $source_volumes_count = @source_volumes;
		
		my $sql = 	"SELECT 
							distinct 
							alm.sharedDeviceId,
							sldlv.isQuasiflag,
							sldlv.destinationHostId
						FROM
							 hostApplianceTargetLunMapping hatlm, srcLogicalVolumeDestLogicalVolume sldlv, arrayLunMapInfo alm
						WHERE
							sldlv.destinationDeviceName = concat('/dev/mapper/',alm.sharedDeviceId)
						AND
							hatlm.sharedDeviceId = sldlv.Phy_LunId
						AND
							hatlm.processServerId = sldlv.sourceHostId
						AND
							sldlv.scenarioId = ?
						AND
							hatlm.state = ?";
		
		$logging_obj->log("DEBUG","invoke_delete_lun_map get the hatlm details for the state :: $sql");
		my @par_array = ($scenario_id, Fabric::Constants::RELEASE_DRIVE_ON_TARGET_DELETE_PENDING);
		my $par_array_ref = \@par_array;
		my $sql_stmt = $conn->sql_query($sql,$par_array_ref);

		my ($shared_device_id, $is_quasiflag, $porcess_server_id, @shared_device_list, $policy_params);
		my @array = (\$shared_device_id, \$is_quasiflag, \$porcess_server_id);
		
		$conn->sql_bind_columns($sql_stmt, @array);
		while ($conn->sql_fetch($sql_stmt)) 
		{
			$policy_params = $shared_device_id;
			$logging_obj->log("DEBUG","invoke_delete_lun_map inserting policy and policyInstance");
			my @params = (0,$policy_params);
			my $serialized_policy_params = PHP::Serialization::serialize(\@params);
			$logging_obj->log("DEBUG","invoke_delete_lun_map policy param = $policy_params serialized = $serialized_policy_params\n");
			&insert_policy_configuration($conn, $porcess_server_id, $scenario_id, $serialized_policy_params,$source_array_guid,"target");			
		}
		
		# while ($conn->sql_fetch($sql_stmt)) 
		# {
			# push @shared_device_list, $shared_device_id;			
		# }
		# foreach my $shared_device (@shared_device_list)
		# {
				# $policy_params = $shared_device;
				# $logging_obj->log("DEBUG","invoke_delete_lun_map inserting policy and policyInstance");
				# my @params = (0,$policy_params);
                # my $serialized_policy_params = PHP::Serialization::serialize(\@params);
				# $logging_obj->log("DEBUG","invoke_delete_lun_map policy param = $policy_params serialized = $serialized_policy_params\n");
				# &insert_policy_configuration($conn, $source_id, $scenario_id, $serialized_policy_params,$source_array_guid,"target");				
		# }
	}
}

sub check_array_type
{
	my ($conn,$sh_dev_id) = @_;
	$logging_obj->log("DEBUG","check_array_type :: came inside");
	my $sql = "	SELECT  
					ap.solutionType,                                         
					appsc.applicationType,                                         
					ap.planType     
				FROM
					applicationScenario appsc, 
					applicationPlan ap,
					srcLogicalVolumeDestLogicalVolume slvdlv
				WHERE
					slvdlv.planId = appsc.planId 
				AND
					slvdlv.planId = ap.planId
				AND
					slvdlv.scenarioId = appsc.scenarioId
				AND
					slvdlv.Phy_Lunid = ?";
	my @par_array = ($sh_dev_id);
	my $par_array_ref = \@par_array;
	$logging_obj->log("DEBUG","check_array_type :: SQL :: $sql \nValues passed are $sh_dev_id, only\n");
	my $sql_stmt = $conn->sql_query($sql,$par_array_ref);
	my ($sol_type, $app_type, $appl_type);
	my @array = (\$sol_type, \$app_type, \$appl_type);
	$conn->sql_bind_columns($sql_stmt, @array);
	while ($conn->sql_fetch($sql_stmt)) 
	{
		if($sol_type eq Fabric::Constants::ARRAY_SOLUTION) #&& $app_type == "BULK" && $appl_type = "BULK")
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
}
sub pair_process_requests
{  
	my %args;
	$logging_obj->log("DEBUG","array Process request");
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

        $conn->set_auto_commit(0,1);
        
	#print $conn;
	
		if(!$conn->sql_ping_test())
		{
			&process_src_logical_volume_dest_logical_volume_requests($conn);
			#$conn->sql_commit();
			
			&process_delete_split_mirror_pending($conn);
			#$conn->sql_commit();
			
			&process_host_shared_device_mapping_requests($conn);
			#$conn->sql_commit();

			&process_delete_split_mirror_done($conn);
			#$conn->sql_commit();
	
			&process_create_at_lun_done($conn);
			#$conn->sql_commit();

			&process_delete_at_lun_done($conn);
	       # $conn->sql_commit();
			
			&process_reset_ai_at_error_state_in_src_logical_volume_dest_logical_volume($conn);
           # $conn->sql_commit();
		
			&process_mirror_device_done($conn); 
		    #$conn->sql_commit();
			
			#&process_mirror_binding_device_pending($conn);
			#	$conn->sql_commit();

			#&process_discovery_binding_done($conn);
			#$conn->sql_commit();

			#&process_target_device_discovery_pending($conn);
			#$conn->sql_commit();

			#&process_target_device_discovery_done($conn);
			#$conn->sql_commit();
				
			&process_resync_cleared_request($conn);
			#$conn->sql_commit();		

			&start_resync($conn);
			#$conn->sql_commit();

			&process_change_db_state_flag_on_failover($conn);
		      #  $conn->sql_commit();
	        
			&process_driver_cleanup_on_failover($conn);
			#$conn->sql_commit();
	        
			&process_cx_cleanup_on_failover($conn);
			#$conn->sql_commit();
			
			&process_reboot_pending($conn);
			
			#$conn->sql_commit();
			
			&process_reboot_failures($conn);
			#$conn->sql_commit();
			
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

sub invoke_mirror_monitor_policy
{
	my ($conn) = @_;
	$logging_obj->log("DEBUG","inside invoke_mirror_monitor_policy \n");
	
	my $sql = 	"SELECT  
					appsc.scenarioId,                                         
					appsc.sourceId,                                         
					appsc.sourceVolumes,                                 
					appsc.sourceArrayGuid
				FROM
					applicationScenario appsc, 
					applicationPlan ap 
				WHERE
					appsc.executionStatus='92'
				AND                                         
					appsc.applicationType='BULK' 
				AND 
					ap.solutionType= '".Fabric::Constants::ARRAY_SOLUTION."' 
				AND 
					ap.planId=appsc.planId";

	$logging_obj->log("DEBUG","invoke_mirror_monitor_policy :: sql_A : $sql \n");
	
    my $pairSqlStmnt = $conn->sql_query($sql);
	my ($scenario_id, $process_server_id, $source_volumes,$source_array_guid,$hash_key, $hash_value);
	my @array = (\$scenario_id, \$process_server_id, \$source_volumes,\$source_array_guid);
    $conn->sql_bind_columns($pairSqlStmnt, @array);
    while ($conn->sql_fetch($pairSqlStmnt)) 
	{
	
		my @source_volumes = split(",",$source_volumes);
		my $source_volumes_count = @source_volumes;
		
		my $sql = 	"SELECT 
							distinct 
							hatlm.sharedDeviceId,
							sldlv.restartResyncCounter,
							sldlv.shouldResync,
							sldlv.isQuasiflag,
							sldlv.sourceHostId,
							sldlv.sourceDeviceName,
							sldlv.destinationHostId,
							sldlv.destinationDeviceName
						FROM
							 hostApplianceTargetLunMapping hatlm, srcLogicalVolumeDestLogicalVolume sldlv
						WHERE
							hatlm.sharedDeviceId = sldlv.Phy_Lunid
						AND
							hatlm.processServerId = sldlv.sourceHostId
						AND
							sldlv.scenarioId = ?
						AND
							(hatlm.state = ?)";
		
		$logging_obj->log("DEBUG","invoke_mirror_monitor_policy :: sql_B : $sql");
		$logging_obj->log("DEBUG","invoke_mirror_monitor_policy :: scenario_id : $scenario_id");
		$logging_obj->log("DEBUG","invoke_mirror_monitor_policy :: state :".Fabric::Constants::PROTECTED);
		
		my @par_array = ($scenario_id, Fabric::Constants::PROTECTED);

		my $par_array_ref = \@par_array;
		my $sql_stmt = $conn->sql_query($sql,$par_array_ref);

		my ($shared_device_id, 
			$restart_resync_counter,
			$should_resync,
			$is_quasiflag, 
			$protected_process_server_id, 			$source_device_name, 
			$destination_host_id, 
			$destination_device_name,
			@first_resync_shared_device_list,
			@restart_resync_shared_device_list, $policy_params);


		my @array = (\$shared_device_id, \$restart_resync_counter,
		\$should_resync,
		\$is_quasiflag, \$protected_process_server_id,
		\$source_device_name,
		\$destination_host_id,
		\$destination_device_name);
		$conn->sql_bind_columns($sql_stmt, @array);


		while ($conn->sql_fetch($sql_stmt)) 
		{
			$logging_obj->log("DEBUG","invoke_mirror_monitor_policy :: PUSH_shared_device_id : $shared_device_id ");
			push @first_resync_shared_device_list, $shared_device_id;
		}
		$logging_obj->log("DEBUG","invoke_mirror_monitor_policy :: first_resync_shared_device_list :: ".Dumper(\@first_resync_shared_device_list));
		my $first_resync_shared_device_list_count = @first_resync_shared_device_list;

		$logging_obj->log("DEBUG","invoke_mirror_monitor_policy :: first_resync_shared_device_list_count : $first_resync_shared_device_list_count :: source_volumes_count : $source_volumes_count ");
		
		my @source_guid_alias = split(",",$source_array_guid);
		$source_array_guid = $source_guid_alias[0];
		if ($first_resync_shared_device_list_count == $source_volumes_count)
		{
			$policy_params = join(',',@first_resync_shared_device_list);
			$logging_obj->log("DEBUG","invoke_mirror_monitor_policy :: policy_params : $policy_params ");
			my $policy_params_value = $policy_params;
			   $policy_params_value =~ s/,/\',\'/g;
			
			#&insert_policy_configuration($conn, $process_server_id, $scenario_id, $policy_params,$source_array_guid,"no","source");
			&insert_run_every_policy_for_mirror_monitor($conn, $process_server_id, $scenario_id, $policy_params,$source_array_guid,"no","source");
		}

	}
}

sub insert_run_every_policy_for_mirror_monitor
{	
	my ($conn, $process_server_id, $scenario_id, $policy_params,$source_array_guid,$is_restart_resync, $reference) = @_;
	$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor called in ");
	$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: process_server_id : $process_server_id ");
	$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: scenario_id       : $scenario_id ");
	$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: policy_params     : $policy_params ");
	$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: source_array_guid : $source_array_guid ");
	$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: is_restart_resync : $is_restart_resync ");
	$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: reference         : $reference ");
	
	#'1' -- Success
	#'3' -- Pending
	#'4' -- Inprogress
	#'5' -- Skipped
	my ($policy_name,$policy_no);
	if ($reference eq "source") 
	{
		$policy_name = "MirrorMonitor";
		$policy_no = 41;
	}
	
	### 
	#   Check if Mirror Configuration (policyType=19) succeeded. 
	#   If its succeeded , then only insert MirrorMonitor (policyType=41) policy.
	###
	my $sql_A = "SELECT 
				   pi.policyState
			   FROM
				   policy p,
				   policyInstance pi
			   WHERE
				   p.policyId=pi.policyId AND
				   p.policyType = '19' AND
				   p.scenarioId = '$scenario_id' AND
				   pi.policyState=1";
				   
	my $sqlStmnt_A = $conn->sql_query($sql_A);
	my $num_rows_A = $conn->sql_num_rows($sqlStmnt_A);
	$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: num_rows_A         : $num_rows_A ");
	if ( $num_rows_A > 0 )
	{
		my $sql = "SELECT
					   p.policyId
				   FROM
					   policy p
				   WHERE
					   p.hostId = '$process_server_id' AND
					   p.policyType = '$policy_no' AND
					   p.policyScheduledType = '1' AND
					   p.scenarioId = '$scenario_id' AND
					   p.policyName = '$policy_name' AND
					   p.policyParameters = '$policy_params'";

		#my $cond = "";
		# Success, pending and in progress
		#if ($is_restart_resync eq "no") 
		#{
			#$cond = " AND pi.policyState in ('1','3','4')";
		#}
		# Failed, pending and in progress
		#elsif ($is_restart_resync eq "yes") 
		#{
		#	$cond = " AND pi.policyState in ('2','3','4')";
		#}

		#$sql = $sql.$cond;

		my $sqlStmnt = $conn->sql_query($sql);
		my $num_rows = $conn->sql_num_rows($sqlStmnt);
		$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitoring :: sql_A : $sql :: num_rows : $num_rows");

		if ($conn->sql_num_rows($sqlStmnt) == 0 ) 
		{		
			my $sql = "INSERT INTO 
						   policy
						  (
						   hostId,
						   policyType,
						   policyScheduledType,
						   policyRunEvery,
						   scenarioId,
						   policyExcludeFrom,
						   policyExcludeTo,
						   policyName,
						   policyTimestamp,
							policyParameters
						  ) 
						 VALUES 
						  (
						   '$process_server_id',
						   '$policy_no',
						   '1',
						   '600',
						   '$scenario_id',
						   '0',
						   '0',
						   '$policy_name',
						   now(),
						   '$policy_params'
						  )";
			my  $insert_stmt =  $conn->sql_query($sql);	

			$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: sql_B : $sql");	
			
			my $dbh = $conn->get_database_handle();
			my $policy_id = $dbh->last_insert_id(undef, undef, "policy","policyId");
			
			$sql = "INSERT INTO
							policyInstance
							(
								policyId,
								lastUpdatedTime,
								policyState,
								executionLog
							)
						VALUES
							(
								'$policy_id',
								now(),
								'3',
								''
							)";
			$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: sql_C : $sql");	
			$conn->sql_query($sql);
			$logging_obj->log("DEBUG","insert_run_every_policy_for_mirror_monitor :: policy_id : $policy_id");	
		}
	}
}


1;
