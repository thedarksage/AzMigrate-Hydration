<?php
class MonitoringHandler extends ResourceHandler 
{

	private $volume_obj;
	private $common_obj;
	private $recovery_obj;
	private $push_Obj;
	private $srm_Obj;
	private $policyObj;
	private $protectionObj;
	private $esxDiscoveryObj;
	private $processServerObj;
	private $fileProtectionObj;
	
	public function MonitoringHandler() 
	{
		global $conn_obj;
		$this->volume_obj = new VolumeProtection();
		$this->common_obj = new CommonFunctions();
		$this->recovery_obj = new Recovery();
		$this->conn = new Connection();
		$this->global_conn = $conn_obj;
		$this->system_obj = new System();
		$this->push_Obj = new PushInstall();
		$this->srm_Obj = new SRM();
		$this->policyObj = new Policy();
		$this->protectionObj = new ProtectionPlan();
		$this->esxDiscoveryObj = new ESXDiscovery();
		$this->processServerObj = new ProcessServer();
		$this->monitor_obj = new Monitor();
		$this->fileProtectionObj = new FileProtection();
	}
	
	public function GetRequestStatus_validate($identity, $version, $functionName, $parameters)
	{		
		$request_list_obj = $parameters['RequestIdList'];
		if(is_object($request_list_obj))
		{
			$obj = $parameters['RequestIdList']->getChildList();
			if(count($obj))
            {
				foreach($obj as $RequestObj)
                {
                    if(!is_object($RequestObj))
                    {
					   ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'RequestId'));
                    }
                    if(trim($RequestObj->getValue()) == "")
                    {
					   ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'RequestId'));
                    }
					if(!is_numeric(trim($RequestObj->getValue())))
                    {
					   ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => 'RequestId'));
                    }
                }
			}
			else
            {
				ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => 'RequestIdList'));
            }   
		}
		else
		{
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'RequestIdList'));
		}
	}
	
	public function GetRequestStatus($identity, $version, $functionName, $parameters)
	{
		global $asycRequestXML;
		
		$requestXml = '';
		$request_list_obj = $parameters['RequestIdList'];
		$obj = $parameters['RequestIdList']->getChildList();
		$response = new ParameterGroup ( "Response" );

		foreach($obj as $request_id_obj)
		{
			$request_id = trim($request_id_obj->getValue());
			$sql = "SELECT 
						requestType,
						requestXml,
						activityId
					FROM
						apiRequest
					WHERE
						apiRequestId = ?";
			$rs = $this->conn->sql_query($sql,array($request_id));
			$count = 0;
			for($i=0;isset($rs[$i]);$i++) {
				$count = 1;
				$requestType = trim($rs[$i]['requestType']);
				$requestXml = trim($rs[$i]['requestXml']);
				$activityId = trim($rs[$i]['activityId']);
			}
			if($count == 0) {
				ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RQUSTID_FOND', array('RequestId' => $request_id));
			}
			$request_type = $requestType;
			$requestType = strtoupper($requestType);
			
			switch($requestType) {
				case 'CREATEPROTECTION':
				case 'CREATEPROTECTIONV2':
				case 'MODIFYPROTECTIONV2':
				case 'MODIFYPROTECTION':
					$requestIdObj = new ParameterGroup ($request_id);
					$sql_data = "SELECT requestdata FROM apiRequestDetail WHERE apiRequestId=?";
					$rs_data = $this->conn->sql_query($sql_data,array($request_id));
					$count_data_rows = 0;
					for($i=0;isset($rs_data[$i]);$i++) {
						$count_data_rows = 1;
						$requestdata = trim($rs_data[$i]['requestdata']);
					}
					/*
					if($count_data_rows == 0) {
						ErrorCodeGenerator::raiseError(GETREQUESTSTATUS, EC_NO_PROTECTION_FOUND, "$request_id");
					}
					*/
					
					$requestdata = unserialize($requestdata);
					
					$obj = $this->_create_protection_status($requestdata, $requestIdObj);
					$response->addParameterGroup ($obj);
				break;
				
				case 'INFRASTRUCTUREDISCOVERY':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));  
					$response_grp = new ParameterGroup($request_id);
					$requestdata = unserialize($requestdata_str);
					
					$response_grp = $this->_infrastructure_discovery_status($requestdata, $response_grp);
					$response->addParameterGroup($response_grp);
                
				break;
				
				case 'UPDATEINFRASTRUCTUREDISCOVERY':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));  
					$response_grp = new ParameterGroup($request_id);
					$requestdata = unserialize($requestdata_str);
					
					$response_grp = $this->_infrastructure_discovery_status($requestdata, $response_grp);
					$response->addParameterGroup($response_grp);
                
				break;
				
				case 'CREATEROLLBACK':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));
					$requestdata = unserialize($requestdata_str);
					
					$response = $this->_create_rollback_status($requestdata, $response);
					
				break;				
				
				case 'INSTALLMOBILITYSERVICE':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));
					$response_grp = new ParameterGroup($request_id);
					$requestdata = unserialize($requestdata_str);
					
					$response_grp = $this->_create_install_mobility_status($requestdata, $response_grp);
					$response->addParameterGroup($response_grp);					
				break;
				
				case 'UNINSTALLMOBILITYSERVICE':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));
					$response_grp = new ParameterGroup($request_id);
					$requestdata = unserialize($requestdata_str);
					
					$response_grp = $this->_create_uninstall_mobility_status($requestdata, $response_grp);
					$response->addParameterGroup($response_grp);					
				break;
				
				case 'UPDATEMOBILITYSERVICE':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));
					$response_grp = new ParameterGroup($request_id);
					$requestdata = unserialize($requestdata_str);
					
					$response_grp = $this->_create_update_mobility_status($requestdata, $response_grp, $requestXml);
					$response->addParameterGroup($response_grp);					
				break;
				
				case 'STOPPROTECTION':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));
					$requestdata = unserialize($requestdata_str);
					
					$response = $this->_stop_protection_status($request_id, $requestdata, $response);
					
				break;
				
				case 'RENEWCERTIFICATE':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));
					$requestdata = unserialize($requestdata_str);
					
					$response = $this->_create_renew_certificate_status($requestdata, $response, $request_id);
					
				break;
				
				case 'MIGRATEACSTOAAD':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));
					$requestdata = unserialize($requestdata_str);
					
					$response = $this->_create_migrate_acs_to_aad_status($requestdata, $response, $request_id);
					
				break;
				
				case 'MACHINEREGISTRATIONTORCM':
				case 'FINALREPLICATIONCYCLE':
				case 'RESUMEREPLICATIONTOCS':
				case 'MOBILITYAGENTSWITCHTORCM':
					$requestdata_str = $this->conn->sql_get_value('apiRequestDetail', 'requestdata', "apiRequestId = ?", array($request_id));
					$requestid_group = new ParameterGroup($request_id);
					$requestdata = unserialize($requestdata_str);					
					$obj = $this->_v2a_migration_status($requestdata, $requestid_group, $request_id, $request_type);
					$response->addParameterGroup($obj);
				break;
					
				default:
					$response = $this->_get_request_status_vcon($identity, $version, $functionName, $parameters);
			}
		}
		$error_set = ErrorCodeGenerator::getWorkFlowErrorCounter();	
		if($error_set) $asycRequestXML = $requestXml;
		
		return $response->getChildList();	
	}
	
	private function _infrastructure_discovery_status($requestdata, $response)
	{
		$discoveries = $requestdata['Inventory']; 
		$inventory_ids = array_values($discoveries);
		
		$inventory_details = $this->esxDiscoveryObj->getInventoryDetails($inventory_ids);
		if(!$inventory_details)
		{
			$response->setParameterNameValuePair("ErrorMessage", "No inventories found for this request/inventories got removed");
			ErrorCodeGenerator::raiseError('INFRASTRUCTUREDISCOVERY', 'EC_NO_RECORD_INFO', array(), 'No inventories found for this request/inventories got removed');
		}
		else
		{
			foreach ($discoveries as $discovery_name => $inventoryId)
			{
				if(!$inventoryId || !$inventory_details[$inventoryId]) 
				{
					$discovery = new ParameterGroup ($discovery_name);
					$discovery->setParameterNameValuePair('InfrastructureId', $inventoryId);
					$discovery->setParameterNameValuePair('DiscoveryStatus', 'Failed');
					
					$error_response = ErrorCodeGenerator::frameErrorResponse(ErrorCodes::EC_NO_RECORD_INFO, 'No inventories found for this request/inventories got removed');
					$discovery->addParameterGroup ($error_response);
					
					$response->addParameterGroup ($discovery);
				}
			}                
			$esx_handler_obj = new ESXHandler();
			$response = $esx_handler_obj->GetInventoryDetailsResponse($inventory_details, $response, $discoveries);	
		}
		
		return $response;
	}
	
	private function _create_rollback_status($requestdata, $response)
	{
		global $ROLLBACK_FAILED,$ROLLBACK_INPROGRESS,$ROLLBACK_COMPLETED;
		$plan_details = $requestdata['planId'];
		$scenario_details = $requestdata['scenarioId'];
		$server_ids = array_keys($scenario_details);
		
		$server_details = $this->recovery_obj->getRollbackState('', $server_ids);
		
		if($server_details)
		{
			#$execution_state = array(1=>"Completed",2=>"Pending",3=>"InProgress",4=>"InProgress",5=>"Failed");
			$plan_count = 1;
			foreach($plan_details as $source_id => $recovery_plan_id)
			{
				$servers_grp = new ParameterGroup("Servers");
				$server_count = 1;
				$application_plan_status = $this->conn->sql_get_value('applicationPlan', 'applicationPlanStatus', "planId = ?", array($recovery_plan_id));
				
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
				
				$request_status = new ParameterGroup("Plan".$plan_count);
				$request_status->setParameterNameValuePair('RollbackPlanId', $recovery_plan_id);
				$request_status->setParameterNameValuePair('Rollback', $rollback_status);
				
				foreach($server_details as $source_host_id => $state_details)
				{
					switch($state_details['executionStatus']) {
						case $ROLLBACK_FAILED:
							$scenario_status = 'Failed';
							break;
						case $ROLLBACK_INPROGRESS:
							$scenario_status = 'Pending';
							break;
						case $ROLLBACK_COMPLETED:
							$scenario_status = 'Completed';
							break;
					}
					
					$server_grp = new ParameterGroup("Server".$server_count);
					$server_grp->setParameterNameValuePair("HostId", $source_host_id);
					$server_grp->setParameterNameValuePair("RollbackStatus", $scenario_status);
					
					if($state_details['Tag']) 
					{
						$server_grp->setParameterNameValuePair("TagName", $state_details['Tag']);
						if(!preg_match('/:/', $state_details['Tag']))
						{
							$tag_time = $this->recovery_obj->getTagTimeStamp($state_details['Tag']);
							$server_grp->setParameterNameValuePair("TagTime", $tag_time);
						}
						else $server_grp->setParameterNameValuePair("TagTime", $state_details['Tag']);
					}	
					
					if ($scenario_status == 'Failed')
					{
						$error_response = ErrorCodeGenerator::frameErrorResponse($state_details['error_code'], $state_details['error_message'], $state_details['error_place_holder']);
						$server_grp->addParameterGroup ($error_response);					
					}
					
					$servers_grp->addParameterGroup($server_grp);
					$server_count++;
				
				}				
				$request_status->addParameterGroup($servers_grp);
			}
			$response->addParameterGroup($request_status);
			$plan_count++;						
		}
		return $response;
	}		
	
	/*
		Common Function to get the status of GetRequestStatus(CreateProtection/ModifyProtection)
			- Returns the Preparetarget status/Replication status for request source
	*/
	private function _create_protection_status($requestdata, $requestIdObj)
	{
		global $VALIDATION_SUCCESS;

		$planId = $requestdata['planId'];
		$scenarioDetails = $requestdata['scenarioId'];
				
		$requestIdObj->setParameterNameValuePair('ProtectionPlanId',$planId);
		$protectionStatusObj = new ParameterGroup ('ProtectionStatus');
		
		$this->conn->enable_exceptions();
		try
		{
			$this->conn->sql_begin();
			$server_count = 1;
			foreach($scenarioDetails as $id=>$scenarioId) {
				/*
					Check PrepareTarget Status
				*/
				$sql = "SELECT b.policyState AS policyState, b.executionLog FROM policy a, policyInstance b WHERE a.policyId=b.policyId AND a.scenarioId = ? AND a.policyType = '5'";
				$rs = $this->conn->sql_query($sql,array($scenarioId));
				$prepareTrgStatus = '';
				for($i=0;isset($rs[$i]);$i++) {
					switch($rs[$i]['policyState']) {
						case 2:
							$prepareTrgStatus = 'Failed';							
							$execution_log_arr = $rs[$i]['executionLog'] ? unserialize($rs[$i]['executionLog']) : array();

							$error_code = $error_message = '';
							$error_place_holders = array();
							
							/* For prepare target, Policy is per VM level
							* hence execution log will contain details for only one VM
							*/
							if (! empty($execution_log_arr))
							{
								// Check errors generated at VM level, else collect at global level
								if (! empty($execution_log_arr[3]))
								{
									$current_vm_data = current($execution_log_arr[3]);
									$error_code = isset($current_vm_data[2]['ErrorCode']) ? $current_vm_data[2]['ErrorCode'] : '';
									$error_message = isset($current_vm_data[2]['ErrorMessage']) ? $current_vm_data[2]['ErrorMessage'] : '';
									$error_place_holders = empty($current_vm_data[3]['PlaceHolder']) ? array() : $current_vm_data[3]['PlaceHolder'][2];
								}
								elseif (! empty($execution_log_arr[2]))
								{
									$error_code = isset($execution_log_arr[2]['ErrorCode']) ? $execution_log_arr[2]['ErrorCode'] : '';
									$error_message = isset($execution_log_arr[2]['ErrorMessage']) ? $execution_log_arr[2]['ErrorMessage'] : '';
									$error_place_holders = empty($execution_log_arr[4]['PlaceHolder']) ? array() : $execution_log_arr[4]['PlaceHolder'][2];
								}
							}
							break;
						case 3:
							$prepareTrgStatus = 'Pending';
							break;
						case 4:
							$prepareTrgStatus = 'InProgress';
							break;
						default:
							$prepareTrgStatus = 'Completed';
					}
				}
							
				/*
				Check Creation of Replication Pairs
					a) Check if all the scenarios are active
				*/
				$sql_scenario_count = "SELECT count(*) AS scenarioCount FROM applicationScenario WHERE executionStatus=? AND scenarioId=?";
				$rs_scenario_count = $this->conn->sql_query($sql_scenario_count,array($VALIDATION_SUCCESS,$scenarioId));
				$CreateReplicationPairs = 'Pending';
				for($i=0;isset($rs_scenario_count[$i]);$i++) {
					if($rs_scenario_count[$i]['scenarioCount'] > 0) {
						$CreateReplicationPairs = 'Complete';
					}
				}
				
				/*
					Check Pair Status
				*/
				$sql_pair_state = "SELECT isQuasiflag,executionState, TargetDataPlane FROM srcLogicalVolumeDestLogicalVolume WHERE sourceHostId=? AND scenarioId=? AND planId=?";
				$rs_pair_state = $this->conn->sql_query($sql_pair_state,array($id,$scenarioId,$planId));
				$resyncStatus = array();
				$status = 'N/A';
				for($i=0;isset($rs_pair_state[$i]);$i++) 
				{
					if($rs_pair_state[$i]['executionState'] == 2) 
					{
						$status = 'Queued';
						$resyncStatus = array();
						break;
					}
					else
					{
						$resyncStatus[] = $rs_pair_state[$i]['isQuasiflag'];
					} 
                    $target_data_plane = $rs_pair_state[$i]['TargetDataPlane'];
				}
                  
				if(count($resyncStatus)) {
					if(in_array(0,$resyncStatus) === TRUE) {
						$status = 'Resync Step I';
					} elseif(in_array(1,$resyncStatus) === TRUE) {
						$status = 'Resync Step II';
					} elseif(in_array(2,$resyncStatus) === TRUE) {
						$status = 'Differential Sync';
					}
				}
                //For Azure data plane, we send prepare target as completed always. Because prepare target not exists.       
				if ($target_data_plane == 2)
				{
					$prepareTrgStatus = 'Completed';
				}
				$serverObj = new ParameterGroup ("Server".$server_count);
				$serverObj->setParameterNameValuePair('HostId',$id);
				$serverObj->setParameterNameValuePair('PrepareTarget',$prepareTrgStatus);
				
				if ($prepareTrgStatus == 'Failed')
				{
					$error_response = ErrorCodeGenerator::frameErrorResponse($error_code, $error_message, $error_place_holders);
					$serverObj->addParameterGroup ($error_response);
				}
				
				$serverObj->setParameterNameValuePair('CreateReplicationPairs',$CreateReplicationPairs);
				$serverObj->setParameterNameValuePair('ProtectionStage',$status);
				$protectionStatusObj->addParameterGroup ($serverObj);
				$server_count++;
			}	
			$this->conn->sql_commit();
		}
		catch(Exception $sql_excep)
		{
			$this->conn->sql_rollback();
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		$this->conn->disable_exceptions();
		
		$requestIdObj->addParameterGroup ($protectionStatusObj);
		return $requestIdObj;
	}
	
	/*
    * Function Name: _create_install_mobility_status
    *
    * Description: 
    * This function is used to created the XML response 
	*	for InstallMobilityService API
    *
    * Parameters:
    *     Param 1 [IN]: RequestData
    *     Param 2 [IN]: Response XML object
    *     
    * Exceptions:
    *     NONE
    */
	
	private function _create_install_mobility_status($requestdata, $requestIdObj)
	{
		$install_job_ids = array_values($requestdata[0]);
		/* Array that defines the PushInstall job status */
		$status_array = array(0=>"Invalid", 1=>"Pending", 2=>"InProgress", 3=>"Completed", -3=>"Failed");
		
		/* Get installation status for all jobs*/
		$install_status = $this->push_Obj->getInstallStatus($install_job_ids);		
		
		/* Build XML for each install request */
		foreach($requestdata[0] as $key_name => $install_job_id)
		{
			$response_grp = new ParameterGroup($key_name);
			
			$response_grp->setParameterNameValuePair("HostIP",$install_status[$install_job_id]['ipaddress']);
			
			if(!$install_status[$install_job_id]){
				ErrorCodeGenerator::raiseError('INSTALLMOBILITYSERVICE', 'EC_NO_RECORD_INFO', array(), 'Data Unavailable on this job');
			}
			elseif(! array_key_exists($install_status[$install_job_id]['state'], $status_array)){			
				ErrorCodeGenerator::raiseError('INSTALLMOBILITYSERVICE', 'EC_NO_RECORD_INFO', array(), 'Job in invalid state');
			}
			else
			{
				$response_grp->setParameterNameValuePair("Status",$status_array[$install_status[$install_job_id]['state']]);
				
				if ($status_array[$install_status[$install_job_id]['state']] == 'Failed')
				{
					$error_response = ErrorCodeGenerator::frameErrorResponse($install_status[$install_job_id]['errCode'], $install_status[$install_job_id]['statusMessage'], $install_status[$install_job_id]['errPlaceHolders'] ? unserialize($install_status[$install_job_id]['errPlaceHolders']) : array(),null,1,0,0,$install_status[$install_job_id]['installerExtendedErrors']);
					$response_grp->addParameterGroup ($error_response);				
				}
				
				$response_grp->setParameterNameValuePair("StatusMessage",trim($install_status[$install_job_id]['statusMessage']));				
				$response_grp->setParameterNameValuePair("LastUpdateTime",$install_status[$install_job_id]['lastUpdatedTime']);
				
				/* We will have the hostId populated only if the job completed */
				if($install_status[$install_job_id]['state'] == 3)
				$response_grp->setParameterNameValuePair("HostId",$install_status[$install_job_id]['hostId']);
				/* We will display the error log contents if failed */
				elseif($install_status[$install_job_id]['state'] == -3 && file_exists($install_status[$install_job_id]['logFileLocation']))
				$response_grp->setParameterNameValuePair("ErrorMessage", "");

				#$response_grp->setParameterNameValuePair("ErrorMessage",file_get_contents($install_status[$install_job_id]['logFileLocation']));

			}
			$requestIdObj->addParameterGroup($response_grp);			
		}
		
		return $requestIdObj;
	}
	
	/*
    * Function Name: _create_uninstall_mobility_status
    *
    * Description: 
    * This function is used to created the XML response 
	*	for UnInstallMobilityService API
    *
    * Parameters:
    *     Param 1 [IN]: RequestData
    *     Param 2 [IN]: Response XML object
    *     
    * Exceptions:
    *     NONE
    */
	
	private function _create_uninstall_mobility_status($requestdata, $requestIdObj)
	{
        global $commonfunctions_obj;
		$uninstall_details = $requestdata[0];
		
		$install_job_ids = array();
		foreach($uninstall_details as $server_key => $detail)
		{
			$install_job_ids[] = $detail['jobId'];
		}
		/* Array that defines the PushInstall job status */
		$status_array = array(-4=>"Invalid", 4=>"Pending", 5=>"InProgress", 6=>"Completed", -6=>"Failed");
		
		/* Get installation status for all jobs*/
		$install_status = $this->push_Obj->getInstallStatus($install_job_ids);		
		
		/* Build XML for each install request */
		foreach($requestdata[0] as $key_name => $details)
		{
			$install_job_id = $details['jobId'];
			$response_grp = new ParameterGroup($key_name);
			
			$response_grp->setParameterNameValuePair("HostId",$details['hostId']);
			$response_grp->setParameterNameValuePair("HostIP",$install_status[$install_job_id]['ipaddress']);
			
			if(!$install_status[$install_job_id]){
				ErrorCodeGenerator::raiseError('UNINSTALLMOBILITYSERVICE', 'EC_NO_RECORD_INFO', array(), 'Data Unavailable on this job');				
			}
			elseif(! array_key_exists($install_status[$install_job_id]['state'], $status_array)){
				ErrorCodeGenerator::raiseError('UNINSTALLMOBILITYSERVICE', 'EC_NO_RECORD_INFO', array(), 'Job in invalid state');
			}
			else
			{
				$response_grp->setParameterNameValuePair("Status",$status_array[$install_status[$install_job_id]['state']]);

				if ($status_array[$install_status[$install_job_id]['state']] == 'Failed')
				{
					$error_response = ErrorCodeGenerator::frameErrorResponse($install_status[$install_job_id]['errCode'], $install_status[$install_job_id]['statusMessage'], $install_status[$install_job_id]['errPlaceHolders'] ? unserialize($install_status[$install_job_id]['errPlaceHolders']) : array());
					$response_grp->addParameterGroup ($error_response);				
				}
				
				$response_grp->setParameterNameValuePair("StatusMessage",trim($install_status[$install_job_id]['statusMessage']));				
				$response_grp->setParameterNameValuePair("LastUpdateTime",$install_status[$install_job_id]['lastUpdatedTime']);
				
				/* We will display the error log contents if failed */
				if($install_status[$install_job_id]['state'] == -6 && file_exists($install_status[$install_job_id]['logFileLocation']))
                $install_log_file = $install_status[$install_job_id]['logFileLocation'];
                $install_log_data = "";
                $file_handle = $commonfunctions_obj->file_open_handle($install_log_file);
                if($file_handle)
                {
                    $install_log_data = fread($file_handle, filesize($install_log_file));
                    fclose($file_handle);
                }
				$response_grp->setParameterNameValuePair("ErrorMessage",$install_log_data);
			}
			$requestIdObj->addParameterGroup($response_grp);			
		}
		
		return $requestIdObj;
	}
	
	/*
    * Function Name: _create_update_mobility_status
    *
    * Description: 
    * This function is used to created the XML response 
	*	for InstallMobilityService API
    *
    * Parameters:
    *     Param 1 [IN]: RequestData
    *     Param 2 [IN]: Response XML object
    *     
    * Exceptions:
    *     NONE
    */
	
	private function _create_update_mobility_status($requestdata, $requestIdObj, $requestXml)
	{
		$install_job_ids = array_values($requestdata[0]);
		/* Array that defines the PushInstall job status */
		$status_array = array(0=>"Invalid", 1=>"Pending", 2=>"InProgress", 3=>"Completed", -3=>"Failed");
		
		/* Get installation status for all jobs*/
		$install_status = $this->push_Obj->getInstallStatus($install_job_ids);		
		
		/* Build XML for each install request */
		foreach($requestdata[0] as $key_name => $install_job_id)
		{
			$response_grp = new ParameterGroup($key_name);
			
			//Get the host id from apiRequest table XML data for the given request id.
			$host_guid = "";
			$vals = array();
			if ($requestXml)
			{
				$request_xml_data = unserialize($requestXml);
				//Creates an XML parser resource.
				$parser = xml_parser_create();
				$parse_status = xml_parse_into_struct($parser, $request_xml_data, $vals, $index);
				if ($parse_status)
				{
					//It frees the XML parser resource.
					xml_parser_free($parser);
					
					/*
					[14] => Array
					(
						[tag] => PARAMETER
						[type] => complete
						[level] => 6
						[attributes] => Array
							(
								[NAME] => HostId
								[VALUE] => df52537c-617a-4808-b6a5-32efc42a9cb2
							)

					)
					*/
					foreach($vals as $key=>$value)
					{
						if ($value['attributes']['NAME'] == 'HostId')
						{
							$host_guid = $value['attributes']['VALUE'];	
						}
					}
				}
			}
			$response_grp->setParameterNameValuePair("HostId",$host_guid);
			
			if(!$install_status[$install_job_id])
			$response_grp->setParameterNameValuePair("Status","Data Unavailable on this job");			
			elseif(! array_key_exists($install_status[$install_job_id]['state'], $status_array))
			$response_grp->setParameterNameValuePair("Status","Job in invalid state");			
			else
			{
				$response_grp->setParameterNameValuePair("Status",$status_array[$install_status[$install_job_id]['state']]);

				if ($status_array[$install_status[$install_job_id]['state']] == 'Failed')
				{
					$error_response = ErrorCodeGenerator::frameErrorResponse($install_status[$install_job_id]['errCode'], $install_status[$install_job_id]['statusMessage'], $install_status[$install_job_id]['errPlaceHolders'] ? unserialize($install_status[$install_job_id]['errPlaceHolders']) : array(),null,1,0,0,$install_status[$install_job_id]['installerExtendedErrors']);	
					$response_grp->addParameterGroup ($error_response);
				}
				
				$response_grp->setParameterNameValuePair("StatusMessage",trim($install_status[$install_job_id]['statusMessage']));				
				$response_grp->setParameterNameValuePair("LastUpdateTime",$install_status[$install_job_id]['lastUpdatedTime']);
				
				/* We will display the error log contents if failed */
				if($install_status[$install_job_id]['state'] == -3 && file_exists($install_status[$install_job_id]['logFileLocation']))
				$response_grp->setParameterNameValuePair("ErrorMessage", "");
				#$response_grp->setParameterNameValuePair("ErrorMessage",file_get_contents($install_status[$install_job_id]['logFileLocation']));
			}
			$requestIdObj->addParameterGroup($response_grp);			
		}
		
		return $requestIdObj;
	}
	
	/*
    * Function Name: _create_renew_certificate_status
    *
    * Description: 
    * This function is used to created the XML response 
	*	for RenewCertificate API
    *
    * Parameters:
    *     Param 1 [IN]: RequestData
    *     Param 2 [IN]: Response XML object
    *     
    * Exceptions:
    *     NONE
    */
	
	private function _create_renew_certificate_status($requestdata, $requestIdObj, $requestId)
	{
		global $requestXML, $activityId, $clientRequestId;	
		$sql_args = array();
		$renewStatus = "Succeeded";
		$policy_id_array = $requestdata["policyId"];
		$policy_ids = join(",", $policy_id_array);
		array_push($sql_args, $policy_ids);
		
		$policy_instance_sql = "SELECT
										policyState,
										executionLog,
										hostId
									FROM
										policyInstance 
									WHERE
										FIND_IN_SET(policyId, ?)
										ORDER BY policyState DESC";						
		$policy_details = $this->global_conn->sql_query($policy_instance_sql,$sql_args);
		
		$global_server_grp = new ParameterGroup("Servers");
		$policy_state = 0;
		$cs_final_status = 0;
		$count = 1;
		$log_data = "";
		
		/* Build XML for each install request */
		foreach($policy_details as $key => $policy_data)
		{
			// In case of CS+PS combined, if atleast one of them is in Inprogress.
			// Send Inprogress for both of them.
			if($policy_data["hostId"] == $requestdata["hostId"]) 
			{
				$policy_state = ($policy_state == 3 || $policy_state == 4) ? 3 : $policy_data["policyState"];
				$cs_final_status = $policy_state;
			}
			
			// In case if policy is in 3 or 4 send status as Inprogress.
			if($policy_data["policyState"] == 3 || ($policy_state == 3 || $policy_state == 4))
			{
				$renewStatus = "InProgress";
			}
			// In case if policy state is 2 send status as Failed.
			else if($policy_data["policyState"] == 2 || $policy_state == 2 )
			{
				$renewStatus = "Failed";
			}
			// In case if policy state is 1 send status as Succeeded.
			else if($policy_data["policyState"] == 1 || $policy_state == 1)
			{
				$renewStatus = "Succeeded";
			}
			
			$server_grp = new ParameterGroup("Server". $count);
			$server_grp->setParameterNameValuePair("HostId",$policy_data["hostId"]);
			$server_grp->setParameterNameValuePair("Status",$renewStatus);
			
			if($renewStatus == "Failed")
			{
				$log_data = $log_data . $policy_data["executionLog"];
				$executionLog = unserialize($policy_data["executionLog"]);
				$error_response = ErrorCodeGenerator::frameErrorResponse(
											$executionLog["ErrorCode"], 
											$executionLog["ErrorDescription"], 
											$executionLog["PlaceHolders"]);
				$server_grp->addParameterGroup ($error_response);
			}
			
			$global_server_grp->addParameterGroup ($server_grp);
			$count++;
		}
		$requestIdObj->addParameterGroup ($global_server_grp);
		
		if ($log_data != "")
		{
			$mds_data_array = array();
			$mds_data_array["activityId"] = $activity_id_to_log;
			$mds_data_array["jobType"] = "Cert";
			$mds_data_array["errorString"] = "Renew Certificate Client reqeust id: $clientRequestId, Activity id: $activity_id_to_log, LogMessage - $log_data";
			$mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
			$mds_data_array["errorType"] = "ERROR";
			$this->mds_obj = new MDSErrorLogging();
			$this->mds_obj->saveMDSLogDetails($mds_data_array);
		}
		
		return $requestIdObj;
	}
	
	/*
    * Function Name: _v2a_migration_status
    *
    * Description: 
    * This function is used to created the XML response 
	*	for V2A Migration API's
    *
    * Parameters:
    *     Param 1 [IN]: RequestData
    *     Param 2 [IN]: Response XML object
	*	  Param 3 [IN]: Request Id
	*	  Param 4 [IN]: Request Type
    *     
    */
	private function _v2a_migration_status($requestdata, $requestIdObj, $requestId, $request_type)
	{
		global $requestXML, $activityId, $clientRequestId;	
		$sql_args = array();
		$log_data = "";
		$policy_id_array = $requestdata["policyId"];
		$policy_ids = join(",", $policy_id_array);
		array_push($sql_args, $policy_ids);
		
		$policy_instance_sql = "SELECT
										policyState,
										executionLog,
										hostId
									FROM
										policyInstance 
									WHERE
										FIND_IN_SET(policyId, ?)
										ORDER BY policyState DESC";						
		$policy_details = $this->global_conn->sql_query($policy_instance_sql,$sql_args);
		
		foreach($policy_details as $key => $policy_data)
		{
			if($policy_data["policyState"] == 3) 
			{
				$status = 'Pending';
			}
			elseif($policy_data["policyState"] == 4) 
			{
				$status = 'InProgress';
			}
			elseif($policy_data["policyState"] == 1) 
			{
				$status = "Completed";
			}else 
			{
				$status = 'Failed';
			}
			
			$requestIdObj->setParameterNameValuePair("HostId",$policy_data["hostId"]);
			$requestIdObj->setParameterNameValuePair($request_type."Status",$status);
			
			if ($status == "Completed" and $request_type == "FinalReplicationCycle")
			{
				$json_content = json_decode($policy_data["executionLog"],TRUE,10,JSON_BIGINT_AS_STRING);
				if (json_last_error() === JSON_ERROR_NONE) 
				{
					$requestIdObj->setParameterNameValuePair("BookmarkType",$json_content[BookmarkType]);
					$requestIdObj->setParameterNameValuePair("BookmarkId",$json_content[BookmarkId]);
					
					if (is_array($json_content[SourceDiskIdToBookmarkDetailsMap]) and
						count($json_content[SourceDiskIdToBookmarkDetailsMap]) > 0)
					{
						$source_disks_details = new ParameterGroup("SourceDiskIdToBookmarkDetailsMap");
						$counter = 0;
						foreach ($json_content[SourceDiskIdToBookmarkDetailsMap] as $disk_name=>$disk_bookmark_details)
						{
							if (is_array($disk_bookmark_details) and count($disk_bookmark_details > 0))
							{
								$disk_grp = new ParameterGroup("Disk".$counter);
								$disk_grp->setParameterNameValuePair("DiskName",$disk_name);
								foreach ($disk_bookmark_details as $key=>$value)
								{
									$disk_grp->setParameterNameValuePair("$key",$value);
								}
								$source_disks_details->addParameterGroup($disk_grp);
								$counter++;	
							}				
						}
						$requestIdObj->addParameterGroup ($source_disks_details);
					}
				}
				//Sending to telemetry in case of invalid json reports from mobility agent.
				else
				{
					$log_data = $log_data ."Invalid JSON reported from mobility agent for HostId:".$policy_data["hostId"]." and for policy id:".$policy_ids.".";
				}
			}
			elseif($status == "Failed")
			{
				$log_data = $log_data . $policy_data["executionLog"];

				$json_content = json_decode($policy_data["executionLog"],TRUE,10,JSON_BIGINT_AS_STRING);
				if (json_last_error() === JSON_ERROR_NONE) 
				{
					
					if (is_array($json_content) and count($json_content) > 0)
					{
						$source_status_arr['error_code'] = 	isset($json_content[Errors][0][error_name]) ? 
															$json_content[Errors][0][error_name] : '';
						$source_status_arr['error_place_holder'] = empty($json_content[Errors][0][error_params]) ? array() : $json_content[Errors][0][error_params];
					
						$error_response = ErrorCodeGenerator::frameErrorResponse(
													$source_status_arr['error_code'], 
													$request_type."Failed", 
													$source_status_arr['error_place_holder']);
						$requestIdObj->addParameterGroup ($error_response);
					}
				}
			}
		}
		
		if ($log_data != "")
		{
			$mds_data_array = array();
			$mds_data_array["activityId"] = $activity_id_to_log;
			$mds_data_array["jobType"] = "V2AMigration";
			$mds_data_array["errorString"] = "MachineRegistrationToRCM reqeust id: $clientRequestId, Activity id: $activity_id_to_log, LogMessage - $log_data";
			$mds_data_array["eventName"] = "CS";
			$mds_data_array["errorType"] = "ERROR";
			$this->mds_obj = new MDSErrorLogging();
			$this->mds_obj->saveMDSLogDetails($mds_data_array);
		}
		
		return $requestIdObj;
	}
	
	/*
    * Function Name: _create_migrate_acs_to_aad_status
    *
    * Description: 
    * This function is used to created the XML response 
	*	for MigrateAcsToAad API
    *
    * Parameters:
    *     Param 1 [IN]: RequestData
    *     Param 2 [IN]: Response XML object
    *     
    * Exceptions:
    *     NONE
    */
	
	private function _create_migrate_acs_to_aad_status($requestdata, $requestIdObj, $requestId)
	{
		global $requestXML, $activityId, $clientRequestId;	
		$sql_args = array();
		$log_data = "";
		
		if(!$requestdata || !is_array($requestdata)) {
			$error_response = ErrorCodeGenerator::frameErrorResponse(
											"ECA0003", 
											"Request data not found", 
											null);
			$requestIdObj->addParameterGroup($error_response);
			
			return $requestIdObj;
		}
		
        $host_ids = $job_ids = array();
		foreach($requestdata as $data) {
            $host_ids[] = $data["hostId"];
            $job_ids[] = $data["jobId"];
        }
		$job_details = $this->system_obj->getAgentJobStatus($job_ids);
		
		foreach($requestdata as $data) {
			$job_data = $job_details[$data["jobId"]];
			$server_param_grp = new ParameterGroup($data["hostId"]);
			$server_param_grp->setParameterNameValuePair("Status", $job_data['jobStatus']);
            $server_param_grp->setParameterNameValuePair("FriendlyName", $data['friendlyName']);
            $job_errors = ($job_data['errorDetails'] != "") ? unserialize($job_data['errorDetails']) : array();
            if(count($job_errors))
            {
                foreach($job_errors as $job_error)
                {
                    $error_response = ErrorCodeGenerator::frameErrorResponse($job_error[1], $job_error[2], $job_error[5] ? unserialize($job_error[5]) : array(), null, 0, $job_error[1]);
                    $server_param_grp->addParameterGroup($error_response);
                }
                $log_data .= "Failed to migrate the Master Target with hostId - $host_id, FriendlyName - ".$data['friendlyName'].", ErrorDetails : ".print_r($job_errors, true);
            }
            $requestIdObj->addParameterGroup ($server_param_grp);
		}
		
		if ($log_data != "")
		{
			$mds_data_array = array();
			$mds_data_array["activityId"] = $activityId;
			$mds_data_array["jobType"] = "MigrateACSToAAD";
			$mds_data_array["errorString"] = "MigrateACSToAAD job failed with Client reqeust id: $clientRequestId, Activity id: $activityId, LogMessage - $log_data";
			$mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
			$mds_data_array["errorType"] = "ERROR";
			$this->mds_obj = new MDSErrorLogging();
			$this->mds_obj->saveMDSLogDetails($mds_data_array);
		}
		
		return $requestIdObj;
	}
	
	private function _get_request_status_vcon($identity, $version, $functionName, $parameters)
	{
		$request_list_obj = $parameters['RequestIdList'];
		$obj = $parameters['RequestIdList']->getChildList();
		foreach($obj as $request_id_obj)
		{
			$request_id = trim($request_id_obj->getValue());
			$sql = "SELECT 
						state,
						requestType
					FROM
						apiRequest
					WHERE
						apiRequestId = $request_id";
			$rs = $this->conn->sql_query($sql);
			$count = $this->conn->sql_num_row($rs);
			if(!$count) {
				$this->raiseException(ErrorCodes:: EC_NO_RQUSTID_FOND,"No request found for this request Id");
			}
			$row = $this->conn->sql_fetch_object($rs);
			$state = $row->state;
			$requestType = $row->requestType;
			if(($state == 4) || ($state == 3)) {
				$sql = "SELECT 
							state,
							status
						FROM
							apiRequestDetail
						WHERE
							apiRequestId = $request_id";
				$rs = $this->conn->sql_query($sql);
				$row = $this->conn->sql_fetch_object($rs);
				$instance_state = $row->state;
				$error = str_replace('"','',$row->status);
			}
			$response = new ParameterGroup ( "Response" );
			$response->setParameterNameValuePair('RequestAPIName', $requestType);
			if($requestType == 'InsertScriptPolicy') {
				$requestData = $this->srm_Obj->get_api_request_data($request_id);
				$policyDetails = unserialize($requestData);
				$policyId = $policyDetails['PolicyId'];
				if($policyId) {
					$policyStatus = $this->policyObj->get_policy_status($policyId);
					$error = $policyStatus[0]['executionLog'];
					$error = str_replace('"','',$error);
					if($policyStatus[0]['policyState'] == 1) {
						$state = 3; //Completed
						$status = "Success";
					}elseif($policyStatus[0]['policyState'] == 2) {
						$state = 4; //Failed
						$status = 'Failed';
					}elseif($policyStatus[0]['policyState'] == 3) {
						$state = 1; //Pending
						$status = 'Pending';
					}else {
						$state = 2; //InProgress
						$status = 'InProgress';
					}
					$this->srm_Obj->update_api_request_data($request_id,$state,$error);
				}
				if($state == 3) {
					$status = "Success";
					$response->setParameterNameValuePair('Status', $status);
					$response->setParameterNameValuePair('ErrorMessage',$error);
				} elseif($state == 4) {
					$status = 'Failed';
					$response->setParameterNameValuePair('Status',$status);
					$response->setParameterNameValuePair('ErrorMessage',$error);
				} else {
					$response->setParameterNameValuePair('Status',$status);
					$response->setParameterNameValuePair('ErrorMessage',$error);
				}
			}
			elseif($requestType == 'UpgradeAgents')
			{
				$requestDetailId = $this->srm_Obj->get_request_data($request_id);
				foreach($requestDetailId as $key=>$requestId)
				{
					$hostIP = $this->push_Obj->get_ip_by_api_request_id($requestId);
					$api_state = $this->push_Obj->push_install_check_status($hostIP);
					if($api_state == -3)
						$api_state = 4;
					$this->push_Obj->update_api_push_install_state($requestId,$api_state);
					$ipAddress = new ParameterGroup ($hostIP);						
					if($api_state!=3)
					{
						$statusMessage = trim($this->push_Obj->push_install_status_message($hostIP));
						if($api_state == 1)$status = "Pending";
						elseif($api_state == 2)$status = "InProgress";
						elseif($api_state == 4)$status = "Failed";
						$ipAddress->setParameterNameValuePair('Status', $status);
						$ipAddress->setParameterNameValuePair('HostIP', $hostIP);
						if($statusMessage == "The installation of UA build  Failed Please Check view log for more INFO")
							$statusMessage = "The installation of UA build Failed Please Check the log";
						elseif(preg_match("/Failed to exec push client on/i",$statusMessage))
							$statusMessage = "Failed to exec push client on $hostIP Machine. Please check log for more info.";
						$ipAddress->setParameterNameValuePair('StatusMessage', $statusMessage);
					}
					else
					{
						$status = "Success";
						$statusMessage = "Upgrade Completed";
						$ipAddress->setParameterNameValuePair('Status', $status);	
						$ipAddress->setParameterNameValuePair('HostIP', $hostIP);
						$ipAddress->setParameterNameValuePair('StatusMessage', $statusMessage);				
					}
					$response->addParameterGroup($ipAddress);
				}
			
			}
			elseif($requestType == 'PauseReplicationPairs')
			{
				if($state == 3) {
					$status = "Success";
					$response->setParameterNameValuePair('Status', $status);
				}
				else{
					$requestData = $this->srm_Obj->get_api_request_data($request_id);
					$pairInfo = unserialize($requestData);
					$status = 'Success';
					$pause_status = $this->volume_obj->get_pair_status($pairInfo['sourceHostId'],$pairInfo['destinationHostId']);
					if($pause_status == 1) {
							$state = 1; //Pending
							$status = "Pending";
						}
					elseif($pause_status == 3) {
							$state = 3; //Completed
							$status = 'Success';
						}
					if($pause_status == 1 || $pause_status == 3)
					$this->srm_Obj->update_api_request_data($request_id,$state,'');
					$response->setParameterNameValuePair('Status',$status);
				}
			}
			elseif($requestType == 'RemoveProtection')
			{
				if($state == 3) {
					$status = "Success";
					$response->setParameterNameValuePair('Status', $status);
				}
				else{
					$requestData = $this->srm_Obj->get_api_request_data($request_id);
					$planInfo = unserialize($requestData);
					$pairInfo = unserialize($planInfo['pairInfo']);
					$status = 'Success';
					$vxPairCount = 0;
					$fxPairCount = 0;
					if(count($pairInfo))
					{			
						foreach($pairInfo as $data) {	
							$planId = $data['planId'];					
							foreach($data['vx'] as $vx) {
								$srcId = $vx['srcId'];
								$trgId = $vx['trgId'];								
								$vxPairStatus = $this->volume_obj->GetPairDeleteStatus($planId,$srcId,$trgId,1);
								if(!$vxPairStatus['pair_count']) $vxPairCount++;
							}
							foreach($data['fx'] as $fx) {
								$srcId = $fx['srcId'];
								$trgId = $fx['trgId'];
								$fxPairStatus = $this->volume_obj->GetPairDeleteStatus($planId,$srcId,$trgId,2);
								if(!$fxPairStatus['pair_count']) $fxPairCount++;
							}					
						}
						if((count($data['vx']) == $vxPairCount) && (count($data['fx']) == $fxPairCount)) 
						{
							$state = 3; //Completed
							$status = 'Success';
						}
						else
						{
							$state = 1; //Pending
							$status = "Pending";
						}
					}			
					if($state == 1 || $state == 3)
					$this->srm_Obj->update_api_request_data($request_id,$state,'');
					$response->setParameterNameValuePair('Status',$status);
				}
			}
			else
			{
				if($state == 3) {
					$data = $this->system_obj->getRequestedHostInfo($request_id);
					if($requestType == 'RefreshHostInfo') {
						$status = "Success";
						$response->setParameterNameValuePair('Status', $status);
						$this->GetHostInfo('','','',$data['requestdata'],$request_id,$response);
					}
					else
					{
						$status = "Success";
						$response->setParameterNameValuePair('Status', $status);
					}
				} else {
					$status = 'Failed';
					if($state == 1) {
						$status = 'Pending';
					} elseif($state == 2) {
						$status = 'InProgress';
					}
					$response->setParameterNameValuePair('Status', $status);				
				}
			}
			return $response;
		}
	}
	
		public function GetHostInfo($identity, $version, $functionName, $parameters,$request_id='',$response='')
        {
                if(is_object($parameters['HostGUID']))
                {
                        $host_id = trim($parameters['HostGUID']->getValue());
                        $host_id = $this->checkAgentGUID($this->conn , $host_id);
                }
                elseif(is_object($parameters['HostIP']))
                {
                        $host_ip = trim($parameters['HostIP']->getValue());
                        $host_id = $this->checkAgentIP($this->conn , $host_ip);
                }
                elseif(is_object($parameters['HostName']))
                {
                        $host_name = trim($parameters['HostName']->getValue());
                        $host_id = $this->checkAgentName($this->conn , $host_name);
                }
                if($request_id) {
                        $host_id = $parameters['HostGUID'];
                }
                if($parameters['InformationType'])
                        $informationType = strtoupper(trim($parameters['InformationType']->getValue()));
                else
                        $informationType = "ALL";
                        $result = $this->system_obj->ListHostInfo($host_id,$informationType);
                if(!$request_id) {
                        $response = new ParameterGroup ( "Response" );
                }
				$disk_array = $nic_array = array();
                foreach($result as $key=>$val)
                {
                        foreach($val as $k=>$v)
                        {
                                if($k != "disk" && $k!= "nicinfo")
                                {
                                        $response->setParameterNameValuePair($k, $v);
                                }
                                if($k == "disk")
                                {
                                        $disk_array = $v;
                                }
								 if($k == "nicinfo")
									{
											$nic_array = $v;
									}
							}
							$disksList = new ParameterGroup("DiskList");
							foreach($disk_array as $key1 =>$val1)
							{
									$key1= $key1+1;
									$disks = new ParameterGroup("Disk$key1");
									foreach($val1 as $k1=>$v1)
									{

									if($k1 != "partition" )
									{
											$disks->setParameterNameValuePair($k1,$v1);
									}
									else
									{
										if($k1 == "partition")
										{
											$j=1;
											foreach($v1 as $k2=>$v2)
											{
												$partitions = new ParameterGroup("Partition$j");
												foreach($v2 as $key_name => $key_value)
												{
												if($key_name != "logicalVolume")
												{
														$partitions->setParameterNameValuePair($key_name,$key_value);
												}
												else
												{
													foreach($key_value as $lv_key => $lv_value)
													{                                                                            
															$lv_key = $lv_key+1;
															$logicalVolume = new ParameterGroup("LogicalVolume$lv_key");
															foreach($lv_value as $lv => $lv_data)
															{                                                                    
																	$logicalVolume->setParameterNameValuePair($lv,$lv_data);
															}
															$partitions->addParameterGroup($logicalVolume);
													}
											   }

											   }
											$disks->addParameterGroup($partitions);
											$j++;
										}
									}
								}
                                }
                                $disksList->addParameterGroup ($disks);
                        }
                        $response->addParameterGroup ($disksList);

                        $i=1;
                        $macAddressList = new ParameterGroup("MacAddressList");
                        foreach($nic_array as $key2 =>$val2)
                        {
                                $macAddress = new ParameterGroup("MacAddress$i");
                                $macAddress->setParameterNameValuePair("MacAddress",$key2);
                                foreach($val2 as $k3 =>$v3)
                                {
                                        if($k3 == 0)
                                        {
                                                $macAddress->setParameterNameValuePair("VendorName",$v3);
                                        }
                                        else
                                        {
                                                $ipAddress = new ParameterGroup("IpAddress$k3");
                                                foreach($v3 as $k4 => $v4)
                                                {
                                                        $ipAddress->setParameterNameValuePair($k4,$v4);
                                                }
                                                $macAddress->addParameterGroup ($ipAddress);
                                        }

                                }
                                $i++;
                                $macAddressList->addParameterGroup($macAddress);
                        }
                        $response->addParameterGroup($macAddressList);
						 }
                if(!$request_id)
                return $response->getChildList();
   }
   
    public function GetHostInfo_validate ($identity, $version, $functionName, $parameters)
	{
			$host_parameter = "HostGUID";
			if($parameters['HostGUID'])
			{
					$host_obj = $parameters['HostGUID'];
			}
			elseif($parameters['HostIP'])
			{
					$host_obj = $parameters['HostIP'];
					$host_parameter = "HostIP";
			}
			elseif($parameters['HostName'])
			{
					$host_obj = $parameters['HostName'];
					$host_parameter = "HostName";
			}
			if(!is_object($host_obj))
			{
					$message = "$host_parameter Parameter Is Missed From request XML";
					$this->raiseException(ErrorCodes::EC_INSFFCNT_PRMTRS,$message);
			}
			if(trim($host_obj->getValue()) == "")
			{
					$message = "No Data Found for $host_parameter";
					$this->raiseException(ErrorCodes::EC_NO_DATA,$message);
			}
			$info_obj = $parameters['InformationType'];
			if(is_object($info_obj))
			{
					$type = array('ALL','BASIC','NETWORK','STORAGE');
					$info_type_value = trim($info_obj->getValue());
					if($info_type_value == "")
					{
					$message = "No Data Found for InformationType";
					$this->raiseException(ErrorCodes::EC_NO_DATA,$message);
					}
					elseif(!in_array(strtoupper($info_type_value),$type))
					{
					$message = "Invalid value passed for InformationType";
					$this->raiseException(ErrorCodes::EC_INVLD_INFRMTNTYP,$message);
					}
			}
	}
    
	/*
	* Function Name: _stop_protection_status
    *
	* Description:
    * Construct the response xml for GetRequestStatus API on StopProtection
	*  
	* Parameters: 
	*   $request_id: request id sent in response xml for StopProtection API call
	*   $requestdata: value contained in requestdata column in apiRequestDetail table for the above request id
	*   $response: object of the ParameterGroup class
    *	
	* Return Value:
	*   Response XML string
	*/
	private function _stop_protection_status($request_id, $requestdata, $response)
	{
        global $RELEASE_DRIVE_ON_TARGET_DELETE_FAIL,$SCENARIO_DELETE_PENDING;

        $plan_id = $requestdata['planId'];
        $plan_name = $requestdata['PlanName'];;
        $plan_delete_status = "Delete Success";
        
        $request_status = new ParameterGroup($request_id);
        $request_status->setParameterNameValuePair('ProtectionPlanId', $plan_id);
        $request_status->setParameterNameValuePair('ProtectionPlanName', $plan_name);
        
        $arr_source_host_ids = $requestdata['sourceHostIdsScenarioIds'];
        $this->conn->enable_exceptions();
		try
		{
			$this->conn->sql_begin();
			if(!(is_array($arr_source_host_ids) && count($arr_source_host_ids)))
			{
				$sql0 = "SELECT sourceId, scenarioId FROM applicationScenario WHERE planId = ? AND executionStatus = ?";
				$arr_source_host_ids = $this->conn->sql_get_array($sql0, "sourceId", "scenarioId", array($plan_id,$SCENARIO_DELETE_PENDING));
			}

            if(is_array($arr_source_host_ids))
            {
                $servers_grp = new ParameterGroup("Servers");
                $server_count = 1;
                foreach($arr_source_host_ids AS $source_host_id => $scenario_id)
                {
                    $server_grp = new ParameterGroup("Server".$server_count);
                    $server_grp->setParameterNameValuePair("HostId", $source_host_id);
                    
                    $sql1 = "SELECT count(*) AS no_of_scenarios FROM applicationScenario WHERE planId = ? AND sourceId = ? AND scenarioId = ?";
                    $rs1 = $this->conn->sql_query($sql1, array($plan_id, $source_host_id, $scenario_id));
                    $no_of_scenarios = $rs1[0]['no_of_scenarios'];
                    
                    $sql2 = "SELECT sourceDeviceName,replication_status, replicationCleanupOptions FROM srcLogicalVolumeDestLogicalVolume WHERE planId = ? AND sourceHostId = ? AND scenarioId = ?";
                    $rs2 = $this->conn->sql_query($sql2, array($plan_id, $source_host_id, $scenario_id));
                    
					$no_of_pairs = count($rs2);
                    $disk_grp = new ParameterGroup("Disks");
					$i=1;
					foreach($rs2 AS $srcData)
					{
						$disk_grp->setParameterNameValuePair("Disk".$i, $srcData['sourceDeviceName']);
						$disk_grp->setParameterNameValuePair("replication_status".$i, $srcData['replication_status']);
						$disk_grp->setParameterNameValuePair("replicationCleanupOptions".$i, $srcData['replicationCleanupOptions']);
						$i++;
					}
					
					if($no_of_scenarios == 0 && $no_of_pairs == 0)
                    {
                        $server_grp->setParameterNameValuePair("DeleteStatus", "Delete Success");
                    }
                    else
                    {
                        if($no_of_pairs)
                        {
                            $sql3 = "SELECT count(*) AS no_of_delete_failed_pairs FROM srcLogicalVolumeDestLogicalVolume WHERE planId = ? AND sourceHostId = ? AND scenarioId = ? AND replication_status = '$RELEASE_DRIVE_ON_TARGET_DELETE_FAIL'";
                            $rs3 = $this->conn->sql_query($sql3, array($plan_id, $source_host_id, $scenario_id));
                            $no_of_delete_failed_pairs = $rs3[0]['no_of_delete_failed_pairs'];
                        }
                        
                        if(isset($no_of_delete_failed_pairs) && $no_of_delete_failed_pairs)
                        {
                            $server_grp->setParameterNameValuePair("DeleteStatus", "Delete Failed");
                            $plan_delete_status = "Delete Failed";
                        }
                        else
                        {
                            $server_grp->setParameterNameValuePair("DeleteStatus", "Delete Pending");
                            if($plan_delete_status != "Delete Failed") $plan_delete_status = "Delete Pending";
                        }
                    }
					$server_grp->setParameterNameValuePair("ScenarioStatus", "$no_of_scenarios");
					$server_grp->addParameterGroup($disk_grp);
                    $servers_grp->addParameterGroup($server_grp);
                    $server_count+=1;
                }
            }
            
            $this->conn->sql_commit();
        }
        catch(Exception $sql_excep)
        {
            $this->conn->sql_rollback();
            ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
        }
        $this->conn->disable_exceptions();                        
        
        $request_status->setParameterNameValuePair('ProtectionPlanStatus', $plan_delete_status);
        if(isset($servers_grp) && $servers_grp) $request_status->addParameterGroup($servers_grp);
        $response->addParameterGroup($request_status);
        return $response;
    }
	
	public function GetProtectionState_validate ($identity, $version, $functionName, $parameters)
	{
		$function_name_user_mode = 'Get Protection State';
		try
		{
			$this->global_conn->enable_exceptions();
			// Validate Protection Plan id check
			$ProtectionPlanId = $this->validateParameter($parameters,'ProtectionPlanId',FALSE,'NUMBER');
			//Error: Raise error if PlanId does not exists
			$planName = $this->protectionObj->getPlanName($ProtectionPlanId);
			if(!$planName) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $ProtectionPlanId));
			
			// server level check
			if(isset($parameters['Servers']) === TRUE) {
				//Validate source server details 
				$sourceServerObj = $parameters['Servers']->getChildList();
				if((is_array($sourceServerObj) === FALSE) || (count($sourceServerObj) == 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'Servers'));
				foreach($sourceServerObj as $k=>$v)
				{
					//Validate SourceId
					$srcId = $this->validateParameter($sourceServerObj,$k,FALSE);
					$srcCount = $this->global_conn->sql_get_value("hosts", "count(*)", "id = ? AND sentinelEnabled = '1' AND outpostAgentEnabled = '1' AND agentRole = 'Agent'",array($srcId));
						
					//Error: Src Not registered
					if($srcCount == 0) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
					
					//Check if SrcID is protected
					$sql = "SELECT count(*) AS srcCount FROM applicationScenario WHERE planId = ? AND scenarioType = ? AND sourceId = ?";
					$rs = $this->global_conn->sql_query($sql,array($ProtectionPlanId,'DR Protection',$srcId));
										
					for($i=0;isset($rs[$i]);$i++) {
						//Error: Src Not Protected
						if($rs[$i]['srcCount'] == 0) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_GUID_EXIST_IN_PLAN', array('SourceHostGUID' => $srcId));
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
	
	public function GetProtectionState ($identity, $version, $functionName, $parameters)
	{
		try
		{
			$this->global_conn->enable_exceptions();
			$response = new ParameterGroup ( "Response" );
			$result = array();
			$params = array();
			$cond = '';
			$ProtectionPlanId = trim($parameters['ProtectionPlanId']->getValue());
			$result['planId'] = $ProtectionPlanId;
			if(isset($parameters['Servers']) === TRUE) 
			{
				$sourceServerObj = $parameters['Servers']->getChildList();
				$srcIds = array();
				foreach($sourceServerObj as $v)	{
					$srcIds[] = trim($v->getValue());
				}
				$srcIdStr = implode(',',$srcIds);
				$cond = " AND FIND_IN_SET(sourceId, ?)";
				$params[] = $srcIdStr;
			}
			
			array_unshift($params, $ProtectionPlanId, 'DR Protection');
			$sql = "SELECT scenarioId,sourceId FROM applicationScenario WHERE planId = ? AND scenarioType = ? $cond";
			$rs = $this->global_conn->sql_query($sql,$params);
						
			for($i=0;isset($rs[$i]);$i++) {
				$result['scenarioId'][$rs[$i]['sourceId']] = $rs[$i]['scenarioId'];
			}
			$requestIdObj = new ParameterGroup ($ProtectionPlanId);
			$obj = $this->_create_protection_status($result, $requestIdObj);
			$response->addParameterGroup ($obj);
			
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
	
	public function ListPlans_validate ($identity, $version, $functionName, $parameters)
	{
        try
		{
			$this->global_conn->enable_exceptions();
			
			$source_host_arr = array();
			$planId = 0;
			
			if(isset($parameters['PlanId']))
			{
				//Validating planId, if planId parameter exists in request xml
				$planId = $this->validateParameter($parameters,'PlanId',TRUE, 'NUMBER');			
				$planName = $this->protectionObj->getPlanName($planId);						
				//Throwing error if no plan exist with the given PlanId
				if(!$planName) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $planId));
			}
			
			$plan_type_array = array("Protection", "Rollback");
			if(isset($parameters['PlanType']))
			{
				//Validating plan type for empty value, if PlanType parameter exists in request xml
				$planType = $this->validateParameter($parameters,'PlanType',TRUE);
				if(!in_array($planType, $plan_type_array)) ErrorCodeGenerator::raiseError($functionName, 'EC_INVALID_PLAN_TYPE');
			}
			
			if(isset($parameters['Servers']))
			{				
				$sourceServerObj = $parameters['Servers']->getChildList();
				//Validating HostId parameter if Servers parameters group exists
				if((is_array($sourceServerObj) === FALSE) || (count($sourceServerObj) == 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'HostId'));
				for($i=1; $i<=count($sourceServerObj); $i++)
				{            
					//Validating HostId parameter for empty value
					$sourceHostId = $this->validateParameter($sourceServerObj,'HostId'.$i,FALSE);
					$source_host_arr[] = $sourceHostId;
				}
				$source_host_arr = array_unique($source_host_arr);
				//$source_hosts_str = implode(",", $source_host_arr);
				//$host_sql = "SELECT id, ipAddress FROM hosts WHERE FIND_IN_SET(id, ?) AND sentinelEnabled = '1' AND outpostAgentEnabled = '1' AND agentRole = 'Agent'";				
				//$src_hosts = $this->global_conn->sql_get_array($host_sql, "id", "ipAddress", array($source_hosts_str));				
				
				if($planId) $source_id = $this->recovery_obj->getProtectedSourceHostId($planId);
				
				foreach($source_host_arr as $id)
				{
					//Throwing error if the given source host id is not registered with CS
					//if(!array_key_exists($id, $src_hosts))  ErrorCodeGenerator::raiseError($functionName, EC_NO_AGENTS,"$id");					
					if(is_array($source_id) and !in_array($id, $source_id)) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $id));					
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
	
	public function ListPlans ($identity, $version, $functionName, $parameters)
	{
        try
		{
			global $log_rollback_string, $activityId;	
			$this->global_conn->enable_exceptions();
			
			$response = new ParameterGroup ( "Response" );
			$source_host_arr = array();
			$planType = '';
			$planId = 0;
			
			if(isset($parameters['PlanId'])) $planId = trim($parameters['PlanId']->getValue());
			
			if(isset($parameters['PlanType']))
			{
				$planType = trim($parameters['PlanType']->getValue());
				$planType = ($planType == "Protection")? "DR Protection" : $planType ;
			}
			if(isset($parameters['Servers']))
			{
				$serverObj = $parameters['Servers']->getChildList();	
				for($i=1; $i<=count($serverObj); $i++)
				{
					$source_host_arr[] = trim($serverObj['HostId'.$i]->getValue());
				} 
			}
			$source_host_arr_log = serialize($source_host_arr);
			$log_rollback_string = $log_rollback_string."plan id = $planId, planType = $planType, source_host_arr = $source_host_arr_log".",";	
			$planDetails = $this->protectionObj->getPlanDetails($planId, $planType, $source_host_arr);
			$protection_planGroup = new ParameterGroup("ProtectionPlans");
			$rollback_planGroup = new ParameterGroup("RollbackPlans");
			$planDetails_log = serialize($planDetails);
			$log_rollback_string = $log_rollback_string."$planDetails_log = $planDetails_log".",";
			if(count($planDetails))
			{            
				foreach($planDetails as $scenarioType => $planInfo)
				{
					if($scenarioType == "DR Protection")
					{
						$i=1;
						foreach($planInfo as $planId => $planDetails)
						{
							$protectionPlanGroup = new ParameterGroup($i);
							$protectionPlanGroup->setParameterNameValuePair("ProtectionPlanId", $planId);
							$protectionPlanGroup->setParameterNameValuePair("ProtectionPlanName", $planDetails['planName']);
							$sourceGroup = new ParameterGroup("SourceServers");
							$k = 1;
							foreach($planDetails['sourceId'] as $sourceId)
							{
								$sourceGroup->setParameterNameValuePair("Server".$k, $sourceId); 
								$k++;
							}
							$protectionPlanGroup->addParameterGroup ($sourceGroup);
							$protection_planGroup->addParameterGroup ($protectionPlanGroup);
							$i++;
						}						
					}
					else if($scenarioType == "Rollback")
					{
						$i=1;
						foreach($planInfo as $planId => $planDetails)
						{
							if($planDetails['planName'])
							{
								$rollbackPlanGroup = new ParameterGroup($i);
								$rollbackPlanGroup->setParameterNameValuePair("RollbackPlanId", $planId);
								$rollbackPlanGroup->setParameterNameValuePair("RollbackPlanName", $planDetails['planName']);
								$rollbackPlanGroup->setParameterNameValuePair("RollbackStatus", $planDetails['rollbackStatus']);
								$sourceGroup = new ParameterGroup("SourceServers");
								$k = 1;
								foreach($planDetails['sourceId'] as $sourceId)
								{
									$sourceGroup->setParameterNameValuePair("Server".$k, $sourceId); 
									$k++;
								}
								$rollbackPlanGroup->addParameterGroup ($sourceGroup);
								$rollback_planGroup->addParameterGroup ($rollbackPlanGroup);
								$i++;
							}
						}
					}
				}				
				if($protection_planGroup) $response->addParameterGroup ($protection_planGroup);
				if($rollback_planGroup) $response->addParameterGroup ($rollback_planGroup);
			}	
			else {
				$response->addParameterGroup($protection_planGroup);
				$response->addParameterGroup($rollback_planGroup);
			}			
			$mds_data_array = array();
			$mds_obj = new MDSErrorLogging();
			if ($activityId)
			{
				$activity_id_to_log = $activityId;
			}
			else
			{
				$activity_id_to_log = "9cc8c6e1-2bfb-4544-a850-a23c9ef8162d";
			}
			$mds_data_array["jobId"] = "";
			$mds_data_array["jobType"] = "Rollback";
			$mds_data_array["errorString"] = $log_rollback_string;
			$mds_data_array["eventName"] = "CS";
			$mds_data_array["errorType"] = "ERROR";
			$mds_data_array["activityId"] = $activity_id_to_log;
			$mds_obj->saveMDSLogDetails($mds_data_array);
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
	
	public function ListEvents_validate($identity, $version, $functionName, $parameters)
	{
		try
		{
			$this->global_conn->enable_exceptions();
			
			$category = $severity = $alert_id = '';
			if(isset($parameters["Category"]))
			{
				$category = trim($parameters["Category"]->getValue());
				if(!$category) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Category"));			
			}
			
			if(isset($parameters["Severity"]))
			{
				$severity = trim($parameters["Severity"]->getValue());
				if(!$severity) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Severity"));
                $severity_lcase = strtolower($severity);
				if($severity_lcase != "all" && $severity_lcase != "error" && $severity_lcase != "warning" && $severity_lcase != "notice") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Severity"));
				if($severity_lcase == "error") $severity = "FATAL";
			}
			
			if(isset($parameters["Token"]))
			{
				$alert_id = trim($parameters["Token"]->getValue());
                if(!$alert_id) 
                {
                    ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Token"));
                }
                elseif(!ctype_digit("$alert_id"))
                {
                    ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Token"));
                }
			}
			
			//$alert_details = $this->monitor_obj->getDashboardAlerts($category, $severity, $alert_id);
			//if(!$alert_details) ErrorCodeGenerator::raiseError($functionName, EC_NO_ALERTS);
			
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
	
	public function ListEvents($identity, $version, $functionName, $parameters)
	{
		try
		{
			$this->global_conn->enable_exceptions();
			$response = new ParameterGroup ( "Response" );
			$category = $severity = $alert_id = '';
			$db_token_id = 0;
			if(isset($parameters["Category"])) $category = trim($parameters["Category"]->getValue());
			if(isset($parameters["Severity"])) $severity = trim($parameters["Severity"]->getValue());
			if(isset($parameters["Token"])) 
			{
				$alert_id = trim($parameters["Token"]->getValue());
				//After ASR processed the alerts, then only mark the alert for pruning.
				$db_token_id = $alert_id;
			}
            if(strtolower($severity) == "error") $severity = "FATAL";
			
			$alert_details = $this->monitor_obj->getDashboardAlerts($category,$severity, $alert_id);
			
			if(!$alert_details)
			{
				$alert_grp = new ParameterGroup("Alerts");
				$response->addParameterGroup($alert_grp);
			}
			else
			{
				$token_id = $this->global_conn->sql_get_value('dashboardAlerts', 'MAX(alertId)', "alertId > ?", array($alert_id));
				
				$response->setParameterNameValuePair("Token",$token_id);
				
				foreach($alert_details as $event_count => $alert_info)
				{
                    $host_id = '';
                    $event_type = '';
                    $infrastructure_vm_id = '';
					$event_obj = new ParameterGroup ("Event".($event_count +1));
					foreach($alert_info as $col_name => $col_value)
					{
                        if($col_name == "HostId") $host_id = trim($col_value);
                        if($col_name == "EventType") $event_type = $col_value;
                        
                        if($col_name == "Severity" && strtolower($col_value) == "fatal") $col_value = "ERROR";
                        
						if($col_name == "errPlaceholders") $event_place_holders = unserialize($alert_info['errPlaceholders']);
						else $event_obj->setParameterNameValuePair($col_name,$col_value);
					}
                    if(((strtolower(trim($event_type)) == "vm health") || (strtolower(trim($event_type)) == "agent health")) && $host_id)
                    {
                        $infrastructure_vm_id = $this->global_conn->sql_get_value('infrastructureVMs', 'infrastructureVMId', "hostId = ?", array($host_id));
                    }
                    if($infrastructure_vm_id) $event_obj->setParameterNameValuePair("InfrastructureVMId",$infrastructure_vm_id);
					
					$event_place_holders_obj = new ParameterGroup ("Placeholders");
					foreach($event_place_holders as $place_key => $event_value)
					{
						$event_place_holders_obj->setParameterNameValuePair($place_key,$event_value);
					}
					$event_obj->addParameterGroup($event_place_holders_obj);
					$response->addParameterGroup($event_obj);
				}
			}
			$sql = "UPDATE
									transbandSettings 
							SET
									ValueData = ? 
							WHERE
									ValueName = ?";

			$rs = $this->conn->sql_query($sql, array($db_token_id, "ALERTS_MAX_TOKEN_ID"));
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
	
	
	public function GetCSPSHealth_validate($identity, $version, $functionName, $parameters)
	{
		return;
	}
	
	public function GetCSPSHealth($identity, $version, $functionName, $parameters)
	{
		try
		{
			$this->global_conn->enable_exceptions();
			$response = new ParameterGroup("Response");
			$cs_ps_details = $this->processServerObj->getPerfAndServiceStatus();				
			$mars_health_status = $this->processServerObj->checkMarsComponentHealth();
			if(!is_array($cs_ps_details))
			{
				$component = new ParameterGroup("CSPSHealth");
				$response->addParameterGroup($component);
			}
			else
			{
				$server_count = 1;
				foreach($cs_ps_details as $key => $details)
				{
					$component = new ParameterGroup($key);
					
					if($key == "ConfigServer")
					{
						$component->setParameterNameValuePair('HostId', $details['HostId']);
						$component->setParameterNameValuePair('ProtectedServers', $details['ProtectedServers']);
						$component->setParameterNameValuePair('ReplicationPairCount', $details['ReplicationPairCount']);
						$component->setParameterNameValuePair('ProcessServerCount', $details['ProcessServerCount']);
						$component->setParameterNameValuePair('AgentCount', $details['AgentCount']);
						$component->setParameterNameValuePair('CSServiceStatus', $details['CSServiceStatus']);
						$component = $this->set_param_name($details['SystemPerformance'], $component);
					}
					elseif($key == "ProcessServers")
					{						
						foreach($details as $ps_guid => $ps_info)
						{
							//Default MARS health is green, when no health detected
							$mars_communication_status = "1";
							$mars_registration_status = "1";
							if (array_key_exists($ps_info['HostId'], $mars_health_status))
							{
								if (array_key_exists("marsCommunicationStatus", $mars_health_status[$ps_info['HostId']]))
									$mars_communication_status = $mars_health_status[$ps_info['HostId']]['marsCommunicationStatus'];
								if (array_key_exists("marsRegistrationStatus", $mars_health_status[$ps_info['HostId']]))
									$mars_registration_status = $mars_health_status[$ps_info['HostId']]['marsRegistrationStatus'];
							}
							$ps_grp = new ParameterGroup("Server".$server_count);
							$ps_grp->setParameterNameValuePair('HostId', $ps_info['HostId']);
							$ps_grp->setParameterNameValuePair('PSServerIP', $ps_info['PSServerIP']);
							$ps_grp->setParameterNameValuePair('Heartbeat', $ps_info['Heartbeat']);
							$ps_grp->setParameterNameValuePair('ServerCount', $ps_info['ServerCount']);
							$ps_grp->setParameterNameValuePair('ReplicationPairCount', $ps_info['ReplicationPairCount']);
							$ps_grp->setParameterNameValuePair('DataChangeRate', $ps_info['DataChangeRate']);
							$ps_grp->setParameterNameValuePair('PSServiceStatus', $ps_info['PSServiceStatus']);
							$ps_grp->setParameterNameValuePair('PSPerformanceStateTimeStamp', $ps_info['PSPerformanceStateTimeStamp']);
							$ps_grp->setParameterNameValuePair('MarsCommunicationStatus', $mars_communication_status);
							$ps_grp->setParameterNameValuePair('MarsRegistrationStatus', $mars_registration_status);
							$ps_grp = $this->set_param_name($ps_info['SystemPerformance'], $ps_grp);							
							if ($ps_info['HealthFactors'])
							{
								$ps_grp = $this->set_hf_param_name($ps_info['HealthFactors'], $ps_grp);		
							}
							$component->addParameterGroup($ps_grp);
							$server_count++;
						}
					}
					else //MT
					{
						$mt_count = 1;
						foreach($details as $mt_guid => $mt_info)
						{
							$mt_grp = new ParameterGroup("Server".$mt_count);
							$mt_grp->setParameterNameValuePair('HostId', $mt_info['HostId']);
							$mt_grp->setParameterNameValuePair('MTServerIP', $mt_info['MTServerIP']);
							$mt_grp->setParameterNameValuePair('Heartbeat', $mt_info['Heartbeat']);
							if ($mt_info['HealthFactors'])
							{
								$mt_grp = $this->set_hf_param_name($mt_info['HealthFactors'], $mt_grp);		
							}
							$component->addParameterGroup($mt_grp);
							$mt_count++;
						}
					}
					$response->addParameterGroup($component);					
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
	
	public function set_hf_param_name($data, $base_grp)
	{		
		$system_grp = new ParameterGroup("HealthFactors");		
		foreach ($data as $sys_key => $sys_val)
		{
			$count = $sys_key+1;
			$system_grp1 = new ParameterGroup("HealthFactor".$count);
			foreach ($sys_val as $k1 => $v1)
			{
				if ($k1 == "PlaceHolders")
				{					
					$ph_grp = new ParameterGroup("PlaceHolders");
					foreach ($v1 as $pkey => $pval)
					{
						$ph_grp->setParameterNameValuePair($pkey,$pval); 
					}
					$system_grp1->addParameterGroup($ph_grp);	
				}
				else
					$system_grp1->setParameterNameValuePair($k1, $v1);
			}
			$system_grp->addParameterGroup($system_grp1);
			$count++;
		}
		$base_grp->addParameterGroup($system_grp);
		
		return $base_grp;
	}

	public function set_param_name($data, $base_grp)
	{
		$system_grp = new ParameterGroup("SystemPerformance");
		foreach ($data as $sys_key => $sys_val)
		{
			$system_grp1 = new ParameterGroup($sys_key);
		
			foreach ($sys_val as $k1 => $v1)
			{
				$system_grp1->setParameterNameValuePair($k1, $v1);
			}
			$system_grp->addParameterGroup($system_grp1);
		}
		$base_grp->addParameterGroup($system_grp);
		
		return $base_grp;
	}
	
	public function CleanRollbackPlans_validate ($identity, $version, $functionName, $parameters)
	{
		try
		{		
			$this->global_conn->enable_exceptions();

			for($i=1;$i<=count($parameters); $i++)
			{
				if(isset($parameters['PlanId'.$i]))
				{
					//Validating planId, if planId parameter exists in request xml
					$planId = $this->validateParameter($parameters,'PlanId'.$i,TRUE, 'NUMBER');			
					if($planId && $planId !=1)
					{
						$planType = $this->protectionObj->getPlanType($planId);
						$application_plan_status = $this->global_conn->sql_get_value('applicationPlan', 'applicationPlanStatus', "planId = ?", array($planId)); 
							
						//Throwing error if no plan exist with the given PlanId
						if($planType != 'CLOUD_MIGRATION') ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $planId));
						
						//Throwing error if the plan type is rollback and it is in progress
						if($application_plan_status == "Rollback InProgress") ErrorCodeGenerator::raiseError($functionName, 'EC_ROLLBACK_INPROGRESS');

						//Throwing error if pairs  exists for the plan and it is in active state
						$delete_allow_states = array("Rollback Completed", "Rollback Failed");
						if(!in_array($application_plan_status, $delete_allow_states)) ErrorCodeGenerator::raiseError($functionName, 'EC_ROLLBACK_INPROGRESS');
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
    
    public function CleanRollbackPlans ($identity, $version, $functionName, $parameters)
    {
		try
		{		
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			$response = new ParameterGroup ( "Response" );
			
			for($i=1;$i<=count($parameters); $i++)
			{          
				if(isset($parameters['PlanId'.$i]))
				{
					$planId  = trim($parameters['PlanId'.$i]->getValue());
					if($planId != 1)
					{
						$deleteStatus = $this->protectionObj->deleteStalePlans($planId);
						$response->setParameterNameValuePair('PlanId'.$i, "Delete Success");
					}				
				}
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
	
	public function UnregisterAgent_validate($identity, $version, $functionName, $parameters)
	{
        try
		{
			global $commonfunctions_obj;
			$this->global_conn->enable_exceptions();
            if(!isset($parameters['HostGUID'])) ErrorCodeGenerator::raiseError(COMMON, EC_INSFFCNT_PRMTRS, "HostGUID");
            if(!trim($parameters['HostGUID']->getValue())) ErrorCodeGenerator::raiseError(COMMON, EC_NO_DATA, "HostGUID");
			
            $host_id = trim($parameters['HostGUID']->getValue());
			$guid_format_status = $commonfunctions_obj->is_guid_format_valid($host_id);
			if ($guid_format_status == false)
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HostGUID"));
			}
		
            // Disable Exceptions for DB
            $this->global_conn->disable_exceptions();
        }
        catch(SQLException $sql_excep)
		{		
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError(COMMON, 'EC_DATABASE_ERROR', '', "Database error occurred");
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
	* This API is used to un register all agent related information from CX DB 
	*/
	public function UnregisterAgent($identity, $version, $functionName, $parameters)
	{
		$retry = 0;
		$done = false;
		global $DB_TRANSACTION_RETRY_LIMIT, $DB_TRANSACTION_RETRY_SLEEP,$requestXML, $activityId;
		$response = new ParameterGroup ( "Response" );
		$hostId = trim($parameters['HostGUID']->getValue());

		while (!$done and ($retry < $DB_TRANSACTION_RETRY_LIMIT)) 
		{
			try
			{
				$this->global_conn->enable_exceptions();
				$this->global_conn->sql_begin();
				$vx_status = $this->volume_obj->unregister_vx_agent( $hostId, 2); //Un registering vx agent from CX DB
				$fx_status = $this->fileProtectionObj->unregister_fr_agent ($hostId , 2); // Un registering FX agent from CX DB
				if(!$vx_status) throw new SQLException('Unregister VX agent failed'); // Throwing  SQL exception if we get SQL error as part of VX un registration call
				if(!$fx_status) throw new SQLException('Unregister FX agent failed'); // Throwing  SQL exception if we get SQL error as part of FX un registration call
				$this->global_conn->sql_commit();
				// Disable Exceptions for DB
				$this->global_conn->disable_exceptions();
				$done = true;
				$response->setParameterNameValuePair("Status", "Success");
				return $response->getChildList();
			}
			catch(SQLException $sql_excep)
			{			
				$this->global_conn->sql_rollback();
				// Disable Exceptions for DB
				$this->global_conn->disable_exceptions();
				if ($retry < $DB_TRANSACTION_RETRY_LIMIT)
				{
					sleep($DB_TRANSACTION_RETRY_SLEEP);
					$retry++;
				}
				
				if ($retry == $DB_TRANSACTION_RETRY_LIMIT)
				{
					$this->common_obj->captureDeadLockDump($requestXML, $activityId, "Protection");	
					ErrorCodeGenerator::raiseError(COMMON, 'EC_DATABASE_ERROR', '', "Database error occurred");
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
				$this->global_conn->sql_rollback();
				// Disable Exceptions for DB
				$this->global_conn->disable_exceptions();		
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
			} 
		}
	}
	
	public function GetRollbackState_validate($identity, $version, $functionName, $parameters)
    {
		try
		{
			$this->global_conn->enable_exceptions();
			
			if(isset($parameters['RollbackPlanId']))
			{
				$rollback_plan_id = $parameters['RollbackPlanId']->getValue();
				if(!$rollback_plan_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "RollbackPlanId"));
				$plan_exists = $this->recovery_obj->validate_migration_plan($rollback_plan_id);
				if(!$plan_exists) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RollbackPlanId"));
			}
			
			if(isset($parameters['Servers']))
			{
				$server_ids = array();
				$servers = $parameters['Servers']->getChildList();
				if(!is_array($servers)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Servers"));
				foreach ($servers as $key_name => $server_obj)
				{
					$server_id = $server_obj->getValue();
					if(in_array($server_id, $server_ids)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Servers. Duplicate ID for Server - ". $key_name));
				}
				$not_rolledback_servers = $this->recovery_obj->get_rollback_server_details($server_ids, "validation");
				if($not_rolledback_servers) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_RLBK_DATA', array('SourceHostGUID' => $not_rolledback_servers));
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
    
    public function GetRollbackState($identity, $version, $functionName, $parameters)
    {
        global $ROLLBACK_FAILED,$ROLLBACK_INPROGRESS,$ROLLBACK_COMPLETED, $asycRequestXML;
		try
		{
			$this->global_conn->enable_exceptions();
			
			$sql_args = array();
			if(isset($parameters['RollbackPlanId'])) 
			{
				$recovery_plan_id = $parameters['RollbackPlanId']->getValue();
				$plan_cond = " AND planId = ?";
				array_push($sql_args, $recovery_plan_id);
			}	
			
			if(isset($parameters['Servers']))
			{
				$server_id_arr = array();
				$servers = $parameters['Servers']->getChildList();
				foreach ($servers as $key_name => $server_obj)
				{
					$server_id = $server_obj->getValue();
					$server_id_arr[] = $server_id;
				}
				$server_id_str = implode(',',$server_id_arr);
				$host_cond = " AND FIND_IN_SET(sourceId, ?)";
				array_push($sql_args, $server_id_str);
			}
			
			$sql = "SELECT scenarioId, sourceId, planId FROM applicationScenario WHERE scenarioType = 'Rollback' $plan_cond $host_cond";
			$rollback_details = $this->global_conn->sql_query($sql,$sql_args);
			
			$server_details = array();
			foreach($rollback_details as $key => $scenario_data) {
				$server_details['scenarioId'][$scenario_data["sourceId"]] = $scenario_data["scenarioId"];
				$server_details['planId'][$scenario_data["sourceId"]] = $scenario_data["planId"];
			}
			
			$response = new ParameterGroup("Response");
			$response = $this->_create_rollback_status($server_details, $response);
			
			$error_set = ErrorCodeGenerator::getWorkFlowErrorCounter();
			if($error_set)
			{
				$policy_id = $this->global_conn->sql_get_value("policy", "policyId", "planId = ? AND policyType = 48", array($recovery_plan_id));
				$asycRequestXML = $this->global_conn->sql_get_value("apiRequest", "requestXml", "referenceId LIKE ? AND requestType = 'CreateRollback'", array("%,$policy_id,%"));
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
}
?>