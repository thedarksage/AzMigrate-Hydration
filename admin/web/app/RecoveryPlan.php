<?php
class recoveryPlan
{
	var $conn;
	var $scenarioId;
	var $planProperties;
	var $editMode;
	var $page;
	var $recoveryType;
	
	var $volume_obj;
	
	function __construct()
    {
       // $this->$volume_obj = new VolumeProtection();
    }

	public function setScenarioId($scenario_id)
    {
		$this->scenarioId = $scenario_id;
    }
	
	public function get_scenario_id()
    {
		return $this->scenarioId;
    }
	
	public function getPlanProperties()
	{
		return $this->planProperties;
	}
	
	private function setConnectionObject()
	{
		global $conn_obj;
		if(!isset($this->conn) || !is_object($this->conn))
		{
			$this->conn = $conn_obj;
		}
	}

	public function getPlanId($scenario_id)
	{		
		$this->setConnectionObject();
		$plan_id = $this->conn->sql_get_value('applicationScenario', 'planId',"scenarioId='$scenario_id'");

		return $plan_id;
	}
		
	public function getSelectedProtectionScenarioDetails($protection_scenario_id)
	{
		$this->setConnectionObject();
		$commonfunctions_obj = new CommonFunctions();
		$sql = "SELECT
					scenarioDetails
				FROM
					applicationScenario
				WHERE
					scenarioId = '$protection_scenario_id'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$cdata = $row->scenarioDetails;
		$cdata_arr = $commonfunctions_obj->getArrayFromXML($cdata);
		$plan_properties = unserialize($cdata_arr['plan']['data']['value']);
		if(!$plan_properties)
		{
			$plan_properties = unserialize($cdata);
		}
		return $plan_properties;
	}
	
	public function getReferenceId($scenario_id)
	{
		$this->setConnectionObject();
		$reference_id = $this->conn->sql_get_value('applicationScenario', 'referenceId',"scenarioId = '$scenario_id'");
		
		return $reference_id;
	}
	
	
	public function getScenarioDetails($plan_id,$type='', $source_id = '')
	{
		$resultArr = array();
		$this->setConnectionObject();
		$arr=array(); 
		
		if($plan_id)
		{
			array_push($arr, $plan_id); 
			$plan_id_cond = "planId = ?";
		}
		elseif($source_id)
		{
			$source_id_cond = "FIND_IN_SET(sourceId, ?)";
			if(count($source_id)) $source_id = implode(",",$source_id);
			array_push($arr, $source_id);
		}

		if($type == 'backup')
		{
			$cond = " AND scenarioType IN ('Data Validation and Backup','Rollback')";
		}
		elseif($type == '')
		{
			$cond = " AND (scenarioType <> 'DR Protection' && scenarioType <> 'Backup Protection' && scenarioType <> 'Data Validation and Backup' && scenarioType <> 'Rollback' && scenarioType <> 'Profiler Protection')";
		}
		else
		{
			$cond = " AND (scenarioType = ?)";
			array_push($arr, $type); 
		}
		$sql = "SELECT
					scenarioId,
					scenarioType,
					sourceId,
					targetId,
					applicationVersion,
					reverseReplicationOption,
					executionStatus,
					scenarioCreationDate,
					srcInstanceId,
					trgInstanceId,
					applicationType,
					isDisabled,
					isModified,
					currentStep,
					referenceId,
					planId
				FROM 
					applicationScenario
				WHERE
					$plan_id_cond $source_id_cond $cond";
		$rs = $this->conn->sql_query($sql,$arr);
		return $rs;
	}
	
	public function getProtectionDirection($protection_scenario_id)
	{
		$this->setConnectionObject();
		$protection_direction = $this->conn->sql_get_value('applicationScenario', 'protectionDirection',"scenarioId = '$protection_scenario_id'");

		return $protection_direction;
	
	}
	
	public function getRecoveryScenarioDetails($scenaioId)
	{
		$this->setConnectionObject();
		$commonfunctions_obj = new CommonFunctions();
		$sql = "SELECT scenarioDetails FROM applicationScenario WHERE scenarioId='$scenaioId'";
		$rs = $this->conn->sql_query($sql);
		
		while($row = $this->conn->sql_fetch_object($rs))
		{
			$cdata = $row->scenarioDetails;
			
			$cdata_arr = $commonfunctions_obj->getArrayFromXML($cdata);
			$ars = unserialize($cdata_arr['plan']['data']['value']);
			$result = $ars;
		}
		
		return $result;
	}
}
?>