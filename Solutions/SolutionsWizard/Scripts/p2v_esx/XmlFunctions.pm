=head
	-----XmlFunctions.pm-----
	1. Creates a set of commonf XML functions which generally perform,
		a. Creation of an XML
		b. Modifying XML.
		c. Reading XML
=cut

package XmlFunctions;

use strict;
use warnings;
use Common_Utils;

#####ValidateNode#####
##Description 			:: It checks whether required parameters are set not in the Node. Some of them are Display Name , vmx version,
##							ResourcePoolName, Host Name , DataCenter , vSphere Host Name , vSphere Host Version , memsize , cpuCount, 
##							Alternate Guest Name(guestId).
##Input 				:: XML::LibXML::Element and Type of node( Source Node or Target Node).
##Output 				:: Returns Success else Failure.
#####ValidateNode#####
sub ValidateNode 
{
	my $Node 			= shift;
	my $type 			= shift;
	my $ReturnCode		= $Common_Utils::SUCCESS;
	
	if ( "" eq $Node->getAttribute('display_name') )
	{
		Common_Utils::WriteMessage("ERROR :: Found empty value for display name of the machine to be protected. Invalid XML.",3);
		$ReturnCode 	= $Common_Utils::FAILURE;
	}
	my $displayName		= $Node->getAttribute('display_name');
	if ( "" eq $Node->getAttribute('ip_address') || "NOT SET" eq $Node->getAttribute('ip_address') )
	{
		Common_Utils::WriteMessage("ERROR :: Found either empty value or it is not set incase of IP address for machine $displayName.",3);
		$ReturnCode 	= $Common_Utils::FAILURE;
	}
	
	if ( "" eq $Node->getAttribute('hostname') || "NOT SET" eq $Node->getAttribute('hostname') )
	{
		Common_Utils::WriteMessage("ERROR :: Host name of the machine $displayName is either empty/NOT SET.",3);
		$ReturnCode 	= $Common_Utils::FAILURE;
	}
	
	if ( "" eq $Node->getAttribute('machinetype') )
	{
		Common_Utils::WriteMessage("ERROR :: Found empty value for attribute 'machinetype', for machine $displayName.",3);
		$ReturnCode 	= $Common_Utils::FAILURE;
	}
		
	if ( "virtualmachine" eq lc( $Node->getAttribute('machinetype') ) )
	{	
		if ( ( ! defined ( $Node->getAttribute('hostversion') ) ) || ( "" eq $Node->getAttribute('hostversion') ) )
		{
			Common_Utils::WriteMessage("ERROR :: Found empty value for host version in case of machine $displayName.",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
		
	}
	
	if ( ( ! defined ( $Node->getAttribute('memsize') ) ) || ( 0 eq ( $Node->getAttribute('memsize') ) ) || ( "" eq ( $Node->getAttribute('memsize') ) ) )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find memory size of machine $displayName.",3);
		$ReturnCode 	= $Common_Utils::FAILURE;
	}
	
	if ( ( ! defined ( $Node->getAttribute('cpucount') ) ) || ( 0 eq ( $Node->getAttribute('cpucount') ) ) || ( "" eq ( $Node->getAttribute('cpucount') ) ) )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find virtual CPU count for machine $displayName.",3);
		$ReturnCode 	= $Common_Utils::FAILURE;
	}
	
	if ( ( !defined ( $Node->getAttribute('resourcepool') ) ) || ( "" eq $Node->getAttribute('resourcepool') ) )
	{
		Common_Utils::WriteMessage("ERROR :: Found empty value for resource pool in case of machine $displayName.",3);
		$ReturnCode 	= $Common_Utils::FAILURE;
	}
	
	if ( ( !defined( $Node->getAttribute('ide_count' ) ) ) || ( "" eq $Node->getAttribute('ide_count') ) )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find number of ide devices in machine $displayName.",3);
		$ReturnCode 	= $Common_Utils::FAILURE;
	}
	
	if ( "target" eq lc($type) )
	{
		if ( ( ! defined ( $Node->getAttribute('vmx_version') ) ) || ( "" eq $Node->getAttribute('vmx_version') ) )
		{
			Common_Utils::WriteMessage("ERROR :: Found empty value for vmx version for maachine $displayName.",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
		
		if (  "" eq $Node->getAttribute('datacenter') )
		{
			Common_Utils::WriteMessage("ERROR :: Datacenter value for machine $displayName is set to empty.",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
	}
	
	if ( "target" ne lc($type) )
	{
		if ( ( ! defined ( $Node->getAttribute('alt_guest_name') ) ) || ( "" eq $Node->getAttribute('alt_guest_name') ) ) 
		{
			Common_Utils::WriteMessage("ERROR :: Unable to find guestId for machine $displayName.",3);
			Common_Utils::WriteMessage("ERROR :: 1. check if the 'Guest OS' configuration is set on 'summary' tab for virtual machine $displayName.",3);
			Common_Utils::WriteMessage("ERROR :: 2. If 'Guest OS' is not set,edit settings say 'ok'.",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
		
		my @networkCards	= $Node->getElementsByTagName('nic');
		if ( -1 == $#networkCards )
		{
			Common_Utils::WriteMessage("ERROR :: Unable to find network card information for machine $displayName.",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
		
		my @disksInfo		= $Node->getElementsByTagName('disk');
		if ( -1 != $#disksInfo )
		{
			foreach my $disk ( @disksInfo )
			{
				if ( "ide" eq $disk->getAttribute('ide_or_scsi') && "virtualmachine" eq lc( $Node->getAttribute('machinetype')))
				{
					Common_Utils::WriteMessage("ERROR :: $displayName contains an IDE disk to be protected. Protection of IDE disk is not supportted.",3);
					$ReturnCode 	= $Common_Utils::FAILURE;
				}
				if ( ( ! defined ( $disk->getAttribute('datastore_selected') ) ) || ( "" eq  $disk->getAttribute('datastore_selected') ) )
				{
					Common_Utils::WriteMessage("ERROR :: 'datastore_selected' attribute is either not defined or equals to an empty string in XML file.",3);
					Common_Utils::WriteMessage("ERROR :: Unable to determine target datastore selected for machine $displayName.",3);
					$ReturnCode 	= $Common_Utils::FAILURE;
				}				
				#Todo.. if the Disk type is RDM we need check for the target Lun selected and capacity size and compatibilityMode. 
			}
		}
		else
		{
			Common_Utils::WriteMessage("ERROR :: Unable to find disk information for machine $displayName.",2);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
	}
	
	return ( $ReturnCode );
}

#####UpdateDiskInfo#####
##Description 		:: Updates the Disk information of the machine on source side.In case of addition of disk all the disk are added,
##						and in case of Resume only the disk which are protected are updated.
##Input				:: XML new doc element, host Node and Disk Information, Resume operation or not.
##Output 			:: Returns SUCCESS else FAILURE.
#####UpdateDiskInfo#####
sub UpdateDiskInfo
{
	my %args 		= @_;
	my @disks		= $args{hostNode}->getElementsByTagName('disk');
	my @diskInfo	= @{$args{diskInfo}};
	my %updatedList	= ();
	foreach my $disk ( @disks )
	{
		my $exist 		= 0;
		my $vmdkName 	= $disk->getAttribute('disk_name');
		my $diskUuid	= $disk->getAttribute('source_disk_uuid');
		my @vmdkNameXml = split( /\[|\]|\//,$vmdkName );
		for( my $i = 0 ; $i <= $#diskInfo ; $i++ )
		{
			my $vmdkUuid	= $diskInfo[$i]->{diskUuid};
			if( ( defined $diskUuid ) && ( $diskUuid ne "" ) )
			{
				Common_Utils::WriteMessage("Comparing $diskUuid and $vmdkUuid.",2);
				if( $diskUuid eq $vmdkUuid )
				{
					$exist		= 1;
				}
			}
			else
			{			
				my @vmdkName= split( /\[|\]|\//, $diskInfo[$i]->{diskName} );
				if( $vmdkNameXml[-1] =~ /_InMage_NC_/ )
				{
					Common_Utils::WriteMessage("Disk Name contains a Name Change and disk Name is \"$vmdkName\" and comparing to vmdk from xml $vmdkName.",2);
					if( $vmdkName[-1] !~ /_InMage_NC_/ )
					{
						my $rDot		= rindex( $vmdkName[-1] , "." );
						my $tempName	= substr( $vmdkName[-1] , 0 , $rDot );
						my @splitContr	= split( ":", $diskInfo[$i]->{scsiVmx} );
						my $disk_number = 2000 + ( $splitContr[0] * 16 ) + $splitContr[1];
						if( $splitContr[0] >= 4)
	
	
	
	
	
						{
							$disk_number= 16000 + (($splitContr[0] - 4) * 30) + $splitContr[1];
	
	
	
	
						}
						$vmdkName[-1]	= $tempName."_InMage_NC_$disk_number.vmdk";			
	
	
	
	
					}
				}
				Common_Utils::WriteMessage("Comparing during resume $vmdkNameXml[-1] and $vmdkName[-1].",2);
				if( $vmdkNameXml[-1] eq $vmdkName[-1] )
				{
					$exist		= 1;
				}
			}
			
			if( $exist == 1 )
			{
				$disk->setAttribute( 'size' , $diskInfo[$i]->{diskSize} );
				$disk->setAttribute( 'disk_type' , $diskInfo[$i]->{diskType} );
				$disk->setAttribute( 'disk_mode' , $diskInfo[$i]->{diskMode} );
				$disk->setAttribute( 'ide_or_scsi' , $diskInfo[$i]->{ideOrScsi} );
				$disk->setAttribute( 'scsi_mapping_vmx' , $diskInfo[$i]->{scsiVmx} );
				$disk->setAttribute( 'scsi_mapping_host' , $diskInfo[$i]->{scsiHost} );
				$disk->setAttribute( 'independent_persistent' , $diskInfo[$i]->{independentPersistent} );
				$disk->setAttribute( 'controller_type' , $diskInfo[$i]->{controllerType} );
				$disk->setAttribute( 'controller_mode' , $diskInfo[$i]->{controllerMode} );
				$disk->setAttribute( 'cluster_disk' , $diskInfo[$i]->{clusterDisk} );
				$disk->setAttribute( 'source_disk_uuid' , $diskInfo[$i]->{diskUuid} );
				$updatedList{$diskInfo[$i]->{diskName}} = "Updated";
				$diskInfo[$i]->{diskNode} = $disk;
				last;
			}
		}
		
		if( "yes" eq lc( $args{resume} ) )
		{
			Common_Utils::WriteMessage("Unable to find disk \"$vmdkName\" at source side.So removing from the list.",2);
			$disk->unbindNode();
		}
	}
	
	if ( "yes" ne lc( $args{resume} ) )
	{
		for( my $i = 0 ; $i <= $#diskInfo ; $i++ )
		{
			if ( exists $updatedList{$diskInfo[$i]->{diskName}} )
			{
				my $diskE	= $diskInfo[$i]->{diskNode};#21928
				my $dName	= $diskE->getAttribute('disk_name');
				Common_Utils::WriteMessage("$dName already protected unbinding and appending disknode.",2);
				$diskE->unbindNode();
				$args{hostNode}->appendChild( $diskE );
				next;
			}	
			my $disk 	= $args{xmlDoc}->createElement('disk');
			$disk->setAttribute( 'disk_name' , $diskInfo[$i]->{diskName} );
			$disk->setAttribute( 'size' , $diskInfo[$i]->{diskSize} );
			$disk->setAttribute( 'disk_type' , $diskInfo[$i]->{diskType} );
			$disk->setAttribute( 'disk_mode' , $diskInfo[$i]->{diskMode} );
			$disk->setAttribute( 'ide_or_scsi' , $diskInfo[$i]->{ideOrScsi} );
			$disk->setAttribute( 'scsi_mapping_vmx' , $diskInfo[$i]->{scsiVmx} );
			$disk->setAttribute( 'scsi_mapping_host' , $diskInfo[$i]->{scsiHost} );
			$disk->setAttribute( 'independent_persistent' , $diskInfo[$i]->{independentPersistent} );
			$disk->setAttribute( 'controller_type' , $diskInfo[$i]->{controllerType} );
			$disk->setAttribute( 'controller_mode' , $diskInfo[$i]->{controllerMode} );
			$disk->setAttribute( 'cluster_disk' , $diskInfo[$i]->{clusterDisk} );
			$disk->setAttribute( 'source_disk_uuid' , $diskInfo[$i]->{diskUuid} );
			$args{hostNode}->appendChild($disk);
		}
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####UpdateTargetInfo#####
##Description		:: Updates target vCenter/vSphere Information.
##Input				:: Target Node.
##Output			:: Returns SUCCESS else FAILURE.
#####UpdateTargetInfo#####
sub UpdateTargetInfo
{
	my %args			= @_;
	my $doc 			= XML::LibXML::Document->new();
	
	my @datastoreNodes				= $args{node}->getElementsByTagName('datastore');
	if ( -1 != $#datastoreNodes )
	{
		foreach my $datastore ( @datastoreNodes )
		{
			$datastore->unbindNode();
		}
	}
	( my $returnCode , $args{node} ) = MakeDataStoreNode( xmlDoc => $doc , rootElement => $args{node} , datastoreInfo => $args{datastoreInfo} );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to list datastore information.",3);
		return $Common_Utils::FAILURE;
	}
	
	my @networkNodes				= $args{node}->getElementsByTagName('network');
	if ( -1 != $#networkNodes )
	{
		foreach my $network ( @networkNodes )
		{
			$network->unbindNode();
		}
	}
	( $returnCode , $args{node} ) 	= MakeNetworkNodes( xmlDoc => $doc , rootElement => $args{node} , networkInfo => $args{networkInfo} );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to list network information.",3);
		return $Common_Utils::FAILURE;
	}
	
	my @lunNodes					= $args{node}->getElementsByTagName('lun');
	if ( -1 != $#lunNodes )
	{
		foreach my $lun ( @lunNodes )
		{
			$lun->unbindNode();
		}
	}
	if ( "" ne $args{scsiDiskInfo} )
	{
		( $returnCode , $args{node} )= MakeScsiDiskNodes( xmlDoc => $doc , rootElement => $args{node} , scsiDiskInfo => $args{scsiDiskInfo} );
		if ( $Common_Utils::SUCCESS ne $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to list SCSI device information.",3);
			return $Common_Utils::FAILURE;
		}
	}
	
	my @resourcepoolNodes			= $args{node}->getElementsByTagName('resourcepool');
	if ( -1 != $#resourcepoolNodes )
	{
		foreach my $resourcepool ( @resourcepoolNodes )
		{
			$resourcepool->unbindNode();
		}
	}
	( $returnCode , $args{node} ) 	= MakeResourcePoolNodes ( xmlDoc => $doc , rootElement => $args{node} , resourcePoolInfo => $args{resourcePoolInfo} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to list vSphere Configuration information.",3);
		return $Common_Utils::FAILURE;
	}
	
	my @configNodes					= $args{node}->getElementsByTagName('config');
	if ( -1 != $#configNodes )
	{
		foreach my $config ( @configNodes )
		{
			$config->unbindNode();
		}
	}
	( $returnCode , $args{node} ) 	= MakeConfigNodes( xmlDoc => $doc , rootElement => $args{node} , configInfo => $args{configInfo} );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to list vSphere Configuration information.",3);
		return $Common_Utils::FAILURE;
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####UpdateNode#####
##Description		:: It updates the Node with the Latest information.
##Input 			:: SourceNode and View of machine with whose details node has to be updated.
##Output 			:: Returns SUCCESS else FAILURE.
#####UpdateNode#####
sub UpdateNode
{
	my $machineInfo		= shift;
	my $vmView			= shift;
	my @diskList		= ();
	my $machineName 	= $machineInfo->getAttribute('display_name');
	my $doc 			= XML::LibXML::Document->new();

	my ( $returnCode , $ideCount , $rdm , $cluster , $diskInfo , $floppyDevices ) = VmOpts::FindDiskDetails( $vmView , \@diskList );
    if( $Common_Utils::SUCCESS != $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to get details of disk for machine \"$machineName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	$machineInfo->setAttribute( 'rdm',$rdm );
	$machineInfo->setAttribute( 'ide_count' , $ideCount );
	$machineInfo->setAttribute( 'source_uuid' , Common_Utils::GetViewProperty( $vmView,'summary.config.uuid') );
	
	if ( defined Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus') )
    {
        if ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsOk" ) 
		{
			$machineInfo->setAttribute( 'vmwaretools' , "OK" );
		}
        elsif ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsNotInstalled" ) 
		{
			$machineInfo->setAttribute( 'vmwaretools' , "NotInstalled" );
		}
		elsif ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsNotRunning")
		{
			$machineInfo->setAttribute( 'vmwaretools' , "NotRunning" );
		}
		elsif ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsOld" )
		{
			$machineInfo->setAttribute( 'vmwaretools' , "OutOfdate" );
		}
    }
	
	( $returnCode ,my $ipAddress ,my $networkInfo ) = VmOpts::FindNetworkDetails( $vmView );
	if( $Common_Utils::SUCCESS != $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find Network information of Virtual Machine \"$machineName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	$machineInfo->setAttribute( 'ip_address' , $ipAddress );
	
	( $returnCode ) = UpdateDiskInfo( xmlDoc => $doc , hostNode => $machineInfo , diskInfo => $diskInfo , resume => "no" );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to make disk node for machine \"$machineName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS );
}

#####SaveXML#####
##Description 			:: Saves the XML file.
##Input 				:: Complete file Path.
##Output 				:: Returns SUCCESS or FAILURE.
#####SaveXML#####
sub SaveXML
{
	my $planNodes 	= shift;
	my $file 		= shift;
	Common_Utils::WriteMessage("Writing file $file.",2);
	my $tempfile	= $Common_Utils::vContinuumMetaFilesPath."/Temp.xml";
	if( 0 == open(XML,">$tempfile"))
	{
		Common_Utils::WriteMessage(" ERROR :: Unable to create XML file \"$tempfile\".",3);
		Common_Utils::WriteMessage(" ERROR :: Please check for adequate permissions to create a file and do a re-protection.",3);
		return $Common_Utils::FAILURE;
	}
	
	my @temp	= @$planNodes;
	if ( $#temp > 0 )
	{
		print XML $$planNodes[0]->toString(1);
	} 
	else
	{
		print XML $$planNodes[0]->toString(1);
	}
	close XML;
	if ( -f $file )
	{
		unlink $file;
	}
	rename $tempfile,$file;
	
	Common_Utils::SetFilePermissions($file);
	
	return ( $Common_Utils::SUCCESS );
}

#####WriteInmageScsiUnitsFile#####
##Description			:: Writes InMage SCSI units file.
##Input 				:: None, it reads ESX.xml file and writes the Information.
##Output 				:: Returns SUCCESS else FAILURE.
#####WriteInmageScsiUnitsFile#####
sub WriteInmageScsiUnitsFile
{
	my %args 				= @_;
	my $inmageFxPath		= File::Basename::dirname($Common_Utils::vContinuumInstDirPath)."/failover/data";
	if ( ! -d $inmageFxPath )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find InMage FX agent installation path.",3);
		Common_Utils::WriteMessage("ERROR :: Please check whether aganet is installed or not.",3);
		return ( $Common_Utils::FAILURE );
	}
		
	my $fileName 			= $args{file};
	my $parser 				= XML::LibXML->new();
	my $tree 				= $parser->parse_file($fileName);
	my $root 				= $tree->getDocumentElement;
	my @planNodes 			= $root->getElementsByTagName('root');
	if ( -1 == $#planNodes )
	{
		@planNodes	= ();
		push @planNodes	, $root;		
	}
	
	my $xmldirecotryPath	= "";
	foreach my $plan ( @planNodes )
	{
		my $xmlDirectoryName= $plan->getAttribute('xmlDirectoryName');
		$Common_Utils::vContinuumPlanDir= $xmlDirectoryName;
		$xmldirecotryPath	= $inmageFxPath."/$xmlDirectoryName";
		if ( ! -d $xmldirecotryPath )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find directory $xmldirecotryPath.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		if ( 0 == open( ISUD ,">$xmldirecotryPath/Inmage_scsi_unit_disks.txt") )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to create Inmage_scsi_unit_disks.txt file.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		my @sourceHosts		= $plan->getElementsByTagName('SRC_ESX');
		my @targetHost		= $plan->getElementsByTagName('TARGET_ESX');
		my @targetNodes		= $targetHost[0]->getElementsByTagName('host');
		my $mtVmxVersion	= $targetNodes[0]->getAttribute('vmx_version');
		my $mtHostId		= $targetNodes[0]->getAttribute('inmage_hostid');
		
		my $isMtCentOs6		= 0;
		if( $targetNodes[0]->getAttribute('os_info') =~ /linux/i )
		{
			my ($returnCode, $hostInfo ) = Common_Utils::GetHostInfoFromCx( hostId => $mtHostId );
			if( ( $returnCode == $Common_Utils::SUCCESS ) && ( $$hostInfo{Caption} =~ /CentOS release 6/i ) )
			{
				$isMtCentOs6 = 1;
			}
		}

		my %map_VirtualNode_Port	=	();
		my @dummyDisksInfo	= $targetHost[0]->getElementsByTagName('dummy_disk');
		if( -1 != $#dummyDisksInfo)
		{
			my @dummy_disk_nodes =	$dummyDisksInfo[0]->getElementsByTagName('MT_Disk');
			foreach (@dummy_disk_nodes)
			{
				$map_VirtualNode_Port{$_->getAttribute('virtual_node')}  = $_->getAttribute('port_number');
			}
		}
		
		my %clusterVmdks	= ();
		foreach my $sourceHost ( @sourceHosts )
		{
			my @srcNodes	= $sourceHost->getElementsByTagName('host');
			foreach my $srcNode ( @srcNodes )
			{
				my @lines 				= ();
				my $sourceHostVersion 	= $srcNode->getAttribute('hostversion');
				my $targetHostVersion 	= $targetNodes[0]->getAttribute('hostversion');
				#read the Source SCSI number and Target SCSI number.
				my @disksInfo			= $srcNode->getElementsByTagName('disk');
				
				my $isDiskSignExist = 1;
				foreach my $disk ( @disksInfo )
				{
					if( ( ! defined $disk->getAttribute('disk_signature') ) || ( "" eq $disk->getAttribute('disk_signature') ) )
					{
						$isDiskSignExist = 0;
						last;
					}
				}
				
				foreach my $disk ( @disksInfo )
				{
					if( (defined $args{operation} ) && ( "remove" eq $args{operation} ) )
					{
						if ( "true" ne lc ( $disk->getAttribute('remove') ) )
						{
							next;
						}						
					}					
					elsif ( "yes" eq lc ( $disk->getAttribute('protected') ) )
					{
						next;
					}
					
					my $targetDiskUuid	= $disk->getAttribute('target_disk_uuid');
					
					if($targetDiskUuid ne "")
					{
						if( defined $clusterVmdks{$targetDiskUuid} )
						{
							next;
						}
						$clusterVmdks{$targetDiskUuid} = "selected";
					}
					
					my @Master_Port_numbers = split(/:/,$disk->getAttribute('scsi_id_onmastertarget'));
					my $sourceScsiNum		= $disk->getAttribute('scsi_mapping_host');
					
					#p2v cluster cases same scsi id for the multiple disks.
					if( (defined $args{operation} ) && ( "remove" eq $args{operation} ) && ( $isDiskSignExist ))
					{
						$sourceScsiNum	= $sourceScsiNum . "(" . $disk->getAttribute('disk_signature') . ")";
					}
					
					if( "yes" eq $srcNode->getAttribute('cluster'))
					{
						$sourceScsiNum	= $sourceScsiNum . "(" . $disk->getAttribute('disk_signature') . ")";
					}
					
					my $changed 			= 0;
		
					foreach my $Virtual_node(sort keys %map_VirtualNode_Port)
					{
						my @Virtual_node_numbers	= split(/:/,$Virtual_node);
						if( $Virtual_node_numbers[0] == $Master_Port_numbers[0])
						{
							if( "" ne $map_VirtualNode_Port{$Virtual_node} )
							{
								$changed 				= 1;
								if( ( lc($mtVmxVersion) ge "vmx-10" ) && ( $Master_Port_numbers[0] >= 4) )
								{
									$Master_Port_numbers[0] = $map_VirtualNode_Port{$Virtual_node} + $Master_Port_numbers[1] - $Virtual_node_numbers[1];
									$Master_Port_numbers[1] = 0;
								}
								else
								{
									$Master_Port_numbers[0] = $map_VirtualNode_Port{$Virtual_node};
								}
								Common_Utils::WriteMessage("Virtual Node Matched = $Virtual_node and Port Number Used for it = $map_VirtualNode_Port{$Virtual_node}.",2);
								last;
							}
						}
					}
					
					if( $srcNode->getAttribute('os_info') =~ /windows/i )
					{	
						if(!$changed)
						{
							if( $targetHostVersion ge "4.1.0")
							{
								$Master_Port_numbers[0] = $Master_Port_numbers[0] + 2;
							}
							else
							{
								$Master_Port_numbers[0] = $Master_Port_numbers[0] + $targetNodes[0]->getAttribute('ide_count') ;	
							}
						}
						push @lines , "$sourceScsiNum!@!@!$Master_Port_numbers[0]:$Master_Port_numbers[1]\n";
					}
					else
					{
						my $driveName	= $disk->getAttribute('src_devicename');
						if(!$changed)
						{
							if( $isMtCentOs6 )
							{
								$Master_Port_numbers[0] = $Master_Port_numbers[0] + 2;								
							}
						}
						push @lines , "$driveName!@!@!$Master_Port_numbers[0]:$Master_Port_numbers[1]\n";
					}
				}
				
				if( (defined $args{operation} ) && ( "resize" eq $args{operation} ) )
				{
					my $hostName 	= $srcNode->getAttribute('hostname');
					my $hostId		= $srcNode->getAttribute('inmage_hostid');
					print ISUD "$hostId($hostName)\n";					
				}
				elsif( -1 != @lines )
				{
					my $hostName 	= $srcNode->getAttribute('hostname');
					my $hostId		= $srcNode->getAttribute('inmage_hostid');
					print ISUD "$hostId($hostName)\n";
					print ISUD ($#lines+1);
					print ISUD "\n";
					foreach my $line ( @lines )
					{
						print ISUD $line;	
					}				
				}		
			}
		}	
	}
	close( ISUD );
	
	Common_Utils::SetFilePermissions("$xmldirecotryPath/Inmage_scsi_unit_disks.txt");
	
	return ( $Common_Utils::SUCCESS );
}
#####GetPlanNodes#####
##Description 	:: Gets the Plan Node information from the XML file. XML may contain multiple Plan information.
##Input 		:: XML file.
##Output 		:: It returns reference to plan nodes on SUCCESS else FAILURE.
#####GetPlanNodes#####
sub GetPlanNodes
{
	my $file		= shift;
	if ( ! -e $file  )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find file \"$file\".",3);
		Common_Utils::WriteMessage("ERROR :: Please check whether above file exists or not.",3);
		return $Common_Utils::FAILURE;
	}
	
	my $parser 		= XML::LibXML->new();
	my $tree		= "";
	eval
	{
		$tree 		= $parser->parse_file($file);
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("ERROR :: $@.",2);
		return ( $Common_Utils::FAILURE );
	}
	
	my $root 		= $tree->getDocumentElement;
	return ( $Common_Utils::SUCCESS , $root );
}

#####WriteDummyDiskInfo#####
##Description 	:: Writes Dummy Disk information to XML file.
##Input 		:: file to modify and Dummy Disk Information.
##Output 		:: Returns SUCCESS else FAILURE.
#####WriteDummyDiskInfo#####
sub WriteDummyDiskInfo
{
	my $file 					= shift;
	my $dummyDiskCreatedInfo	= shift;
	
	my $parser 					= XML::LibXML->new();
	my $doc 					= XML::LibXML::Document->new();
	my $tree 					= $parser->parse_file($file);
	my $root 					= $tree->getDocumentElement;
	my @targetNodes 			= $root->getElementsByTagName('TARGET_ESX');
	
	foreach my $targetNode ( @targetNodes )
	{ 
		my @dummyDiskNodes 		= $targetNode->getElementsByTagName('dummy_disk');
		if( -1 != $#dummyDiskNodes )
		{
			foreach ( @dummyDiskNodes )
			{
				$_->unbindNode();
			}
		}
		my @hosts					= $targetNode->getElementsByTagName('host');
		foreach my $host ( @hosts )
		{
			my $dummyDisks 			= $doc->createElement('dummy_disk');
			
			if ( ! exists $dummyDiskCreatedInfo->{$host->getAttribute('source_uuid')}{$host->getAttribute('hostname')} )
			{
				next;
			}
			my $dummyDiskInfo		= $dummyDiskCreatedInfo->{$host->getAttribute('source_uuid')}{$host->getAttribute('hostname')};
			foreach my $dummyDisk( @$dummyDiskInfo )
			{
				my $dummy_disk_child = $doc->createElement('MT_Disk');
				$dummy_disk_child->setAttribute( 'name', $dummyDisk->{fileName} );
				$dummy_disk_child->setAttribute('virtual_node',"$dummyDisk->{scsiNum}:$dummyDisk->{unitNum}");
				$dummy_disk_child->setAttribute('size',$dummyDisk->{fileSize});
				$dummy_disk_child->setAttribute('disk_uuid',$dummyDisk->{diskUuid});
				$dummy_disk_child->setAttribute('port_number',"");
				$dummyDisks->appendChild($dummy_disk_child);
			}
			$targetNode->appendWellBalancedChunk( $dummyDisks->toString(1) );
		}
	}
	$root->toString(1);
	my @docs						= $root;
	if ( $Common_Utils::SUCCESS ne SaveXML( \@docs , $file ) )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to save XML file after modifications for dummy disk.",3);
		return ( $Common_Utils::FAILURE );	
	}	
	return ( $Common_Utils::SUCCESS );
} 

#####UpdateEsxMasterFile#####
##Description 		:: Exclusive functon to update recovery database entries in to ESX_Master file and also writes a temporary
##						file which contains the host node information which has to be updated to MasterConfig file.
##Input 			:: Recovery database Info , Host Nodes in CX , ESX Master file Name.
##Ouput 			:: Returns SUCCESS else FAILURE.
#####UpdateEsxMasterFile#####
sub UpdateEsxMasterFile
{
	my $recoveryDbInfo 		= shift;
	my $EsxMasterFileName	= shift;
	
	my ( $returnCode , $docElement ) 	= XmlFunctions::GetPlanNodes2( $EsxMasterFileName );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get plan information from file $EsxMasterFileName..",3);
		return $Common_Utils::FAILURE;
	}
	
	my $planNodes 			= $docElement->getElementsByTagName('root');
	my @planNodesTemp		= @$planNodes;
	for ( my $i = 0 ; $i <= $#planNodesTemp ; $i++ )
	{
		my @sourceInfo	= $planNodesTemp[$i]->getElementsByTagName('SRC_ESX');
		foreach my $sourceInfo ( @sourceInfo )
		{
			my @hostsInfo	= $sourceInfo->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				foreach my $db ( @$recoveryDbInfo )
				{
					if ( $host->getAttribute('hostname') ne $$db{hostName} )
					{
						next;
					}
					if ( !defined $$db{update} )
					{
						$host->unbindNode();
					}
				}
			}
		}
	}
	my @docElements = $docElement;
	$returnCode		= SaveXML( \@docElements , $EsxMasterFileName );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS );
}

#####PrepareTempEsxMasterFile#####
##Description		:: It writes temporary master XML file which contains only the nodes which are in CX.
##Input 			:: Recovery database Info and ESX Master File name.
##Output			:: Returns SUCCESS else FAILURE.
#####PrepareTempEsxMasterFile#####
sub PrepareTempEsxMasterFile
{
	my $recoveryDbInfo 		= shift;
	my $EsxMasterFileName	= shift;
	
	my ( $returnCode , $docElement ) 	= XmlFunctions::GetPlanNodes2( $EsxMasterFileName );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get plan information from file $EsxMasterFileName..",3);
		return $Common_Utils::FAILURE;
	}
	
	my $planNodes 			= $docElement->getElementsByTagName('root');
	my @planNodesTemp		= @$planNodes;
	for ( my $i = 0 ; $i <= $#planNodesTemp ; $i++ )
	{
		my $planName		= $planNodesTemp[$i]->getAttribute('plan');
		my @sourceInfo		= $planNodesTemp[$i]->getElementsByTagName('SRC_ESX');
		foreach my $sourceInfo ( @sourceInfo )
		{
			my @hostsInfo	= $sourceInfo->getElementsByTagName('host');
			foreach my $host ( @hostsInfo )
			{
				my $exist	= 0;
				foreach my $db ( @$recoveryDbInfo )
				{
					if ( $host->getAttribute('hostname') ne $$db{hostName} )
					{
						next;
					}
					Common_Utils::WriteMessage("$$db{hostName} exists in recovery database.",2);
					if ( (! defined $$db{existsInCX} ) && ( "yes" ne lc( $$db{existsInCX} ) ) )
					{
						$$db{update}= "yes";
						next;
					}
					Common_Utils::WriteMessage("$$db{hostName} exists in CX.",2);
					$exist 			= 1;
					my @disksNodes	= $host->getElementsByTagName('disk');
					my @scsiIds		= split( /@!@!/ , $$db{diskIds} );
					#todo.. here we need to handle a case where number of SCSI ids and number of disk nodes are not equal 
					for ( my $iter = 0 ; $iter <= $#disksNodes ; $iter++  )
					{
						$scsiIds[$iter] =~ s/^\s+//;
		 				$scsiIds[$iter] =~ s/\s+$//;
						$disksNodes[$iter]->setAttribute('scsi_id_onmastertarget', $scsiIds[$iter]);
					}
				}
				if ( !$exist )
				{
					$host->unbindNode();
				}
			}
		}
	}
	
	my $EsxMasterTempFileName	= "$Common_Utils::vContinuumMetaFilesPath/Esx_Master_Temp.xml";
	my @docElements 			= $docElement;	 
	$returnCode					= SaveXML( \@docElements , $EsxMasterTempFileName );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####UpdateNewMAC#####
##Description		:: Updates New MAC address after machine is created.
##Input 			:: Host Node.
##Output 			:: Returns SUCCESS or FAILURE.
#####UpdateNewMAC#####
sub UpdateNewMAC
{
	my $hostNode	= shift;
	my $vmView		= shift;
	
	my @Nic 		= $hostNode->getElementsByTagName('nic');
	foreach my $Nic( @Nic )
	{
		my $devices 	= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
		foreach my $device (@$devices)
		{
			if( ( $device->deviceInfo->label !~ /^Network adapter /i ) || ( $device->deviceInfo->label ne $Nic->getAttribute('network_label') ) )
			{
				next;
			}
			my $newMacAddress 	= $device->macAddress;
			my $oldMacAddress 	= $Nic->getAttribute('nic_mac');
			Common_Utils::WriteMessage("Old MAC = $oldMacAddress , New MAC = $newMacAddress.",2);
			if ( $oldMacAddress eq $newMacAddress )
			{
				next;
			}
			$Nic->setAttribute('new_macaddress' , $newMacAddress);
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####UpdateMasterXmlCX#####
##Description 		:: Updates CX Master XML file with recovered machines data.
##Input 			:: Path to ESX Master XML file(Latest Modified).
##Output 			:: Returns SUCCESS on success else FAILURE.
#####UpdateMasterXmlCX#####
sub UpdateMasterXmlCX
{
	my %args		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	my $command 	= "--updatetocx \"$args{fileName}\"";
	Common_Utils::WriteMessage("Calling vContinuum.exe --updatetocx with command = $command.",2);
	
	my $returnCode	= system("\"$Common_Utils::vContinuumInstDirPath/vContinuum.exe\" $command");
	Common_Utils::WriteMessage("ReturnCode from vContinuum.exe while updating master xml, system call = $returnCode.",2);
	$returnCode 	= $returnCode >> 8;
	Common_Utils::WriteMessage("ReturnCode from vContinuum.exe while updating master xml = $returnCode.",2);
	if( 0 != $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to update recovered machine data in Master XML file.",3);
		$ReturnCode	= $Common_Utils::FAILURE ;
	} 
	Common_Utils::WriteMessage("UpdateMasterXmlCX = $ReturnCode.",2);	
	return $ReturnCode;
}

#####UpdateMasterXmlCX#####
##Description 		:: Updates CX Master XML file with recovered machines data.
##Input 			:: Path to ESX Master XML file(Latest Modified).
##Output 			:: Returns SUCCESS on success else FAILURE.
#####UpdateMasterXmlCX#####
sub UpdateTempMasterXmlCX
{
	my %args		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	my $command 	= "--updatetolatest \"$args{fileName}\"";
	Common_Utils::WriteMessage("Calling vContinuum.exe --updatetolatest with command = $command.",2);
	
	my $returnCode	= system("\"$Common_Utils::vContinuumInstDirPath/vContinuum.exe\" $command");
	$returnCode = $returnCode >> 8;
	Common_Utils::WriteMessage("Returncode from system command = $returnCode.",2);
	if( 0 != $returnCode )
	{
		$ReturnCode	= $Common_Utils::FAILURE ;
	} 
		
	return $ReturnCode;
}

#####MakeConfigNodes#####
##Description 		:: Makes configuration information nodes listed in Info.xml.
##Input 			:: XML document element, config Information.
##Output 			:: Returns SUCCESS and the XML rootElement else FAILURE.
#####MakeConfigNodes#####
sub MakeConfigNodes
{
	my %args 		 	= @_;
	my $doc 		 	= $args{xmlDoc};
	my @configInfo		= @ { $args{configInfo} };
	
	for ( my $i = 0 ; $i <= $#configInfo ; $i++ )
	{
		my $config 	= $doc->createElement('config');
		$config->setAttribute( 'vSpherehostname' , $configInfo[$i]->{vSphereHostName} );
		$config->setAttribute( 'cpucount' , $configInfo[$i]->{cpu} );
		$config->setAttribute( 'memsize' , $configInfo[$i]->{memory} );
		$args{rootElement}->appendChild($config);
	} 
	
	return ( $Common_Utils::SUCCESS , $args{rootElement} );
}

#####MakeDiskNode#####
##Description 		:: Arranges all data of disks in a Node.
##Input 			:: XML documnet element and DiskInfo;
##Outout 			:: Returns Disk Node on SUCCESS else FAILURE.
#####MakeDiskNode#####
sub MakeDiskNode
{
	my %args 		= @_;
	my $doc 		= $args{xmlDoc};
	if( lc(ref($args{diskInfo})) ne "array" )
	{
		my $dispName	= $args{hostNode}->getAttribute('display_name');
#		Common_Utils::WriteMessage("skipping disk node as diskInfo not an array reference for $dispName",2);
		return ( $Common_Utils::SUCCESS  , $args{hostNode} );
	}
	my @diskInfo	= @{ $args{diskInfo} };
	
	for ( my $i = 0 ; $i <= $#diskInfo ; $i++ )
	{
		my $disk 	= $doc->createElement('disk');
		$disk->setAttribute( 'disk_name' , $diskInfo[$i]->{diskName} );
		$disk->setAttribute( 'size' , $diskInfo[$i]->{diskSize} );
		$disk->setAttribute( 'disk_type' , $diskInfo[$i]->{diskType} );
		$disk->setAttribute( 'disk_mode' , $diskInfo[$i]->{diskMode} );
		$disk->setAttribute( 'source_disk_uuid' , $diskInfo[$i]->{diskUuid} );
		$disk->setAttribute( 'ide_or_scsi' , $diskInfo[$i]->{ideOrScsi} );
		$disk->setAttribute( 'scsi_mapping_vmx' , $diskInfo[$i]->{scsiVmx} );
		$disk->setAttribute( 'scsi_mapping_host' , $diskInfo[$i]->{scsiHost} );
		$disk->setAttribute( 'independent_persistent' , $diskInfo[$i]->{independentPersistent} );
		$disk->setAttribute( 'controller_type' , $diskInfo[$i]->{controllerType} );
		$disk->setAttribute( 'controller_mode' , $diskInfo[$i]->{controllerMode} );
		$disk->setAttribute( 'cluster_disk' , $diskInfo[$i]->{clusterDisk} );
		$disk->setAttribute( 'disk_object_id' , $diskInfo[$i]->{diskObjectId} );
		$disk->setAttribute( 'capacity_in_bytes' , $diskInfo[$i]->{capacityInBytes} );
		$args{hostNode}->appendChild($disk);
	}
	
	return ( $Common_Utils::SUCCESS  , $args{hostNode} );
}

#####MakeNicNode#####
##Description 		:: Arranges all data of Nic's in a Node.
##Input 			:: XML documnet element and NetworkInfo;
##Outout 			:: Returns Nic Node on SUCCESS else FAILURE.
#####MakeNicNode#####
sub MakeNicNode
{
	my %args 		= @_;
	my $doc 		= $args{xmlDoc};
	if( lc(ref($args{networkInfo})) ne "array" )
	{
		my $dispName	= $args{hostNode}->getAttribute('display_name');
#		Common_Utils::WriteMessage("skipping nic node as nicInfo not an array reference for $dispName",2);
		return ( $Common_Utils::SUCCESS  , $args{hostNode} );
	}
	my @networkInfo	= @{ $args{networkInfo} };
	
	for ( my $i = 0 ; $i <= $#networkInfo ; $i++ )
	{
		my $nic 	= $doc->createElement('nic');
		$nic->setAttribute( 'network_label' , $networkInfo[$i]->{networkLabel} );
		$nic->setAttribute( 'network_name' , $networkInfo[$i]->{network} );
		$nic->setAttribute( 'nic_mac' , $networkInfo[$i]->{macAddress} );
		$nic->setAttribute( 'ip' , $networkInfo[$i]->{macAssociatedIp} );
		$nic->setAttribute( 'address_type' , $networkInfo[$i]->{addressType} );
		$nic->setAttribute( 'adapter_type' , $networkInfo[$i]->{adapterType} );
		$nic->setAttribute( 'dhcp' , $networkInfo[$i]->{isDhcp} );
		$nic->setAttribute( 'dnsip' , $networkInfo[$i]->{dnsIp} );
		$args{hostNode}->appendChild($nic);
	}
	
	return ( $Common_Utils::SUCCESS  , $args{hostNode} );
}

#####MakeDataStoreNode#####
##Description 		:: Makes Datastore information nodes listed in Master.xml.
##Input 			:: XML document element, datastore Information.
##Output 			:: Returns SUCCESS and the XML rootElement else FAILURE.
#####MakeDataStoreNode#####
sub MakeDataStoreNode
{
	my %args 		 	= @_;
	my $doc 		 	= $args{xmlDoc};
	my @dataStoreInfo	= @ { $args{datastoreInfo} };
	
	for ( my $i = 0 ; $i <= $#dataStoreInfo ; $i++ )
	{
		my $datastore 	= $doc->createElement('datastore');
		$datastore->setAttribute( 'datastore_name' , $dataStoreInfo[$i]->{symbolicName} );
		$datastore->setAttribute( 'vSpherehostname' , $dataStoreInfo[$i]->{vSphereHostName} );
		$datastore->setAttribute( 'total_size' , $dataStoreInfo[$i]->{capacity} );
		$datastore->setAttribute( 'free_space' , $dataStoreInfo[$i]->{freeSpace} );
		$datastore->setAttribute( 'datastore_blocksize_mb' , $dataStoreInfo[$i]->{blockSize} );
		$datastore->setAttribute( 'filesystem_version' , $dataStoreInfo[$i]->{fileSystem} );
		$datastore->setAttribute( 'type' , $dataStoreInfo[$i]->{type} );
		$datastore->setAttribute( 'uuid' , $dataStoreInfo[$i]->{uuid} );
		$datastore->setAttribute( 'disk_name' , $dataStoreInfo[$i]->{diskName} );
		$datastore->setAttribute( 'display_name' , $dataStoreInfo[$i]->{displayName} );
		$datastore->setAttribute( 'vendor' , $dataStoreInfo[$i]->{vendor} );
		$datastore->setAttribute( 'lun' , $dataStoreInfo[$i]->{lunNumber} );
		$args{rootElement}->appendChild($datastore);
	} 
	
	return ( $Common_Utils::SUCCESS , $args{rootElement} );
}

#####MakeNetworkNodes#####
##Description 		:: Makes Netowrk information nodes listed in Master.xml.
##Input 			:: XML document element, network Information.
##Output 			:: Returns SUCCESS and the XML rootElement else FAILURE.
#####MakeNetworkNodes#####
sub MakeNetworkNodes
{
	my %args 		 	= @_;
	my $doc 		 	= $args{xmlDoc};
	my @networkInfo		= @ { $args{networkInfo} };
	
	for ( my $i = 0 ; $i <= $#networkInfo ; $i++ )
	{
		my @hostNetworkInfo = @{$networkInfo[$i]};
		for ( my $j = 0 ; $j <= $#hostNetworkInfo ; $j++ )
		{
			my $network 	= $doc->createElement('network');
			$network->setAttribute( 'vSpherehostname' , $hostNetworkInfo[$j]->{vSphereHostName} );
			$network->setAttribute( 'name' , $hostNetworkInfo[$j]->{portgroupName} );
			$network->setAttribute('switchUuid',"");
			$network->setAttribute('portgroupKey',"");
			if ( exists $hostNetworkInfo[$j]->{portgroupNameDVPG} )
			{
				$network->setAttribute( 'name' , $hostNetworkInfo[$j]->{portgroupNameDVPG} );
				$network->setAttribute('switchUuid',$hostNetworkInfo[$j]->{switchUuid});
				$network->setAttribute('portgroupKey',$hostNetworkInfo[$j]->{portgroupKey});
			}
					
			if ( exists $hostNetworkInfo[$j]->{portgroupType} && ( "no" eq lc( $args{isItvCenter} ) ) && ( 'ephemeral' ne $hostNetworkInfo[$j]->{portgroupType} ) )
			{
				next;
			}
			$args{rootElement}->appendChild($network);
		}
	} 
	
	return ( $Common_Utils::SUCCESS , $args{rootElement} );
}

#####MakeScsiDiskNodes#####
##Description 		:: Makes SCSI device information nodes listed in Master.xml.
##Input 			:: XML document element, SCSI device Information.
##Output 			:: Returns SUCCESS and the XML rootElement else FAILURE.
#####MakeScsiDiskNodes#####
sub MakeScsiDiskNodes
{
	my %args 		 	= @_;
	my $doc 		 	= $args{xmlDoc};
	my @scsiDiskInfo	= @ { $args{scsiDiskInfo} };
	
	for ( my $i = 0 ; $i <= $#scsiDiskInfo ; $i++ )
	{
		my $scsiDisk 	= $doc->createElement('lun');
		$scsiDisk->setAttribute( 'name' , $scsiDiskInfo[$i]->{displayName} );
		$scsiDisk->setAttribute( 'vSpherehostname' , $scsiDiskInfo[$i]->{vSphereHostName} );
		$scsiDisk->setAttribute( 'devicename' , $scsiDiskInfo[$i]->{deviceName} );
		$scsiDisk->setAttribute( 'adapter' , $scsiDiskInfo[$i]->{adapter} );
		$scsiDisk->setAttribute( 'lun' , $scsiDiskInfo[$i]->{lunNumber} );
		$scsiDisk->setAttribute( 'capacity_in_kb' , $scsiDiskInfo[$i]->{capacity} );
		$args{rootElement}->appendChild($scsiDisk);
	} 
	
	return ( $Common_Utils::SUCCESS , $args{rootElement} );
}

#####MakeResourcePoolNodes#####
##Description		:: Makes resource pool nodes in Info.xml.
##Input 			:: XML document element, resourcePool Information.
##Output 			:: Returns SUCCESS and the XML rootElement else FAILURE.
#####MakeResourcePoolNodes#####
sub MakeResourcePoolNodes
{
	my %args 		 	= @_;
	my $doc 		 	= $args{xmlDoc};
	my @resourcePoolInfo= @ { $args{resourcePoolInfo} };
	
	for ( my $i = 0 ; $i <= $#resourcePoolInfo ; $i++ )
	{
		my $resourcePool= $doc->createElement('resourcepool');
		$resourcePool->setAttribute( 'host' , $resourcePoolInfo[$i]->{inHost} );
		$resourcePool->setAttribute( 'name' , $resourcePoolInfo[$i]->{name} );
		$resourcePool->setAttribute( 'groupId' , $resourcePoolInfo[$i]->{groupName} );
		$resourcePool->setAttribute( 'type' , $resourcePoolInfo[$i]->{resPoolType} );
		$resourcePool->setAttribute( 'owner' , $resourcePoolInfo[$i]->{owner} );
		$resourcePool->setAttribute( 'owner_type' , $resourcePoolInfo[$i]->{ownerType} );
		$args{rootElement}->appendChild($resourcePool);
	} 
	
	return ( $Common_Utils::SUCCESS , $args{rootElement} );
}

#####MakeXML#####
##Description 		:: Makes the XML according to type. Types of XML are Source Target xml's.
##Input 			:: Type of XML.
##Output 			:: Returns SUCCESS else FAILURE.
#####MakeXML#####
sub MakeXML
{
	my %args 			= @_;
	my $filename 		= "$Common_Utils::vContinuumMetaFilesPath/Info.xml";
	my $element_name 	= "INFO";
	
	my @vmList			= @{ $args{vmList} };

	if( ! open(XML,">$filename"))
	{
		Common_Utils::WriteMessage(" ERROR :: Unable to create XML file \"$filename\".",3);
		Common_Utils::WriteMessage(" ERROR :: Please check for adequate permissions to create a file and do a re-protection.",3);
		return $Common_Utils::FAILURE;
	}
	
	my $doc 			= XML::LibXML::Document->new( "1.0", "UTF-8");
	my $ESX_NODE 		= $doc->createElement($element_name);
	$doc->setDocumentElement($ESX_NODE);
	$ESX_NODE->setAttribute("vCenter",$args{IsItvCenter});
	$ESX_NODE->setAttribute("version",$args{version});
	
	for ( my $i = 0 ; $i <= $#vmList ; $i++ )
	{
		my $host 		= $doc->createElement('host');
		$host->setAttribute( 'display_name', $vmList[$i]->{displayName} );
		$host->setAttribute( 'hostname' , $vmList[$i]->{hostName} );
		$host->setAttribute( 'ip_address' , $vmList[$i]->{ipAddress} );
		$host->setAttribute( 'vSpherehostname' , $vmList[$i]->{vSphereHostName} );
		$host->setAttribute( 'datacenter' , $vmList[$i]->{dataCenter} );
		$host->setAttribute( 'resourcepool',$vmList[$i]->{resourcePool} );
		$host->setAttribute( 'resourcepoolgrpname' , $vmList[$i]->{resourcePoolGrp} );
		$host->setAttribute( 'hostversion' , $vmList[$i]->{hostVersion} );
		
		if( (!(exists $vmList[$i]->{powerState})) || ($vmList[$i]->{powerState} ne "poweredOn" ))
		{
			$host->setAttribute( 'powered_status' , "OFF" );
		}
		else
		{
			$host->setAttribute( 'powered_status' , "ON" );
		}

		$host->setAttribute( 'vmx_path' , $vmList[$i]->{vmPathName} );
		$host->setAttribute( 'source_uuid' , $vmList[$i]->{uuid} );
		$host->setAttribute( 'source_vc_id', $vmList[$i]->{instanceUuid} );
		$host->setAttribute( 'memsize' , $vmList[$i]->{memorySizeMB} );
		$host->setAttribute( 'cpucount' , $vmList[$i]->{numCpu} );
		$host->setAttribute( 'vmx_version' , $vmList[$i]->{vmxVersion} );
		$host->setAttribute( 'vmwaretools' , $vmList[$i]->{toolsStatus} );
		$host->setAttribute( 'ide_count' , $vmList[$i]->{ideCount} );
		$host->setAttribute( 'alt_guest_name' , $vmList[$i]->{guestId} );
		$host->setAttribute( 'floppy_device_count' , $vmList[$i]->{floppyDevices} );	
		$host->setAttribute( 'folder_path' , $vmList[$i]->{folderPath} );
		$host->setAttribute( 'datastore', $vmList[$i]->{datastore} );
		$host->setAttribute( 'os_info' , $args{osType} );
		$host->setAttribute( 'operatingsystem' , $vmList[$i]->{operatingSystem} );
		$host->setAttribute( 'rdm' , $vmList[$i]->{rdm} );
		$host->setAttribute( 'number_of_disks' , $vmList[$i]->{numVirtualDisks} );
		$host->setAttribute( 'cluster',$vmList[$i]->{cluster} );
		$host->setAttribute( 'efi', $vmList[$i]->{efi} );
		$host->setAttribute( 'diskenableuuid', $vmList[$i]->{diskEnableUuid} );
		$host->setAttribute( 'vm_console_url', $vmList[$i]->{vmConsoleUrl} );
		$host->setAttribute( 'template', $vmList[$i]->{template} );
		
		( my $returnCode , $host ) = MakeDiskNode( xmlDoc => $doc , hostNode => $host , diskInfo => $vmList[$i]->{diskInfo} );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to make disk node for machine \"$vmList[$i]->{displayName}\".",3);
			return $Common_Utils::FAILURE;
		}
					
		( $returnCode , $host ) = MakeNicNode( xmlDoc => $doc , hostNode => $host , networkInfo => $vmList[$i]->{networkInfo} );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to make netwrok node for machine \"$vmList[$i]->{displayName}\".",3);
			return $Common_Utils::FAILURE;
		}
		
		$ESX_NODE->appendChild($host);
	}
		
	( my $returnCode , $ESX_NODE ) = MakeDataStoreNode( xmlDoc => $doc , rootElement => $ESX_NODE , datastoreInfo => $args{datastoreInfo} );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to list datastore information.",3);
		return $Common_Utils::FAILURE;
	}
	
	( $returnCode , $ESX_NODE ) = MakeNetworkNodes( xmlDoc => $doc , rootElement => $ESX_NODE , networkInfo => $args{networkInfo} , isItvCenter => $args{IsItvCenter} );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to list network information.",3);
		return $Common_Utils::FAILURE;
	}
	
	if ( "" ne $args{scsiDiskInfo} )
	{
		( $returnCode , $ESX_NODE ) = MakeScsiDiskNodes( xmlDoc => $doc , rootElement => $ESX_NODE , scsiDiskInfo => $args{scsiDiskInfo} );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to list SCSI device information.",3);
			return $Common_Utils::FAILURE;
		}
	}
	
	( $returnCode , $ESX_NODE ) = MakeResourcePoolNodes ( xmlDoc => $doc , rootElement => $ESX_NODE , resourcePoolInfo => $args{resourcePoolInfo} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to list vSphere Configuration information.",3);
		return $Common_Utils::FAILURE;
	}
	
	( $returnCode , $ESX_NODE ) = MakeConfigNodes( xmlDoc => $doc , rootElement => $ESX_NODE , configInfo => $args{configInfo} );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to list vSphere Configuration information.",3);
		return $Common_Utils::FAILURE;
	}		

	
	print XML $doc->toString(1);
	close XML;
	
	Common_Utils::SetFilePermissions($filename);
	
	return $Common_Utils::SUCCESS;
}

#####ReadTargetXML#####
##Description 			:: It reads Target ESX node.
##Input 				:: Name of the File to read for Target ESX node.
##Output				:: Returns SUCCESS and Target VM object else FAILURE
#####ReadTargetXML#####
sub ReadTargetXML
{
	my $fileName 			= shift;
	my $parser 				= XML::LibXML->new();
	my $tree 				= $parser->parse_file($fileName);
	my $root 				= $tree->getDocumentElement;
	my @targetESXNodes		= $root->getElementsByTagName('TARGET_ESX');
	my @masterTargetsInfo	= ();	
	$Common_Utils::vContinuumPlanDir = $root->getAttribute('xmlDirectoryName');
	foreach my $targetEsxNode ( @targetESXNodes )
	{
		my @targetEsxHosts	= $targetEsxNode->getElementsByTagName('host');
		foreach my $targetESX ( @targetEsxHosts )
		{
			my %masterTargetInfo 	= ();
			if ( "" eq $targetESX->getAttribute('display_name') )
			{
				Common_Utils::WriteMessage("ERROR :: Found empty value for master target display name in xml \"$fileName\".",3);
				return ( $Common_Utils::FAILURE );
			}
			$masterTargetInfo{displayName}		= $targetESX->getAttribute('display_name');
			
			if ( "" eq $targetESX->getAttribute('hostname') )
			{
				Common_Utils::WriteMessage("ERROR :: Found empty value for master target host name in xml \"$fileName\".",3);
				return ( $Common_Utils::FAILURE );
			}
			$masterTargetInfo{hostName}			= $targetESX->getAttribute('hostname');
			
			if ( "" eq $targetESX->getAttribute('source_uuid') )
			{
				Common_Utils::WriteMessage("ERROR :: Found empty value for master target uuid in xml \"$fileName\".",3);
				return ( $Common_Utils::FAILURE );
			}
			$masterTargetInfo{uuid}			= $targetESX->getAttribute('source_uuid');
			
			if( "" eq $targetESX->getAttribute('vSpherehostname') )
			{
				Common_Utils::WriteMessage("ERROR :: Found empty value for target vSphere hostname in xml \"$fileName\".",3);
				return ( $Common_Utils::FAILURE );
			}
			$masterTargetInfo{vSphereHostName}	= $targetESX->getAttribute('vSpherehostname');
			
			if ( "" eq $targetESX->getAttribute('hostversion') )
			{
				Common_Utils::WriteMessage("ERROR :: Found empty value for target vSphere host version in xml \"$fileName\".",3);
				return ( $Common_Utils::FAILURE );
			}
			$masterTargetInfo{hostVersion}		= $targetESX->getAttribute('hostversion');
			
			if ( "" eq $targetESX->getAttribute('datacenter') )
			{
				Common_Utils::WriteMessage("ERROR :: Found empty value for master target datacenter name in xml \"$fileName\".",3);
				return ( $Common_Utils::FAILURE );
			}
			$masterTargetInfo{dataCenter}		= $targetESX->getAttribute('datacenter');
			
			if ( "" eq $targetESX->getAttribute('operatingsystem') )
			{
				Common_Utils::WriteMessage("ERROR :: Found empty value for master target OS type in xml \"$fileName\".",3);
				return ( $Common_Utils::FAILURE );
			}
			$masterTargetInfo{operatingSystem}	= $targetESX->getAttribute('operatingsystem');
	
			push @masterTargetsInfo , \%masterTargetInfo;
		}
	}
	return ( $Common_Utils::SUCCESS , \@masterTargetsInfo );
}

#####UpdateDRMachineConfigToXmlNode#####
##Description		:: Updates Configuration like New Mac Address , Disk uuids, SCSI on master target.
##						1. Above values are taken from machines created/existing at DR site of the operation.
##Input 			:: VM view , MT view and SourceHost node ( XML::ELEMENT ).
##Output 			:: Returns SUCCESS after updating values else Failure.
#####UpdateDRMachineConfigToXmlNode#####
sub UpdateDRMachineConfigToXmlNode
{
	my %args 		= @_;
	
	my $diskSignMap	= $args{diskSignMap};
	
	my @Nic 		= $args{sourceHost}->getElementsByTagName('nic');
	foreach my $Nic( @Nic )
	{
		my $devices 	= Common_Utils::GetViewProperty( $args{vmView}, 'config.hardware.device');
		foreach my $device (@$devices)
		{
			if( ( $device->deviceInfo->label !~ /^Network adapter /i ) || ( $device->deviceInfo->label ne $Nic->getAttribute('network_label') ) )
			{
				next;
			}
			my $newMacAddress 	= $device->macAddress;
			my $oldMacAddress 	= $Nic->getAttribute('nic_mac');
			Common_Utils::WriteMessage("Old MAC = $oldMacAddress , New MAC = $newMacAddress.",2);
			if ( $oldMacAddress eq $newMacAddress )
			{
				next;
			}
			$Nic->setAttribute('new_macaddress' , $newMacAddress);
		}
	}
	
	my @disks	= $args{sourceHost}->getElementsByTagName('disk');
	my %scsiIdExist	= ();
	foreach my $disk ( @disks )
	{
		my $fileName 		= $disk->getAttribute('disk_name');
		my @diskPath		= split( /\[|\]/ , $fileName );
		$diskPath[-1]		=~ s/^\s+//;
		if ( ( defined( $args{sourceHost}->getAttribute('vmDirectoryPath') ) ) && ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
		{
			$diskPath[-1]	= $args{sourceHost}->getAttribute('vmDirectoryPath')."/$diskPath[-1]";
		}
		$fileName			= "[".$disk->getAttribute('datastore_selected')."] $diskPath[-1]";
		
		my $vSanFileName	= "";
		if ( ( defined( $args{sourceHost}->getAttribute('vsan_folder') ) ) && ( "" ne $args{sourceHost}->getAttribute('vsan_folder') ) )
		{
			@diskPath		= split( /\// , $fileName );
			$vSanFileName	= "[".$disk->getAttribute('datastore_selected')."] " . $args{sourceHost}->getAttribute('vsan_folder')."$diskPath[-1]";
		}
		
		if ( ( defined( $disk->getAttribute('target_disk_name') ) ) && ( "" ne $disk->getAttribute('target_disk_name') ) )
		{
			$fileName	= $disk->getAttribute('target_disk_name');
		}
		
		my $diskUuid	= $disk->getAttribute('source_disk_uuid');
				
		my $devices 		= Common_Utils::GetViewProperty( $args{mtView}, 'config.hardware.device');
		foreach my $device ( @$devices )
		{
			if ( $device->deviceInfo->label !~ /Hard Disk/i )
			{
				next;
			}
			my $diskName 	= $device->backing->fileName;
			my $scsi		= "";
			
			if( ( $device->controllerKey >= 1000 ) && ( $device->controllerKey <= 1003 ) )
			{
				$scsi		= ( ( $device->controllerKey ) % 1000 ).":".( ( $device->key - 2000 ) % 16 );
			}
			elsif( ( $device->controllerKey >= 15000 ) && ( $device->controllerKey <= 15003 ) )
			{
				$scsi		= ( ( ( $device->controllerKey ) % 15000 ) + 4 ).":".( ( $device->key - 16000 ) % 30 );
			}
			
			if( ( "resume" eq $args{operation} ) || ( "failback" eq $args{operation} ) )
			{
				my $diskUuidTgt	= $disk->getAttribute('target_disk_uuid');
				my $vmdkUuid	= $device->backing->uuid;
				if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
				{
					$vmdkUuid	= $device->backing->lunUuid;
				}
				if( $vmdkUuid eq $diskUuidTgt )
				{
					$scsiIdExist{$scsi} = "Exist";
					$disk->setAttribute('scsi_id_onmastertarget' , $scsi);
					
					if( $diskUuid ne "" )
					{
						$$diskSignMap{$diskUuid}{target_disk_uuid}			= $vmdkUuid;
						$$diskSignMap{$diskUuid}{scsi_id_onmastertarget}	= $scsi;
					}
			
					Common_Utils::WriteMessage("FileName = $diskName , target disk uuid = $diskUuid , SCSI id on master target =$scsi.",2);
					last;
				}
			}
			
			if ( ( $fileName ne $diskName ) && ( $vSanFileName ne $diskName ) )
			{
				if( ( "failback" ne $args{operation} ) )
				{
					next;
				}
				my @fileName= split( /\[|\]|\//, $fileName );
				my @diskName= split( /\[|\]|\//, $diskName );
				
				if( ( $fileName[-1] =~ /_InMage_NC_/ ) && ( $diskName[-1] !~ /_InMage_NC_/ ) )
				{
					if( defined $scsiIdExist{$scsi} )
					{
						next;
					}
					my $rDot		= rindex( $diskName[-1] , "." );
					my $tempName	= substr( $diskName[-1] , 0 , $rDot );
					my @splitContr	= split( ":", $disk->getAttribute('scsi_mapping_vmx') );
					my $disk_number =  2000 + ( $splitContr[0] * 16 ) + $splitContr[1];
					if( $splitContr[0] >= 4)
					{
						$disk_number =  16000 + (($splitContr[0] - 4) * 30) + $splitContr[1];
					}
					
					$diskName[-1]	= $tempName."_InMage_NC_$disk_number.vmdk";
					if ( $fileName[-1] ne $diskName[-1] )
					{
						next;
					}			
				}
				elsif( ( $fileName[-1] =~ /_InMage_NC_/ ) && ( $diskName[-1] =~ /_InMage_NC_/ ) )
				{
					if( defined $scsiIdExist{$scsi} )
					{
						next;
					}
					my $startIndex	= index( $diskName[-1] , "_InMage_NC_" );
					my $tempDiskName= substr( $diskName[-1] , 0 , $startIndex );
					$startIndex		= index( $fileName[-1] , "_InMage_NC_" );
					my $tempFileName= substr( $fileName[-1] , 0 , $startIndex );
					if ( $tempFileName ne $tempDiskName )
					{
						next;
					}			
				}
				else
				{
					Common_Utils::WriteMessage("Failback vmdk comparision: FileName =$fileName, diskName =$diskName",2);
					next;
				}
			}
						
			$scsiIdExist{$scsi} = "Exist";
			$disk->setAttribute('scsi_id_onmastertarget' , $scsi);
			my $targetDiskUuid	= "";
			if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
			{
				$targetDiskUuid	= $device->backing->lunUuid;
				$disk->setAttribute('target_disk_uuid' , $device->backing->lunUuid );
			}
			else
			{
				$targetDiskUuid	= $device->backing->uuid;
				$disk->setAttribute('target_disk_uuid' , $device->backing->uuid );
			}
			
			if( $diskUuid ne "" )
			{
				$$diskSignMap{$diskUuid}{target_disk_uuid}			= $targetDiskUuid;
				$$diskSignMap{$diskUuid}{scsi_id_onmastertarget}	= $scsi;
			}
					
			Common_Utils::WriteMessage("FileName = $diskName , SCSI id on master target =$scsi.",2);
			last;		
		}
	} 
	
	return ( $Common_Utils::SUCCESS );
}

#####ConstructXmlForCxApi#####
##Description 		:: It constructs XML for vContinuum CX API. It will use keys either corresponding to InMage/vContinuum project.
##Input 			:: Host ID , Function Name , Information required from a function.
##Output 			:: Returns SUCCESS and Request XML else FAILURE.
#####ConstructXmlForCxApi#####
sub ConstructXmlForCxApi
{
	my %args			= @_;
	my $version 		= "1.0";
	my $FunctionName	= $args{functionName};
	my $accessFile 		= "/ScoutAPI/CXAPI.php";
	my $requestMethod 	= "POST";
	my $RequestID		= substr( "ts:". Time::Local::timelocal( localtime() ) . "-" . Common_Utils::generateUuid() , 0, 32 );
		
	my $parameterStr= PrepareXmlFromHash( hashRef => $args{subParameters} );	
	
	if( $Common_Utils::HostId eq "")
	{		
		Common_Utils::WriteMessage("ERROR :: value of vContinuum HostId is empty.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my ( $returnCode, $AccessSignature )= Common_Utils::GenerateAccessSignature( 	requestMethod	=> $requestMethod,
																					accessFile		=> $accessFile,
																					functionName	=> $FunctionName,
																					requestID		=> $RequestID,
																					version			=> $version,
																				);
	if( $returnCode == $Common_Utils::FAILURE )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to generate Access signature.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my $xmlHeader	= "<Header>
	   <Authentication>
	  	<AuthMethod>ComponentAuth_V2015_01</AuthMethod>
	    <AccessKeyID>$Common_Utils::HostId</AccessKeyID>
	    <AccessSignature>$AccessSignature</AccessSignature>
	   </Authentication>
	</Header>";
	
	my $xmlBody		= "<Body>
	  <FunctionRequest Name=\"$args{functionName}\" Include=\"Yes\">
			 	 $parameterStr    
		</FunctionRequest>
	</Body>";
	
	my $RequestXml = "<Request Id=\"$RequestID\" Version=\"$version\"> 
	$xmlHeader
	$xmlBody
	</Request>";
	
	Common_Utils::WriteMessage("Request XML = $xmlBody.",2);
	
	return ( $Common_Utils::SUCCESS , $RequestXml  );
}

#####ReadResponseOfCxApi#####
##Decription 		:: It reads Response XML which is the output of Request XML sent.See if we could read what we require.
##Input 			:: Response XML from vContinuum CX API.
##Output 			:: Returns SUCCESS if the Response XML contains data we need and there is no error in executing query. Else it reports 
##						Failure and corresponding error message will be displayed.
#####ReadResponseOfCxApi#####
sub ReadResponseOfCxApi
{
	my %args		= @_;
	my $ReturnCode 	= $Common_Utils::SUCCESS;
	my %hash		= ();
	##better to write in between eval();
	my $parser 		= XML::LibXML->new();
	my $xml 		= Encode::encode_utf8( $args{response} );
	my $xmlDoc 		= $parser->parse_string( $xml );
	my $responseInfo= $xmlDoc->getDocumentElement;
	
	unless( defined $args{printXml} )
	{
		Common_Utils::WriteMessage(" $xml.",2);	
	}
		
	if ( ( defined $responseInfo->getAttribute('Returncode') ) && ( 0 != $responseInfo->getAttribute('Returncode') ) )
	{
		my $errorMsg= $responseInfo->getAttribute('Message');
		Common_Utils::WriteMessage("ERROR :: $errorMsg.",3);
		$ReturnCode	= $Common_Utils::FAILURE;
		return ( $ReturnCode ); 
	}
	
	my @body 		= $responseInfo->getElementsByTagName('Body');
	foreach my $body ( @body )
	{
		my @functions 	= $body->getElementsByTagName( 'Function' );
		foreach my $function ( @functions  )
		{
			if ( ( $function->hasAttribute('Returncode') ) && ( 0 != $function->getAttribute('Returncode') ) )
			{
				my $errorMsg = $function->getAttribute('Message');
				Common_Utils::WriteMessage("ERROR :: $errorMsg.",3);
				$ReturnCode	 = $Common_Utils::FAILURE;
				return ( $ReturnCode ); 
			}
			
			my @functionResponse = $function->getElementsByTagName('FunctionResponse');
			foreach my $response ( @functionResponse )
			{
				my @nodes 	= $response->childNodes;
				foreach my $node ( @nodes )
				{
					my $nodeName = $node->nodeName;
					if ( ( "Parameter" eq $nodeName ) && ( "yes" eq lc( $args{parameter} ) ) )
					{
						$hash{$node->getAttribute('Name')} 	= $node->getAttribute('Value');
					}
					elsif( "ParameterGroup" eq $nodeName )
					{
						if ( ( "all" ne lc( $args{parameterGroup} ) ) && ( lc( $node->getAttribute('Id') ) ne lc( $args{parameterGroup} ) ) )
						{
							next;
						}
						my $normalizedInfo = NormalizeNode( node => $node );
					  	$hash{ $node->getAttribute('Id') } 	= $normalizedInfo;
					}										
				}
			}
		}
	}
	return ( $ReturnCode , \%hash );
}

#####NormalizeNode#####
##Description	:: It contructs a hash from an XML::Node::Element and it is recursive.
##Input 		:: XML::Node.
##Output 		:: Returns a HASH reference.
#####NormalizeNode#####
sub NormalizeNode
{
	my %args	= @_;
	my %hash	= ();
	
	my @nodes 	= $args{node}->childNodes;
	foreach my $node ( @nodes )
	{
		my $nodeName = $node->nodeName;
		if ( "Parameter" eq $nodeName )
		{
			$hash{$node->getAttribute('Name')} 	= $node->getAttribute('Value');
		}
		elsif( "ParameterGroup" eq $nodeName )
		{
			my $normalizedInfo = NormalizeNode( node => $node );
			$hash{ $node->getAttribute('Id') } 	= $normalizedInfo;
		}										
	}
	return ( \%hash );
}

#####NormalizeHash#####
##Description	:: It contructs an XML Structure from HASH and it is recursive.
##Input 		:: HASH.
##Output 		:: Returns XML STRING.
#####NormalizeHash#####
sub NormalizeHash
{
	my %args 		= @_;
	my $xmlString 	= "";
	
	foreach my $parameter ( sort keys %{$args{hashRef}} )
	{
		if ( "hash" eq lc( ref( $args{hashRef}->{$parameter} ) ) )
		{
			$xmlString .= "<ParameterGroup Id=\"$parameter\">"."\n";
			my $xmlSubHash = NormalizeHash( hashRef => $args{hashRef}->{$parameter} );
			$xmlString .= $xmlSubHash."</ParameterGroup>"."\n";
		}
		else
		{
			$xmlString .= "<Parameter Name=\"$parameter\" Value=\"$args{hashRef}->{$parameter}\" />"."\n";
		}    
	}
	return ( $xmlString );
}

#####PrepareXmlFromHash#####
##Description	:: Prepares XML from HASH. This XML is compatible with what Inmage CX API is using.
##Input 		:: Hash which contains data. Even a Null Hash could be passed from which a NULL STRING is made.
##Output 		:: Return String which is form of XML on SUCCESS else FAILURE.
#####PrepareXmlFromHash#####
sub PrepareXmlFromHash
{
	my %args		= @_;
	my $xmlString 	= "";
	
	foreach my $parameter ( sort keys %{$args{hashRef}} )
	{
		if ( "hash" eq lc( ref( $args{hashRef}->{$parameter} ) ) )
		{
			$xmlString .= "<ParameterGroup Id=\"$parameter\">"."\n";
			my $xmlSubHash = NormalizeHash( hashRef => $args{hashRef}->{$parameter} );
			$xmlString .= $xmlSubHash."</ParameterGroup>"."\n";
		}
		else
		{
			$xmlString .= "<Parameter Name=\"$parameter\" Value=\"$args{hashRef}->{$parameter}\" />"."\n";
		}    
	}
	$xmlString =~ s/\s+$//;
	return ( $xmlString );
}

#####UpdateTaskStatus#####
##Description	:: Updates the Task status. Right now it updates the Task status for recovery/dr-drill, the same could be used for protection/resume/failback.
##					The same function could be used for updating the status of a task if same task structure followed.
##Input 		:: Plan ID , Host ID(with which we need to call updateStatus API()) , CX IP along with Port Number.
##Output 		:: Returns SUCCESS on successful updation of status to CX, else FAILURE.  					
#####UpdateTaskStatus#####
sub UpdateTaskStatus
{
	my %args 		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	Common_Utils::WriteMessage("UpdateTaskStatus : Plan ID = $args{planId}, host Id = $args{hostId}, cx ip = $args{cxIp}.",2);
	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{
		my %subParameters				 = ( PlanId => $args{planId}, HostGUID => $args{hostId} );
		my( $returnCode , $requestXML )  = ConstructXmlForCxApi( functionName =>"MonitorESXProtectionStatus" , subParameters => \%subParameters );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to construct XML file for querying status of plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode , my $responseXml ) = Common_Utils::PostRequestXml( cxIp => $args{cxIp} , requestXml => $requestXML, functionName => "MonitorESXProtectionStatus" );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to post XML file for querying status of plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode , my $hashedData )  = ReadResponseOfCxApi( response => $responseXml , parameter =>"yes" , parameterGroup => 'all' );		
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to query status of plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		foreach my $taskInfo( @{$args{tasksUpdates}} )
		{
			Common_Utils::WriteMessage("Updating information of task Name : $$taskInfo{TaskName}.",2);
			
			( $returnCode ,my $stepWiseTaskInfo ) = Common_Utils::FindTask( stepsInfo => $hashedData , taskInfo => $taskInfo );
			if ( $Common_Utils::SUCCESS != $returnCode )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to find Step,TaskNumber for task $$taskInfo{TaskName}.",3);
				$ReturnCode = $Common_Utils::FAILURE;
				last;				
			}
												
			( $returnCode , $hashedData )	  = Common_Utils::UpdateTasksStatus( origTasksStatus => $hashedData , updatedTasksStatus => $stepWiseTaskInfo );
			if ( $Common_Utils::SUCCESS != $returnCode )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to update status of tasks for plan with id \"$args{planId}\".",3);
				$ReturnCode = $Common_Utils::FAILURE;
				last;
			} 
		}
		
		$$hashedData{ HostGUID }	= $args{hostId};
		( $returnCode , $requestXML ) = ConstructXmlForCxApi( functionName =>"UpdateESXProtectionDetails" , subParameters => $hashedData );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to construct XML file for updating status of plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode , $responseXml ) = Common_Utils::PostRequestXml( cxIp => $args{cxIp} , requestXml => $requestXML, functionName => "UpdateESXProtectionDetails" );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to post XML file for querying status of plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode , $hashedData )	= ReadResponseOfCxApi( response => $responseXml , parameter =>"no" , parameterGroup => 'no' );		
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update status of plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
	}
	
	return ( $ReturnCode );
}

#####RemovePlanUsingCxApi#####
##Description 			:: It Removes the VX and FX jobs from CX.
##Input 				:: Hash reference for VX and FX jobs to remove.
##Output				:: Returns Success or Failure.
#####RemovePlanUsingCxApi#####
sub RemovePlanUsingCxApi
{
	my %args 		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{
		my( $returnCode , $requestXML ) = ConstructXmlForCxApi( functionName =>"RemoveProtection" , subParameters => $args{subParameters} );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to construct XML file for removing plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		if ( "" eq $Common_Utils::CxIpPortNumber )
		{
			my( $returnCode , $cxIpPortNum )= Common_Utils::FindCXIP();
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
				$ReturnCode = $Common_Utils::FAILURE;
				last;
			}
		}
		
		( $returnCode ,my $responseXml ) = Common_Utils::PostRequestXml( cxIp => $Common_Utils::CxIpPortNumber , requestXml => $requestXML, functionName => "RemoveProtection" );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to post XML file for removing plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode ,my $hashedData )	= ReadResponseOfCxApi( response => $responseXml , parameter =>"no" , parameterGroup => 'no' );		
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to remove plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
	};
	
	return ( $ReturnCode );
}

#####ReadNetworkDataFromXML#####
##Description 			:: It reads network information from xml.
##Input 				:: Target ESX node from xml file.
##Output				:: Returns network information.
#####ReadNetworkDataFromXML#####
sub ReadNetworkDataFromXML
{
	my $targetHostNode		= shift;
	my @networkInfo			= ();
	my @targetNetworkNodes	= $targetHostNode->getElementsByTagName('network');
	foreach my $targetNwNode ( @targetNetworkNodes )
	{
		my %portGroupInfo	= ();		
		$portGroupInfo{vSphereHostName}	= $targetNwNode->getAttribute('vSpherehostname');
		$portGroupInfo{portgroupName}	= $targetNwNode->getAttribute('name');
		$portGroupInfo{switchUuid}		= $targetNwNode->getAttribute('switchUuid');
		$portGroupInfo{portgroupKey}	= $targetNwNode->getAttribute('portgroupKey');
		push @networkInfo,\%portGroupInfo;
	}
	return ( \@networkInfo );	
}

#####UpdateTargetDatastoreName#####
##Description 			:: It updates the new datastore name in xml file.
##Input 				:: Host node, old datastore name and new datastore name.
##Output				:: None.
#####UpdateTargetDatastoreName#####
sub UpdateTargetDatastoreName
{
	my $hostNode	= shift;
	my $oldDsName	= shift;
	my $newDsName	= shift;
	
	my @disks 		= $hostNode->getElementsByTagName('disk');
	foreach my $disk ( @disks )
	{
		if( $oldDsName eq lc($disk->getAttribute('datastore_selected')) )
		{
			$disk->setAttribute('datastore_selected' , $newDsName );
		}
	}	
}

#####ReadDatastoreInfoFromXML#####
##Description 			:: It reads datastore information from xml.
##Input 				:: Target ESX node from xml file.
##Output				:: Returns datastore information.
#####ReadDatastoreInfoFromXML#####
sub ReadDatastoreInfoFromXML
{
	my $targetEsxNode		= shift;
	my @datastoresInfo		= ();
	my @targetDatastoreNodes	= $targetEsxNode->getElementsByTagName('datastore');
	foreach my $targetDsNode ( @targetDatastoreNodes )
	{
		my %datastoreInfo	= ();
				
		$datastoreInfo{vSphereHostName}	= $targetDsNode->getAttribute('vSpherehostname');
		$datastoreInfo{datastoreName}	= $targetDsNode->getAttribute('datastore_name');
		$datastoreInfo{uuid}			= $targetDsNode->getAttribute('uuid');
		$datastoreInfo{diskName}		= $targetDsNode->getAttribute('disk_name');
		$datastoreInfo{capacity}		= $targetDsNode->getAttribute('total_size');
		$datastoreInfo{freeSpace}		= $targetDsNode->getAttribute('free_space');
		$datastoreInfo{blockSize}		= $targetDsNode->getAttribute('datastore_blocksize_mb');
		$datastoreInfo{fileSystem}		= $targetDsNode->getAttribute('filesystem_version');
		$datastoreInfo{displayName}		= $targetDsNode->getAttribute('display_name');
		$datastoreInfo{vendor}			= $targetDsNode->getAttribute('vendor');
		$datastoreInfo{lunNumber}		= $targetDsNode->getAttribute('lun');		
		push @datastoresInfo,\%datastoreInfo;
	}
	return ( \@datastoresInfo );	
}

#####DummyDiskCleanUpTask#####
##Description 			:: Removing dummy disk stale entries in multipath.
##Input 				:: MT host id and cx ip.
##Output				:: Returns Success or Failure.
#####DummyDiskCleanUpTask#####
sub DummyDiskCleanUpTask
{
	my %args 		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	my $requestId	= "";
	
	my @nodesInCx		= ();
	my ( $returnCode, $nodesInCx)	= Common_Utils::GetHostNodesInCX( $args{cxIpPortNum}, \@nodesInCx );
	if( $Common_Utils::SUCCESS != $returnCode)
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get registered agents information from CX.",2);
		return $Common_Utils::FAILURE;
	}
	
	my $vxPath	= "";
	foreach my $cxNode ( @$nodesInCx )
	{
		if( $$cxNode{hostId} eq $args{hostId})
		{
			$vxPath	= $$cxNode{vxPath};
			last;
		}
	}
	
	my @dummyDiskUuids	= ();
	my @dummyDisksInfo	= $args{hostNode}->getElementsByTagName('dummy_disk');
	if( -1 != $#dummyDisksInfo)
	{
		my @dummy_disk_nodes =	$dummyDisksInfo[0]->getElementsByTagName('MT_Disk');
		foreach (@dummy_disk_nodes)
		{
			my $dummyDiskUuid	= $_->getAttribute('disk_uuid');
			$dummyDiskUuid	=~ s/-//g;
			$dummyDiskUuid	= "3" . $dummyDiskUuid;
			push @dummyDiskUuids, $dummyDiskUuid;
		}
	}
	else
	{
		Common_Utils::WriteMessage("ERROR :: Dummy disks node not found in xml file.",2);
		return $Common_Utils::FAILURE;
	}
	
	my $dummyDiskUuids	= lc( join( " ", @dummyDiskUuids) );
	my $scriptToRun		= $vxPath . "/scripts/vCon/linux_script.sh" . " $dummyDiskUuids";
	my %subParameters	= ( ScriptPath => $scriptToRun, HostID => $args{hostId} );
	Common_Utils::WriteMessage("Dummy disks cleanup script:\"$scriptToRun\".",2);	
		
	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{
		( $returnCode , my $requestXML ) = ConstructXmlForCxApi( functionName =>"InsertScriptPolicy" , subParameters => \%subParameters );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to construct XML file for InsertScriptPolicy plan with id \"$args{hostId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode ,my $responseXml ) = Common_Utils::PostRequestXml( cxIp => $args{cxIpPortNum} , requestXml => $requestXML, functionName => "InsertScriptPolicy" );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to post XML file for removing plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode ,my $hashedData )	= ReadResponseOfCxApi( response => $responseXml , parameter =>"yes" , parameterGroup => 'no' );		
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to remove plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		$requestId	= $$hashedData{RequestId};
				
		for(my $i =1; $i < 5; $i++)
		{
			%subParameters	= ( RequestIdList => { RequestId => $requestId } );
			( $returnCode , $requestXML ) = ConstructXmlForCxApi( functionName =>"GetRequestStatus" , subParameters => \%subParameters );
			if ( $Common_Utils::SUCCESS != $returnCode )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to construct XML file for InsertScriptPolicy plan with id \"$args{hostId}\".",3);
				$ReturnCode = $Common_Utils::FAILURE;
				last;
			}
			
			( $returnCode ,my $responseXml ) = Common_Utils::PostRequestXml( cxIp => $args{cxIpPortNum} , requestXml => $requestXML, functionName => "GetRequestStatus" );
			if ( $Common_Utils::SUCCESS != $returnCode )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to post XML file for removing plan with id \"$args{planId}\".",3);
				$ReturnCode = $Common_Utils::FAILURE;
				last;
			}
			
			( $returnCode , $hashedData )	= ReadResponseOfCxApi( response => $responseXml , parameter =>"yes" , parameterGroup => 'no' );		
			if ( $Common_Utils::SUCCESS != $returnCode )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to remove plan with id \"$args{planId}\".",3);
				$ReturnCode = $Common_Utils::FAILURE;
				last;
			}
			if(( $$hashedData{Status} !~ /Pending/i ) && ( $$hashedData{Status} !~ /InProgress/i ) )
			{
				last;
			}
			sleep(60);
		}
		Common_Utils::WriteMessage("Dummy disk cleanup script output: \"$$hashedData{ErrorMessage}\".",2);
	}
	
	return ( $ReturnCode );
}

#####UpdateStatusToCx#####
##Description	:: Updates the plan status to CX.
##Input 		:: Plan ID , Host ID , CX IP along with Port Number.
##Output 		:: Returns SUCCESS on successful updation of status to CX, else FAILURE.  					
#####UpdateStatusToCx#####
sub UpdateStatusToCx
{
	my %args 		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	Common_Utils::WriteMessage("UpdateStatusToCx : Plan ID = $args{planId}, host Id = $args{hostId}, cx ip = $args{cxIp}.",2);
	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{		
		my %hashedData		= ();
		foreach my $taskInfo( @{$args{tasksInfo}} )
		{
			Common_Utils::WriteMessage("Updating information of task Name : $$taskInfo{TaskName}.", 2);
			$hashedData{TargetHostId}	= $args{hostId};
			foreach my $taskKey ( keys %$taskInfo )
			{
				if( lc( ref( $$taskInfo{$taskKey} ) ) eq "hash" )
				{
					my $secondLvlHash	= $$taskInfo{$taskKey};
					my %sourceHash		= ();
				
					if( $taskKey eq $args{hostId} )
					{
						next;
					}
					my $status			= $$secondLvlHash{status};
					my $errorMessage	= $$secondLvlHash{error};
					if( defined $$taskInfo{$args{hostId}})
					{
						$status			= $$secondLvlHash{$args{hostId}}{status};
						$errorMessage	= $$secondLvlHash{$args{hostId}}{error};							
					}						
					$hashedData{$taskKey}	= { SourceHostId 	=> $taskKey,
											Status			=> $$secondLvlHash{status},
											ErrorMessage	=> $$secondLvlHash{error},
											ErrorStep		=> $$taskInfo{TaskName},
											};
				}
			}
		}
		
		my %subParameters	= ( PlanId => $args{planId}, PlanType => $args{planType}, RecoveryServers => \%hashedData );
				
		my( $returnCode, $requestXML ) = ConstructXmlForCxApi( functionName =>"UpdateStatusToCX", subParameters => \%subParameters );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to construct XML file for updating status to cx of plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode, my $responseXml ) = Common_Utils::PostRequestXml( cxIp => $args{cxIp}, requestXml => $requestXML, functionName => "UpdateStatusToCX", );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to post XML file for Updating status to CX of plan with id \"$args{planId}\".",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode, my $hashedData )	= ReadResponseOfCxApi( response => $responseXml, parameter =>"no", parameterGroup => 'no' );		
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update status of plan with id \"$args{planId}\".", 3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
	}
	
	return ( $ReturnCode );
}

#####SendAlertToCX#####
##Description	:: Updates the plan status to CX.
##Input 		:: Plan ID , Host ID , CX IP along with Port Number.
##Output 		:: Returns SUCCESS on successful updation of status to CX, else FAILURE.  					
#####SendAlertToCX#####
sub SendAlertToCX
{
	my %args 		= @_;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	if( $Common_Utils::CxIpPortNumber eq "")
	{
		my( $returnCode , $cxIpPortNum )= Common_Utils::FindCXIP();
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to post status of $args{operation} to CX.",3);
			return ( $Common_Utils::FAILURE );
		}
		$Common_Utils::CxIpPortNumber	= $cxIpPortNum;
	}
	
	my $agentFile	= "$args{directory}/failed_errormessage.rollback";
	unless ( -f $agentFile )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find failed_errormessage file in directory $args{directory}.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	if ( !open( AGENTFILE , $agentFile ) )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to open \"$agentFile\". please check whether user has permission to access it.",3);
		return ( $Common_Utils::FAILURE );
	} 
	
	my @lines 	= <AGENTFILE>;
	close AGENTFILE;
	my %failedAgentMsg	= ();
	foreach my $line ( @lines )
	{
		unless($line =~ /^$/)
		{
			$line =~ s/\n//;
			$line =~ s/\r//; 
			my @line = split("!@!@!",$line);
			Common_Utils::WriteMessage("$line",2);
			$failedAgentMsg{$line[0]}	= $line[1];
		}
	}
	
	Common_Utils::WriteMessage("SendAlertToCX : cx ip = $Common_Utils::CxIpPortNumber .",2);
	for ( my $i = 1 ; $i <= 1 ; $i++ )
	{		
		my %hashedData		= ();
		foreach my $taskInfo( @{$args{tasksInfo}} )
		{
			my $key						= 1;
			foreach my $taskKey ( keys %$taskInfo )
			{
				if( lc( ref( $$taskInfo{$taskKey} ) ) eq "hash" )
				{
					my $secondLvlHash	= $$taskInfo{$taskKey};
					my %sourceHash		= ();
				
					if( $taskKey eq $args{hostId} )
					{
						next;
					}
					my $status			= $$secondLvlHash{status};
					my $errorMessage	= "";#$$secondLvlHash{error};
					my $hostName		= $$secondLvlHash{hostName};
					
					my $severity	= "NOTICE";
					my $eventCode	= "VA0602";
					if( $status eq "Failed" )
					{
						$severity	= "ERROR";
						$eventCode	= "VE0702";
					}
					
					if( defined $$taskInfo{$args{hostId}})
					{
						$status			= $$secondLvlHash{$args{hostId}}{status};
						$errorMessage	.= $$secondLvlHash{$args{hostId}}{error};							
					}
					
					if( defined $failedAgentMsg{$taskKey} )
					{
						$severity	= "ERROR";
						$eventCode	= "VE0702";
						$errorMessage	.= $failedAgentMsg{$taskKey};
					}
					
					if( $severity eq "NOTICE" )
					{
						$errorMessage	= "Recovery successfully completed for host : $hostName";
					}
					else
					{
						$errorMessage	= "Recovery failed for host : $hostName";
					}
											
					$hashedData{$key}	= { EventCode	=> $eventCode,
											HostId 		=> $taskKey,
											Category	=> "VCON_ERROR",
											Severity	=> $severity,
											Message		=> $errorMessage,
											PlaceHolders=> { "HostName" => $hostName },
											};
					$key++;
				}
				if( $key == 1)
				{
					$hashedData{$key}	= { EventCode	=> "VE0702",
											HostId 		=> $args{hostId},
											Category	=> "VCON_ERROR",
											Severity	=> "ERROR",
											Message		=> "Recovery failed for host : $args{hostId}",
											PlaceHolders=> { "HostId" => $args{hostId} },
											};
				}
			}
		}
						
		my( $returnCode, $requestXML ) = ConstructXmlForCxApi( functionName =>"SendAlerts", subParameters => \%hashedData );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to construct XML file for sending alerts to CX .",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode, my $responseXml ) = Common_Utils::PostRequestXml( cxIp => $Common_Utils::CxIpPortNumber, requestXml => $requestXML, functionName => "SendAlerts" );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to post XML file for sending alerts to CX.",3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
		
		( $returnCode, my $responseData )	= ReadResponseOfCxApi( response => $responseXml, parameter =>"no", parameterGroup => 'no' );		
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to send the alerts to CX.", 3);
			$ReturnCode = $Common_Utils::FAILURE;
			last;
		}
#		$$hashedData{Status} need to validate
	}
	
	return ( $ReturnCode );
}

1;