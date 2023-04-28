=head
	-----ReadinessChecks.pl-----
	1. This is going to be a replace ment for "ReadinessChecks.pl" file.
	2. This looks more rigid and each and every minute detail will be checked as part of this.
	3. This employs a new mechanism than which is already there in old "ReadinessChecks.pl" file.
=cut
use strict;
use warnings;
use FindBin qw( $Bin );
use lib $FindBin::Bin;
use Common_Utils;
use VmOpts;
use HostOpts;
use ResourcePoolOpts;
use DatastoreOpts;
use DatacenterOpts;
use ComputeResourceOpts;
use FolderOpts;
use XmlFunctions;
use Data::Dumper;

my %customopts = (
				  incremental_disk 		=> {type => "=s", help => "Set to Yes, if performing runreadiness checks for addition of disk?"		, required => 0,},
				  failback 				=> {type => "=s", help => "Set to Yes, if performing runreadiness checks for Failback?"				, required => 0,},
				  resume		 		=> {type => "=s", help => "Set to Yes, if performing runreadiness checks for Resume?"				, required => 0,},
				  offline_sync_export 	=> {type => "=s", help => "Set to Yes, if performing runreadiness checks for Offline Sync Export?" 	, required => 0,},
				  offline_sync_import	=> {type => "=s", help => "Set to Yes, if performing runreadiness checks for Offline Sync Import?"  , required => 0,},
				  dr_drill				=> {type => "=s", help => "Set to Yes, if performing runreadiness checks for Dr-Drill?"				, required => 0,},
				  target				=> {type => "=s", help => "Set to Yes, if collecting information of protected machine at DR site?"	, required => 0,},
				  dr_drill_array		=> {type => "=s", help => "Set to Yes, if performing runreadiness checks for array based Dr-Drill?"	, required => 0,},
				  array_datastore		=> {type => "=s", help => "To create datastore of array based lun vsnap, set this argument to 'yes'.", required => 0,},
				  );

#####DifferentViews#####
my $vmViews 			= "";
my $hostViews			= "";
my $datacenterViews 	= "";
my $resourcePoolViews	= "";
my $folderViews 		= "";
my $computeResourceViews= "";

my %DatacentersInfo		= ();

#####RunReadinessChecks#####
##Description		:: Runs Readinesschecks required in case of each of the opertion. Operation can be Protection, Addition of Disk
##						Recovery, Resume, DrDrill, Failback.
##Input 			:: Args which state that which operation is set.
##Output 			:: Returns SUCCESS else FAILURE.
#####RunReadinessChecks#####
sub RunReadinessChecks
{
	my %args 						= @_;
	my $doesChecksPassed			= $Common_Utils::SUCCESS;
	my $isDisksThin					= "false";
	
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
	
	foreach my $plan ( @$planNodes )
	{
		my $planName 				= $plan->getAttribute('plan');
		my @sourceNodes				= $plan->getElementsByTagName('SRC_ESX');
		my @TargetNodes				= $plan->getElementsByTagName('TARGET_ESX');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		Common_Utils::WriteMessage("Validating plan $planName.",2);
		if ( $#TargetNodes > 0 )
		{
			Common_Utils::WriteMessage("ERROR :: \"$planName\" contains multiple target nodes.",3);
			Common_Utils::WriteMessage("ERROR :: Please make sure that in a plan single master target is selected.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		my @sourceHosts				= $sourceNodes[0]->getElementsByTagName('host');
		my @TargetHost 				= $TargetNodes[0]->getElementsByTagName('host');
		
		my ( $returnCode , $mtView )= VmOpts::GetVmViewByUuid( $TargetHost[0]->getAttribute('source_uuid') );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		my %masterTargetDetails		= ();
		$returnCode 				= GetLatestDetails( $TargetHost[0] , \%masterTargetDetails , $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed		= $Common_Utils::FAILURE;
		}
		
		$returnCode 				= XmlFunctions::ValidateNode( $TargetHost[0] , "target" );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed		= $Common_Utils::FAILURE;
		}
		
		( $returnCode , my $datacenterView )	= DatacenterOpts::GetDataCenterView( $masterTargetDetails{datacenterName} );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode ,  $vmViews )				= VmOpts::GetVmViewInDatacenter( $datacenterView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , my $tgtHostView)		= HostOpts::GetSubHostView( $masterTargetDetails{vSphereHostName} );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}

		if ( !exists $DatacentersInfo{ $masterTargetDetails{datacenterName} } )
		{
			$returnCode 			= GetAllDetailsOfDatacenter( $datacenterView , $tgtHostView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
		}
		
		my $vmView 					= "";
		my %selectedDisks			= ();	
		foreach my $sourceHost ( @sourceHosts )
		{
			my $machineName 		= $sourceHost->getAttribute("display_name");
			Common_Utils::WriteMessage("Performing checks on machine $machineName.",2);
			
			if ( ( defined ( $sourceHost->getAttribute('new_displayname') ) ) && ( "" ne $sourceHost->getAttribute('new_displayname') ) )
			{
				$machineName		= $sourceHost->getAttribute('new_displayname');
			}
			
			if ( ( defined $args{failback} ) && ( 'yes' eq lc ( $args{failback} ) ) )
			{
				$returnCode 		= RemoveFolderPath( sourceHost => $sourceHost );
			}
						
			( $returnCode, $vmView )= CheckIfMachineExists( $sourceHost , $args{incrementalDisk} );
			if ( ( $returnCode != $Common_Utils::EXIST ) && ( ( ( defined $args{resume} ) && ( "yes" eq lc( $args{resume} ) ) ) || ( ( defined $args{incrmentalDisk} ) && ( "yes" eq lc( $args{incrmentalDisk} ) ) ) ) )
			{
				Common_Utils::WriteMessage("Machine $machineName does not exist on target side. Considering this as an Error.",3);
				return( $Common_Utils::FAILURE );
			}
			
			if ( $Common_Utils::SUCCESS	!= AreSelectedDatastoresAccessible( machineName => $machineName , sourceHost => $sourceHost ,
																		 hostView => $tgtHostView , datacenterName => $masterTargetDetails{datacenterName} ) )
			{
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			
			my 	$existingDisksInfo	= [];
			if ( $returnCode == $Common_Utils::NOTFOUND )
			{
				Common_Utils::WriteMessage("Machine $machineName does not exist on target site.Checking for display name.",2);
				$sourceHost->setAttribute("target_uuid","");
				
				if ( ! defined $args{offlineSyncExport} )
				{
					( $returnCode , $vmView )	= VmOpts::GetVmViewByNameInDatacenter( machineName => $machineName , datacenterView => $datacenterView  );
					if ( $returnCode != $Common_Utils::NOTFOUND )
					{
						Common_Utils::WriteMessage("ERROR :: Already a machine with same name($machineName) exists in target hosts datacenter.",3);
						Common_Utils::WriteMessage("ERROR :: 1. Either have source and target in different datacenter.",3);
						Common_Utils::WriteMessage("ERROR :: 2. Change name of the machine using vContinuum 'change display name at target side' procedure.",3);
						$doesChecksPassed	= $Common_Utils::FAILURE;
					}
				
					my @disksInfo			= $sourceHost->getElementsByTagName('disk');
					my $datastoreSelected 	= $disksInfo[0]->getAttribute('datastore_selected');
					my $vmxPathName 		= $sourceHost->getAttribute('vmx_path');
					my @vmxPathSplit		= split( /\[|\]/ , $vmxPathName );
					my $vmPathName 			= "[$datastoreSelected]$vmxPathSplit[-1]";
					$isDisksThin			= $disksInfo[0]->getAttribute('thin_disk');
					
					( $returnCode , $vmView ) 	= VmOpts::GetVmViewByVmPathNameInDatacenter( vmPathName => $vmPathName , datacenterView => $datacenterView );
					if ( $returnCode != $Common_Utils::NOTFOUND )
					{
						Common_Utils::WriteMessage("ERROR :: Already a machine exists with same path \"$vmPathName\".",3);
						Common_Utils::WriteMessage("ERROR :: 1. Select another datastore or un-select machine \"$machineName\".",3);
						Common_Utils::WriteMessage("ERROR :: 2. Re-run readiness checks after performing above changes.",3);
						$doesChecksPassed	= $Common_Utils::FAILURE;
					}
					
					$returnCode 			= DoesFolderExist( sourceHost => $sourceHost, targetHostView => $tgtHostView , 
																	datastoreSelected => $datastoreSelected, datacenterName => $masterTargetDetails{datacenterName} );
					if ( $returnCode != $Common_Utils::NOTFOUND )
					{
						$doesChecksPassed	= $Common_Utils::FAILURE;
					}
				}
			}
			elsif( $returnCode == $Common_Utils::EXIST )
			{
				my $machineUuid 			= $sourceHost->getAttribute('target_uuid');
				if ( ( defined $args{failback} ) && ( "yes" eq lc( $args{failback} ) ) )
				{
					$machineUuid			= $sourceHost->getAttribute('source_uuid');
				}
				
				( $returnCode , $vmView )	= VmOpts::GetVmViewByUuid( $sourceHost->getAttribute('target_uuid') );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$doesChecksPassed 		= $Common_Utils::FAILURE;
				}
				
				$returnCode 				= IsMachineRunning( vmView => $vmView );
				if ( $returnCode != $Common_Utils::FAILURE )
				{
					Common_Utils::WriteMessage("ERROR :: \"$machineName\" is up and running. Please shut down.",3);
					$doesChecksPassed 		= $Common_Utils::FAILURE;
				}	
				
				( my ($returnCode, $ideCount, $rdm, $cluster), $existingDisksInfo, my $floppyDevices ) 
											= VmOpts::FindDiskDetails( $vmView, [] ); 
			    if( $returnCode != $Common_Utils::SUCCESS )
				{
					Common_Utils::WriteMessage("ERROR :: Failed to get disks information of machine \"$machineName\".",3);
					$doesChecksPassed 		= $Common_Utils::FAILURE;
				}
				
				$returnCode 				= AreDisksInUseByAnotherVM( sourceHost => $sourceHost , uuid => $machineUuid );
				if ( $returnCode != $Common_Utils::FAILURE )
				{
					Common_Utils::WriteMessage("ERROR :: Disks of machine \"$machineName\" are in use by another virtual machine.",3);
					Common_Utils::WriteMessage("ERROR :: 1. Remove disks from another machine if they have to be re-used.",3);
					Common_Utils::WriteMessage("ERROR :: 2. If machine is to be created new at DR, choose different folder or datastore.",3);
					$doesChecksPassed 		= $Common_Utils::FAILURE;
				}
				
			}
			elsif ( $returnCode == $Common_Utils::FAILURE )
			{
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			
			$returnCode 			= XmlFunctions::ValidateNode( $sourceHost , "source" );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			
			$returnCode 			= DecreaseFreeSlotsOnMasterTarget( $sourceHost , \%masterTargetDetails, \%selectedDisks );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$doesChecksPassed 	= $Common_Utils::FAILURE;
			}
			
			$returnCode 			= CommonChecksAtTarget( machineName => $machineName, sourceHost => $sourceHost , 
															datacenterName => $masterTargetDetails{datacenterName} , 
															existingDiskInfo => $existingDisksInfo , failback => $args{failback},
															selectedDisks => \%selectedDisks );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
		}
		
		$returnCode 				= ChecksOnMasterTarget( \%masterTargetDetails );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed 		= $Common_Utils::FAILURE;
		}
		
		$returnCode 				= CheckVmBiosHddOrder( $mtView, \%masterTargetDetails );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
#			$doesChecksPassed 		= $Common_Utils::FAILURE;
		}
	}
	
	if( lc($isDisksThin) eq "false" )
	{
		$returnCode 			= CheckSpaceAvailabilityOnTarget();
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed 	= $Common_Utils::FAILURE ;
		}
	}
	
	my @docElements 		= $docNode;
	$returnCode				= XmlFunctions::SaveXML( \@docElements , $args{file} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		$doesChecksPassed 	= $Common_Utils::FAILURE ;
	}
	return ( $doesChecksPassed );
}

#####MapSourceScsiNumbersOfHost#####
##Description			:: It Maps the each and every disk selected for protection with it's corresponding SCSI number feteched through CX-API.
##Input 				:: Source Host Node. It uses source_hostid and collects information.
##Output 				:: Returns SUCCESS else FAILURE.
#####MapSourceScsiNumbersOfHost#####
sub MapSourceScsiNumbersOfHost
{
	my %args 			= @_;
	my $ReturnCode		= $Common_Utils::SUCCESS;
	
	if ( ! defined $args{host}->getAttribute('inmage_hostid') )
	{
		Common_Utils::WriteMessage("ERROR :: Host Id value is not set for machine $args{machineName}.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}

	my $hostId 			= $args{host}->getAttribute('inmage_hostid');
	my %subParameters	= ( InformationType => "storage", HostGUID => $hostId );
	my( $returnCode	, $requestXml )	= XmlFunctions::ConstructXmlForCxApi( functionName => "GetHostInfo" , subParameters => \%subParameters );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to prepare XML file.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}

	( $returnCode ,my $responseXml )= Common_Utils::PostRequestXml( requestXml => $requestXml , cxIp => $args{cxIpAndPortNum}, functionName => "GetHostInfo" );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		$ReturnCode		= $Common_Utils::FAILURE;
		return( $ReturnCode );	
	}

	( $returnCode , my $diskList )	= XmlFunctions::ReadResponseOfCxApi( response => $responseXml , parameter =>"no" , parameterGroup => 'DiskList' );		
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to read response of CX API.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}	
	
	$returnCode 		= VmOpts::MapScsiInfo( host => $args{host} , diskList => $diskList );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to Map SCSI information for machine \"$args{machineName}\".",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}
		
	return ( $ReturnCode );
}

#####AreSelectedDatastoresAccessible#####
##Description 			:: Checks whether selected datastores are accessible. By searching it and creating a folder.
##Input 				:: Machine Name , details of Source Host Node , target Host View and datacenter name of Target.
##Output 				:: Returns SUCCESS else FAILURE.
#####AreSelectedDatastoresAccessible#####
sub AreSelectedDatastoresAccessible
{
	my %args 			= @_;
	my $ReturnCode		= $Common_Utils::SUCCESS;
	
	my @disksInfo 				= $args{sourceHost}->getElementsByTagName('disk');
	for( my $i = 0 ; $i <= $#disksInfo ; $i++ )
	{
		my $datastoreToBeUsed	= $disksInfo[$i]->getAttribute('datastore_selected');
		my $diskNeedToBeChecked = "[$datastoreToBeUsed]";
		
		my $datastoresInfo		= $DatacentersInfo{ $args{datacenterName} }->{datastoreInfo};
		foreach my $datastore ( @$datastoresInfo )
		{
			if ( $$datastore{symbolicName} ne $datastoreToBeUsed )
			{
				next;
			}
			
			if ( (  exists $$datastore{isAccessible} ) && ( exists $$datastore{readOnly} ) )
			{
				next;
			}
			
			my @listDatastoreInfo = ( $datastore );	
			my( $returnCode , $listDatastoreInfo ) 	= DatastoreOpts::ValidateDatastores( $args{hostView} , \@listDatastoreInfo , $args{datacenterName} );
			if ( $returnCode != $Common_Utils::SUCCESS ) 
			{
				$ReturnCode 	= $Common_Utils::FAILURE;
			}
			$datastore			= ${$listDatastoreInfo}[0];
			last;
		}
	}
	
	return ( $ReturnCode );
} 

#####AreDisksInUseByAnotherVM#####
##Description 			:: checks whether disks are in use by another machine?
##Input 				:: Source Host Node.
##Output 				:: Returns SUCCESS if disks are in use by another machine, else FAILURE. Another machine can even be master target.
#####AreDisksInUseByAnotherVM#####
sub AreDisksInUseByAnotherVM
{
	my %args 			= @_;
	my $ReturnCode 		= $Common_Utils::FAILURE;
	my @disks	 		= $args{sourceHost}->getElementsByTagName('disk');
	foreach my $disk ( @disks )
	{
		if ( ( defined $disk->getAttribute('protected') ) && ( "yes" eq lc ( $disk->getAttribute('protected') ) ) )
		{
			next;
		}
		foreach my $vmView ( @$vmViews )
		{
			if ( ( $vmView->name =~ /^unknown$/i ) || ( Common_Utils::GetViewProperty( $vmView, 'guest.guestState') =~ /^unknown$/i ) || ( Common_Utils::GetViewProperty( $vmView, 'summary.runtime.connectionState')->val =~ /^orphaned$/i ) )
			{
				next;
			}
			
			if ( ( $args{uuid} eq Common_Utils::GetViewProperty( $vmView,'summary.config.uuid') ) || ( 'poweredon' ne lc ( Common_Utils::GetViewProperty( $vmView,'summary.runtime.powerState')->val ) ) )
			{
				next;
			}
			
			my $diskName 	= $disk->getAttribute('disk_name');
			my @datastore	= split( /\[|\]/ , $diskName );
			my $dsSelected	= $disk->getAttribute('datastore_selected');
			$datastore[-1]	=~ s/^\s+//;
			$diskName 	 	= "[$dsSelected] $datastore[-1]";
			if ( ( defined $args{sourceHost}->getAttribute('vmDirectoryPath') ) && ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )   
			{
				$diskName 	= "[$dsSelected] ".$args{sourceHost}->getAttribute('vmDirectoryPath')."/$datastore[-1]";
			} 
			my $displayName = $vmView->name;
			my $devices 	= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
			my $disk_name 	= "";
			foreach my $device ( @$devices )
			{
				if ( $device->deviceInfo->label !~ /Hard Disk/i )
				{
					next;
				}
				$disk_name 	=  $device->backing->fileName;
				if(defined $vmView->snapshot)
				{
					my $final_slash_index 	= rindex($disk_name, "/");
					my $filename 			= substr($disk_name,$final_slash_index + 1,length($disk_name));
					if ( $filename =~ /-[0-9]{6}\.vmdk$/ )
					{
						my $final_dash_index =  rindex($filename, "-");
						if ( $final_dash_index  != -1 )
						{
							$filename 	= substr($filename,0,$final_dash_index);
							$disk_name 	= substr($disk_name,0,$final_slash_index + 1);
							$disk_name 	= $disk_name .  $filename . ".vmdk";
						}
					}
				}
				if ( $diskName eq $disk_name )
				{
					Common_Utils::WriteMessage("ERROR :: \"$diskName\" is in use by virtual machine \"$displayName\".",3);
					$ReturnCode	= $Common_Utils::SUCCESS;
				}
			}					
		}
	}
	
	return ( $ReturnCode );
}

#####IsMachineRunning#####
##Description 			:: Checks if the machine is up and running.
##Input 				:: virtual machine view.
##Output 				:: Returns SUCCESS if machine is up and running, else FAILURE.
#####IsMachineRunning#####
sub IsMachineRunning
{
	my %args			= @_;
	
	if ( 'poweredon' eq lc( Common_Utils::GetViewProperty( $args{vmView},'summary.runtime.powerState')->val ) )
	{
		return ( $Common_Utils::SUCCESS );
	} 
	return ( $Common_Utils::FAILURE );
}

#####RemoveFolderPath#####
##Description			:: Deletes attribute 'vmDirectoryPath' attribute.
##Input 				:: Source XML Node.
##Output 				:: Returns SUCCESS else FAILURE.
#####RemoveFolderPath#####
sub RemoveFolderPath
{
	my %args 			= @_;
	
	my $oldVmFolder		= "";
	if ( ( defined $args{sourceHost}->getAttribute('old_vm_folder_path') ) && ( "" ne $args{sourceHost}->getAttribute('old_vm_folder_path') ) )
	{
		$oldVmFolder	= $args{sourceHost}->getAttribute('old_vm_folder_path');
		Common_Utils::WriteMessage("Vm old folder path= $oldVmFolder.",2);
	}	
	elsif ( (! defined $args{sourceHost}->getAttribute('vmDirectoryPath') ) || ( "" eq $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
	{
		Common_Utils::WriteMessage("vm Directory path is empty.",2);
		return ( $Common_Utils::SUCCESS );
	}
	
	my $vmDirectoryPath	= $args{sourceHost}->getAttribute('vmDirectoryPath');
	my @disks			= $args{sourceHost}->getElementsByTagName('disk');
	if ( -1 == $#disks )
	{
		Common_Utils::WriteMessage("No disk are listed for vmDirectoryPath removal.",2);
		return ( $Common_Utils::SUCCESS );
	}
	
	my $folderPath 		= $args{sourceHost}->getAttribute('folder_path');
	my @splitOnSlash	= split ( /\// , $folderPath );
	my $newFolderPath 	= "";
	for ( my $i = 1; $i <= $#splitOnSlash ; $i++ )
	{
		if ( "" eq $newFolderPath )
		{
			$newFolderPath	= $splitOnSlash[$i];
			next;
		}
		$newFolderPath	= "$newFolderPath/$splitOnSlash[$i]";
	} 
	$newFolderPath		= "$newFolderPath/";
	
	if( $oldVmFolder ne "")
	{
		@splitOnSlash	= split ( /\// , $oldVmFolder );
		my $tempFolderPath 	= "";
		for ( my $i = 0; $i <= $#splitOnSlash ; $i++ )
		{
			if ( "" eq $tempFolderPath )
			{
				$tempFolderPath	= $splitOnSlash[$i];
				next;
			}
			$tempFolderPath	= "$tempFolderPath/$splitOnSlash[$i]";
		} 
		$newFolderPath		= "$tempFolderPath/";
	}
	Common_Utils::WriteMessage("Vm new folder path= $newFolderPath.",2);
	
	my $vmPathName 		= $args{sourceHost}->getAttribute('vmx_path');
	my @datastore 		= split( /\[|\]/, $vmPathName );
	my $final_slash 	= rindex( $datastore[2] , "/" );
	my $vmxFileName		= substr( $datastore[2] , $final_slash + 1 , length( $datastore[2] ) );
	$vmPathName			= "[$datastore[1]] $newFolderPath".$vmxFileName;
		
	foreach my $disk ( @disks )
	{
		my $diskName 	= $disk->getAttribute('disk_name');
		my $final_slash = rindex( $diskName , "/" );
		my $vmdkName 	= substr( $diskName , $final_slash + 1 , length( $diskName ) );
		$diskName 		= "[$datastore[1]] $newFolderPath".$vmdkName;
		$disk->setAttribute( 'disk_name' , $diskName );
	}
	$args{sourceHost}->setAttribute( 'vmx_path' , $vmPathName );
	$args{sourceHost}->setAttribute( 'vmDirectoryPath' ,"" );
	$args{sourceHost}->setAttribute( 'folder_path' , $newFolderPath );
	
	return ( $Common_Utils::SUCCESS );
}

#####DoesFolderExist#####
##Description			:: Checks whether the folder which need to be created already exists on target datastore?
##Input 				:: XML node ( Host Node ), Target host View and Datastore Name.
##Output 				:: Returns SUCCESS if folder exists, else NOTFOUND.
#####DoesFolderExist#####
sub DoesFolderExist
{
	my %args			= @_;
	my $ReturnCode		= $Common_Utils::NOTFOUND;

	my $datastoresInfo	= $DatacentersInfo{ $args{datacenterName} }->{datastoreInfo};
	foreach my $datastore ( @$datastoresInfo )
	{
		if ( $$datastore{symbolicName} ne $args{datastoreSelected} )
		{
			next;
		}
		
		if ( "yes" ne lc( $$datastore{isAccessible} ) )
		{
			next;
		}
		my @folderFileInfo	= @{$$datastore{folderFileInfo}};
		my $folderName 		= $args{sourceHost}->getAttribute('folder_path');

		if ( ( defined $args{sourceHost}->getAttribute('vmDirectoryPath') ) && ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
		{
			my $vmDirPath 	= $args{sourceHost}->getAttribute('vmDirectoryPath');
			if ( grep "$_" eq $args{sourceHost}->getAttribute('vmDirectoryPath') , @folderFileInfo )
			{
				Common_Utils::WriteMessage("vmDirectoryPath \"$vmDirPath\" already exists.",2);
				my( $returnCode, $folderFileInfo, $fileInfo )	= DatastoreOpts::FindFileInfo( $args{targetHostView} , "*" , "[$args{datastoreSelected}] $vmDirPath/" , "folderfileinfo" );
				if ( $returnCode eq $Common_Utils::SUCCESS )
				{
					if ( grep "$_/" eq $args{sourceHost}->getAttribute('folder_path') , @$folderFileInfo )
					{
						Common_Utils::WriteMessage("ERROR :: folder \"$folderName\" already exists.",3);
						$ReturnCode 	= $Common_Utils::FAILURE;						
					}
				}
			}
		}
		else
		{
			Common_Utils::WriteMessage("Checking for folder path $folderName in datastore $args{datastoreSelected}.",2);
			if ( grep "$_/" eq $args{sourceHost}->getAttribute('folder_path') , @folderFileInfo )
			{
				Common_Utils::WriteMessage("ERROR :: folder \"$folderName\" already exists.",3);
				$ReturnCode 	= $Common_Utils::FAILURE;						
			}
		} 
	}
	return ( $ReturnCode );
}

#####CheckSpaceAvailabilityOnTarget#####
##Description			:: Checks if reuired fres space is less then available free space.
##Input 				:: None.
##Output 				:: Returns SUCCESS up on availability else FAILURE.
#####CheckSpaceAvailabilityOnTarget#####
sub CheckSpaceAvailabilityOnTarget
{
	my $ReturnCode 		= $Common_Utils::SUCCESS;
	foreach my $datacenterName ( sort keys %DatacentersInfo )
	{
		if ( ! defined $DatacentersInfo{$datacenterName}->{datastoreInfo} )
		{
			next;
		} 
		
		my $datastoreInfo 	= $DatacentersInfo{$datacenterName}->{datastoreInfo};
		foreach my $datastore ( @$datastoreInfo )
		{
			if ( !defined $datastore->{requiredSpace} )
			{
				next;
			}
			
			my $datastoreName	= $datastore->{symbolicName}; 
			my $freeSpace		= $datastore->{freeSpace};
			my $requiredSpace 	= $datastore->{requiredSpace};
			Common_Utils::WriteMessage("DatastoreName = $datastoreName, Required space = $requiredSpace ,free space = $freeSpace.",2);
			if ( $datastore->{freeSpace} < $datastore->{requiredSpace} )
			{
				Common_Utils::WriteMessage("ERROR :: Space required on datastore $datastoreName = $requiredSpace GB.",3);
				Common_Utils::WriteMessage("ERROR :: Free space available on datastore $datastoreName = $freeSpace GB",3);
				Common_Utils::WriteMessage("ERROR :: Please navigate back to datastore selection screen and choose a different datastore for some of the machines.",3);
				$ReturnCode 	= $Common_Utils::FAILURE;
			}
		}
	}
	return ( $ReturnCode );
}

#####ChecksOnMasterTarget#####
##Description			:: Checks whether the master Target is having free slots, are all controllers are of same type and in case
##							of linux is UUID enabled?
##Input 				:: Master Target Details.
##Output 				:: Returns SUCCESS else FAILURE.
#####ChecksOnMasterTarget#####
sub ChecksOnMasterTarget
{
	my $masterTargetDetails		= shift;
	my $checksPassedOnMT		= $Common_Utils::SUCCESS;
	#checking MT ram size
	if( $$masterTargetDetails{memsize} < 512 )
	{
		Common_Utils::WriteMessage("ERROR :: RAM size of master target \"$$masterTargetDetails{name}\" is \"$$masterTargetDetails{memSize}\".",3);
		Common_Utils::WriteMessage("ERROR :: Minimum supported RAM size is 512MB.Increase RAM size and rerun readinesschecks.",3);
		$checksPassedOnMT		= $Common_Utils::FAILURE;
	}
	#checking controller name for linux
	if($$masterTargetDetails{osInfo} =~ /Linux/i && $$masterTargetDetails{scsiControllerName} ne "VirtualLsiLogicController")
	{
		Common_Utils::WriteMessage("ERROR :: Recommended ScsiController type for Linux MT is \"LSI LOGIC PARALLEL\" .",3);
		Common_Utils::WriteMessage("ERROR :: Please change the ScsiController type from \"$$masterTargetDetails{scsiControllerName}\" to \"LSI LOGIC PARALLEL\" for master target \"$$masterTargetDetails{name}\" and rerun readinesscheck.",3);
		$checksPassedOnMT		= $Common_Utils::FAILURE;
	}
	Common_Utils::WriteMessage("Free Slots = $$masterTargetDetails{freeSlots}, Slots required = $$masterTargetDetails{slotsRequired}.",2);
	if ( $$masterTargetDetails{freeSlots} < $$masterTargetDetails{slotsRequired} )
	{
		Common_Utils::WriteMessage("ERROR :: Free slots to add disk on master target = $$masterTargetDetails{freeSlots}.",3);
		Common_Utils::WriteMessage("ERROR :: Number of slots required on master target = $$masterTargetDetails{slotsRequired}.",3);
		Common_Utils::WriteMessage("ERROR :: Please choose a master target which has free slots equal to or greater than required number.",3);
		$checksPassedOnMT		= $Common_Utils::FAILURE;
	}
	
	if ( "yes" ne lc( $$masterTargetDetails{sameControllerTypes} ) )
	{
		Common_Utils::WriteMessage("ERROR :: $$masterTargetDetails{name} contains scsi controllers of different types.",3);
		Common_Utils::WriteMessage("ERROR :: Please choose a machine which contain scsi contollers of same type as master target.",3);
		$checksPassedOnMT		= $Common_Utils::FAILURE;
	}
	
	return ( $checksPassedOnMT );
}

#####DecreaseFreeSlotsOnMasterTarget#####
##Description 			:: It decreases the free slots on Master target after finding free slots.
##Input 				:: MachineInfo and .
##Output 				:: Returns SUCCESS or FAILURE.
#####DecreaseFreeSlotsOnMasterTarget#####
sub DecreaseFreeSlotsOnMasterTarget
{
	my $machineInfo 		= shift;
	my $masterTargetDetails	= shift;
	my $selectedDisks		= shift;
	
	my $clusterNodes	= "";
	my $inmageHostId	= $machineInfo->getAttribute('inmage_hostid');
	if( "yes" eq $machineInfo->getAttribute('cluster') )
	{
		$clusterNodes	= $machineInfo->getAttribute('clusternodes_inmageguids');
	}
	
	my @disksInfo			= $machineInfo->getElementsByTagName('disk');
	
	if ( ! defined $$masterTargetDetails{freeSlots} )
	{
		$$masterTargetDetails{freeSlots}	= 60 - $$masterTargetDetails{numVirtualDisks};
		if ( "vmx-04" eq $$masterTargetDetails{vmxVersion} )
		{
			my $freeController 				= 4 - $$masterTargetDetails{numEthernetCards};
			my $maximumDiskCount			= $freeController * 15;
			$$masterTargetDetails{freeSlots}= $maximumDiskCount - $$masterTargetDetails{numVirtualDisks}; 		
		}
		if( ( $$masterTargetDetails{osInfo} =~ /Windows/i) && ( $$masterTargetDetails{vmxVersion} ge "vmx-10" ) )
		{
			$$masterTargetDetails{freeSlots}= 60 - $$masterTargetDetails{numVirtualDisks}; #180 		
		}
	}
	
	foreach my $disk ( @disksInfo )
	{
		if ( "yes" eq lc ( $disk->getAttribute('protected') ) )
		{
			next;
		}
		
		my $diskUuid	= $disk->getAttribute('source_disk_uuid');
		if ( "physicalmachine" eq lc( $machineInfo->getAttribute('machinetype') ) )
		{
			$diskUuid	= $disk->getAttribute('disk_signature');
		}
		
		if( ( defined $$selectedDisks{$diskUuid} ) )
		{
			my $clusterDisk	= $$selectedDisks{$diskUuid};
		    if( $clusterNodes =~ /$$clusterDisk{inmageHostId}/i )
			{
				next;
			}
		}
		
		$$masterTargetDetails{slotsRequired}+= 1;	
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####IsDatastoreAccessibleByMasterTarget#####
##Description			:: Check whether protected machines datastore is accessbile by host on master target resides.
##Input 				:: Datastore Name and datacenter where this has to be checked.
##Output 				:: Returns EXIST when datastore is accessible else NOTFOUND.
#####IsDatastoreAccessibleByMasterTarget#####
sub IsDatastoreAccessibleByMasterTarget
{
	my $datastoreName 	= shift;
	my $datacenterName 	= shift;
	
	if ( ! exists $DatacentersInfo{ $datacenterName } )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find the datacenter information of $datacenterName.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	if ( !exists $DatacentersInfo{ $datacenterName }->{datastoreInfo} )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find datastore information under datacenter $datacenterName.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my $datastoreStoreFound	= $Common_Utils::NOTFOUND;
	my $datastoresInfo		= $DatacentersInfo{ $datacenterName }->{datastoreInfo};
	foreach my $datastore ( @$datastoresInfo )
	{
		if ( $$datastore{symbolicName} eq $datastoreName )
		{
			$datastoreStoreFound= $Common_Utils::EXIST;
			last;
		}
	}
	
	if ( $datastoreStoreFound eq $Common_Utils::NOTFOUND  )
	{
		return ( $Common_Utils::NOTFOUND );
	}
	return ( $Common_Utils::EXIST );
}

#####OfflineSyncImportChecksAtTarget#####
##Description	 		:: Checks whether the source Host Folder exists in InMage_Offline_Sync_Folder.
##							checks whether there exists multiple Offline Sync folders in target datastores.
##Input 				:: Source Node and Target Host DatacenterName.
##Ouput 				:: Returns SUCCESS else FAILURE.
#####OfflineSyncImportChecksAtTarget#####
sub OfflineSyncImportChecksAtTarget
{
	my %args 				= @_;
	my $doesChecksPassed	= $Common_Utils::SUCCESS;
	
	Common_Utils::WriteMessage("Offline Sync Import: Validating file $args{file}.",2);
	
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
	
	( $returnCode , $vmViews , $hostViews , $datacenterViews , $resourcePoolViews )	= Common_Utils::GetViews();
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get views of different managed objects in a vCenter/vsphere.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	foreach my $plan ( @$planNodes )
	{
		my $planName 				= $plan->getAttribute('plan');
		my @sourceNodes				= $plan->getElementsByTagName('SRC_ESX');
		my @TargetNodes				= $plan->getElementsByTagName('TARGET_ESX');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');

		if ( $#TargetNodes > 0 )
		{
			Common_Utils::WriteMessage("ERROR :: \"$planName\" contains multiple target nodes.",3);
			Common_Utils::WriteMessage("ERROR :: Please make sure that in a plan single master target is selected.",3);
			return ( $Common_Utils::FAILURE );
		}
		Common_Utils::WriteMessage("Validating plan $planName.",2);
		my @sourceHosts				= $sourceNodes[0]->getElementsByTagName('host');
		my @TargetHost 				= $TargetNodes[0]->getElementsByTagName('host');
		
		my $targetvSphereHostName 	= $TargetHost[0]->getAttribute('vSpherehostname');
		if ( "" eq $targetvSphereHostName )
		{
			Common_Utils::WriteMessage("ERROR :: Found empty value for target vSphere host name in file $args{file}",3);
			return ( $Common_Utils::FAILURE );
		}
		my $mtdatastore 			= $TargetHost[0]->getAttribute('datastore');
		if ( "" eq $mtdatastore )
		{
			Common_Utils::WriteMessage("ERROR :: Found empty value for attribute 'datastore' of master target in file $args{file}",3);
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode ,my $tgtHostView )	= HostOpts::GetSubHostView( $targetvSphereHostName );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}

		( $returnCode ,my $tgtDcView )		= DatacenterOpts::GetDataCenterViewOfHost( $targetvSphereHostName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return( $Common_Utils::FAILURE );
		}
		my $datacenterName					= $tgtDcView->name;
		$TargetHost[0]->setAttribute( 'datacenter' , $datacenterName );
		my @hostViews						= $tgtHostView;
		my @datacenterViews					= $tgtDcView;
		( $returnCode , my $listDatastoreInfo )	= DatastoreOpts::GetDatastoreInfo( \@hostViews , \@datacenterViews );
		if ( $returnCode ne $Common_Utils::SUCCESS ) 
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , $listDatastoreInfo ) 	= DatastoreOpts::ValidateDatastores( $tgtHostView , $listDatastoreInfo , $datacenterName );
		if ( $returnCode ne $Common_Utils::SUCCESS ) 
		{
			return ( $Common_Utils::FAILURE );
		}
		
		my @offlineSyncDatastore 	= ();
		foreach my $datastoreInfo ( @$listDatastoreInfo )
		{
			my $datastoreName 		= $$datastoreInfo{symbolicName};
			if ( ( ! defined $$datastoreInfo{folderFileInfo} ) || ( 'no' eq lc ($$datastoreInfo{isAccessible} ) ) )
			{
				next;
			}
			
			my @folderFileInfo	= @{$$datastoreInfo{folderFileInfo}};
			if ( grep "$_/" eq "InMage_Offline_Sync_Folder/" , @folderFileInfo )
			{
				Common_Utils::WriteMessage("InMage Offline Sync folder exists in datastore $datastoreName.",2);
				push @offlineSyncDatastore , $datastoreName;
			}
		}
		
		if ( -1 == $#offlineSyncDatastore )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find folder 'InMage_Offline_Sync_Folder' in datastores of host $targetvSphereHostName.",3);
			return ( $Common_Utils::FAILURE );
		}
		elsif( 0 < $#offlineSyncDatastore )
		{
			Common_Utils::WriteMessage("ERROR :: 'InMage_Offline_Sync_Folder' is found in multiple datastores of target $targetvSphereHostName. List of datastores where it is found..",3);
			for( my $i = 0 ; $i <= $#offlineSyncDatastore ; $i++ )
			{
				Common_Utils::WriteMessage("$i. $offlineSyncDatastore[$i].",3);
			}
			Common_Utils::WriteMessage("ERROR :: Please have 'InMage_Offline_Sync_Folder' on only one datastore.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , my $tgtvmViews )		= VmOpts::GetVmViewsOnHost( $tgtHostView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		my $masterTargeName 					= $TargetHost[0]->getAttribute('display_name');
		if ( $Common_Utils::EXIST eq CheckIfDisplayNameExists( $masterTargeName , $tgtvmViews ) )
		{
			Common_Utils::WriteMessage("ERROR :: Already a machine with name \"$masterTargeName\" exists in target hosts.",3);
			Common_Utils::WriteMessage("ERROR :: 1. Make sure that Master Target is in un-registered state.",3);
			Common_Utils::WriteMessage("ERROR :: 2. Change name of the machine and re-try.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		foreach my $datastoreInfo ( @$listDatastoreInfo )
		{
			my $datastoreName 		= $$datastoreInfo{symbolicName};
			if ( $mtdatastore ne $datastoreName )
			{
				next;
			}
			
			my @folderFileInfo	= @{$$datastoreInfo{folderFileInfo}};
			my $mtFolderPath	= $TargetHost[0]->getAttribute('folder_path');
			my $mtFolderFound	= grep "$_/" eq $mtFolderPath , @folderFileInfo;
			if ( !$mtFolderFound )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to find master target folder in datastore $mtdatastore.",3);
				return ( $Common_Utils::FAILURE );
			}
		}
		
			
		foreach my $sourceHost ( @sourceHosts )
		{
			my $machineName 		= $sourceHost->getAttribute("display_name");
			Common_Utils::WriteMessage("Performing checks on machine $machineName.",2);
			
			if ( ( defined ( $sourceHost->getAttribute('new_displayname') ) ) && ( "" ne $sourceHost->getAttribute('new_displayname') ) )
			{
				$machineName		= $sourceHost->getAttribute('new_displayname');
			}
			
			$sourceHost->setAttribute("target_uuid","");
			if ( $Common_Utils::EXIST eq CheckIfDisplayNameExists( $machineName , $tgtvmViews ) )
			{
				Common_Utils::WriteMessage("ERROR :: Already a machine with name \"$machineName\" exists in target host.",3);
				Common_Utils::WriteMessage("ERROR :: 1. If possible rename the machine on target and re-try.",3);
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			
			foreach my $datastoreInfo ( @$listDatastoreInfo )
			{
				my $datastoreName 		= $$datastoreInfo{symbolicName};
				if ( $datastoreName ne $offlineSyncDatastore[0] )
				{
					next;
				}
				
				my @folderFileInfo	= @{$$datastoreInfo{folderFileInfo}};
				my $sourceFolderPath= $sourceHost->getAttribute('folder_path');
				if ( grep "$_/" eq $sourceFolderPath , @folderFileInfo )
				{
					Common_Utils::WriteMessage("ERROR :: $sourceFolderPath already exists on datastore $datastoreName.",3);
					$doesChecksPassed= $Common_Utils::FAILURE;
				}
			}
			
			my $disksInfo 			= $sourceHost->getElementsByTagName('disk');
			foreach my $disk ( @$disksInfo )
			{
				$disk->setAttribute( 'datastore_selected' , $offlineSyncDatastore[0] );
			}
		}
	}
	
	my @docElements 		= $docNode;
	$returnCode				= XmlFunctions::SaveXML( \@docElements , $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		$doesChecksPassed 	= $Common_Utils::FAILURE ;
	}
	return ( $doesChecksPassed );
}

#####CommonChecksAtTarget#####
##Description			:: Checks whether
##							1. Free space is available on Target datastore?
##							2. Checks whether all the disk can be protected( it checks whether we can add all disk to MT )?
##							3. Is datastore Accessible and is not read only.
##							4. Does the Folder exists in target Datastore?
##							5. We need to compare disk uuids of protected machines with Disk uuids of Master Target.
##Input					:: MachineInformation Passed as XmlNodeElement and Master Target Information.
##Output 				:: Returns SUCCESS else FAILURE.
#####CommonChecksAtTarget#####
sub CommonChecksAtTarget
{
	my %args 					= @_;
	my $ReturnCode				= $Common_Utils::SUCCESS;
	my $selectedDisks			= $args{selectedDisks};
	
	my $clusterNodes	= "";
	my $inmageHostId	= $args{sourceHost}->getAttribute('inmage_hostid');
	if( "yes" eq $args{sourceHost}->getAttribute('cluster') )
	{
		$clusterNodes	= $args{sourceHost}->getAttribute('clusternodes_inmageguids');
	}
	
	my @disksInfo 				= $args{sourceHost}->getElementsByTagName('disk');
	for( my $i = 0 ; $i <= $#disksInfo ; $i++ )
	{
		my $diskName 			= $disksInfo[$i]->getAttribute('disk_name');
		my @splitDiskName 		= split( /\[|\]/ , $diskName );
		my $datastoreToBeUsed	= $disksInfo[$i]->getAttribute('datastore_selected');
		$splitDiskName[-1]		=~ s/^\s+//;
		my $diskNeedToBeChecked = "[$datastoreToBeUsed] $splitDiskName[-1]";
		
		if ( ( defined ( $args{sourceHost}->getAttribute('vmDirectoryPath') ) ) && ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
		{
			$diskNeedToBeChecked= "[$datastoreToBeUsed] ".$args{sourceHost}->getAttribute('vmDirectoryPath')."/$splitDiskName[-1]";
		}
		if ( ! exists $DatacentersInfo{ $args{datacenterName} } )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find the datacenter information of $args{datacenterName}.",3);
			return ( $Common_Utils::FAILURE );
		}
		if ( !exists $DatacentersInfo{ $args{datacenterName} }->{datastoreInfo} )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find datastore information under datacenter $args{datacenterName}.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		my $diskUuid	= $disksInfo[$i]->getAttribute('source_disk_uuid');
		if ( "physicalmachine" eq lc( $args{sourceHost}->getAttribute('machinetype') ) )
		{
			$diskUuid	= $disksInfo[$i]->getAttribute('disk_signature');
		}
		
		if( ( defined $$selectedDisks{$diskUuid} ) )
		{
			my $clusterDisk	= $$selectedDisks{$diskUuid};
		    if( $clusterNodes =~ /$$clusterDisk{inmageHostId}/i )
			{
				next;
			}
		}
		if(  "yes" eq $disksInfo[$i]->getAttribute('cluster_disk') )
		{
			$$selectedDisks{$diskUuid}	= { diskName => $diskName, inmageHostId => $inmageHostId };
		}
		
		my $diskInfo 			= undef;
		my @tempArray			= @{$args{existingDiskInfo} } ;
		if ( ( "" ne $args{existingDiskInfo} ) && ( -1 != $#tempArray ) )
		{
			foreach my $disk ( @tempArray )
			{
				#3062103 - Failback readinesschecks checking for free space on the datastore and expecting the same size free space 
				#as that of source vmdks files even though it will use the existing VMDK and result is failure if that much space is not available
				my $tgtDiskUuid	= $disksInfo[$i]->getAttribute('target_disk_uuid');
				if( (defined $tgtDiskUuid) && ( $tgtDiskUuid ne "" ) )
				{
					if( $tgtDiskUuid ne $disk->{diskUuid})
					{
						next;
					}
				}
				elsif ( $diskNeedToBeChecked ne $disk->{diskName} )
				{
					next;
				}
				Common_Utils::WriteMessage("$diskNeedToBeChecked exists on target side.",2);
				$diskInfo 		= $disk;
				last;
			}
		}
		
		my $datastoreStoreFound	= $Common_Utils::NOTFOUND;
		my $datastoresInfo		= $DatacentersInfo{ $args{datacenterName} }->{datastoreInfo};
		foreach my $datastore ( @$datastoresInfo )
		{
			if ( $$datastore{symbolicName} ne $datastoreToBeUsed )
			{
				next;
			}
			$datastoreStoreFound= $Common_Utils::EXIST;
			if ( "yes" ne lc( $$datastore{isAccessible} ) )
			{
				Common_Utils::WriteMessage("ERROR :: Datastore \"$datastoreToBeUsed\" is not accessible.",3);
				Common_Utils::WriteMessage("ERROR :: Skipping checks folder existence and space calculation for disk $diskName.",3);
				$ReturnCode		= $Common_Utils::FAILURE;
				last;
			}
			
			if( "yes" eq lc( $$datastore{readOnly} ) )
			{
				Common_Utils::WriteMessage("ERROR :: Datastore \"$datastoreToBeUsed\" can not be used as repository.Check the below check list.",3);
				Common_Utils::WriteMessage("ERROR :: 1. If it a NFS mounted datastore check whether it is mounted in read-only mode.",3);
				Common_Utils::WriteMessage("ERROR :: 2. check whether target ESX/ESXi is licensed to allow API to use datastore.",3);
				$ReturnCode		= $Common_Utils::FAILURE;
				last;
			}
			my $spaceRequired 			= $disksInfo[$i]->getAttribute('size'); 
			if ( defined $diskInfo )
			{
				if ( ( "mapped raw lun" eq lc( $disksInfo[$i]->getAttribute('disk_mode') ) ) && ( "false" eq lc( $disksInfo[$i]->getAttribute('convert_rdm_to_vmdk') ) ) )
				{
					if ( ( "mapped raw lun" ne lc( $diskInfo->{diskMode} ) ) && ( (! defined $args{failback} ) || ( "no" eq lc( $args{failback} ) ) ) )
					{
						Common_Utils::WriteMessage("ERROR :: A RDM disk with name \"$diskName\" on source, already exists on target side as VMDK with name \"$diskNeedToBeChecked\".",3);
						Common_Utils::WriteMessage("ERROR :: 1. Please navigate back and answer question on RDM disk as \"yes\".",3);
						$ReturnCode		= $Common_Utils::FAILURE;
					}
					elsif( $disksInfo[$i]->getAttribute('size') > $diskInfo->{diskSize} )
					{
						Common_Utils::WriteMessage("ERROR :: RDM disk \"$diskName\"(disk on source) is larger than \"$diskNeedToBeChecked\"(disk on target).",3);
						Common_Utils::WriteMessage("ERROR :: 1. Please delete disk on target and provide a LUN at target which is larger(or)equal in size than source.",3);
						Common_Utils::WriteMessage("ERROR :: 2. If possible, extend LUN at target side associated with \"$diskNeedToBeChecked\".",3);
						$ReturnCode		= $Common_Utils::FAILURE;
					}
				}
				else
				{
					if ( ( "mapped raw lun" eq lc( $disksInfo[$i]->getAttribute('disk_mode') ) ) && ( "true" eq lc( $disksInfo[$i]->getAttribute('convert_rdm_to_vmdk') ) ) )
					{
						if ( ( "mapped raw lun" eq lc( $diskInfo->{diskMode} ) ) && ( (! defined $args{failback} ) || ( "no" eq lc( $args{failback} ) ) ) )
						{
							Common_Utils::WriteMessage("ERROR :: A RDM disk with name \"$diskName\" on source, already exists on target side as RDM with name \"$diskNeedToBeChecked\".",3);
							Common_Utils::WriteMessage("ERROR :: 1. Please navigate back and answer question on RDM disk as \"No\".",3);
							$ReturnCode		= $Common_Utils::FAILURE;
						}
					}
					
					if ( $disksInfo[$i]->getAttribute('size') > $diskInfo->{diskSize} )
					{
						$spaceRequired		= $spaceRequired - $diskInfo->{diskSize};
						Common_Utils::WriteMessage("Disk \"$diskNeedToBeChecked\", has to be extended at target side.It has to be exteded by $spaceRequired KB.",2);
						if( $disksInfo[$i]->getAttribute('protected') eq "yes" )
						{
							Common_Utils::WriteMessage("ERROR :: Disk \"$diskNeedToBeChecked\" of machine \"$args{machineName}\", has to be extended at target side.",3);
							Common_Utils::WriteMessage("ERROR :: Please perform either disk resize operation or extend disk on the DR site manually on Master Target and DR VM.",3);
							$ReturnCode		= $Common_Utils::FAILURE;							
						}
					}
#					elsif( $disksInfo[$i]->getAttribute('size') < $diskInfo->{diskSize} )
#					{
#						Common_Utils::WriteMessage("ERROR :: Disk \"$diskName\" which exists on target side as \"$diskNeedToBeChecked\" is smaller in size compared to target disk.",3);
#						Common_Utils::WriteMessage("ERROR :: 1. Delete disk at target side and re-run readiness checks.",3);
#						$ReturnCode		= $Common_Utils::FAILURE;
#					}
					else
					{
						$spaceRequired		= 0;		
					}
				}
			}
			
			if ( ( "mapped raw lun" eq lc( $disksInfo[$i]->getAttribute('disk_mode') ) ) && ( "false" eq lc ( $disksInfo[$i]->getAttribute('convert_rdm_to_vmdk') ) ) )
			{
				Common_Utils::WriteMessage("Skipping the size caclulation for disk \"$diskName\" as it is a RDM device.",2);
				last;
			}
			
			if( "true" eq $disksInfo[$i]->getAttribute('thin_disk') )
			{
				Common_Utils::WriteMessage("Skipping the size caclulation for disk \"$diskName\" as it is creating as thin mode.",2);
				last;
			}
			
			$$datastore{requiredSpace}		+= $spaceRequired/( 1024 * 1024 );
		}
		
		if ( $datastoreStoreFound == $Common_Utils::NOTFOUND )
		{
			Common_Utils::WriteMessage("ERROR :: $diskNeedToBeChecked is in a datastore which is not accessible by master target.",3);
			Common_Utils::WriteMessage("ERROR :: Please place this machine in a shared datastore, so that master target can access disk of this machine.",3);
			$ReturnCode 		= $Common_Utils::FAILURE;
		}
	}

	return ( $ReturnCode );
}

sub CheckIfDiskExistsInDatastore
{
	return ( $Common_Utils::NOTFOUND );
}

#####CheckIfDisplayNameExists#####
##Description			:: checks whether the Display Name exists on DR server.
##Input 				:: Display name of the machine we need to check and virtual machine views in target datacenter.
##Output				:: Returns EXIST if DisplayName is found else NOTFOUND.
#####CheckIfDisplayNameExists#####
sub CheckIfDisplayNameExists
{
	my $nameToCheck		= shift;
	my $vmViews 		= shift;
	Common_Utils::WriteMessage("Checking for display name $nameToCheck.",2);
	foreach my $vmView ( @$vmViews )
	{
		if ( $vmView->name eq $nameToCheck )
		{
			Common_Utils::WriteMessage("Found a machine with display name $nameToCheck.",2);
			return ( $Common_Utils::EXIST );
		}
	}
	Common_Utils::WriteMessage("Failed to find machine with name $nameToCheck.",2);
	return ( $Common_Utils::NOTFOUND );
}

#####CheckIfMachineExists#####
##Description			:: Checks whether the Machine exists on target side.
##Input 				:: Machine information which has to be checked on Target side and virtual machines views in target datacenter.
##Output				:: Returns SUCCESS else FAILURE.
#####CheckIfMachineExists#####
sub CheckIfMachineExists
{
	my $machineInfo			= shift;
	my @vmView 				= ();
	my $incrmentalDisk		= shift;
	my $machineName 		= $machineInfo->getAttribute('display_name');
	my $drFolderPath		= "";
	
	if ( ( defined ( $machineInfo->getAttribute('vmDirectoryPath') ) ) && ( "" ne $machineInfo->getAttribute('vmDirectoryPath') ) )
	{
		$drFolderPath		= $machineInfo->getAttribute('vmDirectoryPath');
	}
	
	my $vSanFolderPath	= "";
	if ( ( defined( $machineInfo->getAttribute('vsan_folder') ) ) && ( "" ne $machineInfo->getAttribute('vsan_folder') ) )
	{
		$vSanFolderPath	= $machineInfo->getAttribute('vsan_folder');
	} 
	 
	if ( ( defined $machineInfo->getAttribute('target_uuid') ) && ( "" ne $machineInfo->getAttribute('target_uuid') ) )
	{
		foreach my $vmView ( @$vmViews )
		{
			if( ( $vmView->name =~ /UnKnown/i ) || ( Common_Utils::GetViewProperty( $vmView, 'summary.runtime.connectionState')->val !~ /^connected$/i ) )
			{
				next;
			}
			if ( Common_Utils::GetViewProperty( $vmView,'summary.config.uuid') eq $machineInfo->getAttribute('target_uuid') )
			{
				Common_Utils::WriteMessage("Pushing virtual machine view on the basis of UUID.",2);
				push @vmView , $vmView;
			}
		}
	}	
	
	if( 1 <= $#vmView )
	{
		my $machineUuid 	= $machineInfo->getAttribute('target_uuid');
		Common_Utils::WriteMessage("ERROR :: Found multiple machines with same UUID($machineUuid) and the machines are.",3);
		for( my $i = 0 ; $i <= $#vmView ; $i++ )
		{
			my $machineName = $vmView[$i]->name;
			Common_Utils::WriteMessage("$i.$machineName.",3);
		}
		return ( $Common_Utils::FAILURE );
	}
	
	if ( 0 == $#vmView )
	{
		Common_Utils::WriteMessage("Going to find $machineName using compare vmdk logic.",2);
		my $disksInfo	= $machineInfo->getElementsByTagName('disk');
		my $returnCode 	= VmOpts::CompareVmdk( $disksInfo , $vmView[0] , $incrmentalDisk , $drFolderPath, $vSanFolderPath );
		if ( $returnCode != $Common_Utils::EXIST )
		{
			return ( $Common_Utils::NOTFOUND );
		}
	}
	
	if ( -1 == $#vmView )
	{
		Common_Utils::WriteMessage("Going to find $machineName using compare vmdk logic.",2);
		my $disksInfo	= $machineInfo->getElementsByTagName('disk');
		foreach my $vmView ( @$vmViews )
		{
			if ( ( Common_Utils::GetViewProperty( $vmView,'summary.config.guestId') ne $machineInfo->getAttribute('alt_guest_name') ) 
					|| ( Common_Utils::IsPropertyValueTrue( $vmView,'summary.config.template' ) )
					|| ( "poweredOn" eq Common_Utils::GetViewProperty( $vmView,'summary.runtime.powerState')->val ) || ( $vmView->name =~ /UnKnown/i ) 
					|| ( Common_Utils::GetViewProperty( $vmView, 'summary.runtime.connectionState')->val !~ /^connected$/i ) )
			{
				next;
			}
			my $vmViewName 	= $vmView->name;
			Common_Utils::WriteMessage("Comparing against vm $vmViewName.",2);
			my $returnCode 	= VmOpts::CompareVmdk( $disksInfo , $vmView , $incrmentalDisk , $drFolderPath, $vSanFolderPath );
			if ( $returnCode == $Common_Utils::EXIST )
			{
				Common_Utils::WriteMessage("Inserting view of $vmViewName for machine $machineName.",2);
				push @vmView , $vmView;
			}
		}
	}
	
	if ( 1 <= $#vmView )
	{
		Common_Utils::WriteMessage("ERROR :: Found multiple views with characteristics of machine $machineName using compare disk logic.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	if ( -1 == $#vmView )
	{
		return ( $Common_Utils::NOTFOUND );
	}
	
	$machineInfo->setAttribute( 'target_uuid' , Common_Utils::GetViewProperty( $vmView[0], 'summary.config.uuid') );
	$machineInfo->setAttribute( 'alt_guest_name' , Common_Utils::GetViewProperty( $vmView[0], 'summary.config.guestId') );
	$machineInfo->setAttribute( 'vm_console_url', VmOpts::GetVmWebConsoleUrl( $vmView[0] ) );
				
	Common_Utils::WriteMessage("Machine exists with similar characteristics of $machineName.",2);
	return ( $Common_Utils::EXIST , $vmView[0] );
}

#####GetAllDetailsOfDatacenter#####
##Description			:: Gets all the view and information like VM display names under the datacenter.
##								1. check if the folder exists on Target side.
##								2. So we need to get the Datastore Information as well for all the hosts.
##Input 				:: DatacenterName.
##Output 				:: Returns SUCCESS else FAILURE.
#####GetAllDetailsOfDatacenter#####
sub GetAllDetailsOfDatacenter
{
	my $datacenterView 		= shift;
	my $hostView			= shift;
	my $vSphereHostName		= $hostView->name;
	my $datacenterName 		= $datacenterView->name;
	Common_Utils::WriteMessage("Getting details of host $vSphereHostName under datacenter $datacenterName.",2);
			
	my @hostViews							= $hostView;
	my @datacenterViews						= $datacenterView;
	my( $returnCode , $listDatastoreInfo )	= DatastoreOpts::GetDatastoreInfo( \@hostViews , \@datacenterViews );
	if ( $returnCode ne $Common_Utils::SUCCESS ) 
	{
		return ( $Common_Utils::FAILURE );
	}
	
#	( $returnCode , $listDatastoreInfo ) 	= DatastoreOpts::ValidateDatastores( $hostView , $listDatastoreInfo , $datacenterName );
#	if ( $returnCode ne $Common_Utils::SUCCESS ) 
#	{
#		return ( $Common_Utils::FAILURE );
#	}
	
	my %DatacenterInfo						= ();
	$DatacenterInfo{datastoreInfo}			= $listDatastoreInfo;
	$DatacentersInfo{$datacenterName}		= \%DatacenterInfo;	
		
	return ( $Common_Utils::SUCCESS );
}

#####GetLatestDetails#####
##Description			:: Get the Latest information of this VM. Might be due to DRS this has been moved to some other ESX?
##Input 				:: XML node of the VM for which details has to be updated?
##Output 				:: Returns SUCCESS else FAILURE.
#####GetLatestDetails#####
sub GetLatestDetails
{
	my $machineInfo				= shift;
	my $machineDetails			= shift;
	my $vmView 					= shift;
	
	my $machineName 			= $machineInfo->getAttribute('display_name');
	my $machineUuid 			= $machineInfo->getAttribute('source_uuid');
	Common_Utils::WriteMessage("Getting latest info for machine $machineName($machineUuid).",2);
	$$machineDetails{name}		= $machineName;
	$$machineDetails{uuid}		= $machineUuid;
	
	my $mor_host 				= Common_Utils::GetViewProperty( $vmView, 'summary.runtime.host');
	my $vSphereHostView			= Vim::get_view( mo_ref => $mor_host, properties => ["name","summary.config.product.version"]);
	my $vSphereHostName 		= $vSphereHostView->name;
	my $hostVersion 			= Common_Utils::GetViewProperty( $vSphereHostView , 'summary.config.product.version');
	
	my $mor_resourcePool		= $vmView->resourcePool;
	my $resourcePoolName 		= Vim::get_view( mo_ref => $mor_resourcePool, properties => ["name"] )->name;
	
	my $datacenterName			= "";
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
	
	$$machineDetails{datacenterName}		= $datacenterName;
	$$machineDetails{vSphereHostName}		= $vSphereHostName;
	$$machineDetails{vSphereHostVersion}	= $hostVersion;
	$$machineDetails{resourcepoolName}		= $resourcePoolName;
	$$machineDetails{vmPathName}			= Common_Utils::GetViewProperty( $vmView,'summary.config.vmPathName');
	$$machineDetails{operatingSystem}		= Common_Utils::GetViewProperty( $vmView,'summary.config.guestFullName');
	$$machineDetails{numVirtualDisks}		= Common_Utils::GetViewProperty( $vmView,'summary.config.numVirtualDisks');
	$$machineDetails{numEthernetCards}		= Common_Utils::GetViewProperty( $vmView,'summary.config.numEthernetCards');
	$$machineDetails{vmxVersion}			= Common_Utils::GetViewProperty( $vmView,'config.version');
	$$machineDetails{sameControllerTypes}	= "Yes";
	$$machineDetails{slotsRequired}			= 0;
	$$machineDetails{requiredSpace}			= 0;
	$$machineDetails{memsize}				= Common_Utils::GetViewProperty( $vmView,'summary.config.memorySizeMB');
	$$machineDetails{scsiControllerName}	= VmOpts::GetVmScsiControllerName($vmView);
	$$machineDetails{osInfo}				= $machineInfo->getAttribute('os_info');
	
	( my( $returnCode, $ideCount, $rdm, $cluster, $diskInfo, $floppyDevices ) ) = VmOpts::FindDiskDetails( $vmView, [] ); 
    if( $Common_Utils::SUCCESS != $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to get details of disk for machine \"$machineName\".",3);
		return $Common_Utils::FAILURE;
	}
	
	my $differentControllers				= VmOpts::IsVmHavingDifferentControllerTypes( $vmView );
	if ( $differentControllers )
	{
		$$machineDetails{sameControllerTypes}	= "No";
	}
	
	$machineInfo->setAttribute( 'ide_count' ,  $ideCount );	
	$machineInfo->setAttribute( 'vmx_version' ,  $$machineDetails{vmxVersion} );	
	$machineInfo->setAttribute( 'hostversion' , $hostVersion );		
	$machineInfo->setAttribute( 'datacenter' , $datacenterName );
	$machineInfo->setAttribute( 'resourcepool' , $resourcePoolName );
	$machineInfo->setAttribute( 'resourcepoolgrpname' , $vmView->resourcePool->value );
	return ( $Common_Utils::SUCCESS );
}

#####UpdateTargetInfo#####
##Description			:: It Updates details of machines at DR-site -- like their UUID, Datastore where they are and information
##							Regarding Target vCenter/vSphere.
##Input 				:: File Name.
##Output 				:: Returns SUCCESS else FAILURE.
#####UpdateTargetInfo#####
sub UpdateTargetInfo
{
	my %args 		= @_;
	
	Common_Utils::WriteMessage("Validating file $args{file} at DR-Site.",2);
	
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
	
	( $returnCode , $vmViews , $hostViews , $datacenterViews , $resourcePoolViews )	= Common_Utils::GetViews();
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get views of different managed objects in a vCenter/vsphere.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	foreach my $plan ( @$planNodes )
	{
		my $planName 				= $plan->getAttribute('plan');
		my @sourceNodes				= $plan->getElementsByTagName('SRC_ESX');
		my @TargetNodes				= $plan->getElementsByTagName('TARGET_ESX');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');

		if ( $#TargetNodes > 0 )
		{
			Common_Utils::WriteMessage("ERROR :: \"$planName\" contains multiple target nodes.",3);
			Common_Utils::WriteMessage("ERROR :: Please make sure that in a plan single master target is selected.",3);
			return ( $Common_Utils::FAILURE );
		}
		Common_Utils::WriteMessage("Validating plan $planName.",2);
		my @sourceHosts				= $sourceNodes[0]->getElementsByTagName('host');
		my @TargetHost 				= $TargetNodes[0]->getElementsByTagName('host');
		
		my %masterTargetDetails		= ();
		( $returnCode ,my $mtView )	= VmOpts::GetVmViewByUuid( $TargetHost[0]->getAttribute('source_uuid') );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		} 
		
		$returnCode 				= GetLatestDetails( $TargetHost[0] , \%masterTargetDetails , $mtView );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		$returnCode 				= XmlFunctions::ValidateNode( $TargetHost[0] , "target" );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		foreach my $sourceHost ( @sourceHosts )
		{
			my $machineName 		= $sourceHost->getAttribute("display_name");
			Common_Utils::WriteMessage("Performing checks on machine $machineName.",2);
			
			if ( ( defined ( $sourceHost->getAttribute('new_displayname') ) ) && ( "" ne $sourceHost->getAttribute('new_displayname') ) )
			{
				$machineName		= $sourceHost->getAttribute('new_displayname');
			}
			
			$returnCode 			= CheckIfMachineExists( $sourceHost , "yes" );
			if ( $returnCode eq $Common_Utils::NOTFOUND )
			{
				Common_Utils::WriteMessage("ERROR :: Machine $machineName does not exist on Target side.",3);
				return ( $Common_Utils::FAILURE );
			}
				
			if ( $returnCode eq $Common_Utils::FAILURE )
			{
				return ( $Common_Utils::FAILURE );
			}
		}
		
		( $returnCode , my $resourcePoolInfo )	= ResourcePoolOpts::FindResourcePoolInfo( $resourcePoolViews , $hostViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
		
		( $returnCode , my $listDataStoreInfo )	= DatastoreOpts::GetDatastoreInfo( $hostViews , $datacenterViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}

		( $returnCode , my $networkInfo ) 		= HostOpts::GetNetworkInfo( $hostViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
		
		( $returnCode , my $listConfigInfo ) 	= HostOpts::GetHostConfigInfo( $hostViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
		
		$returnCode 		= XmlFunctions::UpdateTargetInfo( 
																datastoreInfo => $listDataStoreInfo , networkInfo => $networkInfo ,
																configInfo => $listConfigInfo , scsiDiskInfo => "" , 
																resourcePoolInfo => $resourcePoolInfo , node => $TargetNodes[0],
											 				);
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}											 
	}
	
	my @docElements 		= $docNode;
	$returnCode				= XmlFunctions::SaveXML( \@docElements , $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####ArrayBasedDrDrillChecks#####
##Description	 		:: Checks whether the vmx files and vmdk files exist in the given datastores.
##Input 				:: Drdrill xml file.
##Ouput 				:: Returns SUCCESS else FAILURE.
#####ArrayBasedDrDrillChecks#####
sub ArrayBasedDrDrillChecks
{
	my %args 		= @_;
	
	Common_Utils::WriteMessage("Readiness Checks for Array based DrDrill :: file $args{file}.",2);
	my $doesChecksPassed			= $Common_Utils::SUCCESS;
	
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
	
	foreach my $plan ( @$planNodes )
	{
		my $planName 				= $plan->getAttribute('plan');
		my @sourceNodes				= $plan->getElementsByTagName('SRC_ESX');
		my @TargetNodes				= $plan->getElementsByTagName('TARGET_ESX');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		Common_Utils::WriteMessage("Validating plan $planName.",2);
		if ( $#TargetNodes > 0 )
		{
			Common_Utils::WriteMessage("ERROR :: \"$planName\" contains multiple target nodes.",3);
			Common_Utils::WriteMessage("ERROR :: Please make sure that in a plan single master target is selected.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		my @sourceHosts				= $sourceNodes[0]->getElementsByTagName('host');
		my @TargetHost 				= $TargetNodes[0]->getElementsByTagName('host');
		
		my ( $returnCode , $mtView )= VmOpts::GetVmViewByUuid( $TargetHost[0]->getAttribute('source_uuid') );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		my %masterTargetDetails		= ();
		$returnCode 				= GetLatestDetails( $TargetHost[0] , \%masterTargetDetails , $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed		= $Common_Utils::FAILURE;
		}
		
		$returnCode 				= XmlFunctions::ValidateNode( $TargetHost[0] , "target" );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed		= $Common_Utils::FAILURE;
		}
		
		( $returnCode , my $datacenterView )	= DatacenterOpts::GetDataCenterView( $masterTargetDetails{datacenterName} );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode ,  $vmViews )				= VmOpts::GetVmViewInDatacenter( $datacenterView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , my $tgtHostView)		= HostOpts::GetSubHostView( $masterTargetDetails{vSphereHostName} );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}

		if ( !exists $DatacentersInfo{ $masterTargetDetails{datacenterName} } )
		{
			$returnCode 			= GetAllDetailsOfDatacenter( $datacenterView , $tgtHostView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
		}
		
		my $vmView 					= "";
		my %selectedDisks			= ();	
		foreach my $sourceHost ( @sourceHosts )
		{
			my $machineName 		= $sourceHost->getAttribute("display_name");
			my $inmageHostId		= $sourceHost->getAttribute('inmage_hostid');
			Common_Utils::WriteMessage("Performing checks on machine $machineName.",2);
			
			if ( ( defined ( $sourceHost->getAttribute('new_displayname') ) ) && ( "" ne $sourceHost->getAttribute('new_displayname') ) )
			{
				$machineName		= $sourceHost->getAttribute('new_displayname');
			}
						
			$returnCode				= AreSelectedDatastoresAccessible( machineName => $machineName , sourceHost => $sourceHost ,
																		 hostView => $tgtHostView , datacenterName => $masterTargetDetails{datacenterName} );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			
			( $returnCode , $vmView )= VmOpts::GetVmViewByNameInDatacenter( machineName => $machineName , datacenterView => $datacenterView  );
			if ( $returnCode != $Common_Utils::NOTFOUND )
			{
				Common_Utils::WriteMessage("ERROR :: Already a machine with same name($machineName) exists in target hosts datacenter.",3);
				Common_Utils::WriteMessage("ERROR :: 1. Either have source and target in different datacenter.",3);
				Common_Utils::WriteMessage("ERROR :: 2. Change name of the machine using vContinuum 'change display name at target side' procedure.",3);
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
		
			my @disksInfo			= $sourceHost->getElementsByTagName('disk');
			my $datastoreSelected 	= $disksInfo[0]->getAttribute('datastore_selected');
			my $vmxPathName 		= $sourceHost->getAttribute('vmx_path');
			my @vmxPathSplit		= split( /\[|\]/ , $vmxPathName );
			my $vmPathName 			= "[$datastoreSelected]$vmxPathSplit[-1]";
			if ( ( defined( $sourceHost->getAttribute('vmDirectoryPath') ) ) && ( "" ne $sourceHost->getAttribute('vmDirectoryPath') ) )
			{
				$vmxPathSplit[-1]	=~ s/^\s+//;
				$vmPathName 		= "[$datastoreSelected] ".$sourceHost->getAttribute('vmDirectoryPath')."/".$vmxPathSplit[-1];
			}
			
			( $returnCode , $vmView ) 	= VmOpts::GetVmViewByVmPathNameInDatacenter( vmPathName => $vmPathName , datacenterView => $datacenterView );
			if ( $returnCode != $Common_Utils::NOTFOUND )
			{
				Common_Utils::WriteMessage("ERROR :: Already a machine exists with same path \"$vmPathName\".",3);
				Common_Utils::WriteMessage("ERROR :: 1. Select another datastore or un-select machine \"$machineName\".",3);
				Common_Utils::WriteMessage("ERROR :: 2. Re-run readiness checks after performing above changes.",3);
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			
			my $lastIndex			= rindex($vmPathName,"/");
			my $vmxFileName			= substr($vmPathName, $lastIndex+1);
			my $vmFolderPath 		= substr($vmPathName, 0, $lastIndex+1);
			my( $returnCode, $vmxFileInfo, $fileInfo )	= DatastoreOpts::FindFileInfo( $tgtHostView, $vmxFileName, $vmFolderPath, "vmconfigfileinfo" );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: Vmx file( $vmPathName ) not found in $vmFolderPath.",3);
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			( $returnCode, my $vmdkFileInfo, $fileInfo )= DatastoreOpts::FindFileInfo( $tgtHostView, "*" , $vmFolderPath, "vmdiskfileinfo" );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: Vmdk Files not found in $vmFolderPath.",3);
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			
			$returnCode 			= AreDisksInUseByAnotherVM( sourceHost => $sourceHost , uuid => "" );
			if ( $returnCode != $Common_Utils::FAILURE )
			{
				Common_Utils::WriteMessage("ERROR :: Disks of machine \"$machineName\" are in use by another virtual machine.",3);
				Common_Utils::WriteMessage("ERROR :: 1. Remove disks from another machine if they have to be re-used.",3);
				Common_Utils::WriteMessage("ERROR :: 2. Unable to continue Array Based DrDrill.",3);
				$doesChecksPassed 	= $Common_Utils::FAILURE;
			}
			
			$returnCode 			= XmlFunctions::ValidateNode( $sourceHost , "source" );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$doesChecksPassed	= $Common_Utils::FAILURE;
			}
			
			$returnCode 			= DecreaseFreeSlotsOnMasterTarget( $sourceHost , \%masterTargetDetails, \%selectedDisks );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$doesChecksPassed 	= $Common_Utils::FAILURE;
			}
			
			my @disks					= $sourceHost->getElementsByTagName('disk');
			foreach my $diskInfo ( @disks )
			{
				my $diskUuid	= $diskInfo->getAttribute('source_disk_uuid');
				if ( "physicalmachine" eq lc( $sourceHost->getAttribute('machinetype') ) )
				{
					$diskUuid	= $diskInfo->getAttribute('disk_signature');
				}
				
				if( ( defined $selectedDisks{$diskUuid} ) )
				{
					next;
				}
				
				if( defined ($diskInfo->getAttribute('selected') ) && ( "no" eq $diskInfo->getAttribute('selected') ) )
				{
					next;
				}	
				
				if(  "yes" eq $diskInfo->getAttribute('cluster_disk') )
				{
					$selectedDisks{$diskUuid}	= { diskName => $diskInfo->getAttribute('disk_name'), inmageHostId => $inmageHostId };
				}
				
				my @diskPath			= split( /\[|\]/ , $diskInfo->getAttribute('disk_name') );
				my @splitonSlash		= split( /\// , $diskPath[-1] );
				my $vmdkName			= $splitonSlash[-1];
				if( grep { $vmdkName eq $_} @$vmdkFileInfo)
				{
					next;
				}
				else
				{
					Common_Utils::WriteMessage("ERROR :: $vmdkName not found in $vmPathName.",3);
					$doesChecksPassed	= $Common_Utils::FAILURE;
				}								
			}
		}
		
		$returnCode 				= ChecksOnMasterTarget( \%masterTargetDetails );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed 		= $Common_Utils::FAILURE;
		}
		
		$returnCode 				= CheckVmBiosHddOrder( $mtView, \%masterTargetDetails );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
#			$doesChecksPassed 		= $Common_Utils::FAILURE;
		}
	}
		
	my @docElements = $docNode;
	$returnCode		= XmlFunctions::SaveXML( \@docElements , $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		$doesChecksPassed 		= $Common_Utils::FAILURE;
	}
	
	return ( $doesChecksPassed );
}

#####ArrayDatastoreChecks#####
##Description 		:: It reads snapshot.xml file for source machine information and for Master target information. 
##						This calls all sub-sequent calls to checks for a datastore creation on array vsnap lun.
##Input 			:: Complete file name for snapshot.xml file.
##Output 			:: Returns SUCCESS else FAILURE.
#####ArrayDatastoreChecks#####
sub ArrayDatastoreChecks
{
	my %args				= @_;
	my $doesChecksPassed 	= $Common_Utils::SUCCESS;
	
	my ( $returnCode , $docNode ) 		= XmlFunctions::GetPlanNodes( $args{file} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get plan information from file \"args{file}\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my $planNodes 						= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 						= [$docNode];
	}
	
	foreach my $plan ( @$planNodes )
	{
		my $planName					= $plan->getAttribute('plan');
		my @targetNodes					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetHost					= $targetNodes[0]->getElementsByTagName('host');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		Common_Utils::WriteMessage("validating Array Datastore creation = $planName.",2);
		
		my $datastoresInfo	= XmlFunctions::ReadDatastoreInfoFromXML( $targetNodes[0] );
		
		my $targetvSphereHostName		= $targetHost[0]->getAttribute('vSpherehostname');
		Common_Utils::WriteMessage("Target chosen = $targetvSphereHostName.",2);
		( $returnCode ,my $tgthostView )= HostOpts::GetSubHostView( $targetvSphereHostName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed						= $Common_Utils::FAILURE;
			last;
		}
		
		Common_Utils::WriteMessage("Rescaning all the HBA ports of $targetvSphereHostName.",2);
		$returnCode	= HostOpts::RescanAllHba( $tgthostView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$doesChecksPassed						= $Common_Utils::FAILURE;
			last;
		}
		
		Common_Utils::WriteMessage("Searching for datastores virtual copies.",2);
		( $returnCode, my $unResVmfsVols)	= DatastoreOpts::QueryUnresolvedVmfsVolumes( $tgthostView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		my @requiredDs	= ();
		my %requiredDs	= ();		
		my @sourceHosts					= $plan->getElementsByTagName('SRC_ESX');
		foreach my $sourceHost	( @sourceHosts )
		{
			my @hostsInfo				= $sourceHost->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				my @disksInfo			= $host->getElementsByTagName('disk');
				my $datastoreSelected 	= $disksInfo[0]->getAttribute('datastore_selected');
				if( $requiredDs{$datastoreSelected})
				{
					next;
				}
				Common_Utils::WriteMessage("Target Datastore : $datastoreSelected",2);
				foreach my $datastoreInfo ( @$datastoresInfo )
				{
					if( ( $$datastoreInfo{vSphereHostName} eq $targetvSphereHostName ) && ( $$datastoreInfo{datastoreName} eq $datastoreSelected ) )
					{
						push @requiredDs, $datastoreInfo;
						$requiredDs{$datastoreSelected}	= "exists";
						Common_Utils::WriteMessage("existing datastore = $datastoreSelected; uuid = $$datastoreInfo{uuid}.",2);
						last;
					}
				}
			}
		}
		
		$returnCode	= DatastoreOpts::ArrayDatastoresResolvebilityCheck( $unResVmfsVols, \@requiredDs );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Unable to perform array based drdrill.",3);
			return ( $Common_Utils::FAILURE );
		}
	}
		
	return ( $doesChecksPassed );
}
 						
#####CheckVmBiosHddOrder#####
##Description	:: set the configuation parameters into vmx file .
##Input 		:: VM View to which option value hat to set.
##Output 		:: Returns SUCCESS else FAILURE.
#####CheckVmBiosHddOrder#####
sub CheckVmBiosHddOrder
{
	my $vmView 		= shift;	
	my $masterTargetDetails	= shift;	
	my $machineName	= $vmView->name;
	
	if( ( lc( $$masterTargetDetails{vmxVersion} ) lt "vmx-10" ) || ( $$masterTargetDetails{operatingSystem} !~ /Windows/i ) )
	{
		return ( $Common_Utils::SUCCESS );
	}
	
	my $extraConfig	= Common_Utils::GetViewProperty( $vmView, 'config.extraConfig');
	foreach my $config ( @$extraConfig )
	{
		if ( $config->key =~ /^bios\.hddOrder$/i )
		{
			Common_Utils::WriteMessage("Already set bios.hddOrder value as $config->{value} to $machineName.",2);
			return ( $Common_Utils::SUCCESS ); 
		}
	}
	
	my $devices = Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
	foreach my $device ( @$devices )
	{
		if ( $device->deviceInfo->label =~ /Hard Disk/i )
		{
			if( $device->controllerKey >= 15000 )
			{
				return ( $Common_Utils::SUCCESS );
			}
		}
	}
	
	my $datacenterName	= $$masterTargetDetails{datacenterName};
		
	my $fileName	= $$masterTargetDetails{vmPathName};
	Common_Utils::WriteMessage("Downloading the file from datastore \"$fileName\".",2);

	if( $Common_Utils::SUCCESS != DatastoreOpts::do_get( $fileName, $Common_Utils::vContinuumMetaFilesPath , $datacenterName ) )
	{
		return ( $Common_Utils::FAILURE );
	}

	my $last_slash_index 	= rindex( $fileName , "/" );
	my $vmxFileName			= substr( $fileName , $last_slash_index+1 , length( $fileName ) );
	if ( ! open( FR,"<$Common_Utils::vContinuumMetaFilesPath/$vmxFileName" ) )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to open file \"$vmxFileName\" for reading.Please check for the permissions on the file.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my $found = 0;
	while(<FR>)
	{
		my @line_split 		= split("=", $_);
		if( $line_split[0] =~ /bios\.hddOrder\s+$/ )
		{
			Common_Utils::WriteMessage("Found bios\.hddOrder value as $line_split[1] .",2);
			$found	= 1;
			last;
		}
	}
	close FR;
	unlink "$Common_Utils::vContinuumMetaFilesPath/$vmxFileName";
	if( ! $found )
	{
		Common_Utils::WriteMessage("ERROR :: attribute 'bios.hddOrder' parameter is not set in master target.",3);
		Common_Utils::WriteMessage("ERROR :: It may cause boot failure in next restart.",3);
		Common_Utils::WriteMessage("ERROR :: Please set above attribute (or) change bios settings in MT in next reboot.",3);
		return ( $Common_Utils::FAILURE );
	}
		
	return ( $Common_Utils::SUCCESS );
}

BEGIN:

my $ReturnCode				= $Common_Utils::SUCCESS;

my ( $sec, $min, $hour )	= localtime(time);
my $logFileName 		 	= "vContinuum_Temp_$sec$min$hour.log";

Common_Utils::CreateLogFiles( "$Common_Utils::vContinuumLogsPath/$logFileName" , "$Common_Utils::vContinuumLogsPath/vContinuum_ESX.log" );

Opts::add_options(%customopts);
Opts::parse();

my( $returnCode, $serverIp, $userName, $password )	= HostOpts::getEsxCreds( Opts::get_option('server') );
Opts::set_option('username', $userName);
Opts::set_option('password', $password );

Opts::validate();

my $file 					= "$Common_Utils::vContinuumMetaFilesPath/$Common_Utils::EsxXmlFileName";
if ( ( Opts::option_is_set('dr_drill') ) && ( "yes" eq lc( Opts::get_option('dr_drill') ) ) || ( ( Opts::option_is_set('dr_drill_array') ) && ( "yes" eq lc ( Opts::get_option('dr_drill_array') ) ) ) )
{
	$file					= "$Common_Utils::vContinuumMetaFilesPath/$Common_Utils::DrDrillXmlFileName";
}

if($returnCode	!= $Common_Utils::SUCCESS )
{
	Common_Utils::WriteMessage("ERROR :: Failed to get credentials from CX.",3);
}
elsif ( ! ( Opts::option_is_set('username') && Opts::option_is_set('password') && Opts::option_is_set('server') ) )
{
	Common_Utils::WriteMessage("ERROR :: vCenter/vSphere IP,Username,Password and prechecks values are the basic parameters required.",3);
	Common_Utils::WriteMessage("ERROR :: One or all of the above parameters are not set/passed to script.",3);
}
else
{
	$ReturnCode				= Common_Utils::Connect( Server_IP => $serverIp, UserName => Opts::get_option('username'), Password => Opts::get_option('password') );
	if ( $ReturnCode eq $Common_Utils::SUCCESS )
	{
		eval
		{
			if ( ( Opts::option_is_set('target') && ( "yes" eq lc( Opts::get_option('target') ) ) ) && ( Opts::option_is_set('incremental_disk') && ( "yes" eq lc( Opts::get_option('incremental_disk') ) ) ) )
			{
				$ReturnCode			= UpdateTargetInfo(
															file			 => $file,  
													);
				
			}
			elsif( ( Opts::option_is_set('offline_sync_import') ) && ( "yes" eq lc ( Opts::get_option('offline_sync_import') ) ) )
			{
				$ReturnCode			= OfflineSyncImportChecksAtTarget(
																		file	=> $file,				
																	);
			}
			elsif( ( Opts::option_is_set('dr_drill_array') ) && ( "yes" eq lc ( Opts::get_option('dr_drill_array') ) ) )
			{
				$ReturnCode			= ArrayBasedDrDrillChecks(
																file	=> $file,				
															);
			}
			elsif ( "yes" eq lc ( Opts::get_option('array_datastore') ) )
			{
				$file				= "$Common_Utils::vContinuumMetaFilesPath/$Common_Utils::DrDrillXmlFileName";
				$ReturnCode			= ArrayDatastoreChecks(
																file 	=> $file,
															);
			}
			else
			{
				$ReturnCode 		= RunReadinessChecks( 	
															file			  => $file,
															incrementalDisk   => Opts::get_option('incremental_disk'),
															failback		  => Opts::get_option('failback'), 
															resume 			  => Opts::get_option('resume'), 
															offlineSyncExport => Opts::get_option('offline_sync_export'),
								  						);
			}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			$ReturnCode 			= $Common_Utils::FAILURE;
		}
	}
}

Common_Utils::LoggedData( tempLog => "$Common_Utils::vContinuumLogsPath/$logFileName" , planLog => $Common_Utils::vContinuumPlanDir );

exit( $ReturnCode );

__END__