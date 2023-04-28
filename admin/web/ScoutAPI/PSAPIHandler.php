<?php
/*
 *$Header: /home/svsystems/system/admin/PSAPIHandler.php,
 *================================================================= 
 * FILENAME
 *    PSAPIHandler.php
 *
 * DESCRIPTION
 *    This Class contains functions related to Suport CX related opertaion for API call  
 *
 * HISTORY
 *     28-May-2013  Madhavi    
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/

class PSAPIHandler extends ResourceHandler 
{	

	/**     * conn_obj private variable.    */	
	private $conn_obj;	
	
	/**     * commonfunctions_obj private variable.    */	
	private $commonfunctions_obj;	
	private $ps_obj;	
	private $processServer_obj;	
	
	/**     * Volume protection object.    */
	private $vol_obj;
	
	/**     * apiName private variable.    */	
	private $apiName;		
	private $inputData;	
	
	/*		constructor to intialise objects	*/	
	public function PSAPIHandler($apiName='', $inputData='')
	{				
		global $conn_obj;
		$this->conn_obj = $conn_obj;
		$this->commonfunctions_obj = new CommonFunctions();
		$this->batchsql_obj = new BatchSqls();		
		$this->ps_obj = new PSAPI();		
		$this->processServer_obj = new ProcessServer();
		$this->vol_obj = new VolumeProtection();
	}
	
	public function GetPSSettings_validate($identity, $version, $functionName, $parameters) 
	{		
		$host_obj = $parameters['HostId'];
		
		if(!is_object($host_obj))
		{
			$message = "HostId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$host_id = $host_obj->getValue();
		$host_id = trim($host_id);
		if($host_id == "")
		{	
			$message = "hostId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($host_id);
		if ($guid_format_status == false)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$is_ps = $this->conn_obj->sql_get_value("hosts","id","id = '$host_id' AND (csEnabled = '1' OR processServerEnabled = '1')");
		if(!$is_ps)
		{
			$message = "Not a Valid Process Server hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
	}		

	public function GetPSSettings($identity, $version, $functionName, $parameters)
	{	
		// fetching the pair details with the corresponding process server id.	
		$host_id = $parameters['HostId']->getValue();
		$response = new ParameterGroup("Response");
		
		//fetching pair details
		$pair_info = $this->ps_obj->getPairData($host_id);
		$pair_response = $this->set_parameter_name($pair_info[0],"pair_data");
		if($pair_info[1]) $protected_host_id = implode("','",$pair_info[1]);
		if($pair_info[2]) $protected_src_vols = implode("','",$pair_info[2]);
		if($pair_info[3]) $protected_tgt_vols = implode("','",$pair_info[3]);
		if($pair_info[4]) $phy_lun_ids = implode("','",$pair_info[4]);
		if($pair_info[5]) $plan_ids = implode("','",$pair_info[5]);
		if($pair_info[6]) $one_to_many_pair_res = $this->set_parameter_name($pair_info[6],"one_to_many_targets");
		if($pair_response != '') $response->addParameterGroup($pair_response);
		if($one_to_many_pair_res !='') $response->addParameterGroup($one_to_many_pair_res);
		
		//fetching the cluster ha details
		$app_node_info = $this->ps_obj->getClusterHA();
		$app_node_response = $this->set_parameter_name($app_node_info[0],"appliance_node_data");
		if($app_node_response != '') $response->addParameterGroup($app_node_response);
		
		$app_clus_response = $this->set_parameter_name($app_node_info[1],"app_clus_data");
		if($app_clus_response != '') $response->addParameterGroup($app_clus_response);		
		
		// fetching the host details with the protected hostids		
		$host_data = $this->ps_obj->getHostData();		
		$host_response = $this->set_parameter_name($host_data,"host_data");	
		if($host_response != '') $response->addParameterGroup($host_response);	
		
		// fetching the lv details with the protected hostids
		$lv_data = $this->ps_obj->getLVData($protected_host_id, $protected_src_vols,$protected_tgt_vols);		
		$lv_response = $this->set_parameter_name($lv_data,"lv_data");	
		if($lv_response != '') $response->addParameterGroup($lv_response);
		
		// fetching complete cluster details
		$cluster_data = $this->ps_obj->getClustersData();
		$cluster_response = $this->set_parameter_name($cluster_data,"cluster_data");	
		if($cluster_response != '') $response->addParameterGroup($cluster_response);
		
		// fetching the arraylunmapids with the protected phy_lun_ids
		
		if($phy_lun_ids) {		
			$app_lun_data = $this->ps_obj->getHostAppLunMap($phy_lun_ids);
			$app_lun_response = $this->set_parameter_name($app_lun_data,"app_lun_data");	
			if($app_lun_response != '') $response->addParameterGroup($app_lun_response);
		}
		
		//fetching the ps details with the requested ps id
		$ps_data = $this->ps_obj->getPSDetails($host_id);
		$ps_response = $this->set_parameter_name($ps_data,"ps_data");	
		if($ps_response != '') $response->addParameterGroup($ps_response);
		
		//fetching the netwrok address details with the requested ps id
		$ntw_data = $this->ps_obj->getNTWAddDetails($host_id);	
		$ntw_add_res = $this->set_parameter_name($ntw_data,"ntw_data");	
		if($ntw_add_res != '') $response->addParameterGroup($ntw_add_res);
		
		//fetching the application plan details with the fetched planId
		if($plan_ids) {
			$plan_data = $this->ps_obj->getPlanDetails($plan_ids);	
			$plan_res = $this->set_parameter_name($plan_data,"plan_data");
			if($plan_res != '') $response->addParameterGroup($plan_res);	
		}
		
		// fetching the log details	
		list($tbs_data, $log_rot_data) = $this->ps_obj->getLogDetails();
		$tbs_data_res = new ParameterGroup("tbs_data");
		
		foreach($tbs_data as $tbs_info) {
			$tbs_data_res->setParameterNameValuePair($tbs_info['ValueName'],$tbs_info['ValueData']);
		}
		$log_rot_response = $this->set_parameter_name($log_rot_data,"log_rot_data");
		if($tbs_data_res != '') $response->addParameterGroup ($tbs_data_res);
		if($log_rot_response != '') $response->addParameterGroup($log_rot_response);
		
		// fetching the stand by details data
		$stand_by_data = $this->ps_obj->getStandbyDetails();
		$stand_by_response = $this->set_parameter_name($stand_by_data,"stand_by_data");
		if($stand_by_response != '') $response->addParameterGroup($stand_by_response);
		
		$sys_response = $this->getSystemHealth();
		if($sys_response != '') $response->addParameterGroup($sys_response);
		
		$bpm_data = array();
		$bpm_response = $this->set_parameter_name($bpm_data,"bpm_policy_data");
		if($bpm_response != '') $response->addParameterGroup($bpm_response);
		
		$bpm_alloc_data = array();
		$bpm_alloc_response = $this->set_parameter_name($bpm_alloc_data,"bpm_alloc_data");
		if($bpm_alloc_response != '') $response->addParameterGroup($bpm_alloc_response);
		
		//fetching the ps details with the requested ps id
		$policy_data = $this->ps_obj->getPolicyData($host_id);
		$policy_response = $this->set_parameter_name($policy_data,"policy_data");	
		if($policy_response != '') $response->addParameterGroup($policy_response);
		
		return $response->getChildList();
	}

	
	public function GetDBData_validate($identity, $version, $functionName, $parameters) 
	{		
        
		$host_obj = $parameters['HostId'];
		$pair_id_obj = $parameters['PairId'];
		if(!is_object($host_obj))
		{
			$message = "HostId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$host_id = $host_obj->getValue();
		$host_id = trim($host_id);
		if($host_id == "")
		{	
			$message = "HostId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		$host_id_data = $this->commonfunctions_obj->get_host_info($host_id);
		$valid_ps_hostId = FALSE;
		if (is_array($host_id_data))
		{
			if (count($host_id_data) > 0 and $host_id_data['processServerEnabled'])
			{
				$valid_ps_hostId = TRUE;
			}
		}
		
		if (!$valid_ps_hostId)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		if(!is_object($pair_id_obj))
		{
			$message = "PairId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$pair_id = $pair_id_obj->getValue();
		$pair_id = trim($pair_id);
		if($pair_id == "")
		{	
			$message = "PairId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
        
	}		

	public function GetDBData($identity, $version, $functionName, $parameters)
	{	
        
		// fetching the pair details with the corresponding process server id.	
		$host_id = $parameters['HostId']->getValue();
		$pair_id = $parameters['PairId']->getValue();
		$response = new ParameterGroup("Response");
        
        $get_pair_data = array();
		$get_pair_data = $this->ps_obj->getPairData($host_id);
        
        if (is_array($get_pair_data))
        {
            if ($get_pair_data[0])
            {
                $get_pair_data = $get_pair_data[0];
                foreach ($get_pair_data as $key=>$value)
                {
                    $pair_key = explode("!!",$key);
                    if ($pair_key[0] == $pair_id)
                    {
                        $job_id = $value['jobId'];
                        break;
                    }
                }
               
                $response->setParameterNameValuePair('jobId', $job_id);
           }
        }
		return $response->getChildList();
	}
	
	public function IsRollbackInProgress_validate($identity, $version, $functionName, $parameters) 
	{		
		$host_obj = $parameters['HostId'];
		$source_id_obj = $parameters['SourceId'];
		if(!is_object($host_obj))
		{
			$message = "HostId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$host_id = $host_obj->getValue();
		$host_id = trim($host_id);
		if($host_id == "")
		{	
			$message = "HostId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($host_id);
		if ($guid_format_status == false)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$is_ps = $this->conn_obj->sql_get_value("hosts","id","id = '$host_id' AND (csEnabled = '1' OR processServerEnabled = '1')");
		if(!$is_ps)
		{
			$message = "Not a Valid Process Server hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		if(!is_object($source_id_obj))
		{
			$message = "Source Id Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$src_id = $source_id_obj->getValue();
		$src_id = trim($src_id);
		if($src_id == "")
		{	
			$message = "Source hostid Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($src_id);
		if ($guid_format_status == false)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
        
	}		

	public function IsRollbackInProgress($identity, $version, $functionName, $parameters)
	{
		// fetching the pair details with the corresponding process server id.	
		$host_id = $parameters['HostId']->getValue();
		$src_id = $parameters['SourceId']->getValue();
		$response = new ParameterGroup("Response");
		$rollback_status = $this->commonfunctions_obj->is_protection_rollback_in_progress($src_id);	
        $response->setParameterNameValuePair('rollbackStatus', $rollback_status);
		return $response->getChildList();
	}
	
	public function GetCsRegistry_validate($identity, $version, $functionName, $parameters) 
	{		
		
	}		

	public function GetCsRegistry($identity, $version, $functionName, $parameters)
	{
		global $ACTUAL_INSTALLATION_DIR;
		$bin_path = "$ACTUAL_INSTALLATION_DIR\bin\get_cs_reg_data.pl";
		$out_put = `perl "$bin_path"`;
		if ($out_put == NULL)
		{	
			ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "AcsUrl"));
		}
		$reg_data = explode("##!!##",$out_put);
		foreach ($reg_data as $key=>$value)
		{
			$geo_name = $reg_data[0];
			$acs_url = $reg_data[1];
			$privateendpoint_state = $reg_data[2];
			$resource_id = $reg_data[3];
		}
		if ($acs_url == "UNDEF")
		{
			ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "AcsUrl"));
		}
		$response = new ParameterGroup("Response");	
		$response->setParameterNameValuePair('AcsUrl', $acs_url);
		$response->setParameterNameValuePair('GeoName', $geo_name);
		$response->setParameterNameValuePair('PrivateEndpointState', $privateendpoint_state);
		$response->setParameterNameValuePair('ResourceId', $resource_id);
		return $response->getChildList();
	}
	
	public function GetCxPsConfiguration_validate($identity, $version, $functionName, $parameters) 
	{		
			
	}		

	public function GetCxPsConfiguration($identity, $version, $functionName, $parameters)
	{
		$cxps_values = $this->commonfunctions_obj->cxpsconf_values();
		$ssl_port = "";
		if (isset($cxps_values['ssl_port']))
		{
			$ssl_port = $cxps_values['ssl_port'];
		}
		$response = new ParameterGroup("Response");	
		$response->setParameterNameValuePair('CxPsSslPort', $ssl_port);
		return $response->getChildList();
	}
	
	public function MarkResyncRequiredToPair_validate($identity, $version, $functionName, $parameters) 
	{
		$host_obj = $parameters['HostId'];
		$src_id_obj = $parameters['SourceHostId'];
		$src_vol_obj = $parameters['SourceVolume'];
		$dest_id_obj = $parameters['DestHostId'];
		$dest_vol_obj = $parameters['DestVolume'];
		
		if(!is_object($host_obj))
		{
			$message = "HostId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$host_id = $host_obj->getValue();
		$host_id = trim($host_id);
		if($host_id == "")
		{	
			$message = "HostId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($host_id);
		if ($guid_format_status == false)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$is_ps = $this->conn_obj->sql_get_value("hosts","id","id = '$host_id' AND (csEnabled = '1' OR processServerEnabled = '1')");
		if(!$is_ps)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		if(!is_object($src_id_obj))
		{
			$message = "Source Id Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$src_id = $src_id_obj->getValue();
		$src_id = trim($src_id);
		if($src_id == "")
		{	
			$message = "Source Id Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		if(!is_object($src_vol_obj))
		{
			$message = "Source volume Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$src_vol = $src_vol_obj->getValue();
		$src_vol = trim($src_vol);
		if($src_vol == "")
		{	
			$message = "Source volume Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		if(!is_object($dest_id_obj))
		{
			$message = "Destination Id Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$dest_id = $dest_id_obj->getValue();
		$dest_id = trim($dest_id);
		if($dest_id == "")
		{	
			$message = "Destination Id Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		if(!is_object($dest_vol_obj))
		{
			$message = "Destination volume Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$dest_vol = $dest_vol_obj->getValue();
		$dest_vol = trim($dest_vol);
		if($dest_vol == "")
		{	
			$message = "Destination volume Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
	}
	
	public function MarkResyncRequiredToPair($identity, $version, $functionName, $parameters)
	{
		// starting the resync set for pair.	
		$host_id = $parameters['HostId']->getValue();
		$src_id = $parameters['SourceHostId']->getValue();
		$src_vol = $parameters['SourceVolume']->getValue();
		$dest_id = $parameters['DestHostId']->getValue();
		$dest_vol = $parameters['DestVolume']->getValue();
		$Hardlink_from = $parameters['HardlinkFromFile']->getValue();
		$Hardlink_to = $parameters['HardlinkToFile']->getValue();
		
		$error = "Hardlink failure from $Hardlink_from to $Hardlink_to";
		$origin = "ps_agent";
		$response = new ParameterGroup("Response");
		$resync_status = $this->vol_obj->setResyncRequiredToPair($dest_id, $dest_vol, 1, $origin, $error);
		if ($resync_status == FALSE)
		{
			$resync_status = 1;
		}
		else
		{
			$resync_status = 0;
		}
        $response->setParameterNameValuePair('resyncStatus', $resync_status);
		return $response->getChildList();
	}
	
	public function MarkResyncRequiredToPairWithReasonCode_validate($identity, $version, $functionName, $parameters) 
	{
		$host_obj = $parameters['HostId'];
		$src_id_obj = $parameters['SourceHostId'];
		$src_vol_obj = $parameters['SourceVolume'];
		$dest_id_obj = $parameters['DestHostId'];
		$dest_vol_obj = $parameters['DestVolume'];
		$timestamp_obj = $parameters['detectionTime'];
		$errorcode_obj = $parameters['resyncReasonCode'];
		
		if(!is_object($host_obj))
		{
			$message = "HostId Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$host_id = $host_obj->getValue();
		$host_id = trim($host_id);
		if($host_id == "")
		{	
			$message = "HostId Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($host_id);
		if ($guid_format_status == false)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		$is_ps = $this->conn_obj->sql_get_value("hosts","id","id = '$host_id' AND (csEnabled = '1' OR processServerEnabled = '1')");
		if(!$is_ps)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		if(!is_object($src_id_obj))
		{
			$message = "Source Id Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$src_id = $src_id_obj->getValue();
		$src_id = trim($src_id);
		if($src_id == "")
		{	
			$message = "Source Id Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		if(!is_object($src_vol_obj))
		{
			$message = "Source volume Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$src_vol = $src_vol_obj->getValue();
		$src_vol = trim($src_vol);
		if($src_vol == "")
		{	
			$message = "Source volume Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		if(!is_object($dest_id_obj))
		{
			$message = "Destination Id Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$dest_id = $dest_id_obj->getValue();
		$dest_id = trim($dest_id);
		if($dest_id == "")
		{	
			$message = "Destination Id Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		
		if(!is_object($dest_vol_obj))
		{
			$message = "Destination volume Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$dest_vol = $dest_vol_obj->getValue();
		$dest_vol = trim($dest_vol);
		if($dest_vol == "")
		{	
			$message = "Destination volume Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		if(!is_object($timestamp_obj))
		{
			$message = "Timestamp Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$timestamp = $timestamp_obj->getValue();
		$timestamp = trim($timestamp);
		if($timestamp == "")
		{	
			$message = "Timestamp Value Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
		if(!is_object($errorcode_obj))
		{
			$message = "Error code Parameter Is Missed From request XML";
			$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
			$this->raiseException($returnCode,$message);
		}
		
		$error_code = $errorcode_obj->getValue();
		$error_code = trim($error_code);
		if($error_code == "")
		{	
			$message = "Error code Is Missed From request XML";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}	
	}
	
	public function MarkResyncRequiredToPairWithReasonCode($identity, $version, $functionName, $parameters)
	{
		global $PROCESS_SERVER;
		// starting the resync set for pair.	
		$host_id = $parameters['HostId']->getValue();
		$src_id = $parameters['SourceHostId']->getValue();
		$src_vol = $parameters['SourceVolume']->getValue();
		$dest_id = $parameters['DestHostId']->getValue();
		$dest_vol = $parameters['DestVolume']->getValue();
		$Hardlink_from = $parameters['HardlinkFromFile']->getValue();
		$Hardlink_to = $parameters['HardlinkToFile']->getValue();
		

		$resync_reason_details['detectionTime'] = $parameters['detectionTime']->getValue();
		$resync_reason_details['resyncReasonCode'] = $parameters['resyncReasonCode']->getValue();
		
		$error = "Hardlink failure from $Hardlink_from to $Hardlink_to";
		$origin = "ps_agent";
		$response = new ParameterGroup("Response");
		$resync_status = $this->vol_obj->setResyncRequiredToPair($dest_id, $dest_vol, 1, $origin, $error, 1, $resync_reason_details, $PROCESS_SERVER);
		if ($resync_status == FALSE)
		{
			$resync_status = 1;
		}
		else
		{
			$resync_status = 0;
		}
        $response->setParameterNameValuePair('resyncStatus', $resync_status);
		return $response->getChildList();
	}

	public function UpdateDB_validate($identity, $version, $functionName, $parameters) 
	{	
		global $LIST_OF_TABLES_PS_TO_ALLOW_FOR_DB_UPDATES;
		$cnt = 0;
		while($parameters['Id'.$cnt])
		{
			$updateObj = $parameters['Id'.$cnt]->getChildList();

			if (isset($updateObj['queryType']))
			{
				$queryTypes = array("INSERT", "UPDATE", "DELETE", "REPLACE");

				$queryType = trim($updateObj['queryType']->getValue());
				if (!$queryType)
				{
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_NO_DATA', array('Parameter' => 'queryType'));
				}
				elseif (!in_array(strtoupper($queryType), $queryTypes))
				{
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_INVALID_DATA', array('Parameter' => $queryType));
				}
			}
			
			if (isset($updateObj['tableName']))
			{
				$tableName = trim($updateObj['tableName']->getValue());
				if (!$tableName)
				{
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_NO_DATA', array('Parameter' => 'tableName'));
				}
				elseif (preg_match('/\bsleep\b\s*\(/i', $tableName))
				{
					// Throw error if sleep interval is found
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_INVALID_DATA', array('Parameter' => $tableName));
				}
			}
			
			if (isset($updateObj['tableName']))
			{
				$tableName = trim($updateObj['tableName']->getValue());
				if (in_array(strtolower($tableName), $LIST_OF_TABLES_PS_TO_ALLOW_FOR_DB_UPDATES))
				{
					//Valid table to allow db updates from PS.
				}
				else
				{
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_INVALID_DATA', array('Parameter' => $tableName));
				}
			}

			if (isset($updateObj['fieldNames']))
			{
				$fieldNames = trim($updateObj['fieldNames']->getValue());
				if (preg_match('/\bsleep\b\s*\(/i', $fieldNames))
				{
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_INVALID_DATA', array('Parameter' => $fieldNames));
				}
			}

			if (isset($updateObj['condition']))
			{
				$condition = trim($updateObj['condition']->getValue());
				if (!$condition)
				{
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_NO_DATA', array('Parameter' => 'condition'));
				}
				elseif (preg_match('/\bsleep\b\s*\(/i', $condition))
				{
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_INVALID_DATA', array('Parameter' => $condition));
				}
			}
			
			if (isset($updateObj['condition']))
			{
				$condition = trim($updateObj['condition']->getValue());
				if (strlen($condition)>0)
				{
					$this->commonfunctions_obj->validateUpdateDBAdditionalInputs($condition);
				}
			}

			if(isset($updateObj['additionalInfo']))
			{
				$additionalInfo = trim($updateObj['additionalInfo']->getValue());
				if (preg_match('/\bsleep\b\s*\(/i', $additionalInfo))
				{
					ErrorCodeGenerator::raiseError(
						'COMMON', 'EC_INVALID_DATA', array('Parameter' => $additionalInfo));
				}
			}
			
			if (isset($updateObj['additionalInfo']))
			{
				$additionalInfo = trim($updateObj['additionalInfo']->getValue());
				if (strlen($additionalInfo)>0)
				{
					$this->commonfunctions_obj->validateUpdateDBAdditionalInputs($additionalInfo);
				}
			}

			if (isset($queryType) && $queryType != "" &&
					isset($condition) && $condition != "")
			{
				if (strcasecmp($queryType, "UPDATE") == 0 ||
					strcasecmp($queryType, "DELETE") == 0)
				{
					// Split the records in condition using AND/OR delimiter
					$conditions = preg_split('/\s*\bAND\b\s*|\s*\bOR\b\s*/i', $condition);

					if (count($conditions) > 1)
					{
						foreach($conditions as $cond)
						{
							$this->commonfunctions_obj->validateUpdateDBInput($cond);
						}
					}
					else
					{
						$this->commonfunctions_obj->validateUpdateDBInput($condition);
					}
				}
			}
			$cnt++;
		}
	}
	
	public function UpdateDB($identity, $version, $functionName, $parameters) 
	{
		$response = new ParameterGroup ( "Response" );
		$cnt = 0;
		global $multi_query_list;
		$multi_query_list = array();
		$return_val = false;
		$additionalInfo = "";
		$this->apiLogtest("updateDB : inputData : ".print_r($parameters,true));
		while($parameters['Id'.$cnt])
		{
			$updateObj = $parameters['Id'.$cnt]->getChildList();
			$query_type = trim($updateObj['queryType']->getValue());
			$table_name = trim($updateObj['tableName']->getValue());
			$field_name = trim($updateObj['fieldNames']->getValue());
			$condition = trim($updateObj['condition']->getValue());
			if(isset($updateObj['additionalInfo']))
			{
				$additionalInfo = trim($updateObj['additionalInfo']->getValue());
			}
			
			$this->apiLogtest("updateDB:: $query_type,$table_name,$field_name,$condition,$additionalInfo");
			if($condition) $cond = "WHERE $condition";
			
			if($query_type == 'UPDATE')	
			{			
				$multi_query_list['update'][] = "UPDATE $table_name SET $field_name $cond";
			} 
			elseif($query_type == 'DELETE')	
			{				
				$multi_query_list['delete'][] = "DELETE FROM $table_name $cond";
			} 
			elseif($query_type == 'INSERT')	
			{
				$query = $query_type." ".$table_name;
				$fields = explode(",",$condition);
				$host_row = $this->conn_obj->sql_get_value("hosts","count(*)","id=$fields[0]");
				if(($query == "INSERT hosts") && (!$host_row) || ($query != "INSERT hosts"))
				{
					$multi_query_list['insert'][] = "INSERT INTO $table_name ($field_name) VALUES ($condition) $additionalInfo";
				}	
			}
			elseif($query_type == 'REPLACE')
			{
				$multi_query_list['replace'][] = "REPLACE INTO $table_name ($field_name) VALUES ($condition)";
			}
			$cnt++;	
		}
		$monitor_cnt  = 0;
		$final_array = array();
		$protected_host_id = array();
		$protected_src_vols = array();
		$protected_tgt_vols = array();
		
		while($parameters['monitor_pairs'.$monitor_cnt])
		{
			$this->apiLogtest("final_array::inside parameter group");
			$monitorObj = $parameters['monitor_pairs'.$monitor_cnt]->getChildList();
			$current_rpo = trim($monitorObj['rpo']->getValue());
			$src_id = trim($monitorObj['src_id']->getValue());
			$src_vol = trim($monitorObj['src_vol']->getValue());
			$dest_id = trim($monitorObj['dest_id']->getValue());
			$dest_vol = trim($monitorObj['dest_vol']->getValue());
			$rpo_violation = trim($monitorObj['rpo_violation']->getValue());
			array_push($protected_host_id,$src_id);
			array_push($protected_host_id,$dest_id);
			array_push($protected_src_vols,$this->conn_obj->sql_escape_string($src_vol));
			array_push($protected_tgt_vols,$this->conn_obj->sql_escape_string($dest_vol));
			$final_array[] = $current_rpo."!!".$src_id."!!".$src_vol."!!".$dest_id."!!".$dest_vol."!!".$rpo_violation;	
			$monitor_cnt++;
		}
		$this->apiLogtest("final_array::".print_r($final_array,true));
		if($monitor_cnt)
		{
			$protected_host_id = array_unique($protected_host_id);
			$protected_host_id1 = implode("','",$protected_host_id);
			$protected_src_vols = array_unique($protected_src_vols);
			$protected_src_vols1 = implode("','",$protected_src_vols);
			$protected_tgt_vols = array_unique($protected_tgt_vols);
			$protected_tgt_vols1 = implode("','",$protected_tgt_vols);
		}	
		
		$this->apiLogtest("multi_query_list::".print_r($multi_query_list,true));
		if(sizeof($multi_query_list['insert']) || sizeof($multi_query_list['delete']) || sizeof($multi_query_list['update']) || sizeof($multi_query_list['replace']))
		{
			$this->apiLogtest("multi_query_list:: size of".sizeof($multi_query_list['update']));
			$return_val = $this->batchsql_obj->filterAndUpdateBatches($multi_query_list, "PSAPIHandler :: updateDB");
			$this->apiLogtest("return_val::$return_val");
			if($return_val) {
				$this->raiseException(ErrorCodes::EC_NO_PLNID_FOND,"Transaction Failure");
			}			
		}		
		return $response->getChildList();
	}
	
	public function addErrorMessage_validate($identity, $version, $functionName, $parameters) 
	{		
		$alertObj = $parameters['addErrorMessage']->getChildList();
		
		if(isset($alertObj['alert_info']) || isset($alertObj['trap_info']))
		{
			$alert_info = isset($alertObj['alert_info']) ? trim($alertObj['alert_info']->getValue()) : "";
			$trap_info = isset($alertObj['trap_info']) ? trim($alertObj['trap_info']->getValue()) : "";
			if(!$alert_info && !$trap_info) {
				$this->raiseException(ErrorCodes::EC_NO_DATA,"No Data found for alert_info & trap_info Parameter");
			}
		}
	}		
	
	public function addErrorMessage($identity, $version, $functionName, $parameters) 
	{
		$alertObj = $parameters['addErrorMessage']->getChildList();
		if(isset($alertObj['alert_info']))
		{			
			$alert_info = trim($alertObj['alert_info']->getValue());
			$alert_info = unserialize($alert_info);
			if(is_array($alert_info))
			{
				$error_id = $alert_info['error_id'];
				$err_template_id = $alert_info['err_temp_id'];
				$err_summary = $alert_info['summary'];
				$err_message = $alert_info['err_msg'];
				$hostid = $alert_info['id'];
                $err_code = (isset($alert_info['err_code'])) ? $alert_info['err_code'] : '';
                $err_placeholders = (isset($alert_info['err_placeholders'])) ? serialize($alert_info['err_placeholders']) : '';

				$this->commonfunctions_obj->add_error_message_v2($error_id, $err_template_id, $err_summary, $err_message, $hostid, $err_code, $err_placeholders);
			}	
		}	
	}
	
	public function UnRegisterHost_Validate($identity, $version, $functionName, $parameters)
	{
		$unregisterObj = $parameters['UnRegisterHost']->getChildList();
		
		if(isset($unregisterObj['hostId']))
		{
			$hostId = trim($unregisterObj['hostId']->getValue());
			if(!$hostId) 
			{
				$this->raiseException(ErrorCodes::EC_NO_DATA,"No Data found for hostId Parameter");
			}
		}
		
		$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($hostId);
		if ($guid_format_status == false)
		{
			$message = "Not a Valid hostId";
			$returnCode = ErrorCodes::EC_NO_DATA;
			$this->raiseException($returnCode,$message);
		}
	}
	
	public function UnRegisterHost($identity, $version, $functionName, $parameters)
	{
		$unregisterObj = $parameters['UnRegisterHost']->getChildList();	
		$hostId = trim($unregisterObj['hostId']->getValue());
		$hostType = trim($unregisterObj['hostType']->getValue());
		
		if($hostType == "PS")
		{
			$this->ps_obj->unregister_host($hostId);	
		}
		else
		{
			//$this->storageObj->unregister_host($hostId);
		}
	}
	
	public function RegisterHost_Validate($identity, $version, $functionName, $parameters)
	{
		$registerStaticObj = $parameters['RegisterHost']->getChildList();
		
		if(isset($registerStaticObj['host_info']))
		{
			$host_info = trim($registerStaticObj['host_info']->getValue());
			if(!$host_info) {
				$this->raiseException(ErrorCodes::EC_NO_DATA,"No Data found for host_info Parameter");
			}

			$host_info = unserialize($host_info);
			$host_id = $host_info['id'];

			if(!$host_id)
			{
				$this->raiseException(ErrorCodes::EC_NO_DATA, "No Data found for id Parameter");
			}

			$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($host_id);
			if ($guid_format_status == false)
			{
				$message = "Not a Valid hostId";
				$returnCode = ErrorCodes::EC_NO_DATA;
				$this->raiseException($returnCode,$message);
			}
		}

		if(isset($registerStaticObj['network_info']))
		{
			$network_info = trim($registerStaticObj['network_info']->getValue());
			$network_info = unserialize($network_info);

			foreach($network_info as $nic_name => $network_details)
			{
				$host_id = $network_details['hostId'];
				if(!$host_id)
				{
					$this->raiseException(ErrorCodes::EC_NO_DATA, "No Data found for hostId Parameter");
				}

				$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($host_id);
				if ($guid_format_status == false)
				{
					$message = "Not a Valid hostId";
					$returnCode = ErrorCodes::EC_NO_DATA;
					$this->raiseException($returnCode,$message);
				}
			}
		}
        
        if(isset($registerStaticObj['ha_info']))
		{
			$ha_info = trim($registerStaticObj['ha_info']->getValue());
			if(!$ha_info) {
				$this->raiseException(ErrorCodes::EC_NO_DATA,"No Data found for ha_info Parameter");
			}
		}
	}
	
	public function RegisterHost($identity, $version, $functionName, $parameters)
	{
		$registerStaticObj = $parameters['RegisterHost']->getChildList();
		
		if(isset($registerStaticObj['host_info']))
		{			
			$host_info = trim($registerStaticObj['host_info']->getValue());
			$host_info = unserialize($host_info);
			$host_id = $host_info['id'];			
			$hostname = $host_info['name'];
			$ipaddress = $host_info['ipaddress'];
			
			# Raising exception if the hostId, hostname, ipaddress values are 
			#   not found in the host info serialized string
			if(!$host_id || !$hostname || !$ipaddress) {
				$this->raiseException(ErrorCodes::EC_NO_DATA,"No Data found for mandatory parameters (id, hostname, ipaddress) in host_info values");
			}
		
			$this->ps_obj->registerHost($host_id,$host_info);
		}
		if(isset($registerStaticObj['network_info']))
		{
			$network_info = trim($registerStaticObj['network_info']->getValue());
			$network_info = unserialize($network_info);
			$this->ps_obj->registerNetworkDevices($network_info);
		}
        // if(isset($registerStaticObj['ha_info']))
		// {
			// $ha_info = trim($registerStaticObj['ha_info']->getValue());
			// $ha_info = unserialize($ha_info);
            // $host_id[] = $ha_info['nodeId'];

			// $this->ps_obj->registerHA($ha_info['nodeId'], $ha_info);
		// }    
	}
	
	private function getSystemHealth() {
		$sys_health_flag = $this->conn_obj->sql_get_value("systemHealth","healthFlag","component='Space Constraint'");
		$sys_data_res = new ParameterGroup("sys_health");
		$sys_data_res->setParameterNameValuePair('healthFlag',$sys_health_flag);
		return $sys_data_res;
	}
	
	private function set_parameter_name($result,$group_name)
	{
		if(count($result)) $data_res = new ParameterGroup($group_name);
		else $data_res = '';
		
		foreach($result as $key=>$val)
		{
			$data_result = new ParameterGroup($key);
			
			foreach($val as $k=>$v)
			{
				$data_result->setParameterNameValuePair($k,$v);
			}
			$data_res->addParameterGroup($data_result);
			
		}
		return $data_res;
	}
	
	public function apiLogtest($debugString)    {	
		return;
		global $REPLICATION_ROOT;
		$log_path = "$REPLICATION_ROOT/var/psapitest.log";		
		$fr = fopen($log_path, 'a+');
		
		if(!$fr) echo "Error! Couldn't open the file.";	
		
		if (!fwrite($fr, $debugString . "\n")) {		
			print "Cannot write to file ($filename)";		
		}		
		if(!fclose($fr)) echo "Error! Couldn't close the file.";    
	}
	
	/*
    * Function Name: PSFailover_validate
    *
    * Description: 
    * This function is to validate the PSFailover input
    *
    * Parameters:
    *     Param 1 [IN]: Refer the RequestSample XML
    *     Param 2 [OUT]: 
    *     
    * Exceptions:
    *     SQL exception on transaction failure
    */
	
	public function PSFailover_validate($identity, $version, $functionName, $parameters)
	{
		$function_name_user_mode = 'PS Failover';
		
        try
		{
			$server_ps_array = array();
			
			// Enable exceptions for DB.
			$this->conn_obj->enable_exceptions();
			
            // Validate mandatory parameters
            $mandatory_params = array("FailoverType", "OldProcessServer");
            foreach($mandatory_params as $param)
            {
                if(!isset($parameters[$param])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $param));
                $value = trim($parameters[$param]->getValue());			
                if(!$value) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $param));

                // OldProcessServer validate
                if($param == "OldProcessServer")
                {
                    // Validating the existing PS
                    $old_ps_id  = $value;
                    $ps_valid = $this->processServer_obj->validatePS($old_ps_id);
                    if(!$ps_valid) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
                }
                if($param == "FailoverType")
                {
                    // FailoverType validate
                    if(!in_array($value, array("SystemLevel", "ServerLevel"))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $param));
                    if($value == "SystemLevel")
                    {
                        if(!isset($parameters['NewProcessServer'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "NewProcessServer"));						
                        if(isset($parameters['Servers'])) ErrorCodeGenerator::raiseError($functionName, 'EC_MTAL_EXCLSIVE_ARGS', array(), "Expected Servers only if failover is at ServerLevel");
                    }				
                }			
            }

            $server_ps_array = array();
            if(isset($parameters['NewProcessServer']))
            {
                $new_ps_id = $parameters['NewProcessServer']->getValue();
                if(!$new_ps_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "NewProcessServer"));
				if($old_ps_id == $new_ps_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "NewProcessServer"));				
                
				// Validate the New Process Server
                $new_ps_details = $this->processServer_obj->validatePS($new_ps_id);
                if(!$new_ps_details) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
                
				// Validate the New Process Server heartbeat
				if($new_ps_details['currentTime'] - $new_ps_details['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $new_ps_details['name'], 'PsIP' => $new_ps_details['ipaddress'], 'Interval' => ceil(($new_ps_details['currentTime'] - $new_ps_details['lastHostUpdateTimePs'])/60)));
            }
            
            // Server level parameters validation
            if(isset($parameters['Servers']))
            {
                $ps_ids = array();
                $servers = $parameters['Servers']->getChildList();
				if(!is_array($servers)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Servers"));
                foreach($servers as $key => $servers_obj)
                {
					$server_obj = $servers_obj->getChildList();
                    // Validate the source host id
                    if(!isset($server_obj['HostId'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $key." - HostId"));
                    $host_id = trim($server_obj['HostId']->getValue());
                    // Validation for duplicate hosts
                    if(array_key_exists($host_id, $server_ps_array)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Duplicate HostId - ".$key));
                    
                    // Validating for the new process server to be used.
                    if(!isset($server_obj['NewProcessServer']) && !$new_ps_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $key." - NewProcessServer"));
                    $host_ps_id = trim($server_obj['NewProcessServer']->getValue());
					if($old_ps_id == $host_ps_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $key." - NewProcessServer"));
                    
					// If already existing PS, no need to validate
                    if(!in_array($host_ps_id, $ps_ids))
                    {
                        $ps_details = $this->processServer_obj->validatePS($host_ps_id);

                        // Validating for the new process server heartbeat.
						if($ps_details['currentTime'] - $ps_details['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ps_details['name'], 'PsIP' => $ps_details['ipaddress'], 'Interval' => ceil(($ps_details['currentTime'] - $ps_details['lastHostUpdateTimePs'])/60)));

                        $ps_ids[] = $host_ps_id;
                    }
                    $server_ps_array[$host_id] = $host_ps_id;
                }
            }
                
            // If any protection is set
            if(count($server_ps_array))
            {
                // Check if all the sources mentioned are protected
                list($validation_status, $placeholder_array) = $this->processServer_obj->validatePsHosts($server_ps_array, $old_ps_id);
				$error_array = array(1 => 'EC_NO_PROTECTION_SERVER', 2 => 'EC_NO_MT_HEART_BEAT');
				if($validation_status) ErrorCodeGenerator::raiseError($functionName, $error_array[$validation_status], array_merge($placeholder_array, array('OperationName' => $function_name_user_mode)));
            }
			
			$this->conn_obj->disable_exceptions();
           
        }
        catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn_obj->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
		{
			// Disable Exceptions for DB
			$this->conn_obj->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn_obj->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	/*
    * Function Name: PSFailover
    *
    * Description: 
    * This function is to failover the process server
    *
    * Parameters:
    *     Param 1 [IN]: Refer the RequestSample XML
    *     Param 2 [OUT]: Status of the failover
    *     
    * Exceptions:
    *     SQL exception on transaction failure
    */
	
	public function PSFailover($identity, $version, $functionName, $parameters)
	{
		global $ASRAPI_EVENTS_LOG;
		try
		{
			$server_ps_array = array();
			
			// Enable exceptions for DB.
			$this->conn_obj->enable_exceptions();
			$this->conn_obj->sql_begin();
			
			$response = new ParameterGroup("Response");
			
			$old_ps_id = isset($parameters['OldProcessServer']) ? trim($parameters['OldProcessServer']->getValue()) : "";
			$failover_type = trim($parameters['FailoverType']->getValue());
			// SystemLevel
			if($failover_type == "SystemLevel")
			{
				$new_ps_id = isset($parameters['NewProcessServer']) ? trim($parameters['NewProcessServer']->getValue()) : "";
				$source_ids = $this->processServer_obj->getPSSourceHosts($old_ps_id);
				foreach($source_ids as $source_id)
				{
					$server_ps_array[$source_id] = $new_ps_id;
				}
			}
			// ServerLevel
			else
			{
				if(isset($parameters['Servers']))
				{
                    $source_ps_array = array();
					$server_mappings = $parameters['Servers']->getChildList();
					foreach($server_mappings as $key => $maps_obj)
					{
						$map_obj = $maps_obj->getChildList();
						$source_id = $map_obj['HostId']->getValue();
						$ps_id = $map_obj['NewProcessServer']->getValue();
                        
                        $source_ps_array[$source_id] = $ps_id;
					}
                    $current_source_ps_array = $this->processServer_obj->getPsForSource(array_keys($source_ps_array));
                    foreach($source_ps_array as $source_id => $ps_id)
                    {
                        // Skip the servers that are already assigned to the new PS
                        if($current_source_ps_array[$source_id] != $ps_id) $server_ps_array[$source_id] = $ps_id;
						else error_log ("Skipping for Source server : $source_id as already assigned to Process Server : $ps_id\n", 3, $ASRAPI_EVENTS_LOG);
                    }
				}
			}
			
			// Failover the PS
			$call_from_api = 1;
            if(count($server_ps_array)) 
			{
				$this->processServer_obj->psFailover($server_ps_array, $old_ps_id, $call_from_api);
			}
			
			$response->setParameterNameValuePair($functionName,"Success");
			// Disable Exceptions for DB
			$this->conn_obj->sql_commit();
			$this->conn_obj->disable_exceptions();
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn_obj->sql_rollback();
			$this->conn_obj->disable_exceptions();			
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
			$this->conn_obj->sql_rollback();
			$this->conn_obj->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}	
	}
    
    /*
    * Function Name: PSUpdateDetails_validate
    *
    * Description: 
    * This function is to validate the PSUpdateDetails input
    *
    * Parameters:
    *     Param 1 [IN]: Refer the RequestSample XML
    *     Param 2 [OUT]: 
    *     
    * Exceptions:
    *     SQL exception on transaction failure
    */
	
	public function PSUpdateDetails_validate($identity, $version, $functionName, $parameters)
	{
		$function_name_user_mode = 'PS Update Details';
        try
		{
            // Enable exceptions for DB.
			$this->conn_obj->enable_exceptions();
            
            // Validate PS Host ID
            if(!isset($parameters['HostId'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "HostId"));
            $ps_host_id = trim($parameters['HostId']->getValue()); 
            if(!$ps_host_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "HostId"));
            $ps_valid = $this->processServer_obj->validatePS($ps_host_id);
            if(!$ps_valid) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
            
            // Validate Update Details of PS
            if(!isset($parameters['UpdateDetails'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "UpdateDetails"));
            $update_details = trim($parameters['UpdateDetails']->getValue()); 
            if(!$update_details) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "UpdateDetails"));
			
            $this->conn_obj->disable_exceptions();           
        }
        catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn_obj->disable_exceptions();		
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
			$this->conn_obj->disable_exceptions();
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
    }
    
    /*
    * Function Name: PSUpdateDetails
    *
    * Description: 
    * This function is to upload PS update details
    *
    * Parameters:
    *     Param 1 [IN]: Refer the RequestSample XML
    *     Param 2 [OUT]: Status of the Update
    *     
    * Exceptions:
    *     SQL exception on transaction failure
    */
	
	public function PSUpdateDetails($identity, $version, $functionName, $parameters)
	{		
		try
		{
            // Enable exceptions for DB.
			$this->conn_obj->enable_exceptions();
			$this->conn_obj->sql_begin();
			
			$response = new ParameterGroup("Response");
            
            $ps_host_id = trim($parameters['HostId']->getValue());
            $update_details = unserialize(trim($parameters['UpdateDetails']->getValue()));
            if(is_array($update_details))
            {
                $this->processServer_obj->psUpdateDetails($ps_host_id, $update_details);
            }
            
            $response->setParameterNameValuePair($functionName,"Success");
			// Disable Exceptions for DB
			$this->conn_obj->sql_commit();
			$this->conn_obj->disable_exceptions();
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn_obj->sql_rollback();
			$this->conn_obj->disable_exceptions();
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
			$this->conn_obj->sql_rollback();
			$this->conn_obj->disable_exceptions();
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
}
?>
