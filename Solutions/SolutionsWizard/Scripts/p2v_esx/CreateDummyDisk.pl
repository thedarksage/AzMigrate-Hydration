=head
	-----CreateDummyDisk.pl-----
	1. Main moto of this file is to create dummy disk, either to find or have
		1. SCSI Controller port and Target ID number's inside Guest OS(Guest OS either Windows or Linux).
		2. To maintain disk order in case of Linux.
	
	Parameters which are accepted by this file are:
	1. Takes the controller information where to add the dummy disks, this is needed in case of linux, as we are adding 2MB dummy 
		disk in slots of removed disks.
	2. Should we read Master Target name as parameter or read from XML?
		
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

my %customopts = (
				  	dr_drill 			=> {type => "=s", help => "If Dr_Drill, set this argument to 'yes'.", required => 0,},
				  );

#####FindDatastoreInfoInHost#####
##Description 		:: Finds datastore information in the specified Host. IT determines all information in related to datastore
##						like datastoreName, free space, capacity, Block Size. 
##Input 			:: vSphere host name for which Datastore information is to be found and Datacenter name where this host resides.
##Output 			:: Returns DatastoreInfo on SUCCESS else FAILURE.
#####FindDatastoreInfoInHost#####
sub FindDatastoreInfoInHost
{
	my $vSphereHostName 				= shift;
	my $datacenterName					= shift;
	
	my( $returnCode , $hostView ) 		= HostOpts::GetSubHostView( $vSphereHostName );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	
	( $returnCode ,my $datacenterView )	= DatacenterOpts::GetDataCenterView( $datacenterName );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	
	my @hostViews						= $hostView;
	my @datacenterViews					= $datacenterView;
	( $returnCode ,my $datastoreInfo )	= DatastoreOpts::GetDatastoreInfo( \@hostViews , \@datacenterViews );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return $Common_Utils::FAILURE;
	} 
	
	return ( $Common_Utils::SUCCESS , $datastoreInfo )
}					

#####FindEmptyScsiSlotsandSizes#####
##Description 		:: For each of the master Target we will find empty SCSI slot information as well Size of disk to be created respectively.
##Input 			:: View of Master Target.
##Output 			:: Returns SUCCESS if information required is gathered else FAILURE.
#####FindEmptyScsiSlotsandSizes#####
sub FindEmptyScsiSlotsandSizes
{
	my $masterTargetView 		= shift;
	my $masterTargetonHost		= shift;
	my $masterTargetInDatacenter= shift;
	my $mtDatastoreFreeSpace	= 0;
	
	my $masterTargetName		= $masterTargetView->name;
	my $masterTargetVmPathName 	= Common_Utils::GetViewProperty( $masterTargetView,'summary.config.vmPathName');
	my $vmxVersion 				= Common_Utils::GetViewProperty( $masterTargetView, 'config.version');
	
	my ( $returnCode , $datastoreInfo )	= FindDatastoreInfoInHost( $masterTargetonHost , $masterTargetInDatacenter );
	if ( $Common_Utils::SUCCESS ne $returnCode )	
	{
		return $Common_Utils::FAILURE;
	}
	
	my @vmPathInfo				= split( /\[|\]/ , $masterTargetVmPathName );
	my $slashIndex				= rindex( $masterTargetVmPathName , "/");
	my $folderPath				= substr( $masterTargetVmPathName , 0 , $slashIndex + 1 );
	foreach my $datastore ( @$datastoreInfo )
	{
		if ( $vmPathInfo[1] eq $datastore->{symbolicName} )
		{
			$mtDatastoreFreeSpace	= $datastore->{freeSpace};
		}
	}  
	
	if( !defined ( Common_Utils::GetViewProperty( $masterTargetView,'config.hardware.device') ) )
	{
		Common_Utils::WriteMessage("ERROR :: No devices are found on machine \"$masterTargetName\".",3);
		return $Common_Utils::FAILURE;
	}
	
	my $mtDevices 					= Common_Utils::GetViewProperty( $masterTargetView,'config.hardware.device');
	my %masterTargetDiskInfo		= ();
	my %masterTargetControllerInfo	= ();
	foreach my $device ( @$mtDevices )
	{
		if ( ( defined  $device->controllerKey ) && ( ( ($device->controllerKey >= 1000) && ($device->controllerKey <= 1004) ) ||
			( ($device->controllerKey >= 15000) && ($device->controllerKey <= 15004) ) ) )
		{
			$masterTargetDiskInfo{$device->key}	= "key";
			if ( $device->deviceInfo->label =~ /^Hard disk/i )
			{
				$masterTargetDiskInfo{$device->capacityInKB} = "size";
			}
			if ( ! exists $masterTargetControllerInfo{ $device->controllerKey }{DiskCount} )
			{
				$masterTargetControllerInfo{$device->controllerKey}{DiskCount} = 1;
				next;
			}	
			$masterTargetControllerInfo{$device->controllerKey}{DiskCount} += 1;			
		}
	}
	
	my @dummyDiskInfo 				= ();
	my $dummyDiskSize				= 1.25; #while comparing we need to multiply by 1024.
	for ( my $i = 1000 ; $i < 1004 ; $i++ )
	{
		my %dummyDiskInfo			= ();
		if ( defined( $masterTargetControllerInfo{$i}{DiskCount} ) && ( 15 eq $masterTargetControllerInfo{$i}{DiskCount} ) )
		{
			next;
		}
		
		my( $minKeyVal , $maxKeyVal ) = ( 2000 + ( ($i%1000) * 16 ) , 2000 + ( ($i%1000) * 16 ) + 16 ); 
		
		for ( my $j = $minKeyVal ; $j < $maxKeyVal ; $j++ )
		{
			if ( $j == $minKeyVal + 7 )
			{
				next;
			}
		
			if ( ! exists $masterTargetDiskInfo{$j} )
			{
				$dummyDiskInfo{controllerKey}= $i;
				$dummyDiskInfo{unitNumber} 	 = ($j - 2000)%16;
				$dummyDiskInfo{unitNum}	 	 = ($j - 2000)%16;
				$dummyDiskInfo{scsiNum}	 	 = $i - 1000;
				$dummyDiskInfo{fileName} 	 = $folderPath."DummyDisk_".($i - 1000).( ($j - 2000)%16 ).".vmdk";

				my $isthisSizeExists	 = $dummyDiskSize * 1024;
				if (  exists $masterTargetDiskInfo{$isthisSizeExists} )
				{
					for( $dummyDiskSize +=  0.5 ; ; $dummyDiskSize += 0.5 )
					{
						if ( $dummyDiskSize >= 8 )
						{
							Common_Utils::WriteMessage("ERROR :: Unable to create a dummy disk with size less than 8MB.",3);
							Common_Utils::WriteMessage("ERROR :: Please remove small disk's on Master Target and re-run.",3);
							return $Common_Utils::FAILURE;
						}
						
						$isthisSizeExists = $dummyDiskSize *1024;
						if ( ! exists $masterTargetDiskInfo{$isthisSizeExists} )
						{
							last;
						}
					}
				}
				
				$dummyDiskInfo{fileSize} 	 = $isthisSizeExists;
				$dummyDiskSize			+= 0.5;
				
				$dummyDiskInfo{diskType}	= "false";
				$dummyDiskInfo{diskMode}	= "persistent";
				push @dummyDiskInfo	, \%dummyDiskInfo;
				last;
			}
		}
	}
	
	if( ( lc($vmxVersion) ge "vmx-10" ) && ( Common_Utils::GetViewProperty( $masterTargetView,'summary.config.guestFullName') =~ /Windows/i ) )
	{
		for ( my $i = 15000 ; $i < 15004 ; $i++ )
		{
			my %dummyDiskInfo			= ();
			if ( defined( $masterTargetControllerInfo{$i}{DiskCount} ) && ( 29 eq $masterTargetControllerInfo{$i}{DiskCount} ) )
			{
				next;
			}
			
			my( $minKeyVal , $maxKeyVal ) = ( 16000 + ( ($i%15000) * 30 ) , 16000 + ( ($i%15000) * 30 ) + 30 ); 
			
			for ( my $j = $minKeyVal ; $j < $maxKeyVal ; $j++ )
			{			
				if ( ! exists $masterTargetDiskInfo{$j} )
				{
					$dummyDiskInfo{controllerKey}= $i;
					$dummyDiskInfo{unitNumber} 	 = ($j - 16000)%30;
					$dummyDiskInfo{unitNum}	 	 = ($j - 16000)%30;
					$dummyDiskInfo{scsiNum}	 	 = $i - 15000 + 4;
					$dummyDiskInfo{fileName} 	 = $folderPath."DummyDisk_".($i - 15000 + 4).( ($j - 16000)%30 ).".vmdk";
	
					my $isthisSizeExists	 = $dummyDiskSize * 1024;
					if (  exists $masterTargetDiskInfo{$isthisSizeExists} )
					{
						for( $dummyDiskSize +=  0.5 ; ; $dummyDiskSize += 0.5 )
						{
							if ( $dummyDiskSize >= 8 )
							{
								Common_Utils::WriteMessage("ERROR :: Unable to create a dummy disk with size less than 8MB.",3);
								Common_Utils::WriteMessage("ERROR :: Please remove small disk's on Master Target and re-run.",3);
								return $Common_Utils::FAILURE;
							}
							
							$isthisSizeExists = $dummyDiskSize *1024;
							if ( ! exists $masterTargetDiskInfo{$isthisSizeExists} )
							{
								last;
							}
						}
					}
					
					$dummyDiskInfo{fileSize} 	 = $isthisSizeExists;
					$dummyDiskSize			+= 0.5;
					
					$dummyDiskInfo{diskType}	= "false";
					$dummyDiskInfo{diskMode}	= "persistent";
					push @dummyDiskInfo	, \%dummyDiskInfo;
					last;
				}
			}
		}
	}
	return ( $Common_Utils::SUCCESS , \@dummyDiskInfo );
}

#####CreateDisk#####
##Description 	:: Creates dummy disk which are in the range of 1.25 - 8 MB. These disk are used to find SCSI bus number inside 
##					Guest OS of master target. In this way we can send correct information of SCSI number inside master target
##					with out any assumptions.
##Input 		:: Array of hashes. Each has is for each disk to be created on Master Target.
##Output		:: Returns Input array by modifying up on SUCCESS or FAILURE.
#####CreateDisk#####
sub CreateDisk
{
	my $dummyDiskInfo	= shift;
	my $masterTargetView= shift;
	my $ReturnCode		= $Common_Utils::SUCCESS;
	
	my %ControllerInfo	= ();
	my %controllerType	= ();	
	my @deviceChange 	= ();
	
	my $vmxVersion 		= Common_Utils::GetViewProperty($masterTargetView, 'config.version');
	my $numEthernetCard	= Common_Utils::GetViewProperty($masterTargetView, 'summary.config.numEthernetCards');
	
	my $ControllerCount	= 4;#Max nunmber of SCSI controllers allowed till now are 4 per vm.But this value differs if vmxVersion < 7.
	if ( $vmxVersion lt "vmx-07" )
	{
		$ControllerCount= 5 - $numEthernetCard; 
	}
	if ( ( lc($vmxVersion) ge "vmx-10" ) && ( Common_Utils::GetViewProperty( $masterTargetView,'summary.config.guestFullName') =~ /Windows/i ) )
	{
		$ControllerCount= 8; #including SATA controllers
	}
	
	my $devices 		= Common_Utils::GetViewProperty( $masterTargetView,'config.hardware.device');
	foreach my $device ( @$devices )
	{
		if ( $device->deviceInfo->label =~ /SCSI controller/i )
		{
			$controllerType{scsi}	= ref ( $device );		
			$ControllerCount--;
			$ControllerInfo{ $device->key }	= ref ( $device );
		}
		elsif ( $device->deviceInfo->label =~ /SATA controller/i )
		{
			$controllerType{sata}	= ref ( $device );
			$ControllerCount--;
			$ControllerInfo{ $device->key }	= ref ( $device );
		}
	}
	
	if( !defined $controllerType{scsi} )
	{
		$controllerType{scsi}	= "VirtualLsiLogicSASController"; #linux coding required
	}
	
	if( !defined $controllerType{sata})
	{
		$controllerType{sata}	= "VirtualAHCIController";
	}
	
	if ( 0 == $ControllerCount )
	{
		Common_Utils::WriteMessage("SCSI controllers are already added to Master Target.",2);
	}
	else
	{
		for ( my $i = 0; ( ( $i < 4 ) && ( $ControllerCount != 0 ) ) ; $i++ )
		{
			my $key 	= 1000 + $i;
			if ( exists $ControllerInfo{$key} )
			{
				next;
			} 
			Common_Utils::WriteMessage("Constructing new SCSI Controller specification : busNumber => $i , controllerType => $controllerType{scsi} , controllerKey => $key.",2);
			my( $returnCode , $scsiControllerSpec ) = VmOpts::NewSCSIControllerSpec( busNumber => $i , sharing => "noSharing" , controllerType => $controllerType{scsi} , controllerKey => $key );
			if ( $returnCode != $Common_Utils::SUCCESS ) 
			{
				$ReturnCode =  $Common_Utils::FAILURE;
				last;
			}
			push @deviceChange , $scsiControllerSpec;
		}
		if ( ( lc($vmxVersion) ge "vmx-10" ) && ( Common_Utils::GetViewProperty( $masterTargetView,'summary.config.guestFullName') =~ /Windows/i ) )
		{
			for ( my $i = 0; ( ( $i < 4 ) && ( $ControllerCount != 0 ) ) ; $i++ )
			{
				my $key 	= 15000 + $i;
				if ( exists $ControllerInfo{$key} )
				{
					next;
				} 
				Common_Utils::WriteMessage("Constructing new SATA Controller specification : busNumber => $i , controllerType => $controllerType{sata} , controllerKey => $key.",2);
				my( $returnCode , $scsiControllerSpec ) = VmOpts::NewSATAControllerSpec( busNumber => $i , controllerType => $controllerType{sata} , controllerKey => $key );
				if ( $returnCode != $Common_Utils::SUCCESS ) 
				{
					$ReturnCode =  $Common_Utils::FAILURE;
					last;
				}
				push @deviceChange , $scsiControllerSpec;
			}
		}
	}
	
	my $key					= -1; 	
 	foreach my $diskInfo ( @$dummyDiskInfo )
 	{
	 	Common_Utils::WriteMessage("Creating disk $$diskInfo{fileName} of size $$diskInfo{fileSize} at SCSI unit number $$diskInfo{unitNumber} of type $$diskInfo{diskType} in mode $$diskInfo{diskMode}.",2);
		my $diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $$diskInfo{diskMode} , fileName => $$diskInfo{fileName} ,
																		thinProvisioned =>$$diskInfo{diskType} );
		if(( exists $$diskInfo{clusterDisk} ) && ("yes" eq $$diskInfo{clusterDisk} ))
		{
			$diskBackingInfo	= VirtualDiskFlatVer2BackingInfo->new( diskMode => $$diskInfo{diskMode}, fileName => $$diskInfo{fileName},
																		eagerlyScrub => "true", thinProvisioned => "false" );
		}
																		
		my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
		my $virtualDisk			= VirtualDisk->new( controllerKey => $$diskInfo{controllerKey} , unitNumber => $$diskInfo{unitNumber} ,key => $key, 
													backing => $diskBackingInfo, capacityInKB => $$diskInfo{fileSize} );
													
		my $virtualDeviceSpec 	= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('add'),device => $virtualDisk,
																	fileOperation => VirtualDeviceConfigSpecFileOperation->new('create'));
		push @deviceChange, $virtualDeviceSpec;
		$key--;
	}
	
	my $returnCode	= VmOpts::ReconfigVm( vmView => $masterTargetView, changeConfig => { deviceChange => \@deviceChange } );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	Common_Utils::WriteMessage("Dummy disks are successfully created to Master Target.",2);
	return ( $Common_Utils::SUCCESS , $dummyDiskInfo );
}

#####GetControllerNameMT#####
##Description 	:: Gets the Master Target controller Name from the View. The same Controller type has to be added to master Target for creating Dummy Disk.
##Input 		:: Master View.
##Output 		:: Returns SUCCESS as well controller Name or FAILURE.
#####GetControllerNameMT#####
sub GetControllerNameMT
{
	my $mtView 			= shift;
	my $deviceInfo		= Common_Utils::GetViewProperty( $mtView,'config.hardware.device');
	my $controller_name = "";
	my $controllerType 	= "";
	foreach my $device ( @$deviceInfo )
	{
		if ( exists $device->{scsiCtlrUnitNumber} ) #should have gone with the Controller Label.
		{
			if (! length( $controller_name) )
			{
				$controller_name 	 	= $device->deviceInfo->summary;
				$controllerType			= ref ( $device );
			}
			my $compare_to_controller 	= $device->deviceInfo->summary;
			if( !( $controller_name eq $compare_to_controller ) )
			{
				Common_Utils::WriteMessage("ERROR :: Master Target is having different SCSI controller types.",3);
				return ( $Common_Utils::FAILURE );
			}
		}
	}
	
	return ( $Common_Utils::SUCCESS , $controllerType );
}

#####CreateDummyDisk#####
##Description 	:: Creates Dummy disk during protection/Failback/Resume. Dummy Disk are used to find SCSI port and Target numbers at Guest OS level.
##Input 		:: None.
##Output 		:: Returns SUCCESS if creation of dummy disk is successful. 
sub CreateDummyDisk
{
	my %args									= @_;
	my %dummyDiskCreatedInfo					= ();

	my ( $returnCode , $masterTargetsInfo )		= XmlFunctions::ReadTargetXML( $args{ConfigFilePath} );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to read XML file.",3);
		return $Common_Utils::FAILURE;
	}
	
	foreach my $masterTargetInfo( @$masterTargetsInfo )
	{
		Common_Utils::WriteMessage("Creating Dummy Disk on Virtual Machine : $masterTargetInfo->{displayName}.",2);
		
		( $returnCode , my $masterTargetView )	= VmOpts::GetVmViewByUuid( $masterTargetInfo->{uuid} );
		if ( $Common_Utils::SUCCESS ne $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
		
		( $returnCode , my $dummyDiskInfo  )	= VmOpts::GetDummyDiskInfo( $masterTargetView );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find dummy disk information.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		my @tempArr	= @$dummyDiskInfo;
		if ( -1 != $#tempArr )
		{
			if( $Common_Utils::SUCCESS ne VmOpts::DeleteDummyDisk( $dummyDiskInfo , $masterTargetView ) )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to clear existing dummy disks. Clear all dummy disks on Master Target and re-run.",3);
				return ( $Common_Utils::FAILURE );
			}
		}
		$masterTargetView->ViewBase::update_view_data( VmOpts::GetVmProps() );
		
		undef $dummyDiskInfo;
		( $returnCode , $dummyDiskInfo ) 		= FindEmptyScsiSlotsandSizes( $masterTargetView , $masterTargetInfo->{vSphereHostName} , $masterTargetInfo->{dataCenter} ); 
		if( $Common_Utils::SUCCESS ne $returnCode  )
		{
			return ( $Common_Utils::FAILURE );
		}

		( $returnCode , $dummyDiskInfo )		= CreateDisk( $dummyDiskInfo , $masterTargetView );
		if ( $Common_Utils::SUCCESS ne $returnCode )
		{
			if( $Common_Utils::SUCCESS ne VmOpts::RemoveDummyDisk( $masterTargetView ) )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to clear dummy disk. Clear all dummy disks on Master Target and re-run.",3);
			}
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode, $masterTargetView )		= VmOpts::UpdateVmView( $masterTargetView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to get latest view of Master Target.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , $dummyDiskInfo ) = GetDummyDiskUuids( $masterTargetView, $dummyDiskInfo );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find dummy disk information.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		$dummyDiskCreatedInfo{$masterTargetInfo->{uuid}}{$masterTargetInfo->{hostName}}	= $dummyDiskInfo;  	
	}	
	
	$returnCode									= XmlFunctions::WriteDummyDiskInfo( $args{ConfigFilePath} ,\%dummyDiskCreatedInfo );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to write dummy disk information.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####GetDummyDiskUuids#####
##Description 		:: Find all the dummy disk uuids.
##Input 			:: Master Target Name.
##Output 			:: Return's 0 on success else 1.
#####GetDummyDiskUuids#####
sub GetDummyDiskUuids
{
	my $vmView			= shift;
	my $dummyDiskInfo	= shift;
	my $devices 		= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
		
	foreach my $dummyDisk ( @$dummyDiskInfo )
	{
		foreach my $device (@$devices)
		{
			if($device->deviceInfo->label =~ /Hard Disk/i)
			{	
				my $disk_name 				= $device->backing->fileName;
				if( $disk_name eq $dummyDisk->{fileName})
				{
					$dummyDisk->{diskUuid}	= $device->backing->uuid;
				}
			}
		}
	}
	return ( $Common_Utils::SUCCESS , $dummyDiskInfo );
}

#####Usage#####
##Description 		:: Display's usage information for CreateDummyDisk.pl file.
##Input 			:: None.
##Output 			:: None, displays format of calls that can be made to CreateDummyDisk.pl file.
#####Usage#####
sub Usage
{
	Common_Utils::WriteMessage("***** Usage of CreateDummyDisk.pl *****",3);
	Common_Utils::WriteMessage("---------------------------------------",3);
	Common_Utils::WriteMessage("To create Dummy Disk on Master Target -- >CreateDummyDisk.pl -server TARGET_SERVER_IP -username 'USERNAME' -password 'PASSWORD'",3);
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

my $ConfigFileName 			= "ESX.xml";
if ( ( Opts::option_is_set('dr_drill') ) && ( "yes" eq lc( Opts::get_option('dr_drill') ) ) )
{
	$ConfigFileName			= "$Common_Utils::DrDrillXmlFileName";
}

if($returnCode	!= $Common_Utils::SUCCESS )
{
	Common_Utils::WriteMessage("ERROR :: Failed to get credentials from CX.",3);
}
elsif ( ! ( Opts::option_is_set('username') && Opts::option_is_set('password') && Opts::option_is_set('server') ) )
{
	Common_Utils::WriteMessage("ERROR :: vCenter/vSphere IP,Username and Password values are the basic parameters required.",3);
	Common_Utils::WriteMessage("ERROR :: One or all of the above parameters are not set/passed to script.",3);
	Usage();
}
else
{
	$ReturnCode		= Common_Utils::Connect( Server_IP => $serverIp, UserName => Opts::get_option('username'), Password => Opts::get_option('password') );
	if ( $Common_Utils::SUCCESS == $ReturnCode )
	{
		eval
		{
			my $ConfigFilePath	= "$Common_Utils::vContinuumMetaFilesPath/$ConfigFileName";
			$ReturnCode = CreateDummyDisk( ConfigFilePath => $ConfigFilePath );
			if ( $Common_Utils::SUCCESS ne $ReturnCode )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to Create dummy disks.",3);
			}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			$ReturnCode			= $Common_Utils::FAILURE;
		}
	}
}
Common_Utils::LoggedData( tempLog => "$Common_Utils::vContinuumLogsPath/$logFileName" , planLog => $Common_Utils::vContinuumPlanDir );
exit( $ReturnCode );							 

__END__

Two cases are pending in this.
1. Before creating Dummy Disk we need find if any dummy disk exists or not and accordingly we need to  remove if any exists.
2. Removing the dummy disk created till point of failure.

