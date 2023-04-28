=head
	OfflineSync.pl
	--------------
	1. Offline Sync Export.
	2. Offline Sync Import.
=cut

use strict;
use warnings;
use FindBin qw( $Bin );
use lib $FindBin::Bin;
use Common_Utils;
use HostOpts;
use DatacenterOpts;
use VmOpts;
use DatastoreOpts;
use ResourcePoolOpts;
use FolderOpts;
use ComputeResourceOpts;
use XmlFunctions;
use UpdateMasterConfig;

my %customopts = (
				  	offline_sync_export	=> {type => "=s", help => "Performing Offline sync export(yes/no)?", required => 0,},
				  	offline_sync_import	=> {type => "=s", help => "Performing Offline sync import(yes/no)?", required => 0,},
				  );

#####AddOfflineSyncPathToVmdk#####
##Description			:: Adds InMage_Offline_Sync_Folder path to VMDK's. this is during Offline Sync Export.
##Input 				:: XML element node.
##Output 				:: Returns SUCCESS else FAILURE.
#####AddOfflineSyncPathToVmdk#####
sub AddOfflineSyncPathToVmdk
{
	my $hostNode	= shift;
	my $machineName	= $hostNode->getAttribute('display_name');
	my @disks 		= $hostNode->getElementsByTagName('disk');
	
	if ( -1 != $#disks )
	{
		foreach my $disk ( @disks )
		{
			my @diskPath = split( /\[|\]/ , $disk->getAttribute('disk_name') );
			$diskPath[-1]=~ s/^\s+//;
			my $diskName = "[$diskPath[1]] InMage_Offline_Sync_Folder/$diskPath[-1]";
			Common_Utils::WriteMessage("Disk Name after Export Change = $diskName.",2);
			$disk->setAttribute( 'disk_name' , $diskName );
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####RemoveOfflineSyncPathToVmdk#####
##Description			:: Removes InMage_Offline_Sync_Folder/ from the Disk Name.
##Input 				:: All the Source Hosts.
##Output 				:: Returns SUCCESS else FAILURE.
#####RemoveOfflineSyncPathToVmdk#####
sub RemoveOfflineSyncPathToVmdk
{
	my $sourceHosts 	= shift;
	
	foreach my $host ( @$sourceHosts )
	{
		my @disks	 	= $host->getElementsByTagName('disk');
		foreach my $disk( @disks )
		{
			my @diskParts		= split( /\[|\]/ , $disk->getAttribute('disk_name') );
			my @splitonSlash	= split( /\// , $diskParts[-1] );
			my $folderPath 		= $host->getAttribute('folder_path');
			my $fileName		= "[".$disk->getAttribute('datastore_selected')."] $folderPath$splitonSlash[-1]";
			$disk->setAttribute( 'disk_name' , $fileName );
		}
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####GetDefaultResourcePoolName#####
##Description			:: Gets the Default resource Pool Name.
##Input 				:: Resource Pools Info.
##Output 				:: Returns SUCCESS else FAILURE.
#####GetDefaultResourcePoolName#####
sub GetDefaultResourcePoolName
{
	my $resourcePoolInfo 	= shift;
	
	foreach my $resourcePool ( @$resourcePoolInfo )
	{
		if ( ( "computeresource" eq lc( $$resourcePool{ownerType} ) ) || ( "clustercomputeresource" eq lc ( $$resourcePool{ownerType} ) ) )
		{
			return ( $Common_Utils::SUCCESS , $$resourcePool{groupName} );
		} 
	}
	Common_Utils::WriteMessage("ERROR :: Failed to find default resource name.",3);
	return ( $Common_Utils::FAILURE );
}

#####CreateVmdkUsingMT#####
##Description			:: Creates VMDK under Inmage_Offline_sync_folder using the Master Target View.
##Input 				:: XML node element and Master Target UUID.
##Output 				:: Returns SUCCESS else FAILURE.
#####CreateVmdkUsingMT#####
sub CreateVmdkUsingMT
{
	my $hostNode 		= shift;
	my $masterTargetUuid= shift;
	
	my %mapDiskScsiId	= ();
	my ( $returnCode , $vmView ) = VmOpts::GetVmViewByUuid( $masterTargetUuid );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	my $devices 		= Common_Utils::GetViewProperty( $vmView, 'config.hardware.device');
	foreach my $device ( @$devices )
	{
		if ( $device->deviceInfo->label =~ /Hard Disk/i )
		{
			my $key_file 					= $device->key;		
			my $diskName 					= $device->backing->fileName;
			my @diskPath 					= split( /\[|\]/ , $diskName );
			if ( $key_file>=2000 && $key_file<=2100 )
			{
				my $controller_number 		= Common_Utils::floor(($key_file - 2000)/16);
				my $unit_number 			= ($key_file - 2000)%16;
				my $scsiControllerNumber	= "$controller_number:$unit_number";
				$mapDiskScsiId{$scsiControllerNumber}= $diskPath[-1];
			}
		}
	}
	
	my @disks			= $hostNode->getElementsByTagName('disk');
	my @freeslots		= ();
	for ( my $i = 0 ; $i <= 3 ; $i++ )
	{
		for ( my $j = 0 ; $j <= 15 ; $j++ )
		{
			if ( 7 == $j )
			{
				next;
			}	
			my $scsiNum		= "$i:$j";
			if ( exists $mapDiskScsiId{$scsiNum} )
			{
				next;
			}
			push @freeslots	, $scsiNum;
			if ( $#freeslots == $#disks )
			{
				last;
			}
		}
	}
	
	for( my $i = 0 ; $i <= $#disks ; $i ++ )
	{
		my @diskPath			= split( /\[|\]/ , $disks[$i]->getAttribute('disk_name') );
		my $fileName			= "[".$disks[$i]->getAttribute('datastore_selected')."]$diskPath[-1]";
		
		my @scsiController		= split( /:/ , $freeslots[$i] );
		
		$returnCode 			= VmOpts::CreateVmdk( 
												 		fileName		=> $fileName,
												  		fileSize		=> $disks[$i]->getAttribute('size'),
												  		controllerKey	=> $scsiController[0] + 1000,
												  		unitNumber		=> $scsiController[1],
												  		diskType		=> $disks[$i]->getAttribute('thin_disk'),
												  		diskMode 		=> $disks[$i]->getAttribute('independent_persistent'),
												  		clusterDisk		=> $disks[$i]->getAttribute('cluster_disk'),
												  		vmView			=> $vmView,
												  	);		
		if( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		$disks[$i]->setAttribute('scsi_id_onmastertarget' , $freeslots[$i] );										 	
	}
	return ( $Common_Utils::SUCCESS );
}

#####MoveVmdk#####
##Description			:: Moves the VMDK and it's flat file between two different paths.
##Input 				:: xml source Node.
##Output 				:: Returns SUCCESS else FAILURE.
#####MoveVmdk#####
sub MoveVmdk
{
	my $hostNode 		= shift;
	my $datacenterName	= shift;
	
	my @disks 			= $hostNode->getElementsByTagName('disk');
	foreach my $disk ( @disks )
	{
		my $diskName 	= $disk->getAttribute('disk_name');
		my @diskParts	= split( /\[|\]/, $diskName);
		my $srcDiskPath	= "[".$disk->getAttribute('datastore_selected')."]$diskParts[-1]";
		my $folderPath	= $hostNode->getAttribute('folder_path');
		my @splitonSlash= split( /\// , $diskParts[-1] );
		my $tgtDiskPath	= "[".$disk->getAttribute('datastore_selected')."] $folderPath$splitonSlash[-1]";
		Common_Utils::WriteMessage("Moving $srcDiskPath to $tgtDiskPath.",2);
		
		my $returnCode  = DatastoreOpts::MoveOrCopyFile("move" , $datacenterName , $srcDiskPath , $tgtDiskPath ,  1 );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		} 
		
		$srcDiskPath	=~ s/.vmdk$/-flat.vmdk/;
		$tgtDiskPath	=~ s/.vmdk$/-flat.vmdk/;
		Common_Utils::WriteMessage("Moving $srcDiskPath to $tgtDiskPath.",2);
		$returnCode  	= DatastoreOpts::MoveOrCopyFile("move" , $datacenterName , $srcDiskPath , $tgtDiskPath ,  1 );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####AddVmdkToImportedVm#####
##Description			:: In case of Offline sync IMport it adds vmdk to Imported virtual machine.
##Input 				:: host element node and virtual machine.
##Output				:: Returns SUCCESS else FAILURE.
#####AddVmdkToImportedVm#####
sub AddVmdkToImportedVm
{
	my $hostNode 				= shift;
	my $vmView 					= shift;
	
	my @disks					= $hostNode->getElementsByTagName('disk');
	foreach my $diskInfo ( @disks )
	{
		my $sourceScsiId		= $diskInfo->getAttribute( 'scsi_mapping_vmx' );
		my @splitController 	= split ( /:/ , $sourceScsiId );
		my $Controllerkey		= 1000 + $splitController[0];
		if ( $Common_Utils::FAILURE eq VmOpts::FindScsiController( $vmView , $Controllerkey ) ) 
		{
			my $sharing 		= $diskInfo->getAttribute('controller_mode');
			if( "" eq $sharing )
			{
				$sharing		= "noSharing";
			}
			my $ControllerType 	= $diskInfo->getAttribute('controller_type');
			my $returnCode		= VmOpts::AddSCSIController( vmView => $vmView , busNumber => $splitController[0] , sharing => $sharing , controllerType => $ControllerType );
			if ( $returnCode ne $Common_Utils::SUCCESS ) 
			{
				Common_Utils::WriteMessage("ERROR :: Failed to add SCSI controller( machine UUID = ).",3);
				return  ( $Common_Utils::FAILURE );
			}
		}
		
		my @diskPath			= split( /\[|\]/ , $diskInfo->getAttribute('disk_name') );
		my @splitonSlash		= split( /\// , $diskPath[-1] );
		my $folderPath 			= $hostNode->getAttribute('folder_path');
		my $fileName			= "[".$diskInfo->getAttribute('datastore_selected')."] $folderPath$splitonSlash[-1]";
		my $fileSize 			= $diskInfo->getAttribute('size');
		my $unitNumber 			= $splitController[1];
		my $diskType 			= $diskInfo->getAttribute('thin_disk');
		my $diskMode			= $diskInfo->getAttribute('independent_persistent');
		my $clusterDisk			= $diskInfo->getAttribute('cluster_disk');
		
		Common_Utils::WriteMessage("Adding disk $fileName of size $fileSize at SCSI unit number $unitNumber of type $diskType in mode $diskMode.",2);
		my $returnCode 			= VmOpts::AddVmdk2( 
												 	fileName		=> $fileName,
												  	fileSize		=> $fileSize,
												  	controllerKey	=> $Controllerkey,
												  	unitNumber		=> $splitController[1],
												  	diskType		=> $diskType,
												  	diskMode 		=> $diskMode,
												  	clusterDisk		=> $clusterDisk,
												  	vmView			=> $vmView,
												  );		
		if( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
		
	return ( $Common_Utils::SUCCESS );
}

#####ModifyDiskPathsInMasterTarget#####
##Description			:: Modifies the Disk path of VMDK in Master Target. This is required in case of Offline Sync Import.
##Input 				:: Master Target View.
##Output 				:: Returns SUCCESS else FAILURE.
#####ModifyDiskPathsInMasterTarget#####
sub ModifyDiskPathsInMasterTarget
{
	my $mtVmView		= shift;
	my $sourceHosts	 	= shift;
	
	my $devices 		= Common_Utils::GetViewProperty( $mtVmView, 'config.hardware.device');
	foreach my $host ( @$sourceHosts )
	{
		my @disks	 	= $host->getElementsByTagName('disk');
		foreach my $disk ( @disks )
		{
			my $diskNameXml				= $disk->getAttribute('disk_name');
			my @diskPartsXml			= split( /\[|\]/ , $diskNameXml );
			foreach my $device ( @$devices )
			{
				if ( ( $device->deviceInfo->label !~ /Hard Disk/i ) )
				{
					next;
				}
				my $diskName 			= $device->backing->fileName;
				my @diskParts			= split( /\[|\]/ , $diskName );
				$diskPartsXml[-1] 		=~ s/^\s//;
				if ( $diskParts[-1] !~ /$diskPartsXml[-1]/ )
				{
					next;
				}
				my $keyfile 			= $device->key;		
				
				my $capacity 			= $device->capacityInKB;
				my $diskMode 			= $device->backing->diskMode;
				my $controllerkey		= $device->controllerKey;
				my $unitNumber 			= ( $keyfile - 2000 )%16;
				
				my @splitDiskNameXml	= split( /\[|\]/ , $diskNameXml );
				my @splitonSlash		= split( /\// , $splitDiskNameXml[-1] );
				my $folderPath 			= $host->getAttribute('folder_path');
				my $newDiskName 		= "[".$disk->getAttribute('datastore_selected')."] $folderPath$splitonSlash[-1]";
				Common_Utils::WriteMessage("New Disk Name = $newDiskName.",2);
				my $diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $diskMode, fileName => $diskName );
																		
				my $virtualDisk			= VirtualDisk->new( controllerKey => $controllerkey , unitNumber => $unitNumber ,key => $keyfile, backing => $diskBackingInfo , capacityInKB => $capacity );
														
				my $virtualDeviceSpecR 	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $virtualDisk );
				
				$diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $diskMode, fileName => $newDiskName );
				
				$virtualDisk			= VirtualDisk->new( controllerKey => $controllerkey , unitNumber => $unitNumber ,key => -1, backing => $diskBackingInfo , capacityInKB => $capacity );
												
				my $virtualDeviceSpecA 	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('add'), device => $virtualDisk );
				
				my $vmConfigSpec 		= VirtualMachineConfigSpec->new( deviceChange => [$virtualDeviceSpecR, $virtualDeviceSpecA ] );
				
				for ( my $i = 0 ;; $i++ )
				{
					eval
					{
						$mtVmView->ReconfigVM( spec => $vmConfigSpec );
					};
					if ( $@ )
					{
						if ( ref($@->detail) eq 'TaskInProgress' )
						{
							Common_Utils::WriteMessage("Reconfigure Disks on MT :: waiting for 3 secs as another task is in progress.",2);
							sleep(3);
							next;
						}
						Common_Utils::WriteMessage("ERROR :: $@.",3);
						Common_Utils::WriteMessage("ERROR :: Failed to reconfigure disk $diskName ",3);
						return ( $Common_Utils::FAILURE );
					}
					last;
				}
				Common_Utils::WriteMessage("Successfully reconfigured $diskName.",2);
				last;
			}
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####OfflineSyncExport#####
##Description			:: Performs Offline Sync Export. It created all the directories under InMageOfflineSync/ folder.
##Input 				:: Xml file in which configuration is specified.
##Output				:: Returns SUCCESS else FAILURE.
#####OfflineSyncExport#####
sub OfflineSyncExport
{
	my %args 				= @_;
	
	Common_Utils::WriteMessage("Validating file $args{file}.",2);
	
	my ( $returnCode , $docNode )	= XmlFunctions::GetPlanNodes( $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get root node of xml file $args{file}.",2);
		return ( $Common_Utils::FAILURE );
	}
	
	my $planNodes 					= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 					= [$docNode];
	}
	
	( $returnCode ,my $cxIpPortNum )= Common_Utils::FindCXIP();
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	foreach my $plan ( @$planNodes )
	{
		my $planName 				= $plan->getAttribute('plan');
		my @sourceNodes				= $plan->getElementsByTagName('SRC_ESX');
		my @TargetNodes				= $plan->getElementsByTagName('TARGET_ESX');
		$Common_Utils::vContinuumPlanDir = $plan->getAttribute('xmlDirectoryName');

		if ( $#TargetNodes > 0 )
		{
			Common_Utils::WriteMessage("ERROR :: \"$planName\" contains multiple target nodes.",3);
			Common_Utils::WriteMessage("ERROR :: Please make sure that in a plan single master target is selected.",3);
			return ( $Common_Utils::FAILURE );
		}
		Common_Utils::WriteMessage("Performing export on plan $planName.",2);
		my @sourceHosts				= $sourceNodes[0]->getElementsByTagName('host');
		my @TargetHost 				= $TargetNodes[0]->getElementsByTagName('host');
		my $mtHostId				= $TargetHost[0]->getAttribute('inmage_hostid');
		
		my( $returnCode , $mtView )= VmOpts::GetVmViewByUuid( $TargetHost[0]->getAttribute('source_uuid') );
		if ( $Common_Utils::SUCCESS ne $returnCode )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		if ( $TargetHost[0]->getAttribute('hostversion') lt "4.0.0" )
		{
			$returnCode 			= VmOpts::PowerOff( $mtView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
		}
		my  $datacenterName 		= $TargetHost[0]->getAttribute('datacenter');
		
		if( "linux" eq lc( $TargetHost[0]->getAttribute('os_info') ) )
		{
			$returnCode		= XmlFunctions::DummyDiskCleanUpTask( hostId => $mtHostId, cxIpPortNum => $cxIpPortNum, hostNode => $TargetNodes[0] );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
			}
		}
		
		if ( $Common_Utils::SUCCESS != VmOpts::RemoveDummyDisk( $mtView ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to remove dummy disk from master target.\n",3);
			return ( $Common_Utils::FAILURE );
		}
		
		$returnCode						= VmOpts::SetVmExtraConfigParam( vmView => $mtView, paramName => "disk.enableuuid", paramValue => "true" );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to set disk.enableuuid value as true to Master Target.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		foreach my $sourceHost ( @sourceHosts )
		{
			my $machineName 		= $sourceHost->getAttribute('display_name');
			if ( ( defined ( $sourceHost->getAttribute('new_displayname') ) ) && ( "" ne $sourceHost->getAttribute('new_displayname') ) )
			{
				$machineName		= $sourceHost->getAttribute('new_displayname');
			}
			
			if( "windows" eq lc( $sourceHost->getAttribute('os_info') ) )
			{
				my @hostNameSplit 	= split('\.',$sourceHost->getAttribute('hostname') );
				Common_Utils::WriteMessage("Setting Host name to $hostNameSplit[0].",2);
				$sourceHost->setAttribute( 'hostname' , $hostNameSplit[0] );
			}
			
			if( $Common_Utils::SUCCESS ne AddOfflineSyncPathToVmdk( $sourceHost ) )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			my %CreatedVirtualMachineInfo	= ();
			my $masterTargetUuid			= $TargetHost[0]->getAttribute('source_uuid');
			$CreatedVirtualMachineInfo{uuid}= $masterTargetUuid;
			
			my @disks						= $sourceHost->getElementsByTagName('disk');
			my $pathToCreate				= "[".$disks[0]->getAttribute('datastore_selected')."] InMage_Offline_Sync_Folder";
			$returnCode 					= DatastoreOpts::CreatePath( $pathToCreate , $datacenterName);
			if ( $returnCode == $Common_Utils::FAILURE )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			$pathToCreate					= "[".$disks[0]->getAttribute('datastore_selected')."] InMage_Offline_Sync_Folder/".$sourceHost->getAttribute('folder_path');
			if( rindex($pathToCreate,"/") == length($pathToCreate)-1)
			{
				#$pathToCreate = substr($pathToCreate,0, length($pathToCreate)-1);
				chop $pathToCreate;
			}
			$returnCode 					= DatastoreOpts::CreatePath( $pathToCreate , $datacenterName);
			if ( $returnCode == $Common_Utils::FAILURE )
			{
				return ( $Common_Utils::FAILURE );
			}
				
			$returnCode						= CreateVmdkUsingMT( $sourceHost , $masterTargetUuid );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to create disk in machine \"$machineName\".",3);
				return ( $Common_Utils::FAILURE );
			}
			
			$returnCode 			= VmOpts::MapSourceScsiNumbersOfHost( cxIpAndPortNum => $cxIpPortNum , host => $sourceHost );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to Write InMage SCSI units file for \"$machineName\".",3);
				return ( $Common_Utils::FAILURE );
			}

		}
		if ( $TargetHost[0]->getAttribute('hostversion') lt "4.0.0" )
		{
			$returnCode 	= VmOpts::PowerOn( $mtView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
		}
	}
	
	my @docElements = $docNode;
	$returnCode		= XmlFunctions::SaveXML( \@docElements , $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	$returnCode 	= XmlFunctions::WriteInmageScsiUnitsFile( file => $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to Write InMage SCSI units file.",3);
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS );
}

#####OfflineSyncImport#####
##Description			:: Performs Offline Sync Import. It moves all the folders from Offline Sync folder to root level of datastore.
##Input 				:: Xml file which contains all the data of configuration which has to be imported.
##Output 				:: Returns SUCCESS else FAILURE.
#####OfflineSyncImport#####
sub OfflineSyncImport
{
	my %args 		= @_;
	
	Common_Utils::WriteMessage("Offline Sync Import :: file $args{file}.",2);
	
	my ( $returnCode , $docNode )	= XmlFunctions::GetPlanNodes( $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get root node of xml file $args{file}.",2);
		return ( $Common_Utils::FAILURE );
	}
	
	my $planNodes 					= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 					= [$docNode];
	}
	
	( $returnCode , my $resourcePoolViews )	= ResourcePoolOpts::GetResourcePoolViews();
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	foreach my $plan ( @$planNodes )
	{
		my $planName 				= $plan->getAttribute('plan');
		my @sourceNodes				= $plan->getElementsByTagName('SRC_ESX');
		my @TargetNodes				= $plan->getElementsByTagName('TARGET_ESX');
		$Common_Utils::vContinuumPlanDir = $plan->getAttribute('xmlDirectoryName');

		if ( $#TargetNodes > 0 )
		{
			Common_Utils::WriteMessage("ERROR :: \"$planName\" contains multiple target nodes.",3);
			Common_Utils::WriteMessage("ERROR :: Please make sure that in a plan single master target is selected.",3);
			return ( $Common_Utils::FAILURE );
		}
		Common_Utils::WriteMessage("Performing import on plan $planName.",2);
		my @sourceHosts				= $sourceNodes[0]->getElementsByTagName('host');
		my @TargetHost 				= $TargetNodes[0]->getElementsByTagName('host');
		
		my $targetvSphereHostName 	= $TargetHost[0]->getAttribute( 'vSpherehostname' );
		my $targetDatacenterName	= $TargetHost[0]->getAttribute( 'datacenter' );
		if ( "" eq $targetvSphereHostName )
		{
			Common_Utils::WriteMessage("ERROR :: Found empty value for vSphereHostName for plan $planName.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , my $hostView ) 			= HostOpts::GetSubHostView( $targetvSphereHostName );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		my @hostViews		= ( $hostView );
		( $returnCode , my $dvPortGroupInfo ) 	= HostOpts::GetDvPortGroups( \@hostViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , my $datacenterView )	= DatacenterOpts::GetDataCenterView( $targetDatacenterName );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		my $vmFolderView 						= Vim::get_view( mo_ref => $datacenterView->vmFolder, properties => [] );	
			
		( $returnCode , my $resourcePoolInfo )	= ResourcePoolOpts::FindResourcePoolInfo( $resourcePoolViews , \@hostViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
		
		( $returnCode,my $resourcePoolGroupName)= GetDefaultResourcePoolName( $resourcePoolInfo );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , my $resourcepoolView )	= ResourcePoolOpts::FindResourcePool( $resourcePoolGroupName );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		my $IsItvCenter 			= VmOpts::IsItvCenter();
		
		my @ImportedVirtualMachineInfo 	= ();
		foreach my $sourceHost ( @sourceHosts )
		{
			my %ImportedVirtualInfo = (); 
			my $machineName 		= $sourceHost->getAttribute('display_name');
			if ( ( defined ( $sourceHost->getAttribute('new_displayname') ) ) && ( "" ne $sourceHost->getAttribute('new_displayname') ) )
			{
				$machineName		= $sourceHost->getAttribute('new_displayname');
			}
			
			$sourceHost->setAttribute( 'resourcepoolgrpname' , $resourcePoolGroupName );
			$returnCode 		= FolderOpts::CreateVmConfig( $sourceHost, $TargetHost[0], \%ImportedVirtualInfo , $hostView , $datacenterView  );
			if ( $returnCode != $Common_Utils::SUCCESS )
					{
				Common_Utils::WriteMessage("Creation of virtual machine configuration failed on target side.",3);
				push @ImportedVirtualMachineInfo , \%ImportedVirtualInfo;
				return ( $Common_Utils::FAILURE );
			}
			
			( $returnCode , my $vmView )= VmOpts::GetVmViewByUuid( $ImportedVirtualInfo{uuid} );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				push @ImportedVirtualMachineInfo , \%ImportedVirtualInfo;
				last;
			}
			
			$returnCode 			= VmOpts::UpdateVm( vmView => $vmView , sourceHost => $sourceHost , isItvCenter => $IsItvCenter ,
															dvPortGroupInfo => $dvPortGroupInfo );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			$returnCode 			= MoveVmdk( $sourceHost , $targetDatacenterName );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}

			$returnCode 			= AddVmdkToImportedVm( $sourceHost , $vmView );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}

			$returnCode 			= VmOpts::OnOffMachine( $vmView );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			( $returnCode ,$vmView )= VmOpts::GetVmViewByUuid( $ImportedVirtualInfo{uuid} );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				push @ImportedVirtualMachineInfo , \%ImportedVirtualInfo;
				last;
			}
			
			my $guestId				= $sourceHost->getAttribute('alt_guest_name');
			$returnCode 			= VmOpts::CheckGuestOsFullName( $vmView, $guestId );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			if ( $Common_Utils::SUCCESS ne XmlFunctions::UpdateNewMAC( $sourceHost , $vmView ) )
			{
			}
		}
		
		my $mtDatastore			= $TargetHost[0]->getAttribute('datastore');
		my $vmPathName 			= $TargetHost[0]->getAttribute('vmx_path');
		my $mtDisplayName 		= $TargetHost[0]->getAttribute('display_name');
		my @vmPathSplit			= split ( /\[|\]/ , $vmPathName );
		$vmPathName 			= "[$mtDatastore]$vmPathSplit[-1]";
		Common_Utils::WriteMessage("Registering $mtDisplayName, vmPathName = $vmPathName.",2);
		( $returnCode, my $mtView )	= VmOpts::RegisterVm( vmName => $mtDisplayName, vmPathName => $vmPathName , folderView => $vmFolderView , resourcepoolView => $resourcepoolView ,
														hostView => $hostView );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , $mtView )	= VmOpts::GetVmViewByVmPathName( $vmPathName , $hostView );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
					
		$returnCode 			= ModifyDiskPathsInMasterTarget( $mtView , \@sourceHosts  );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		$returnCode						= VmOpts::SetVmExtraConfigParam( vmView => $mtView, paramName => "disk.enableuuid", paramValue => "true" );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to set disk.enableuuid value as true to Master Target.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		$returnCode 			= VmOpts::PowerOn( $mtView );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		if( $Common_Utils::SUCCESS ne RemoveOfflineSyncPathToVmdk( \@sourceHosts ) )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
	
	my @docElements = $docNode;
	$returnCode		= XmlFunctions::SaveXML( \@docElements , $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS );
}  

BEGIN:

my $ReturnCode				= $Common_Utils::SUCCESS;
my ( $sec, $min, $hour )	= localtime(time);
my $logFileName 		 	= "vContinuum_Temp_$sec$min$hour.log";
my $file					= "";

Common_Utils::CreateLogFiles( "$Common_Utils::vContinuumLogsPath/$logFileName" , "$Common_Utils::vContinuumLogsPath/vContinuum_ESX.log" );

Opts::add_options(%customopts);
Opts::parse();

my( $returnCode, $serverIp, $userName, $password )	= HostOpts::getEsxCreds( Opts::get_option('server') );
Opts::set_option('username', $userName);
Opts::set_option('password', $password );

Opts::validate();

if($returnCode	!= $Common_Utils::SUCCESS )
{
	Common_Utils::WriteMessage("ERROR :: Failed to get credentials from CX.",3);
}
elsif ( ! ( Opts::option_is_set('username') && Opts::option_is_set('password') && Opts::option_is_set('server') ) )
{
	Common_Utils::WriteMessage("ERROR :: vCenter/vSphere IP,Username and Password values are the basic parameters required.",3);
	Common_Utils::WriteMessage("ERROR :: One or all of the above parameters are not set/passed to script.",3);
}
else
{
	my $ESX_IP 		= Opts::get_option('server');

	$ReturnCode		= Common_Utils::Connect( Server_IP => $ESX_IP, UserName => Opts::get_option('username'), Password => Opts::get_option('password') );
	if ( $Common_Utils::SUCCESS == $ReturnCode )
	{
		$file	= "$Common_Utils::vContinuumMetaFilesPath/ESX.xml";
		eval
		{
			if ( Opts::option_is_set('offline_sync_export') && ( "yes" eq lc ( Opts::get_option('offline_sync_export') ) ) )
			{
				$ReturnCode	= OfflineSyncExport( file => $file );
			}
			elsif ( Opts::option_is_set('offline_sync_import') && ( "yes" eq lc ( Opts::get_option('offline_sync_import') ) ) )
			{
				$ReturnCode	= OfflineSyncImport( file => $file );
			}
			else
			{
				Common_Utils::WriteMessage("ERROR :: Please specify an option which indicates to the operation we are performing.",3);
				Common_Utils::WriteMessage("ERROR :: Either it can be Offline sync export or Offline sync Import.",3);
				$ReturnCode	= $Common_Utils::FAILURE;
			}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			$ReturnCode	= $Common_Utils::FAILURE;
		}
	}
}
Common_Utils::LoggedData( tempLog => "$Common_Utils::vContinuumLogsPath/$logFileName" , planLog => $Common_Utils::vContinuumPlanDir );
if ( -e $Common_Utils::planTempLogFile )
{
	unlink $Common_Utils::planTempLogFile;
}
exit( $ReturnCode );

__END__