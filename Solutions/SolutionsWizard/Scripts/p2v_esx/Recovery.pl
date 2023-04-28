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

my %customopts = (
					directory 	=> {type => "=s", help => "If performing recovery, this parameter specifies path to Recovery.xml file", required => 1,},
				  	recovery 	=> {type => "=s", help => "If recovery, pass 'yes' to this parameter",required => 0,},
				  	remove		=> {type => "=s", help => "If removal of old protections, pass 'yes' to this parameter.",required => 0,},
				  	dr_drill	=> {type => "=s", help => "If Dr-Drill, set this argument to 'yes'.",required => 0,},
				  	cxpath		=> {type => "=s", help => "Path to be used ib CX to place log files.",required => 0,},
				  	planid		=> {type => "=s", help => "Plan ID of current Recovery/DrDrill plan.",required => 0,},
				  	recovery_v2p=> {type => "=s", help => "If v2p recovery, pass 'yes' to this parameter",required => 0,},
				  	removedisk	=> {type => "=s", help => "If removal of protected vmdks, pass 'yes' to this parameter.",required => 0,},
				  );

my @cmdArgs	= @ARGV;

#####RemoveVmdkFromMT#####
##Description 	:: First gets the View of Master Target and then it pre-pares all the information about the disk which has to be removed.
##Input 		:: Source Node, Target Node and Hash for collecting recovered vm Information.
##Output 		:: Returns SUCCESS or FAILURE.
#####RemoveVmdkFromMT#####
sub RemoveVmdkFromMT
{
	my %args 			= @_;	
	my $ReturnCode		= $Common_Utils::SUCCESS;
	my @disksInfo 		= $args{sourceHost}->getElementsByTagName('disk');
	my $machineName 	= $args{vmView}->name;
	my $inmageHostId	= $args{sourceHost}->getAttribute('inmage_hostid');
	my $displayName 	= $args{sourceHost}->getAttribute('display_name');
	if ( ( defined $args{sourceHost}->getAttribute('new_displayname') ) && ( "" ne $args{sourceHost}->getAttribute('new_displayname') ) )
	{
		$displayName	= $args{sourceHost}->getAttribute('new_displayname');
	}
	if ( -1 == $#disksInfo )
	{
		Common_Utils::WriteMessage("ERROR :: Disks are not listed for machine $displayName in XML file.Could not remove its disks from master target.",3);
		return ( $Common_Utils::FAILURE );
	}
	my $devices 		= Common_Utils::GetViewProperty( $args{vmView}, 'config.hardware.device');
	my $diskCountXml	= 0;
	my $diskCountVm		= 0;	
	foreach my $disk ( @disksInfo )
	{
		my $detachDiskWithScsiID= $disk->getAttribute('scsi_id_onmastertarget');
		my $detachDiskWithUuid	= $disk->getAttribute('target_disk_uuid');
		my $diskName 			= $disk->getAttribute('disk_name');
		my @splitDiskName 		= split( /\[|\]/ , $diskName );
		
		if( defined ${$args{mapKeysOfVm}}{$detachDiskWithUuid} )
		{
			next;
		}
		
		$diskCountXml++;
		foreach my $device ( @$devices )
		{
			if( $device->deviceInfo->label !~ /Hard Disk/i )
			{
				next;
			}
			my $key_file 				= $device->key;		
			my $disk_name 				= $device->backing->fileName;
			if ( ( defined $args{vmView}->snapshot ) && ( Common_Utils::GetViewProperty( $args{vmView}, 'layout.disk') ) )
			{
				my $diskLayoutInfo 		= Common_Utils::GetViewProperty( $args{vmView}, 'layout.disk');
				foreach ( @$diskLayoutInfo )
				{
					if ( $_->key != $key_file )
					{
						next;
					}
					my @baseDiskInfo = @{ $_->diskFile };
					foreach my $disk ( @baseDiskInfo )
					{
						if( $disk =~ /-[0-9]{6}\.vmdk$/)
						{
							next;
						}
						$disk_name = $disk;
						Common_Utils::WriteMessage("Base disk information collected for key( $key_file ) = $disk_name.",2);
					}
				}
			}
				
			my @splitDisk_Name 	= split( /\[|\]/ , $disk_name );
			my $scsiNumber 	= "";
			my $scsiUnitNum	= "";
			if( ($device->controllerKey >= 1000) && ($device->controllerKey <= 1004) )
			{
				$scsiNumber 	= $device->controllerKey - 1000;
				$scsiUnitNum	= ( $device->key - 2000 ) % 16;							
			}
			elsif( ($device->controllerKey >= 15000) && ($device->controllerKey <= 15004) )
			{
				$scsiNumber 	= $device->controllerKey - 15000 + 4;
				$scsiUnitNum	= ( $device->key - 16000 ) % 30;							
			}
						
			my $vmdkUuid		= $device->backing->uuid;
			if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
			{
				$vmdkUuid	= $device->backing->lunUuid;
			}
			
			if ( ( defined $detachDiskWithUuid ) && ( $detachDiskWithUuid ne "" ) && ( $vmdkUuid eq $detachDiskWithUuid ) )
			{
				Common_Utils::WriteMessage("Removing disk $disk_name from machine $machineName of size $device->{capacityInKB} at $scsiNumber:$scsiUnitNum based on Uuid.",2);
				my $diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
				$diskCountVm++;
				push @{ $args{configSpec} }	, $diskSpec;
				${$args{mapKeysOfVm}}{$key_file}= $inmageHostId;
				${$args{mapKeysOfVm}}{$vmdkUuid}= $machineName;
				last;
			}
			
			if ( ( ( !defined $detachDiskWithUuid ) || ( $detachDiskWithUuid eq "" ) ) && ( defined $detachDiskWithScsiID ) && ( "$scsiNumber:$scsiUnitNum" eq $detachDiskWithScsiID ) )
			{
				Common_Utils::WriteMessage("Removing disk $disk_name from machine $machineName of size $device->{capacityInKB} at $scsiNumber:$scsiUnitNum based on scsi id.",2);
				my $diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
				$diskCountVm++;
				push @{ $args{configSpec} }	, $diskSpec;
				${$args{mapKeysOfVm}}{$key_file}= $inmageHostId;
				${$args{mapKeysOfVm}}{$vmdkUuid}= $machineName;
				last;
			}
				
			if ( ( !defined $detachDiskWithScsiID ) && ( $splitDiskName[-1] eq $splitDisk_Name[-1] ) )
			{
				Common_Utils::WriteMessage("Removing disk $disk_name from machine $machineName of size $device->{capacityInKB} at unit number $key_file based on Disk name.",2);
				my $diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
				$diskCountVm++;
				push @{ $args{configSpec} }	, $diskSpec;
				${$args{mapKeysOfVm}}{$key_file}= $inmageHostId;
				${$args{mapKeysOfVm}}{$vmdkUuid}= $machineName;
				last;
			}
		}
	}
	
	if ( 0 == $diskCountVm )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find \"$displayName\"'s disks attached with master target.",3);
		Common_Utils::WriteMessage("ERROR :: 1. Check if any storage vMotion happened and disk names are changed?",3);
		Common_Utils::WriteMessage("ERROR :: 2. Are the disks removed manually from master target?",3);
		$ReturnCode		= $Common_Utils::FAILURE;
	}
	elsif( $diskCountXml != $diskCountVm )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find \"$displayName\"'s some of disks attached with master target.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
	}
	
	return ( $ReturnCode );
} 

#####ReadRollBackFile#####
##Description 		:: Reads Rollback file and collects all the recovered host names.
##Input 			:: Rollback file , OS type of machines under rollback, IN_OUT RecoveredHostNames.
##Output 			:: Returns SUCCESS or FAILURE.
#####ReadRollBackFile#####
sub ReadRollBackFile
{
	my $rollbackFile 		= shift;
	my $recoveredHostName	= shift;
	
	if ( !open( ROLLBACKFILE , $rollbackFile ) )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to open \"$rollbackFile\". please check whether user has permission to access it.",3);
		return ( $Common_Utils::FAILURE );
	} 
	
	my @lines 		= <ROLLBACKFILE>;
	close ROLLBACKFILE;
	foreach my $line ( @lines )
	{
		if($line =~ /^$/)
		{
		}
		else
		{
			$line =~ s/\n//;
			$line =~ s/\r//;
			$line =~ s/\s+//g; 
			my @line = split(";",$line);
			if($#line == 0)
			{
				Common_Utils::WriteMessage("$line",2);
				print "$line \n";
				$$recoveredHostName{$line}	= "Recovered VM";
				next;
			}
			my $uuid 		= "";
			my $hostName 	= "";
			foreach my $str (@line)
			{
				if( $str =~ /InMageHostId/i) 
				{
				 	$uuid = (split("\"",$str))[1];
				}
				if( $str =~ /HostName/i)
				{
					$hostName = (split("\"",$str))[1];
				}
			}
			if($uuid !~ /^$/)
			{
				$$recoveredHostName{$uuid} = "Recovered VM";
			}
			else
			{
				$$recoveredHostName{$hostName} = "Recovered VM";
			}
			Common_Utils::WriteMessage("ReadRollBackFile: $uuid : $hostName .",2);
			print  "$uuid : $hostName \n";
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####UpdateVm#####
##Description		:: Updates Network card labels, memory and CHS values.
##Input 			:: Guest machine view and host Node which contain all the details.
##Output 			:: Returns SUCCESS else FAILURE.
#####UpdateVm#####
sub UpdateVm
{
	my $vmView			= shift;
	my $hostNode		= shift;
	my $networkInfo		= shift;
	my $vSphereHostName = shift;
	
	my $queryDVPortGroup= 0;
	my $machineName		= $vmView->name;
	my $change 			= 0;
	my @VirtualDeviceConfigSpec 				= ();
	my $virtualMachineConfigSpec 				= VirtualMachineConfigSpec->new ();
	
	if( Common_Utils::GetViewProperty( $vmView, 'summary.config.guestFullName' ) =~ /^[\s]*$/ )
	{
		$change									= 1;
		$virtualMachineConfigSpec->{guestId} 	= $hostNode->getAttribute('alt_guest_name');
	}
					
	if( Common_Utils::GetViewProperty( $vmView, 'summary.config.memorySizeMB') != $hostNode->getAttribute('memsize') && $hostNode->getAttribute('memsize') ne "" )
	{
		$change									= 1;
		$virtualMachineConfigSpec->{memoryMB} 	= $hostNode->getAttribute('memsize');
	}
						
	if ( Common_Utils::GetViewProperty( $vmView, 'summary.config.numCpu') != $hostNode->getAttribute('cpucount') && $hostNode->getAttribute('cpucount') ne "" )
	{
		$change									= 1;
		$virtualMachineConfigSpec->{numCPUs} 	= $hostNode->getAttribute('cpucount');
		
		my $serviceContent	= Vim::get_service_content();
		my $apiVersion		= $serviceContent->about->apiVersion;
		if( $apiVersion ge "5.0")
		{
			$virtualMachineConfigSpec->{numCoresPerSocket}	= 1;
		}
	}
						
	my @Nic 									= $hostNode->getElementsByTagName('nic');
	foreach my $Nic( @Nic )
	{
		if( ( $Nic->getAttribute('new_network_name') ne "" ) )
		{
			my $devices 						= Common_Utils::GetViewProperty( $vmView, 'config.hardware.device');
			foreach my $device ( @$devices )
			{
				if( ( $device->deviceInfo->label =~ /^Network adapter /i ) && ( $device->deviceInfo->label eq $Nic->getAttribute('network_label') ) )
				{
					my $networkLabel			= $Nic->getAttribute('network_label');
					my $adapterType 			= ref($device);
					my $macAddress				= $Nic->getAttribute('new_macaddress');
					my $addressType 			= $device->addressType;
					
					if ( ( $macAddress ne $device->macAddress ) && ( "" ne $macAddress ) && ( $device->macAddress !~ /^00:0c:29/ ) )
					{
						$addressType			= "assigned";
						Common_Utils::WriteMessage("Mac address changed for network adapter $networkLabel of machine $machineName.",2);
					}
					
					if( ( $device->macAddress =~ /^00:0c:29/ ) || ( $macAddress =~ /^00:0c:29/ ) )
					{ 
						my $macAddressDev		= $device->macAddress;
						$addressType			= "generated";
						Common_Utils::WriteMessage("Mac Address is falling in the range of \"00:0c:29\"($macAddressDev), so forcing addressType to \"generated\".",2);
					}
					
					if (  "" eq $macAddress )
					{
						Common_Utils::WriteMessage("Received empty mac address for network adapter $networkLabel of machine $machineName.",2);
						$macAddress				= "";
						$addressType 	 		= "generated";
					}
					
					my $backing_info			= "";
					if( $Nic->getAttribute('new_network_name') =~ /\[DVPG\]$/i )
					{
						my $newPortGroup		= $Nic->getAttribute('new_network_name');
						my ($portgroupKey , $switchUuid ) 	= ("","");
						
						foreach my $portGroupInfo ( @$networkInfo )
						{							
							if ( $$portGroupInfo{vSphereHostName} ne $vSphereHostName )
							{
								next;
							}
							
							if ( $$portGroupInfo{portgroupName} ne $Nic->getAttribute('new_network_name') )
							{
								next;
							}
							$portgroupKey		= $$portGroupInfo{portgroupKey};
							$switchUuid			= $$portGroupInfo{switchUuid};
						}

						Common_Utils::WriteMessage("PortGroupKey = $portgroupKey and UUID = $switchUuid.",2);
						if ( ( "" eq $portgroupKey ) || ( "" eq $switchUuid ) )
						{
							Common_Utils::WriteMessage("ERROR :: Either portgroup key or switchUuid values are empty for port group $newPortGroup.",2);
							$backing_info 		= VirtualEthernetCardNetworkBackingInfo->new( deviceName => $Nic->getAttribute('new_network_name') );
						}
						else
						{
							my $DVPG			= DistributedVirtualSwitchPortConnection->new( portgroupKey => $portgroupKey , switchUuid => $switchUuid );
							$backing_info 		= VirtualEthernetCardDistributedVirtualPortBackingInfo->new( port=> $DVPG );
						}
					}
					else
					{
						$backing_info 			= VirtualEthernetCardNetworkBackingInfo->new( deviceName => $Nic->getAttribute('new_network_name') );
					}
					Common_Utils::WriteMessage("$networkLabel : addressType = $addressType , macAddress = $macAddress.",2);
					my $key 					= $device->key;
					my $VirtualDevice			= $adapterType->new( key => $key , backing => $backing_info , macAddress => $macAddress , addressType => $addressType );
					push @VirtualDeviceConfigSpec , VirtualDeviceConfigSpec->new( device => $VirtualDevice , operation => VirtualDeviceConfigSpecOperation->new('edit'));
				}
			}
		}			
	}
	
	if( $#VirtualDeviceConfigSpec != -1 )
	{
		$change 								  	= 1;
		$virtualMachineConfigSpec->{deviceChange}   = [@VirtualDeviceConfigSpec]; 		
	}
							
	if( $change )
	{
		for( my $i = 0 ; ; $i++ )
		{
			eval
			{
				$vmView->ReconfigVM( spec => $virtualMachineConfigSpec );
			};
			if($@)
			{
				if( ( ref($@->detail) eq 'TaskInProgress' ) and ($i < 3) )
				{
					sleep(5);
					next;
				}
				Common_Utils::WriteMessage("ERROR :: Unable to update Memory,CPU and Network changes for machine $machineName.",3);
				Common_Utils::WriteMessage("ERROR :: $@.",3);
				return ( $Common_Utils::FAILURE );
			}
			last;
		}
	}
	else
	{
		Common_Utils::WriteMessage("There are no changes for Machine $machineName.",2);
	}	

	return ( $Common_Utils::SUCCESS );
} 

#####UpdateChsValues#####
##Description 		:: Updates CHS values for disks on which updatechsvalue is set to yes.
##Input 			:: HostNode.
##Output 			:: Returns SUCCESS or FAILURE.
#####UpdateChsValues#####
sub UpdateChsValues
{
	my $hostNode		= shift;
	my $datacenterName	= "";
	my $displayName 	= $hostNode->getAttribute('display_name');
	my @disksInfo		= $hostNode->getElementsByTagName('disk');
	
	foreach my $disk ( @disksInfo )
	{
		if ( ( defined( $disk->getAttribute('updatechs') ) && ( $disk->getAttribute('updatechs') =~ /^yes$/i ) )
			|| ( defined( $disk->getAttribute('updateCHS') ) && ( $disk->getAttribute('updateCHS') =~ /^yes$/i ) ) )
		{ 
		}
		else
		{
			my $diskName = $disk->getAttribute('disk_name');
			if ( defined $disk->getAttribute('updatechs') )
			{
				my $updatechsValue 	= $disk->getAttribute('updatechs');
				Common_Utils::WriteMessage("Update chs value for disk $diskName = $updatechsValue.",2);
			}
			
			if ( defined $disk->getAttribute('updateCHS') )
			{
				my $updatechsValue 	= $disk->getAttribute('updateCHS');
				Common_Utils::WriteMessage("Update CHS value for disk $diskName = $updatechsValue.",2);
			}
			
			Common_Utils::WriteMessage("Skipping update of chs values for disk $diskName of machine $displayName.",2);
			next;
		}
		
		if ( "" eq $datacenterName )
		{
			my $vmUuid					= $hostNode->getAttribute('target_uuid');
			my ( $returnCode , $vmView )= VmOpts::GetVmViewByUuid( $vmUuid );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			if ( defined $vmView->parent )
			{
				my $parentView 			= $vmView;
				while( $parentView->parent->type !~ /Datacenter/i )
				{	   
					$parentView 		= Vim::get_view( mo_ref => ($parentView->parent), properties => ["parent"] );
			   }
			   
			   $datacenterName			= Vim::get_view( mo_ref => ($parentView->parent), properties => ["name"] )->name;
			}
			elsif( defined $vmView->parentVApp )
			{
				my $parentView			= $vmView;
				$parentView 			= Vim::get_view( mo_ref => ($parentView->parentVApp), properties => ["parent"] );
				while( $parentView->parent->type !~ /Datacenter/i )
				{	   
					$parentView = Vim::get_view( mo_ref => ($parentView->parent), properties => ["parent"] );
			    }
			    $datacenterName 		= Vim::get_view( mo_ref => ($parentView->parent), properties => ["name"] )->name;
			}
			
			if( ( "" eq  $datacenterName ) )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to determine in which datacenter machine with uuid = $vmUuid exists.",3);
				return ( $Common_Utils::FAILURE );
			}
		}
		
		my $OrigFileName		= $disk->getAttribute('disk_name');
		my $dataStoreSelected 	= $disk->getAttribute('datastore_selected');
		my @origFileNameSplit 	= split( /\[|\]/,$OrigFileName );
		$origFileNameSplit[2]	=~ s/^\s+//;
		my $fileName 			= "[$dataStoreSelected] $origFileNameSplit[2]";
		if ( ( defined( $hostNode->getAttribute('vmDirectoryPath') ) ) && ( "" ne $hostNode->getAttribute('vmDirectoryPath') ) )
		{
			$fileName 			= "[$dataStoreSelected] ".$hostNode->getAttribute('vmDirectoryPath')."/".$origFileNameSplit[2];
		}
		Common_Utils::WriteMessage("Changing CHS values for disk \"$fileName\".",2);
		
		if( $Common_Utils::SUCCESS != DatastoreOpts::do_get( $fileName, $Common_Utils::vContinuumMetaFilesPath , $datacenterName ) )
		{
			return ( $Common_Utils::FAILURE );
		}
	
		my $last_slash_index 	= rindex( $fileName , "/" );
		my $vmdk_name 			= substr( $fileName , $last_slash_index+1 , length( $fileName ) );
		if ( ! open( FR,"<$Common_Utils::vContinuumMetaFilesPath/$vmdk_name" ) )
		{
			Common_Utils::WriteMessage("ERROR :: Unable to open file \"$vmdk_name\" for reading.Please check for the permissions on the file.",3);
			return ( $Common_Utils::FAILURE );
		}

		if ( ! open( FW, ">$Common_Utils::vContinuumMetaFilesPath/$vmdk_name.temp" ) )
		{	
			Common_Utils::WriteMessage("ERROR :: Unable to open file \"$vmdk_name\" for Writing.Please check for the permissions on the file.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		while(<FR>)
		{
			my @line_split 		= split("=", $_);
			if( $line_split[0] =~ /geometry.heads\s+$/ )
			{
				my $headCount	= $disk->getAttribute('heads');
				Common_Utils::WriteMessage("Updating the heads count to $headCount.",2);
				print FW "$line_split[0] = \"$headCount\"\n";
			}
			elsif($line_split[0] =~ /geometry.sectors\s+$/)
			{
				my $sectorsCount= $disk->getAttribute('sectors'); 
				Common_Utils::WriteMessage("Updating the sector count to $sectorsCount.",2);
				print FW "$line_split[0] =  \"$sectorsCount\"\n";
			}
			else
			{
				print FW $_;
			}
		}
		close FR;
		close FW;
		unlink "$Common_Utils::vContinuumMetaFilesPath/$vmdk_name";
		rename "$Common_Utils::vContinuumMetaFilesPath/$vmdk_name.temp","$Common_Utils::vContinuumMetaFilesPath/$vmdk_name";
		sleep(1);

		if( $Common_Utils::SUCCESS != DatastoreOpts::do_put( "$Common_Utils::vContinuumMetaFilesPath/$vmdk_name", $fileName , $datacenterName ) )
		{
			return ( $Common_Utils::FAILURE );
		}
	
		unlink "$Common_Utils::vContinuumMetaFilesPath/$vmdk_name";
	}
	return ( $Common_Utils::SUCCESS );
}

#####PreRecoveryOperations#####
##Description 		:: Performs pre-recovery operations such as updating the network cards, memory and CPU values, in case of physical
##						machine it also updates CHS value of the disk.
##Input				:: Host Node.
##Output 			:: Returns SUCCESS or FAILURE.
#####PreRecoveryOperations#####
sub PreRecoveryOperations
{
	my %args			= @_;
	my $hostNode		= $args{sourceHost};
	my $networkInfo		= $args{networkInfo};
	my $tgtNode			= $args{tgtNode};	
	my $ReturnCode		= $Common_Utils::SUCCESS;	
	my $vmUuid			= $hostNode->getAttribute('target_uuid');
	
	my ( $returnCode , $vmView )			= VmOpts::GetVmViewByUuid( $vmUuid );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	my $recVmName			= $vmView->name;
	my $vSphereHostName		= $hostNode->getAttribute('targetvSphereHostName');
	Common_Utils::WriteMessage("vSphere HostName of recovered vm \"$recVmName\" is \"$vSphereHostName\".",2);
		
	if ( $Common_Utils::SUCCESS ne UpdateVm( $vmView , $hostNode , $networkInfo , $vSphereHostName ) ) 
	{
		$ReturnCode	= $Common_Utils::FAILURE;
	}
	
	if ( "physicalmachine" ne lc( $hostNode->getAttribute('machinetype') ) )
	{
		return ( $ReturnCode );
	}
	
	if( ( $tgtNode->getAttribute('hostversion') eq "5.1.0" ) && ( lc($hostNode->getAttribute('operatingsystem')) eq "windows_2012" ) && 
			( Common_Utils::GetViewProperty( $vmView,'summary.config.guestId') eq "windows8Server64Guest" ) && 
			( lc(Common_Utils::GetViewProperty( $vmView,'config.version')) lt "vmx-08" ) )
	{
		Common_Utils::WriteMessage("upgrading vm is required for vm \"$recVmName\".",2);
		push @{ $args{upGradeVms} }, $vmView;
	}

	if ( $Common_Utils::SUCCESS ne UpdateChsValues( $hostNode ) )
	{
		$ReturnCode	= $Common_Utils::FAILURE;
	}
	
	return ( $ReturnCode );
}

#####Remove#####
##Description		:: It detaches disks of these machine from MT.
##Input 			:: XML file which contains information of the machines which are to be removed.
##Ouput				:: Returns SUCCESS else FAILURE.
#####Remove#####
sub Remove
{
	my %args 							= @_;
	my $ReturnCode						= $Common_Utils::SUCCESS;
	my $recoveryXmlPath 				= "$args{directory}/$args{file}";

	if ( ! -f $recoveryXmlPath )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find Recovery.xml file in path $args{directory}.",3);
		Common_Utils::WriteMessage("ERROR :: Please check whether above file exists or not.",3);
		return ( $Common_Utils::FAILURE );
	}

	my ( $returnCode , $docNode ) 	= XmlFunctions::GetPlanNodes( $recoveryXmlPath );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get plan information from file \"$recoveryXmlPath\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my $planNodes 						= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 						= [$docNode];
	}
	
	my @RecoveredVirtualMachineInfo		= ();
	foreach my $plan ( @$planNodes )
	{
		my $planName					= $plan->getAttribute('plan');
		my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetInfo					= $targetHost[0]->getElementsByTagName('host');		
		my @sourceInfo					= $plan->getElementsByTagName('SRC_ESX');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		
		if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
		{
			$returnCode 				= VmOpts::PowerOff( $targetInfo[0]->getAttribute('source_uuid') );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
		}
		
		my ( $returnCode , $mtView ) 	= VmOpts::GetVmViewByUuid( $targetInfo[0]->getAttribute('source_uuid') );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Unable to find view of master target.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		my @diskRemovalSpecs= ();
		my %mapKeyAndVm		= ();
		foreach my $sourceInfo	( @sourceInfo )
		{
			my @hostsInfo				= $sourceInfo->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				if ( "yes" eq lc( $host->getAttribute('failover') ) )
				{
					next;
				}
				
				my %RecoveredVmInfo				= ();
				$RecoveredVmInfo{hostName}		= $host->getAttribute('hostname');
				$RecoveredVmInfo{inmage_hostid}	= $host->getAttribute('inmage_hostid');				
				push @RecoveredVirtualMachineInfo , \%RecoveredVmInfo;
				
				$returnCode 			= RemoveVmdkFromMT( sourceHost => $host, vmView => $mtView, configSpec => \@diskRemovalSpecs, mapKeysOfVm => \%mapKeyAndVm );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
					$ReturnCode			= $Common_Utils::FAILURE;
					next;
				}
			}
		}
		$returnCode	= DetachVmdksFromMT( vmView => $mtView, 
										configSpec => \@diskRemovalSpecs, 
										mapKeysOfVm => \%mapKeyAndVm,
										recVmInfo	=> \@RecoveredVirtualMachineInfo,
										xmlFilePath => $recoveryXmlPath,
										);
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$ReturnCode					= $Common_Utils::FAILURE;
		}
			
		( $returnCode, $mtView )= VmOpts::UpdateVmView( $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
		}
		
		if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
		{
			$returnCode 				= VmOpts::PowerOn( $targetInfo[0]->getAttribute('source_uuid') );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
		}
	}
	
	return ( $ReturnCode );	
}

#####DrDrill#####
##Description 		:: Recovery Wrok Flow after in DR-Drill.
##Input 			:: Recovery file Name and directory path to recovery file.
##Output 			:: Returns SUCCESS else FAILURE.
#####DrDrill#####
sub DrDrill
{
	my %args 							= @_;
	my $ReturnCode						= $Common_Utils::SUCCESS;
	my $recoveryXmlPath 				= "$args{directory}/$args{file}";
	my @tasksInfo		 				= ();
	my $mtHostId 						= "";
	my $osType 							= "";
	
	$Common_Utils::taskInfo{TaskName} 	= "Powering on the dr drill VM(s)";
	$Common_Utils::taskInfo{TaskStatus}	= "Completed";
	
	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{
		if ( ! -f $recoveryXmlPath )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find $args{file} file in path $args{directory}.",3);
			Common_Utils::WriteMessage("ERROR :: Please check whether above file exists or not.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find $args{file} file in path $args{directory}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file $args{file} exists or not.\n";
			last;
		}
		
		my( $returnCode , $docNode ) 		= XmlFunctions::GetPlanNodes( $recoveryXmlPath );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to get snapshot plan information from file \"$recoveryXmlPath\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to get snapshot plan information from file $recoveryXmlPath.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file $recoveryXmlPath exits and have permissions to access it.\n";
			last;
		}
		
		my $planNodes 						= $docNode->getElementsByTagName('root');
		if ( "array" ne lc ( ref( $planNodes ) ) )
		{
			$planNodes 						= [$docNode];
		}
		
		foreach my $plan ( @$planNodes )
		{
			my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
			my @targetInfo					= $targetHost[0]->getElementsByTagName('host');
			$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
			$mtHostId						= $targetInfo[0]->getAttribute('inmage_hostid');
		}
		
		( $returnCode , my $cxIpPortNum )	= Common_Utils::FindCXIP();
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find CX IP from registry.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether unified agent is installed and registry has CXIP.\n";
			last;
		}
		
		my $rCode 	= Common_Utils::DownloadFilesFromCX(	'file' 				=> "input.txt",
															'filePath'			=> "$args{cxpath}",
															'downloadSubFiles' 	=> "yes",
															'dirPath'			=> "$args{directory}",
															'cxIp'				=> "$Common_Utils::CxIp",
							 							);
		if ( $rCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to download \"input.txt\" from CX or subfiles listed in it.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to download file \"input.txt\" from CX.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file \"input.txt\" exists or not in CX under path.\n";
			#$ReturnCode = $Common_Utils::FAILURE;
			#last;
		}
		
		my $rollbackFile					= "$args{directory}/InMage_Recovered_Vms.snapshot";
		Common_Utils::WriteMessage("Snapshot file = $rollbackFile.",2);
		if ( ! -f $rollbackFile )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find snapshot file in directory $args{directory}.",3);
			Common_Utils::WriteMessage("ERROR :: Please check whether the snapshot process completed successfully or not.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find snapshot file in directory $args{directory}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether snapshot process completed successfully or not.\n";
			last;
		}
		
		my %recoveredHostName				= ();
		$returnCode							= ReadRollBackFile( $rollbackFile , \%recoveredHostName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to read snapshot file.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to read snapshot file.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check permisssions on file $rollbackFile.\n";
			last;
		}
					
		my @RecoveredVirtualMachineInfo		= ();
		foreach my $plan ( @$planNodes )
		{
			my $planName					= $plan->getAttribute('plan');
			my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
			my @targetInfo					= $targetHost[0]->getElementsByTagName('host');		
			my @sourceInfo					= $plan->getElementsByTagName('SRC_ESX');
			$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
			$mtHostId						= $targetInfo[0]->getAttribute('inmage_hostid');
						
			my ( $returnCode , $mtView ) = VmOpts::GetVmViewByUuid( $targetInfo[0]->getAttribute('source_uuid') );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$ReturnCode = $Common_Utils::FAILURE;
				$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
				$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find view of master target.\n";
				$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether 1. master target exists?.\n";
				last;
			}
			
			my $networkInfo		= XmlFunctions::ReadNetworkDataFromXML( $targetInfo[0] );
			
			if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
			{
				$returnCode 				= VmOpts::PowerOff( $mtView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$ReturnCode = $Common_Utils::FAILURE;
					$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
					$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to power off master target.Could not detach disk from master target.\n";
					last;
				}
			}			
			
			my @diskRemovalSpecs= ();
			my %mapKeyAndVm		= ();
			my @upGradeVmxVms	= ();
			foreach my $sourceInfo	( @sourceInfo )
			{
				my @hostsInfo				= $sourceInfo->getElementsByTagName('host');
				foreach my $host ( @hostsInfo )
				{
					my $hostName			= $host->getAttribute('hostname');
					my $InMageHostId		= $host->getAttribute('inmage_hostid');
					Common_Utils::WriteMessage("Checking for HostName $hostName .",2);
					if ( !((defined $InMageHostId && $InMageHostId !~ /^$/ && exists $recoveredHostName{$InMageHostId} ) || (exists $recoveredHostName{$hostName}))) 
					{
						$ReturnCode = $Common_Utils::FAILURE;
						Common_Utils::WriteMessage("ERROR :: Snapshot process failed for machine \"$hostName\" .",3);
						$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Snapshot process failed for machine $hostName.\n";
						$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether snapshot operation is succeeded and volume replication are removed?.\n";
						next;
					}
					
					if ( $Common_Utils::SUCCESS != PreRecoveryOperations( sourceHost => $host, networkInfo => $networkInfo, tgtNode => $targetInfo[0], upGradeVms => \@upGradeVmxVms ) ) 
					{
						$ReturnCode	= $Common_Utils::FAILURE;
						$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Network related changes are failed for machine $hostName.\n";
					}
					
					my %RecoveredVmInfo				= ();
					$RecoveredVmInfo{planName}		= $host->getAttribute('plan');
					$RecoveredVmInfo{uuid}			= $host->getAttribute('target_uuid');
					$RecoveredVmInfo{recovered}		= "yes";
					$RecoveredVmInfo{recOrder}		= $host->getAttribute('recoveryOrder');
					$RecoveredVmInfo{hostName}		= $host->getAttribute('hostname');
					$RecoveredVmInfo{machineType}	= $host->getAttribute('machinetype');
					$RecoveredVmInfo{mtHostName}	= $host->getAttribute('mastertargethostname');
					$RecoveredVmInfo{vConName}		= $host->getAttribute('vconname');
					$RecoveredVmInfo{inmage_hostid}	= $host->getAttribute('inmage_hostid');
					$RecoveredVmInfo{powerOnVM}		= $host->getAttribute('power_on_vm');
					$RecoveredVmInfo{mtInmageHostId}= $mtHostId;
					$osType							= $host->getAttribute('os_info');
						
					$returnCode 					= RemoveVmdkFromMT( sourceHost => $host, vmView => $mtView, configSpec => \@diskRemovalSpecs, mapKeysOfVm => \%mapKeyAndVm );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						$RecoveredVmInfo{recovered}		= "no";
						push @RecoveredVirtualMachineInfo , \%RecoveredVmInfo;
						$ReturnCode					= $Common_Utils::FAILURE;
						$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find disks of machine $hostName attached with master target.\n";
						$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether 1. SCSI information for each of the disk of host $hostName matches with date in file $recoveryXmlPath.\n2.Make sure all disks of this host are found on master target.\n";
						next;
					}
					push @RecoveredVirtualMachineInfo , \%RecoveredVmInfo;
				}
			}
			
			$returnCode	= DetachVmdksFromMT( vmView => $mtView, 
											 configSpec => \@diskRemovalSpecs, 
											 mapKeysOfVm => \%mapKeyAndVm,
											 recVmInfo	=> \@RecoveredVirtualMachineInfo,
											 xmlFilePath => $recoveryXmlPath,
											);
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$ReturnCode					= $Common_Utils::FAILURE;
			}
			
			if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
			{
				$returnCode 	= VmOpts::PowerOn( $mtView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$ReturnCode	= $Common_Utils::FAILURE;
					$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
					$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to bring up master target.\n";
				}
			}
		}
		
		$returnCode		= PostRecoveryOperations( \@RecoveredVirtualMachineInfo );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$ReturnCode	= $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to bring up snapshot machines.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check 1. Machines are powered on?\n2.OS is loaded on not?.\n";
		}
	}
	
	push @tasksInfo , \%Common_Utils::taskInfo;
	  
	my $returnCode	= UpdateTaskStatusToCx( hostId => $mtHostId , tasksInfo => \@tasksInfo, operation => "drdrill" );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		$ReturnCode	= $Common_Utils::FAILURE;
	}
	
	return ( $ReturnCode );	
}

#####Recovery#####
##Description 		:: performs detaching of disk for each of recovered vm.
##Input 			:: Recovery file Name and directory path to recovery file.
##Output 			:: Returns SUCCESS else FAILURE.
#####Recovery#####
sub Recovery
{
	my %args 							= @_;
	my $ReturnCode						= $Common_Utils::SUCCESS;
	my $recoveryXmlPath 				= "$args{directory}/$args{file}";
	my @tasksInfo		 				= ();
	my $mtHostId 						= "";

	$Common_Utils::taskInfo{TaskName} 	= "Powering on the recovered VM(s)";
	$Common_Utils::taskInfo{TaskStatus}	= "Completed";
	
	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{
		if ( ! -f $recoveryXmlPath )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find $args{file} file in path $args{directory}.",3);
			Common_Utils::WriteMessage("ERROR :: Please check whether above file exists or not.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find $args{file} file in path $args{directory}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file $args{file} exists or not.\n";
			last;
		}
		
		my( $returnCode , $docNode ) 		= XmlFunctions::GetPlanNodes( $recoveryXmlPath );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to get recovery plan information from file \"$recoveryXmlPath\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to get recovery plan information from file $recoveryXmlPath.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file $recoveryXmlPath exits and have permissions to access it.\n";
			last;
		}
		
		my $planNodes 						= $docNode->getElementsByTagName('root');
		if ( "array" ne lc ( ref( $planNodes ) ) )
		{
			$planNodes 						= [$docNode];
		}
		
		foreach my $plan ( @$planNodes )
		{
			my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
			my @targetInfo					= $targetHost[0]->getElementsByTagName('host');
			$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
			$mtHostId						= $targetInfo[0]->getAttribute('inmage_hostid');
		}
		
		( $returnCode , my $cxIpPortNum )	= Common_Utils::FindCXIP();
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find CX IP from registry.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether unified agent is installed and registry has CXIP.\n";
			last;
		}
		
		if ( ( !defined $args{cxpath} ) || ( "" eq $args{cxpath} ) )
		{
			Common_Utils::WriteMessage("ERROR :: cxpath is not defined.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "cxpath is not defined.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check the command, send cx path also.\n";
			last;	
		}
		
		my $rCode 	= Common_Utils::DownloadFilesFromCX(	'file' 				=> "input.txt",
															'filePath'			=> "$args{cxpath}",
															'downloadSubFiles' 	=> "yes",
															'dirPath'			=> "$args{directory}",
															'cxIp'				=> "$Common_Utils::CxIp",
							 							);
		if ( $rCode != $Common_Utils::SUCCESS )
		{
			WriteMessage("ERROR :: Failed to download \"input.txt\" from CX or subfiles listed in it.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to download file \"input.txt\" from CX.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file \"input.txt\" exists or not in CX under path.\n";
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		my $rollbackFile					= "$args{directory}/InMage_Recovered_Vms.rollback";
		Common_Utils::WriteMessage("Rollback file = $rollbackFile.",2);
		if ( ! -f $rollbackFile )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find Rollback file in directory $args{directory}.",3);
			Common_Utils::WriteMessage("ERROR :: Please check whether the rollback process completed successfully or not.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find Rollback file in directory $args{directory}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether the rollback process completed successfully or not.\n";
			last;
		}
		
		my %recoveredHostName				= ();
		$returnCode							= ReadRollBackFile( $rollbackFile , \%recoveredHostName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to read rollback file.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to read rollback file.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check permisssions on file $rollbackFile.\n";
			last;
		}
		
		my %recoveredVMs	= ();
		my $powerOnStatusFile				= "$args{directory}/Power_On_Vms.status";
		if ( -f $powerOnStatusFile )
		{
			$returnCode						= ReadRollBackFile( $powerOnStatusFile , \%recoveredVMs );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				#don't need block for this failures				
			}			
		}
						
		my @RecoveredVirtualMachineInfo		= ();
		foreach my $plan ( @$planNodes )
		{
			my $planName					= $plan->getAttribute('plan');
			my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
			my @targetInfo					= $targetHost[0]->getElementsByTagName('host');		
			my @sourceInfo					= $plan->getElementsByTagName('SRC_ESX');
			$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
			$mtHostId						= $targetInfo[0]->getAttribute('inmage_hostid');
			
			my ( $returnCode , $mtView ) = VmOpts::GetVmViewByUuid( $targetInfo[0]->getAttribute('source_uuid') );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$ReturnCode = $Common_Utils::FAILURE;
				$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
				$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find view of master target.\n";
				$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether 1. master target exists?.\n";
				$Common_Utils::taskInfo{$mtHostId}{status}	= "Failed";
				$Common_Utils::taskInfo{$mtHostId}{error}	.= "Failed to find view of master target.";
				last;
			}
			
			my $networkInfo		= XmlFunctions::ReadNetworkDataFromXML( $targetInfo[0] );
			
			if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
			{
				$returnCode 				= VmOpts::PowerOff( $mtView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$ReturnCode = $Common_Utils::FAILURE;
					$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
					$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to power off master target.Could not detach disk from master target.\n";
					$Common_Utils::taskInfo{$mtHostId}{status}	= "Failed";
					$Common_Utils::taskInfo{$mtHostId}{error}	.= "Failed to power off master target.Could not detach disk from master target.";
					last;
				}
			}
			
			my @diskRemovalSpecs= ();
			my %mapKeyAndVm		= ();
			my @upGradeVmxVms	= ();
			foreach my $sourceInfo	( @sourceInfo )
			{
				my @hostsInfo				= $sourceInfo->getElementsByTagName('host');
				foreach my $host ( @hostsInfo )
				{
					my $hostName			= $host->getAttribute('hostname');
					my $InMageHostId		= $host->getAttribute('inmage_hostid');
					Common_Utils::WriteMessage("Checking for HostName $hostName .",2);
					$Common_Utils::taskInfo{$InMageHostId}{status}	= "Success";
					$Common_Utils::taskInfo{$InMageHostId}{hostName}= $hostName;
					
					if( defined $recoveredVMs{$InMageHostId} )
					{
						Common_Utils::WriteMessage("Recovery already completed for machine \"$hostName\".",2);
						next;
					}
					
					if ( !((defined $InMageHostId && $InMageHostId !~ /^$/ && exists $recoveredHostName{$InMageHostId} ) || (exists $recoveredHostName{$hostName}))) 
					{
						$ReturnCode = $Common_Utils::FAILURE;
						Common_Utils::WriteMessage("ERROR :: Rollback process failed for machine \"$hostName\" .",3);
						$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Rollback process failed for machine $hostName.\n";
						$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether rollback operation is succeeded and volume replication are removed?.\n";
						$Common_Utils::taskInfo{$InMageHostId}{status}	= "Failed";
						$Common_Utils::taskInfo{$InMageHostId}{error}	.= "Rollback process failed for machine $hostName.";
						next;
					}
					
					if ( $Common_Utils::SUCCESS != PreRecoveryOperations( sourceHost => $host, networkInfo => $networkInfo, tgtNode => $targetInfo[0], upGradeVms => \@upGradeVmxVms ) ) 
					{
						$ReturnCode	= $Common_Utils::FAILURE;
						$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Network related changes are failed for machine $hostName.\n";
						$Common_Utils::taskInfo{$InMageHostId}{status}	= "Failed";
						$Common_Utils::taskInfo{$InMageHostId}{error}	.= "Network related changes are failed for machine $hostName.";
					}
					
					my %RecoveredVmInfo				= ();
					$RecoveredVmInfo{planName}		= $host->getAttribute('plan');
					$RecoveredVmInfo{uuid}			= $host->getAttribute('target_uuid');
					$RecoveredVmInfo{recovered}		= "yes";
					$RecoveredVmInfo{recOrder}		= $host->getAttribute('recoveryOrder');
					$RecoveredVmInfo{hostName}		= $host->getAttribute('hostname');
					$RecoveredVmInfo{machineType}	= $host->getAttribute('machinetype');
					$RecoveredVmInfo{mtHostName}	= $host->getAttribute('mastertargethostname');
					$RecoveredVmInfo{vConName}		= $host->getAttribute('vconname');
					$RecoveredVmInfo{inmage_hostid}	= $host->getAttribute('inmage_hostid');
					$RecoveredVmInfo{powerOnVM}		= $host->getAttribute('power_on_vm');
					$RecoveredVmInfo{mtInmageHostId}= $mtHostId;
						
					$returnCode 					= RemoveVmdkFromMT( sourceHost => $host, vmView => $mtView, configSpec => \@diskRemovalSpecs, mapKeysOfVm => \%mapKeyAndVm );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						$RecoveredVmInfo{recovered}		= "no";
						push @RecoveredVirtualMachineInfo , \%RecoveredVmInfo;
						$ReturnCode					= $Common_Utils::FAILURE;
						$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find disks of machine $hostName attached with master target.\n";
						$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether 1. SCSI information for each of the disk of host $hostName matches with date in file $recoveryXmlPath.\n2.Make sure all disks of this host are found on master target.\n";
						$Common_Utils::taskInfo{$InMageHostId}{status}	= "Failed";
						$Common_Utils::taskInfo{$InMageHostId}{error}	.= "Failed to find disks of machine $hostName attached with master target.";
						next;
					}
					push @RecoveredVirtualMachineInfo , \%RecoveredVmInfo;
				}
			}
			
			if(-1 == $#RecoveredVirtualMachineInfo )
			{
				Common_Utils::WriteMessage("No VMs found for power-on.",2);
				if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
				{
					$returnCode 	= VmOpts::PowerOn( $mtView );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						$ReturnCode	= $Common_Utils::FAILURE;
						$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to bring up master target.\n";
						$Common_Utils::taskInfo{$mtHostId}{status}	= "Failed";
						$Common_Utils::taskInfo{$mtHostId}{error}	.= "Failed to bring up master target.";
					}
				}
				
				push @tasksInfo , \%Common_Utils::taskInfo;

				my $returnCode	= UpdateTaskStatusToCx( hostId => $mtHostId , tasksInfo => \@tasksInfo, operation => "recovery", directory => $args{directory} );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$ReturnCode	= $Common_Utils::FAILURE;
				}
				return ( $ReturnCode );				
			}			
			
			$returnCode	= DetachVmdksFromMT( vmView => $mtView, 
											 configSpec => \@diskRemovalSpecs, 
											 mapKeysOfVm => \%mapKeyAndVm,
											 recVmInfo	=> \@RecoveredVirtualMachineInfo,
											 xmlFilePath => $recoveryXmlPath,
											);
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$ReturnCode					= $Common_Utils::FAILURE;
			}
			
			if ( 0 == open( FH ,">>$powerOnStatusFile") )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to write into $powerOnStatusFile file.",3);
				$ReturnCode	= $Common_Utils::FAILURE;
				$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
				$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to write into $powerOnStatusFile file. Manually prepare the file.\n";
				$Common_Utils::taskInfo{$mtHostId}{status}	= "Failed";
				$Common_Utils::taskInfo{$mtHostId}{error}	.= "Failed to write into $powerOnStatusFile file.";
			}
		
			foreach my $sourceInfo	( @sourceInfo )
			{
				my @hostsInfo				= $sourceInfo->getElementsByTagName('host');
				foreach my $host ( @hostsInfo )
				{
					foreach my $recVmInfo ( @RecoveredVirtualMachineInfo )
					{
						if( ($$recVmInfo{inmage_hostid} eq $host->getAttribute('inmage_hostid')) && ( $$recVmInfo{recovered} eq "yes" ))
						{
							if ( "yes" eq lc( $host->getAttribute('failover') ) )
							{
								$host->setAttribute('failback',"yes");
							}
							$host->setAttribute('failover',"yes");
							print FH "$$recVmInfo{inmage_hostid}\n";							
						}
					}
				}
			}
			
			eval
			{
				close FH;
			};
					
			if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
			{
				$returnCode 	= VmOpts::PowerOn( $mtView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$ReturnCode	= $Common_Utils::FAILURE;
					$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
					$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to bring up master target.\n";
					$Common_Utils::taskInfo{$mtHostId}{status}	= "Failed";
					$Common_Utils::taskInfo{$mtHostId}{error}	.= "Failed to bring up master target.";
				}
			}
			
			if( 0 <= $#upGradeVmxVms )
			{
				$returnCode 	= VmOpts::UpgradeVms( \@upGradeVmxVms );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$ReturnCode	= $Common_Utils::FAILURE;
					$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
					$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to upgrade some of vms , power on may failed.\n";
					$Common_Utils::taskInfo{FixSteps}		.= "->Manually upgrade vm and power on .\n";
				}								
			}
		}
		
		my @docElements = $docNode;
		$returnCode		= XmlFunctions::SaveXML( \@docElements , $recoveryXmlPath );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to modify $args{file} file.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to update recovery status in file $args{file}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please update failover or failback parameter for successfully recovered machines in Esx_Master.xml file in CX.\n";
			$Common_Utils::taskInfo{$mtHostId}{status}	= "Failed";
			$Common_Utils::taskInfo{$mtHostId}{error}	.= "Failed to update recovery status in file $args{file}.";
		}
		
		if( $Common_Utils::SUCCESS != DeleteJobs( \@RecoveredVirtualMachineInfo ) )
		{
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to remove Protection/Consistency jobs for recovered machines.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please remove jobs manually from CX.\n";
			Common_Utils::WriteMessage("ERROR :: Failed to remove Protection/Consistency jobs for recovered machines.",3);
			Common_Utils::WriteMessage("ERROR :: Either delete jobs manually or leave them intact.",3);
			$Common_Utils::taskInfo{$mtHostId}{status}	= "Failed";
			$Common_Utils::taskInfo{$mtHostId}{error}	.= "Failed to remove Protection/Consistency jobs for recovered machines.";
		}
		
		$returnCode		= PostRecoveryOperations( \@RecoveredVirtualMachineInfo );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$ReturnCode	= $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to bring up recovered machines.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check 1. Machines are powered on?\n2.OS is loaded on not?.\n";
		}
		
		if ( $Common_Utils::SUCCESS != XmlFunctions::UpdateMasterXmlCX( fileName => $recoveryXmlPath ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update master xml.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to update recovery status to CX.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please update failover or failback parameter for successfully recovered machines in Esx_Master.xml file in CX.\n";
			$Common_Utils::taskInfo{$mtHostId}{status}	= "Failed";
			$Common_Utils::taskInfo{$mtHostId}{error}	.= "Failed to update recovery status to CX.";
			$ReturnCode	= $Common_Utils::FAILURE;
		}
	}
	
	push @tasksInfo , \%Common_Utils::taskInfo;

	my $returnCode	= UpdateTaskStatusToCx( hostId => $mtHostId , tasksInfo => \@tasksInfo, operation => "recovery", directory => $args{directory} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		$ReturnCode	= $Common_Utils::FAILURE;
	}
	
	return ( $ReturnCode );	
}

#####PostRecoveryOperations#####
##description 		:: Performs post recovery operations like power-on, wait till machine is booted up, attaching VMware tools drive
##						if the machine is of type physical.
##Input 			:: Recovered Virtual machine Info. Each element in list is a ref to virtual machine hash data.
##Output 			:: Returns SUCCESS else FAILURE.
#####PostRecoveryOperations#####
sub PostRecoveryOperations
{
	my $RecoveredVirtualMachineInfo	= shift;
	my $returnCode					= $Common_Utils::SUCCESS;
	my $diffRecOrder				= "no";
	my %cdROMAdded					= ();
	
	foreach my $recVmInfo ( @$RecoveredVirtualMachineInfo )
	{
		if($recVmInfo->{recOrder} != $$RecoveredVirtualMachineInfo[0]->{recOrder})
		{
			$diffRecOrder	= "yes";
			last;
		}
	}
	
	my @tempRecVmInfo	= @$RecoveredVirtualMachineInfo;
	for ( my $recoveryOrder = 1; $recoveryOrder <= ( $#tempRecVmInfo + 1 ) ; $recoveryOrder++  )
	{
		my @poweredOnMachineInfo 	= ();
		my @vmViews					= ();
		foreach my $recVmInfo ( @$RecoveredVirtualMachineInfo )
		{
			$cdROMAdded{ $recVmInfo->{uuid} } 	= 1;
			if ( ( "no" eq lc( $recVmInfo->{recovered} ) ) || ( exists $recVmInfo->{powerOn} ) || ( $recoveryOrder ne $recVmInfo->{recOrder} ) )
			{
				next;	
			}
			
			if ( "physicalmachine" eq lc( $recVmInfo->{machineType} ) )
			{
				if ( $Common_Utils::SUCCESS ne AddCdRom( $recVmInfo->{uuid} ) )
				{
					$cdROMAdded{ $recVmInfo->{uuid} }	= 0;
					$returnCode							= $Common_Utils::FAILURE;
				}
			}
			
			if( ( defined $recVmInfo->{powerOnVM} ) && ( "false" eq lc( $recVmInfo->{powerOnVM} ) ) )
			{
				$recVmInfo->{powerOn}= "no";
				Common_Utils::WriteMessage("PowerOn not initiated for the machine \"$recVmInfo->{hostName}\".",2);
				next;
			}
			
			$recVmInfo->{powerOn}	= "yes";
			my ( $ReturnCode , $vmView )	= VmOpts::GetVmViewByUuid( $recVmInfo->{uuid} );
			if ( $ReturnCode != $Common_Utils::SUCCESS )
			{
				$recVmInfo->{powerOn}= "no";
				$returnCode			= $Common_Utils::FAILURE;
				next;
			}	
			push @vmViews, $vmView;			
		}
		my ( $ReturnCode , $vmPoweronInfo )	= VmOpts::PowerOnMultiVM( \@vmViews );
		
		foreach my $recVmInfo ( @$RecoveredVirtualMachineInfo )
		{
			if ( !(defined $$vmPoweronInfo{$recVmInfo->{uuid}} ) )
			{
				next;
			}
			if ( "no" eq $$vmPoweronInfo{$recVmInfo->{uuid}} )
			{
				$recVmInfo->{powerOn}= "no";
				$returnCode			 = $Common_Utils::FAILURE;
				my $machineHostName	 = $recVmInfo->{hostName};
				Common_Utils::WriteMessage("ERROR :: Please power-on virtual machine manually, whose host name is \"$machineHostName\".",3);
				$Common_Utils::taskInfo{$recVmInfo->{inmage_hostid}}{status}= "Failed";
				$Common_Utils::taskInfo{$recVmInfo->{inmage_hostid}}{error}	.= "Power on failed for $machineHostName, power-on manually.";
				next;	
			}
			if ( ( $cdROMAdded{$recVmInfo->{uuid}} ) && ( "physicalmachine" eq lc( $recVmInfo->{machineType} ) ) )
			{
				if ( $Common_Utils::SUCCESS ne MountVMwareTools( $recVmInfo->{uuid} ) )
				{
					$returnCode		= $Common_Utils::FAILURE;
					next;
				}
			}
			push @poweredOnMachineInfo , $recVmInfo->{uuid};			
		}		
		
		if ( (-1 != $#poweredOnMachineInfo) && ($diffRecOrder eq "yes") )
		{
			if ( $Common_Utils::SUCCESS ne VmOpts::AreMachinesBooted( \@poweredOnMachineInfo ) )
			{
				$returnCode			= $Common_Utils::FAILURE;
			}
		}
	}
	Common_Utils::WriteMessage("Post Recovery operations : $returnCode.",2);
	return $returnCode;
}

#####AddCdRom#####
##Description		:: Adds CD ROM if it does not exist.
##Input 			:: Guest machine view.
##Output 			:: Return SUCCESS or FAILURE.
#####AddCdRom#####
sub AddCdRom
{
	my $vmUuid 		= shift;

	my( $returnCode , $vmView )	= VmOpts::GetVmViewByUuid( $vmUuid ); 
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	my $machineName	= $vmView->name;
	
	if( ! defined ( Common_Utils::GetViewProperty( $vmView, 'config.hardware.device') ) )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find hardware configuration of machine $machineName.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my $devices 	= Common_Utils::GetViewProperty( $vmView, 'config.hardware.device');
	my $cdROMExists	= 0;
	foreach my $device ( @$devices )
	{
		if ( "VirtualCdrom" eq ref( $device ) )
		{
			$cdROMExists 	= 1;
			Common_Utils::WriteMessage("CD ROM controller exists in machine $machineName.",2);
			last;
		}
	}
		
	if( !$cdROMExists )
	{
		Common_Utils::WriteMessage("Attaching CD ROM to $machineName.",2);
		my $vmConfigSpec 					= VirtualMachineConfigSpec->new();
		my ( $returnCode , $cdRomDevice )	= VmOpts::CreateCdRomDeviceSpec( 1 );
		my @cdRomDevices					= @$cdRomDevice;
		$vmConfigSpec->{deviceChange}		= [ @cdRomDevices ];	
		
		eval 
		{			
			$vmView->ReconfigVM( spec => $vmConfigSpec );
		};
		if ($@)
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to attach IDE device controller in machine $machineName.",3);
			return ( $Common_Utils::FAILURE );
		}									
	}
	
	return ( $Common_Utils::SUCCESS );	
}

#####MountVMwareTools#####
##Description		:: Mounts VMware tool installer to guest machine.
##Input 			:: guest machine UUID.
##Output 			:: Returns SUCCESS or FAILURE.
#####MountVMwareTools#####
sub MountVMwareTools
{
	my $vmUuid 		= shift;
	Common_Utils::WriteMessage("Came to Mount the Tools.",2);
	my( $returnCode , $vmView )	= VmOpts::GetVmViewByUuid( $vmUuid ); 
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	my $machineName	= $vmView->name;
	
	eval
	{
		$vmView->MountToolsInstaller();
	};
	if ( $@ )
	{
		if ( ref($@->detail) ne 'InvalidState' )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to mount VMware Tools installer in machine $machineName.",3);
			return ( $Common_Utils::FAILURE );
		}
	}
	Common_Utils::WriteMessage("Mounted VMware tools installer.",2);
	return ( $Common_Utils::SUCCESS );
}

#####DeleteJobs#####
##Description 		:: Deletes Protection/Consistency jobs by calling vContinuum.exe.
##Input 			:: Details of the Recovered machines(Array, where each element point to a HASH).
##Output 			:: Returns SUCCESS on success else FAILURE.
#####DeleteJobs#####
sub DeleteJobs
{
	my $RecoveredVirtualMachineInfo	= shift;
	my $planName 					= "";
	my $returnCode					= $Common_Utils::SUCCESS;
	my %RemoveJobs					= ();
	my %removePlan					= ();
	my %vxJobs						= ();
	my %fxJobs						= ();
	
	my @tempRecVmInfo		= @$RecoveredVirtualMachineInfo;
	for ( my $i = 0 ; $i <= $#tempRecVmInfo ; $i++ )
	{
		if ( "yes" eq $$RecoveredVirtualMachineInfo[$i]->{recovered} )
		{
			$planName 				= $$RecoveredVirtualMachineInfo[$i]->{planName};
			my $j					= $i+1;
			$vxJobs{"host$j"}		= { SourceHostId => $$RecoveredVirtualMachineInfo[$i]->{inmage_hostid} , TargetHostId => $$RecoveredVirtualMachineInfo[$i]->{mtInmageHostId} };
			$fxJobs{"host$j"}		= { SourceHostId => $$RecoveredVirtualMachineInfo[$i]->{inmage_hostid} , TargetHostId => $$RecoveredVirtualMachineInfo[$i]->{inmage_hostid} };;
		}
		else
		{
			Common_Utils::WriteMessage("Not calling the clean -up script for machine $$RecoveredVirtualMachineInfo[$i]->{hostName}, as it is not recovered.",2);
		}
		if ( $i == $#tempRecVmInfo )
		{
			my $id 					= $i+2;
			$fxJobs{"host$id"}		= { SourceHostId => $$RecoveredVirtualMachineInfo[$i]->{mtInmageHostId} , TargetHostId => $$RecoveredVirtualMachineInfo[$i]->{mtInmageHostId} };;
			$planName 				= $$RecoveredVirtualMachineInfo[$i]->{planName};
		}
	}
	
	$removePlan{vx}					= \%vxJobs;
	$removePlan{fx}					= \%fxJobs;
	$removePlan{PlanName}			= $planName;
	$RemoveJobs{Plan1}				= \%removePlan;
	
	$returnCode						= XmlFunctions::RemovePlanUsingCxApi( hostId => "" , subParameters => \%RemoveJobs );
	
	Common_Utils::WriteMessage("DeleteJobs = $returnCode.",2);
	return $returnCode;
}

#####DetachVmdksFromMT#####
##Description 	:: First gets the View of Master Target and then it pre-pares all the information about the disk which has to be removed.
##Input 		:: Source Node, Target Node and Hash for collecting recovered vm Information.
##Output 		:: Returns SUCCESS or FAILURE.
#####DetachVmdksFromMT#####
sub DetachVmdksFromMT
{
	my %args	= @_;
	my $machineName = $args{vmView}->name;
	my @diskRemovalSpecs= @{ $args{configSpec} };
	my %mapKeyAndVm		= %{ $args{mapKeysOfVm} };
	my $ReturnCode		= $Common_Utils::SUCCESS;
	my $noOfConfigSpecs	= $#diskRemovalSpecs;
	
	for( my $i=0; $i <= $noOfConfigSpecs; $i++ )
	{
		my $vmSpec = VirtualMachineConfigSpec->new( deviceChange => \@diskRemovalSpecs );
		my $taskView = "";
		
		eval
		{
			my $task	= $args{vmView}->ReconfigVM_Task( spec => $vmSpec );
			$taskView 	= Vim::get_view( mo_ref => $task );
		};
		if ($@) 
		{
			if ( ref($@->detail) eq 'TaskInProgress' )
			{
				Common_Utils::WriteMessage("ReconfigVM:: waiting for 3 secs as another task is in progress.",2);
				sleep(2);
				redo;
			}
			Common_Utils::WriteMessage("ERROR ::Reconfiguration failed for $machineName.",3);
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			
			#Bug 2966197 Recovery job succeded with vCenter(Read-only)user but Recovered VM failed to power-on
			foreach my $recVmInfo ( @{ $args{recVmInfo} } )
			{
				$$recVmInfo{recovered}	= "no";
				$Common_Utils::taskInfo{$$recVmInfo{inmage_hostid}}{status}	= "Failed";
				$Common_Utils::taskInfo{$$recVmInfo{inmage_hostid}}{error}	.= "Failed to detach disks from the Master Target $machineName.";
			}
			
			if ( ref($@->detail) eq 'NoPermission' )
			{
				my $vCenterIp	= $Common_Utils::ServerIp;
				my $command		= "perl Recovery.pl" . "@cmdArgs" ;
				Common_Utils::WriteMessage("ERROR :: Provided user don't have sufficient permissons.",3);
				Common_Utils::WriteMessage("ERROR :: Perform the following steps from the vContinuum machine.",3);
				Common_Utils::WriteMessage("ERROR :: 1. Run the discovery for vCetner  $vCenterIp using vContinuum (going to new protection operation in vCon wizard) with read-write user.",3);
				Common_Utils::WriteMessage("ERROR :: 2. Close vContinuum.",3);
				Common_Utils::WriteMessage("ERROR :: 3. Run the following command from the path: " . $Common_Utils::vContinuumScriptsPath, 3);
				Common_Utils::WriteMessage("ERROR :: \n\n\t $command \n\n",3);
				$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
				$Common_Utils::taskInfo{ErrorMessage}	.= "vCenter/ESX user don't have write permissions.\n";
				$Common_Utils::taskInfo{FixSteps}		.= " Please provide sufficient permissions to the user.\n";	
				$Common_Utils::taskInfo{FixSteps}		.= " Run the discovery for the vCenter $vCenterIp with read-write user.\n";
				$Common_Utils::taskInfo{FixSteps}		.= " Run the command $command  from the path   $Common_Utils::vContinuumScriptsPath .\n";
				return ( $Common_Utils::FAILURE );
			}
			
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to detach disks from MasterTarget $machineName.\n";
			$Common_Utils::taskInfo{FixSteps}		.= " Manually detach the disks from Master target and Power-on the DR VMs.\n";
			$Common_Utils::taskInfo{FixSteps}		.= " Delete the consistency jobs from CX and also update failover as yes in master xml file.\n";
			Common_Utils::WriteMessage("ERROR :: Failed to detach disks from MasterTarget $machineName.",3);
			Common_Utils::WriteMessage("ERROR :: Manually detach the disks from Master target and Power-on the DR VMs.",3);
			Common_Utils::WriteMessage("ERROR :: Delete the consistency jobs from CX and also update failover as yes in master xml file.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		my $returnCode	= $Common_Utils::SUCCESS;
		my $returnIndex = "";
		eval
		{
	   		while(1)
	   		{   			
			   	my $info 		= $taskView->info;
		        if( $info->state->val eq 'running' )
		        {
		        	
		        } 
		        elsif( $info->state->val eq 'success' ) 
		        {
		            Common_Utils::WriteMessage("Reconfigure of virtual machine \"$machineName\" is success.",2);
		            last;
		        }  
		        elsif( $info->state->val eq 'error' ) 
		        {
		        	my $soap_fault 	= SoapFault->new;
		            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
		            Common_Utils::WriteMessage("Reconfiguration failed on machine \"$machineName\" due to error \"$errorMesg\".",2);
		            if( exists $info->{error}->{fault}->{deviceIndex} )
		            {
		            	$returnIndex	= $info->error->fault->deviceIndex;
		            	$returnCode		= $Common_Utils::FAILURE;
		            }
		            $ReturnCode		= $Common_Utils::FAILURE;
		            last;
		        }
		        $taskView->ViewBase::update_view_data();
	   		}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR ::Reconfiguration failed for $machineName.",3);
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to detach disks from MasterTarget $machineName.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Manually detach the disks from Master target and Power-on the DR VMs.\n";
			$Common_Utils::taskInfo{FixSteps}		.= " Delete the consistency jobs from CX and also update failover as yes in master xml file.\n";
			
			Common_Utils::WriteMessage("ERROR :: Failed to detach disks from MasterTarget $machineName.",3);
			Common_Utils::WriteMessage("ERROR :: Manually detach the disks from Master target and Power-on the DR VMs.",3);
			Common_Utils::WriteMessage("ERROR :: Delete the consistency jobs from CX and also update failover as yes in master xml file.",3);
							
			return ( $Common_Utils::FAILURE );
		}
		
		if( $returnCode == $Common_Utils::SUCCESS)
		{
			last;
		}
		else
		{
			my $skipKey	= $diskRemovalSpecs[$returnIndex]->device->key;
			my @tempDiskRemovalSpecs	= @diskRemovalSpecs;
			@diskRemovalSpecs			= ();
			foreach my $configSpec ( @tempDiskRemovalSpecs )
			{
				if( $mapKeyAndVm{$skipKey} eq $mapKeyAndVm{$configSpec->device->key} )
				{
					$i++;
					next;
				}
				push @diskRemovalSpecs, $configSpec;
			}
			$i--;
			foreach my $recVmInfo ( @{ $args{recVmInfo} } )
			{
				if( $mapKeyAndVm{$skipKey} eq $$recVmInfo{inmage_hostid})
				{
					$$recVmInfo{recovered}	= "no";
					Common_Utils::WriteMessage("ERROR :: Failed to detach the vmdks for \"$$recVmInfo{hostName}\" from \"$machineName\".",3);
					$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
					$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to detach disks of machine $$recVmInfo{hostName}.\n";
					$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether 1. SCSI information for each of the disk of host $$recVmInfo{hostName} matches with data in file $args{xmlFilePath} .\n2.Make sure all disks of this host are found on master target.\n";
					$Common_Utils::taskInfo{$$recVmInfo{inmage_hostid}}{status}	= "Failed";
					$Common_Utils::taskInfo{$$recVmInfo{inmage_hostid}}{error}	.= "Failed to detach disks of machine $$recVmInfo{hostName}.";
										
				}
			} 
			next;
		}
	}
	return ( $ReturnCode );
}

#####Recovery_V2P#####
##Description 		:: performs delete protection jobs and update masterconfig file for each of recovered vm.
##Input 			:: Recovery file Name and directory path to recovery file.
##Output 			:: Returns SUCCESS else FAILURE.
#####Recovery_V2P#####
sub Recovery_V2P
{
	my %args 							= @_;
	my $ReturnCode						= $Common_Utils::SUCCESS;
	my $recoveryXmlPath 				= "$args{directory}/$args{file}";
	my @tasksInfo		 				= ();
	my $mtHostId 						= "";

	$Common_Utils::taskInfo{TaskName} 	= "Powering on the recovered VM(s)";
	$Common_Utils::taskInfo{TaskStatus}	= "Completed";
	
	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{
		if ( ! -f $recoveryXmlPath )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find $args{file} file in path $args{directory}.",3);
			Common_Utils::WriteMessage("ERROR :: Please check whether above file exists or not.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find $args{file} file in path $args{directory}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file $args{file} exists or not.\n";
			last;
		}
		
		my( $returnCode , $docNode ) 		= XmlFunctions::GetPlanNodes( $recoveryXmlPath );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to get recovery plan information from file \"$recoveryXmlPath\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to get recovery plan information from file $recoveryXmlPath.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file $recoveryXmlPath exits and have permissions to access it.\n";
			last;
		}
		
		my $planNodes 						= $docNode->getElementsByTagName('root');
		if ( "array" ne lc ( ref( $planNodes ) ) )
		{
			$planNodes 						= [$docNode];
		}
		
		foreach my $plan ( @$planNodes )
		{
			my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
			my @targetInfo					= $targetHost[0]->getElementsByTagName('host');
			$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
			$mtHostId						= $targetInfo[0]->getAttribute('inmage_hostid');
		}
		
		( $returnCode , my $cxIpPortNum )	= Common_Utils::FindCXIP();
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find CX IP from registry.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether unified agent is installed and registry has CXIP.\n";
			last;
		}
		
		if ( ( !defined $args{cxpath} ) || ( "" eq $args{cxpath} ) )
		{
			Common_Utils::WriteMessage("ERROR :: cxpath is not defined.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "cxpath is not defined.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check the command, send cx path also.\n";
			last;	
		}
		
		my $rCode 	= Common_Utils::DownloadFilesFromCX(	'file' 				=> "input.txt",
															'filePath'			=> "$args{cxpath}",
															'downloadSubFiles' 	=> "yes",
															'dirPath'			=> "$args{directory}",
															'cxIp'				=> "$Common_Utils::CxIp",
							 							);
		if ( $rCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to download \"input.txt\" from CX or subfiles listed in it.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to download file \"input.txt\" from CX.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file \"input.txt\" exists or not in CX under path.\n";
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		my $rollbackFile					= "$args{directory}/InMage_Recovered_Vms.rollback";
		Common_Utils::WriteMessage("Rollback file = $rollbackFile.",2);
		if ( ! -f $rollbackFile )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find Rollback file in directory $args{directory}.",3);
			Common_Utils::WriteMessage("ERROR :: Please check whether the rollback process completed successfully or not.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find Rollback file in directory $args{directory}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether the rollback process completed successfully or not.\n";
			last;
		}
		
		my %recoveredHostName				= ();
		$returnCode							= ReadRollBackFile( $rollbackFile , \%recoveredHostName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to read rollback file.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to read rollback file.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check permisssions on file $rollbackFile.\n";
			last;
		}
		
		my %recoveredVMs	= ();
		my $powerOnStatusFile				= "$args{directory}/Power_On_Vms.status";
		if ( -f $powerOnStatusFile )
		{
			$returnCode						= ReadRollBackFile( $powerOnStatusFile , \%recoveredVMs );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				#don't need block for this failures				
			}			
		}
		
		if ( 0 == open( FH ,">>$powerOnStatusFile") )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to write into $powerOnStatusFile file.",3);
			$ReturnCode	= $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to write into $powerOnStatusFile file. Manually prepare the file.\n";
		}
			
		my @RecoveredVirtualMachineInfo		= ();
		foreach my $plan ( @$planNodes )
		{
			my $planName					= $plan->getAttribute('plan');
			my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
			my @targetInfo					= $targetHost[0]->getElementsByTagName('host');		
			my @sourceInfo					= $plan->getElementsByTagName('SRC_ESX');
			$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
			$mtHostId						= $targetInfo[0]->getAttribute('inmage_hostid');
						
			foreach my $sourceInfo	( @sourceInfo )
			{
				my @hostsInfo				= $sourceInfo->getElementsByTagName('host');
				foreach my $host ( @hostsInfo )
				{
					my $hostName			= $host->getAttribute('hostname');
					my $InMageHostId		= $host->getAttribute('inmage_hostid');
					Common_Utils::WriteMessage("Checking for HostName $hostName .",2);
					
					if( defined $recoveredVMs{$InMageHostId} )
					{
						Common_Utils::WriteMessage("Recovery already completed for machine \"$hostName\".",2);
						next;
					}
					
					if ( !((defined $InMageHostId && $InMageHostId !~ /^$/ && exists $recoveredHostName{$InMageHostId} ) || (exists $recoveredHostName{$hostName}))) 
					{
						$ReturnCode = $Common_Utils::FAILURE;
						Common_Utils::WriteMessage("ERROR :: Rollback process failed for machine \"$hostName\" .",3);
						$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Rollback process failed for machine $hostName.\n";
						$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether rollback operation is succeeded and volume replication are removed?.\n";
						next;
					}
					
					my %RecoveredVmInfo				= ();
					$RecoveredVmInfo{planName}		= $host->getAttribute('plan');
					$RecoveredVmInfo{recovered}		= "yes";
					$RecoveredVmInfo{hostName}		= $host->getAttribute('hostname');
					$RecoveredVmInfo{inmage_hostid}	= $host->getAttribute('inmage_hostid');
					$RecoveredVmInfo{mtInmageHostId}= $mtHostId;
					if ( "yes" eq lc( $host->getAttribute('failover') ) )
					{
						$host->setAttribute('failback',"yes");
					}
					$host->setAttribute('failover',"yes");
					print FH "$RecoveredVmInfo{inmage_hostid}\n";	
					push @RecoveredVirtualMachineInfo , \%RecoveredVmInfo;
				}
			}
		}
		
		eval
		{
			close FH;
		};
		
		if(-1 == $#RecoveredVirtualMachineInfo )
		{
			Common_Utils::WriteMessage("No VMs found for delete jobs.",2);
			last;				
		}
			
		my @docElements = $docNode;
		$returnCode		= XmlFunctions::SaveXML( \@docElements , $recoveryXmlPath );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to modify $args{file} file.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to update recovery status in file $args{file}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please update failover or failback parameter for successfully recovered machines in Esx_Master.xml file in CX.\n";
		}
		
		if( $Common_Utils::SUCCESS != DeleteJobs( \@RecoveredVirtualMachineInfo ) )
		{
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to remove Protection/Consistency jobs for recovered machines.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please remove jobs manually from CX.\n";
			Common_Utils::WriteMessage("ERROR :: Failed to remove Protection/Consistency jobs for recovered machines.",3);
			Common_Utils::WriteMessage("ERROR :: Either delete jobs manually or leave them intact.",3);
		}
		
		if ( $Common_Utils::SUCCESS != XmlFunctions::UpdateMasterXmlCX( fileName => $recoveryXmlPath ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update master xml.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to update recovery status to CX.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please update failover or failback parameter for successfully recovered machines in Esx_Master.xml file in CX.\n";
			$ReturnCode	= $Common_Utils::FAILURE;
		}
	}
	
	push @tasksInfo , \%Common_Utils::taskInfo;

	my $returnCode	= UpdateTaskStatusToCx( hostId => $mtHostId , tasksInfo => \@tasksInfo, operation => "recovery" );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		$ReturnCode	= $Common_Utils::FAILURE;
	}
	
	return ( $ReturnCode );	
}

#####UpdateTaskStatusToCx#####
##Description 		:: List's all possible calls for this Script in different scenarios.
##Input 			:: None.
##Output			:: None.
#####UpdateTaskStatusToCx#####
sub UpdateTaskStatusToCx
{
	my %args		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	if ( !( Opts::option_is_set('planid') ) || ( !defined Opts::get_option('planid') ) || ( "" eq Opts::get_option('planid') ) ) 
	{
		Common_Utils::WriteMessage("Found planid options not defined.",2);
		Common_Utils::WriteMessage("This is treated as WMI based $args{operation}.",2);		
	}
	else
	{
		my $planId	= Opts::get_option('planid');
		my $cxPath	= Opts::get_option('cxpath');
		$Common_Utils::taskInfo{LogPath}= $cxPath."/vContinuum_Scripts.log";
		my( $returnCode , $cxIpPortNum )= Common_Utils::FindCXIP();
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to post status of $args{operation} to CX.",3);
			return ( $Common_Utils::FAILURE );
		}
		$Common_Utils::CxIpPortNumber	= $cxIpPortNum;
		
		if ( $Common_Utils::SUCCESS != XmlFunctions::UpdateTaskStatus( planId => $planId , cxIp => $cxIpPortNum , hostId => $args{hostId} , tasksUpdates => $args{tasksInfo} ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update task status xml.",2);
			$ReturnCode	= $Common_Utils::FAILURE;
		}

#	To update the each VM status to CX (telemetry) , currently not using status, just sending alerts	
#		if( $args{operation} eq "recovery" )
#		{
#			if ( $Common_Utils::SUCCESS != XmlFunctions::UpdateStatusToCx( planId => $planId , cxIp => $cxIpPortNum , hostId => $args{hostId} , tasksInfo => $args{tasksInfo}, planType => "RecoveryServers" ) )
#			{
#				Common_Utils::WriteMessage("ERROR :: Failed to update plan status to CX.",2);
#				$ReturnCode	= $Common_Utils::FAILURE;
#			}
#		}	
	}
	
	if( $args{operation} eq "recovery" )
	{
		if ( $Common_Utils::SUCCESS != XmlFunctions::SendAlertToCX( hostId => $args{hostId}, tasksInfo => $args{tasksInfo}, directory => $args{directory} ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update plan status to CX.",2);
#			$ReturnCode	= $Common_Utils::FAILURE;
		}
	}
		
	return ( $ReturnCode );
}

#####RemoveDiskFromProtection#####
##Description		:: It Removes the Disks From Protection.
##Input 			:: XML file which contains information of the machines which disks are to be removed.
##Ouput				:: Returns SUCCESS else FAILURE.
#####RemoveDiskFromProtection#####
sub RemoveDiskFromProtection
{
	my %args 							= @_;
	my $ReturnCode						= $Common_Utils::SUCCESS;
	my $recoveryXmlPath 				= "$args{directory}/$args{file}";
	my @tasksInfo		 				= ();
	my $mtHostId						= "";

	$Common_Utils::taskInfo{TaskName} 	= "Detaching disks from MT and DR VM(s)";
	$Common_Utils::taskInfo{TaskStatus}	= "Completed";

	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{
		if ( ! -f $recoveryXmlPath )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find $args{file} file in path $args{directory}.",3);
			Common_Utils::WriteMessage("ERROR :: Please check whether above file exists or not.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find $args{file} file in path $args{directory}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file $args{file} exists or not.\n";
			last;
		}
	
		my ( $returnCode , $docNode ) 	= XmlFunctions::GetPlanNodes( $recoveryXmlPath );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to get plan information from file \"$recoveryXmlPath\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to get remove plan information from file $recoveryXmlPath.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether file $recoveryXmlPath exits and have permissions to access it.\n";
			last;
		}
		
		my $planNodes 						= $docNode->getElementsByTagName('root');
		if ( "array" ne lc ( ref( $planNodes ) ) )
		{
			$planNodes 						= [$docNode];
		}
		
		my @RecoveredVirtualMachineInfo		= ();
		foreach my $plan ( @$planNodes )
		{
			my $planName					= $plan->getAttribute('plan');
			my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
			my @targetInfo					= $targetHost[0]->getElementsByTagName('host');		
			my @sourceInfo					= $plan->getElementsByTagName('SRC_ESX');
			$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
			$mtHostId						= $targetInfo[0]->getAttribute('inmage_hostid');
					
			my ( $returnCode , $mtView ) 	= VmOpts::GetVmViewByUuid( $targetInfo[0]->getAttribute('source_uuid') );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: Unable to find view of master target.",3);
				$ReturnCode = $Common_Utils::FAILURE;
				$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
				$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find view of master target.\n";
				$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether 1. master target exists?.\n";
				last;
			}
			
			my $mtName		= $mtView->name;
			my $mtDevices	= Common_Utils::GetViewProperty( $mtView, 'config.hardware.device');
			my @deviceChange= ();
			my %deviceUuids	= ();
			my %clusterDisks= ();
			foreach my $sourceInfo	( @sourceInfo )
			{
				my @hostsInfo				= $sourceInfo->getElementsByTagName('host');
				foreach my $host ( @hostsInfo )
				{
					my $displayName	= $host->getAttribute('display_name');
					my @disksInfo 	= $sourceInfo->getElementsByTagName('disk');									
					
					if ( "yes" eq lc( $host->getAttribute('failover') ) )
					{
						Common_Utils::WriteMessage("Machine $displayName is already recovered.",2);
						next;
					}
					
					foreach my $disk ( @disksInfo )
					{
						if ( "true" ne lc ( $disk->getAttribute('remove') ) )
						{
							next;
						}
							
						my $diskScsiID	= $disk->getAttribute('scsi_id_onmastertarget');
						my $diskUuid	= $disk->getAttribute('target_disk_uuid');
						my $diskName	= $disk->getAttribute('disk_name');
						my $found		= 0;
						
						Common_Utils::WriteMessage("unbinding disk $diskName from host $displayName.",2);
						$disk->unbindNode();
						
						foreach my $device ( @$mtDevices )
						{
							if( $device->deviceInfo->label !~ /Hard Disk/i )
							{
								next;
							}	
							my $vmdkName 	= $device->backing->fileName;					
							my $scsiNumber 	= "";
							my $scsiUnitNum	= "";
							if( ($device->controllerKey >= 1000) && ($device->controllerKey <= 1004) )
							{
								$scsiNumber 	= $device->controllerKey - 1000;
								$scsiUnitNum	= ( $device->key - 2000 ) % 16;							
							}
							elsif( ($device->controllerKey >= 15000) && ($device->controllerKey <= 15004) )
							{
								$scsiNumber 	= $device->controllerKey - 15000 + 4;
								$scsiUnitNum	= ( $device->key - 16000 ) % 30;							
							}
							my $vmdkUuid	= $device->backing->uuid;
							if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
							{
								$vmdkUuid	= $device->backing->lunUuid;
							}
							
							if( defined $clusterDisks{$vmdkUuid} )
							{
								$found	= 1;
								next;
							}
							$clusterDisks{$vmdkUuid} = "removed";
							
							if ( ( defined $diskUuid ) && ( $diskUuid ne "" ) && ( $vmdkUuid eq $diskUuid ) )
							{
								$found	= 1;
								Common_Utils::WriteMessage("Detaching the disk $vmdkName of machine $displayName based on Uuid.",2);
								my $diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
								push @deviceChange , $diskSpec;
								$deviceUuids{$vmdkUuid}	= "delete";							
								last;
							}
							
							if ( ( ( !defined $diskUuid ) || ( $diskUuid eq "" ) ) && ( defined $diskScsiID ) && ( "$scsiNumber:$scsiUnitNum" eq $diskScsiID ) )
							{
								$found	= 1;
								Common_Utils::WriteMessage("Detaching the disk $vmdkName of machine $displayName based on scsi id.",2);
								my $diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
								push @deviceChange , $diskSpec;
								$deviceUuids{$vmdkUuid}	= "delete";
								last;
							}
						}
						if( ! $found )
						{
							Common_Utils::WriteMessage("ERROR :: Vmdk $diskName of machine $displayName not found on Master Target \"$mtName\" .",3);
							$ReturnCode = $Common_Utils::FAILURE;
							$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
							$Common_Utils::taskInfo{ErrorMessage}	.= "Vmdk $diskName of machine $displayName not found on Master Target $mtName.\n";
							$Common_Utils::taskInfo{FixSteps}		.= "->Manually remove the vmdks from master target and protected vms update the xml file .\n";
							last;
						}		
					}				
				}
			}
			
			if ( -1 == $#deviceChange )
			{
				Common_Utils::WriteMessage("ERROR :: No disk found for deleting, considering volume deletion only and proceeding.",2);
				last;
			}
			
			if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
			{
				$returnCode 				= VmOpts::PowerOff( $mtView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$ReturnCode = $Common_Utils::FAILURE;
					$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
					$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to power off master target.Could not detach disk from master target.\n";
					last;
				}
			}
		
			Common_Utils::WriteMessage("Reconfiguring the VM \"$mtName\".",2);
			$returnCode	= VmOpts::ReconfigVm( vmView => $mtView , changeConfig => {deviceChange => \@deviceChange });
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: Reconfiguration failed for the VM \"$mtName\" while detaching the vmdks.",3);
				$ReturnCode = $Common_Utils::FAILURE;
				$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
				$Common_Utils::taskInfo{ErrorMessage}	.= "Reconfiguration failed on Master Target $mtName.\n";
				$Common_Utils::taskInfo{FixSteps}		.= "->Manually remove the vmdks from master target and protected vms update the xml file .\n";
				last;
			}
			
			( $returnCode, $mtView )= VmOpts::UpdateVmView( $mtView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$mtName\".",2);
			}
			
			if ( "poweredOn" ne Common_Utils::GetViewProperty( $mtView, 'summary.runtime.powerState')->val )
			{
				$returnCode 			= VmOpts::PowerOn( $mtView );
				if ( $returnCode == $Common_Utils::SUCCESS )
				{
					my @poweredOnMachineInfo= ( Common_Utils::GetViewProperty( $mtView, 'summary.config.uuid') );
					if ( $Common_Utils::SUCCESS ne VmOpts::AreMachinesBooted( \@poweredOnMachineInfo ) )
					{
						$ReturnCode 		= $Common_Utils::FAILURE;
					}
				}
				elsif ( $returnCode != $Common_Utils::SUCCESS )
				{
					$ReturnCode	= $Common_Utils::FAILURE;
					$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
					$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to bring up master target. Manually power-on the master target.\n";
					$Common_Utils::taskInfo{FixSteps}		.= "->Manually power on the master target and remove disks from protected vms update the xml file .\n";
				}
			}
			
			foreach my $sourceHost	( @sourceInfo )
			{
				my @hostsInfo				= $sourceHost->getElementsByTagName('host');
				foreach my $host ( @hostsInfo )
				{
					my $displayName	= $host->getAttribute('display_name');
					my $vmUuid		= $host->getAttribute('target_uuid');
									
					( $returnCode,my $vmView )= VmOpts::GetVmViewByUuid( $vmUuid );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						$ReturnCode = $Common_Utils::FAILURE;
						$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to find view of virtual machine $displayName.\n";
						$Common_Utils::taskInfo{FixSteps}		.= "->Please check whether $displayName virtual machine exists?.\n";
						last;
					}
					
					$returnCode	= VmOpts::RemoveMultipleVmdks( vmView => $vmView, uuidMap => \%deviceUuids );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						Common_Utils::WriteMessage("ERROR::Vmdks removing failed for \"$displayName\".",3);
						$ReturnCode = $Common_Utils::FAILURE;
						$Common_Utils::taskInfo{TaskStatus} 	= "Failed";
						$Common_Utils::taskInfo{ErrorMessage}	.= "Removing vmdks failed on virtual machine $displayName.\n";
						$Common_Utils::taskInfo{FixSteps}		.= "->Manually remove the vmdks from $displayName and update the xml file .\n";
						last;
					}
				}
			}
		}
		
		
		my @docElements = $docNode;				
		$returnCode		= XmlFunctions::SaveXML( \@docElements , $recoveryXmlPath );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to modify $args{file} file.",3);
			$ReturnCode	= $Common_Utils::FAILURE;
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to update remove disk status in file $args{file}.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please remove the disk node for successfull machines in Esx_Master.xml file in CX.\n";
		}
		
		if ( $Common_Utils::SUCCESS != XmlFunctions::UpdateMasterXmlCX( fileName => $recoveryXmlPath ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update master xml.",3);
			$Common_Utils::taskInfo{TaskStatus} 	= "Warning";
			$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to update remove disk status to CX.\n";
			$Common_Utils::taskInfo{FixSteps}		.= "->Please remove the disk node for successfull machines in Esx_Master.xml file in CX.\n";
			$ReturnCode	= $Common_Utils::FAILURE;
		}
	}
	
	push @tasksInfo , \%Common_Utils::taskInfo;

	my $returnCode	= UpdateTaskStatusToCx( hostId => $mtHostId , tasksInfo => \@tasksInfo, operation => "removedisk" );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		$ReturnCode	= $Common_Utils::FAILURE;
	}
		
	return ( $ReturnCode );	
}

#####UpdateTaskStatusAsFail#####
##Description 		:: Update the task status to CX on connection failed.
##Input 			:: None.
##Output			:: None.
#####UpdateTaskStatusAsFail#####
sub UpdateTaskStatusAsFail
{
	my $ReturnCode	= $Common_Utils::SUCCESS;
	my @tasksInfo	= ();
	my $mtHostId 	= "";
	my $file		= "";
	my $operation	= "";
	
	$Common_Utils::taskInfo{TaskStatus}		= "Failed";
	$Common_Utils::taskInfo{ErrorMessage}	.= "Failed to connect to vCenter/ESX server.\n";
	
	if ( "yes" eq lc ( Opts::get_option('recovery') ) )
	{
		$file 		= "Recovery.xml";
		$operation	= "recovery";
		$Common_Utils::taskInfo{TaskName} 	= "Powering on the recovered VM(s)";
		$Common_Utils::taskInfo{FixSteps}	.= "->Please detach the vmdks from MT, power on the DR-VMs.\n";
		$Common_Utils::taskInfo{FixSteps}	.= "->Remove the FX jobs related to recovered machines.\n";
		$Common_Utils::taskInfo{FixSteps}	.= "->Update the Master config file for the recovered hosts, set the failover values as yes.\n";
	}
	elsif ( "yes" eq lc ( Opts::get_option('remove') ) )
	{
		#task status not updating
		return ( $ReturnCode );
	}
	elsif ( "yes" eq lc ( Opts::get_option('removedisk') ) )
	{
		$file		= "Remove.xml";
		$operation	= "removedisk";
		$Common_Utils::taskInfo{TaskName} 	= "Detaching disks from MT and DR VM(s)";
		$Common_Utils::taskInfo{FixSteps}	.= "->Please detach the vmdk from MT and remove the disk node for successfull machines in Esx_Master.xml file in CX.\n";				
	}
	elsif ( "yes" eq lc ( Opts::get_option('dr_drill') ) )
	{
		$file		= "Snapshot.xml";
		$operation	= "drdrill";
		$Common_Utils::taskInfo{TaskName} 	= "Powering on the dr drill VM(s)";	
		$Common_Utils::taskInfo{FixSteps}	.= "->Please detach the vmdks from MT, power on the Dr-Drill VMs.\n";
	}
	
	my $recoveryXmlPath = Opts::get_option('directory') . "/" . $file;
	
	my( $returnCode , $docNode ) 		= XmlFunctions::GetPlanNodes( $recoveryXmlPath );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get recovery plan information from file \"$recoveryXmlPath\".",3);
		return $Common_Utils::FAILURE;
	}
		
	my $planNodes 						= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 						= [$docNode];
	}
	
	foreach my $plan ( @$planNodes )
	{
		my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetInfo					= $targetHost[0]->getElementsByTagName('host');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		$mtHostId						= $targetInfo[0]->getAttribute('inmage_hostid');
	}
	
	( $returnCode , my $cxIpPortNum )	= Common_Utils::FindCXIP();
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
		return $Common_Utils::FAILURE;
	}
	
	push @tasksInfo , \%Common_Utils::taskInfo;

	$returnCode	= UpdateTaskStatusToCx( hostId => $mtHostId , tasksInfo => \@tasksInfo, operation => $operation );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to update the task status.",3);
		return $Common_Utils::FAILURE;
	}
}

#####Usage#####
##Description 		:: List's all possible calls for this Script in different scenarios.
##Input 			:: None.
##Output			:: None.
#####Usage#####
sub Usage
{
	Common_Utils::WriteMessage("*****Usage of Recovery.pl*****",3);
	Common_Utils::WriteMessage("==============================",3);
	Common_Utils::WriteMessage("For Recovery ---> Recovery.pl --server <IP> --username <USERNAME> --password <PASSWORD> --recovery 'yes' --directory 'Directory Path to Recovery.xml file' --cxpath \"Path in CX where log files are to be placed\" --planid \"Plan ID generated on CX for current recovery plan name.\".",3);
	Common_Utils::WriteMessage("For Recovery ---> Recovery.pl --server <IP> --username <USERNAME> --password <PASSWORD> --dr_drill 'yes' --directory 'Directory Path to Snapshot.xml file' --cxpath \"Path in CX where log files are to be placed\" --planid \"Plan ID generated on CX for current dr-drill plan name.\".",3);
}

BEGIN:

my $ReturnCode				= 1;
my ( $sec, $min, $hour )	= localtime(time);
my $logFileName 		 	= "vContinuum_Temp_$sec$min$hour.log";

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
elsif ( ( ( !defined Opts::get_option('directory') ) || ( "" eq Opts::get_option('directory') ) ) )
{
	Common_Utils::WriteMessage("ERROR :: Found directory parameter is undefined or set to empty values.",3);
	Common_Utils::WriteMessage("ERROR :: Please specify valid values to above parameters and Usage is displayed for help.",3);
	$ReturnCode		= $Common_Utils::FAILURE;
	Usage();
}
elsif( ( Opts::option_is_set('recovery_v2p') ) && ( "yes" eq lc ( Opts::get_option('recovery_v2p') ) ) )
{
	my $file 		= "Recovery.xml";
	$ReturnCode		= Recovery_V2P( 
									file 		=> $file,
									directory 	=> Opts::get_option('directory'),
									cxpath		=> Opts::get_option('cxpath'),
								);
}
else
{
	$ReturnCode		= Common_Utils::Connect( Server_IP => Opts::get_option('server'), UserName => Opts::get_option('username'), Password => Opts::get_option('password') );
	if ( $Common_Utils::SUCCESS == $ReturnCode )
	{
		eval
		{
			if ( $ReturnCode eq $Common_Utils::SUCCESS )
			{
				if ( "yes" eq lc ( Opts::get_option('recovery') ) )
				{
					my $file 		= "Recovery.xml";
					$ReturnCode		= Recovery( 
												file 		=> $file,
												directory 	=> Opts::get_option('directory'),
												cxpath		=> Opts::get_option('cxpath'),
											);
				}
				elsif ( "yes" eq lc ( Opts::get_option('remove') ) )
				{
					my $file		= "Remove.xml";
					$ReturnCode		= Remove(
												file 		=> $file,
												directory 	=> Opts::get_option('directory'),
											);		
				}
				elsif ( "yes" eq lc ( Opts::get_option('removedisk') ) )
				{
					my $file		= "Remove.xml";
					$ReturnCode		= RemoveDiskFromProtection(
																file 		=> $file,
																directory 	=> Opts::get_option('directory'),
															);		
				}
				elsif ( "yes" eq lc ( Opts::get_option('dr_drill') ) )
				{
					my $file		= "Snapshot.xml";
					$ReturnCode		= DrDrill(
												file 		=> $file,
												directory 	=> Opts::get_option('directory'),
												cxpath		=> Opts::get_option('cxpath'),
											);		
				}
			}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			$ReturnCode 			= $Common_Utils::FAILURE;
		}
	}
	else
	{
		UpdateTaskStatusAsFail();
	}
}

if( (!defined $Common_Utils::vContinuumPlanDir ) && ( defined (Opts::get_option('directory') ) ) )
{
	my $localPath	= Opts::get_option('directory');
	my @pathSplit	= split( m/\\|\//, $localPath );
	$Common_Utils::vContinuumPlanDir	= $pathSplit[-1];
	Common_Utils::WriteMessage("Plan name directory : $Common_Utils::vContinuumPlanDir.",2);
}

Common_Utils::LoggedData( tempLog => "$Common_Utils::vContinuumLogsPath/$logFileName" , planLog => $Common_Utils::vContinuumPlanDir );
if ( "yes" eq lc ( Opts::get_option('remove') ) && -e $Common_Utils::planTempLogFile )
{
	unlink $Common_Utils::planTempLogFile;
}

if ( ( $Common_Utils::FAILURE == $ReturnCode ) && ( "yes" ne lc ( Opts::get_option('remove') ) ) )
{
	sleep(5);
	my $file = Opts::get_option('directory')."/vContinuum_Scripts.log";
	Common_Utils::UploadUsingCsps( cxIp => $Common_Utils::CxIp , file => $file , dirPath => Opts::get_option('cxpath') );
}

exit( $ReturnCode );							 

__END__
