<?php
class Policy
{
	private $conn;
	private $app_obj;
	
	function __construct()
    {
		global $conn_obj;
		$this->conn = $conn_obj;
		$this->recovery_plan_obj = $this->_getPolicyObj('RecoveryPlan');
		$app_obj = $this->_getPolicyObj('Application');
		$this->common_functions_obj = $this->_getPolicyObj('CommonFunctions');
    }
		
	private function _getPolicyObj($application)
	{
		switch($application)
		{
			case 'RecoveryPlan':
				$obj = new RecoveryPlan($this);
				break;
				
			case 'Application':
				$obj = new Application($this);
				break;
				
			case 'CommonFunctions':
				$obj = new CommonFunctions($this);
				break;		
				
			default:
				$obj = 0;
		}
		return $obj;
	}
	
	public function insert_policy($trg_id, $policy_type, $policyScheduledType, $policy_run_every, $scenario_id, $policy_exclude_from, $policy_exclude_to, $policy_parameters, $step,$solution_type = '',$app_type = '',$configuration_options='',$consistency='1',$forward_solution_type ='')
	{
		global $logging_obj;
		$sql = "INSERT INTO 
						policy
						(
							hostId,
							policyType,
							policyScheduledType,
							policyRunEvery,
							scenarioId,
							policyExcludeFrom,
							policyExcludeTo,
							policyParameters,
							policyName
						) 
					VALUES 
						(
							'$trg_id',
							'$policy_type',
							'$policyScheduledType',
							'$policy_run_every',
							'$scenario_id',
							'$policy_exclude_from',
							'$policy_exclude_to',
							'$policy_parameters',
							'$step'							
						)";
						
		$rs = $this->conn->sql_query($sql);
		$policy_id = $this->conn->sql_insert_id();		
		$logging_obj->my_error_handler("DEBUG","insert_prepare_target_policy : sql::$sql \n policy_id : $policy_id \n");
		
		if(!$configuration_options)
		{
			$trg_id_array = explode(",",$trg_id );
			foreach($trg_id_array as $index=> $host_id_val)
			{
				$sql = "INSERT INTO
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
								'$host_id_val'
							)";
				$rs = $this->conn->sql_query($sql);
			}
			$logging_obj->my_error_handler("DEBUG","insert_prepare_target_policy : sql::$sql \n");
		}		
		return $policy_id;
	}
	
	public function getPolicyInfo($policy_id)
	{
		$sql = "SELECT
					*
				FROM
					policy
				WHERE
					policyId = '$policy_id'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$scenario_id = $row->scenarioId;
		$policy_type = $row->policyType;
		$policy_scheduled_type = $row->policyScheduledType;
		$planId = $row->planId;
		
		return $result = array('scenarioId' => $scenario_id,
							   'policyType' => $policy_type,
							   'policyScheduledType' => $policy_scheduled_type,
							   'planId' => $planId);
	}
	
	public function updateAppScenario($scenario_details, $scenario_id)
	{
		$cdata = serialize($scenario_details);
		$str = "<plan><header><parameters>";
		$str.= "<param name='name'>".$scenario_details['planName']."</param>";
		$str.= "<param name='type'>Protection Plan</param>";
		$str.= "<param name='version'>1.1</param>";
		$str.= "</parameters></header>";
		$str.= "<data><![CDATA[";
		$str.= $cdata;
		$str.= "]]></data></plan>";

		$str = $this->conn->sql_escape_string($str);

		$sql = "UPDATE 
					applicationScenario
				SET
					scenarioDetails = '$str'
				WHERE
					scenarioId='$scenario_id'";
		$update_sql_rs = $this->conn->sql_query($sql);
	}
	
	public function _getScriptPolicy($data)
	{
		global $logging_obj,$app_obj;
		$logging_obj->my_error_handler("INFO","_getScriptPolicy---data".print_r($data,true));
		$result = array();
		$app_type = '0';
		$result[] = 0;
		$result[] = '1.0';
			
		$policyParameters = unserialize($data->policyParameters);
		if($policyParameters['StartTime'])
		{
			$start_time = $policyParameters['StartTime'];
			$start_time = "$start_time";
		}
		else
		{
			$start_time = '';
		}
		$sql = "SELECT scenarioId FROM policy WHERE policyId='$data->policyId'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$scenario_id = $row->scenarioId;
		
		$protection_scenario_id = $this->conn->sql_get_value('applicationScenario', 'referenceId',"scenarioId='$scenario_id' and scenarioType='Data Validation and Backup'");
		
		$params = $app_obj->getScenarioParams($protection_scenario_id);
		$logging_obj->my_error_handler("DEBUG","protection_scenario_id---$protection_scenario_id");
		
		if($params['scenarioType'] == 'Backup Protection' || $params['scenarioType'] == 'DR Protection')
		{
			$app_type = $params['applicationType'];
		}
		
		$result[] = array(  'ApplicationType'=>$app_type,
							'Context' => 'BackUp',
							'PolicyId'=>$data->policyId,
							'PolicyType'=>$data->policyName,
							'ScenarioType' => $params['scenarioType'],
							'ScheduleInterval'=>$data->policyRunEvery,
							'ScheduleType'=>$data->policyScheduledType,
							'StartTime' => $start_time
						);
						
		
		$recovery_details = $app_obj->getTagDetails($data->policyId);
				
		$result[] = serialize($recovery_details);
		return $result;
	}
	
	public function _getProtectionSettings($host_id)
	{
		global $logging_obj, $app_obj;
		$logging_obj->my_error_handler("DEBUG"," _getProtectionSettings with host_id : $host_id ");
		$scenario_id_list = array();
		
		$sql = "SELECT 
				DISTINCT
				scenarioId,
				applicationType
			FROM
				applicationScenario 
			WHERE
				(sourceId like '%$host_id%' OR targetId like '%$host_id%') AND
				scenarioType IN ('DR Protection','Backup Protection')";
		$result_set = $this->conn->sql_query($sql);
		while($row = $this->conn->sql_fetch_object($result_set))
		{
			if( $row->scenarioId )
			{	
				$scenario_id_list[] = $row->scenarioId;
			}
		}
		$scenario_id_list = array_unique($scenario_id_list);
		$logging_obj->my_error_handler("DEBUG"," _getPairInfo called from _getProtectionSettings for host_id : $host_id ");
		$pair_info = $this->_getPairInfo($host_id,$scenario_id_list);
		return $pair_info;
	}

	private function _getPairInfo($host_id,$scenario_id_list)
	{
		global $logging_obj, $app_obj;

		$logging_obj->my_error_handler("DEBUG"," _getPairInfo with host_id : $host_id scenario_id_list : ".print_r($scenario_id_list,TRUE));

		$result = array();
		$src_vs = '0';
		$trg_vs = '0';
		$src_vs_ip = '0';
		$trg_vs_ip = '0';
		
		$commonfunctions_obj = new CommonFunctions();
		$protection_plan_obj = new ProtectionPlan();
		$pair_details = array();
		foreach($scenario_id_list as $scenario_id)
		{
			$app_volumes = array();
			$other_volumes = array();
			$scenario_details = $app_obj->getScenarioParams($scenario_id);
            
			$qry = "SELECT 
						solutionType 
					FROM 
						applicationPlan appP , 
						applicationScenario appS
					WHERE 
						appS.planId = appP.planId AND
						appS.scenarioId ='$scenario_id'";
			
			$qry_rs = $this->conn->sql_query($qry);
			$qry_row = $this->conn->sql_fetch_object($qry_rs);
			$solution_type = $qry_row->solutionType;
			
			$src_host_id = $scenario_details['sourceHostId'];
			$trg_host_id = $scenario_details['targetHostId'];
			$app_type = $scenario_details['applicationType'];
			$app_version = $scenario_details['applicationVersion'];
			
			$scenario_protection_direction = $protection_plan_obj->getProtectionDirection($scenario_id);
				
			if( $solution_type == 'PRISM' ) // PRISM SOLUTION
			{
				if( $scenario_protection_direction == 'forward' || $scenario_protection_direction == '')
				{
					foreach ($scenario_details['pairInfo'] as $key=>$value)
					{
						$current_src_instance_id = $value['srcAPPId'];
						$current_src_instance_id_arr = explode(",",$current_src_instance_id);
						$current_src_instance_id = implode("','",$current_src_instance_id_arr);
						
						$original_src_instance_id = $value['srcAPPId'];
						$original_src_instance_id_arr = explode(",",$original_src_instance_id);
						$original_src_instance_id = implode("','",$original_src_instance_id_arr);
					}
					$original_is_src_clustered = $scenario_details['srcIsClustered'];
					$original_is_trg_clustered = $scenario_details['targetIsClustered'];
				}
				elseif( $scenario_protection_direction == 'reverse')
				{
					foreach ($scenario_details['pairInfo'] as $reverse_key=>$reverse_value)
					{
						$temp_src_host_id = $reverse_value['sourceId'];
						$temp_src_host_id_arr = explode(",",$temp_src_host_id);
						
						$current_trg_instance_id = $reverse_value['srcAPPId'];	
						$current_trg_instance_id_arr = explode(",",$current_trg_instance_id);
						$current_trg_instance_id = implode("','",$current_trg_instance_id_arr);						
						
						$temp_trg_app_ins_name = $app_obj->getInstanceName($current_trg_instance_id_arr[0]);
						
						$qry = "SELECT applicationInstanceId FROM applicationHosts WHERE hostId='$temp_src_host_id_arr[0]' AND applicationInstanceName='$temp_trg_app_ins_name'";
						$rs_qry = $this->conn->sql_query($qry);
						$rs_qry_data = $this->conn->sql_fetch_object($rs_qry);
						
						$current_src_instance_id = $rs_qry_data->applicationInstanceId;
						$current_trg_instance_id_arr = explode(",",$current_src_instance_id);
						$current_src_instance_id = implode("','",$current_trg_instance_id_arr);
						
	
						$original_src_instance_id = $current_src_instance_id;
						
						$original_trg_instance_id = $reverse_value['targetAPPId'];
						$original_trg_instance_id_arr = explode(",",$original_trg_instance_id);
						$original_trg_instance_id = implode("','",$original_trg_instance_id_arr);
					}
					$original_is_src_clustered = $scenario_details['targetIsClustered'];
					$original_is_trg_clustered = $scenario_details['srcIsClustered'];
				}				
			}
			else // NON-PRISM SOLUTION			
			{
				$current_src_instance_id = $scenario_details['srcAppId'];
				$current_src_instance_id_arr = explode(",",$current_src_instance_id);
				$current_src_instance_id = implode("','",$current_src_instance_id_arr);
				
				$current_trg_instance_id = $scenario_details['targetAPPId'];
				$current_trg_instance_id_arr = explode(",",$current_trg_instance_id);
				$current_trg_instance_id = implode("','",$current_trg_instance_id_arr);
					
				if( $scenario_protection_direction == 'forward' || $scenario_protection_direction == '')
				{
					$original_src_instance_id = $scenario_details['srcAppId'];
					$original_src_instance_id_arr = explode(",",$original_src_instance_id);
					$original_src_instance_id = implode("','",$original_src_instance_id_arr);
					
					$original_trg_instance_id = $scenario_details['targetAPPId'];
					$original_trg_instance_id_arr = explode(",",$original_trg_instance_id);
					$original_trg_instance_id = implode("','",$original_trg_instance_id_arr);
					
					$original_is_src_clustered = $scenario_details['srcIsClustered'];
					$original_is_trg_clustered = $scenario_details['targetIsClustered'];
				}
				elseif($scenario_protection_direction == 'reverse')
				{
					$original_src_instance_id = $scenario_details['targetAPPId'];
					$original_src_instance_id_arr = explode(",",$original_src_instance_id);
					$original_src_instance_id = implode("','",$original_src_instance_id_arr);
					
					$original_trg_instance_id = $scenario_details['srcAppId'];
					$original_trg_instance_id_arr = explode(",",$original_trg_instance_id);
					$original_trg_instance_id = implode("','",$original_trg_instance_id_arr);
					
					$original_is_src_clustered = $scenario_details['targetIsClustered'];
					$original_is_trg_clustered = $scenario_details['srcIsClustered'];
				}
			}
			
			$where_src_cond = " applicationInstanceId IN ('$current_src_instance_id')";
			$where_tgt_cond = " applicationInstanceId IN ('$current_trg_instance_id')";
			$scenario_type = $scenario_details['scenarioType'];
						
			$application = $app_type;
			$app_type = $app_obj->get_version($app_type,$app_version);
			
			if($application == 'BULK')
			{
				$app_type = $application;
			}            
						
			$sql = "SELECT
					sourceId,
					targetId
				FROM
					applicationScenario
				WHERE
					scenarioId='$scenario_id'";
			$rs = $this->conn->sql_query($sql);
			while($row = $this->conn->sql_fetch_object($rs))
			{
				$source_host_id = $row->sourceId;
				$target_host_id = $row->targetId;
			}

			//---------------------
			
			/*
				Below is for current
			*/
			$is_src_clustered = $scenario_details['srcIsClustered'];
			$is_trg_clustered = $scenario_details['targetIsClustered'];
			
			$key = 'VERSION_'.$app_type;
			
			if($is_src_clustered)
			{
				$sql = "SELECT
							applicationInstanceName,
							clusterId AS clusterId
						FROM
							applicationHosts
						WHERE
							$where_src_cond";
				$rs = $this->conn->sql_query($sql);
				
				if ($rs)
				{
					while($data = $this->conn->sql_fetch_object($rs))
					{
						$src_instance_name = $data->applicationInstanceName;
						$src_cluster_id = $data->clusterId;
					}
				}
			}
			
			if($is_trg_clustered && $application != 'ORACLE')
			{				
				$sql = "SELECT
							applicationInstanceName,
							clusterId AS clusterId
						FROM
							applicationHosts
						WHERE
							$where_tgt_cond";
				$rs = $this->conn->sql_query($sql);
				if( $rs )
				{
					while($data = $this->conn->sql_fetch_object($rs))
					{
						$trg_instance_name = $data->applicationInstanceName;
						$trg_cluster_id = $data->clusterId;
					}
				}
			}
			
			if($original_is_src_clustered && $application != 'ORACLE' && $application != 'DB2')
			{
				$src_vs_details = $this->getProdDRServerDetails($original_src_instance_id);
				$src_vs = $src_vs_details['virtualServerName'];
				$src_vs_ip = $src_vs_details['virtualServerIP'];
			}
			
			if($original_is_trg_clustered && $application != 'ORACLE' && $application != 'DB2') 
			{
				$trg_vs_details = $this->getProdDRServerDetails($original_trg_instance_id);
				$trg_vs = $trg_vs_details['virtualServerName'];
				$trg_vs_ip = $trg_vs_details['virtualServerIP'];
			}			
			
			if($solution_type == "HOST" && ($scenario_type == 'DR Protection') && ($app_type != 'ORACLE') && ($application !='BULK') && ($app_type != 'DB2'))
			{
				$sql = "SELECT
							 hostId
					 FROM
							 applicationHosts
					 WHERE
							 applicationInstanceId IN ('$original_trg_instance_id')";
				 $rs = $this->conn->sql_query($sql);
				 $data1 = $this->conn->sql_fetch_object($rs);
				 $target_host_id = $data1->hostId;
				 $target_host_name = $commonfunctions_obj->get_host_info($target_host_id);
			}
			else
			{
				$target_host_id = explode(",",$target_host_id);
				$target_host_name = $commonfunctions_obj->get_host_info($target_host_id[0]);
			}
			
			$app_arr['ApplicationType'] = $app_type;
			$app_arr['ProtectionType'] = $scenario_type;
						
			$app_arr['DRVirtualServerName'] = $trg_vs;
			$app_arr['DRServerVirtualServerIP'] = $trg_vs_ip;
			$app_arr['DRServerName'] = $target_host_name['name'];
			$app_arr['DRServerIP'] = $target_host_name['ipaddress'];
			
			if($solution_type == "HOST" && ($scenario_type == 'DR Protection') && ($app_type != 'ORACLE') &&  ($application !='BULK') && ($app_type != 'DB2'))
			{
				$sql = "SELECT
								hostId
						 FROM
								 applicationHosts
						 WHERE
								 applicationInstanceId IN ('$original_src_instance_id')";
				$rs = $this->conn->sql_query($sql);
				$data1 = $this->conn->sql_fetch_object($rs);
				$source_host_id = $data1->hostId;
				$source_host_name = $commonfunctions_obj->get_host_info($source_host_id);
			}
			else
			{
				$src_host = explode(",",$source_host_id);
				$source_host_name = $commonfunctions_obj->get_host_info($src_host[0]);
			}	
			
			$app_arr['ProductionVirtualServerName'] = $src_vs;
			$app_arr['ProductionVirtualServerIP'] = $src_vs_ip;
			$app_arr['ProductionServerName'] = $source_host_name['name'];
			$app_arr['ProductionServerIP'] = $source_host_name['ipaddress'];
			$app_arr['Direction'] = '0';
			if($scenario_details['direction'])
			{
				$app_arr['Direction'] = $scenario_details['direction'];
			}			
			
			if ( $solution_type != "PRISM")
			{
				$logging_obj->my_error_handler("DEBUG"," _getPairInfo aaaaa : ".print_r($scenario_details,TRUE)."\n");
				if (array_key_exists("pairInfo",$scenario_details))
				{
					foreach ($scenario_details['pairInfo'] as $keyA=>$valueA)
					{
						if (array_key_exists("pairDetails",$valueA))
						{
							foreach ($valueA['pairDetails'] as $keyB=>$valueB)
							{
								if ($application == 'ORACLE' || $application == 'DB2')
								{
									if($trg_host_id == $host_id)
									{
										$app_volumes[] = $valueB['trgVol'];
									}
									else
									{
										$app_volumes[] = $valueB['srcVol'];
									}
								}
								
								if($scenario_details['applicationType'] == 'BULK' || $scenario_details['applicationType'] == 'AWS')		
								{
									if($solution_type == "ARRAY")
									{
										$app_volumes[] = "/dev/mapper/".$valueB['trgVol'];
									}
									else
									{
										$app_volumes[] = $valueB['trgVol'];
									}							
								}
							}
						}
					}
				}
				
				if(($scenario_type == 'DR Protection' || ($scenario_type == 'Backup Protection')) && ($scenario_details['applicationType'] != 'BULK') && ($scenario_details['applicationType'] != 'ORACLE') && ($scenario_details['applicationType'] != 'DB2'))	
				{
					if($scenario_details['applicationVolumes']) $app_volumes = explode(",",$scenario_details['applicationVolumes']);
					if($scenario_details['otherVolumes']) 
					{
						if($scenario_protection_direction == 'reverse')
						{
							$other_volumes = explode(",",$scenario_details['otherVolumes']);
						}
						else
						{
							$other_volumes1 = explode(",",$scenario_details['otherVolumes']);
							$remote_host1 = explode(",",$src_host_id);
							foreach($other_volumes1 as $key => $volume)
							{
								$pair_details1 = $this->getPairdetails($trg_host_id, $remote_host1[0], $scenario_id, $volume);
								$other_volumes[] = 	$pair_details1[0]['destinationDeviceName'];
							}
						}	
					}
						
				}					
			}

			if($trg_host_id == $host_id)
			{
				$local_host = $host_id;				
				
				if ( $solution_type == "PRISM" ) // PRISM SOLUTION
				{
					$app_volumes = array();

					foreach ($scenario_details['pairInfo'] as $key_B=>$value_B)
					{
						foreach ($value_B['pairDetails'] as $key_C=>$value_C)
						{
							$app_volumes[] = $value_C['trgVol'];
							$process_server_id = $value_C['processServerid'];
						}
					}
					$pair_details = $this->getPairdetails($local_host, $process_server_id, $scenario_id);
				}
				else // NON-PRISM SOLUTION
				{
					$remote_host = explode(",",$src_host_id);
					$pair_details = $this->getPairdetails($local_host, $remote_host[0], $scenario_id);
				}				
				
				$flag = 1;
				$app_arr['Protected'] = '1';
				if($is_src_clustered)
				{
					$remote_host = explode(",",$src_cluster_id);
				}
				$src_discovery_info = $app_obj->_getSourceDiscoveryInfo($current_src_instance_id,$application,$app_type,$app_version , $scenario_id);
				$src_discovery_info = serialize($src_discovery_info);
				
				if($is_trg_clustered)
				{
					$key = 'INSTANCE_'.$trg_instance_name;
				}
			}
			else
			{
				$local_host = $host_id;
				$remote_host = explode(",",$trg_host_id);
				if($is_trg_clustered)
				{
					$remote_host = explode(",",$trg_cluster_id);
				}
				
				if ( $solution_type == "PRISM" ) // PRISM SOLUTION
				{
					$app_volumes = array();
	
					if( $scenario_protection_direction == 'forward')
					{
						$app_volume_array = explode(",",$scenario_details['applicationVolumes']);
					}
					elseif( $scenario_protection_direction == 'reverse' )
					{
						foreach ($scenario_details['pairInfo'] as $key_1=>$value_1)
						{
							foreach ($value_1['pairDetails'] as $key_1=>$value_1)
							{
								$app_volume_array[] = $value_1['srcVol'];
							}
						}
					}
					if ($app_volume_array)
					{
						foreach ($app_volume_array as $key_B=>$value_B)
						{
							$sql_app_vol = "SELECT
												DISTINCT
												srcDeviceName 
											FROM
												hostApplianceTargetLunMapping 
											WHERE
												hostId  ='$host_id' AND 
												sharedDeviceId = '$value_B'";
							
							$rs_sql_app_vol = $this->conn->sql_query($sql_app_vol);
							$i = 1;
							while($rs_row = $this->conn->sql_fetch_object($rs_sql_app_vol))
							{
								$device_name = $rs_row->srcDeviceName;
								
								if($i == 1)
								{
									$app_volumes[] = $device_name;
								}
								$i++;
							}				
						}
					}	
				
					foreach ($scenario_details['pairInfo'] as $key=>$value)
					{
						foreach ($value['pairDetails'] as $key_A=>$value_A)
						{
							$process_server_id = $value_A['processServerid'];
						}
					}
					$pair_details = $this->getPairdetails($trg_host_id, $process_server_id, $scenario_id);
				}
				else // NON-PRISM SOLUTION
				{
					$pair_details = $this->getPairdetails($trg_host_id, $host_id, $scenario_id);			
				}

				$flag = 0;
				$src_discovery_info = '0';
				
				if($is_src_clustered)
				{
					$key = 'INSTANCE_'.$src_instance_name;
				}
				
			}
			
			$rs = $this->conn->sql_query($sql);
			$pair_info = array();
			if (count($pair_details) == 0)
			{
				$sql = "select referenceId from applicationScenario where referenceId = '$scenario_id' and scenarioType = 'Rollback' and executionStatus = 116";
				$sql_result = $this->conn->sql_query($sql);
				$num_rows = $this->conn->sql_num_row($sql_result);
				if ($num_rows > 0)
				{
					$app_scenario_id_obj = $this->conn->sql_fetch_object($sql_result);
					$app_scenario_id = $app_scenario_id_obj->referenceId;
					$sql = "select scenarioDetails	from applicationScenario where scenarioId = '$app_scenario_id'";
					$sql_result = $this->conn->sql_query($sql);
					$num_rows = $this->conn->sql_num_row($sql_result);
					$logging_obj->my_error_handler("DEBUG"," _getPairInfo :Entered $sql, $num_rows \n");
					if ($num_rows > 0)
					{
						$app_scenario_det_obj = $this->conn->sql_fetch_object($sql_result);
						$scenario_details = $app_scenario_det_obj->scenarioDetails;
						$scenario_details = unserialize($scenario_details);
						$logging_obj->my_error_handler("DEBUG"," _getPairInfo :".print_r($scenario_details,TRUE)."\n");

						foreach ($scenario_details['pairInfo'] as $keyA=>$valueA)
						{
							foreach ($valueA['pairDetails'] as $keyB=>$valueB)
							{
								$temp_array = array();
								$dest_device = $valueB['trgVol'];
								$src_device = $valueB['srcVol'];
								$logging_obj->my_error_handler("DEBUG"," _getPairInfo :Entered temp_array \n");
								$temp_array['ConfiguredState'] = "1";
								$temp_array['PairState'] = "Diff";
								$temp_array['RemoteMountPoint'] = "$src_device";
								$temp_array['ResyncRequired'] = "No";
								$pair_info[$dest_device] = array(0,'1.0',$temp_array);
							}
						}
					}
				}
				$logging_obj->my_error_handler("DEBUG"," _getPairInfo , pair details zero with host_id : $host_id , $sql : ".print_r($pair_info,TRUE)."\n");
			}
	
			foreach($pair_details as $key1 => $valueI)
			{
				$logging_obj->my_error_handler("DEBUG"," _getPairInfo :Entered pair_details \n");
				$temp_array = array();
				$pair_state = "Diff";
				$is_resync = "No";
				
				if($valueI['shouldResync'])
				{
					$is_resync = "Yes";
				}
				
				if($valueI['isQuasiflag'] == 0)
				{
					$pair_state = "Resync1";
				}
				elseif($valueI['isQuasiflag'] == 1)
				{
					$pair_state = "Resync2";
				}
				
				$local_drive = $valueI['sourceDeviceName'];
				$remote_drive = $valueI['destinationDeviceName'];
				if($flag)
				{
					$local_drive = $valueI['destinationDeviceName'];
					$remote_drive = $valueI['sourceDeviceName'];
				}
				
				$temp_array['ConfiguredState'] = $valueI['executionState'];
				$temp_array['PairState'] = $pair_state;
				$temp_array['RemoteMountPoint'] = $remote_drive;
				$temp_array['ResyncRequired'] = $is_resync;
					
				$pair_info[$local_drive] = array(0,'1.0',$temp_array);
			}
			
			$get_instance_names = $app_obj->getInstanceNames($original_src_instance_id);
			$result["ID_".$scenario_id] = array(0,'1.0',$app_arr,$src_discovery_info,$app_volumes,$other_volumes,$pair_info,$get_instance_names);
			
		}
		return $result;
	}
	
	public function getPairdetails($dest_id='', $src_id='', $scenario_id='', $src_volume="")
	{
		$pair_info= array();
		if($src_volume)
		{
			$cond=" AND sourceDeviceName='".$this->conn->sql_escape_string($src_volume)."'";
		}	
		$sql = "SELECT
							sourceHostId,
							sourceDeviceName,
							destinationDeviceName,
							executionState,
							isQuasiflag,
							shouldResync
						FROM
							srcLogicalVolumeDestLogicalVolume
						WHERE
							destinationHostId='$dest_id' AND
							sourceHostId = '$src_id' AND
							scenarioId = '$scenario_id' $cond";
		$rs = $this->conn->sql_query($sql);

		while($data = $this->conn->sql_fetch_object($rs))
		{
			$should_resync = $data->shouldResync;
			$is_quasi_flag = $data->isQuasiflag;
			$local_drive = $data->sourceDeviceName;
			$remote_drive = $data->destinationDeviceName;
			$configured_state = $data->executionState;

			$pair_info[] = array('shouldResync' => $should_resync,
								 'isQuasiflag' => $is_quasi_flag,
								 'sourceDeviceName' => $local_drive,
								 'destinationDeviceName' => $remote_drive,
								 'executionState' => $configured_state);
		}
		return $pair_info;		
	}
	
	public function getProdDRServerDetails($instance_id)
	{
		$sql = "SELECT
						a.virtualServerName,
						a.ipAddress,
						b.applicationInstanceName
					FROM
						clusterNetworkNameInfo a,
						applicationHosts b
					WHERE
						b.applicationInstanceId IN ('$instance_id') AND
						a.applicationInstanceId = b.applicationInstanceId";
			$rs = $this->conn->sql_query($sql);
			$row_count = $this->conn->sql_num_row($rs);
		if( $row_count )
		{		
			while($data = $this->conn->sql_fetch_object($rs))
			{
				$vs = $data->virtualServerName;
				$vs_ip = $data->ipAddress;
				$instance_name = $data->applicationInstanceName;
			}
			$result = array('virtualServerName' => $vs,
							   'virtualServerIP' => $vs_ip,
							   'instanceName' => $instance_name);	
		}
		else
		{
			$result = array('virtualServerName' => '0',
							   'virtualServerIP' => '0',
							   'instanceName' => '0');	
		}
		return $result;
	}
	
	public function insert_script_policy($trg_id, $policy_type, $policyScheduledType, $policy_run_every, $scenario_id, $policy_exclude_from, $policy_exclude_to, $policy_parameters, $step, $planId=0)	
	{
		$policy_parameters = $this->conn->sql_escape_string($policy_parameters);
		$sql = "INSERT INTO 
						policy
						(
							hostId,
							policyType,
							policyScheduledType,
							policyRunEvery,
							scenarioId,
							policyExcludeFrom,
							policyExcludeTo,
							policyParameters,
							policyName,
							planId
						) 
					VALUES 
						(
							'$trg_id',
							'$policy_type',
							'$policyScheduledType',
							'$policy_run_every',
							'$scenario_id',
							'$policy_exclude_from',
							'$policy_exclude_to',
							'$policy_parameters',
							'$step',
							'$planId'
						)";
		$rs = $this->conn->sql_query($sql);
		$policy_id = $this->conn->sql_insert_id();
		$host_id = explode(",",$trg_id);
		
		if($policyScheduledType != 1)
		{
			foreach($host_id as $key => $host_id_value)
			{
				$sql = "INSERT INTO
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
									'$host_id_value'
								)";
				$rs = $this->conn->sql_query($sql);
			}
		}	
		return $policy_id;
	}
	
	public function get_policy_status($policy_id)
	{
		$result = array();
		$sql = "SELECT
					policyState,
					executionLog
				FROM
					policyInstance
				WHERE
					policyId = '$policy_id'";
		$rs = $this->conn->sql_query($sql);
		while($row = $this->conn->sql_fetch_object($rs))
		{
			$result[] = array('policyState'=>$row->policyState,'executionLog'=>$row->executionLog);
		}
		return $result;
	}
	
	public function _getRescanScriptPolicy($data,$host_id)
	{
		global $logging_obj;
		$result = array();
		$result[] = 0;
		$result[] = '1.0';
		
        $policyParameters = unserialize($data->policyParameters);
		$is_linux = $this->common_functions_obj->is_host_linux($host_id);	       
		$script_path = $this->conn->sql_get_value("hosts","vxAgentPath","id='".$host_id."'");
		
		if($policyParameters['option'])
		{	            
			if(!$is_linux) 
			{
				$script_path = "\"$script_path\\vacp.exe\"";
				$script = preg_replace("/vacp.exe/",$script_path,$policyParameters['option']);
			}    
			else 
			{
				$script = $script_path."/bin/".$policyParameters['option'];            
			}	
            $policyParameters = array('ScriptCmd'=>$script);
		}
		else if ($data->planId)
		{
			if(!$is_linux) 
			{
				$script = "\"$script_path\\vacp.exe\" -systemlevel";
			}    
			else 
			{
				$script = $script_path."/bin/vacp -systemlevel";            
			}		
			$policyParameters = array('ScriptCmd'=>$script);			
		}
	   
						
		$result[] = array( 	'PolicyId'=>$data->policyId,
							'PolicyType'=>$data->policyName,
							'ScheduleInterval'=>$data->policyRunEvery,
							'ScheduleType'=>$data->policyScheduledType,
							'ExcludeFrom'=>$data->policyExcludeFrom,
							'ExcludeTo'=>$data->policyExcludeTo
						);
		/*
			struct ScriptPolicyDetails
			{
				std::string m_version ; // 1.0
				std::map<std::string, std::string> m_Propertires; // this array should have "ScriptCmd" key and command as value
				std::map<std::string, VSnapDetails> m_vSnapDetails; // can be empty array
			};

			struct VSnapDetails
			{
				std::string m_version ; // 1.0
				std::map<std::string, std::string> m_vsnapDetailsMap; // can be empty array
			};

		*/
		$policyData = array(0,'1.0',$policyParameters,array());
		$result[] = serialize($policyData);
		return $result;
	}
	
	public function _getV2AMigrationPolicy($data,$host_id)
	{
		global $logging_obj, $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
		$result = array();
		$plan_id = $data->planId;
		$scenario_id = "ID_".$data->scenarioId;
		$cond = '';
		$sql = "SELECT planId,planName,solutionType FROM applicationPlan WHERE planId='$plan_id'";
		$rs = $this->conn->sql_query($sql);
		$num_rows = $this->conn->sql_num_row($rs);
		if($num_rows)
		{
			$row = $this->conn->sql_fetch_object($rs);
			
			$result[] = 0;
			$result[] = '1.0';
            if($row->solutionType == $CLOUD_PLAN_AZURE_SOLUTION)
            {
                $application_name = ucfirst(strtolower($CLOUD_PLAN_AZURE_SOLUTION));
            }
            elseif($row->solutionType == $CLOUD_PLAN_VMWARE_SOLUTION)
            {
                $application_name = ucfirst(strtolower($CLOUD_PLAN_VMWARE_SOLUTION));
            }
			$policyParameters = $data->policyParameters;
			
			$result[] = array( 	'ApplicationName'=>$application_name,
								'ApplicationType'=>'CLOUD',
								'PolicyId'=>$data->policyId,
								'PlanId'=>$plan_id,
								'ScenarioId'=>$scenario_id,
								'PlanName'=>$row->planName,
								'PolicyType'=>$data->policyName,
								'ScheduleInterval'=>$data->policyRunEvery,
								'ScheduleType'=>$data->policyScheduledType,
								'ExcludeFrom'=>$data->policyExcludeFrom,
								'ExcludeTo'=>$data->policyExcludeTo
							);				
			$policyData = $policyParameters;
			$result[] = $policyData;
		}
		return $result;
	}
	
	public function _getRollbackPolicy($data,$host_id)
	{
		global $logging_obj, $app_obj, $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
		
		$logging_obj->my_error_handler("DEBUG"," _getRollbackPoliy came in with host_id : $host_id , data :: ".print_r($data,TRUE));
		
		$policy_parameters = $data->policyParameters;
		$policy_id = $data->policyId;
		$scheduled_type = $data->policyScheduledType;
		$plan_id = $data->planId;
        $vol_information = array();
        $policy_details = array();
        $policy_data = array();
        
		$policy_details['PolicyId'] = $policy_id;
        $policy_details['ApplicationType'] = 'CLOUD';
		$policy_details['PolicyType'] = "CloudRecovery";
		$policy_details['ScheduleType'] = $scheduled_type;
		$policy_details['ScheduleInterval'] = '0';
        
        $solution_type = $this->conn->sql_get_value("applicationPlan","solutionType","planId='$plan_id'");
        
		if($solution_type == 'AWS') 
        {
            $policy_details['ApplicationName'] = 'AWS';
            
            $os_flag = $this->conn->sql_get_value("hosts","osFlag","id='$host_id'"); 	 
            if($os_flag != 1) 	 
            { 	 
                 $policy_details['PostScript'] = '/usr/local/InMage/Vx/scripts/Cloud/AWS/aws_recover.sh'; 	 
            }
        }           
		elseif ($solution_type == $CLOUD_PLAN_AZURE_SOLUTION)
        {
            $policy_details['ApplicationName'] = ucfirst(strtolower($CLOUD_PLAN_AZURE_SOLUTION));
        }
        elseif($solution_type == $CLOUD_PLAN_VMWARE_SOLUTION)
        {
            $policy_details['ApplicationName'] = ucfirst(strtolower($CLOUD_PLAN_VMWARE_SOLUTION));
        }
        
        $sql = "SELECT scenarioDetails,referenceId,sourceId FROM applicationScenario WHERE planId='$plan_id' and targetId='$host_id' and executionStatus='116'";
        $app_sce_data = $this->conn->sql_exec($sql);
        $system_drive_disk_extents = "";
		foreach($app_sce_data as $app_data)
        {           
            $scenarioDetails = unserialize($app_data['scenarioDetails']);
            $source_host_id = $app_data['sourceId'];
            $protection_scenario_id = $app_data['referenceId'];
			$os_details = $this->conn->sql_query("SELECT osCaption, osFlag, SysVolPath, systemDriveDiskExtents FROM hosts WHERE id='$source_host_id'");
			foreach($os_details as $os_data)
			{
				$os_caption = $os_data["osCaption"];
				$sys_vol_path = $os_data["SysVolPath"];
				if($os_data["osFlag"] == 1)
				{
					$sys_vol_path = str_replace("Windows","",$sys_vol_path);
					$sys_vol_details = explode(":\\",$sys_vol_path);
					$system_volume = ($sys_vol_details[1]) ? $sys_vol_path : $sys_vol_details[0];
				}
				else
				{
					$system_volume = $this->conn->sql_get_value("logicalVolumes","deviceName","hostId='$source_host_id' AND systemVolume = '1'");
				}
                $system_drive_disk_extents = $os_data["systemDriveDiskExtents"];
			}
			
			$details[$source_host_id]['TagType'] = $scenarioDetails['host_Details']['tagType'];
            $details[$source_host_id]['TagName'] = $scenarioDetails['host_Details']['tagName'];
            $details[$source_host_id]['HostId'] = $source_host_id;
            $details[$source_host_id]['HostName'] = $scenarioDetails['host_Details']['sourceHostname'];
            $details[$source_host_id]['MachineType'] = 'PhysicalMachine';
            $details[$source_host_id]['Failback'] = 'no';            
            $details[$source_host_id]['SystemVolume'] = "$system_volume";
            $details[$source_host_id]['OperatingSystem'] = "$os_caption";
            
            $app_details = $this->conn->sql_get_value("applicationScenario","scenarioDetails","scenarioId='$protection_scenario_id' AND scenarioType='DR Protection'");
            $protection_app_details = unserialize($app_details);
           
            foreach($protection_app_details['pairInfo'] as $key => $pair_data)
            {
                $pair_details = $pair_data['pairDetails'];
                foreach($pair_details as $pair_key => $pair_src_dev_info)
                {
                    $srcVol = $pair_src_dev_info['srcVol'];
                    $trgVol = $pair_src_dev_info['trgVol'];
                    $vol_information[$source_host_id][$srcVol] = $trgVol;
                }
            }
			
			 //Only for windows
           if($os_data["osFlag"] == 1)
            {
                if ($system_drive_disk_extents != NULL)
                {
                    $system_drive_disk_extents = explode(":", $system_drive_disk_extents);
					$src_disk_signature = $system_drive_disk_extents[0];
                    #$target_scsi_id = $this->conn->sql_get_value("srcLogicalVolumeDestLogicalVolume","destinationDeviceName","SourceHostId='$source_host_id' AND sourceDeviceName = '$src_disk_signature'");
					$target_scsi_id = $vol_information[$source_host_id][$src_disk_signature];
					$var_str = $target_scsi_id.":".$system_drive_disk_extents[1].":".$system_drive_disk_extents[2];
					$var_str = substr($var_str, 0, -1);
                    $details[$source_host_id]['boot_disk_drive_extent'] = $var_str;
					
					#Simulate failure purpose sending it to zero. $details[$source_host_id]['boot_disk_drive_extent'] = "0";
					
                }
            }
			
			if(is_array($protection_app_details['diskmapping'])) 
            {
				foreach($protection_app_details['diskmapping'] as $src_lun_id => $tgt_signature)
				{
					$disk_signature[$source_host_id][$src_lun_id] = "$tgt_signature"; 
				}
			}
			else
            {
				$disk_signature = array();
			}
        }
        
        $policy_data[] = 0;
        $policy_data[] ="1.0";
        $policy_data[] = $details;
        $policy_data[] = $vol_information;
        $policy_data[] = $disk_signature;
        
		$result = array(0,'1.0',$policy_details,serialize($policy_data));
		$logging_obj->my_error_handler("INFO"," _getRollbackPoliy Result :: ".print_r($result,TRUE));
		return $result;
	}
	
	public function getCloudPrepareTarget($plan_id,$policy_id,$host_id,$scenarioId)
	{
		global $APPLICATION_PLAN_STATUS,$SRC_MT_DSK_MPPNG_STUS,$MBR_PATH,$PREPARE_TARGET_PENDING,$CLOUD_PLAN_AZURE_SOLUTION,$CLOUD_PLAN_VMWARE_SOLUTION;
		global $logging_obj;
		global $REPLICATION_ROOT,$SLASH;
		$result = array();
		
		//Check if the plan is in Prepare Target Pending
		//Get the scenario Id whose status is PrepareTarget Pending
		//Get the required information from srcMtDiskMapping
		
		/*
			ModifyProtection API Changes
			- Get Plan Details based on the scenario Status
		*/
		$cond = '';
		$sql = "SELECT planId,planName,solutionType FROM applicationPlan WHERE applicationPlanStatus='$APPLICATION_PLAN_STATUS[9]' AND planId='$plan_id'";
		if($scenarioId!=0) {
			$cond = " AND a.scenarioId='$scenarioId'";
			$sql = "SELECT a.planId AS planId,a.planName AS planName,a.solutionType AS solutionType FROM applicationPlan a,applicationScenario b WHERE a.planId='$plan_id' AND a.planId=b.planId AND b.scenarioId='$scenarioId' AND b.executionStatus='$PREPARE_TARGET_PENDING'";
		}
		$rs = $this->conn->sql_query($sql);
		$num_rows = $this->conn->sql_num_row($rs);
		if($num_rows)
		{
			$row = $this->conn->sql_fetch_object($rs);
			$result[] = 0;
			$result[] = '1.0';
			
			$tmp_result = array();
            if ($row->solutionType == 'AWS')
            {
                $application_name = "AWS";
            }
            elseif($row->solutionType == $CLOUD_PLAN_AZURE_SOLUTION)
            {
                $application_name = ucfirst(strtolower($CLOUD_PLAN_AZURE_SOLUTION));
            }
            elseif($row->solutionType == $CLOUD_PLAN_VMWARE_SOLUTION)
            {
                $application_name = ucfirst(strtolower($CLOUD_PLAN_VMWARE_SOLUTION));
            }
			
			$tmp_result['ApplicationName'] = $application_name;
			$tmp_result['ApplicationType'] = 'CLOUD';
			$tmp_result['PlanId'] = $plan_id;
			$tmp_result['PlanName'] = $row->planName;
			
			$sql_policy = "SELECT * FROM policy WHERE policyId='$policy_id'";
			$rs_policy = $this->conn->sql_query($sql_policy);
			while($row_policy = $this->conn->sql_fetch_object($rs_policy))
			{
				$tmp_result['PolicyId'] = $row_policy->policyId;
				$tmp_result['PolicyType'] = $row_policy->policyName;
				$tmp_result['ScheduleInterval'] = $row_policy->policyRunEvery;
				$tmp_result['ScheduleType'] = $row_policy->policyScheduledType;
				$tmp_result['ExcludeFrom'] = $row_policy->policyExcludeFrom;
				$tmp_result['ExcludeTo'] = $row_policy->policyExcludeTo;
			}
			$result[] = $tmp_result;
			
			$sql1 = "SELECT 
						a.srcHostId AS srcHostId,
						a.srcDisk AS srcDisk,
						a.trgDisk AS trgDisk,
						a.trgLunId AS trgLunId,
						b.name AS name,
						b.latestMBRFileName AS latestMBRFileName,
						b.osType AS osType
					FROM
						srcMtDiskMapping a,hosts b
					WHERE
						a.planId = '$plan_id' AND
						a.status = '$SRC_MT_DSK_MPPNG_STUS[0]' AND
						a.trgHostId = '$host_id' AND
						a.srcHostId = b.id $cond";
			$rs1 = $this->conn->sql_query($sql1);
			$host_info = array();
			while($row1 = $this->conn->sql_fetch_object($rs1))
			{
				$host_info[$row1->srcHostId][0] = 0;
				$host_info[$row1->srcHostId][1] = '1.0';
				$host_info[$row1->srcHostId][2]['HostName'] = $row1->name;
				$host_info[$row1->srcHostId][2]['OSName'] = 'test';
				#$host_info[$row1->srcHostId][2]['MBRFilePath'] = $MBR_PATH.$row1->srcHostId.'\\'.$row1->latestMBRFileName;
				#$host_info[$row1->srcHostId][2]['MBRFilePath'] = 'SourceMBR\\'.$row1->srcHostId.'\\'.$row1->latestMBRFileName;
				#$host_info[$row1->srcHostId][2]['MBRFilePath'] = 'SourceMBR/'.$row1->srcHostId.'/'.$row1->latestMBRFileName;
                if($row->solutionType != $CLOUD_PLAN_VMWARE_SOLUTION)
                {
                    $host_info[$row1->srcHostId][2]['MBRFilePath'] = $REPLICATION_ROOT.$SLASH.'admin'.$SLASH.'web'.$SLASH.'SourceMBR'.$SLASH.$row1->srcHostId.$SLASH.$row1->latestMBRFileName;
                }
				$host_info[$row1->srcHostId][2]['HostID'] = $row1->srcHostId;
				$host_info[$row1->srcHostId][3][$row1->srcDisk] = (string)$row1->trgLunId;
			}
			$policy_data = array(0,'1.0',array(),$host_info); 
			$result[] = serialize($policy_data);
		}
		$logging_obj->my_error_handler("INFO","getCloudPrepareTarget::".print_r($result,true));
		return $result;
	}
	
	public function updatePrepareTargetforCloud($host_id,$policy_id,$status,$data,$plan_id,$scenario_id=0)
	{
		global $PREPARE_TARGET_PENDING,$PREPARE_TARGET_DONE,$SRC_MT_DSK_MPPNG_STUS,$APPLICATION_PLAN_STATUS,$PROTECTION_FAILED_ERROR_CODE, $PROTECTION_FAILED_ERROR_CODE_NAME;
		global $logging_obj,$WEB_CLI_PATH,$CLOUD_PLAN_VMWARE_SOLUTION, $commonfunctions_obj;
		
		$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud::$host_id,$policy_id,$status,$plan_id,".print_r($data,true));
		$esc_log = '';
		
		if($status == 1) {
			$data = unserialize($data);
			if(is_array($data)) {
				$trg_vol_arr = array();
				$cnd = '';
				if($scenario_id != 0) {
					$cnd = " AND scenarioId = '$scenario_id'";
				}
				$sql = "SELECT
							scenarioId,
							scenarioDetails,
							sourceId,
							sourceVolumes,
							targetVolumes,
                            applicationType 
						FROM
							applicationScenario
						WHERE
							planId='$plan_id' AND
							executionStatus = '$PREPARE_TARGET_PENDING' AND 
							targetId = '$host_id' $cnd";
				$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud::$sql");
				$rs = $this->conn->sql_query($sql);
				while($row = $this->conn->sql_fetch_object($rs)) 
                {
					$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud:: SCENARIO : $scenario_details");
					$scenario_details = unserialize($row->scenarioDetails);
					$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud:: UNSERIALIZE : ".print_r($scenario_details,true));
					$scenario_id = $row->scenarioId;
					$src_host_id = $row->sourceId;
                    $application_type = $row->applicationType;
					$is_linux = false;
					
					$src_ip = $this->conn->sql_get_value("hosts", "ipaddress", "id = ?", array($src_host_id));
					
					if($this->common_functions_obj->is_host_linux($src_host_id)) {
						$is_linux = true;
					}
					
					$pair_details = $data[2][$src_host_id];
					$src_vol_arr = array();
					$trg_vol_arr = array();
					if($row->sourceVolumes) {
						$src_vol_arr = explode(",",$row->sourceVolumes);
					}
					
					if($row->targetVolumes) {
						$trg_vol_arr = explode(",",$row->targetVolumes);
					}
					$count = 1;
					$src_vol_exists = 1;
					
					foreach($pair_details as $srcVol=>$trgVol) {
					$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud:: Pair Details entered srcVol : $srcVol, trgVol : $trgVol\n");
						if(!$is_linux) 
						{
							$srcVol = rtrim($srcVol,'\\');
						}
						$trgVol = rtrim($trgVol,'\\');
						
						$srcVol_esc = $this->conn->sql_escape_string($srcVol);
						$trgVol_esc = $this->conn->sql_escape_string($trgVol);
						$src_vol_arr[] = $srcVol_esc;
						$trg_vol_arr[] = $trgVol_esc;
						
						$row_capacity = $this->conn->sql_get_value("logicalVolumes","capacity", "hostId = ? AND deviceName = ?", array($src_host_id, $srcVol));
						if(!$row_capacity) 
						{
							$src_vol_exists = 0;
							$src_vol_name =  $srcVol;
						}	
						
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['srcVol'] = $srcVol;
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['trgVol'] = $trgVol;
						
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['srcCapacity'] = $row_capacity;
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['processServerid'] = $scenario_details['globalOption']['processServerid'];
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['srcNATIpflag'] = $scenario_details['globalOption']['srcNATIPflag'];
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['trgNATIPflag'] = $scenario_details['globalOption']['trgNATIPflag'];
						if(!$is_linux) {
							$tmp_ret_path = $scenario_details['pairInfo'][$src_host_id.'='.$host_id]['retentionOptions']['retentionPathVolume'];
							$tmp_ret_path = trim($tmp_ret_path);
							if(strlen($tmp_ret_path)>1) {
								$retention_path = $scenario_details['pairInfo'][$src_host_id.'='.$host_id]['retentionOptions']['retentionPathVolume'].'\\vol'.$count++;
							} else {
								$retention_path = $scenario_details['pairInfo'][$src_host_id.'='.$host_id]['retentionOptions']['retentionPathVolume'].':\\vol'.$count++;
							}
						}else {
							$retention_path = $scenario_details['pairInfo'][$src_host_id.'='.$host_id]['retentionOptions']['retentionPathVolume'].'//vol'.$count++;
						}
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['retentionPath'] = $retention_path;
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['retentionPathVolume'] = $scenario_details['pairInfo'][$src_host_id.'='.$host_id]['retentionOptions']['retentionPathVolume'];
						$scenario_details['pairInfo'][$src_host_id.'='.$host_id]['pairDetails'][$srcVol.'='.$trgVol]['logpath'] = '';
					}
					
					// When the requested source volume does not exist update prepare target failed.
					if(!$src_vol_exists)
					{
						$status = 2;
						$data = serialize(array("0" => "0", "1" => "1.0", "2" => array("ErrorCode" => "EA0526"), "3" => array(), "4" => array("PlaceHolder" => array("2" => array("VolumeList" => $src_vol_name, "SourceIP" => $src_ip, "ErrorCode" => "EA0526")))));
						$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud:: src_vol_exists --> $src_vol_exists \n src_vol_name ::$src_vol_name \n data::$data \n");
					}
					else
					{
                        $vol_arr_str = implode("','",$src_vol_arr);
						if(!$is_linux) 
                        {
							if(count($data[3][$src_host_id])) 
                            {
								$device_list = array();
								foreach($data[3][$src_host_id] as $k=>$v) 
                                {
									if(!$is_linux) 
                                    {
										$scenario_details['diskmapping'][$k] = $v;
                                        $device = '\\\\.\PHYSICALDRIVE'.$k;
										$device_list[] = $this->conn->sql_escape_string($device);
									} 
                                    else 
                                    {
										$device_list[] = $this->conn->sql_escape_string($k);
									}
								}
								
								$vol_arr_str = implode("','",$device_list);
							}
						}
                        
                        $update_map = "UPDATE srcMtDiskMapping SET status='$SRC_MT_DSK_MPPNG_STUS[1]' WHERE srcHostId='$src_host_id' AND trgHostId='$host_id' AND status='$SRC_MT_DSK_MPPNG_STUS[0]' AND srcDisk IN ('$vol_arr_str')";                            
                        $result_map = $this->conn->sql_query($update_map);
                        
						$trg_vol_arr_str = implode(",",$trg_vol_arr);
						$scenario_details = serialize($scenario_details);
						$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud:: scenario_details --> $scenario_details");
						$scenario_details = $this->conn->sql_escape_string($scenario_details);
						
						$update = "UPDATE applicationScenario SET
										scenarioDetails='$scenario_details',executionStatus='$PREPARE_TARGET_DONE',targetVolumes='$trg_vol_arr_str' 
								   WHERE scenarioId='$scenario_id'";
						$result_set = $this->conn->sql_query($update);
					}
				}
			}else {
				$logging_obj->my_error_handler("INFO","Failed to unserialize");
			}
		}
		if($status == 2) $esc_log = $data;
		
		
		$update = "UPDATE policyInstance SET policyState='$status',lastUpdatedTime=now() WHERE policyId='$policy_id' AND policyState IN ('3','4')";
		$result_set = $this->conn->sql_query($update);
		
		//update log if available
		if($esc_log) 
		{
			$update_log = "UPDATE policyInstance SET executionLog = ? WHERE policyId = ?";
			$rs_log = $this->conn->sql_query($update_log, array($esc_log, $policy_id));
			$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud::$update_log");
		}
		$policy_duration = 0;
		$policy_data = $this->conn->sql_query("SELECT policyTimestamp, now() as currentTime, (UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(policyTimestamp)) as policyDuration FROM policy WHERE policyId = ? AND policyType = ?", array($policy_id, 5));
		foreach($policy_data as $policy_info)
		{
			$policy_start_time = $policy_info["policyTimestamp"];
			$policy_end_time = $policy_info["currentTime"];
			$policy_duration = $policy_info["policyDuration"];
		}
		
		$mds_log = "";
		if($status == 2 || ($policy_duration >= 7200))
		{
			$mds_data_array = array();
			$this->mds_obj = new MDSErrorLogging();
			$this->mds_error_obj  = new MDSErrors();
			
			$log_file = $data;
			$data = unserialize($data);
			$log_file_name = $WEB_CLI_PATH . $data[2]['LogFile'];
			
			$mds_data_array["jobId"] = $policy_id;
			$mds_data_array["jobType"] = "Protection";
			$mds_data_array["eventName"] = "Master Target";
			$mds_data_array["errorType"] = "ERROR";
			$dependent_log_string = $this->mds_error_obj->GetErrorString("Protection", array($scenario_id));
			$mds_data_array["errorString"] = $dependent_log_string;
			if (!file_exists($log_file_name)) 
			{
				$mds_data_array["errorString"] .= "Prepare Target log file not found";
			}
            else
			{
                $file_handle = $commonfunctions_obj->file_open_handle($log_file_name);
                if(!$file_handle)
                {                    
                    $mds_log .= "Prepare Target log file could not be opened";
                }
                else
                {
                    $mds_log .= fread($file_handle, filesize($log_file_name));
                    fclose($file_handle);
                }
				if($dependent_log_string) 
				{
					$mds_data_array["errorString"] .= $mds_log;
					unlink($log_file_name);
				}
            }
			
			# In case prepare target policy took more than 2 hours update the policy status to MDS
			if($policy_duration >= 7200)
			{
				$policy_status = ($status == 1) ? "Success" : "Failed";
				$mds_data_array["errorString"] .= "Prepare Target policy started at $policy_start_time completed at $policy_end_time. Duration of prepare target policy is $policy_duration and the policy status is $policy_status with the log $log_file";
			}
			$this->mds_obj->saveMDSLogDetails($mds_data_array);		
		}
		
		$mds_log = "";
		if($status == 2)
		{
			$data = unserialize($data);
			$log_file_name = $WEB_CLI_PATH . $data[2]['LogFile'];
			if (file_exists($log_file_name)) 
			{
				$mds_data_array = array();
				$this->mds_obj = new MDSErrorLogging();
				$this->mds_error_obj  = new MDSErrors();
				
				$dependent_log_string = $this->mds_error_obj->GetErrorString("Protection", array($scenario_id));
				
                $file_handle = $commonfunctions_obj->file_open_handle($log_file_name);
                if(!$file_handle)
                {                    
                    $mds_log .= "Prepare Target log file could not be opened";
                }
                else
                {
                    $mds_log .= fread($file_handle, filesize($log_file_name));
                    fclose($file_handle);
                }
				if($dependent_log_string) $mds_log .= $dependent_log_string;
				
				$mds_data_array["jobId"] = $policy_id;
				$mds_data_array["jobType"] = "Protection";
				$mds_data_array["errorString"] = $mds_log;
				$mds_data_array["eventName"] = "Master Target";
				$mds_data_array["errorType"] = "ERROR";
				
				$this->mds_obj->saveMDSLogDetails($mds_data_array);
				unlink($log_file_name);
			}
		}
		
		$logging_obj->my_error_handler("INFO","updatePrepareTargetforCloud::$update");
		
	}
	
    public function getCloudConsistency($data,$host_id,$participated_hosts)
    {
        global $logging_obj, $app_obj, $LINUX_CONSISTENCY_COMMAND_OPTIONS_VERSION, $commonfunctions_obj;
        $result = array();
		$result[] = 0;
		$result[] = '1.0';
		$policy_details = array();
        $policy_parameters = unserialize($data->policyParameters);
        
		$logging_obj->my_error_handler("DEBUG"," _getRollbackPoliy came in with host_id : $host_id , data :: ".print_r($data,TRUE));
        
		$protection_scenario_id = $this->conn->sql_get_value("applicationScenario", "scenarioId","sourceId = '$host_id' AND planId = '".$data->planId."'");
            
        if($policy_parameters['option'])
        {
            $vacp_array = array("vacp.exe -systemlevel","vacp -systemlevel");            
            $cmd_options = str_replace($vacp_array,'',$policy_parameters['option']);
            if($cmd_options)
            {
                $policy_details['CmdOptions'] = $cmd_options;
            }    
        }
		$host_info = $commonfunctions_obj->get_host_info($host_id);
		$os_flag = $host_info['osFlag']; 	 
		if($os_flag != 1)
		{
			$agent_version = $os_flag = $host_info['compatibilityNo'];
			//Applicable for less than 9.47 version mobility agents.
			if ($agent_version < $LINUX_CONSISTENCY_COMMAND_OPTIONS_VERSION)
			{
				$install_dir = $this->conn->sql_get_value("hosts", "vxAgentPath", "id= '$host_id'");
				$policy_details['CmdOptions'] .= ' -prescript "' . $install_dir . '/scripts/vCon/AzureDiscovery.sh"';
			}
		}
        
        $policy_details['PolicyId'] = $data->policyId;
        $policy_details['ApplicationType'] = "CLOUD";
		$policy_details['PolicyType'] = "Consistency";
		$policy_details['ScenarioId'] = "ID_".$protection_scenario_id;
		$policy_details['ScheduleInterval'] = $data->policyRunEvery;
        $policy_details['ExcludeFrom'] = $data->policyExcludeFrom;
        $policy_details['ExcludeTo'] = $data->policyExcludeTo;
        $policy_details['ScheduleType'] = $data->policyScheduledType;   

		if (preg_match('/distributed/i', $data->policyParameters)) 
		{
			$parallel_consistency_status = $this->common_functions_obj->parallel_consistency_supported($participated_hosts);
			if ($parallel_consistency_status)
			{
				$policy_details['MultiVMConsistencyPolicyScheduleInterval'] = (string)$policy_parameters['run_every_interval'];
			}
		}        
        $result[] = $policy_details;
        $result[] = '0';
		return $result;
    }
	
	public function verifyPauseCompleted($data)
	{
		global $logging_obj, $PAUSE_PENDING;
		$src_id_arr = array();
		$pause_not_completed = 0;
		$logging_obj->my_error_handler("DEBUG"," _getRollbackPoliy came in with host_id : $host_id , data :: ".print_r($data,TRUE));
		
		/*
		2016-02-17 15:31:09 (UTC) CX :DEBUG: _getRollbackPoliy came in with host_id :  , data :: stdClass Object
(
    [policyId] => 64
    [policyType] => 48
    [policyParameters] => 0
    [policyRunEvery] => 0
    [policyScheduledType] => 2
    [policyExcludeFrom] => 0
    [policyExcludeTo] => 0
    [scenarioId] => 0
    [policyName] => Rollback
    [timestamp] => 1455722441
    [hostId] => A2FB12A8-C48A-DC4C-BBB33DCDAC6031E2
    [planId] => 19
)
*/
		$policy_id = $data->policyId;
		$plan_id = $data->planId;
		
		$scenario_data = $this->conn->sql_query("SELECT scenarioId, sourceId FROM applicationScenario WHERE planId = ? AND scenarioType = 'Rollback'",array($plan_id));
		
		foreach($scenario_data as $scenario_id => $source_id)
		{
			$src_id_arr[] = $source_id;
		}
		$src_id_arr = implode(",",$src_id_arr);
		
		$sql = "SELECT pairId, replication_status FROM srcLogicalVolumeDestLogicalVolume WHERE FIND_IN_SET(sourceHostId, ?)";
		$replication_details = $this->conn->sql_get_array($sql, "pairId","replication_status", array($src_id_arr));
		if (count($replication_details) > 0)
		{
			foreach($replication_details as $pair_id => $replication_status)
			{
				if($replication_status == $PAUSE_PENDING)
				$pause_not_completed = 1;
			}
		}
		$logging_obj->my_error_handler("DEBUG"," _getRollbackPoliy Paused status returned for Hosts: $src_id_arr , policy $policy_id, planId $plan_id , pause status $pause_not_completed");
		return $pause_not_completed;
	}
}
?>