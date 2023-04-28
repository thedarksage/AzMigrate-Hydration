<?php
ini_set('memory_limit', '128M');
if (preg_match('/Linux/i', php_uname())) 
{
	include_once 'config.php';
}
else
{
	include_once 'config.php';
}

$volume_obj= new VolumeProtection();
$commonfunctions_obj = new CommonFunctions();
$recovery_obj = new Recovery();
$retention_obj = new Retention();
$bandwidth_obj = new BandwidthPolicy();
$app_obj = new Application();

$sourceId =$argv[1];
$sourceDeviceName =$argv[2];
$destId = $argv[3];
$destDeviceName =$argv[4] ; 
$scenarioId = $argv[5];
 
global $conn_obj, $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION, $TARGET_DATA, $SOURCE_DATA;

//This call from tmansvc CS event back end call which calls periodically.
$scenario_id_str = (string)$scenarioId ;
//If the string contains all digits, then only allow, otherwise throw 500 header.
if (isset($scenarioId) && $scenarioId != '' && (!preg_match('/^\d+$/',$scenario_id_str))) 
{
	$commonfunctions_obj->bad_request_response("Invalid scenario id $scenario_id_str post data in stop_unlock_replication file.");
}

$valid_protection = $commonfunctions_obj->isValidProtection($destId , $destDeviceName, $TARGET_DATA);
if (!$valid_protection)
{
	$commonfunctions_obj->bad_request_response("Invalid destination details $destId, $destDeviceName post data in stop_unlock_replication file");
}

$valid_protection = $commonfunctions_obj->isValidProtection($sourceId , $sourceDeviceName, $SOURCE_DATA);
if (!$valid_protection)
{
	$commonfunctions_obj->bad_request_response("Invalid source details $sourceId, $sourceDeviceName post data in stop_unlock_replication file.");
}

/*
* Distinguish pair delete call from UI or Rollback.
*/
$conn_obj->enable_exceptions();
try
{
	$conn_obj->sql_begin();
	$dest_device_name = $conn_obj->sql_escape_string($destDeviceName);
	$sql = "SELECT 
				count(*) as num_entries
			FROM
				snapShots 
			WHERE 
				srcHostid='$destId' AND
				 srcDeviceName='$dest_device_name' AND 
				snaptype='2'";

	$result_set = $conn_obj->sql_query($sql);
	$row = $conn_obj->sql_fetch_object ($result_set);
	$flag = ($row->num_entries) ? '0' : '1';
	$pair = $sourceId."=".$sourceDeviceName."=".$destId."=".$destDeviceName;

	$sql = "SELECT deleted FROM srcLogicalVolumeDestLogicalVolume WHERE scenarioId='$scenarioId'" ;
	$rs = $conn_obj->sql_query($sql);
	$total_num_of_pairs = $conn_obj->sql_num_row($rs);

	$deleted_pairs = $commonfunctions_obj->getDeletedPairInfo($scenarioId);
	$execution_status = $conn_obj->sql_get_value("applicationScenario","executionStatus","scenarioId='$scenarioId'");
	$application_type = $conn_obj->sql_get_value("applicationScenario","applicationType","scenarioId='$scenarioId'");
	$application_type = strtoupper($application_type);
	$volume_obj->clear_replication($pair,$flag); 

	if($scenarioId != 0)
	{	
		if(($total_num_of_pairs != $deleted_pairs) && $execution_status == 92 && $application_type != 'AWS' && $application_type != $CLOUD_PLAN_AZURE_SOLUTION && $application_type != $CLOUD_PLAN_VMWARE_SOLUTION)
		{
			$query1 = "SELECT policyId,policyScheduledType FROM policy WHERE scenarioId='$scenarioId' AND policyType='13'";
			$rs = $conn_obj->sql_query($query1);
			$volpack_delete_policy = $conn_obj->sql_num_row($rs);
			if($volpack_delete_policy)
			{
				$row = $conn_obj->sql_fetch_object($rs);
				$policy_id = $row->policyId;
				$policy_scheduled_type = $row->policyScheduledType;
			
				if($policy_scheduled_type == 0)
				{
					$query1 = "UPDATE policy set policyScheduledType='2' WHERE policyId='$policy_id'";
					$conn_obj->sql_query($query1);
				}
			}
			$app_obj->deletePairFromPlan($pair, $scenarioId);
		}	
	}
	$conn_obj->sql_commit();
}
catch(SQLException $e)
{
	$conn_obj->sql_rollback();
}
$conn_obj->disable_exceptions();
?>
