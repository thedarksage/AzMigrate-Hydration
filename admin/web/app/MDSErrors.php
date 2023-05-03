<?php
Class MDSErrors
{
	private $error_string;
	
	function __construct()
	{
		global $conn_obj;
		$this->conn_obj = $conn_obj;
	}
	
	public function GetErrorString($request_type, $reference_id)
	{
		$reference_id = implode(",",$reference_id);
		
		if($request_type == "PushInstall")
		{
			$error_string = $this->getPushInstallErrorData($reference_id);
		}
		if($request_type == "Protection")
		{
			$error_string = $this->getProtectionErrorData($reference_id);
		}
		if($request_type == "Rollback")
		{
			$error_string = $this->getRollbackErrorData($reference_id);
		}
		return $error_string;
	}
	
	private function getPushInstallErrorData($push_job_id)
	{
		//Fetching API Request details.
		$request_xml = $this->getAPIRequestData($push_job_id, 'PushInstall');
		
		if($request_xml == '') return $request_xml;
		
		//Fetching Agent installer details table info
		$push_install_sql = "SELECT ipaddress,lastUpdatedTime,startTime,endTime,statusMessage,errCode,errPlaceHolders FROM agentInstallerDetails WHERE FIND_IN_SET(jobId, ?)";
		$push_install_details = $this->conn_obj->sql_query($push_install_sql, array($push_job_id));
		
		foreach($push_install_details as $push_data)
		{
			$ip_address = $push_data['ipaddress'];
			$error_message .= "Mobility Service Install/Uninstall of host - ".$push_data['ipaddress']." started at  ".$push_data['startTime']." and completed at ".$push_data['endTime']." Failure - ".$push_data['statusMessage'];
		}
		
		$error_string = $request_xml."\n".$error_message;
		return $error_string;
	}

	private function getProtectionErrorData($scenario_id)
	{
		//Fetching API Request details.
		$request_xml = $this->getAPIRequestData($scenario_id, 'Protection');
		
		if($request_xml == '') return $request_xml;
		
		//Fetching application scenario table data
		$app_data_error_string = $this->getScenarioDetails($scenario_id, 'Protection');
		
		//Fetching policy and policy instance table details.
		list($plan_id, $policy_error_string) = $this->getPolicyData($scenario_id, 'Protection');
		
		$error_string = $request_xml."\n".$app_data_error_string."\n".$policy_error_string."\n";
		return $error_string;
	}
	
	private function getRollbackErrorData($reference_id)
	{
		//Fetching API Request details.
		$request_xml = $this->getAPIRequestData($reference_id, 'Rollback');
		if($request_xml == '') return $request_xml;
		
		//Fetching policy and policy instance table details.
		list($plan_id, $policy_error_string) = $this->getPolicyData($reference_id, 'Rollback');
		
		//Fetching application scenario table data
		$app_data_error_string = $this->getScenarioDetails($plan_id, 'Rollback');
		
		$error_string = $request_xml."\n".$app_data_error_string."\n".$policy_error_string;
		return $error_string;
	}
	
	private function getAPIRequestData($reference_id, $request_type)
	{
		$api_details = $this->getAPINames($request_type);
		
		//Fetching API Request details.
		$api_request_sql = "SELECT 
									requestXml, 
									requestTime
								FROM 
									apiRequest 
								WHERE 
									referenceId LIKE ? AND 
									FIND_IN_SET(requestType, ?) ORDER BY apiRequestId DESC LIMIT 1";
		$request_details = $this->conn_obj->sql_get_array($api_request_sql, "requestXml", "requestTime", array("%,$reference_id,%",implode(",", $api_details)));
		$request_xml = '';
		
		foreach($request_details as $xml => $request_time)
		{
			$xml = unserialize($xml);
			$request_xml .= "$request_type request was received at $request_time with the input xml as ".$xml;
		}
		return $request_xml;
	}
	
	private function getScenarioDetails($reference_id, $scenario_type)
	{
		$sql_args = array();
		if($scenario_type == 'Protection') 
		{
			$cond = "FIND_IN_SET(scenarioId, ?)";
			array_push($sql_args, $reference_id);
		}
		else
		{
			$cond = "FIND_IN_SET(p.planId, ?)";
			array_push($sql_args, implode(",", $reference_id));
		}
		
		//Fetching application scenario table data
		$app_sql = "SELECT 
							s.scenarioDetails, 
							s.planId, 
							FROM_UNIXTIME(p.planCreationTime) as planCreationTime, 
							p.applicationPlanStatus,
							p.planDetails
						FROM 
							applicationPlan p, 
							applicationScenario s 
						WHERE 
							p.planId = s.planId AND
							$cond";
		$app_details = $this->conn_obj->sql_query($app_sql, $sql_args);
		$scenario_data = '';
		foreach($app_details as $app_info)
		{
			$scenario_data .= "$scenario_type plan created at ".$app_info['planCreationTime']." with details - ".$app_info['scenarioDetails']." planStatus ".$app_info['applicationPlanStatus']." planId ".$app_info['planId']." planDetails ".$app_info['planDetails'];
		}
		return $scenario_data;
	}
	
	private function getPolicyData($reference_id, $request_type)
	{
		$sql_args = array();
		if($request_type == "Rollback") 
		{
			$cond = "FIND_IN_SET(p.policyId, ?)";
			array_push($sql_args,$reference_id,'48');
		}
		else
		{
			$cond = "FIND_IN_SET(scenarioId, ?)";
			array_push($sql_args,$reference_id,'5');
		}
		
		//Fetching policy and policy instance table details.
		$policy_sql = "SELECT 
								p.hostId, 
								p.policyId, 
								p.planId,
								p.policyTimestamp,
								pi.lastUpdatedTime, 
								pi.policyState, 
								pi.executionLog
							FROM 
								policyInstance pi, 
								policy p 
							WHERE 
								p.policyId = pi.policyId AND 
								$cond AND 
								policyType = ?";
		$policy_info = $this->conn_obj->sql_query($policy_sql, $sql_args);
		
		$plan_id_arr = array();
		foreach($policy_info as $policy_data)
		{
			$plan_id_arr[] = $policy_data['planId'];
			$policy_message .= "PolicyId - ".$policy_data['policyId']." HostGUID - ".$policy_data['hostId']." ErrorLog - ".$policy_data['executionLog']." ExecutionState - ".$policy_data['policyState']." policyStartTime - ".$policy_data['policyTimestamp'] ." LastUpdatedTime - ".$policy_data['lastUpdatedTime'];
		}		
		return array($plan_id_arr, $policy_message);
	}
	
	public function getAPINames($api_name)
	{
		$request_types = array(
							"Discovery" => array("InfrastructureDiscovery", "UpdateInfrastructureDiscovery"),
							"PushInstall" => array("InstallMobilityService", "UnInstallMobilityService", "UpdateMobilityService"),
							"Protection" => array("CreateProtection", "ModifyProtection"),
							"Rollback" => array("CreateRollback"),
							"ProtectionTimeout" => array("CreateProtection", "ModifyProtection"),
		);
		return isset($request_types[$api_name]) ? $request_types[$api_name] : array();
	}
}
?>