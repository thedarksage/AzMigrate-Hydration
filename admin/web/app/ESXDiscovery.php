<?php
/*
 *$Header: /src/admin/web/app/ESXDiscovery.php,v 1.1 2013/29/10 10:45:27 
 *======================================================================== 
 * FILENAME
 *  ESXDiscovery.php
 *
 * DESCRIPTION
 *  This script contains ESX discovery class which is useful to discover ESX.
 *
 *=======================================================================
 *                 Copyright (c) InMage Systems                    
 *=======================================================================
*/
class ESXDiscovery
{
	private $commonfunctions_obj;
    private $conn;
	private $logging_obj;
	private $account_obj;
	/*
	* Function Name: ESXDiscovery
	*
	* Description:
	*    This is the constructor of the class.Invokes the set error handler 
	*    method to the call.
	*
	*/
	function __construct()
    {
		global $conn_obj, $logging_obj;
        $this->commonfunctions_obj = new CommonFunctions();
        $this->conn = $conn_obj;		
		$this->logging_obj = $logging_obj;
		$this->account_obj = new Accounts();
    }

	function getDiscoveryJobs()
	{
		$encryption_key = $this->commonfunctions_obj->getCSEncryptionKey();
		if($encryption_key)
		{
			$discover_sql = "SELECT 
								inventoryId,
								hostType,
								hostIp,
                                hostPort,
								login,
								IF(ISNULL(passwd) OR passwd = '', passwd, AES_DECRYPT(UNHEX(passwd), ?)) as passwd,								
                                accountId,
								organization
							FROM 
								infrastructureInventory 
							WHERE
								hostType IN ('vSphere','vCenter','vCloud','HyperV')
								AND
								(
									(autoDiscovery = '1' AND 
										(
											((unix_timestamp(now()) - unix_timestamp(loginVerifyTime)) > discoveryInterval) OR 
											(unix_timestamp(now()) < unix_timestamp(loginVerifyTime))
										)
									) OR 
									discoverNow = '1'
								)";

			$inventory_items = $this->conn->sql_query ($discover_sql, array($encryption_key));
            $inventory_details = array();
            $account_details = array();
            foreach($inventory_items as $inventory)
            {
                $inv_acc_id = $inventory['accountId'];
                if($inv_acc_id) 
                {
                    if(!$account_details[$inv_acc_id])
                    {
                        $account_data = $this->account_obj->getAccountDetails($inv_acc_id);
                        $account_name = ($account_data[0]['domain']) ? $account_data[0]['domain']."\\" : "";
                        $account_details[$inv_acc_id]['userName'] = $account_name.$account_data[0]['userName'];
                        $account_details[$inv_acc_id]['password'] = $account_data[0]['password'];
                    }                    
                    $inventory['login'] = $account_details[$inv_acc_id]['userName'];
                    $inventory['passwd'] = $account_details[$inv_acc_id]['password'];
                }
                $inventory_details[] = $inventory;
            }
			return $inventory_details;
		}
		else
        {
           $this->logging_obj->my_error_handler("EXCEPTION","Encryption key not found for get Discovery Jobs");
			return false;  
        }

	}
    
    function saveInventoryDetails($inventoryParams)
	{
        $encryption_key = $this->commonfunctions_obj->getCSEncryptionKey();
        $siteName = isset($inventoryParams['siteName']) ? $inventoryParams['siteName'] : '';
        $hostIp = isset($inventoryParams['hostIp']) ? $inventoryParams['hostIp'] : '';
        $hostPort = isset($inventoryParams['hostPort']) ? $inventoryParams['hostPort'] : '';
        $infrastructureType = $inventoryParams['infrastructureType'];
        $hostType = $inventoryParams['hostType'];
        $accountId = isset($inventoryParams['accountId']) ? $inventoryParams['accountId'] : '';
        $loginId = isset($inventoryParams['loginId']) ? $inventoryParams['loginId'] : '';
        $password = isset($inventoryParams['password']) ? $inventoryParams['password'] : '';
        $organization = isset($inventoryParams['organization']) ? $inventoryParams['organization'] : '';		
        // This following is used only in case of Azure or AWS discovery from CS.
        // We are not doing this now.
        // So making this empty.
        $cert_file = '';
        $esxAgentId = isset($inventoryParams['esxAgentId']) ? $inventoryParams['esxAgentId'] : ''; 
		$request_has_host_id = $inventoryParams['request_has_host_id'];		
		$password_sql = "IF(? = '', ?,  HEX(AES_ENCRYPT(?, ?)))";
		$requested_inventory_id = ($inventoryParams['inventoryId']) ? $inventoryParams['inventoryId'] : '';
		
		if ($hostType == 'Physical')
		{
			$inventoryId = 0;
			$inv_sql =		"select 
				ini.inventoryId 
			from 
				infrastructureInventory ini,
				infrastructureHosts inh,
				infrastructureVMs inv 
			where
				ini.infrastructureType = ?
			AND
				ini.hostIp = ?
			AND
				ini.hostType = ?
			AND
				ini.inventoryId = inh.inventoryId
			AND
				inh.infrastructureHostId = inv.infrastructureHostId "; 
			if ($request_has_host_id != '')
			{	
				$inv_sql = $inv_sql." AND inv.hostId = ?";
				$inventory = $this->conn->sql_query($inv_sql, array($inventoryParams['infrastructureType'],$inventoryParams['hostIp'],$inventoryParams['hostType'], $request_has_host_id));
			}
			else
			{
				$inventory = $this->conn->sql_query($inv_sql, array($inventoryParams['infrastructureType'],$inventoryParams['hostIp'],$inventoryParams['hostType']));
			}
			$this->logging_obj->my_error_handler("INFO",array("Sql"=>$inv_sql, "Args"=>array($inventoryParams['infrastructureType'],$inventoryParams['hostIp'],$inventoryParams['hostType'],$request_has_host_id)), Log::BOTH);
			foreach($inventory as $inventory_data)
			{
				$inventoryId = $inventory_data['inventoryId'];
			}
		}
		else
		{
			$this->logging_obj->my_error_handler("INFO",array("Sql"=>"infrastructureType = ? AND hostType = ? AND  hostIp = ?", "Args"=>array($infrastructureType, $hostType, $hostIp)), Log::BOTH);
			$inventoryId = $this->conn->sql_get_value("infrastructureInventory", "inventoryId", "infrastructureType = ? AND hostType = ? AND  hostIp = ?", array($infrastructureType, $hostType, $hostIp));
		}
		
		if (!$inventoryId && !$requested_inventory_id)
        {
            $sql_args = array($siteName, $hostIp, $hostPort, $infrastructureType, $hostType, $accountId, $loginId, $password, $password, $password, $encryption_key, $organization, $cert_file, $esxAgentId);
			$sql = "INSERT INTO  
                        infrastructureInventory
                        (
                            siteName,
                            hostIp,
                            hostPort,
                            infrastructureType,
                            hostType,
                            accountId,
                            login,
                            passwd,
                            organization, 
                            certificate, 
                            esxAgentId
                        )
                        VALUES
                        (
                            ?,
                            ?, 
                            ?,
                            ?,
                            ?, 
                            ?, 
                            ?, 
                            {$password_sql}, 
                            ?,
                            ?,
                            ?
                        )";
			$inventory_id = $this->conn->sql_query($sql, $sql_args);
		}
		else
		{
            $inventory_id = ($requested_inventory_id) ? $requested_inventory_id : $inventoryId;
			$cond = "";
			$sql_args = array();
			if($hostIp)
			{
				$cond .= ", hostIp = ?";
				array_push($sql_args, $hostIp);
			}
            if($hostPort)
			{
				$cond .= ", hostPort = ?";
				array_push($sql_args, $hostPort);
			}
			if($accountId)
			{
				$cond .= ", accountId = ?, login = '', passwd = ''";
				array_push($sql_args, $accountId);
			}
            if($loginId)
			{
				$cond .= ", login = ?";
				array_push($sql_args, $loginId);
			}
			if($password)
			{
				$cond .= ", passwd = {$password_sql}";
				array_push($sql_args, $password, $password, $password, $encryption_key);
			}
			if($esxAgentId)
			{
				$cond .= ", esxAgentId = ?";
				array_push($sql_args, $esxAgentId);
			}
			
			$sql = "UPDATE  infrastructureInventory
					SET
						discoverNow = '1',
						loginVerifyTime = '0000-00-00 00:00:00',
						lastDiscoveryErrorMessage = ''
						$cond
					WHERE
						`inventoryId` = ?";
			array_push($sql_args, $inventory_id);
			$this->conn->sql_query($sql, $sql_args);			
		}
        $this->logging_obj->my_error_handler("INFO",array("Sql"=>$sql, "Args"=>$sql_args), Log::BOTH);
		
		return $inventory_id;
	}

	function saveInventoryPhysical()
	{
		global $CLOUD_PLAN_AZURE_SOLUTION;
		$inventoryParams['siteName'] 	= isset($_POST['new_site']) ? (empty($_POST['new_site']) ? trim($_POST['select_site']) : trim($_POST['new_site'])) : '';
        $inventoryParams['hostType'] 	= trim($_POST['servertype']);
		$inventoryParams['hostIp']	= trim($_POST['ipaddress']);
		$inventoryParams['hostPort']	= trim($_POST['port']);
		$inventoryParams['inventoryId']	= trim($_POST['inventoryid']);
		$inventoryParams['infrastructureType']	= "Primary";
		$request_has_host_id = trim($_POST['input_host_id']);
		$inventoryParams['request_has_host_id']	= $request_has_host_id;
        $hostName = trim($_POST['host_name']);
        $os       = trim($_POST['operating_system']);
		$biosId	= isset($_POST['bios_id']) ? trim($_POST['bios_id']) : '';
		$mac_address_list = isset($_POST['mac_address_list']) ? trim($_POST['mac_address_list']) : '';
		$call_is_from = isset($_POST['call_is_from']) ? trim($_POST['call_is_from']) : '';
	
		//Do a join query to check with the given request host id already records exists in infra discovery tables.
		if ($request_has_host_id != '')
		{
				// checking infrastructurevms if any VM is exist with requested host id then remove it from infrastructureInventory as in case of reprotect if VM is having multi nic
				// then agent can report ip1 while reprotect, CS can receive ip2 for protection, due to this multiple entry is getting inserted in to infrastructureInventory.
				$inv_exist_sql ="select 
									ini.inventoryId,
									inv.hostUuid,
									inv.macAddressList 
								from 
									infrastructureInventory ini,
									infrastructureHosts inh,
									infrastructureVMs inv 
								where
									ini.hostType = ?
								AND
									ini.inventoryId = inh.inventoryId
								AND
									inh.infrastructureHostId = inv.infrastructureHostId
								AND
									inv.hostId = ?";
				$exist_inventory = $this->conn->sql_query($inv_exist_sql, array($inventoryParams['hostType'], $request_has_host_id));
				$this->logging_obj->my_error_handler("INFO",array("Sql"=>$inv_exist_sql, "Args"=>array($inventoryParams['hostType'], $request_has_host_id)), Log::BOTH);
				foreach($exist_inventory as $exist_data)
				{
					$exist_inventoryId = $exist_data['inventoryId'];
					$biosId = $exist_data['hostUuid'];
					$mac_address_list = $exist_data['macAddressList'];
					$delete_sql = "DELETE FROM infrastructureInventory WHERE inventoryId = ?";
					$this->conn->sql_query($delete_sql, array($exist_inventoryId));
					$this->logging_obj->my_error_handler("INFO",array("Sql"=>$delete_sql, "DeletedInfrastructureVmId"=>$exist_inventoryId), Log::BOTH);
				}
								
				$inv_sql =		"select 
				ini.inventoryId 
			from 
				infrastructureInventory ini,
				infrastructureHosts inh,
				infrastructureVMs inv 
			where
				ini.infrastructureType = ?
			AND
				ini.hostIp = ?
			AND
				ini.hostType = ?
			AND
				ini.inventoryId = inh.inventoryId
			AND
				inh.infrastructureHostId = inv.infrastructureHostId
			AND
				inv.hostId = ?";
			$inventory = $this->conn->sql_query($inv_sql, array($inventoryParams['infrastructureType'],$inventoryParams['hostIp'],$inventoryParams['hostType'], $request_has_host_id));
			$this->logging_obj->my_error_handler("INFO",array("Sql"=>$inv_sql, "Args"=>array($inventoryParams['infrastructureType'],$inventoryParams['hostIp'],$inventoryParams['hostType'], $request_has_host_id)), Log::BOTH);
			foreach($inventory as $inventory_data)
			{
				$inv_id = $inventory_data['inventoryId'];
				$sql = "UPDATE infrastructureInventory SET hostPort = ?, lastDiscoverySuccessTime = now(), autoLoginVerify = '1', loginVerifyTime = now(), loginVerifyStatus = '1', connectionStatus = '1' WHERE inventoryId = ?";
				$update_inventory = $this->conn->sql_query($sql, array($inventoryParams['hostPort'], $inv_id));
				$this->logging_obj->my_error_handler("INFO",array("Sql"=>$sql, "Args"=>array($inventoryParams['hostPort'], $inv_id)), Log::BOTH);
				return $inv_id;
			}
		}
	
		$inventoryId = $this->saveInventoryDetails($inventoryParams);		
		if($inventoryId)
		{
			$sql = "UPDATE infrastructureInventory SET hostPort = ?, lastDiscoverySuccessTime = now(), autoLoginVerify = '1', loginVerifyTime = now(), loginVerifyStatus = '1', connectionStatus = '1' WHERE inventoryId = ?";
			$update_inventory = $this->conn->sql_query($sql, array($inventoryParams['hostPort'], $inventoryId));
			$this->logging_obj->my_error_handler("INFO",array("Sql"=>$sql, "Args"=>array($inventoryParams['hostPort'], $inventoryId)), Log::BOTH);
			$hostId = $this->conn->sql_get_value("infrastructureHosts", "infrastructureHostId", "inventoryId = ? AND hostIp = ?", array($inventoryId, $inventoryParams['hostIp']));
			$infra_host_sql = (!$hostId) ? "INSERT INTO " : "UPDATE ";
			$infra_host_sql .= " `infrastructureHosts` SET `inventoryId` = ?, `hostIp` = ?, `hostName` = ?";
			$sql_args = array($inventoryId, $inventoryParams['hostIp'], $hostName);
			if($hostId)
			{
				$infra_host_sql .= " WHERE infrastructureHostId = ?";
				$sql_args[] = $hostId;
				$this->conn->sql_query($infra_host_sql, $sql_args);
			}
			else
			{
				$hostId = $this->conn->sql_query($infra_host_sql, $sql_args);
			}
			$this->logging_obj->my_error_handler("INFO",array("Sql"=>$infra_host_sql, "Args"=>$sql_args), Log::BOTH);
			/*
			Purpose of request_has_host_id introduction:
			When VMs failover to Azure then they ping back to CS with a newly generated (random) hostId. Reprotect of these VMs as physical machines works off IP address based discovery which is causing problems due to IP address reuse in in Azure. Multiple failed over VMs can end up getting the same IP assigned at different times and the CS DB then ends up having multiple entries in the hosts table with the same IP. When that happens then the discovery code in CS effectively picks a random host from the hosts table which has the IP specified in discovery and creates a row in infrastructurevms table. ASR then starts working off a wrong hostId and things have gone belly up.

			The fix here is to control the hostId with which recovery VMs would come up in Azure. Then during reprotect ASR can request discovery of the "physical machine" based on both IP address that the VM currently has plus the hostId with which it would have reported to CS. Then CS can make a correct mapping in its infrastructurevms table and we work on the right host.
			*/
			if ($request_has_host_id == '')
			{
				$vm_id = $this->conn->sql_get_value("infrastructureVMs", "infrastructureVMId", "infrastructureHostId = ? AND hostIp = ?", array($hostId, $inventoryParams['hostIp']));
				$do_host_id = " ";
				$vm_sql_args = array($hostId, $inventoryParams['hostIp'], $hostName, $hostName, $os, $biosId, $mac_address_list);
			}
			else
			{
				$vm_id = $this->conn->sql_get_value("infrastructureVMs", "infrastructureVMId", "infrastructureHostId = ? AND hostIp = ? AND hostId = ?", array($hostId, $inventoryParams['hostIp'], $request_has_host_id));
				$do_host_id = ", `hostId` = ?";
				if ($call_is_from)
				{
					$do_host_id .= ", `discoveryType` = ?";
					$vm_sql_args = array($hostId, $inventoryParams['hostIp'], $hostName, $hostName, $os, $biosId, $mac_address_list, $request_has_host_id, $CLOUD_PLAN_AZURE_SOLUTION);
				}
				else
				{
					$vm_sql_args = array($hostId, $inventoryParams['hostIp'], $hostName, $hostName, $os, $biosId, $mac_address_list, $request_has_host_id);
				}
			}
			if (!$vm_id)
			{
				$do_uuid = "`infrastructureVMId` = UUID(), ";
			}
			else
			{
				$do_uuid = " ";
			}
			
			$infra_vm_sql = (!$vm_id)  ? "INSERT INTO " : "UPDATE ";
			$infra_vm_sql .= " `infrastructureVMs` SET $do_uuid `infrastructureHostId` = ?, `hostIp` = ?, `hostName` = ?,`displayName` = ?, `OS` = ? , `hostUuid` = ?, `macAddressList` = ? $do_host_id";
			
			if($vm_id)
			{
				$infra_vm_sql .= " WHERE infrastructureVMId = ?";
				$vm_sql_args[] = $vm_id;
			}
			$this->logging_obj->my_error_handler("INFO",array("Sql"=>$infra_vm_sql, "Args"=>$vm_sql_args), Log::BOTH);
			$this->conn->sql_query($infra_vm_sql, $vm_sql_args);
			return $inventoryId;
		}
		else
		{
			return 0;
		}
    }
    
	function saveInventoryAws()
	{
		$inventoryParams['siteName'] 	= isset($_POST['new_site']) ? (empty($_POST['new_site']) ? trim($_POST['select_site']) : trim($_POST['new_site'])) : '';
        $inventoryParams['hostType'] 	= trim($_POST['servertype']);
        $inventoryParams['hostType'] 	= trim($_POST['servertype']);
        $inventoryParams['accountId'] 	= trim($_POST['accountid']);
        $inventoryParams['loginId'] 	= trim($_POST['loginid']);
        $inventoryParams['password'] 	= trim($_POST['loginpwd']);
        $inventoryParams['organization'] = trim($_POST['accountname']);
        $inventoryParams['inventoryId'] = trim($_POST['inventoryid']);
		$inventoryParams['infrastructureType']	= "Primary";

		return $this->saveInventoryDetails($inventoryParams);
	}
    
    function saveInventoryvCloud()
	{
		$inventoryParams['siteName'] 	= isset($_POST['new_site']) ? (empty($_POST['new_site']) ? trim($_POST['select_site']) : trim($_POST['new_site'])) : '';
        $inventoryParams['hostType'] 	= trim($_POST['servertype']);
        $inventoryParams['accountId'] 	= trim($_POST['accountid']);
        $inventoryParams['loginId'] 	= trim($_POST['loginid']);
        $inventoryParams['password'] 	= trim($_POST['loginpwd']);
		$inventoryParams['hostIp']	= trim($_POST['ipaddress']);
		$inventoryParams['hostPort']	= trim($_POST['port']);
        $inventoryParams['organization'] = empty($_POST['org_name']) ? null : trim($_POST['org_name']);
		$inventoryParams['infrastructureType']	= "Primary";
        $inventoryParams['esxAgentId'] = trim($_POST['discovery_agent']);
        $inventoryParams['inventoryId'] = trim($_POST['inventoryid']);

        return $this->saveInventoryDetails($inventoryParams);
    }

	function saveInventoryvCenter()
	{
		$inventoryParams['siteName'] 	= isset($_POST['new_site']) ? (empty($_POST['new_site']) ? trim($_POST['select_site']) : trim($_POST['new_site'])) : '';
        $inventoryParams['hostType'] 	= trim($_POST['servertype']);
        $inventoryParams['accountId'] 	= trim($_POST['accountid']);
        $inventoryParams['loginId'] 	= trim($_POST['loginid']);
        $inventoryParams['password'] 	= trim($_POST['loginpwd']);
		$inventoryParams['hostIp']	= trim($_POST['ipaddress']);
		$inventoryParams['hostPort']	= trim($_POST['port']);
		$inventoryParams['infrastructureType']	= "Primary";
        $inventoryParams['esxAgentId'] = trim($_POST['discovery_agent']);
        $inventoryParams['inventoryId'] = trim($_POST['inventoryid']);

        return $this->saveInventoryDetails($inventoryParams);
    }

	function saveInventoryvSphere()
	{
		$inventoryParams['siteName'] 	= isset($_POST['new_site']) ? (empty($_POST['new_site']) ? trim($_POST['select_site']) : trim($_POST['new_site'])) : '';
        $inventoryParams['hostType'] 	= trim($_POST['servertype']);
        $inventoryParams['accountId'] 	= trim($_POST['accountid']);
        $inventoryParams['loginId'] 	= trim($_POST['loginid']);
        $inventoryParams['password'] 	= trim($_POST['loginpwd']);
		$inventoryParams['hostIp']	= trim($_POST['ipaddress']);
		$inventoryParams['hostPort']	= trim($_POST['port']);
		$inventoryParams['infrastructureType']	= "Primary";
        $inventoryParams['esxAgentId'] = trim($_POST['discovery_agent']);
        $inventoryParams['inventoryId'] = trim($_POST['inventoryid']);
        
        return $this->saveInventoryDetails($inventoryParams);
    }
       
	function verifyInventoryId($infra_type=NULL, $inventoryId)
    {
		$args_array = array();
		$cond = '';
		
		array_push($args_array, $inventoryId);
		
		if($infra_type)
		{
			$cond .= " AND hostType = ?";
			array_push($args_array, $infra_type);
		}
		
        $inventory_exists = $this->conn->sql_get_value("infrastructureInventory", "inventoryId", "inventoryId = ? $cond", $args_array);
        
		return $inventory_exists;
    }
    
    /*
     -----------------------------------------------------------------------
     If inventory ids are supplied and its not array, returning empty array.
     So even if only one value is there, pass as array only
     -----------------------------------------------------------------------
    */
    function getInventoryDetails($inventory_ids=NULL, $infra_type=NULL, $discoveries=NULL, $infrastructureVmId=NULL)
    {        
        global $CONN_INFRA_ALIVE_RETRY;
		$cond_inv_sql = "";
		$args_array = array();
		
		$conj = "WHERE";
        if($infra_type)
        {
            $cond_inv_sql .= " $conj hostType = ?";
			$conj = "AND";
			array_push($args_array, $infra_type);
        }
        if($inventory_ids) 
        {
            /*
             If inventory ids are supplied and its not array, returning.
             So even if only one value is there, pass as array only
            */
            if(!is_array($inventory_ids)) return array();
            
            $inventory_ids = array_unique(array_filter($inventory_ids));
            $inventory_ids_str = implode(",", $inventory_ids);
            $cond_inv_sql .=  " $conj FIND_IN_SET(inventoryId, ?)";
			array_push($args_array, $inventory_ids_str);
        }
        
        $sql = "select inventoryId, hostIp, hostPort, hostType, lastDiscoverySuccessTime, UNIX_TIMESTAMP(lastDiscoverySuccessTime) as lastDiscoverySuccessTimestamp, loginVerifyTime, UNIX_TIMESTAMP(loginVerifyTime) as loginVerifyTimestamp, discoverNow, connectionStatus, loginVerifyStatus, lastDiscoveryErrorMessage, errCode, errPlaceHolders, accountId, additionalDetails from infrastructureInventory ".$cond_inv_sql;
        $inventories = ($cond_inv_sql) ? $this->conn->sql_query($sql, $args_array) : $this->conn->sql_exec($sql);
        
		if(!is_array($inventories)) return;
        $inventory_details = array();
        foreach($inventories as $inventory_data)
        {
            $inventory = array();
            $inv_id = $inventory_data['inventoryId'];
            $inventory['inventory_id'] = $inv_id;
            $inventory['ip_address'] = $inventory_data['hostIp'];
            $inventory['port'] = $inventory_data['hostPort'];
            $inventory['discovery_type'] = $inventory_data['hostType'];
            $inventory['last_discovery_time'] = $inventory_data['lastDiscoverySuccessTimestamp'];
            $inventory['last_verify_time'] = $inventory_data['loginVerifyTimestamp'];
			$inventory['accountId'] = $inventory_data['accountId'];
			$inventory['additionalDetails'] = $inventory_data['additionalDetails'] ? unserialize($inventory_data['additionalDetails']) : array();
            if($inventory_data['connectionStatus'] == '1')
            {
                $inventory['discovery_status'] = 'Completed';
            }
            elseif($inventory_data['lastDiscoverySuccessTime'] == '0000-00-00 00:00:00' || $inventory_data['loginVerifyTime'] == '0000-00-00 00:00:00')
            {
                if($inventory_data['loginVerifyTime'] == '0000-00-00 00:00:00') 
					$inventory['discovery_status'] = 'Pending';
                elseif($inventory_data['connectionStatus'] != '1')
				{
					$inventory['discovery_status'] = "Failed";
					$inventory['error_log'] = $inventory_data['lastDiscoveryErrorMessage'];
					$inventory['error_code'] = $inventory_data['errCode'];
					$inventory['error_place_holders'] = $inventory_data['errPlaceHolders'] ? unserialize($inventory_data['errPlaceHolders']) : array();
				}
            }
            else 
			{
				$inventory['discovery_status'] = 'Failed';
				$inventory['error_log'] = $inventory_data['lastDiscoveryErrorMessage'];
				$inventory['error_code'] = $inventory_data['errCode'];
				$inventory['error_place_holders'] = $inventory_data['errPlaceHolders'] ? unserialize($inventory_data['errPlaceHolders']) : array();				
			}
			
            $inventory_details[$inv_id] = $inventory;
        }
        $inv_ids = array_keys($inventory_details);
        $inv_ids_str = implode(",", $inv_ids);
        
        $cond_vms_sql = "";
		$args_arr = array();
		if($inv_ids_str)
		{
			$cond_vms_sql .= "AND FIND_IN_SET(ii.inventoryId, ?)";
			array_push($args_arr, $inv_ids_str);
		}
		
		if($infra_type)
		{
			$cond_vms_sql .= "AND FIND_IN_SET(ii.hostType, ?)";
			array_push($args_arr, $infra_type);
		}
		
		if($infrastructureVmId)
		{
			$cond_vms_sql .= "AND FIND_IN_SET(iv.infrastructureVMId, ?)";
			array_push($args_arr, $infrastructureVmId);
		}
		
        $get_vms_sql = "SELECT
						DISTINCT
                            iv.`infrastructureVMId`,
                            iv.`isVMPoweredOn`,
                            iv.`OS`,                            
                            iv.`isVMProtected`,
                            iv.`displayName`,
                            iv.`hostIp`,
                            iv.`hostName`,
                            iv.`publicIp`,
                            iv.`isTemplate`,
                            iv.`macIpAddressList`,
							iv.`macAddressList`,
							iv.`hostUuid`,
							iv.`instanceUuid`,
							iv.`validationErrors`,
							iv.`isVmDeleted`,
                            ii.`inventoryId` as inventoryId,
							ii.`hostType`,
                            ih.`infrastructureHostId` as infrastructureHostId,
                            ih.`hostIp` as infrastructureHostIp,
                            ih.`hostName` as infrastructureHostName,
                            ih.`organization` as infrastructureOrganization,
							ih.`datastoreDetails` as dataStoreDetails, 
							(CASE WHEN iv.hostId = p.sourceHostId THEN '1' ELSE '0' END) as isProtected,
							h.id as hostId,
							h.agentRole,
							h.processServerEnabled,
							iv.hostDetails,
							h.FirmwareType,
							h.osCaption as osVersion 
                        FROM
                            infrastructureInventory ii,
                            infrastructureHosts ih,
                            infrastructureVMs iv
						LEFT JOIN
							hosts h
						ON
							h.id != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' AND h.id = iv.hostId
                        LEFT JOIN
                            srcLogicalVolumeDestLogicalVolume p
                        ON
                            p.sourceHostId = iv.hostId
                        WHERE
                            ii.`inventoryId`=ih.`inventoryId` AND
                            ih.`infrastructureHostId`=iv.`infrastructureHostId`".                            
                            $cond_vms_sql."
						GROUP BY iv.`infrastructureVMId`";
        $server_data = ($cond_vms_sql) ? $this->conn->sql_query($get_vms_sql, $args_arr) : $this->conn->sql_exec($get_vms_sql);
		
        global $host_ip_arr, $host_nic_ip_arr;
		for($i=0;isset($server_data[$i]);$i++)
        { 
            // Running light weight query to alive connection object after every 300 records.
			if(!($i%$CONN_INFRA_ALIVE_RETRY))
			{
				$sql = "SELECT now()";
				$rs = $this->conn->sql_query($sql);
			}
			
			// Fix for Physical Server Multiple NIC
			if($server_data[$i]['hostId'] == '')
			{
				$hostId = $this->mapMacAddressForDiscoveryData($server_data[$i]['hostType'], $server_data[$i]['infrastructureVMId'], $server_data[$i]['hostIp'], $server_data[$i]['macAddressList'], $server_data[$i]['hostDetails']);
				if($hostId) $server_data[$i]['hostId'] = $hostId;
			}
			
			$inventory_details[$server_data[$i]['inventoryId']]['host_details'][$server_data[$i]['infrastructureHostId']]['infrastructureHostIp'] = $server_data[$i]['infrastructureHostIp'];
            $inventory_details[$server_data[$i]['inventoryId']]['host_details'][$server_data[$i]['infrastructureHostId']]['infrastructureHostName'] = $server_data[$i]['infrastructureHostName'];
            $inventory_details[$server_data[$i]['inventoryId']]['host_details'][$server_data[$i]['infrastructureHostId']]['infrastructureOrganization'] = $server_data[$i]['infrastructureOrganization'];
			$inventory_details[$server_data[$i]['inventoryId']]['host_details'][$server_data[$i]['infrastructureHostId']]['infrastructureHostId'] = $server_data[$i]['infrastructureHostId'];
			$inventory_details[$server_data[$i]['inventoryId']]['host_details'][$server_data[$i]['infrastructureHostId']]['dataStoreDetails'] = $server_data[$i]['dataStoreDetails'];
            $inventory_details[$server_data[$i]['inventoryId']]['host_details'][$server_data[$i]['infrastructureHostId']]['vm_data'][] = $server_data[$i];
        }
        return $inventory_details;
    }
	
	function mapMacAddressForDiscoveryData($infra_type, $infra_vm_id, $host_ip, $mac_address_list, $host_details)
	{
		global $host_nic_ip_arr, $INFRASTRUCTURE_VM_ID_DEFAULT, $API_ERROR_LOG;
		global $infraStr;
		$errStr = '';
		
		if(!$host_nic_ip_arr)
		{
			$sql = "SELECT hostId, LCASE(TRIM(TRAILING '\n' FROM macAddress)) as macAddress FROM hostNetworkAddress where ipAddress != ''";
			$host_nic_ip_arr = $this->conn->sql_get_array($sql,"macAddress","hostId",array());
		}
		
        if($infra_type == "Physical")
		{
			$sql_hosts = "SELECT id, name FROM hosts where ipAddress = ?";
			$host_data = $this->conn->sql_get_array($sql_hosts,"id","name",array($host_ip));
			
			$sql = "SELECT hostId FROM hostNetworkAddress where ipAddress = ? group by hostId UNION select id as hostId from hosts where publicIp = ?";
			$host_ip_arr = $this->conn->sql_query($sql,array($host_ip, $host_ip));
			
			//The given IP address, not found into host network address table and means not in hosts table, so return.
			if (count($host_ip_arr) == 0)
			{
				$errStr = "infra_vm_id:$infra_vm_id, There is no host id found for the given IP address $host_ip in hostNetworkAddress, Hence returning from host id mapping for the given IP address.";
				$infraStr = "$errStr,host_ip:$host_ip,infra_type:$infra_type,agent_reported_mac:".print_r($host_nic_ip_arr,true).",discovery_mac:".print_r($mac_address_list,true).",host_data:".print_r($host_data,true);
				return '';
			}
			else
			{
				$host_id_arr = array();
				$infra_vm_host_id_arr = array();
				foreach ($host_ip_arr as $key=>$value)
				{
					$host_id_arr[] = $value['hostId'];
				}
				$host_id_arr = array_unique($host_id_arr);
				if (count($host_id_arr))
				{
					$host_id_arr_str = implode("','", $host_id_arr);
				}
				
				$sql = "SELECT hostId from infrastructureVms where hostId in ('$host_id_arr_str') AND (hostIp = ? OR hostIp = '')";
				$infra_vm_host_ids = $this->conn->sql_query($sql,array($host_ip));
				foreach ($infra_vm_host_ids as $key=>$value)
				{
					$infra_vm_host_id_arr[] = $value['hostId'];
				}
				$free_host_ids = array_diff($host_id_arr, $infra_vm_host_id_arr);
				
				//For the given IP address, found host id(s) into host network address table, but already such host id(s) mapped and doesn't have any additional host id to map, so return.
				if (count($free_host_ids) == 0)
				{
					$errStr = "infra_vm_id:$infra_vm_id, For the given IP address, found host id(s) into hostnetworkaddress table, but already such host id(s) mapped and doesn't have any additional host id to map, so return.";
					$infraStr = "$errStr,host_ip:$host_ip,infra_type:$infra_type,agent_reported_mac:".print_r($host_nic_ip_arr,true).",discovery_mac:".print_r($mac_address_list,true).",host_data:".print_r($host_data,true);
					return '';
				}
				//For the given IP address, found more than one host id(s) host network address table, but don't know which one to assign.
				elseif (count($free_host_ids) > 1)
				{
					$errStr = "infra_vm_id:$infra_vm_id: More than one host id found for the given IP address $host_ip, Hence returning from host id mapping for the given IP address.,".print_r($host_id_arr,TRUE);
					$infraStr = "$errStr,host_ip:$host_ip,infra_type:$infra_type,agent_reported_mac:".print_r($host_nic_ip_arr,true).",discovery_mac:".print_r($mac_address_list,true).",host_data:".print_r($host_data,true);
					return '';
				}
				//For the given IP address, found one free host id in host network address table, and so mapping with that hostid.
				else
				{
					$host_id = array_shift($free_host_ids);
					$sql = "SELECT 
									h.biosId,
									hn.macAddress
								FROM
									hosts h,
									hostNetworkAddress hn
								WHERE
									h.id = hn.hostId
								AND
									hn.hostId = ?"; 
					$vm_data = $this->conn->sql_query($sql, array($host_id));
					
					$mac_list = $bios_list = array();
					foreach($vm_data as $key => $vm_value)
					{
						$bios_list[] = $vm_value["biosId"];
						$mac_list[] = $vm_value["macAddress"];
					}
					$physical_mac_list = join(",", array_unique($mac_list));
					$bios_id = join(",", array_unique($bios_list));
					$this->logging_obj->my_error_handler("INFO",array("PhysicalMacList"=>$physical_mac_list, "BiosId"=>$bios_id),Log::BOTH);
				}
			}
		}
        elseif($infra_type == "vCenter")
        {
			$mac_address_list_before = $mac_address_list;
            $mac_address_list = array_filter(array_unique(explode(",", $mac_address_list)));
			foreach($mac_address_list as $mac_ip)
			{
				$host_id = $host_nic_ip_arr[$mac_ip];
				if($host_id)
				{
					$this->logging_obj->my_error_handler("INFO",array("MacAddressListBefore"=>$mac_address_list_before , "MacAddressListAfter"=>$mac_address_list, "HostId"=>$host_id),Log::BOTH);
					break;
				}
			}
		}
		if($host_id)
		{
			if($infra_type == "Physical")
			{
				$protection_exist_sql = "SELECT count(*) as pairCount FROM applicationScenario, srcLogicalVolumeDestLogicalVolume WHERE sourceId = sourceHostId AND sourceId = ?";
				$protection_data = $this->conn->sql_query($protection_exist_sql, array($host_id));
				$this->logging_obj->my_error_handler("INFO","Physical VM list protection_data::".print_r($protection_data, true));
				// Before updating the physical VM & delete the vmware vm which has the same bios Id and whose and no vmware tools.
				// There are chances that customer protected and then uninstalled vmware tools in those cases host id will set so not deleting those.
				if($protection_data[0]["pairCount"] <= 0)
				{
					$vmware_infra_vm_id = $this->conn->sql_get_value("infrastructureVMs", "infrastructureVMId", "hostUuid = ? AND macAddressList = ? AND hostIp = ''", array($bios_id, $physical_mac_list));
					
					$delete_sql = "DELETE FROM infrastructureVMs WHERE infrastructureVMId = ?";
					$this->conn->sql_query($delete_sql, array($vmware_infra_vm_id));
					$this->logging_obj->my_error_handler("INFO",array("PhysicalVmListVmWareInfraVmId"=>$vmware_infra_vm_id, "ProtectedPairCount"=>$protection_data[0]["pairCount"]), Log::BOTH);
					
					if($vmware_infra_vm_id) 
					{
						$cond = ", isPersonaChanged = '1'";
					}
				}				
				$sql = "UPDATE infrastructureVMs SET hostId = ?, macAddressList = ?, hostUuid = ? $cond WHERE infrastructureVMId = ?";
				$sql_args = array($host_id, $physical_mac_list, $bios_id, $infra_vm_id);
			} 
			else
			{
				$sql = "UPDATE infrastructureVMs SET hostId = ? WHERE infrastructureVMId = ?";
				$sql_args = array($host_id, $infra_vm_id);
			}	
			
			$infra_vm_id_arr = $this->commonfunctions_obj->get_infrastructure_id_for_hostid($host_id);
			//As of now host id mapping doesn't exist (or) 
			//Host id mapping exists and for same infrastructure vm id allow to update required properties.
			if (count($infra_vm_id_arr) == 0 || in_array($infra_vm_id, $infra_vm_id_arr))
			{
				$this->conn->sql_query($sql, $sql_args);
			
				debug_access_log($API_ERROR_LOG, "HostId mapping done $sql, hostid: $host_id, infra vm id: $infra_vm_id, infravm_default: $INFRASTRUCTURE_VM_ID_DEFAULT, hostip: $host_ip, infra type: $infra_type \n");
				$this->logging_obj->my_error_handler("INFO",array("InfrastructureVmId"=>$infra_vm_id , "HostId"=>$host_id, "HostIp"=>$host_ip, "InfraType"=>$infra_type, "Sql"=>$sql, "SqlArgs"=>$sql_args, "AgentReportedMac"=>$host_nic_ip_arr, "InfraVmDefault"=>$INFRASTRUCTURE_VM_ID_DEFAULT),Log::BOTH);
			
				// Restting the originalVMId in case of a failovered VM
				$sql = "UPDATE hosts SET originalVMId = ? WHERE id = ?";
				$this->conn->sql_query($sql, array($INFRASTRUCTURE_VM_ID_DEFAULT, $host_id));
			}
		}
		else
		{
			$infraStr = "Mapping Failed: infra_vm_id:$infra_vm_id,host_ip:$host_ip,infra_type:$infra_type,\ndiscovery_mac:".print_r($mac_address_list,true)."\nagent_reported_mac:".print_r($host_nic_ip_arr,true).",host_data:".print_r($host_data,true);
		}
		return $host_id;		
	}
	
	function deleteInventory($inventoryId)
    {
		$delete_inv_sql = "DELETE FROM infrastructureInventory WHERE inventoryId = ?";
		$inventory_deleted = $this->conn->sql_query($delete_inv_sql, array($inventoryId));
        return 1;
    }
	
	public function getProtectionsForId($inventoryId)
	{
		$hostId = array();
		$replicaId = array();
		$servers_protected = 0;
		
		$sql = "SELECT
							iv.hostId,
							iv.replicaId
						FROM
							infrastructureInventory ii,
                            infrastructureHosts ih,
                            infrastructureVMs iv
						WHERE
							ii.inventoryId = ih.inventoryId AND 
							ih.infrastructureHostId = iv.infrastructureHostId AND
							ii.inventoryId = ?";
		$hostid_list = $this->conn->sql_query($sql, array($inventoryId));
		
		if(is_array($hostid_list))
		{
			foreach($hostid_list as $infra_vm)	{
				if($infra_vm['hostId']) $hostId[] = $infra_vm['hostId'];
				if($infra_vm['replicaId']) $replicaId[] = $infra_vm['replicaId'];
			}
			$vm_hst_id = implode(",",array_unique($hostId));
			// 103 - Failover Completed
			// 130 - Rollback Completed
			$servers_protected = $this->conn->sql_get_value("applicationScenario","count(*)","FIND_IN_SET(sourceId,?) AND executionStatus NOT IN ('103', '130')",array($vm_hst_id));
			if(!$servers_protected) $servers_protected = count($replicaId);
		}
		return $servers_protected;
	}
	
	public function getProtectedVMsMigrationStatusForInventoryId($inventoryId)
	{
		$hostId = array();
		$source_id_list = array();
		$protected_source_id_list = array();
		$return_status = 0;
		
		$sql = "SELECT
							iv.hostId,
							iv.replicaId
						FROM
							infrastructureInventory ii,
                            infrastructureHosts ih,
                            infrastructureVMs iv
						WHERE
							ii.inventoryId = ih.inventoryId AND 
							ih.infrastructureHostId = iv.infrastructureHostId AND
							ii.inventoryId = ?";
		$hostid_list = $this->conn->sql_query($sql, array($inventoryId));
		
		if(is_array($hostid_list))
		{
			foreach($hostid_list as $infra_vm)	
			{
				if($infra_vm['hostId']) $hostId[] = $infra_vm['hostId'];
			}
			$vm_hst_id = implode(",",array_unique($hostId));
			// 103 - Failover Completed
			// 130 - Rollback Completed
			$sql = "SELECT
						sourceId
					FROM
						applicationScenario
					WHERE
						FIND_IN_SET(sourceId,?) 
					AND 
						executionStatus NOT IN (?,?)";
							
			$protected_server_list = $this->conn->sql_query($sql, array($vm_hst_id,103,130));
			
			if ( is_array($protected_server_list) && (count($protected_server_list) > 0) )
			{
				foreach($protected_server_list as $source_id)	
				{
					if ($source_id['sourceId'])
					{
						$source_id_list[] = $source_id['sourceId'];
					}
				}
				
				$host_id_list = implode(",",array_unique($source_id_list));
				
				$sql = "SELECT 
							DISTINCT  
							sourceHostId 
						FROM
							srcLogicalVolumeDestLogicalVolume 
						WHERE
							FIND_IN_SET(sourceHostId,?)";
				$servers_protected = $this->conn->sql_query($sql, array($host_id_list));
				
				if ( is_array($servers_protected) && (count($servers_protected) > 0) )
				{
					foreach($servers_protected as $protected_source_id)	
					{
						if ($protected_source_id['sourceHostId'])
						{
							$protected_source_id_list[] = $protected_source_id['sourceHostId'];
						}
					}
					$protected_source_host_id_list = implode(",",array_unique($protected_source_id_list));

					$sql = "SELECT
								id 
							FROM
								hosts 
							WHERE
								FIND_IN_SET(id,?)
							AND 
								isGivenToMigration = ?";
								
					$migrated_vms_list = $this->conn->sql_query($sql, array($protected_source_host_id_list, 1));
					
					if ( is_array($migrated_vms_list) && (count($migrated_vms_list) > 0) )
					{
						foreach($migrated_vms_list as $migrated_vm_id)	
						{
							if ($migrated_vm_id['id'])
							{
								$migrated_vm_id_list[] = $migrated_vm_id['id'];
							}
						}
						if (count($protected_source_id_list) == count($migrated_vm_id_list))
							$return_status = 1;
					}
				}
		
			}
		}
		
		return $return_status;
	}
	
	public function getProtectedVMsMigrationStatusForInventoryIp($infra_ip ,$infra_type)
	{
		$hostId = array();
		$source_id_list = array();
		$protected_source_id_list = array();
		$return_status = 0;
		
		$sql = "SELECT
					iv.hostId,
					iv.replicaId
				FROM
					infrastructureInventory ii,
					infrastructureHosts ih,
					infrastructureVMs iv
				WHERE
					ii.inventoryId = ih.inventoryId AND 
					ih.infrastructureHostId = iv.infrastructureHostId AND 
					ii.hostIp = ? AND 
					ii.hostType = ?";
		$hostid_list = $this->conn->sql_query($sql, array($infra_ip ,$infra_type));
		
		if(is_array($hostid_list))
		{
			foreach($hostid_list as $infra_vm)	
			{
				if($infra_vm['hostId']) $hostId[] = $infra_vm['hostId'];
			}
			$vm_hst_id = implode(",",array_unique($hostId));
			// 103 - Failover Completed
			// 130 - Rollback Completed
			$sql = "SELECT
						sourceId
					FROM
						applicationScenario
					WHERE
						FIND_IN_SET(sourceId,?) 
					AND 
						executionStatus NOT IN (?,?)";
							
			$protected_server_list = $this->conn->sql_query($sql, array($vm_hst_id,103,130));
			
			if ( is_array($protected_server_list) && (count($protected_server_list) > 0) )
			{
				foreach($protected_server_list as $source_id)	
				{
					if ($source_id['sourceId'])
					{
						$source_id_list[] = $source_id['sourceId'];
					}
				}
				
				$host_id_list = implode(",",array_unique($source_id_list));
				
				$sql = "SELECT 
							DISTINCT  
							sourceHostId 
						FROM
							srcLogicalVolumeDestLogicalVolume 
						WHERE
							FIND_IN_SET(sourceHostId,?)";
				$servers_protected = $this->conn->sql_query($sql, array($host_id_list));
				
				if ( is_array($servers_protected) && (count($servers_protected) > 0) )
				{
					foreach($servers_protected as $protected_source_id)	
					{
						if ($protected_source_id['sourceHostId'])
						{
							$protected_source_id_list[] = $protected_source_id['sourceHostId'];
						}
					}
					$protected_source_host_id_list = implode(",",array_unique($protected_source_id_list));

					$sql = "SELECT
								id 
							FROM
								hosts 
							WHERE
								FIND_IN_SET(id,?)
							AND 
								isGivenToMigration = ?";
								
					$migrated_vms_list = $this->conn->sql_query($sql, array($protected_source_host_id_list, 1));
					
					if ( is_array($migrated_vms_list) && (count($migrated_vms_list) > 0) )
					{
						foreach($migrated_vms_list as $migrated_vm_id)	
						{
							if ($migrated_vm_id['id'])
							{
								$migrated_vm_id_list[] = $migrated_vm_id['id'];
							}
						}
						if (count($protected_source_id_list) == count($migrated_vm_id_list))
							$return_status = 1;
					}
				}
		
			}
		}
		
		return $return_status;
	}
	
	/*
    * Function : getProtectionsForInfrastructureDetails
    * Description : This function will check the protections existence
    *               with the given infrastructure ip and type and returns
	*				the count of protected VM's.
	* Parameters: Infrastructure IP Address, Infrastructure Type
    */
	public function getProtectionsForInfrastructureDetails($infrastructureIp, $infrastructureType)
	{
		global $FAILOVER_DONE, $ROLLBACK_COMPLETED;
		$hostId = array();
		$replicaId = array();
		$servers_protected = 0;
		
		$sql = "SELECT
							iv.hostId,
							iv.replicaId
						FROM
							infrastructureInventory ii,
                            infrastructureHosts ih,
                            infrastructureVMs iv
						WHERE
							ii.inventoryId = ih.inventoryId AND 
							ih.infrastructureHostId = iv.infrastructureHostId AND
							ii.hostIp = ? AND 
							ii.hostType = ?";
		$hostid_list = $this->conn->sql_query($sql, array($infrastructureIp, $infrastructureType));
		
		if(is_array($hostid_list))
		{
			foreach($hostid_list as $infra_vm)	
			{
				if($infra_vm['hostId']) $hostId[] = $infra_vm['hostId'];
				if($infra_vm['replicaId']) $replicaId[] = $infra_vm['replicaId'];
			}
			$vm_hst_id = implode(",",array_unique($hostId));
			// 103 - Failover Completed
			// 130 - Rollback Completed
			$servers_protected = $this->conn->sql_get_value("applicationScenario","count(*)","FIND_IN_SET(sourceId,?) AND executionStatus NOT IN ('$FAILOVER_DONE', '$ROLLBACK_COMPLETED')",array($vm_hst_id));
			if(!$servers_protected) $servers_protected = count($replicaId);
		}
		return $servers_protected;
	}
	
    /*
    * Function : getInventoryId
    * Description : This function will return the inventory Id 
    *               matching the given IP Address and infrastructure type
    */    
	public function getInventoryId($ip_address, $discovery_type)
	{
		return $this->conn->sql_get_value("infrastructureInventory", "inventoryId", "hostIp = ? AND hostType = ?", array($ip_address, $discovery_type));
    }
    
    /*
    * Function : checkVmExists
    * Description : This function will validate if a VM
    *               matches the given IP Address
    */    
    public function checkVmExists($ip_address)
    {
		$sql = "SELECT 
					ivm.infrastructureVMId
				FROM
					infrastructureVMs ivm,
					infrastructureHosts ivh,
					infrastructureInventory ivi
				WHERE
					( ivm.hostIp = ? OR 
					ivm.ipList LIKE ? ) AND
					ivm.infrastructureHostId = ivh.infrastructureHostId AND
					ivh.inventoryId = ivi.inventoryId AND
					ivi.hostType != 'Physical'";
		return $this->conn->sql_query($sql, array($ip_address, "%,".$ip_address.",%"));
	}
	
	/*
    * Function : updateReplicaId
    * Description : This function will update the replica id
    *               of the matching source VMs
    */    
    public function updateReplicaId($srcIdList)
	{
		if(!is_array($srcIdList)) return 0;
		
		$srcIdList = array_unique(array_filter($srcIdList));
        $srcIdList_str = implode(",", $srcIdList);
		$get_resourceids_sql = "SELECT id, resourceId FROM hosts WHERE FIND_IN_SET(id, ?)";
		$srcIdResourceIdMapping = $this->conn->sql_get_array($get_resourceids_sql, "id", "resourceId", array($srcIdList_str));
		$resourceIdsArray = array_values($srcIdResourceIdMapping);
    
		$resourceIds_str = implode(",", $resourceIdsArray);
		$get_resource_ids_sql = "SELECT id, resourceId FROM hosts WHERE FIND_IN_SET(resourceId, ?)";
		$resourceIdMappingArray = $this->conn->sql_get_array($get_resource_ids_sql, "id", "resourceId", array($resourceIds_str));
		$idResourceIdMapping = array();
		foreach($resourceIdMappingArray as $hostId => $resourceId) {
			$idResourceIdMapping[$resourceId][] = $hostId;
		}
		$hostIdsArray = array_keys($resourceIdMappingArray);
		
		$hostIds_str = implode(",", $hostIdsArray);
		$get_infra_vms_sql = "SELECT hostId, infrastructureVMId FROM infrastructureVMs WHERE FIND_IN_SET(hostId, ?)";
		$infraVmIdMapping = $this->conn->sql_get_array($get_infra_vms_sql, "hostId", "infrastructureVMId", array($hostIds_str));
		
		foreach($srcIdList as $hostId) {
			$resourceId = $resourceIdMappingArray[$hostId];
			$hostIds = $idResourceIdMapping[$resourceId];
			foreach($hostIds as $id) {
				if(!in_array($id, $srcIdList)) {
					if($infraVmIdMapping[$id] != "" && $infraVmIdMapping[$hostId] != "") {
						$sql = "UPDATE infrastructureVMs SET replicaId = ? WHERE infrastructureVMId = ?";
						$this->conn->sql_query($sql, array($infraVmIdMapping[$id], $infraVmIdMapping[$hostId]));
					}
				}
			}
		}
	}
	
	/*
    * Function : cleanStaleDiscoveredVMs
    * Description : This function will clean the replica id
    *               of the matching source VMs
    */    
    public function cleanStaleDiscoveredVMs($srcIdList)
	{
		if(!is_array($srcIdList)) return 0;
		$srcIdList_str = implode(",", $srcIdList);
		$get_infra_hostids_sql = "SELECT hostId, replicaId FROM infrastructureVMs WHERE FIND_IN_SET(hostId, ?)";
		$replicaIdMapping = $this->conn->sql_get_array($get_infra_hostids_sql, "hostId", "replicaId", array($srcIdList_str));
		$replicaIds = array_values($replicaIdMapping);
		$replica_ids_str = implode(",", $replicaIds);
		$this->logging_obj->my_error_handler("INFO",array("SrcIdList"=>$srcIdList, "ReplicaIdMapping"=>$replicaIdMapping, "ReplicaIdString"=>$replica_ids_str), Log::BOTH);
		$get_delete_status_sql = "SELECT infrastructureVMId, isVmDeleted FROM infrastructureVMs WHERE FIND_IN_SET(infrastructureVMId, ?)";
		$replicaVmData = $this->conn->sql_get_array($get_delete_status_sql, "infrastructureVMId", "isVmDeleted", array($replica_ids_str));
		$this->logging_obj->my_error_handler("INFO",array("ReplicaVmData"=>$replicaVmData), Log::BOTH);
		foreach($replicaIdMapping as $hostId => $replicaVmId) 
		{
			$clean_replica_sql = "UPDATE infrastructureVMs SET replicaId = ? WHERE hostId = ?";
			$this->logging_obj->my_error_handler("INFO",array("Sql"=>$clean_replica_sql, "Args"=>array('', $hostId)), Log::BOTH);
			$this->conn->sql_query($clean_replica_sql, array('', $hostId));
			if($replicaVmData[$replicaVmId] == '1') 
			{				
				$delete_vm_sql = "DELETE FROM infrastructureVMs WHERE infrastructureVMId = ?";
				$this->logging_obj->my_error_handler("INFO",array("Sql"=>$delete_vm_sql, "Args"=>array($replicaVmId)), Log::BOTH);
				$this->conn->sql_query($delete_vm_sql, array($replicaVmId));
			}
		}
	}
	
	/*
    * Function : validatePhysicalServer
    * Description : This function will validate if provided ip address
    *               is belonging to PS or MT.
    */ 	
	public function validatePhysicalServer($server_ip)
	{
		$serverFlag = false;
		$sql = "SELECT 
					h.processServerEnabled,
					h.agentRole 
				FROM
					hosts h,
					hostnetworkaddress hn 
				WHERE
					h.id = hn.hostId 
				AND 
					hn.ipAddress = ?";
		$rs = $this->conn->sql_query($sql, array($server_ip));
		foreach($rs as $server)
		{
			$processServerEnabled = $server['processServerEnabled'];
			$agentRole = $server['agentRole'];
			if((strtolower($agentRole) == 'mastertarget') || (isset($processServerEnabled)))
			{
				$serverFlag = true;	
			}
		}
		return $serverFlag;
	}
}
?>