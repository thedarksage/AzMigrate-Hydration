<?php
/* 
//----------------------------------------------------------------
//  <copyright file="MigrationHandler.php" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  Description: V2A Migration handler methods for the required migration jobs.
//
//  History:     25-November-2021   srpatnan   Created
*/
class MigrationHandler extends ResourceHandler
{
	private $conn;
	private $policyObj;
	public function MigrationHandler() 
	{
		global $conn_obj;
		$this->conn = $conn_obj;
		$this->commonfunc_obj = new CommonFunctions();
		$this->policyObj = new Policy();
	}	
	
	public function MachineRegistrationToRcm_validate ($identity, $version, $functionName, $parameters)
	{	
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			$host_obj = $parameters['HostId'];
			if(!is_object($host_obj))
			{
				$message = "HostId Parameter Is Missed From request XML";
				$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
				$this->raiseException($returnCode,$message);
			}
			
			$source_host_id = $host_obj->getValue();
			$source_host_id = trim($source_host_id);
			if($source_host_id == "")
			{	
				$message = "HostId Value Is Missed From request XML";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
			
			$is_source = $this->commonfunc_obj->isValidSourceComponent($source_host_id);
			if(!$is_source)
			{
				$message = "Not a Valid hostId";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}

			$appliance_fqdn_obj = $parameters['ApplianceFqdn'];
			if(!is_object($appliance_fqdn_obj))
			{
				$message = "ApplianceFqdn Parameter Is Missed From request XML";
				$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
				$this->raiseException($returnCode,$message);
			}
			
			$appliance_fqdn = $appliance_fqdn_obj->getValue();
			$appliance_fqdn = trim($appliance_fqdn);
			if($appliance_fqdn == "")
			{	
				$message = "ApplianceFqdn Value Is Missed From request XML";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
			
			$ip_addresses_obj = $parameters['IpAddresses'];
			if(!is_object($ip_addresses_obj))
			{
				$message = "IpAddresses Parameter Is Missed From request XML";
				$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
				$this->raiseException($returnCode,$message);
			}
			
			$ip_addresses = $ip_addresses_obj->getValue();
			$ip_addresses = trim($ip_addresses);
			if($ip_addresses == "")
			{	
				$message = "IpAddresses Value Is Missed From request XML";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
			
			$rcm_proxy_port_obj = $parameters['RcmProxyPort'];
			if(!is_object($rcm_proxy_port_obj))
			{
				$message = "RcmProxyPort Parameter Is Missed From request XML";
				$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
				$this->raiseException($returnCode,$message);
			}
			
			$rcm_proxy_port = $rcm_proxy_port_obj->getValue();
			$rcm_proxy_port = trim($rcm_proxy_port);
			if($rcm_proxy_port == "")
			{	
				$message = "RcmProxyPort Value Is Missed From request XML";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
			
			$otp_obj = $parameters['Otp'];
			if(!is_object($otp_obj))
			{
				$message = "Otp Parameter Is Missed From request XML";
				$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
				$this->raiseException($returnCode,$message);
			}
			
			$otp = $otp_obj->getValue();
			$otp = trim($otp);
			if($otp == "")
			{	
				$message = "Otp Value Is Missed From request XML";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
			
			$is_credential_less_discovery_obj = $parameters['IsCredentialLessDiscovery'];
			if(!is_object($is_credential_less_discovery_obj))
			{
				$message = "IsCredentialLessDiscovery Parameter Is Missed From request XML";
				$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
				$this->raiseException($returnCode,$message);
			}
			
			$is_credential_less_discovery = $is_credential_less_discovery_obj->getValue();
			$is_credential_less_discovery = trim($is_credential_less_discovery);
			if($is_credential_less_discovery == "")
			{	
				$message = "IsCredentialLessDiscovery Value Is Missed From request XML";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
	}
	

	public function MachineRegistrationToRcm ($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			global $requestXML,  $activityId, $clientRequestId;
			$functionName = "MachineRegistrationToRcm";
			$source_host_id = trim($parameters['HostId']->getValue());
			$policy_params_data['HostId'] = $source_host_id;
			$app_scenario_data = $this->commonfunc_obj->get_application_scenario_data($source_host_id);
			
			foreach ($app_scenario_data as $key=>$value)
			{
				$app_scenario_id = $app_scenario_data['scenarioId'];
				$plan_id = $app_scenario_data['planId'];
			}
			
			$scenario_id_array["$source_host_id"] = $app_scenario_id;
            
			$policy_params_data['ApplianceFqdn'] = trim($parameters['ApplianceFqdn']->getValue());
			$policy_params_data['IpAddresses'] = trim($parameters['IpAddresses']->getValue());
			$policy_params_data['RcmProxyPort'] = trim($parameters['RcmProxyPort']->getValue());
			$policy_params_data['Otp'] = trim($parameters['Otp']->getValue());
			$policy_params_data['IsCredentialLessDiscovery'] = trim($parameters['IsCredentialLessDiscovery']->getValue());
			$policy_params_data['ClientRequestId'] = $clientRequestId;
			
			$policy_parameters = json_encode($policy_params_data);
			$policy_type = 61;
			$policy_scheduled_type = 2;
			$policy_run_every = 0;
			$policy_name = "RcmRegistration";
					
			$policy_id = $this->policyObj->insert_script_policy($source_host_id,$policy_type,$policy_scheduled_type,$policy_run_every,$app_scenario_id ,0,0,$policy_parameters,$policy_name,$plan_id);
			$policy_id_array["$source_host_id"] = $policy_id;
			$referenceIds = $referenceIds . $policy_id . ",";
			
			// insert API request details
			$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $referenceIds);
			$request_id = $api_ref['apiRequestId'];
			$requestedData = array("planId" => $plan_id, "scenarioId" => $scenario_id_array, "policyId" => $policy_id_array);
			$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($requestedData, $request_id);
			$response  = new ParameterGroup ( "Response" );
			$response->setParameterNameValuePair('RequestId', $request_id);
			
			// Disable Exceptions for DB
			$this->conn->sql_commit();
			$this->conn->disable_exceptions();
			
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
		{
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function FinalReplicationCycle_validate ($identity, $version, $functionName, $parameters)
	{	
		// Enable Exceptions for DB.
		$this->conn->enable_exceptions();
		
		$host_obj = $parameters['HostId'];
		if(!is_object($host_obj))
		{
			$message = "HostId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$source_host_id = $host_obj->getValue();
		$source_host_id = trim($source_host_id);
		if($source_host_id == "")
		{	
			$message = "HostId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$is_source = $this->commonfunc_obj->isValidSourceComponent($source_host_id);
		if(!$is_source)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
	
		$tag_type_obj = $parameters['BookmarkType'];
		if(!is_object($tag_type_obj))
		{
			$message = "BookmarkType Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$tag_type = $tag_type_obj->getValue();
		$tag_type = trim($tag_type);
		if($tag_type == "")
		{	
			$message = "BookmarkType Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$tag_guid_obj = $parameters['BookmarkId'];
		if(!is_object($tag_guid_obj))
		{
			$message = "BookmarkId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$tag_guid = $tag_guid_obj->getValue();
		$tag_guid = trim($tag_guid);
		if($tag_guid == "")
		{	
			$message = "BookmarkId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		// Disable Exceptions for DB
		$this->conn->disable_exceptions();
	}
	
	public function FinalReplicationCycle ($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			global $requestXML,  $activityId, $clientRequestId;
			$functionName = "FinalReplicationCycle";
			$source_host_id = trim($parameters['HostId']->getValue());
			$policy_params_data['HostId'] = $source_host_id;
			$app_scenario_data = $this->commonfunc_obj->get_application_scenario_data($source_host_id);
			foreach ($app_scenario_data as $key=>$value)
			{
				$app_scenario_id = $app_scenario_data['scenarioId'];
				$plan_id = $app_scenario_data['planId'];
			}
				
			$scenario_id_array["$source_host_id"] = $app_scenario_id;
            
			$policy_params_data['TagType'] = trim($parameters['BookmarkType']->getValue());
			$policy_params_data['TagGuid'] = trim($parameters['BookmarkId']->getValue());
			$policy_params_data['ClientRequestId'] = $clientRequestId;
			
			$policy_parameters = json_encode($policy_params_data);
			$policy_type = 62;
			$policy_scheduled_type = 2;
			$policy_run_every = 0;
			$policy_name = "RcmFinalReplicationCycle";
					
			$policy_id = $this->policyObj->insert_script_policy($source_host_id,$policy_type,$policy_scheduled_type,$policy_run_every,$app_scenario_id ,0,0,$policy_parameters,$policy_name,$plan_id);
			$policy_id_array["$source_host_id"] = $policy_id;
			$referenceIds = $referenceIds . $policy_id . ",";
			
			// insert API request details
			$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $referenceIds);
			$request_id = $api_ref['apiRequestId'];
			$requestedData = array("planId" => $plan_id, "scenarioId" => $scenario_id_array, "policyId" => $policy_id_array);
			$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($requestedData, $request_id);
			$response  = new ParameterGroup ( "Response" );
			$response->setParameterNameValuePair('RequestId', $request_id);
			
			// Disable Exceptions for DB
			$this->conn->sql_commit();
			$this->conn->disable_exceptions();
			
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
		{
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}

	public function MobilityAgentSwitchToRcm_validate ($identity, $version, $functionName, $parameters)
	{	
		// Enable Exceptions for DB.
		$this->conn->enable_exceptions();
		
		$host_obj = $parameters['HostId'];
		if(!is_object($host_obj))
		{
			$message = "HostId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$source_host_id = $host_obj->getValue();
		$source_host_id = trim($source_host_id);
		if($source_host_id == "")
		{	
			$message = "HostId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$is_source = $this->commonfunc_obj->isValidSourceComponent($source_host_id);
		if(!$is_source)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		// Disable Exceptions for DB
		$this->conn->disable_exceptions();
	}
	
	public function MobilityAgentSwitchToRcm ($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			global $requestXML,  $activityId, $clientRequestId;
			$functionName = "MobilityAgentSwitchToRcm";
			$source_host_id = trim($parameters['HostId']->getValue());
			$policy_params_data['HostId'] = $source_host_id;
			$app_scenario_data = $this->commonfunc_obj->get_application_scenario_data($source_host_id);
			foreach ($app_scenario_data as $key=>$value)
			{
				$app_scenario_id = $app_scenario_data['scenarioId'];
				$plan_id = $app_scenario_data['planId'];
			}
				
			$scenario_id_array["$source_host_id"] = $app_scenario_id;
            
			$policy_params_data['ClientRequestId'] = $clientRequestId;
			
			$policy_parameters = json_encode($policy_params_data);
			$policy_type = 64;
			$policy_scheduled_type = 2;
			$policy_run_every = 0;
			$policy_name = "RcmMigration";
					
			$policy_id = $this->policyObj->insert_script_policy($source_host_id,$policy_type,$policy_scheduled_type,$policy_run_every,$app_scenario_id ,0,0,$policy_parameters,$policy_name,$plan_id);
			$policy_id_array["$source_host_id"] = $policy_id;
			$referenceIds = $referenceIds . $policy_id . ",";
			
			// insert API request details
			$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $referenceIds);
			$request_id = $api_ref['apiRequestId'];
			$requestedData = array("planId" => $plan_id, "scenarioId" => $scenario_id_array, "policyId" => $policy_id_array);
			$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($requestedData, $request_id);
			$response  = new ParameterGroup ( "Response" );
			$response->setParameterNameValuePair('MobilityAgentSwitchToRcm', "Success");
			
			$update_migration_flag = "UPDATE hosts SET isGivenToMigration = 1 WHERE id = ?";
			$update_migration_flag_status = $this->conn->sql_query($update_migration_flag,array($source_host_id));
			
			// Disable Exceptions for DB
			$this->conn->sql_commit();
			$this->conn->disable_exceptions();
			
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
		{
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}

	public function ResumeReplicationToCS_validate ($identity, $version, $functionName, $parameters)
	{	
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			$host_obj = $parameters['HostId'];
			if(!is_object($host_obj))
			{
				$message = "HostId Parameter Is Missed From request XML";
				$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
				$this->raiseException($returnCode,$message);
			}
			
			$source_host_id = $host_obj->getValue();
			$source_host_id = trim($source_host_id);
			if($source_host_id == "")
			{	
				$message = "HostId Value Is Missed From request XML";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
			
			$is_source = $this->commonfunc_obj->isValidSourceComponent($source_host_id);
			if(!$is_source)
			{
				$message = "Not a Valid hostId";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
	}
	
	public function ResumeReplicationToCs ($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			global $requestXML,  $activityId, $clientRequestId;
			$functionName = "ResumeReplicationToCs";
			$source_host_id = trim($parameters['HostId']->getValue());
			$policy_params_data['HostId'] = $source_host_id;
			$app_scenario_data = $this->commonfunc_obj->get_application_scenario_data($source_host_id);
			foreach ($app_scenario_data as $key=>$value)
			{
				$app_scenario_id = $app_scenario_data['scenarioId'];
				$plan_id = $app_scenario_data['planId'];
			}
				
			$scenario_id_array["$source_host_id"] = $app_scenario_id;
            
			$policy_params_data['ClientRequestId'] = $clientRequestId;
			
			$policy_parameters = json_encode($policy_params_data);
			$policy_type = 63;
			$policy_scheduled_type = 2;
			$policy_run_every = 0;
			$policy_name = "RcmResumeReplication";
					
			$policy_id = $this->policyObj->insert_script_policy($source_host_id,$policy_type,$policy_scheduled_type,$policy_run_every,$app_scenario_id ,0,0,$policy_parameters,$policy_name,$plan_id);
			$policy_id_array["$source_host_id"] = $policy_id;
			$referenceIds = $referenceIds . $policy_id . ",";
			
			// insert API request details
			$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $referenceIds);
			$request_id = $api_ref['apiRequestId'];
			$requestedData = array("planId" => $plan_id, "scenarioId" => $scenario_id_array, "policyId" => $policy_id_array);
			$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($requestedData, $request_id);
			$response  = new ParameterGroup ( "Response" );
			$response->setParameterNameValuePair('RequestId', $request_id);
			
			// Disable Exceptions for DB
			$this->conn->sql_commit();
			$this->conn->disable_exceptions();
			
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
		{
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}	
}
?>