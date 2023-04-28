package RX::CloudProtection;

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
use Common::Log;
use Common::DB::Connection;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use TimeShotManager;

my $logging_obj  = new Common::Log();
my $trg_id_list;
my $src_id_list;
my $dvacp_consistency_set;

sub new
{
	my ($class) = @_;
	my $self = {};
		
	$self->{'conn'} = new Common::DB::Connection();	
	
	bless($self, $class);	
}

sub insertPlans
{
	my $self = shift;
	my $data = shift;
	
	$trg_id_list = [];
	$src_id_list = [];
	#insert application Plans
	#insert application scenarios
	#insert srcMtDiskMapping
	#insert policies
	my $plan_id = $self->insert_application_plan($data);
	if($plan_id){
		if(scalar @{$data->{'ScenarioDetails'}}) {
			my $scenario_id = $self->insert_application_scenario($data->{'ScenarioDetails'},$plan_id);
			if(scalar @$trg_id_list) {
				my %hash   = map { $_, 1 } @$trg_id_list;
				my @trg_host_id_list = keys %hash;
				my %src_hash   = map { $_, 1 } @$src_id_list;
				my @src_host_id_list = keys %src_hash;
				$self->insert_prepare_target($plan_id,\@trg_host_id_list,$scenario_id);
				
				#If the consistency type is DVACP then insert the policies respectively.
				if($dvacp_consistency_set == 1)
				{
					$self->insert_dvacp_consistency_policy(\@src_host_id_list,$plan_id);
				}
				
				if($data->{'CXNATIP'})
				{
					$self->update_cx_nat_ip(\@src_host_id_list,\@trg_host_id_list,$data->{'CXNATIP'});
				}	
			}
		}
	}
}

sub update_cx_nat_ip
{
	my $self = shift;
	my $src_id_list = shift;
	my $trg_id_list = shift;
	my $cx_nat_ip = shift;
	my @host_id = (@{$src_id_list},@{$trg_id_list});
	
	my $all_host_id = join("','",@host_id);
	
	my $update_host_sql = "UPDATE hosts SET usecxnatip=1, cx_natip='$cx_nat_ip' WHERE id IN ('$all_host_id')";
	$self->{'conn'}->sql_query($update_host_sql);
}

sub insert_application_plan
{
	my $self = shift;
	my $data = shift;
	
	my $result = 0;
	my $SCENARIO_DELETE_PENDING = 101;
	
	my $planName = $data->{'planName'};
	my $planHealth = $data->{'planHealth'};
	my $planCreationTime = $data->{'planCreationTime'};
	my $batchResync = $data->{'batchResync'};
	my $applicationPlanStatus = $data->{'applicationPlanStatus'};
	my $dependentPlanId = $data->{'dependentPlanId'};
	my $planType = $data->{'planType'};
	my $solutionType = $data->{'solutionType'};
	my $rxPlanId = $data->{'planId'};
	
	
	my $check_sql = "SELECT planId,applicationPlanStatus FROM applicationPlan where rxPlanId='$rxPlanId'";
	my $sth_check = $self->{'conn'}->sql_query($check_sql);
	if((!$self->{'conn'}->sql_num_rows($sth_check)) and ($applicationPlanStatus eq 'Configuration Success'))
	{
		##
		# Insert Plan for very first Time
		##
		$applicationPlanStatus = 'Prepare Target Pending';
		my $sql = "INSERT INTO applicationPlan(planName,planHealth,planCreationTime,batchResync,applicationPlanStatus,dependentPlanId,planType,solutionType,rxPlanId) VALUES('$planName','$planHealth','$planCreationTime','$batchResync','$applicationPlanStatus','$dependentPlanId','$planType','$solutionType','$rxPlanId')";
		my $sth = $self->{'conn'}->sql_query($sql);
		my $planId = $sth->{mysql_insertid};
		$result = $planId;
	}else{
		my $row = $self->{'conn'}->sql_fetchrow_hash_ref($sth_check);
		my $id = $row->{'planId'};
		##
		# In case of Prepare Target Failed and Retry from RX
		##
		if(($row->{'applicationPlanStatus'} eq 'Prepare Target Failed') and ($applicationPlanStatus eq 'Configuration Success')) {
			$applicationPlanStatus = 'Prepare Target Pending';
			#Update the applicationPlanStatus
			my $update_plan = "UPDATE applicationPlan SET applicationPlanStatus='$applicationPlanStatus',errorMessage='' WHERE planId='$id'";
			my $sth_update = $self->{'conn'}->sql_query($update_plan);
			
			#Update the Policy
			my $update_policy = "UPDATE policyInstance SET policyState='3',uniqueId='0',executionLog='' WHERE policyState='2' AND policyId IN (SELECT policyId FROM policy WHERE policyType='5' AND planId='$id')";
			my $sth_update_policy = $self->{'conn'}->sql_query($update_policy);
		
		}
		##
		# 1) In case of Plan marked for Deletion from RX
		# 2) If all the VM's in the plan are migrated then mark the plan for deletion
		##
		elsif((($row->{'applicationPlanStatus'} ne 'Delete InProgress') ) and (($applicationPlanStatus eq 'Delete Pending') or ($applicationPlanStatus eq 'Recovery Success'))) {
			
			#mark pairs for delete
			my $del_pairs = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status='31' WHERE planId='$id'";
			my $del_pairs_status = $self->{'conn'}->sql_query($del_pairs);
			
			#mark scenario for delete
			my $del_scenario = "UPDATE applicationScenario SET executionStatus='$SCENARIO_DELETE_PENDING' WHERE planId='$id'";
			my $del_scenario_status = $self->{'conn'}->sql_query($del_scenario);
			
			#Update planstatus to Inprogress
			my $planStatus = 'Delete InProgress';
			my $update_plan = "UPDATE applicationPlan SET applicationPlanStatus='$planStatus',errorMessage='' WHERE planId='$id'";
			my $sth_update = $self->{'conn'}->sql_query($update_plan);
		}else{
			$result = $id;
		}
	}
	return $result;
}

sub insert_application_scenario
{
	my $self = shift;
	my $app_scenario_data = shift;
	
	my $planId = shift;
	my $scenarioId;
	my $PREPARE_TARGET_PENDING = 98;
	my $SCENARIO_DELETE_PENDING = 101;
	my $MIGRATION_SUCCESS = 103;
	
	foreach my $data(@{$app_scenario_data}) {
		my $scenarioType = $data->{'scenarioType'};
		my $scenarioName = '';
		my $scenarioDescription = '';
		my $scenarioVersion = '';
		my $scenarioCreationDate = '';
		my $scenarioDetails = $self->{'conn'}->sql_escape_string($data->{'scenarioDetails'});
		my $validationStatus = $data->{'validationStatus'};
		my $executionStatus = $data->{'executionStatus'};
		my $applicationType = $data->{'applicationType'}; #Needs to come from RX
		my $applicationVersion = '';
		my $srcInstanceId = '';
		my $trgInstanceId = '';
		my $referenceId = '';
		my $resyncLimit = '';
		my $autoProvision = '';
		my $sourceId = $data->{'sourceId'};
		my $targetId = $data->{'targetId'};
		my $sourceVolumes = '';
		my $targetVolumes = '';
		my $retentionVolumes = '';
		my $reverseReplicationOption = '';
		my $protectionDirection = $data->{'protectionDirection'}; # Needs to come from RX
		my $currentStep = $data->{'currentStep'};
		my $creationStatus = $data->{'creationStatus'};
		my $isPrimary = '';
		my $isDisabled = '';
		my $isModified = '';
		my $reverseRetentionVolumes = '';
		my $volpackVolumes = '';
		my $volpackBaseVolume = '';
		my $isVolumeResized = '';
		my $sourceArrayGuid = '';
		my $targetArrayGuid = '';
		my $rxScenarioId = $data->{'scenarioId'};
		my $protection_scenario_details = unserialize($data->{'scenarioDetails'});		
		
		my $sql = "SELECT scenarioId,executionStatus FROM applicationScenario WHERE rxScenarioId='$rxScenarioId'";
		my $sth_check = $self->{'conn'}->sql_query($sql);
		
		##
		# Insert for very first time
		##
		if((!$self->{'conn'}->sql_num_rows($sth_check)) and ($executionStatus == $PREPARE_TARGET_PENDING)) 
		{
			push @$trg_id_list,$targetId;
			push @$src_id_list,$sourceId;
			my $sql = "INSERT INTO applicationScenario(planId,
							scenarioType,
							scenarioName,
							scenarioDescription,
							scenarioVersion,
							scenarioCreationDate,
							scenarioDetails,
							validationStatus,
							executionStatus,
							applicationType,
							applicationVersion,
							srcInstanceId,
							trgInstanceId,
							referenceId,
							resyncLimit,
							autoProvision,
							sourceId,
							targetId,
							sourceVolumes,
							targetVolumes,
							retentionVolumes,
							reverseReplicationOption,
							protectionDirection,
							currentStep,
							creationStatus,
							isPrimary,
							isDisabled,
							isModified,
							reverseRetentionVolumes,
							volpackVolumes,
							volpackBaseVolume,
							isVolumeResized,
							sourceArrayGuid,
							targetArrayGuid,
							rxScenarioId)  VALUES('$planId','$scenarioType','$scenarioName','$scenarioDescription','$scenarioVersion','$scenarioCreationDate','$scenarioDetails','$validationStatus','$executionStatus','$applicationType','$applicationVersion','$srcInstanceId','$trgInstanceId','$referenceId','$resyncLimit','$autoProvision','$sourceId','$targetId','$sourceVolumes','$targetVolumes','$retentionVolumes','$reverseReplicationOption','$protectionDirection','$currentStep','$creationStatus','$isPrimary','$isDisabled','$isModified','$reverseRetentionVolumes','$volpackVolumes','$volpackBaseVolume','$isVolumeResized','$sourceArrayGuid','$targetArrayGuid','$rxScenarioId')";
			my $sth = $self->{'conn'}->sql_query($sql);
			
			$scenarioId = $sth->{mysql_insertid};
			
			my $is_linux = $self->{'conn'}->sql_get_value("hosts","osFlag","id='$sourceId'");
			if($protection_scenario_details->{'globalOption'}->{'vacp'} eq 'Normal' or ($is_linux != 1))
			{
				#Normal consistency is set.				
				$self->insert_consistency_policy($scenarioId,$planId,$sourceId);
			}
			else			
			{				
				$dvacp_consistency_set = 1;
			}
			$self->insert_src_mt_disk_mapping($data->{'DiskMapping'},$planId,$scenarioId);
			
			#Updating PS NAT IP value based on the user provided IP Addess.
			my $trg_nat_ip = $protection_scenario_details->{'globalOption'}->{'trgNATIPflag'};
			my $src_nat_ip = $protection_scenario_details->{'globalOption'}->{'srcNATIPflag'};
			if($src_nat_ip || $trg_nat_ip)
			{
				my $nat_ip = $protection_scenario_details->{'globalOption'}->{'trgNATIP'};
				my $ps_id = $protection_scenario_details->{'globalOption'}->{'processServerid'};
				$self->update_ps_nat_ip($ps_id,$nat_ip,$sourceId);	
			}
		}
		else
		{
			my $row = $self->{'conn'}->sql_fetchrow_hash_ref($sth_check);
			my $id = $row->{'scenarioId'};
			my $CXStatus = $row->{'executionStatus'};
			my $RXStatus = $executionStatus;
			##
			# 1) If Scenario is marked for delete from RX
			# 2) If the VM is Migrated then delete the Scenario from CX
			##
			if(($CXStatus!=$SCENARIO_DELETE_PENDING) and (($RXStatus == $SCENARIO_DELETE_PENDING) or ($RXStatus == $MIGRATION_SUCCESS)))
			{
				#mark pairs for delete
				my $del_pairs = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status='31' WHERE scenarioId='$id'";
				my $del_pairs_status = $self->{'conn'}->sql_query($del_pairs);
			
				#mark scenario for delete
				my $del_scenario = "UPDATE applicationScenario SET executionStatus='$SCENARIO_DELETE_PENDING' WHERE scenarioId='$id'";
				my $del_scenario_status = $self->{'conn'}->sql_query($del_scenario);
			}
		}
	}
	
	return $scenarioId;
}

sub update_ps_nat_ip
{
	my $self = shift;
	my $ps_id = shift;
	my $nat_ip = shift;
	my $sourceId= shift;
	
	my $ps_nat_ip_rows = $self->{'conn'}->sql_get_value("processServer","count(*)","ps_natip='$nat_ip' AND processServerId='$ps_id'");
	
	if(!$ps_nat_ip_rows)
	{
		my $sql = "UPDATE processServer SET ps_natip='$nat_ip' WHERE processServerId='$ps_id'";
		my $sth = $self->{'conn'}->sql_query($sql);
	}	
}

sub insert_src_mt_disk_mapping
{
	my $self = shift;
	my $mapping_info = shift;
	
	my $planId = shift;
	my $scenarioId = shift;
	foreach my $data(@$mapping_info) {
		my $srcHostId = $data->{'srcHostId'};
		my $trgHostId = $data->{'trgHostId'};
		my $srcDisk = $self->{'conn'}->sql_escape_string($data->{'srcDisk'});
		my $trgDisk = $self->{'conn'}->sql_escape_string($data->{'trgDisk'});
		my $trgLunId = $data->{'trgLunId'};
		my $status = 'Disk Layout Pending';
		
		my $sql = "INSERT INTO srcMtDiskMapping(srcHostId,trgHostId,srcDisk,trgDisk,trgLunId,status,scenarioId,planId) VALUES('$srcHostId','$trgHostId','$srcDisk','$trgDisk','$trgLunId','$status','$scenarioId','$planId')";
		my $sth = $self->{'conn'}->sql_query($sql);
	}
}

sub insert_prepare_target
{
	my $self = shift;
	my $planId = shift;	
	my $trg_ids = shift;
	my $scenarioId = shift;

	
	foreach my $trg_id(@{$trg_ids}) {
		
		my $sql = "INSERT into policy(policyType,policyName,policyParameters,policyScheduledType,		policyRunEvery,nextScheduledTime,policyExcludeFrom,policyExcludeTo,applicationInstanceId,hostId,	scenarioId,dependentId,policyTimestamp,planId) 
		VALUES(5,'PrepareTarget','0',2,0,0,0,0,0,'$trg_id','$scenarioId',0,now(),'$planId')";
		my $sth = $self->{'conn'}->sql_query($sql);
		my $policy_id = $sth->{mysql_insertid};
		
		my $sql1 = "INSERT INTO policyInstance(policyId,lastUpdatedTime,policyState	,executionLog,outputLog,policyStatus,uniqueId,dependentInstanceId,hostId) VALUES('$policy_id',now(),3,'','','Active',0,0,'$trg_id')";
		my $sth1 = $self->{'conn'}->sql_query($sql1);
	}
}

sub insert_consistency_policy
{
	my $self = shift;
	my $scenarioId = shift;	
	my $planId = shift;
	my $src_id = shift;
	
	my $sql = "INSERT into policy(policyType,policyName,policyParameters,policyScheduledType,		policyRunEvery,nextScheduledTime,policyExcludeFrom,policyExcludeTo,applicationInstanceId,hostId,	dependentId,policyTimestamp,scenarioId,planId) 
	VALUES(4,'Consistency','0','1','900',0,0,0,0,'$src_id',0,now(),'$scenarioId','$planId')";
	my $sth = $self->{'conn'}->sql_query($sql);
	my $policy_id = $sth->{mysql_insertid};
	
	my $sql1 = "INSERT INTO policyInstance(policyId,lastUpdatedTime,policyState	,executionLog,outputLog,policyStatus,uniqueId,dependentInstanceId,hostId) VALUES('$policy_id',now(),3,'','','Active',0,0,'$src_id')";
	my $sth1 = $self->{'conn'}->sql_query($sql1);
	
}

sub insert_dvacp_consistency_policy
{
	my $self = shift;
	my $src_id_list = shift;
	my $plan_id = shift;
	my $consistency_options;
	my ($mn,$cn) = '';
	my(@win_ips,@win_host_ids,@linux_host_ids);
	
	$consistency_options->{'runTime'} = "15";
	$consistency_options->{'exceptFrom1'} = "0";
	$consistency_options->{'exceptFrom2'} = "0";
	$consistency_options->{'exceptTo1'} = "0";
	$consistency_options->{'exceptTo2'} = "0";
	$consistency_options->{'interval'} = "1";
	$consistency_options->{'run_every_interval'} = "900";
	
	my $all_host_id = join("','",@{$src_id_list});
	
	my $sql = "SELECT osFlag, ipaddress, id FROM hosts WHERE id IN ('$all_host_id')";
	my $sth = $self->{'conn'}->sql_query($sql);
	
	while(my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($sth))
	{
		if($ref->{'osFlag'} == 1)
		{
			push(@win_ips,$ref->{'ipaddress'});
			push(@win_host_ids,$ref->{'id'});
		}
		else
		{
			push(@linux_host_ids,$ref->{'id'});
		}
	}
	
	my $vacp_command = "vacp.exe -systemlevel";
	$consistency_options->{'os_flag'} = 1;	
	if(scalar(@win_ips) > 1 && scalar(@linux_host_ids) == 0)
	{		
		$mn = shift(@win_ips);
		$cn = join(",",@win_ips);
		$vacp_command .= " -distributed -mn $mn -cn $cn";		
	}	
	$consistency_options->{'option'} = $vacp_command;
	if(scalar(@win_ips) >= 1) { $self->insert_policy($consistency_options,\@win_host_ids,$mn,$cn,$plan_id) };
	
	#my $vacp_command = "vacp -systemlevel";
	#$consistency_options->{'option'} = $vacp_command;	
	#if(scalar(@linux_host_ids) >= 1) { $self->insert_policy($consistency_options,\@linux_host_ids,$mn,$cn,$plan_id); }
}

sub insert_policy
{
	my $self = shift;
	my $consistency_options = shift;
	my $host_ids = shift;
	my $master_node = shift;
	my $co_nodes = shift;
	my $plan_id = shift;
	
	my $src_host_ids = join(",",@{$host_ids});	
	
	$consistency_options = serialize($consistency_options);
	
	my $plan_name = $self->{'conn'}->sql_get_value("applicationPlan","planName","planId='$plan_id'");
	my $group_name = $plan_name."_dvacp_consistency".$plan_id;
	
	my $sql = "INSERT 
						into 
					policy
							(policyType,
							 policyName,
							 policyParameters,
							 policyScheduledType,		
							 policyRunEvery,
							 nextScheduledTime,
							 policyExcludeFrom,
							 policyExcludeTo,
							 applicationInstanceId,
							 hostId,
							 dependentId,
							 policyTimestamp,
							 scenarioId,
							 planId) 
						VALUES
							(4,'Consistency','$consistency_options','2',0,0,0,0,0,'$src_host_ids',0,now(),'0','$plan_id')";
	my $sth = $self->{'conn'}->sql_query($sql);
	my $policy_id = $sth->{mysql_insertid};
	
	foreach my $src_id(@{$host_ids})
	{
		my $sql1 = "INSERT INTO policyInstance(policyId,lastUpdatedTime,policyState	,executionLog,outputLog,policyStatus,uniqueId,dependentInstanceId,hostId) VALUES('$policy_id',now(),3,'','','Active',0,0,'$src_id')";
		my $sth1 = $self->{'conn'}->sql_query($sql1);
	}

	my $insert_sql = "INSERT INTO 
							consistencyGroups
							(
								policyId,
								groupName,
								masterNode,
								coordinateNodes
							)
							VALUES
							(
								$policy_id,
								'$group_name',
								'$master_node',
								'$co_nodes'
							)";
	my $sth1 = $self->{'conn'}->sql_query($insert_sql);	
}

sub getSyncedPlanIdList
{
	my $self = shift;
	my $result=[];
	
	my $sql = "SELECT rxPlanId FROM applicationPlan WHERE planType='CLOUD'";
	my $sth = $self->{'conn'}->sql_query($sql);
	while(my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($sth))
	{
		push @$result,$ref->{'rxPlanId'};
	}
	
	my $input = serialize($result);
	return $input;
}

sub process_job
{
	my $self = shift;
	my $result = shift;
	
	my $plan_status;
	
	my $res_ref = ref($result);
	$logging_obj->log("DEBUG","Entered into CloudProtection process_job");
	eval{
		if($res_ref eq "ARRAY")
		{
			foreach(@$result) {
				$self->insertPlans($_);
			}
		}
		
		$plan_status = $self->getPlanStatus();
	};
	if($@)
	{		
		$logging_obj->log("EXCEPTION","CloudProtection process_job ".$@);
	}
	
	return $plan_status;
}

sub getPlanStatus
{
	my $self = shift;
	my $final_result = [];
				
	my $sql = "SELECT planId, rxPlanId, applicationPlanStatus, errorMessage FROM applicationPlan WHERE planType='CLOUD'";
	my $rs = $self->{'conn'}->sql_exec($sql);
	if(defined $rs)
	{
		foreach my $ref (@{$rs})
		{
			my $plan_details = {};
			$plan_details->{'PlanDetails'} = $ref;
			my $plan_id = $ref->{"planId"};
			my $sql_scenario = "SELECT rxScenarioId,planId,executionStatus FROM applicationScenario WHERE planId = '$plan_id'";
			my $rs1 = $self->{'conn'}->sql_exec($sql_scenario);
			$plan_details->{'ScenarioDetails'} = $rs1;
			push(@{$final_result},$plan_details);
		}
	}
	return $final_result;
}


1;