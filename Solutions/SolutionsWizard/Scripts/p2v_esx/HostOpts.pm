=head 
	PackageName : Host_Opts.pm
		1. Connect/Disconnect to ESX standlaone/vCenter.
		2. Host ESX version.
		3. Register/Un-register Machine.
		
=cut

package HostOpts;

use strict;
use warnings;
use Common_Utils;

my $return_code 	= "";

sub EstablishConnection
{
	my ( $server, $username, $password ) = @_;
	
	Common_Utils::WriteMessage("Connecting to vCenter/vSphere Server : $server.",2);
	
	my $url 		= "https://$server/sdk/webService";
	
	$ENV{HTTPS_CA_FILE} = undef if defined( $ENV{HTTPS_CA_FILE} );
	
	for(my $i=1; $i<=4 ; $i++ )
	{
		eval
		{
			$return_code 	= Vim::login(service_url => $url, user_name => $username, password => $password );
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("$@.",3);
			sleep(15);
			Common_Utils::WriteMessage("Retrying for connection.",2);
			next if $i<3;
			
			#to seperentiate invalid creds and invalid ip( not reachable, not a vcenter ip)
			if( $@ =~ /500 Connect failed: connect: Unknown error;/i )
			{
				return $Common_Utils::INVALIDIP;
			}
			else
			{
				return $Common_Utils::INVALIDCREDENTIALS;
			}			
		}
		last;
	}	
	Common_Utils::WriteMessage("Successfully Connected to vCenter/vSphere Server : $server .",2);
	
	my $serviceContent	= Vim::get_service_content();
	my $sessionManager	= Vim::get_view( mo_ref => $serviceContent->sessionManager );
	$sessionManager->SetLocale( locale => "en_US" );
		
	return $Common_Utils::SUCCESS;
}

sub AbolishConnection
{
	Util::disconnect(); 
}

#####GetHostViews#####
##Description 			:: Gets Host Views of ESX/vCenter.
##Input 				:: None.
##Output 				:: Returns host view on SUCCESS else FAILURE.
#####GetHostViews#####
sub GetHostViews
{
	my $hostViews 	= Vim::find_entity_views( view_type => "HostSystem" , filter => { 'runtime.powerState' => 'poweredOn' } );
	
	unless ( @$hostViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get vSphere host views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $hostViews );
	}
	
	my @newHostViews= (); 
	foreach my $hostView ( @$hostViews )
	{
		my $hostName= $hostView->name;
		if ( $hostView->runtime->inMaintenanceMode )
		{
			Common_Utils::WriteMessage("\"$hostName\" is in Maintenance Mode.",3);
			next;
		}
		push @newHostViews , $hostView;
	}
	
	if ( -1 == $#newHostViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get vSphere host views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , \@newHostViews );
	}
	
	return ( $Common_Utils::SUCCESS , \@newHostViews );
}

#####GetHostViewsByProps#####
##Description 			:: Gets Host Views of ESX/vCenter.
##Input 				:: None.
##Output 				:: Returns host view on SUCCESS else FAILURE.
#####GetHostViewsByProps#####
sub GetHostViewsByProps
{
	my @newHostViews= (); 
	eval
	{
		my $hostViews 	= Vim::find_entity_views( view_type => "HostSystem" , filter => { 'runtime.powerState' => 'poweredOn' } , properties => GetHostProps() );
		
		unless ( @$hostViews )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to get vSphere host views from vCenter/vSphere.",3);
			return ( $Common_Utils::FAILURE , $hostViews );
		}
		
		foreach my $hostView ( @$hostViews )
		{
			my $hostName= $hostView->name;
			if ( $hostView->{'summary.runtime.inMaintenanceMode'} !~ /^false$/i )
			{
				Common_Utils::WriteMessage("\"$hostName\" is in Maintenance Mode.",3);
				next;
			}
			push @newHostViews , $hostView;
		}
	};	
	if( $@ )
	{
		Common_Utils::WriteMessage("Failed to get host system views by poperties from vCenter/vSphere : \n $@.",3);
		return ( $Common_Utils::FAILURE , \@newHostViews );
	}
	
	if ( -1 == $#newHostViews )
	{
		Common_Utils::WriteMessage("ERROR :: No vSphere host views found on vCenter/vSphere.",3);
		return ( $Common_Utils::NOVMSFOUND , \@newHostViews );
	}
	
	return ( $Common_Utils::SUCCESS , \@newHostViews );
}

#####GetVmProps#####
##Description 	:: To get the Virtual Machine properties to be collected.
##Input 		:: None.
##Output 		:: Virtual Machine properties.
#####GetVmProps#####
sub GetHostProps
{
	my @hostProps = ( "name", "datastoreBrowser", "parent", "configManager.datastoreSystem", "configManager.storageSystem",
						"summary.config.product.version", "summary.runtime.inMaintenanceMode", "summary.hardware.numCpuCores", "summary.hardware.memorySize");
#"config.fileSystemVolume", "config.network.portgroup", "config.storageDevice.scsiLun", "config.storageDevice.scsiTopology", 
	return \@hostProps;
}

#####GetSubHostView#####
##Description 			:: Gets Host View of ESX. This is called when we are having multiple host view in case of vCenter.
##Input 				:: Host Name for which the View has to be Found.
##Output 				:: Returns the Host_view on SUCCESS	esle returns FAILURE.
#####GetSubHostView#####
sub GetSubHostView
{
	my $hostName 	= shift;
	my $hostView 	= Vim::find_entity_view( view_type => "HostSystem" ,filter => { 'name' => $hostName }, , properties => GetHostProps() );
	unless ( $hostView )
	{
		Common_Utils::WriteMessage("ERROR :: No Host System found with hostname \"$hostName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS , $hostView );
}

#####GetHostViewsInDatacenter#####
##Description 			:: It finds HostSystem view under a datacenter.
##Input 				:: Datacenter view.
##Output 				:: Returns SUCCESS and HostView else FAILURE.
#####GetHostViewsInDatacenter#####
sub GetHostViewsInDatacenter
{
	my $datacenterView	= shift;
	
	my $datacenterName	= $datacenterView->name;
	my $hostViews 		= Vim::find_entity_views( view_type =>'HostSystem' , begin_entity => $datacenterView, properties => GetHostProps() );
	if ( ! $hostViews )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find host view under datacenter \"$datacenterName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS , $hostViews );
}

#####GetHostVersion#####
##Description 			:: Finds ESX Host Version of a host.
##Input 				:: HostViews and HostName for which its version has to be determined.
##Output 				:: Returns ESX Host Version, SUCCESS else FAILURE.
#####GetHostVersion#####
sub GetHostVersion
{
	my $hostViews		= shift;
	my $hostName 		= shift;
	my $hostVersion 	= "";
	
	foreach my $hostView ( @$hostViews )
	{
		if ( $hostView->name ne $hostName  )
		{
			next
		}
		$hostVersion 	= Common_Utils::GetViewProperty( $hostView, 'summary.config.product.version');
		Common_Utils::WriteMessage("$hostName is having a vSphere of version = $hostVersion.",2);
		return ( $Common_Utils::SUCCESS , $hostVersion );
	}			
	return ( $Common_Utils::FAILURE );
}
	
#####GetNetworkInfo#####
##Description 			:: Finds Network Information on vCenter/vSphere.
##Input 				:: HostViews.
##Output 				:: Returns map of Network Information on SUCCESS else FAILURE.
#####GetNetworkInfo#####
sub GetNetworkInfo
{
	my $hostViews 		= shift;
	my ( $returnCode , $dvPortGroupInfo ) 	= GetDvPortGroups( $hostViews );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
	( $returnCode , my $portGroupInfo ) 	= GetHostPortGroups( $hostViews );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}

	( $returnCode , my $networkInfo ) 		= MergeNetworkInfo( $portGroupInfo, $dvPortGroupInfo );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	
	return ( $Common_Utils::SUCCESS , $networkInfo );
}

#####MergeNetworkInfo#####
##Description 			:: merge the dvport group info and host portgroup info into Network Information on vCenter/vSphere.
##Input 				:: dvport group info and host portgroup info.
##Output 				:: Returns map of Network Information on SUCCESS else FAILURE.
#####MergeNetworkInfo#####
sub MergeNetworkInfo
{
	my $portGroupInfo	= shift;
	my $dvPortGroupInfo	= shift;
	my @networkInfo		= ();
		
	my @portGroupInfo	= @$portGroupInfo;
	if( -1 != $#portGroupInfo )
	{
		push @networkInfo , \@portGroupInfo;
	}
	
	my @dvPortGroupInfo	= @$dvPortGroupInfo;
	if( -1 != $#dvPortGroupInfo )
	{
		push @networkInfo , \@dvPortGroupInfo;
	}
	
	return ( $Common_Utils::SUCCESS , \@networkInfo );
} 

#####GetDvPortGroups#####
##Description 			:: Gets Distributed Virtual PortGroupInfo on vSphere/ESX host.
##Input 				:: Host View(s) of vSphere/ESX.
##Output 				:: Returns list of DV portGroups mapping to Host.
#####GetDvPortGroups#####
sub GetDvPortGroups
{
	my $hostViews 		= shift;
	
	if( lc(ref($hostViews)) ne "array" )
	{
		my @tempHostViews	= ( $hostViews );
		$hostViews	= \@tempHostViews; 	
	}
	
	my @dvPortGroupInfo	= ();
	my $serviceContent 	= Vim::get_service_content();
	my $distVSwitchMng	= "";
	eval 
	{
    	$distVSwitchMng = Vim::get_view( mo_ref => $serviceContent->dvSwitchManager, properties => [] );
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("ERROR :: $@",2);
		return ( $Common_Utils::SUCCESS , \@dvPortGroupInfo );
	}
	
	foreach my $hostView ( @$hostViews )
	{
		my $hostName 	= $hostView->name;		
	    
		my $dvsConfigTarget = $distVSwitchMng->QueryDvsConfigTarget( host => $hostView );
		my $dvPortGroup 	= $dvsConfigTarget->{distributedVirtualPortgroup};
		if ( ! defined $dvPortGroup )
		{
			Common_Utils::WriteMessage("Distributed virtual port groups are not configured on host $hostName.",2);
			next; 
		}
		
		my @tempArr	= @$dvPortGroup;
		if( -1 == $#tempArr )
		{
			Common_Utils::WriteMessage("Distributed virtual port groups are not exist on host $hostName.",2);
			next; 
		}
		
		foreach my $dvPortGroupInfo( @$dvPortGroup )
		{
			my %dvPortGroupInfo = ();
			if ( 0 eq $dvPortGroupInfo->uplinkPortgroup ) 
			{
				eval
				{
					$dvPortGroupInfo{vSphereHostName}	= $hostName; 
					$dvPortGroupInfo{value} 			= $dvPortGroupInfo->{portgroup}->value;
					$dvPortGroupInfo{switchUuid}		= $dvPortGroupInfo->{switchUuid};
					$dvPortGroupInfo{portgroupKey} 		= $dvPortGroupInfo->{portgroupKey};
					$dvPortGroupInfo{portgroupName} 	= $dvPortGroupInfo->{portgroupName};
					$dvPortGroupInfo{portgroupNameDVPG} = $dvPortGroupInfo->{portgroupName}."[DVPG]";
					$dvPortGroupInfo{switchName} 		= $dvPortGroupInfo->{switchName};
					$dvPortGroupInfo{portgroupType}		= $dvPortGroupInfo->{portgroupType};
					Common_Utils::WriteMessage("vSphere Host Name = $dvPortGroupInfo{vSphereHostName} , dvPortGroupName = $dvPortGroupInfo{portgroupName}, dvPortGroupType = $dvPortGroupInfo{portgroupType}.",2);
					push @dvPortGroupInfo , \%dvPortGroupInfo;
				};
				if ( $@ )
				{
					Common_Utils::WriteMessage("ERROR :: $@, hostName : $hostName.",2);
					#skiiping this distributed virtual port group. #any of the machines using this port group are to be skipped.
					next;
				}
			}
		}
		Common_Utils::WriteMessage("Successfully discovered distributed virtual switch configuration on host $hostName.",2);
	}
	return ( $Common_Utils::SUCCESS , \@dvPortGroupInfo );
}

#####GetHostPortGroups#####
##Description 			:: List all the portGroups on ESX host.
##Input 				:: HostView, specific to vSphere.
##Output 				:: Returns the List of PortGroups mapping to Host.
#####GetHostPortGroups#####
sub GetHostPortGroups
{
	my $hostViews 		= shift;
	my @portGroupInfo   = ();
	
	foreach my $hostView ( @$hostViews )
	{
		my $hostName		= $hostView->name;		
		$hostView			= Common_Utils::UpdateViewProperties( view => $hostView , properties => [ "config.network.portgroup" ] );
		my $portGroups 		= Common_Utils::GetViewProperty( $hostView, 'config.network.portgroup');
		foreach my $portGroup ( @$portGroups )
		{
			my %portGroupInfo	= ();
			if( defined( $portGroup->port ) )
			{
				my $hostPortGroup 	= 0;
				foreach my $port( @{$portGroup->port} )
				{
					if ( "host" eq $port->type || "systemManagement" eq $port->type )
					{
						$hostPortGroup = 1;
						last;
					}
				} 
				if( $hostPortGroup )
				{
					next;
				}
			}
			$portGroupInfo{vSphereHostName}	= $hostName;
			$portGroupInfo{portgroupName} 	= $portGroup->spec->name;
			Common_Utils::WriteMessage("vSphere Host Name = $hostName , portGroupName = $portGroupInfo{portgroupName}.",2);
			push @portGroupInfo , \%portGroupInfo;
		}
	}
	
	return ( $Common_Utils::SUCCESS , \@portGroupInfo );
}

#####GetHostConfigInfo#####
##Description 			:: Finds configuration information of each of the host in the HostViews
##Input 				:: HostViews( either of vCenter/vSphere )
##Output 				:: Return the Configuartion info of each Host. At present we return ( CPU and Memmory )
#####GetHostConfigInfo#####
sub GetHostConfigInfo
{
	my $hostViews 		= shift;
	my @hostConfigInfo	= ();
	
	foreach my $hostView ( @$hostViews )
	{
		my %hostConfigInfo 					= ();
		$hostConfigInfo{vSphereHostName} 	= $hostView->name;
		$hostConfigInfo{cpu}				= Common_Utils::GetViewProperty( $hostView, 'summary.hardware.numCpuCores');
		$hostConfigInfo{memory} 			= sprintf( "%.2f" , ( ( Common_Utils::GetViewProperty( $hostView, 'summary.hardware.memorySize') ) / (1024 * 1024 ) ) ); 
		push @hostConfigInfo , \%hostConfigInfo;
	}
	
	return ( $Common_Utils::SUCCESS , \@hostConfigInfo );	
}

#####GetScsiDiskInfo#####
##Description 			:: List all the SCSI disk information which are available under a vCenter/vSphere Host.
##Input 				:: Host Views and ComputeResource Views.
##Output 				:: Returns SCSI device Info on SUCCESS else FAILURE.
#####GetScsiDiskInfo#####
sub GetScsiDiskInfo
{
	my $hostViews 				= shift;
	my $computeResourceViews 	= shift;
	my @scsiDiskInfo			= ();
	
	foreach my $hostView ( @$hostViews )
	{
		my $hostGroup	= $hostView->{mo_ref}->{value};
		
		foreach my $computeResourceView ( @$computeResourceViews )
		{
			my $hosts = $computeResourceView->host;
			foreach my $host ( @$hosts )
			{
				if ( $host->value ne $hostGroup )
				{
					next;
				}
				my $environmentBrowser 	= Vim::get_view( mo_ref =>$computeResourceView->environmentBrowser, properties => [] );
				my $configTarget 		= $environmentBrowser->QueryConfigTarget( host => $hostView );
				
				if( ! defined $configTarget->{scsiDisk} )
				{
					next;
				}
				my @lunInfo				= @{$configTarget->{scsiDisk}};
				
				foreach my $lun ( @lunInfo )
				{
					my %scsiDiskInfo				= ();
					$scsiDiskInfo{vSphereHostName}	= $hostView->name;;
					$scsiDiskInfo{displayName} 		= $lun->disk->displayName;
					$scsiDiskInfo{deviceName}		= $lun->name;
					$scsiDiskInfo{adapter}			= $lun->transportHint;
					$scsiDiskInfo{capacity}			= ( ($lun->capacity) / 1024);
					$scsiDiskInfo{lunNumber}		= $lun->lunNumber;
					push @scsiDiskInfo , \%scsiDiskInfo;
				}			
			}
		}
	}
	Common_Utils::WriteMessage("Successfully discovered rdm capable LUN's.",2);
	return ( $Common_Utils::SUCCESS , \@scsiDiskInfo );
}

#####FindHostOfVM#####
##Description		:: Finds host name of host on which machine is resided.
##Input 			:: Vm view for which host has to be determined.
##Output 			:: Returns SUCCESS and Host name of Host else FAILURE.
#####FindHostOfVM#####
sub FindHostOfVM
{
	my $vmView 		= shift;
	my $hostViews	= shift;
	my $machineName = $vmView->name;
	
	foreach my $host ( @$hostViews )
	{
		if ( Common_Utils::GetViewProperty( $vmView, 'summary.runtime.host')->value eq $host->{mo_ref}->{value} )
		{
			my $vSphereHostName 	= $host->name;
			Common_Utils::WriteMessage("Found $machineName on host $vSphereHostName.",2);
			return ( $Common_Utils::SUCCESS , $vSphereHostName );
		}
	}
	Common_Utils::WriteMessage("ERROR :: Failed to find vSphere host for machine $machineName.",2);
	return ( $Common_Utils::FAILURE );	
}

#####GetDvPortGroupsOfHost#####
##Description 			:: Finds dvPortGroup information of required host.
##Input 				:: vSphere host name and complete dvPortGroup info of vCenter/Esx.
##Output 				:: Return the dvPortGroup information of required host on Success.
#####GetDvPortGroupsOfHost#####
sub GetDvPortGroupsOfHost
{
	my $vSphereHostName	= shift;
	my $dvpgInfo		= shift;
	my @dvPortGroupInfo	= ();
	foreach my $dvpg ( @$dvpgInfo )
	{
		if( $dvpg->{vSphereHostName} eq $vSphereHostName )
		{
			push @dvPortGroupInfo, $dvpg;
		}
	}
	return ( $Common_Utils::SUCCESS , \@dvPortGroupInfo );
}

#####RescanAllHba#####
##Description 			:: Rescan all the HBA ports of host for new devices.
##Input 				:: hostview.
##Output 				:: Return Success or Failure.
#####RescanAllHba#####
sub RescanAllHba
{
	my $hostView	= shift;
	my $hostStorageSystem	= Vim::get_view( mo_ref => Common_Utils::GetViewProperty( $hostView, 'configManager.storageSystem' ), properties => [] );
	eval
	{
		$hostStorageSystem->RescanAllHba();
	};
	if($@)
	{
		Common_Utils::WriteMessage("ERROR :: unble to rescan the HBA ports for new storage devices: \n $@.",3);
		return $Common_Utils::FAILURE;
	}	
	$hostView->ViewBase::update_view_data( GetHostProps() );
	return $Common_Utils::SUCCESS;
}

#####getEsxCreds#####
##Description 		:: To get the ESX credential from the CS.
##Input 			:: None.
##Output 			:: username and password on Success else failure.
#####getEsxCreds#####
sub getEsxCreds
{
	my $serverIp	= shift;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	my $userName	= "";
	my $password	= "";
	
	chomp ( $serverIp );
	my ( $returnCode, $validIp)	= Common_Utils::is_a_valid_ip_address( ip => $serverIp );
	if ( $returnCode == $Common_Utils::FAILURE )
	{
		Common_Utils::WriteMessage("ERROR :: Invalid IP address entered for ESX IP.Please provide a valid ESX IP.",3);
		return ( $Common_Utils::INVALIDIP );
	}
	
	( $returnCode , my $cxIpPortNum )= Common_Utils::FindCXIP();
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
		return ( $Common_Utils::FAILURE );
	}
	$Common_Utils::CxIpPortNumber	= $cxIpPortNum;
	
	for( my $i=1; $i <= 4 ; $i++ )
	{
		my %subParameters				 = ( MachineId => $validIp );
		( $returnCode , my $requestXML ) = XmlFunctions::ConstructXmlForCxApi( functionName =>"GetHostCredentials" , subParameters => \%subParameters );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to construct XML file for GetHostCredentials call.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			sleep(15);
			next;
		}
				
		( $returnCode, my $responseXml ) = Common_Utils::PostRequestXml( cxIp => $cxIpPortNum, requestXml => $requestXML, functionName =>"GetHostCredentials" );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to post XML file for GetHostCredentials .",3);
			$ReturnCode = $Common_Utils::FAILURE;
			sleep(15);
			next;
		}
		
		( $returnCode ,my $hashedData )	= XmlFunctions::ReadResponseOfCxApi( response => $responseXml , parameter =>"yes" , parameterGroup => 'no', printXml => "no" );		
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to read response from CX API GetHostCredentials .",3);
			$ReturnCode = $Common_Utils::FAILURE;
			sleep(15);
			next;
		}
		
		$userName	= $$hashedData{UserName};
		$password	= $$hashedData{Password};
		last;
	}
		
	return ( $ReturnCode, $validIp, $userName, $password );
}
 
1;