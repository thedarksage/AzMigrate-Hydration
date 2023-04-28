=head
	Protect.pl
	1. It is called for normal protection.
	Call and it's behaviour are explained in detail as we go on.
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
				  incremental_disk 	=> {type => "=s", help => "Performing the Incremental_disks (yes/no)?", required => 0,},
				  power_on			=> {type => "=s", help => "List of machine UUID's, which are to be powered on.", required => 0,},
				  power_off 		=> {type => "=s", help => "List of machine UUID's, which are to be powered off.",required => 0,},
				  dr_drill 			=> {type => "=s", help => "If Dr_Drill, set this argument to 'yes'.", required => 0,},
				  update 			=> {type => "=s", help => "specify parameter to update protections with 5.5/5.5Sp1 to CX.", required => 0,},
				  dr_drill_array	=> {type => "=s", help => "If array based Dr_Drill, set this argument to 'yes'.", required => 0,},
				  array_datastore	=> {type => "=s", help => "To create datastore of array based lun vsnap, set this argument to 'yes'.", required => 0,},
				  resize		 	=> {type => "=s", help => "Performing the Incremental_disks (yes/no)?", required => 0,},
				  extend_disk	 	=> {type => "=s", help => "Eager zeroing the cluster disks (yes/no)?", required => 0,},
				  datacenter	 	=> {type => "=s", help => "For eager zeroing cluster disk respective VM's datacenter name", required => 0,},
				  vmdkname		 	=> {type => "=s", help => "Eager zeroing cluster disk name with full path", required => 0,},
				  );

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

#####IncrementalProtection#####
##Description		:: Performs the Incremental protection.
##Input 			:: Complete file name for ESX.xml file.
##Output			:: Returns SUCCESS else FAILURE.
#####IncrementalProtection#####
sub IncrementalProtection
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
		my @targetNodes					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetHost					= $targetNodes[0]->getElementsByTagName('host');	
		my $IsItvCenter					= $targetHost[0]->getAttribute('vCenterProtection');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		my $mtHostId					= $targetHost[0]->getAttribute('inmage_hostid');
		Common_Utils::WriteMessage("Incremental protection plan name = $planName.",2);
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
								
				( $returnCode,my $vmView )= VmOpts::GetVmViewByUuid( $CreatedVmInfo{uuid} );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
					$failed				= $Common_Utils::FAILURE;
					last;
				}
				
				if( "yes" eq $host->getAttribute('cluster') )
				{
					$foundCluster	= 1;
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
				
				$returnCode 			= VmOpts::MapSourceScsiNumbersOfHost( cxIpAndPortNum => $cxIpPortNum , host => $host );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= XmlFunctions::UpdateDRMachineConfigToXmlNode( vmView => $vmView , mtView => $mtView , sourceHost => $host, operation => "protection", diskSignMap => \%mapSourceIdDiskSign );
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
	
	if ( "poweredOn" ne Common_Utils::GetViewProperty( $mtView, 'summary.runtime.powerState')->val )
	{
		$returnCode 			= VmOpts::PowerOn( $mtView );
		if ( $returnCode == $Common_Utils::SUCCESS )
		{
			my @poweredOnMachineInfo= ( Common_Utils::GetViewProperty( $mtView, 'summary.config.uuid') );
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

#####Protect#####
##Description 		:: This is the main function for Protect.pl file. It reads ESX.xml file for source machine information and 
##						for Master target information. This calls all sub-sequent calls to create a machine on DR site.
##Input 			:: Complete file name for ESX.xml file.
##Output 			:: Returns SUCCESS on successfully creating all virtual machines else FAILURE.
#####Protect#####
sub Protect
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
		my @targetNodes					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetHost					= $targetNodes[0]->getElementsByTagName('host');	
		my $IsItvCenter					= $targetHost[0]->getAttribute('vCenterProtection');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		my $mtHostId					= $targetHost[0]->getAttribute('inmage_hostid');
		Common_Utils::WriteMessage("Protection plan name = $planName.",2);
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
			$failed 			= $Common_Utils::FAILURE;
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
				#Check whether it is under resource pool ? if not we can change it.
				my $resourcePoolGrpName = $host->getAttribute('resourcepoolgrpname');
				if($vmView->resourcePool->value ne $resourcePoolGrpName)
				{
					( $returnCode , my $resourcePoolView ) =  ResourcePoolOpts::FindResourcePool( $resourcePoolGrpName );
					$returnCode = ResourcePoolOpts::MoveVmToResourcePool( $resourcePoolView, $vmView );
				}
				
				$returnCode 	 		= VmOpts::RemoveDevices( vmView => $vmView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= VmOpts::RemoveUnProtectedDisks( $host , $vmView );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
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
				
				$returnCode 			= XmlFunctions::UpdateDRMachineConfigToXmlNode( vmView => $vmView , mtView => $mtView , sourceHost => $host, operation => "protection", diskSignMap => \%mapSourceIdDiskSign );
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
	
	#we have to take care of this order.
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
			my @poweredOnMachineInfo= ( Common_Utils::GetViewProperty( $mtView, 'summary.config.uuid') );
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

#####PowerOffMachines#####
##Description 		:: power's off virtual machines.
##Input 			:: List of Machine UUID which are to be powered-on.
##Output 			:: Returns SUCCESS else FAILURE.
#####PowerOffMachines#####
sub PowerOffMachines
{
	my $listOfUuids		= shift;
	my $ReturnCode		= $Common_Utils::SUCCESS;
	if ( ( !defined $listOfUuids ) || ( "" eq $listOfUuids ) )
	{
		Common_Utils::WriteMessage("ERROR :: Found empty list specified for list of machines to be powered-off.",3);
		Common_Utils::WriteMessage("ERROR :: Please specify UUID's of machines which are to be powered off.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my @powerOffList		= split( "!@!@!", $listOfUuids );		
	Common_Utils::WriteMessage("Power Off list = @powerOffList.",2);
	foreach my $uuid ( @powerOffList )
	{
		my ( $returnCode , $vmView ) 	= VmOpts::GetVmViewByUuid( $uuid );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find machine with uuid = \"$uuid\".",3);
			$ReturnCode	= $Common_Utils::FAILURE;
			next;
		}
		if ( $Common_Utils::SUCCESS ne VmOpts::PowerOff( $vmView ) ) 
		{
			$ReturnCode	= $Common_Utils::FAILURE;	
		}
	}
	
	return ( $ReturnCode );
}

#####PowerOnMachines#####
##Description 		:: power's on virtual machines.
##Input 			:: List of Machine UUID which are to be powered-on.
##Output 			:: Returns SUCCESS else FAILURE.
#####PowerOnMachines#####
sub PowerOnMachines
{
	my $listOfUuids		= shift;
	my $ReturnCode		= $Common_Utils::SUCCESS;
	if ( ( !defined $listOfUuids ) || ( "" eq $listOfUuids ) )
	{
		Common_Utils::WriteMessage("ERROR :: Found empty list specified for list of machines to be powered-on.",3);
		Common_Utils::WriteMessage("ERROR :: Please specify UUID's of machines which are to be powered on.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my @powerOnList		= split( "!@!@!", $listOfUuids );		
	Common_Utils::WriteMessage("Power Off list = @powerOnList.",2);
	foreach my $uuid ( @powerOnList )
	{
		my ( $returnCode , $vmView ) 	= VmOpts::GetVmViewByUuid( $uuid );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find machine with uuid = \"$uuid\".",3);
			$ReturnCode	= $Common_Utils::FAILURE;
			next;
		}
		
		if ( $Common_Utils::SUCCESS ne VmOpts::PowerOn( $vmView ) ) 
		{
			$ReturnCode	= $Common_Utils::FAILURE;	
		}
	}
	
	return ( $ReturnCode );
}

#####AppendTargetXmlToCX#####
##Description		:: It downloads the Target XML from Target ESX/vSphere and appends it to CX.
##Input 			:: Target ESX IP.
##Output 			:: Returns SUCCESS else FAILURE.
#####AppendTargetXmlToCX#####
sub AppendTargetXmlToCX
{
	my $esxIp		= shift;
		
	my $IsItvCenter	= VmOpts::IsItvCenter();
	if ( "yes" eq lc( $IsItvCenter ) )
	{
		Common_utils::WriteMessage("ERROR :: $esxIp is a vCenter IP.",3);
		Common_Utils::WriteMessage("ERROR :: Please specify an ESX IP in the update command.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my( $returnCode , $hostViews ) 		= HostOpts::GetHostViewsByProps( );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	( $returnCode , my $datacenterViews )	= DatacenterOpts::GetDataCenterViews();
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	my $datacenterName						= $$datacenterViews[0]->name;
	Common_Utils::WriteMessage("Datacenter Name for esx $esxIp = $datacenterName.",2);
	
	my $hostView							= $$hostViews[0];
	( $returnCode , my $listDatastoreInfo )	= DatastoreOpts::GetDatastoreInfo( $hostViews , $datacenterViews );
	if ( $returnCode != $Common_Utils::SUCCESS ) 
	{
		return ( $Common_Utils::FAILURE );
	}

	my $EsxMasterFileName 					 = "ESX_Master_".$esxIp.".xml";
	( $returnCode , my $EsxMasterFilesInfo ) = DatastoreOpts::GetLatestFileInfo( $hostView , $EsxMasterFileName , "fileinfo" , $listDatastoreInfo );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	if (  0 != scalar( keys %$EsxMasterFilesInfo ) )
	{
		Common_Utils::WriteMessage("Found atleast one ESX Master file which belongs to $esxIp.",2);
		if ( $Common_Utils::SUCCESS ne DatastoreOpts::DownloadFiles( $EsxMasterFilesInfo , $datacenterName ) )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		if ( $Common_Utils::SUCCESS ne XmlFunctions::UpdateTempMasterXmlCX( fileName => "$Common_Utils::vContinuumMetaFilesPath/$EsxMasterFileName" ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update $EsxMasterFileName with CX MasterConfig file.",2);
			return ( $Common_Utils::FAILURE );
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####ArrayBasedDrDrill#####
##Description 		:: It reads snapshot.xml file for source machine information and for Master target information. 
##						This calls all sub-sequent calls to prepare a Dr-Drill machine on Snapshot Array.
##Input 			:: Complete file name for snapshot.xml file.
##Output 			:: Returns SUCCESS on successfully creating all virtual machines else FAILURE.
#####ArrayBasedDrDrill#####
sub ArrayBasedDrDrill
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
		my @targetNodes					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetHost					= $targetNodes[0]->getElementsByTagName('host');	
		my $IsItvCenter					= $targetHost[0]->getAttribute('vCenterProtection');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		my $mtHostId					= $targetHost[0]->getAttribute('inmage_hostid');
		Common_Utils::WriteMessage("Array based DrDrill plan name = $planName.",2);
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
			$failed 			= $Common_Utils::FAILURE;
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
		
		( $returnCode , my $dvPortGroupInfo ) 	= HostOpts::GetDvPortGroups( $tgthostView );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			$failed 					= $Common_Utils::FAILURE;
			last;
		}
		
		my $targetDatacenter			= $targetHost[0]->getAttribute('datacenter');
	   	( $returnCode , my $tgtDatacenterView)	= DatacenterOpts::GetDataCenterView( $targetDatacenter );
	   	if ( $returnCode ne $Common_Utils::SUCCESS )
	   	{
	   		$failed 					= $Common_Utils::FAILURE;
	   		last;
	   	}
		
		my $vmFolderView 				= Vim::get_view( mo_ref => $tgtDatacenterView->vmFolder, properties => [] );	
			
		my @sourceHosts					= $plan->getElementsByTagName('SRC_ESX');
		foreach my $sourceHost	( @sourceHosts )
		{
			my $foundCluster			= 0;
			my %createdDisks			= ();
			my %diskUuidChanged			= ();
			my @hostsInfo				= $sourceHost->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				$returnCode				= VmOpts::ChangeDisksUuid( sourceHost => $host, datacenter => $tgtDatacenterView, diskUuidMap => \%diskUuidChanged );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
			}
			
			foreach my $host ( @hostsInfo )
			{
				my %CreatedVmInfo		= ();
				$CreatedVmInfo{planName}= $planName;
				$CreatedVmInfo{uuid}	= $host->getAttribute('target_uuid');
				my $machineName 		= $host->getAttribute('display_name');
				if ( ( defined ( $host->getAttribute('new_displayname') ) ) && ( "" ne $host->getAttribute('new_displayname') ) )
				{
					$machineName		= $host->getAttribute('new_displayname');
				}
				
				my @disksInfo			= $host->getElementsByTagName('disk');
				my $datastoreSelected 	= $disksInfo[0]->getAttribute('datastore_selected');
				my $vmPathName 			= $host->getAttribute('vmx_path');
				my @vmPathSplit			= split ( /\[|\]/ , $vmPathName );
				$vmPathName 			= "[$datastoreSelected]$vmPathSplit[-1]";
				if ( ( defined( $host->getAttribute('vmDirectoryPath') ) ) && ( "" ne $host->getAttribute('vmDirectoryPath') ) )
				{
					$vmPathSplit[-1]	=~ s/^\s+//;
					$vmPathName 		= "[$datastoreSelected] ".$host->getAttribute('vmDirectoryPath')."/".$vmPathSplit[-1];
				}
				Common_Utils::WriteMessage("Registering $machineName, vmPathName = $vmPathName.",2);
				
				my $resourcePoolGroupId	= $host->getAttribute('resourcepoolgrpname');
			   	if ( "" eq $resourcePoolGroupId )
			   	{
			   		$resourcePoolGroupId= $targetHost[0]->getAttribute('resourcepoolgrpname');
			   	}
			   	Common_Utils::WriteMessage("Resource Pool Group Id = $resourcePoolGroupId.",2);
			   	my( $returnCode , $resourcePoolView )= ResourcePoolOpts::FindResourcePool( $resourcePoolGroupId );
			   	if ( $returnCode ne $Common_Utils::SUCCESS )
			   	{
			   		Common_Utils::WriteMessage("ERROR :: Failed to find resourcepool view of \"$resourcePoolGroupId\".",3);
			   		return ( $Common_Utils::FAILURE );
			   	}
				
				( $returnCode , my $vmView )= VmOpts::RegisterVm( vmName => $machineName, vmPathName => $vmPathName, folderView => $vmFolderView, 
																	resourcepoolView => $resourcePoolView,	hostView => $tgthostView );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
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
				
				my @deviceChangeA	= ();
				if( "yes" eq $host->getAttribute('cluster') )
				{
					$foundCluster	= 1;
					
					my $devices 		= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
					my @deviceChangeR	= ();
					foreach my $device ( @$devices )
					{
						my %diskData					= ();
						if ( $device->deviceInfo->label =~ /Hard Disk/i )
						{
							my $diskName 		= $device->backing->fileName;
							my @vmdkNameTarget 	= split( /\[|\]/,$diskName);
							
							my $deviceSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
							push @deviceChangeR, $deviceSpec;							
							
							unless( lc($vmdkNameTarget[1]) eq lc( $datastoreSelected ) )
							{
								Common_Utils::WriteMessage("Datastore name mismatch $vmdkNameTarget[1] and $datastoreSelected.",2);
								my $newDiskName	= "[" . $datastoreSelected . "]" . $vmdkNameTarget[2];
								Common_Utils::WriteMessage("modified vmdk name $newDiskName.",2);
								$device->backing->{fileName}	= $newDiskName;
							}
							$deviceSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('add'), device => $device );
							push @deviceChangeA, $deviceSpec;
						}
					}
					
					if( $#deviceChangeR != -1)
					{
						Common_Utils::WriteMessage("Detaching the vmdks from the VM \"$machineName\".",2);
						my $returnCode	= VmOpts::ReconfigVm( vmView => $vmView , changeConfig => {deviceChange => \@deviceChangeR });
						if ( $returnCode != $Common_Utils::SUCCESS )
						{
							Common_Utils::WriteMessage("ERROR :: Reconfiguration failed for the VM \"$machineName\" .",3);
							$failed	= $Common_Utils::FAILURE;
							last;
						}
						
						( $returnCode, $vmView )= VmOpts::UpdateVmView( $vmView );
						if ( $returnCode != $Common_Utils::SUCCESS )
						{
							Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$machineName\".",2);
						}
					}
				}
				
				if( "windows" eq lc( $host->getAttribute('os_info') ) )
				{
					my @hostNameSplit 		= split('\.',$host->getAttribute('hostname') );
					Common_Utils::WriteMessage("Setting Host to $hostNameSplit[0].",2);
					$host->setAttribute( 'hostname' , $hostNameSplit[0] );
				}
				
				#to answer poweron vm as "i copy it"
				$Common_Utils::PowerOnAnswer = 2;
				$returnCode 			= VmOpts::OnOffMachine( $vmView );
				if ( $returnCode ne $Common_Utils::SUCCESS )
				{
					# ignoring as "power-on" failed due to invalid mac address range.
					# uuid validated while power-on , so seems to be not an issue.
				}
				$Common_Utils::PowerOnAnswer = 1;
				
				( $returnCode, $vmView )= VmOpts::UpdateVmView( $vmView );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				my $uuid	= Common_Utils::GetViewProperty( $vmView, 'summary.config.uuid');
				$host->setAttribute( 'target_uuid', $uuid );
				Common_Utils::WriteMessage("New uuid of \"$machineName\" is \"$uuid\".",2);
				
				if ( "yes" eq $host->getAttribute('cluster') )
				{
					if( $#deviceChangeA == -1)
					{
						Common_Utils::WriteMessage("ERROR :: Failed to find attachable disks to machine \"$machineName\".",3);
						$failed 			= $Common_Utils::FAILURE;
						last;
					}
					
					Common_Utils::WriteMessage("Attaching the vmdks back to the VM \"$machineName\".",2);
					my $returnCode	= VmOpts::ReconfigVm( vmView => $vmView , changeConfig => {deviceChange => \@deviceChangeA });
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						Common_Utils::WriteMessage("ERROR :: Reconfiguration failed for the VM \"$machineName\" .",3);
						$failed	= $Common_Utils::FAILURE;
						last;
					}
					
					( $returnCode, $vmView )= VmOpts::UpdateVmView( $vmView );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$machineName\".",2);
					}
				}
		
				$returnCode 			= ConfigureClonedDisks( sourceHost => $host , vmView => $vmView, isCluster => $foundCluster, createdDisks => \%createdDisks );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					Common_Utils::WriteMessage("ERROR :: Failed to configure disks in machine \"$machineName\".",3);
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
					
				$host->setAttribute( 'vm_console_url', VmOpts::GetVmWebConsoleUrl( $vmView ) );
				
				my $guestId				= $host->getAttribute('alt_guest_name');
				$returnCode 			= VmOpts::CheckGuestOsFullName( $vmView, $guestId );
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
				
				$returnCode 			= VmOpts::MapSourceScsiNumbersOfHost( cxIpAndPortNum => $cxIpPortNum , host => $host, diskSignMap => \%mapSourceIdDiskSign );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode 			= XmlFunctions::UpdateDRMachineConfigToXmlNode( vmView => $vmView , mtView => $mtView , sourceHost => $host, operation => "protection", diskSignMap => \%mapSourceIdDiskSign );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed 			= $Common_Utils::FAILURE;
					last;
				} 
				push @CreatedVirtualMachineInfo , \%CreatedVmInfo;
			}
			
			if( $foundCluster)
			{
				$returnCode 			= VmOpts::ConfigureClusterDisks( sourceHosts => \@hostsInfo, diskSignMap => \%mapSourceIdDiskSign, operation => "array" );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					Common_Utils::WriteMessage("ERROR :: Failed to add cluster disks to other nodes.",3);
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
			}
		}
	}
	
	#we have to take care of this order.
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
	
	if ( "poweredOn" ne Common_Utils::GetViewProperty( $mtView, 'summary.runtime.powerState')->val )
	{
		$returnCode 			= VmOpts::PowerOn( $mtView );
		if ( $returnCode == $Common_Utils::SUCCESS )
		{
			my @poweredOnMachineInfo= ( Common_Utils::GetViewProperty( $mtView, 'summary.config.uuid') );
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

#####ConfigureClonedDisks#####
##Description	:: It validates cloned Disks/uses an existing disk.
##					1. In all cases it check whether the SCSI controller exists or not. IF does not exist, creates it.
##					2. If disk is to be re-used, it checks for its size as well.					
##					3. If vmdk is RDM, it deleted exisitng disk and re-creates it.
##Input 		:: Configuration of machine to be created( XML::Element ) and Virtual Machine View to which disks are to be configured.
##Output 		:: Returns SUCCESS if everything is smooth else in any case it returns FAILURE.
#####ConfigureClonedDisks#####
sub ConfigureClonedDisks
{
	my %args 		= @_;
	my $machineName = $args{vmView}->name;
	my $createdDisks= $args{createdDisks};
	my $failed 		= $Common_Utils::SUCCESS;
	my @deviceChange= ();
	Common_Utils::WriteMessage("Configuring disks of machine \"$machineName\".",2);
	
	my $clusterNodes	= "";
	my $inmageHostId	= $args{sourceHost}->getAttribute('inmage_hostid');
	if($args{isCluster})
	{
		$clusterNodes	= $args{sourceHost}->getAttribute('clusternodes_inmageguids');
	}
	
	my @disksInfo	= ();
	eval
	{
		@disksInfo 	= $args{sourceHost}->getElementsByTagName('disk');
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("ERROR :: $@.",3);
		Common_Utils::WriteMessage("ERROR :: Failed to configure disks of machine \"$machineName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my( $returnCode , $ideCount , $rdm , $cluster , $existingDisksInfo , $floppyDevices ) = VmOpts::FindDiskDetails( $args{vmView}, [] );
    if( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get disks information of machine \"$machineName\".",3);
		return ( $Common_Utils::FAILURE );
	}

	my %scsiControllerDefined 	= ();
	foreach my $disk ( @disksInfo )
	{
		if( defined ($disk->getAttribute('selected') ) && ( "no" eq $disk->getAttribute('selected') ) )
		{
			next;
		}
		
		my $diskUuid	= $disk->getAttribute('source_disk_uuid');
		if ( "physicalmachine" eq lc( $args{sourceHost}->getAttribute('machinetype') ) )
		{
			$diskUuid	= $disk->getAttribute('disk_signature');
		}
		
		my $diskExist			= $Common_Utils::NOTFOUND; 
		my $existingDiskInfo 	= ();
		my $diskName			= $disk->getAttribute('disk_name');
		my @diskPath			= split( /\[|\]/ , $diskName );
		$diskPath[-1]			=~ s/^\s+//;
		if ( defined( $args{sourceHost}->getAttribute('vmDirectoryPath') ) && ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
		{ 
			$diskPath[-1]	= $args{sourceHost}->getAttribute('vmDirectoryPath')."/$diskPath[-1]";
		}
		$diskName				= "[".$disk->getAttribute('datastore_selected')."] $diskPath[-1]";
		
		my @tempArr	= @$existingDisksInfo;
		if ( -1 != $#tempArr )
		{
			foreach my $existingDisk ( @$existingDisksInfo )
			{
				my $clusterDisk	= $$createdDisks{$diskUuid};
				if ( ( $diskName ne $existingDisk->{diskName} ) && ( ( ! defined $$createdDisks{$diskUuid} ) && ( $clusterNodes !~ /$$clusterDisk{inmageHostId}/i ) ) )
				{
					next;
				}
				$existingDiskInfo= $existingDisk;
				$diskExist		= $Common_Utils::EXIST; 
				last;
			}
		}
			
		if( ( "Mapped Raw LUN" eq $existingDiskInfo->{diskMode} ) && ( ! defined $$createdDisks{$diskUuid} ) )
		{
			Common_Utils::WriteMessage("Editing the RDM vmdk $diskName.",2);
			my $sourceScsiId	= $disk->getAttribute( 'scsi_mapping_vmx' );
			my @splitController = split ( /:/ , $sourceScsiId );
			my $controllerKey	= 1000 + $splitController[0];
			my $unitNumber		= $splitController[1];
			my $diskKey			= 2000 + ( ( $controllerKey % 1000 ) * 16 ) + $unitNumber;
			my $operationA		= "create";
			my $operationR		= "destroy";
			
			my $clusterDisk	= $$createdDisks{$diskUuid};
			if( ( defined $$createdDisks{$diskUuid} ) && ( $clusterNodes =~ /$$clusterDisk{inmageHostId}/i ) )
			{
				$operationA		= "add";
				$operationR		= "remove";
			}
									
			my $rootFolderPath 	= "";
			if ( ( defined( $args{sourceHost}->getAttribute('vmDirectoryPath') ) )&& ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
			{
				$rootFolderPath		= $args{sourceHost}->getAttribute('vmDirectoryPath');
				Common_Utils::WriteMessage("RootFolderPath = $rootFolderPath.",2);
			}
			
			my $returnCode	= VmOpts::RemoveVmdk(
													fileName 		=> $diskName,
													fileSize		=> $disk->getAttribute('size'),
													diskMode		=> $disk->getAttribute('independent_persistent'),
													controllerKey	=> $controllerKey,
													key				=> $diskKey,
													operation		=> $operationR,
													vmView			=> $args{vmView},
													displayName		=> $machineName,
												);
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				$failed		= $Common_Utils::FAILURE;
			}
			
			( $returnCode , my $virtualDeviceSpec )= VmOpts::VirtualDiskSpec( diskConfig => $disk , controllerKey => $controllerKey , unitNumber => $unitNumber, 
																		diskKey => $diskKey , rootFolderName => $rootFolderPath , operation => $operationA );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				$failed 	= $Common_Utils::FAILURE;
				last;
			}
			push @deviceChange , $virtualDeviceSpec;
			next;			
		}
		
		if(  "yes" eq $disk->getAttribute('cluster_disk') )
		{
			$$createdDisks{$diskUuid}	= { diskName => $diskName, inmageHostId => $inmageHostId };
		}
			
		if ( $diskExist	== $Common_Utils::EXIST )
		{
			Common_Utils::WriteMessage("Already exisitng Diskname = $diskName.",2);
			if ( ( $disk->getAttribute('size') <= $existingDiskInfo->{diskSize} ) && ( $disk->getAttribute('scsi_mapping_vmx') eq $existingDiskInfo->{scsiVmx} ) )
			{
				next;
			}
		}
		else
		{
			Common_Utils::WriteMessage("ERROR :: $diskName not found.",3);
			return ( $Common_Utils::FAILURE );
		}		
	}		
	
	if ( $failed )
	{
		Common_Utils::WriteMessage("ERROR :: Cloned vmdks modification has failed.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	( $returnCode, $args{vmView} )= VmOpts::UpdateVmView( $args{vmView} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$machineName\".",2);
	}
	
	$returnCode		= VmOpts::ReconfigVm( vmView => $args{vmView} , changeConfig => {deviceChange => \@deviceChange });
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS );
}

#####CreateArrayDatastore#####
##Description 		:: It reads snapshot.xml file for source machine information and for Master target information. 
##						This calls all sub-sequent calls to prepare a datastore of array vsnap lun.
##Input 			:: Complete file name for snapshot.xml file.
##Output 			:: Returns SUCCESS on successfully creating the datastore else FAILURE.
#####CreateArrayDatastore#####
sub CreateArrayDatastore
{
	my %args							= @_;
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
	
	foreach my $plan ( @$planNodes )
	{
		my $planName					= $plan->getAttribute('plan');
		my @targetNodes					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetHost					= $targetNodes[0]->getElementsByTagName('host');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		Common_Utils::WriteMessage("Datastore creation for Array based DrDrill plan name = $planName.",2);
		
		my $datastoresInfo	= XmlFunctions::ReadDatastoreInfoFromXML( $targetNodes[0] );
		
		my $targetvSphereHostName		= $targetHost[0]->getAttribute('vSpherehostname');
		Common_Utils::WriteMessage("Target chosen = $targetvSphereHostName.",2);
		( $returnCode ,my $tgthostView )= HostOpts::GetSubHostView( $targetvSphereHostName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed						= $Common_Utils::FAILURE;
			last;
		}
		
		Common_Utils::WriteMessage("Searching for datastores virtual copies.",2);
		( $returnCode, my $unResVmfsVols)	= DatastoreOpts::QueryUnresolvedVmfsVolumes( $tgthostView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		my %requiredDs	= ();
		my @resignPaths	= ();		
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
				
				foreach my $datastoreInfo ( @$datastoresInfo )
				{
					if( ( $$datastoreInfo{vSphereHostName} eq $targetvSphereHostName ) && ( $$datastoreInfo{datastoreName} eq $datastoreSelected ) )
					{
						Common_Utils::WriteMessage("existing datastore = $datastoreSelected; uuid = $$datastoreInfo{uuid}.",2);
						foreach my $unResVmfsVol ( @$unResVmfsVols )
						{
							if( $$datastoreInfo{uuid} ne $unResVmfsVol->vmfsUuid)
							{
								next;
							}
							my $unResVmfsExtents	= $unResVmfsVol->extent;
							foreach my $unResVmfsExtent ( @$unResVmfsExtents )
							{
								if( ( $unResVmfsExtent->reason eq "uuidConflict" ) && ( $unResVmfsExtent->isHeadExtent ) )
								{
									my $devicePath	= $unResVmfsExtent->devicePath;
									$requiredDs{$datastoreSelected}	= $devicePath;
									push @resignPaths, $devicePath;
									Common_Utils::WriteMessage("Resignaturing the datastore path $devicePath.",2);
									last;
								}
							}
						}
						last;
					}
				}
			}
		}
		
		( $returnCode, my $mapDeviceNewDsName)	= DatastoreOpts::ResignatureUnresolvedVmfsVolumes( $tgthostView, \@resignPaths );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		foreach my $sourceHost	( @sourceHosts )
		{
			my @hostsInfo				= $sourceHost->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				my @disksInfo			= $host->getElementsByTagName('disk');
				foreach my $disk ( @disksInfo )
				{
					my $datastoreSelected 	= $disk->getAttribute('datastore_selected');
					my $newDatastore		= $$mapDeviceNewDsName{ $requiredDs{$datastoreSelected} };
					$disk->setAttribute( 'datastore_selected', $newDatastore );
					Common_Utils::WriteMessage("Set the datastore name from $datastoreSelected to $newDatastore.",2);
				}
			}
		}
		Common_Utils::WriteMessage("Rescaning for new datastores on $targetvSphereHostName.",2);
		$returnCode	= DatastoreOpts::RescanVmfs( $tgthostView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed						= $Common_Utils::FAILURE;
			last;
		}
	}
	
	$returnCode					= XmlFunctions::SaveXML( $planNodes , $args{file} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to modify ESX.xml file.",3);
		$failed 	 			= $Common_Utils::FAILURE;
	}
		
	return ( $failed );
}

#####ProtectionResize#####
##Description		:: Performs the Incremental protection.
##Input 			:: Complete file name for ESX.xml file.
##Output			:: Returns SUCCESS else FAILURE.
#####ProtectionResize#####
sub ProtectionResize
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
	
	foreach my $plan ( @$planNodes )
	{
		my $planName					= $plan->getAttribute('plan');
		my @targetNodes					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetHost					= $targetNodes[0]->getElementsByTagName('host');	
		my $IsItvCenter					= $targetHost[0]->getAttribute('vCenterProtection');
		$Common_Utils::vContinuumPlanDir= $plan->getAttribute('xmlDirectoryName');
		Common_Utils::WriteMessage("Protection resize plan name = $planName.",2);
		Common_Utils::WriteMessage("IsItvCenter = $IsItvCenter.",2);	
		
		( my $returnCode , $mtView )	= VmOpts::GetVmViewByUuid( $targetHost[0]->getAttribute('source_uuid') );
		if ( $Common_Utils::SUCCESS ne $returnCode )
		{
			$failed 				= $Common_Utils::FAILURE;
			last;
		}
		
		my $mtName	= $mtView->name;			
		my $targetvSphereHostName		= $targetHost[0]->getAttribute('vSpherehostname');
		Common_Utils::WriteMessage("Target chosen = $targetvSphereHostName.",2);
		
		my $mtDevices	= Common_Utils::GetViewProperty( $mtView, 'config.hardware.device');
		
		my @deviceChange	= ();
		my %deviceUuids		= ();
		my %mapDsReqSpace	= ();
		my %clusterDevices	= ();
		my %sataDevices		= ();
		my @mtAttachSpecs	= ();
		
		my $rdmFound	= 0;
		my $isDisksThick= 0;
			
		my @sourceHosts	= $plan->getElementsByTagName('SRC_ESX');
		foreach my $sourceHost	( @sourceHosts )
		{
			my @hostsInfo				= $sourceHost->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				my $displayName	= $host->getAttribute('display_name');
				my @disksInfo 	= $sourceHost->getElementsByTagName('disk');									
				
				foreach my $disk ( @disksInfo )
				{
					my $diskScsiID	= $disk->getAttribute('scsi_id_onmastertarget');
					my $diskUuid	= $disk->getAttribute('target_disk_uuid');
					my $diskName	= $disk->getAttribute('disk_name');
					my $found		= 0;
					
					foreach my $device ( @$mtDevices )
					{
						if( $device->deviceInfo->label !~ /Hard Disk/i )
						{
							next;
						}
						my $diskSizeXml	= $disk->getAttribute('size');
						my $diskSizeVm	= $device->capacityInKB;	
						my $vmdkName 	= $device->backing->fileName;
						my @diskPath 	= split(/\[|\]/,$vmdkName);
						my $dsName		= $diskPath[1];						
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
						
						if ( ( defined $diskUuid ) && ( $diskUuid ne "" ) && ( $vmdkUuid eq $diskUuid ) )
						{
							$found	= 1;
							if( $diskSizeVm < $diskSizeXml)
							{
								my $diskSpec	= "";
								Common_Utils::WriteMessage("Resizing disk $vmdkName of machine $displayName of old size $diskSizeVm to new size $diskSizeXml based on Uuid.",2);
								if( exists $device->{backing}->{compatibilityMode} )
								{
									$rdmFound	= 1;
									if( 'virtualmode'eq lc( $device->backing->compatibilityMode ) )
									{
										$deviceUuids{$device->backing->lunUuid}	= $diskSizeXml;
									}
									$diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('edit'), device => $device );
								}
								else
								{
									$device->{capacityInKB}	= $diskSizeXml;
									$isDisksThick			= 1 unless( $device->backing->thinProvisioned );
									$mapDsReqSpace{$dsName}	+= ( $diskSizeXml - $diskSizeVm ) / ( 1024 * 1024 );
									$diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('edit'), device => $device );
									if( ( defined $device->backing->eagerlyScrub ) && ( $device->backing->eagerlyScrub ) )
									{
										$diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
										push @mtAttachSpecs, VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('add'), device => $device );
										$clusterDevices{$vmdkUuid}	= $diskSizeXml;
									}
									elsif( ( ($device->controllerKey >= 15000) && ($device->controllerKey <= 15004) ) || ( $diskSizeXml > 2040109465 ) ) #1.9 TB
									{
										$diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
										push @mtAttachSpecs, VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('add'), device => $device );
										$sataDevices{$vmdkUuid}	= $diskSizeXml;
									}
								}
								push @deviceChange , $diskSpec;
								$deviceUuids{$vmdkUuid}	= $diskSizeXml;
							}
							elsif( $diskSizeVm > $diskSizeXml)
							{
								Common_Utils::WriteMessage("ERROR :: Resize found on disk $vmdkName of machine $displayName of old size $diskSizeVm to new size $diskSizeXml based on Uuid.",3);
								Common_Utils::WriteMessage("ERROR :: Vmdk shrink is not supported.",3);
								return( $Common_Utils::FAILURE );
							}
							last;
						}
						
						if ( ( ( !defined $diskUuid ) || ( $diskUuid eq "" ) ) && ( defined $diskScsiID ) && ( "$scsiNumber:$scsiUnitNum" eq $diskScsiID ) )
						{
							$found	= 1;
							if( $diskSizeVm < $diskSizeXml)
							{
								my $diskSpec	= "";
								Common_Utils::WriteMessage("Resizing disk $vmdkName of machine $displayName of old size $diskSizeVm to new size $diskSizeXml based on scsi id.",2);
								if( exists $device->{backing}->{compatibilityMode} )
								{
									$rdmFound	= 1;
									if( 'virtualmode'eq lc( $device->backing->compatibilityMode ) )
									{
										$deviceUuids{$device->backing->lunUuid}	= $diskSizeXml;
									}
									$diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('edit'), device => $device );
								}
								else
								{
									$device->{capacityInKB}	= $diskSizeXml;
									$isDisksThick			= 1 unless( $device->backing->thinProvisioned );
									$mapDsReqSpace{$dsName}	+= ( $diskSizeXml - $diskSizeVm )/ ( 1024 * 1024 );
									$diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('edit'), device => $device );
									if( ( defined $device->backing->eagerlyScrub ) && ( $device->backing->eagerlyScrub ) )
									{
										$diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
										push @mtAttachSpecs, VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('add'), device => $device );
										$clusterDevices{$vmdkUuid}	= $diskSizeXml;
									}
									elsif( ( ($device->controllerKey >= 15000) && ($device->controllerKey <= 15004) ) || ( $diskSizeXml > 2040109465 ) ) #1.9 TB
									{
										$diskSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
										push @mtAttachSpecs, VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('add'), device => $device );
										$sataDevices{$vmdkUuid}	= $diskSizeXml;
									}
								}								
								push @deviceChange , $diskSpec;
								$deviceUuids{$vmdkUuid}	= $diskSizeXml;
							}
							elsif( $diskSizeVm > $diskSizeXml)
							{
								Common_Utils::WriteMessage("ERROR :: Resize found on disk $vmdkName of machine $displayName of old size $diskSizeVm to new size $diskSizeXml based on scsi id.",3);
								Common_Utils::WriteMessage("ERROR :: Vmdk shrink is not supported.",3);
								return( $Common_Utils::FAILURE );
							}
							last;
						}
					}
					if( ! $found )
					{
						Common_Utils::WriteMessage("ERROR :: Vmdk $diskName of machine $displayName not found on Master Target \"$mtName\" .",3);
						Common_Utils::WriteMessage("ERROR :: Please perform addition of disk to protect it.",3);
						return( $Common_Utils::FAILURE );
					}		
				}				
			}
		}
		
		if ( -1 == $#deviceChange )
		{
			Common_Utils::WriteMessage("ERROR :: No disk found for resize, considering volume resize and proceeding.",2);
			
			my %clusterDisks	= ();		
			foreach my $sourceHost	( @sourceHosts )
			{
				my @hostsInfo				= $sourceHost->getElementsByTagName('host');
				foreach my $host ( @hostsInfo )
				{
					my $displayName	= $host->getAttribute('display_name');
					my $vmUuid		= $host->getAttribute('target_uuid');
									
					( $returnCode,my $vmView )= VmOpts::GetVmViewByUuid( $vmUuid );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						$failed				= $Common_Utils::FAILURE;
						last;
					}
					
					$returnCode	= VmOpts::UpdateVmdks( vmView => $vmView, uuidMap => \%deviceUuids, sataUuid => \%clusterDevices, clusterDisks => \%clusterDisks );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						Common_Utils::WriteMessage("ERROR::Vmdks updation failed for \"$displayName\".",3);
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
				}
			}
			
			last;
		}
		
		($returnCode, my $targetDatacenterName)	= DatacenterOpts::GetVmDataCenterName( $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode , my $datacenterView )	= DatacenterOpts::GetDataCenterView( $targetDatacenterName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
				
		if( $isDisksThick || $rdmFound )
		{
			( $returnCode , my $targetHostView) = HostOpts::GetSubHostView( $targetvSphereHostName );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			if( $isDisksThick )
			{			
				my @hostViews		= $targetHostView;
				my @datacenterViews	= $datacenterView;
				( $returnCode , my $listDatastoreInfo )	= DatastoreOpts::GetDatastoreInfo( \@hostViews , \@datacenterViews );
				if ( $returnCode ne $Common_Utils::SUCCESS ) 
				{
					return ( $Common_Utils::FAILURE );
				}
				
				foreach my $dsInfo ( @$listDatastoreInfo )
				{
					if( ( exists $mapDsReqSpace{ $$dsInfo{symbolicName} } ) && ( $$dsInfo{freeSpace} < $mapDsReqSpace{ $$dsInfo{symbolicName} } ) )
					{
						my $datastoreName	= $$dsInfo{symbolicName};
						my $requiredSpace	= $mapDsReqSpace{ $$dsInfo{symbolicName} };
						my $freeSpace		= $$dsInfo{freeSpace};
						Common_Utils::WriteMessage("ERROR :: Space required on datastore $datastoreName = $requiredSpace GB.",3);
						Common_Utils::WriteMessage("ERROR :: Free space available on datastore $datastoreName = $freeSpace GB",3);
						return( $Common_Utils::FAILURE );
					}
				}
			}
			
			if( $rdmFound )
			{	
				my $rescanReq	= 0; 
				$targetHostView	= Common_Utils::UpdateViewProperties( view => $targetHostView , properties => [ "config.storageDevice.scsiLun" ] );
				my $scsiLun		= Common_Utils::GetViewProperty( $targetHostView, 'config.storageDevice.scsiLun' );
				foreach my $lun (@$scsiLun)
				{
					if( exists $deviceUuids{$lun->uuid} )
					{
						my $lunSize	= ($lun->capacity->block) * ($lun->capacity->blockSize) / 1024;
						if( $lunSize < $deviceUuids{$lun->uuid} )
						{
							$rescanReq	= 1;
						}						
					}
				}
				
				if($rescanReq)
				{
					Common_Utils::WriteMessage("Rescaning all the HBA ports of $targetvSphereHostName.",2);
					$returnCode	= HostOpts::RescanAllHba( $targetHostView );
					if ( $returnCode != $Common_Utils::SUCCESS )
					{
						#skip and checking for new size
					}
					
					$targetHostView	= Common_Utils::UpdateViewProperties( view => $targetHostView , properties => [ "config.storageDevice.scsiLun" ] );
					$scsiLun	= Common_Utils::GetViewProperty( $targetHostView, 'config.storageDevice.scsiLun' );
					foreach my $lun (@$scsiLun)
					{
						if( exists $deviceUuids{$lun->uuid} )
						{
							my $lunSize	= ($lun->capacity->block) * ($lun->capacity->blockSize) / 1024;
							if( $lunSize < $deviceUuids{$lun->uuid} )
							{
								my $requiredSpace	= $deviceUuids{$lun->uuid};
								Common_Utils::WriteMessage("ERROR :: Lun \"$lun->{canonicalName}\" has to be extended, old size $lunSize,required size $requiredSpace in KB.",3);
								Common_Utils::WriteMessage("ERROR :: Rescan storage devices on target ESX and Rerun.",3);
								return( $Common_Utils::FAILURE );							
							}						
						}
					}					
				}				
			}
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
	
		Common_Utils::WriteMessage("Reconfiguring the VM \"$mtName\".",2);
		$returnCode	= VmOpts::ReconfigVm( vmView => $mtView , changeConfig => {deviceChange => \@deviceChange });
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Reconfiguration failed for the VM \"$mtName\" while extending the vmdks.",3);
			$failed	= $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode, $mtView )= VmOpts::UpdateVmView( $mtView );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$mtName\".",2);
		}
		
		my %clusterDisks	= ();		
		foreach my $sourceHost	( @sourceHosts )
		{
			my @hostsInfo				= $sourceHost->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				my $displayName	= $host->getAttribute('display_name');
				my $vmUuid		= $host->getAttribute('target_uuid');
								
				( $returnCode,my $vmView )= VmOpts::GetVmViewByUuid( $vmUuid );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					$failed				= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode	= VmOpts::UpdateVmdks( vmView => $vmView, uuidMap => \%deviceUuids, sataUuid => \%clusterDevices, clusterDisks => \%clusterDisks );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					Common_Utils::WriteMessage("ERROR::Vmdks updation failed for \"$displayName\".",3);
					$failed 			= $Common_Utils::FAILURE;
					last;
				}
				
				$returnCode	= VmOpts::ResizeClusterDisks( vmView => $vmView, uuidMap => \%clusterDevices, datacenter => $datacenterView, clusterDisks => \%clusterDisks );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					Common_Utils::WriteMessage("ERROR::Eager zeroing vmdks of the machine \"$displayName\" failed.",3);
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
			}
		}
		if( $#mtAttachSpecs != -1)
		{
			Common_Utils::WriteMessage("Attaching the vmdks back to the VM \"$mtName\".",2);
			$returnCode	= VmOpts::ReconfigVm( vmView => $mtView , changeConfig => {deviceChange => \@mtAttachSpecs });
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: Reconfiguration failed for the VM \"$mtName\" while extending the vmdks.",3);
				$failed	= $Common_Utils::FAILURE;
				last;
			}
			
			( $returnCode, $mtView )= VmOpts::UpdateVmView( $mtView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$mtName\".",2);
			}
		}
		
		if ( "poweredOn" ne Common_Utils::GetViewProperty( $mtView, 'summary.runtime.powerState')->val )
		{
			$returnCode 			= VmOpts::PowerOn( $mtView );
			if ( $returnCode == $Common_Utils::SUCCESS )
			{
				my @poweredOnMachineInfo= ( Common_Utils::GetViewProperty( $mtView, 'summary.config.uuid') );
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
	}
		
	$returnCode					= XmlFunctions::SaveXML( $planNodes , $args{file} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to modify ESX.xml file.",3);
		$failed 	 			= $Common_Utils::FAILURE;
	} 
	
	$returnCode 			= XmlFunctions::WriteInmageScsiUnitsFile( file => $args{file}, operation => "resize" );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to Write InMage SCSI units file.",3);
		$failed 			= $Common_Utils::FAILURE;
	}	
	
	return ( $failed );
}

#####EagerZeroingVmdk#####
##Description 		:: eager zeroing the vmdk.
##Input 			:: datacenter name and vmdk name with full datastore path.
##Output 			:: Returns SUCCESS else FAILURE.
#####EagerZeroingVmdk#####
sub EagerZeroingVmdk
{
	my %args		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	my $serviceContent		= Vim::get_service_content();	
	my $virtualDiskManager 	= Vim::get_view( mo_ref => $serviceContent->virtualDiskManager);
	
	Common_Utils::WriteMessage("Eager zeroing the vmdk $args{vmdkname}.",2);
	
	my( $returnCode, $datacenterView)	= DatacenterOpts::GetDataCenterView( $args{datacenter} );
   	if ( $returnCode != $Common_Utils::SUCCESS )
   	{
   		return($Common_Utils::FAILURE);
   	}
	
	eval
	{
		my $task	= $virtualDiskManager->EagerZeroVirtualDisk_Task( 	name => $args{vmdkname},
																	 datacenter => $datacenterView,																	
																);
												
		my $task_view 	= Vim::get_view(mo_ref => $task);
		while ( 1 ) 
	    {    	
	        my $info 	= $task_view->info;
	        if( $info->state->val eq 'running' )
	        {
	        } 
	        elsif( $info->state->val eq 'success' ) 
	        {
	           Common_Utils::WriteMessage("$args{vmdkname} eager zeroing is completed.",2);
	           last;
	        } 
	        elsif( $info->state->val eq 'error' ) 
	        {
	        	my $soap_fault 	= SoapFault->new;
	            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
	            Common_Utils::WriteMessage("ERROR :: Task Failed due to error :\"$errorMesg\".",3);
	            $ReturnCode		= $Common_Utils::FAILURE;
	            last;
	        }
	        sleep 10;
	        $task_view->ViewBase::update_view_data();
	     }
	};
	if ($@)
	{
		Common_Utils::WriteMessage("$@.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
	}
	
	return $ReturnCode;
}

#####Help#####
##Description 		:: Usage of Protect.pl.
##Input				:: None.
##Output			:: Display's Usage of Protect.pl file.
#####Help#####
sub Help
{
	Common_Utils::WriteMessage("***** Usage of Protect.pl *****",3);
	Common_Utils::WriteMessage("-------------------------------",3);
	Common_Utils::WriteMessage("For Protection --> Protect.pl --server \"<TARGET_IP>\" --username \"<USERNAME>\"  --password \"<PASSWORD>\".",3);
}

BEGIN:

my $ReturnCode				= 1;
my ( $sec, $min, $hour )	= localtime(time);
my $logFileName 		 	= "vContinuum_Temp_$sec$min$hour.log";
my $file					= "";

Common_Utils::CreateLogFiles( "$Common_Utils::vContinuumLogsPath/$logFileName" , "$Common_Utils::vContinuumLogsPath/vContinuum_ESX.log" );

Opts::add_options(%customopts);
Opts::parse();

my $serverIp	= Opts::get_option('server');
unless ( Opts::option_is_set('extend_disk') )
{
	(my $returnCode, $serverIp, my $userName, my $password )	= HostOpts::getEsxCreds( Opts::get_option('server') );
	Opts::set_option('username', $userName);
	Opts::set_option('password', $password );
}
Opts::validate();

if ( ! ( Opts::option_is_set('username') && Opts::option_is_set('password') && Opts::option_is_set('server') ) )
{
	Common_Utils::WriteMessage("ERROR :: vCenter/vSphere IP,Username and Password values are the basic parameters required.",3);
	Common_Utils::WriteMessage("ERROR :: One or all of the above parameters are not set/passed to script.",3);
}
else
{
	$ReturnCode		= Common_Utils::Connect( Server_IP => $serverIp, UserName => Opts::get_option('username'), Password => Opts::get_option('password') );
	
	if ( $Common_Utils::SUCCESS == $ReturnCode )
	{
		eval
		{
			$file	= "$Common_Utils::vContinuumMetaFilesPath/ESX.xml";
			if ( Opts::option_is_set('update') )
			{
				$ReturnCode	= AppendTargetXmlToCX( $serverIp );
			}
			elsif ( Opts::option_is_set('power_on') )
			{
				$ReturnCode	= PowerOnMachines( Opts::get_option('power_on') );
			}
			elsif ( Opts::option_is_set('power_off') )
			{
				$ReturnCode	= PowerOffMachines( Opts::get_option('power_off') );
			}
			elsif ( "yes" eq lc ( Opts::get_option('incremental_disk') ) )
			{
				$ReturnCode	= IncrementalProtection(
														file 	=> $file,
													);
			}
			elsif ( "yes" eq lc ( Opts::get_option('dr_drill') ) )
			{
				$file		= "$Common_Utils::vContinuumMetaFilesPath/Snapshot.xml";
				$ReturnCode	= Protect(
										file 	=> $file,
									);
			}
			elsif ( "yes" eq lc ( Opts::get_option('dr_drill_array') ) )
			{
				$file		= "$Common_Utils::vContinuumMetaFilesPath/Snapshot.xml";
				$ReturnCode	= ArrayBasedDrDrill(
													file 	=> $file,
												);
			}
			elsif ( "yes" eq lc ( Opts::get_option('array_datastore') ) )
			{
				$file		= "$Common_Utils::vContinuumMetaFilesPath/Snapshot.xml";
				$ReturnCode	= CreateArrayDatastore(
														file 	=> $file,
													);
			}
			elsif ( "yes" eq lc ( Opts::get_option('resize') ) )
			{
				$file 		= "$Common_Utils::vContinuumMetaFilesPath/$Common_Utils::ResizeXmlFileName";
				$ReturnCode	= ProtectionResize(
													file 	=> $file,
												);
			}
			elsif ( "yes" eq lc ( Opts::get_option('extend_disk') ) )
			{
				if ( ! ( Opts::option_is_set('datacenter') && Opts::option_is_set('vmdkname') ) )
				{
					Common_Utils::WriteMessage("ERROR :: Datacenter and vmdk name values are required.",3);
					Common_Utils::WriteMessage("ERROR :: One or all of the above parameters are not set/passed to script.",3);
				}
				$ReturnCode	= EagerZeroingVmdk(
												datacenter 	=> Opts::get_option('datacenter'),
												vmdkname 	=> Opts::get_option('vmdkname'),
											);
				Common_Utils::WriteMessage("Successfully Eager zeroed the vmdk.",3);
			}
			else
			{
				$ReturnCode	= Protect(
										file 	=> $file,
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

__END__
