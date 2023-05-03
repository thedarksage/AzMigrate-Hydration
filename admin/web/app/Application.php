<?php
class Application
{
	private $exchange_obj;
	private $sql_obj;
	private $fileserver_obj;
	private $prism_obj;
	private $oracle_obj;
	private $policy_obj;
	private $conn;
	
	public function __construct($discovery="")
	{
		global $conn_obj;
		$this->conn = $conn_obj;
		$this->exchange_obj = $this->_getApplicationObj('exchange');
		$this->sql_obj = $this->_getApplicationObj('sql');
		$this->fileserver_obj = $this->_getApplicationObj('fileserver');
		$this->prism_obj = $this->_getApplicationObj('prism');
		$this->oracle_obj = $this->_getApplicationObj('oracle');
		$this->db2_obj = $this->_getApplicationObj('db2');
	}
	
	private function _getApplicationObj($application)
	{
		switch($application)
		{
			case 'exchange':
				$obj = new ExchangeProtection($this);
				break;
			
			case 'sql':
				$obj = new SqlProtection($this);
				break;
				
			case 'fileserver':
				$obj = new FileServerProtection($this);
				break;
				
			case 'prism':
				$obj = new PrismConfigurator($this);
				break;	
			
			case 'oracle':
				$obj = new OracleDiscovery($this);
				break;
				
			case 'db2':
				$obj = new Db2Discovery($this);
				break;	
			
			default:
				$obj = 0;
		}
		return $obj;
	}
		
	/*
		Used to get application settings
	*/
	public function vxstubGetApplicationSettings($host_id,$policy_list,$passive_cx,$time_out)
	{
		global $logging_obj, $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
		$policy_obj = new Policy();
		$result = array();
		$final_result = array();
		$flag = true;
		$existing_policies = array();
		$logging_obj->my_error_handler("DEBUG","vxstubGetApplicationSettings \n host_id =  $host_id \n\n policy_list = ".print_r($policy_list,TRUE)."\n passive_cx = ".print_r($passive_cx,TRUE)."\n time_out = $time_out \n");
		
		$sql = "UPDATE policy SET policyTimestamp=now() WHERE hostId='$host_id' AND policyType=44 AND (UNIX_TIMESTAMP()- UNIX_TIMESTAMP(policyTimestamp)) >= 240";
		$rs = $this->conn->sql_query($sql);
		
		$new_arr = array();
		foreach($policy_list as $key => $val)
		{
			$new_arr[$key] = $val;
		}
		
		$policy_list = $new_arr;
			
		$sql = "SELECT
					policyId,
					policyType,
					policyParameters,
					policyRunEvery,
					policyScheduledType,
					policyExcludeFrom,
					policyExcludeTo,
					scenarioId,
					policyName,
					unix_timestamp(policyTimestamp) as timestamp,
					hostId,
					planId
				FROM
					policy
				WHERE
					hostId LIKE '%$host_id%' AND
					policyType >= '1'";
		
		$result_set = $this->conn->sql_query($sql);
		$logging_obj->my_error_handler("DEBUG"," vxstubGetApplicationSettings :: sql::$sql, sql_num_row :".$this->conn->sql_num_row($result_set));
		while($data = $this->conn->sql_fetch_object($result_set))
		{
			$existing_policies[] = $data->policyId;
			$scenario_exists = 1;
			$dns_failover = '1';
			$scenario_id = '';
			
			if($data->scenarioId)
			{
				$scenario_id = $data->scenarioId;
				$sql_chk = "SELECT
								scenarioId
							FROM
								applicationScenario
							WHERE
								scenarioId = '$scenario_id'";
				$result_set_chk = $this->conn->sql_query($sql_chk);
				$scenario_exists = $this->conn->sql_num_row($result_set_chk);
			}
			$flag = true;
			$discovery_src_readiness_info = array();
			$trg_readiness_info = array();
			$policy_id = $data->policyId;
			
			if(!count($policy_list))
			{
				$flag = true;
			}
			elseif(array_key_exists($data->policyId,$policy_list))
			{
				$logging_obj->my_error_handler("DEBUG"," vxstubGetApplicationSettings : policy_list111 ".$policy_list[$data->policyId]."timestamp111 ".$data->timestamp);
				
				$flag = false;
				if($policy_list[$policy_id] != $data->timestamp)
				{
					$flag = true;
				}
			}
		
			$logging_obj->my_error_handler("DEBUG"," vxstubGetApplicationSettings :: flag : $flag  :: scenario_exists : $scenario_exists :: policyId : ".$data->policyId." :: policyType : ".$data->policyType." :: policyName : ".$data->policyName." :: policyScheduledType : ".$data->policyScheduledType);
			
			if($flag && $scenario_exists)
			{
				$sel_solution_type = "SELECT
								solutionType,
								applicationType,
								targetArrayGuid,
								sourceArrayGuid,
								protectionDirection
							FROM
								applicationPlan ap,
								applicationScenario aps
							WHERE
								aps.scenarioId = '$scenario_id'
								AND
								aps.planId = ap.planId";
								
				$result_solution_type = $this->conn->sql_query($sel_solution_type);
				$num_rows_solution_type = $this->conn->sql_num_row($result_solution_type);
				$result_row = $this->conn->sql_fetch_object($result_solution_type);
				$solution_type = $result_row->solutionType;
				$application_type = $result_row->applicationType;
				if($result_row->protectionDirection == 'reverse')
				{
					$target_array_guid = $result_row->sourceArrayGuid;
				}
				else
				{
					$target_array_guid = $result_row->targetArrayGuid;
				}
				##
					 # Getting the solutionType explicitly as AZURE Plans consistency policy will not have scenario ID
				##
				if(($data->planId) && ($num_rows_solution_type == 0))
				{
					$plan_id = $data->planId;
					$solution_type = $this->conn->sql_get_value("applicationPlan","solutionType","planId = ?", array($plan_id));
				}

				$num_rows = $this->_getPolicyNumRows($data->policyId,$data->policyScheduledType,$data->policyType,$solution_type,$host_id);
			
				if (($solution_type == 'ARRAY') && 
					($data->policyType == 5 || $data->policyType == 42) && $data->policyScheduledType ==2)
				{
					
				}
				else
				if(($data->policyType == 1) || ($data->policyType == 2) || ($data->policyType == 4)
					|| ($data->policyType == 5) || ($data->policyType == 16))
				{
					if($data->policyType == 4 && $num_rows && ($solution_type == $CLOUD_PLAN_AZURE_SOLUTION || $solution_type == $CLOUD_PLAN_VMWARE_SOLUTION || $data->scenarioId == 0))
                    {
                        $final_result[] = $policy_obj->getCloudConsistency($data,$host_id,$data->hostId);
                    }
					elseif(($solution_type == $CLOUD_PLAN_AZURE_SOLUTION || $solution_type == $CLOUD_PLAN_VMWARE_SOLUTION || $solution_type == 'AWS')  && $num_rows)
                    {
						##
						# ModifyProtection API Changes
						# - PrepareTarget will be for each scenario instead of Master Target. Passing ScenarioId.
						##
						$prepare_target_data = $policy_obj->getCloudPrepareTarget($data->planId,$data->policyId,$host_id,$data->scenarioId);
						if (count($prepare_target_data))
						{
							$final_result[] = $prepare_target_data;
						}
					}
					elseif($data->policyScheduledType && $num_rows)
					{
						
					}
				}
				elseif(($data->policyType == 3) || ($data->policyType == 17))
				{
					$scenario_id = $data->scenarioId;
				}
				elseif($data->policyType == 6 || $data->policyType == 7 || $data->policyType == 30 || $data->policyType == 31 || $data->policyType == 32)
				{
					
				}
				elseif($data->policyType == 8 || $data->policyType == 36 || $data->policyType == 37 || $data->policyType == 38)
				{
					
				}
				elseif($data->policyType == 9 || $data->policyType == 10 || $data->policyType == 33 || $data->policyType == 34 || $data->policyType == 35)
				{
				
				}
				elseif($data->policyType == 11 || $data->policyType == 18)
				{
				
				}
				elseif($data->policyType == 12 || $data->policyType == 13)
				{
				
				}
				elseif($data->policyType == 14)
				{
					if($num_rows)
					{
						$final_result[] = $policy_obj->_getScriptPolicy($data);	
					}
				}
				elseif($data->policyType == 15)
				{
					if($num_rows)
					{
						$final_result[] = $policy_obj->_getScriptPolicy($data);	
					}
				}
				elseif($data->policyType == 19)
				{
					
				}
				elseif($data->policyType == 20)
				{
					
				}
				elseif($data->policyType == 22)
				{
					
				}
				elseif($data->policyType == 23)
				{
					
				}
				elseif($data->policyType == 24)
				{
					
				}
				elseif($data->policyType == 25)
				{
					
				}
				elseif($data->policyType == 41) // Run Every Policy For Mirror Monitoring
				{
					
				}
				elseif($data->policyType == 39)
				{
					
				}
				elseif($data->policyType == 42)
				{
				}
				elseif($data->policyType == 43)
				{
					
				}
				elseif($data->policyType == 44)
				{
					
				}
				elseif($data->policyType == 45)
				{
					if($num_rows)
					{
						$final_result[] = $policy_obj->_getRescanScriptPolicy($data,$host_id);
					}
				}
				elseif($data->policyType == 61 || $data->policyType == 62 || $data->policyType == 63 || $data->policyType == 64)
				{
					if($num_rows)
					{
						$final_result[] = $policy_obj->_getV2AMigrationPolicy($data,$host_id);
					}
				}
				else if($data->policyType == 48)
				{
					$pause_pending = $policy_obj->verifyPauseCompleted($data);
					if($num_rows && !$pause_pending)
					{
						$final_result[] = $policy_obj->_getRollbackPolicy($data, $host_id);
					}
				}
				else if($data->policyType == 49)
				{
					
				}
			}
		}
		
		/*
			code block for deleted policies
		*/
		if(count($policy_list))
		{
			$policy_list = array_keys($policy_list);
			$deleted_policies = array_diff($policy_list,$existing_policies);
			foreach($deleted_policies as $id)
			{
				$final_result[] = array(0,'1.0',array('PolicyId'=>"$id",'ScheduleType'=>'-1'),'');
			}
		}
	
		$pair_info = $policy_obj->_getProtectionSettings($host_id);
		
		$result = array(0,'1.0',$final_result,$pair_info,$time_out,$passive_cx);
		$logging_obj->my_error_handler("DEBUG"," vxstubGetApplicationSettings_result for $host_id :: ".print_r($result,TRUE)."\n ");
		return $result;
	}
	
	public function getEvsName($app_id)
	{
		$resultArr = array();

		$sql1 = " SELECT
					applicationVersion,
					applicationInstanceName
				FROM
					applicationHosts 
				WHERE 
					applicationInstanceId = '$app_id'";
		
		$result1 = $this->conn->sql_query($sql1);
		$row1	= $this->conn->sql_fetch_object($result1);		
		$applicationVersion 	= $row1->applicationVersion;
		$applicationInstanceName 	= $row1->applicationInstanceName;
		
		$resultArr			= array('applicationInstanceName'	=>	$applicationInstanceName,
									'applicationVersion' 	=>	$applicationVersion);
		return $resultArr;
	}
	
	public function add_slash($device_name)
	{
		$volume 	= $this->conn->sql_escape_string($device_name);
		return $volume;
	}
	
	public function getServerName($hostIds,$app_id)
    {  
        $host_id = explode(",",$hostIds);
		if(count($host_id)>1)
			$hostId = $host_id[0];
		else
			$hostId = $hostIds;
		$sql1 = "SELECT
                    DISTINCT
					clusterId
				FROM
                    clusters
                WHERE
					hostId = '$hostId'";
				
        $rs1=$this->conn->sql_query($sql1);
		$row1=$this->conn->sql_fetch_object($rs1);
		if($row1->clusterId && $app_id)
		{
			$sql2 = "SELECT
						clusterName
					FROM
						clusters
					WHERE
						clusterId='$row1->clusterId'";
					
			$rs2=$this->conn->sql_query($sql2);
			$row2=$this->conn->sql_fetch_object($rs2);
			$cluster_name = $row2->clusterName;
			
			$sql3 = "SELECT
						applicationInstanceName
					FROM
						applicationHosts
					WHERE
						applicationInstanceId='$app_id'";
			$rs3=$this->conn->sql_query($sql3);        
						
			$row3=$this->conn->sql_fetch_object($rs3);
			$app_name = $row3->applicationInstanceName;
			
			$name = $cluster_name.":".$app_name;
		}
		else
		{
			//incase of Oracle Protection $host_id is comma seperated 
			foreach($host_id as $hostId)
			{
				$sql = "SELECT
							name
						FROM
							hosts
						WHERE
							id='$hostId'";
				$rs=$this->conn->sql_query($sql);
				
				$row=$this->conn->sql_fetch_object($rs);
				$names[] = $row->name;
			}
			$name = implode(",",$names);
		}       
        return $name;   
    }
	
	public function getPlanName($plan_id)
	{		
		$this->__construct();
		$plan_name = $this->conn->sql_get_value('applicationPlan', 'planName',"planId='$plan_id'");
	
		return $plan_name;
	}
	
	public	function getAuditLogStr($scenario_data,$flag='',$app_type=0,$instance=0,$host_name=0)
	{
		global $logging_obj;
		$alerts = $sql_args = array();
		$alerts['summary'] = '';
		$alerts['message'] = '';
		
		$cond = "scenarioId = ?";
		if(is_array($scenario_data))
		{			
			$cond = "planId = ? AND FIND_IN_SET(sourceId, ?)";
			array_push($sql_args, $scenario_data["plan_id"], $scenario_data["host_id"]);
		}
		else $sql_args[] = $scenario_data;
		
		$sql = "SELECT
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
					$cond";
		$scenario_details = $this->conn->sql_query($sql, $sql_args);
		$logging_obj->my_error_handler("INFO","SOURCE VOLUMES  :: sql::$sql \n scenario_details".print_r($scenario_details,true)."\n sql_args".print_r($sql_args,true));
		if(count($scenario_details) > 0)
		{
			$src_hosts = array();
			foreach($scenario_details as $key => $details)
			{
				$planId 		= $details["planId"];
				$scenarioType 	= $details["scenarioType"];
				$sourceId 		= $details["sourceId"];
				$targetId 		= $details["targetId"];
				$srcInstanceId  = $details["srcInstanceId"];
				$trgInstanceId  = $details["trgInstanceId"];
				$protection_direction = $details["protectionDirection"];
				
				$planName = $this->conn->sql_get_value("applicationPlan","planName","planId = ?", array($planId));
				
				$src_hosts[] = $this->getServerName($sourceId,$srcInstanceId);
				$tgthost_name = $this->getServerName($targetId,$trgInstanceId);
			}
			$srchost_name = implode(",",$src_hosts);
			
			if($scenarioType == 'DR Protection')
			{
				$scenarioType = 'HA/DR Protection';
			}
			if($protection_direction == '')
			{
				$protection_direction = 'forward';
			}
			
			if($flag == 'DR-Exercise')
			{
				$alerts['message'] = "DR-Exercise: The HA/DR scenario for $srchost_name -> $tgthost_name ";
			}
			elseif($flag == 'prepareTarget')
			{
				$alerts['summary'] = "Prepare Target failed for $planName($srchost_name -> $tgthost_name) plan";
				$alerts['message'] = "Prepare Target failed for $planName($srchost_name -> $tgthost_name) plan";
			}
			elseif($flag == 'readinessCheck')
			{
				$alerts['summary'] = "Readiness Check failed for $planName($srchost_name -> $tgthost_name) plan";
				$alerts['message'] = "for $planName($srchost_name -> $tgthost_name) plan in $protection_direction direction";
			}
			elseif($flag == 'consistencyJob')
			{
				$alerts['summary'] = "Consistency job failed for $planName($srchost_name -> $tgthost_name) plan";
				$alerts['message'] = "Consistency job failed for $planName($srchost_name -> $tgthost_name) plan";
				$alerts['placeHolders'] = array("PlanName" => $planName, "SrcHostName" => $srchost_name, "DestHostName" => $tgthost_name);
			}
			elseif($flag == 'Failover' || $flag == 'Failback')
			{
				$alerts['summary'] = "$flag job failed for $planName($srchost_name -> $tgthost_name) plan";
				$alerts['message'] = "$flag job failed for $planName($srchost_name -> $tgthost_name) plan";
			}
			else
			{
				$alerts['message'] = "$scenarioType Scenario ($srchost_name -> $tgthost_name) for $planName plan.";
			}
		}
        
        if($flag == 'discoveryJob')
        {
            $alerts['summary'] = "Discovery failed for $app_type($instance) application in $host_name host";
            $alerts['message'] = "Discovery failed for $app_type($instance) application in $host_name host";
        }
        elseif($flag == 'discoveryJob1')
        {
            $logging_obj->my_error_handler("DEBUG","getAuditLogStr in loop");
            $alerts['summary'] = "No Discovery Information reported for $host_name host";
            $value = $alerts['summary'];
            $logging_obj->my_error_handler("DEBUG","getAuditLogStr in loop $value");
            $alerts['message'] = "Discovery Information is not found for $app_type($instance) application";
            $value1 = $alerts['message'];
            $logging_obj->my_error_handler("DEBUG","getAuditLogStr in loop $value1");
        }
		
		if($alerts['summary'] == '')
		{
			return $alerts['message'];
		}
		else
		{
			return $alerts;
		}
	}
	
	public function vxstubUpdatePrepareTarget($host_id,$policy_id,$status,$log)
	{
		global $VALIDATION_SUCCESS,$PREPARE_TARGET_PENDING,$PREPARE_TARGET_FAILED;
		global $REVERSE_REPLICATION_ACTIVE,$REPLICATION_PAIRS_ACTIVE;
		global $CREATING_REVERSE_REPLICATION_PAIRS,$CREATING_REPLICATION_PAIRS;
		global $logging_obj;
				
		$commonfunctions_obj = new CommonFunctions();
		$obj = new ProtectionPlan();
		$policy_obj = new Policy();
				

		$logging_obj->my_error_handler("INFO","vxstubUpdatePrepareTarget :: $host_id : $policy_id : $status : $log ");
		
		// $status = 1; i.e SUCCESS
		// $status = 4; i.e INPROGRESS
		// $status = 5; i.e SKIPPED
		// $status = 2; i.e FAILED
		
		$policy_details = $policy_obj->getPolicyInfo($policy_id);
		if($policy_details['planId']) {
			$policy_obj->updatePrepareTargetforCloud($host_id,$policy_id,$status,$log,$policy_details['planId'],$policy_details['scenarioId']);
		
		}
		else
		{
			$log = $this->conn->sql_escape_string($log);
			$scenario_id = $policy_details['scenarioId'];
							
			$scenario_details = $this->getScenarioParams($scenario_id);
			
			$solution_type = $scenario_details['solutionType'] ;
			 $forward_solution_type = $scenario_details['forwardSolutionType'];
			$execution_status = $scenario_details['executionStatus'];
			
			$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget :: scenario_details".print_r($scenario_details,TRUE));
			
			if($forward_solution_type == 'PRISM')
			{
				$flag = " AND hostId='$host_id'";
			}
			else
			{
				$flag = '';
			}
			
			if($solution_type == 'ARRAY')
			{
				/*This helps to keep the prepare target policy in pending state, after  plan activation and after HA fail over, if the prepared target policy get failed.*/
				if ($execution_status == $VALIDATION_SUCCESS)
				{
					if ($status == 2)
					{
						$status = 3;
					}
				}
				$sql = "UPDATE policyInstance SET policyState='$status', executionLog='$log', lastUpdatedTime = now() WHERE policyId='$policy_id' AND policyState = '4' $flag";
			}
			else
			{
				$sql = "UPDATE policyInstance SET policyState='$status', lastUpdatedTime = now() WHERE policyId='$policy_id' AND policyState = '4' $flag";
			}
			$rs = $this->conn->sql_query($sql);
			
			$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget :: sql : $sql ");
			
			$sql_check = "SELECT
							executionStatus
						FROM
							applicationScenario
						WHERE
							executionStatus = '$PREPARE_TARGET_PENDING' AND
							scenarioId = '$scenario_id'";
			$rs_check = $this->conn->sql_query($sql_check);
			
			$num_rows = $this->conn->sql_num_row($rs_check);
			$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget :: num_rows : $num_rows ");
			if($num_rows)
			{
				$app_type = $scenario_details['applicationType'];
			
				$update_status = 0;
				$temp_update_status = 0;
				if ($forward_solution_type == 'PRISM' AND $app_type == 'ORACLE') // ONLY ORACLE APPLICATION WITH PRISM SOLUTION
				{
					$sql_policy_ins_state = " SELECT
													policyInstanceId,
													policyState
											  FROM
													policyInstance
											  WHERE
													policyId = '$policy_id'";
					$rs_sql_policy_ins_state = $this->conn->sql_query($sql_policy_ins_state);
					
					$num_row_A = $this->conn->sql_num_row($rs_sql_policy_ins_state);
					
					while ( $row_sql_policy_ins_state = $this->conn->sql_fetch_object($rs_sql_policy_ins_state))
					{
						$temp_policyInstanceId = $row_sql_policy_ins_state->policyInstanceId;
						$temp_policyState = $row_sql_policy_ins_state->policyState;
						
						$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget :: temp_policyInstanceId : $temp_policyInstanceId :: temp_policyState : $temp_policyState ");
						
						if ( $temp_policyState == 2) 
						{
							$update_status = 12;
						}
						elseif ( $temp_policyState == 1)
						{
							$policy_ins_state_arr [$temp_policyInstanceId] = $temp_policyState;
						}	
						elseif ( $temp_policyState == 4 || $temp_policyState == 3)
						{
							$temp_update_status = 1;
						}
					}
					
					$count_policy_ins_state_arr =  count( $policy_ins_state_arr ) ;
					
					$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget :: temp_update_status : $temp_update_status , num_row_A : $num_row_A, count_policy_ins_state_arr : $count_policy_ins_state_arr , policy_ins_state_arr ::".print_r($policy_ins_state_arr,TRUE));
					
					if( $num_row_A == $count_policy_ins_state_arr )
					{
						$update_status = 11;
					}
				}
				else // ONLY HOST BASED SOLUTION
				{
					if($status == 1) 
					{
						$update_status = 11;
					}
					elseif($status == 2) 
					{
						$update_status = 12;
					}
				}
				
				$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget :: update_status : $update_status ");
				
				if($update_status == 11 )
				{
					if($scenario_details['targetIsClustered'])
					{
						if($scenario_details['direction'] == 'reverse')
						{
							if ($forward_solution_type == 'PRISM' AND $app_type == 'ORACLE')
							{
								// NO NEED TO UPDATE sourceId IF solution_type is PRISM and app_type is ORACLE
								$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget AA :: NO NEED TO UPDATE sourceId IF solution_type is PRISM and app_type is ORACLE ");
							}
							else
							{
								$app_instance_id = $scenario_details['targetAPPId'];
								
								$sql = "UPDATE
											applicationScenario
										SET
											sourceId = '$host_id'
										WHERE
											scenarioId = '$scenario_id'";
							}
						}
						else
						{
							if ($app_type == 'ORACLE')
							{
								foreach ($scenario_details['pairInfo'] as $key => $val )
								{
									$host_id = $val['targetId'];
								}
							}
							else
							{
								$app_instance_id = $scenario_details['targetAPPId'];
							}
							$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget host_id :: $host_id ");
							$sql = "UPDATE
										applicationScenario
									SET
										targetId = '$host_id'
									WHERE
										scenarioId = '$scenario_id'";
										
						}
						$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget :: sql3 : $sql ");
						$rs2 = $this->conn->sql_query($sql);
						
						
						$logging_obj->my_error_handler("INFO","vxstubUpdatePrepareTarget  app_type::$app_type, solution_type::$solution_type app_instance_id::$app_instance_id host_id::$host_id");
						if($app_type != 'ORACLE' && $solution_type != 'ARRAY')
						{
							$sql = "UPDATE applicationHosts SET hostId='$host_id' WHERE applicationInstanceId=$app_instance_id";
							$rs2 = $this->conn->sql_query($sql);
						}
					}
					$sql = "UPDATE
								applicationScenario
							SET
								executionStatus = '$VALIDATION_SUCCESS'
							WHERE
								executionStatus = '$PREPARE_TARGET_PENDING' AND
								scenarioId = '$scenario_id'";
					$logging_obj->my_error_handler("DEBUG","vxstubUpdatePrepareTarget :: sql4 : $sql ");
					$rs3 = $this->conn->sql_query($sql);
									
					$sql = "UPDATE
								policy
							SET
								policyScheduledType = '1'
							WHERE
								policyScheduledType = '0' AND
								scenarioId='$scenario_id' AND
								policyType IN ('2','3','4')";
					$rs4 = $this->conn->sql_query($sql);
					
					
					if ($solution_type == 'ARRAY')
					{
						$protection_plan_obj = new ProtectionPlan();
						#$return_result = $protection_plan_obj->insertPillarProtectionPlan($scenario_id);
					}
					else
					{
						$obj->setScenarioId($scenario_id);
						$return_result = $obj->insertProtectionPlan();
					}
								
					$sql = "SELECT
								scenarioId
							FROM
								applicationScenario
							WHERE
								referenceId = '$scenario_id' AND
								scenarioType = 'DR - Exercise'";
					$rs = $this->conn->sql_query($sql);
					$num_rows = $this->conn->sql_num_row($rs); 
					if($num_rows)
					{
						$row = $this->conn->sql_fetch_object($rs);
						$dr_exercise_id = $row->scenarioId;
						$sql = "SELECT
									executionStatus
								FROM
									applicationScenario
								WHERE
									scenarioId = '$dr_exercise_id'";
						$rs = $this->conn->sql_query($sql);
						$row = $this->conn->sql_fetch_object($rs);
						$state = $REPLICATION_PAIRS_ACTIVE;
						if($row->executionStatus == $CREATING_REVERSE_REPLICATION_PAIRS)
						{
							$state = $REVERSE_REPLICATION_ACTIVE;
						}
						
						$sql = "UPDATE
									applicationScenario
								SET
									executionStatus = '$state'
								WHERE
									scenarioId = '$dr_exercise_id'";
						$rs = $this->conn->sql_query($sql);
					}
				}
				elseif($update_status == 12 && !($temp_update_status) )
				{
					$sql = "UPDATE
								applicationScenario
							SET
								executionStatus = '$PREPARE_TARGET_FAILED'
							WHERE
								executionStatus = '$PREPARE_TARGET_PENDING' AND
								scenarioId = '$scenario_id'";
					$logging_obj->my_error_handler("DEBUG","SQL4::$sql");
					$rs3 = $this->conn->sql_query($sql);
									
					
					$error_id = "PREPARE_TARGET";
					$error_template_id = "PROTECTION_ALERT";
					$alert_details = $this->getAuditLogStr($scenario_id,'prepareTarget');
					
					$summary = $alert_details['summary'];
					$message = $alert_details['message'];
					if($message)
					{
						$this->sendTrapsforApplicationAlerts($error_template_id,$message);
						$commonfunctions_obj->add_error_message($error_id, $error_template_id, $summary, $message, $host_id);
					}
				}
			}
		}
	}
	
	public function vxstubUpdateConsistencyPolicy($host_id,$policy_id,$policy_instance_id,$status,$log, $policy_instance_id_primary)
	{
		global $logging_obj, $PROTECTION_FAILED_ERROR_CODE, $PROTECTION_FAILED_ERROR_CODE_NAME;
		$policy_obj = new Policy();
		$commonfunctions_obj = new CommonFunctions();
		$logging_obj->my_error_handler("INFO","vxstubUpdateConsistencyPolicy :: host_id : $host_id :: policy_id : $policy_id :: policy_instance_id : $policy_instance_id :: status : $status");
		
		$policy_id = $policy_id;
		$log = $this->conn->sql_escape_string($log);
		
		$sql = "UPDATE
					policyInstance
				SET
					policyState = '$status',
					lastUpdatedTime = now() 
				WHERE
					policyInstanceId = '$policy_instance_id_primary'";
		$rs = $this->conn->sql_query($sql);
		$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: sql : $sql");
		if($status == 2)
		{		
			$sel_sql = "SELECT  
							appS.sourceId ,
							appS.planId , 
							appP.solutionType,
							appS.scenarioId,
							appS.scenarioDetails,
							appS.protectionDirection
						FROM 
							applicationScenario appS ,  
							policy pol , 
							applicationPlan appP 
						WHERE 
							pol.scenarioId=appS.scenarioId AND 
							pol.policyId='$policy_id' AND 
							policyType=4 AND 
							appS.planId = appP.planId";  
			$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: sel_sql : $sel_sql  ");					
			$sql_rs = $this->conn->sql_query($sel_sql);	
			if ( $this->conn->sql_num_row($sql_rs) > 0 )
			{
				while($sql_row = $this->conn->sql_fetch_object($sql_rs))
				{
					$src_host_id = $sql_row->sourceId;
					$solution_type = $sql_row->solutionType;	
					$scenario_id = $sql_row->scenarioId;
					$scenario_details_str = $sql_row->scenarioDetails;
					$protection_direction = $sql_row->protectionDirection;
					
					$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: src_host_id : $src_host_id , solution_type : $solution_type , scenario_id : $scenario_id , protection_direction : $protection_direction ");	
					
					if($solution_type == 'PRISM')
					{
						$cdata_arr = $commonfunctions_obj->getArrayFromXML($scenario_details_str);
						$scenario_details = unserialize($cdata_arr['plan']['data']['value']);
					
						$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: scenario_details ::".print_r($scenario_details,TRUE));		
					
						$src_host_id_arr = explode(",",$src_host_id);
						$count_array = count($src_host_id_arr);
						$new_count = $count_array-1;
						
						$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: new_count : $new_count , count_array : $count_array , src_host_id_arr ::".print_r($src_host_id_arr,TRUE));	
						foreach($src_host_id_arr as $key=>$val)
						{
							$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: BEFORE MATCH host_id : $host_id :: val : $val :: key : $key ");
							if ( $host_id == $val )
							{
								if ( $key < $new_count )
								{
									$new_host_id = $src_host_id_arr[$key+1];
								}
								elseif ( $key == $new_count )
								{
									$new_host_id = $src_host_id_arr[0];
								}
								$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: new_host_id : $new_host_id  ");	
								
								if( $protection_direction == 'reverse')
								{									
									$pair_info_key = 'reversepairInfo';
								}
								else
								{	
									$pair_info_key = 'pairInfo';
								}
																
								foreach($scenario_details[$pair_info_key] as $key_A=>$val_A)
								{
									$lun_id_vendor_orig_array = array();
									$lun_id_volume_type_array = array();
									$lun_id_array = array();
									$lun_id_arr_bulk = array();
									$new_vacp_cmd = '';
									
									######### GET THE NEW vacp -v src_vol_list ###########
									foreach($val_A['pairDetails'] as $key_B=>$val_B)
									{
										$temp_lun_id = $val_B['srcVol'];
										$lun_id_vendor_orig_array[$temp_lun_id] = $val_B['prismSpecificInfo']['vendorOrigin']; 
										$lun_id_volume_type_array[$temp_lun_id] = $val_B['prismSpecificInfo']['volumeType'];
										$lun_id_arr_bulk[]=$temp_lun_id;
									}
									$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: lun_id_vendor_orig_array ::".print_r($lun_id_vendor_orig_array,TRUE));
									
									foreach($val_A['consistencyOptions'] as $key_C=>$val_C)
									{
										if( $scenario_details['planType'] != 'BULK')
										{
											$lun_id_array = explode(",",$scenario_details['applicationVolumes']);
										}
										else
										{
											$lun_id_array = $lun_id_arr_bulk;
										}
										$new_src_vol = $this->get_protected_volume_for_consistency($new_host_id, $lun_id_array, $lun_id_vendor_orig_array, $lun_id_volume_type_array);									
										$src_device_name_list = $new_src_vol;
										$old_vacp_cmd = $val_C['option'];
										
										$temp_vacp_cmd_array = explode('-v ',$old_vacp_cmd);	
										
										$old_temp_vacp_cmd = ltrim($temp_vacp_cmd_array[1]);
										
										$new_temp_vacp_cmd_arr = explode(" ",$old_temp_vacp_cmd);
										$new_temp_vacp_cmd_arr[0] = $src_device_name_list;
										
										$new_str = implode(" ",$new_temp_vacp_cmd_arr);
										$new_vacp_cmd = $temp_vacp_cmd_array[0].' -v '.$new_str;
										$scenario_details[$pair_info_key][$key_A]['consistencyOptions'][$key_C]['option'] = $new_vacp_cmd;
										
										$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy : old_temp_vacp_cmd ::  $old_temp_vacp_cmd, src_device_name_list ::  $src_device_name_list , new_str ::  $new_str, new_vacp_cmd ::  $new_vacp_cmd");
									}
									
									######### UPDATE THE NEW vacp -v src_vol_list into session ###########
								
									$policy_obj->updateAppScenario($scenario_details, $scenario_id);
								}
								$select_sql = "SELECT 
													policyParameters 
											   FROM 
													policy 
											   WHERE 
													scenarioId='$scenario_id' AND
													policyId='$policy_id'";
								$select_sql_rs = $this->conn->sql_query($select_sql);	
								if ( $this->conn->sql_num_row($select_sql_rs) > 0 )
								{
									$select_sql_row = $this->conn->sql_fetch_object($select_sql_rs);
									$policy_parameter_str = $select_sql_row->policyParameters;									
								}	
								$policy_parameter_arr = unserialize($policy_parameter_str);
								$policy_parameter_arr['ConsistencyCmd'] = $new_vacp_cmd;
								$new_policy_parameter_str = serialize($policy_parameter_arr);
								$new_policy_parameter_str = $this->conn->sql_escape_string($new_policy_parameter_str);
								
								$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: new_policy_parameter_str : $new_policy_parameter_str , policy_parameter_str::$policy_parameter_str");
								
								$update_sql = "UPDATE policy SET hostId='$new_host_id' , policyParameters='$new_policy_parameter_str', policyTimestamp = now() WHERE policyId='$policy_id'";
								
								$logging_obj->my_error_handler("DEBUG","vxstubUpdateConsistencyPolicy :: update_sql : $update_sql  ");								
								$update_rs = $this->conn->sql_query($update_sql);	
							}					
						}
					}				
				}
			}
		}		
	}	
	
	public function _getSourceDiscoveryInfo($src_instance_id,$application,$app_type,$app_version , $scenario_id =0 )
	{
		global $logging_obj;
		$logging_obj->my_error_handler("DEBUG"," _getSourceDiscoveryInfo ");
		$logging_obj->my_error_handler("DEBUG"," _getSourceDiscoveryInfo src_instance_id : $src_instance_id ");
		$logging_obj->my_error_handler("DEBUG"," _getSourceDiscoveryInfo application : $application ");
		$logging_obj->my_error_handler("DEBUG"," _getSourceDiscoveryInfo app_type : $app_type ");
		$logging_obj->my_error_handler("DEBUG"," _getSourceDiscoveryInfo app_version : $app_version ");
		$logging_obj->my_error_handler("DEBUG"," _getSourceDiscoveryInfo scenario_id : $scenario_id ");
		
		$result = array();
		if($application == 'Exchange')
		{
			$flag = 0;
			if($app_type == 'EXCHANGE2010') $flag = 1;
			$result = $this->_getExchangeSourceDiscoveryInfo($src_instance_id,$app_type,$flag);
		}
		else if($application == 'MSSQL')
		{
			$sql = "SELECT
						applicationInstanceId,
						applicationInstanceName
					FROM
						applicationHosts
					WHERE
						applicationInstanceId IN ('$src_instance_id')";
			$rs = $this->conn->sql_query($sql);
			$instance_arr = array();
			while($data = $this->conn->sql_fetch_object($rs))
			{
				$instance_name = $data->applicationInstanceName;
				$instance_id = $data->applicationInstanceId;
				
				$sql = "SELECT
							dbName,
							dbVolume,
							dbFiles
						FROM
							sqlDatabaseInfo
						WHERE
							applicationInstanceId = '$instance_id'";
				$result_set = $this->conn->sql_query($sql);
				$db_arr = array();
				$vol_array = array();
				$db_files = array();
				while($row = $this->conn->sql_fetch_object($result_set))
				{
					$db_files = unserialize($row->dbFiles);
					$db_arr[$row->dbName] = array(0,'1.0',$db_files);
					if(!in_array($row->dbVolume,$vol_array))
					{
						$vol_array[] = $row->dbVolume;
					}
				}
				
				$instance_arr[$instance_name] = array(0,'1.0',array('Version'=>$app_type),$db_arr,$vol_array);
			}
			$result = array(0,'1.0',$instance_arr);
		}
		else if($application == 'ORACLE' || $application == 'DB2')
		{
		}
		else
		{
			$result = $this->_getFileServerSourceDiscoveryInfo($src_instance_id,$app_version,$scenario_id);
		}
		return $result;
	}
	
	private function _getExchangeSourceDiscoveryInfo($src_instance_id,$app_type,$flag)
	{
		$result = array();
		if(!$flag)
		{
			$sql = "SELECT
						exchangeStorageGroupId,
						storageGroupName,
						logPath,
						logFolderPath,
						StorageGroupSystemPath,
						StorageGroupSystemVolume
					FROM
						exchangeStorageGroup
					WHERE
						applicationInstanceId IN ('$src_instance_id')";
			$rs = $this->conn->sql_query($sql);
			$storage_grp_arr = array();
			while($data = $this->conn->sql_fetch_object($rs))
			{
				$storage_group_id = $data->exchangeStorageGroupId;
				$storage_group_name = $data->storageGroupName;
				$log_vol = $data->logPath;
				$log_folder_path = $data->logFolderPath;
				$storage_system_path = $data->StorageGroupSystemPath;
				$storage_system_volume = $data->StorageGroupSystemVolume;
				$sql = "SELECT
							mailStoreName,
							dbPath,
							edbFilePath
						FROM
							exchangeMailStores
						WHERE
							exchangeStorageGroupId = '$storage_group_id'";
				$result_set = $this->conn->sql_query($sql);
				$mail_store_arr = array();
				$tmp_mail_store_arr = array();
				while($row = $this->conn->sql_fetch_object($result_set))
				{
					$mail_store_name = $row->mailStoreName;
					$db_vol = $row->dbPath;
					$edb_file_path = $row->edbFilePath;
					$tmp_mail_store_arr[$mail_store_name][$edb_file_path] = $db_vol;
				}
				
				foreach($tmp_mail_store_arr as $key=>$value)
				{
					$mail_store_arr[$key] = array(0,'1.0',array(),$value);
				}
				
				$storage_grp_arr[$storage_group_name] = 
						array(0,'1.0',array('LogLocation'=>$log_folder_path,'VolumeName'=>$log_vol,'SystemPath'=>$storage_system_path,'SystemVolume'=>$storage_system_volume),$mail_store_arr);
			}
			$result = array(0,'1.0',array('VersionName'=>$app_type),$storage_grp_arr);
		}
		else
		{
			$sql = "SELECT
						logpath,
						edbFilePath,
						logFolderPath,
						mailStoreName,
						guid,
						mountInfo,
						dbpath
					FROM
						Exchange2010Mailstore
					WHERE
						applicationInstanceId IN ('$src_instance_id')";
			$rs = $this->conn->sql_query($sql);
			$mail_store_info = array();
			while($data = $this->conn->sql_fetch_object($rs))
			{
				$log_vol = $data->logpath;
				$log_path = $data->logFolderPath;
				$edp_path = $data->edbFilePath;
				$db_volume = $data->dbpath;
				$mail_store_name = $data->mailStoreName;
				
				$guid = $data->guid;
				$mountInfo = $data->mountInfo;
				$mail_store_info[$mail_store_name] = array(0,'1.0',array('LogLocation'=>$log_path,'VolumeName'=>$log_vol,'GUID'=>$guid,'MountInfo'=>$mountInfo),array($edp_path=>$db_volume));
			}
			$result = array(0,'1.0',array('VersionName'=>$app_type),$mail_store_info);
		}
		return $result;
	}
	
	private function _getFileServerSourceDiscoveryInfo($src_instance_id,$app_version,$scenario_id)
	{
		$result = array();
		
		$sql = "SELECT protectionDirection, targetVolumes FROM applicationScenario WHERE scenarioId='$scenario_id'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$protection_direction = $row->protectionDirection;
		$targetVolumes = $row->targetVolumes;
		$protected_volumes = explode(",",$targetVolumes);
		
		$sql = "SELECT
					shareName,
					ShareInfo,
					SecurityInfo,
					MountPoint,
					ShareType,
					ShareUsers,
					AbsolutePath,
					SecurityPwd,
					ShareRemark
				FROM
					fileServerInfo
				WHERE
					applicationInstanceId IN ('$src_instance_id')";
		$rs = $this->conn->sql_query($sql);
		$share_info = array();
		$security_info = array();
		$share_type = array();
		$share_users = array();
		$absolute_path = array();
		$share_pwd = array();
		$share_remark = array();
		$mount_point = array();
		$volume_list = array();
		while($data = $this->conn->sql_fetch_object($rs))
		{
			if((in_array($data->MountPoint,$protected_volumes) && $protection_direction == 'reverse') || ($protection_direction == 'forward' || $protection_direction == ''))
			{
				$share_info[$data->shareName] = $data->ShareInfo;
				$security_info[$data->shareName] = $data->SecurityInfo;			
				$share_type[$data->shareName] = $data->ShareType;
				$share_users[$data->shareName] = $data->ShareUsers;
				$absolute_path[$data->shareName] = $data->AbsolutePath;
				$share_pwd[$data->shareName] = $data->SecurityPwd;
				$share_remark[$data->shareName] = $data->ShareRemark;			
				$mount_point[$data->shareName] = $data->MountPoint;
				
				if(!in_array($data->MountPoint,$volume_list))
				{
					$volume_list[] = $data->MountPoint;
				}
			}	
		}
		$result = array(0,'1.0',array('SrcRegistryVersion'=>$app_version),$share_info,$security_info,$volume_list,$share_type,$share_users,$absolute_path,$share_pwd,$share_remark,$mount_point);
		return $result;
	}
		
	public function get_version($app_type, $version, $format=0)
	{
        global $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
		$exchange_2003 = array('EXCHANGE2003','2003','Exchange Server 2003','Exchange_Server_2003','','Microsoft Exchange','Microsoft Exchange Server 2003');
		$exchange_2007 = array('EXCHANGE2007','2007','Exchange Server 2007','Exchange_Server_2007','','Microsoft Exchange Information Store','Microsoft Exchange Server 2007');
		$exchange_2010 = array('EXCHANGE2010','2010','Exchange Server 2010','Exchange_Server_2010','','Microsoft Exchange Information Store','Microsoft Exchange Server 2010');
		
		$sql_2000 = array('SQL','2000','SQL Server 2000','SQL_Server_2000','8.','SQL Server 2000','Microsoft SQL Server 2000');
		$sql_2005 = array('SQL2005','2005','SQL Server 2005','SQL_Server_2005','9.','SQL Server 2005','Microsoft SQL Server 2005');
		$sql_2008 = array('SQL2008','2008','SQL Server 2008','SQL_Server_2008','10.','SQL Server 2008','Microsoft SQL Server 2008');
		$sql_2012 = array('SQL2012','2012','SQL Server 2012','SQL_Server_2012','11.','SQL Server 2012','Microsoft SQL Server 2012');
		
		$file_server = array('FILESERVER','','File Server','File_Server','','','File Server');
		
		
		$app_version = 'N/A';
		if($app_type == 'Exchange')
		{
			$pattern1 = '/Version 6./i';
			$pattern2 = '/Version 8./i';     
			$pattern3 = '/Version 14./i';
			$matched1 = preg_match($pattern1, $version, $matches);
			$matched2 = preg_match($pattern2, $version, $matches);
			$matched3 = preg_match($pattern3, $version, $matches);
		
			if($matched1) $app_version = $exchange_2003[$format];
			elseif($matched2) $app_version = $exchange_2007[$format];
			elseif($matched3) $app_version = $exchange_2010[$format];
		}
		elseif($app_type == 'MSSQL')
		{	
			$pattern1 = '/^8./i';     
			$pattern2 = '/^9./i';
			$pattern3 = '/^10./i';
			$pattern4 = '/^11./i';
			
			$matched1 = preg_match($pattern1, $version, $matches);
			$matched2 = preg_match($pattern2, $version, $matches);
			$matched3 = preg_match($pattern3, $version, $matches);
			$matched4 = preg_match($pattern4, $version, $matches);
			
			if($matched1) $app_version = $sql_2000[$format];
			elseif($matched2) $app_version = $sql_2005[$format];
			elseif($matched3) $app_version = $sql_2008[$format];		
			elseif($matched4) $app_version = $sql_2012[$format];		
		}
		elseif($app_type == 'File Servers')
		{
			$app_version = $file_server[$format];
		}
		elseif($app_type == 'ORACLE')
		{
			$app_version = 'ORACLE';
		}
		elseif($app_type == 'DB2')
		{
			$app_version = 'DB2';
		}
        elseif(strtoupper($app_type) == $CLOUD_PLAN_AZURE_SOLUTION || strtoupper($app_type) == $CLOUD_PLAN_VMWARE_SOLUTION || $app_type == 'AWS')
		{
			$app_version = 'CLOUD';
		}
		else
		{
			$pattern1 = '/Version 6./i';
			$pattern2 = '/Version 8./i';     
			$pattern3 = '/Version 14./i';
			$pattern4 = '/8./i';     
			$pattern5 = '/9./i';
			$pattern6 = '/10./i';
			
			$matched1 = preg_match($pattern1, $version, $matches);
			$matched2 = preg_match($pattern2, $version, $matches);
			$matched3 = preg_match($pattern3, $version, $matches);
			$matched4 = preg_match($pattern4, $version, $matches);
			$matched5 = preg_match($pattern5, $version, $matches);
			$matched6 = preg_match($pattern6, $version, $matches);			
		
			if($matched1) $app_version = $exchange_2003[$format];
			elseif($matched2) $app_version = $exchange_2007[$format];
			elseif($matched3) $app_version = $exchange_2010[$format];	
			elseif($matched4) $app_version = $sql_2000[$format];
			elseif($matched5) $app_version = $sql_2005[$format];
			elseif($matched6) $app_version = $sql_2008[$format];				
		}		
		return $app_version;
	}
	
	public function getScenarioExecutionStatus($scenario_id)
	{
		$sql = "SELECT
					executionStatus
				FROM
					applicationScenario
				WHERE
					scenarioId='$scenario_id'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$execution_status = $row->executionStatus;
		return $execution_status;
	}

	public function getServerNameForBulk($hostIds,$solution_type='')
    {  
        $host_id_array = explode(",",$hostIds);
		$hostIds = implode("','",$host_id_array);
		
		$name ='';
		if($solution_type == "PRISM")
		{
			foreach($host_id_array as $id)
			{
				$sql = "SELECT
							name
						FROM
							hosts
						WHERE
							id='$id'";
				$rs=$this->conn->sql_query($sql);        
				$row=$this->conn->sql_fetch_object($rs);
				$name .= ",".$row->name;
			}
			$name = substr($name,1);
		}
		else
		{
			if( count($host_id_array) > 1) // CLUSTER_BULK
			{
				$sql1 = "SELECT
							DISTINCT
							clusterName,
							clusterId
						FROM
							clusters
						WHERE
							hostId IN ('$hostIds')";
					
				$rs1=$this->conn->sql_query($sql1);        
				$row1=$this->conn->sql_fetch_object($rs1);
				if($row1->clusterName) 
				{
					$clusterId = $row1->clusterId;
					$sql2 = "SELECT
								DISTINCT h.name as host_name
							FROM
								hosts h,
								clusters c
							WHERE
								c.hostId = h.id
							AND 
								c.clusterId = '$clusterId'";
						
					$rs2=$this->conn->sql_query($sql2);        
					$i = 0;
					while( $row2=$this->conn->sql_fetch_object($rs2))
					{
						$temp_host_name = $row2->host_name;
						
						if( $i == 0)
						{
							$host_name = $temp_host_name;
						}
						else
						{
							$host_name = $host_name.",".$temp_host_name;
						}
						$i++;
					}
					$name = $row1->clusterName."(".$host_name.")";
				}
			}
			else // NON_CLUSTER_BULK
			{
				$sql = "SELECT
							name
						FROM
							hosts
						WHERE
							id='$hostIds'";
						
				$rs=$this->conn->sql_query($sql);        
				$row=$this->conn->sql_fetch_object($rs);
				$name = $row->name;
			}
		}
        return $name;
    }	
	
	public function getTagDetails($policyId)
	{		
		global $logging_obj;
		$recovery_plan_obj = new RecoveryPlan();
		
		$sql = "SELECT scenarioId,policyType FROM policy WHERE policyId='$policyId'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$scenario_id = $row->scenarioId;
		$scenario_details = $recovery_plan_obj->getRecoveryScenarioDetails($scenario_id);
		
		if($row->policyType == 14)
		{
			$script_cmd = $scenario_details['pre_command_source'];
		}
		else
		{
			$script_cmd = $scenario_details['post_command_source'];
		}
		if($scenario_details[0][radrecdrv] == 1)
		{
			$drive_type = 'Virtual';
		}
		else
		{
			$drive_type = 'Physical';
		}
		if($scenario_details[0][chkVirtualRW] == 'on')
		{
			$read_write = 'ReadWrite';
		}
		else
		{
			$read_write = 'ReadOnly';
		}
		$protection_scenario_id = $this->conn->sql_get_value('applicationScenario', 'referenceId',"scenarioId='$scenario_id' and scenarioType='Data Validation and Backup'");
		
		$protection_scenario_details = $recovery_plan_obj->getSelectedProtectionScenarioDetails($protection_scenario_id);
		$params = $this->getScenarioParams($protection_scenario_id);
		
		if($params['srcIsClustered'] == 1)
		{
			$source_evs_name = $this->getEvsName($params['srcAppId']);
			$source_evs_name = $source_evs_name['applicationInstanceName'];
		}
		else
		{
			$source_evs_name = '';
		}
		if($params['targetIsClustered'] == 1)
		{
			$target_evs_name = $this->getEvsName($params['targetAPPId']);
			$target_evs_name = $target_evs_name['applicationInstanceName'];
		}
		else
		{
			$target_evs_name = '';
		}
			
		$target_server = explode(" ",$scenario_details['target_server']);
		$source_server = explode(" ",$scenario_details['source_server']);
		
		$pair_count = $scenario_details['pair_count'];
		$result[] = 0;
		$result[] = '1.0';
		$result[] = array("CreateConfFile" => '1',
							             "ScriptCmd" => $script_cmd,
										 "SourceVirtualServerName" => $source_evs_name,
										 "TargetServerName" => $target_server[0],
										 "TargetVirtualServerName" => $target_evs_name,
										 "VsnapDriveType" => $drive_type,
										 "SourceServerName" => $source_server[0]
						 );
		$rec_tag = '';
		$recovery_time = '';
		for($i=0;$i<$pair_count;$i++)
		{			
			if($scenario_details['execution_type'] == 'Scheduled')
			{
				$recovery_time = '';
				if($scenario_details['bookmark_prefix'])
				$rec_tag = $scenario_details['bookmark_prefix'];
				else
				$rec_tag = '';
				
			}
			else
			{
				if($scenario_details['hid_recovery_time'.($i+1)])
				{
					$str = $scenario_details['hid_recovery_time'.($i+1)];
					$str1 = explode(",",$str);
					$j = 0;
					foreach($str1 as $value)
					{
						if($j == 0 || $j == 1 || $j == 2)
						{
							if($j == 0)
							{
								$recovery_time = $value;
							}
							else
							{
								$recovery_time = $recovery_time.'/'.$value;
							}
						}
						else
						{
							if($j == 3)
							$recovery_time = $recovery_time.' '.$value;
							else
							$recovery_time = $recovery_time.':'.$value;
						}
						$j++;
							
					}					
				}	
				else
				{
					$recovery_time = '';
				}	
				
				if($scenario_details['tag_name'.($i+1)])
				{
					$rec_tag = $scenario_details['tag_name'.($i+1)];
					$recovery_time = '';
				}
				else
				{
					$rec_tag = '';
				}	
			}
			$vsnap_path = explode(",",$scenario_details[$i]['hidVolumes'.($i+1)]);
			if($scenario_details['target_volume'.($i+1)])
			{
				$result[3][$scenario_details['target_volume'.($i+1)]] = array("0" => 0,
			                                                              "1" => '1.0',
																		  "2" => array("RecoverTagName" => $rec_tag,
																						  "RecoveryTime" => $recovery_time,
																						  "VsnapFlag" => $read_write,
																						  "VsnapPath" => $vsnap_path[0])
																			 );
			}
			else
			{
				$result[3][$scenario_details['target_volume_other'.($i+1)]] = array("0" => 0,
			                                                              "1" => '1.0',
																		  "2" => array("RecoverTagName" => $rec_tag,
																						  "RecoveryTime" => $recovery_time,
																						  "VsnapFlag" => $read_write,
																						  "VsnapPath" => $vsnap_path[0])
																						  );
			}
		}
		return $result;							
	}
	
	public function vxstubUpdatePolicy($host_id,$policy_id,$status,$log)
	{
		global $logging_obj;
		$commonfunctions_obj = new CommonFunctions();
		$policy_obj = new Policy();
		$logging_obj->my_error_handler("INFO","vxstubUpdatePolicy---$policy_id,$status,$log, ");
		$policy_id = $policy_id;
		$log = $this->conn->sql_escape_string($log);
		
		$policy_details = $policy_obj->getPolicyInfo($policy_id);
		
		$policyType = $policy_details['policyType'];
		$scenarioId = $policy_details['scenarioId'];
				
		if($policyType == 12 && $status == 1)
		{
			$scenario_details = $this->getScenarioParams($scenarioId);
			
			$isVolumeResized = $scenario_details['isVolumeResized'];
			$executionStatus = $scenario_details['executionStatus'];
			$sourceVolumes = $scenario_details['sourceVolumes'];
			$sourceId = $scenario_details['sourceHostId'];
			
			$source_id_arr = explode(",",$sourceId);
			$sourceVolumes = str_replace(",","','",$sourceVolumes);
			
			if($isVolumeResized)
			{
				if($executionStatus != 90)
				{
					$sql1 = "UPDATE 
								applicationScenario
							SET
								currentStep = ''
							WHERE
								scenarioId='$scenarioId'";	
					
					$this->conn->sql_query($sql1);
				}
				if($executionStatus == 95 || $executionStatus == 90)
				{
					foreach($source_id_arr as $key5)
					{
						$sql2 = "UPDATE 
								volumeResizeHistory
							SET
								isValid = '0'
							WHERE
								hostId='$key5'
							AND
								deviceName IN('$sourceVolumes')";	
					
						$this->conn->sql_query($sql2);
					}
				}				
				$sql1 = "UPDATE 
							applicationScenario
						SET
							isVolumeResized = '0'
						WHERE
							scenarioId='$scenarioId'";	
				$this->conn->sql_query($sql1);
			}
		}
		$sql = "UPDATE
					policyInstance
				SET
					policyState='$status',
					lastUpdatedTime=now()
				WHERE
					policyId='$policy_id' AND
					policyState = '4'";
		$rs = $this->conn->sql_query($sql);		
	}
	
	public function sec2hms ($sec, $padHours = true) 
	{
		$hms = "";
		$hours = intval(intval($sec) / 3600);

		$hms .= ($padHours) 
			  ? str_pad($hours, 2, "0", STR_PAD_LEFT). ':'
			  : $hours. ':';
		 
		$minutes = intval(($sec / 60) % 60); 

		$hms .= str_pad($minutes, 2, "0", STR_PAD_LEFT). ':';

		$seconds = intval($sec % 60); 

		$hms .= str_pad($seconds, 2, "0", STR_PAD_LEFT);

		return $hms;	
	}	
	
	public function getScenarioSessionData($scenario_id)
	{
		$commonfunctions_obj = new CommonFunctions();
		$sql = "SELECT
				scenarioDetails
			FROM
				applicationScenario
			WHERE
				scenarioId='$scenario_id'";
		$rs = $this->conn->sql_query($sql);
		while($row = $this->conn->sql_fetch_object($rs))
		{
			$cdata = $row->scenarioDetails;
		}		
		$cdata_arr = $commonfunctions_obj->getArrayFromXML($cdata);
		$scenario_data = unserialize($cdata_arr['plan']['data']['value']);
		
		return $scenario_data;
	}
	
	private function _deleteReplicationPairsForPlan($plan_id,$reference_id)
	{
		global $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING,$STOP_PROTECTION_PENDING,$SOURCE_DELETE_PENDING;
		global $logging_obj,$commonfunctions_obj;
		$volume_obj = new VolumeProtection();
		$process_obj = new ProcessServer();
		$sql = "SELECT
					sourceHostId,
					sourceDeviceName,
					destinationHostId,
					destinationDeviceName,
					executionState,
					scenarioId,
					Phy_Lunid 
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					planId = '$plan_id'";
		$rs = $this->conn->sql_query($sql);
		$logging_obj->my_error_handler("DEBUG","_deleteReplicationPairsForPlan:: $sql");
		while($row = $this->conn->sql_fetch_object($rs))
		{
			$src_id = $row->sourceHostId;
			$src_dev = $row->sourceDeviceName;
			$dest_id = $row->destinationHostId;
			$dest_dev = $row->destinationDeviceName;
			$execution_state = $row->executionState;
			$scenario_id = $row->scenarioId;
			$lunId = $row->Phy_Lunid;
			
			$logging_obj->my_error_handler("DEBUG","_deleteReplicationPairsForPlan::src_id::$src_id, src_dev::$src_dev, dest_id::$dest_id, dest_dev::$dest_dev, execution_state::$execution_state, scenario_id::$scenario_id, lunId::$lunId");
			
			if(($execution_state == 1) || ($execution_state == 4))
			{
				$processServerId = $process_obj->getProcessServerId($src_id, $src_dev, $dest_id, $dest_dev);
				$get_sync_state = $volume_obj->get_qausi_flag($src_id, $src_dev, $dest_id, $dest_dev);
				$file_system_type = $volume_obj->get_file_system_type($src_id,$src_dev);
				
				$logging_obj->my_error_handler("DEBUG","_deleteReplicationPairsForPlan:: $file_system_type");
				
				if((isset($file_system_type)) && ($file_system_type !=''))
				{
					$unlockDrive ='10';
				}
				else
				{
					$unlockDrive ='00';
				}
				
				if(($processServerId == $dest_id) || ($get_sync_state == 0))
				{
					$rep_options = '00101010'.$unlockDrive;
				}
				else
				{
					$rep_options = '10101010'.$unlockDrive;
				}
				
				$is_ret_reuse = 0;
				if($scenario_id != $reference_id)
				{
					$is_ret_reuse = $commonfunctions_obj->is_retention_reuse($scenario_id);
				}
				if($is_ret_reuse == 1)
				{
					if(($processServerId == $dest_id) || ($get_sync_state == 0))
					{
						$rep_options = '0010000000';
					}
					else
					{
						$rep_options = '1010000000';
					}	
				}
				$prismDeletion = '';
				if((isset($lunId)) && ($lunId !=''))
				{
					// pairType 0=host,1=fabric,2=prism
					$pairType = $volume_obj->get_pair_type($lunId);
					if($pairType == 1 || $pairType == 2)
					{
						$prismDeletion =  ",lun_state= $STOP_PROTECTION_PENDING";
					}
				}
				if($volume_obj->is_array_lun($src_id, $source_dev,$dest_id, $dest_dev))
				{
					$nextState = $SOURCE_DELETE_PENDING;
				}
				else
				{
					$nextState = $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;
				}
				
				$src_dev = $this->conn->sql_escape_string($src_dev);
				$dest_dev = $this->conn->sql_escape_string($dest_dev);
				$sql = "UPDATE 
							srcLogicalVolumeDestLogicalVolume
						SET 
							replication_status = '$nextState',
							replicationCleanupOptions = '$rep_options',
							deleted = '1' $prismDeletion
							
						WHERE
							sourceHostId = '$src_id' AND
							sourceDeviceName = '$src_dev' AND
							destinationHostId ='$dest_id' AND
							destinationDeviceName = '$dest_dev' AND
							replication_status = '0'";
				$rs1 = $this->conn->sql_query($sql);
			}
			else
			{
				$src_dev = $this->conn->sql_escape_string($src_dev);
				$dest_dev = $this->conn->sql_escape_string($dest_dev);
				$sql = "DELETE FROM
							srcLogicalVolumeDestLogicalVolume
						WHERE
							sourceHostId = '$src_id' AND
							sourceDeviceName = '$src_dev' AND
							destinationHostId ='$dest_id' AND
							destinationDeviceName = '$dest_dev'";
				$rs1 = $this->conn->sql_query($sql);
			}
			$logging_obj->my_error_handler("DEBUG","_deleteReplicationPairsForPlan:: $sql");
		}
	}
	
	public function getScenarioParams($scenario_id,$direction=0)
	{
        global $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
		$result = array();
		$commonfunctions_obj = new CommonFunctions();
		$sql = "SELECT
					*
				FROM
					applicationScenario
				WHERE
					scenarioId = '$scenario_id'";
		$rs = $this->conn->sql_query($sql);
		$rs = $this->conn->sql_fetch_object($rs);
		$scenario_details = $rs->scenarioDetails;
		
		if($rs->applicationType != ucfirst(strtolower($CLOUD_PLAN_AZURE_SOLUTION)) && $rs->applicationType != ucfirst(strtolower($CLOUD_PLAN_VMWARE_SOLUTION)) ) {
			$cdata_arr = $commonfunctions_obj->getArrayFromXML($scenario_details);
			$scenario_details = unserialize($cdata_arr['plan']['data']['value']);
		}else {
			$scenario_details = unserialize($rs->scenarioDetails);
		}

		if(!$direction)
		$direction = $rs->protectionDirection;
		else
		$direction = $direction;
		
		$result['applicationType'] = $rs->applicationType;
		$result['applicationVersion'] = $rs->applicationVersion;
		$result['globalOption'] = $scenario_details['globalOption'];
		$result['reverseProtection'] = $scenario_details['reverseProtection'];
		$result['scenarioType'] = $rs->scenarioType;
		$result['planId'] = $rs->planId;
		$result['participatingNodes'] = $scenario_details['participatingNodes'];
		$result['planType'] = $scenario_details['planType'];
		$result['solutionType'] = $scenario_details['solutionType'];
		$result['executionStatus'] = $rs->executionStatus;
		$result['sourceVolumes'] = $rs->sourceVolumes;
		$result['isVolumeResized'] = $rs->isVolumeResized;
		$result['targetVolumes'] = $rs->targetVolumes;
		$result['retentionVolumes'] = $rs->retentionVolumes;
		$result['targetArrayGuid'] = $rs->targetArrayGuid;
		$result['applicationVolumes'] = $scenario_details['applicationVolumes'];
		$result['otherVolumes'] = $scenario_details['otherVolumes'];
		$result['sourceArrayGuid'] = $rs->sourceArrayGuid;
		$result['forwardSolutionType']=$scenario_details['forwardSolutionType'];
		
		if($direction == 'reverse')
		{
			$result['sourceHostId'] = $rs->targetId;
			$result['srcAppId'] = $rs->trgInstanceId;
			$result['srcIsClustered'] = $scenario_details['targetIsClustered'];
			$result['applicationVolumes'] = $scenario_details['applicationVolumes'];
			$result['targetHostId'] = $rs->sourceId;
			$result['targetAPPId'] = $rs->srcInstanceId;
			$result['targetIsClustered'] = $scenario_details['srcIsClustered'];
			$result['pairInfo'] = $scenario_details['reversepairInfo'];
			$result['direction'] = $direction;
		}
		else
		{
			$result['sourceHostId'] = $rs->sourceId;
			$result['srcAppId'] = $rs->srcInstanceId;
			$result['srcIsClustered'] = $scenario_details['srcIsClustered'];
			$result['applicationVolumes'] = $scenario_details['applicationVolumes'];
			$result['targetHostId'] = $rs->targetId;
			$result['targetAPPId'] = $rs->trgInstanceId;
			$result['targetIsClustered'] = $scenario_details['targetIsClustered'];
			$result['pairInfo'] = $scenario_details['pairInfo'];
			$result['direction'] = $direction;
		}
		return $result;
	}
	
	public function sendTrapsforApplicationAlerts($error_template_id,$message)
	{
		global $ABHAI_MIB, $TRAP_AMETHYSTGUID, $TRAP_AGENT_ERROR_MESSAGE, $TRAP_APPLICATION_PROTECTION_ALERTS, $HOST_GUID;
		$commonfunctions_obj = new CommonFunctions();
		$trap_source = "ApplicationProtectionAlerts";
		
        $cx_hostid = $HOST_GUID;
		$cx_os_flag = $commonfunctions_obj->get_osFlag($cx_hostid);
		
		$trap_cmdline = '';
		if($cx_os_flag == 1)
		{
			$trap_cmdline   .= " -traptype ".$TRAP_APPLICATION_PROTECTION_ALERTS;
			$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."::s::\"".$cx_hostid."\"";
			$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."::s::\"".$message."\"";
		}
		else
		{
			$trap_cmdline   .= "Abhai-1-MIB-V1::$trap_source ";
			$trap_cmdline   .= "Abhai-1-MIB-V1::trapAmethystGuid ";
			$trap_cmdline   .= "s \"$cx_hostid\" ";
			$trap_cmdline   .= "Abhai-1-MIB-V1::$TRAP_AGENT_ERROR_MESSAGE ";
			$trap_cmdline   .= "s \"$message \" ";
		}
		$return_result = $commonfunctions_obj->send_trap($trap_cmdline, $error_template_id);
		return $return_result;
	}
	
	public function updatePartialProtection($scenarioId,$isModified)
	{
		$sql = "UPDATE  
					applicationScenario
				SET
					isModified = '$isModified'
				WHERE
					scenarioId= '$scenarioId'";
		
		$rs = $this->conn->sql_query($sql);
	}
	
	private function _getPolicyNumRows($policy_id,$policy_scheduled_type,$policy_type,$solution_type,$host_id)
	{
        global $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
		$num_rows = 0;
		if($policy_type>=6 && $policy_type<=11)
		{
			if(!$policy_scheduled_type)
			{
				$num_rows = 1;
			}
			else
			{
				$sql = "SELECT
							policyInstanceId
						FROM
							policyInstance
						WHERE
							policyState in ('3','4') AND
							policyId='$policy_id'";
				$rs = $this->conn->sql_query($sql);
				$num_rows = $this->conn->sql_num_row($rs);
			}
		}
		else
		{
			if($policy_scheduled_type == 2 || $policy_scheduled_type == 0)
			{
				##
                # Bug - 1961444
                # Checking if their are really any new/pending Jobs configured against this hostId.
                ##
                if ((($solution_type == 'PRISM') && ($policy_type == 2 )) || (($policy_type == 4) && ($solution_type == $CLOUD_PLAN_AZURE_SOLUTION || $solution_type == $CLOUD_PLAN_VMWARE_SOLUTION) && ($host_id)))
				{	
					$cond = " AND hostId = '$host_id'";
				}
				else
				{
					$cond = "";
				}
				$sql = "SELECT
							policyInstanceId
						FROM
							policyInstance
						WHERE
							policyState in ('3','4') AND
							policyId='$policy_id' $cond";
			}
			else
			{
				$sql = "SELECT
							policyInstanceId
						FROM
							policyInstance
						WHERE
							policyId='$policy_id'";
			}
			$rs = $this->conn->sql_query($sql);
			$num_rows = $this->conn->sql_num_row($rs);
			if($policy_scheduled_type == 1) 
			{
				$num_rows = 1;
			}
		}
		return $num_rows;
	}
	
	public function getInstanceNames($src_instance_id)
	{
		$src_instance_id = explode(",",$src_instance_id);
		$src_instance_id = implode("','",$src_instance_id);
		
		$sql = "SELECT
					applicationInstanceName
				FROM
					applicationHosts
				WHERE
					applicationInstanceId IN ('$src_instance_id')";
		$rs = $this->conn->sql_query($sql);
		$instance_names = array();
		while($row = $this->conn->sql_fetch_object($rs))
		{
			$instance_names[] = $row->applicationInstanceName;
		}
		
		return $instance_names;
	}
	
	public function isScenarioResized($scenarioId)
	{
		$sql = "SELECT
					isVolumeResized
				FROM
					applicationScenario
				WHERE
					scenarioId = '$scenarioId'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$isVolumeResized = $row->isVolumeResized;
		return $isVolumeResized;		
	}
	
	function getInstanceName($app_id)
    {		
		$sql3 = "SELECT
					applicationInstanceName
				FROM
					applicationHosts
				WHERE
					applicationInstanceId In ('$app_id')";
		$rs3 = $this->conn->sql_query($sql3);        
		$num_rows = $this->conn->sql_num_row($rs3);
		$app_name = '0';
		if ($num_rows > 0)
		{
			$row3 = $this->conn->sql_fetch_object($rs3);
			$instance_name = $row3->applicationInstanceName;
			if ($instance_name)
			{
				$app_name = $instance_name;
			}
		}
		return $app_name;    
    }
	
	private function updateConsistencyCommand($new_consistency_cmd,$scenario_id, $old_consistency_cmd)
	{
		global $logging_obj;
		$policy_params_arr = array();
		
		$sql = "SELECT
					policyParameters,
					policyId
				FROM 
					policy
				WHERE 
					scenarioId = '$scenario_id'
				AND
					policyType IN('2','4')";		
		
		$rs = $this->conn->sql_query($sql);
		$logging_obj->my_error_handler("INFO","updateConsistencyCommand :: SQL1::$sql\n");
		while($row = $this->conn->sql_fetch_object($rs))
		{
			$policyParameters = $row->policyParameters;
			$policy_id = $row->policyId;
			
			$policy_params_arr = unserialize($policyParameters);
			$policy_params_arr['ConsistencyCmd'] = $new_consistency_cmd;
			$session_consistency_cmd = $policy_params_arr['ConsistencyCmd'];
			$newPolicyParameters = serialize($policy_params_arr);
			$newPolicyParameters = $this->conn->sql_escape_string($newPolicyParameters);
			
			$logging_obj->my_error_handler("INFO","updateConsistencyCommand :: policy_id::$policy_id old_consistency_cmd::$old_consistency_cmd session_consistency_cmd::$session_consistency_cmd");			
			$logging_obj->my_error_handler("INFO","updateConsistencyCommand :: newPolicyParameters::$newPolicyParameters".print_r($policy_params_arr,TRUE));			
			
			if($old_consistency_cmd == $session_consistency_cmd)
			{
				$sql1 = "UPDATE 
						policy
					SET
						policyParameters = '$newPolicyParameters'
					WHERE
						policyId='$policy_id'";
						
				$logging_obj->my_error_handler("INFO","updateConsistencyCommand :: update sql:: $sql1\n");	
				$this->conn->sql_query($sql1);
			}	
		}		
	}
	
	public function get_protected_volume_for_consistency($host_id, $phy_lun_id_arr, $vendor_origin_arr, $volume_type_arr)
	{
		$temp_array = array();
		global $logging_obj;
		$logging_obj->my_error_handler("INFO","get_protected_volume_for_consistency host_id : $host_id");
		$logging_obj->my_error_handler("INFO","get_protected_volume_for_consistency phy_lun_ids_array :: ".print_r($phy_lun_id_arr,TRUE));
		$logging_obj->my_error_handler("INFO","get_protected_volume_for_consistency vendor_origin_array : ".print_r($vendor_origin_arr,TRUE));
		$logging_obj->my_error_handler("INFO","get_protected_volume_for_consistency volume_type_arr : ".print_r($volume_type_arr,TRUE));
		foreach($phy_lun_id_arr as $phy_lun_id)
		{
			$vendor_origin = $vendor_origin_arr[$phy_lun_id];
			$volume_type = $volume_type_arr[$phy_lun_id];
			$sql = "SELECT 
					DISTINCT 
						deviceName
					FROM 
						logicalVolumes 
					WHERE 
						hostId='$host_id' AND 
						Phy_Lunid='$phy_lun_id' AND
						vendorOrigin='$vendor_origin' AND
						volumeType='$volume_type'";
		
			$result_sql = $this->conn->sql_query($sql);
			if($this->conn->sql_num_row( $result_sql) > 0)
			{
				while ($row = $this->conn->sql_fetch_object($result_sql))
				{
					$temp_array[$phy_lun_id][]= $row->deviceName;				
				}		
			}
		}
		$logging_obj->my_error_handler("DEBUG","get_protected_volume_for_consistency temp_array :: ".print_r($temp_array,TRUE));
		$i=1;
		foreach($temp_array as $key_phy_lun_id => $value_src_device_name)
		{
			if($i == 1)
			{
				$src_vol_str = $temp_array[$key_phy_lun_id][0];
			}
			else
			{
				$src_vol_str = $src_vol_str.",".$temp_array[$key_phy_lun_id][0];
			}
			$i++;
		}
		$logging_obj->my_error_handler("INFO","get_protected_volume_for_consistency src_vol_str : $src_vol_str");
		return $src_vol_str;
	}

	public function checkSolutionType($plan_id)
	{
		$sel_solution_type = "SELECT
								solutionType
							FROM
								applicationPlan 
							WHERE
								planId = '$plan_id'";
		$rs = $this->conn->sql_query($sel_solution_type);
		$row = $this->conn->sql_fetch_object($rs);
		$solutionType = $row->solutionType;
		return $solutionType;
	
	}
	
	public function updateConsistencyPolicy($deleted_instance_info,$flag=0)
	{
		global $VALIDATION_SUCCESS,$INACTIVE,$FAILOVER_DONE;
		global $logging_obj;
		$commonfunctions_obj = new CommonFunctions();
		$logging_obj->my_error_handler("DEBUG","deleted_instance_info ".print_r($deleted_instance_info,true));
		$logging_obj->my_error_handler("DEBUG","FLAG::$flag");
		foreach($deleted_instance_info as $key=>$data)
		{
			$instance_id = $data['id'];
			if(is_array($data['vol']))
			{
				$vol_list = array_unique($data['vol']);
			}	
			$logging_obj->my_error_handler("DEBUG","vol_list ".print_r($vol_list,true));
			
			$sql = "SELECT
						scenarioId,
						srcInstanceId,
						trgInstanceId,
						scenarioDetails,
						protectionDirection,
						executionStatus,
						planId
					FROM
						applicationScenario
					WHERE
						(srcInstanceId LIKE '%$instance_id%' OR trgInstanceId LIKE '%$instance_id%') AND
						scenarioType IN ('DR Protection','Backup Protection') AND
						executionStatus IN ('$VALIDATION_SUCCESS','$INACTIVE','$FAILOVER_DONE')";
			$logging_obj->my_error_handler("DEBUG","SQL::$sql");
			
			$rs = $this->conn->sql_query($sql);
			while($row = $this->conn->sql_fetch_object($rs))
			{
				$scenario_id = $row->scenarioId;
				$scenario_direction = $row->protectionDirection;
				$executionStatus = $row->executionStatus;
				$planId = $row->planId;
				$logging_obj->my_error_handler("DEBUG","scenario_direction::$scenario_direction");
				if(($scenario_direction == 'forward') || 
					($scenario_direction == 'reverse'))
				{
					$src_instance_id = $row->srcInstanceId;
					$trg_instance_id = $row->trgInstanceId;
								
					$scenario_details = $row->scenarioDetails;
					$src_instance_id_list = explode(",",$src_instance_id);
					$logging_obj->my_error_handler("DEBUG","$scenario_id -- $src_instance_id -- $trg_instance_id src_instance_id_list::".print_r($src_instance_id_list,true));
					
					$tgt_instance_id_list = explode(",",$trg_instance_id);
					$logging_obj->my_error_handler("DEBUG","src_instance_id_list::".print_r($tgt_instance_id_list,true));
					
					if((($scenario_direction == 'forward' && in_array($instance_id,$src_instance_id_list)) ||($scenario_direction == 'reverse' && $executionStatus == $INACTIVE))  || (($scenario_direction == 'reverse' && in_array($instance_id,$tgt_instance_id_list)) || ($scenario_direction == 'forward' && $executionStatus == $FAILOVER_DONE)))
					{
						$cdata_arr = $commonfunctions_obj->getArrayFromXML($scenario_details);
						$scenario_details = unserialize($cdata_arr['plan']['data']['value']);
						$logging_obj->my_error_handler("DEBUG","scenario_details inside while::".print_r($scenario_details,true));
						$application_vol = array();
						$other_vol = array();
						$application_vol = explode(",",$scenario_details['applicationVolumes']);
												
						if($scenario_details['otherVolumes'])
						{
							$other_vol = explode(",",$scenario_details['otherVolumes']);
							$logging_obj->my_error_handler("DEBUG","other_vol::".print_r($other_vol,true));
						}
						
						
						$logging_obj->my_error_handler("DEBUG","application_vol::".print_r($application_vol,true));
						$logging_obj->my_error_handler("DEBUG","other_vol::".print_r($other_vol,true));
						$all_volumes1 = array_merge($application_vol,$other_vol);
						$all_volumes1 = array_filter($all_volumes1);
						$logging_obj->my_error_handler("DEBUG","all_volumes1::".print_r($all_volumes1,true));
						$vol_list1 = array();
						foreach($vol_list as $key => $value)
						{
							$logging_obj->my_error_handler("DEBUG","value::$value");
							if(in_array($value,$all_volumes1))
							{
								$logging_obj->my_error_handler("DEBUG","value::$value in loop");
								$vol_list1[] = $value;
								$logging_obj->my_error_handler("DEBUG","vol_list".print_r($vol_list1,TRUE));	
							}
							else
							{
								$logging_obj->my_error_handler("DEBUG","value::$value else loop");
							}
						}
						$logging_obj->my_error_handler("DEBUG","vol_list".print_r($vol_list1,TRUE));	
						$new_app_vols = array_diff($vol_list1,$application_vol);
						$new_app_vols = array_unique($new_app_vols);
						
						if(count($new_app_vols))
						{
							$logging_obj->my_error_handler("DEBUG","new_app_vols::".print_r($new_app_vols,true));
							$app_vols = implode(",",$new_app_vols);
							$scenario_details['applicationVolumes'] = $scenario_details['applicationVolumes'].','.$app_vols;
							$new_app_vols = explode(",",$scenario_details['applicationVolumes']);
							$logging_obj->my_error_handler("DEBUG","other_vol::".print_r($other_vol,true));
							$other_vols=array();
							if(count($other_vol))
							{
								$other_vols = array_diff($other_vol,$new_app_vols);
							}
							$new_other_vols = explode(",",$scenario_details['applicationVolumes']);
							//$other_vols1 = $scenario_details['otherVolumes'];
							$scenario_details['otherVolumes'] = implode(",",$other_vols);
							$logging_obj->my_error_handler("DEBUG","new_app_vols::$new_app_vols".print_r($new_app_vols,true));
							$logging_obj->my_error_handler("DEBUG","other_vols::".print_r($other_vols,true));
						
													
							###################################################
							// Fix for #13767
							if($new_other_vols || $app_vols)
							{							
								$logging_obj->my_error_handler("DEBUG","new_app_vols::$new_app_vols".print_r($new_app_vols,true));
								$logging_obj->my_error_handler("DEBUG","other_vols::".print_r($other_vols,true));
							
													
								$newly_added_vols = '';
								foreach($new_other_vols as $keyA=> $valA)
								{
									$is_mount_arr = explode(":\\",$valA);
									$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy::is_mount_arr".print_r($is_mount_arr,TRUE));
									$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy::$valA");
									$newly_added_vols .= ( count($is_mount_arr) > 1) ? $valA.';' : $valA.':;';
								}
								$newly_added_vols = substr_replace($newly_added_vols,"",-1);
								$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy :: newly_added_vols:: $newly_added_vols other_vols::".print_r($other_vols));	
								
								if($other_vols)
								{
									foreach($other_vols as $keyG=> $valG)
									{
										$is_mount_arr = explode(":\\",$valG);
										$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy::is_mount_arr".print_r($is_mount_arr,TRUE));
										$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy::$valG");
										$other_vols1 .= ( count($is_mount_arr) > 1) ? $valG.';' : $valG.':;';
									}
								}								
								$other_vols1 = substr_replace($other_vols1,"",-1);
								$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy :: other_vols1:: $other_vols1");	
								
								foreach($scenario_details['pairInfo'] as $key=>$value)
								{
									for($j=0; $j<(count($value['consistencyOptions'])); $j++)
									{
										$new_option = '';
										$new_consistency_cmd = '';
										$app_consistency_cmd = $value['consistencyOptions'][$j]['option'];
										$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy :: app_consistency_cmd:: $app_consistency_cmd");								
										$pattern1 = '/\s+-v\s+/i';					
										$matched1 = preg_match($pattern1, $app_consistency_cmd, $matches);
										
										$pattern2 = '/\s+-x\s+/i';					
										$matched2 = preg_match($pattern2, $app_consistency_cmd, $matches);
										
										if(count($other_vols) && $scenario_details['applicationType'] == 'File Servers')
										{
											$new_option = "-v \"$newly_added_vols".";"."$other_vols1\" ";
										}
										elseif(count($other_vols))
										{
											$new_option = "-v \"$other_vols1\" ";
										}
										elseif($scenario_details['applicationType'] == 'File Servers')
										{
											$new_option = "-v \"$newly_added_vols\" ";
										}	
										else
										{
											$new_option = "";
										}									
										
										$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy :: matched1:: $matched1 new_option:: $new_option");	
										
										if($matched1)
										{
											$new_consistency_cmd = preg_replace('/-v\s+\"(.*?)\"\s*/', $new_option, $app_consistency_cmd);
										}
										else
										{
											$new_consistency_cmd = $app_consistency_cmd." ".$new_option;
										}
										
										$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy :: new_consistency_cmd:: $new_consistency_cmd");
										$this->updateConsistencyCommand($new_consistency_cmd, $scenario_id, $value['consistencyOptions'][$j]['option']);
										$new_consistency_for_session[] = $new_consistency_cmd;
									}	
								}								
								$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy :: new_consistency_for_session".print_r($new_consistency_for_session,true));
								foreach($scenario_details['pairInfo'] as $keyA=>$valueA)
								{
									foreach($valueA['consistencyOptions'] as $keyB=>$valueB)
									{
										$scenario_details['pairInfo'][$keyA]['consistencyOptions'][$keyB]['option'] = $new_consistency_for_session[$keyB];
									}
								}
								foreach($scenario_details['reversepairInfo'] as $keyC=>$valueC)
								{
									foreach($valueC['consistencyOptions'] as $keyD=>$valueD)
									{
										$scenario_details['reversepairInfo'][$keyC]['consistencyOptions'][$keyD]['option'] = $new_consistency_for_session[$keyD];
									}
								}
								if(is_array($scenario_details['primaryProtectionInfo']))
								{
									foreach($scenario_details['primaryProtectionInfo']['pairInfo'] as $keyF=>$valueF)
									{
										foreach($valueF['consistencyOptions'] as $keyE=>$valueE)
										{
											$scenario_details['primaryProtectionInfo']['pairInfo'][$keyF]['consistencyOptions'][$keyE]['option'] = $new_consistency_for_session[$keyE];
										}
									}
								}
															
							}
							// End of Fix
							######################################################						
						}
						$tmp_arr = array();
						$new_src_instance_ids = $src_instance_id_list;
						$new_trg_instance_ids = $trg_instance_id;
						if($flag == 1)
						{
							$tmp_arr[] = $instance_id;
							$new_src_instance_ids = array_diff($src_instance_id_list,$tmp_arr);
							#$scenario_details['srcAPPId'] = implode(",",$new_src_instance_ids);
							$logging_obj->my_error_handler("DEBUG","srcAPPId::".$scenario_details['srcAPPId']);
							
							$trg_instance_id_list = explode(",",$trg_instance_id);
							$new_trg_instance_ids = array_diff($trg_instance_id_list,$tmp_arr);
							$new_trg_instance_ids = implode(",",$new_trg_instance_ids);
							$logging_obj->my_error_handler("DEBUG","new_trg_instance_ids::$new_trg_instance_ids");
							#$scenario_details['targetAPPId'] = $new_trg_instance_ids;
						}
						
						$new_src_instance_ids = implode(",",$new_src_instance_ids);
						
						$logging_obj->my_error_handler("DEBUG","scenario_details outside loop::".print_r($scenario_details,true));
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
									scenarioId = '$scenario_id'";
						$logging_obj->my_error_handler("DEBUG","updateConsistencyPolicy SQL::::$sql");
						$result_set = $this->conn->sql_query($sql);
					}
				}
			}
		}
	}
	
	public function getScenarioType($scenario_id)	
	{
		$scenario_type = $this->conn->sql_get_value('applicationScenario', 'scenarioType',"scenarioId='$scenario_id'");
		return $scenario_type;
	}
	
	public function vxstubUpdateStatusForManualFailoverfailbackPlans($scenarioId)
	{
		global $logging_obj, $FAILOVER_COMPLETE, $INACTIVE,$CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
		
		$sql_pairs_in_plan = "SELECT destinationDeviceName, deleted FROM srcLogicalVolumeDestLogicalVolume WHERE scenarioId='$scenarioId'";
		
		$logging_obj->my_error_handler("DEBUG","Inside IF scenarioId:: $sql_pairs_in_plan");
		$rs3 = $this->conn->sql_query($sql_pairs_in_plan);
		$dest_volume = array();
		$deleted = array();
		while($fetch_data3 = $this->conn->sql_fetch_object($rs3))
		{
			$dest_volume[] = $fetch_data3->destinationDeviceName;
			$deleted[] = $fetch_data3->deleted;
		}
		$logging_obj->my_error_handler("DEBUG","Inside IF dest_volume::".print_r($dest_volume,TRUE));
		$logging_obj->my_error_handler("DEBUG","Inside IF deleted::".print_r($deleted,TRUE));
		$pair_count = count($dest_volume);
		$logging_obj->my_error_handler("DEBUG","Inside IF pair_count:: $pair_count");
		$flag =0;
		for($i=0;$i<$pair_count;$i++)
		{
			$value2 = $deleted[$i];
			$logging_obj->my_error_handler("DEBUG","Inside IF i:$i , value2:: $value2");
			if($value2 == 1)
			{
					$flag++;
			}
			$logging_obj->my_error_handler("DEBUG","Inside IF flag:: $flag");
		}
		$logging_obj->my_error_handler("DEBUG","Inside IF flag:: $flag");
		$logging_obj->my_error_handler("DEBUG","Inside IF pair_count:: $pair_count");
		if($pair_count == $flag)
		{
			$query2 = "SELECT planId, protectionDirection, scenarioType, applicationType FROM applicationScenario WHERE scenarioId='$scenarioId'";
			$logging_obj->my_error_handler("DEBUG","Inside IF query2:: $query2");
			$rs_query2 = $this->conn->sql_query($query2);
			$row_query2 = $this->conn->sql_fetch_object($rs_query2);
			$protection_direction = $row_query2->protectionDirection;
			$plan_id = $row_query2->planId;
			$scenario_type = $row_query2->scenarioType;
			$app_type = strtoupper($row_query2->applicationType);
			$logging_obj->my_error_handler("DEBUG","Inside IF protection_direction:: $protection_direction");
			if($protection_direction == 'forward')
			{
				$query3 = "SELECT executionStatus FROM applicationScenario WHERE referenceId='$scenarioId' AND scenarioType IN ('Planned Failover','Unplanned Failover')";
				$logging_obj->my_error_handler("DEBUG","Inside IF query3:: $query3");
				$value1 = '103';
			}
			elseif($protection_direction == 'reverse')
			{
				$query3 = "SELECT executionStatus FROM applicationScenario WHERE referenceId='$scenarioId' AND scenarioType IN ('Failback','Fast Failback')";
				$value1 = '95';
				$logging_obj->my_error_handler("DEBUG","Inside IF query3:: $query3");
			}
			
			$rs_query3 = $this->conn->sql_query($query3);
			$flag1 =0;
			while($row_query3 = $this->conn->sql_fetch_object($rs_query3))
			{
				if($row_query3->executionStatus != $INACTIVE)
				{
						$flag1 =1;
						break;
				}
			}	
			$logging_obj->my_error_handler("DEBUG","Inside IF flag1:: $flag1");
			if(!$flag1 && $scenario_type != 'Backup Protection')
			{
				$sql = "UPDATE applicationScenario set executionStatus='$value1' WHERE scenarioId='$scenarioId'";
				$rs2 = $this->conn->sql_query($sql);
				$logging_obj->my_error_handler("DEBUG","Inside IF sql:: $sql");
				$sql1 = "DELETE FROM policy WHERE scenarioid='$scenarioId'";
				$rs11 = $this->conn->sql_query($sql1);
				if($app_type != $CLOUD_PLAN_AZURE_SOLUTION && $app_type != $CLOUD_PLAN_VMWARE_SOLUTION && $app_type != 'AWS')
				{
					$this->_deleteReplicationPairsForPlan($plan_id,$scenarioId);
				}
			}
			
		}
	}
	
	public function vxstubUpdateRescanScriptPolicy($host_id,$policy_id,$policy_instance_id,$status,$log)
	{
		global $logging_obj;
		
		$sql = "UPDATE
					policyInstance
				SET
					policyState = ?,
					executionLog = ?,
					lastUpdatedTime = now() 
				WHERE
					policyId = ? AND
					uniqueId = ?";
		$logging_obj->my_error_handler("DEBUG"," vxstubUpdateRescanScriptPolicy :: $sql");
		$rs = $this->conn->sql_query($sql, array($status, $log, $policy_id, $policy_instance_id));
	}
	
	/*This method is for V2A migration policy updates*/
	public function vxstubUpdateV2AMigrationPolicy($host_id,$policy_id,$policy_instance_id,$status,$log)
	{
		global $logging_obj;
		
		$sql = "UPDATE
					policyInstance
				SET
					policyState = ?,
					executionLog = ?,
					lastUpdatedTime = now()
				WHERE
					policyId = ? AND
					uniqueId = ?";
		$logging_obj->my_error_handler("DEBUG"," vxstubUpdateV2AMigrationPolicy :: $sql");
		$rs = $this->conn->sql_query($sql, array($status, $log, $policy_id, $policy_instance_id));
	}
	
	public function deletePairFromPlan($pair, $scenario_id)
	{
		global $commonfunctions_obj, $logging_obj;
		$scenario_details = $this->getScenarioSessionData($scenario_id);	
		$pair_details = explode("=",$pair);
		$update_direction = array('pairInfo', 'reversepairInfo');
		$logging_obj->my_error_handler("INFO"," deletePairFromPlan pair :: $pair scenario_id::$scenario_id");
		foreach($update_direction as $pair_key => $pair_value)
		{
			if($pair_value == 'pairInfo')
			{
				$src_dest_name = $pair_details[1]."=".$pair_details[3];	
			}
			else
			{
				$src_dest_name = $pair_details[3]."=".$pair_details[1];
			}
			$fwd_src_vol = array();
			$application_volumes = explode(",",$scenario_details['applicationVolumes']);
			$source_id = explode(",",$scenario_details['sourceHostId']);
			$src_os_flag = $commonfunctions_obj->get_osFlag($source_id[0]);
			$logging_obj->my_error_handler("INFO"," deletePairFromPlan application_volumes".print_r($application_volumes, true));
			foreach($scenario_details[$pair_value] as $key => $value)
			{
				foreach($value['pairDetails'] as $key1 => $value2)
				{
					if($src_dest_name == $key1)
					{
						unset($scenario_details[$pair_value][$key]['pairDetails'][$key1]);
					}
					else
					{
						$logging_obj->my_error_handler("INFO"," deletePairFromPlan applicationType".$scenario_details['applicationType']);
						if($scenario_details['applicationType'] == 'File Servers')
						{
							$fwd_src_vol[] = $value2['srcVol'];
						}
						elseif(!in_array($value2['srcVol'], $application_volumes))
						{
							$fwd_src_vol[] = $value2['srcVol'];
						}	
						if($pair_value == 'pairInfo')
						{
							$source_volumes[] = $this->conn->sql_escape_string($value2['srcVol']);
							$target_volumes[] = $this->conn->sql_escape_string($value2['trgVol']);
						}
					}
				}
				$logging_obj->my_error_handler("INFO"," deletePairFromPlan source_volumes".print_r($source_volumes, true)."target_volumes::".print_r($target_volumes,true));
				
				if($pair_value == 'pairInfo') // fetching the source & target volumes to update in db after selected pair got deleted
				{
					$source_volumes = implode(",",$source_volumes);
					$target_volumes = implode(",",$target_volumes);
				}
				
				$src_device_name_list="";
				$array_vol_count=0;
				
				foreach($fwd_src_vol as $key3 => $value1)
				{
					if(strlen($value1) == 1)
					{
						$src_device_name_list .= $value1.":";
					}
					elseif(strlen($value1) > 1)
					{
						$src_device_name_list .= $value1;
					}
					
					$array_vol_count++;
					if(count($fwd_src_vol) != $array_vol_count)
					{									
						if( $src_os_flag == 1)
						{
							$src_device_name_list .= ";";
						}
						else
						{
							$src_device_name_list .= ",";
						}
					}
				}
				$logging_obj->my_error_handler("INFO"," deletePairFromPlan src_device_name_list::$src_device_name_list");
				// updating the consistency command with the deleted pair
				foreach($value['consistencyOptions'] as $key4 => $value3)
				{
					$option = $value3['option'];
					$consistency_cmd = explode(" -v",$option);	
					$logging_obj->my_error_handler("INFO"," deletePairFromPlan consistency_cmd".print_r($consistency_cmd,true));
					if($consistency_cmd[1])
					{
						$logging_obj->my_error_handler("INFO"," deletePairFromPlan src_device_name_list::$src_device_name_list");		
						if($src_device_name_list)
						{
							$new_temp_vacp_cmd = "-v \"".$src_device_name_list."\" ";
						}
						$logging_obj->my_error_handler("INFO"," deletePairFromPlan new_temp_vacp_cmd_arr::$new_temp_vacp_cmd option::$option");	
						
						$cmd12 = preg_replace('/-v\s+\"(.*?)\"\s*/', $new_temp_vacp_cmd, $option);		
						$logging_obj->my_error_handler("INFO"," deletePairFromPlan cmd12::$cmd12");							
						$scenario_details[$pair_value][$key]['consistencyOptions'][$key4]['option'] = $cmd12;
						$logging_obj->my_error_handler("INFO"," deletePairFromPlan session value".$scenario_details[$pair_value][$key]['consistencyOptions'][$key4]['option']);					
					}
					else
					{
						$scenario_details[$pair_value][$key]['consistencyOptions'][$key4]['option'] = $option;
					}
					//if there are multiple consistency policies then always need to update first window consistenct policy and consider only pair info consistency as we are allowing deletion only in forward direction.
					if($pair_value == 'pairInfo') 
					{					
						$cmd[$option] = $scenario_details[$pair_value][$key]['consistencyOptions'][$key4]['option'];	
					}
					$logging_obj->my_error_handler("INFO"," deletePairFromPlan scenario_details".print_r($scenario_details,true)."cmd::".print_r($cmd,true));
				}
			}
		}
		
		//Updating the volpack details information.
		foreach($scenario_details['tgt_vol_pack_details'] as $key5 => $value5)
		{
			if($pair_details[3] == $value5['new_device_name'])
			{
				unset($scenario_details['tgt_vol_pack_details'][$key5]);	
			}
		}
		if($scenario_details['autoVolumes'])
		{
			$target_volpack = explode(",",$scenario_details['autoVolumes']);
			foreach($target_volpack as $key6 => $value6)
			{
				if($pair_details[3] != $value6)
				{
					$volpack_volumes[] = $value6;
				}
			}
			$target_volpack = implode(",",$volpack_volumes);
			$scenario_details['autoVolumes'] = $target_volpack;	
		}	
		$scenario_details = $this->setServerVols($scenario_details, $pair_details[1]);
		$commonfunctions_obj->serialise($scenario_details, $source_volumes, $target_volumes, 1, $scenario_id);		
		$this->updateRecoveryScenarios($scenario_id, $cmd, $pair_details[1]);
	}
	
	private function updateRecoveryScenarios($scenario_id, $consistency_cmd, $src_vol)
	{
		global $logging_obj;
		$commonfunctions_obj = new CommonFunctions();
		$sql = "SELECT policyParameters, policyId FROM policy WHERE policyType IN (2,4) AND scenarioId='$scenario_id'";
		$rs = $this->conn->sql_query($sql);
		$logging_obj->my_error_handler("INFO"," updateRecoveryScenarios src_vol::$src_vol scenario_id::$scenario_id consistency_cmd".print_r($consistency_cmd,true));
		while($data = $this->conn->sql_fetch_object($rs))
		{
			$policy_data = unserialize($data->policyParameters);
			foreach($consistency_cmd as $key => $value)
			{
				if($key == $policy_data['ConsistencyCmd'])
				{
					$policy_data['ConsistencyCmd'] = $value;
				}
			}
			$latest_policy_data = serialize($policy_data);
			$latest_policy_data = $this->conn->sql_escape_string($latest_policy_data);
			// in order to update source readiness & consistency with the consistency command
			$sql = "UPDATE policy SET policyParameters='$latest_policy_data' WHERE policyId='$data->policyId'";
			$this->conn->sql_query($sql);
			$logging_obj->my_error_handler("INFO"," updateRecoveryScenarios latest_policy_data::$latest_policy_data sql::$sql");
		}
				
		$sql1 = "SELECT scenarioId, scenarioType, scenarioDetails FROM applicationScenario WHERE referenceId='$scenario_id'";
		$rs1 = $this->conn->sql_query($sql1);
		$logging_obj->my_error_handler("INFO"," updateRecoveryScenarios sql::$sql");
		while($data1 = $this->conn->sql_fetch_object($rs1))
		{
			$scenario_id1[] = $data1->scenarioId;
			if($data1->scenarioType == 'Data Validation and Backup' || $data1->scenarioType == 'Rollback')
			{
				$cdata = $data1->scenarioDetails;
				$cdata_arr = $commonfunctions_obj->getArrayFromXML($cdata);
				$recovery_scenario_details[$data1->scenarioId] = unserialize($cdata_arr['plan']['data']['value']);
			}
		}
		$scenario_ids = implode("','",$scenario_id1);
		// Inorder to update all the policies of the particualr recovery scenarios
		$sql = "UPDATE policy SET policyTimestamp=now() WHERE scenarioId IN ('$scenario_ids','$scenario_id')";
		$this->conn->sql_query($sql);
		$logging_obj->my_error_handler("INFO"," updateRecoveryScenarios src_vol :: $src_vol sql::$sql recovery_scenario_details".print_r($recovery_scenario_details,true));
		$this->updateBackupScenarioSession($recovery_scenario_details, $src_vol);
	}
	
	private function setServerVols($scenario_details, $src_vol)
	{
		if($scenario_details['planType'] == 'BULK')
		{
			$server_vols = $scenario_details['Servers']['server_vols'] ? $scenario_details['Servers']['server_vols']:$scenario_details['primaryProtectionInfo']['Servers']['server_vols'];
			$flag=1;
			if(!$server_vols)
			{
				$server_vols = $scenario_details['Cluster Servers']['clusters_vols'] ? $scenario_details['Cluster Servers']['clusters_vols']:$scenario_details['primaryProtectionInfo']['Cluster Servers']['clusters_vols'];	
				$flag =2;
			}
			elseif(!$server_vols)
			{
				$server_vols = $scenario_details['Xen Servers']['xen_vols'] ? $scenario_details['Xen Servers']['xen_vols'] : $scenario_details['primaryProtectionInfo']['Xen Servers']['xen_vols'];	
				$flag =3;
			}
			elseif(!$server_vols)
			{
				$server_vols = $scenario_details['Shared Devices']['shared_device_list'] ? $scenario_details['Shared Devices']['shared_device_list'] : $scenario_details['primaryProtectionInfo']['Shared Devices']['shared_device_list'];	
				$flag=4;
			}
			$server_vols = explode(",", $server_vols);
			foreach($server_vols as $key4 => $value4)
			{
				$vols = explode("(", $value4);
				if($vols[0] != $src_vol)
				{
					$src_vols[] = $value4;
				}
			}
			$final_server_vols = implode(",",$src_vols);
			
			if($flag == 1)
			{
				$scenario_details['Servers']['server_vols'] = $final_server_vols;
				if($scenario_details['primaryProtectionInfo']['Servers']['server_vols'])
				{
					$scenario_details['primaryProtectionInfo']['Servers']['server_vols'] = $final_server_vols;
				}	
			}
			elseif($flag == 2)
			{
				$scenario_details['Cluster Servers']['clusters_vols'] = $final_server_vols;
				if($scenario_details['primaryProtectionInfo']['Cluster Servers']['clusters_vols'])
				{
					$scenario_details['primaryProtectionInfo']['Cluster Servers']['clusters_vols'] = $final_server_vols;
				}	
			}
			elseif($flag == 3)
			{
				$scenario_details['Xen Servers']['xen_vols'] = $final_server_vols;
				$scenario_details['primaryProtectionInfo']['Xen Servers']['xen_vols'] = $final_server_vols;
			}
			elseif($flag == 4)
			{
				$scenario_details['Shared Devices']['shared_device_list'] = $final_server_vols;
				$scenario_details['primaryProtectionInfo']['Shared Devices']['shared_device_list'] = $final_server_vols;
			}
		}
		elseif($scenario_details['protectionType'] == 'DR PROTECTION' || $scenario_details['protectionType'] == 'BACKUP PROTECTION')
		{
			$server_vols = $scenario_details['otherVolumes'] ;
			$vols = explode(",",$server_vols);
			$src_vols = "";
			foreach($vols as $key1 => $value1)
			{
				if($value1 != $src_vol)
				{
					$src_vols[] = $value1;
				}
			}
			$final_server_vols = implode(",",$src_vols);
			$scenario_details['otherVolumes'] = $final_server_vols;
			if($scenario_details['primaryProtectionInfo'])
			{
				$scenario_details['primaryProtectionInfo']['otherVolumes'] = $final_server_vols;
			}	
		}	
		return $scenario_details;
	}
	
	private function updateBackupScenarioSession($recovery_scenario_details, $src_vol)
	{
		global $commonfunctions_obj, $logging_obj;
		if($recovery_scenario_details)
		{
			foreach($recovery_scenario_details as $scenario_id_tmp => $scenario_details)
			{
				$pair_count = 0;
				foreach($scenario_details['ruleid_arr'] as $key => $value)
				{
					$pairs = explode(" ",$key);
					$pairs1 = explode("!!--!!",$pairs[1]);
					// unsetting the deleted pair information from ruleId_arr session.
					if($src_vol == $pairs1[0])
					{
						unset($recovery_scenario_details[$scenario_id_tmp]['ruleid_arr'][$key]);
					}
					else
					{
						$pairs = explode("!!--!!",$key);
						$key1 = $pairs[0]."!!--!!".$pair_count;
						unset($recovery_scenario_details[$scenario_id_tmp]['ruleid_arr'][$key]);
						$recovery_scenario_details[$scenario_id_tmp]['ruleid_arr'][$key1] = $value;
						$pair_count++;
					}
				}
				
				$tmp = 1;
				$tmp_pairs = 0;
				for ($i=0; $i< $scenario_details['pair_count']; $i++)
				{
					$j = $i+1;
					$session_vol = explode("=",$scenario_details[$i]['pairs'.$j]);
					$tmp_value="";
					$recovery_type = $scenario_details['recoverytype'];
					
					// unsetting the deleted pair information for data validation & backup scenario for deleted pairs and reassigning the proper values in session.
					if($src_vol == $session_vol[1])
					{
						unset($recovery_scenario_details[$scenario_id_tmp][$i]);
					}
					elseif($recovery_type == 'Data Validation and Backup')
					{
						$tmp_value = $recovery_scenario_details[$scenario_id_tmp][$i];
						$tmp_value['hidVolumes'.$tmp] = $tmp_value['hidVolumes'.$j];
						$tmp_value['snapshot_drive'.$tmp] = $tmp_value['snapshot_drive'.$j];
						$tmp_value['pairs'.$tmp] = $tmp_value['pairs'.$j];
						$tmp_value['hidRuleId'.$tmp] = $tmp_value['hidRuleId'.$j];
						$tmp_value['hidmountpoint'.$tmp] = $tmp_value['hidmountpoint'.$j];
						$tmp_value['physical_drive'.$tmp] = $tmp_value['physical_drive'.$j];
						$tmp_value['virtual_drive'.$tmp] = $tmp_value['virtual_drive'.$j];
						$tmp_value['deviceName'.$tmp] = $tmp_value['deviceName'.$j];
						
						unset($recovery_scenario_details[$scenario_id_tmp][$i]);
						
						$recovery_scenario_details[$scenario_id_tmp][$tmp_pairs] = $tmp_value;
						if($tmp_pairs != $i)
						{
							unset($recovery_scenario_details[$scenario_id_tmp][$tmp_pairs]['hidVolumes'.$j]);
							unset($recovery_scenario_details[$scenario_id_tmp][$tmp_pairs]['snapshot_drive'.$j]);
							unset($recovery_scenario_details[$scenario_id_tmp][$tmp_pairs]['pairs'.$j]);
							unset($recovery_scenario_details[$scenario_id_tmp][$tmp_pairs]['hidRuleId'.$j]);
							unset($recovery_scenario_details[$scenario_id_tmp][$tmp_pairs]['hidmountpoint'.$j]);
							unset($recovery_scenario_details[$scenario_id_tmp][$tmp_pairs]['physical_drive'.$j]);
							unset($recovery_scenario_details[$scenario_id_tmp][$tmp_pairs]['virtual_drive'.$j]);
							unset($recovery_scenario_details[$scenario_id_tmp][$tmp_pairs]['deviceName'.$j]);
						}
						
						$tmp_pairs++;
					}
										
					if($scenario_details['source_volume'.$j] == $src_vol)
					{
						unset($recovery_scenario_details[$scenario_id_tmp]['source_volume'.$j]);
						unset($recovery_scenario_details[$scenario_id_tmp]['target_volume'.$j]);
						unset($recovery_scenario_details[$scenario_id_tmp]['process_server'.$j]);
						unset($recovery_scenario_details[$scenario_id_tmp]['rollback_mountpoint'.$j]);
						$recovery_scenario_details[$scenario_id_tmp]['pair_count'] = $pair_count;
					}
					else
					{
						$src_tmp = $recovery_scenario_details[$scenario_id_tmp]['source_volume'.$j];
						$trg_tmp = $recovery_scenario_details[$scenario_id_tmp]['target_volume'.$j];
						$ps_tmp = $recovery_scenario_details[$scenario_id_tmp]['process_server'.$j];
						$roll_mnt_tmp = $recovery_scenario_details[$scenario_id_tmp]['rollback_mountpoint'.$j];
					
						unset($recovery_scenario_details[$scenario_id_tmp]['source_volume'.$j]);
						unset($recovery_scenario_details[$scenario_id_tmp]['target_volume'.$j]);
						unset($recovery_scenario_details[$scenario_id_tmp]['process_server'.$j]);
						unset($recovery_scenario_details[$scenario_id_tmp]['rollback_mountpoint'.$j]);
						
						$recovery_scenario_details[$scenario_id_tmp]['source_volume'.$tmp] = $src_tmp;
						$recovery_scenario_details[$scenario_id_tmp]['target_volume'.$tmp] = $trg_tmp;
						if($recovery_type != 'Data Validation and Backup')
						{
							$recovery_scenario_details[$scenario_id_tmp]['process_server'.$tmp] = $ps_tmp;
							$recovery_scenario_details[$scenario_id_tmp]['rollback_mountpoint'.$tmp] = $roll_mnt_tmp;
						}	
						$tmp++;
					}
				}
				$commonfunctions_obj->serialise($recovery_scenario_details[$scenario_id_tmp], '', '', 2, $scenario_id_tmp);
				$logging_obj->my_error_handler("INFO"," updateBackupScenarioSession scenario_id_tmp :: $scenario_id_tmp recovery_scenario_details".print_r($recovery_scenario_details,true));
			}
		}
	}
	
	function vxstubUpdateRollbackPolicy($host_id,$policy_id,$policy_instance_id,$status,$log)
	{
		global $ROLLBACK_COMPLETED, $ROLLBACK_FAILED, $SNAPSHOT_FAILED, $SNAPSHOT_SUCESS, $SCENARIO_DELETE_PENDING, $ROLLBACK_INPROGRESS;
		global $logging_obj,$WEB_CLI_PATH, $commonfunctions_obj;
		$volume_obj = new VolumeProtection();
		//Return result to app agent "success"
		$result = 0;
		$policy_status = 1;
		$logging_obj->my_error_handler("INFO","vxstubUpdateRollbackPolicy :: host_id : $host_id \n policy_id::$policy_id \n policy_instance_id::$policy_instance_id \n  status::$status \n  status::$log");	
		
		$src_id_list = $src_data = array();
		$sql = "SELECT a.sourceId AS sourceId,a.planId AS planId FROM applicationScenario a,policy b WHERE a.planId = b.planId AND a.targetId = b.hostId AND b.policyId = ? AND b.policyName = 'Rollback' AND a.executionStatus = ?";
		$rs = $this->conn->sql_query($sql,array($policy_id,$ROLLBACK_INPROGRESS));
		$logging_obj->my_error_handler("INFO","vxstubUpdateRollbackPolicy :: sql : $sql \n params:: $policy_id,$ROLLBACK_INPROGRESS");
		if(is_array($rs)) 
		{
			foreach($rs as $k=>$v) 
			{
				$src_id_list[] = $v['sourceId'];
				$recovery_plan_id = $v['planId'];
			}
		}
		
		$mds_data_array = array();
		$this->mds_obj = new MDSErrorLogging();
		$mds_data_array["jobId"] = $policy_id;
		$mds_data_array["jobType"] = "Rollback";
		$mds_data_array["eventName"] = "Master Target";
		$mds_data_array["errorType"] = "ERROR";
		if(count($src_id_list)) 
		{
			if($log) 
			{
				$data = unserialize($log);
				//Failed
				if($status == 2) 
				{
					$policy_status = 2;
					//update applicationScenario
					$update_scenario_sql = "UPDATE applicationScenario SET executionStatus = ? WHERE planId = ?";
					$update_scenario_rs = $this->conn->sql_query($update_scenario_sql,array($ROLLBACK_FAILED,$recovery_plan_id));
					$logging_obj->my_error_handler("INFO","vxstubUpdateRollbackPolicy :: update_scenario_sql : $update_scenario_sql \n params:: $recovery_plan_id");
					
					$scenario_data = $this->conn->sql_get_array("SELECT scenarioId, sourceId FROM applicationScenario WHERE planId = ? AND scenarioType = 'Rollback'","scenarioId","sourceId", array($recovery_plan_id));		
					foreach($scenario_data as $scenario_id => $source_id)
					{
						if(!$data[3][$source_id][2]['Tag']) $src_id_arr[] = $source_id;
					}
					if(count($src_id_arr) > 0) $volume_obj->ResumeReplicationPairs($src_id_arr,$host_id, 1);
				}
				//Success
				if(($status == 1)) 
				{
					if((is_array($data) === TRUE) && (count($data[3]))) 
					{
						if(is_array($data[3]) && count($data[3])) $src_data = array_keys($data[3]);
						
						//check if agent has reported all the source hostIds
						$logging_obj->my_error_handler("INFO","vxstubUpdateRollbackPolicy :: src_id_list : ".print_r($src_id_list,true)."src_data : ".print_r($src_data,true));
						if(count($src_data) == count($src_id_list)) 
						{
							$plan_status = 'Rollback Completed';
							foreach($src_id_list as $id) 
							{
								$scenario_status = $ROLLBACK_COMPLETED;
								if($data[3][$id][2]['RecoveryStatus'] == 2) 
								{
									$scenario_status = $ROLLBACK_FAILED;
									$plan_status = 'Rollback Failed';
									$policy_status = 2;
								}
								
								//update applicationScenario								
								$update_scenario_sql = "UPDATE applicationScenario SET executionStatus = ? WHERE planId = ? AND sourceId = ?";
								$update_scenario_rs = $this->conn->sql_query($update_scenario_sql,array($scenario_status,$recovery_plan_id,$id));
								$logging_obj->my_error_handler("INFO","vxstubUpdateRollbackPolicy :: update_scenario_sql : $update_scenario_sql \n params:: $scenario_status,$recovery_plan_id");
								
								if($policy_status == 2 && !$data[3][$id][2]['Tag']) $volume_obj->ResumeReplicationPairs($id, $host_id, 1);
							}
						} 
						else 
						{
							$result = 1; //failed, as agent has not sent all the hostIds
						}
					}
					$mds_data_array["errorString"] = "Rollback completed successfully for policy: $policy_id, Hostid: $host_id, Policyinstanceid: $policy_instance_id, Log: $log, Source ids: ".print_r($src_id_list,true).", Result = $result \n";
					$this->mds_obj->saveMDSLogDetails($mds_data_array);
				}
			}
			
			if($result == 0) 
			{
				if($status == 2) $plan_status = 'Rollback Failed';
				//update applicationPlan
				$update_plan_sql = "UPDATE applicationPlan SET applicationPlanStatus=? WHERE planId=?";
				$update_plan_rs = $this->conn->sql_query($update_plan_sql,array($plan_status,$recovery_plan_id));
				$logging_obj->my_error_handler("INFO","vxstubUpdateRollbackPolicy :: update_plan_sql : $update_plan_sql \n params:: $plan_status,$recovery_plan_id");
					
				//update policy
				$update_policy_sql = "UPDATE policyInstance SET policyState=?,lastUpdatedTime=now(),executionLog=? WHERE policyId=? AND policyState IN(3,4)";
				$update_policy_rs = $this->conn->sql_query($update_policy_sql,array($policy_status,$log,$policy_id));
				$logging_obj->my_error_handler("INFO","vxstubUpdateRollbackPolicy :: update_policy_sql : $update_policy_sql \n params:: $policy_status,$log,$policy_id");
				
				$policy_duration = 0;
				$policy_data = $this->conn->sql_query("SELECT policyTimestamp, now() as currentTime, (UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(policyTimestamp)) as policyDuration FROM policy WHERE policyId = ? AND policyType = ?", array($policy_id, 48));
				foreach($policy_data as $policy_info)
				{
					$policy_start_time = $policy_info["policyTimestamp"];
					$policy_end_time = $policy_info["currentTime"];
					$policy_duration = $policy_info["policyDuration"];
				}
				
				$mds_log = '';
				if($status == 2 || ($policy_duration >= 7200))
				{
					$this->mds_error_obj  = new MDSErrors();
					$log_file_name = $WEB_CLI_PATH . $data[2]['LogFile'];
					$dependent_log_string = $this->mds_error_obj->GetErrorString("Rollback", array($policy_id));
					$mds_data_array["errorString"] = $dependent_log_string;
					if (file_exists($log_file_name)) 
					{					
                        $file_handle = $commonfunctions_obj->file_open_handle($log_file_name);
                        if(!$file_handle)
                        {
                            // return failure if file handle not available.
                            return 1;
                        }
						$mds_log = fread($file_handle, filesize($log_file_name));
                        fclose($file_handle);
						if($dependent_log_string) 
						{
							$mds_data_array["errorString"] .= "\n".$mds_log;
							$this->mds_obj->saveMDSLogDetails($mds_data_array);
							unlink($log_file_name);
						}	
					}
					else
					{						
						$mds_data_array["errorString"] .= "\nLog file for failover not found";
						$this->mds_obj->saveMDSLogDetails($mds_data_array);
					}
					
					# In case prepare target policy took more than 2 hours update the policy status to MDS
					if($policy_duration >= 7200)
					{
						$policy_status = ($status == 1) ? "Success" : "Failed";
						$mds_data_array["errorString"] .= "Rollback policy started at $policy_start_time completed at $policy_end_time. Duration of rollback policy is $policy_duration and the policy status is $policy_status with the log $log";
						$this->mds_obj->saveMDSLogDetails($mds_data_array);
					}
				}	
			}
		}
		$logging_obj->my_error_handler("INFO","vxstubUpdateRollbackPolicy :: result : $result");
		return $result;
	}
}
?>
