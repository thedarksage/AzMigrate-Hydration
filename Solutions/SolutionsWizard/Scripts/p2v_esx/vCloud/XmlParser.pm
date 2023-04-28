=head
	Parses XML response returned by vCloud REST API calls.
	1. These functions are generic to the XML of vCloud.
	2. These parses only information in related to vCloud responses.
	3. Do not clud other functions in to this module.
=cut

package XmlParser;

use strict;
use warnings;
use XML::LibXML;

my $SUCCESS = 0;
my $FAILURE = 1;

#####GetLoginURL#####
##Description 		:: Fetches Login URL in given XML string.
##Input 			:: XML response string returned by GET request to vCloud IP.
##Output 			:: Returns SUCCESS along with Login URL or FAILURE.
#####GetLoginURL#####
sub GetLoginURL
{
	my %args = @_;
	my $loginURL	= "";
	
	print  "Version of API to be supported = $args{version}.\n";
	
	my $parser = XML::LibXML->new();

	my $xmlTree 	= $parser->parse_string( $args{response} );
	
	my @supportedVersions = $xmlTree->getElementsByTagName('SupportedVersions');
	
	foreach (@supportedVersions)
	{
		my @versionList 	= $_->getElementsByTagName('VersionInfo');
		
		foreach ( @versionList )
		{
			my @versionslist  = $_->getChildrenByTagName('Version');
			my @LoginURLList  = $_->getChildrenByTagName('LoginUrl');
			foreach ( @versionslist )
			{
				print   $_->nodeName."=";
				print $_->getFirstChild->getData."\n";
				my $version =  $_->getFirstChild->getData;
				if ( $version == $args{version} )
				{
					$loginURL = $LoginURLList[0]->getFirstChild->getData;
					print  "Login URL = $loginURL.\n";
				}
			}
		}
	}
	return ( $SUCCESS , $loginURL );
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
		return ( $SUCCESS  , $args{hostNode} );
	}
	my @diskInfo	= @{ $args{diskInfo} };
	
	for ( my $i = 0 ; $i <= $#diskInfo ; $i++ )
	{
		while ( my ( $key , $value ) = each %{$diskInfo[$i]} )
		{
			my $disk 	= $doc->createElement('disk');
			my %diskInfo = %{$value};
			$disk->setAttribute( 'disk_name' , $diskInfo{disk_name} );
			$disk->setAttribute( 'size' , $diskInfo{size} );
			$disk->setAttribute( 'disk_mode' , $diskInfo{disk_mode} );
			$disk->setAttribute( 'ide_or_scsi' , $diskInfo{ide_or_scsi} );
			$disk->setAttribute( 'scsi_mapping_vmx' , $diskInfo{scsi_mapping_vmx} );
			$disk->setAttribute( 'scsi_mapping_host' , $diskInfo{scsi_mapping_host} );
			$disk->setAttribute( 'independent_persistent' , $diskInfo{independent_persistent} );
			$disk->setAttribute( 'controller_type' , $diskInfo{controller_type} );
			$disk->setAttribute( 'controller_mode' , $diskInfo{controller_mode} );
			$disk->setAttribute( 'cluster_disk' , $diskInfo{cluster_disk} );
			$args{hostNode}->appendChild($disk);
		}
	}
	
	return ( $SUCCESS  , $args{hostNode} );
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
		return ( $SUCCESS  , $args{hostNode} );
	}
	my @networkInfo	= @{ $args{networkInfo} };
	
	for ( my $i = 0 ; $i <= $#networkInfo ; $i++ )
	{
		while ( my ( $key , $value ) = each %{$networkInfo[$i]} )
		{
			my $nic 	= $doc->createElement('nic');
			my %nic_info= %{$value};
			$nic->setAttribute( 'network_label' , $nic_info{network_label} );
			$nic->setAttribute( 'network_name' , $nic_info{network_name} );
			$nic->setAttribute( 'nic_mac' , $nic_info{nic_mac} );
			$nic->setAttribute( 'ip' , $nic_info{ip} );
			$nic->setAttribute( 'address_type' , $nic_info{address_type} );
			$nic->setAttribute( 'adapter_type' , $nic_info{adapter_type} );
			$nic->setAttribute( 'dhcp' , $nic_info{dhcp} );
			$nic->setAttribute( 'dnsip' , $nic_info{dnsip} );
			$args{hostNode}->appendChild($nic);
		}
	}
	
	return ( $SUCCESS  , $args{hostNode} );
}

#####PrepareXML#####
##Description		:: Prepares XML file from HASH sent.
##Input 			:: Hash which contains all the information in relation to Organizations.
##Output 			:: Returns SUCCESS and XML file is prepared else FAILURE MESSAGE.
#####PrepareXML#####
sub PrepareXML
{
	my %args 	= @_;
	
	my $filename 		= "Info.xml";
	my $element_name 	= "INFO";
	
	if( ! open(XML,">$filename"))
	{
		print  " ERROR :: Unable to create XML file \"$filename\".";
		print  "ERROR :: Please check for adequate permissions to create a file and do a re-protection.";
		return $FAILURE;
	}
	
	my $doc 			= XML::LibXML::Document->new();
	my $ESX_NODE 		= $doc->createElement($element_name);
	$doc->setDocumentElement($ESX_NODE);
	$ESX_NODE->setAttribute("cloud","yes");
	
	while ( my ( $key , $value ) = each %{$args{'info'}} )
	{
		my $orgName 	= $key;
		my %map_vdcInfo = %{$value};
		
		while ( my ( $key , $value ) = each %map_vdcInfo )
		{
			my $vdcName 	 = $key;
			my %map_vAppInfo = %{$value};
			
			while ( my ( $key , $value ) = each %map_vAppInfo )
			{
				my $vAppName 	= $key;
				my %map_vmsInfo	= %{$value};
				
				while ( my ( $key , $value ) = each %map_vmsInfo )
				{
					my %map_vmInfo = %{$value};
					
					my $host 		= $doc->createElement('host');
					$host->setAttribute( 'display_name' , $map_vmInfo{'display_name'} );
					$host->setAttribute( 'org_name' , $orgName );
					$host->setAttribute( 'vdc_name' , $vdcName );
					$host->setAttribute( 'vapp_name' , $vAppName );
					$host->setAttribute( 'hostname' , $map_vmInfo{'hostname'} );
					$host->setAttribute( 'ip_address' , $map_vmInfo{'ip_address'} );
					$host->setAttribute( 'powered_status' , $map_vmInfo{'powered_status'} );
					$host->setAttribute( 'source_uuid' , $map_vmInfo{'uuid'} );
					$host->setAttribute( 'memsize' , $map_vmInfo{'memory'} );
					$host->setAttribute( 'cpucount' , $map_vmInfo{'cpucount'} );
					$host->setAttribute( 'vmx_version' , $map_vmInfo{'vmx_version'} );
					$host->setAttribute( 'ide_count' , $map_vmInfo{'ide_count'} );
					$host->setAttribute( 'floppy_device_count' , $map_vmInfo{'floppy_device_count'} );
					$host->setAttribute( 'alt_guest_name' , $map_vmInfo{'alt_guest_name'} );
					$host->setAttribute( 'operatingsystem' , $map_vmInfo{'operatingsystem'});
					$host->setAttribute( 'rdm' , $map_vmInfo{'rdm'} );
					$host->setAttribute( 'number_of_disks' , $map_vmInfo{'number_of_disks'} );
					$host->setAttribute( 'cluster' , $map_vmInfo{'cluster'} );
					
					( my $returnCode , $host ) = MakeDiskNode( xmlDoc => $doc , hostNode => $host , diskInfo => $map_vmInfo{diskConfig} );
					if ( $SUCCESS != $returnCode )
					{
						print  "ERROR :: Failed to make disk node for machine\n.";
						return $FAILURE;
					}
								
					( $returnCode , $host ) = MakeNicNode( xmlDoc => $doc , hostNode => $host , networkInfo => $map_vmInfo{networkConfig} );
					if ( $SUCCESS != $returnCode )
					{
						print  "ERROR :: Failed to make netwrok node for machine.";
						return $FAILURE;
					}
					
					$ESX_NODE->appendChild($host);
					
				}
			}
		}
		
	}
	
	print XML $doc->toString(1);
	close XML;
	
	return $SUCCESS;
	
}


1;