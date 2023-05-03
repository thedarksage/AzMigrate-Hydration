#!/usr/bin/perl
package SystemMonitor::Update;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Utilities;
use TimeShotManager;
use SystemMonitor::Monitor;
use SystemMonitor::Graph;
use Time::HiRes qw(gettimeofday);
use Data::Dumper;
use RequestHTTP;
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
# Function Name: getCurrentTime
#
# Description:  Used to get current date and time
#
# Parameters: None 
#
# Return Value: Returns time interval
#
sub getCurrentTime
{
	my $self = shift;
	my ($epochseconds, $microseconds) = gettimeofday;
	my $time_interval =   $epochseconds.".".$microseconds;
	return $time_interval;
}

# 
# Function Name: updatePerf
#
# Description:  gets perf parameters and calls createPerfRrdDb to create/update rrd
#
# Parameters: None 
#
# Return Value: None
#
sub updatePerf
{
	my $self = shift;	
	
	my $cs_guid = $self->{'mon_obj'}->getGuid();
	
	#File to read all the PS GUIDS. We cannot rely on DB, as the thread has to run though DB is not available
	my $FILE_LOC = $self->{'base_dir'}."/var/PS_GUIDS";
	my @host_guids = $self->getContents($FILE_LOC);
	push(@host_guids, $cs_guid);
	@host_guids = keys %{{ map { $_ => 1 } @host_guids }};
	
	# Constructing RRD Path
	my $rrd_path = $self->{'base_dir'}."/SystemMonitorRrds/";
	
	foreach my $guid (@host_guids) 
	{
		if($guid)
		{
			my $perf_file = $rrd_path . $guid . "-perf.txt";
			$perf_file = Utilities::makePath($perf_file);
			my @perf_input = $self->getContents($perf_file);
			foreach my $perf_input (@perf_input)
			{
				my $rrd_data;
				# Constructing RRD string
				my $rrd_name = $rrd_path . $guid . "-perf.rrd";
				if($perf_input ne "") 
				{
					$perf_input =~ s/\n//;
					my @data = split(/:/,$perf_input);
					shift(@data); 
					$rrd_data = "";
					$rrd_data = join(":",@data);
					$rrd_data .= ":0:0:0";
				}
				else
				{
					# If file contains nothing then update 0's
					$rrd_data = "0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0";
				}
				$logging_obj->log("DEBUG","Updating PS RRD $rrd_name with $rrd_data\n");
				$self->{'mon_obj'}->createPerfRrdDb($rrd_name, $rrd_data);
				
			}
		}
	}
}

# 
# Function Name: updateService
#
# Description:  gets services status and calls createServiceRrdDb to create/update rrd
#
# Parameters: None 
#
# Return Value: None
#
sub updateService
{
	my $self = shift;
	
	my $cs_guid = $self->{'mon_obj'}->getGuid();
	my $FILE_LOC = $self->{'base_dir'}."/var/PS_GUIDS";
	my @host_guids = $self->getContents($FILE_LOC);
	push(@host_guids, $cs_guid);
	@host_guids = keys %{{ map { $_ => 1 } @host_guids }};
	
	# Constructing RRD Path
	my $rrd_path = $self->{'base_dir'}."/SystemMonitorRrds/";
	
	foreach my $guid (@host_guids) 
	{
		if($guid)
		{
			my $serv_file = $rrd_path.$guid."-service.txt";
			$serv_file = Utilities::makePath($serv_file);
			my @serv_input = $self->getContents($serv_file);
			foreach my $serv_input (@serv_input)
			{
				my $rrd_name = $rrd_path . $guid . "-service.rrd";
				my $rrd_data;
				if($serv_input ne "") 
				{
					$serv_input =~ s/\n//;
					my @data = split(/:/,$serv_input);
					shift(@data);
					$rrd_data = join(":",@data);		
				}
				else
				{
					$rrd_data = "0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0";
				}
				$logging_obj->log("DEBUG","Updating PS RRD $rrd_name with $rrd_data\n");
				$self->{'mon_obj'}->createServiceRrdDb($rrd_name, $rrd_data);
			}
		}
	}
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

	eval
	{
		my $cs_ps_guid = $self->{'mon_obj'}->getGuid();
		my $cx_type = $self->{'amethyst_vars'}->{'CX_TYPE'};
		
		if($cx_type eq "1" || $cx_type eq "3")
		{
			my $serv_params = $self->{'mon_obj'}->getServiceStatus();				
			my $serv_file = $self->{'amethyst_vars'}->{"INSTALLATION_DIR"}."/SystemMonitorRrds/".$cs_ps_guid."-service.txt";
			open(SERVFILE, ">$serv_file") || die "Could not open $serv_file $!";
			print SERVFILE $serv_params;
			close(SERVFILE);
			
			my $perf_params = $self->{'mon_obj'}->getAll();
			my $perf_file = $self->{'amethyst_vars'}->{"INSTALLATION_DIR"}."/SystemMonitorRrds/".$cs_ps_guid."-perf.txt";
			open(PERFFILE, ">$perf_file") || die "Could not open $perf_file $!";
			print PERFFILE $perf_params;
			close(PERFFILE);
		}
	
		if($cx_type ne "1")
		{
			my $perf_params = $self->{'mon_obj'}->getPerfPs();
			my $serv_params = $self->{'mon_obj'}->getServiceStatus();
			
			my $response;
			my $http_method = "POST";
			my $https_mode = ($self->{'amethyst_vars'}->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;
			my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $self->{'amethyst_vars'}->{'PS_CS_IP'}, 'PORT' => $self->{'amethyst_vars'}->{'PS_CS_PORT'});
			my $param = {'access_method_name' => "updatePS", 'access_file' => "/updatePS.php", 'access_key' => $self->{'amethyst_vars'}->{'HOST_GUID'}};
			my %perl_args = ('perf_params' => $perf_params,'serv_params' => $serv_params);
			my $content = {'content' => \%perl_args};
		
			my $request_http_obj = new RequestHTTP(%args);
			$response = $request_http_obj->call($http_method, $param, $content);
			
			$logging_obj->log("DEBUG","Response::".Dumper($response));
			
			if ($response->{'status'}) 
			{
				$logging_obj->log("DEBUG","perf_params  : $perf_params :: serv_params : $serv_params :: SUCCESS \n");
			}
			else 
			{
				$logging_obj->log("EXCEPTION","Post failed:$perf_params and $serv_params \n");
			}
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : " . $@);
	}	
}

sub updateHA
{	
	my $self = shift;
	$self->updatePS();
	$self->updatePerf();
	$self->updateService();	
}

sub getContents
{
	my ($self, $file) = @_;
	my @content;
	#print "getcontents::file::$file\n";
	if((-e $file) && (-s $file))
	{
		open(PS,$file);
		while(<PS>)
		{
			chomp $_;
			push(@content, $_);
		}
		close(PS);
		if(($file =~ /perf.txt/) || ($file =~ /service.txt/))
		{
			`rm -f $file`;
		}
	}
	return @content;
}


1;
