=head
	FolderOpts.pm
	-------------
	#will be added as we develop this file.
=cut

package FolderOpts;

use strict;
use warnings;
use Common_Utils;
use HostOpts;
use DatacenterOpts;
use VmOpts;
use XmlFunctions;

#####GetFolderViews#####
##Description 		:: Gets folder view of vCenter/vSphere.
##Input 			:: None.
##Output 			:: Returns folder views on SUCCESS else FAILURE.
#####GetFolderViews#####
sub GetFolderViews
{
	my $folderViews 	= Vim::find_entity_views( view_type => 'Folder' );
	unless ( @$folderViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get folder views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $folderViews );
	}
	return ( $Common_Utils::SUCCESS , $folderViews );
}

#####MapFolderToItsChildFolder#####
##Description 		:: Maps Folder to it's child Folder's if any. As we are not able to get each and every machines DataCenter using
##						it's Folder Group, this is other way around in mapping data. Place holder till correct method is found.
##Input 			:: Folder view.
##Output 			:: Returns mapper on SUCCESS else FAILURE.
#####MapFolderToItsChildFolder#####
sub MapFolderToItsChildFolder
{
	my $folderViews 			= shift;
	my %mapFolderToChildFolder	= ();
	my %mapDatacenterAndFolder  = ();
	
	foreach my $folderView ( @$folderViews )
	{
		if ( !defined ( $folderView->childEntity ) )
		{
			next;
		}
		my $childEntity 	= $folderView->childEntity;
		foreach  my $child ( @$childEntity )
		{
			if ( "Folder" eq $child->type )
			{
				$mapFolderToChildFolder{ $child->value }	= $folderView->{mo_ref}->value;
			}
		}
		
		if ( ( ! defined ( $folderView->parent ) ) || ( $folderView->parent->type  !~ /^Datacenter$/ ) )
		{
			next;
		}
		$childEntity 	= $folderView->childEntity;
		foreach  my $child ( @$childEntity )
		{
			if ( "Folder" eq $child->type )
			{
				$mapDatacenterAndFolder{ $child->value }	= $folderView->{mo_ref}->value;		
			}
		}
	}
	return ( $Common_Utils::SUCCESS , \%mapFolderToChildFolder , \%mapDatacenterAndFolder );
} 						

#####CreateVmConfig#####
##Description	:: Create configuration of Machine which will not have any VMDK or Network. It is just template configuration of VM.
##Input 		:: XML node and Target Node.
##Output 		:: Returns SUCCESS else FAILURE.
#####CreateVmConfig#####
sub CreateVmConfig
{
	my $sourceNode 							= shift;
	my $targetNode 							= shift;
	my $CreatedVirtualMachineInfo			= shift;
	my $targetHostView 						= shift;
	my $tgtDatacenterView					= shift;
	my $createdVmView						= "";
	
	my $targetDatacenter					= $tgtDatacenterView->name;
	my $displayName 						= $sourceNode->getAttribute('display_name');
	unless( !defined ( $sourceNode->getAttribute('new_displayname') ) )
	{
		$displayName 						= $sourceNode->getAttribute('new_displayname');
	}
	Common_Utils::WriteMessage("Creating virtual machine $displayName.",2);

	my $vmConfigSpec 						= VirtualMachineConfigSpec->new();
	$vmConfigSpec->{name}					= $displayName;
	$vmConfigSpec->{cpuHotAddEnabled} 		= "TRUE";
	$vmConfigSpec->{cpuHotRemoveEnabled} 	= "TRUE";
	$vmConfigSpec->{memoryMB}				= $sourceNode->getAttribute('memsize');
	$vmConfigSpec->{numCPUs} 				= $sourceNode->getAttribute('cpucount');

	my $fileInfo 							= VirtualMachineFileInfo->new();
	my @disksInfo							= $sourceNode->getElementsByTagName('disk');
	my $datastoreSelected 					= $disksInfo[0]->getAttribute('datastore_selected');
	my $vmxPathName 						= $sourceNode->getAttribute('vmx_path');
	my @vmxPathSplit						= split( /\[|\]/ , $vmxPathName );
	my $vmPathName 							= "[$datastoreSelected]$vmxPathSplit[-1]";
	my $configFilesPath 					= "[$datastoreSelected] ".$sourceNode->getAttribute('folder_path');
	if ( ( defined( $sourceNode->getAttribute('vmDirectoryPath') ) ) && ( "" ne $sourceNode->getAttribute('vmDirectoryPath') ) )
	{
		$vmxPathSplit[-1]					=~ s/^\s+//;
		$vmPathName 						= "[$datastoreSelected] ".$sourceNode->getAttribute('vmDirectoryPath')."/".$vmxPathSplit[-1];
		$configFilesPath 					= "[$datastoreSelected] ".$sourceNode->getAttribute('vmDirectoryPath')."/".$sourceNode->getAttribute('folder_path');
	}
	$fileInfo->{vmPathName}					= $vmPathName;
	$fileInfo->{logDirectory}				= $configFilesPath;
	$fileInfo->{snapshotDirectory}			= $configFilesPath;
	$fileInfo->{suspendDirectory}			= $configFilesPath;
	$vmConfigSpec->{files} 					= $fileInfo;			
	Common_Utils::WriteMessage("vmx path on target = $vmPathName.",2);
	
	$vmConfigSpec->{guestId}				= $sourceNode->getAttribute('alt_guest_name');
	$vmConfigSpec->{version}				= $sourceNode->getAttribute('vmx_version');
	if ( "" eq $sourceNode->getAttribute('vmx_version') )
	{
		if ( $sourceNode->getAttribute('hostversion') lt "4.0.0" )
		{
			$vmConfigSpec->{version}		= "vmx-04";
		}
		if ( $sourceNode->getAttribute('hostversion') ge "4.0.0"  )
		{
			$vmConfigSpec->{version}		= "vmx-07";
		}
	}
	
	if( "true" eq lc($sourceNode->getAttribute('efi')) ) 
	{
		$vmConfigSpec->{firmware}		= "efi";
		$vmConfigSpec->{version}		= "vmx-08";
		$sourceNode->setAttribute('vmx_version',"vmx-08");
		Common_Utils::WriteMessage("Firmware type of virtual machine $displayName is EFI and vmx version is vmx-08.",2);
	}
	
   	my $resourcePoolGroupId					= $sourceNode->getAttribute('resourcepoolgrpname');
   	if ( "" eq $resourcePoolGroupId )
   	{
   		$resourcePoolGroupId				= $targetNode->getAttribute('resourcepoolgrpname');
   	}
   	Common_Utils::WriteMessage("Resource Pool Group Id = $resourcePoolGroupId.",2);
   	my( $returnCode , $tgtResourcePoolView )= ResourcePoolOpts::FindResourcePool( $resourcePoolGroupId );
   	if ( $returnCode ne $Common_Utils::SUCCESS )
   	{
   		Common_Utils::WriteMessage("ERROR :: Failed to find resourcepool view of \"$resourcePoolGroupId\".",3);
   		return ( $Common_Utils::FAILURE );
   	}
   	my $vmFolderView 						= Vim::get_view( mo_ref => $tgtDatacenterView->vmFolder, properties => [] );
	my $task 								= "";
	
   	eval
   	{
   			$task 							= $vmFolderView->CreateVM_Task(	config 	=> $vmConfigSpec,
               																pool 	=> $tgtResourcePoolView,
                   															host 	=> $targetHostView 
                   						  								);
   	};
   	if( $@ )
   	{
   		Common_Utils::WriteMessage("ERROR :: Failed to Create virtual machine \"$displayName\" \n $@.",3);
   		return ( $Common_Utils::FAILURE );
	}
	
	my $taskView 							= Vim::get_view(mo_ref => $task);
	my $vmView 								= "";
	while( 1 )
	{
		my $taskInfo						= $taskView->info;
		if ( $taskInfo->state->val eq "success" )
		{
			$createdVmView	= Vim::get_view( mo_ref => $taskInfo->result );
			my $createdVmName	= $createdVmView->name;
			Common_Utils::WriteMessage("successfully created VM \"$createdVmName\".",2);
			$$CreatedVirtualMachineInfo{uuid}		= $createdVmView->summary->config->uuid;
			$$CreatedVirtualMachineInfo{hostName}	= $sourceNode->getAttribute('hostname'); 
			$sourceNode->setAttribute( 'target_uuid' , $createdVmView->summary->config->uuid );
			$sourceNode->setAttribute( 'vm_console_url', VmOpts::GetVmWebConsoleUrl( $createdVmView ) );
			
			if( ( defined $sourceNode->getAttribute('vsan') ) && ( "true" eq lc($sourceNode->getAttribute('vsan') ) ) )
			{
				my @datastore 			= split( /\[|\]/, $createdVmView->summary->config->vmPathName );
		        my $final_slash 		= rindex( $datastore[2] , "/" );
		        my $vSanFolder			= substr( $datastore[2] , 1 , $final_slash );
				$sourceNode->setAttribute( 'vsan_folder', $vSanFolder );
				Common_Utils::WriteMessage("Found VM vSan folder path as \"$vSanFolder\".",2);
				return ( $Common_Utils::SUCCESS );
			}
			last;
		}
		if ( $taskInfo->state->val 	eq "error" )
		{
			my $soap_fault 					= SoapFault->new;
	        my $errorMesg 					= $soap_fault->fault_string($taskInfo->error->localizedMessage);
	        my $faultType 					= $taskInfo->error->fault;
	        my @errorMsgDetail				= $faultType->faultMessage;
	        Common_Utils::WriteMessage("ERROR :: $soap_fault.",3);
	        Common_Utils::WriteMessage("ERROR :: Creation of virtual machine failed with error message \"$errorMesg\".",3);
	        return ( $Common_Utils::FAILURE );
		}
		sleep(1);
		Common_Utils::WriteMessage("Updating view of virtual machine \"$displayName\".",2);
		$taskView->ViewBase::update_view_data();
	}
	
	$returnCode 							= CheckVmPath( vmView => $createdVmView , vmPathName => $vmPathName , sourceNode => $sourceNode,
															datastoreSelected => $datastoreSelected , targetDatacenter => $targetDatacenter ,
															tgtDatacenterView => $tgtDatacenterView , targetHostView => $targetHostView ,
															tgtResourcePoolView => $tgtResourcePoolView , vmFolderView => $vmFolderView );
															
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	 
	return ( $Common_Utils::SUCCESS );
}

#####CheckVmPath#####
##Description		:: Check what is the vmPath configured at Dr-Site. If it differs from what we need it makes necessary changes.
##Input				:: VmView, SourceNode, Datastoreselected, target DatacenterName,  targetDatacenter View, Target HostView , port group Information, IsItvCenter.
##Output 			:: Returns SUCCESS after reconfiguring virtual machine else FAILURE.
sub CheckVmPath
{
	my %args 			= @_;
	
	my $displayName 	= $args{vmView}->name; 
	my $CreatedVmPath	= Common_Utils::GetViewProperty( $args{vmView},'summary.config.vmPathName');
	my $ReqVmPath		= $args{vmPathName};
	
	my $rindex			= rindex( $CreatedVmPath , "/" );
	my $CreatedDsPath	= substr( $CreatedVmPath , 0 , $rindex );
	
	$rindex				= rindex( $ReqVmPath , "/" );
	my $ReqDsPath		= substr( $ReqVmPath , 0 , $rindex );
	
	Common_Utils::WriteMessage("ReqDsPath = $ReqDsPath, path created = $CreatedDsPath.",2);
	if ( $CreatedDsPath eq $ReqDsPath )
	{
		return ( $Common_Utils::SUCCESS );
	}

	Common_Utils::WriteMessage("changing vmPathName to $ReqDsPath.",2);
	my $returnCode	= DatastoreOpts::CreatePath( $ReqDsPath , $args{targetDatacenter} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	$returnCode 	= VmOpts::UnregisterVm( vmView => $args{vmView} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
		
	$returnCode 	= DatastoreOpts::MoveOrCopyFile( "move" , $args{targetDatacenter} , $CreatedVmPath , $ReqVmPath , 1 );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
		
	( $returnCode, my $vmView)	= VmOpts::RegisterVm( vmName => $displayName, vmPathName => $ReqVmPath , folderView => $args{vmFolderView} , resourcepoolView => $args{tgtResourcePoolView} ,
													hostView => $args{targetHostView} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	my $folderPath 	= $args{sourceNode}->getAttribute('folder_path');
	chop($folderPath);
	if ( ( !defined ( $args{sourceNode}->getAttribute('vmDirectoryPath') ) || ( "" eq $args{sourceNode}->getAttribute('vmDirectoryPath') ) )  
			||( defined ( $args{sourceNode}->getAttribute('vmDirectoryPath') ) && ( $args{sourceNode}->getAttribute('vmDirectoryPath') !~ /^$folderPath$/ ) ) )
	{
		$returnCode = DatastoreOpts::DeletePath( pathToDelete => $CreatedDsPath , datacenterView => $args{tgtDatacenterView} );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
	
	return ( $Common_Utils::SUCCESS );
}

1;