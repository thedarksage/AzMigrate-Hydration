#================================================================= 
# FILENAME
#   Register.pm 
#
# DESCRIPTION
#    This perl module is responsible for the registration 
#    section of HighAvailability. 
#    
# HISTORY
#     06 Jan 2009  Shrinivas M A    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================

package ConfigServer::HighAvailability::Register;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Config;
use Utilities;
use Sys::Hostname;
use TimeShotManager;
use Common::Constants;
use Common::Log;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use RequestHTTP;
my $logging_obj = new Common::Log();
my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();

#
#Registration of HA Nodes 
#
sub register_node
{    
    eval
	{		
        my $cx_type = $AMETHYST_VARS->{'CX_TYPE'};
        my $http = ($cx_type == 1) ? $AMETHYST_VARS->{'CS_HTTP'} : $AMETHYST_VARS->{'PS_CS_HTTP'};
        my $cs_url_ip = ($cx_type == 1) ? $AMETHYST_VARS->{'CS_IP'} : $AMETHYST_VARS->{'PS_CS_IP'};
        my $cs_url_port = ($cx_type == 1) ? $AMETHYST_VARS->{'CS_PORT'} : $AMETHYST_VARS->{'PS_CS_PORT'};
		my ($ha_data, $node_role);
        my $host_guid = $AMETHYST_VARS->{'HOST_GUID'};
        
        if($host_guid eq "")
        {
            $logging_obj->log("EXCEPTION","monitor_ha:Host id is empty.");
            return;
        }
        
        #Check if Current node is Active
        if (TimeShotManager::isHeartbeatActiveNode() == 0)
        {
            $node_role = Common::Constants::ACTIVE_ROLE;
        }
        else
        {
            $node_role = Common::Constants::PASSIVE_ROLE;
        }
        
        $ha_data->{"applianceId"} = $AMETHYST_VARS->{'CLUSTER_NAME'};
        $ha_data->{"applianceIp"} = $AMETHYST_VARS->{'CLUSTER_IP'};
        $ha_data->{"nodeId"} = $host_guid;
        $ha_data->{"nodeRole"} = $node_role;
		
        $ha_data = serialize($ha_data);
			
		my $request_xml = "<Body>
						<FunctionRequest Name='RegisterHost'>
							<ParameterGroup Id='RegisterHost'>
								<Parameter Name='ha_info' Value='".$ha_data."'/>
							</ParameterGroup>
						</FunctionRequest>
					</Body>"."\n";
			

		my $http_method = "POST";
		my $https_mode = ($http eq 'https') ? 1 : 0;	
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $cs_url_ip, 'PORT' => $cs_url_port);
		my $param = {'access_method_name' => "RegisterHost", 'access_file' => "/ScoutAPI/CXAPI.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my $content = {'type' => 'text/xml', 'content' => $request_xml};

		
		my $request_http_obj = new RequestHTTP(%args);
		my $response = $request_http_obj->call($http_method, $param, $content);
		
		if (! $response->{'status'}) 
		{
			$logging_obj->log("DEBUG","Response::".Dumper($response));
			$logging_obj->log("EXCEPTION","Failed to post the details for HA registration");
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : ".$@);
	}
}
1;
