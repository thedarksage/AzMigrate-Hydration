package RX::Migration;

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
use HTML::Entities;
use PHP::Serialization qw(serialize unserialize);

my $logging_obj  = new Common::Log();
my @rx_plan_id_list = ();
my @rx_scenario_id_list = ();
my @trg_id_list;

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
	
	if(scalar @{$data->{'ScenarioDetails'}}) 
	{
		@trg_id_list = ();
		my ($plan_id,$scenario_type) = $self->insert_application_scenario($data);
        if(scalar @trg_id_list) 
        {				
            my %tgt_hash   = map { $_, 1 } @trg_id_list;
			my @tgt_host_id_list = keys %tgt_hash;
            $self->insert_rollback_policy($plan_id,$scenario_type,\@tgt_host_id_list);
        }
	}
}

sub insert_application_scenario
{
	my $self = shift;
	my $plan_data = shift;
	my ($scenarioId,$scenarioType,$plan_id);
	
	my $app_scenario_data = $plan_data->{'ScenarioDetails'};
	my $ROLLBACK_INPROGRESS = 116;
	my $ROLLBACK_COMPLETED = 130;
	my $ROLLBACK_FAILED = 118;
	
	foreach my $data(@{$app_scenario_data}) 
	{
		$logging_obj->log("EXCEPTION","Migration insert_application_scenario data::".Dumper($data));
		$scenarioType = $data->{'scenarioType'};
		my $planId = $data->{'planId'};
		my $rxScenarioId = $data->{'scenarioId'};
		push @rx_scenario_id_list,$rxScenarioId;
		my $scenarioName = '';
		my $scenarioDescription = '';
		my $scenarioVersion = '';
		my $scenarioCreationDate = $data->{'scenarioCreationDate'};
		my $scenarioDetails = $self->{'conn'}->sql_escape_string($data->{'scenarioDetails'});
		my $validationStatus = $data->{'validationStatus'};
		my $execution_status = $data->{'executionStatus'};
		my $applicationType = $data->{'applicationType'};		
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
		my $protectionDirection = $data->{'protectionDirection'};
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
		my $protection_scenario_id;
				
		
		my $sql = "SELECT scenarioId,executionStatus,planId FROM applicationScenario WHERE rxScenarioId='$rxScenarioId'";
		my $sth_check = $self->{'conn'}->sql_query($sql);
		##
		# Insert for very first time
		##
		if((!$self->{'conn'}->sql_num_rows($sth_check)) and ($execution_status == $ROLLBACK_INPROGRESS))
		{
			my $all_diff_sync = $self->check_pairs_in_diffsync($sourceId, $targetId);
			$logging_obj->log("EXCEPTION","Migration insert_application_scenario $sourceId, $targetId, $all_diff_sync");
			
			if($all_diff_sync)
			{
				push @trg_id_list,$targetId;
                
                $plan_id = $self->insert_application_plan($plan_data,$scenarioType);
				
				my $recover_scenario_details = unserialize($data->{'scenarioDetails'});
				my $rx_scenario_id = $recover_scenario_details->{'host_Details'}->{'protectionScenarioId'};		
				my $protection_scenario_id = $self->{'conn'}->sql_get_value("applicationScenario","scenarioId","sourceId='$sourceId' AND targetId='$targetId' AND rxScenarioId='$rx_scenario_id'");
				
				my $sql = "INSERT INTO 
								applicationScenario
									(planId,
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
									rxScenarioId)  
								VALUES
									('$plan_id','$scenarioType','$scenarioName','$scenarioDescription','$scenarioVersion','$scenarioCreationDate','$scenarioDetails','$validationStatus','$execution_status','$applicationType','$applicationVersion','$srcInstanceId','$trgInstanceId','$protection_scenario_id','$resyncLimit','$autoProvision','$sourceId','$targetId','$sourceVolumes','$targetVolumes','$retentionVolumes','$reverseReplicationOption','$protectionDirection','$currentStep','$creationStatus','$isPrimary','$isDisabled','$isModified','$reverseRetentionVolumes','$volpackVolumes','$volpackBaseVolume','$isVolumeResized','$sourceArrayGuid','$targetArrayGuid','$rxScenarioId')";
								
				my $sth = $self->{'conn'}->sql_query($sql);	
				$scenarioId = $sth->{mysql_insertid};
				$logging_obj->log("EXCEPTION","Migration insert_application_scenario sql::$sql, \n scenarioId::$scenarioId");
				
				if($scenarioType eq 'Recovery')
				{
					$self->insert_src_mt_disk_mapping($data->{'DiskMapping'},$plan_id,$scenarioId);
				}	
			}
		}else {
			my $row = $self->{'conn'}->sql_fetchrow_hash_ref($sth_check);
			my $id = $row->{'scenarioId'};
			my $CXStatus = $row->{'executionStatus'};
			my $cxPlanId = $row->{'planId'};
			my $RXStatus = $execution_status;
			##
			# Retry Scenario from RX if Rollback failed
			##
			if(($RXStatus == $ROLLBACK_INPROGRESS) && ($CXStatus == $ROLLBACK_FAILED)) {
				my $sql_policy = "SELECT b.policyInstanceId AS policyInstanceId FROM policy a,policyinstance b WHERE a.scenarioId='$id' AND a.policyId=b.policyId AND b.policyState='2' AND a.policyType='48'";
				my $sth_policy = $self->{'conn'}->sql_query($sql_policy);
				if($self->{'conn'}->sql_num_rows($sth_policy)) {
					my $update_scenario = "UPDATE applicationScenario SET executionStatus='$ROLLBACK_INPROGRESS' WHERE scenarioId='$id'";
					$self->{'conn'}->sql_query($update_scenario);
				
					my $planStatus = 'Rollback InProgress';
					my $update_plan = "UPDATE applicationPlan applicationPlanStatus='$planStatus' WHERE planId='$cxPlanId'";
					$self->{'conn'}->sql_query($update_scenario);
					
					my $row_policy = $self->{'conn'}->sql_fetchrow_hash_ref($sth_policy);
					my $policyInstanceId = $row_policy->{'policyInstanceId'};
					
					my $update_policy = "UPDATE policyInstance SET policyState=3 WHERE policyInstanceId='$policyInstanceId'";
					$self->{'conn'}->sql_query($update_scenario);
				}
			}
			
		}		
	}
    return ($plan_id, $scenarioType);
}

sub insert_rollback_policy
{
	my $self = shift;
	my $planId = shift;
	my $scenario_type = shift;
    my $targetId = shift;
	my $policy_type;
    
	foreach my $trg_id(@{$targetId})
    {
        if($scenario_type  eq "Rollback")
        {
           $policy_type = 48;
        }
        else
        {
           $policy_type = 49;
        }
        my $sql = "INSERT INTO 
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
                                scenarioId,
                                dependentId,
                                policyTimestamp,
                                planId) 
                        VALUES
                                ($policy_type,
                                '$scenario_type',
                                '0',
                                2,
                                0,
                                0,
                                0,
                                0,
                                0,
                                '$trg_id',
                                0,
                                0,
                                now(),
                                '$planId')";
                                
        my $sth = $self->{'conn'}->sql_query($sql);
        my $policy_id = $sth->{mysql_insertid};
        $logging_obj->log("EXCEPTION","Migration insert_application_scenario sql::$sql, \n policy_id::$policy_id");
        
        my $sql1 = "INSERT INTO 
                            policyInstance
                                (policyId,
                                lastUpdatedTime,
                                policyState	,
                                executionLog,
                                outputLog,
                                policyStatus,
                                uniqueId,
                                dependentInstanceId,
                                hostId) 
                            VALUES
                                ('$policy_id',
                                now(),
                                3,
                                '',
                                '',
                                'Active',
                                0,
                                0,
                                '$trg_id')";
        my $sth1 = $self->{'conn'}->sql_query($sql1);
        $logging_obj->log("EXCEPTION","Migration insert_application_scenario sql::$sql1, \n policy_id::$policy_id");
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
		my $srcDisk = $data->{'srcDisk'};
		my $trgDisk = $data->{'trgDisk'};
		my $trgLunId = $data->{'trgLunId'};
		my $status = $data->{'status'};
		
			
		my $sql = "INSERT INTO srcMtDiskMapping(srcHostId,trgHostId,srcDisk,trgDisk,trgLunId,status,scenarioId,planId) VALUES('$srcHostId','$trgHostId','$srcDisk','$trgDisk','$trgLunId','$status','$scenarioId','$planId')";
		my $sth = $self->{'conn'}->sql_query($sql);
	}
}


sub getSyncedPlanIdList
{
	my $self = shift;
	my $result=[];
	
	my $app_scenario_sql = "SELECT rxPlanId FROM applicationPlan WHERE planType='CLOUD_MIGRATION'";
	my $app_sth1 = $self->{'conn'}->sql_query($app_scenario_sql);
	
	while(my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($app_sth1))
	{
		push @$result,$ref->{'rxPlanId'};
	}	
	my $input = serialize($result);
	return $input;
}

sub check_pairs_in_diffsync
{
	my $self = shift;
	my $src_host_id = shift;
	my $tgt_host_id = shift;
	my $all_diff_sync = 0;
	my $diff_sync_count = 0;
	my $pair_count = 0;
	
	my $pair_sql = "SELECT isQuasiFlag, pairId FROM srcLogicalVolumeDestLogicalVolume WHERE sourceHostId='$src_host_id' AND destinationHostId='$tgt_host_id'";
	my $pair_sth1 = $self->{'conn'}->sql_query($pair_sql);
	
	while(my $pair_data = $self->{'conn'}->sql_fetchrow_hash_ref($pair_sth1))
	{
		if($pair_data->{'isQuasiFlag'} == 2)
		{
			$diff_sync_count++;
		}
		$pair_count++
	}
	$logging_obj->log("EXCEPTION","Migration insert_application_scenario diff_sync_count::$diff_sync_count, \n pair_count::$pair_count");
	if($pair_count == $diff_sync_count && $pair_count > 0)
	{
		$all_diff_sync = 1;
	}
	return $all_diff_sync;
}

sub insert_application_plan
{
	my $self = shift;
	my $data = shift;
	my $scenarioType = shift;
	
	my $planName = $data->{'planName'};
	my $planHealth = $data->{'planHealth'};
	my $planCreationTime = $data->{'planCreationTime'};
	my $batchResync = $data->{'batchResync'};
	my $applicationPlanStatus = $data->{'applicationPlanStatus'};
	my $dependentPlanId = $data->{'dependentPlanId'};
	my $planType = $data->{'planType'};
	my $solutionType = $data->{'solutionType'};
	my $rxPlanId = $data->{'planId'};
	
	my $planId = $self->{'conn'}->sql_get_value("applicationPlan","planId","planName='$planName'");
	
	if($scenarioType eq 'Recovery')
	{
		$applicationPlanStatus = 'Recovery InProgress';
	}
	else
	{
		$applicationPlanStatus = 'Rollback InProgress';
	}
	
	if(!$planId)
	{
		my $sql = "INSERT INTO 
						applicationPlan
							(planName,
							planHealth,
							planCreationTime,
							batchResync,
							applicationPlanStatus,
							dependentPlanId,
							planType,
							solutionType,
							rxPlanId) 
						VALUES
							('$planName',
							'$planHealth',
							'$planCreationTime',
							'$batchResync',
							'$applicationPlanStatus',
							'$dependentPlanId',
							'$planType',
							'$solutionType',
							'$rxPlanId')";
		
		my $sth = $self->{'conn'}->sql_query($sql);
		$planId = $sth->{mysql_insertid};
		$logging_obj->log("EXCEPTION","Migration insert_application_scenario  \n sql::$sql \n planId::$planId");					
	}
	$logging_obj->log("EXCEPTION","Migration insert_application_scenario  \n planId::$planId");
	return $planId;
}

sub process_job
{
	my $self = shift;
	my $result = shift;
	$logging_obj->log("DEBUG","Entered into Migration process_job");
	my $plan_status;	
	
	my $res_ref = ref($result);
	
	eval
	{
		if($res_ref eq "ARRAY")
		{
			foreach my $var(@$result) 
			{
				$self->insertPlans($var);
				push @rx_plan_id_list,$var->{'planId'};
			}
			$self->clean_unreported_plans();
		}
		
		$plan_status = $self->getPlanStatus();
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Migration process_job".$@);
	}
	return $plan_status;	
}

sub getPlanStatus
{
	my $self = shift;
	my $final_result;
	
	my $sql = "SELECT planId, rxPlanId, applicationPlanStatus, errorMessage FROM applicationPlan WHERE planType='CLOUD_MIGRATION'";
	my $plan_data = $self->{'conn'}->sql_exec($sql);
	foreach my $ref (@{$plan_data})		  
	{
		my $recovery_plan_id = $ref->{'planId'};
        my $plan_status = $ref->{'applicationPlanStatus'};
		my $protection_scenario_id =  $self->{'conn'}->sql_get_value('applicationScenario', 'referenceId', "planId='$recovery_plan_id' AND applicationType IN ('AWS','Azure','AZURE')");
		my $pair_exists =  $self->{'conn'}->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'count(*)', "scenarioId='$protection_scenario_id'");

		if(($plan_status eq 'Rollback Failed') or (($plan_status eq 'Rollback Completed') and ($pair_exists == 0)))
		{
			push(@{$final_result},$ref);
		}
	}
	return $final_result;
}

sub clean_unreported_plans
{
	my $self = shift;
	if(scalar @rx_plan_id_list) {
		my $rx_plan_list = join("','",@rx_plan_id_list);
		
		my $delete_plan = "DELETE FROM applicationPlan WHERE rxPlanId NOT IN ('$rx_plan_list') AND planType='CLOUD_MIGRATION'";
		$self->{'conn'}->sql_query($delete_plan);
	}
	
	if(scalar @rx_scenario_id_list) {
		my $rx_scenario_list = join("','",@rx_scenario_id_list);
		
		my $delete_plan = "DELETE FROM applicationScenario WHERE rxScenarioId NOT IN ('$rx_scenario_list') AND scenarioType='Rollback'";
		$self->{'conn'}->sql_query($delete_plan);
	}
	
}	
1;