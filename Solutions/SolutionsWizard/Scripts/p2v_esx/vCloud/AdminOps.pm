=head
	Administration options.
	1. Connect with Cloud.
	2. Retrieve session URL.
	3. Get authorization code.
	4. If authorization code expired get a new session.
=cut

package AdminOps;

use strict;
use warnings;

use LWP::UserAgent;
use HTTP::Request::Common qw(POST GET );
use MIME::Base64;
use CommonUtils;
use XmlParser;
use Data::Dumper;

my $SUCCESS = 0;
my $FAILURE = 1;

#first we need to set path variablem, then only we could include CommonUtils.pm file.

#####Connect#####
##Description 		:: Connects with vCloud.
##Input 			:: Cloud IP, Organization Name, Username and Password.
##Output 			:: Returns SUCCESS Message along with session URL to connect.
#####Connect#####
sub Connect
{
	my %args = @_;
	my $returnCode = 0;
	
	print  "vCloud IP = $args{cloud_ip}.\n";
	my $url = "http://$args{cloud_ip}/api/versions";
		
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
	my $response = $ua->request( GET $url );
	
	my ( $ReturnCode , $loginURL ) = XmlParser::GetLoginURL( response => $response , version => "5.1" );
	if ( $SUCCESS != $ReturnCode )
	{
		print  "ERROR :: Failed to get login URL of vCloud.\n";
		$returnCode		= $FAILURE;
	}
	else
	{
		print  "Success :: Login URL = $loginURL.\n";
		$returnCode 	= $SUCCESS
	}
	
	return ( $returnCode , $loginURL );
}


#####ConnectAndGetAuthorizationCode#####
##Description 		:: Not only establishes a connection to vCloud but also makes authorization request.
##Input 			:: Cloud IP , Organization Name, User Name and Password.
##Output 			:: Returns SUCCESS message along with Authorization Code or FAILURE.
#####ConnectAndGetAuthorizationCode#####
sub ConnectAndGetAuthorizationCode
{
	my %args = @_;
	my $returnCode = $FAILURE;
	
	print  "vCloud IP = $args{cloud_ip}.\n";
	my $url = "http://$args{cloud_ip}/api/versions";
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(100);
	my $response = $ua->request( GET $url );
	my $xmlContent = $response->{_content};

	my ( $ReturnCode , $loginURL ) = XmlParser::GetLoginURL( response => $xmlContent, version => "5.1" );
	if ( $SUCCESS != $ReturnCode )
	{
		print  "ERROR :: Failed to get login URL of vCloud.\n";
		$returnCode		= $FAILURE;
	}
	else
	{
		$loginURL	=~ s/^http/https/;
		print  "Success :: Login URL = $loginURL.\n";
		$returnCode 	= $SUCCESS
	}
	
	#Todo..Always there has to be a function which checks whether GET/POST call is successful or not.
	#Todo.. First we need to make call using HTTP and then use HTTPS if returned error code = 302.
	
	my $credentials = $args{userName}."@".$args{orgName}.":".$args{password};
	
	$credentials = encode_base64($credentials,"");
	
	$response = $ua->post( $loginURL , 'Authorization' => "Basic $credentials" , 'Accept' => "application/*+xml;version=5.1" );
	
	#ToDo... Check whether the Authentication request is successful or not. If not we have to Return ERROR.
	#Also the version number has to be taken care accordingly.
	my $authorizationCode	= $response->{'_headers'}->{'x-vcloud-authorization'};
	print  "Success :: Authorization Code = $authorizationCode.\n";
	
	return ( $returnCode , $authorizationCode );
}

#####GetListOfOrganizations######
##Description		:: Discover Information of organizations and their associated vApps, VM and Network Pools.
##Input 			:: Authorization Code. 
##						1. First Query for organizations.
##						2. Query vApps under organizations.
##						3. Query VM's under a vApp.
##						4. Query Network information under a vApp.
##Output 			:: Returns HASHED information and a Success Message or FAILURE.
#####GetListOfOrganizations######
sub GetListOfOrganizations
{
	my %args		= @_;
	my %mapOrgHref	= ();
	#Todo...we have to get the URL for Org As well?
	my $url = "https://$args{cloud_ip}/api/org/";

	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);

	my $response = $ua->get( $url , 'x-vcloud-authorization' => $args{'x-auth-code'}, 'Accept' => "application/*+xml;version=5.1" );
	
	#TODO... need to add error checking here.
	
	my $xmlContent 	= $response->{_content};
	
	#Take Organisations in to a Hash and Return It.
	my $parser 	= XML::LibXML->new();

	my $xmlTree = $parser->parse_string( $xmlContent );
	
	my @orgList = $xmlTree->getElementsByTagName('OrgList');
	
	foreach ( @orgList )
	{
		my @organizations 	= $_->getChildrenByTagName('Org');
		foreach ( @organizations )
		{
			print  "Organization Name = ".$_->getAttribute("name")."\n";
			print  "Organization href = ".$_->getAttribute("href")."\n";
			$mapOrgHref{$_->getAttribute("name")} = $_->getAttribute("href");
		}
	}
	print  Dumper(%mapOrgHref);
	return ( $SUCCESS , \%mapOrgHref );
} 

#####GetVDCInformation#####
##Description 			:: Query Information of VDC of Organization.
##Input 				:: Organization information like Orgnaization Name and its HREF.
##Output				:: Returns VDC information along with SUCCESS message or FAILURE message.
#####GetVDCInformation#####
sub GetVDCInformation
{
	my %args 	= @_;
	my %map_vdcInfo	= ();
	
	print  "$args{'orgName'}=$args{'href_org'}\n";
		
	my $url = $args{'href_org'};
		
	my $ua	= LWP::UserAgent->new();
	$ua->timeout(30);
		
	my $response = $ua->get( $url , 'x-vcloud-authorization' => $args{'x-auth-code'}, 'Accept' => "application/*+xml;version=5.1" );
		
	my $xmlContent = $response->{_content};
		
	my $parser = XML::LibXML->new();
	my $xmlTree = $parser->parse_string( $xmlContent );
			
	my @orgList = $xmlTree->getElementsByTagName('Org');
		
	foreach my $list ( @orgList )
	{
		my @links = $list->getChildrenByTagName('Link');
		foreach my $link ( @links )
		{
			if ( "application/vnd.vmware.vcloud.vdc+xml" eq $link->getAttribute('type') )
			{
				print  $link->getAttribute("name").",".$link->getAttribute("href")."\n";
				$map_vdcInfo{$link->getAttribute("name")}	= $link->getAttribute("href");
			}
		}
	}
	 				 
	return ( $SUCCESS ,  \%map_vdcInfo );
}
1;