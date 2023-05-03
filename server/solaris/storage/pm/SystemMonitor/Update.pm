#!/usr/bin/perl
package SystemMonitor::Update;

use lib ("/var/apache2/2.2/htdocs");
use lib ("/home/svsystems/pm");
use strict;
use Utilities;
use SystemMonitor::Monitor;
use Data::Dumper;
#use LWP::UserAgent;

my $logging_obj = new Common::Log();

# 
# Function Name: new
#
# Description: Constructor for the Update class.
#
# Parameters: None 
#
# Return Value:None
#  	
sub new
{
	my $class = shift;
    my $self = {};

    my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
	$self->{'amethyst_vars'} = $amethyst_vars;
	$self->{'base_dir'} = $amethyst_vars->{"INSTALLATION_DIR"};
	$self->{'mon_obj'} = new SystemMonitor::Monitor();
    bless ($self, $class);
}

# 
# Function Name: updatePS
#
# Description:  
#	 This method will dispatch the post request on cs_url with perf and serv params
#
# Parameters: None 
#
# Return Value: None
#
sub updatePS
{
    my $self = shift;
	my ($response,$host_guid,$service_status,$request_xml);
	
	my $cs_url = "http://".$self->{'amethyst_vars'}->{'CS_IP'}.":".$self->{'amethyst_vars'}->{'CS_PORT'}."/ScoutAPI/CXAPI.php";	

	$host_guid = $self->{'amethyst_vars'}->{'HOST_GUID'};
	$service_status = $self->{'mon_obj'}->getSNService();
	#$disk_usage = $self->{'mon_obj'}->getDiskUsage();
	
	
	my $request_xml = "<Request Id='0001' Version='1.0'>
					<Header>
						<Authentication> 
							<AccessKeyID>DDC9525FF275C104EFA1DFFD528BD0145F903CB1</AccessKeyID>
							<AccessSignature></AccessSignature>
						</Authentication> 
					</Header>
					<Body>
						<FunctionRequest Name='RegisterStorageDynamicInfo'>
							<ParameterGroup Id='RegisterStorageDynamicInfo'>
								<Parameter Name='service_status' Value='".$service_status."'/>
								<Parameter Name='hostId' Value='".$host_guid."'/>
							</ParameterGroup>
						</FunctionRequest>
					</Body> 
				</Request>"."\n";
	
	$logging_obj->log("EXCEPTION","request_xml ::$request_xml \n");
	my $response;
	
	my $content_type = 'text/xml';
	$request_xml =~ s/\"/\\"/g;
	my $post_data = "Content_Type##".$content_type."!##!"."Content##".$request_xml;
	$response = `php /home/svsystems/web/curl_dispatch_http.php "$cs_url" "$post_data"`;
	
	$logging_obj->log("EXCEPTION","Post the content for registerStorageDynamicInfo request_xml - $request_xml");
}
1;
