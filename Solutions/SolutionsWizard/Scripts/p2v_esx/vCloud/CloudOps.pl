#!/usr/bin/perl

=head
	CloudOps.pl file.
	1. Operations are specified in command line.
	2. Wrapper script for all operations on Cloud.
=cut

use strict;
use warnings;
use PHP::Serialization qw(serialize unserialize);
use vAppOps;
use OrganizationOps;
use AdminOps;
use CommonUtils;
use Getopt::Long;
use Data::Dumper;
use XmlParser;
use XmlOps;

my $op			= ""; #by default set this to empty string.
my $cloud_ip 	= "";
my $org 		= "";
my $username 	= "";
my $password 	= "";
my $SUCCESS 	= 0;
my $FAILURE 	= 1;
my $xml 		= "";
my $tmp_file = "/tmp/xxx";
if( $^O =~ /MSWin32/i )
{
	$tmp_file = "C:\\Temp\\xxx";
}
GetOptions( "op=s" => \$op, 
			"cloud_ip=s" => \$cloud_ip,
			"org=s" => \$org,
			"username=s" => \$username,
			"password=s" => \$password,
			"xml:s" => \$xml
			) or die ("Error in command line arguments.\n");

#####Discover#####
##Description 	:: Discovers all information in regard to cloud..futher this has to be enhanced to discover only certain infomration like
##					VM information, network information etc.
##Input			:: Cloud IP, Organization Name , User Name, Password. xml=>yes/no.
##Output 		:: In the process it writes XML if option set to yes or it can return a hash like structure which will be used to store in DB.
#####Discover#####
sub Discover
{
	my %args 		= @_;
	my $returnCode 	= 0;
	my %info		= (); 	 
	
	my ( $ReturnCode , $authorizationCode ) = AdminOps::ConnectAndGetAuthorizationCode( cloud_ip => $args{cloud_ip} , orgName => $args{org} ,
																						userName => $args{username} , password => $args{password} );
	if ( $ReturnCode != $SUCCESS )
	{
		$returnCode = $FAILURE;
	}
	else
	{
		#we have got the authorization code.Now we have to write discovery procedure.
		#ALgorithm for Discovery.
		#Query Organization list.
		my ( $ReturnCode , $map_orgInfo ) = AdminOps::GetListOfOrganizations( 'x-auth-code' => $authorizationCode , 'cloud_ip' => $args{cloud_ip} );
		if ( $ReturnCode != $SUCCESS )
		{
			print  "ERROR :: Failed to Discover Organization List.";
		}
		
		my %map_orgInfo 	= %$map_orgInfo;
		while ( my ( $key , $value ) = each %map_orgInfo )
		{
			#query VDC Information.
			my $orgName = $key;
			my ( $ReturnCode , $map_vdcInfo ) = AdminOps::GetVDCInformation( 'x-auth-code' => $authorizationCode , 'orgName' => $key , 'href_org' => $value );
			if ( $ReturnCode != $SUCCESS )
			{
				print  "ERROR :: Failed to discover VDC information.";
			}
			
			my %map_vdcInfo	= %$map_vdcInfo;
			while ( my ( $key , $value ) = each %map_vdcInfo )
			{
				#query VApp Information.
				my $vdcName 	= $key;
				my ( $ReturnCode , $map_vAppInfo ) = vAppOps::GetListOfvAppsInVDC( 'x-auth-code' => $authorizationCode , 'vdcName' => $key , 'href_vdc' => $value );
				if ( $ReturnCode != $SUCCESS )
				{
					print  "ERROR :: Failed to list vApp's in vdc $key.";
				}	
				
				my %map_vAppInfo	= %$map_vAppInfo;
				while ( my ( $key , $value ) = each %map_vAppInfo )
				{
					#Query VM Information.
					my $vAppName 	= $key;
					my ( $ReturnCode , $map_vAppInfo ) = vAppOps::GetVmInformationInvApp( 'x-auth-code' => $authorizationCode , 'vAppName' => $key , 'href_vApp' => $value );
					if ( $ReturnCode != $SUCCESS )
					{
						print  "ERROR :: Failed to list vm's in vApp $key.";
					}
					
					my %map_vmsInfo	= %$map_vAppInfo;
					$info{$orgName}{$vdcName}{$vAppName} = \%map_vmsInfo;
				}
			}
		}
	}
	
	
	#if($args{xml} eq "no")
	if($xml eq "no")
	{
		my $hash_str = serialize(\%info);
		
		close(X);
		local $| = 1;
		select(STDOUT);		
		
		print "$hash_str";
		open(X,">$tmp_file");
		select(X);
		return($returnCode);
	}
	else
	{	
		#print  Dumper(\%info);
		
		$ReturnCode = XmlParser::PrepareXML( info => \%info ); 
	}
																								
	return ( $returnCode );	
}

#####Protect#####
##Description 	:: Protects Virtual machines to cloud specified in xml File.
##Input			:: It reads the XML file.
##Output 		:: Returns SUCCESS on PROTECTION or FAILURE.
#####Protect#####
sub Protect
{
	my %args 	= @_;
	my $returnCode = $SUCCESS;
	
	my ( $ReturnCode , $docNode ) 		= XmlOps::GetPlanNodes( $args{file} );
	if ( $ReturnCode != $SUCCESS )
	{
		print "ERROR :: Failed to get plan information from file \"args{file}\".";
		return ( $FAILURE );
	}
	
	my $planNodes 						= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 						= [$docNode];
	}
	
	( $ReturnCode ,my $cxIpPortNum )= CommonUtils::FindCXIP();
	if ( $ReturnCode != $SUCCESS )
	{
		print  "ERROR :: Failed to find CX IP and Port Number.";
		return ( $FAILURE );
	}
	
	my @CreatedVirtualMachineInfo		= ();
	foreach my $plan ( @$planNodes )
	{
		my $planName					= $plan->getAttribute('plan');
		my @targetNodes					= $plan->getElementsByTagName('TARGET_ESX');
		my @targetHost					= $targetNodes[0]->getElementsByTagName('host');	
		my $vContinuumPlanDir			= $plan->getAttribute('xmlDirectoryName');
		print  "Protection plan name = $planName.";
		
		my $mtOrg 						= $targetHost[0]->getAttribute('org_name');
		my $mtVdcName 					= $targetHost[0]->getAttribute('vdc_name');
		my $mtVappName					= $targetHost[0]->getAttribute('vapp_name');
		
		print  "MT: Org = $mtOrg , vdc = $mtVdcName , vapp = $mtVappName.\n";
		
		my ( $ReturnCode , $authorizationCode ) = AdminOps::ConnectAndGetAuthorizationCode( cloud_ip => $args{cloud_ip} , orgName => $mtOrg ,
																						userName => $args{username} , password => $args{password} );
		if ( $ReturnCode != $SUCCESS )
		{
			$returnCode = $FAILURE;
			return $returnCode;
		}
		else
		{
			
			CreateVirtualMachine( 'x-auth-code' => $authorizationCode );
			#QueryVAPP to get Newly Created VM Info.
			my @sourceHosts					= $plan->getElementsByTagName('SRC_ESX');
			foreach my $sourceHost	( @sourceHosts )
			{
				my @hostsInfo				= $sourceHost->getElementsByTagName('host');
				foreach my $host ( @hostsInfo )
				{
					my $displayName 		= $host->getAttribute('display_name');
					my $memory				= $host->getAttribute('memsize');
					my $cpuCount			= $host->getAttribute('cpucount');
					my $hostName 			= $host->getttribute('hostname');
					
					my $href	=	GetRefToNewVM( 'x-auth-code' => $authorizationCode );
					ReconfigureVM( 'href' => $href , 'x-auth-code' => $authorizationCode , 'name' => $displayName ,
									'hostName' => $hostName , 'memory' => $memory , 'cpu' => $cpuCount );
									
					##Add Disks To MT									
					AddDisksToMT( sourceHost => $host , 'x-auth-code' => $authorizationCode );				
				}
			}

		}
	}
	return ( $SUCCESS );
}


BEGIN:
open(X,">$tmp_file");
select(X);

# Setting default value for XML
$xml = ($xml eq "") ? "yes" : $xml;
my $return_code;

if ( lc( $op ) eq lc( "discover" ) )
{
	print  "Choosen option = Discover.\n";
	
	#Todo..Make sure that we have received all parameters required for discovery.As of now not checking whether they are empty or not.
	
	$return_code = Discover( 'cloud_ip' => $cloud_ip , 'org' => $org , 'username' => $username , 'password' => $password , 'xml' => $xml );
	if ( !$return_code )
	{
		print  "ERROR :: Discover operation is incomplete.\n";
	}
	else
	{
		print  "Successfully discovered information.\n"
	}
	close(X);
	local $| = 1;
	select(STDOUT);	
}
elsif( lc( $op ) eq lc( 'protect' ) )
{
	print  "Choosen Option = Protect.\n";
	
	if ( !(  Protect( 'cloud_ip' => $cloud_ip , 'username' => $username , 'password' => $password  ) ) )
	{
		print  "ERROR :: Failed to protect.\n";
	}
	else
	{
		print  "Info : Successfully protected machines.\n";
	}
}
exit($return_code);

__END__