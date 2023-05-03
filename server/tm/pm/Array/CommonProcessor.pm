# File       : CommonProcessor.pm
#
# Description: this file will handle common 
# process method which used by Prism and Array service.

package Array::CommonProcessor;

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
use Messenger;
use Common::Log;
use Common::DB::Connection;
use Data::Dumper;
use Prism::PrismProtector;
use PHP::Serialization qw(serialize unserialize);

our $WEB_ROOT;
my $logging_obj = new Common::Log();

my $MYSQL_ER_DUP_KEY = 1002;
my $MYSQL_ER_DUP_ENTRY = 1062;
my $MYSQL_ER_DUP_UNIQUE = 1169;
my $MYSQL_ER_ROW_IS_REFERENCED = 1217;
my $MYSQL_ER_ROW_IS_REFERENCED_2 = 1451;

use constant
{
	ITL_PROTECTOR_UPDATE_ONLY => 1,
	ITL_PROTECTOR_ADD => 2,
	ITL_PROTECTOR_SKIP => 3
};

sub new
{
    my ($class, %args) = @_;
    my $self = {};

    bless ($self, $class);
    return $self;
}

sub process_src_logical_volume_dest_logical_volume_start_protection_requests
{
	my ($conn,$solutionType,$scsiId) = @_;
	$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_start_protection_requests::$conn,$solutionType,$scsiId");
	
	my @par_array = (
					Fabric::Constants::START_PROTECTION_PENDING,
					Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING,
					Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING,
					Fabric::Constants::REBOOT_PENDING,
					0,
					$scsiId
				  );
	$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_start_protection_requests::@par_array");			  
				  
	my $fabricId = Fabric::Constants::PRISM_FABRIC;			  
				  
	my $processServerId;
	my $state;
	my $sharedDeviceId;
	my $initiator;
	my $srcDeviceName;
	my $nodeHostId;

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
						" hostSharedDeviceMapping hsdm ".
						" WHERE ".
						" lun_state in (?,?,?,?) ".
						" AND sd.Phy_Lunid = hsdm.sharedDeviceId ".
						" AND sd.replication_status = ? ".
						" AND sd.Phy_Lunid = ? ".
						" ORDER BY sharedDeviceId", $par_array_ref);
	my @array = (\$processServerId, \$state, \$sharedDeviceId, \$initiator, \$srcDeviceName, \$nodeHostId);
	$conn->sql_bind_columns($sqlStmnt,@array);
	
	print "$processServerId,  $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId";	
	$logging_obj->log("DEBUG","process_src_logical_volume_dest_logical_volume_start_protection_requests::$processServerId,  $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId");

	while($conn->sql_fetch($sqlStmnt))
	{
		&protect_device($conn, $processServerId, $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId);
	
	}
}

sub protect_device
{
	my ($conn, $processServerId, $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId) = @_;
	$logging_obj->log("DEBUG","protect_device:: $processServerId, $sharedDeviceId, $state, $initiator, $srcDeviceName, $nodeHostId, $fabricId");
	my $protectAction = &get_one_to_n_protection_action($conn, $sharedDeviceId);
	my $pathExists = &device_exists_in_host_appliance_target_lun_mapping($conn, $processServerId, $sharedDeviceId, $initiator, $srcDeviceName);
	
	if (ITL_PROTECTOR_UPDATE_ONLY == $protectAction && $pathExists) 
	{
        &finish_start_protection($conn, $sharedDeviceId, $processServerId, $nodeHostId, Fabric::Constants::NO_WORK);
   	}
	elsif (ITL_PROTECTOR_SKIP != $protectAction) 
	{
        if (!$pathExists) 
		{
            # we could limit the number of times get_appliance_initiator_infos and get_appliance_target_info
            # are called by tracking fabricId, processServerId and lunId, but since setting up jobs doesn't
            # happen that often, we'll keep things simple and always get them
            my @applianceInitiatorInfos;
            &get_appliance_initiator_infos($conn, $sharedDeviceId, $fabricId, $processServerId, \@applianceInitiatorInfos);

            my @applianceTargetInfos;
            &get_appliance_target_infos($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId, \@applianceTargetInfos);

            &process_start_protection(
                $conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
                $processServerId, \@applianceInitiatorInfos, \@applianceTargetInfos);
        }
    }
}

sub process_start_protection
{
    my ($conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
        $processServerId, $applianceInitiators, $applianceTargets) = @_;

	my $syncDeviceName;	
	my ($tgtSharedDeviceId, $tgtProcessServerId, $tgtState);
    $logging_obj->log("DEBUG","process_start_protection($initiator, $sharedDeviceId, $fabricId, $processServerId, $applianceInitiators, $applianceTargets)");
	
	
	my $pair_type = TimeShotManager::get_pair_type($conn,$sharedDeviceId); # pair_type 0=host,1=fabric,2=prism,3=array
	$logging_obj->log("DEBUG","process_start_protection::pairType:$pair_type");
	
	if($pair_type == 2)
	{
		$syncDeviceName = &get_sync_device_name($conn, $sharedDeviceId, $processServerId);
		if(!$syncDeviceName)
		{
			die "Sync device not avaiable for sharedDevice=$sharedDeviceId and processServerId=$processServerId";
		}
		&insert_pair_info(
                $conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
                $processServerId, $applianceInitiators, $applianceTargets, $syncDeviceName);
		
	}
	elsif ($pair_type == 3)
	{
	
		my $syncDeviceName = Fabric::Constants::INITIAL_LUN_DEVICE_NAME.$sharedDeviceId; 
		
		$logging_obj->log("DEBUG","process_start_protection::checking prepate target policy");
		my @par_array = (
						Fabric::Constants::START_PROTECTION_PENDING,
						Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING,
						Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING,
						Fabric::Constants::REBOOT_PENDING,
						'TARGET',$sharedDeviceId
					  );
		my $par_array_ref = \@par_array;			  
		my $sqlStmnt = $conn->sql_query(
							"SELECT DISTINCT ".
							" almi.sharedDeviceId, ".
							" almi.protectedProcessServerId, ".
							" almi.state ".
							" FROM ".
							" srcLogicalVolumeDestLogicalVolume sd, ".
							" logicalVolumes lv, ".
							" arrayLunMapInfo almi ".
							" WHERE ".
							" sd.lun_state in (?,?,?,?) ".
							" AND sd.destinationDeviceName = lv.deviceName ".
							" AND (lv.Phy_Lunid = almi.sharedDeviceId AND almi.lunType = ? )".
							" AND sd.Phy_Lunid = ? ".
							" ORDER BY sharedDeviceId", $par_array_ref);
		my @array = (\$tgtSharedDeviceId, \$tgtProcessServerId, \$tgtState);
		
		if($conn->sql_num_rows($sqlStmnt) > 0)
		{
			$conn->sql_bind_columns($sqlStmnt,@array);
			while($conn->sql_fetch($sqlStmnt))
			{
				$logging_obj->log("DEBUG","process_start_protection-target info::$tgtSharedDeviceId,  $tgtProcessServerId, $tgtState");
				
				if($tgtState == Fabric::Constants::MAP_DISCOVERY_DONE)
				{
					$logging_obj->log("DEBUG","process_start_protection::arrayLumMapInfo is not in prepare target.");
					$logging_obj->log("DEBUG","process_start_protection::: $syncDeviceName");	
					
					&insert_pair_info(
                $conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
                $processServerId, $applianceInitiators, $applianceTargets, $syncDeviceName);
					
					
				}
			}
		}
		else
		{
			my $tgtDeviceName;
			my @par_array = (
						Fabric::Constants::START_PROTECTION_PENDING,
						Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING,
						Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING,
						Fabric::Constants::REBOOT_PENDING,$sharedDeviceId
					  );
		my $par_array_ref = \@par_array;			  
		my $sqlStmnt = $conn->sql_query(
							"SELECT ".
							" destinationDeviceName  ".
							" FROM ".
							" srcLogicalVolumeDestLogicalVolume sd ".
							" WHERE ".
							" sd.lun_state in (?,?,?,?) ".
							" AND sd.Phy_Lunid = ? ", $par_array_ref);
			my @array = (\$tgtDeviceName);
			$conn->sql_bind_columns($sqlStmnt,@array);
			while($conn->sql_fetch($sqlStmnt))
			{
				$logging_obj->log("DEBUG","process_start_protection- profiler block, tgtDeviceName::$tgtDeviceName");
				if($tgtDeviceName eq Fabric::Constants::PROFILER_DEVICE_NAME)
				{
					&insert_pair_info(
                $conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
                $processServerId, $applianceInitiators, $applianceTargets, $syncDeviceName);	
				
				}	
			
			}	
			
		}
		
	}
	
	
}


sub insert_pair_info
{

	my ($conn, $initiator, $sharedDeviceId, $srcDeviceName, $nodeHostId, $fabricId,
			$processServerId, $applianceInitiators, $applianceTargets, $syncDeviceName) = @_;
	
	$logging_obj->log("DEBUG","insert_pair_info($initiator, $sharedDeviceId, $fabricId, $processServerId, $applianceInitiators, $applianceTargets, $syncDeviceName, $srcDeviceName, $nodeHostId)");	
	
	foreach my $aiInfo (@{$applianceInitiators}) 
	{
	   foreach my $atInfo (@{$applianceTargets}) 
		{
		
			my @par_array = ($nodeHostId, $sharedDeviceId, $atInfo->{lunId}, $atInfo->{lunNumber}, 
							  $initiator, $atInfo->{acgId}, $processServerId, $aiInfo, $atInfo->{portWwn},  
							 $fabricId, $srcDeviceName, $syncDeviceName);
			my $par_array_ref = \@par_array;

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
			" syncDeviceName = ? ",$par_array_ref);
			$logging_obj->log("DEBUG","insert_pair_info - (checking entry existance)");
			if ($conn->sql_num_rows($sqlSelectStmnt) == 0)
			{
			
				my @par_array = ($nodeHostId, $sharedDeviceId, $atInfo->{lunId}, $atInfo->{lunNumber}, 
								  $initiator, $atInfo->{acgId}, $processServerId, $aiInfo, $atInfo->{portWwn},  
								 $fabricId, $srcDeviceName, $syncDeviceName, Fabric::Constants::CREATE_AT_LUN_PENDING);
				
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
				
					 $logging_obj->log("DEBUG","insert_pair_info-1");

					if (!$sqlStmnt
						&& !($conn->sql_err() == $MYSQL_ER_DUP_KEY
							 || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
							 || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) 
					{
								$logging_obj->log("EXCEPTION","ERROR - insert_pair_info ".$conn->sql_error());
								die $conn->sql_error();
					}
			}	
		}
	}
}

sub get_sync_device_name
{
	my($conn, $sharedDeviceId, $processServerId) = @_;
	my $syncDeviceName = Fabric::Constants::INITIAL_LUN_DEVICE_NAME.$sharedDeviceId;
	$logging_obj->log("DEBUG","get_sync_device_name::  $sharedDeviceId, $processServerId, $syncDeviceName");
	
	my @par_array = ($sharedDeviceId, $processServerId, $syncDeviceName);
	my $par_array_ref = \@par_array;

	my $sql = "select ".
        " deviceName ".
        "from ".
        " logicalVolumes ".
        "where ".
        " hostId = '$processServerId' and ".
        " Phy_Lunid = '$sharedDeviceId' and ".
		" deviceName = '$syncDeviceName'";
	$logging_obj->log("DEBUG","get_sync_device_name syncDeviceName2222:: $sql");	
	
	my $sqlStmnt = $conn->sql_query($sql);
	
	$logging_obj->log("DEBUG","get_sync_device_name syncDeviceName111::". $conn->sql_num_rows($sqlStmnt));	
	if ($conn->sql_num_rows($sqlStmnt) > 0) 
	{
		$logging_obj->log("DEBUG","get_sync_device_name syncDeviceName::  $syncDeviceName");
		return $syncDeviceName;
	}

}


sub get_one_to_n_protection_action
{
	my($conn, $sharedDeviceId) = @_;
	my $destHostId;
	my $destDeviceName;
	my $state;
	$logging_obj->log("DEBUG","get_one_to_n_protection_action :: sharedDeviceId ::  $sharedDeviceId ");
	my @par_array = ($sharedDeviceId, Fabric::Constants::START_PROTECTION_PENDING);
	my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
						"SELECT DISTINCT ".
						" destinationHostId, ".
						" destinationDeviceName, ".
						" lun_state ".
						" FROM ".
						" srcLogicalVolumeDestLogicalVolume ".
						" WHERE ".
						" Phy_Lunid = ? ".
						" AND ".
						" lun_state != ?", $par_array_ref);
	
	my @array=(\$destHostId, \$destDeviceName, \$state);
	$conn->sql_bind_columns($sqlStmnt, @array);
	while($conn->sql_fetch($sqlStmnt))
	{
		if($state == Fabric::Constants::STOP_PROTECTION_PENDING)
		{
			# skip this until there are no stop protection pendings
			return ITL_PROTECTOR_SKIP;
		}
	
	}
	
	# OK no stop protections, want to update if 1-to-n otherwise add
    return (&san_lun_one_to_n($conn, $sharedDeviceId) ? ITL_PROTECTOR_UPDATE_ONLY : ITL_PROTECTOR_ADD);
	
}

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
	
    # 1-to-n if more then one row returned
    return $conn->sql_num_rows($sqlStmnt) > 1;					
}

sub device_exists_in_host_appliance_target_lun_mapping
{
    my ($conn, $processServerId, $sharedDeviceId, $initiator, $srcDeviceName) = @_;


    $logging_obj->log("DEBUG","device_exists_in_host_appliance_target_lun_mapping($processServerId, $sharedDeviceId, $initiator, $srcDeviceName)");

    my @par_array = ($processServerId, $sharedDeviceId, $initiator, $srcDeviceName);
	my $par_array_ref = \@par_array;

    my $sqlStmnt = $conn->sql_query(
        "select distinct ".
        " processServerId, ".
        " sharedDeviceId, ".
        " exportedInitiatorPortWwn, ".
        " applianceTargetLunMappingId ".
        "from   ".
        " hostApplianceTargetLunMapping ".
        "where  ".
        " processServerId = ? and ".
        " sharedDeviceId = ? and  ".
        " exportedInitiatorPortWwn = ? and ".
		" srcDeviceName = ? ",$par_array_ref);

    return $conn->sql_num_rows($sqlStmnt);
}

sub get_appliance_initiator_infos
{
    my ($conn, $sharedDeviceId, $fabricId, $processServerId, $applianceInitiators) = @_;


    my $ai_state_to_error = Fabric::Constants::START_PROTECTION_PENDING .
    "," . Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING .
    "," . Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING;

    $logging_obj->log("DEBUG","get_appliance_initiator_infos($sharedDeviceId $fabricId, $processServerId, $applianceInitiators)");

    my $applianceInitiator;

    my @par_array = ($fabricId, $processServerId);
	my $par_array_ref = \@par_array;
	  my $sqlStmnt = $conn->sql_query(
        "select ".
        " applianceFrBindingInitiatorPortWwn ".
        "from ".
        " applianceFrBindingInitiators ".
        "where ".
        " fabricId = ? and".
        " processServerId = ? and ".
        " state = 'stable' and ".
        " error = 0",$par_array_ref);


	if (0 == $conn->sql_num_rows($sqlStmnt)) {
        &set_protection_job_state_to_error($conn, $sharedDeviceId, $ai_state_to_error,
                                           Fabric::Constants::APPLIANCE_INITIATORS_NOT_CONFIGURED);

        $logging_obj->log("DEBUG","Appliance initiators for fr binding are not configured");
		#die "Appliance initiators for fr binding are not configured";
    }


    my @array = (\$applianceInitiator);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) {
        push (@{$applianceInitiators}, $applianceInitiator);
    }
}

sub set_protection_job_state_to_error
{

    my ($conn, $sharedDeviceId, $currentState, $errorState) = @_;
     my $inCurrentState = Fabric::Constants::START_PROTECTION_PENDING .
    "," . Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING .
    "," . Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING;

	$logging_obj->log("DEBUG","set_protection_job_state_to_error($sharedDeviceId, $inCurrentState, $errorState)");

    my @par_array = ($errorState, $sharedDeviceId, $inCurrentState);
	my $par_array_ref = \@par_array;
	   my $sqlStmnt = $conn->sql_query(
        "update srcLogicalVolumeDestLogicalVolume ".
        " set lun_state = ? ".
        "where Phy_lunid = ? ".
        " and lun_state in (?)",$par_array_ref);

}

sub get_appliance_target_infos
{

    my ($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId, $applianceTargets) = @_;

    $logging_obj->log("DEBUG","get_appliance_target_infos($initiator, $sharedDeviceId, $fabricId, $processServerId, $applianceTargets)");

    my $at_state_to_error = Fabric::Constants::START_PROTECTION_PENDING .
    "," . Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING .
    "," . Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING;

    my $applianceTargetLunId;
    my $applianceTarget;
    my $applianceTargetLunNumber;

    &create_appliance_target_lun_mapping_info($conn, $sharedDeviceId, $fabricId, $processServerId);
    my $accessControlGroupId = &create_access_control_group_info($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId);
	
    my $sqlStmnt = &get_appliance_target_lun_info($conn, $sharedDeviceId, $fabricId, $processServerId);

    if (0 == $conn->sql_num_rows($sqlStmnt)) 
	{
        &set_protection_job_state_to_error($conn, $sharedDeviceId, $at_state_to_error,
                                           Fabric::Constants::APPLIANCE_TARGETS_NOT_CONFIGURED);

        $logging_obj->log("DEBUG","ERROR - Appliance targets for fr binding are not configured");
		#die "Appliance targets for fr binding are not configured";
    }


    my @array = (\$applianceTargetLunId, \$applianceTarget, \$applianceTargetLunNumber);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) 
	{
        push (@{$applianceTargets},
              {portWwn => $applianceTarget, lunId => $applianceTargetLunId, 
			  lunNumber => $applianceTargetLunNumber, acgId => $accessControlGroupId});
    }
}

sub get_appliance_target_lun_info
{

    my ($conn, $sharedDeviceId, $fabricId, $processServerId) = @_;

    $logging_obj->log("DEBUG","get_appliance_target_lun_info($fabricId, $sharedDeviceId, $processServerId)");

    my @par_array = ($fabricId, $sharedDeviceId, $processServerId);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query(
        "select ".
        " atlm.applianceTargetLunMappingId,".
        " afbt.applianceFrBindingTargetPortWwn, ".
		" atlm.applianceTargetLunMappingNumber ".
        "from ".
        " applianceFrBindingTargets afbt, ".
        " applianceTargetLunMapping atlm ".
        "where ".
        " afbt.fabricId = ? and ".
        " afbt.processServerId  = atlm.processServerId and ".
        " atlm.sharedDeviceId  = ? and ".
        " afbt.processServerId = ? ",$par_array_ref);
	
	
	return $sqlStmnt;
}

sub create_appliance_target_lun_mapping_info
{

    my ($conn, $sharedDeviceId, $fabricId, $processServerId) = @_;
    $logging_obj->log("DEBUG","create_appliance_target_lun_mapping_info($sharedDeviceId, $fabricId, $processServerId)");

    my $applianceTarget;
    my $rc = 0;
    my @par_array = ($fabricId, $processServerId);
	my $par_array_ref = \@par_array;

	  my $sqlStmnt = $conn->sql_query(
        "select ".
        " applianceFrBindingTargetPortWwn ".
        "from ".
        " applianceFrBindingTargets ".
        "where ".
        " fabricId = ? and ".
        " processServerId = ? and ".
        " state = 'stable' and ".
        " error = 0",$par_array_ref);
    my @array = (\$applianceTarget);
    $conn->sql_bind_columns($sqlStmnt,@array);
    while ($conn->sql_fetch($sqlStmnt)) 
	{
        my ($atLunId, $atLunNumber) = get_appliance_lun_info_for_shared_devices($conn, $sharedDeviceId, $processServerId);
        if (!$atLunId)
		{
            $atLunId = &get_next_appliance_target_lun_id($conn);
            $atLunNumber = &get_next_appliance_target_lun_number($conn, $processServerId);
        }

        my @par_array = ($atLunId, $sharedDeviceId, $atLunNumber, $processServerId);
		my $par_array_ref = \@par_array;

		my $sqlSelectStmnt = $conn->sql_query(
		"select ".
		" sharedDeviceId ".
		"from ".
		" applianceTargetLunMapping ".
		"where ".
		" applianceTargetLunMappingId = ? and ".
		" sharedDeviceId = ? and ".
		" applianceTargetLunMappingNumber = ? and ".
		" processServerId = ?",$par_array_ref);
		$logging_obj->log("DEBUG","create_appliance_target_lun_mapping_info($atLunId, $sharedDeviceId, $atLunNumber, $processServerId)");
		if ($conn->sql_num_rows($sqlSelectStmnt) == 0)
		{
			my @par_array = ($atLunId, $sharedDeviceId, $atLunNumber, $processServerId);
			my $par_array_ref = \@par_array;

			my $insertSqlStmnt = $conn->sql_query(
			"insert into ".
			" applianceTargetLunMapping ".
			"(applianceTargetLunMappingId,".
			" sharedDeviceId,".
			" applianceTargetLunMappingNumber, ".
			" processServerId) ".
			"values (?, ?, ?, ?)",$par_array_ref);


				if (!$insertSqlStmnt
				&& !($conn->sql_err() == $MYSQL_ER_DUP_KEY
					 || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
					 || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) {
				$logging_obj->log("EXCEPTION","ERROR - create_appliance_target_lun_mapping_info ".$conn->sql_error());
				die $conn->sql_error();

				}
        }
    }
}

sub get_appliance_lun_info_for_shared_devices
{
    my ($conn, $sharedDeviceId, $processServerId) = @_;

    my $sqlStmnt;
	$logging_obj->log("DEBUG","get_appliance_lun_info_for_shared_devices($sharedDeviceId, $processServerId)");
	
	my @arrayInfo = ($sharedDeviceId, $processServerId,Fabric::Constants::FAILOVER_RECONFIGURATION_PENDING);
   	my $arrayInfoRef = \@arrayInfo;
	my $srcSqlStmnt = $conn->sql_query("
								select 
								lun_state  
								from 
								srcLogicalVolumeDestLogicalVolume 
								where 
								Phy_Lunid = ? and 
								sourceHostId = ? and 
								lun_state = ?",$arrayInfoRef);
	if ($conn->sql_num_rows($srcSqlStmnt) > 0)
	{
		my @par_array = ($sharedDeviceId);
		my $par_array_ref = \@par_array;

		$sqlStmnt = $conn->sql_query(
			"select ".
			" applianceTargetLunMappingId,".
			" applianceTargetLunMappingNumber ".
			"from ".
			" applianceTargetLunMapping ".
			"where ".
			" sharedDeviceId = ? ".
			"limit 1",$par_array_ref);
	}
	else
	{
		my @par_array = ($sharedDeviceId, $processServerId);
		my $par_array_ref = \@par_array;

		$sqlStmnt = $conn->sql_query(
			"select ".
			" applianceTargetLunMappingId,".
			" applianceTargetLunMappingNumber ".
			"from ".
			" applianceTargetLunMapping ".
			"where ".
			" sharedDeviceId = ? and processServerId = ? ".
			"limit 1",$par_array_ref);
	}
	
	my ($atLunId, $atLunNumber);
	my @array = (\$atLunId, \$atLunNumber);
	   
	$conn->sql_bind_columns($sqlStmnt, @array);

	$conn->sql_fetch($sqlStmnt);
      
    return ($atLunId, $atLunNumber);
}

sub get_next_appliance_target_lun_id
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","get_next_appliance_target_lun_id");

    my $nextLunId;
    my $lunIdPrefix;
    my $sqlStmnt;

    $sqlStmnt = $conn->sql_query(
        "update nextApplianceLunIdLunNumber ".
        " set nextLunId = nextLunId + 1");

    $sqlStmnt = $conn->sql_query(
        "select nextLunId, lunIdPrefix ".
        "from nextApplianceLunIdLunNumber");

    my @array = (\$nextLunId,\$lunIdPrefix);
    $conn->sql_bind_columns($sqlStmnt,@array);

     if ($conn->sql_num_rows($sqlStmnt) == 0) {
        die "no entries found in nextApplianceLunIdLunNumber.";
    }

    $conn->sql_fetch($sqlStmnt);
    return sprintf("%s%010d", $lunIdPrefix, $nextLunId);
}

sub get_next_appliance_target_lun_number
{

    my ($conn, $processServerId) = @_;
    $logging_obj->log("DEBUG","get_next_appliance_target_lun_number($processServerId)");

     my $nextLunNumber;

     my @par_array = ($processServerId, $processServerId, $processServerId);
	 my $par_array_ref = \@par_array;

	  my $sqlStmnt = $conn->sql_query(
         "(select distinct applianceTargetLunNumber as nextLunNumber ".
         "from applianceTLNexus ".
         "where processServerId = ?) ".
         "union ".
         "(select distinct exportedLunNumber as nextLunNumber ".
         "from exportedTLNexus ".
         "where hostId = ?) ".
		 "union ".
         "(select distinct applianceTargetLunMappingNumber as nextLunNumber ".
         "from applianceTargetLunMapping ".
         "where processServerId = ?) ".
         "order by nextLunNumber",$par_array_ref);

     my $currentLunNumber = 0;


     my @array = (\$nextLunNumber);
     $conn->sql_bind_columns($sqlStmnt,@array);
	while ($conn->sql_fetch($sqlStmnt)) 
	{
        	if ($nextLunNumber - $currentLunNumber > 1) 
		{
             	return $currentLunNumber + 1;
         	}
         $currentLunNumber = $nextLunNumber;
	}

     return $currentLunNumber + 1;
}

sub create_access_control_group_info
{
	my ($conn, $initiator, $sharedDeviceId, $fabricId, $processServerId) = @_;
    $logging_obj->log("DEBUG","create_access_control_group_info($initiator, $sharedDeviceId, $fabricId, $processServerId)");
	
	my $insertState = Fabric::Constants::CREATE_ACG_DONE;
	
	my @par_array = ($initiator);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query(
        "select ".
        " acg.accessControlGroupId ".
        "from ".
        " accessControlGroup acg, ".
        " accessControlGroupPorts acgp ".
        "where ".
        " acgp.exportedInitiatorportWwn = ? and ".
        " acg.accessControlGroupId = acgp.accessControlGroupId ",$par_array_ref);
    if (0 == $conn->sql_num_rows($sqlStmnt)) 
	{
		
		my $accessControlGroupId = `uuidgen`;
		chomp($accessControlGroupId);	
		my $accessControlGroupName = $initiator;
		
		$logging_obj->log("DEBUG","create_access_control_group_info::accessControlGroupId=$accessControlGroupId,accessControlGroupName=$accessControlGroupName");
		my @par_array = ($accessControlGroupId, $accessControlGroupName, $insertState);
		my $par_array_ref = \@par_array;
		
		my $insertSqlStmnt = $conn->sql_query(
			"insert into ".
			" accessControlGroup ".
			"(accessControlGroupId,".
			" accessControlGroupName,".
			" state) ".
			"values (?, ?, ?)",$par_array_ref);


		if (!$insertSqlStmnt
		&& !($conn->sql_err() == $MYSQL_ER_DUP_KEY
			 || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
			 || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) 
		{
            $logging_obj->log("EXCEPTION","ERROR - create_access_control_group_info ".$conn->sql_error());
			die $conn->sql_error();
        }
		
		my @par_array = ($initiator, $accessControlGroupId);
		my $par_array_ref = \@par_array;
		
		my $sqlStmnt1 = $conn->sql_query(
			"insert into ".
			" accessControlGroupPorts ".
			"(exportedInitiatorportWwn,".
			" accessControlGroupId) ".
			"values (?, ?)",$par_array_ref);


		if (!$sqlStmnt1
		&& !($conn->sql_err() == $MYSQL_ER_DUP_KEY
			 || $conn->sql_err() == $MYSQL_ER_DUP_ENTRY
			 || $conn->sql_err() == $MYSQL_ER_DUP_UNIQUE)) 
		{
            $logging_obj->log("EXCEPTION","ERROR - create_access_control_group_info ".$conn->sql_error());
			die $conn->sql_error();
        }
		
		return $accessControlGroupId;
	}
	else
	{
		my $accessControlGroupId;
		my @array = (\$accessControlGroupId);
		$conn->sql_bind_columns($sqlStmnt,@array);
		$conn->sql_fetch($sqlStmnt); 
		$logging_obj->log("DEBUG","create_access_control_group_info::existing acgID :$accessControlGroupId");
		return $accessControlGroupId;
	}
	
}

sub finish_start_protection
{
	my ($conn, $sharedDeviceId, $processServerId,  $nodeHostId, $currentState) = @_;
    $logging_obj->log("DEBUG","finish_start_protection($sharedDeviceId, $processServerId, $nodeHostId, $currentState)");

    if (Fabric::Constants::NO_WORK != $currentState) 
	{
        # entries were added so we can now set them to protected
        &update_host_appliance_target_lun_mapping_state_for_lun($conn, $sharedDeviceId, $processServerId, $nodeHostId, $currentState, Fabric::Constants::PROTECTED);
    }

    if (!&all_host_appliance_target_lun_mapping_lun_ready_for_protection($conn, $sharedDeviceId, $processServerId)) 
	{
        return;
    }
	
	my $pairState;	
	my @info_array = ($sharedDeviceId, $processServerId);
 	my $par_array_ref = \@info_array;
	my $sqlStmnt = $conn->sql_query("select lun_state from srcLogicalVolumeDestLogicalVolume ".
	"where Phy_Lunid = ? and sourceHostId = ? ",$par_array_ref);
	my @array=(\$pairState);
	$conn->sql_bind_columns($sqlStmnt, @array);
	$conn->sql_fetch($sqlStmnt);
	if($pairState != Fabric::Constants::NODE_REBOOT_OR_PATH_ADDITION_PENDING)
	{
		my $write_log = 1;
		$logging_obj->log("DEBUG","finish_start_protection::pairState::$pairState");
		
		my @info_array = ($sharedDeviceId, $processServerId, Fabric::Constants::RESYNC_NEEDED);
		my $par_array_ref = \@info_array;
		my $sqlStmnt = $conn->sql_query("select shouldResync from srcLogicalVolumeDestLogicalVolume ".
		"where Phy_Lunid = ? and sourceHostId = ? and shouldResync = ?",$par_array_ref);
		
		$logging_obj->log("DEBUG","finish_start_protection::pair count with shouldResync set to 3 is ::".$conn->sql_num_rows($sqlStmnt));
		if ($conn->sql_num_rows($sqlStmnt) == 0)
		{		
		
			my @par_array = (&get_should_resync_value($conn, $sharedDeviceId, $processServerId,$write_log), time, 0, $sharedDeviceId, $processServerId,
								  Fabric::Constants::START_PROTECTION_PENDING,
								  Fabric::Constants::PROTECTED,
								  Fabric::Constants::REBOOT_PENDING,
								  Fabric::Constants::PROFILER_HOST_ID,
								  Fabric::Constants::PROFILER_DEVICE_NAME);
			my $par_array_ref = \@par_array;

			my $sdvSqlStmnt = $conn->sql_query(
				"update srcLogicalVolumeDestLogicalVolume ".
				" set ".
				"  shouldResync = ?, ".
				"  resyncSetCxtimestamp = ?, ".
				"  ShouldResyncSetFrom = ? ".
				"where Phy_lunid = ? and sourceHostId = ?" .
				" and lun_state in(?,?,?) ".
				" and destinationHostId != ? " .
				" and destinationDeviceName != ?",$par_array_ref);
		}
	}
	else
	{
		$logging_obj->log("DEBUG","finish_start_protection else::pairState::$pairState");
	}
    #
    # For LUN resize, final confirmation mail should be set
    # from here based on lun_state before it became PROTECTED
    #

	&update_and_send_email_alerts_for_lun_resize($conn, $sharedDeviceId);
    my @par_array = (Fabric::Constants::PROTECTED, $sharedDeviceId, $processServerId, Fabric::Constants::STOP_PROTECTION_PENDING);
	my $par_array_ref = \@par_array;

	my $sdvSqlUpdateStateStmnt = $conn->sql_query(
        "update srcLogicalVolumeDestLogicalVolume ".
        "set lun_state = ? ".
        "where Phy_lunid = ? and sourceHostId = ? and
		lun_state != ?",$par_array_ref);

}

sub update_and_send_email_alerts_for_lun_resize
{

    my ($conn, $sharedDeviceId) = @_;
	$logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize");
    my ($sourceName, $sourceIp, $sourceDevice, $sourceHostId,$nodeHostId);
    my ($targetName, $targetIp, $targetDevice,$targetId);
	my @idList;

    my @par_array = ($sharedDeviceId,Fabric::Constants::LUN_RESIZE_RECONFIGURATION_PENDING);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query(
				   "SELECT ".
					"app.sourceId as nodeId, ".
					"sd.sourceHostId , ".
					"sd.destinationHostId as tgtId, ".
					"sd.destinationDeviceName as tgtName ".	
					"FROM ".
					"applicationScenario app, ".
					"srcLogicalVolumeDestLogicalVolume sd  ".
					"WHERE ".
					"app.scenarioId=sd.scenarioId AND  ".
					"sd.Phy_Lunid = ? AND ".
					" sd.lun_state = ?",$par_array_ref);
		

    if ($conn->sql_num_rows($sqlStmnt) > 0)
	{
        #
        # Send mail with reconfiguration success
        #

       my @array = (\$nodeHostId, \$sourceHostId, \$targetId, \$targetDevice);
       $conn->sql_bind_columns($sqlStmnt,@array);

        #my $errId = $sourceHostId;
        
		my (@source_host_name_list,@source_ip_address_list);
		my ($source_host_name,$source_ip_address,$nodeId);
		my ($source_host,$source_ip);
        while ($conn->sql_fetch($sqlStmnt))
        {
            $logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::$nodeHostId, $sourceHostId, $targetId, $targetDevice");
			@idList = split(",",$nodeHostId);
			$targetName = &getHostName($conn,$targetId);
			$targetIp = &getHostIpaddress($conn,$targetId);
			$logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::$targetIp, $targetName");
			@source_ip_address_list = ();
			@source_host_name_list = ();
			$source_host_name = "";
			$source_ip_address = "";
			foreach $nodeId (@idList)
			{
				$source_host = &getHostName($conn,$nodeId);
				push(@source_host_name_list,$source_host);
				$source_ip = &getHostIpaddress($conn,$nodeId);
				push(@source_ip_address_list,$source_ip);
			}
			$source_host_name = join(",",@source_host_name_list);
			$source_ip_address = join(",",@source_ip_address_list);
			
			$sourceDevice = &getNodeDeviceName($conn,$sharedDeviceId);
			$logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::source_host_name:$source_host_name,source_ip_address:$source_ip_address");
			$logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::getNodeDeviceName:$sourceDevice");
		}	
			
			my $errSummary = "Source LUN Resize Warning";
			my $message = "The source Device(s)($sourceDevice) has been reconfigured upon resize and the following replication pair is paused. Please resize your target LUN to greater than or equal to source LUN and then resume the replication pair (Not applicable for profiler targets).";
			$message = $message."<br>Pair Details: <br><br>";
            $message = $message."Source Host                  : $source_host_name <br>";
            $message = $message."Source IP                    : $source_ip_address <br>";
            $message = $message."Source Volume                : $sourceDevice <br>";
            $message = $message."Destination Host             : $targetName <br>";
            $message = $message."Destination IP               : $targetIp <br>";
            $message = $message."Destination Volume           : $targetDevice <br>";

            $message = $message."===============================================<br>";

        my $errCode = "EC0123";
        my %err_placeholders;
        $err_placeholders{"SrcHostName"} = $source_host_name;
        $err_placeholders{"SrcIPAddress"} = $source_ip_address;
        $err_placeholders{"SrcVolume"} = $sourceDevice;
        $err_placeholders{"DestHostName"} = $targetName;
        $err_placeholders{"DestIPAddress"} = $targetIp;
        $err_placeholders{"DestVolume"} = $targetDevice;
        my $ser_err_placeholders = serialize(\%err_placeholders);

		$logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::add_error_message- $sourceHostId, $errSummary, $message ");
		&Messenger::add_error_message ($conn, $sourceHostId, "VOL_RESIZE", $errSummary, $message, $sourceHostId, $errCode, $ser_err_placeholders);
	
		my $sourceDeviceName = Fabric::Constants::INITIAL_LUN_DEVICE_NAME.$sharedDeviceId;
		$logging_obj->log("DEBUG","update_and_send_email_alerts_for_lun_resize::update_agent_log- $sourceHostId, $sourceDeviceName, $WEB_ROOT, $message ");
        Utilities::update_agent_log($sourceHostId, $sourceDeviceName, $WEB_ROOT, $message);
    }


    my @par_array = (Fabric::Constants::CAPACITY_CHANGE_RESET,$sharedDeviceId,Fabric::Constants::CAPACITY_CHANGE_PROCESSING);
	my $par_array_ref = \@par_array;
    my $sdvSqlUpdateStateStmnt = $conn->sql_query(
        "update sharedDevices ".
        "set volumeResize = ? ".
        "where sharedDeviceId = ? and volumeResize = ?",$par_array_ref);
}

sub get_should_resync_value
{
    my ($conn, $sharedDeviceId, $processServerId,$write_log) = @_;
    my ($sourceDeviceName,$agentLogString);

    my ($destinationHostId);
    my @par_array = ($sharedDeviceId, $processServerId, 0);
    my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query("select distinct destinationHostId from srcLogicalVolumeDestLogicalVolume ".
	"where Phy_lunid = ? and sourceHostId = ? and resyncStartTagtime = ?",$par_array_ref);
	
	if ($conn->sql_num_rows($sqlStmnt) > 0)
	{

		return 1;
	}
	else
	{
	    my @array = (\$destinationHostId );
	    $conn->sql_bind_columns($sqlStmnt,@array);
		$conn->sql_fetch($sqlStmnt);
		if($destinationHostId ne Fabric::Constants::PROFILER_HOST_ID)
		{
			#
			# Need to log this, as once pair moves to diff sync
			# resync required flag will be set
			#
			$sourceDeviceName = Fabric::Constants::INITIAL_LUN_DEVICE_NAME.$sharedDeviceId;
			$agentLogString = "Resync required flag is being set";
			if($write_log)
			{
				Utilities::update_agent_log($processServerId, $sourceDeviceName, $WEB_ROOT, $agentLogString);
			}
			return 2;
		}
		else
		{
			return 1;
		}

	}
}

sub getHostName
{
	
	my ($conn, $hostId) = @_;
	my ($name);
	$logging_obj->log("DEBUG","getHostName($hostId)");

	my @par_array = ($hostId);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = $conn->sql_query("SELECT 
				distinct name 
			FROM 
				hosts 
			WHERE 
				id=?",$par_array_ref);
	
	#my $rs = $conn->sql_query($sqlStmnt);

	if ($conn->sql_num_rows($sqlStmnt) == 0)
	{
		return 0;
    }
	my @array = (\$name );			
	$conn->sql_bind_columns($sqlStmnt,@array); 			
	$conn->sql_fetch($sqlStmnt);
	return $name;
}
 

sub getHostIpaddress
{
	
	my ($conn,$hostId) = @_;
	my ($ipaddress);
	$logging_obj->log("DEBUG","getHostIpaddress($hostId)");

	my @par_array = ($hostId);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = "SELECT 
				distinct ipaddress 
			FROM 
				hosts 
			WHERE 
				id='$hostId'";
				
	my $rs = $conn->sql_query($sqlStmnt);

	#$logging_obj->log("DEBUG","getHostIpaddress($sqlStmnt)");

	if (my $ref = $conn->sql_fetchrow_hash_ref($rs))
	{
		return $ipaddress = $ref->{'ipaddress'};
	}
	else
	{
		return 0;
	}	
}

sub getNodeDeviceName
{
	
	my ($conn,$sharedDeviceId) = @_;
	my ($hostId,$srcDeviceName,$hostName,$deviceNameStr,$processServerId);
	my @str;
	$logging_obj->log("DEBUG","getNodeDeviceName($sharedDeviceId)");
	
	my $pair_type = TimeShotManager::get_pair_type($conn,$sharedDeviceId); # pair_type 0=host,1=fabric,2=prism,3=array
    $logging_obj->log("DEBUG","getNodeDeviceName::pairType:$pair_type");
	
	my @par_array = ($sharedDeviceId);
	my $par_array_ref = \@par_array;

	my $sqlStmnt = "SELECT
                                    distinct  hostId,
                                    srcDeviceName,
									processServerId 		
									FROM
                                    hostApplianceTargetLunMapping  
									WHERE
                                    sharedDeviceId ='$sharedDeviceId'";
	#&fabric_util_trace("getNodeDeviceName($sqlStmnt)");			
	my $rs = $conn->sql_query($sqlStmnt);
	my $selRows = $conn->sql_num_rows($rs);
	if($selRows>0)
	{
		while (my $ref = $conn->sql_fetchrow_hash_ref($rs))
		{
			$hostId = $ref->{'hostId'};
			$srcDeviceName = $ref->{'srcDeviceName'};
			$processServerId = $ref->{'processServerId'};
			
			#
			# if pair is pillar type then we need fetch the info for processserver
			# since for pillar arrayid will be nodeid and we don't want to use arrayid.
			if($pair_type == 3)
			{
				$hostName = &getHostName($conn,$processServerId);
			}
			else
			{
				$hostName = &getHostName($conn,$hostId);
			}
			
			if($selRows == 1)
			{
				$deviceNameStr = $deviceNameStr.$srcDeviceName ." on ". $hostName;
			}
			else
			{
				$deviceNameStr = $deviceNameStr.$srcDeviceName ." on ". $hostName.",";
			}
			
		}
		$logging_obj->log("DEBUG","getNodeDeviceName($deviceNameStr)");
	}
	else
	{
		$deviceNameStr = 0;
	}
		return $deviceNameStr;
}


sub update_host_appliance_target_lun_mapping_state_for_lun
{

    my ($conn, $sharedDeviceId, $processServerId, $nodeHostId, $currentState, $nextState) = @_;

    $logging_obj->log("DEBUG"," update_host_appliance_target_lun_mapping_state_for_lun($sharedDeviceId, $nodeHostId, $currentState, $nextState)");

    my @par_array = ($nextState, $sharedDeviceId, $processServerId, $nodeHostId, $currentState);
	my $par_array_ref = \@par_array;

	 my $sqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ? ".
        "where sharedDeviceId = ? and processServerId = ? ".
        " and hostId = ? and state = ?",$par_array_ref);
}

sub all_host_appliance_target_lun_mapping_lun_ready_for_protection
{

    my ($conn, $sharedDeviceId, $processServerId) = @_;

    $logging_obj->log("DEBUG","all_host_appliance_target_lun_mapping_lun_ready_for_protection($sharedDeviceId, $processServerId)");

    my @par_array = ($sharedDeviceId, $processServerId, Fabric::Constants::PROTECTED);
	my $par_array_ref = \@par_array;
	my $sqlStmnt = $conn->sql_query(
        "select sharedDeviceId ".
        "from hostApplianceTargetLunMapping ".
        "where sharedDeviceId = ? and processServerId = ? and ".
        " state != ?",$par_array_ref);

     return ($conn->sql_num_rows($sqlStmnt) == 0);
}

sub process_create_appliance_lun_done
{
    my ($conn) = @_;

    $logging_obj->log("DEBUG","process_create_appliance_lun_done");

	my @par_array = (Fabric::Constants::CREATE_SPLIT_MIRROR_PENDING,Fabric::Constants::CREATE_AT_LUN_DONE);
	my $par_array_ref = \@par_array;
        my $sqlStmnt = $conn->sql_query(
        "update hostApplianceTargetLunMapping ".
        " set state = ? ".
        " where state = ?",$par_array_ref);
}


1;

