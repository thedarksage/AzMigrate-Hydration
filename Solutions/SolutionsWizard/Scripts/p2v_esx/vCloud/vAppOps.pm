=head
	vApp operations
	1. Configure/Reconfigure vApps.
	2. Get/Set Configuration of vApp.
	3. vApp Network list.
=cut

package vAppOps;

use strict;
use warnings;
use CommonUtils;
use Switch;
use Data::Dumper;
use LWP::UserAgent;
use HTTP::Request::Common;

my $SUCCESS = 0;
my $FAILURE = 1;


#####GetListOfvAppsInVDC#####
##Description 		:: Fetches list of vApp information in a VDC.
##Input 			:: VDC name and it's HREF.
##Output 			:: Returns SUCCESS and vAPP information or a FAILURE message.
#####GetListOfvAppsInVDC##### 
sub GetListOfvAppsInVDC
{
	my %args 	= @_;
	
	#Collect vApp Information.
	my %map_vAppInfo = ();
	
	print  "$args{'vdcName'} = $args{'href_vdc'}.\n";
		
	my $url = $args{'href_vdc'};
	
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
	
	#todo.. we need to take care of version. which will be used.		
	my $response = $ua->get( $url , 'x-vcloud-authorization' => $args{"x-auth-code"}, 'Accept' => "application/*+xml;version=5.1" );
	
	my $xmlContent = $response->{_content};
	my $parser = XML::LibXML->new();
	my $xmlTree = $parser->parse_string( $xmlContent );
		
	my @vdcList = $xmlTree->getElementsByTagName('Vdc');
	
	foreach my $vdc ( @vdcList )
	{
		my @resourceEntities = $vdc->getChildrenByTagName('ResourceEntities');
		foreach my $resourceentity ( @resourceEntities )
		{
			my @resources 	= $resourceentity->getChildrenByTagName('ResourceEntity');
			foreach my $entity ( @resources )
			{
				if ( "application/vnd.vmware.vcloud.vApp+xml" eq $entity->getAttribute('type') )
				{
					print  $entity->getAttribute("name").",".$entity->getAttribute("href")."\n";
					$map_vAppInfo{$entity->getAttribute("name")}	= $entity->getAttribute("href");
				}
			}
		}
	}
	print  Dumper(\%map_vAppInfo);
	
	return ( $SUCCESS , \%map_vAppInfo );
}

#####GetVmInformationInvApp#####
##Description 		:: Collects Information of Virtual machine under a vApp.
##Input 			:: Name of vApp,Href to vApp.
##Output 			:: Returns VM information along with SUCCESS message or FAILURE.
#####GetVmInformationInvApp#####
sub GetVmInformationInvApp
{
	my %args 	= @_;
	
	my %map_vmsInfo	= ();
	
	print  "$args{'vAppName'} = $args{'href_vApp'}.\n";
	
	my $url = $args{'href_vApp'};
	
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
		
	my $response = $ua->get( $url , 'x-vcloud-authorization' => $args{'x-auth-code'}, 'Accept' => "application/*+xml;version=5.1" );
	
	my $xmlContent = $response->{_content};
	
	my $parser = XML::LibXML->new();
	my $xmlTree = $parser->parse_string( $xmlContent );
			
	my @vAppInfo = $xmlTree->getElementsByTagName('VApp');
	foreach my $vApp ( @vAppInfo )
	{
		print  "Name of vApp = ".$vApp->getAttribute('name').".\n";
		my @childrens 	= $vApp->getElementsByTagName('Children');
		my $i = 0;
		foreach my $child ( @childrens )
		{
			my @vmList 	= $child->getElementsByTagName('Vm');
			foreach my $vm ( @vmList )
			{
				$i = $i + 1;
				my %map_vmInfo	= ();
				print  "Name of virtual machine = ".$vm->getAttribute('name').".\n";
				$map_vmInfo{'display_name'} = $vm->getAttribute('name');
				$map_vmInfo{'uuid'}			= $vm->getAttribute('id');
				$map_vmInfo{'rdm'}			= "false";
				$map_vmInfo{'cluster'}		= "no";
				$map_vmInfo{'vapp_name'}	= $vApp->getAttribute('name');
				$map_vmInfo{'number_of_disks'} = 0;
				$map_vmInfo{'ide_count'}	= 0;
				$map_vmInfo{'floppy_device_count'} = 0;
				$map_vmInfo{'networkConfig'}	= ();
				$map_vmInfo{'diskConfig'}		= ();
				$map_vmInfo{'ip_address'}		= "";
							
				my @OsInformation = $vm->getChildrenByTagName("ovf:OperatingSystemSection");
				foreach my $info ( @OsInformation )
				{
					$map_vmInfo{'alt_guest_name'} = $info->getAttribute('vmw:osType');
					my @description 	= $info->getChildrenByTagName('ovf:Description');
					$map_vmInfo{'operatingsystem'} = $description[0]->getFirstChild->getData;
					$map_vmInfo{'powered_status'} = "ON"; #Return as Default This has to be corrected.
				}
					
				my @GuestInformation = $vm->getChildrenByTagName("GuestCustomizationSection");
				foreach my $guestInfo ( @GuestInformation )
				{
					my $computerName 	= ${$guestInfo->getChildrenByTagName('ComputerName')}[0]->getFirstChild->getData; 
					$map_vmInfo{hostname}=$computerName;
					$map_vmInfo{uuid}	= ${$guestInfo->getChildrenByTagName('VirtualMachineId')}[0]->getFirstChild->getData;
				}
				
				my @virtualHardwareSections = $vm->getChildrenByTagName("ovf:VirtualHardwareSection");
				foreach my $hardware ( @virtualHardwareSections )
				{
					my @list_networkInfo	= ();
					my @list_diskInfo		= ();
					my @systemInfo = $hardware->getChildrenByTagName("ovf:System");
					foreach my $sysinfo ( @systemInfo )
					{
						my @systemType = $sysinfo->getChildrenByTagName("vssd:VirtualSystemType");
						$map_vmInfo{vmx_version} = $systemType[0]->getFirstChild->getData;
					}
	
					my @hardwareInfo 	= $hardware->getChildrenByTagName("ovf:Item");
					foreach my $hardwareItem ( @hardwareInfo )
					{
						my $resourceType = ${$hardwareItem->getChildrenByTagName("rasd:ResourceType")}[0]->getFirstChild->getData;
						switch( $resourceType )
						{
							case 3 
							{
								$map_vmInfo{'cpucount'} = ${$hardwareItem->getChildrenByTagName("rasd:VirtualQuantity")}[0]->getFirstChild->getData;
							}
								
							case 4
							{
								$map_vmInfo{'memory'} = ${$hardwareItem->getChildrenByTagName("rasd:VirtualQuantity")}[0]->getFirstChild->getData;
							}
							case 10
							{
								my %netwrokCard 		= ();
								my %networkInfo 		= ();
								$networkInfo{'nic_mac'} = ${$hardwareItem->getChildrenByTagName("rasd:Address")}[0]->getFirstChild->getData;
								$networkInfo{'network_label'} = ${$hardwareItem->getChildrenByTagName("rasd:ElementName")}[0]->getFirstChild->getData;
								$networkInfo{'adapter_type'} = ${$hardwareItem->getChildrenByTagName("rasd:ResourceSubType")}[0]->getFirstChild->getData;
								$networkInfo{'network_name'} = ${$hardwareItem->getChildrenByTagName("rasd:Connection")}[0]->getFirstChild->getData;
								
								my @attList				= ${$hardwareItem->getChildrenByTagName("rasd:Connection")}[0]->attributes();
								foreach my $attribute ( @attList )
								{
									my @attributeName 	= split ( "=" , $attribute->toString(1));
									$networkInfo{'ip'}	= $attribute->getValue() if ( $attributeName[0] =~ /:ipAddress$/ );
									$networkInfo{'address_type'} = $attribute->getValue() if ( $attributeName[0] =~ /:ipAddressingMode$/ );
									$networkInfo{'primaryNetworkConnection'}	= $attribute->getValue() if ( $attributeName[0] =~ /:primaryNetworkConnection$/ );
								}						
								
								$networkInfo{'dhcp'}	= "";
								$networkInfo{'dnsip'}	= "";
								if ( defined ( $networkInfo{'ip'} ) )
								{
									if ( "" ne $map_vmInfo{'ip_address'} )
									{
										$map_vmInfo{'ip_address'}= "$map_vmInfo{'ip_address'},$networkInfo{'ip'}";
									}
									else
									{
										$map_vmInfo{'ip_address'}= "$networkInfo{'ip'}";
									}
								}	
								$netwrokCard{ $networkInfo{'network_label'} } = \%networkInfo;
								$map_vmInfo{'number_of_disks'} = $map_vmInfo{'number_of_disks'} + 1;
								push @list_networkInfo , \%netwrokCard;
							}
							case 14
							{
								$map_vmInfo{'floppy_device_count'} = $map_vmInfo{'floppy_device_count'} + 1;
							}
							case 15
							{
								$map_vmInfo{'ide_count'}	= $map_vmInfo{'ide_count'} + 1;
							}
							case 17
							{
								my %disks		= ();
								my %diskInfo	= ();
								$diskInfo{'disk_name'} = ${$hardwareItem->getChildrenByTagName("rasd:ElementName")}[0]->getFirstChild->getData;								
								$diskInfo{'disk_mode'}	= "Virtual Disk";
								$diskInfo{'cluster_disk'} = "no";
								$diskInfo{'ide_or_scsi'} = "scsi";
								$diskInfo{'independent_persistent'} = "persistent";
								my @attList			= ${$hardwareItem->getChildrenByTagName("rasd:HostResource")}[0]->attributes();
								foreach my $attribute ( @attList )
								{
									my @attributeName 	= split ( "=" , $attribute->toString(1));
									$diskInfo{'size'}	= ( $attribute->getValue() * 1024 ) if ( $attributeName[0] =~ /:capacity$/ );
									$diskInfo{'controller_type'} = $attribute->getValue() if ( $attributeName[0] =~ /:busSubType$/ );
									$diskInfo{'href_disk'}	= "";
									$diskInfo{'href_disk'}	= $attribute->getValue() if ( $attributeName[0] =~ /:disk$/ );
								}
								$diskInfo{'controller_mode'}	= "noSharing";
								my $instanceID		= ${$hardwareItem->getChildrenByTagName("rasd:InstanceID")}[0]->getFirstChild->getData;
								my $scsi_busNumber 	= $instanceID%2000;
								my $scsi_unitNumber	= ($instanceID - 2000 )%16;
								$diskInfo{'scsi_mapping_vmx'} = "$scsi_busNumber:$scsi_unitNumber";
								$diskInfo{'scsi_mapping_host'}= $diskInfo{'scsi_mapping_vmx'};
								$disks{ $diskInfo{'disk_name'} } = \%diskInfo;
								push @list_diskInfo	, \%disks 									
							}
						}
					}
					$map_vmInfo{'diskConfig'}		= \@list_diskInfo;
					$map_vmInfo{'networkConfig'}	= \@list_networkInfo;
				}
				$map_vmsInfo{"host$i"} = \%map_vmInfo;
			} 
		}
	}
	return ( $SUCCESS , \%map_vmsInfo );
}

sub CreateVirtualMachine()
{
	my %args 	= @_;
	my $url = "https://10.0.110.12/api/vApp/vapp-ea81ab2b-c3d5-4273-bab3-742b0f353d61/action/recomposeVApp";

	my $xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
	<RecomposeVAppParams name=\"vApp_system_1\" xmlns=\"http://www.vmware.com/vcloud/v1.5\" xmlns:ovf=\"http://schemas.dmtf.org/ovf/envelope/1\">
	<Description>DR-Org VApp1</Description>
	<SourcedItem>
	<Source href=\"https://10.0.110.12/api/vAppTemplate/vappTemplate-ab9848e6-b8d6-4c49-9c0f-8aff796645ad\" name=\"TEST2\"  />
	</SourcedItem>
	<AllEULAsAccepted>true</AllEULAsAccepted>
	</RecomposeVAppParams>";
	
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
	
	my $response = $ua->request( POST $url , 'x-vcloud-authorization' => $args{'x-auth-code'}, 'Accept' => "application/*+xml;version=5.1" , 
								'Content-Type' => 'application/vnd.vmware.vcloud.recomposeVAppParams+xml' , 'content' => $xml );

}

sub GetRefToNewVM
{
	my %args	= @_;
	my $url 	= "https://10.0.110.12/api/vApp/vapp-ea81ab2b-c3d5-4273-bab3-742b0f353d61";
	my $href 	= "";
	
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
	
	my $response = $ua->request( GET $url , 'x-vcloud-authorization' => $args{'x-auth-code'}, 'Accept' => "application/*+xml;version=5.1" );
	
	my $xmlContent = $response->{_content};
	
	my $parser = XML::LibXML->new();
	my $xmlTree = $parser->parse_string( $xmlContent );
			
	my @vAppInfo = $xmlTree->getElementsByTagName('VApp');
	foreach my $vApp ( @vAppInfo )
	{
		print  "Name of vApp = ".$vApp->getAttribute('name').".\n";
		my @childrens 	= $vApp->getElementsByTagName('Children');
		my $i = 0;
		foreach my $child ( @childrens )
		{
			$i = $i + 1;
			my @vmList 	= $child->getElementsByTagName('Vm');
			foreach my $vm ( @vmList )
			{
				print  "Name of virtual machine = ".$vm->getAttribute('name').".\n";
				if ( "VM-Template-Plain" eq $vm->getAttribute('name') )
				{
					$href	= $vm->getAttribute('href');
				}
			}
		}
	}
	return $href;
}

sub ReconfigureVM
{
	my %args 	= @_;
	my $url 	= $args{'href'};
	my $contentType	= "application/vnd.vmware.vcloud.vm+xml";
	my $vmInfo	= "<Vm xmlns=\"http://www.vmware.com/vcloud/v1.5\" xmlns:rasd=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData\"
							 xmlns:ovf=\"http://schemas.dmtf.org/ovf/envelope/1\" xmlns:vssd=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_VirtualSystemSettingData\"
							  xmlns:vmw=\"http://www.vmware.com/schema/ovf\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" name=\"$args{'name'}\" 
							  xsi:schemaLocation=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_VirtualSystemSettingData http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2.22.0/CIM_VirtualSystemSettingData.xsd http://www.vmware.com/schema/ovf http://www.vmware.com/schema/ovf http://schemas.dmtf.org/ovf/envelope/1 http://schemas.dmtf.org/ovf/envelope/1/dsp8023_1.1.0.xsd http://www.vmware.com/vcloud/v1.5 http://10.0.110.12/api/v1.5/schema/master.xsd http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2.22.0/CIM_ResourceAllocationSettingData.xsd\">
						    <Description>Changed Description</Description>
    						<ovf:VirtualHardwareSection>
        						<ovf:Info>Virtual hardware requirements</ovf:Info>
        						<ovf:System>
            						<vssd:InstanceID>0</vssd:InstanceID>
            						<vssd:VirtualSystemIdentifier>$args{'name'}</vssd:VirtualSystemIdentifier>
            						<vssd:VirtualSystemType>vmx-07</vssd:VirtualSystemType>
        						</ovf:System>
        						<ovf:Item>
						            <rasd:AllocationUnits>hertz * 10^6</rasd:AllocationUnits>
						            <rasd:InstanceID>1</rasd:InstanceID>
						            <rasd:ResourceType>3</rasd:ResourceType>
						            <rasd:VirtualQuantity>$args{'cpu'}</rasd:VirtualQuantity>
						        </ovf:Item>
						        <ovf:Item>
						            <rasd:AllocationUnits>byte * 2^20</rasd:AllocationUnits>
						            <rasd:InstanceID>2</rasd:InstanceID>
						            <rasd:ResourceType>4</rasd:ResourceType>
						            <rasd:VirtualQuantity>$args{'memory'}</rasd:VirtualQuantity>
						        </ovf:Item>
						    </ovf:VirtualHardwareSection>
    						<ovf:OperatingSystemSection ovf:id=\"74\" vmw:osType=\"winLonghorn64Guest\">
						        <ovf:Info>Specifies the operating system installed</ovf:Info>
						    </ovf:OperatingSystemSection>
						    <GuestCustomizationSection>
        						<ovf:Info>Specifies Guest OS Customization Settings</ovf:Info>
						        <ComputerName>$args{'hostName'}</ComputerName>
						    </GuestCustomizationSection>
						    <VmCapabilities>
						        <MemoryHotAddEnabled>true</MemoryHotAddEnabled>
						        <CpuHotAddEnabled>true</CpuHotAddEnabled>
    						</VmCapabilities>
						</Vm>"; 
						
			PostRequest( content => $vmInfo , href => $url , contentType => $contentType , 'x-auth-code' => $args{'x-auth-code'} );					
}

sub PostRequest
{
	my %args 	= @_;
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
		
	my $response = $ua->request(
								 	POST $args{'href'} , 
									'x-vcloud-authorization' => $args{'x-auth-code'},
									'Accept' => "application/*+xml;version=5.1", 
									'Content-Type' => $args{'contentType'},
									'content' => $args{'content'} 
								);
	#print Dumper($response);	
}

sub AddDisksToMT
{
	my %args 	= @_;
	my @disksInfo	= ();
	my $displayName 	= $args{sourceHost}->getAttribute('display_name');
	eval
	{
		@disksInfo 	= $args{sourceHost}->getElementsByTagName('disk');
	};
	
	my $number 	= 0;
	foreach my $disk ( @disksInfo )
	{
		my $size 				= $disk->getAttribute('size');
		
		my $diskName 			= $displayName."_Disk".$number;	
		CreateIndependentDisk( 'diskname' => $diskName , 'size' => $size , 'x-auth-code' => $args{'x-auth-code'} );
		$number 	= $number + 1;
		
		#Get Disk Reference.
		
		my $href 	= GetDiskRef( 'name' => $diskName , 'x-auth-code' => $args{'x-auth-code'} );
		
		AttachDiskToVM( 'vm_DiskRef' => "https://10.0.110.12/api/vApp/vm-1c1c6397-be6f-4f50-aa3d-caa9714c269d/disk/action/attach" ,
						'diskRef' => $href , 'x-auth-code' => $args{'x-auth-code'} );
	}
}

sub AttachDiskToVM
{
	my %args 	= @_;
	my $url 	= $args{'vm_DiskRef'};

	my $xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
		<DiskAttachOrDetachParams xmlns=\"http://www.vmware.com/vcloud/v1.5\">
		<Disk type=\"application/vnd.vmware.vcloud.disk+xml\" href=\"$args{'diskRef'}\" />
		</DiskAttachOrDetachParams>";

	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
	
	my $response = $ua->request( POST $url , 'x-vcloud-authorization' => $args{'x-auth-code'}, 'Accept' => "application/*+xml;version=5.1" , 
								'Content-Type' => 'application/vnd.vmware.vcloud.diskAttachOrDetachParams+xml' , 'content' => $xml );
	
	#print  Dumper($response);
}

sub GetDiskRef
{
	my %args 	= @_;
	my $url = "https://10.0.110.12/api/vdc/4c096484-6958-4dc4-a628-ff3339f3a2aa";
	my $href	= "";
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
	
	
	my $response = $ua->get( $url , 'x-vcloud-authorization' => $args{'x-auth-code'}, 'Accept' => "application/*+xml;version=5.1" );
	
	my $xmlContent = $response->{_content};
	
	print $xmlContent;
	
	my $parser = XML::LibXML->new();
	my $xmlTree = $parser->parse_string( $xmlContent );
		
	my @vdcList = $xmlTree->getElementsByTagName('Vdc');
	
	foreach my $vdc ( @vdcList )
	{
		my @resourceEntities = $vdc->getChildrenByTagName('ResourceEntities');
		foreach my $resourceentity ( @resourceEntities )
		{
			my @resources 	= $resourceentity->getChildrenByTagName('ResourceEntity');
			foreach my $entity ( @resources )
			{
				if ( $args{'name'} eq $entity->getAttribute('name') )
				{
					print  $entity->getAttribute("name").",".$entity->getAttribute("href")."\n";
					$href	= $entity->getAttribute("href");
				}
			}
		}
	}
	return ( $href );
}

sub CreateIndependentDisk()
{
	my %args	= @_;
	
	my $url = "https://10.0.110.12/api/vdc/4c096484-6958-4dc4-a628-ff3339f3a2aa/disk";

	my $size_in_MB 	= $args{'size'} / ( 1024 );

	my $xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
	<DiskCreateParams xmlns=\"http://www.vmware.com/vcloud/v1.5\">
	<Disk name=\"$args{'diskname'}\" size=\"$size_in_MB\">
	<Description>IndependentDisk</Description>
	</Disk>
	</DiskCreateParams>";
	
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
	
	
	my $response = $ua->request( POST $url , 'x-vcloud-authorization' => $args{'x-auth-code'}, 'Accept' => "application/*+xml;version=5.1" , 
								'Content-Type' => 'application/vnd.vmware.vcloud.diskCreateParams+xml' , 'content' => $xml );
	
	#print  Dumper($response);
	
}

1;
