<?php
class RecoveryHandler extends ResourceHandler
{
	private $conn;
	private $volume_obj;
	private $recovery_obj;
	private $commonfunctions_obj;
	private $global_conn;
        
	public function RecoveryHandler() 
	{
        global $conn_obj;
		$this->volume_obj = new VolumeProtection();
		$this->conn = new Connection();
		$this->recovery_obj = new Recovery();
		$this->commonfunctions_obj = new CommonFunctions();
        $this->global_conn = $conn_obj;
	}
	
    /*
    Validate for the Rollback Request from ASR
    1. Verify if protection plan id passed is valid protection plan.
    2. Verify if the rollback plan id passed is valid rollback plan that has failed (used only for retry).
    3. Verify if the servers passed are hosts pointing to this CS
    4. Verify if the protection plan / server passed are having pairs.
    5. Verify that all the pairs for each server are reported as in diffsync.
    6. Verify if the Recovery Plan Name passed is stale.
    7. Verify if the recovery point name passed is valid.
    8. Verify if the servers selected have the recovery point available.
    9. Verify is mentioned servers are already part of rollback that is in progress.
    10. Verify if custom tag exists if passed.
	11. Verify if custom tag provided across the servers is available.
    */
    
    public function CreateRollback_validate ($identity, $version, $functionName, $parameters)
	{   
		$function_name_user_mode = 'Create Rollback';
		
        try
		{
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			global $activityId;
			$host_names = array();
			/* Validation 6 */
			if(isset($parameters['RollbackPlanName'])) 
			{
				$rollback_plan_name = $parameters['RollbackPlanName']->getValue();
				if(!$rollback_plan_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "RollbackPlanName"));
				$existing_plan_id = $this->recovery_obj->get_plan_id($rollback_plan_name);				
				if($existing_plan_id) ErrorCodeGenerator::raiseError($functionName, 'EC_PLAN_EXISTS', array('PlanType' => 'Recovery', 'PlanName' => $rollback_plan_name));
			}
			
			/* Validation 7 */
			if(isset($parameters['RecoveryPoint'])) $recovery_option = $parameters['RecoveryPoint']->getValue();
			if(isset($parameters['MutilVMSyncPoint'])) $multivm_sync_point = $parameters['MutilVMSyncPoint']->getValue();
			if($recovery_option && !in_array($recovery_option, array("MultiVMLatestTag", "LatestTag", "LatestTime", "Custom")))
			ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECOVERY_OPTION', array("RecoveryOptions" => "MultiVMLatestTag/LatestTag/LatestTime/Custom"));
			
			/* Validation 10 */
			if($recovery_option == "Custom")
			{
				if(!isset($parameters['TagName'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "TagName"));
				$custom_tag_name = trim($parameters['TagName']->getValue());
				if(!$custom_tag_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "TagName"));
			}
			
			// Array that will hold the rollback host ids
			$rollback_host_ids = $custom_rollback_host_ids = $server_tag_info = array();
			
			/* Validation 1, 4 and 5 */
			if(isset($parameters['ProtectionPlanId']))
			{				
				if(isset($parameters['RollbackPlanId'])) ErrorCodeGenerator::raiseError($functionName, 'EC_MTAL_EXCLSIVE_ARGS', array(), 'Expected either RollbackPlanId/ProtectionPlanId/Server Host GUID. Retry accepts only RollbackPlanId.');
				if(isset($parameters['Servers'])) ErrorCodeGenerator::raiseError($functionName, 'EC_MTAL_EXCLSIVE_ARGS', array(), 'Expected either RollbackPlanId/ProtectionPlanId/Server Host GUID. Retry accepts only RollbackPlanId.');
				$protection_plan_id = $parameters['ProtectionPlanId']->getValue();
				if(!$protection_plan_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "ProtectionPlanId"));
				$plan_name = $this->recovery_obj->validate_protection_plan($protection_plan_id);
				if(!$plan_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionPlanId"));
				
				$rollback_hosts = $this->conn->sql_get_array("SELECT sourceId, targetId FROM applicationScenario WHERE planId = ?", "sourceId", "targetId", array($protection_plan_id));
				$rollback_host_ids = array_unique(array_keys($rollback_hosts));
				$src_host_names_array = $this->conn->sql_get_array("SELECT id, name FROM hosts WHERE FIND_IN_SET(id, ?)", "name", "name", array(implode(",", $rollback_host_ids)));
				$host_names = array_keys($src_host_names_array);
				$mt_hosts = array_unique(array_values($rollback_hosts));
				if(count($mt_hosts))
				{
					foreach($mt_hosts as $target_id) {						
						$host_info = $this->commonfunctions_obj->get_host_info($target_id);
						if($host_info['currentTime'] - $host_info['lastHostUpdateTime'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_HEART_BEAT', array('MTHostName' => $host_info['name'], 'MTIP' => $host_info['ipaddress'], 'Interval' => ceil(($host_info['currentTime'] - $host_info['lastHostUpdateTime'])/60), 'OperationName' => $function_name_user_mode));
					}
				}			
			}
			/* Validation 2 */
			elseif(isset($parameters['RollbackPlanId']))
			{
				if(isset($parameters['Servers']) || isset($parameters['ProtectionPlanId'])) ErrorCodeGenerator::raiseError($functionName, 'EC_MTAL_EXCLSIVE_ARGS', array(), 'Expected either ProtectionPlanId / Servers.');
				$recovery_plan_id = $parameters['RollbackPlanId']->getValue();
				if(!$recovery_plan_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "RollbackPlanId"));
				$plan_name = $this->recovery_obj->validate_migration_plan($recovery_plan_id);
				if(!$plan_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RollbackPlanId"));
				
				$rollback_hosts = $this->conn->sql_get_array("SELECT sourceId, targetId FROM applicationScenario WHERE planId = ?", "sourceId", "targetId", array($recovery_plan_id));
				if(count($rollback_hosts))
				{
					$rollback_host_ids = array_keys($rollback_hosts);
					$mt_hosts = array_unique(array_values($rollback_hosts));
					foreach($mt_hosts as $target_id) {						
						$host_info = $this->commonfunctions_obj->get_host_info($target_id);
						if($host_info['currentTime'] - $host_info['lastHostUpdateTime'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_HEART_BEAT', array('MTHostName' => $host_info['name'], 'MTIP' => $host_info['ipaddress'], 'Interval' => ceil(($host_info['currentTime'] - $host_info['lastHostUpdateTime'])/60), 'OperationName' => $function_name_user_mode));
					}
				}			
			}			
			/* Validation 3, 4 and 5 */
			elseif(isset($parameters['Servers']))
			{				
				if(isset($parameters['RollbackPlanId'])) ErrorCodeGenerator::raiseError($functionName, 'EC_MTAL_EXCLSIVE_ARGS', array(), 'Expected either RollbackPlanId/ProtectionPlanId/Server Host GUID. Retry accepts only RollbackPlanId.');
				if(isset($parameters['ProtectionPlanId']))  ErrorCodeGenerator::raiseError($functionName, 'EC_MTAL_EXCLSIVE_ARGS', array(), 'Expected either RollbackPlanId/ProtectionPlanId/Server Host GUID. Retry accepts only RollbackPlanId.');			
				
				$hosts = $parameters['Servers']->getChildList();
				foreach ($hosts as $key => $host_obj)
				{
					$server_recovery_option = "";
					$host = $host_obj->getChildList();
					$host_id = $host['HostId']->getValue();
					$host_info = $this->commonfunctions_obj->get_host_info($host_id);					
					/* Validation 3 */
					if(!$host_info) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
					
					$host_names[] = $host_info["name"];
					
					/* Validation 7 */
					//In case of failure, the recoveryPoint value will be tag name in case of latestTag/latestTime. In case of custom option and recovery failure case, the recoveryPoint value is Custom, and we can see TagName is additional data.
					if(isset($host['RecoveryPoint'])) $server_recovery_option = $host['RecoveryPoint']->getValue();
					if($server_recovery_option && !in_array($server_recovery_option, array("LatestTag", "LatestTime", "Custom")))
					ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECOVERY_OPTION', array("RecoveryOptions" => "LatestTag/LatestTime/Custom"));
				
					//In case first time rollback failed, and on re-try in second roll back request, the failed server have details of recovery point which failed last time. In Failure/retry case, the control goes into the server_recovery_option and keeps the servers in custom_rollback_host_ids array.
					if($server_recovery_option)
					{
						if($server_recovery_option == "Custom")
						{
							if(!isset($host['TagName'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "TagName for $key"));
							$tag_name = $host['TagName']->getValue();
							if(!$tag_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "TagName"));
							$server_tag_info[$host_id] = $tag_name;
						}
						//Failed roll back host ids.
						$custom_rollback_host_ids[$host_id] = $server_recovery_option;						
					}
					else 
					//First time roll back request CX got for the servers, then the control goes to rollback_host_ids array (or) Earlier failback successed ones.
					$rollback_host_ids[] = $host_id;					
				}
			}
			else
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ProtectionPlanId/RollbackPlanId/Servers"));
			}			
			
			$src_host_ids = $rollback_ids = array();
			//Merge all host ids here.
			$rollback_ids = array_merge($rollback_host_ids, array_keys($custom_rollback_host_ids));
			
			$scenario_status = $this->conn->sql_get_array("SELECT sourceId, executionStatus FROM applicationScenario WHERE FIND_IN_SET(sourceId, ?) AND scenarioType = 'DR Protection'","sourceId", "executionStatus", array(implode(",", $rollback_ids)));			
			foreach($scenario_status as $source_id => $execution_status)
			{
				//103 = FAIL_OVER_DONE. RP level it sends all the VMs in a request even though one roll back got success and others got failed. Re-try time they send all VMs in a request. In that case success ones we are resetting to avoid roll back request again. Failed ones will be captured in src_host_ids.
				/*if($execution_status == 103) 
				{
					$key = array_search($source_id, $rollback_host_ids);
					unset($rollback_host_ids[$key]);
				}
				else */
				//Earlier rollback failed sources/Fresh requests
					$src_host_ids[] = $source_id;
			}
			
			//Fresh or failed ones.
			$pair_details = $this->recovery_obj->get_pair_details($src_host_ids);
			/*print_r($pair_details); Sample data below.
			Array
(
    [1DB140AC-1CDE-7649-8E792DCDECE105FC] => Array
        (
            [0] => Array
                (
                    [sourceHostId] => 1DB140AC-1CDE-7649-8E792DCDECE105FC
                    [sourceDeviceName] => {1049541033}
                    [destinationHostId] => B14ABA93-010C-F443-913072F041F01688
                    [destinationDeviceName] => 3f055ec062b734837b71558faba9537c1
                    [ruleId] => 1603201449
                    [pairId] => 1301276107
                    [planId] => 11
                    [scenarioId] => 12
                    [pairCount] => 3
                )

            [1] => Array
                (
                    [sourceHostId] => 1DB140AC-1CDE-7649-8E792DCDECE105FC
                    [sourceDeviceName] => {2908697911}
                    [destinationHostId] => B14ABA93-010C-F443-913072F041F01688
                    [destinationDeviceName] => 36000c2999bd21f6d39de0f9c7ce3655f
                    [ruleId] => 268010181
                    [pairId] => 1739001621
                    [planId] => 11
                    [scenarioId] => 12
                    [pairCount] => 3
                )

            [2] => Array
                (
                    [sourceHostId] => 1DB140AC-1CDE-7649-8E792DCDECE105FC
                    [sourceDeviceName] => {3997066985}
                    [destinationHostId] => B14ABA93-010C-F443-913072F041F01688
                    [destinationDeviceName] => 37a17c484c09f4c06b1fbc90bd011cf4c
                    [ruleId] => 191027298
                    [pairId] => 2100531964
                    [planId] => 11
                    [scenarioId] => 12
                    [pairCount] => 3
                )

        )

)
*/
			
			//key as protection scenario execution status in src_host_ids
			#if ((count($pair_details) == 0) && protection_scenario_status == 92) 
			#ErrorCodeGenerator::raiseError($functionName, 'EC_NOT_IN_DIFF_SYNC', array('SourceHostGUID' => implode(',', $src_host_ids)));			
			#if(count($pair_details) == 0) ErrorCodeGenerator::raiseError($functionName, 'EC_NOT_IN_DIFF_SYNC', array('SourceHostGUID' => implode(',', $src_host_ids)));	
			
			foreach($src_host_ids as $host_id)
			{
				/* Validation 5 */
				if (array_key_exists($host_id, $pair_details))
				{
					if ($pair_details[$host_id][0]['pairCount'] > 0)
					{
						#print "entered, pair count exists\n";
						if(!array_key_exists($host_id, $pair_details) || $pair_details[$host_id][0]['pairCount'] != count($pair_details[$host_id]))
						ErrorCodeGenerator::raiseError($functionName, 'EC_NOT_IN_DIFF_SYNC', array('SourceHostGUID' => $host_id));
					}
				}
			}
			
			//Only for fresh rollback requests in case of Fresh single VM/Fresh Multi VM/Earlier failed single VM.
			if(count($rollback_host_ids) > 0 || $recovery_plan_id || $protection_plan_id)
			{
				/* Validation 8 */
				$dvap_is_set = ($recovery_option == "MultiVMLatestTag" || $multivm_sync_point == "Yes") ? "1" : "0";			
			
				/* Validation 9 */
				$check_rollback_exists = $this->recovery_obj->check_rollback_exists($rollback_host_ids);
				if($check_rollback_exists) ErrorCodeGenerator::raiseError($functionName, 'EC_ROLLBACK_EXISTS', array('SourceHostGUID' => implode(',', $rollback_host_ids)));				
				
				if($dvap_is_set)
				{
					$global_settings_pair_details = array();
					$pair_count_exists = TRUE;
					foreach($rollback_host_ids as $src_host_id)
					{
						if (array_key_exists($src_host_id, $pair_details))
						{
							if ($pair_details[$src_host_id][0]['pairCount'] > 0)
							{
								$global_settings_pair_details[$src_host_id] = $pair_details[$src_host_id];
							}
						}
						else
						{
							$pair_count_exists = FALSE;
						}
					}
					
					if ($pair_count_exists == TRUE)
					{		
						list($status, $return_data) = $this->recovery_obj->check_common_tag_exists($global_settings_pair_details, $recovery_option, $custom_tag_name);
						if(!$status) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECOVERY_POINT', array('RecoveryPoint' => $return_data));
					}
				}
				else
				{
					foreach($rollback_host_ids as $src_host_id)
					{	
						if (array_key_exists($src_host_id, $pair_details))
						{
							if ($pair_details[$src_host_id][0]['pairCount'] > 0)
							{
								$server_pair_details = array();
								$server_pair_details[$src_host_id] = $pair_details[$src_host_id];
								list($status, $return_data) = $this->recovery_obj->check_common_tag_exists($server_pair_details, $recovery_option, $custom_tag_name);
								if(!$status) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECOVERY_POINT', array('RecoveryPoint' => $return_data));
							}
						}
					}
				}
			}
			
			$server_tags = array();
			//Failed roll back sources on re-try for multi VMs. The failed ones the tag never pruned, because target is marking that tag to keep.
			if(count($custom_rollback_host_ids))
			{
				foreach(array_keys($custom_rollback_host_ids) as $src_host_id)
				{
					if (array_key_exists($src_host_id, $pair_details))
					{
						if ($pair_details[$src_host_id][0]['pairCount'] > 0)
						{
							$recovery_option = $custom_rollback_host_ids[$src_host_id];
							$server_custom_tag = $server_tag_info[$src_host_id];
							$server_tags[] = $server_tag_info[$src_host_id];
							$server_pair_details = array();
							$server_pair_details[$src_host_id] = $pair_details[$src_host_id];
							list($status, $return_data) = $this->recovery_obj->check_common_tag_exists($server_pair_details, $recovery_option, $server_custom_tag);
							if(!$status) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECOVERY_POINT', array('RecoveryPoint' => $return_data));
						}
					}
				}
			}
			//VM massage failure time, no pair data and so no data exists related to tags in DB, nothing to verify for tags.
			if(is_array($return_data))
			{
				if(count($host_names)) $host_names = implode(",", $host_names);
				
				if($recovery_option == "LatestTag") $recovery_point = "Recovery Point option is Latest Tag. Tag Name - ".$return_data[0][2]." Tag Time - ".$return_data[0][3]." For Hosts -- $host_names";
				elseif($recovery_option == "LatestTime") $recovery_point = "Recovery Point option is Latest Time. Available latest Start Time - ".$return_data[0]["RStartTime"]." For Hosts -- $host_names";
				else $recovery_point = "Recovery Point option is Custom. Custom tag available is ".$custom_tag_name." For Hosts -- $host_names";
				
				if(count($server_tags)) $recovery_point .= "Recovery Point option is server level Custom. Custom tag available is ".implode(",",$server_tags)." For Hosts -- $host_names";
				
				$mds_data_array = array();
				$mds_obj = new MDSErrorLogging();
				
				$mds_data_array["jobId"] = "";
				$mds_data_array["jobType"] = "Rollback";
				$mds_data_array["errorString"] = $recovery_point;
				$mds_data_array["eventName"] = "CS";
				$mds_data_array["errorType"] = "ERROR";
				$mds_data_array["activityId"] = $activityId;
				
				$mds_obj->saveMDSLogDetails($mds_data_array);
			}
			// Disable Exceptions for DB
			$this->global_conn->sql_commit();
			$this->global_conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->global_conn->sql_rollback();
			$this->global_conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->global_conn->sql_rollback();
			$this->global_conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function CreateRollback ($identity, $version, $functionName, $parameters)
	{
		global $DB_TRANSACTION_RETRY_LIMIT, $DB_TRANSACTION_RETRY_SLEEP;
		$retry = 0;
		$done = false;
		while (!$done and ($retry < $DB_TRANSACTION_RETRY_LIMIT)) 
		{
			try
			{		
				global $log_rollback_string;
				$log_rollback_string = "CreateRollback:";
				$this->global_conn->enable_exceptions();
				$this->global_conn->sql_begin();
				
				global $requestXML, $activityId, $clientRequestId, $A2E, $E2A, $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
				$from_capi=1;
				$host_ids = array();
				$solution_type = $CLOUD_PLAN_AZURE_SOLUTION;
				
				//We never get rollback plan id
				if(isset($parameters['RollbackPlanId']))
				{
					$db_status = 0;
					$recovery_plan_id = $parameters['RollbackPlanId']->getValue();
					$log_rollback_string = $log_rollback_string."planid = $recovery_plan_id".",";
					$requestedData = $this->recovery_obj->retry_rollback($recovery_plan_id);
				}
				else
				{
					$src_hosts = array();
					$rollback_plan_name = (isset($parameters['RollbackPlanName'])) ? $parameters['RollbackPlanName']->getValue() : 'RollbackPlan_'.time();
					if(isset($parameters['RecoveryPoint'])) $recovery_option = $parameters['RecoveryPoint']->getValue();
					if($recovery_option == "Custom") $global_tag_name = $parameters['TagName']->getValue();
					
					if(isset($parameters['ProtectionPlanId']))
					{
						$protection_plan_id = $parameters['ProtectionPlanId']->getValue();
						$log_rollback_string = $log_rollback_string."protection_plan_id: $protection_plan_id".",";
					}
					elseif(isset($parameters['Servers']))
					{
						$hosts = $parameters['Servers']->getChildList();
						$server_tag_info = array();
						//Collect all the sent source servers from the createRollback request
						foreach ($hosts as $key => $host_obj)
						{
							$host = $host_obj->getChildList();
							$host_id = $host['HostId']->getValue();
							$tag_name = (isset($host['TagName'])) ? $host['TagName']->getValue() : '';
							$host_ids[] = $host_id;
							$server_tag_info[$host_id] = $tag_name;
						}
						$host_ids_log = serialize($host_ids);
						$server_tag_info_log =  serialize($server_tag_info);
						$log_rollback_string = $log_rollback_string."host ids: $host_ids_log, server_tag_info = $server_tag_info_log".",";
					}
				
					if(count($host_ids) || $protection_plan_id)
					{
						$sql_args = array();
						if($protection_plan_id) 
						{
							$cond = " planId = ?";
							$sql_args[] = $protection_plan_id;
						}	
						else 
						{
							$cond = " FIND_IN_SET(sourceId, ?)";
							$sql_args[] = implode(",", $host_ids);
						}	
						
						$scenario_data = $this->global_conn->sql_query("SELECT sourceId, targetId, scenarioId, executionStatus, planId, applicationType FROM applicationScenario WHERE scenarioType = 'DR Protection' AND $cond", $sql_args);
						
						$scenario_data_log = serialize($scenario_data);
						$log_rollback_string = $log_rollback_string."Scenario data: $scenario_data_log".",";
						
						/*$ROLLBACK_INPROGRESS = 116;
						$ROLLBACK_FAILED = 118; 
						$ROLLBACK_COMPLETED = 130;
						$FAILOVER_DONE = 103;
						$VALIDATION_SUCCESS = 92;
						*/
						$pair_details_struct = array();
						$protection_scenario_status_of_sources = array();
						foreach($scenario_data as $key => $scenario_details)
						{
							$source_id = $scenario_details["sourceId"];
							$protection_scenario_status = $scenario_details["executionStatus"];
							$protection_scenario_status_of_sources[$source_id] = $protection_scenario_status;
							$pair_details_struct = $this->recovery_obj->get_pair_details($source_id);
							//Earlier rollback successed ones and those protection scenarios marked with FAIL OVER DONE(103). Earlier rollback failed ones(those protection scenarios marked with ROLL BACK FAILED and no pairs exist in DB(MT side rollback done, but VM massaging time got failures).
							#if($scenario_details["executionStatus"] == 103 || ($pair_details_struct[$source_id][0]['pairCount'] == 0 and $protection_scenario_status == 118)) 
							if(count($pair_details_struct) == 0) 
							{
								$src_hosts[$scenario_details["sourceId"]][0]['destinationHostId'] = $scenario_details["targetId"];
								$src_hosts[$scenario_details["sourceId"]][0]['planId'] = $scenario_details["planId"];
								$src_hosts[$scenario_details["sourceId"]][0]['scenarioId'] = $scenario_details["scenarioId"];
								$src_hosts[$scenario_details["sourceId"]][0]['pairCount'] = 0;
							}
							else 
							//(Fresh or rollback earlier failed ones) and pairs exists in DB
							$host_ids[] = $scenario_details["sourceId"];
							$solution_type = strtoupper($scenario_details["applicationType"]);
						}		
					}
					$src_hosts_log = serialize($src_hosts);
					$host_ids_log = serialize($host_ids);
					$log_rollback_string = $log_rollback_string."src hosts: $src_hosts_log, host ids: $host_ids_log".",";
						
					/*print_r($src_hosts);
					exit;
					Array
					(
						[1DB140AC-1CDE-7649-8E792DCDECE105FC] => Array
							(
								[0] => Array
									(
										[destinationHostId] => B14ABA93-010C-F443-913072F041F01688
										[planId] => 11
										[scenarioId] => 12
										[pairCount] => 0
									)

							)

					)*/
					//Fresh or rollback earlier failed ones.
					$pair_details = $this->recovery_obj->get_pair_details($host_ids);

					//Merge all(fresh/earlier failed/earlier success)		
					$pair_data = array_merge($pair_details, $src_hosts);
					
					$pair_data_log = serialize($pair_data);
					$log_rollback_string = $log_rollback_string."pair data: $pair_data_log ".",";
					
					//Default is LatestTag option, if no input RecoveryPoint found.
					$recovery_option = ($parameters['RecoveryPoint']) ? trim($parameters['RecoveryPoint']->getValue()) : "LatestTag";
					$recovery_vm_details = $pair_id_arr = $rule_id_arr = array();
					
					foreach($pair_data as $src_host_id => $pairs) 
					{
						$server_recovery_option = $recovery_option;
						$server_tag_name = ($recovery_option == "Custom") ?  $global_tag_name : '';
						
						if($server_tag_info[$src_host_id]) 
						{
							$server_tag_name = $server_tag_info[$src_host_id];
							$server_recovery_option = "Custom";
						}
						$recovery_vm_details[$src_host_id]['targethostid'] = $pairs[0]['destinationHostId']; 
						$recovery_vm_details[$src_host_id]['planid'] = $pairs[0]['planId'];            
						$recovery_vm_details[$src_host_id]['scenarioId'] = $pairs[0]['scenarioId'];            
						$recovery_vm_details[$src_host_id]['tagname'] = $server_tag_name;
						$recovery_vm_details[$src_host_id]['recoveryoption'] = $server_recovery_option;
						
						//If rollback success earlier for the VM from n VM(out of which some vms rollback failed), but in the next request ASR sends all the VMs again. For the protection rollback success earlier Vms, we should insert rollback plans with ROLLBACK SUCCESS(Which is 130).
						/*$ROLLBACK_INPROGRESS = 116;
						$ROLLBACK_FAILED = 118; 
						$ROLLBACK_COMPLETED = 130;
						$FAILOVER_DONE = 103;
						$VALIDATION_SUCCESS = 92;
						*/
						/*Only fail over done case, we are marking as rollback completed.
						if($protection_scenario_status_of_sources[$src_host_id] == 103)
							$recovery_vm_details[$src_host_id]['executionstatus'] = 130;
						else 
						*/
						$recovery_vm_details[$src_host_id]['executionstatus'] = 116;
							
						
						
						foreach($pairs as $pair_info)
						{
							if ($pair_info['pairId'] and $pair_info['ruleId'])
							{
								$pair_id_arr[] = $pair_info['pairId'];
								$rule_id_arr[] = $pair_info['ruleId'];
							}
						}
					}
					/*print_r($pair_id_arr);
					print_r($rule_id_arr);
					Array
	(
		[0] => 1301276107
		[1] => 1739001621
		[2] => 2100531964
	)
	Array
	(
		[0] => 1603201449
		[1] => 268010181
		[2] => 191027298
	)*/
					
					$rule_id_arr_log = serialize($rule_id_arr);
					$recovery_vm_details_log = serialize($recovery_vm_details);
					
					if (count($pair_id_arr) > 0)
					{
						// Pause pairs and retention in case of rollback in order to have the older retenion also.
						$this->volume_obj->pausePairs($pair_id_arr, $rule_id_arr);
					}	
					$log_rollback_string = $log_rollback_string."rule_id_arr_log: $rule_id_arr_log, recovery_vm_details_log = $recovery_vm_details_log".",";
					// Create rollback plan and scenarios once pause is done.
					$requestedData = $this->recovery_obj->create_rollback_plan($recovery_vm_details, $solution_type, $recovery_plan_id, $rollback_plan_name);
				}
				
				$policy_id_arr = $requestedData['policyId'];
				if(count($policy_id_arr))
				{
					foreach($policy_id_arr as $key => $policy_id)
					{
						$policy_ids = $policy_ids . $policy_id . ',';		
					}
					$policy_ids =  ','.$policy_ids ;			
				}
				$log_rollback_string = $log_rollback_string."policy_ids = $policy_ids".",";
				$api_ref = $this->commonfunctions_obj->insertRequestXml($requestXML,$functionName,$activityId,$clientRequestId,$policy_ids);
				$request_id = $api_ref['apiRequestId'];
				$log_rollback_string = $log_rollback_string."request_id = $request_id".",";
				$apiRequestDetailId = $this->commonfunctions_obj->insertRequestData($requestedData,$request_id);
				
				$response  = new ParameterGroup ( "Response" );
				if(!is_array($api_ref) || !$request_id || !$apiRequestDetailId)
				{
					ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Failed due to internal error");
				}
				else
				{
					$response->setParameterNameValuePair('RequestId',$request_id);					
				}    
				$mds_data_array = array();
				if ($activityId)
				{
					$activity_id_to_log = $activityId;
				}
				else
				{
					$activity_id_to_log = "9cc8c6e1-2bfb-4544-a850-a23c9ef8162d";
				}
				$mds_obj = new MDSErrorLogging();
				$mds_data_array["jobId"] = "";
				$mds_data_array["jobType"] = "Rollback";
				$mds_data_array["errorString"] = $log_rollback_string;
				$mds_data_array["eventName"] = "CS";
				$mds_data_array["errorType"] = "ERROR";
				$mds_data_array["activityId"] = $activity_id_to_log;
				$mds_obj->saveMDSLogDetails($mds_data_array);
				// Disable Exceptions for DB
				$this->global_conn->sql_commit();
				$this->global_conn->disable_exceptions();
				$done = true;
				return $response->getChildList();
			}
			catch(SQLException $sql_excep)
			{
				// Disable Exceptions for DB
				$this->global_conn->sql_rollback();
				$this->global_conn->disable_exceptions();
				if ($retry < $DB_TRANSACTION_RETRY_LIMIT)
				{
					sleep($DB_TRANSACTION_RETRY_SLEEP);
					$retry++;
				}	
				if ($retry == $DB_TRANSACTION_RETRY_LIMIT)
				{
					$this->commonfunctions_obj->captureDeadLockDump($requestXML, $activityId, "Rollback");
					
					ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
				}
			}
			catch (APIException $apiException)
			{
				// Disable Exceptions for DB
				$this->conn->disable_exceptions();           
				throw $apiException;
			}
			catch(Exception $excep)
			{
				// Disable Exceptions for DB
				$this->global_conn->sql_rollback();
				$this->global_conn->disable_exceptions();
				$done = true;	
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
			}
		}		
	}
	
	public function ListConsistencyPoints_validate($identity, $version, $functionName, $parameters)
	{
		$function_name_user_mode = 'List Consistency Points';
		try
		{
			$this->global_conn->enable_exceptions();
			
			$other_parameters = array("ProtectionPlanId", "TagStartDate", "TagEndDate", "ApplicationName", "UserDefinedEvent", "TagName", "Accuracy", "GetRecentConsistencyPoint", "MultiVMSyncPoint", "Servers");
			$mandatory_parameters = array("TagStartDate", "TagEndDate");
			
			foreach($parameters as $param => $param_obj)
			{
				/*
				if(in_array($param, $mandatory_parameters))
				{
					if(!isset($parameters[$param]))  ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $param));
				} */
				if(isset($parameters[$param]))
				{
					if($param != "Servers")
					{
						$value = $parameters[$param]->getValue();
						if(!$value) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $param));
					}
				
					if($param == "TagStartDate") $tag_start_time = $value;
					elseif($param == "TagEndDate") $tag_end_time = $value;		
					elseif($param == "ProtectionPlanId")
					{
						$plan_name = $this->recovery_obj->validate_protection_plan($value);
						if(!$plan_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionPlanId"));
						$protection_details = $this->recovery_obj->getPairsFromPlanId($value, "plan");
						/* Validation 4 */
						if(!count($protection_details)) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_SCENARIO_EXIST', array('PlanID' => $value));	
						$pair_details = $protection_details[$value];
						$src_host_ids = array_keys($pair_details);
						foreach ($src_host_ids as $host_id)
						{
							if(!$pair_details[$host_id] || $pair_details[$host_id][0]['pairCount'] != count($pair_details[$host_id]))
							ErrorCodeGenerator::raiseError($functionName, 'EC_NOT_IN_DIFF_SYNC', array('SourceHostGUID' => $host_id));
						}
					}
					elseif($param == "ApplicationName")
					{				
						$consistency_tag_list = $this->conn->sql_get_array("SELECT ValueName, ValueData FROM consistencyTagList WHERE ValueType = 'ACM'", "ValueName", "ValueData", array());
						if(!in_array($value, array_values($consistency_tag_list))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $param));
					}
					elseif($param == "UserDefinedEvent")
					{
					}
					elseif($param == "TagName")
					{
					}
					elseif($param == "Accuracy")
					{
						if(!in_array($value, array("Exact", "Approximate", "Not Guaranteed"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $param));
					}
					elseif($param == "GetRecentConsistencyPoint")
					{
						if(!in_array($value, array("Yes", "No"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $param));
					}
					elseif($param == "MultiVMSyncPoint")
					{
						if(!in_array($value, array("Yes", "No"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $param));
					}
					elseif($param == "Servers")
					{
						if(isset($parameters['ProtectionPlanId']))  ErrorCodeGenerator::raiseError($functionName, 'EC_MTAL_EXCLSIVE_ARGS', array(), "Expected either ProtectionPlanId/Server Host GUID.");
						// Get all servers
						$servers = $parameters[$param]->getChildList();
						if(!is_array($servers)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $param));
						$server_ids = array();
						foreach($servers as $name => $server)
						{
							$server_id = $servers[$name]->getValue();
							if(in_array($server_id, $server_ids)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $param.". Duplicate ServerHostId - ". $name));
							// Validate as a valid host
							$host_info = $this->commonfunctions_obj->get_host_info($server_id);
							// Find duplicates
							if(!$host_info) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
							$server_ids[] = $server_id;
						}
						// Get Pair Details
						$pair_details = $this->recovery_obj->get_pair_details($server_ids);
						
						// If no protection found
						//if(!count($pair_details))
						//ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PROTECTION_SERVER', array('SourceHostName' => implode(',', $server_ids)));
						
						// If no protection found for specific server
						foreach($server_ids as $src_host_id)
						{
						   if(!array_key_exists($src_host_id, $pair_details) || $pair_details[$src_host_id][0]['pairCount'] != count($pair_details[$src_host_id])) 
						   ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PROTECTION_SERVER', array('SourceHostName' => $name));
						}
					}
					elseif(!in_array($param, $other_parameters)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_PRM', array('Parameter' => $param));
				}
			}
			/* Validate the difference between dates */
			if($tag_start_time && $tag_end_time)
			{
				if($tag_end_time <= $tag_start_time) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "TagStartDate and TagEndDate"));
				// When the search period is more than 5 days
				if(($tag_end_time - $tag_start_time) > 5*24*60*60) ErrorCodeGenerator::raiseError($functionName, 'EC_TAG_TIME_GREATER');
			}
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function ListConsistencyPoints($identity, $version, $functionName, $parameters)
	{
		try
		{
			$this->global_conn->enable_exceptions();
			$response = new ParameterGroup("Response");
			
			$accuracy_array = array("Exact" => "0", "Approximate" => "1", "Not Guarenteed" => "2");
			$src_host_ids = array();
			if(isset($parameters['Servers']))
			{
				// Get all servers
				$servers = $parameters['Servers']->getChildList();
				foreach($servers as $name => $server)
				{
					$server_id = $servers[$name]->getValue();
					$src_host_ids[$name] = $server_id;
				}
				// Get Pair Details
				$protection_details = $this->recovery_obj->get_pair_details($src_host_ids, "plan");		
			}		
			elseif(isset($parameters['ProtectionPlanId']))
			{
				$protection_plan_id = $parameters['ProtectionPlanId']->getValue();
				$protection_details = $this->recovery_obj->getPairsFromPlanId($protection_plan_id, "plan");
				$pair_details = $protection_details[$protection_plan_id];
				$src_host_ids = array_keys($pair_details);
				foreach ($src_host_ids as $host_id)
				{
					if(!$pair_details[$host_id] || $pair_details[$host_id][0]['pairCount'] != count($pair_details[$host_id]))
					ErrorCodeGenerator::raiseError($functionName, 'EC_NOT_IN_DIFF_SYNC', array('SourceHostGUID' => $host_id));
				}
			}
			else
			{			
				$protection_details = $this->recovery_obj->get_pair_details(NULL, "plan");
			}
			
			if(!$protection_details || !count($protection_details)) 
			{
				$grp = new ParameterGroup("Empty");
				$response->addParameterGroup($grp);
				return $response->getChildList();
			}
			
			$tag_query_data['api_calling'] = 1;
			$tag_query_data['hidRadio'] = 1;
			
			if(isset($parameters['TagStartDate']) && isset($parameters['TagEndDate']))
			{
				$tag_query_data['filter_tag_str'] = "between_times";
				$tag_query_data['filter_tag_start_time'] = $parameters['TagStartDate']->getValue();
				$tag_query_data['filter_tag_end_time'] = $parameters['TagEndDate']->getValue();
			}
			if(isset($parameters['ApplicationName']))
			{
				$tag_query_data['filter_tag_str'] .= ",application";
				$tag_query_data['filter_tag_application'] = $parameters['ApplicationName']->getValue();
			}
			if(isset($parameters['TagName']))
			{
				$tag_query_data['filter_tag_str'] .= ",tagname";
				$tag_query_data['filter_tag_name'] = $parameters['TagName']->getValue();
			}
			if(isset($parameters['Accuracy']))
			{
				$accuracy = $parameters['Accuracy']->getValue();
				$tag_query_data['filter_tag_str'] .= ",accuracy";
				$tag_query_data['filter_tag_accuracy'] = $accuracy_array[$accuracy];
			}		
			
			$consistency_points = array();		
			foreach($protection_details as $plan_id => $protection)
			{
				list($common_consistency_points, $host_consistency_points) = $this->recovery_obj->get_common_tag_across_pairs($protection, 2, $tag_query_data);
				if(count($common_consistency_points))
				$consistency_points[$plan_id]['plan_level_common_points'] = $common_consistency_points;
				if(count($host_consistency_points))
				$consistency_points[$plan_id]['server_level_common_points'] = $host_consistency_points;
			}
			
			if(!count($consistency_points))
			{
				$grp = new ParameterGroup("Empty");
				$response->addParameterGroup($grp);
				return $response->getChildList();
			}
			else{
				/*
				2 - TagName
				1 - Application Name
				5 - Accuracy
				9 - Timestamp	
				12 - Timestamp	Unix
				*/
				$plan_count = 1;
				foreach($consistency_points as $plan_id => $details)
				{
					$server_level_consistency_points = $details['server_level_common_points'];
					if(count($server_level_consistency_points))
					{
						$plan_grp = new ParameterGroup("Plan".$plan_count);
						if(count($details['plan_level_common_points']))
						{
							$plan_grp->setParameterNameValuePair("ProtectionPlanId", $plan_id);
							$tags_grp = new ParameterGroup("ConsistencyPoints");
							$tags_grp = $this->GetConsistencyPointsResponse($details['plan_level_common_points'], $tags_grp);
							$plan_grp->addParameterGroup($tags_grp);
						}
						$servers_grp = new ParameterGroup("Servers");
						$server_count = 1;						
						foreach($server_level_consistency_points as $server_id => $common_tags)
						{
							$server_grp = new ParameterGroup('Server'.$server_count);
							$server_grp->setParameterNameValuePair("HostId", $server_id);
							if(count($common_tags))	$server_grp = $this->GetConsistencyPointsResponse($common_tags, $server_grp);
							$servers_grp->addParameterGroup($server_grp);
							$server_count++;
						}
						$plan_grp->addParameterGroup($servers_grp);
						$response->addParameterGroup($plan_grp);
						$plan_count++;
					}
				}
			}
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function GetConsistencyPointsResponse($consistency_points, $response_handle)
	{
		/*
		2 - TagName
		1 - Application Name
		5 - Accuracy
		9 - Timestamp
		10 - Short code for application name.
		12 - Timestamp Unix
		*/
		$tag_count = 1;
		$temp_server_tag_details = array();
		foreach ($consistency_points as $point)
		{
			if(count($temp_server_tag_details) == 2) break;

			// Always sending only 2 tags to DRA. 1 - SystemLevel, 2 - FS tag.
			// We don't have custom option in case of failback, so no need to send all the tags.
			if(($point[10] == "SystemLevel" && $temp_server_tag_details[0][10] != "SystemLevel") ||
			($point[10] == "FS" && $temp_server_tag_details[0][10] != "FS"))
			{
				$tag_name = $point[2];
				$tag_time = substr($point[9], 0, 11);
				$tag_time_utc = $tag_time - 11644473600;
				$consistency_grp = new ParameterGroup("Tag".$tag_count);
				$consistency_grp->setParameterNameValuePair("TagName", $tag_name);
				$consistency_grp->setParameterNameValuePair("LatestTagAccuracy", $point[5]);
				$consistency_grp->setParameterNameValuePair("LatestTagApplication", $point[1]);
				$consistency_grp->setParameterNameValuePair("LatestTagTimeStamp", $tag_time_utc);
				$response_handle->addParameterGroup($consistency_grp);
				$tag_count++;
				array_push($temp_server_tag_details, $point);
			}
		}
		return $response_handle;
	}
}
?>