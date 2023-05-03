package RX::ESXJobs;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use DBI();
use File::Copy;
use LWP::UserAgent;
use HTTP::Request::Common;
use Utilities;
use Net::FTP;
use Common::Log;
use Common::DB::Connection;
use RX::CommonFunctions;
use RX::CloudProtection;
use RX::Migration;
use RX::PushInstall;
use ESX::Recovery;
use ESX::Discovery;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);

my $logging_obj  = new Common::Log();

sub new
{
	my ($class) = @_;
	my $self = {};
		
	$self->{'recovery'} = new ESX::Recovery();	
	$self->{'common'} = new RX::CommonFunctions();	
	$self->{'pushinstall'} = new RX::PushInstall();	
	$self->{'protection'} = new RX::CloudProtection();	
	$self->{'inventory'} = new ESX::Discovery();
	$self->{'migration'} = new RX::Migration();		
	$self->{'conn'} = new Common::DB::Connection();	
	
	$self->{'cx_install_path'} = $self->{'common'}->{'cx_install_path'};
	
	$self->{'cs_ip'} = $self->{'common'}->{'cs_ip'};
	$self->{'rx_cs_ip'} = $self->{'common'}->{'rx_cs_ip'};
	$self->{'cs_port'} = $self->{'common'}->{'cs_port'};
	
	$self->{'cs_api_url'} = $self->{'common'}->{'cs_api_url'};
	$self->{'cx_key'} = $self->{'conn'}->sql_get_value('rxSettings', 'ValueData', "ValueName='RX_KEY'");
	bless($self, $class);	
}

sub rx_process_recovery
{	
	my ($self) = @_;

	my $num_esx_records = $self->{'conn'}->sql_get_value('applicationPlan', 'count(*)', "solutionType='ESX'");
	if(!$num_esx_records)
	{
		$logging_obj->log("INFO","rx_process_recovery : No ESX Plans exist on this server");
	}
	else
	{
		$self->process_job("recovery");
	}
	return;
}

sub rx_process_drdrill
{	
	my ($self) = @_;
	
	my $num_esx_records = $self->{'conn'}->sql_get_value('applicationPlan', 'count(*)', "solutionType='ESX'");
	if(!$num_esx_records)
	{
		$logging_obj->log("INFO","rx_process_drdrill : No ESX Plans exist on this server");
	}
	else
	{	
		$self->process_job("drdrill");
	}
	return;
}

sub rx_process_protection
{	
	my ($self) = @_;
	
	my $input = $self->{'protection'}->getSyncedPlanIdList();
	$self->process_job("protection", $input);
	
	return;
}

sub rx_process_inventory
{	
	my ($self) = @_;
	
	$self->process_job("inventory");
	return;
}

sub rx_process_pushinstall
{	
	my ($self) = @_;
	
	$self->process_job("pushinstall");
	return;
}

sub rx_process_migration
{	
	my ($self) = @_;
	my $scenario_list = $self->{'migration'}->getSyncedPlanIdList();
	$self->process_job("migration",$scenario_list);
	return;
}

sub process_job
{	
	my ($self, $job_type, $input) = @_;
	$logging_obj->log("DEBUG","Entered into process_job");	
	
	eval
	{
		if(!$self->{'cx_key'})
		{
			$logging_obj->log("EXCEPTION","process_job : Not registered to RX");
			return;
		}
		
		my @update_params = ($self->{'cx_key'}, $job_type, $input);	
		my $result;
		
		$result = $self->{'common'}->make_xml_rpc_call('report.getCsJobs', \@update_params);
		
		my $res_type = ref($result);
		if($res_type eq "HASH")
		{	
			if(($result && $result->{'faultCode'}) || $result eq undef) 
			{
				$logging_obj->log("EXCEPTION","XML RPC call - getCsJobs failed.");
			}	
		}
		
		my $recoveryUpdate = $self->{$job_type}->process_job($result);							
		$self->exit_progress($job_type,$recoveryUpdate);
	};
	if($@)
	{		
		$logging_obj->log("EXCEPTION","process_job ".$@);
	}
}

sub check_unserialize
{
	my($reference) = @_;
	
	return ($reference) ? unserialize($reference) : undef;
}

sub exit_progress
{			
	my $self = shift;
	my $job_type =  shift;
	my $job_update =  shift;
	$logging_obj->log("DEBUG","Entered into exit_progress");	
	
	my $response;
	$response->{"cxKey"} = $self->{'cx_key'};
	$response->{"jobType"} = $job_type;
	$response->{"jobDetails"} = serialize($job_update);
	
	my $update_resp = $self->{'common'}->make_xml_rpc_call('report.updateCsJob', $response);		
	$logging_obj->log("DEBUG","exit_progress : updateCSResponse : ".Dumper($update_resp));
}

1;