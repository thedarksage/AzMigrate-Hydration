<?php
ini_set('memory_limit', '128M');
if (preg_match('/Linux/i', php_uname())) 
{
	include_once "config.php";
}
else
{
	include_once "config.php";
}

$sparseretention_obj = new SparseRetention();
$retention_obj = new Retention();
$commonfunctions_obj = new CommonFunctions();
$app_obj = new Application();
$obj = new ProtectionPlan();
$volume_obj = new VolumeProtection();
$discovery_obj = new ESXDiscovery();
$scenario_id = $argv[1];
$plan_id = $argv[2];
global $logging_obj, $PROTECTION_TIMEOUT_ERROR_CODE;

//debugtest12("Inside insert_application_pairs.php ::: $scenario_id:::::$plan_id");
$logging_obj->my_error_handler("DEBUG","Inside insert_application_pairs.php ::: $scenario_id:::::$plan_id");
$obj->setScenarioId($scenario_id);

$conn_obj->enable_exceptions();
try
{
	$conn_obj->sql_begin();
	if($scenario_id)
	{
		$scenario_id_str = (string)$scenario_id ;
		//If the string contains all digits, then only allow, otherwise throw 500 header.
		////This call from tmansvc CS event back end call. In case data mismatch throwing 500 header and next event will trigger periodically.
		if (isset($scenario_id) && $scenario_id != '' && (!preg_match('/^\d+$/',$scenario_id_str))) 
		{
			$conn_obj->sql_rollback();
			$commonfunctions_obj->bad_request_response("Invalid post data scenario id $scenario_id in file insert_application_pairs.");
		}
		
		$obj->insertProtectionPlan();
		$execution_status = $VALIDATION_SUCCESS;
		$update_scenario = "UPDATE
								applicationScenario
							   SET
									executionStatus = $execution_status
							   WHERE
									scenarioId='$scenario_id' AND
									executionStatus = $VALIDATION_INPROGRESS";
		$update_rs = $conn_obj->sql_query($update_scenario);
	}
	else
	{
		$plan_id_arr = explode(",",$plan_id);
		foreach($plan_id_arr as $id) 
		{
			$plan_id_str = (string)$id;
			//If the string contains all digits, then only allow, otherwise throw 500 header.
			////This call from tmansvc CS event back end call. In case data mismatch throwing 500 header and next event will trigger periodically.
			if (isset($id) && $id != '' && (!preg_match('/^\d+$/',$plan_id_str))) 
			{
				$conn_obj->sql_rollback();
				$commonfunctions_obj->bad_request_response("Invalid post data plan id str $id in file insert_application_pairs.");
			}
			
			$flag = true;
			$sql = "SELECT 
						scenarioId,
						scenarioDetails,
						targetVolumes,
						targetId
					FROM
						applicationScenario
					WHERE
						planId='$id' AND
						executionStatus = '$PREPARE_TARGET_DONE'";
			$rs = $conn_obj->sql_query($sql);
			$protected_vms = array();
			$reprotected_vms = array();
			while($row = $conn_obj->sql_fetch_object($rs)) 
			{
				$scenario_id = $row->scenarioId;
				$scenario_details = unserialize($row->scenarioDetails);
				$trg_id = $row->targetId;	
				//Check target devices are reported to CX or not
				$trg_vol_str = $row->targetVolumes;
				$trg_vol_list = explode(",", $trg_vol_str);
				
				$db_data = $conn_obj->sql_get_array("SELECT deviceName, hostId FROM logicalVolumes WHERE hostId= ? AND FIND_IN_SET(deviceName, ?)", "deviceName", "hostId", array($trg_id, $trg_vol_str));
				
				$db_vols = array();
				foreach($db_data as $device_name => $host_id)
				{
					$db_vols[] = $db_vol_data["deviceName"];
				}
				
				$logging_obj->my_error_handler("INFO","Inside insert_application_pairs.php :::".print_r($trg_vol_list,true));
				if(count($db_vols) != count($trg_vol_list)) 
				{
					$flag = false;
					
					$policy_id = $conn_obj->sql_get_value("policy", "policyId", "scenarioId = ? AND policyType = 5", array($scenario_id));
					$policy_time = $conn_obj->sql_get_value("policyInstance", "(UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(lastUpdatedTime)) > 1800", "policyId = ? AND lastUpdatedTime != ''", array($policy_id));
					
					# If the difference of current time and policy last update time is more than 1800 sec then report the error in MDS.
					if($policy_time)
					{
						$vol_list = array_diff($trg_vol_list, $db_vols);							
						$vol_list = implode(",",$vol_list);
						
						$mds_data_array = array();
						$mds_obj = new MDSErrorLogging();
						
						$activityId = $conn_obj->sql_get_value("apiRequest", "activityId", "referenceId LIKE ? AND requestType IN ('CreateProtection', 'ModifyProtection')", array("%,$scenario_id,%"));
						$conn_obj->sql_query("DELETE FROM MDSLogging WHERE activityId = ? AND errorCode = ?", array($activityId, $PROTECTION_TIMEOUT_ERROR_CODE));
						
						$mds_data_array["jobId"] = $scenario_id;
						$mds_data_array["jobType"] = "ProtectionTimeout";
						$mds_data_array["errorString"] = "Master target server($trg_id) volumes($vol_list) are not registered with CS";
						$mds_data_array["eventName"] = "CS";
						$mds_data_array["errorType"] = "ERROR";
						
						$mds_obj->saveMDSLogDetails($mds_data_array);
					}
				}
				else
				{
				
					//for now create a new function..but later should be merged with "insertProtectionPlan" or may be another class
					$obj->insertCloudProtectionPlan($scenario_id);
					$execution_status = $VALIDATION_SUCCESS;
					$update_scenario = "UPDATE
										applicationScenario
									   SET
											executionStatus = $execution_status
									   WHERE
											scenarioId='$scenario_id'";
					$update_rs = $conn_obj->sql_query($update_scenario);
					
					// update the replica id only when the protection direction is from Azure to On-Premises.
					if($scenario_details['protectionPath'] == "A2E")
					$reprotected_vms[] = $scenario_details['sourceHostId'];
					elseif($scenario_details['protectionPath'] == "E2A")
					$protected_vms[] = $scenario_details['sourceHostId'];
				}
			}
			if($flag) {
				$sql = "UPDATE applicationPlan SET applicationPlanStatus='$APPLICATION_PLAN_STATUS[12]' WHERE planId='$id'";
				$rs = $conn_obj->sql_query($sql);
			}
			// update the replica id only when the protection direction is from Azure to On-Premises.
			if(count($reprotected_vms) > 0) {
				$update_status = $discovery_obj->updateReplicaId($reprotected_vms);
				if(!$update_status) {
					$mds_data_array = array();
					$mds_data_array["activityId"] = $activityId;
					$mds_data_array["jobType"] = "Protection";
					$mds_log = "Could not update the replica id for the source hosts $srcIdsList";
					$mds_data_array["errorString"] = $mds_log;
					$mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
					$mds_data_array["errorType"] = "ERROR";
					$mds_obj = new MDSErrorLogging();
					$mds_obj->saveMDSLogDetails($mds_data_array);
				}		
			}
			// reset the replicaId if the forward protection is triggered.
			if(count($protected_vms) > 0) {			
				$update_status = $discovery_obj->cleanStaleDiscoveredVMs($protected_vms);
			}
		}
		
	}
	$conn_obj->sql_commit();
}
catch(Exception $e)
{
	$conn_obj->sql_rollback();
}
$conn_obj->disable_exceptions();

function debugtest12 ( $debugString)
{
  //global $PHPDEBUG_FILE;
  global $REPLICATION_ROOT;
  $PHPDEBUG_FILE = "$REPLICATION_ROOT/insertPair.log123";
  # Some debug
  $fr = fopen($PHPDEBUG_FILE, 'a+');
  if(!$fr) {
	echo "Error! Couldn't open the file.";
  }
  if (!fwrite($fr, $debugString . "\n")) {
	print "Cannot write to file ($filename)";
  }
  if(!fclose($fr)) {
	echo "Error! Couldn't close the file.";
  }
}
?>