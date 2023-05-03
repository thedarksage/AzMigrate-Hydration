<?php
class PrismProtection
{
	private $conn;
	
	function __construct()
	{
		global $conn_obj;
		
		$this->conn = $conn_obj;
	}
	
	public function update_protection_scenario_session($scenario_id,$hostId,$scenario_type)
	{
		global $logging_obj, $app_obj ,$commonfunctions_obj;
		$logging_obj->my_error_handler("DEBUG","update_protection_scenario_session :: scenario_id : $scenario_id :: hostId : $hostId :: scenario_type : $scenario_type " );
		$selectProtectionScenarioId = "	SELECT
											referenceId
										FROM
											applicationScenario
										WHERE
											scenarioId = '$scenario_id'";
		$logging_obj->my_error_handler("DEBUG","update_protection_scenario_session : OUERY_VOLUMES_A :: ".$selectProtectionScenarioId);									
		$resultSelectProtectionScenarioId	= $this->conn->sql_query($selectProtectionScenarioId);		 
		$row = $this->conn->sql_fetch_object($resultSelectProtectionScenarioId);
		$pScenario_id = $row->referenceId;
		$logging_obj->my_error_handler("DEBUG","update_protection_scenario_session :: pScenario_id : $pScenario_id " );
		if($scenario_type == 'Planned Failover')
		{
			$logging_obj->my_error_handler("DEBUG","update_protection_scenario_session if :: scenario_id : $scenario_id :: hostId : $hostId :: scenario_type : $scenario_type " );
			$cond = "('ProductionServerPlannedFailover')";			
			$sql_A = "SELECT scenarioDetails FROM applicationScenario WHERE scenarioId = '$pScenario_id'";
			$rs = $this->conn->sql_query($sql_A);
			$rs = $this->conn->sql_fetch_object($rs);
			$scenario_details = $rs->scenarioDetails;
			$cdata_arr = $commonfunctions_obj->getArrayFromXML($scenario_details);
			$scenario_details = unserialize($cdata_arr['plan']['data']['value']);
		
			foreach ($scenario_details['reversepairInfo'] as $key => $val )
			{
				$tgt_host_id = $val['targetId'];
			}			
			$logging_obj->my_error_handler("DEBUG","update_protection_scenario_sessionafter :: scenario_id : $scenario_id :: hostId : $hostId :: scenario_type : $scenario_type " );			
		}
		else if ($scenario_type == 'Failback')
		{
			$cond = "('DRServerPlannedFailback')";
		}	
			
		$selectPolicyId =  "SELECT
								policyId
							FROM
								policy
							WHERE
								scenarioId = '$scenario_id' AND
								policyName IN $cond";
		$resultSelectPolicyId	= $this->conn->sql_query($selectPolicyId);	
		$logging_obj->my_error_handler("DEBUG","update_protection_scenario_session OUERY_VOLUMES_D :: ".$selectPolicyId);
		while($row = $this->conn->sql_fetch_object($resultSelectPolicyId))
		{
			$policy_id = $row->policyId;

			$updatePolicyId =  "UPDATE
									policy
								SET
									hostId = '$hostId'
								WHERE
									policyId = '$policy_id'";
		
			$logging_obj->my_error_handler("DEBUG","update_protection_scenario_session OUERY_VOLUMES_E :: ".$updatePolicyId);	
			$this->conn->sql_query($updatePolicyId);
		}	
	}		
}
?>