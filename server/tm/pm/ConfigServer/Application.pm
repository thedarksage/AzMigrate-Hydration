#$
#=================================================================
# FILENAME
#   Application.pm
#
# DESCRIPTION
#    This perl module is responsible for Application monitoring 
#
# HISTORY
#     27 Feb 2010  cpadhi    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================
package ConfigServer::Application;

use DBI;
use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Utilities;
use Common::Log;
use POSIX qw(strftime);
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use XML::Simple;
my $logging_obj = new Common::Log();
my $max_attempts = 5;
my $MDS_BACKEND_ERRORS_ID = Common::Constants::MDS_BACKEND_ERRORS;
my $MDS_BACKEND_ERROR_CODE = Common::Constants::MDS_BACKEND_ERROR_CODE;

#
# Constructor. Create a new object of the class and pass
# all required args from outside.
#
sub new 
{
   my ($class, %args) = @_;
   
   my $self = {};
   $self->{'conn'} = $args{'conn'};
   $self->{'csData'} = $args{'csData'};
   $self->{'conn'}->{'mysql_auto_reconnect'} = 1;
   $self->{'amethyst_vars'} = Utilities::get_ametheyst_conf_parameters();
   $self->{'cs_installation_path'} = Utilities::get_cs_installation_path();

   bless ($self, $class);
}
sub writeLog
{
    my($self,$actionMsg) = @_;
	
	#Capture DateTime in the format of:: 2006-12-11 05:25:26*
    my $now = strftime('%Y-%m-%d %H:%M:%S',localtime);                
   
	my $sql_insert_query = "INSERT INTO
			    auditLog(
				    userName,
				     dateTime,
				     ipAddress,
				     details)
			     VALUES (
				 'System',
				 '$now',
				 '',
				 '$actionMsg')";
	
	$self->{'conn'}->sql_query($sql_insert_query);
} 
sub updateScenario
{
	my $self = shift;
	
	my $VALIDATION_INPROGRESS = 91;
	my $VALIDATION_SUCCESS = 92;
	my $VALIDATION_FAILED = 93;
	my $PREPARE_TARGET_PENDING = 98;
	my $FAILOVER_INPROGRESS = 102;
	my $FAILBACK_INPROGRESS = 105;
	my @values;
	my $sql = "SELECT
					scenarioId,
					planId,
					scenarioType,
					applicationType,
					sourceId,
					protectionDirection,
					targetId,
					scenarioDetails		
				FROM
					applicationScenario
				WHERE
					executionStatus = '$VALIDATION_INPROGRESS'";
	my $rs = $self->{'conn'}->sql_exec($sql);

	if(defined $rs)
	{
		foreach my $ref (@{$rs})
		{
			my $scenario_id = $ref->{"scenarioId"};
			my $plan_id = $ref->{"planId"};
			my $scenario_type = $ref->{"scenarioType"};
			my $app_type = $ref->{"applicationType"};
			my $source_id = $ref->{"sourceId"};
			my $target_id = $ref->{"targetId"};
			my $protection_direction = $ref->{"protectionDirection"};
			my $request_xml = $ref->{"scenarioDetails"};
			my $parse = new XML::Simple();
			my $parsed_data = $parse->XMLin($request_xml);
			my $plan_properties = unserialize($parsed_data->{'data'});
			my $forward_solution_type = $plan_properties->{'forwardSolutionType'};
			if (($protection_direction eq 'forward') || ($scenario_type eq 'Planned Failover'))
			{
				@values = split(',', $source_id);
			}
			else
			{
				@values = split(',', $target_id);
			}
			if($app_type eq 'BULK' and $scenario_type eq 'DR Protection' and $protection_direction eq 'reverse')
			{
				@values = split(',', $plan_properties->{'sourceHostId'});
			}	
			my $number_of_nodes = scalar(@values);
			$number_of_nodes = $number_of_nodes + 1;
			

			my $policy_type;
			
			my $solution_type = $self->{'conn'}->sql_get_value('applicationPlan', 'solutionType', "planId = '$plan_id'");
			
			$policy_type = "('2','3')";
			if($scenario_type eq 'Planned Failover' || $scenario_type eq 'Failback' || $scenario_type eq 'Fast Failback')
			{
				$policy_type = "('16','17')";
			}
			elsif($scenario_type eq 'Unplanned Failover')
			{
				$policy_type = "('17')";
			}
			# elsif($scenario_type eq 'Fast Failback')
			# {
				# $policy_type = "('16')";
			# }
			
			$sql = "SELECT
						b.policyState AS state
					FROM
						policy a,
						policyInstance b
					WHERE
						a.policyId = b.policyId AND
						a.policyScheduledType = '2' AND
						a.policyType IN $policy_type AND
						a.scenarioId = '$scenario_id' AND
						b.policyState NOT IN ('3','4')";
			
			my $result = $self->{'conn'}->sql_exec($sql);
			
			my $success_flag = 0;
			my $failed_flag = 0;
			foreach my $row (@{$result})
			{
				if($row->{"state"} == 1)
				{
					$success_flag = $success_flag+1;
				}
				elsif($row->{"state"} == 2)
				{
					$failed_flag = 1;
				}
			}
			# Below condition is only for fast failback in case of file server
			if(($scenario_type eq 'Fast Failback') and ($app_type eq 'File Servers'))
			{
				if($failed_flag == 1)
				{
					$success_flag = 0;
				}
				elsif($success_flag == 2)
				{
					$success_flag = 1;
				}
			}
			
			
			if ($forward_solution_type eq 'PRISM')
			{
				if ($scenario_type eq 'DR Protection' || $scenario_type eq 'Planned Failover' || $scenario_type eq 'Backup Protection')
				{
					if ($success_flag == $number_of_nodes)
					{
						$success_flag = 2;
					}
					else
					{
						$success_flag = 1;
					}
				}	
			}	
			
			
			if($scenario_type eq 'DR Protection' and $app_type ne 'BULK')
			{
				if ($forward_solution_type eq 'PRISM')
				{
					# $logging_obj->log("DEBUG","updateScenario : scenario_type1 : $scenario_type");
					# if (Utilities::isWindows())
					# {    
						# my $node_selection = $self->{'cs_installation_path'}."\\home\\svsystems\\admin\\web\\node_status.php";
						# `$self->{'amethyst_vars'}->{'PHP_PATH'} "$node_selection" "$scenario_id" "$plan_id"`;      
					# }
					# else
					# {
						# if (-e "/usr/bin/php")
						# {
							# $logging_obj->log("DEBUG","updateScenario : scenario_typephp 1 : $scenario_id  $plan_id");
							# `cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php node_status.php  "$scenario_id" "$plan_id"`;
						# }
						# elsif(-e "/usr/bin/php5")
						# {
							# $logging_obj->log("DEBUG","updateScenario : scenario_typephp 5 : $scenario_id  $plan_id");
							# `cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php5 node_status.php  "$scenario_id" "$plan_id"`;
						# }
					# }
				}
				else
				{
					if ($success_flag == 2) 
					{
						my $execution_status = $PREPARE_TARGET_PENDING;
						my $update_scenario = "UPDATE
												applicationScenario
										   SET
												executionStatus = '$execution_status'
										   WHERE
												scenarioId='$scenario_id' AND
												executionStatus = '$VALIDATION_INPROGRESS'";
						my $update_rs = $self->{'conn'}->sql_query($update_scenario);
						
						my $update_policy = "UPDATE
												policy
											SET
												policyScheduledType = '2'
											WHERE
												policyScheduledType = '0' AND
												scenarioId='$scenario_id' AND
												policyType = '5'";
						my $update_policy_rs = $self->{'conn'}->sql_query($update_policy);
					}
					elsif($failed_flag == 1)
					{
						my $execution_status = $VALIDATION_FAILED;
						my $update_scenario = "UPDATE
												applicationScenario
										   SET
												executionStatus = '$execution_status'
										   WHERE
												scenarioId='$scenario_id' AND
												executionStatus = '$VALIDATION_INPROGRESS'";
						my $update_rs = $self->{'conn'}->sql_query($update_scenario);
					}	
				}	
			}
			elsif(($success_flag == 2) and (($scenario_type eq 'Backup Protection') or ($scenario_type eq 'DR Protection')))
			{
				my $update_policy = "UPDATE
										policy
									SET
										policyScheduledType = '1'
									WHERE
										policyScheduledType = '0' AND
										scenarioId='$scenario_id' AND
										policyType IN $policy_type";
				my $update_policy_rs = $self->{'conn'}->sql_query($update_policy);
				
				if (Utilities::isWindows())
				{    
					my $insert_pairs = $self->{'cs_installation_path'}."\\home\\svsystems\\admin\\web\\insert_application_pairs.php";
					`$self->{'amethyst_vars'}->{'PHP_PATH'} "$insert_pairs" "$scenario_id" "$plan_id"`;      
				}
				else
				{
					if (-e "/usr/bin/php")
					{
						`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php insert_application_pairs.php  "$scenario_id" "$plan_id"`;
					}
					elsif(-e "/usr/bin/php5")
					{
						`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php5 insert_application_pairs.php  "$scenario_id" "$plan_id"`;
					}
				}
			}
			elsif(($scenario_type eq 'Planned Failover') or ($scenario_type eq 'Failback'))
			{
				$logging_obj->log("DEBUG","updateScenario : scenario_typephp 5 : $scenario_id  $plan_id");
				if ($solution_type eq 'PRISM')
				{
					# $logging_obj->log("DEBUG","updateScenario : scenario_type1 : $scenario_type");
					# if (Utilities::isWindows())
					# {    
						# my $node_selection = $self->{'cs_installation_path'}."\\home\\svsystems\\admin\\web\\node_status.php";
						# `$self->{'amethyst_vars'}->{'PHP_PATH'} "$node_selection" "$scenario_id" "$plan_id"`;    
					# }
					# else
					# {
						# if (-e "/usr/bin/php")
						# {
							# $logging_obj->log("DEBUG","updateScenario : scenario_typephp 1 : $scenario_id  $plan_id");
							# `cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php node_status.php  "$scenario_id" "$plan_id"`;
						# }
						# elsif(-e "/usr/bin/php5")
						# {
							# $logging_obj->log("DEBUG","updateScenario : scenario_typephp 5 : $scenario_id  $plan_id");
							# `cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php5 node_status.php  "$scenario_id" "$plan_id"`;
						# }
					# }
				}
				else
				{
					if ($success_flag == 2) 
					{
						my $execution_status = $FAILBACK_INPROGRESS;
						my $policy_type = 9;
						if($scenario_type eq 'Planned Failover')
						{
							$execution_status = $FAILOVER_INPROGRESS;
							$policy_type = 6;
						}
						
						my $update_scenario = "UPDATE
												applicationScenario
										   SET
												executionStatus = '$execution_status'
										   WHERE
												scenarioId='$scenario_id' AND
												executionStatus = '$VALIDATION_INPROGRESS'";
						my $update_rs = $self->{'conn'}->sql_query($update_scenario);
						
						$update_scenario = "UPDATE
												policy
										   SET
												policyScheduledType = '2'
										   WHERE
												scenarioId='$scenario_id' AND
												policyType = '$policy_type'";
						$update_rs = $self->{'conn'}->sql_query($update_scenario);
					}	
					elsif($failed_flag == 1)
					{
						my $execution_status = $VALIDATION_FAILED;
						my $update_scenario = "UPDATE
												applicationScenario
										   SET
												executionStatus = '$execution_status'
										   WHERE
												scenarioId='$scenario_id' AND
												executionStatus = '$VALIDATION_INPROGRESS'";
						my $update_rs = $self->{'conn'}->sql_query($update_scenario);
					}
				}	
				
			}
			elsif(($success_flag == 1) and (($scenario_type eq 'Unplanned Failover') or ($scenario_type eq 'Fast Failback')))
			{
				my $execution_status = $FAILBACK_INPROGRESS;
				my $policy_type = 11;
				if($scenario_type eq 'Unplanned Failover')
				{
					$execution_status = $FAILOVER_INPROGRESS;
					if ($app_type eq 'ORACLE' or $app_type eq 'DB2')
					{
						$policy_type = 36;
					}
					else
					{
						$policy_type = 8;
					}	
				}
				my $update_scenario = "UPDATE
										applicationScenario
								   SET
										executionStatus = '$execution_status'
								   WHERE
										scenarioId='$scenario_id' AND
										executionStatus = '$VALIDATION_INPROGRESS'";
				my $update_rs = $self->{'conn'}->sql_query($update_scenario);
				
				$update_scenario = "UPDATE
										policy
								   SET
										policyScheduledType = '2'
								   WHERE
										scenarioId='$scenario_id' AND
										policyType = '$policy_type'";
				$update_rs = $self->{'conn'}->sql_query($update_scenario);
				
				
			}
			elsif($failed_flag == 1)
			{
				my $execution_status = $VALIDATION_FAILED;
				my $update_scenario = "UPDATE
										applicationScenario
								   SET
										executionStatus = '$execution_status'
								   WHERE
										scenarioId='$scenario_id' AND
										executionStatus = '$VALIDATION_INPROGRESS'";
				my $update_rs = $self->{'conn'}->sql_query($update_scenario);
			}
		}
	}
}
sub getServerName
{
	my ($self,$hostIds,$app_id) = @_; 
	my ($hostId,$name, $cluster_id);
	my @host_id = split(",",$hostIds);
	my $host_cnt = @host_id;
	if($host_cnt >1)
	{
		$hostId = $host_id[0];
	}
	else
	{
		$hostId = $hostIds;
	}
	my $sql1 = "SELECT
				DISTINCT
				clusterId
			FROM
				clusters
			WHERE
				hostId = '$hostId'";

	my $result = $self->{'conn'}->sql_exec($sql1);
	
	if(defined $result)
	{
		my @value = @$result;
		$cluster_id = $value[0]{'clusterId'};
	}
	
	if($cluster_id && ($app_id))
	{
		my $cluster_name = $self->{'conn'}->sql_get_value('clusters', 'clusterName', "clusterId='$cluster_id'");
		my $app_name = $self->{'conn'}->sql_get_value('applicationHosts', 'applicationInstanceName', "applicationInstanceId='$app_id'");
		
		$name = $cluster_name.":".$app_name;
	}
	else
	{
		$name = $self->{'conn'}->sql_get_value('hosts', 'name', "id='$hostId'");
	}
	
	return $name;
}

sub deleteScenario
{
	my $self = shift;
	
	my $SCENARIO_DELETE_PENDING = 101;
	my $DISABLE = 122;
	my $INACTIVE = 95;
	my $VALIDATION_SUCCESS =92;
	my $FAILOVER_DONE = 103;
		
	my $check_query = "SELECT 
						   planId,
						   scenarioId,
						   isPrimary,
						   sourceId,
						   targetId,
						   protectionDirection,
						   executionStatus
					   FROM 
						   applicationScenario 
					   WHERE 
						   executionStatus = '$SCENARIO_DELETE_PENDING' or executionStatus = '$DISABLE'";
	
	$logging_obj->log("DEBUG","deleteScenario : check_query : $check_query");
	my $result = $self->{'conn'}->sql_exec($check_query);

	foreach my $ref1 (@{$result})
	{
		my $planId = $ref1->{"planId"};
		my $scenarioId = $ref1->{"scenarioId"};
		my $isPrimary = $ref1->{"isPrimary"};
		my $protectionDirection = $ref1->{"protectionDirection"};
		my $hostId = $ref1->{"sourceId"};
		my $executionStatus = $ref1->{"executionStatus"};
				
		my $num_records = $self->{'conn'}->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'COUNT(*)', "planId='$planId' AND scenarioId='$scenarioId'");
		
		if($num_records == 0)
		{
				my $policy_state = 0;
				
				# Check for volpack deletion policy
				my $query1 = "SELECT policyId,policyScheduledType FROM policy WHERE scenarioId='$scenarioId' AND policyType='13'";
				my $check_sth3 = $self->{'conn'}->sql_query($query1);
				
				my $volpack_delete_policy = $self->{'conn'}->sql_num_rows($check_sth3);
				
				if($executionStatus == 101)
				{
					if($volpack_delete_policy)
					{
						my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($check_sth3);
						my $policy_id = $ref->{"policyId"};
						my $policy_scheduled_type = $ref->{"policyScheduledType"};
						
						if($policy_scheduled_type == 0)
						{
							my $query1 = "UPDATE policy set policyScheduledType='2' WHERE policyId='$policy_id'";
							$logging_obj->log("DEBUG","deleteScenario : query1 : $query1");
							my $check_sth3 = $self->{'conn'}->sql_query($query1);
						}
						else
						{
							$policy_state = $self->{'conn'}->sql_get_value('policyInstance', 'policyState', "policyId='$policy_id'");
						}
					}
				}	
				
				if($volpack_delete_policy == 0 or ($volpack_delete_policy != 0 and ($policy_state == 1)))
				{
					my $sql1 = "SELECT scenarioId FROM applicationScenario WHERE referenceId = '$scenarioId'";
					$logging_obj->log("DEBUG","deleteScenario : sql1 : $sql1");
					my $result1 = $self->{'conn'}->sql_exec($sql1);
					foreach my $ref4 (@{$result1})
					{
						my $recovery_scenarioId = $ref4->{"scenarioId"};
						if($executionStatus == 101)
						{
							my $query5 = "DELETE FROM policy WHERE scenarioId='$recovery_scenarioId'";
							$logging_obj->log("DEBUG","deleteScenario : query5 : $query5");
							$self->{'conn'}->sql_query($query5);
													
							my $query6 = "DELETE FROM recoveryExecutionSteps WHERE scenarioId='$recovery_scenarioId'";
							$logging_obj->log("DEBUG","deleteScenario : query6 : $query6");
							$self->{'conn'}->sql_query($query6);
							
							my $writeLogstr = getAuditLogStr($self,$recovery_scenarioId);
							writeLog($self,"$writeLogstr has been deleted");
						}
						#------Recovery Scenerio logs
					}
					
					if($executionStatus == 122)
					{
						my $query = "DELETE FROM applicationScenario WHERE referenceId='$scenarioId' AND (scenarioType='Data Validation and Backup' OR scenarioType='Rollback')";
						$self->{'conn'}->sql_query($query);
						$logging_obj->log("DEBUG","deleteScenario : query : $query");
						if($protectionDirection eq 'reverse')
						{
							my $query7 = "UPDATE applicationScenario set executionStatus='$FAILOVER_DONE',protectionDirection = 'forward' where scenarioId='$scenarioId' and executionStatus='$DISABLE'";
							$self->{'conn'}->sql_query($query7);
							$logging_obj->log("DEBUG","deleteScenario : query7 : $query7");
						}
						else
						{
							my $query7 = "UPDATE applicationScenario set executionStatus='$INACTIVE' where scenarioId='$scenarioId' and executionStatus='$DISABLE'";
							$self->{'conn'}->sql_query($query7);
							$logging_obj->log("DEBUG","deleteScenario : query7 : $query7");
						}
					}
					else
					{
						my $writeLogstr = $self->getAuditLogStr($scenarioId);
						my $query = "DELETE FROM applicationScenario WHERE scenarioId='$scenarioId' or referenceId='$scenarioId'";
						$self->{'conn'}->sql_query($query);
						writeLog($self,"$writeLogstr has been deleted\n");
					}
					
					# 
					# Delete Plan only if no more Protection Scenario left in the Plan
					#
					my $secondary_scenario_id = $self->{'conn'}->sql_get_value('applicationScenario', 'scenarioId', "planId='$planId' ORDER BY scenarioId");

					if(!$secondary_scenario_id)
					{
						my $query2 = "DELETE FROM applicationPlan WHERE planId='$planId'";
						$self->{'conn'}->sql_query($query2);
						
						my $query = "DELETE FROM policy WHERE planId='$planId' OR scenarioId='$scenarioId'";
						$self->{'conn'}->sql_query($query);
						
						my $query3 = "DELETE FROM srcMtDiskMapping  WHERE planId='$planId'";
						$self->{'conn'}->sql_query($query3);
					}
					elsif($isPrimary == 1 && $executionStatus == 101)
					{
						if($protectionDirection eq 'reverse')
						{
							my $query_G = "DELETE FROM policy WHERE scenarioId='$scenarioId'";
							my $check_sth_G = $self->{'conn'}->sql_query($query_G);
						}
						else
						{
							my $sql = "UPDATE applicationScenario set isPrimary='1' WHERE scenarioId='$secondary_scenario_id'";
							$self->{'conn'}->sql_query($sql);
							
							if (Utilities::isWindows())
							{    
								my $insert_pairs = $self->{'cs_installation_path'}."\\home\\svsystems\\admin\\web\\update_secondary_scenario.php";
								`$self->{'amethyst_vars'}->{'PHP_PATH'} "$insert_pairs" "$secondary_scenario_id"`;    
							}
							else
							{
								if (-e "/usr/bin/php")
								{
									`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php update_secondary_scenario.php  "$secondary_scenario_id"`;
								}
								elsif(-e "/usr/bin/php5")
								{
									`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php5 update_secondary_scenario.php  "$secondary_scenario_id"`;
								}
							}
							
							my $SELECT_QRY = "SELECT policyId FROM policy WHERE hostId LIKE '%$hostId%' AND scenarioId='$scenarioId' AND policyType='1' AND policyScheduledType='1'";
							my $rs = $self->{'conn'}->sql_exec($SELECT_QRY);	
							
							my $i=0;
							my $policy_id;
							foreach my $hash_ref_QRY (@{$rs})
							{
								if($i == 0)
								{
									$policy_id = $hash_ref_QRY->{"policyId"};
								}
								else
								{
									$policy_id = $policy_id.",".$hash_ref_QRY->{"policyId"};
								}
								$i++;
							}
							
							my $query_F = "UPDATE policy set scenarioId='$secondary_scenario_id' WHERE policyId IN ($policy_id)";
							my $check_sth_F = $self->{'conn'}->sql_query($query_F);
							
							$query_F = "UPDATE policy set scenarioId='$secondary_scenario_id' WHERE scenarioId='$scenarioId' AND policyType IN ('2','4') AND policyScheduledType='1'";
							$check_sth_F = $self->{'conn'}->sql_query($query_F);
							
								my $query_G = "DELETE FROM policy WHERE scenarioId='$scenarioId'";
								$logging_obj->log("DEBUG","deleteScenario : query_G : $query_G");
								my $check_sth_G = $self->{'conn'}->sql_query($query_G);
							
						}
					}
					else
					{
						my $query_G = "DELETE FROM policy WHERE scenarioId='$scenarioId'";
						my $check_sth_G = $self->{'conn'}->sql_query($query_G);
					}
				}
		}		
		
	}
	
	# Clean the stale entries from applicationPlan table
	$self->clean_all_plan;
}
sub getAuditLogStr
{
	my($self,$scenario_id) = @_;
	my $message;
	my $sql = "SELECT
				planId,
				scenarioType,
				sourceId,
				targetId,
				srcInstanceId,
				trgInstanceId,
				protectionDirection
			FROM 
				applicationScenario
			WHERE
				scenarioId = '$scenario_id'";
	my $rs = $self->{'conn'}->sql_exec($sql);

	if(defined $rs)
	{
		foreach my $ref (@{$rs})
		{
			my $planId 		= $ref->{"planId"};
			my $scenarioType 	= $ref->{"scenarioType"};
			my $sourceId 		= $ref->{"sourceId"};
			my $targetId 		= $ref->{"targetId"};
			my $srcInstanceId  = $ref->{"srcInstanceId"};
			my $trgInstanceId  = $ref->{"trgInstanceId"};
			my $protection_direction = $ref->{"protectionDirection"};
			
			my $planName = $self->{'conn'}->sql_get_value('applicationPlan', 'planName', "planId = '$planId'");
			
			my $srchost_name = getServerName($self,$sourceId,$srcInstanceId);
			my $tgthost_name = getServerName($self,$targetId,$trgInstanceId);

			if($protection_direction == '')
			{
				$protection_direction = 'forward';
			}
			
			$message = "$scenarioType Scenario ( $srchost_name -> $tgthost_name ) for $planName plan ";

		}
	}
		
	return $message;

}

sub monitorPolicyRotation
{
    my $self = shift;
	my $sqlStmnt = 	"SELECT 
	                    policyId, 
						policyType 
					FROM 
						policy";
	my $result = $self->{'conn'}->sql_exec($sqlStmnt);
	
	foreach my $ref (@{$result})
	{
		my  $policyId = $ref->{"policyId"};
        my  $policyType = $ref->{"policyType"};
		my 	$getPolicyInstances =  "SELECT 
										policyInstanceId,
										lastUpdatedTime 							
									FROM 
										policyInstance 
									WHERE 
									    policyId = '$policyId' ORDER BY policyInstanceId DESC";
										
		my $rs = $self->{'conn'}->sql_exec($getPolicyInstances);
		my $i = 0;
		foreach my $refInstanceId (@{$rs})
		{
			my $policyInstanceId = $refInstanceId->{"policyInstanceId"};
			my $lastUpdatedTime = $refInstanceId->{"lastUpdatedTime"};
							
			if ($policyType != 4)
			{
				if ($i > 2)
				{
					$self->deletePolicyInstance($policyInstanceId);
				}
			}
			$i++;	
		} 
	}
}

sub deletePolicyInstance
{
	my $self = shift;
	my $policyInstanceId = shift;
	my $sqlStmnt = 	"DELETE 
					 FROM 
						policyInstance
					 WHERE 
						policyInstanceId = '$policyInstanceId'";
	$self->{'conn'}->sql_query($sqlStmnt);
}


sub commonConsistencyPointAvailable
{
	my $self = shift;
	$logging_obj->log("DEBUG","commonConsistencyPointAvailable called");
	if (Utilities::isWindows())
	{    
		my $common_consistency_point_available = $self->{'cs_installation_path'}."\\home\\svsystems\\admin\\web\\common_consistency_point_available.php";
		`$self->{'amethyst_vars'}->{'PHP_PATH'} "$common_consistency_point_available"`; 
		
	}
	else
	{
		if (-e "/usr/bin/php")
		{
			`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php common_consistency_point_available.php`;
		}
		elsif(-e "/usr/bin/php5")
		{
			`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php5 common_consistency_point_available.php`;
			
		}
		$logging_obj->log("DEBUG","commonConsistencyPointAvailable EXITED :: cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php5 common_consistency_point_available.php");
	}
}


sub monitorPartialProtection
{
	my $self = shift;
	
	if (Utilities::isWindows())
	{    
		my $partial_protection = $self->{'cs_installation_path'}."\\home\\svsystems\\admin\\web\\mail_partial_protection.php";
		`$self->{'amethyst_vars'}->{'PHP_PATH'} "$partial_protection"`;    
	}
	else
	{
		if (-e "/usr/bin/php")
		{
			`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php mail_partial_protection.php`;
		}
		elsif(-e "/usr/bin/php5")
		{
			`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php5 mail_partial_protection.php`;
		}
	}
}

#
# Clean all plans from DB, which are :
#  1) not part of any scenario 
#  2) not part of any normal pairs.
#  3) stale entries in applicationPlanp
#
sub clean_all_plan
{
	my $self = shift;
	my $rollback_log_msg = undef;
	my $qry = "SELECT 
					planId,
					planCreationTime ,
					planName,
					planType,
					UNIX_TIMESTAMP(now()) as currenttime
				FROM 
					applicationPlan
				WHERE
					planId !=1 AND
					planId NOT IN(SELECT planId FROM applicationScenario UNION 	
					SELECT planId FROM srcLogicalVolumeDestLogicalVolume UNION
					SELECT planId FROM frbJobConfig WHERE deleted = 0)";
					
	# print "qry : $qry \n";
	my $result = $self->{'conn'}->sql_exec($qry);

	foreach my $refPlanId (@{$result})
	{
		my $plan_id = $refPlanId->{"planId"};
		my $plan_creation_time = $refPlanId->{"planCreationTime"}; # in seconds
		my $current_time = $refPlanId->{"currenttime"}; # in seconds
		my $plan_name = $refPlanId->{"planName"};
		my $plan_type = $refPlanId->{"planType"};
		my $diffrence = $current_time - $plan_creation_time ;
		#print "diffrence : $diffrence \n";
		if( $diffrence > 300 ) # Diff shud be greate than 300 sec.
		{
			my $query2 = "DELETE FROM applicationPlan WHERE planId='$plan_id'";
			$self->{'conn'}->sql_query($query2);
			
			my $query3 = "DELETE FROM srcMtDiskMapping  WHERE planId='$plan_id'";
			$self->{'conn'}->sql_query($query3);
			writeLog($self,"$plan_type plan with plan_name :: $plan_name, plan id : $plan_id has been deleted\n");
			
			my $query4 = "DELETE FROM policy WHERE planId='$plan_id'";
			$self->{'conn'}->sql_query($query4);
			$rollback_log_msg = $rollback_log_msg.$plan_id."-";
		}		
	}
	
	if ($rollback_log_msg ne undef)
	{
		$rollback_log_msg = "Deletedplanids: ".$rollback_log_msg;
		my $activity_id = $MDS_BACKEND_ERRORS_ID;
		my $client_request_id = '';
		my $event_name = 'CS_BACKEND_ERRORS';
		my $error_code = $MDS_BACKEND_ERROR_CODE;
		my $error_type = 'ERROR';
		my $errorCodeName = "monitor_protection.log";
		my $error_upload_sql = "INSERT 
								INTO
									MDSLogging
									(activityId, clientRequestId, eventName, errorCode, errorCodeName, errorType, errorDetails, logCreationDate)
								VALUES ('$activity_id', '$client_request_id', '$event_name', '$error_code', '$errorCodeName', '$error_type', '$rollback_log_msg', now())";
		$self->{'conn'}->sql_query($error_upload_sql);
	}
}

sub update_policy_after_rollback
{
	my $self = shift;
	my $ROLLBACK_INPROGRESS = 116;
	my $ROLLBACK_DONE = 117;
	my $ROLLBACK_FAILED = 118;
	my $INACTIVE = 95;
	my $SCENARIO_DELETE_PENDING = 101;
	my $VALIDATION_INPROGRESS = 91;
	my $VALIDATION_SUCCESS = 92;
	
	$logging_obj->log("DEBUG","update_policy_after_rollback is called ");	
	my $check_query = "SELECT 
						   planId,
						   scenarioId,
						   referenceId,
						   sourceId,
						   executionStatus
					   FROM 
						   applicationScenario 
					   WHERE 
						   executionStatus IN ('$ROLLBACK_DONE','$ROLLBACK_INPROGRESS','$ROLLBACK_FAILED')
						AND applicationType NOT IN ('AZURE','AWS','Azure','Vmware','VMWARE')";
						   
	$logging_obj->log("DEBUG","update_policy_after_rollback : check_query : $check_query");
	my $result = $self->{'conn'}->sql_exec($check_query);
	foreach my $ref1 (@{$result})
	{		
		my $planId = $ref1->{"planId"};
		my $rollback_scenarioId = $ref1->{"scenarioId"};
		my $referenceId = $ref1->{"referenceId"};
		my $sourceId = $ref1->{"sourceId"};
		my $rollback_scenario_execution_status = $ref1->{"executionStatus"};
		
		my $qry = "SELECT isPrimary,executionStatus FROM applicationScenario WHERE scenarioId='$referenceId'";
		$logging_obj->log("DEBUG","update_policy_after_rollback : qry : $qry");
		
		my $rs = $self->{'conn'}->sql_exec($qry);
		my @value = @$rs;

		my $isPrimary = $value[0]{'isPrimary'};
		my $protection_scenario_execution_status = $value[0]{'executionStatus'};
		$logging_obj->log("DEBUG","update_policy_after_rollback : planId : $planId, rollback_scenarioId : $rollback_scenarioId, referenceId : $referenceId, sourceId : $sourceId, isPrimary : $isPrimary");

		my $scenarioId = $referenceId; # primary scenarioId
		
		my $num_records = $self->{'conn'}->sql_get_value('srcLogicalVolumeDestLogicalVolume', 'COUNT(*)', "planId='$planId' AND scenarioId='$scenarioId'");
		$logging_obj->log("DEBUG","update_policy_after_rollback : num_records : $num_records");
		if($num_records == 0)
		{
			$logging_obj->log("DEBUG","update_policy_after_rollback : NO PAIR IN PAIR TABLE");
			# Update policy if Primary Scenario is under rollback.
			if ( $isPrimary )
			{
				$logging_obj->log("DEBUG","update_policy_after_rollback : PRIMARY SCENARIO");
				
				my $scenario_num_records = $self->{'conn'}->sql_get_value('applicationScenario', 'count(scenarioId)', "planId='$planId' AND isPrimary='0' AND scenarioType='Backup Protection'");
				$logging_obj->log("DEBUG","update_policy_after_rollback : scenario_num_records : $scenario_num_records");
				if( $scenario_num_records == 0) #only one scenario is there for the planId( 1 to 1 scenario)
				{										
					my $query_B = "DELETE FROM policy WHERE scenarioId='$scenarioId' AND policyType NOT IN ('2','3')";
					$logging_obj->log("DEBUG","update_policy_after_rollback : query_B : $query_B ");
					my $check_sth_B = $self->{'conn'}->sql_query($query_B);	
					
					my $query_P = "DELETE FROM policy WHERE scenarioId='$scenarioId' AND policyScheduledType='1'";
					$logging_obj->log("DEBUG","update_policy_after_rollback : query_P : $query_P");
					my $check_sth_P = $self->{'conn'}->sql_query($query_P);					
					if(($protection_scenario_execution_status == $VALIDATION_SUCCESS) && ($rollback_scenario_execution_status == $ROLLBACK_DONE))
					{
						my $query_C = "UPDATE applicationScenario set executionStatus ='$INACTIVE' WHERE scenarioId='$scenarioId'";
						$logging_obj->log("DEBUG","update_policy_after_rollback : query_C : $query_C");
						my $check_sth_C = $self->{'conn'}->sql_query($query_C);
					}
										
				}
				else #more than one scenario is there for the planId ( 1 to N scenario)
				{
					my $secondary_scenario_id;
					$secondary_scenario_id = $self->{'conn'}->sql_get_value('applicationScenario', 'scenarioId', "planId='$planId' AND isPrimary='0' AND scenarioType='Backup Protection' ORDER BY scenarioId");
					
					$logging_obj->log("DEBUG","update_policy_after_rollback : secondary_scenario_id : $secondary_scenario_id");
					if ($secondary_scenario_id)
					{
						$logging_obj->log("DEBUG","update_policy_after_rollback : secondary_scenario_id came in");
						my $query_E = "UPDATE applicationScenario set isPrimary='1' WHERE scenarioId='$secondary_scenario_id'";
						$logging_obj->log("DEBUG","update_policy_after_rollback : query_E : $query_E");
						my $check_sth_E = $self->{'conn'}->sql_query($query_E);	

						
						my $SELECT_QRY = "SELECT policyId FROM policy WHERE hostId LIKE '%$sourceId%' AND scenarioId='$scenarioId' AND policyType='1' AND policyScheduledType=1";
						$logging_obj->log("DEBUG","update_policy_after_rollback : SELECT_QRY : $SELECT_QRY");
						my $rs = $self->{'conn'}->sql_exec($SELECT_QRY);
						
						my $i=0;
						my $policy_id;
						foreach my $hash_ref_QRY (@{$rs})
						{
							if($i == 0)
							{
								$policy_id = $hash_ref_QRY->{"policyId"};
							}
							else
							{
								$policy_id = $policy_id.",".$hash_ref_QRY->{"policyId"};
							}
							$i++;
						}
						$logging_obj->log("DEBUG","update_policy_after_rollback : policy_id : $policy_id");
						my $query_F = "UPDATE policy set scenarioId='$secondary_scenario_id' WHERE policyId IN ($policy_id)";
						$logging_obj->log("DEBUG","update_policy_after_rollback : query_F : $query_F");
						my $check_sth_F = $self->{'conn'}->sql_query($query_F);
						
						my $query_G = "UPDATE policy set scenarioId='$secondary_scenario_id' WHERE scenarioId='$scenarioId' AND policyType IN ('2','4') AND policyScheduledType=1";
						$logging_obj->log("DEBUG","update_policy_after_rollback : query_G : $query_G");
						my $check_sth_G = $self->{'conn'}->sql_query($query_G);
						
						my $query_H = "DELETE FROM policy WHERE scenarioId='$scenarioId'";
						$logging_obj->log("DEBUG","update_policy_after_rollback : query_H : $query_H");
						my $check_sth_H = $self->{'conn'}->sql_query($query_H);
						if(($protection_scenario_execution_status == $VALIDATION_SUCCESS) && ($rollback_scenario_execution_status == $ROLLBACK_DONE))
						{
							my $query_I = "UPDATE applicationScenario set isPrimary='0', executionStatus ='$INACTIVE' WHERE scenarioId='$scenarioId'";
							$logging_obj->log("DEBUG","update_policy_after_rollback : query_I : $query_I");
							my $check_sth_I = $self->{'conn'}->sql_query($query_I);
						}
					}					
				}
			}
			else
			{
				$logging_obj->log("DEBUG","update_policy_after_rollback : SECONDARY SCENARIO");
				
				my $protection_scenario_execution_status = $self->{'conn'}->sql_get_value('applicationScenario', 'executionStatus', "scenarioId='$scenarioId'");
				
				my $rollback_scenario_execution_status = $self->{'conn'}->sql_get_value('applicationScenario', 'executionStatus', "scenarioId='$rollback_scenarioId'");
				
				if(($protection_scenario_execution_status == $VALIDATION_SUCCESS) && ($rollback_scenario_execution_status == $ROLLBACK_DONE))
				{
					my $query_J = "DELETE FROM policy WHERE scenarioId='$scenarioId'";
					$logging_obj->log("DEBUG","update_policy_after_rollback : query_J : $query_J");
					my $check_sth_J = $self->{'conn'}->sql_query($query_J);
							
					my $query_K = "UPDATE applicationScenario set executionStatus ='$INACTIVE' WHERE scenarioId='$scenarioId'";
					$logging_obj->log("DEBUG","update_policy_after_rollback : query_K : $query_K");
					my $check_sth_K= $self->{'conn'}->sql_query($query_K);
				}	
			}
			
			my $query_M = "SELECT scenarioId FROM applicationScenario WHERE referenceId='$scenarioId' AND scenarioType='Data Validation and Backup'";
			$logging_obj->log("DEBUG","update_policy_after_rollback : query_M : $query_M");
			my $result_M = $self->{'conn'}->sql_exec($query_M);
			
			### Once rollback done . delete recovery scenario which has taken earlier.
			foreach my $ref_check_sth_M (@{$result_M})
			{
				my $recovery_scenario_id = $ref_check_sth_M->{"scenarioId"};
				
				my $query_N = "DELETE FROM applicationScenario WHERE scenarioId='$recovery_scenario_id'";
				$logging_obj->log("DEBUG","update_policy_after_rollback : query_N : $query_N");
				my $check_sth_N = $self->{'conn'}->sql_query($query_N);
				
				my $query_O = "DELETE FROM policy WHERE scenarioId='$recovery_scenario_id'";
				$logging_obj->log("DEBUG","update_policy_after_rollback : query_O : $query_O");
				my $check_sth_O = $self->{'conn'}->sql_query($query_O);
			}				
		}
	}
}

##
# Update the new devuces in oracleDevices table on node reboot
##
sub update_oracle_devices
{
	my $self = shift;
	$logging_obj->log("DEBUG","Entered update_oracle_devices");
	my $select_sql = "SELECT oraD.srcDeviceName, oraD.applicationInstanceId , appH.hostId , oraD.sharedDeviceId, oraD.vendorOrigin , oraD.volumeType FROM applicationHosts appH , oracleDevices oraD WHERE oraD.applicationInstanceId=appH.applicationInstanceId";
	$logging_obj->log("DEBUG","update_oracle_devices : select_sql : $select_sql");
	my $result = $self->{'conn'}->sql_exec($select_sql);

	if (defined $result)
	{
		foreach my $ref (@{$result})
		{	
			my $oracle_old_src_dev_name =  $ref->{"srcDeviceName"}; 
			my $application_instance_id = $ref->{"applicationInstanceId"}; 
			my $application_host_id = $ref->{"hostId"}; 
			my $phy_lun_id = $ref->{"sharedDeviceId"}; 
			my $vendor_origin =  $ref->{"vendorOrigin"}; 
			my $volume_type =  $ref->{"volumeType"}; 
			$logging_obj->log("DEBUG","update_oracle_devices : oracle_old_src_dev_name : $oracle_old_src_dev_name, application_instance_id : $application_instance_id, application_host_id : $application_host_id, phy_lun_id : $phy_lun_id, vendor_origin : $vendor_origin, volume_type : $volume_type");
			
			my $num_records_A = $self->{'conn'}->sql_get_value('logicalVolumes', 'count(deviceName)', "hostId ='$application_host_id' AND deviceName ='$oracle_old_src_dev_name' AND Phy_Lunid ='$phy_lun_id' AND vendorOrigin ='$vendor_origin' AND volumeType ='$volume_type'");
				
			$logging_obj->log("DEBUG","num_records_A : $num_records_A");
			if ( $num_records_A == 0)
			{  
				my $oracle_new_src_dev_name = $self->{'conn'}->sql_get_value('logicalVolumes', 'deviceName', "hostId = '$application_host_id' AND Phy_Lunid ='$phy_lun_id' AND vendorOrigin ='$vendor_origin' AND volumeType ='$volume_type'");
				 
				$logging_obj->log("DEBUG","oracle_new_src_dev_name : $oracle_new_src_dev_name");
				my $update_qry = "UPDATE oracleDevices SET srcDeviceName ='$oracle_new_src_dev_name' WHERE srcDeviceName ='$oracle_old_src_dev_name' AND applicationInstanceId ='$application_instance_id' AND sharedDeviceId ='$phy_lun_id' AND vendorOrigin ='$vendor_origin' AND volumeType ='$volume_type'";
				$logging_obj->log("DEBUG","update_qry : $update_qry");
				my $check_sth_update_qry = $self->{'conn'}->sql_query($update_qry);
			}
		}
	}
	$logging_obj->log("DEBUG"," update_oracle_devices EXITED");	
}
sub monitorRecoveryScenarioDeletion
{
	
	my $self = shift;
	my $RECOVERY_SCENARIO_DELETE_PENDING = 128;
	$logging_obj->log("DEBUG","Entered monitorRecoveryScenarioDeletion");
	my $check_query = "SELECT 
						   planId,
						   scenarioId,
						   isPrimary,
						   sourceId,
						   targetId,
						   protectionDirection,
						   executionStatus
					   FROM 
						   applicationScenario 
					   WHERE 
						   executionStatus = '$RECOVERY_SCENARIO_DELETE_PENDING'";
	$logging_obj->log("DEBUG","monitorRecoveryScenarioDeletion : check_query : $check_query");					   
	my $result = $self->{'conn'}->sql_exec($check_query);
	foreach my $ref1 (@{$result})
	{
		my $planId = $ref1->{"planId"};
		my $scenarioId = $ref1->{"scenarioId"};
		my $sql = "SELECT snapshotId FROM snapshotMain WHERE scenarioId='$scenarioId'";
		my $rs = $self->{'conn'}->sql_exec($sql);
		foreach my $ref2 (@{$rs})
		{
			my $snapshotId = $ref2->{"snapshotId"};
			my $num_records = $self->{'conn'}->sql_get_value('snapShots', 'COUNT(*)', "snapshotId='$snapshotId'");
	
			if($num_records == 0)
			{	
				my $delete_sql = "DELETE FROM applicationScenario WHERE scenarioId='$scenarioId'";
				$self->{'conn'}->sql_query($delete_sql);
			}	
		}
		my $num_records2 = $self->{'conn'}->sql_get_value('snapShots', 'COUNT(*)', "scenarioId='$scenarioId'");
		
		if($num_records2 == 0)
		{
			my $delete_sql1 = "DELETE FROM applicationScenario WHERE scenarioId='$scenarioId'";
			$logging_obj->log("DEBUG","monitorRecoveryScenarioDeletion : delete_sql1 : $delete_sql1");
			$self->{'conn'}->sql_query($delete_sql1);
		}	
	}
}

sub monitorRunOncePolices
{
	my $self = shift;
	my $policy_sql = "SELECT 
							p.policyParameters,
							p.hostId,
							p.policyTimestamp,
							p.policyId
						FROM
							policy p,
							consistencyGroups cg
						WHERE
							p.policyId=cg.policyId
						AND
							p.policyType IN (4,45)";
	my $policy_rs = $self->{'conn'}->sql_exec($policy_sql);
	$logging_obj->log("DEBUG","Entered monitorRunOncePolices policy_sql::$policy_sql");
	
	foreach my $policy_ref (@{$policy_rs})
	{
		my $currentTime = time();
		my $lastUpdateTimeInUnixTimeStamp = Utilities::datetimeToUnixTimestamp($policy_ref->{"policyTimestamp"});
		my $policy_parmas = unserialize($policy_ref->{"policyParameters"});
		my $run_every_interval = $policy_parmas->{"run_every_interval"};
		my $policy_id = $policy_ref->{"policyId"};
		my @host_ids = split(/,/,$policy_ref->{"hostId"});		
		
		my @current_date = split(/ /,HTTP::Date::time2iso());

		my $exlude_from = $current_date[0]." ".$policy_parmas->{"exceptFrom1"}.":".$policy_parmas->{"exceptFrom2"}.":00";
		my $exlude_to = $current_date[0]." ".$policy_parmas->{"exceptTo1"}.":".$policy_parmas->{"exceptTo2"}.":00";		
		my $policy_exlude_from_date = Utilities::datetimeToUnixTimestamp($exlude_from);
		my $policy_exlude_to_date = Utilities::datetimeToUnixTimestamp($exlude_to);
		
		my $differenceOfCurrentLastUpdate = $currentTime - $lastUpdateTimeInUnixTimeStamp;
		
		$logging_obj->log("DEBUG","Entered monitorRunOncePolices currentTime::$currentTime \n lastUpdateTimeInUnixTimeStamp::$lastUpdateTimeInUnixTimeStamp \n differenceOfCurrentLastUpdate::$differenceOfCurrentLastUpdate \n run_every_interval::$run_every_interval \n policy_id::$policy_id \n");
		
		 if(($differenceOfCurrentLastUpdate >= $run_every_interval && (($currentTime <= $policy_exlude_from_date) || ($currentTime >= $policy_exlude_to_date))) || ($differenceOfCurrentLastUpdate < 0))
		{
			my $update_sql = "UPDATE policy SET policyTimestamp=now() WHERE policyId = '$policy_id'";
			$self->{'conn'}->sql_query($update_sql);
			$logging_obj->log("DEBUG","Entered monitorRunOncePolices update_sql::$update_sql");		
			
			for my $host_id (@host_ids) 
			{
				##
				# Bug - 1961444
				# Inserting the Policy instance job only when there is no Pending job already configured/running.
				##
				my $policy_count = $self->{'conn'}->sql_get_value('policyInstance', 'count(*)', "policyId='$policy_id' AND policyState IN ('3','4') AND hostId='$host_id'");
				if($policy_count == 0) 
				{	
					my $insert_sql = "INSERT INTO
								policyInstance
								(
									policyId,
									lastUpdatedTime,
									policyState,
									executionLog,
									hostId
								)
							VALUES
								(
									'$policy_id',
									now(),
									'3',
									'',
									'$host_id'
								)";
					$self->{'conn'}->sql_query($insert_sql);
					$logging_obj->log("DEBUG","Entered monitorRunOncePolices insert_sql::$insert_sql");
				}
			}
		}		
	}	
}

sub activateCloudPlan
{
	my $PREPARE_TARGET_DONE = 99;
	my $self = shift;
	my @plan_id_list = ();
	$logging_obj->log("DEBUG","activateCloudPlan");
	my $sql = "SELECT 
				planId 
			FROM 
				applicationPlan
			WHERE applicationPlanStatus='Prepare Target Pending' AND planType = 'CLOUD'";
	my $check_sth = $self->{'conn'}->sql_query($sql);
	while (my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($check_sth))
	{
		my $plan_id = $ref->{"planId"};
		
		my $sql_policy = "SELECT 
							b.policyState AS policyState,
							b.executionLog AS executionLog
					   FROM
							policy a,
							policyInstance b
					   WHERE
							a.policyId = b.policyId AND
							a.policyType = '5' AND
							a.planId = '$plan_id'";
		my $rs = $self->{'conn'}->sql_query($sql_policy);
		my $flag;
		while (my $ref1 = $self->{'conn'}->sql_fetchrow_hash_ref($rs)) {
			if(($ref1->{'policyState'} == 3) or (($ref1->{'policyState'} == 4))) {
				$flag = 0;
				last;
			} elsif(($ref1->{'policyState'} == 2)) {
				my $log = $self->{'conn'}->sql_escape_string($ref1->{'executionLog'});
				my $update = "UPDATE applicationPlan SET applicationPlanStatus='Prepare Target Failed',errorMessage='$log' WHERE planId='$plan_id'";
				my $update_rs = $self->{'conn'}->sql_query($update);
				$flag = 0;
				last;
			} else {
				$flag = 1;
			}
		}
		
		if($flag == 1) {
			my $update = "UPDATE applicationPlan SET applicationPlanStatus='Prepare Target Success' WHERE planId='$plan_id'";
			my $update_rs = $self->{'conn'}->sql_query($update);
		}
	}
	my $sql = "SELECT 
				planId 
			FROM 
				applicationPlan
			WHERE applicationPlanStatus='Prepare Target Success' AND planType = 'CLOUD'";
	my $check_sth = $self->{'conn'}->sql_query($sql);
	while (my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($check_sth)) {
		push(@plan_id_list,$ref->{"planId"});
	}
	
	##
	# ModifyProtection API CHanges
	# - Prepare Target will be for each scenario. So, fetching the plan Id's for all the scenario which passed the prepare target
	##
	my $sql = "SELECT DISTINCT a.planId AS planId FROM applicationPlan a, applicationScenario b WHERE a.planId = b.planId AND b.executionStatus = '$PREPARE_TARGET_DONE'";
	my $check_sth = $self->{'conn'}->sql_query($sql);
	while (my $ref = $self->{'conn'}->sql_fetchrow_hash_ref($check_sth)) {
		push(@plan_id_list,$ref->{"planId"});
	}
	
	if(scalar @plan_id_list) {
		#call PHP to create pairs
		my $plan_id_str = join(",",@plan_id_list);
		
		$logging_obj->log("DEBUG","activateCloudPlan $plan_id_str");
		my $scenario_id = 0;
		if (Utilities::isWindows()) {    
			my $insert_pairs = $self->{'cs_installation_path'}."\\home\\svsystems\\admin\\web\\insert_application_pairs.php";
			`$self->{'amethyst_vars'}->{'PHP_PATH'}  "$insert_pairs" $scenario_id $plan_id_str`;
		
		}
		else
		{
			if (-e "/usr/bin/php")
			{
				`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php insert_application_pairs.php  "$scenario_id" "$plan_id_str"`;
			}
			elsif(-e "/usr/bin/php5")
			{
				`cd $self->{'amethyst_vars'}->{WEB_ROOT}/; php5 insert_application_pairs.php  "$scenario_id" "$plan_id_str"`;
			}
		}
	}
}

#  
# Function Name: monitorCSRenewCertificatePolicies  
# Description: 
#   This sub routine is used to renew CS SSL certificate.
# Parameters: None 
# Return Value: None
#

sub monitorCSRenewCertificatePolicies
{
	my $self = shift;
	my $host_guid = $self->{'amethyst_vars'}->{'HOST_GUID'};
	my %cs_renew_cert_status = (
				  "Response" => "0",
				  "ErrorDescription" => "Success",
				  "logmessage" => "",
				  "ErrorCode" => "",
				  "PlaceHolders" => ""
				);
	my($error_details, $exception_log, $policy_state, $cs_policy_id);
	
	# policy states. 
	# 3 - pending, 4 - inprogress, 1 - success, 2- failed. 
	# policy type. 50 - CS Renew Certificate, 51 - PS Renew Certificate.
	
	# Get CS renew certificate pending policies.
	my $cs_policy_sql = "SELECT
								p.policyId as policyId
							FROM
								policy p,
								policyInstance pi
							WHERE
								p.policyId = pi.policyId
							AND
								pi.policyState = '3'
							AND
								p.policyType = '50'
							AND
								p.hostId = '$host_guid'";
	my $cs_policy_handler = $self->{'conn'}->sql_query($cs_policy_sql);
	my $cs_policy_rows = $self->{'conn'}->sql_num_rows($cs_policy_handler);
	$logging_obj->log("INFO","monitorCSRenewCertificatePolicies: cs_policy_sql::$cs_policy_sql \n configuration server renew certificate jobs count::$cs_policy_rows");
	
	# Checking if there are any CS Renew certificate jobs to be triggered.
	if($cs_policy_rows == 1)
	{	
		# Checking if there are any process server renew cert policies are in pending/inprogress state.
		my $policy_sql = "SELECT 
								p.hostId,
								p.policyId
							FROM
								policy p,
								policyInstance pi
							WHERE
								p.policyId = pi.policyId
							AND
								p.policyType = '51'
							AND 
								pi.policyState = '3'";
		my $policy_sth = $self->{'conn'}->sql_query($policy_sql);
		my $ps_policy_rows = $self->{'conn'}->sql_num_rows($policy_sth);		
		$logging_obj->log("INFO","monitorCSRenewCertificatePolicies ps_policy_sql::$policy_sql \n process server renew certificate jobs count:: $ps_policy_rows");	
		
		# If there are no Process Server Renew Certificate jobs in Pending and In Progress states.
		if ($ps_policy_rows == 0) 
		{
			my $cs_policy_data = $self->{'conn'}->sql_fetchrow_hash_ref($cs_policy_handler);
			$cs_policy_id = $cs_policy_data->{"policyId"};
		
			# Update Configuration Server Renew certificate policy state from pending to in progress state.
			my $update_policy = "UPDATE
									policyInstance
								SET
									policyState = '4'
								WHERE
									hostId = '$host_guid'
								AND
									policyState = '3'
								AND 
									policyId = '$cs_policy_id'";
			$self->{'conn'}->sql_query($update_policy);
			
			my $start_counter = 0;
			while ($start_counter < $max_attempts) 
			{
				# Invoking actual CS Renew Certificate process.
				&tmanager::cs_cert_regeneration(\%cs_renew_cert_status,5);
				$logging_obj->log("INFO","monitorCSRenewCertificatePolicies: Attempt= $start_counter, Renew cert status::".\%cs_renew_cert_status);
				if($cs_renew_cert_status{"Response"} eq "0")
				{
					$logging_obj->log("INFO","monitorCSRenewCertificatePolicies: CS Renew certificate Success");
					last;
				}
				$start_counter++;
				sleep(1);
			}
			
			$logging_obj->log("INFO","monitorCSRenewCertificatePolicies update_policy::$update_policy \n renew cert status::".\%cs_renew_cert_status);
			$logging_obj->log("INFO","monitorCSRenewCertificatePolicies renew cert status response::".$cs_renew_cert_status{"Response"});
			
			$policy_state = "1";
			$exception_log = "";
			
			# In case CS renew certificate failed then fetch the renew cert status.
			if($cs_renew_cert_status{"Response"} ne "0")
			{
				$policy_state = "2";
				
				$error_details->{"ErrorDescription"} = $cs_renew_cert_status{"ErrorDescription"};
				$error_details->{"ErrorCode"} = $cs_renew_cert_status{"ErrorCode"};
				$error_details->{"PlaceHolders"} = $cs_renew_cert_status{"PlaceHolders"};
				$error_details->{"LogMessage"} = serialize($cs_renew_cert_status{"logmessage"});
				$exception_log = serialize($error_details);
			}
			
			# Update CS renew certificate policy state to success/failure with the corresponding log data.
			my $policy_instance_sql = "UPDATE 
												policyInstance 
											SET 
												policyState = '$policy_state', 
												executionLog = '$exception_log' 
											WHERE 
												hostId = '".$host_guid."'
											AND 
												policyState = '4'
											AND 
												policyId = '$cs_policy_id'";
			my $conn = &tmanager::get_db_connection();
			$conn->sql_query($policy_instance_sql);
			$logging_obj->log("INFO","monitorCSRenewCertificatePolicies policy_instance_sql::$policy_instance_sql");
		}
		else
		{
			my @hostIds;
			my $policy_ref = $self->{'conn'}->sql_exec($policy_sql);
			if(defined $policy_ref)
			{
				foreach my $policy_data (@{$policy_ref})
				{
					push(@hostIds, $policy_data->{"hostId"});
				}
				$logging_obj->log("INFO","monitorCSRenewCertificatePolicies paused. PS Renew certificates are in pending state for below hosts. hostId::".\@hostIds);
			}
		}
	}
	else
	{
		$logging_obj->log("INFO","monitorCSRenewCertificatePolicies : No CS Renew Certificate Jobs Found");
	}
}

#  
# Function Name: monitorPSRenewCertificatePolices  
# Description: 
#   This sub routine is used to renew PS SSL certificate.
# Parameters: Policy data. 
# Return Value: None
#

sub monitorPSRenewCertificatePolicies
{
	my $self = shift;
	my $policy_data = $self->{'csData'}->{'policy_data'};
	my %ps_renew_cert_status = (
				  "Response" => "0",
				  "ErrorDescription" => "Success",
				  "logmessage" => "",
				  "ErrorCode" => "0",
				  "PlaceHolders" => ""
				);
	my ($policy_state, $exception_log, $error_details);
	
	$logging_obj->log("INFO","monitorPSRenewCertificatePolicies: Process Server Renew cert jobs data::".Dumper($policy_data));
		
	foreach my $policy_id (keys %{$policy_data})
	{
		my $host_id = $policy_data->{$policy_id}->{'hostId'};
		
		# Invoking PS Renew Certificate.
		
		my $start_counter = 0;
		while ($start_counter < $max_attempts) 
		{
			&tmanager::ps_cert_regeneration(\%ps_renew_cert_status,5);
			$logging_obj->log("INFO","monitorPSRenewCertificatePolicies: Attempt= $start_counter, Renew cert status::".\%ps_renew_cert_status);
			if($ps_renew_cert_status{"Response"} eq "0")
			{
				$logging_obj->log("INFO","monitorPSRenewCertificatePolicies: PS Renew certificate Success");
				last;
			}
			$start_counter++;
			sleep(1);
		}
		
		$policy_state = "1";
		$exception_log = "";
		
		# In case PS renew certificate failed then update policy state as failed.
		if($ps_renew_cert_status{"Response"} ne "0")
		{
			$policy_state = "2";
			$error_details->{"ErrorDescription"} = $ps_renew_cert_status{"ErrorDescription"};
			$error_details->{"ErrorCode"} = $ps_renew_cert_status{"ErrorCode"};
			$error_details->{"PlaceHolders"} = $ps_renew_cert_status{"PlaceHolders"};
			$error_details->{"LogMessage"} = serialize($ps_renew_cert_status{"logmessage"});			
			$exception_log = serialize($error_details);
		}
		
		my $update_query;
		$logging_obj->log("INFO","monitorPSRenewCertificatePolicies renew cert status::".$ps_renew_cert_status{"Response"});
				
		# Update policy state with the corresponding state and log data.
		$update_query->{'cleanup_update'}->{"queryType"} = "UPDATE";
		$update_query->{'cleanup_update'}->{"tableName"} = "policyInstance";
		$update_query->{'cleanup_update'}->{"fieldNames"} = "policyState = '$policy_state', executionLog = '$exception_log'";
		$update_query->{'cleanup_update'}->{"condition"} = "hostId = '$host_id' AND policyId = '$policy_id' AND policyState = '3'";
		
		$logging_obj->log("INFO","monitorPSRenewCertificatePolicies update_query::".Dumper($update_query));
		# Call CX API to update states in policy table.
		&tmanager::update_cs_db("monitorPSRenewCertificatePolicies",$update_query);
	}
}
1;