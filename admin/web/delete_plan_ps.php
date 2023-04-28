<?php
ini_set('memory_limit', '128M');
include_once 'config.php';

$commonfunctions_obj = new CommonFunctions();
$ps_id = $argv[1];
global $HOST_GUID, $CS_IP, $logging_obj;
$logging_obj->appinsights_log_file = "delete_plan_ps";
$cs_version_array = $commonfunctions_obj->get_inmage_version_array();
$GLOBALS['record_meta_data']	= array("CSGUID"=>$HOST_GUID,
										"CSIP"=> $CS_IP,
										"CSVERSION"=>$cs_version_array[0],
										"RecordType"=>"delete_plan_ps",
										"PlanId"=>$ps_id);	
delete_plan(" delete_plan_ps ps_id::$ps_id");
$sql = "SELECT scenarioId, scenarioDetails FROM applicationScenario WHERE scenarioType IN ('DR Protection','Backup Protection','Profiler Protection')";
$rs = $conn_obj->sql_query($sql);
if($conn_obj->sql_num_row($rs) > 0)
{
	while($row = $conn_obj->sql_fetch_object($rs))
	{
		$xml_data = $row->scenarioDetails;
		$scenarioId = $row->scenarioId;
		$cdata = $commonfunctions_obj->getArrayFromXML($xml_data);
		$plan_properties = unserialize($cdata['plan']['data']['value']);
		$scenario_id = $plan_properties['scenario_id'];
		$plan_id = $plan_properties['planId'];				
		$match_cnt = 0;
		$pair_count = 0;
		delete_plan(" delete_plan_ps solutionType".$plan_properties['solutionType']);
		if($plan_properties['solutionType'] == 'HOST')
		{
			foreach($plan_properties['pairInfo'] as $key => $value)
			{
				delete_plan(" delete_plan_ps ps_id::$ps_id, pairDetails".print_r($value['pairDetails'],true));
				foreach($value['pairDetails'] as $key1 => $value1)
				{
					delete_plan(" delete_plan_ps ps_id::$ps_id, processServerid".$value1['processServerid']);
					if($ps_id == $value1['processServerid']) // check for each pair unregisterd ps is being used or not
					{
						$match_cnt++;
					}
					$pair_count++;
				}
			}
			delete_plan(" delete_plan_ps match_cnt::$match_cnt------------pair_count:::$pair_count");
			if($match_cnt == $pair_count) // if the number of pairs in a plan and the number of pairs with the matching process serverid are equal then delete the plan data.
			{
				// Fetching the recovery scenarios scenarioId
				$reference_id_tmp = array();
				$sel_sql = "SELECT scenarioId FROM applicationScenario WHERE referenceId='$scenario_id'";
				$rs = $conn_obj->sql_query($sql_sql);
				while($row1 = $conn_obj->sql_fetch_object($rs))
				{
					$reference_id[] = $row1->sccenarioId;
				}
				$reference_id_tmp = array_unique($reference_id);
				$reference_id = implode("','",$reference_id_tmp);
				delete_plan(" delete_plan_ps sel_sql::$sel_sql------------rs:::$rs----------reference_id::$reference_id");
				
				$app_sce_sql = "DELETE FROM applicationScenario WHERE scenarioId='$scenario_id' OR referenceId IN ('$reference_id')";
				$app_rs = $conn_obj->sql_query($app_sce_sql);
				delete_plan(" delete_plan_ps app_sce_sql::$app_sce_sql------------app_rs:::$app_rs");
				
				$policy_sql = "DELETE FROM policy WHERE scenarioId IN ($scenario_id) OR scenarioId IN ('$reference_id')";
				$policy_rs = $conn_obj->sql_query($policy_sql);
				delete_plan(" delete_plan_ps policy_sql::$policy_sql------------policy_rs:::$policy_rs");
				
				$plan_sql = "DELETE FROM applicationPlan WHERE planId='$plan_id'";
				$plan_rs = $conn_obj->sql_query($plan_sql);
				delete_plan(" delete_plan_ps plan_sql::$plan_sql------------plan_rs:::$plan_rs");
			}
		}
	}
}
if (!$GLOBALS['http_header_500'])
	$logging_obj->my_error_handler("INFO",array("Status"=>"Success"),Log::BOTH);
else
	$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
	
function delete_plan ( $debugString)
{
    global $REPLICATION_ROOT, $commonfunctions_obj, $logging_obj;
  $PHPDEBUG_FILE = "$REPLICATION_ROOT/var/delete_plan_ps.txt";
	$logging_obj->my_error_handler("INFO",array("Message"=>$debugString),Log::APPINSIGHTS);
  $fr = $commonfunctions_obj->file_open_handle($PHPDEBUG_FILE, 'a+');
  if(!$fr) {
	  // return if file handle not available.
	  return;
  }
  if (!fwrite($fr, $debugString . "\n")) {
	print "Cannot write to file ($filename)";
  }
  if(!fclose($fr)) {
	echo "Error! Couldn't close the file.";
  }
}

?>