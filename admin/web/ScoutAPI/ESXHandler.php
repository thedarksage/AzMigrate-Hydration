<?php
class ESXHandler extends ResourceHandler
{
	private $conn;
	private $commonfunctionObj;
	private $esxDiscoveryObj;    
	private $global_conn;
	
	public function ESXHandler() 
	{
        global $conn_obj;
		$this->conn = new Connection();
		$this->global_conn = $conn_obj;
		$this->commonfunctionObj = new CommonFunctions();
		$this->esxDiscoveryObj = new ESXDiscovery();
		$this->account_obj = new Accounts();
	}
	
	/*
    * Function Name: GetCloudDiscovery
    *
    * Description: 
    * This function is to deliver PS its dicovery jobs
    *
    * Parameters:
    *     Param 1 [IN]: HostId
    *     Param 2 [OUT]: All the discovery jobs for the PS with HostId
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
	
	public function GetCloudDiscovery_validate ($identity, $version, $functionName, $parameters) 
	{
		if(isset($parameters['HostId'])) {
			$hostId = trim($parameters['HostId']->getValue());
			if(!$hostId) {
				$this->raiseException(ErrorCodes::EC_NO_DATA,"No Data found for HostId Parameter");
			}
		}
	}
	
	public function GetCloudDiscovery($identity, $version, $functionName, $parameters) 
	{
		$response = new ParameterGroup ( "Response" );
		
		if(isset($parameters['HostId'])) {
			$hostId = trim($parameters['HostId']->getValue());
			if(!($this->commonfunctionObj->getHostName($hostId))) {
				$this->raiseException(ErrorCodes::EC_NO_AGENTS,"No Agent found with the given HostIdentification Id");
			}
		}
		
		$result = $this->esxDiscoveryObj->getDiscoveryJobs();
		
		foreach($result as $invDetails) {
			$inventoryId = $invDetails['inventoryId'];
			
			$inventory_item = new ParameterGroup ($inventoryId);
			foreach ($invDetails as $key => $value)
			{
				$inventory_item->setParameterNameValuePair($key,$value);
			}			
			$response->addParameterGroup ($inventory_item);
		}
		return $response->getChildList();
	}
	
	/*
    * Function Name: InfrastructureDiscovery
    *
    * Description: 
    * This function is to add a discovery job at primary side through CX API
    *
    * Parameters:
    *     Param 1 [IN]: Refer the RequestSample XML
    *     Param 2 [OUT]: RequestId for the API
    *     
    * Exceptions:
    *     SQL exception on transaction failure
    */
    
    public function InfrastructureDiscovery_validate($identity, $version, $functionName, $parameters) 
    {
        $function_name_user_mode = 'Infrastructure Discovery';
        try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			
			// Infrastructure mandatory fields
			$infrastructures_mandatory_fields = array(
														"vCenter" => array ("ServerIP", "DiscoveryAgent"),
														"Physical" => array ("ServerIP", "OS")
													 );
													 /*
														"vSphere" => array ("ServerIP", "LoginId", "Password", "DiscoveryAgent")
														"HyperV" => array ("ServerIP"),
														"vSphere" => array ("ServerIP", "LoginId", "Password", "DiscoveryAgent"),
														"vCloud" => array ("ServerIP", "LoginId", "Password", "DiscoveryAgent", "Organization"),
														"AWS" => array ("AccountName", "AccessKeyID", "SecretAccessKey"),
														"Azure" => array ("AccountName", "SubscriptionID"),
													);*/
			
			if(!isset($parameters['Infrastructures']))
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'Infrastructures'));
			}
			
			# Validating infrastructures
			$infrastructures = $parameters['Infrastructures']->getChildList();
			if(!count($infrastructures))
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'Infrastructures'));
			}
			
			foreach( $infrastructures as $index => $infra_obj)
			{
				if($infrastructures[$index])
				{
					$inv_obj = $infrastructures[$index]->getChildList();
					
					if(isset($inv_obj['InfrastructureId']))
					{
						$inventory_id = $inv_obj['InfrastructureId']->getValue();
						if(!trim($inventory_id)){
							ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'InfrastructureId'));
						}
						
						$discovery_type = $this->global_conn->sql_get_value("infrastructureInventory","hostType","inventoryId = ?", array($inventory_id));
					}
					else
					{
						if(!$inv_obj['DiscoveryType']) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'DiscoveryType'));                
						if(isset($inv_obj['DiscoveryType']))
						{
							$discovery_type = $inv_obj['DiscoveryType']->getValue();
							if(!trim($discovery_type)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'DiscoveryType - ' . $index));
						}	
					}
				   
					/* vCloud, vCenter, vSphere, HyperV */
					if(array_key_exists($discovery_type, $infrastructures_mandatory_fields))
					{
						if(isset($inv_obj['InfrastructureType']))
						{
							$infra_type = $inv_obj['InfrastructureType']->getValue();
							if(!trim($infra_type)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'InfrastructureType'));
							/*
							When we will add the Target Cloud discovery to CS, we can open this
							*/
							//elseif($infra_type !== "Primary" && $infra_type != "Secondary") ErrorCodeGenerator::raiseError(COMMON, EC_INVALID_DATA, "InfrastructureType - $index");
							elseif($infra_type !== "Primary") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => 'InfrastructureType - ' . $index));
						}
						
						if(isset($inv_obj['SiteName']))
						{
							$site_name = $inv_obj['SiteName']->getValue();
							if(!trim($site_name)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'SiteName'));
						}
						
						if(isset($inv_obj['HostName']))
						{
							$host_name = $inv_obj['HostName']->getValue();
							if(!trim($host_name)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'HostName'));
						}
						
						if($discovery_type != 'Physical')
						{
							if(!isset($inv_obj['AccountId']) && (!isset($inv_obj['LoginId']) || !isset($inv_obj['Password'])))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "AccountId / LoginId and Password"));
							
							if(isset($inv_obj['AccountId']))
							{
								$account_id = $inv_obj['AccountId']->getValue();
								if(!trim($account_id))	 ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'AccountId'));
								// Validate the account id
								$account_exists = $this->account_obj->validateAccount($account_id);
								if(!$account_exists)	ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => 'AccountId'));
							}
							
							if(isset($inv_obj['LoginId']))
							{
								$username = $inv_obj['LoginId']->getValue();
								if(trim(!$username))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "LoginId - $index"));
							}
							
							if(isset($inv_obj['Password']))
							{
								$password = $inv_obj['Password']->getValue();
								if(trim(!$password))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Password - $index"));
							}
						}
						
						foreach($infrastructures_mandatory_fields[$discovery_type] as $param)
						{
							// When the mandatory params are not supplied.                           
							if(!$inventory_id && !isset($inv_obj[$param]))  ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $param . ' - ' . $index));
							
							$param_obj = $inv_obj[$param];
							if(isset($param_obj) && !trim($param_obj->getValue()))
							{
								ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $param . ' - ' . $index));
							}
							elseif(isset($param_obj) && $param == "DiscoveryAgent")
							{
								$ps_host_id = $param_obj->getValue();
								$ps_details = $this->commonfunctionObj->get_host_info($ps_host_id);
								// Validating the DiscoveryAgent provided is a valid PS pointing to this CS.
								if(!$ps_details || !is_array($ps_details)) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
								
								if($ps_details['currentTime'] - $ps_details['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ps_details['name'], 'PsIP' => $ps_details['ipaddress'], 'Interval' => ceil(($ps_details['currentTime'] - $ps_details['lastHostUpdateTimePs'])/60)));
							}
							elseif(isset($param_obj) && $param == "ServerIP")
							{
								$server_ip_port = $param_obj->getValue();
                                list($server_ip, $server_port) = explode(":", $server_ip_port);
								// Validating the IP Address
								if($discovery_type == "Physical" && ! filter_var(trim($server_ip), FILTER_VALIDATE_IP)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => 'ServerIP - ' . $index));
								$server_port = trim($server_port);
								if($server_port && ($server_port <= 0 || $server_port > 65535)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => 'Server Port - ' . $index));
								
								// Validate only in case of Physical server and throw the error
								if($discovery_type == "Physical")
								{									
									if(isset($inv_obj['HostId']))
									{
										$req_host_id = $inv_obj['HostId']->getValue();
										if(!trim($req_host_id)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'HostId'));
										$req_host_id_details = $this->commonfunctionObj->get_host_info($req_host_id);
										#print_r($req_host_id_details);
										
										// Validating the DiscoveryAgent provided is a valid PS pointing to this CS.
										if(!$req_host_id_details || !is_array($req_host_id_details)) 
										{
											ErrorCodeGenerator::raiseError($functionName, 'EC_NO_HOST_FOUND', array('ErrorCode' => ErrorCodes::EC_NO_HOST_FOUND, 'HostIdHolder' => $req_host_id , 'ServerIP' => $server_ip));
										}
									}
									$vm_exists = $this->esxDiscoveryObj->checkVmExists($server_ip); 	 
									if($vm_exists) ErrorCodeGenerator::raiseError($functionName, 'EC_VM_EXISTS', array('ServerIP' => $server_ip));
									
									// Validate if physical discovery is for PS/MT.
									$isServerHasPSEnabled = $this->esxDiscoveryObj->validatePhysicalServer($server_ip); 	 
									if($isServerHasPSEnabled) ErrorCodeGenerator::raiseError($functionName, 'EC_PS_MT_CAN_NOT_BE_PROTECTED',array('ServerIP' => $server_ip));
								}
							}
							elseif(isset($param_obj) && $param == "OS")
							{
								$os_type = $param_obj->getValue();
								// Validation of OS. It can be Linux/Windows only
								if($os_type != "Linux" && $os_type != "Windows") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => 'OS - ' . $index));
							}
						}
					}
					else
					{   
						// If the given discovery type is not mentioned in the valid discovery type array
						ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => 'DiscoveryType - ' . $index));
					}
				}	
			}			
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), 'Database error occurred');
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
	
	public function InfrastructureDiscovery($identity, $version, $functionName, $parameters) 
    {
        try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			global $requestXML, $activityId, $clientRequestId; 		
			$infrastructures = $parameters['Infrastructures']->getChildList();
			
			$inv_add_result = array();
			$inv_request_data = array();
			
			foreach ($infrastructures as $index => $infra_obj)
			{   
				$post_details = array();
				if($infrastructures[$index])
				{
					$inv_obj = $infrastructures[$index]->getChildList();
					
					// Get all the input fields
					$infrastructureType = (isset($inv_obj['InfrastructureType'])) ? $inv_obj['InfrastructureType']->getValue() : "Primary";
					
					$discovery_type = $inv_obj['DiscoveryType']->getValue();
					
					$site_name = (isset($inv_obj['SiteName'])) ? $inv_obj['SiteName']->getValue() : '';
					$post_details['inventoryid'] = (isset($inv_obj['InfrastructureId'])) ? $inv_obj['InfrastructureId']->getValue() : "";
					$post_details['new_site'] = ($site_name) ? $site_name : 'VDC1';
					$post_details['servertype'] = $discovery_type;
					
					/* vCloud, vCenter, vSphere, HyperV */
					if(in_array($discovery_type, array("vCloud", "vCenter", "vSphere", "HyperV")))
					{
						list($post_details['ipaddress'], $post_details['port']) = explode(":", $inv_obj['ServerIP']->getValue());
						$post_details['ipaddress'] = trim($post_details['ipaddress']);
                        // Defaulting the port to 443 if nothing is specified
                        $post_details['port'] = ($post_details['port'] == "") ? 443 : trim($post_details['port']);
                        if(isset($inv_obj['AccountId']))
                        {
                            $post_details['accountid'] = trim($inv_obj['AccountId']->getValue());
                        }
                        else
                        {
                            $post_details['loginid'] = trim($inv_obj['LoginId']->getValue());
                            $post_details['loginpwd'] = trim($inv_obj['Password']->getValue());
                        }
						$post_details['discovery_agent'] = trim($inv_obj['DiscoveryAgent']->getValue());
						
						$_POST = $post_details;
						if($discovery_type == "vCloud")
						{
							$post_details['org_name'] = (isset($inv_obj['Organization'])) ? $inv_obj['Organization']->getValue() : '';
							$_POST = $post_details;
							$inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryvCloud($infrastructureType);
						}                    
						elseif($discovery_type == "vCenter") $inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryvCenter($infrastructureType);
						elseif($discovery_type == "vSphere") $inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryvSphere($infrastructureType);
						elseif($discovery_type == "HyperV") $inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryHyperV($infrastructureType);
						
						$inv_request_data[] = $post_details['ipaddress'];
					}
					/* AWS */
					elseif($discovery_type == "AWS")
					{
						$post_details['accountname'] = $inv_obj['AccountName']->getValue();
						$post_details['loginid'] = $inv_obj['AccessKeyID']->getValue();
						$post_details['loginpwd'] = trim($inv_obj['SecretAccessKey']->getValue());
						
						$_POST = $post_details;
						$inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryAws($infrastructureType);
						
						$inv_request_data[] = $post_details['accountname'];
					}
					/* Physical */
					elseif($discovery_type == "Physical")
					{
                        list($post_details['ipaddress'], $post_details['port']) = explode(":", $inv_obj['ServerIP']->getValue());
						$post_details['host_name'] = isset($inv_obj['HostName']) ? $inv_obj['HostName']->getValue() : '';
						$post_details['operating_system'] = (isset($inv_obj['OS'])) ? strtoupper($inv_obj['OS']->getValue()) : '';
						$post_details['input_host_id'] = isset($inv_obj['HostId']) ? $inv_obj['HostId']->getValue() : '';
						$post_details['call_is_from'] = "DraClient";
						$_POST = $post_details;
						$inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryPhysical($infrastructureType);
						$inv_request_data[] = $post_details['ipaddress'];
					}
					/* Azure */
					/*elseif($discovery_type == "Azure")
					{
						$post_details['accountname'] = $inv_obj['AccountName']->getValue();
						$post_details['loginid'] = $inv_obj['SubscriptionID']->getValue();*/
						/*
							Following changes are still required here 
							1. Certificate has to be captured
							2. Certificate to be sent as $_FILES parameters
							3. Uncomment the following save
						*/
						/*
						$_POST = $post_details;
						$inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryAzure($infrastructureType);
						$inv_request_data[] = $inv_obj['AccountName']->getValue();
					}*/
				}
			}
				
			// Add the quick look data to API table and return the RequestId in the response
			$requestedData = array("Inventory" => $inv_add_result);
			
			if(count($inv_add_result))
			{
				foreach($inv_add_result as $key => $inventory_id)
				{
					$inventory_ids = $inventory_ids . $inventory_id . ',';		
				}
				$inventory_ids = ',' . $inventory_ids ;
			}	
			
			$api_ref = $this->commonfunctionObj->insertRequestXml($requestXML,$functionName,$activityId,$clientRequestId,$inventory_ids);
			$request_id = $api_ref['apiRequestId'];
			$apiRequestDetailId = $this->commonfunctionObj->insertRequestData($requestedData,$request_id);
			
			$response  = new ParameterGroup ( "Response" );
			// Something in case misses and any important parameter is missed
			if(!is_array($api_ref) || !$request_id || !$apiRequestDetailId)
			{		
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(),  "Failed due to internal error");
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
    * Function Name: GetInventoryDetailsResponse
    *
    * Description: 
    * This function is to create the response XML format for returning the 
	*  infrastructure details
    *
    * Parameters:
    *     Param 1 [IN]: Inventory Details
    *     Param 2 [OUT]: Inventory details in pre-defined format XML
    *     
    * Exceptions:
    *     NONE
    */
    
    public function GetInventoryDetailsResponse($inventories, $response_handle, $discoveries=NULL)
    {
	
		if($discoveries) $discovery_names = array_flip($discoveries);
        $inv_count = 1;
		$uefi_hosts = $this->commonfunctionObj->getUefiHosts();
        foreach ($inventories as $inventoryId => $inventory_details)
        {
			// Use the Request ParameterGroup name if GetRequestStatus else a static incrementer string
            if($discovery_names) $discovery_name = $discovery_names[$inventoryId];
            else $discovery_name = "Infrastructure".$inv_count;
           
		   // The vCenter level details
            $discovery = new ParameterGroup ($discovery_name);
            $discovery->setParameterNameValuePair('InfrastructureId', $inventoryId);
            $discovery->setParameterNameValuePair('DiscoveryType', $inventory_details['discovery_type']);
            $discovery->setParameterNameValuePair('Reference', $inventory_details['ip_address']);
			$discovery->setParameterNameValuePair('ReferencePort', $inventory_details['port']);
			$discovery->setParameterNameValuePair('AccountId', $inventory_details['accountId']);
			$discovery->setParameterNameValuePair('InstanceUuid', $inventory_details['additionalDetails']['instanceUuid']);
			$discovery->setParameterNameValuePair('VmCount', $inventory_details['additionalDetails']['vmCount']);

			if($inventory_details['last_discovery_time'] == 0) $discovery->setParameterNameValuePair('LastDiscoveryTime','');
			else $discovery->setParameterNameValuePair('LastDiscoveryTime', $inventory_details['last_discovery_time']);
			
			$discovery->setParameterNameValuePair('DiscoveryStatus', $inventory_details['discovery_status']);
			// Discovery failure
			if($inventory_details['discovery_status'] == "Failed")
			{
				$discovery->setParameterNameValuePair('DiscoveryErrorLog', $inventory_details['error_log']);
				$error_response = ErrorCodeGenerator::frameErrorResponse($inventory_details['error_code'], $inventory_details['error_log'], $inventory_details['error_place_holders'],$error_type = null, $is_error_to_mds = 0,0,1);
				$discovery->addParameterGroup ($error_response);
			}
			
			// On a successful discovery, return the VMs listed under the vCenter
            //if($inventory_details['discovery_status'] == "Completed")
            //{
                $infra_hosts = $inventory_details['host_details'];
                $discovered_servers = new ParameterGroup ('DiscoveredServers');
                
                // Case to handle when there are no ESX Hosts in the vCenter discovery
                if(!count($infra_hosts))
                {
                    $response_handle->addParameterGroup ($discovery);
					$inv_count++;
                    continue;
                }
                
                $server_count = 1;
                foreach ($infra_hosts as $infra_id => $infra_details)
                {
					// The vSphere level details
                    $server = new ParameterGroup ('Server'.($server_count));
                    $server->setParameterNameValuePair("ServerIP", $infra_details['infrastructureHostIp']);
                    $server->setParameterNameValuePair("ServerName", $infra_details['infrastructureHostName']);
                    $server->setParameterNameValuePair("Organization", $infra_details['infrastructureOrganization']);
					$server->setParameterNameValuePair("InfrastructureHostId", $infra_details['infrastructureHostId']);
					if (isset($infra_details['dataStoreDetails']) && $infra_details['dataStoreDetails']!="" && $infra_details['dataStoreDetails']!=NULL && $infra_details['dataStoreDetails']!= 'N;')
					{
						$data_store_structure = unserialize($infra_details['dataStoreDetails']);
						$datastoregroup = new ParameterGroup ('DataStores');
						$data_store_count = 0;
						foreach ($data_store_structure as $data_store_id => $data_store_details)
						{
							$datastoredatagroup = new ParameterGroup ('DataStore'.$data_store_count);
							$datastoredatagroup->setParameterNameValuePair("blockSize", $data_store_details['blockSize']);
							$datastoredatagroup->setParameterNameValuePair("capacity", $data_store_details['capacity']);							
							$datastoredatagroup->setParameterNameValuePair("Uuid", $data_store_details['uuid']);
							$datastoredatagroup->setParameterNameValuePair("Freespace", $data_store_details['freeSpace']);
							$datastoredatagroup->setParameterNameValuePair("Type", $data_store_details['type']);
							$datastoredatagroup->setParameterNameValuePair("SymbolicName", $data_store_details['symbolicName']);
							$datastoredatagroup->setParameterNameValuePair("RefId", $data_store_details['refId']);
							$datastoredatagroup->setParameterNameValuePair("FileSystem", $data_store_details['fileSystem']);
							$datastoredatagroup->setParameterNameValuePair("VSphereHostName", $data_store_details['vSphereHostName']);
							$datastoregroup->addParameterGroup($datastoredatagroup);
							$data_store_count++;
						}
						$server->addParameterGroup($datastoregroup);
					}
					// The VM level details
                    $server_details = $infra_details['vm_data'];
                    
                    // Case to handle when there are no VMs in the discovery
                    // When the ESX server has no VMs we will not add the Host to the response
                    if(!count($server_details))  continue;
                    
                    foreach ($server_details as $details)
                    {
                        $key = $details['infrastructureVMId'];
                        $host_grp = new ParameterGroup ('Host'.($key));
                        
                        // Filtering out the templates
                        if($details['isTemplate'] == "1") continue;
                        
						// sending validation error on deleting the VM.
						if ( $details['isVmDeleted'] == 1)
						{
							if ( $details['validationErrors'] == "")
							{
								$details['validationErrors'] = "ECD005";
							}
							else
							{
								$details['validationErrors'] .= ",ECD005";
							}
						}
						
						if ( $details['agentRole'] == 'MasterTarget' || $details['processServerEnabled'] == 1)
						{
							if ( $details['validationErrors'] == "")
							{
								$details['validationErrors'] = "ECD010";
							}
							else
							{
								$details['validationErrors'] .= ",ECD010";
							}
						}
						
						//If FirmwareType column has empty value,
                        if (($details['hostId']) && (empty($details['FirmwareType'])))
						{
							$discovery_rec_hostid = trim(strtoupper($details['hostId']));
							
							if (in_array($discovery_rec_hostid, $uefi_hosts))
							{
								if ( $details['validationErrors'] == "")
								{
									$details['validationErrors'] = "ECD004";
								}
								else
								{
									$details['validationErrors'] .= ",ECD004";
								}
							}
						}
						
						$host_grp->setParameterNameValuePair('InfrastructureVMId', $details['infrastructureVMId']);
                        $host_grp->setParameterNameValuePair('ServerIP', $details['hostIp']);
                        $host_grp->setParameterNameValuePair('DisplayName', $details['displayName']);
						$host_grp->setParameterNameValuePair('HostName', $details['hostName']);
                        $host_grp->setParameterNameValuePair('OS', $details['OS']);
						$host_grp->setParameterNameValuePair('OsVersion', $details['osVersion']);
                        $host_grp->setParameterNameValuePair('PoweredOn', ($details['isVMPoweredOn']) ? 'Yes' : 'No');
                        $host_grp->setParameterNameValuePair('AgentInstalled', ($details['hostId']) ? 'Yes' : 'No');
                        $host_grp->setParameterNameValuePair('Protected', ($details['isProtected']) ? 'Yes' : 'No');
                        if($details['hostUuid']) $host_grp->setParameterNameValuePair('HostUuid', $details['hostUuid']);
						if($details['hostId']) $host_grp->setParameterNameValuePair('HostId', $details['hostId']);
						if($details['instanceUuid']) $host_grp->setParameterNameValuePair('InstanceUuid', $details['instanceUuid']);
                        
						$host_details = unserialize($details['hostDetails']);
						$vm_data_store_map = "";
						
                        // To show the tool status
                        $host_grp->setParameterNameValuePair('VMWareToolsStatus', $host_details['toolsStatus']);
						$host_grp->setParameterNameValuePair('EFI',$host_details['efi']);
						$disk_info = $host_details['diskInfo'];
						if(is_array($disk_info) && count($disk_info))
						{
							$disk_details_grp = new ParameterGroup("DiskDetails");
							$data_store = array();
							for($di_i = 0; $di_i < count($disk_info); $di_i++)
							{
								$disk_index = $di_i + 1;
								$disk_grp = new ParameterGroup("Disk".$disk_index);
								$disk_grp->setParameterNameValuePair("DiskName", $disk_info[$di_i]['diskName']);
								$disk_grp->setParameterNameValuePair("Capacity", $disk_info[$di_i]['diskSize']);
								$disk_grp->setParameterNameValuePair("DiskId", $disk_info[$di_i]['diskUuid']);
								$disk_grp->setParameterNameValuePair("InterfaceType", $disk_info[$di_i]['ideOrScsi']);
								$disk_grp->setParameterNameValuePair("BusAddress", $disk_info[$di_i]['scsiVmx'].":".$disk_info[$di_i]['scsiHost']);
								$disk_details_grp->addParameterGroup($disk_grp);
								
								$disk_name = $disk_info[$di_i]['diskName'];
								if ($disk_name)
								{
									preg_match('~\[(.*?)\]~', $disk_name, $output);
									$data_store[] = $output[1];
								}
							}
							//var_dump($data_store);
							$data_store = array_unique($data_store);
							$vm_data_store_map = implode(",", $data_store);
							$host_grp->addParameterGroup($disk_details_grp);
						}
						$host_grp->setParameterNameValuePair('VmDataStore', $vm_data_store_map);
						
						$validationErrors_grp = new ParameterGroup("ValidationErrors");
						if ($details['validationErrors'] != "")
						{
							$errorsArray = explode(',', $details['validationErrors']);
							$errorsArray = array_unique($errorsArray);
							foreach ($errorsArray as $errorCode)
							{
								$error_response = ErrorCodeGenerator::frameErrorResponse($errorCode, $errorCode, "", null, 0, 1);
								$validationErrors_grp->addParameterGroup ($error_response);
							}
						}
						$host_grp->addParameterGroup($validationErrors_grp);
                        $server->addParameterGroup ($host_grp);  
                    }
                    $discovered_servers->addParameterGroup ($server);
                    $server_count++;
                }
                $discovery->addParameterGroup ($discovered_servers);
            //}
            $response_handle->addParameterGroup ($discovery);
            $inv_count++;
        }
        return $response_handle;
    }
    
	/*
    * Function Name: ListInfrastructure
    *
    * Description: 
    * This function is to get the list of all source 
	*  infrastructures
    *
    * Parameters:
    *     Param 1 [IN]: InfrastructureType : vCenter, Physical
    *     Param 2 [IN]: InfrastructureId 
    *     Param 2 [OUT]: Inventory details in pre-defined format XML 
	*						matching the filter input
    *     
    * Exceptions:
    *     NONE
    */
	
    public function ListInfrastructure_validate($identity, $version, $functionName, $parameters)
    {
		global $API_ERROR_LOG, $logging_obj;
        try
		{
			// Enable exceptions for DB.			
			$this->global_conn->enable_exceptions();
			
			if(isset($parameters['InfrastructureType'])) 
			{        
				$infra_type = $parameters['InfrastructureType']->getValue();
				if(!trim($infra_type)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA',  array('Parameter' => "InfrastructureType"));
				if($infra_type != "vCenter" && $infra_type != "Physical") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA',  array('Parameter' => "InfrastructureType"));
			}
			
			# Validating infrastructures
			if(isset($parameters['InfrastructureId']))
			{
				$inventoryId = $parameters['InfrastructureId']->getValue();
				if(!trim($inventoryId))  ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA',  array('Parameter' => "InfrastructureId"));
				
				# Check if inventory Id exists
				$inventory_exists = $this->esxDiscoveryObj->verifyInventoryId($infra_type, $inventoryId);
				if(!$inventory_exists) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA',  array('Parameter' => "InfrastructureId"));
			}
			
			# Validating infrastructure vmid
			if(isset($parameters['InfrastructureVmId']))
			{
				$vmId = $parameters['InfrastructureVmId']->getValue();
				if(!trim($vmId))  ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA',  array('Parameter' => "InfrastructureVmId"));
				
				$vmid_exists = $this->global_conn->sql_get_value("infrastructurevms", "displayName", "infrastructureVMId = ?", array($vmId));
				#for now ignoring validation as DRA side we need to handle it. 
				if(!$vmid_exists)
				{
					$logging_obj->my_error_handler("INFO",array("InfrastructureVmId"=>$vmId,"Reason"=>"It is not available with CS"),Log::APPINSIGHTS);
					debug_access_log($API_ERROR_LOG, "infra_vm_id:$vmId is not available with CS.");
				}
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
    
    public function ListInfrastructure($identity, $version, $functionName, $parameters)
    {
        global $infraStr, $API_ERROR_LOG, $logging_obj;
		$infraStr = '';
		try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			
			$response = new ParameterGroup ("Response");
			$infra_type = (isset($parameters['InfrastructureType'])) ? $parameters['InfrastructureType']->getValue() : NULL;
			$inventoryId = (isset($parameters['InfrastructureId'])) ? $parameters['InfrastructureId']->getValue() : NULL;
			$InfrastructureVmId = (isset($parameters['InfrastructureVmId'])) ? $parameters['InfrastructureVmId']->getValue() : NULL;
						
			$inv_args = ($inventoryId) ? array($inventoryId) : NULL;
			$inventories = $this->esxDiscoveryObj->getInventoryDetails($inv_args, $infra_type, NULL ,$InfrastructureVmId);
			
			if(!count($inventories))
			{
				$infra_data = new ParameterGroup ("Infrastructure");
				$response->addParameterGroup($infra_data);
			}
			else $response = $this->GetInventoryDetailsResponse($inventories, $response);
			
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();
			
			if(($infraStr !='') && isset($InfrastructureVmId))
			{
				debug_access_log($API_ERROR_LOG, "$infraStr");
				$logging_obj->my_error_handler("INFO",array("InfrastructureVmId"=>$InfrastructureVmId,"InfraString"=>$infraStr),Log::APPINSIGHTS);
			}
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
	/*
    * Function Name: RemoveInfrastructure
    *
    * Description: 
    * This function is to remove the infrastructure from CX
    *
    * Parameters:
    *     Param 1 [IN]: InfrastructureId
    *     Param 2 [OUT]: Status of removing the infrastructure
    *     
    * Exceptions:
    *     NONE
    */
	
	public function RemoveInfrastructure_validate($identity, $version, $functionName, $parameters)
    {
        try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
            
			if(!isset($parameters['InfrastructureId']) && (!isset($parameters['InfrastructureType']) || !isset($parameters['InfrastructureIp']))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "InfrastructureId OR InfrastructureType and InfrastructureIp"));
                   
            if(isset($parameters['InfrastructureId']))
            {
                $inventoryId = $parameters['InfrastructureId']->getValue();
                if(!trim($inventoryId))  ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "InfrastructureId"));
                
                if($inventoryId) 
                {
					$protected_vms_migration_status = $this->esxDiscoveryObj->getProtectedVMsMigrationStatusForInventoryId($inventoryId);
					if ($protected_vms_migration_status)
					{
						//All protected vm's have been migrated, allow for vcenter deletion
					}
					else
					{	
						$protection_exist = $this->esxDiscoveryObj->getProtectionsForId($inventoryId);
						if($protection_exist) ErrorCodeGenerator::raiseError($functionName, 'EC_PROTECTION_EXIST');
					}
                }
            }
            
            if(isset($parameters['InfrastructureType']))
            {
                if(!isset($parameters['InfrastructureIp'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "InfrastructureIp"));
				$infra_type = $parameters['InfrastructureType']->getValue();
                if(!trim($infra_type))  ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "InfrastructureType"));
				if(!in_array($infra_type, array("vCenter","Physical"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "InfrastructureType"));
				$infra_ip = $parameters['InfrastructureIp']->getValue();
                if(!trim($infra_ip))  ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "InfrastructureIp"));
				
				//Validation to know protections exists or not before removing the given infrastucture data.
				//Idempotency handles automatically
				if($infra_type && $infra_ip)
                {
					$protected_vms_migration_status = $this->esxDiscoveryObj->getProtectedVMsMigrationStatusForInventoryIp($infra_ip ,$infra_type);
					if ($protected_vms_migration_status)
					{
						//All protected vm's have been migrated, allow for vcenter deletion
					}
					else
					{
						$protection_exist = $this->esxDiscoveryObj->getProtectionsForInfrastructureDetails($infra_ip ,$infra_type);
						if($protection_exist) ErrorCodeGenerator::raiseError($functionName, 'EC_PROTECTION_EXIST');
					}
                }
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
    
    public function RemoveInfrastructure($identity, $version, $functionName, $parameters)
    {      
		try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
            if(isset($parameters['InfrastructureId']))
            {
                $inventoryId = $parameters['InfrastructureId']->getValue();
            }
            elseif(isset($parameters['InfrastructureType']) && isset($parameters['InfrastructureIp']))
            {
                $infra_type = $parameters['InfrastructureType']->getValue();
                $infra_ip = $parameters['InfrastructureIp']->getValue();
                $inventoryId = $this->esxDiscoveryObj->getInventoryId($infra_ip, $infra_type);
            }
            
            $response = new ParameterGroup ("Response");
            
            if(!$inventoryId)
            {
                $response->setParameterNameValuePair($functionName, "Success");                
            }
            else
            {            
                $inventory_deleted = $this->esxDiscoveryObj->deleteInventory($inventoryId);
                $delete_status = ($inventory_deleted) ? "Success" : "Failed";
                $response->setParameterNameValuePair($functionName, $delete_status);
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
	
	public function UpdateInfrastructureDiscovery_validate($identity, $version, $functionName, $parameters)
	{
		$function_name_user_mode = 'Update Infrastructure Discovery';
		try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();			
			
			if(!isset($parameters['Infrastructures'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Infrastructures"));
			
			# Validating infrastructures
			$infrastructures = $parameters['Infrastructures']->getChildList();
			if(!count($infrastructures)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Infrastructures"));
			
			foreach( $infrastructures as $index => $infra_obj)
			{
				if($infrastructures[$index])
				{
					$inv_obj = $infrastructures[$index]->getChildList();
					
					if(isset($inv_obj['InfrastructureId']))
					{
						$inventory_id = $inv_obj['InfrastructureId']->getValue();
						if(!trim($inventory_id)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "InfrastructureId"));
						$inventory_exists = $this->esxDiscoveryObj->verifyInventoryId(NULL,$inventory_id);
						if(!$inventory_exists) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECORD_INFO', array(), 'No data available for InfrastructureId - ' . $inventory_id);
					}
					
					if(isset($inv_obj['HostName']))
					{
						$host_name = $inv_obj['HostName']->getValue();
						if(!trim($host_name)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'HostName'));
					}
				   
					if(isset($inv_obj['DiscoveryAgent']))
					{
						$ps_details = $this->commonfunctionObj->get_psName_ipadress_by_id($inv_obj['DiscoveryAgent']->getValue());
						// Validating the DiscoveryAgent provided is a valid PS pointing to this CS.
						if(!$ps_details || !is_array($ps_details)) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
						$ps_details = $this->commonfunctionObj->get_host_info($inv_obj['DiscoveryAgent']->getValue());		
						if($ps_details['currentTime'] - $ps_details['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ps_details['name'], 'PsIP' => $ps_details['ipaddress'], 'Interval' => ceil(($ps_details['currentTime'] - $ps_details['lastHostUpdateTimePs'])/60)));
					}
					
					if(isset($inv_obj['ServerIP']))
					{
						$server_ip_port = $inv_obj['ServerIP']->getValue();
						if(!trim($server_ip_port)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'ServerIP'));
                        list($server_ip, $server_port) = explode(":", $server_ip_port);
						
						$server_port = trim($server_port);
                        if($server_port && ($server_port <= 0 || $server_port > 65535)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Server Port - $index"));
					}
					
					if(isset($inv_obj['OS']))
					{
						$os_type = $inv_obj['OS']->getValue();
						// Validation of OS. It can be Linux/Windows only
						if(trim(!$os_type) || ($os_type != "Linux" && $os_type != "Windows")) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "OS - $index"));
					}
					
                    if(!isset($inv_obj['AccountId']) && (!isset($inv_obj['LoginId']) || !isset($inv_obj['Password']))) if(!$inventory_id && !isset($inv_obj[$param]))  ErrorCodeGenerator::raiseError(COMMON, EC_INSFFCNT_PRMTRS, "AccountId / LoginId and Password");
                    
                    if(isset($inv_obj['AccountId']))
					{
						$account_id = $inv_obj['AccountId']->getValue();
						if(!trim($account_id))	ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "AccountId"));
                        // Validate the account id
                        $account_exists = $this->account_obj->validateAccount($account_id);
                        if(!$account_exists)	ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "AccountId"));
					}
                    
					if(isset($inv_obj['LoginId']))
					{
						$username = $inv_obj['LoginId']->getValue();
						if(trim(!$username)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "LoginId - $index"));
					}
					
					if(isset($inv_obj['Password']))
					{
						$password = $inv_obj['Password']->getValue();
						if(trim(!$password)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Password - $index"));
					}
					
					if(isset($inv_obj['HostId']))
					{
						$req_host_id = $inv_obj['HostId']->getValue();
						if(!trim($req_host_id)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'HostId'));
						$req_host_id_details = $this->commonfunctionObj->get_host_info($req_host_id);
						#print_r($req_host_id_details);
						
						// Validating the DiscoveryAgent provided is a valid PS pointing to this CS.
						if(!$req_host_id_details || !is_array($req_host_id_details)) 
						{
							ErrorCodeGenerator::raiseError($functionName, 'EC_NO_HOST_FOUND', array('ErrorCode' => ErrorCodes::EC_NO_HOST_FOUND, 'HostIdHolder' => $req_host_id , 'ServerIP' => $server_ip));
						}
					}
				}	
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
	
	public function UpdateInfrastructureDiscovery($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			global $requestXML, $activityId, $clientRequestId; 	 		
			$infrastructures = $parameters['Infrastructures']->getChildList();
			
			$inv_add_result = array();
			$inv_request_data = array();			
			
			foreach ($infrastructures as $index => $infra_obj)
			{   
				$post_details = array();
				if($infrastructures[$index])
				{
					$inv_obj = $infrastructures[$index]->getChildList();
					
					// Get all the input fields
					$infrastructureType = (isset($inv_obj['InfrastructureType'])) ? $inv_obj['InfrastructureType']->getValue() : "Primary";
					$discovery_type = $this->global_conn->sql_get_value("infrastructureInventory","hostType","inventoryId = ?", array($inv_obj['InfrastructureId']->getValue()));					
					$post_details['inventoryid'] = (isset($inv_obj['InfrastructureId'])) ? $inv_obj['InfrastructureId']->getValue() : "";
					$post_details['servertype'] = $discovery_type;
					
					/* vCloud, vCenter, vSphere, HyperV */
					if(in_array($discovery_type, array("vCloud", "vCenter", "vSphere", "HyperV")))
					{
                        if(isset($inv_obj['ServerIP']))
                        {
                            list($post_details['ipaddress'],$post_details['port']) = explode(":", $inv_obj['ServerIP']->getValue());
                            $post_details['ipaddress'] = trim($post_details['ipaddress']);
							if ($post_details['port'] != "" and isset($post_details['port']))
							{
								$post_details['port'] =  trim($post_details['port']);
							}
                        }
                        if(isset($inv_obj['AccountId']))
                        {
                            $post_details['accountid'] = trim($inv_obj['AccountId']->getValue());
                        }
                        else
                        {
                            $post_details['loginid'] = (isset($inv_obj['LoginId'])) ? trim($inv_obj['LoginId']->getValue()) : '';
                            $post_details['loginpwd'] = (isset($inv_obj['Password'])) ? trim($inv_obj['Password']->getValue()) : '';
                        }
						$post_details['discovery_agent'] = (isset($inv_obj['DiscoveryAgent'])) ? $inv_obj['DiscoveryAgent']->getValue() : '';
						
						$_POST = $post_details;
						if($discovery_type == "vCloud")
						{
							$post_details['org_name'] = (isset($inv_obj['Organization'])) ? $inv_obj['Organization']->getValue() : '';
							$_POST = $post_details;
							$inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryvCloud($infrastructureType);
						}                    
						elseif($discovery_type == "vCenter") $inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryvCenter($infrastructureType);
						elseif($discovery_type == "vSphere") $inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryvSphere($infrastructureType);
						elseif($discovery_type == "HyperV") $inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryHyperV($infrastructureType);
						
						$inv_request_data[] = $post_details['ipaddress'];
					}
					/* AWS */
					elseif($discovery_type == "AWS")
					{
						$post_details['accountname'] = $inv_obj['AccountName']->getValue();
						$post_details['loginid'] = $inv_obj['AccessKeyID']->getValue();
						$post_details['loginpwd'] = trim($inv_obj['SecretAccessKey']->getValue());
						
						$_POST = $post_details;
						$inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryAws($infrastructureType);
						
						$inv_request_data[] = $post_details['accountname'];
					}
					/* Physical */
					elseif($discovery_type == "Physical")
					{					
						$post_details['ipaddress'] = $inv_obj['ServerIP']->getValue();
						$post_details['host_name'] = isset($inv_obj['HostName']) ? $inv_obj['HostName']->getValue() : '';
						$post_details['operating_system'] = (isset($inv_obj['OS'])) ? strtoupper($inv_obj['OS']->getValue()) : '';
						$post_details['input_host_id'] = isset($inv_obj['HostId']) ? $inv_obj['HostId']->getValue() : '';
						
						$_POST = $post_details;
						$inv_add_result[$index] = $this->esxDiscoveryObj->saveInventoryPhysical($infrastructureType);
						$inv_request_data[] = $post_details['ipaddress'];
					}					
				}
			}
			// Add the quick look data to API table and return the RequestId in the response
			$requestedData = array("Inventory" => $inv_add_result);
			
			if(count($inv_add_result))
			{
				foreach($inv_add_result as $key => $inventory_id)
				{
					$inventory_ids = $inventory_ids . $inventory_id . ',';		
				}
				$inventory_ids = ',' . $inventory_ids ;
			}	
			
			$api_ref = $this->commonfunctionObj->insertRequestXml($requestXML,$functionName,$activityId,$clientRequestId,$inventory_ids);
			$request_id = $api_ref['apiRequestId'];
			$apiRequestDetailId = $this->commonfunctionObj->insertRequestData($requestedData,$request_id);
			
			$response  = new ParameterGroup ( "Response" );
			// Something in case misses and any important parameter is missed
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
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(),  "Database error occurred");
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