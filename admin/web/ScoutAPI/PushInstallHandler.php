<?php
class PushInstallHandler extends ResourceHandler 
{
	private $pushObj;
	private $commonfunctions_obj;
	private $SRMObj;
	private $processServer_obj;
	private $conn;
	private $global_conn;
	
	public function PushInstallHandler() 
	{
		global $conn_obj;
		 
		$this->pushObj = new PushInstall();
		$this->commonfunctions_obj = new CommonFunctions();
		$this->SRMObj = new SRM();
		$this->processServer_obj = new ProcessServer();
		$this->conn = new Connection();
		$this->global_conn = $conn_obj;
		$this->encryption_key = $this->commonfunctions_obj->getCSEncryptionKey();
        $this->account_obj = new Accounts();
	}
	
	/*
    * Function Name: InstallMobilityService
    *
    * Description: 
    * This function is to add push install request to CX
    *
    * Parameters:
    *     Param 1 [IN]: ServerIP
    *     Param 2 [IN]: OS
    *     Param 3 [IN]: UserName
    *     Param 4 [IN]: Password
    *     Param 5 [OUT]: RequestId of the PushInstall request
    * 
	* Defaults:
		AgentType - ScoutAgent (ScoutAgent/MasterTarget)    
    * Exceptions:
    *     NONE
    */
	
	public function InstallMobilityService_validate($identity, $version, $functionName, $parameters)
	{
        $function_name_user_mode = 'Install Mobility Service';
		try
		{	
			$this->global_conn->enable_exceptions();
			
			if(!$this->encryption_key) ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array('Parameter' => "Encryption key not found."));
			
			/* Validating the ConfigServer level details */
			$connectivity_type = $this->global_conn->sql_get_value("transbandSettings","ValueData","ValueName='CONNECTIVITY_TYPE'");
			if(isset($parameters['ConfigServer'])) 
			{
				$config_server = $parameters['ConfigServer']->getChildList();
								
				/* Validating Protocol */
				if(isset($config_server['Protocol']))
				{
					$protocol = trim($config_server['Protocol']->getValue());
					if(!$protocol) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Protocol"));
					elseif(!in_array(strtoupper($protocol), array("HTTP", "HTTPS"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Protocol"));
				}
				
				/* Validating CSIPAddress */
				if(isset($config_server['IPAddress']))
				{
					$ipaddress = trim($config_server['IPAddress']->getValue());
					if(!$ipaddress) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "IPAddress"));
					elseif(! filter_var($ipaddress, FILTER_VALIDATE_IP)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "IPAddress"));
				}
				
				/* Validating CS Port */
				if(isset($config_server['Port']))
				{
					$port = trim($config_server['Port']->getValue());
					if(!$port) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Port"));
					elseif(!is_numeric($port)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Port"));
				}
			}
			if($connectivity_type == "NON-VPN")
			{
				if(!$protocol || !$ipaddress || !$port) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Protocol/IPAddress/Port"));
			}
			
			#PushServerId - Madatory (only global field)
			if(!isset($parameters['PushServerId'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "PushServerId"));
			$push_server_id = $parameters['PushServerId']->getValue();
			if(!$push_server_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "PushServerId"));
			$push_server_data = $this->commonfunctions_obj->get_host_info($push_server_id);
			if(!$push_server_data || !$push_server_data['processServerEnabled']) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
			
			if($push_server_data['currentTime'] - $push_server_data['lastHostUpdateTimePs'] > 900)
				ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $push_server_data['name'], 'PsIP' => $push_server_data['ipaddress'], 'Interval' => ceil(($push_server_data['currentTime'] - $push_server_data['lastHostUpdateTimePs'])/60)));
			
			/* Validating the server level details */
			if(!isset($parameters['Servers'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Servers"));
			$servers = $parameters['Servers']->getChildList();	
			if(!isset($servers)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Server Installation Details"));
			
			/* Server level mandatory parameters */
			$mandatory_fields = array("HostIP", "OS");
			
			$host_ips = array();
			$account_ids = array();
			/* Loop for all the paramters */
			foreach ($servers as $param_name => $param_obj)
			{
				/* Server parameters validation */
				$server_params = $param_obj->getChildList();
				if(!is_array($server_params)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Server Details - ".$param_name));
				
				foreach ($mandatory_fields as $field)
				{
					if(!isset($server_params[$field])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $field." - ".$param_name));
					$field_value = $server_params[$field]->getValue();
					if(!$field_value) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA',  array('Parameter' => $field." - ".$param_name));
					if($field == "HostIP") 
					{
						/* Validating the IP address */
						if(filter_var($field_value, FILTER_VALIDATE_IP, FILTER_FLAG_IPV6)) ErrorCodeGenerator::raiseError($functionName, 'EC_IPV6_DATA', array('SourceIP' => "HostIP - $param_name",'ErrorCode' => "ECA0140"));
						else if(! filter_var($field_value, FILTER_VALIDATE_IP)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('SourceIP' => "HostIP - $param_name"));
						/* Validating duplicate Host IPs in same request */
						if(in_array($field_value, $host_ips)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HostIP. Duplicate HostIP - ".$param_name));
						$host_ips[] = $field_value;
					}
					elseif($field == "OS") 
					{
						/* Validating OS */
						if(!in_array($field_value, array("Linux", "Windows"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "OS - ".$param_name));
					}
					
				}
				if(!isset($server_params['AccountId']) && (!isset($server_params['UserName']) || !isset($server_params['Password']))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS',  array('Parameter' => "AccountId / UserName and Password"));
                
                if(isset($server_params['AccountId']))
				{
					$account_id = trim($server_params['AccountId']->getValue());
                    if(!$account_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "AccountId"));
					if(!in_array($account_id, $account_ids))
					{
						$account_exists = $this->account_obj->validateAccount($account_id);
						if(!$account_exists) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "AccountId"));
						$account_ids[] = $account_id;
					}
				}
                
				if(isset($server_params['InfrastructureVmId']))
				{
					$infrastructure_vmid = trim($server_params['InfrastructureVmId']->getValue());
                    if(!$infrastructure_vmid) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "InfrastructureVmId"));
				}
				
                if(isset($server_params['UserName']))
				{
					$user_name = trim($server_params['UserName']->getValue());
                    if(!$user_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "UserName"));
				}
                
                if(isset($server_params['Password']))
				{
					$password = trim($server_params['Password']->getValue());
                    if(!$password) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Password"));
				}
                
                if(isset($server_params['AgentType']))
				{
					$agent_type = $server_params['AgentType']->getValue();
					if(!in_array($agent_type, array("ScoutAgent", "MasterTarget"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA',  array('Parameter' => "AgentType - ".$param_name));
				}
			}
			/* Validating the server level details */
			if(!is_array($host_ips) || !count($host_ips)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS',  array('Parameter' => "Server Installation Details"));
			
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
	
	public function InstallMobilityService($identity, $version, $functionName, $parameters)
	{
		try
		{
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			/* To get the request XML to insert in to the API table */
			global $requestXML, $activityId, $CS_IP, $CS_PORT, $CS_SECURED_COMMUNICATION, $clientRequestId;
			
			$connectivity_type = $this->global_conn->sql_get_value("transbandSettings","ValueData","ValueName='CONNECTIVITY_TYPE'");
			
			if(isset($parameters['ConfigServer'])) $config_server = $parameters['ConfigServer']->getChildList();
			$communication_mode = ($CS_SECURED_COMMUNICATION) ? "HTTPS" : "HTTP";
			$cs_ip = (isset($config_server['IPAddress']) && $connectivity_type == 'NON VPN') ? trim($config_server['IPAddress']->getValue()) : $CS_IP;
			$cs_port = (isset($config_server['Port']) && $connectivity_type == 'NON VPN') ? trim($config_server['Port']->getValue()) : $CS_PORT;
			$protocol = (isset($config_server['Protocol']) && $connectivity_type == 'NON VPN') ? strtoupper(trim($config_server['Protocol']->getValue())) : $communication_mode;
				
			
			$push_server_id = $parameters['PushServerId']->getValue();			
			$servers = $parameters['Servers']->getChildList();
			
			$install_details = array();
			/* Loop for all the paramters */
			foreach ($servers as $param_name => $param_obj)
			{
				$server_details = array();
				$server_params = $param_obj->getChildList();
				$install_ip_address = trim($server_params['HostIP']->getValue());	
				$server_details['IPAddress'] = $install_ip_address;	
				$server_details['HostName'] = (isset($server_params['HostName'])) ? trim($server_params['HostName']->getValue()) : '';
				$server_details['DisplayName'] = (isset($server_params['DisplayName'])) ? trim($server_params['DisplayName']->getValue()) : '';
				$server_details['OSType'] = $server_params['OS']->getValue();
				$server_details['OSFlag'] = ($server_params['OS']->getValue() == "Windows") ? 1 : 2;
				$server_details['Domain'] = (isset($server_params['Domain'])) ? trim($server_params['Domain']->getValue()) : '';
                if(isset($server_params['AccountId']))
                {
                    $server_details['AccountId'] = trim($server_params['AccountId']->getValue());	
                }
                else
                {
                    $server_details['UserName'] = trim($server_params['UserName']->getValue());
                    $server_details['Password'] = trim($server_params['Password']->getValue());
                }
				$server_details['AgentType'] = (isset($server_params['AgentType'])) ? $server_params['AgentType']->getValue() : 'ScoutAgent';
				$server_details['InfrastructureVmId'] = trim($server_params['InfrastructureVmId']->getValue());
				$server_details['PushServerId'] = $push_server_id;
				$server_details['CSIP'] = $cs_ip;
				$server_details['CSPort'] = $cs_port;
				$server_details['Protocol'] = $protocol;
				$install_details[$param_name] = $server_details;
			}
			
			/* Insert the push install request for all the hosts */
			$install_add_result = $this->pushObj->add_push_install_request($install_details);	
			
			if(count($install_add_result))
			{
				$requestedData = array($install_add_result);
				foreach($install_add_result as $key => $job_id)
				{
					$job_ids = $job_ids . $job_id . ',';		
				}
				$job_ids = ',' . $job_ids ;			
				$api_ref = $this->commonfunctions_obj->insertRequestXml($requestXML,$functionName, $activityId, $clientRequestId, $job_ids);
				$request_id = $api_ref['apiRequestId'];
				$apiRequestDetailId = $this->commonfunctions_obj->insertRequestData($requestedData,$request_id);
			}
			
			$response  = new ParameterGroup ( "Response" );
			if(!is_array($api_ref) || !$request_id || !$apiRequestDetailId)
			{		
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(),  "Failed due to some internal error");
			}
			else
			{
				$response->setParameterNameValuePair('RequestId',$request_id);					
			}

			// Disable Exceptions for DB
			$this->global_conn->sql_commit();
			$this->global_conn->disable_exceptions();
			return $response->getChildList();
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
	
	/*
    * Function Name: UnInstallMobilityService
    *
    * Description: 
    * This function is to add push uninstall request to CX
    *
    * Parameters:
    *     Param 1 [IN]: HostId
    *     Param 3 [IN]: UserName
    *     Param 4 [IN]: Password
    *     Param 5 [OUT]: RequestId of the PushInstall Uninstall request
    * 
	* Defaults:		
    * Exceptions:
    *     NONE
    */
	
	public function UnInstallMobilityService_validate($identity, $version, $functionName, $parameters)
	{	
        $function_name_user_mode = 'Uninstall Mobility Service';
		try
		{
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Invalid API call."));

			$this->global_conn->enable_exceptions();
			
			if(!$this->encryption_key) ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array('Parameter' => "Encryption key not found."));
			
			/* Server level mandatory parameters */
			if(!isset($parameters['Servers'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Servers"));
			$servers = $parameters['Servers']->getChildList();
			if(!is_array($servers)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Server uninstallation Details"));
			
			$host_ips = array();
			$host_ids = array();
			$account_ids = array();
			/* Loop for all the paramters */
			foreach ($servers as $param_name => $param_obj)
			{
				/* Server parameters validation */
				$server_params = $param_obj->getChildList();
				if(!is_array($server_params)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Server Details - ".$param_name));
				
				/* HostId mandatory parameter for uninstall */
				if(!isset($server_params['HostId'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "HostId - ".$param_name));			
				$host_id = $server_params['HostId']->getValue();
				if(!$host_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "HostId - ".$param_name));
				
				// Validate the Host is not already part of the earlier request
				if(in_array($host_id, $host_ids)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HostId. Duplicate HostId - ".$param_name));
				$host_ids[] = $host_id;
				
				// Validating the agent is already installed
				$installed = $this->commonfunctions_obj->verify_agent($host_id);
				if(!$installed) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECORD_INFO', array(), 'No data availabe for HostId - ' . $host_id);

				/* Validate if the host id is installed using PushInstall 
				*	If not we need the username, password and PushServerId
				*/
				$host_ip = $this->commonfunctions_obj->getHostIpaddress($host_id);
				$state = $this->pushObj->push_install_check_status($host_ip);
				if(!$state)
				{
					if(!isset($server_params['AccountId']) && (!isset($server_params['UserName']) || !isset($server_params['Password']))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "AccountId / UserName and Password - ".$param_name));
					if(isset($server_params['AccountId']))
					{
						$account_id = trim($server_params['AccountId']->getValue());
						if(!$account_id)	ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "AccountId"));
						if(!in_array($account_id, $account_ids))
						{
							$account_exists = $this->account_obj->validateAccount($account_id);
							if(!$account_exists) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA',  array('Parameter' => "AccountId"));
							$account_ids[] = $account_id;
						}
					}
                    if(isset($server_params['UserName']) && (!trim($server_params['UserName']->getValue()))) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "UserName - ".$param_name));
					if(isset($server_params['Password']) && !trim($server_params['Password']->getValue())) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Password - ".$param_name));
					if(!isset($server_params['PushServerId']))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "PushServerId - ".$param_name));
				}				
				/* Validate the PushServerId if provided */
				if(isset($server_params['PushServerId']))
				{
					$push_server_id = $server_params['PushServerId']->getValue();
					if(!$push_server_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "PushServerId - ".$param_name));
					$valid_push_server = $this->pushObj->validatePS($push_server_id);
					if(!$valid_push_server) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
				}			
				$host_ips[] = $host_ip;
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
	
	public function UnInstallMobilityService($identity, $version, $functionName, $parameters)
	{
		try
		{
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			global $requestXML, $activityId, $clientRequestId;
			
			/* Server level mandatory parameters */
			$servers = $parameters['Servers']->getChildList();
			
			/* Loop for all the paramters */
			$uninstall_details = array();
			foreach ($servers as $param_name => $param_obj)
			{			
				$server_params = $param_obj->getChildList();
				
				/* HostId mandatory parameter for uninstall */
				$host_id = $server_params['HostId']->getValue();
				
				$uninstall_details[$param_name]['HostId'] = $host_id;
				$host_info = $this->pushObj->get_host_info($host_id);
				$state = $this->pushObj->push_install_check_status($host_info[$host_id]['ipAddress']);
				//$uninstall_details[$param_name]['IPAddress'] = $host_ip;
				$uninstall_details[$param_name]['IPAddress'] = $host_info[$host_id]['ipAddress'];
				$uninstall_details[$param_name]['OS'] = $host_info[$host_id]['osFlag'];
				/* Other optional parameters */
				$uninstall_details[$param_name]['AccountId'] = (isset($server_params['AccountId'])) ? $server_params['AccountId']->getValue() : '';
				$uninstall_details[$param_name]['UserName'] = (isset($server_params['UserName'])) ? $server_params['UserName']->getValue() : '';
				$uninstall_details[$param_name]['Password'] = (isset($server_params['Password'])) ? $server_params['Password']->getValue() : '';
				$uninstall_details[$param_name]['PushServerId'] = (isset($server_params['PushServerId'])) ? $server_params['PushServerId']->getValue() : '';			
			}
			
			$uninstall_add_result = $this->pushObj->add_push_uninstall_request($uninstall_details);	
			
			
			if(count($uninstall_add_result))
			{
				foreach($uninstall_add_result as $key => $value)
				{
					foreach($value as $key1 => $job_id)
					{
						if ($key1 == 'hostId')continue;
						$job_ids = $job_ids . $job_id . ',';
					}	
				}
				$job_ids = ',' . $job_ids ;
			}		

			$requestedData = array($uninstall_add_result);
			$api_ref = $this->commonfunctions_obj->insertRequestXml($requestXML,$functionName,$activityId,$clientRequestId,$job_ids);
			$request_id = $api_ref['apiRequestId'];
			$apiRequestDetailId = $this->commonfunctions_obj->insertRequestData($requestedData,$request_id);				
			
			$response  = new ParameterGroup ( "Response" );
			if(!is_array($api_ref) || !$request_id || !$apiRequestDetailId)
			{		
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(),  "Failed due to some internal error");
			}
			else
			{
				$response->setParameterNameValuePair('RequestId',$request_id);					
			}
			
			// Disable Exceptions for DB
			$this->global_conn->sql_commit();
			$this->global_conn->disable_exceptions();
			return $response->getChildList();
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
	
	/*
    * Function Name: InstallMobilityService
    *
    * Description: 
    * This function is to add push install request to CX
    *
    * Parameters:
    *     Param 1 [IN]: ServerIP
    *     Param 2 [IN]: OS
    *     Param 3 [IN]: UserName
    *     Param 4 [IN]: Password
    *     Param 5 [OUT]: RequestId of the PushInstall request
    * 
	* Defaults:
		AgentType - ScoutAgent (ScoutAgent/MasterTarget)    
    * Exceptions:
    *     NONE
    */
	
	public function UpdateMobilityService_validate($identity, $version, $functionName, $parameters)
	{
		try
		{	
			$this->global_conn->enable_exceptions();
			
			$function_name_user_mode = 'Update Mobility Service';
			if(!$this->encryption_key) ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array('Parameter' => "Encryption key not found."));
			
			$connectivity_type = $this->global_conn->sql_get_value("transbandSettings","ValueData","ValueName='CONNECTIVITY_TYPE'");
			/* Validating the ConfigServer level details */
			if(isset($parameters['ConfigServer'])) 
			{
				$config_server = $parameters['ConfigServer']->getChildList();
								
				/* Validating Protocol */
				if(isset($config_server['Protocol']))
				{
					$protocol = trim($config_server['Protocol']->getValue());
					if(!$protocol) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Protocol"));
					elseif(!in_array(strtoupper($protocol), array("HTTP", "HTTPS"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Protocol"));
				}
				
				/* Validating CSIPAddress */
				if(isset($config_server['IPAddress']))
				{
					$ipaddress = trim($config_server['IPAddress']->getValue());
					if(!$ipaddress) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "IPAddress"));
					elseif(! filter_var($ipaddress, FILTER_VALIDATE_IP)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "IPAddress"));
				}
				
				/* Validating CS Port */
				if(isset($config_server['Port']))
				{
					$port = trim($config_server['Port']->getValue());
					if(!$port) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Port"));
					elseif(!is_numeric($port)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Port"));
				}
			}
			if($connectivity_type == "NON-VPN")
			{
				if(!$protocol || !$ipaddress || !$port) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Protocol/IPAddress/Port"));
			}
			
			/*PushServerId - Mandatory (only global field) */
			if(!isset($parameters['PushServerId'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "PushServerId"));
			$push_server_id = $parameters['PushServerId']->getValue();
			if(!$push_server_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "PushServerId"));
			
			/* Fetching Push Server Details from hosts table */
			$push_server_data = $this->commonfunctions_obj->get_host_info($push_server_id);
			if(!$push_server_data || !$push_server_data['processServerEnabled']) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
			
			if($push_server_data['currentTime'] - $push_server_data['lastHostUpdateTimePs'] > 900)
				ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $push_server_data['name'], 'PsIP' => $push_server_data['ipaddress'], 'Interval' => ceil(($push_server_data['currentTime'] - $push_server_data['lastHostUpdateTimePs'])/60)));
			
			/* Validating the server level details */
			if(!isset($parameters['Servers'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Servers"));
			$servers = $parameters['Servers']->getChildList();	
			if(!isset($servers)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Server Installation Details"));
			
			/* Server level mandatory parameters */
			$mandatory_fields = array("HostId", "UpdateType");
			
			$host_ids = array();
			$account_ids = array();
			
			/* Loop for all the paramters */
			foreach ($servers as $param_name => $param_obj)
			{
				/* Server parameters validation */
				$server_params = $param_obj->getChildList();
				if(!is_array($server_params)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Server Details - ".$param_name));
				
				if(!isset($server_params["HostId"])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $field." - ".$param_name));
				$host_id = trim($server_params["HostId"]->getValue());
				if(!$host_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA',  array('Parameter' => $host_id." - ".$param_name));
					
				if($host_id) 
				{
					/* Validating duplicate Host IPs in same request */
					if(in_array($host_id, $host_ids)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HostId. Duplicate HostId - ".$param_name));
					$host_ids[] = $source_host_id = $host_id;
				}
				
				if(isset($server_params["UpdateType"])) 
				{
					$update_type = trim($server_params["UpdateType"]->getValue());
					if(!$update_type) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA',  array('Parameter' => $update_type." - ".$param_name));
					/* Validating OS */
					if(!in_array($update_type, array("Update", "Upgrade"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "UpdateType - ".$param_name));
				}
							
				if(!isset($server_params['AccountId']) && (!isset($server_params['UserName']) || !isset($server_params['Password']))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS',  array('Parameter' => "AccountId / UserName and Password"));
                
                if(isset($server_params['AccountId']))
				{
					$account_id = trim($server_params['AccountId']->getValue());
                    if(!$account_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "AccountId"));
					if(!in_array($account_id, $account_ids))
					{
						$account_exists = $this->account_obj->validateAccount($account_id);
						if(!$account_exists) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "AccountId"));
						$account_ids[] = $account_id;
					}
				}
                
				if(isset($server_params['InfrastructureVmId']))
				{
					$infrastructure_vmid = trim($server_params['InfrastructureVmId']->getValue());
                    if(!$infrastructure_vmid) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "InfrastructureVmId"));
				}
				
                if(isset($server_params['UserName']))
				{
					$user_name = trim($server_params['UserName']->getValue());
                    if(!$user_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "UserName"));
				}
				
				 if(isset($server_params['Domain']))
				{
					$domain = trim($server_params['Domain']->getValue());
                    if(!$domain) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Domain"));
				}
                
                if(isset($server_params['Password']))
				{
					$password = trim($server_params['Password']->getValue());
                    if(!$password) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Password"));
				}
                
                if(isset($server_params['AgentType']))
				{
					$agent_type = $server_params['AgentType']->getValue();
					if(!in_array($agent_type, array("ScoutAgent", "MasterTarget"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA',  array('Parameter' => "AgentType - ".$param_name));
				}
			}
			/* Validating the server level details */
			if(!is_array($host_ids) || !count($host_ids)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS',  array('Parameter' => "Server Installation Details"));
			
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
	
	public function UpdateMobilityService($identity, $version, $functionName, $parameters)
	{
		try
		{
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			/* To get the request XML to insert in to the API table */
			global $requestXML, $activityId, $CS_IP, $CS_PORT, $CS_SECURED_COMMUNICATION, $clientRequestId;
			
			$connectivity_type = $this->global_conn->sql_get_value("transbandSettings","ValueData","ValueName='CONNECTIVITY_TYPE'");
			
			$patch_info = $this->commonfunctions_obj->getPatchDetails();
			$agent_version_details = $this->commonfunctions_obj->getPatchVersion();
			
			if(isset($parameters['ConfigServer'])) $config_server = $parameters['ConfigServer']->getChildList();
			$communication_mode = ($CS_SECURED_COMMUNICATION) ? "HTTPS" : "HTTP";
			$cs_ip = (isset($config_server['IPAddress']) && $connectivity_type == 'NON VPN') ? trim($config_server['IPAddress']->getValue()) : $CS_IP;
			$cs_port = (isset($config_server['Port']) && $connectivity_type == 'NON VPN') ? trim($config_server['Port']->getValue()) : $CS_PORT;
			$protocol = (isset($config_server['Protocol']) && $connectivity_type == 'NON VPN') ? strtoupper(trim($config_server['Protocol']->getValue())) : $communication_mode;
			
			$push_server_id = $parameters['PushServerId']->getValue();			
			$servers = $parameters['Servers']->getChildList();
			
			$install_details = $install_job_exists = $install_add_result = array();
			$update_exists = 0;
			/* Loop for all the paramters */
			foreach ($servers as $param_name => $param_obj)
			{
				$server_details = array();
				$latest_version_exists = 0;
				
				$server_params = $param_obj->getChildList();
				$host_id = trim($server_params['HostId']->getValue());
				$host_details = $this->global_conn->sql_query("SELECT ipaddress, osFlag, osType, prod_version, id FROM hosts WHERE id = ?",  array($host_id));
				foreach($host_details as $host_data)
				{
					$os_flag = $host_data["osFlag"];
					$ip_address = $host_data["ipaddress"];
					$host_version = str_replace(".","",$host_data["prod_version"]);
					$os_name = ($os_flag == 1) ? "Windows" : $host_data["osType"];
				}
				
				$update_type = $patch_info[$os_name]["updateType"];
				
				if($update_type == "Upgrade")
				{
					$avl_upgrade_version = str_replace(".","",$patch_info[$os_name]["patchVersion"]);
					
					/* In case host product version is greater than available patch version throw error*/
					if($host_version >= $avl_upgrade_version) $latest_version_exists = 1;
				}
				elseif($update_type == "Update")
				{
					$avl_update_version = str_replace(".","",$patch_info[$os_name]["patchVersion"]);
					$agent_patch_version = $agent_version_details[$host_id];
					
					/* In case agent patch version or host product version is greater than available patch version throw error*/
					if($agent_patch_version >= $avl_update_version || $host_version >= $avl_update_version) $latest_version_exists = 1;	
				}
				
				if($latest_version_exists == 0)
				{
					$server_details['IPAddress'] = $ip_address;
					$server_details['BuildName'] = $patch_info[$os_name]["patchFilename"];
					$server_details['PatchVersion'] = $patch_info[$os_name]["patchVersion"];
					$server_details['RebootRequired'] = $patch_info[$os_name]["rebootRequired"];
					$server_details['UpdateType'] = $patch_info[$os_name]["updateType"];
					$server_details['OSFlag'] = $os_flag;
					$server_details['OSType'] = ($os_flag == 1) ? "Windows" : "Linux";
					
					if(isset($server_params['AccountId']))
					{
						$server_details['AccountId'] = trim($server_params['AccountId']->getValue());	
					}
					else
					{
						$server_details['UserName'] = trim($server_params['UserName']->getValue());
						$server_details['Password'] = trim($server_params['Password']->getValue());
						$server_details['Domain'] = (isset($server_params['Domain'])) ? trim($server_params['Domain']->getValue()) : '';
					}
					$server_details['AgentType'] = (isset($server_params['AgentType'])) ? $server_params['AgentType']->getValue() : 'ScoutAgent';
					$server_details['PushServerId'] = $push_server_id;
					$server_details['CSIP'] = $cs_ip;
					$server_details['CSPort'] = $cs_port;
					$server_details['Protocol'] = $protocol;
					$server_details['InfrastructureVmId'] = trim($server_params['InfrastructureVmId']->getValue());
					$install_details[$param_name] = $server_details;
					$update_exists = 1;
				}
				else
				{
					$request_type = ($update_type == "Update") ? 2 : 1;
					$job_id = $installation_exists = $this->conn->sql_get_value("agentinstallerdetails", "jobId", "ipaddress = ? AND upgradeRequest = $request_type", array($ip_address));
					$install_job_exists[$param_name] = (int) $job_id;
				}
			}
			
			/* Insert the push install request for all the hosts */
			if($update_exists == 1) $install_add_result = $this->pushObj->add_push_install_request($install_details);	
			if(count($install_job_exists) > 0) $install_add_result = array_merge($install_add_result, $install_job_exists);
			
			if(count($install_add_result))
			{
				$requestedData = array($install_add_result);
				foreach($install_add_result as $key => $job_id)
				{
					$job_ids = $job_ids . $job_id . ',';		
				}
				$job_ids = ',' . $job_ids ;			
				$api_ref = $this->commonfunctions_obj->insertRequestXml($requestXML,$functionName, $activityId, $clientRequestId, $job_ids);
				$request_id = $api_ref['apiRequestId'];
				$apiRequestDetailId = $this->commonfunctions_obj->insertRequestData($requestedData,$request_id);
			}
			
			$response  = new ParameterGroup ( "Response" );
			if(!is_array($api_ref) || !$request_id || !$apiRequestDetailId)
			{		
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(),  "Failed due to some internal error");
			}
			else
			{
				$response->setParameterNameValuePair('RequestId',$request_id);					
			}

			// Disable Exceptions for DB
			$this->global_conn->sql_commit();
			$this->global_conn->disable_exceptions();
			return $response->getChildList();
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
}
?>