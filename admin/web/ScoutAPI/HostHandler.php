<?php
class HostHandler extends ResourceHandler 
{
	private $system_obj;
	private $conn;
	
	public function HostHandler() 
	{
        global $conn_obj;
		$this->system_obj = new System();
		$this->conn = new Connection();
        $this->global_conn = $conn_obj;
		$this->SRMObj = new SRM();
		$this->commonfunctions_obj = new CommonFunctions();
		$this->processserver_obj = new ProcessServer();
		$this->retention_obj = new Retention();
		$this->accounts_obj = new Accounts();
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
									elseif(is_array($key_value))
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
				$key2 = rtrim($key2);
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
	
	public function RefreshHostInfo_validate ($identity, $version, $functionName, $parameters) 
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
		else
		{
			$count = 1;
			while(is_object($parameters['Host'.$count])) {
				$obj = $parameters['Host'.$count]->getChildList();
				if(!is_object($obj['HostGUID'])) {
					$message = "Missing HostGUID parameter for Host$count";
					$this->raiseException(ErrorCodes::EC_NO_DATA,$message);
				}else {
					if(!trim($obj['HostGUID']->getValue())) {
						$message = "No Data Found in HostGUID parameter for Host$count";
						$this->raiseException(ErrorCodes::EC_NO_DATA,$message);
					}
					$host_id = trim($obj['HostGUID']->getValue());
					$host_id = $this->checkAgentGUID($this->conn , $host_id);
				}
				if(!is_object($obj['Option'])) {
					$message = "Missing Option parameter for Host$count";
					$this->raiseException(ErrorCodes::EC_NO_DATA,$message);
				}
				else {
					if(!trim($obj['Option']->getValue())) {
						$message = "No Data Found in Option parameter for Host$count";
						$this->raiseException(ErrorCodes::EC_NO_DATA,$message);
					}
				}
				$count++;
			}
			if($count == 1) {
				$message = "Missing HostGUID parameter";
				$this->raiseException(ErrorCodes::EC_NO_DATA,$message);
			}
			return;
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
	}
	
	public function RefreshHostInfo($identity, $version, $functionName, $parameters)
	{
		global $requestXML;
		$host_id_list = array();
		$options = array();
		if(is_object($parameters['HostGUID']))
		{
			$host_id = trim($parameters['HostGUID']->getValue());
			$host_id = $this->checkAgentGUID($this->conn , $host_id);
			$host_id_list[] = $host_id;
		}
		elseif(is_object($parameters['HostIP']))
		{
			$host_ip = trim($parameters['HostIP']->getValue());
			$host_id = $this->checkAgentIP($this->conn , $host_ip);
			$host_id_list[] = $host_id;
		}
		elseif(is_object($parameters['HostName']))
		{
			$host_name = trim($parameters['HostName']->getValue());
			$host_id = $this->checkAgentName($this->conn , $host_name);
			$host_id_list[] = $host_id;
		}
		else {
			$count = 1;
			while(is_object($parameters['Host'.$count])) {
				$obj = $parameters['Host'.$count]->getChildList();
				$host_id = trim($obj['HostGUID']->getValue());
				$option = trim($obj['Option']->getValue());
				
				$host_id_list[] = $host_id;
				$options[$host_id] = $option;
				
				$count++;
			}
		}
		if(count($host_id_list)) {
			$host_id_list = array_unique($host_id_list);
			sort($host_id_list);
			$md5_str = implode(",",$host_id_list);
			$md5CheckSum = md5($md5_str);
			
			try
			{
				$this->conn->sql_query("BEGIN");
				$result = $this->SRMObj->insert_request_xml($requestXML,$functionName,$md5CheckSum);
				$requestApiId = $result['apiRequestId'];
				if(!$result['requestType']) {
					foreach($host_id_list as $id) {
						$requestedData = array("HostGUID" => $id, 'Option' => $options[$id]);
						$this->SRMObj->insert_request_data($requestedData,$requestApiId,$functionName);
						$this->system_obj->updateHostRefreshStatus($id,$requestApiId);
					}
				}
				$this->conn->sql_query("COMMIT");
				$response = new ParameterGroup("Response");
				$response->setParameterNameValuePair('RequestId',$requestApiId);
				return $response->getChildList();
			}
			catch (Exception $e)
			{
				$this->conn->sql_query("ROLLBACK");
				$message = "Internal Server failure";
				$returnCode = ErrorCodes::EC_UNKNWN;
				$this->raiseException($returnCode,$message);
			}
		}
	}
	
	/*
    * Function Name: ListScoutComponents
    *
    * Description: 
    * This function is retrieve the information on ScoutComponents
	*	(CS/PS/SourceServer/MasterTarget)
    *
    * Parameters:
    *     Param 1 [IN]: ComponentType
    *     Param 3 [IN]: OSType
    *     Param 4 [IN]: Servers (List of Server Host Ids)
    *     Param 5 [OUT]: The general information about the ScoutComponents
    * 
	* Defaults:		
    * Exceptions:
    *     NONE
    */

    public function ListScoutComponents_validate($identity, $version, $functionName, $parameters) 
    {
		$function_name_user_mode = 'List Scout Components';
		try
		{
			$this->conn->enable_exceptions();	
			
			$hosts_query = "SELECT
								name,
								ipaddress 
							FROM
								hosts 
							WHERE
								csEnabled = 1";						
			$hosts_details = $this->conn->sql_query($hosts_query);
			
			if($this->conn->sql_num_row($hosts_details) > 1)
			{	
				ErrorCodeGenerator::raiseError($functionName, 'EC_MULTIPLE_CS_FOUND');
			}
			
			/* Validating the ComponentType if passed */
	        if(isset($parameters['ComponentType']))
	        {
	            $component_type = $parameters['ComponentType']->getValue();
	            if(!$component_type) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "ComponentType"));
				// Validating the string passed
	            if($component_type != "All" && $component_type != "ProcessServers" && $component_type != "SourceServers" && $component_type != "MasterTargets") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ComponentType"));
	        }
	        
			/* Validating the OSType if passed */
			if($parameters['OSType'])
			{
				$os_type = $parameters['OSType']->getValue();
				if(!$os_type) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "OSType"));
				//Validating the string passed
	            if($os_type != "Linux" && $os_type != "Windows" && $os_type != "All") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "OSType"));
			}
	        
			/* Validating the Servers if passed */
	        if(isset($parameters['Servers']))
	        {
	            $hosts = $parameters['Servers']->getChildList();
	            foreach ($hosts as $key => $host)
	            {
					// Retrieving the hostId
	                $host_id = $host->getValue();
					if(!$host_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $key));
					// Validating the hostId for its existence in the DB
	                $host_info = $this->commonfunctions_obj->get_host_info($host_id);				
	                if(!$host_info) 
					{
						if($component_type == "ProcessServers") ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
						elseif($component_type == "SourceServers") ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND)); 
						elseif($component_type == "MasterTargets") ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
						else ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $key));
					}
					
					// Validating if specific component is passed and the hostId does not report as the given component type.
					if($component_type && $component_type != "All")
					{
						if($component_type == "ProcessServers" &&  $host_info['processServerEnabled'] != "1") ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECORD_INFO');
						if($component_type == "SourceServers" &&  ($host_info['outpostAgentEnabled'] != "1") && $host_info['sentinelEnabled'] != "1") ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECORD_INFO');
						if($component_type == "MasterTargets" &&  ($host_info['outpostAgentEnabled'] != "1") && $host_info['sentinelEnabled'] != "1") ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECORD_INFO');
					}
					// Validating if specific os type is passed and the hostId does not report as the given os type.
					if($os_type)
					{
						if($os_type == "Linux" && $host_info['osFlag'] != "1") ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECORD_INFO');
						if($os_type == "Windows" && $host_info['osFlag'] == "1") ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECORD_INFO');
					}
				}
				// Disable Exceptions for DB
				$this->conn->disable_exceptions();
			}			
        }
		catch(SQLException $sql_excep)
		{
			//$this->conn->sql_rollback();

			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
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
			//$this->conn->sql_rollback();

			// Disable Exceptions for DB
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function ListScoutComponents($identity, $version, $functionName, $parameters) 
    {
		try
    	{
			global $MDS_ACTIVITY_ID,$requestXML, $CS_IP;
			global $CONN_ALIVE_RETRY, $ROOT_ON_MULTIPLE_DISKS_SUPPORT_VERSION;
			$mds_log = "";
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			/* Retrieving the Input parameters */
			$component_type = (isset($parameters['ComponentType'])) ? $parameters['ComponentType']->getValue() : NULL;
			$os_type = (isset($parameters['OSType'])) ? $parameters['OSType']->getValue() : NULL;
			$host_ids = NULL;
			if(isset($parameters['Servers']))
			{
				$host_ids = array();
				$hosts = $parameters['Servers']->getChildList();			
				foreach($hosts as $host_obj)
				{
					$host_id = $host_obj->getValue();
					$host_ids[] = $host_id;
				}
			}
			
			/* Getting the component details from DB */
			$component_details =  $this->system_obj->getScoutComponents($component_type, $os_type, $host_ids);
			$inventory_id_details =  $this->system_obj->getInventoryIdsOfInfrastructureHosts();

			if(!$component_type || $component_type == "All" || $component_type == "ProcessServers")
			// Retrieving the NAT IPs
			$nat_details =  $this->system_obj->getPsNatIps($host_ids);
			
			// Fetching Patch Details
			$patch_details = $this->commonfunctions_obj->getPatchDetails(1);
			#$cs_version = $this->commonfunctions_obj->getPatchVersion(1);
			if(!$component_type || $component_type == "All" || $component_type == "SourceServers" || $component_type == "MasterTargets")
			{
				// Required for SourceServers / MasterTargets
				// Retrieving the disk details
				$device_details =  $this->system_obj->getDevices($host_ids);
				list($disk_group_struct) =  $this->system_obj->get_disk_volume_count($host_ids);
				
				/*
				Array
(
    [5C1DAEF0-9386-44a5-B92A-31FE2C79236A] => Array
        (
            [0] => Array
                (
                    [hostId] => 5C1DAEF0-9386-44a5-B92A-31FE2C79236A
                    [deviceName] => P
                    [systemVolume] => No
                    [mountPoint] => 
                    [capacity] => 81920000
                    [freespace] => 0
                    [volumeType] => 0
                    [deviceId] => P
                    [farLineProtected] => 0
                    [deviceProperties] => 
                    [fileSystemType] => 
                )

        )

    [30B7FA01-9568-5A4E-8828A076584962AD] => Array
        (
            [0] => Array
                (
                    [hostId] => 30B7FA01-9568-5A4E-8828A076584962AD
                    [deviceName] => 3556673838
                    [systemVolume] => No
                    [mountPoint] => 
                    [capacity] => 21474836480
                    [freespace] => 0
                    [volumeType] => 0
                    [deviceId] => 3556673838
                    [farLineProtected] => 0
                    [deviceProperties] => a:15:{s:11:"device_name";s:18:"\\.\PHYSICALDRIVE0";s:22:"has_bootable_partition";s:4:"true";s:14:"interface_type";s:3:"ide";s:12:"manufacturer";s:22:"(Standard disk drives)";s:10:"media_type";s:21:"Fixed hard disk media";s:5:"model";s:21:"Virtual HD ATA Device";s:8:"scsi_bus";s:1:"0";s:17:"scsi_logical_unit";s:1:"0";s:9:"scsi_port";s:1:"0";s:14:"scsi_target_id";s:1:"0";s:17:"sectors_per_track";s:2:"63";s:12:"storage_type";s:7:"dynamic";s:15:"total_cylinders";s:4:"2610";s:11:"total_heads";s:3:"255";s:13:"total_sectors";s:8:"41929650";}
                    [fileSystemType] => 
                )
)
				*/
				$disk_details = $this->system_obj->getDisks($host_ids);
			
				$partitions_data = $this->system_obj->getAllPartitions($host_ids);
				#print_r($partitions_data);
				#exit;
				/*Disk groups sample data
				[F755DE0-AD01-9841-B6EB63AA5C269F58912] => Array
        (
            [C] => Array
                (
                    [0] => \\.\PHYSICALDRIVE0
                )

            [C:\SRV] => Array
                (
                    [0] => \\.\PHYSICALDRIVE0
                )

            [E] => Array
                (
                    [0] => \\.\PHYSICALDRIVE1
                )

            [F] => Array
                (
                    [0] => \\.\PHYSICALDRIVE1
                )

            [G] => Array
                (
                    [0] => \\.\PHYSICALDRIVE2
                )

            [\\.\PHYSICALDRIVE0] => Array
                (
                    [0] => \\.\PHYSICALDRIVE0
                )

            [\\.\PHYSICALDRIVE1] => Array
                (
                    [0] => \\.\PHYSICALDRIVE1
                )

            [\\.\PHYSICALDRIVE2] => Array
                (
                    [0] => \\.\PHYSICALDRIVE2
                )

        )

*/
				$disk_groups_data = $this->system_obj->getDiskGroupsData($host_ids);
				
				$retention_device_details =  $this->retention_obj->getValidRetentionVolumes($host_ids,'','');
				// Required only for SourceServers
				if(!$component_type || $component_type == "All" || $component_type == "SourceServers")
				{
					// Retrieving the hardware and network configuration
					$hardware_details =  $this->system_obj->getHardwareConfiguration($host_ids);
					$network_details =  $this->system_obj->getNetworkConfiguration($host_ids);
				}
			}
	        
			$response  = new ParameterGroup ( "Response" );
			$process_servers = new ParameterGroup ("ProcessServers");

			/* If no components are found matching the filters in the input */
			if(!is_array($component_details) || !count($component_details))
			{	
				$response->addParameterGroup($process_servers);
			}
			else
			{
				$src_count = $mt_count = $ps_count = 1;
				$source_servers = new ParameterGroup ("SourceServers");
				$master_targets = new ParameterGroup ("MasterTargets");
                
                a:
                $other_component_details = array();
				#print_r($component_details);
				#exit;
				foreach($component_details as $details)
				{
					$protected_volumes = 0;
					$patch_version_arr = array();
					$cert_expiry = null;
					/* Config Server */
					unset($update_historty_grp);
					unset($account_grp);
	                if($details['isCs'] == 1) 
	                {                       
						if (($details['agentRole'] == 'MasterTarget') and (($component_type == "All") or !$component_type or ($component_type == "MasterTargets")))
						{
							$detailsalias = $details;
							$detailsalias['isPs'] = 0;
							$detailsalias['isCs'] = 0;
							$other_component_details[] = $detailsalias;
						}
						if(($details['isPs']) and (($component_type == "All") or !$component_type or ($component_type == "ProcessServers")))
						{
							$detailsalias = $details;
							$detailsalias['isCs'] = 0;
							$detailsalias['agentRole'] = "";
							$other_component_details[] = $detailsalias;
						}
						if (($component_type == "MasterTargets") or ($component_type == "ProcessServers") or ($component_type == "SourceServers"))
						continue;
						
	                   $component_name = "ConfigServer";
	                    $cx_version = $this->commonfunctions_obj->get_inmage_version_array();						
						$cx_patch_details = $this->commonfunctions_obj->get_cx_patch_history(); 
						$account_details = $this->accounts_obj->getAccountDetails(); 
						if(!empty($cx_patch_details)) 
						{ 
							$update_historty_grp = new ParameterGroup("CSUpdateHistory"); 
							$i=1; 
							
							foreach($cx_patch_details as $update_info) 
							{ 
								$update_install_datetime = explode("_", $update_info[1]); 
								$patch_version_details = explode("_",$update_info[0]);
								$cs_update_grp = new ParameterGroup("Update".$i); 
								$cs_update_grp->setParameterNameValuePair("Version", $patch_version_details[2]); 
								$cs_update_grp->setParameterNameValuePair("InstallationTimeStamp", strtotime($update_install_datetime[0]." ".$update_install_datetime[1])); 
								$update_historty_grp->addParameterGroup($cs_update_grp); 
								$patch_version_arr[$patch_version_details[2]] = str_replace(".","",$patch_version_details[2]);
								$i++; 
							}
						}
						
						if(!empty($account_details)) 
						{ 
							$account_grp = new ParameterGroup("Accounts"); 
							$i=1; 
							foreach($account_details as $key => $account_info) 
							{ 
								$acc_grp = new ParameterGroup("Account".$i); 
								$acc_grp->setParameterNameValuePair("AccountId", $account_info["accountId"]); 
								$acc_grp->setParameterNameValuePair("AccountName", $account_info["accountName"]); 
								$account_grp->addParameterGroup($acc_grp); 
								$i++; 
							} 
						}
						$cs_base_version_details = explode("_",$cx_version[0]);
						$comp_version = $cs_version = $cs_base_version_details[1];
						
						 // Getting the ParameterGroup name
	                    $xml_comp_object = "response";
						// Assigning the ParameterGroup address to the variable
	                    $component = ${$xml_comp_object};
						
						$connectivityType = $this->global_conn->sql_get_value("transbandSettings", "ValueData", "ValueName = ?", array('CONNECTIVITY_TYPE'));
						if(!$connectivityType) $connectivityType = "NON VPN";
						$component->setParameterNameValuePair('ConnectivityType', $connectivityType);
						if ($details['certExpiryDetails'])
							$cert_expiry = unserialize($details['certExpiryDetails']);
						if ($cert_expiry)
						{
							$cert_expiry = $cert_expiry['CsCertExpiry'];
							$component->setParameterNameValuePair('CertExpiry', $cert_expiry);
						}
						else
						{
							$component->setParameterNameValuePair('CertExpiry', "");
						}
	                }
					/* Process Server */
	                elseif($details['isPs']) 
	                {
						if (($details['agentRole'] == 'MasterTarget') and (($component_type == "All") or !$component_type or ($component_type == "MasterTargets")))
						{
							
							$detailsalias = $details;
							$detailsalias['isPs'] = 0;
							$detailsalias['isCs'] = 0;
							$other_component_details[] = $detailsalias;
						}
                        
						
                        if (($component_type == "MasterTargets") or ($component_type == "SourceServers"))
						continue;
						
                        $is_ps_processed = TRUE;
	                    $component_name = "ProcessServer";
						
	                    $lastUpdateTime = $details['lastUpdateTimePs'];
	                    $ps_base_version_details = explode("_",$details['psVersion']);
						$comp_version = $ps_base_version_details[1];
						$ps_patch_version = explode(",", $details['psPatchVersion']); 
						$ps_patch_install_time = explode(",", $details['patchInstallTime']); 
						if($details['psPatchVersion']) 
						{ 
							$update_historty_grp = new ParameterGroup("PSUpdateHistory"); 
							$i=1; 
							
							foreach( $ps_patch_version as $patch_version) 
							{ 
								$ps_update_install_datetime = explode("_", $ps_patch_install_time[$i-1]); 
								$ps_patch_version_details = explode("_", $patch_version); 
								$ps_update_grp = new ParameterGroup("Update".$i); 
								$ps_update_grp->setParameterNameValuePair("Version", $ps_patch_version_details[2]); 
								$ps_update_grp->setParameterNameValuePair("InstallationTimeStamp", strtotime($ps_update_install_datetime[0]." ".$ps_update_install_datetime[1])); 
								$update_historty_grp->addParameterGroup($ps_update_grp);
								$patch_version_arr[$ps_patch_version_details[2]] = str_replace(".","", $ps_patch_version_details[2]);
								$i++;
							}
						} 
						
	                    $xml_comp_object = "process_servers";					
	                    $component = new ParameterGroup ("Server".$ps_count);
	                    if($nat_details[$details['hostId']]['ps_natip']) $component->setParameterNameValuePair('PSNatIP', $nat_details[$details['hostId']]['ps_natip']);
						
						if ($details['certExpiryDetails'])
						{
							$cert_expiry = unserialize($details['certExpiryDetails']);
						}
						if ($nat_details[$details['hostId']]['psCertExpiryDetails'])
						{
							$ps_cert_expiry_details = unserialize($nat_details[$details['hostId']]['psCertExpiryDetails']);
						}
						
						$ps_cert_expiry = "";
						//For New PS agents.
						if (is_array($ps_cert_expiry_details) && array_key_exists('psCertExpiry', $ps_cert_expiry_details))
						{
							$ps_cert_expiry = $ps_cert_expiry_details['psCertExpiry'];
						}
						//For Old PS agents.
						elseif (is_array($cert_expiry) && array_key_exists('PsCertExpiry', $cert_expiry))
						{
							$ps_cert_expiry = $cert_expiry['PsCertExpiry'];
						}
						
						$component->setParameterNameValuePair('CertExpiry', $ps_cert_expiry);
						
						$ps_count++;
	                }
					/* SourceServer / MasterTarget */
	                else 
	                {
						//Common for both
	                    $component_name = $details['agentRole'];
	                    // It actually should be lastUpdateTimeVx, but it is always zeroes
	                    $lastUpdateTimeApp = $details['lastUpdateTimeApp'];
	                    $lastUpdateTimeVx = $details['lastUpdateTimeVx'];
						
						$lastUpdateTime = $lastUpdateTimeVx;
						if($lastUpdateTimeApp < $lastUpdateTimeVx)
						{
							$lastUpdateTime = $lastUpdateTimeApp;
						}
						
						$mt_base_version_details = explode("_",$details['vxVersion']);
						$comp_version = $mt_base_version_details[1];
						$vxPatch = $details['vxPatchVersion']; 
						if(!empty($vxPatch)) 
						{ 
							$update_historty_grp = new ParameterGroup("AgentUpdateHistory"); 
							$patchHistoryVX = explode ("|", $vxPatch); 
							$i=1;
							
							foreach ($patchHistoryVX as $value) 
							{ 
							   $patch_array = explode(",",$value); 
							   $vx_update_grp = new ParameterGroup("Update".$i); 
							   $vx_update_install_datetime = explode("_", $patch_array[1]); 
							   $vx_patch_version_details = explode("_", $patch_array[0]);
							   $vx_update_grp->setParameterNameValuePair("Version", $vx_patch_version_details[2]); 
							   $vx_update_grp->setParameterNameValuePair("InstallationTimeStamp", strtotime($vx_update_install_datetime[0]." ".$vx_update_install_datetime[1])); 
							   $update_historty_grp->addParameterGroup($vx_update_grp);
							   $patch_version_arr[$vx_patch_version_details[2]] = str_replace(".","",$vx_patch_version_details[2]);
							   $i++; 
							}
						} 

	                    $xml_comp_object = ($component_name == "MasterTarget") ? "master_targets" : "source_servers";
	                    $devices = $device_details[$details['hostId']];
						
						/* Only MasterTarget */
	                    if($component_name == "MasterTarget")
	                    {
							$ret_details = array();
							$component = new ParameterGroup ("Server".$mt_count);
							
							$validationErrors_grp = new ParameterGroup("ValidationErrors");
							//print_r($validationErrors_grp);
							if ($details['validationErrors'] != "")
							{
								$errorsArray = explode(',', $details['validationErrors']);
								
								foreach ($errorsArray as $errorCode)
								{
									$error_response = ErrorCodeGenerator::frameErrorResponse($errorCode, $errorCode, "", null, 0, 1);
									$validationErrors_grp->addParameterGroup ($error_response);
								}
							}
							$component->addParameterGroup($validationErrors_grp);
							
							$protected_volumes = $this->conn->sql_get_value("srcLogicalVolumeDestLogicalVolume", "count(*)", "destinationHostId = ?", array($details['hostId']));
							if(is_array($devices))	
							{
								$retention_details = $this->system_obj->getRetentionDetails($host_ids);
								$devices_grp = new ParameterGroup("RetentionVolumes");
								
								if(
									is_array($retention_device_details) && 
									count($retention_device_details) && 
									array_key_exists($details['hostId'], $retention_device_details)
								) 
								$ret_details = array_keys($retention_device_details[$details['hostId']]);
								
								$vol_count = 1;
								// Retention Volumes
								foreach ($devices as $device)
								{
									if(!in_array($device['mountPoint'],$ret_details)) continue;
									$device_grp = new ParameterGroup("Volume".$vol_count);                        
									$device_grp->setParameterNameValuePair("VolumeName", $device['mountPoint']);
									$device_grp->setParameterNameValuePair("Capacity", $device['capacity']);
									$device_grp->setParameterNameValuePair("Freespace", $device['freespace']);									
									if($retention_details[$details['hostId']."!!".$device['mountPoint']]) 
									$device_grp->setParameterNameValuePair("RetentionSpaceThreshold", $retention_details[$details['hostId']."!!".$device['mountPoint']]); 
									$devices_grp->addParameterGroup($device_grp);
									$vol_count++;									
								}	
							}
							
							// disk count to be added for MT
                            $component->setParameterNameValuePair("DiskCount", $details['diskCount']);
							
							// Mars agent version only in case of MT
                            $component->setParameterNameValuePair("MarsAgentVersion", $details['marsVersion']);
                            
							$mt_count++;
	                    }
						/* Only SourceServer */
	                    else
	                    {
							if(!$details['infrastructureVMId']) 
							{
								$host_id = isset($details['hostId'])?$details['hostId']:"";
								$host_name = isset($details['hostName'])?$details['hostName']:"";
								$ip_address = isset($details['ipAddress'])?$details['ipAddress']:"";
								$ostype = ($details['osType'] == 1) ? "Windows" : "Linux";
								$vx_prod_version = isset($details['vxVersion'])?$details['vxVersion']:"";
								$mds_log .= "LSC:Hostid:".$host_id.",Name:".$host_name.",IP:".$ip_address.",OS:".$ostype.",version:".$vx_prod_version;							
								continue;
							}
							
							$component = new ParameterGroup ("Server".$src_count);
							$is_protected = False;
							$component_name = "SourceServer";
	                        $disk_data = $disk_details[$details['hostId']];
							// Running light weight query to alive connection object after every 30 records.
							if(!($src_count%$CONN_ALIVE_RETRY))
							{
								$sql = "SELECT now()";
								$rs = $this->conn->sql_query($sql);
							}
							
							// Disk details
							if(is_array($disk_data))
							{
								$devices_grp = new ParameterGroup("DiskDetails");
								$system_disk_set = 0;
								for($d=0; $d < count($disk_data); $d++)
								{
									$is_system_disk = "No";
									$is_dynamic = "No";
									$disk_properties = array();
									if (isset($disk_data[$d]['deviceProperties']))
									{
										$disk_properties = unserialize($disk_data[$d]['deviceProperties']);
									}
									
									// in case of usb device serial number is reporting windows control characters
									// so not sending the usb devices as part of this API.
									if(strpos(strtolower($disk_properties['interface_type']), "usb") !== false) continue;
									
									// System Disk , if bootable partition is set and the disk has a system volume.
									if ($details['osType'] == 1)
									{
										//$dg_data[$value['hostId']][$value['deviceName']][] = $value['diskGroupName']; Making join between disk group and hosts table based on host id.
										//Disk group data: for each host id, device name assign to disk group name.
										if (array_key_exists($details['hostId'], $disk_groups_data))
										{
											//If disk name exists in disk group data, then allow.
											if (array_key_exists($disk_data[$d]['deviceName'], $disk_groups_data[$details['hostId']]))
											{
												//Get disk disk group name.
												$disk_group_name_for_disk = $disk_groups_data[$details['hostId']][$disk_data[$d]['deviceName']][0];
												//Joining with disk groups and logical volumes based on host id nad device name for both disks/lvs records and collects disk group name details if system volume is true. Check that disk, disk group name exists in disk_group_struct.
												if(strtolower($disk_properties['has_bootable_partition']) == "true" && in_array($disk_group_name_for_disk, $disk_group_struct[$details['hostId']])) 
												$is_system_disk = "Yes";
											}
										}
		                            }
									else
									{
										if ($details['compatibilityNo'] >= $ROOT_ON_MULTIPLE_DISKS_SUPPORT_VERSION)
										{
											if ($disk_data[$d]['systemVolume'] == "Yes")
											{
												$is_system_disk = "Yes";
											}
										}
										else
										if(strtolower($disk_properties['has_bootable_partition']) == "true" && $disk_data[$d]['systemVolume'] == "Yes" && !$system_disk_set) 
										{
											$is_system_disk = "Yes";
											$system_disk_set = 1;
										}	
									}
									// Dynamic Disk
		                            if(strtolower($disk_properties['storage_type']) == "dynamic") $is_dynamic = "Yes";
									// Disk Guid
		                            $disk_guid = ($disk_properties['disk_guid']) ? $disk_properties['disk_guid'] : str_replace("/", "_", $disk_data[$d]['deviceName']);
		                            
		                            $device_grp = new ParameterGroup("Disk".($d + 1));
/*
The below is the way it reports data from S2 comparing with V1 & V2 for caspian. V2 stop reporting the raw disks. V1 even though reports raw disks, protection can not be allowed and validation exists.
                                        V1	                        V2
----------------------------------------------------------------------------------
FieldNames/Map	            Windows	         Linux	       Windows	     Linux
----------------------------------------------------------------------------------
deviceName	            //./physicaldrive0	/dev/sda	 343434323423	   /dev/sda
deviceProperties(disk_guid)	343434323423	Not exists	  Not exists	 Not exists
deviceProperties(display_name)	Not exists	Not exists	//./physicaldrive0	/dev/sda

DeviceName/DiskName Logic:

If display_name key/value exists in the map, then devicename=display name
   Else, devicename = DeviceName column value in logicalvolumes table
*/
                                if ($disk_properties['display_name'])
                                {
                                    $device_grp->setParameterNameValuePair("DiskName", $disk_properties['display_name']);
                                }
                                else
                                {
                                    $device_grp->setParameterNameValuePair("DiskName", $disk_data[$d]['deviceName']);
                                }
									if ($disk_properties['is_resource_disk'] == "true")
									{
										$is_resource_disk = $disk_properties['is_resource_disk'];
									}
									else
									{
										$is_resource_disk = "false";
									}
									$device_grp->setParameterNameValuePair("IsResourceDisk", $is_resource_disk);
		                            $device_grp->setParameterNameValuePair("Capacity", $disk_data[$d]['capacity']);
		                            $device_grp->setParameterNameValuePair("SystemDisk", $is_system_disk);
									$device_grp->setParameterNameValuePair("Bootable", $disk_properties['has_bootable_partition']);
		                            /*
Device id/diskId logic:

If disk_guid exists in map, then device id= disk_guid from map
  Else device id = devicename column value in logicalvolumes table.
*/  
                                    if ($disk_properties['disk_guid'])
                                    {
                                        $device_grp->setParameterNameValuePair("DiskId", $disk_properties['disk_guid']);
                                    }
                                    else
                                    {
                                        $device_grp->setParameterNameValuePair("DiskId", $disk_data[$d]['deviceName']);
                                    }
		                            $device_grp->setParameterNameValuePair("DynamicDisk", $is_dynamic);
									if ($disk_properties['interface_type'])
                                    {
                                        $device_grp->setParameterNameValuePair("InterfaceType", $disk_properties['interface_type']);
                                    }
                                    else
                                    {
                                    	$device_grp->setParameterNameValuePair("InterfaceType", "Unknown");	
                                    }
									
									if ($disk_properties['serial_number'])
									{
										$device_grp->setParameterNameValuePair("SerialNumber", $disk_properties['serial_number']);
									}
									else
									{
										$device_grp->setParameterNameValuePair("SerialNumber", "");	
									}
									
									if ($disk_properties['has_uefi'])
									{
										$device_grp->setParameterNameValuePair("HasUefi", $disk_properties['has_uefi']);
									}
									else
									{
										$device_grp->setParameterNameValuePair("HasUefi", "false");	
									}
									
                                    $device_grp->setParameterNameValuePair("BusAddress", $disk_properties['scsi_bus'].":".$disk_properties['scsi_port'].":".$disk_properties['scsi_target_id'].":".$disk_properties['scsi_logical_unit']);
									$device_grp->setParameterNameValuePair("FileSystemType", $disk_data[$d]['fileSystemType']);
									$device_grp->setParameterNameValuePair("MountPoint", $disk_data[$d]['mountPoint']);
									
									$disk_related_details = $this->system_obj->getAllRelatedStorageDevices($details['hostId'],$disk_data[$d]['deviceName'],$partitions_data,$disk_groups_data);
							
									if($disk_data[$d]['farLineProtected']  == 1)
									$is_protected = True;
		                           
									foreach($disk_related_details as $k1=>$v1)
									{			
										if($k1 != "partition")
										{
											$device_grp->setParameterNameValuePair($k1,$v1);
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
														elseif(is_array($key_value))
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
													$device_grp->addParameterGroup($partitions);
													$j++;
												}
											}
										}
									}
									$devices_grp->addParameterGroup($device_grp);
								}
							}
	                        // Hardware Infomation
	                        $cpu_info = $hardware_details[$details['hostId']];
	                        $hw_grp = new ParameterGroup("HardwareConfiguration");
	                        $hw_grp->setParameterNameValuePair("CPUCount", $cpu_info);
	                        $hw_grp->setParameterNameValuePair("Memory", $details['memory']);
	                        // Network Configuration
	                        $nics = $network_details[$details['hostId']];
							if(is_array($nics))
							{
								$nw_grp = new ParameterGroup("NetworkConfiguration");
								$nic_count = 1;
								foreach($nics as $nic)
								{
                                    // Skip the Virtual NIC(s) or Adapter(s) in case of Windows only
                                    if($details['osType'] == 1 && $nic['ipType'] != 1) continue;
                                    
									$nw_device_grp = new ParameterGroup("Nic".$nic_count);
									$nw_device_grp->setParameterNameValuePair("InterfaceName", $nic['deviceName']);
									$nw_device_grp->setParameterNameValuePair("MACAddress", trim($nic['macAddress']));
									$nw_device_grp->setParameterNameValuePair("IPAddress", $nic['ipAddress']);
									$nw_device_grp->setParameterNameValuePair("IPV6Address", $nic['IPV6Address']);
									$nw_device_grp->setParameterNameValuePair("SubNetmask", $nic['subnetMask']);
									$nw_device_grp->setParameterNameValuePair("Gateway", $nic['gateway']);
									$nw_device_grp->setParameterNameValuePair("DNS", $nic['dnsServerAddresses']);
									$nw_device_grp->setParameterNameValuePair("isDHCPEnabled", ($nic['isDhcpEnabled']) ? "Yes" : "No");
									$nw_grp->addParameterGroup($nw_device_grp);
									$nic_count++;
								}	
							}
							$component->setParameterNameValuePair('FirmwareType', $details['FirmwareType']);
							$component->setParameterNameValuePair('DriverVersion', $details['involflt_version']);
							$component->setParameterNameValuePair('AgentBiosId', $details['AgentBiosId']);
							$component->setParameterNameValuePair('AgentFqdn', $details['AgentFqdn']);
                            $component->setParameterNameValuePair('InfrastructureVMId', $details['infrastructureVMId']);
							$is_protected = ($is_protected) ? "Yes" : "No";
							$component->setParameterNameValuePair("ResourceId", $details['resourceId']);
							$src_count++;
	                    }  
	                }
					$component->setParameterNameValuePair('ComponentType', $component_name);
					// Currently for PS its not reporting the OS	
					$os_type = ($details['osType'] == 1) ? "Windows" : "Linux";										
					$component->setParameterNameValuePair('HostId', $details['hostId']);
					$component->setParameterNameValuePair('IPAddress', $details['ipAddress']);
					$component->setParameterNameValuePair('HostName', $details['hostName']);
					$component->setParameterNameValuePair('OSType', $os_type);			
					$component->setParameterNameValuePair('HostUuid', $details['hostUuid']);
					$component->setParameterNameValuePair('InstanceUuid', $details['instanceUuid']);
					$component->setParameterNameValuePair('InfrastructureHostId', $details['infrastructureHostId']);
					$component->setParameterNameValuePair('VCenterInfrastructureId', $inventory_id_details[$details['infrastructureHostId']]);

					
					 // Only for source and master targets
					if($details['isCs'] != 1 && $details['isPs'] != 1) 
					{			
						// Os details and Server Type 
						$server_type = ($details['hypervisorState'] == 1) ? "Physical" : "Virtual"; 
						$component->setParameterNameValuePair('OSVersion', $details['osName']); 
						$component->setParameterNameValuePair('ServerType', $server_type);						
						$component->setParameterNameValuePair('Vendor', $details['hypervisorName']);
						$system_drive = ($details['systemDrive'] == NULL)?"":$details['systemDrive'];
						$component->setParameterNameValuePair('OSVolume', $system_drive);
						$system_directory = ($details['systemDirectory'] == NULL)?"":$details['systemDirectory'];
						$component->setParameterNameValuePair('OSInstallPath', $system_directory);
						$system_drive_disk_extents = ($details['systemDriveDiskExtents'] == NULL)?"":$details['systemDriveDiskExtents'];
						$component->setParameterNameValuePair('OSVolumeDiskExtents', $system_drive_disk_extents);
						if($component_name == "MasterTarget") $component->setParameterNameValuePair('ProtectedVolumes', $protected_volumes);
                        else $component->setParameterNameValuePair("isProtected", $is_protected);

						// Devices group 
						if($devices_grp) $component->addParameterGroup($devices_grp);
						unset($devices_grp);
						
						// Only for source server
						if($component_name != "MasterTarget") 
						{						
							if($hw_grp) $component->addParameterGroup($hw_grp);
							if($nw_grp) $component->addParameterGroup($nw_grp);
							// Unsetting so that this will not be assigned to the next component
							unset($hw_grp);
							unset($nw_grp);
						}
					}
					// Version and update history
					$patch_version_arr[$comp_version] =  str_replace(".","",$comp_version);
					$latest_version = max($patch_version_arr);
					$component->setParameterNameValuePair('Version', $comp_version);
					if($account_grp) $component->addParameterGroup($account_grp);
					if($update_historty_grp) $component->addParameterGroup($update_historty_grp);
					$component->setParameterNameValuePair('LatestVersion', array_search($latest_version, $patch_version_arr));
					
					if($details['isCs'] != 1) 
					{ 
					   $component->setParameterNameValuePair('Heartbeat', $lastUpdateTime); 
					   // Assigning the ParameterGroup address to the variable					
					   // For CS it has been already part of the reponse (Line 730)
					   ${$xml_comp_object}->addParameterGroup ($component); 
					}
					
					// Adding Installers PG to PS group only in case CS & PS versions are same.
					if(($details['isPs'] == 1) && ($comp_version >= $cs_version))
					{
						if(count($patch_details))
						{
							$patch_upgarde_grp = new ParameterGroup("Installers");
							foreach($patch_details as $os_type => $value)
							{
								if($value["osType"] != 1) $os_type = "Linux";
								$tmp_upgrade_grp = new ParameterGroup($os_type);
								$tmp_upgrade_grp->setParameterNameValuePair("Version", $value["patchVersion"]);
								$tmp_upgrade_grp->setParameterNameValuePair("RebootRequired", ($value["rebootRequired"]) ? "Yes" : "No");
								$tmp_upgrade_grp->setParameterNameValuePair("UpdateType", $value["updateType"]);
								
								if($tmp_upgrade_grp) $patch_upgarde_grp->addParameterGroup($tmp_upgrade_grp);	
							}
							unset($tmp_upgrade_grp);
						}
						if($patch_upgarde_grp) $component->addParameterGroup($patch_upgarde_grp);
					}
				}
                
				
                if (count($other_component_details) > 0)
                {  
                    $component_details = $other_component_details;
                    goto a;
                }
                 
				$response->addParameterGroup ($process_servers);
				$response->addParameterGroup ($source_servers);
				$response->addParameterGroup ($master_targets);
			}
			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
			if ($mds_log != "")
			{
				$mds_data_array = array();
				$log_string = "";
				preg_match('/\<AccessKeyID\>(.*?)\<\/AccessKeyID\>/', $requestXML, $match);
				if (count($match) > 0)
				{
					$actual_string = $match[1];
					$full_string = substr_replace($actual_string, "#####",-5, 5);
					$log_string = preg_replace("/$actual_string/sm", $full_string, $requestXML);
				}
				preg_match('/\<AccessSignature\>(.*?)\<\/AccessSignature\>/', $requestXML, $match);
				if (count($match) > 0)
				{
					$actual_string = $match[1];
					$log_string = preg_replace("/$actual_string/sm", "xxxxx", $log_string);
				}
				$mds_data_array["activityId"] = $MDS_ACTIVITY_ID;
				$mds_data_array["jobType"] = "APIErrors";
				$mds_data_array["errorString"] = "Request XML:$log_string".$mds_log;
				$mds_data_array["eventName"] = "CS";
				$mds_obj = new MDSErrorLogging();
				$mds_obj->saveMDSLogDetails($mds_data_array);
			}
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
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
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function SetVPNDetails_validate($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();            
            $connectivityType = strtoupper($this->conn->sql_get_value("transbandSettings","ValueData","ValueName = ?", array('CONNECTIVITY_TYPE')));
			
            $server_host_ids = array();
			
            if(isset($parameters['Servers']))
			{
				$servers_obj = $parameters['Servers']->getChildList();				
				foreach($servers_obj as $server_key => $hosts)
				{					
					$host_obj = $hosts->getChildList();                        
					if(!isset($host_obj['HostId'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $server_key." - HostId"));
					$hostId = trim($host_obj['HostId']->getValue());
					if(!trim($hostId)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $server_key." - HostId"));
					
					$ipAddress = $this->conn->sql_get_value("hosts", "ipAddress", "id = ?", array($hostId));	
					if(!$ipAddress) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_HOST_FOUND_SETVPN');
				   
					if(in_array($hostId, $server_host_ids)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HostId. Duplicate HostId for ".$server_key));
					$server_host_ids[] = $hostId;
					
					if($connectivityType == "VPN")
					{
						if(!isset($host_obj['PrivateIP'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $server_key." - PrivateIP"));
						if(!isset($host_obj['PrivateSSLPort'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $server_key." - PrivateSSLPort"));
						
						$privateIP = trim($host_obj['PrivateIP']->getValue());
						if(!$privateIP) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $server_key." - PrivateIP"));
						if(!filter_var($privateIP, FILTER_VALIDATE_IP)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $server_key." - PrivateIP"));
						
						$privateSslPort = trim($host_obj['PrivateSSLPort']->getValue());
						if(!$privateSslPort) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $server_key." - PrivateSSLPort"));
						elseif(!is_numeric($privateSslPort)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $server_key." - PrivateSSLPort"));
						
						if(isset($host_obj['PrivatePort']))
						{
							$privatePort = trim($host_obj['PrivatePort']->getValue());
							if(!$privatePort) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $server_key." - PrivatePort"));
							elseif(!is_numeric($privatePort)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $server_key." - PrivatePort"));
						}
					}
					
					if($connectivityType == "NON VPN")
					{
						if(!isset($host_obj['PublicIP'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $server_key." - PublicIP"));
						if(!isset($host_obj['PublicSSLPort'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $server_key." - PublicSSLPort"));
						
						$publicIP = trim($host_obj['PublicIP']->getValue());
						if(!$publicIP) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $server_key." - PublicIP"));                       
						if(!filter_var($publicIP, FILTER_VALIDATE_IP)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $server_key." - PublicIP"));
						
						$publicSslPort = trim($host_obj['PublicSSLPort']->getValue());
						if(!$publicSslPort) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $server_key." - PublicSSLPort"));
						elseif(!is_numeric($publicSslPort)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $server_key." - PublicSSLPort"));
						
						if(isset($host_obj['PublicPort']))
						{ 
							$publicPort = trim($host_obj['PublicPort']->getValue());
							if(!$publicPort) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $server_key." - PublicPort"));
							elseif(!is_numeric($publicPort)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $server_key." - PublicPort"));
						}
					}	
                }
            }           
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
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
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function SetVPNDetails($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			$response = new ParameterGroup("Response");
            
			$connectivityType = strtoupper($this->conn->sql_get_value("transbandSettings","ValueData","ValueName = ?", array('CONNECTIVITY_TYPE')));
			
			if(isset($parameters['Servers']))
			{
				$servers_obj = $parameters['Servers']->getChildList();				
				foreach($servers_obj as $hosts)
				{
					$host_obj = $hosts->getChildList();					
					$hostId = trim($host_obj['HostId']->getValue());    
					$privateIP = (isset($host_obj['PrivateIP'])) ? trim($host_obj['PrivateIP']->getValue()) : "";
					$privatePort = (isset($host_obj['PrivatePort'])) ? trim($host_obj['PrivatePort']->getValue()) : "9080";
					$privateSslPort = (isset($host_obj['PrivateSSLPort'])) ? trim($host_obj['PrivateSSLPort']->getValue()) : "9443";						
					$publicIP = (isset($host_obj['PublicIP'])) ? trim($host_obj['PublicIP']->getValue()) : "";
					$publicPort = (isset($host_obj['PublicPort'])) ? trim($host_obj['PublicPort']->getValue()) : "9080";
					$publicSslPort = (isset($host_obj['PublicSSLPort'])) ? trim($host_obj['PublicSSLPort']->getValue()) : "9443";
					$this->processserver_obj->setVPNDetails($connectivityType, $hostId, $privateIP, $privatePort, $privateSslPort, $publicIP, $publicPort, $publicSslPort);					
				}
			}
			
			$response->setParameterNameValuePair("Status", "Success");

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
	
	public function RenewCertificate_validate($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$mandatory_parameters = array("HostId", "CertType");
			
			foreach($parameters as $param => $param_obj)
			{
				if(in_array($param, $mandatory_parameters))
				{
					if(!isset($parameters[$param]))  ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $param));
				}
				if(isset($parameters[$param]))
				{
					if($param == "HostId")
					{
						$csHostId = trim($parameters[$param]->getValue());
						if(!$csHostId) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $param));
						
						$host_sql = "SELECT
										ipaddress,
										name,
										processServerEnabled
									FROM
										hosts
									WHERE
										id  = ?";
						$host_data = $this->conn->sql_query($host_sql, array($csHostId)); 
						
						if(!$host_data[0]["ipaddress"]) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_HOST_FOUND', array('ErrorCode' => ErrorCodes::EC_NO_HOST_FOUND, 'HostIdHolder' => $csHostId));
						
						// if inbuilt process server is not found fail the renew certificate.
						if($host_data[0]["processServerEnabled"] == 0) 
						{
							ErrorCodeGenerator::raiseError($functionName, 'EC_NO_ASSOCIATED_PS', array('HostName' => $host_data[0]["name"]));
						}
					}
					
					if($param == "CertType")
					{
						$certType = trim($parameters[$param]->getValue());
						if(!$certType) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $param));
						
						if($certType != "SSL") ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $param));
					}
				}
			}
			
			$sql_args = array();
			$host_sql = "SELECT
								distinct
								name,
								ipaddress,
								prod_version,
								processServerEnabled,
								UNIX_TIMESTAMP(now()) as currentTime,
								UNIX_TIMESTAMP(lastHostUpdateTimePs) as lastHostUpdateTimePs,
								lastHostUpdateTime,
								psVersion
							FROM
								hosts h,
								srcLogicalVolumeDestLogicalVolume s
							WHERE
								(h.id = s.sourceHostId  
							OR
								h.id = s.destinationHostId
							OR
								h.id = s.processServerId)
							AND
								(pauseActivity = '' 
							OR 
								pauseActivity IS NULL)";
			$host_data = $this->conn->sql_query($host_sql, $sql_args);
			$prod_version = "";
			$server_version = $servers_no_heartbeat = array();
			
			foreach($host_data as $host_key => $host_info)
			{
				$prod_version = $host_info["prod_version"];
				$agent_version = str_replace(".", "", $host_info["prod_version"]);
				
				if($agent_version == "" && $host_info["psVersion"])
				{
					$prod_version = $host_info["psVersion"];
					$psDetails = split("_", $host_info["psVersion"]);
					$agent_version = str_replace(".", "", $psDetails[1]);
				}
				 
				if($agent_version < 9400)
				{
					$server_version[] = $host_info["name"]."(".$host_info["ipaddress"].")";
				}
				else if($host_info["processServerEnabled"] && 
						($host_info["currentTime"] - $host_info["lastHostUpdateTimePs"] > 900))
				{
					$servers_no_heartbeat[] = $host_info["name"]."(".$host_info["ipaddress"].")";
				}
			}			
			
			// In case PS/MT/Mobility service version is less than 9400 then throw exception.
			if(count($server_version) > 0)
			{
				$agent_details = join(",", $server_version);
				ErrorCodeGenerator::raiseError($functionName, 'EC_AGENT_VERSION_MISMATCH', array('HostName' => $agent_details, 'ServerVersion' => "9.6.0.0"));
			}
			// In case if ps exists and heart beat is less than 15 mins throw exception.
			// or else agent heart beat is less than 2 hours throw exception.
			else if(count($servers_no_heartbeat) > 0)
			{
				$server_details = join(",", $servers_no_heartbeat);
				ErrorCodeGenerator::raiseError($functionName, 'EC_NO_SERVER_HEARTBEAT', array('HostName' => $server_details));
			}
			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
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
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function RenewCertificate($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			global $requestXML, $activityId, $clientRequestId;
			
			$response = new ParameterGroup("Response");
			$policy_ids = null;
			$requestedData = array();
			
			$csHostId = trim($parameters["HostId"]->getValue());
			$certType = trim($parameters["CertType"]->getValue());

			if($certType == "SSL")
			{
				// insert renew certificate policies.
				$policy_id_arr = $this->system_obj->renewCertificate($csHostId);
			}
			$requestedData["policyId"] = $policy_id_arr;
			$requestedData["hostId"] = $csHostId;
			
			foreach($policy_id_arr as $key => $policy_id)
			{
				$policy_ids = $policy_ids . $policy_id . ',';		
			}
			$policy_ids =  ','.$policy_ids ;	
				
			$api_ref = $this->commonfunctions_obj->insertRequestXml(
								$requestXML,
								$functionName,
								$activityId,
								$clientRequestId,
								$policy_ids);
			$request_id = $api_ref['apiRequestId'];
			$apiRequestDetailId = $this->commonfunctions_obj->insertRequestData($requestedData,$request_id);
			
			$response  = new ParameterGroup ( "Response" );
			if(!is_array($api_ref) || !$request_id || !$apiRequestDetailId)
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Failed due to internal error");
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
	
	public function MigrateACSToAAD_validate($identity, $version, $functionName, $parameters)
	{
        try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			global $requestXML, $activityId, $clientRequestId, $HOST_GUID;
			
			$job_ids = array();
			$response = new ParameterGroup("Response");
			
			// get all windows master targets only.
			$master_targets = $this->system_obj->ListHosts("agentRole = 'MasterTarget' AND (outpostAgentEnabled = '1' OR sentinelEnabled = '1') 
															AND osFlag = '1'", 1);
			if(count($master_targets) <= 0)
            {
                ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RECORD_INFO');
            }
			// Disable Exceptions for DB
			$this->global_conn->disable_exceptions();
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
	
	public function MigrateACSToAAD($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->global_conn->enable_exceptions();
			$this->global_conn->sql_begin();
			
			global $requestXML, $activityId, $clientRequestId, $HOST_GUID;
			
			$job_ids = array();
			$response = new ParameterGroup("Response");
			
			// get all windows master targets only.
			$master_targets = $this->system_obj->ListHosts("agentRole = 'MasterTarget' AND (outpostAgentEnabled = '1' OR sentinelEnabled = '1') 
															AND osFlag = '1'", 1);
			
			// check if job already exists
			$flag_job_exists = 0;
            if(count($master_targets))
            {
                foreach($master_targets as $master_target)
                {
                    $job_id = $this->system_obj->agentHasJobs($master_target['HostAgentGUID'], "MTPrepareForAadMigrationJob");
                    if($job_id) 
                    {
                        $flag_job_exists = $job_id;
                        break;
                    }
                }
            }
            
			if($flag_job_exists)
			{
				$api_ref = $this->system_obj->getMigrationJobDetails($flag_job_exists);
				$request_id = $api_ref['apiRequestId'];
                $apiRequestDetailId = $api_ref['apiRequestDetailId'];
			}
			else
			{			
				foreach($master_targets as $master_target)
				{
					$job_id = $this->system_obj->insertJobForAgent($master_target['HostAgentGUID'], "MTPrepareForAadMigrationJob");
					$requestedData[] = array("hostId" => $master_target['HostAgentGUID'], "jobId" => $job_id, "friendlyName" => $master_target['HostName']);
					
					$job_ids[] = $job_id;
				}
					
				$api_ref = $this->commonfunctions_obj->insertRequestXml(
									$requestXML,
									$functionName,
									$activityId,
									$clientRequestId,
									",".implode(",", $job_ids).",");
				$request_id = $api_ref['apiRequestId'];
				$apiRequestDetailId = $this->commonfunctions_obj->insertRequestData($requestedData,$request_id);
			}
            
			$response  = new ParameterGroup ( "Response" );
			if(!is_array($api_ref) || !$request_id || !$apiRequestDetailId)
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Failed due to internal error");
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