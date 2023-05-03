=head
	-----Failback_Resume.pl-----
	1. Used for Failback or Resume protection.
	2. It can also be used for DR-Drill option( but need to check with Retention details ).
	3. In Resume we will protect what is the already protected configuration. New updates will not be covered as 
		we are not connecting to source again.
	4. In failback latest configuration on DR-site will be taken in to consideration(Need to check with newly added RDM devices).
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
				  	resume 		=> {type => "=s", help => "Is it resume protection?(yes/no).", required => 0,},
				  	failback	=> {type => "=s", help => "Is it failback protection?(yes/no).", required => 0,},
				  	upgrade_tools => {type => "=s", help => "List of machine UUID's, which tools have to be upgrade.",required => 0,},
				  	upgrade_vms	=> {type => "=s", help => "List of machine UUID's, which vmx version have to be upgrade.",required => 0,},
				  );

#####Failback#####
##Description		:: Does the Failback protection. By this time it expects checks for 
##						1. Presence of machine on Target Validates.
##						2. The datastore should be accessible for Master Target.
##						3. No need to move from its resource pool.
##						4. What if some manual changes are made to machine at Target side.
##						5. Sees all the disks are there in Target VM. Should we create the disk if any misses?
##						6. If disk is created in case of 5, we need to check datastore free Space in readiness checks.
##						7. Same is the case with VM not being present at DR-Site.
##Input 			:: File path which contains information about all machines whicg are to be resumed.
##Output 			:: Returns SUCCESS else FAILURE.
#####Failback####
sub Failback
{
	my %args							= @_;
	my $mtView 							= "";
	my $failed 							= $Common_Utils::SUCCESS;
	
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
	
	( $returnCode ,my $cxIpPortNum )= Common_Utils::FindCXIP();
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my @CreatedVirtualMachineInfo		= ();
	my %mapSourceIdDiskSign				= ();
	foreach my $plan ( @$planNodes )
	{
		my $planName					= $plan->getAttribute('plan');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		my @targetNodes					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetHost					= $targetNodes[0]->getElementsByTagName('host');	
		my $IsItvCenter					= $targetHost[0]->getAttribute('vCenterProtection');
		my $mtHostId					= $targetHost[0]->getAttribute('inmage_hostid');
		Common_Utils::WriteMessage("Failback protection plan name = $planName.",2);
		Common_Utils::WriteMessage("IsItvCenter = $IsItvCenter.",2);	
		
		( my $returnCode , $mtView )	= VmOpts::GetVmViewByUuid( $targetHost[0]->getAttribute('source_uuid') );
		if ( $Common_Utils::SUCCESS ne $returnCode )
		{
			$failed 				= $Common_Utils::FAILURE;
			last;
		}
		
		if ( $targetHost[0]->getAttribute('hostversion') lt "4.0.0" )
		{
			$returnCode 				= VmOpts::PowerOff( $mtView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$failed 				= $Common_Utils::FAILURE;
				last;
			}
		}
		
		if( "linux" eq lc( $targetHost[0]->getAttribute('os_info') ) )
		{
			$returnCode		= XmlFunctions::DummyDiskCleanUpTask( hostId => $mtHostId, cxIpPortNum => $cxIpPortNum, hostNode => $targetNodes[0] );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
			}
		}
		
		if ( $Common_Utils::SUCCESS != VmOpts::RemoveDummyDisk( $mtView ) )
		{
			$failed 					= $Common_Utils::FAILURE;
			last;
		}
		
		$returnCode						= VmOpts::SetVmExtraConfigParam( vmView => $mtView, paramName => "disk.enableuuid", paramValue => "true" );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 					= $Common_Utils::FAILURE;
			Common_Utils::WriteMessage("ERROR :: Failed to set disk.enableuuid value as true to Master Target.",3);
			last;
		}
		
		$returnCode						= VmOpts::AddSCSIControllersToVM( vmView => $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 					= $Common_Utils::FAILURE;
			Common_Utils::WriteMessage("ERROR :: Failed to Add SCSI controllers to Master Target.",3);
			last;
		}
		
		( $returnCode, $mtView )		= VmOpts::UpdateVmView( $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 					= $Common_Utils::FAILURE;
			Common_Utils::WriteMessage("ERROR :: Failed to get latest view of Master Target.",3);
			last;
		}
		
		my $targetvSphereHostName		= $targetHost[0]->getAttribute('vSpherehostname');
		Common_Utils::WriteMessage("Target chosen = $targetvSphereHostName.",2);
		( $returnCode ,my $tgthostView )= HostOpts::GetSubHostView( $targetvSphereHostName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed						= $Common_Utils::FAILURE;
			last;
		}
		
		my $targetDatacenter			= $targetHost[0]->getAttribute('datacenter');
	   	( $returnCode , my $tgtDatacenterView)	= DatacenterOpts::GetDataCenterView( $targetDatacenter );
	   	if ( $returnCode ne $Common_Utils::SUCCESS )
	   	{
	   		$failed 					= $Common_Utils::FAILURE;
	   		last;
	   	}
		
		( $returnCode , my $dvPortGroupInfo ) 	= HostOpts::GetDvPortGroups( $tgthostView );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			$failed 					= $Common_Utils::FAILURE;
			last;
		}
		
		my @sourceHosts					= $plan->getElementsByTagName('SRC_ESX');
		foreach my $sourceHost	( @sourceHosts )
		{
			my $foundCluster			= 0;
			my %createdDisks			= ();
			my @hostsInfo				= $sourceHost->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				my %CreatedVmInfo		= ();
				$CreatedVmInfo{planName}= $planName;
				my $displayName 		= $host->getAttribute('display_name');
				$CreatedVmInfo{uuid}	= $host->getAttribute('target_uuid');
				if ( ( ! defined $host->getAttribute('target_uuid') ) || ( $host->getAttribute('target_uuid') =~ /^$/ ) )
				{
					$returnCode 		= FolderOpts::CreateVmConfig( $host, $targetHost[0], \%CreatedVmInfo , $tgthostView , $tgtDatacenterView  );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						Common_Utils::WriteMessage("Creation of virtual machine configuration failed on target side.",3);
						push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
						$failed			= $Common_Utils::FAILURE;
						last;
					}
				}
				
				if( "yes" eq $host->getAttribute('cluster') )
				{
					$foundCluster	= 1;
				}

				( $returnCode , my $vmView )= VmOpts::GetVmViewByUuid( $CreatedVmInfo{uuid} );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
					$failed				= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= MergeSnapShots( $vmView );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
					return ( $Common_Utils::FAILURE );
				}
				
				$returnCode 			= VmOpts::RemoveUnProtectedDisks( $host , $vmView );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
				}
				
				$returnCode 	 		= VmOpts::RemoveDevices( vmView => $vmView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= VmOpts::UpdateVm( vmView => $vmView , sourceHost => $host , isItvCenter => $IsItvCenter ,
															dvPortGroupInfo => $dvPortGroupInfo );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode  			= VmOpts::OnOffMachine( $vmView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					#for this reason should we stop the protection flow?
					#return $Common_Utils::FAILURE;
					#for cluster automation moved to before adding disks
				}
				
				( $returnCode, $vmView )= VmOpts::UpdateVmView( $vmView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
								
				$returnCode 			= VmOpts::ConfigureDisks( sourceHost => $host , vmView => $vmView, isCluster => $foundCluster, createdDisks => \%createdDisks );
				
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					Common_Utils::WriteMessage("ERROR :: Failed to configure disks in machine \"$displayName\".",3);
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				if( "windows" eq lc( $host->getAttribute('os_info') ) )
				{
					my @hostNameSplit 		= split('\.',$host->getAttribute('hostname') );
					Common_Utils::WriteMessage("Setting Host to $hostNameSplit[0].",2);
					$host->setAttribute( 'hostname' , $hostNameSplit[0] );
				}
				
				( $returnCode, $vmView )= VmOpts::UpdateVmView( $vmView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= VmOpts::ReuseExistingDiskSpec( CreatedVmInfo => \%CreatedVmInfo , vmView => $vmView  , mtView => $mtView );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
					push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
					$failed				= $Common_Utils::FAILURE;
					last;
				}
				
				( $returnCode, $mtView )= VmOpts::UpdateVmView( $mtView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				my $guestId				= $host->getAttribute('alt_guest_name');
				$returnCode 			= VmOpts::CheckGuestOsFullName( $vmView, $guestId );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= VmOpts::MapSourceScsiNumbersOfHost( cxIpAndPortNum => $cxIpPortNum , host => $host, diskSignMap => \%mapSourceIdDiskSign );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= XmlFunctions::UpdateDRMachineConfigToXmlNode( vmView => $vmView , mtView => $mtView , sourceHost => $host, operation => "failback", diskSignMap => \%mapSourceIdDiskSign );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				} 
				push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
			}
			
			if( ($foundCluster ) )
			{
				$returnCode 			= VmOpts::ConfigureClusterDisks( sourceHosts => \@hostsInfo, diskSignMap => \%mapSourceIdDiskSign );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					Common_Utils::WriteMessage("ERROR :: Failed to add cluster disks to other nodes.",3);
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
			}
		}
	}
	
	if ( $failed )
	{
		$returnCode 			= DeleteConfigurationOnTarget( \@CreatedVirtualMachineInfo );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed while deleting new configurations created at DR site.",3);
			Common_Utils::WriteMessage("ERROR :: Please clear all machines and disks which are created on DR site under this plan.",3);
		}  
		$failed 				= $Common_Utils::FAILURE;
	}
	
	if ( "poweredOn" ne Common_Utils::GetViewProperty( $mtView,'summary.runtime.powerState')->val )
	{
		$returnCode 			= VmOpts::PowerOn( $mtView );
		if ( $returnCode == $Common_Utils::SUCCESS )
		{
			my @poweredOnMachineInfo= ( Common_Utils::GetViewProperty( $mtView,'summary.config.uuid') );
			if ( $Common_Utils::SUCCESS ne VmOpts::AreMachinesBooted( \@poweredOnMachineInfo ) )
			{
				$failed 		= $Common_Utils::FAILURE;
			}
		}
		elsif ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 			= $Common_Utils::FAILURE;
		}
	}
	
	$returnCode					= XmlFunctions::SaveXML( $planNodes , $args{file} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to modify ESX.xml file.",3);
		$failed 	 			= $Common_Utils::FAILURE;
	}
		
	if ( -1 != $#CreatedVirtualMachineInfo )
	{
		$returnCode 			= XmlFunctions::WriteInmageScsiUnitsFile( file => $args{file} );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to Write InMage SCSI units file.",3);
			$failed 			= $Common_Utils::FAILURE;
		}
	} 
		
	return ( $failed );
	
}

#####DeleteConfigurationOnTarget#####
##Description 		:: Clears all the changes made at DR site, like deleting machines created till point and VMDK's as well.
##Input 			:: Summary of actions performed at DR site for each of the machine.
##Output 			:: Returns SUCCESS or FAILURE.
#####DeleteConfigurationOnTarget#####
sub DeleteConfigurationOnTarget
{
	my $CreatedVirtualMachineInfo 	= shift;
	
	foreach my $vmInfo( @$CreatedVirtualMachineInfo )
	{
		#check if the machine is created, delete the virtual machine which will result in Clearing all it's VMDK and it's folder.
		
		#if machine is not created, is there any chance for presence of folder while protection?
		
		#yes, if we delete the VM before detaching disk from master target.
		
		#remove VMDK form master target if they are already attached.		
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####MergeSnapShots#####
##Description		:: Merges snapshots if any found.
##Input 			:: virtual machine  view.
##Ouput				:: Returns SUCCESS else FAILURE.
#####MergeSnapShots#####
sub MergeSnapShots
{
	my $vmView 		= shift;
	
	if ( defined $vmView->snapshot )
	{
		if ( $Common_Utils::SUCCESS != VmOpts::MergeSnapshots( $vmView ) )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####Resume#####
##Description		:: Does the resume protection. By this time it expects checks for 
##						1. Presence of machine on Target Validates.
##						2. The datastore should be accessible for Master Target.
##						3. No need to move from its resource pool.
##						4. What if some manual changes are made to machine at Target side.
##						5. Sees all the disks are there in Target VM. Should we create the disk if any misses?
##						6. If disk is created in case of 5, we need to check datastore free Space in readiness checks.
##						7. Same is the case with VM not being present at DR-Site.
##Input 			:: File path which contains information about all machines whicg are to be resumed.
##Output 			:: Returns SUCCESS else FAILURE.
#####Resume####
sub Resume
{
	my %args							= @_;
	my $mtView 							= "";
	my $failed 							= $Common_Utils::SUCCESS;
	
	my ( $returnCode , $docNode ) 		= XmlFunctions::GetPlanNodes( $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get plan information from file \"args{file}\".",3);
		return $Common_Utils::FAILURE;
	}
	
	my $planNodes 						= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 						= [$docNode];
	}
	
	( $returnCode ,my $cxIpPortNum )= Common_Utils::FindCXIP();
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my @CreatedVirtualMachineInfo		= ();
	my %mapSourceIdDiskSign				= ();
	foreach my $plan ( @$planNodes )
	{
		my $planName					= $plan->getAttribute('plan');
		Common_Utils::WriteMessage("Trying to re-create plan \"$planName\".",2);
		$Common_Utils::vContinuumPlanDir = $plan->getAttribute('xmlDirectoryName');
		
		my @targetHost					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetInfo					= $targetHost[0]->getElementsByTagName('host');		
		my $mtHostId					= $targetInfo[0]->getAttribute('inmage_hostid');	
		
		( my $returnCode , $mtView )	= VmOpts::GetVmViewByUuid( $targetInfo[0]->getAttribute('source_uuid') );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			$failed 					= $Common_Utils::FAILURE;
			last;
		}
		
		if ( $targetInfo[0]->getAttribute('hostversion') lt "4.0.0" )
		{
			$returnCode 				= VmOpts::PowerOff( $mtView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$failed 				= $Common_Utils::FAILURE;
				last;
			}
		}
		
		if( "linux" eq lc( $targetInfo[0]->getAttribute('os_info') ) )
		{
			$returnCode		= XmlFunctions::DummyDiskCleanUpTask( hostId => $mtHostId, cxIpPortNum => $cxIpPortNum, hostNode => $targetHost[0] );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
			}
		}
		
		if ( $Common_Utils::SUCCESS != VmOpts::RemoveDummyDisk( $mtView ) )
		{
			$failed 					= $Common_Utils::FAILURE;
			Common_Utils::WriteMessage("ERROR :: Failed to remove dummy disk from master target.",3);
			last;
		}
		
		$returnCode						= VmOpts::SetVmExtraConfigParam( vmView => $mtView, paramName => "disk.enableuuid", paramValue => "true" );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 					= $Common_Utils::FAILURE;
			Common_Utils::WriteMessage("ERROR :: Failed to set disk.enableuuid value as true to Master Target.",3);
			last;
		}		
		
		$returnCode						= VmOpts::AddSCSIControllersToVM( vmView => $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 					= $Common_Utils::FAILURE;
			Common_Utils::WriteMessage("ERROR :: Failed to Add SCSI controllers to Master Target.",3);
			last;
		}
		
		( $returnCode, $mtView )		= VmOpts::UpdateVmView( $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 					= $Common_Utils::FAILURE;
			Common_Utils::WriteMessage("ERROR :: Failed to get latest view of Master Target.",3);
			last;
		}
		
		my @sourceInfo					= $plan->getElementsByTagName('SRC_ESX');
		foreach my $sourceInfo	( @sourceInfo )
		{
			my @hostsInfo				= $sourceInfo->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				my %CreatedVmInfo		= ();
				$CreatedVmInfo{planName}= $planName;
				$CreatedVmInfo{uuid}	= $host->getAttribute('target_uuid');
				$CreatedVmInfo{hostName}= $host->getAttribute('hostname');
				
				if( "windows" eq lc( $host->getAttribute('os_info') ) )
				{
					my @hostNameSplit 	= split('\.',$host->getAttribute('hostname') );
					Common_Utils::WriteMessage("Setting Host to $hostNameSplit[0].",2);
					$host->setAttribute( 'hostname' , $hostNameSplit[0] );
				}
				
				( $returnCode, my $vmView )= VmOpts::GetVmViewByUuid( $CreatedVmInfo{uuid} );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
					push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
					$failed				= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= VmOpts::ReuseExistingDiskSpec( CreatedVmInfo => \%CreatedVmInfo , vmView => $vmView  , mtView => $mtView );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
					push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
					$failed				= $Common_Utils::FAILURE;
					last;
				}
				
				( $returnCode, $mtView )= VmOpts::UpdateVmView( $mtView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				my $guestId				= $host->getAttribute('alt_guest_name');
				$returnCode 			= VmOpts::CheckGuestOsFullName( $vmView, $guestId );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
								
				$returnCode 			= XmlFunctions::UpdateDRMachineConfigToXmlNode( vmView => $vmView , mtView => $mtView , sourceHost => $host, operation => "resume", diskSignMap => \%mapSourceIdDiskSign );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
			}
		}
	}
	
	if ( $failed )
	{
		$returnCode 			= DeleteConfigurationOnTarget( \@CreatedVirtualMachineInfo );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed while deleting new configurations created at DR site.",3);
			Common_Utils::WriteMessage("ERROR :: Please clear all machines and disks which are created on DR site under this plan.",3);
		}  
		$failed 				= $Common_Utils::FAILURE;
	}
	
	if ( "poweredOn" ne Common_Utils::GetViewProperty( $mtView,'summary.runtime.powerState')->val )
	{
		$returnCode 			= VmOpts::PowerOn( $mtView );
		if ( $returnCode == $Common_Utils::SUCCESS )
		{
			my @poweredOnMachineInfo= ( Common_Utils::GetViewProperty( $mtView,'summary.config.uuid') );
			if ( $Common_Utils::SUCCESS ne VmOpts::AreMachinesBooted( \@poweredOnMachineInfo ) )
			{
				$failed 		= $Common_Utils::FAILURE;
			}
		}
		elsif ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 			= $Common_Utils::FAILURE;
		}
	}
	
	$returnCode					= XmlFunctions::SaveXML( $planNodes , $args{file} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to modify ESX.xml file.",3);
		$failed 	 			= $Common_Utils::FAILURE;
	}
		
	if ( -1 != $#CreatedVirtualMachineInfo )
	{
		$returnCode 			= XmlFunctions::WriteInmageScsiUnitsFile( file => $args{file} );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to Write InMage SCSI units file.",3);
			$failed 			= $Common_Utils::FAILURE;
		}
	} 
		
	return ( $failed );
} 
#####UpgradeVmwareToolsMachines#####
##Description 		:: upgrade vmware tools for given virtual machines.
##Input 			:: list of machines to be upgrade tools.
##Output 			:: Returns SUCCESS else FAILURE.
#####UpgradeVmwareToolsMachines#####
sub UpgradeVmwareToolsMachines
{
	my $listOfUuids		= shift;
	my @vmViewsList		= ();
	my $ReturnCode		= $Common_Utils::SUCCESS;
	if ( ( !defined $listOfUuids ) || ( "" eq $listOfUuids ) )
	{
		Common_Utils::WriteMessage("ERROR :: Found empty list specified for list of machines to be upgrade tools.",3);
		Common_Utils::WriteMessage("ERROR :: Please specify UUID's of machines which are to be upgrade tools.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my @vmUuidsList		= split( "!@!@!", $listOfUuids );		
	Common_Utils::WriteMessage("upgrade tools vm list = @vmUuidsList.",2);
	foreach my $uuid ( @vmUuidsList )
	{
		my ( $returnCode , $vmView ) 	= VmOpts::GetVmViewByUuid( $uuid );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find machine with uuid = \"$uuid\".",3);
			$ReturnCode	= $Common_Utils::FAILURE;
			next;
		}
		push @vmViewsList, $vmView;
	}
	my $returnCode	= VmOpts::UpgradeVmwareTools( \@vmViewsList );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to Upgrade vmware tools.",3);
		$ReturnCode	= $Common_Utils::FAILURE;
	}	
	return ( $ReturnCode );
}

#####UpgradeVmVersion#####
##Description 		:: upgrade vmware hardware version for given virtual machines.
##Input 			:: list of uuids of machines to be upgraded.
##Output 			:: Returns SUCCESS else FAILURE.
#####UpgradeVmVersion#####
sub UpgradeVmVersion
{
	my $listOfUuids		= shift;
	my @vmViewsList		= ();
	my $ReturnCode		= $Common_Utils::SUCCESS;
	if ( ( !defined $listOfUuids ) || ( "" eq $listOfUuids ) )
	{
		Common_Utils::WriteMessage("ERROR :: Found empty list specified for list of machines to be upgrade vmx version.",3);
		Common_Utils::WriteMessage("ERROR :: Please specify UUID's of machines which are to be upgrade vmx version.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my @vmUuidsList		= split( "!@!@!", $listOfUuids );		
	Common_Utils::WriteMessage("upgrade vmx version vms list = @vmUuidsList.",2);
	foreach my $uuid ( @vmUuidsList )
	{
		my ( $returnCode , $vmView ) 	= VmOpts::GetVmViewByUuid( $uuid );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find machine with uuid = \"$uuid\".",3);
			$ReturnCode	= $Common_Utils::FAILURE;
			next;
		}
		
		push @vmViewsList, $vmView;
	}
	my $returnCode	= VmOpts::UpgradeVms( \@vmViewsList );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to Upgrade vmware hardware version.",3);
		$ReturnCode	= $Common_Utils::FAILURE;
	}	
	return ( $ReturnCode );
}

BEGIN:

my $ReturnCode				= 1;
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
	$ReturnCode		= Common_Utils::Connect( Server_IP => $serverIp, UserName => Opts::get_option('username'), Password => Opts::get_option('password') );
	if ( $Common_Utils::SUCCESS == $ReturnCode )
	{
		$file	= "$Common_Utils::vContinuumMetaFilesPath/ESX.xml";
		eval
		{
			if ( Opts::option_is_set('upgrade_tools') )
			{
				Common_Utils::WriteMessage("Performing upgrade tools.",2);
				$ReturnCode	= UpgradeVmwareToolsMachines( Opts::get_option('upgrade_tools') );
			}
			if ( Opts::option_is_set('upgrade_vms') )
			{
				Common_Utils::WriteMessage("Performing upgrade tools.",2);
				$ReturnCode	= UpgradeVmVersion( Opts::get_option('upgrade_vms') );
			}
			if ( ( defined Opts::get_option('resume') ) && ( "yes" eq lc( Opts::get_option('resume') ) ) )
			{
				Common_Utils::WriteMessage("Performing resume protection.",2);
				$ReturnCode	= Resume(
										file 	=> $file,
									);
			}
			
			if ( ( defined Opts::get_option('failback') ) && ( "yes" eq lc( Opts::get_option('failback') ) ) )
			{
				Common_Utils::WriteMessage("Performing failback protection.",2);
				$ReturnCode	= Failback(
										file	=> $file,
									  );
			}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			$ReturnCode		= $Common_Utils::FAILURE;
		}		
	}
}

Common_Utils::LoggedData( tempLog => "$Common_Utils::vContinuumLogsPath/$logFileName" , planLog => $Common_Utils::vContinuumPlanDir );

exit( $ReturnCode );
