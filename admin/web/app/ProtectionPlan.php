<?php
class protectionPlan
{
	private $conn;
	public $scenarioId;
	public $planProperties;
	public $editMode;
	public $backupData;
	public $scenarioCount;
	function __construct($conn_obj=0)
    {
		if($conn_obj)
		{
			$this->conn = $conn_obj;
		}
    
	}
	
	public function setScenarioId($scenario_id)
    {
		$this->scenarioId = $scenario_id;
    }
		
	private function setConnectionObject()
	{
		global $conn_obj;
		
		if(!isset($this->conn) || !is_object($this->conn))
		{
			$this->conn = $conn_obj;
		}
	}	
	
	public function createPlan($plan_name,$flag,$batch_resync=0,$is_esx=0,$plan_details='',$application_plan_status = '', $target_data_palne=1)
	{
        global $CLOUD_PLAN, $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION, $CLOUD_PLAN_PHYSICAL_SOLUTION, $CLOUD_PLAN_HYPERV_SOLUTION;
		$this->setConnectionObject();
		$commonfunctions_obj = new CommonFunctions();
		$solutionType = 'HOST';
		$applicationPlanStatus = '';	
		if ($flag == 1 )
		{
			$planType = 'APPLICATION';
		}			
		elseif ($flag == 2 )
		{
			$planType = 'BULK';
		}
		elseif ($flag == 4 )
		{
			$planType = $CLOUD_PLAN;
			$solutionType = $CLOUD_PLAN_AZURE_SOLUTION;
			$applicationPlanStatus = 'Prepare Target Pending';
		}
        elseif ($flag == 5 )
		{
			$planType = $CLOUD_PLAN;
			$solutionType = $CLOUD_PLAN_VMWARE_SOLUTION;
			$applicationPlanStatus = 'Prepare Target Pending';
		}
        elseif ($flag == 6 ) //When we start support PHYSICAL for failback, this block helps
		{
			$planType = $CLOUD_PLAN;
			$solutionType = $CLOUD_PLAN_PHYSICAL_SOLUTION;
			$applicationPlanStatus = 'Prepare Target Pending';
		}
        elseif ($flag == 7 ) //When we start support HYPERV for failback, this block helps
		{
			$planType = $CLOUD_PLAN;
			$solutionType = $CLOUD_PLAN_HYPERV_SOLUTION;
			$applicationPlanStatus = 'Prepare Target Pending';
		}
		else
		{
			$planType = 'OTHER';
		}		
		
        if ($application_plan_status == "Active")
        {
            $applicationPlanStatus = "Active";
        }
        
		$plan_name = trim($plan_name);
		$plan_exists = $this->conn->sql_get_value('applicationPlan', 'planId',"planName='$plan_name'");
		if(!$plan_exists)
		{
			if(!$is_esx) 
            {
				$sql = "INSERT INTO
						applicationPlan 
						(planName, planType,planCreationTime,batchResync,solutionType,applicationPlanStatus,planDetails,TargetDataPlane)
					VALUES
						('$plan_name', '$planType',UNIX_TIMESTAMP(now()),'$batch_resync','$solutionType','$applicationPlanStatus', '$plan_details',$target_data_palne)";
			} 
            else 
            {
			$sql = "INSERT INTO
						applicationPlan 
						(planName, planType,planCreationTime,batchResync,solutionType,TargetDataPlane)
					VALUES
						('$plan_name', '$planType',UNIX_TIMESTAMP(now()),'$batch_resync','ESX',$target_data_palne)";
			}
            
			$rs = $this->conn->sql_query($sql);
			$plan_id = $this->conn->sql_insert_id();
			$result = $commonfunctions_obj->writeLog($_SESSION['username'],"$planType protection plan '$plan_name' is created");
		}
		else
		{
			$plan_id = 0; // to handle error case
		}
		
		return $plan_id;
	}
	
	/*
	tihs will updates the Solution type of applicationPlan table.
	*/
	public function updateSolutionType($plan_id,$solution_type)
	{		
		$this->setConnectionObject();
		$qry = "UPDATE 
					applicationPlan 
				SET 
					solutionType = '$solution_type'
				WHERE
					planId = '$plan_id'";
		
		$rs = $this->conn->sql_query($qry);
	}
	
	public function getPlanProperties()
	{	
		return $this->planProperties;
	}
	
	public function get_scenario_id()
	{
		return $this->scenarioId;
	}
	
	public function getPlanName($plan_id, $plan_type='')
	{		
		$this->setConnectionObject();
		$cond = '';
		if($plan_type) $cond = "AND planType = '$plan_type'";
		$plan_name = $this->conn->sql_get_value('applicationPlan', 'planName',"planId='$plan_id' $cond");

		return $plan_name;
	}	
    
    public function getScenarioIdByType($scenarioType)
	{		
		$this->setConnectionObject();
		$scenarioId = $this->conn->sql_get_value('applicationScenario', 'scenarioId',"scenarioType='$scenarioType'");

		return $scenarioId;
	}
	
	public function getPlanId($plan_nm){
		$this->setConnectionObject();
		$plan_id = $this->conn->sql_get_value('applicationPlan', 'planId',"planName='$plan_nm'");
		return $plan_id;
	}
	
	public function getPlanType($plan_id)
	{		
		$this->setConnectionObject();
		$plan_type = $this->conn->sql_get_value('applicationPlan', 'planType',"planId='$plan_id'");

		return $plan_type;
	}
	
	public function getPlanStatus($plan_id)
	{		
		$this->setConnectionObject();
		$plan_status = $this->conn->sql_get_value('applicationPlan', 'applicationPlanStatus',"planId='$plan_id'");
		return $plan_status;
	}
	
	public function getPlanBatchResync($plan_id)
	{		
		$this->setConnectionObject();
		$plan_batch_resync = $this->conn->sql_get_value('applicationPlan', 'batchResync',"planId='$plan_id'");

		return $plan_batch_resync;
	}
	
	public function getScenarioId($plan_id, $move_plan='')
	{		
		$this->setConnectionObject();
		$cond = "and scenarioType IN ('DR Protection')";
		if($move_plan) $cond = " and scenarioType IN ('Backup Protection','DR Protection','Profiler Protection')";
		$scenario_id = $this->conn->sql_get_value('applicationScenario', 'scenarioId',"planId='$plan_id' $cond");
		
		return $scenario_id;
	}
	
	private function getConnectionObject()
	{
		global $conn_obj;
		$this->conn = $conn_obj;
	}
	
	public function setPlanDetails($arr)
	{
		global $app_obj,$INACTIVE;
		$flag = 0;
		foreach($this->planProperties['pairInfo'] as $key=>$value)
		{
			$src_host_details = explode("=",$key);
		}
		if($this->planProperties['protectionType'] != $arr['protectionType'])
		{
			$this->unsetBakupProvisioningOptions();
			unset($this->planProperties['reversepairInfo']);
			unset($this->planProperties['reverseProtection']);
			unset($this->planProperties['pairInfo']);
			unset($this->planProperties['globalOption']);
			unset($this->planProperties['targetHostId']);
			unset($this->planProperties['targetIsClustered']);
		}
		$src_host_id = $arr['sourceHostId'];
		
		if($src_host_id != $src_host_details[0])
		{
			$this->unsetBakupProvisioningOptions();
			unset($this->planProperties['reversepairInfo']);
			unset($this->planProperties['reverseProtection']);
			unset($this->planProperties['pairInfo']);
			unset($this->planProperties['globalOption']);
			unset($this->planProperties['targetHostId']);
			unset($this->planProperties['targetIsClustered']);
			
			$scenario_id = $this->scenarioId;
			$this->planProperties['run_readiness'] = 1;
		}
		else
		{
			$this->planProperties['run_readiness'] = 0;
		}
		$session_other_vols = $this->planProperties['otherVolumes'];
		$other_vols = $arr['otherVolumes'];
		if($other_vols != $session_other_vols)
		{
			foreach($this->planProperties['pairInfo'] as $key=>$value)
			{
				unset($this->planProperties['pairInfo'][$key]['consistencyOptions']['option']);
			}
			$scenario_id = $this->scenarioId;
			$this->planProperties['run_readiness'] = 1;
		}
		else
		{
			$this->planProperties['run_readiness'] = 0;
		}
		
		foreach($arr as $key => $val)
		{
			$this->planProperties[$key] = $val;
		}
		$execution_status = $app_obj->getScenarioExecutionStatus($this->scenarioId);
		if($execution_status == $INACTIVE)
		{
			if( $this->planProperties['protectionType'] != $arr['protectionType'] ||
				$other_vols != $session_other_vols ||
				$src_host_id != $src_host_details[0])
			{
				$flag = 3;
			}
			$this->saveScenario($flag);
		}
	}
	
	
	public function unsetBakupProvisioningOptions()
	{		
		unset($this->planProperties['drDrill']);
		unset($this->planProperties['repositoryVol']);
		unset($this->planProperties['autoVolumes']);
		unset($this->planProperties['repositoryPath']);
		unset($this->planProperties['src_total_capacity']);
	}
		
	public function saveScenario($flag = '')
	{
		global $INACTIVE,$logging_obj, $CREATION_INCOMPLETE, $VALIDATION_SUCCESS, $EDIT_CONFIGURATION,$app_obj;
		$this->setConnectionObject();
	
		$logging_obj->my_error_handler("DEBUG","saveScenario called with flag : $flag \n");		
		$execution_status = $this->conn->sql_get_value('applicationScenario', 'executionStatus',"scenarioId='$this->scenarioId'");
		$execution_status_db = $execution_status;
		$src_volume_list = array();
		$tgt_volume_list = array();
		$retention_volume_list = array();
		$commonfunctions_obj = new CommonFunctions();
		$logging_obj->my_error_handler("DEBUG","execution_status : $execution_status \n");
		$isVolumeResized = $app_obj->isScenarioResized($this->scenarioId);
		if($execution_status == $VALIDATION_SUCCESS || $execution_status == $EDIT_CONFIGURATION)
		{
			$execution_status = $EDIT_CONFIGURATION;
		}
		else
		{
			if($flag == 1)
			{
				$execution_status = $INACTIVE;
			}
			elseif($flag == 2)
			{
				$sql = "SELECT
							executionStatus
						FROM
							applicationScenario
						WHERE
							scenarioId='$this->scenarioId'";
				$rs = $this->conn->sql_query($sql);
				$row = $this->conn->sql_fetch_object($rs);
				$execution_status = $row->executionStatus;
			}
			elseif($flag == 3)
			{
				$execution_status = $CREATION_INCOMPLETE;
			}
			elseif($flag == 4)
			{
				$app_obj->updatePartialProtection($this->scenarioId,2);
			}
			elseif($flag == 5)
			{
				$execution_status = $VALIDATION_SUCCESS;
			}
			else
			{
				$sql = "SELECT
							executionStatus
						FROM
							applicationScenario
						WHERE
							scenarioId='$this->scenarioId'";
				$rs = $this->conn->sql_query($sql);
				$row = $this->conn->sql_fetch_object($rs);
				$execution_status = $row->executionStatus;
			}
		}

		$cdata = serialize($this->planProperties);
		$plan_id = $this->planProperties['planId'];
		$plan_name = $this->planProperties['planName'];
		$isPrimary = $this->planProperties['isPrimary'];
		$scenario_type = $this->planProperties['protectionType'];
		$scenarioDescription = $this->planProperties['scenarioDescription'];
		$currentStep = $this->planProperties['currentStep'];
		$reverseProtection = $this->planProperties['reverseProtection'];

		if($reverseProtection == 1)
		{
			$reversereplicationoption = 1;
			$retention_reverse_volume_list = array();
			foreach($this->planProperties['reversepairInfo'] as $key=>$value)
			{			
				foreach($value['pairDetails'] as $vol=>$data)
				{
					if($data['retentionPathVolume'])
					$retention_reverse_volume_list[] = $this->conn->sql_escape_string($data['retentionPathVolume']);
				}
			}
			$retention_reverse_volume_list = array_unique($retention_reverse_volume_list);
			$retention_reverse_volume_list = implode(",",$retention_reverse_volume_list);
		}
		else
		{
			$reversereplicationoption = '';
		}
		$app_type = $this->planProperties['applicationType'];			
		$app_version = $this->planProperties['applicationVersion'];	
		$batch_resync = $this->planProperties['globalOption']['batchResync'];

		$logging_obj->my_error_handler("DEBUG","planType ::".$this->planProperties['planType']."\n");

		if ( $this->planProperties['planType'] != "BULK")
		{
			$sourceId = $this->planProperties['sourceHostId'];
			$logging_obj->my_error_handler("DEBUG","sourceId_APPLICATION :: $sourceId \n");
		}
		else
		{
			$app_type = "BULK";
		}
		
		if($this->planProperties['protectionType'])
		{
			$scenario_type = $this->planProperties['protectionType'];
		}
		
		foreach($this->planProperties['pairInfo'] as $key=>$value)
		{			
			$key_arr = explode("=",$key);
			$app_type_arr = explode("_",$key_arr[2]);
			$sourceId = $value['sourceId'];
			
			$srcAPPId = $value['srcAPPId'];
			$targetId = $value['targetId'];
			$targetAPPId = $value['targetAPPId'];
			foreach($value['pairDetails'] as $vol=>$data)
			{
				$src_volume_list[] = $this->conn->sql_escape_string($data['srcVol']);
				$tgt_volume_list[] = $this->conn->sql_escape_string($data['trgVol']);
				if($data['retentionPathVolume'])
				$retention_volume_list[] = $this->conn->sql_escape_string($data['retentionPathVolume']);
			}
		}
		if($this->planProperties['backupProvisioning'] == 'auto')
		{
			$sql = "SELECT
						policyId
					FROM
						policy
					WHERE
						scenarioId='$this->scenarioId'
					AND
						policyType='12'";
			$rs = $this->conn->sql_query($sql);
			$row = $this->conn->sql_fetch_object($rs);
			$policyId = $row->policyId;
			
			$sql = "SELECT
						policyState
					FROM
						policyInstance
					WHERE
						policyId='$policyId'";
			$rs = $this->conn->sql_query($sql);
			$row = $this->conn->sql_fetch_object($rs);
			$policyState = $row->policyState;
			if($policyState == 1) $tgt_volume_str = implode(",",$tgt_volume_list);
		}
		else
		{
			$tgt_volume_str = implode(",",$tgt_volume_list);
		}
		$src_volume_str = implode(",",$src_volume_list);
		
		$retention_volume_list = array_unique($retention_volume_list);
		if(!is_null($retention_volume_list))
		$retention_volume_str = implode(",",$retention_volume_list);
		else
		$retention_volume_str = '';
		
		if($execution_status == $INACTIVE || $execution_status == $CREATION_INCOMPLETE)
		{
			$cond = " sourceId = '$sourceId',targetId = '$targetId', ";
		}
		
		if($execution_status_db != $VALIDATION_SUCCESS)
		{
			$cond1 = " executionStatus = '$execution_status', ";
		}
		
		$logging_obj->my_error_handler("DEBUG","src_volume_str : $src_volume_str \n");
		$logging_obj->my_error_handler("DEBUG","tgt_volume_str : $tgt_volume_str \n");
		$logging_obj->my_error_handler("DEBUG","retention_volume_str : $retention_volume_str \n");
		
		$str = "<plan><header><parameters>";
		$str.= "<param name='name'>".$this->planProperties['planName']."</param>";
		$str.= "<param name='type'>Protection Plan</param>";
		$str.= "<param name='version'>1.1</param>";
		$str.= "</parameters></header>";
		$str.= "<data><![CDATA[";
		$str.= $cdata;
		$str.= "]]></data></plan>";
		$str = $this->conn->sql_escape_string($str);

		if(!$this->scenarioId)
		{			
			$sql = "INSERT INTO
					applicationScenario
					(
						scenarioType,
						scenarioDescription,
						scenarioVersion,
						scenarioCreationDate,
						scenarioDetails,
						planId,
						executionStatus,
						currentStep,
						applicationType,
						applicationVersion,
						srcInstanceId,
						trgInstanceId,
						sourceId,
						targetId,
						isPrimary,
						resyncLimit,
						sourceVolumes,
						targetVolumes,
						retentionVolumes,
						reverseReplicationOption,
						reverseRetentionVolumes,
						isRetentionReuse
					)
					VALUES
					(
						'$scenario_type',
						'$scenarioDescription',
						'1.1',
						now(),
						'$str',
						'$plan_id',
						'$execution_status',
						'$currentStep',
						'$app_type',
						'$app_version',
						'$srcAPPId',
						'$targetAPPId',
						'$sourceId',
						'$targetId',
						'$isPrimary',
						'$batch_resync',
						'$src_volume_str',
						'$tgt_volume_str',
						'$retention_volume_str',
						'$reversereplicationoption',
						'$retention_reverse_volume_list',
						'0'
					)";
			$logging_obj->my_error_handler("DEBUG","insert_sql : $sql\n");		
			$this->conn->sql_query($sql);
			$scenario_id = $this->conn->sql_insert_id();	
		
			$this->setPlanDetails(array('scenario_id' => $scenario_id,
			                            'protectionType' => $scenario_type,
										'sourceHostId' => $sourceId));
		
			$this->setScenarioId($scenario_id);
			$_SESSION['scenario_id='.$scenario_id]['protection_plan_obj'] = $_SESSION['scenario_id=0']['protection_plan_obj'];
			unset($_SESSION['scenario_id=0']);
			
			$planName = $app_obj->getPlanName($plan_id);
			$source_host_Id = $_POST['src_host_id'];
			$target_host_Id = $_POST['target_host_id'];
			
			if($this->planProperties['planType'] == "BULK")
			{
				$srchost_name = $app_obj->getServerNameForBulk($source_host_Id);
				$tgthost_name = $app_obj->getServerNameForBulk($target_host_Id);
			}
			else
			{
				$srcInstanceId = $this->planProperties['srcAppId'];
				$tgt_is_cluster = $_POST['tgt_is_cluster'];
				if($tgt_is_cluster == 1)
				{
					$tgt_evs = $_POST['tgt_evs'];
					$app_ids = explode("##",$tgt_evs);
					$trgInstanceId = $app_ids[0];
				}
				else
				{
					$trgInstanceId = '';
				}
				$srchost_name = $app_obj->getServerName($sourceId,$srcInstanceId);
				$tgthost_name = $app_obj->getServerName($target_host_Id,$trgInstanceId);
			}
			$commonfunctions_obj->writeLog($_SESSION['username'],"$scenario_type Scenario ( $srchost_name -> $tgthost_name ) for $planName plan is created");
		}
		else
		{
			$sql = "UPDATE 
						applicationScenario
					SET
						scenarioType = '$scenario_type',
						scenarioDescription = '$scenarioDescription',
						scenarioVersion = '1.1',
						scenarioCreationDate = now(),
						scenarioDetails = '$str',
						planId = '$plan_id',
						$cond1
						currentStep = '$currentStep',
						applicationType = '$app_type',
						applicationVersion = '$app_version',
						srcInstanceId = '$srcAPPId',
						trgInstanceId = '$targetAPPId',
						$cond
						resyncLimit = '$batch_resync',
						sourceVolumes = '$src_volume_str',					
						targetVolumes = '$tgt_volume_str',
						retentionVolumes = '$retention_volume_str',
						reverseReplicationOption = '$reversereplicationoption',
						reverseRetentionVolumes = '$retention_reverse_volume_list'
					WHERE
						scenarioId='$this->scenarioId'";	
			$logging_obj->my_error_handler("DEBUG","update_sql : $sql\n");		
			$this->conn->sql_query($sql);
			
			$sql = "SELECT
						executionStatus
					FROM
						applicationScenario
					WHERE
						scenarioId='$this->scenarioId'";
			
			$rs = $this->conn->sql_query($sql);
			$row = $this->conn->sql_fetch_object($rs);
			$execution_status = $row->executionStatus;
		
			if($execution_status != $CREATION_INCOMPLETE && $execution_status != '0')
			{
				$auditlog_string = $app_obj->getAuditLogStr($this->scenarioId);
				$commonfunctions_obj->writeLog($_SESSION['username'],"$auditlog_string is updated");
			}			
		}

	}
	
	public function getScenarioDetails($flag = '', $xmlToArr=true)
	{
		$commonfunctions_obj = new CommonFunctions();
		$this->setConnectionObject();
		$sql = "SELECT
		            appsc.scenarioId,
					appsc.scenarioDetails,
					appsc.currentStep,
					appsc.executionStatus,
					appsc.protectionDirection,
					appsc.targetId,
					appsc.sourceId,
					appsc.sourceVolumes,
					appsc.targetVolumes,
					appsc.isModified,
					appsc.scenarioType,
                    ap.solutionType
				FROM
					applicationScenario appsc,
                    applicationPlan ap
				WHERE
					appsc.scenarioId='$this->scenarioId'
                AND
                    appsc.planId = ap.planId";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);

		$xml_data = $row->scenarioDetails;
		
		if($xmlToArr)
		{
			$cdata = $commonfunctions_obj->getArrayFromXML($xml_data);
			$plan_properties = unserialize($cdata['plan']['data']['value']);
			if($flag != 1) $this->planProperties = $plan_properties;
		}
		else
		{
			$plan_properties = unserialize($xml_data);
		}
		
		$scenario_details['currentStep'] = $row->currentStep;
		$scenario_details['executionStatus'] = $row->executionStatus;
		$scenario_details['protectionDirection'] = $row->protectionDirection;
		$scenario_details['session_array'] = $plan_properties;
		$scenario_details['target_id'] = $row->targetId;
		$scenario_details['sourceId'] = $row->sourceId;
		$scenario_details['sourceVolumes'] = $row->sourceVolumes;
		$scenario_details['targetVolumes'] = $row->targetVolumes;
		$scenario_details['scenarioId'] = $row->scenarioId;
		$scenario_details['isModified'] = $row->isModified;
		$scenario_details['scenarioType'] = $row->scenarioType;
		$scenario_details['solutionType'] = $row->solutionType;
		
		return $scenario_details;
	}
	
	public function insertProtectionPlan($plan_properties=array(), $scenario_id=0)
	{
		global $retention_obj, $VALIDATION_SUCCESS;
		global $commonfunctions_obj;
		global $logging_obj;
		
		$logging_obj->my_error_handler("DEBUG","Inside insertProtectionPlan");
		$this->getConnectionObject();
				
		$scenario_details = $this->getScenarioDetails();
		
		$plan_properties = $this->getPlanProperties();
		$scenario_id = $this->scenarioId;
		
		$logging_obj->my_error_handler("INFO",array("PlanProperties"=>$plan_properties), Log::BOTH);
		$plan_id = $plan_properties['planId'];
		$plan_name = $plan_properties['planName'];
		$app_type = $plan_properties['applicationType'];
		$paln_type = $plan_properties['planType'];
		
		
		$solution_type = $plan_properties['solutionType'];
		$src_oracle_database_instance_id = $plan_properties['srcOracleInstanceId'];
		$hidMediaret = 1;
		$volumegroup = '';
		$txtlocmeta = "ret_log_meta_info";
		$timeRecFlag = '';
		$hidTime = '';
		$change_by_time_day = '';
		$change_by_time_hr = '';
		$txtlocdir_move = '';
		$policy_type = 2;
		$log_policy_rad = '';
		$hdrive1 = '';
		$htimelapse = 0;
		$sparse_enable = 0;
		$hfastresync = '';
		$hcompenable = '';
		$hsecure_transport_src = '';
		$hsecure_transport = '';
		$batch_resync= '';
		$rpo_threshold = '';
		$diff_threshold = '';
		$resync_threshold = '';
		$batch_resync_val = 1; 
		if(isset($plan_properties['globalOption']['secureSrctoCX']))
		{
			$hfastresync = $plan_properties['globalOption']['syncOptions']; 
			$hcompenable = $plan_properties['globalOption']['compression'];
			$hsecure_transport_src = $plan_properties['globalOption']['secureSrctoCX']; 
			$hsecure_transport =  $plan_properties['globalOption']['secureCXtotrg'];
			$batch_resync = $plan_properties['globalOption']['batchResync'];
			$rpo_threshold = $plan_properties['globalOption']['rpo'];
			$diff_threshold = $plan_properties['globalOption']['diffThreshold'] * 1024 * 1024;
			$resync_threshold = $plan_properties['globalOption']['resynThreshold'] * 1024 * 1024;
			if(!$batch_resync)
			{
				$batch_resync = 0;
				$batch_resync_val = 0;
			}
		}
		
		$hwindowstarthours = '';
		$hwindowstartminutes = '';
		$hwindowstophours = '';
		$hwindowstopminutes = '';
		
		if(isset($plan_properties['globalOption']['autoResync']))
		{
			$hwindowstarthours =  $plan_properties['globalOption']['autoResync']['start_hr'];
			$hwindowstartminutes =  $plan_properties['globalOption']['autoResync']['start_min'];
			$hwindowstophours =  $plan_properties['globalOption']['autoResync']['stop_hr'];
			$hwindowstopminutes =  $plan_properties['globalOption']['autoResync']['stop_min'];
			$htimelapse = $plan_properties['globalOption']['autoResync']['wait_min']*60;
		}
        $hsynccopy = $plan_properties['globalOption']['syncCopyOptions'];

		$txtdiskthreshold =  80;
		$logExceedPolicy =  0;
		$total_pairs = 0;
		
		$protection_direction = trim($scenario_details['protectionDirection']);
		if($protection_direction == 'reverse')
		{
			$source_id = $scenario_details['target_id'];
			$target_id = $scenario_details['sourceId'];
			$pair_info = $plan_properties['reversepairInfo'];
		}
		else
		{
			$source_id = $scenario_details['sourceId'];
			$target_id = $scenario_details['target_id'];
			$pair_info = $plan_properties['pairInfo'];
		}
				
		
		foreach($pair_info as $key=>$data)
		{
			$total_pairs = $total_pairs+count($data['pairDetails']);
		}
		
		$source_id = explode(",",$source_id);
		$id = $source_id[0];
		$target_id = explode(",",$target_id);
		$destId = $target_id[0];
		$src_host_id_list = implode("','",$source_id);
		
		$inner_loop = 1;
		$execution_state = 1;
		foreach($pair_info as $key=>$data)
		{
			if($batch_resync)
			{
				if($inner_loop > $batch_resync)
				{
					$batch_resync_val++;
					$inner_loop = 1;
					$execution_state = 2;
				}
			}
			
			if ($solution_type == 'PRISM')
			{
				if ($protection_direction == 'reverse')
				{
					$destId = $data['targetId'];
				}
			}		
			$src_app_ids = $data['srcAPPId'];
			foreach($data['pairDetails'] as $vol=>$details)
			{
				$consistency_options = $data['consistencyOptions'];
				$configuration_options = $data['configurationChange'];
				$retention_options = $data['retentionOptions'];
				if($batch_resync)
				{
					if($inner_loop > $batch_resync)
					{
						$batch_resync_val++;
						$inner_loop = 1;
						$execution_state = 2;
					}
				}
				
				
				if ($solution_type == 'PRISM' || $solution_type == 'ARRAY')
				{
					$deviceName = "/dev/mapper/".$details['prismSpecificInfo']['phyLunId'];
				}
				else
				{
					$deviceName = $details['srcVol'];
				}	

				
				$retention_dir = $details['retentionPath'];
				
				$previous_ret_path = $details['previousRetentionPath'];
				
				$hdrive2 = $details['retentionPathVolume'];
				$logDeviceName = $details['retentionPathVolume'];
				 if($plan_properties['arrayPillar']==1)
				 {
					 $process_server =  $data['sourceId'];
					 #$this->usedAIForTargetPorts($destId);
					 
					 $destDeviceName = "/dev/mapper/".$details['trgVol'];
				 }
				 else
				 {
					$process_server =  $details['processServerid'];
					$destDeviceName = $details['trgVol'];
				 }
				$hid_ps_natip_source =  $details['srcNATIpflag'];
				$hid_ps_natip_target =  $details['trgNATIPflag'];
				
				if ($solution_type == 'PRISM' || $solution_type == 'ARRAY')
				{
					$lun_process_server_info = $details['prismSpecificInfo']['phyLunId'];
				}
				else
				{
					$lun_process_server_info = '';
				}
				
				if(isset($retention_options['space_based']) && isset($retention_options['time_based_flag']))
				{
					$policy_type = 2;
				}
				elseif(isset($retention_options['time_based_flag']))
				{
					$policy_type = 1;
				}
				elseif(isset($retention_options['space_based']))
				{
					$policy_type = 0;
				}
				if($retention_options['SparseEnable'])
				{
					$sparse_enable = $retention_options['SparseEnable'];
				}
				
				
				
				if(isset($retention_options['space_based']))
				{
					switch($retention_options['selUnits'])
					{
						case 0:
							$hidUnit = $retention_options['change_by_size']*1024*1024;
							break;
						case 1:
							$hidUnit = $retention_options['change_by_size']*1024*1024*1024;
							break;
						case 2:
							$hidUnit = $retention_options['change_by_size']*1024*1024*1024*1024;
							break;
						default:
							$hidUnit = $retention_options['selUnits'];
					}
				}
				
				
				$ret_arr['dest'] = $destId.','.$destDeviceName;
				$ret_arr['id'] = $id;
				$ret_arr['deviceName'] = $deviceName;
				$ret_arr['select_resync'] = $hfastresync;
                $ret_arr['syncCopy'] = $hsynccopy;
				$ret_arr['hidMediaret'] = $hidMediaret;
				$ret_arr['hcompenable'] = $hcompenable;
				$ret_arr['volumegroup'] = $volumegroup;
				$ret_arr['hsecure_transport_src'] = $hsecure_transport_src;
				$ret_arr['hsecure_transport'] = $hsecure_transport;
				$ret_arr['log_policy_rad'] = $log_policy_rad;
				$ret_arr['hidUnit'] = $hidUnit;
				$ret_arr['txtlocmeta'] = $txtlocmeta;
				$ret_arr['hdrive1'] = $hdrive1; 
				$ret_arr['hdrive2'] = $hdrive2; 
				$ret_arr['timeRecFlag'] = $timeRecFlag; 
				// Fix for bug #13347
				$ret_arr['hidTime'] = "" ;
				$ret_arr['change_by_time_day'] = $change_by_time_day; 
				$ret_arr['change_by_time_hr'] = $change_by_time_hr;
				$ret_arr['txtlocdir_move'] = $txtlocdir_move;
				$ret_arr['txtdiskthreshold'] = $retention_options['txtdiskthreshold'];
				$ret_arr['htimelapse'] = $htimelapse;
				$ret_arr['hwindowstarthours'] = $hwindowstarthours;
				$ret_arr['hwindowstartminutes'] = $hwindowstartminutes;
				$ret_arr['hwindowstophours'] = $hwindowstophours;
				$ret_arr['hwindowstopminutes'] = $hwindowstopminutes;
				$ret_arr['logExceedPolicy'] = $retention_options['logExceedPolicy'];
				$ret_arr['lun_process_server_info'] = $lun_process_server_info;
				$ret_arr['process_server'] = $process_server;
				$ret_arr['hid_ps_natip_source'] = $hid_ps_natip_source;
				$ret_arr['hid_ps_natip_target'] = $hid_ps_natip_target;
				$ret_arr['hid_sparse_enable'] = $sparse_enable;
		
				$ret_arr['logDeviceName'] = $logDeviceName;
				$ret_arr['policy_type'] = $policy_type;
				$ret_arr['retention_dir'] = $retention_dir;
				
				$ret_arr['batch_resync_val'] = $batch_resync_val;
				$ret_arr['execution_state'] = $execution_state;
				$ret_arr['scenario_id'] = $scenario_id;
				$ret_arr['plan_id'] = $plan_id;
				$ret_arr['app_type'] = $app_type;
				
				
				$ret_arr['hid_retention_text'] = $retention_options['hid_retention_text'];
				$ret_arr['hid_retention_sel'] = $retention_options['hid_retention_sel'];
				$ret_arr['time_based'] = $retention_options['time_based'];
				$ret_arr['time_based_flag'] = $retention_options['time_based_flag'];
				$ret_arr['space_based'] = $retention_options['space_based'];
				
				$ret_arr['previous_ret_path'] = $previous_ret_path;
				$ret_arr['plan_type'] = $paln_type;
				$ret_arr['src_oracle_database_instance_id'] = $src_oracle_database_instance_id;
				$ret_arr['solution_type'] = $solution_type;
				$ret_arr['srcCapacity'] = $data['pairDetails'][$vol]['srcCapacity'];
				
				
				$ret_arr['prismSpecificInfo'] = $details['prismSpecificInfo'];
				$ret_arr['arrayPillar'] = $plan_properties['arrayPillar'];
				$ret_arr['srcLunId'] = $details['srcVol'];
				$ret_arr['trgLunId'] = $details['trgVol'];
 
				
				if($retention_options['time_based_flag'])
				{
					
					for($i=1;$i<=5;$i++)
					{
						$str1 = 'hid_application'.($i-1);
						$str2 = 'hid_bookmark'.($i-1);
						$str3 = 'hid_userDefined'.($i-1);
						$str4 = 'hid_granularity'.$i;
						$str5 = 'hid_granularity_range'.$i;
						$str6 = 'hid_seltime_range'.$i;
						$str7 = 'hid_seltime'.$i;
						$str8 = 'hid_add_remove'.$i;
						$str9 = 'seltime_range'.$i;
						
						if($i == 1)
						{
							$ret_arr[$str1] = 'all';
							$ret_arr[$str2] = 0;
							$ret_arr[$str3] = "";
						}
						else
						{
							$ret_arr[$str1] = $retention_options[$str1];
							$ret_arr[$str2] = $retention_options[$str2];
							$ret_arr[$str3] = $retention_options[$str3];
						}
						$ret_arr[$str4] = $retention_options[$str4];
						$ret_arr[$str5] = $retention_options[$str5];
						$ret_arr[$str6] = $retention_options[$str6];
						$ret_arr[$str7] = $retention_options[$str7];
						$ret_arr[$str8] = $retention_options[$str8];
						$ret_arr[$str9] = $retention_options[$str9];
					}									
				}
				if($plan_properties['arrayPillar']==1)
				{
					// Specific to Pillar for Rajeev Demo
					#$this->insertApplicationInfo($process_server);
				}
				
				$volume_obj = new VolumeProtection();
				
				$logging_obj->my_error_handler("DEBUG","Pair doesn't exist: $id, $deviceName, $destId, $destDeviceName");
				$volume_obj->configure_mediaRetention($ret_arr);
													
				$inner_loop++;
			}
		}
	}	
	
	public function getProtectionDirection($protection_scenario_id)
	{
		$this->setConnectionObject();
		$protection_direction = $this->conn->sql_get_value('applicationScenario', 'protectionDirection',"scenarioId = '$protection_scenario_id'");

		return $protection_direction;
	}
	
	public function insertCloudProtectionPlan($scenario_id)
	{
		global $retention_obj, $VALIDATION_SUCCESS,$PREPARE_TARGET_DONE;
		global $commonfunctions_obj;
		global $logging_obj;
		
		$logging_obj->my_error_handler("DEBUG","Inside insertProtectionPlan");
		$this->getConnectionObject();
		
		$sql = "SELECT 
					planId,
					scenarioDetails
				FROM
					applicationScenario
				WHERE
					scenarioId = '$scenario_id' AND
					executionStatus = '$PREPARE_TARGET_DONE'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$plan_properties = unserialize($row->scenarioDetails);
		
		$logging_obj->my_error_handler("DEBUG","PLAN PROPERTIES::".print_r($plan_properties,TRUE));
		$plan_id = $row->planId;
		$plan_name = $plan_properties['planName'];
		$app_type = $plan_properties['applicationType'];
		$paln_type = $plan_properties['planType'];
		
		
		$solution_type = $plan_properties['solutionType'];
		$src_oracle_database_instance_id = $plan_properties['srcOracleInstanceId'];
		$hidMediaret = 1;
		$volumegroup = '';
		$txtlocmeta = "ret_log_meta_info";
		$timeRecFlag = '';
		$hidTime = '';
		$change_by_time_day = '';
		$change_by_time_hr = '';
		$txtlocdir_move = '';
		$policy_type = 2;
		$log_policy_rad = '';
		$hdrive1 = '';
		$htimelapse = 0;
		$sparse_enable = 0;
		$hfastresync = '';
		$hcompenable = '';
		$hsecure_transport_src = '';
		$hsecure_transport = '';
		$batch_resync= '';
		$rpo_threshold = '';
		$diff_threshold = '';
		$resync_threshold = '';
		$batch_resync_val = 1; 
		if(isset($plan_properties['globalOption']['secureSrctoCX']))
		{
			$hfastresync = $plan_properties['globalOption']['syncOptions']; 
			$hcompenable = $plan_properties['globalOption']['compression'];
			$hsecure_transport_src = $plan_properties['globalOption']['secureSrctoCX']; 
			$hsecure_transport =  $plan_properties['globalOption']['secureCXtotrg'];
			$batch_resync = $plan_properties['globalOption']['batchResync'];
			$rpo_threshold = $plan_properties['globalOption']['rpo'];
			$diff_threshold = $plan_properties['globalOption']['diffThreshold'] * 1024 * 1024;
			$resync_threshold = $plan_properties['globalOption']['resynThreshold'] * 1024 * 1024;
			if(!$batch_resync)
			{
				$batch_resync = 0;
				$batch_resync_val = 0;
			}
		}
		
		$hwindowstarthours = '';
		$hwindowstartminutes = '';
		$hwindowstophours = '';
		$hwindowstopminutes = '';
		
		if(isset($plan_properties['globalOption']['autoResync']))
		{
			$hwindowstarthours =  $plan_properties['globalOption']['autoResync']['start_hr'];
			$hwindowstartminutes =  $plan_properties['globalOption']['autoResync']['start_min'];
			$hwindowstophours =  $plan_properties['globalOption']['autoResync']['stop_hr'];
			$hwindowstopminutes =  $plan_properties['globalOption']['autoResync']['stop_min'];
			$htimelapse = $plan_properties['globalOption']['autoResync']['wait_min']*60;
		}
        $hsynccopy = $plan_properties['globalOption']['syncCopyOptions'];
        $use_cfs = $plan_properties['globalOption']['use_cfs'];

		$txtdiskthreshold =  80;
		$logExceedPolicy =  0;
		$total_pairs = 0;
		
		$sql = "SELECT
					sourceId,
					targetId,
					protectionDirection
				FROM
					applicationScenario
				WHERE
					scenarioId='$scenario_id'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$protection_direction = $row->protectionDirection;
		
		if($protection_direction == 'reverse')
		{
			$source_id = $row->targetId;
			$target_id = $row->sourceId;
			$pair_info = $plan_properties['reversepairInfo'];
		}
		else
		{
			$source_id = $row->sourceId;
			$target_id = $row->targetId;
			$pair_info = $plan_properties['pairInfo'];
		}
				
		
		foreach($pair_info as $key=>$data)
		{
			$total_pairs = $total_pairs+count($data['pairDetails']);
		}
		
		$source_id = explode(",",$source_id);
		$id = $source_id[0];
		$target_id = explode(",",$target_id);
		$destId = $target_id[0];
		$src_host_id_list = implode("','",$source_id);
		
		$inner_loop = 1;
		$execution_state = 1;
		foreach($pair_info as $key=>$data)
		{
			if($batch_resync)
			{
				if($inner_loop > $batch_resync)
				{
					$batch_resync_val++;
					$inner_loop = 1;
					$execution_state = 2;
				}
			}
			
			//$id = $data['sourceId'];
			if ($solution_type == 'PRISM')
			{
				if ($protection_direction == 'reverse')
				{
					$destId = $data['targetId'];
				}
			}		
			$src_app_ids = $data['srcAPPId'];
			foreach($data['pairDetails'] as $vol=>$details)
			{
				$consistency_options = $data['consistencyOptions'];
				$configuration_options = $data['configurationChange'];
				$retention_options = $data['retentionOptions'];
				if($batch_resync)
				{
					if($inner_loop > $batch_resync)
					{
						$batch_resync_val++;
						$inner_loop = 1;
						$execution_state = 2;
					}
				}
				
				
				if ($solution_type == 'PRISM' || $solution_type == 'ARRAY')
				{
					$deviceName = "/dev/mapper/".$details['prismSpecificInfo']['phyLunId'];
				}
				else
				{
					$deviceName = $details['srcVol'];
				}	

				
				$retention_dir = $details['retentionPath'];
				
				$previous_ret_path = $details['previousRetentionPath'];
				
				$hdrive2 = $details['retentionPathVolume'];
				$logDeviceName = $details['retentionPathVolume'];
				 if($plan_properties['arrayPillar']==1)
				 {
					 $process_server =  $data['sourceId'];
					 #$this->usedAIForTargetPorts($destId);
					 $destDeviceName = "/dev/mapper/".$details['trgVol'];
				 }
				 else
				 {
					$process_server =  $details['processServerid'];
					$destDeviceName = $details['trgVol'];
				 }
				$hid_ps_natip_source =  $details['srcNATIpflag'];
				$hid_ps_natip_target =  $details['trgNATIPflag'];
				
				if ($solution_type == 'PRISM' || $solution_type == 'ARRAY')
				{
					$lun_process_server_info = $details['prismSpecificInfo']['phyLunId'];
				}
				else
				{
					$lun_process_server_info = '';
				}
				
				if(isset($retention_options['space_based']) && isset($retention_options['time_based_flag']))
				{
					$policy_type = 2;
				}
				elseif(isset($retention_options['time_based_flag']))
				{
					$policy_type = 1;
				}
				elseif(isset($retention_options['space_based']))
				{
					$policy_type = 0;
				}
				if($retention_options['SparseEnable'])
				{
					$sparse_enable = $retention_options['SparseEnable'];
				}
				
				
				
				if(isset($retention_options['space_based']))
				{
					switch($retention_options['selUnits'])
					{
						case 0:
							$hidUnit = $retention_options['change_by_size']*1024*1024;
							break;
						case 1:
							$hidUnit = $retention_options['change_by_size']*1024*1024*1024;
							break;
						case 2:
							$hidUnit = $retention_options['change_by_size']*1024*1024*1024*1024;
							break;
						default:
							$hidUnit = $retention_options['selUnits'];
					}
				}
				
				
				$ret_arr['dest'] = $destId.','.$destDeviceName;
				$ret_arr['id'] = $id;
				$ret_arr['deviceName'] = $deviceName;
				$ret_arr['select_resync'] = $hfastresync;
                $ret_arr['syncCopy'] = $hsynccopy;
                if ($data['retentionOptions'])
                {
                    $ret_arr['hidMediaret'] = $hidMediaret;
                }
                else
                {
                      $ret_arr['hidMediaret'] = 0;
                }
                
				$ret_arr['hcompenable'] = $hcompenable;
				$ret_arr['volumegroup'] = $volumegroup;
				$ret_arr['hsecure_transport_src'] = $hsecure_transport_src;
				$ret_arr['hsecure_transport'] = $hsecure_transport;
				$ret_arr['log_policy_rad'] = $log_policy_rad;
				$ret_arr['hidUnit'] = $hidUnit;
				$ret_arr['txtlocmeta'] = $txtlocmeta;
				$ret_arr['hdrive1'] = $hdrive1; 
				$ret_arr['hdrive2'] = $hdrive2; 
				$ret_arr['timeRecFlag'] = $timeRecFlag; 
				// Fix for bug #13347
				$ret_arr['hidTime'] = "" ;
				$ret_arr['change_by_time_day'] = $change_by_time_day; 
				$ret_arr['change_by_time_hr'] = $change_by_time_hr;
				$ret_arr['txtlocdir_move'] = $txtlocdir_move;
				$ret_arr['txtdiskthreshold'] = $retention_options['txtdiskthreshold'];
				$ret_arr['htimelapse'] = $htimelapse;
				$ret_arr['hwindowstarthours'] = $hwindowstarthours;
				$ret_arr['hwindowstartminutes'] = $hwindowstartminutes;
				$ret_arr['hwindowstophours'] = $hwindowstophours;
				$ret_arr['hwindowstopminutes'] = $hwindowstopminutes;
				$ret_arr['logExceedPolicy'] = $retention_options['logExceedPolicy'];
				$ret_arr['lun_process_server_info'] = $lun_process_server_info;
				$ret_arr['process_server'] = $process_server;
				$ret_arr['hid_ps_natip_source'] = $hid_ps_natip_source;
				$ret_arr['hid_ps_natip_target'] = $hid_ps_natip_target;
				$ret_arr['hid_sparse_enable'] = $sparse_enable;
		
				$ret_arr['logDeviceName'] = $logDeviceName;
				$ret_arr['policy_type'] = $policy_type;
				$ret_arr['retention_dir'] = $retention_dir;
				
				$ret_arr['batch_resync_val'] = $batch_resync_val;
				$ret_arr['execution_state'] = $execution_state;
				$ret_arr['scenario_id'] = $scenario_id;
				$ret_arr['plan_id'] = $plan_id;
				$ret_arr['app_type'] = $app_type;
				
				
				$ret_arr['hid_retention_text'] = $retention_options['hid_retention_text'];
				$ret_arr['hid_retention_sel'] = $retention_options['hid_retention_sel'];
				$ret_arr['time_based'] = $retention_options['time_based'];
				$ret_arr['time_based_flag'] = $retention_options['time_based_flag'];
				$ret_arr['space_based'] = $retention_options['space_based'];
				
				$ret_arr['previous_ret_path'] = $previous_ret_path;
				$ret_arr['plan_type'] = $paln_type;
				$ret_arr['src_oracle_database_instance_id'] = $src_oracle_database_instance_id;
				$ret_arr['solution_type'] = $solution_type;
				$ret_arr['srcCapacity'] = $data['pairDetails'][$vol]['srcCapacity'];
				
				
				$ret_arr['prismSpecificInfo'] = $details['prismSpecificInfo'];
				$ret_arr['arrayPillar'] = $plan_properties['arrayPillar'];
				$ret_arr['srcLunId'] = $details['srcVol'];
				$ret_arr['trgLunId'] = $details['trgVol'];
                $ret_arr['use_cfs'] = $use_cfs;
 
				
				if($retention_options['time_based_flag'])
				{
					
					for($i=1;$i<=5;$i++)
					{
						$str1 = 'hid_application'.($i-1);
						$str2 = 'hid_bookmark'.($i-1);
						$str3 = 'hid_userDefined'.($i-1);
						$str4 = 'hid_granularity'.$i;
						$str5 = 'hid_granularity_range'.$i;
						$str6 = 'hid_seltime_range'.$i;
						$str7 = 'hid_seltime'.$i;
						$str8 = 'hid_add_remove'.$i;
						$str9 = 'seltime_range'.$i;
						
						if($i == 1)
						{
							$ret_arr[$str1] = 'all';
							$ret_arr[$str2] = 0;
							$ret_arr[$str3] = "";
						}
						else
						{
							$ret_arr[$str1] = $retention_options[$str1];
							$ret_arr[$str2] = $retention_options[$str2];
							$ret_arr[$str3] = $retention_options[$str3];
						}
						$ret_arr[$str4] = $retention_options[$str4];
						$ret_arr[$str5] = $retention_options[$str5];
						$ret_arr[$str6] = $retention_options[$str6];
						$ret_arr[$str7] = $retention_options[$str7];
						$ret_arr[$str8] = $retention_options[$str8];
						$ret_arr[$str9] = $retention_options[$str9];
					}									
				}
				if($plan_properties['arrayPillar']==1)
				{
					// Specific to Pillar for Rajeev Demo

					/*$sql = "UPDATE
								applicationScenario
							SET
								executionStatus = '$VALIDATION_SUCCESS'
							WHERE
								scenarioId = '$scenario_id' AND
								planId = '$plan_id'";
							$rs = $this->conn->sql_query($sql);*/
					
					#$this->insertApplicationInfo($process_server);
				}
				
				$volume_obj = new VolumeProtection();
				$rule_id = $volume_obj->get_ruleId ($id, $deviceName, $destId, $destDeviceName);
				
				if(!$rule_id)
				{
					$logging_obj->my_error_handler("DEBUG","Pair doesn't exist: $id, $deviceName, $destId, $destDeviceName");
					$volume_obj->configure_mediaRetention($ret_arr);
					
					$deviceName = $this->conn->sql_escape_string($deviceName);
					$logging_obj->my_error_handler("DEBUG","insertProtectionPlan :: solution_type : $solution_type :: resync_threshold : $resync_threshold :: diff_threshold : $diff_threshold ");
					$logging_obj->my_error_handler("DEBUG","insertProtectionPlan :: process_server : $process_server :: src_host_id_list_A : $src_host_id_list");
					if  ($solution_type == 'PRISM')
					{
						$src_host_id_list = $process_server;
					}
					$logging_obj->my_error_handler("DEBUG","insertProtectionPlan :: src_host_id_list_B : $src_host_id_list");
					$sql = "UPDATE 
								logicalVolumes
							SET
								maxDiffFilesThreshold = '$diff_threshold'
							WHERE
								hostId IN ('$src_host_id_list') AND
								deviceName = '$deviceName'";
					$logging_obj->my_error_handler("DEBUG","insertProtectionPlan :: sql : $sql");
					$rs = $this->conn->sql_query($sql);
					
					$inner_loop++;
				}
				else
				{
					$logging_obj->my_error_handler("DEBUG","Pair already exists for: $id, $deviceName, $destId, $destDeviceName");
					$trg_dev_name = $this->conn->sql_escape_string($destDeviceName);
					
					$sql = "UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								planId = '$plan_id',
								scenarioId = '$scenario_id',
								applicationType = '$app_type'
							WHERE
								destinationHostId = '$destId' AND
								destinationDeviceName = '$trg_dev_name'";
					$rs = $this->conn->sql_query($sql);
				}
			}
		}
		/*
			Update maxResyncFilesThreshold and rpoSLAThreshold in srcLogicalVolumeDestLogicalVolume table
		*/
		$sql = "UPDATE 
					srcLogicalVolumeDestLogicalVolume
				SET
					rpoSLAThreshold = '$rpo_threshold',
					maxResyncFilesThreshold='$resync_threshold'
				WHERE
					scenarioId = '$scenario_id' AND
					planId = '$plan_id'";
		$rs = $this->conn->sql_query($sql);

	}
	
	/*
	  insert scenario information
	 */
	public function insertScenarioDetails($scenarioData)
	{
		$this->setConnectionObject();
		$planId = $scenarioData['planId'];
		$scenarioType = $scenarioData['scenarioType'] ;
		$scenarioName = $scenarioData['scenarioName'];
		$scenarioDescription = $scenarioData['scenarioDescription'];
		$scenarioVersion = $scenarioData['scenarioVersion'];
		$scenarioCreationDate = $scenarioData['scenarioCreationDate'];
		$scenarioDetails = $scenarioData['scenarioDetails'];
		$validationStatus = $scenarioData['validationStatus'];
		$executionStatus = $scenarioData['executionStatus'];
		$applicationType = $scenarioData['applicationType'];
		$applicationVersion = $scenarioData['applicationVersion'];
		$srcInstanceId = $scenarioData['srcInstanceId'];
		$trgInstanceId = $scenarioData['trgInstanceId'];
		$referenceId = $scenarioData['referenceId'];
		$resyncLimit = $scenarioData['resyncLimit'];
		$autoProvision = $scenarioData['autoProvision'];
		$sourceId = $scenarioData['sourceId'];
		$targetId = $scenarioData['targetId'];
		$sourceVolumes = $scenarioData['sourceVolumes'];
		$targetVolumes = $scenarioData['targetVolumes'];
		$retentionVolumes = $scenarioData['retentionVolumes'];
		$reverseReplicationOption = $scenarioData['reverseReplicationOption'];
		$protectionDirection = $scenarioData['protectionDirection'];
		$currentStep = $scenarioData['currentStep'];
		$creationStatus = $scenarioData['creationStatus'];
		$isPrimary = $scenarioData['isPrimary'];
		$isDisabled = $scenarioData['isDisabled'];
		$isModified = $scenarioData['isModified'];
		$reverseRetentionVolumes = $scenarioData['reverseRetentionVolumes'];
		$volpackVolumes = $scenarioData['volpackVolumes'];
		$volpackBaseVolume = $scenarioData['volpackBaseVolume'];
		$isVolumeResized = $scenarioData['isVolumeResized'];
		$sourceArrayGuid = $scenarioData['sourceArrayGuid'];
		$targetArrayGuid = $scenarioData['targetArrayGuid'];
		$rxScenarioId = $scenarioData['rxScenarioId'];
		
		// Verifying any rollback scenario exist with the given source server.
		$rollback_scenario_id = $this->conn->sql_get_value("applicationScenario","scenarioId","sourceId = ? AND scenarioType = ?", array($sourceId,'Rollback'));
		// If rollback scenario exist cleaning the scenario data
		if($rollback_scenario_id) $this->conn->sql_query("DELETE FROM applicationScenario where scenarioId = ?", array($rollback_scenario_id));
		
		if($planId)
		{			
			$sql = "INSERT INTO applicationScenario(planId,
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
							rxScenarioId)  VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";


							
			#$logging_obj->my_error_handler("DEBUG","insert_sql : $sql\n");		
			$scenario_id = $this->conn->sql_query($sql, array("$planId","$scenarioType","$scenarioName","$scenarioDescription","$scenarioVersion","$scenarioCreationDate","$scenarioDetails","$validationStatus","$executionStatus","$applicationType","$applicationVersion","$srcInstanceId","$trgInstanceId","$referenceId","$resyncLimit","$autoProvision","$sourceId","$targetId","$sourceVolumes","$targetVolumes","$retentionVolumes","$reverseReplicationOption","$protectionDirection","$currentStep","$creationStatus","$isPrimary","$isDisabled","$isModified","$reverseRetentionVolumes","$volpackVolumes","$volpackBaseVolume","$isVolumeResized","$sourceArrayGuid","$targetArrayGuid","$rxScenarioId"));
			return $scenario_id;
		}
	}
	
	/*
	  insert prepare target request  
	 */
	public function insert_prepare_target($planId, $tgtId, $scenarioId) 
    {
		$this->setConnectionObject();
		$sql = "INSERT into policy(policyType,policyName,policyParameters,policyScheduledType,		policyRunEvery,nextScheduledTime,policyExcludeFrom,policyExcludeTo,applicationInstanceId,hostId,scenarioId,dependentId,policyTimestamp,planId) 
		VALUES(5,'PrepareTarget','0',2,0,0,0,0,0,?,?,0,now(),?)";
		$policy_id = $this->conn->sql_query($sql, array("$tgtId","$scenarioId","$planId"));
		
		$sql1 = "INSERT INTO policyInstance(policyId,lastUpdatedTime,policyState,executionLog,outputLog,policyStatus,uniqueId,dependentInstanceId,hostId) 
		VALUES (?,now(),3,'','','Active',0,0,?)";
		$policyInstanced = $this->conn->sql_query($sql1, array("$policy_id","$tgtId"));
	    
	return $policy_id;
	}
	
	/*
	  insert normal consistency policy 
	 */
	public function insert_consistency_policy($planId, $srcId, $scenarioId, $consistencyPolicyOptions) 
    {
		$this->setConnectionObject();
		$interVal = $consistencyPolicyOptions['ConsistencyInterval'];
		$interValInSec = $interVal*60;
		$sql = "INSERT into policy(policyType,policyName,policyParameters,policyScheduledType,		policyRunEvery,nextScheduledTime,policyExcludeFrom,policyExcludeTo,applicationInstanceId,hostId,dependentId,policyTimestamp,scenarioId,planId) 
		VALUES(4,'Consistency','0',1,?,0,0,0,0,?,0,now(),?,?)";
		$policy_id = $this->conn->sql_query($sql, array("$interValInSec", "$srcId", "$scenarioId","$planId"));
		
		$sql1 = "INSERT INTO policyInstance(policyId,lastUpdatedTime,policyState,executionLog,outputLog,policyStatus,uniqueId,dependentInstanceId,hostId) 
		VALUES (?,now(),3,'','','Active',0,0,?)";
		$policyInstanced = $this->conn->sql_query($sql1, array("$policy_id","$srcId"));
	    
	return $policy_id;
	}
	
	
	/*
	  insert normal consistency policy 
	 */
	public function insert_crash_consistency_policy($planId, $srcId, $scenarioId, $crashConsistencyPolicyOptions) 
    {
		$this->setConnectionObject();
        #print "\n$planId, $srcId, $scenarioId";
    
        #print_r($crashConsistencyPolicyOptions);
        
		$interValInSec = $crashConsistencyPolicyOptions['CrashConsistencyInterval'];
		$policy_params = serialize(array("option" => "vacp.exe -systemlevel -cc"));
		$sql = "INSERT into policy(policyType,policyName,policyParameters,policyScheduledType,		policyRunEvery,nextScheduledTime,policyExcludeFrom,policyExcludeTo,applicationInstanceId,hostId,dependentId,policyTimestamp,scenarioId,planId) 
		VALUES(4,'Consistency',?,1,?,0,0,0,0,?,0,now(),?,?)";
		$policy_id = $this->conn->sql_query($sql, array($policy_params,$interValInSec, $srcId, "$scenarioId","$planId"));
		
		$sql1 = "INSERT INTO policyInstance(policyId,lastUpdatedTime,policyState,executionLog,outputLog,policyStatus,uniqueId,dependentInstanceId,hostId) 
		VALUES (?,now(),3,'','','Active',0,0,?)";
		$policyInstanced = $this->conn->sql_query($sql1, array("$policy_id","$srcId"));
	    
        return $policy_id;
	}
	
	/*
	  construct dvacp command and insert distributed consistency policy 
	 */
	public function insert_dvacp_consistency_policy($srcIdsList, $planId, $consistencyOptions) 
    {
        global $DVACP_PORT, $DVACP_DEFAULT_PORT;
		$this->setConnectionObject();
		$consistencyPolicyOptions['runTime'] = $consistencyOptions['ConsistencyInterval'];
		$consistencyPolicyOptions['exceptFrom1'] = "0";
		$consistencyPolicyOptions['exceptFrom2'] = "0";
		$consistencyPolicyOptions['exceptTo1'] = "0";
		$consistencyPolicyOptions['exceptTo2'] = "0";
		$consistencyPolicyOptions['interval'] = "1";
		$consistencyPolicyOptions['run_every_interval'] = $consistencyOptions['ConsistencyInterval'] * 60;
		
		$allHostIds = implode(",", $srcIdsList);
		$sql = "SELECT osFlag, ipaddress, id FROM hosts WHERE FIND_IN_SET(id, ?)";
		$hostDetails = $this->conn->sql_query($sql,array("$allHostIds"));
		
		$winIps = array();
		$winHostIds = array();
		$linuxHostIds = array();
		foreach($hostDetails as $key=>$details)
		{
			$osFlag = $details['osFlag'];
			if($details['osFlag'] == 1)
			{
				array_push($winIps, $details['ipaddress']);
				array_push($winHostIds, $details['id']);
			}
			else
			{
				array_push($linuxHostIds, $details['id']);
			}
		}
		
		$vacpCommand = "vacp.exe -systemlevel";
		$consistencyPolicyOptions['os_flag'] = 1;
		 if(count($winIps) > 1 && count($linuxHostIds) == 0)
		{
			$mn = array_shift($winIps);
			$cn = implode(",",$winIps);
             $dport_val = $DVACP_DEFAULT_PORT;
            if (isset($DVACP_PORT) && $DVACP_PORT != "")
                $dport_val = $DVACP_PORT;
			$vacpCommand .= " -distributed -mn $mn -cn $cn -dport $dport_val";		
		}
		
		$consistencyPolicyOptions['option'] = $vacpCommand;
		if(count($winIps) >= 1) 
		{ 
			$policyId = $this->insert_dvacp_policy($consistencyPolicyOptions,$winHostIds,$mn,$cn,$planId);
			return $policyId;
		}
	}
	
	public function insert_dvacp_consistency_policy_v2($srcIdsList, $planId, $consistencyOptions)
	{
        global $DVACP_PORT, $DVACP_DEFAULT_PORT;
		$this->setConnectionObject();
		$consistencyPolicyOptions['runTime'] = $consistencyOptions['ConsistencyInterval'];
		$consistencyPolicyOptions['exceptFrom1'] = "0";
		$consistencyPolicyOptions['exceptFrom2'] = "0";
		$consistencyPolicyOptions['exceptTo1'] = "0";
		$consistencyPolicyOptions['exceptTo2'] = "0";
		$consistencyPolicyOptions['interval'] = "1";
		$consistencyPolicyOptions['run_every_interval'] = $consistencyOptions['ConsistencyInterval'] * 60;
		#print_r($consistencyPolicyOptions);
        
		$allHostIds = implode(",", $srcIdsList);
       
		$sql = "SELECT osFlag, ipAddress, id FROM hosts WHERE FIND_IN_SET(id, ?)";
		$hostDetails = $this->conn->sql_query($sql,array("$allHostIds"));
		
		$ips = array();
		$HostIds = array();
		foreach($hostDetails as $key=>$details)
		{
			array_push($ips, $details['ipAddress']);
			$osFlag = $details['osFlag'];
			array_push($HostIds, $details['id']);
		}
		
		$vacpCommand = "vacp.exe -systemlevel";
		$consistencyPolicyOptions['os_flag'] = 1;
		 if(count($ips) > 1)
		{
            #print "yes.......";
			$mn = array_shift($ips);
			$cn = implode(",",$ips);
            $dport_val = $DVACP_DEFAULT_PORT;
            if (isset($DVACP_PORT) && $DVACP_PORT != "")
                $dport_val = $DVACP_PORT;
			$vacpCommand .= " -distributed -mn $mn -cn $cn -dport $dport_val";		
		}
		
		$consistencyPolicyOptions['option'] = $vacpCommand;
       
		if(count($ips) >= 1) 
		{ 
			$policyId = $this->insert_dvacp_policy($consistencyPolicyOptions,$HostIds,$mn,$cn,$planId);
		}
        return $policyId;
        
	}
	
	public function insert_dvacp_crash_consistency_policy($srcIdsList, $planId, $consistencyOptions) 
    {
        global $DVACP_PORT, $DVACP_DEFAULT_PORT;
		$this->setConnectionObject();
		//Crash consistency interval value input receiving in seconds.
		$consistencyPolicyOptions['runTime'] = $consistencyOptions['CrashConsistencyInterval'];
		$consistencyPolicyOptions['exceptFrom1'] = "0";
		$consistencyPolicyOptions['exceptFrom2'] = "0";
		$consistencyPolicyOptions['exceptTo1'] = "0";
		$consistencyPolicyOptions['exceptTo2'] = "0";
		$consistencyPolicyOptions['interval'] = "1";
		$consistencyPolicyOptions['run_every_interval'] = $consistencyOptions['CrashConsistencyInterval'];
		
        #print_r($srcIdsList);
		$allHostIds = implode(",", $srcIdsList);
		$sql = "SELECT osFlag, ipAddress, id FROM hosts WHERE FIND_IN_SET(id, ?)";
		$hostDetails = $this->conn->sql_query($sql,array("$allHostIds"));
		
		$ips = array();
		$HostIds = array();
        #print_r($hostDetails);
        
		foreach($hostDetails as $key=>$details)
		{
			array_push($ips, $details['ipAddress']);
			$osFlag = $details['osFlag'];
			array_push($HostIds, $details['id']);
		}
		
        #print_r($ips);
		$vacpCommand = "vacp.exe -systemlevel";
		$consistencyPolicyOptions['os_flag'] = 1;
        
		 if(count($ips) > 1)
		{
			$mn = array_shift($ips);
			$cn = implode(",",$ips);
            $dport_val = $DVACP_DEFAULT_PORT;
            if (isset($DVACP_PORT) && $DVACP_PORT != "")
                $dport_val = $DVACP_PORT;
			$vacpCommand .= " -cc -distributed -mn $mn -cn $cn -dport $dport_val";		
		}
		
		$consistencyPolicyOptions['option'] = $vacpCommand;
        #print_r($consistencyPolicyOptions);
        #print_r($ips);
        #print_r($HostIds);
        #print "mn is $mn, cn is $cn";
        #exit;
		if(count($ips) >= 1) 
		{ 
			$policyId = $this->insert_dvacp_policy($consistencyPolicyOptions,$HostIds,$mn,$cn,$planId);
		}
        return $policyId;
	}
    	/*
	  insert distributed consistency policy 
	 */
	function insert_dvacp_policy($consistencyPolicyOptions,$hostIds,$masterNode,$coNodes,$planId)
	{
		$this->setConnectionObject();
		$srcHostIds = implode(",",$hostIds);
		$consistencyData = serialize($consistencyPolicyOptions);
		
		$planName = $this->conn->sql_get_value('applicationPlan', 'planName',"planId='$planId'");
		$groupName = $planName."_dvacp_consistency".$planId;
		
		$sql = "INSERT into policy(policyType,policyName,policyParameters,policyScheduledType,		policyRunEvery,nextScheduledTime,policyExcludeFrom,policyExcludeTo,applicationInstanceId,hostId,dependentId,policyTimestamp,scenarioId,planId) 
		VALUES(4,'Consistency',?,2,0,0,0,0,0,?,0,now(),0,?)";
		$policyId = $this->conn->sql_query($sql, array($consistencyData, $srcHostIds,$planId));
	
		foreach ($hostIds as $key=>$srcId) 
		{
			$sql1 = "INSERT INTO policyInstance(policyId,lastUpdatedTime,policyState,uniqueId,hostId) VALUES(?,now(),3,0,?)";
			$policyInstanceId = $this->conn->sql_query($sql1, array($policyId,$srcId));
		}
        
		$sql2 = "INSERT INTO consistencyGroups(policyId,groupName,masterNode,coordinateNodes) VALUES (?,?,?,?)";
		$consistencyGroupId = $this->conn->sql_query($sql2, array($policyId,$groupName,$masterNode,$coNodes));
		return $policyId;
	}
    
	/*
	  insert source and target info into the srcMtDiskMapping 
	 */
	public function insert_src_mt_disk_mapping($diskMapping, $planId, $scenarioId)
	{
	$this->setConnectionObject();
	foreach ($diskMapping as $k=>$mapping) {
		$srcHostId = $mapping['sourceHostId'];
		$trgHostId = $mapping['targetHostId'];
		$srcDisk = $mapping['SourceDiskName'];
		$trgDisk = $mapping['TargetDiskName'];
		$trgLunId = $mapping['TargetLUNId'];
		$status = $mapping['status'];
		
		$sql = "INSERT INTO srcMtDiskMapping(srcHostId,trgHostId,srcDisk,trgDisk,trgLunId,status,scenarioId,planId) 
		VALUES(?,?,?,?,?,?,?,?)";
		$refence = $this->conn->sql_query($sql, array("$srcHostId","$trgHostId","$srcDisk","$trgDisk","$trgLunId","$status","$scenarioId","$planId"));
		}
	}
	
	//This function should be used for V2 forward protection target disks impersonate.
	public function insert_target_disk_details_in_lv_table($sourceHostId, $source_disk_names, $MasterTarget)
	{
		$this->setConnectionObject();
		global $logging_obj;
		$protection_disk_details = array();
        
		if (is_array($source_disk_names))
		{
			$source_disk_names = implode(",",$source_disk_names);
           #print "$sourceHostId, $source_disk_names";
           
            /*
            SELECT 
						deviceName,
						capacity,
                        rawSize,
                        maxDiffFilesThreshold
					FROM 
						logicalVolumes
					WHERE 
						hostId = ?  
					AND	
						FIND_IN_SET(deviceName, ?)
           mysql> explain SELECT
    ->                                          deviceName,
    ->                                          capacity,
    ->                                          rawSize,
    ->                                          maxDiffFilesThreshold
    ->                                  FROM
    ->                                          logicalVolumes
    ->                                  WHERE
    ->                                          hostId = "9F755DE0-AD01-9841-B6EB63AA5C269F58761"
    ->                                  AND
    ->                                          FIND_IN_SET(deviceName, "\\.\PHYSICALDRIVE1,\\.\PHYSICALDRIVE0,\\.\PHYSICALDRIVE2"
)\G;
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: logicalVolumes
         type: ref
possible_keys: hostId
          key: hostId
      key_len: 162
          ref: const
         rows: 28
        Extra: Using where
1 row in set (0.00 sec)
*/
			$sql = "SELECT 
						deviceName,
						capacity,
                        rawSize,
                        maxDiffFilesThreshold
					FROM 
						logicalVolumes
					WHERE 
						hostId = ?  
					AND	
						FIND_IN_SET(deviceName, ?)";
           
			$result_set = $this->conn->sql_query($sql, array($sourceHostId, $source_disk_names));
            
			/*Sample result_set data
            Array
            (
                [0] => Array
                    (
                        [deviceName] => 10000000
                        [capacity] => 64424509440
                        [rawSize] => 64424509440
                        [maxDiffFilesThreshold] => 
                    )
                    [1] => Array
                    (
                        [deviceName] => 20000000
                        [capacity] => 64424509440
                        [rawSize] => 64424509440
                        [maxDiffFilesThreshold] => 
                    )

            )

*/
             $volume_obj = new VolumeProtection();
            $os_type = $volume_obj->os_type($sourceHostId);
			foreach($result_set as $data)
			{
				$capacity = $data['capacity'];
				$source_device_name = $data['deviceName'];
                $raw_size = $data['rawSize'];
                $max_diff_files_threshold = $data['maxDiffFilesThreshold'];
                
				$target_disk_name = $source_device_name;
                $target_disk_name = $volume_obj->get_unformat_dev_name($MasterTarget,$target_disk_name);
                //Windows protected disk device name for target --> C:\9F755DE0-AD01-9841-B6EB63AA5C269F58751\23232232232
                if ($os_type == 1)
                {
                    $target_disk_name = "C:\\".$sourceHostId."\\".$target_disk_name;
                }
                //Linux protected disk device name for target --> C:\9F755DE0-AD01-9841-B6EB63AA5C269F58751\dev\sda
                elseif ($os_type == 2)
                {
                    $target_disk_name = "C:\\".$sourceHostId.$target_disk_name;
                }
                
                #$target_disk_name = $this->conn->sql_escape_string($target_disk_name);
                
				$lastDeviceUpdateTime=time();
                #print "$MasterTarget,$target_disk_name,$target_disk_name,$target_disk_name,$capacity,$lastDeviceUpdateTime";
              
				$sql="INSERT
				   INTO
					   `logicalVolumes`
					(
					 `hostId` ,
					 `deviceName` ,
					 `deviceId`,
					 `mountPoint` ,
					 `capacity` ,
					 `lastDeviceUpdateTime`,
					   deviceFlagInUse,
                       rawSize,
                       maxDiffFilesThreshold,
					   isImpersonate 
					)
				   VALUES
					(?,?,?,?,?,?,?,?,?,?)";
				$result = $this->conn->sql_query($sql, array($MasterTarget,$target_disk_name,$target_disk_name,$target_disk_name,$capacity,$lastDeviceUpdateTime,1,$raw_size,$max_diff_files_threshold,1));
				
				$protection_disk_details[] = array("sourceDiskName"=>$source_device_name,"capacity"=>$capacity, "targetDiskName" =>$target_disk_name);
			}
            #print_r($protection_disk_details);
		}
		return $protection_disk_details;
	}
	/*
	  Update plan and scenario to prepare target based upon planid
	 */
	public function updatePrepareTarget($planId, &$protectedPlanInfo)
	{
		$this->setConnectionObject();
		global $logging_obj;
		$logging_obj->my_error_handler("INFO",array("PlanId"=>$planId), Log::BOTH);
		global $PREPARE_TARGET_PENDING,$PREPARE_TARGET_FAILED;
		$sql = "SELECT 
					p.policyId, 
					p.scenarioId,  
					p.hostId 
				FROM 
					policy p,
					policyinstance pi 
				WHERE 
					p.planId = ? AND 
					p.policyType = '5' AND 
					p.policyId = pi.policyId AND 
					pi.policyState = '2'
					";
		$rs = $this->conn->sql_query($sql, array($planId));
		$id_list = array();
		if(count($rs) > 0)
		{
			foreach($rs as $data)
			{
				$scenarioId = $data['scenarioId'];
				$policyId = $data['policyId'];
				$targetId = $data['hostId'];
				$sourceId = $this->conn->sql_get_value('applicationScenario', 'sourceId', "scenarioId='$scenarioId'");
				
				$protectedPlanInfo["scenarioId"]["$sourceId"] = $scenarioId;
				$protectedPlanInfo["policyId"]["$targetId"] = $policyId;
				
				$id_list[] = $policyId;
				
				$sql = "UPDATE
								applicationScenario
							SET
								executionStatus = '$PREPARE_TARGET_PENDING'
							WHERE
								executionStatus = '$PREPARE_TARGET_FAILED' AND
								scenarioId = ?";
				$logging_obj->my_error_handler("DEBUG","updatePrepareTarget :: sql_C : $sql ");
				$rs = $this->conn->sql_query($sql, array($scenarioId));
					
			}
			
			$id_list = implode(",",$id_list);
			$logging_obj->my_error_handler("INFO",array("IdList"=>$id_list),Log::BOTH);	
			$sql = "UPDATE
						policy 
					SET 
						policyTimestamp = now()
					WHERE 
						FIND_IN_SET(policyId, ?)";
			$logging_obj->my_error_handler("DEBUG","updatePrepareTarget :: sql_A : $sql ");
			$updateRs = $this->conn->sql_query($sql, array($id_list));	
				
			$sql = "UPDATE
						policyInstance 
					SET 
						policyState = '3',
						outputLog = '',
						executionLog = '',
						lastUpdatedTime = now() 
					WHERE 
						FIND_IN_SET(policyId, ?)";
			$logging_obj->my_error_handler("DEBUG","updatePrepareTarget :: sql_B : $sql ");
			$rs = $this->conn->sql_query($sql, array($id_list));
			
			$sql = "UPDATE
						applicationplan 
					SET
						applicationPlanStatus = 'Prepare Target Pending'
					WHERE
						applicationPlanStatus = 'Prepare Target Failed' AND
						planId = ?";
			$logging_obj->my_error_handler("DEBUG","updatePrepareTarget :: sql_D : $sql ");
			$rs = $this->conn->sql_query($sql, array($planId));
			
		}
		else{
		
		}
		return $id_list;
	}
	/*
	  verify if plan is exist for provided source guid and scenario type
	 */
	function isPlanExist($sourceHostId, $scenarioType)
	{
		$this->setConnectionObject();
		
		 $sql = "SELECT
					targetId
				FROM
					applicationScenario 
				WHERE
					sourceId = ? AND 
					scenarioType = ? 
					";
		$rs = $this->conn->sql_query($sql, array($sourceHostId, $scenarioType));	
		return $rs;
	}
	
	/*
	  Update plan level batch resync
	 */
	public function updateBatchResync($planId,$batchResync)
	{		
		$this->setConnectionObject();
		$qry = "UPDATE 
					applicationPlan 
				SET 
					batchResync = ?
				WHERE
					planId = ?";
		
		$rs = $this->conn->sql_query($qry, array($batchResync, $planId));
	}
	
	/*
	  Update protection pair properties.
	 */
	public function update_protection_properties($updateColumnStr, $parameterArr)
	{
		$this->setConnectionObject();
		$qry = "UPDATE 
						srclogicalvolumedestlogicalvolume 
					SET 
						$updateColumnStr 
					WHERE 
						sourceHostId = ? AND destinationHostId = ?";
		$rs = $this->conn->sql_query($qry, $parameterArr);
	}
	
	/*
		Update the newly added server IP's to the existing DVacp policy
	*/
	public function update_distributed_consistency_policy($planId, $srcHostIdAry, $interval,$protection_path="A2E")
	{
		$this->setConnectionObject();
		
		$sql = "SELECT policyId,policyParameters,policyRunEvery FROM policy WHERE planId = ? AND policyType = '4'";
		$rs = $this->conn->sql_query($sql,array($planId));
		
		if(is_array($rs) && (count($rs)>0)) 
		{
			foreach($rs as $k=>$v) 
			{
				$policyId = $v['policyId'];
				
				$del_dvap = "DELETE FROM policy WHERE policyId = ?";
				$rs = $this->conn->sql_query($del_dvap,array($policyId));
				
				$del_dvap_grp = "DELETE FROM consistencyGroups WHERE policyId = ?";
				$rs = $this->conn->sql_query($del_dvap_grp,array($policyId));
			}
		}
		if ($protection_path == "E2A")
        {
           $policyId = insert_dvacp_consistency_policy_v2($srcHostIdAry,$planId,array('ConsistencyInterval'=>$interval));
        }
        else
        {
            $policyId = $this->insert_dvacp_consistency_policy($srcHostIdAry,$planId,array('ConsistencyInterval'=>$interval));
        }
		return $policyId;
	}
	
	/*
		Update consistency policy
	*/
	public function update_consistency_policy($planId, $srcHostIdAry, $interval)
	{
		$this->setConnectionObject();
		
		//Default value of normal consistency policy interval
		
		$sql = "SELECT policyId,policyParameters FROM policy WHERE planId = ? AND policyType = '4'";
		$rs = $this->conn->sql_query($sql,array($planId));
		
		if(is_array($rs) && (count($rs)>0)) 
		{
			foreach($rs as $k=>$v) 
			{
				$policyId = $v['policyId'];
												
				$del_dvap = "DELETE FROM policy WHERE policyId = ?";
				$rs = $this->conn->sql_query($del_dvap,array($policyId));
								
				if($v['policyParameters'])
				{
					$del_dvap_grp = "DELETE FROM consistencyGroups WHERE policyId = ?";
					$rs = $this->conn->sql_query($del_dvap_grp,array($policyId));
				}	
			}			
		}
		
		$consistencyPolicyOptions = array();
		$consistencyPolicyOptions['ConsistencyInterval'] = $interval;
		foreach($srcHostIdAry as $srcId) {
			$scenarioId = $this->conn->sql_get_value("applicationScenario", "scenarioId", "planId = ? AND sourceId = ? AND scenarioType = 'DR Protection'", array($planId,$srcId));
			$this->insert_consistency_policy($planId, $srcId, $scenarioId, $consistencyPolicyOptions);
		}
	}
	
	public function getPlanDetails($planId=0, $planType='', $sourceServers='')
	{
		global $log_rollback_string;
		$this->setConnectionObject();
        $sourceServers = array_unique($sourceServers);
        $source_servers_str = implode(",", $sourceServers);
        $param_arr = array();
        $planInfo = array();
        $condition = "";        
        
        if($planId) 
        {
            $condition .= " a.planId = ? AND";
            $param_arr[] = $planId;				
        }
        if($planType)
        {
            $condition .= " a.scenarioType = ? AND";
            $param_arr[] = $planType;
        }
        if(count($sourceServers))
        {
            $condition .= " FIND_IN_SET(a.sourceId, ?) AND";
            $param_arr[] = $source_servers_str;
        }
        
        $select_plans_sql = "SELECT
                                    a.scenarioType,
                                    a.planId,
                                    a.sourceId,
                                    a.targetId,
                                    b.planName                             
                            FROM	
                                applicationScenario a,
                                applicationPlan b
                                WHERE
                                $condition 
                                a.planId = b.planId";	
        $result = $this->conn->sql_query($select_plans_sql, $param_arr);
		$log_rollback_string = $log_rollback_string."select_plans_sql = $select_plans_sql".",";
	   if(is_array($result))
{
        foreach($result as $key => $scenarioInfo)
        {                
            if($scenarioInfo['scenarioType'] == "Rollback")
            {
                $application_plan_status = $this->conn->sql_get_value('applicationPlan', 'applicationPlanStatus', "planId = ?", array($scenarioInfo['planId']));	                
                if(($application_plan_status == 'Rollback Completed') || (($application_plan_status == 'Recovery Success') 
                || ($application_plan_status == 'Recovery Failed') 	|| ($application_plan_status == 'Recovery InProgress') 
                || ($application_plan_status == 'Recovery Pending'))) 	
                {
                    $rollback_status = 'Completed';
                } 
                elseif($application_plan_status == 'Rollback Failed') 
                {
                    $rollback_status = 'Failed';
                } 
                elseif ($application_plan_status == 'Rollback InProgress') 
                {
                    $rollback_status = 'InProgress';
                } 
                else 
                {
                    $rollback_status = 'Pending';
                }  
                $planInfo[$scenarioInfo['scenarioType']][$scenarioInfo['planId']]['rollbackStatus'] = $rollback_status;
            }           
            $planInfo[$scenarioInfo['scenarioType']][$scenarioInfo['planId']]['targetId'] = $scenarioInfo['targetId'];
            $planInfo[$scenarioInfo['scenarioType']][$scenarioInfo['planId']]['planName'] = $scenarioInfo['planName'];
            $planInfo[$scenarioInfo['scenarioType']][$scenarioInfo['planId']]['sourceId'][] = $scenarioInfo['sourceId'];
        }   
}
		$planInfo_log = serialize($planInfo);
		$log_rollback_string = $log_rollback_string."planInfo = $planInfo_log ".",";
        return $planInfo;				
	}
    
    public function deleteStalePlans($planId)
    {
        $this->setConnectionObject();
        $this->conn->sql_query("DELETE FROM policy WHERE planId = ?", array($planId));
        $this->conn->sql_query("DELETE FROM applicationScenario WHERE planId = ?", array($planId));
        $this->conn->sql_query("DELETE FROM applicationPlan WHERE planId = ?", array($planId));
    }
	
	public function updateRetentionWindow($planId, $user_defined_time_window)
	{
		$this->setConnectionObject();
		$sql = "SELECT 
						rw.Id
					FROM 
						retentionWindow rw,                                
						retLogPolicy ret                           
					 LEFT JOIN
						srcLogicalVolumeDestLogicalVolume src
					ON
						src.ruleId = ret.ruleid
					WHERE
						src.planId = ? AND                             
						rw.retId = ret.retId AND 
						rw.retentionWindowId = 1";
		$ret_id_array = $this->conn->sql_get_array($sql,"Id","Id",array($planId));
		
		if(count($ret_id_array) > 0)
		{
			foreach($ret_id_array as $value => $ret_id)	{
				$ret_id_list[] = $ret_id;
			}
			
			$ret_ids = implode(",",$ret_id_list);
			$end_time = ($user_defined_time_window) ? $user_defined_time_window : 72;
			
			$sql = "UPDATE retentionWindow SET endTime=? WHERE FIND_IN_SET(Id, ?) AND retentionWindowId = 1";
			$this->conn->sql_query($sql, array($end_time, $ret_ids));
			
			$sql = "DELETE FROM retentionWindow WHERE FIND_IN_SET(Id, ?) AND retentionWindowId = 2";
			$this->conn->sql_query($sql, array($ret_ids));
		}
		
		$sql = "SELECT scenarioId, scenarioDetails FROM applicationScenario WHERE planId = ?";
		$scenario_result = $this->conn->sql_query($sql, array($planId));
		foreach($scenario_result as $data)
		{
			$scenario_data = unserialize($data['scenarioDetails']);
			$scenario_id = $data['scenarioId'];
			$src_id = $scenario_data['sourceHostId'];			
			$tgt_id = $scenario_data['targetHostId'];			
			$scenario_data['pairInfo'][$src_id."=".$tgt_id]['retentionOptions']['hid_seltime1'] = $user_defined_time_window;
			$scenario_data['pairInfo'][$src_id."=".$tgt_id]['retentionOptions']['time_based'] = $user_defined_time_window;
			$scenario_data['pairInfo'][$src_id."=".$tgt_id]['retentionOptions']['hid_retention_text'] = $user_defined_time_window;
			$scenario_details = serialize($scenario_data);
			$sql = "UPDATE applicationScenario SET scenarioDetails = ? WHERE scenarioId = ?";
			$this->conn->sql_query($sql, array($scenario_details, $scenario_id));
		}
	}
    
    public function get_pair_id_by_source($src_host_ids)
    {
		$this->setConnectionObject();
        if(!is_array($src_host_ids)) $src_host_ids_str = $src_host_ids;
        else
        {
            $src_host_ids_str = implode(",", $src_host_ids);
        }
        
        $pair_details = array();
        if($src_host_ids_str != "")
        {
            $get_pairs_sql = "SELECT sourceHostId, destinationHostId, pairId from srcLogicalVolumeDestLogicalVolume WHERE FIND_IN_SET (sourceHostId, ?)";
            $pair_details = $this->conn->sql_query($get_pairs_sql, array($src_host_ids_str));
        }
        return $pair_details;
    }
	
	/*
	 * Function Name: get_pair_id_by_destination
	 *
	 * Description:
	 *     	This API to give pair ids based on target host id and target data plane.
	 *		Get pair id's for Forward protection only which were involved by MARS.
	 *  
	 * Parameters:
	 *     	[IN]: $dest_host_ids
	 * Return Value:
	 *    	$pair_details
	 *
	 * Exceptions:
	 *     
	 */ 
	public function get_pair_id_by_destination($dest_host_ids)
	{
		global $AZURE_DATA_PLANE;
		$this->setConnectionObject();
		if(!is_array($dest_host_ids)) 
		{
			$dest_host_ids_str = $dest_host_ids;
		}
		else
		{
			$dest_host_ids_str = implode(",", $dest_host_ids);
		}
		
		$pair_details = array();
		if($dest_host_ids_str != "")
		{
			$get_pairs_sql = "SELECT sourceHostId, destinationHostId, pairId from srcLogicalVolumeDestLogicalVolume WHERE FIND_IN_SET (destinationHostId, ?) AND TargetDataPlane = ?";
			$pair_details = $this->conn->sql_query($get_pairs_sql, array($dest_host_ids_str,$AZURE_DATA_PLANE));
		}
		return $pair_details;
	}
   

    public function get_pair_id_by_disk_id($src_host_id, $disk_ids)
    {		
		$this->setConnectionObject();
        if(!is_array($disk_ids)) $disks_str = $disk_ids;
        else
        {
            $disks_str = implode(",", $disk_ids);
        }
        
        $pair_details = array();
        if($disks_str != "")
        {
            $get_pairs_sql = "SELECT sourceHostId, destinationHostId, pairId, sourceDeviceName from srcLogicalVolumeDestLogicalVolume WHERE FIND_IN_SET (sourceDeviceName, ?) AND sourceHostId = ?";
            $pair_details = $this->conn->sql_query($get_pairs_sql, array($disks_str, $src_host_id));			
        }
        return $pair_details;
    }
};
?>