<?php
/*
 *================================================================= 
 * FILENAME
 *    PushInstall.php
 *
 * DESCRIPTION
 *    This script contains Push Install Related Functions.
 *
 *
 * HISTORY
 *     <29 April 2009>  Author    Created.	hpatel.
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
 */

class PushInstall
{
   /*
     $db_handle: database connection object
   */
    
    private $commonfunctions_obj;
    private $conn;
    private $db_handle;
    private $pushinstall_obj;
	private $encryption_key;
	private $account_obj;
    
    /*
     * Function Name: PushInstall
     *
     * Description:
     *               Constructor
     *               It initializes the private variable
    */
    function __construct()
    {
      global $INSTALLER_PATH,$WINDOW_AGENT_INSTALL_PATH, $LINUX_AGENT_INSTALL_PATH,$CONFIG_WEB_ROOT,$conn_obj;
      $this->commonfunctions_obj = new CommonFunctions(); 
      $this->account_obj = new Accounts(); 
	  $this->conn = $conn_obj;
      $this->db_handle = $this->conn->getConnectionHandle();

		if (preg_match('/Windows/i', php_uname()))
			$this->agent_installer_path = $CONFIG_WEB_ROOT."\sw";
    	else
			$this->agent_installer_path = $INSTALLER_PATH;

			$this->window_agent_install_path = $WINDOW_AGENT_INSTALL_PATH;
			$this->linux_agent_install_path = $LINUX_AGENT_INSTALL_PATH;
			$this->encryption_key = $this->commonfunctions_obj->getCSEncryptionKey();
    }

  
   /*		
   * Function Name: register_push_proxy
   *
   * Description:
   *    This function is responcible for PUSH Server registration
   *
   * Parameters:
   * Return Value: [IN] Host ID
   * Return Value: [IN] IP Address
   * Return Value: [IN] Host Name
   * Return Value: [IN] OS Flag
   * Return Value: [OUT] Query result
   *
   * Exceptions:
   */
   public function register_push_proxy ($host_id, $ip_address, $host_name, $os_flag)
   {
		/*
		* Check if the host is already registered 
		*/
		$check_host_registered = "SELECT 
						count(*) as proxyServerCount 
					  FROM
						pushProxyHosts 
					  WHERE
						id = ? 
					  GROUP BY
						ipaddress";
		$host_result = $this->conn->sql_query($check_host_registered, array($host_id));
				
		/*
		 * Update the host params if the host is already
		 * registered, else register.
		 */
		if (is_array($host_result) && count($host_result) > 0)
		{
			if ($host_result[0]['proxyServerCount'] > 0) 
			{
				$update_host = "UPDATE 
							pushProxyHosts
						SET
							ipaddress = ?,
							name = ?,
							lastHostUpdateTime = now()
						WHERE
							id = ?";
				$result = $this->conn->sql_query($update_host, array($ip_address, $host_name, $host_id));							
			}
		}
		else
		{
		 
			$sql = "INSERT INTO
				pushProxyHosts 
				   (id,
				   ipaddress, 
				   name, 
				   osFlag,
				   lastHostUpdateTime)
				 values 
				   (?, 
					?,
					?,
					?,
					now())";
			$result = $this->conn->sql_query($sql, array($host_id, $ip_address, $host_name, $os_flag));
		}
		
		//This return value is actually not giving to caller agent, but it is using in caller function to write logging.
		return TRUE;
   }
   /*		
   * Function Name: unregister_push_proxy
   *
   * Description:
   *    This function is responcible for PUSH Server Un registration
   *
   * Parameters:
   * Return Value: [IN] Host ID
   * Return Value: [OUT] Query result
   *
   * Exceptions:
   */   

   public function unregister_push_proxy ($host_id)
   {
		$unregister_push_proxy = "DELETE 
						FROM pushProxyHosts
					  WHERE 
						id='".$this->conn->sql_escape_string($host_id)."'";
		$result = $this->conn->sql_query($unregister_push_proxy);
		return $result;
				  
   }
	
   /*
    * Function Name: get_install_hosts
    *
    * Description: 
    * Push install service from proxy host gets the list of all windows hosts
    * where installation is pending.
    *
    * Parameters:
    *     Param 1 [IN]:IP
    *     Param 1 [OUT]:status
    *
    * Return Value:
    *     Ret Value:
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */

	public function get_install_hosts($obj) 
	{
		global $DB_HOST,$DB_DATABASE_NAME,$DB_ROOT_USER,$DB_ROOT_PASSWD,$logging_obj, $HOST_GUID;
		$host_id = $obj[host_id];
		
		if($host_id != "")
		{
			$push_proxy_server_ostype = $this->check_push_server_ostype($host_id);
			if($push_proxy_server_ostype == 1 )
				$push_proxy_server_ostypestr = "1,2";
			elseif($push_proxy_server_ostype == 2 )
				$push_proxy_server_ostypestr = "2";
			
			if($this->encryption_key)	
			{	
				$process_server_host_info = $this->commonfunctions_obj->getAllPsDetails();
				$accounts_details = $this->account_obj->getAccountDetails();
				
				$telemetry_log_string["ProcessServerHostInfo"]=$process_server_host_info;
				$telemetry_log_string["PushProxyServerOsType"]=$push_proxy_server_ostype;
				$telemetry_log_string["HostId"]=$host_id;
				$logging_obj->my_error_handler("INFO",$telemetry_log_string,Log::APPINSIGHTS);
				
				/*
				*Backward compatibility query condition check is a.infrastructureVmId = ''.
				*Latest version has infrastructureVmId support, hence do the join with infraStructureVmId 
				instead of IP address join, to avoid multiple same IP addresses in push installation settings. 
				*Later DHCP case identified, because of this IP Adress changing. Hence, we need to have join with ip address and infrastructureVmId comparison.
				*/
				$sql2 = "(SELECT DISTINCT
							a.jobId,
							b.ipaddress, 
							IF (ISNULL(b.userName) OR b.userName = '', b.userName, AES_DECRYPT(UNHEX(b.userName), ?)) as userName,
							AES_DECRYPT(UNHEX(b.password), ?) as password,
							accountId,
							b.osType, 
							IF (ISNULL(b.domain) OR b.domain = '', b.domain, AES_DECRYPT(UNHEX(b.domain), ?)) as domain,							
							b.parllelPushCount
						FROM
							`agentInstallerDetails` AS a,
							`agentInstallers` AS b
						WHERE
							a.infrastructureVmId = b.infrastructureVmId AND
							a.`ipaddress` = b.`ipaddress` AND
							a.infrastructureVmId = ? AND
							(a.`state` =? OR a.`state` = ?) AND 
							FIND_IN_SET(b.`osType` , ?) AND
							b.pushServerId = ?)
						UNION
						(SELECT DISTINCT
							a.jobId,
							b.ipaddress, 
							IF (ISNULL(b.userName) OR b.userName = '', b.userName, AES_DECRYPT(UNHEX(b.userName), ?)) as userName,
							AES_DECRYPT(UNHEX(b.password), ?) as password,
							accountId,
							b.osType, 
							IF (ISNULL(b.domain) OR b.domain = '', b.domain, AES_DECRYPT(UNHEX(b.domain), ?)) as domain,							
							b.parllelPushCount
						FROM
							`agentInstallerDetails` AS a,
							`agentInstallers` AS b
						WHERE
							a.infrastructureVmId = b.infrastructureVmId AND
							a.infrastructureVmId != ?  AND
							a.`ipaddress` = b.`ipaddress` AND 
							(a.`state` =? OR a.`state` = ?) AND 
							FIND_IN_SET(b.`osType` , ?) AND
							b.pushServerId = ?) ORDER BY domain";
				$Arr = array();
				$MyDom = array();
				$result_set = array();
                $account_details = array();
				$result_set = $this->conn->sql_query($sql2, array($this->encryption_key, $this->encryption_key, $this->encryption_key,'',1,4,$push_proxy_server_ostypestr, $host_id,$this->encryption_key, $this->encryption_key, $this->encryption_key,'',1,4,$push_proxy_server_ostypestr, $host_id));
				
				$num = count($result_set);
				$MainArr = array();
				$MainArr[0] = 0;
				$DomainArr = array();
				$DomainIPArr = array();//This flags indicates that current push jobs are related to the called push server
				if($num>0)
					$MainArr[1] = TRUE;
				else
					$MainArr[1] = FALSE;
				$MainArr[2] = 1;
				if($num > 0)
				{
					$i=0;
					$j=0;
					$k=0;
					global $curr_com1;			
					
					foreach($result_set as $key => $data_arr)
					{
						$curr_com = $data_arr['domain'];
						$parllelPushCount = $data_arr['parllelPushCount'];
						if($parllelPushCount == 0 || $parllelPushCount == "")
						{
							$parllelPushCount = 1;
						}
						$MainArr[2] = (int)$parllelPushCount;
                        $install_acc_id = $data_arr['accountId'];
                        if($install_acc_id)
                        {
                            if(!$account_details[$install_acc_id])
                            {
                                $account_data = $this->account_obj->getAccountDetails($install_acc_id);
                                $account_details[$install_acc_id]['userName'] = $account_data[0]['userName'];
                                $account_details[$install_acc_id]['password'] = $account_data[0]['password'];
                                $account_details[$install_acc_id]['domain'] = $account_data[0]['domain'];
                            }
                            $data_arr['userName'] = $account_details[$install_acc_id]['userName'];
                            $data_arr['password'] = $account_details[$install_acc_id]['password'];
                            $data_arr['domain'] = $account_details[$install_acc_id]['domain'];
                        }
						if($curr_com1 != $curr_com || $curr_com == NULL) 
						{
					 
							$data[$j][$i] = $data_arr['ipaddress'];
							$DomainIPArr[$i]  = $data_arr['domain'];
							$DomainArr[0] = 0;
							$DomainArr[1] = intval($data_arr['osType']);
							$DomainArr[2] = $data_arr['userName'];

							$DomainArr[3] = $data_arr['password'];
							$DomainArr[4] = $data_arr['domain'];
							$DomainArr[5] = $this->get_ip_by_domain($data_arr['domain'],$push_proxy_server_ostypestr,$data_arr['jobId'],$process_server_host_info,$accounts_details);

							array_push($MyDom,$un,$pw,$ot,$do,$data);

							$MainArr1[$j] = $DomainArr;

							$j++;
						} 
						else
						{				
							$data[$j][$i] = $data_arr['ipaddress'];
							$un = $data_arr['userName'];
							$pw = $data_arr['password'];
							$ot = $data_arr['osType'];
							$do = $data_arr['domain'];
						}
					$i++;
				   }
				} 

				if(count($MainArr1) > 0) 
				{
					array_push($MainArr,$MainArr1);
				} 
				else
				{
					$MainArr1 = Array();
					array_push($MainArr,$MainArr1); 
				}
			}
			else
			{
				$telemetry_logs["Message"]="Encryption key not found for get install hosts";
				$telemetry_logs["Status"]="Fail";
				$telemetry_logs["HostId"]=$host_id;
				$logging_obj->my_error_handler("EXCEPTION",$telemetry_logs,Log::APPINSIGHTS);
				$MainArr = array(0, FALSE, 1 , array());
			}
			
		}
		return $MainArr;
   }


	public function check_push_server_ostype($host_id) {
	
		  $sql = "SELECT
					  osFlag
				  FROM
					  pushProxyHosts 
				  WHERE
			 id = '$host_id'";		 
			
		  $iter = $this->conn->sql_query( $sql );
		  $data = $this->conn->sql_fetch_row($iter);
		  $data1 = $data[0];
		  
		  return $data1;
	}


   /*
    * Function Name: get_ip_by_domain
    *
    * Description: 
    * Push install client on windows hosts will communicate with CX and download
    * the agent installer and execute it
    *
    * Parameters:
    *     Param 1 [IN]:DOMAIN
    *     Param 1 [OUT]:DATA
    *
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */

	private function get_ip_by_domain($domain,$ps_os_type,$job_id,$process_server_host_info=array(),$account_data=array()) 
	{   
		global $logging_obj, $PUSH_USING_VSPHERE_API_VERSION;
	
		if($this->encryption_key)
		{	 
			$domain_cond = "";
			$param_arr =  array('',1,4,$ps_os_type, $job_id,'',1,4,$ps_os_type, $job_id);	
			$sql = "SELECT DISTINCT 
						   b.ipaddress, 
						   iv.ipList,
						   a.jobId, 
						   a.state,
						   a.agentinstallationPath,
						   a.rebootRequired,
						   b.hostName,
						   b.pushServerId,
						   b.infrastructureVmId,
						   b.accountId 
						FROM 
						   `agentInstallerDetails` as a, 
						   `agentInstallers` as b,		   
							infrastructureVMs as iv
						WHERE 
							a.`infrastructureVmId` = b.`infrastructureVmId` AND 
							b.infrastructureVmId = ?	AND 
							iv.hostIp = b.ipaddress AND 
							a.`ipAddress` = b.`ipAddress` AND 
							(a.`state` =? OR a.`state` =?) AND 
							FIND_IN_SET (b.`osType`, ?) AND 
							a.`jobId` = ? 
					UNION 
					SELECT DISTINCT 
						   b.ipaddress, 
						   iv.ipList,
						   a.jobId, 
						   a.state,
						   a.agentinstallationPath,
						   a.rebootRequired,
						   b.hostName,
						   b.pushServerId,
						   b.infrastructureVmId,
						   b.accountId 
						FROM 
						   `agentInstallerDetails` as a, 
						   `agentInstallers` as b,		   
							infrastructureVMs as iv 
						WHERE 
							a.`infrastructureVmId` = b.`infrastructureVmId` AND 
							b.infrastructureVmId != ?	AND 
							b.infrastructureVMId = iv.infrastructureVMId AND
							a.`ipAddress` = b.`ipAddress` AND 
							(a.`state` =? OR a.`state` =?) AND 
							FIND_IN_SET (b.`osType`, ?) AND 
							a.`jobId` = ?";
			$result_data = array();		
			$result_data = $this->conn->sql_query ($sql, $param_arr);
			$i=0;
			$logging_obj->my_error_handler("INFO","get_ip_by_domain sql::$sql \n param_arr : ".print_r($param_arr, true)); 
			$cx_patch_details = $this->commonfunctions_obj->get_cx_patch_history();
			$cs_version = 0;
			if(is_array($cx_patch_details))
			{
				$cs_version_details = explode("_",$cx_patch_details[count($cx_patch_details)-1][0]);
				$cs_version = str_replace(".","",$cs_version_details[2]);
			}	
			$logging_obj->my_error_handler("INFO","get_ip_by_domain cs_version::$cs_version \n cx_patch_details : ".print_r($cx_patch_details, true)); 
			$ps_version_info = array();
			
			foreach ($result_data as $key => $host_info) 
			{
				$ps_version = 0;
				if(!$ps_version_info[$host_info['pushServerId']])
				{
					$version_info = $this->conn->sql_get_value("hosts","psPatchVersion","id=?", array($host_info['pushServerId']));
					$ps_patch_details = explode(",",$version_info);
					$ps_version_details = explode("_",$ps_patch_details[count($ps_patch_details)-1]);
					$ps_version = str_replace(".","",$ps_version_details[2]);
					$ps_version_info[$host_info['pushServerId']] = $ps_version;
				}
				else $ps_version = $ps_version_info[$host_info['pushServerId']];	
					
				$logging_obj->my_error_handler("INFO","get_ip_by_domain ps_version::$ps_version \n ps_version_info::$ps_version_info \n"); 
				
				$domainip[$i][0] = 0;
				$ip_list_arr = array();
				if(($host_info['ipaddress'] == "") || ($host_info['ipaddress'] == NULL))
				{
				   $domainip[$i][1] = $host_info['hostName'];
				}
				else 
				{
					$domainip[$i][1] = $host_info['ipaddress'];
					$logging_obj->my_error_handler("INFO","get_ip_by_domain ps_version::$ps_version \n cs_version::$cs_version \n "); 
					if(!empty($host_info['ipList']) && (($ps_version >= $cs_version) || (!$cs_version && !$ps_version)))
					{					
						$logging_obj->my_error_handler("INFO","get_ip_by_domain \n Inside if loop"); 
						$ip_list_arr = explode(",", $host_info['ipaddress'].",".trim($host_info['ipList'], ","));									
						$ip_list_arr = array_unique($ip_list_arr);
						$ip_list_arr = array_unique(array_filter($ip_list_arr, function($a) { return filter_var($a, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4); } ));
						$domainip[$i][1] = implode(",",$ip_list_arr);
					}			  				
				}
				
				if(($host_info['rebootRequired']) == 1)
				   $domainip[$i][2] = TRUE;
				elseif(($host_info['rebootRequired']) == 0)
				   $domainip[$i][2] = FALSE; 
			
				$logging_obj->my_error_handler("INFO","get_ip_by_domain agentinstallationPath : ".$host_info['agentinstallationPath']);   

				if($host_info['state'] == 4)
				{
				   $sql_default_install_path = "SELECT DISTINCT 
				   b.vxAgentPath, 
				   b.fxAgentPath 
				   FROM 
				   `hosts` AS b 
				   WHERE 
				   b.`ipaddress` = '".$host_info['ipaddress']."' LIMIT 0,1";
					$logging_obj->my_error_handler("DEBUG","get_ip_by_domain sql : ".$sql_default_install_path);

					$rst_dft_path = $this->conn->sql_query ($sql_default_install_path);
					$data = $this->conn->sql_fetch_row($rst_dft_path);
					if($data[0])
						 $agentinstallationPath1 = str_replace("/Vx", "/", trim($data[0]));
					else
						$agentinstallationPath1 = str_replace("/Fx", "/", trim($data[1]));
				}
				else
				{
				   $agentinstallationPath1 = $host_info['agentinstallationPath'];
				}
				$logging_obj->my_error_handler("DEBUG","get_ip_by_domain agentinstallationPath1 : ".$agentinstallationPath1);   		   
			   
				$domainip[$i][3] = $agentinstallationPath1;
				$domainip[$i][4] = strval($host_info['jobId']);
				$domainip[$i][5] = $host_info['state'];
				
				/*
					If process server is greater than or equal to 9.13 version send the additional contract data details to push service.
				*/
				$ps_compatibility_no = $process_server_host_info[$host_info['pushServerId']]['compatibilityNo'];
				
				if ($ps_compatibility_no >= $PUSH_USING_VSPHERE_API_VERSION)
				{
					$inv_data = array();
					$inv_sql = "select iv.displayName, iiv.hostType, iiv.hostIp,iiv.accountId from infrastructureInventory iiv, infrastructureHosts ih, infrastructureVms iv where ih.infrastructureHostId = iv.infrastructureHostId and ih.inventoryId = iiv.inventoryId and iv.infrastructureVMId = ?";
					$inv_data_arr =  array($host_info['infrastructureVmId']);
					$inv_data = $this->conn->sql_query ($inv_sql, $inv_data_arr);
					foreach ($inv_data as $key => $inv_data_info) 
					{
						$host_type = $inv_data_info['hostType'];
						$inventory_ip = $inv_data_info['hostIp'];
						$display_name = $inv_data_info['displayName'];
						$account_id = $inv_data_info['accountId'];
					}
					$domainip[$i][6] = $host_type;
					if ($domainip[$i][6]){}else{$domainip[$i][6] = "";}
					
					foreach ($account_data as $acckey=>$accvalue)
					{
						if ($accvalue["accountId"] == $account_id)
						{
							$domainip[$i][7] = $accvalue['userName'];
							if ($domainip[$i][7]){}else{$domainip[$i][7] = "";}
							$domainip[$i][8] = $accvalue['password'];
							if ($domainip[$i][8]){}else{$domainip[$i][8] = "";}
						}
					}
					if ($domainip[$i][7]){}else{$domainip[$i][7] = "";}
					if ($domainip[$i][8]){}else{$domainip[$i][8] = "";}
					$domainip[$i][9] = $inventory_ip;
					if ($domainip[$i][9]){}else{$domainip[$i][9] = "";}
					$domainip[$i][10] = $display_name;
					if ($domainip[$i][10]){}else{$domainip[$i][10] = "";}
					$domainip[$i][11] = (string) $host_info['accountId'];
					if ($domainip[$i][11]){}else{$domainip[$i][11] = "";}
					$domainip[$i][12] = (string) $account_id;
					if ($domainip[$i][12]){}else{$domainip[$i][12] = "";}
					$api_res_data = array();
					$api_sql = "select activityId, clientRequestId, srsActivityId from apiRequest where (requestType = ? or requestType = ?) and requestXml like ? order by apiRequestId desc limit ?";
					$data_arr =  array("InstallMobilityService","UpdateMobilityService","%".$host_info['infrastructureVmId']."%",1);				
					$api_res_data = $this->conn->sql_query ($api_sql, $data_arr);
					foreach ($api_res_data as $key => $api_request_info) 
					{
						$domainip[$i][13] = $api_request_info['activityId'];
						if ($domainip[$i][13]){}else{$domainip[$i][13] = "";}
						$domainip[$i][14] = $api_request_info['clientRequestId'];
						if ($domainip[$i][14]){}else{$domainip[$i][14] = "";}
						$domainip[$i][15] = $api_request_info['srsActivityId'];
						if ($domainip[$i][15]){}else{$domainip[$i][15] = "";}
					}
				}
				$i++;
			}
			$logging_obj->my_error_handler("DEBUG","get_ip_by_domain A : ".print_r($domainip,TRUE));
			return($domainip);
		}	
		else
		{
			$logging_obj->my_error_handler("EXCEPTION","Encryption key not found for get Agent details get_ip_by_domain ");
			return '';
		}		
	}
	
	private function push_command($job_id,$os_string,$installType)
	{
		if($os_string == "Win")
		{
			$os_type = 1;
		}
		else
		{
			$os_type = 2;
		}
		$sql = "SELECT DISTINCT 
				   a.agentinstallationPath,
				   b.cs_ip,
				   b.port,
				   b.agentFeature,
				   b.agentType,
				   a.rebootRequired,
				   b.communicationMode
			FROM 
					`agentInstallerDetails` AS a, 
					`agentInstallers` AS b 
			WHERE 
					a.`ipaddress` = b.`ipaddress` AND
					b.`osType` IN ($os_type) AND 
					a.`jobId` = '$job_id'";
		$result_set = $this->conn->sql_query ($sql);
		while ($data_obj = $this->conn->sql_fetch_object($result_set))
		{
			$installpath = $data_obj->agentinstallationPath;
			$CXIP = $data_obj->cs_ip;
			$CXPort = $data_obj->port;
			$type = $data_obj->agentFeature;
			$agentType = $data_obj->agentType;
			$reboot = $data_obj->rebootRequired;
			$communication_mode = strtolower($data_obj->communicationMode);
			if($reboot == 1)
			{
				$reboot = "Y";
			}
			else
			{
				$reboot = "N";
			}
			$service_type = "";
			if($os_type == 1)
			{
				$Role = "MS";
				if($type == "MasterTarget")
				{
					$Role = "MT";
				}
				$reboot_option = ($reboot == "Y")?"/restart y":"/norestart";
				if($installType == 0)
				{
						$command = " /Role ".'"'.$Role.'"'." /Platform VmWare /Silent ";
				}
				elseif($installType == 1)
				{					
					$command = ' /Platform VmWare /Silent';
				}
				elseif($installType == 2)
				{
					$command = '-au y /LogFilePath="C:\\remoteinstall\\pushInstall.log" /VERYSILENT /SUPPRESSMSGBOXES '.$reboot_option;
				}
			}
			else
			{
				$Role = "MS";
				if($type == "MasterTarget")
				{
					$Role = "MT";
				}
				if($installType == 0)
				{					
					$command = "-r ".'"'.$Role.'"'." -v VmWare -q";
				}
				elseif($installType == 1)
				{
					$command = " -v VmWare -q";					
				}
				elseif($installType == 2)
				{
					#$command = "-L /tmp/remoteinstall/pushInstall.log -R ".$reboot;
					$command = " ";
				}
			}
		}
		return $command;
   }
   
   /*
    * Function Name: get_install_details
    *
    * Description: 
    * Responcible to sending installtion details
    *
    * Parameters:
    *     Param 1 [IN]:JOB ID
    *     Param 2 [IN]: Host ID
    *     Param 3 [OUT]:Data
    *
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
	public function get_install_details($job_id=0,$host_id="",$os_type) 
	{
		global $INSTALLER_PATH;
		global $logging_obj;  
		$Arr = array();    
		if(($this->check_upgrade_flag($job_id)) == 2 )
		{
			$dir = "sw/patches";   
		}
		else if($job_id)
		{
			$dir = "sw";
	 	}
	 	else
	 	{
	 		$dir = $INSTALLER_PATH;
	 	}
		if(is_dir($dir))
		{	    
			if($dirH=opendir($dir))
			{
				while (($file = readdir($dirH)) !== false) 
				{
					$fileNames[]=$file;
				}	
			}
		}
		clearstatcache();
		$i=0;
		$data1= array();
		$cnt  = count($fileNames);
		$temp = "";
		$flag = 0;
		
		// upgrade_flag == 0 (Fresh Install), upgrade_flag == 1 (Upgrade), upgrade_flag == 2 (patches)
		$upgrade_flag = $this->check_upgrade_flag($job_id);
		$build_name = $this->conn->sql_get_value("agentInstallerDetails", "build", "jobId = ?", array($job_id));
		
		$data[$i][0] = 0;
		$data[$i][1] = "";
		$data[$i][2] = "BuildIsNotAvailableAtCX";							
		$data[$i][3] = "";
		$data[$i][4] = (int) $upgrade_flag;
		
		$data[$i][5] = $this ->push_command($job_id,$os_type,$data[$i][4]); 
		$data[$i][6] = array(0 =>"frsvc"); 
		
		$telemetry_log["BuildDetails"] = $data;
		$logging_obj->my_error_handler("INFO",$telemetry_log,Log::BOTH);
		
		if (count($fileNames)>0) 
		{
			foreach ($fileNames as $k=>$v)
			{
				if (($v!='.')&&($v!='..')) 
				{
					if ((preg_match('/exe/i', $v)) || (preg_match('/tar.gz/i', $v)))
					{		
						
						if (!preg_match('/CX/i', $v)  && (preg_match('/'.trim($os_type).'/i', $v) || ($dir == "sw/patches")) && !preg_match('/Win_/i', $v))
						{
							if ((preg_match('/UnifiedAgent/i', $v)) || (preg_match('/UA/i', $v)) || ((preg_match('/FX/i', $v)) && (preg_match('/HP/i', $os_type))) || ((preg_match('/FX/i', $v)) && (preg_match('/AIX/i', $os_type))) || (($this->check_upgrade_flag($job_id)) == 1 ) || (($this->check_upgrade_flag($job_id)) == 2))
							{				
								$flag1 = $this->check_upgrade_flag($job_id);
								$logging_obj->my_error_handler("DEBUG","Flag :: $flag1");
								$path = explode("_",$v);
								$build_version_data = explode(".",$path[2]);
								if($os_type == "Win")
								{
									$build = explode(".",$path[5]);					
								}
								else
								{
									$build = explode(".",$path[6]);
								}
								$build_date = substr($build[0],-9,2);
								$build_month = substr($build[0],-7,3);
								$build_year = substr($build[0],-4);
								if(!is_numeric($build_date))
								{
									$build_date = substr($build_date, 0,1);
								}				
								$date_array = array('Jan'=>1,'Feb'=>2,'Mar'=>3,'Apr'=>4,'May'=>5,'Jun'=>6,'Jul'=>7,'Aug'=>8,'Sep'=>9,'Oct'=>10,'Nov'=>11,'Dec'=>12);
								if($this->check_upgrade_flag($job_id) == 2 || $this->check_upgrade_flag($job_id) == 1)
								{
								   $final_build = $this->get_upgrade_build($os_type,$job_id);
								}
								else
								{
									$temp_data = explode("_",$temp);
									if($os_type == "Win")
									{
										$build = explode(".",$temp_data[5]);	
									}
									else
									{
										$build = explode(".",$temp_data[6]);	
									}
									$temp_date = substr($build[0],-9,2);
									$temp_month = substr($build[0],-7,3);
									$temp_year = substr($build[0],-4);
									if(!is_numeric($temp_date))
									{
										$temp_date = substr($temp_date, 0,1);
									}
									$temp_version = explode(".",$temp_data[2]);
									if($temp_version[0] < $build_version_data[0])
									{
										$final_build = $v;
									}
									elseif($temp_version[0] == $build_version_data[0])
									{
										if($temp_version[1] < $build_version_data[1])
										{
											$final_build = $v;
										}
										elseif($temp_version[1] == $build_version_data[1])
										{
											if(($temp_year <= $build_year) && ($date_array[$temp_month] <= $date_array[$build_month]))
											{
												if(($temp_date <= $build_date))
												{
													$final_build = $v;
												}
											}
										}
									}
									$temp = $final_build;
								}
								
								$data[$i][2] = $dir."/".$final_build;
								$build_name = $final_build;
								//0 For Install
								//1 For Upgrade
								//2 For Patches
								$build_path = explode("/",$data[$i][2]);				

							
								if($this->check_upgrade_flag($job_id) == 2)
								   $this->set_pushclient_buildname($job_id,$this->get_upgrade_build($os_type,$job_id));
								else
								   $this->set_pushclient_buildname($job_id,$build_path[1]);
							}
						}
					}
				}
			}
			  
		}
		
		$telemetry_log_data["BuildPath"] = $build_path;
		$logging_obj->my_error_handler("INFO",$telemetry_log_data,Log::APPINSIGHTS);
       	
		$first_arg=0;
		$os_type=intval(1);
		array_push($Arr,$first_arg, $os_type,$data);
		if($job_id)
		{
			return $Arr;		
		}
		else
		{
			return $build_name;
		}
	}

   /*
    * Function Name: update_agent_installation_status
    *
    * Description: 
    * Responcible to updating agent installation log messages & status
    * During Installtion
    *
    * Parameters:
    *     Param 1 [IN]: Job id
    *     Param 2 [IN]: state
    *     Param 3 [IN]: install status messadge
    *     Param 4 [IN]: host id
    *     Param 5 [IN]: Log Messadge
    *     Param 6 [OUT]: Data
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */

	public function update_agent_installation_status($job_id,$state,$install_status_message,$host_id="",$log_details = array()) 
	{		
		global $REPLICATION_ROOT, $logging_obj;
   
		$host_id_cond = $end_time_update = "";
		
		$log_message = empty($log_details['message']) ? '' : $log_details['message'];
		$installer_extended_errors = empty($log_details['installerExtendedErrors']) ? '' : $log_details['installerExtendedErrors'];
		$flag = 1;
		
		$telemetry_log["JobId"] = $job_id;
		$telemetry_log["State"] = $state;
		$telemetry_log["InstallStatusMessage"] = $install_status_message;
		$telemetry_log["LogDetails"] = $log_details;
		$telemetry_log["HostId"] = $host_id;
		$logging_obj->my_error_handler("INFO",$telemetry_log,Log::BOTH);
		
		$sql_args = array();
		if($state == 3) 
		{
			$host_id_cond = "hostId = ? , ";
			$sql_args[] = $host_id;
		}
		if(in_array($state, array(3,-3,6,-6))) $end_time_update = "endTime = now(), ";
		
		# adding account name if push install gets failed. DRA will use AccountName place holder for message if required.
		if(in_array($state, array(-3,-6)))
		{
			$get_account_id_sql = "SELECT ai.accountId FROM agentinstallers ai, agentinstallerdetails aid WHERE jobId = ? and ai.infrastructureVmId = aid.infrastructureVmId";
			$accountArray = $this->conn->sql_query($get_account_id_sql, array($job_id));
			
			$account_data = $this->account_obj->getAccountDetails($accountArray[0]['accountId']);
			$accountName = $account_data[0]['accountName'];
			$log_details['placeHolders']['AccountName'] = "$accountName"; 
		}
			
		$error_code =  empty($log_details['errorCode']) ? '' : $log_details['errorCode'];
		$error_place_holders = (empty($log_details['placeHolders']) == false && is_array($log_details['placeHolders'])) ? serialize($log_details['placeHolders']) : serialize(array());

		$sql = "UPDATE 
					agentInstallerDetails 
				SET
					$end_time_update
					$host_id_cond
					lastUpdatedTime = now(),
					state = ?,
					statusMessage = ?,
					errCode = ?,
					errPlaceholders = ?,
					installerExtendedErrors = ?
				WHERE
					jobId = ?
				AND state IN ('1','2','4','5')";
		array_push($sql_args, $state, $install_status_message, $error_code, $error_place_holders, $installer_extended_errors, $job_id);
		
		$result_set = $this->conn->sql_query($sql, $sql_args);				
		$logging_obj->my_error_handler("INFO","update_agent_installation_status sql :: $sql, arguments".print_r($sql_args,true)." \n");
		if(! $this->conn->sql_affected_row())
		{
			$logging_obj->my_error_handler("INFO","update_agent_installation_status sql :: $sql, arguments".print_r($sql_args,true)." \n");
			$logging_obj->my_error_handler("INFO","update_agent_installation_status failed \n");
			$flag = 0;
		}		
		
		$telemetry_logs["Sql"] = $sql;
		$telemetry_logs["Args"] = $sql_args;
		$telemetry_logs["AffectedRows"] = $this->conn->sql_affected_row();
		$logging_obj->my_error_handler("INFO",$telemetry_logs,Log::APPINSIGHTS);
		
		if(in_array($state, array(3,-3,6,-6)))
		{
			$telemetry_data = $this->get_telemetry_flag($job_id);	
	
			$telemetry_log_string["JobId"]=$job_id;
			$telemetry_log_string["HostId"]=$host_id;
			$telemetry_log_string["ipaddress"]=$ipaddress;
			$telemetry_log_string["StartTime"]=$telemetry_data['startTime'];
			$telemetry_log_string["endTime"]=$telemetry_data['endTime'];
			$telemetry_log_string["LastUpdatedTime"]=$telemetry_data['lastUpdatedTime'];
			$telemetry_log_string["StatusMessage"]=$telemetry_data['statusMessage'];
			$logging_obj->my_error_handler("INFO",$telemetry_log_string,Log::APPINSIGHTS);
		}
		
		// -3 installation failure, -6 uninstallation failure.
		if($state == -3 || $state == -6)  
		{
			if ($flag && $log_message)
			{
				$mds_data_array = array();
				$this->mds_obj = new MDSErrorLogging();
				$this->mds_error_obj  = new MDSErrors();
				
				$dependent_log_string = $this->mds_error_obj->GetErrorString("PushInstall", array($job_id));
				
				$mds_log = $log_message;
				if($dependent_log_string) 
				{
					$mds_log .= $dependent_log_string;
					
					$mds_data_array["jobId"] = $job_id;
					$mds_data_array["jobType"] = "PushInstall";
					$mds_data_array["errorString"] = $mds_log;
					$mds_data_array["eventName"] = "Process Server";
					$mds_data_array["errorType"] = "ERROR";
					
					$this->mds_obj->saveMDSLogDetails($mds_data_array);
				}
			}		
		}
		if ($flag && $log_message)	$flag = $this->push_install_agent_log ($job_id, $log_message, $state);
     
		return $flag;
   }

   /*
    * Function Name: push_install_agent_log
    *
    * Description: 
    * Function to responcible for generating logs
    *
    * Parameters:
    *     Param 1 [IN]: Job id
    *     Param 2 [IN]: Log String 
    *     Param 3 [IN]: State
    *     Param 4 [OUT]: status 
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */

   public function push_install_agent_log ($job_id, $LogString, $state)
   {
		global $REPLICATION_ROOT;
		global $INSTALLATION_DIR,$MV,$SLASH,$logging_obj, $commonfunctions_obj;
		$sql = "SELECT
					ipaddress
				FROM 
					agentInstallerDetails
				WHERE
					jobId= ?";
		$result_set = $this->conn->sql_query ($sql, array($job_id));
		foreach($result_set as $rs)
		{
			$ip = $rs['ipaddress'];
			break;
		}
		
		$location = $REPLICATION_ROOT.$SLASH."var".$SLASH."push_install_logs";
		if(!file_exists($location)) 
		{
			mkdir($location,0777);
		}
		if(file_exists($location) && !is_writeable($location))
		{
			chmod($location, 0777);
		}
		if ($state == 3 || $state == 6) 
		{
			$PIINSTALLLOG_FILE = "install.out";
			$old_log_file = "install_".time().".out";
		}
		elseif ($state == -3 || $state == -6)
		{
			$PIINSTALLLOG_FILE = "install.err";
			$old_log_file = "install_".time().".err";
		}
		$ip_dir = $location.$SLASH.strval($ip);
		$jobid_dir  = $ip_dir.$SLASH.strval($job_id);
		$log_dir = $jobid_dir.$SLASH.$PIINSTALLLOG_FILE;
		
		if(!file_exists($ip_dir)) 
		{
			mkdir($ip_dir,0777);
		}
		if(!file_exists($jobid_dir)) 
		{
			mkdir($jobid_dir,0777);
		}
		
		if(file_exists($log_dir))
		{			
			$rename_status = rename($log_dir,$jobid_dir.$SLASH.$old_log_file);
		}
		$log_dir1 = $this->conn->sql_escape_string ($log_dir);
    
		if (file_exists($log_dir)){
			$sql = "UPDATE 
						agentInstallerDetails
					SET 
						logFileLocation = '$log_dir1'
					WHERE 
						jobId = '$job_id'";
		$this->conn->sql_query ($sql);
		}

		$log_dir = strval($log_dir);

		if(! file_exists($jobid_dir))	return 0;	
		
		#$logging_obj->my_error_handler("INFO",array("LogDir"=>$log_dir),Log::APPINSIGHTS);
		$fr = $commonfunctions_obj->file_open_handle($log_dir, "w+");
		if (!$fr)
		{
			return 0;
		}
	
		$file_status_flag = 1;
		if (fwrite($fr, $LogString . "\n"))
		{
			$logging_obj->my_error_handler("INFO","Cannot write to file : $log_dir. \n");
			$file_status_flag = 0;
		}		
		fclose ($fr);
		
		return $file_status_flag;
	}
	
   /*
    * Function Name: push_install_check_status
    *
    * Description: 
    * Function to checks specific host installation state
    *
    * Parameters:
    *     Param 1 [IN]: IP
    *     Param 2 [OUT]: status 
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */

	public function push_install_check_status($ip) {
		$sql = "SELECT
					state
				FROM
					agentInstallerDetails 
				WHERE";
		$sql .=  " ipaddress='$ip'";
		$sql .= " ORDER BY
			lastUpdatedTime";
		$data1  = 0;
		$iter = $this->conn->sql_query( $sql );
		if($this->conn->sql_num_row($iter))
		{
			while($data = $this->conn->sql_fetch_row($iter))
			{
				$data1 = $data[0];
			}
		}
		return $data1;
	}
   
   /*
    * Function Name: get_push_client_path
    *
    * Description: 
    * This function is returns pushever path to download 
    *
    * Parameters:
    *     Param 1 [IN]: os_type
    *     Param 2 [OUT]: return build path 
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */

	public function get_push_client_path($os_type,$job_id)
	{
		global $INSTALLER_PATH, $logging_obj;
		if($job_id)
		{
			$this->set_pushclient_os_details_type($job_id,$os_type);
			$dir = "sw";
		}
		else
		{
			$dir = $INSTALLER_PATH;
		}
		
		$telemetry_log["JobId"] = $job_id;
		$telemetry_log["OStype"] = $os_type;
		$telemetry_log["Dir"] = $dir;
		$logging_obj->my_error_handler("INFO",$telemetry_log,Log::APPINSIGHTS);
		
		if(is_dir($dir))
		{
			if($dirH=opendir($dir))
			{
				while (($file = readdir($dirH)) !== false) 
				{
					$fileNames[]=$file;
				}	
			}
		}
		clearstatcache();
		$i=0;
		$data= array();
		if (count($fileNames)>0) 
		{
			foreach ($fileNames as $k=>$v)
			{
				if (($v!='.')&&($v!='..')) 
				{
					$os_type1 = $os_type."_pushinstallclient";
					if (preg_match('/pushinstallclient/i', $v) && (preg_match('/'.trim($os_type1).'/i', $v))) {
						$data[0] = 0;
						$data[1] = $dir."/".$v;
						break;
					}
				}
			}
		}
		return $data;
	}  
	
	private function set_pushclient_os_details_type($job_id,$os)
	{  
		$sql = "SELECT
					ipaddress
				FROM
					agentInstallerDetails 
				WHERE
					jobId = ?";	
		$data = $this->conn->sql_query($sql, array($job_id));
		if ($data && (count($data)>0))
		{	
			$ip = $data[0]['ipaddress'];
		}
		
		if ($ip)
		{
			$update_sql = "UPDATE 
								agentInstallers 
							SET  
								osDetailType = ?
							WHERE
								ipaddress = ?";
			$result_set = $this->conn->sql_query($update_sql, array($os,$ip));
			
			//If affected rows count is greater than zero.
			if ($result_set > 0)
			{
				return 1;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}   
   
	private function set_pushclient_buildname($job_id,$buildname){
      
		$update_sql = "UPDATE 
							agentInstallerDetails 
						SET  
							build = ?
						WHERE
							jobId= ?";
		$result_set = $this->conn->sql_query ($update_sql, array($buildname, $job_id));
		return 1;
	}
   
	private function check_upgrade_flag($jobid)
	{
		if($jobid)
		{
			$sql = "SELECT upgradeRequest FROM agentInstallerDetails WHERE jobId = ?";
			$rs = $this->conn->sql_query($sql, array($jobid));
			foreach ($rs as $data)
			{
				$upgrade_flg = $data['upgradeRequest'];
				return $upgrade_flg;
			}
		}
		else
		{
			return 0;
		}
   }
   
	private function  get_upgrade_build($os_type,$job_id)
	{
		$sql = "SELECT
					build
				FROM 
					agentInstallerDetails 
				WHERE
					jobId = ? ";
		$result =  $this->conn->sql_query ($sql, array($job_id));
		foreach($result as $rs)
		{
			$upgrade_build = $rs['build'];
			break;
		}
		return $upgrade_build;
	}
		
	public function update_api_push_install_state($apiRequestDetailId,$state)
	{
		$update_query = "UPDATE 
							apiRequestDetail
						SET
							state = '$state'
						WHERE
							apiRequestDetailId = '$apiRequestDetailId'";
							  
		$update_result = $this->conn->sql_query( $update_query );
		return ($update_result)?1:0;
	}
	
	public function get_ip_by_api_request_id($requestId)
	{
		$select_id = "SELECT
						ipaddress
					FROM
						agentInstallers
					WHERE
						apiRequestId = '$requestId'";
		$reult_sql = $this->conn->sql_query( $select_id );
		$data = $this->conn->sql_fetch_object($reult_sql);
		return $data->ipaddress;
	}
	
	public function push_install_status_message($ip) 
	{
		$sql = "SELECT
				  statusMessage
			  FROM
				  agentInstallerDetails 
			  WHERE";
		  
		$sql .=  " ipaddress='$ip'";

		$sql .= " ORDER BY
		  lastUpdatedTime";
		$iter = $this->conn->sql_query( $sql );
		while($obj = $this->conn->sql_fetch_object($iter))
		{
			$statusMessage = $obj->statusMessage;
		}

		return $statusMessage;
	}

    public function isMasterTarget($host_ip)
	{
        $return_value = 0;
        $agent_role = $this->conn->sql_get_value('hosts', 'agentRole', "ipaddress = '$host_ip'");
        if($agent_role == "MasterTarget")
        {
            $return_value = 1;
        }
        return $return_value;
	}
	
	public function get_build_path($os_type , $job_id) 
	{
		global $logging_obj; 
		
		$build_name = "BuildIsNotAvailableAtCX";
		if(($this->check_upgrade_flag($job_id)) == 2 )
		{
			$dir = "sw/patches";   
			$build_name = $this->conn->sql_get_value("agentInstallerDetails", "build", "jobId = ?", array($job_id));
		}	
		else
		$dir = "sw";
		if(is_dir($dir))
		{	    
			if($dirH=opendir($dir))
			{
				while (($file = readdir($dirH)) !== false) 
				{
					$fileNames[]=$file;
				}	
			}
		}
		clearstatcache();
		$i=0;
		$data = array();
		$data[0] = 0;
		$temp = "";
		$flag = 0;
		
		$telemetry_log["CountOfFileNames"] = count($fileNames);
		$telemetry_log["OsType"] = $os_type;
		$telemetry_log["JobId"] = $job_id;
		$logging_obj->my_error_handler("INFO",$telemetry_log,Log::BOTH);
		
		if (count($fileNames)>0) 
		{
			foreach ($fileNames as $k=>$v)
			{
				if (($v!='.')&&($v!='..') && (!preg_match('/pushinstallclient/i', $v))) 
				{
					if ((preg_match('/exe/i', $v)) || (preg_match('/tar.gz/i', $v)))
					{		
						
						if (!preg_match('/CX/i', $v)  && (preg_match('/'.trim($os_type).'/i', $v) || ($dir == "sw/patches")) && !preg_match('/Win_/i', $v))
						{
							if ((preg_match('/UnifiedAgent/i', $v)) || (preg_match('/UA/i', $v)) || ((preg_match('/FX/i', $v)) && (preg_match('/HP/i', $os_type))) || ((preg_match('/FX/i', $v)) && (preg_match('/AIX/i', $os_type))) || (($this->check_upgrade_flag($job_id)) == 1 ) || (($this->check_upgrade_flag($job_id)) == 2))
							{				
								$path = explode("_",$v);
								$build_version_data = explode(".",$path[2]);
								if($os_type == "Win")
								{
									$build = explode(".",$path[5]);					
								}
								else
								{
									$build = explode(".",$path[6]);
								}
								$build_date = substr($build[0],-9,2);
								$build_month = substr($build[0],-7,3);
								$build_year = substr($build[0],-4);
								if(!is_numeric($build_date))
								{
									$build_date = substr($build_date, 0,1);
								}				
								$date_array = array('Jan'=>1,'Feb'=>2,'Mar'=>3,'Apr'=>4,'May'=>5,'Jun'=>6,'Jul'=>7,'Aug'=>8,'Sep'=>9,'Oct'=>10,'Nov'=>11,'Dec'=>12);
								if($this->check_upgrade_flag($job_id) == 2 || $this->check_upgrade_flag($job_id) == 1)
								{
								   $final_build = $this->get_upgrade_build($os_type,$job_id);
								}
								else
								{
									$temp_data = explode("_",$temp);
									if($os_type == "Win")
									{
										$build = explode(".",$temp_data[5]);	
									}
									else
									{
										$build = explode(".",$temp_data[6]);	
									}
									$temp_date = substr($build[0],-9,2);
									$temp_month = substr($build[0],-7,3);
									$temp_year = substr($build[0],-4);
									if(!is_numeric($temp_date))
									{
										$temp_date = substr($temp_date, 0,1);
									}
									$temp_version = explode(".",$temp_data[2]);
									if($temp_version[0] < $build_version_data[0])
									{
										$final_build = $v;
									}
									elseif($temp_version[0] == $build_version_data[0])
									{
										if($temp_version[1] < $build_version_data[1])
										{
											$final_build = $v;
										}
										elseif($temp_version[1] == $build_version_data[1])
										{
											if(($temp_year <= $build_year) && ($date_array[$temp_month] <= $date_array[$build_month]))
											{
												if(($temp_date <= $build_date))
												{
													$final_build = $v;
												}
											}
										}
									}
									$temp = $final_build;
								}
								
								$data[1] = $dir."/".$final_build;								
							}
						}
					}
				}
			}
		}
		if(!$data[1]) $data[1] = $build_name;
		
		$telemetry_log["BuildPath"] = $data;
		$logging_obj->my_error_handler("INFO",$telemetry_log,Log::BOTH);
		return $data;
	}
	
	/*		
   * Function Name: add_push_install_request
   *
   * Description:
   *    This function is responsible to add installtion request
   *    using API InstallMobilityService
   *
   * Parameters:
   * Return Value: [IN] Agent ip
   * Return Value: [IN] Agent password
   * Return Value: [IN] Agent os type
   * Return Value: [IN] Agent user name
   * Return Value: [IN] Agent Host name
   * Return Value: [OUT] AgentInstallerIds
   *
   * Exceptions:
   */   
   public function add_push_install_request($install_details){
     
		global $HOST_GUID, $WINDOW_AGENT_INSTALL_PATH, $LINUX_AGENT_INSTALL_PATH;       
		
		$installer_details = array();
		
		foreach ($install_details as $server_identity => $server_details)			
		{
			$host_ip 	 	= 	$server_details['IPAddress'];
			$host_name 	 	= 	$server_details['HostName'];
			$account_id 	= 	$server_details['AccountId'];
			$user_name 	 	= 	$server_details['UserName'];
			$password	 	= 	$server_details['Password'];
			$os_type 	 	= 	$server_details['OSType'];
			$os_flag 	 	= 	$server_details['OSFlag'];
			$agent_feature	= 	$server_details['AgentType'];
			$push_server_id	= 	$server_details['PushServerId'];
			$cs_ip			= 	$server_details['CSIP'];
			$cs_port		= 	$server_details['CSPort'];
			$communication_mode	= 	$server_details['Protocol'];
			$infrastructure_vm_id = $server_details['InfrastructureVmId'];
			$update_type	= 	isset($server_details['UpdateType']) ? $server_details['UpdateType'] : "";
			$domain 	 = 	($os_flag == 1) ? $server_details['Domain'] : "";
			$install_dir = 	($os_flag == 1) ? $WINDOW_AGENT_INSTALL_PATH : $LINUX_AGENT_INSTALL_PATH;
			
			$installation_exists = $this->conn->sql_get_array("SELECT jobId,ipaddress  FROM agentinstallerdetails WHERE ipaddress = ? AND (state = 1 OR state = 2) ", "ipaddress","jobId", array($host_ip));
			
			if(!count($installation_exists))
			{
				# Default values
				$reboot_required = 0;
				$version = ""; // Push Client picks the latest version build if empty.
				$upgrade_flag = ($update_type) ? (($update_type == "Upgrade") ? 1 : 2) : 0;
				$parallel_push_count = 5;
				$agent_type = "UA";
				$buildName = ""; // If empty, Push Client picks the build based on OS, Latest Version available
				$apiRequestId = 0;
				$sql_args = array();
									
				$host_ip_exists = $this->conn->sql_get_value("agentInstallers", "ipaddress", "ipaddress = ?", array($host_ip));
				
				
				$install_query = ($host_ip_exists) ? "UPDATE " : "INSERT INTO ";
				$install_query .= " `agentInstallers` 
									SET
										`osType` = ?,
										`hostName` = ?,
										`osDetailType` = ?,
										`cs_ip` = ?,
										`port` = ?,
										`agentFeature` = ?,
										`parllelPushCount` = ?,
										`agentType`	= ?,
										`pushServerId` = ?,
										`apiRequestId` = ?,
										`communicationMode` = ?,
										`infraStructureVmId` = ?";
				array_push($sql_args, $os_flag, $host_name, $os_type, $cs_ip, $cs_port, $agent_feature, $parallel_push_count, $agent_type, $push_server_id, $apiRequestId, $communication_mode, $infrastructure_vm_id);
				if($account_id)
				{
					$install_query .= ($account_id) ? " ,accountId = ? " : "";
					array_push($sql_args, $account_id);
				}
				if($user_name)
				{
					$username_str = "HEX(AES_ENCRYPT(?, ?))";
					$install_query .= ($username_str) ? " ,`userName` = IF(? = '', ?, {$username_str}) " : "";
					array_push($sql_args, $user_name, $user_name, $user_name, $this->encryption_key);
				}
				if($password)
				{
					$password_str = "HEX(AES_ENCRYPT(?, ?))";
					$install_query .= ($password_str) ? ", `password` = {$password_str}" : "";
					array_push($sql_args, $password, $this->encryption_key);
				}
				if($domain)
				{
					$domain_str = "HEX(AES_ENCRYPT(?, ?))";
					$install_query .= ($domain_str) ? ", `Domain` = IF(? = '', ?, {$domain_str})" : "";
					array_push($sql_args, $domain, $domain, $domain, $this->encryption_key);
				}
				
				$install_query .= ($host_ip_exists) ? " WHERE ipaddress = ?" : ", ipaddress = ?";
				array_push($sql_args, $host_ip);
				
				$agent_installer_id = $this->conn->sql_query($install_query, $sql_args);
				
				$existing_agent_installer_details_id = $this->conn->sql_get_value("agentInstallerDetails", "jobId", "ipaddress = ? AND state IN ('0', '-3')", array($host_ip));
				if($upgrade_flag)
				{
					/* Fetching Patch/ upgrade details */
					$actual_patch_verison = $server_details['PatchVersion'];
					$reboot_required = $server_details['RebootRequired'];
					$buildName = $server_details['BuildName'];
				}
				
				$installer_details_sql = ($existing_agent_installer_details_id) ? "UPDATE " : "INSERT INTO ";
				$installer_details_sql .= "	`agentInstallerDetails`
										   SET
											`agentInstallerLocation` = ?,
											`state` = '1',
											`statusMessage` = '',
											`startTime` = now(),
											`lastUpdatedTime` = now(),
											`agentinstallationPath` = ?,
											`rebootRequired` = ?,
											`upgradeRequest` = ?,
											`build` = ?,
											`infraStructureVmId` = ?";
				$installer_details_sql .= ($existing_agent_installer_details_id) ? " WHERE jobId = ? AND state IN ('0', '-3')" : ", ipaddress = ?";
				$sql_conditional_arg = ($existing_agent_installer_details_id) ?  $existing_agent_installer_details_id : $host_ip;
				$agent_installer_details_id = $this->conn->sql_query($installer_details_sql, array($this->agent_installer_path, $install_dir, $reboot_required, $upgrade_flag, $buildName,$infrastructure_vm_id, $sql_conditional_arg));
				$installer_details[$server_identity] = ($existing_agent_installer_details_id) ? $existing_agent_installer_details_id : $agent_installer_details_id;
			}
			else{
				$installer_details[$server_identity] = (int) $installation_exists[$host_ip];	
			}			
		}		
		return $installer_details;
	}
	
    /*		
   * Function Name: getInstallStatus
   *
   * Description:
   *    This function is responsible to get the installation
   *    status for the push install request
   *
   * Parameters:
   * Return Value: [IN] Jobs Ids ( Unique identity to the installation table)
   * Return Value: [OUT] Details along with the status of the jobids
   *
   * Exceptions:
   */   
	public function getInstallStatus($job_ids)
	{
		$job_ids_str = implode(",", $job_ids);
		$get_status_sql = "	SELECT 
								jobId,
								ipaddress,
								state,
								statusMessage,
								UNIX_TIMESTAMP(lastUpdatedTime) as lastUpdatedTime,
								hostId,
								logFileLocation,
								errCode,
								errPlaceHolders,
								installerExtendedErrors	
							FROM
								agentInstallerDetails
							WHERE
								FIND_IN_SET(jobId, ?)";
		$install_status = $this->conn->sql_query($get_status_sql, array($job_ids_str));
		$status_details = array();
		foreach ($install_status as $server_status)
		{
			$status_details[$server_status['jobId']] = $server_status;
		}
		return $status_details;
	}
	
	/*		
   * Function Name: add_push_uninstall_request
   *
   * Description:
   *    This function is responsible to add installtion request
   *    using API InstallMobilityService
   *
   * Parameters:
   * Return Value: [IN] Agent ip
   * Return Value: [IN] Agent password
   * Return Value: [IN] Agent user name
   * Return Value: [IN] Push Server ID
   * Return Value: [OUT] AgentInstallerIds
   *
   * Exceptions:
   */   
   public function add_push_uninstall_request($install_details)
   {     
		global $HOST_GUID, $logging_obj;
		
		$installer_details = array();
		foreach ($install_details as $server_identity => $server_details)			
		{
			$uninstall_sql_args = array();
			
			$host_ip 	 	= 	$server_details['IPAddress'];
			$os_flag 	 	= 	$server_details['OS'];
			$account_id 	= 	$server_details['AccountId'];
			$user_name 	 	= 	$server_details['UserName'];
			$password	 	= 	$server_details['Password'];
			$push_server_id	= 	$server_details['PushServerId'];				
			
			$installation_exists = $this->conn->sql_get_array("SELECT jobId,ipaddress  FROM agentinstallerdetails WHERE ipaddress = ? AND (state = 4 OR state = 5) ", "ipaddress","jobId", array($host_ip));
			
			if(!count($installation_exists))
			{
				$host_ip_exists = $this->conn->sql_get_value("agentInstallers", "ipaddress", "ipaddress = ?", array($host_ip));
				
				$uninstall_query = ($host_ip_exists) ? "UPDATE " : "INSERT INTO ";
				$uninstall_query .= " `agentInstallers` SET ";
				
				if(!$host_ip_exists)
				{
					$uninstall_query .= (count($uninstall_sql_args)) ? ", osType = ?, domain = '' " : "osType = ?, domain = '' ";
					$uninstall_sql_args[] = $os_flag;
				}				
				if($account_id)
                {
                    $uninstall_query .= (count($uninstall_sql_args)) ? " , accountId = ?, userName = '', password = '' " : " accountId = ? ";
					$uninstall_sql_args[] = $account_id;
                }
				if($user_name)
				{
					$username_str = "HEX(AES_ENCRYPT(?, ?))";	
					$uninstall_query .= (count($uninstall_sql_args)) ? ", userName = {$username_str}" : "userName = {$username_str}";
					array_push($uninstall_sql_args, $user_name, $this->encryption_key);
				}			
				if($password)
				{
					$password_str = "HEX(AES_ENCRYPT(?, ?))";	
					$uninstall_query .= (count($uninstall_sql_args)) ? ", password = {$password_str}" : "password = {$password_str}";
					array_push($uninstall_sql_args, $password, $this->encryption_key);
				}			
				if($push_server_id)
				{
					$uninstall_query .= (count($uninstall_sql_args)) ? ", pushServerId = ?" : "pushServerId = ?";
					$uninstall_sql_args[] = $push_server_id;
				}		
				
				if(!$host_ip_exists || ($host_ip_exists && count($uninstall_sql_args)))
				{				
					$ip_query_string = "ipaddress = ?";
					$cond = (count($uninstall_sql_args)) ? ", ".$ip_query_string : "";
					$uninstall_query .= ($host_ip_exists) ? " WHERE ".$ip_query_string : $cond;
					$uninstall_sql_args[] = $host_ip;
					$this->conn->sql_query($uninstall_query, $uninstall_sql_args);
				}
				
				$existing_agent_installer_details_id = $this->conn->sql_get_value("agentInstallerDetails", "jobId", "ipaddress = ?", array($host_ip));
				
				$installer_details_sql = ($existing_agent_installer_details_id) ? "UPDATE " : "INSERT INTO ";
				$installer_details_sql .= "	`agentInstallerDetails`
										   SET
											`state` = '4',
											`statusMessage` = '',
											`startTime` = now(),
											`lastUpdatedTime` = now(),
											`agentinstallationPath` = ?,
											`rebootRequired` = ?,
											`upgradeRequest` = ?";
				$installer_details_sql .= ($existing_agent_installer_details_id) ? " WHERE jobId = ?" : ", ipaddress = ?";
				$sql_conditional_arg = ($existing_agent_installer_details_id) ?  $existing_agent_installer_details_id : $host_ip;
				$agent_installer_details_id = $this->conn->sql_query($installer_details_sql, array($install_dir, $reboot_required, $upgrade_flag, $sql_conditional_arg));
				$installer_details[$server_identity]['jobId'] = ($existing_agent_installer_details_id) ? $existing_agent_installer_details_id : $agent_installer_details_id;
			}
			else{
				$installer_details[$server_identity]['jobId'] = (int)$installation_exists[$host_ip];
			}			
			$installer_details[$server_identity]['hostId'] = $server_details['HostId'];
		}
		
		return $installer_details;
	}
	
	function get_host_info($host_ids)
	{
		$host_id_array = array();
		
		if(!is_array($host_ids)) $host_id_array[] = $host_ids;
		else $host_id_array = $host_ids;
		
		$host_ids_str = implode(",", $host_id_array);
		
		$host_sql = "SELECT
						id,
						ipAddress,
						osFlag
					FROM
						hosts
					WHERE
						FIND_IN_SET(id, ?)";
		$host_details = $this->conn->sql_query($host_sql, array($host_ids_str));
		
		$host_info = array();
		foreach ($host_details as $host)
		{
			$host_info[$host['id']] = $host;
		}
		
		return $host_info;
	}
	
	public function validatePS($push_server_id)
	{
		return $this->conn->sql_get_value("pushProxyHosts","id","id = ?", array($push_server_id));
	}
	
	function get_telemetry_flag($job_id)
	{
		$sql = "SELECT
						startTime,
						endTime,
						lastUpdatedTime,
						statusMessage,
						ipaddress,
						telemetry						
				FROM 
						agentinstallerdetails 
				WHERE
						jobId = ? ";
		$rs = $this->conn->sql_query($sql, array($job_id));
		
		$info = array();
		foreach ($rs as $data)
		{
			$info['startTime'] = $data['startTime'];
			$info['endTime'] = $data['endTime'];
			$info['lastUpdatedTime'] = $data['lastUpdatedTime'];
			$info['statusMessage'] = $data['statusMessage'];
			$info['ipaddress'] = $data['ipaddress'];
			$info['telemetry'] = $data['telemetry'];
		}
		
		return $info;
	}
	
	function update_telemetry_flag($jobIds)
	{
		$jobIds = array_unique($jobIds);		
		$jobid_str = implode(",",$jobIds);
		
		$sql = "UPDATE 
					agentInstallerDetails 
				SET
					telemetry = 1
				WHERE
					(FIND_IN_SET(jobId, ?))";
		
		$result_set = $this->conn->sql_query($sql, array($jobid_str));
   }
	
};
?>