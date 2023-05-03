<?php
class ProtectionHandler extends ResourceHandler
{
	private $conn;
	private $volObj;
	private $protectionObj;
	private $appObj;
	private $fxObj;
	private $commonfunc_obj;
	private $retention_obj;
	private $policyObj;
	private $systemObj;
    private $recoveryPlanObj;
	private $recovery_obj;
	private $discovery_obj;
	public function ProtectionHandler() 
	{
		global $conn_obj;
		$this->volObj = new VolumeProtection();
		$this->conn = $conn_obj;
		$this->protectionObj = new ProtectionPlan();
		$this->appObj = new Application();
		$this->fxObj = new FileProtection();
		$this->SRMObj = new SRM();
		$this->commonfunc_obj = new CommonFunctions();
		$this->retention_obj = new Retention();
		$this->policyObj = new Policy();
		$this->systemObj = new System();
		$this->recoveryPlanObj = new recoveryPlan();
		$this->recovery_obj = new Recovery();
		$this->discovery_obj = new ESXDiscovery();
	}	
	
	public function CreateProtection_validate ($identity, $version, $functionName, $parameters)
	{	
		global $REPLICATION_ROOT,$DISK_PROTECTION_UNIT,$VOLUME_PROTECTION_UNIT, $protection_direction;
		global $BATCH_SIZE;
		
		$function_name_user_mode = 'Create Protection';
		try
		{
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			if(isset($parameters['PlanId'])) 
			{
				$PlanId = trim($parameters['PlanId']->getValue());
				if($PlanId != '')
				{
					// request for retry
					if(!is_numeric($PlanId))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "PlanId"));
					$planStatus = $this->protectionObj->getPlanStatus($PlanId);

					if($planStatus == 'Prepare Target Failed') return;
					else ErrorCodeGenerator::raiseError($functionName, 'EC_INVALID_PLAN_STATE', array('ProtectionState' => "$planStatus"));
				}
			}
			//validate plan name
			if(isset($parameters['PlanName'])) 
			{
				$planName = trim($parameters['PlanName']->getValue());
				$existingPlanId = $this->protectionObj->getPlanId($planName);
				
				if($existingPlanId) ErrorCodeGenerator::raiseError($functionName, 'EC_PLAN_EXISTS', array('PlanType' => 'Protection', 'PlanName' => $planName));
			}
			
            
			//validate plan properties
			if(isset($parameters['ProtectionProperties']))
			{
				$Obj = $parameters['ProtectionProperties']->getChildList();
				
				// if amethyst.conf having BATCH_SIZE value then overriding with ASR passed BatchResync value.
				if(isset($BATCH_SIZE))
				{
					if((!is_numeric($BATCH_SIZE)) || $BATCH_SIZE <= 0) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "BATCH_SIZE"));
				}
				else
				{
					$BatchResync = trim($Obj['BatchResync']->getValue());
					if((!is_numeric($BatchResync)) || $BatchResync < 0) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "BatchResync"));	
				}
				
				$MultiVmSync = trim($Obj['MultiVmSync']->getValue());
				if((strtolower($MultiVmSync) != 'on') && (strtolower($MultiVmSync) != 'off'))  ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "MultiVmSync"));
				
				$RPOThreshold = trim($Obj['RPOThreshold']->getValue());
				if(!is_numeric($RPOThreshold)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RPOThreshold"));
				
				$HourlyConsistencyPoints = trim($Obj['HourlyConsistencyPoints']->getValue());
				if(!is_numeric($HourlyConsistencyPoints)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HourlyConsistencyPoints"));
				
				$ConsistencyInterval = trim($Obj['ConsistencyInterval']->getValue());
				if((!is_numeric($ConsistencyInterval)) || ($ConsistencyInterval < 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ConsistencyInterval"));
                
				if (isset($Obj['CrashConsistencyInterval']))
				{
					$CrashConsistencyInterval = trim($Obj['CrashConsistencyInterval']->getValue());
					if((!is_numeric($CrashConsistencyInterval)) || ($CrashConsistencyInterval < 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "CrashConsistencyInterval"));
				}
				
				$Compression = trim($Obj['Compression']->getValue());
				if((strtolower($Compression) != 'enabled') && (strtolower($Compression) != 'disabled'))  ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Compression"));
				
				// validate auto resync option 
				if(isset($Obj['AutoResyncOptions']))
				{
					$autoResyncObj = $Obj['AutoResyncOptions']->getChildList();
					$patternMatch = '/^([01]?[0-9]|2[0-3]):[0-5][0-9]$/';
					if(isset($autoResyncObj['StartTime']))
					{
						$StartTime = trim($autoResyncObj['StartTime']->getValue());
						preg_match($patternMatch, $StartTime, $StartTimeMatches);
						if(!$StartTimeMatches[0]) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "StartTime"));
					}
					else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "StartTime"));
				  
					if(isset($autoResyncObj['EndTime']))
					{
						$EndTime = trim($autoResyncObj['EndTime']->getValue()); 	
						preg_match($patternMatch, $EndTime, $EndTimeMatches);
						if(!$EndTimeMatches[0]) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "EndTime"));
					}
					else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "EndTime"));
				  
					if(isset($autoResyncObj['Interval'])){
						$Interval = trim($autoResyncObj['Interval']->getValue()); 
						if($Interval < 10) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Interval"));
					}
					else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Interval"));
				}
			}
            
			$cfs_mt_details = $this->conn->sql_get_array("SELECT id, (CASE WHEN connectivityType = 'VPN' THEN privateIpAddress  ELSE publicIpAddress  END) as ipAddress from cfs", "id", "ipAddress");
            
			//validate global options
			if($parameters['GlobalOptions'])
			{
				$globalOptObj = $parameters['GlobalOptions']->getChildList();
				$ProcessServer = trim($globalOptObj['ProcessServer']->getValue()); 
				if(!$ProcessServer)ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProcessServer"));
               
				$ProcessServerInfo = $this->commonfunc_obj->get_host_info($ProcessServer);
				if((!$ProcessServerInfo) || (!$ProcessServerInfo['processServerEnabled'])) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
				
				if($ProcessServerInfo['currentTime'] - $ProcessServerInfo['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ProcessServerInfo['name'], 'PsIP' => $ProcessServerInfo['ipaddress'], 'Interval' => ceil(($ProcessServerInfo['currentTime'] - $ProcessServerInfo['lastHostUpdateTimePs'])/60)));
				
				$UseNatIPFor = 'None';
				if($globalOptObj['UseNatIPFor'])  $UseNatIPFor = trim($globalOptObj['UseNatIPFor']->getValue());
				
				if((strtolower($UseNatIPFor) != 'source') && (strtolower($UseNatIPFor) != 'target') && (strtolower($UseNatIPFor) != 'both') && (strtolower($UseNatIPFor) != 'none'))
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "UseNatIPFor"));
					
				$MasterTarget = trim($globalOptObj['MasterTarget']->getValue()); 
				if(!$MasterTarget) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "MasterTarget"));

				$MasterTargetInfo = $this->commonfunc_obj->get_host_info($MasterTarget);

				if(!$MasterTargetInfo) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
				elseif(strtolower($MasterTargetInfo['agentRole']) != 'mastertarget') ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
				
				if($MasterTargetInfo['currentTime'] - $MasterTargetInfo['lastHostUpdateTime'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_HEART_BEAT', array('MTHostName' => $MasterTargetInfo['name'], 'MTIP' => $MasterTargetInfo['ipaddress'], 'Interval' => ceil(($MasterTargetInfo['currentTime'] - $MasterTargetInfo['lastHostUpdateTime'])/60), 'OperationName' => $function_name_user_mode));
				
				$tgt_os_flag = $this->commonfunc_obj->get_osFlag($MasterTarget);
				
				if($tgt_os_flag == 1) $RetentionDrive = "R";
				else $RetentionDrive = "/mnt/retention";
				
				if($globalOptObj['RetentionDrive']) 
				{
					$RetentionDrive = trim($globalOptObj['RetentionDrive']->getValue());
					if(!$RetentionDrive) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RetentionDrive"));
				}	
				
				$retention_details = $this->retention_obj->getValidRetentionVolumes($MasterTarget,$RetentionDrive);

				if (! is_array($retention_details) && ! $retention_details)	ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RETENTION_VOLUMES', array('RetentionDrive' => $RetentionDrive));
				if (! is_array($retention_details) && $retention_details)	ErrorCodeGenerator::raiseError($functionName, 'EC_NO_SUFFICIENT_SPACE_FOR_RETENTION', array('RetentionDrive' => $RetentionDrive));
						
				if(isset($globalOptObj['ProtectionDirection']))
				{
					$protection_direction = trim($globalOptObj['ProtectionDirection']->getValue());
					if($protection_direction != "E2A" && $protection_direction != "A2E") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionDirection"));
				}
                
                if ($protection_direction != "A2E")
                {
                    if($MasterTarget && !$cfs_mt_details[$MasterTarget]) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PUBLIC_IP', array('MTHostName' => $MasterTargetInfo['name']));
                }
			}
			
            
			//get source MBR information
			$dbSourceHostList = $this->commonfunc_obj->get_host_id_by_agent_role("Agent");
			$mbrPath = join(DIRECTORY_SEPARATOR, array($REPLICATION_ROOT, 'admin', 'web', 'SourceMBR'));
			
			
			//validate source server details 
			$target_id_list = array();
			$retention_drive_list = array();
			if($parameters['SourceServers'])
			{
				$sourceServerObj = $parameters['SourceServers']->getChildList();
				if(!is_array($sourceServerObj)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "SourceServers"));
				$cluster_hosts = $this->commonfunc_obj->getAllClusters();
				
				foreach($sourceServerObj as $v)
				{
					// validate source server
					$pairs_to_configure = array();
					$encryptedDiskList = array();
					$encryptedDisks = array();
					$sourceDetailsObj = $v->getChildList();
					$sourceHostId = trim($sourceDetailsObj['HostId']->getValue()); 
					if(!$sourceHostId) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HostId"));
					$sourceIdInfo = $this->commonfunc_obj->get_host_info($sourceHostId);
						
					if(!$sourceIdInfo) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
					elseif(strtolower($sourceIdInfo['agentRole']) != 'agent') ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
                    
                    if(isset($sourceDetailsObj['ProtectionDirection']))
                    {
						$protection_direction = trim($sourceDetailsObj['ProtectionDirection']->getValue());
                        if($protection_direction != "E2A" && $protection_direction != "A2E") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionDirection"));
                    }
					
                    $protection_unit = $VOLUME_PROTECTION_UNIT;
                    if ($protection_direction == 'A2E')
                    {
                        $protection_unit = $DISK_PROTECTION_UNIT;
                    }
                    
                    if ($protection_unit == $VOLUME_PROTECTION_UNIT)
                    {
                        foreach($dbSourceHostList as $dbHostId => $latestMbrFile)
                        {
                            if(($dbHostId == $sourceHostId) && ($sourceIdInfo['osFlag'] == 1))
                            {
                                if (empty($latestMbrFile) || !(file_exists($mbrPath . DIRECTORY_SEPARATOR . $dbHostId . DIRECTORY_SEPARATOR . $latestMbrFile))) 
                                {	
                                    $dbSourceHostList = $this->commonfunc_obj->set_agent_refresh_state();
                                    ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MBR_FOUND', array('SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
                                }
                            }
                        }	
                     }
                   
                    if ($protection_direction == 'A2E')
                    {     
                        if (isset($Obj['HyperVisorType']))
                        {
                            $HyperVisorType = trim($Obj['HyperVisorType']->getValue());
                           
                            if (! empty($HyperVisorType))
                            {
                                if((!is_string($HyperVisorType)) || ($HyperVisorType < 0))
                                {                        
                                    ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HyperVisorType"));
                                }
                            }
                        }   
                    }
                   
					foreach($cluster_hosts as $cluster_id => $cluster_data)
					{
						if(in_array($sourceHostId, $cluster_data)) ErrorCodeGenerator::raiseError($functionName, 'EC_CLUSTER_SERVER', array('SourceIP' => $sourceIdInfo['ipaddress'], 'ErrorCode' => '5117'));
					}
					
					// collect all the encrypted disks based upon hostid.	
					$encryptedDiskList = $this->commonfunc_obj->get_encrypted_disks($sourceHostId);
					
					// validate process server
					if(!isset($sourceDetailsObj['ProcessServer']) && !$ProcessServer) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProcessServer"));
					if(isset($sourceDetailsObj['ProcessServer']))
					{
						$ProcessServer = trim($sourceDetailsObj['ProcessServer']->getValue());
						if($ProcessServer) 
						{
							$ProcessServerInfo = $this->commonfunc_obj->get_host_info($ProcessServer);							
							if((!$ProcessServerInfo) || (!$ProcessServerInfo['processServerEnabled'])) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
							
							if($ProcessServerInfo['currentTime'] - $ProcessServerInfo['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ProcessServerInfo['name'], 'PsIP' => $ProcessServerInfo['ipaddress'], 'Interval' => ceil(($ProcessServerInfo['currentTime'] - $ProcessServerInfo['lastHostUpdateTimePs'])/60)));
						}
					}

                    
					if(!isset($sourceDetailsObj['UseNatIPFor']) && !$UseNatIPFor) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "UseNatIPFor"));				
					if(isset($sourceDetailsObj['UseNatIPFor']))  $UseNatIPFor = trim($sourceDetailsObj['UseNatIPFor']->getValue());				
					if((strtolower($UseNatIPFor) != 'source') && (strtolower($UseNatIPFor) != 'target') && (strtolower($UseNatIPFor) != 'both') && (strtolower($UseNatIPFor) != 'none')) 
						ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "UseNatIPFor"));
                  

					// validate target server
					if(isset($sourceDetailsObj['MasterTarget']))
					{
						$MasterTarget = trim($sourceDetailsObj['MasterTarget']->getValue());
						if($MasterTarget) {
							$MasterTargetInfo = $this->commonfunc_obj->get_host_info($MasterTarget);
								
							if(!$MasterTargetInfo){
								ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
							}
							elseif(strtolower($MasterTargetInfo['agentRole']) != 'mastertarget'){
								ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
							}
							
							if($MasterTargetInfo['currentTime'] - $MasterTargetInfo['lastHostUpdateTime'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_HEART_BEAT', array('MTHostName' => $MasterTargetInfo['name'], 'MTIP' => $MasterTargetInfo['ipaddress'], 'Interval' => ceil(($MasterTargetInfo['currentTime'] - $MasterTargetInfo['lastHostUpdateTime'])/60), 'OperationName' => $function_name_user_mode));
							if ($protection_direction != 'A2E')
							{	
								if(!$cfs_mt_details[$MasterTarget]) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PUBLIC_IP', array('MTHostName' => $MasterTargetInfo['name']));
							}
						}
					}
					$target_id_list[] = $MasterTarget;

					// validate source and target already protected 
					$existingPlanDetails = $this->protectionObj->isPlanExist($sourceHostId, 'DR Protection');
					 	
					if(count($existingPlanDetails) > 0)
					{
						$existing_target_host = $this->commonfunc_obj->getHostName($existingPlanDetails[0]['targetId']);
						$existing_target_host_ip = $this->commonfunc_obj->getHostIpaddress($existingPlanDetails[0]['targetId']);

						ErrorCodeGenerator::raiseError($functionName, 'EC_SRC_TGT_PROTECTED', array('SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress'], 'MTHostName' => $existing_target_host, 'MTIP' => $existing_target_host_ip));
					}

					// validate Retention drive
					$tgt_os_flag = $this->commonfunc_obj->get_osFlag($MasterTarget);
				
					if($tgt_os_flag == 1) $RetentionDrive = "R";
					else $RetentionDrive = "/mnt/retention";
					if(isset($sourceDetailsObj['RetentionDrive'])) 
					{
						$RetentionDrive = trim($sourceDetailsObj['RetentionDrive']->getValue());
						if(!$RetentionDrive) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RetentionDrive"));
					}					
					$retention_drive_list[$MasterTarget] =  $RetentionDrive;
            
					if($sourceIdInfo['osFlag'] != $tgt_os_flag)
                    {
                        ErrorCodeGenerator::raiseError($functionName,'EC_OS_CHECK');
					}
 
					if(!isset($sourceDetailsObj['DiskMapping'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "DiskMapping"));
					if($sourceDetailsObj['DiskMapping'])
					{
					   $diskMapObj = $sourceDetailsObj['DiskMapping']->getChildList();
					   unset($encryptedStr);
					   foreach ($diskMapObj as $diskData)
					   {
						  $diskInfoObj = $diskData->getChildList();
						  $SourceDiskName = trim($diskInfoObj['SourceDiskName']->getValue());
						  if(!$SourceDiskName)ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "SourceDiskName"));
						  
						  $TargetLUNId = trim($diskInfoObj['TargetLUNId']->getValue()); 
						  
						  if(($TargetLUNId == ""))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "TargetLUNId"));
						  $pairs_to_configure[$SourceDiskName] = $TargetLUNId;
						  
						  if(in_array("$SourceDiskName", $encryptedDiskList)) 
						  {
							$encryptedDisks = array_keys($encryptedDiskList, "$SourceDiskName");
							$encryptedStr .= implode(",", $encryptedDisks);
							$encryptedStr = $encryptedStr.",";
						  }
					    }
					  		
                        $src_vols = array_keys($pairs_to_configure);
                        
                        
                        //Validating source disk encryption.
					    // Protection needs to block if disk is encrypted.
					    if(isset($encryptedStr))
					    {
							ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_ENCRYPTED', array('ListVolumes' => rtrim($encryptedStr,","), 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
					    }
						
					   //Validating source disks for UEFI partition/dynamic disks. This is applicable only for windows.
                        if ($sourceIdInfo['osFlag'] == 1)
                        {    
                            $src_vols_str = implode(",", $src_vols);
                            $validation_status = $this->volObj->check_disk_signature_validation($sourceHostId, $src_vols, $src_vols_str, $protection_direction);
                            
                            if ($validation_status["validation"] == FALSE)
                            {
                                if ($validation_status["disk_ref"] == "SIGNATURE")
                                {
                                    ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_SIGNATURE_INPUT_INVALID', array('ListVolumes' => $src_vols_str, 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
                                }
                                elseif($validation_status["disk_ref"] == "NAME")
                                {
                                    ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_NAME_INPUT_INVALID', array('ListVolumes' => $src_vols_str, 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
                                }
                            }
                            
                            list($uefi_disk_arr, $dynamic_disk_arr) = $this->volObj->check_disk_eligibility($sourceHostId, $src_vols_str);
                            //For E2A, it is forward protection and is volume based protection
                            if ($protection_unit == $VOLUME_PROTECTION_UNIT)
                            {
                                /*
                                    For V1 block any dynamic disk whether it is data or boot dynamic disk.
                                */
                                if(count($dynamic_disk_arr) >= 1) 
                                {
                                    ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_DYNAMIC', array('ListVolumes' => implode(",", $dynamic_disk_arr), 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
                                }
                            }
                            //For A2E, it is failback protection and it is disk based protection.
                            elseif ($protection_unit == $DISK_PROTECTION_UNIT)
                            { 
                                /*
                                    Boot Dynamic disk:- V2 does not support
                                    Data Dynamic disk:- V2 can support this
                                */
                                $dynamic_boot_disk_status = $this->volObj->is_dynamic_boot_disk_exists($sourceHostId, $src_vols);
                                
                                if($dynamic_boot_disk_status == TRUE) 
                                {
                                    ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_DYNAMIC', array('ListVolumes' => implode(",", $dynamic_disk_arr), 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
                                }
                            }
                        }
                         
						if ($sourceIdInfo['osFlag'] != 1)
						{
							$multi_path_status = $this->volObj->isMultiPathTrue($sourceHostId);
							if ($multi_path_status == TRUE)
							{
								ErrorCodeGenerator::raiseError($functionName, 'EC_MULTI_PATH_FOUND', array('ListVolumes' => implode(",", $src_vols),'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
							}
						}
						
						//Checks Given Source Volumes
						$src_vol_not_registered_arr = $this->volObj->checkDeviceexists($sourceHostId,$src_vols,$sourceIdInfo['osFlag'],1,'azure');
						$dest_device_array = array(); // as we are not getting target device list so passing as blank array.
						
						$retention_details = $this->retention_obj->getValidRetentionVolumes($MasterTarget,$RetentionDrive,$sourceHostId);				
						if (! is_array($retention_details) && ! $retention_details)	ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RETENTION_VOLUMES', array('RetentionDrive' => $RetentionDrive));
						if (! is_array($retention_details) && $retention_details)	ErrorCodeGenerator::raiseError($functionName, 'EC_NO_SUFFICIENT_SPACE_FOR_RETENTION', array('RetentionDrive' => $RetentionDrive));
						
						//if(count($retention_details) && array_key_exists($MasterTarget, $retention_details)) $retention_details = array_keys($retention_details[$MasterTarget]);
						//if(!in_array($RetentionDrive,$retention_details))  ErrorCodeGenerator::raiseError($functionName, EC_NO_RETENTION_VOLUMES,$RetentionDrive);
							
						if(count($src_vol_not_registered_arr)) {
							$src_vol_not_registered_str = implode(",",$src_vol_not_registered_arr);
							ErrorCodeGenerator::raiseError($functionName, 'EC_IS_SOURCE_VOLUME_ELIGIBLE', array('ListVolumes' => $src_vol_not_registered_str, 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
						}
					}
				}	
			}
			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();

		}
		catch(SQLException $sql_excep)
		{
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
	
    /*
        This API is using for E2A forward protection for V1 volume based protection.
        A2E failback protection for V2 disk based protection.
        Both above protections uses the InMage data plane.
    */
	public function CreateProtection ($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			global $requestXML, $PREPARE_TARGET_PENDING, $activityId, $clientRequestId, $CLOUD_PLAN_AZURE_SOLUTION,
$CLOUD_PLAN_VMWARE_SOLUTION, $CLOUD_PLAN_AZURE_SOLUTION_TYPE, $CLOUD_PLAN_VMWARE_SOLUTION_TYPE, $protection_direction,$CRASH_CONSISTENCY_INTERVAL,$DVACP_CRASH_CONSISTENCY_INTERVAL;
			global $BATCH_SIZE;
			
        
			$scenarioData = array();
			
			// update applicationPlan,applicationScenario and policy table during retry.
			if(isset($parameters['PlanId'])) 
			{
				$PlanId = trim($parameters['PlanId']->getValue());
				if($PlanId != '')
				{
					$planIds = $this->protectionObj->updatePrepareTarget($PlanId, $protectedPlanInfo);
					$planIds = ',' . $planIds . ',';					
					// insert API request details
					$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $planIds);
					$request_id = $api_ref['apiRequestId'];
					$protectedPlanInfo["planId"] = $PlanId;
					$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($protectedPlanInfo, $request_id);
					$response  = new ParameterGroup ( "Response" );
					$response->setParameterNameValuePair('RequestId', $request_id);
					$this->conn->sql_commit();
					$this->conn->disable_exceptions();
					return $response->getChildList();
				}
			}	
			
			$BatchResync = 3;
			if($parameters['ProtectionProperties'])
			{
				$Obj = $parameters['ProtectionProperties']->getChildList();
				
				// if amethyst.conf having BATCH_SIZE value then overriding with ASR passed BatchResync value.
				if(isset($BATCH_SIZE))
				{
					$BatchResync = $BATCH_SIZE;
				}
				else
				{
					$BatchResync = trim($Obj['BatchResync']->getValue());
				}
				
				$MultiVmSync = trim($Obj['MultiVmSync']->getValue());
				$RPOThreshold = trim($Obj['RPOThreshold']->getValue());
				$HourlyConsistencyPoints = trim($Obj['HourlyConsistencyPoints']->getValue());
				$ConsistencyInterval = trim($Obj['ConsistencyInterval']->getValue());
				$Compression = trim($Obj['Compression']->getValue());
                //For V2 failback,we are supporting vmware HyperVisor as the first step.Later we support to Physical, Hyper-v
                if ($protection_direction == "A2E")
                {
                    $HyperVisorType = "Vmware";
                    if ($Obj['HyperVisorType'])
                    {
                        $HyperVisorType = trim($Obj['HyperVisorType']->getValue());
                    }
                }
               
				if($Obj['AutoResyncOptions'])
				{
					$autoResyncObj = $Obj['AutoResyncOptions']->getChildList();
					$StartTime = trim($autoResyncObj['StartTime']->getValue()); 
					$EndTime = trim($autoResyncObj['EndTime']->getValue()); 
					$Interval = trim($autoResyncObj['Interval']->getValue());
				}
			}
			$PlanName = uniqid('Protection_', false);
			if($parameters['PlanName'])
			{
				$PlanName = trim($parameters['PlanName']->getValue());
			}
			
			if (strtolower($MultiVmSync) == 'on')
			{
				$CrashConsistencyInterval = $DVACP_CRASH_CONSISTENCY_INTERVAL;
			}
			else
			{
				$CrashConsistencyInterval = $CRASH_CONSISTENCY_INTERVAL;
			}
			if (isset($Obj['CrashConsistencyInterval']))
				$CrashConsistencyInterval = trim($Obj['CrashConsistencyInterval']->getValue());
			
			$consistencyOptions = array("ConsistencyInterval" => $ConsistencyInterval,"CrashConsistencyInterval" => $CrashConsistencyInterval);	
			$planDetails = serialize($consistencyOptions);
			//	insert plan details
            if ($protection_direction == "A2E") 
            {
                if (strtoupper($HyperVisorType) == strtoupper($CLOUD_PLAN_VMWARE_SOLUTION))
                {
                    $planId = $this->protectionObj->createPlan($PlanName,$CLOUD_PLAN_VMWARE_SOLUTION_TYPE,$BatchResync,0,$planDetails);	
                }
                //if ($HyperVisorType = $NON_HYPERVISOR_TYPE)
                //if ($HyperVisorType = $HYPERV_HYPERVISOR_TYPE)
            }
            else
            {
                $planId = $this->protectionObj->createPlan($PlanName,$CLOUD_PLAN_AZURE_SOLUTION_TYPE,$BatchResync,0,$planDetails);
            }
			
            if ($protection_direction == "A2E")
            {
                $sourceProtectionDirection = "A2E";
            }
            else
            {
                $sourceProtectionDirection = "E2A";
            }
			if($parameters['GlobalOptions'])
			{
				$globalOptObj = $parameters['GlobalOptions']->getChildList();
				$ProcessServer = trim($globalOptObj['ProcessServer']->getValue()); 

				$UseNatIPFor = 'None';
				if($globalOptObj['UseNatIPFor']) $UseNatIPFor = trim($globalOptObj['UseNatIPFor']->getValue());				
				$MasterTarget = trim($globalOptObj['MasterTarget']->getValue()); 

				$tgt_os_flag = $this->commonfunc_obj->get_osFlag($MasterTarget);

				if($tgt_os_flag == 1) $RetentionDrive = "R";
				else $RetentionDrive = "/mnt/retention";
				if($globalOptObj['RetentionDrive']) $RetentionDrive = trim($globalOptObj['RetentionDrive']->getValue());
				
				if($globalOptObj['ProtectionDirection']) $protectionDirection = trim($globalOptObj['ProtectionDirection']->getValue());   
			}
            
			$cfs_arr = $this->conn->sql_get_array("SELECT id, (CASE WHEN connectivityType = 'VPN' THEN privateIpAddress  ELSE publicIpAddress  END) as ipAddress from cfs", "id", "ipAddress");  

			if($parameters['SourceServers'])
			{
			  $sourceServerObj = $parameters['SourceServers']->getChildList();
			  
			   $serverCount = count($sourceServerObj);			   
			   $dvacp_consistency_set = 0;
			   if($serverCount > 1) $dvacp_consistency_set = 1;
				
				foreach($sourceServerObj as $v)
				{
					$sourceDetailsObj = $v->getChildList();
					$sourceHostId = trim($sourceDetailsObj['HostId']->getValue());
					
					if($sourceDetailsObj['ProcessServer']) $ProcessServer = trim($sourceDetailsObj['ProcessServer']->getValue());
					if($sourceDetailsObj['UseNatIPFor']) $UseNatIPFor = trim($sourceDetailsObj['UseNatIPFor']->getValue());
					if($sourceDetailsObj['MasterTarget']) $MasterTarget = trim($sourceDetailsObj['MasterTarget']->getValue());

					$src_os_flag = $this->commonfunc_obj->get_osFlag($sourceHostId);
					$tgt_os_flag = $this->commonfunc_obj->get_osFlag($MasterTarget);
					
					//Assign to default values in case if no scenario exists for that plan
					$defaults = $this->_get_defaults();
					$arr_global_options = $defaults['globalOption'];
					$arr_retention_options = $defaults['retentionOptions'];
					
					# If retention is set from ASR UI picking that value otherwise maintaining 72 hours retention.
					if($HourlyConsistencyPoints) 
					{
						$arr_retention_options['hid_seltime1'] = $HourlyConsistencyPoints;
						$arr_retention_options['hid_retention_text'] = $HourlyConsistencyPoints;
						$arr_retention_options['time_based'] = $HourlyConsistencyPoints;
					}	
					
					if($tgt_os_flag == 1) $RetentionDrive = "R";
					else $RetentionDrive = "/mnt/retention";
					
					if($sourceDetailsObj['RetentionDrive']) 
					{
						$RetentionDrive = trim($sourceDetailsObj['RetentionDrive']->getValue()); 
						$arr_retention_options['retentionPathVolume'] = $RetentionDrive;
					}
					
                    if($sourceDetailsObj['ProtectionDirection']) $sourceProtectionDirection = trim($sourceDetailsObj['ProtectionDirection']->getValue());
										
                    if(isset($sourceDetailsObj['SecureSourcetoPS']))
					{
						$secureSourcetoPS = trim($sourceDetailsObj['SecureSourcetoPS']->getValue()); 
						$arr_global_options['secureSrctoCX'] = ($secureSourcetoPS == "yes") ? 1 : 0;
					}	
					
					$arr_global_options['vacp'] = $MultiVmSync;
					$arr_global_options['rpo'] = $RPOThreshold;
					$arr_global_options['batchResync'] = $BatchResync;
					$arr_global_options['compression'] = ($Compression == "Enabled") ? 1 : 0;
					$arr_global_options['processServerid'] = $ProcessServer;
                    if ($sourceProtectionDirection == "A2E")
                    {
                        $arr_global_options['HyperVisorType'] = $HyperVisorType;
                    }
					
					if(($StartTime) && ($EndTime)) 
					{
						$StartTimeArr = explode(":", $StartTime);
						$EndTimeArr = explode(":", $EndTime);
						
						$arr_global_options['autoResync']['start_hr'] = $StartTimeArr[0];
						$arr_global_options['autoResync']['start_min'] = $StartTimeArr[1];
						$arr_global_options['autoResync']['stop_hr'] = $EndTimeArr[0];
						$arr_global_options['autoResync']['stop_min'] = $EndTimeArr[1];
						$arr_global_options['autoResync']['wait_min'] = $Interval;
					}
					
					if(strtolower($UseNatIPFor) == 'source') $arr_global_options['srcNATIPflag'] = '1';
					if(strtolower($UseNatIPFor) == 'target') $arr_global_options['trgNATIPflag'] = '1';
					if(strtolower($UseNatIPFor) == 'both')
					{
						$arr_global_options['srcNATIPflag'] = '1';
						$arr_global_options['trgNATIPflag'] = '1';
					}
					
					if($sourceProtectionDirection == "E2A" && $cfs_arr[$MasterTarget]) 
                    {
                        $arr_global_options['use_cfs']  = 1;
                    }
                    else
                    {
                        $arr_global_options['use_cfs']  = 0;
                    }
                    
					$sourceHostName = $this->commonfunc_obj->getHostName($sourceHostId);
					$targetHostName = $this->commonfunc_obj->getHostName($MasterTarget);
					$arr_pair_info = array($sourceHostId.'='.$MasterTarget => array(
																				'sourceId' => $sourceHostId,
																				'sourceName' => $sourceHostName,
																				'targetId' => $MasterTarget,
																				'targetName' => $targetHostName,
																				'retentionOptions' => $arr_retention_options
																				) );
					
                    $solution_type = ucfirst(strtolower($CLOUD_PLAN_AZURE_SOLUTION));
                    if ($sourceProtectionDirection == "A2E") 
                    {
                        if (strtoupper($HyperVisorType) == strtoupper($CLOUD_PLAN_VMWARE_SOLUTION))
                        {
                            $solution_type = ucfirst(strtolower($CLOUD_PLAN_VMWARE_SOLUTION));
                        }
                        else
                        {
                            //Physical solution
                            //Hyperv solution
                        }
                    }

					$unserializeScenarioDetails = array("globalOption" => $arr_global_options, 
											 "pairInfo" => $arr_pair_info, 
											 "planId" => $planId,
											 "planName" => $PlanName,
											 "isPrimary" => 1,
											 "protectionType" => "BACKUP PROTECTION",
                                             "protectionPath" => $sourceProtectionDirection,
											 "solutionType" => $solution_type,
											 "sourceHostId" => $sourceHostId,
											 "targetHostId" => $MasterTarget
											 );
					$scenarioDetails = serialize($unserializeScenarioDetails);
					
					$scenarioData['planId'] = $planId;
					$scenarioData['scenarioType'] = 'DR Protection';
					$scenarioData['scenarioName'] = '';
					$scenarioData['scenarioDescription'] = '';
					$scenarioData['scenarioVersion'] = '';
					$scenarioData['scenarioCreationDate'] = '';
					$scenarioData['scenarioDetails'] = $scenarioDetails;
					$scenarioData['validationStatus'] = '';
					$scenarioData['executionStatus'] = $PREPARE_TARGET_PENDING;
					$scenarioData['applicationType'] = $solution_type;
					$scenarioData['applicationVersion'] = '';
					$scenarioData['srcInstanceId'] = '';
					$scenarioData['trgInstanceId'] = '';
					$scenarioData['referenceId'] = '';
					$scenarioData['resyncLimit'] = '';
					$scenarioData['autoProvision'] = '';
					$scenarioData['sourceId'] = $sourceHostId;
					$scenarioData['targetId'] = $MasterTarget;
					$scenarioData['sourceVolumes'] = '';
					$scenarioData['targetVolumes'] = '';
					$scenarioData['retentionVolumes'] = '';
					$scenarioData['reverseReplicationOption'] = '';
					$scenarioData['protectionDirection'] = 'forward';
					$scenarioData['currentStep'] = '';
					$scenarioData['creationStatus'] = '';
					$scenarioData['isPrimary'] = '';
					$scenarioData['isDisabled'] = '';
					$scenarioData['isModified'] = '';
					$scenarioData['reverseRetentionVolumes'] = '';
					$scenarioData['volpackVolumes'] = '';
					$scenarioData['volpackBaseVolume'] = '';
					$scenarioData['isVolumeResized'] = '';
					$scenarioData['sourceArrayGuid'] = '';
					$scenarioData['targetArrayGuid'] = '';
					$scenarioData['rxScenarioId'] = '';
					
					// insert scenario details
					$scenarioId = $this->protectionObj->insertScenarioDetails($scenarioData);	
						
					$scenarioIdArr["$sourceHostId"] = $scenarioId;
					$targetIdArr[$scenarioId][] = $MasterTarget;
					
					// insert consistency policy
					if(((!$dvacp_consistency_set) || (($dvacp_consistency_set) && (strtolower($MultiVmSync) == 'off'))) && ($ConsistencyInterval)) 
					{
					   $policyId = $this->protectionObj->insert_consistency_policy($planId, $sourceHostId, $scenarioId, $consistencyOptions);
					   $referenceIds = $referenceIds . $policyId . ",";
					}
					
					// insert crash consistency policy
					if(((!$dvacp_consistency_set) || (($dvacp_consistency_set) && (strtolower($MultiVmSync) == 'off')) ) && ($CrashConsistencyInterval)) 
					{
                        #print "entered for  crash consistency policy.\n";
						$policyId = $this->protectionObj->insert_crash_consistency_policy($planId, $sourceHostId, $scenarioId, $consistencyOptions);
						$referenceIds = $referenceIds . $policyId . ",";
					}
					
					if($sourceDetailsObj['DiskMapping'])
					{
						$diskMapObj = $sourceDetailsObj['DiskMapping']->getChildList();
						foreach ($diskMapObj as $diskData)
						{
						  $diskInfoObj = $diskData->getChildList();
						  $SourceDiskName = trim($diskInfoObj['SourceDiskName']->getValue());
						  $TargetLUNId = trim($diskInfoObj['TargetLUNId']->getValue());
						  $diskMappingList[] = array("sourceHostId" => $sourceHostId, "targetHostId" => $MasterTarget, "SourceDiskName" => $SourceDiskName,
										"TargetDiskName" => $TargetDiskName, "TargetLUNId" => $TargetLUNId , "status" => "Disk Layout Pending");						  
						}
						// insert source and target disk details
						$this->protectionObj->insert_src_mt_disk_mapping($diskMappingList, $planId, $scenarioId);
					}
				}
			}	
			foreach($targetIdArr as $protectionScenarioId => $targetIdData)
			{
				$targetIdList = array_unique($targetIdData);
				foreach($targetIdList as $tgtId)
				{
					// insert prepare target policy
					$policyId = $this->protectionObj->insert_prepare_target($planId, $tgtId, $protectionScenarioId);
					$policyIdArr["$tgtId"] = $policyId;
					$referenceIds = $referenceIds . $policyId . ",";
				}
			}
			
            $dvacp_enable = FALSE;
            if(($dvacp_consistency_set) && (strtolower($MultiVmSync) == 'on') && ($ConsistencyInterval))
            {
                $srcIdsList = array_keys($scenarioIdArr);
                if ($sourceProtectionDirection == "A2E") 
                {
                       $dvacp_enable =  TRUE;
                }
                else //E2A case forward
                {
                    if ($src_os_flag == 1)
                    {
                        $dvacp_enable =  TRUE;
                    }
                }
                if ($dvacp_enable == TRUE)
                {
                    // insert dvacp policy
                    $policyId = $this->protectionObj->insert_dvacp_consistency_policy_v2($srcIdsList, $planId, $consistencyOptions);
                    $referenceIds = $referenceIds . $policyId . ",";
                }
            }
			
			$dvacp_enable = FALSE;
            if(($dvacp_consistency_set) && (strtolower($MultiVmSync) == 'on') && ($CrashConsistencyInterval))
            {
                $srcIdsList = array_keys($scenarioIdArr);
                if ($sourceProtectionDirection == "A2E") 
                {
                       $dvacp_enable =  TRUE;
                }
                if ($dvacp_enable == TRUE)
                {
                    // insert dvacp policy
					$policyId = $this->protectionObj->insert_dvacp_crash_consistency_policy($srcIdsList, $planId, $consistencyOptions);
                    $referenceIds = $referenceIds . $policyId . ",";
                }
            }
			
			$referenceIds = "," . $referenceIds;
			
			// insert API request details
			$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $referenceIds);
			$request_id = $api_ref['apiRequestId'];
			$requestedData = array("planId" => $planId, "scenarioId" => $scenarioIdArr, "policyId" => $policyIdArr);
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
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function ProtectionReadinessCheck_validate ($identity, $version, $functionName, $parameters)
	{
		global $REPLICATION_ROOT;
		$function_name_user_mode = 'Protection Readiness Check';
		try
		{
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			// Process server check
			if(isset($parameters['ProcessServer'])){
				$globalProcessServer = trim($parameters['ProcessServer']->getValue());
				if(!$globalProcessServer)ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ProcessServer"));
				
				// verify if ProcessServer is registered or not
				$globalProcessServerInfo = $this->commonfunc_obj->get_host_info($globalProcessServer);
				
				if((!$globalProcessServerInfo) || (!$globalProcessServerInfo['processServerEnabled'])){
				  ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
				}
			}
			
			// Master target check
			if(isset($parameters['MasterTarget'])){
				$globalMasterTarget = trim($parameters['MasterTarget']->getValue());
				if(!$globalMasterTarget)ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "MasterTarget"));
				
				// verifying if MasterTarget is registered or not
				$globalMasterTargetInfo = $this->commonfunc_obj->get_host_info($globalMasterTarget);
				
				if(!$globalMasterTargetInfo){
					ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
				}
				elseif(strtolower($globalMasterTargetInfo['agentRole']) != 'mastertarget'){
					ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
				}
			}
			//get source MBR information
			$dbSourceHostList = $this->commonfunc_obj->get_host_id_by_agent_role("Agent");
			$mbrPath = join(DIRECTORY_SEPARATOR, array($REPLICATION_ROOT, 'admin', 'web', 'SourceMBR'));
			
			//server mapping check
			if(isset($parameters['ServerMapping'])){
				$obj = $parameters['ServerMapping']->getChildList();
				if(count($obj) > 0){
					foreach($obj as $key=>$val){
							$configObj = $val->getChildList();
							if(isset($configObj['ProcessServer'])){
								$ProcessServer = trim($configObj['ProcessServer']->getValue());
								if(!$ProcessServer)ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ProcessServer"));
								
								$ProcessServerInfo = $this->commonfunc_obj->get_host_info($ProcessServer);
								
								if((!$ProcessServerInfo) || (!$ProcessServerInfo['processServerEnabled'])){
								  ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
								}
							}
							else{
								if(!$globalProcessServer){
									ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ProcessServer"));
								}
							}
							if(isset($configObj['Source'])){
								$Source = trim($configObj['Source']->getValue());
								if(!$Source)ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Source"));
								
								// verify if Source is registered or not
								$SourceInfo = $this->commonfunc_obj->get_host_info($Source);
								
								if(!$SourceInfo){
									ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
								}
								elseif(strtolower($SourceInfo['agentRole']) != 'agent'){
									ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
								}

								$srcOS = $this->commonfunc_obj->get_osFlag($Source);
								foreach($dbSourceHostList as $dbHostId => $latestMbrFile)
								{
									if(($dbHostId == $Source) && ($srcOS == 1))
									{
										if (empty($latestMbrFile) || !(file_exists($mbrPath . DIRECTORY_SEPARATOR . $dbHostId . DIRECTORY_SEPARATOR . $latestMbrFile))) 
										{	
											$dbSourceHostList = $this->commonfunc_obj->set_agent_refresh_state();
											ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MBR_FOUND', array('SourceHostName' => $SourceInfo['name'], 'SourceIP' => $SourceInfo['ipaddress']));
										}
									}
								}	
							}
							else{
								ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Source"));
							}
							if(isset($configObj['MasterTarget'])){
								$MasterTarget = trim($configObj['MasterTarget']->getValue());
								if(!$MasterTarget)ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "MasterTarget"));
								
								// verify if MasterTarget is registered or not
								$MasterTargetInfo = $this->commonfunc_obj->get_host_info($MasterTarget);
								
								if(!$MasterTargetInfo){
									ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
								}
								elseif(strtolower($MasterTargetInfo['agentRole']) != 'mastertarget'){
									ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
								}
							}
							else{
								$MasterTarget = $globalMasterTarget;
								if(!$globalMasterTarget){
									ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "MasterTarget"));
								}
							}
							
							// validate source and target already protected or not
							$existingPlanDetails = $this->protectionObj->isPlanExist($Source, 'DR Protection');
							
							if(count($existingPlanDetails) > 0){
								$existing_target_host = $this->commonfunc_obj->getHostName($existingPlanDetails[0]['targetId']);								
								$existing_target_host_ip = $this->commonfunc_obj->getHostIpaddress($existingPlanDetails[0]['targetId']);
								
								ErrorCodeGenerator::raiseError($functionName, 'EC_SRC_TGT_PROTECTED', array('SourceHostName' => $SourceInfo['name'], 'SourceIP' => $SourceInfo['ipaddress'], 'MTHostName' => $existing_target_host, 'MTIP' => $existing_target_host_ip));
							} 
					}
				}
				else{
					ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Source"));
				}
			}
			else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Source"));
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
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
	
	public function ProtectionReadinessCheck ($identity, $version, $functionName, $parameters)
	{
		try
		{	
			$this->conn->enable_exceptions();
			$response = new ParameterGroup ( "Response" );
			
			// get registered agent information
			$agentInfotArr = $this->systemObj->get_agent_heartbeat();
			if(isset($parameters['ServerMapping'])){
				$obj = $parameters['ServerMapping']->getChildList();
				
				foreach($obj as $key=>$val){
					$configurationObj = new ParameterGroup ($key);
					$configObj = $val->getChildList();
					if(isset($configObj['ProcessServer'])){
						$ProcessServer = trim($configObj['ProcessServer']->getValue());
						// verify agent heartbeat status
						if(array_key_exists($ProcessServer, $agentInfotArr)){
							$serverObj = new ParameterGroup ("$ProcessServer");
							$latestGMTTime = time(); 
							$ps_heartbeat_ts = $agentInfotArr["$ProcessServer"]["ps_heartbeat_ts"];
							if(($ps_heartbeat_ts+900) < $latestGMTTime){
							$serverObj->setParameterNameValuePair('ReadinessCheck','Failure');
							$serverObj->setParameterNameValuePair('ErrorMessage',"Agent not able to communicate with CS");
							}
							else{
								$serverObj->setParameterNameValuePair('ReadinessCheck','Success');
							}
							$configurationObj->addParameterGroup ($serverObj);
						}
					}
					else{
						if(isset($parameters['ProcessServer']))
						{
							$globalProcessServer = trim($parameters['ProcessServer']->getValue());
							// verify agent heartbeat status
							if(array_key_exists($globalProcessServer, $agentInfotArr)){
								$serverObj = new ParameterGroup ("$globalProcessServer");
								$latestGMTTime = time(); 
								$ps_heartbeat_ts = $agentInfotArr["$globalProcessServer"]["ps_heartbeat_ts"];
								if(($ps_heartbeat_ts+900) < $latestGMTTime){
								$serverObj->setParameterNameValuePair('ReadinessCheck','Failure');
								$serverObj->setParameterNameValuePair('ErrorMessage',"Agent not able to communicate with CS");
								}
								else{
									$serverObj->setParameterNameValuePair('ReadinessCheck','Success');
								}
							$configurationObj->addParameterGroup ($serverObj);
							}
						}
					}
					
					if(isset($configObj['Source'])){
						$Source = trim($configObj['Source']->getValue());
						// verify agent heartbeat status
						if(array_key_exists($Source, $agentInfotArr))
						{
							$serverObj = new ParameterGroup ("$Source");
							$latestGMTTime = time(); 
							$sentinel_ts = $agentInfotArr["$Source"]["sentinel_ts"];
							if(($sentinel_ts+900) < $latestGMTTime){
							$serverObj->setParameterNameValuePair('ReadinessCheck','Failure');
							$serverObj->setParameterNameValuePair('ErrorMessage',"Agent not able to communicate with CS");
							}
							else{
								$serverObj->setParameterNameValuePair('ReadinessCheck','Success');
							}
							$configurationObj->addParameterGroup ($serverObj);
						}
					}

					if(isset($configObj['MasterTarget'])){
						$MasterTarget = trim($configObj['MasterTarget']->getValue());
						// verify agent heartbeat status
						if(array_key_exists($MasterTarget, $agentInfotArr)){
							$serverObj = new ParameterGroup ("$MasterTarget");
							$latestGMTTime = time(); 
							$sentinel_ts = $agentInfotArr["$MasterTarget"]["sentinel_ts"];
							if(($sentinel_ts+900) < $latestGMTTime){
							$serverObj->setParameterNameValuePair('ReadinessCheck','Failure');
							$serverObj->setParameterNameValuePair('ErrorMessage',"Agent not able to communicate with CS");
							}
							else{
								$serverObj->setParameterNameValuePair('ReadinessCheck','Success');
							}
							$configurationObj->addParameterGroup ($serverObj);	
						}
					}
					else{
						if(isset($parameters['MasterTarget'])){
							$globalMasterTarget = trim($parameters['MasterTarget']->getValue());
							// verify agent heartbeat status
							if(array_key_exists($globalMasterTarget, $agentInfotArr)){
								$serverObj = new ParameterGroup ("$globalMasterTarget");
								$latestGMTTime = time(); 
								$sentinel_ts = $agentInfotArr["$globalMasterTarget"]["sentinel_ts"];
								if(($sentinel_ts+900) < $latestGMTTime){
								$serverObj->setParameterNameValuePair('ReadinessCheck','Failure');
								$serverObj->setParameterNameValuePair('ErrorMessage',"Agent not able to communicate with CS");
								}
								else{
									$serverObj->setParameterNameValuePair('ReadinessCheck','Success');
								}
								$configurationObj->addParameterGroup ($serverObj);
							}
						}
					}
					
					$response->addParameterGroup ($configurationObj);	
				}
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
			
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
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
	
	public function RestartResync_validate ($identity, $version, $functionName, $parameters)
	{
		$function_name_user_mode = 'Restart Resync';
		try
		{
			$this->conn->enable_exceptions();
			$ProtectionPlanId = '';
			
			// protection plan check
			if(isset($parameters['ProtectionPlanId']))
			{
				$ProtectionPlanId = trim($parameters['ProtectionPlanId']->getValue());
				if(!is_numeric($ProtectionPlanId)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionPlanId"));
				
				// verify if plan is protected or not
				$getPlanType = $this->protectionObj->getPlanType($ProtectionPlanId);
				if(!$getPlanType) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $ProtectionPlanId));
				
				// get scenario details to check provided source ids are protected or not
				$scenarioArr = $this->recoveryPlanObj->getScenarioDetails($ProtectionPlanId, 'DR Protection');
				if(count($scenarioArr) > 0)
				{
					foreach($scenarioArr as $scenarioData)
					{
						$scenarioId = $scenarioData['scenarioId'];
						$scenarioSourceIds[] = $scenarioData['sourceId'];
					}
				}
				else ErrorCodeGenerator::raiseError($functionName, 'EC_NO_SCENARIO_EXIST', array('PlanID' => $ProtectionPlanId));
			}
			
			if(isset($parameters['ForceResync']))
			{
				$ForceResync = trim($parameters['ForceResync']->getValue());
				if((strtolower($ForceResync) != 'yes') && (strtolower($ForceResync) != 'no')) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ForceResync"));
			}
			
			// server level check
			if(isset($parameters['Servers']))
			{
				$serverObj = $parameters['Servers']->getChildList();
				if(count($serverObj) > 0)
				{
					for($i=1;$i<=count($serverObj);$i++)
					{
						if(isset($serverObj['Server'.$i]))
						{
							$sourceId = trim($serverObj['Server'.$i]->getValue());
							$allSrcIds[] = $sourceId;
							$sourceIdInfo = $this->commonfunc_obj->get_host_info($sourceId);
							
							if(!$sourceIdInfo) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
							elseif(strtolower($sourceIdInfo['agentRole']) != 'agent') ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
							
							if($ProtectionPlanId && !in_array($sourceId, $scenarioSourceIds))ErrorCodeGenerator::raiseError($functionName, 'EC_NO_GUID_EXIST_IN_PLAN', array('SourceHostGUID' => $sourceId));
						}
						else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Server$i"));
					}
					
					$planIds = array();					
					// In case planId is not set then validating all the provided servers are part of same plan.
					if(!$ProtectionPlanId)
					{
						// get scenario details to check provided source ids are protected or not
						$scenarioArr = $this->recoveryPlanObj->getScenarioDetails('', 'DR Protection',$allSrcIds);
						// Verifying all the provided source servers are protected.
						if(count($scenarioArr) == count($allSrcIds))
						{
							foreach($scenarioArr as $scenarioData){
								$planIds[] = $scenarioData['planId'];
							}
							// Validating all the provided source servers belong to same plan.
							if(count($planIds) > 1) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_GUID_EXIST_IN_PLAN', array('SourceHostGUID' => $sourceId));
						}
						else ErrorCodeGenerator::raiseError($functionName, 'EC_NOT_PART_OF_PLAN'); 
					}
				}
				else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Server"));
			}
			
			// Validating atleast one parameter is set or not.
			if(!(isset($parameters['Servers'])) && !(isset($parameters['ProtectionPlanId']))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Server & ProtectionPlanId"));
			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
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
	
	public function RestartResync ($identity, $version, $functionName, $parameters)
	{
		try
		{	
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			$response = new ParameterGroup ( "Response" );
			$allSourceIds = array();
			$ProtectionPlanId = '';
			
			// get scenario list
			if(isset($parameters['ProtectionPlanId'])) $ProtectionPlanId = trim($parameters['ProtectionPlanId']->getValue());
			elseif(isset($parameters['Servers']))
			{
				$serverObj = $parameters['Servers']->getChildList();
				for($i=1;$i<=count($serverObj);$i++)
				{
					$allSourceIds[] = trim($serverObj['Server'.$i]->getValue());	
				}
			}
			
			$scenarioArr = $this->recoveryPlanObj->getScenarioDetails($ProtectionPlanId, 'DR Protection', $allSourceIds);
			
			$resyncRequiredFlag = 'NO';
			if(isset($parameters['ForceResync']))
			{
				$ForceResync = trim($parameters['ForceResync']->getValue());
				if(strtolower($ForceResync) == 'no') $resyncRequiredFlag = 'YES';
			}
			
			$serversObj = new ParameterGroup ("Servers");
			$j=1;
			foreach($scenarioArr as $scenarioData)
			{
				$scenarioSourceId = $scenarioData['sourceId'];
				$scenariotargetId = $scenarioData['targetId'];
				$hostObj = new ParameterGroup ('Server'.$j);
				if(isset($parameters['Servers']))
				{
					$serverObj = $parameters['Servers']->getChildList();
					for($i=1;$i<=count($serverObj);$i++)
					{
						$sourceId = trim($serverObj['Server'.$i]->getValue());
						if($scenarioSourceId === $sourceId)
						{
							// initiate restart resync for provided servers 
							$status = $this->volObj->RestartResyncReplicationPairsWithPause($scenarioSourceId,$scenariotargetId,$resyncRequiredFlag);
							$hostObj->setParameterNameValuePair('HostId',$scenarioSourceId);	
							$message = 'Success';
							if(!$status)$message = 'Failed';
							$hostObj->setParameterNameValuePair('Status',$message);
						}
					}
				}
				else
				{	
					// initiate restart  resync for all the servers
					$status = $this->volObj->RestartResyncReplicationPairsWithPause($scenarioSourceId,$scenariotargetId,$resyncRequiredFlag);
					$hostObj->setParameterNameValuePair('HostId',$scenarioSourceId);	
					$message = 'Success';
					if(!$status)$message = 'Failed';
					$hostObj->setParameterNameValuePair('Status',$message);	
				}
				$ProtectionPlanId = $scenarioData['planId'];
				$serversObj->addParameterGroup ($hostObj);
				$j++;
			}
			$response->setParameterNameValuePair('ProtectionPlanId',$ProtectionPlanId);
			$response->addParameterGroup ($serversObj);
			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
			$this->conn->sql_commit();
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
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}		
	}
	
	public function ModifyProtectionProperties_validate ($identity, $version, $functionName, $parameters)
	{
		try
		{
			global $BATCH_SIZE;
			$this->conn->enable_exceptions();
					
			if(isset($parameters['ProtectionPlanId'])){
				$ProtectionPlanId = trim($parameters['ProtectionPlanId']->getValue());
				// request for retry
				if(!is_numeric($ProtectionPlanId))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionPlanId"));
				
				// get plan to verify if plan is protected or not
				$getPlanType = $this->protectionObj->getPlanType($ProtectionPlanId);
				// get scenario details to check provided source ids are protected or not
				$scenarioArr = $this->recoveryPlanObj->getScenarioDetails($ProtectionPlanId, 'DR Protection');
				
				if(!$getPlanType)ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $ProtectionPlanId));
				
				if(!count($scenarioArr)){
					ErrorCodeGenerator::raiseError($functionName, 'EC_NO_SCENARIO_EXIST', array('PlanID' => $ProtectionPlanId));
				}
			}
			else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ProtectionPlanId"));
			}
			
			$param = false;
			// if amethyst.conf having BATCH_SIZE value then overriding with ASR passed BatchResync value.
			if(isset($BATCH_SIZE))
			{
				$param = true;
				if((!is_numeric($BATCH_SIZE)) || $BATCH_SIZE <= 0) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "BATCH_SIZE"));
			}
			else if(isset($parameters['BatchResync'])){
				$param = true;
				$BatchResync = trim($parameters['BatchResync']->getValue());
				if((!is_numeric($BatchResync)) || ($BatchResync < 0))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "BatchResync"));
			}
			
			if(isset($parameters['MultiVmSync'])){
				$param = true;
				$MultiVmSync = trim($parameters['MultiVmSync']->getValue());
				if((strtolower($MultiVmSync) != 'on') && (strtolower($MultiVmSync) != 'off')) {
					ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "MultiVmSync"));
				}
			}
			
			if(isset($parameters['RPOThreshold'])){
				$param = true;
				$RPOThreshold = trim($parameters['RPOThreshold']->getValue());
				if(!is_numeric($RPOThreshold))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RPOThreshold"));
			}
			
			if(isset($parameters['ConsistencyPoints'])){
				$param = true;
				$ConsistencyPoints = trim($parameters['ConsistencyPoints']->getValue());
				if(!is_numeric($ConsistencyPoints))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ConsistencyPoints"));
			}
			
			if(isset($parameters['ApplicationConsistencyPoints'])){
				$param = true;
				$ApplicationConsistencyPoints = trim($parameters['ApplicationConsistencyPoints']->getValue());
				if(!is_numeric($ApplicationConsistencyPoints))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ApplicationConsistencyPoints"));
			}
			
			if(isset($parameters['CrashConsistencyInterval'])){
				$param = true;
				$CrashConsistencyInterval = trim($parameters['CrashConsistencyInterval']->getValue());
				if(!is_numeric($CrashConsistencyInterval))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "CrashConsistencyInterval"));
			}
			
			if(isset($parameters['Compression'])){
			$param = true;	
			$Compression = trim($parameters['Compression']->getValue());
			if((strtolower($Compression) != 'enabled') && (strtolower($Compression) != 'disabled')) {
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Compression"));
			}
			}
			
			// validate auto resync option 
			if(isset($parameters['AutoResyncOptions'])){
				$param = true;	
			  $autoResyncObj = $parameters['AutoResyncOptions']->getChildList();
			  $patternMatch = '/^([01]?[0-9]|2[0-3]):[0-5][0-9]$/';
			  if(isset($autoResyncObj['StartTime'])){
				$StartTime = trim($autoResyncObj['StartTime']->getValue());
				preg_match($patternMatch, $StartTime, $StartTimeMatches);
				if(!$StartTimeMatches[0]){
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "StartTime"));
				}
			  }
			  else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "StartTime"));
			  }
			  
			  if(isset($autoResyncObj['EndTime'])){
				$EndTime = trim($autoResyncObj['EndTime']->getValue()); 	
				preg_match($patternMatch, $EndTime, $EndTimeMatches);
				if(!$EndTimeMatches[0]){
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "EndTime"));
				}
			  }
			  else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "EndTime"));
			  }
			  
			  if(isset($autoResyncObj['Interval'])){
				$Interval = trim($autoResyncObj['Interval']->getValue()); 
				if($Interval < 10){
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Interval"));
				}	
			  }
			   else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Interval"));
			  }
			}
			if(!$param){
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ModifyProtectionProperties"));
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
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
	
	public function ModifyProtectionProperties ($identity, $version, $functionName, $parameters)
	{
		try
		{
			global $BATCH_SIZE;
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			$ProtectionPlanId = trim($parameters['ProtectionPlanId']->getValue());
			
            $planDetails = $this->conn->sql_get_value("applicationPlan","planDetails","planId=?", array($ProtectionPlanId));
			$planDetailsArr = unserialize($planDetails);
		
			$consistencyInterval = $planDetailsArr['ConsistencyInterval'];
			$crashConsistencyInterval = $planDetailsArr['CrashConsistencyInterval'];
		
			if(isset($parameters['RPOThreshold'])){
			$RPOThreshold = trim($parameters['RPOThreshold']->getValue());
			}
			
			if(isset($parameters['MultiVmSync'])){
			$MultiVmSync = trim($parameters['MultiVmSync']->getValue());
			}
			
			if(isset($parameters['Compression'])){
			$Compression = trim($parameters['Compression']->getValue());
			}
			
			if(isset($parameters['ConsistencyPoints'])){
			$HourlyConsistencyPoints = trim($parameters['ConsistencyPoints']->getValue());
			}
			
			if(isset($parameters['ApplicationConsistencyPoints'])){
				$consistencyInterval = trim($parameters['ApplicationConsistencyPoints']->getValue());
			}
			$consistencyPolicyOptions["ConsistencyInterval"] = $consistencyInterval;
			if ($parameters['CrashConsistencyInterval'] && isset($parameters['CrashConsistencyInterval']))
			{
				$crashConsistencyInterval = trim($parameters['CrashConsistencyInterval']->getValue());
			}
			$consistencyPolicyOptions["CrashConsistencyInterval"] = $crashConsistencyInterval;
			
			if(isset($parameters['AutoResyncOptions'])){
			$autoResyncObj = $parameters['AutoResyncOptions']->getChildList();
			$StartTime = trim($autoResyncObj['StartTime']->getValue()); 
			$EndTime = trim($autoResyncObj['EndTime']->getValue()); 
			$Interval = trim($autoResyncObj['Interval']->getValue()); 
			
			$StartTimeArr = explode(":", $StartTime);
			$EndTimeArr = explode(":", $EndTime);
			
			}

			// if amethyst.conf having BATCH_SIZE value then overriding with ASR passed BatchResync value.
			if(isset($BATCH_SIZE))
			{
				$BatchResync = $BATCH_SIZE;
				$updateBatch = $this->protectionObj->updateBatchResync($ProtectionPlanId,$BatchResync);
			}
			else if(isset($parameters['BatchResync']))
			{
				$BatchResync = trim($parameters['BatchResync']->getValue());
				$updateBatch = $this->protectionObj->updateBatchResync($ProtectionPlanId,$BatchResync);
			}
			
			$sourceIdList = array();
			
			// get scenario details to check provided source ids are protected or not
			$scenarioArr = $this->recoveryPlanObj->getScenarioDetails($ProtectionPlanId, 'DR Protection');
			if(count($scenarioArr) > 0){
				foreach($scenarioArr as $scenarioData){
					$updateColumnStr = '';
					$parameterArr = array();
					$scenarioId = $scenarioData['scenarioId'];
					$sourceId = $scenarioData['sourceId'];
					$targetId = $scenarioData['targetId'];
					$scenarioDetails = $this->conn->sql_get_value('applicationscenario', 'scenarioDetails','scenarioId=?', array($scenarioId));
					$plan_properties = unserialize($scenarioDetails);
					/* $sessionBatchResync = $plan_properties['globalOption']['batchResync'];
					$sessionRpo = $plan_properties['globalOption']['rpo'];
					$sessionCompression = $plan_properties['globalOption']['compression'];
					$sessionAutoResync = $plan_properties['globalOption']['autoResync']; */
					
					if(isset($BatchResync)){
						$plan_properties['globalOption']['batchResync'] = $BatchResync;
					}
					
					if(isset($MultiVmSync)){
						if(strtolower($MultiVmSync) == 'on'){
							// insert dvacp
							$dvacpSourceIdList[] = $sourceId;
						}
						elseif(strtolower($MultiVmSync) == 'off'){
							
							// insert normal consistency
							$normalConsistencySourceList[] = $sourceId;
						}
						$plan_properties['globalOption']['vacp'] = $MultiVmSync;
					}
					
					$scenario_ids[] = $scenarioId;
					
					if(isset($RPOThreshold)){
						$plan_properties['globalOption']['rpo'] = $RPOThreshold;
						$updateColumnStr .= 'rpoSLAThreshold = ?';
						array_push($parameterArr, $RPOThreshold);
					}
					
					if(isset($Compression))
                    {
						$CompressionVal = 0;
						if(strtolower($Compression) == 'enabled'){
						$CompressionVal = 1;
						}
					
						if(isset($updateColumnStr) && $updateColumnStr != ''){
							$updateColumnStr .= ',';
						}
						
						$updateColumnStr .= 'compressionEnable = ?';
						array_push($parameterArr, $CompressionVal);
                        $plan_properties['globalOption']['compression'] = $CompressionVal;
					}
					
					if(isset($parameters['AutoResyncOptions'])){
						$plan_properties['globalOption']['autoResync']['isEnabled'] = 1;
						$plan_properties['globalOption']['autoResync']['start_hr'] = $StartTimeArr[0];
						$plan_properties['globalOption']['autoResync']['start_min'] = $StartTimeArr[1];
						$plan_properties['globalOption']['autoResync']['stop_hr'] = $EndTimeArr[0];
						$plan_properties['globalOption']['autoResync']['stop_min'] = $EndTimeArr[1];
						$plan_properties['globalOption']['autoResync']['wait_min'] = $Interval;
						
						if(isset($updateColumnStr) && $updateColumnStr != ''){
							$updateColumnStr .= ',';
						}
						
						$updateColumnStr .= "autoResyncStartTime = ?, autoResyncStartHours = ?, autoResyncStartMinutes = ?, autoResyncStopHours = ?, autoResyncStopMinutes = ?";
						array_push($parameterArr, $Interval, $StartTimeArr[0], $StartTimeArr[1], $EndTimeArr[0], $EndTimeArr[1]);
					
					}
					
					if((isset($RPOThreshold)) || (isset($Compression)) || (isset($AutoResyncOptions))){
						array_push($parameterArr, $sourceId, $targetId);
						$updateDb = $this->protectionObj->update_protection_properties($updateColumnStr, $parameterArr);
					}
					$protection_path = $plan_properties['protectionPath'];
					$str = serialize($plan_properties);
					$sql = "UPDATE
									applicationScenario
							SET
									scenarioDetails = ? 
							WHERE
									scenarioId = ? AND
									planId = ?";

					$rs = $this->conn->sql_query($sql, array($str, $scenarioId, $ProtectionPlanId));
				}
			}
			
			$planDetails = serialize($consistencyPolicyOptions);
			$this->conn->sql_query("UPDATE applicationPlan SET planDetails = ? WHERE planId = ?", array($planDetails,$ProtectionPlanId));
			
			 if (isset($consistencyInterval) && isset($crashConsistencyInterval) && isset($MultiVmSync))
			{
				$sql = "SELECT policyId,policyParameters,policyRunEvery FROM policy WHERE planId = ? AND policyType = '4'";
				$rs = $this->conn->sql_query($sql,array($ProtectionPlanId));
				
				if(is_array($rs) && (count($rs)>0)) 
				{
					foreach($rs as $k=>$v) 
					{
						$policyId = $v['policyId'];
						
						$del_dvap = "DELETE FROM policy WHERE policyId = ?";
						$rs = $this->conn->sql_query($del_dvap,array($policyId));
						
						$del_dvap_grp = "DELETE FROM consistencyGroups WHERE policyId = ?";
						$rs = $this->conn->sql_query($del_dvap_grp,array($policyId));
					}
				}
				
				if(count($dvacpSourceIdList) > 1)
				{                
						if ($consistencyPolicyOptions["ConsistencyInterval"] > 0)
						{
							$this->protectionObj->insert_dvacp_consistency_policy_v2($dvacpSourceIdList,$ProtectionPlanId, $consistencyPolicyOptions);	
						}
						
						if ($consistencyPolicyOptions["CrashConsistencyInterval"] > 0)
						{
							$this->protectionObj->insert_dvacp_crash_consistency_policy($dvacpSourceIdList,$ProtectionPlanId, $consistencyPolicyOptions);  
						}
				}
				elseif((count($normalConsistencySourceList) > 0 || count($dvacpSourceIdList) == 1) && $consistencyPolicyOptions)
				{
					$sourceServerList = $dvacpSourceIdList;
					if(count($normalConsistencySourceList) > 0)
					{
						$sourceServerList = $normalConsistencySourceList;
					}
					
					for($i=0;$i<count($sourceServerList);$i++)
					{
						if ($consistencyPolicyOptions["ConsistencyInterval"] > 0)
						{
							$policyId = $this->protectionObj->insert_consistency_policy($ProtectionPlanId, $sourceServerList[$i], $scenario_ids[$i], $consistencyPolicyOptions);
						}
						
						if ($consistencyPolicyOptions["CrashConsistencyInterval"] > 0)
						{
							$policyId = $this->protectionObj->insert_crash_consistency_policy($ProtectionPlanId, $sourceServerList[$i], $scenario_ids[$i], $consistencyPolicyOptions);
						}
					}
				}
			}
			
			if($HourlyConsistencyPoints) $this->protectionObj->updateRetentionWindow($ProtectionPlanId, $HourlyConsistencyPoints);
			
			// Disable Exceptions for DB
			$this->conn->sql_commit();
			$this->conn->disable_exceptions();
			
			//return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			$this->conn->sql_rollback();
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
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
		
	}
	
	public function ModifyProtection_validate ($identity, $version, $functionName, $parameters)
	{	
		global $REPLICATION_ROOT,$DISK_PROTECTION_UNIT,$VOLUME_PROTECTION_UNIT, $protection_direction;
		$function_name_user_mode = 'Modify Protection';
		
		try
		{
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			//Validate ProtectionPlanId
			$ProtectionPlanId = $this->validateParameter($parameters,'ProtectionPlanId',FALSE,'NUMBER');
			//Error: Raise error if PlanId does not exists
			$planName = $this->protectionObj->getPlanName($ProtectionPlanId, "CLOUD");
			if(!$planName) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $ProtectionPlanId));
			
			//Error: Servers Parameter is missed
			if((isset($parameters['Servers'])) === FALSE) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Servers"));
			
			//Validate source server details 
			$sourceServerObj = $parameters['Servers']->getChildList();
			if((is_array($sourceServerObj) === FALSE) || (count($sourceServerObj) == 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'Server'));
			
			//get source MBR information
			$dbSourceHostList = $this->commonfunc_obj->get_host_id_by_agent_role("Agent");
			$mbrPath = join(DIRECTORY_SEPARATOR, array($REPLICATION_ROOT, 'admin', 'web', 'SourceMBR'));
			
			$target_id_list = array();
			//$ret_vol_list = array();
			
			$cfs_mt_details = $this->conn->sql_get_array("SELECT id, (CASE WHEN connectivityType = 'VPN' THEN privateIpAddress  ELSE publicIpAddress  END) as ipAddress from cfs", "id", "ipAddress");
			$cluster_hosts = $this->commonfunc_obj->getAllClusters();
			
			foreach($sourceServerObj as $v)
			{
				$encryptedDiskList = array();
				$encryptedDisks = array();
				//Validate Source Server Details
				$sourceDetailsObj = $v->getChildList();

				// Validate HostId Parameter
				$sourceHostId = $this->validateParameter($sourceDetailsObj,'HostId',FALSE);
				//Error: Raise Error if Source Host Id is not registered with CS
				$srcCount = $this->conn->sql_get_value("hosts ", "count(*)", "id = ? AND sentinelEnabled = '1' AND outpostAgentEnabled = '1' AND agentRole = 'Agent'",array($sourceHostId));
				if($srcCount == 0) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
				
                if(isset($sourceDetailsObj['ProtectionDirection']))
				{
					$protection_direction = trim($sourceDetailsObj['ProtectionDirection']->getValue());
					if($protection_direction != "E2A" && $protection_direction != "A2E") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionDirection"));
				}
                
                $protection_unit = $VOLUME_PROTECTION_UNIT;
                if ($protection_direction == 'A2E')
                {
                    $protection_unit = $DISK_PROTECTION_UNIT;
                }
                
                $src_details = $this->commonfunc_obj->get_host_info($sourceHostId);
                
                if ($protection_unit == $VOLUME_PROTECTION_UNIT)
                {
                    foreach($dbSourceHostList as $dbHostId => $latestMbrFile)
                    {
                        if(($dbHostId == $sourceHostId) && ($src_details['osFlag'] == 1))
                        {
                            if (empty($latestMbrFile) || !(file_exists($mbrPath . DIRECTORY_SEPARATOR . $dbHostId . DIRECTORY_SEPARATOR . $latestMbrFile))) 
                            {	
                                $dbSourceHostList = $this->commonfunc_obj->set_agent_refresh_state();
                                ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MBR_FOUND', array('SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
                            }
                        }
                    }
				}
				foreach($cluster_hosts as $cluster_id => $cluster_data)
				{
					if(in_array($sourceHostId, $cluster_data)) ErrorCodeGenerator::raiseError($functionName, 'EC_CLUSTER_SERVER', array('SourceIP' => $src_details['ipaddress'], 'ErrorCode' => '5117'));
				}	
				
				// collect all the encrypted disks based upon hostid.	
				$encryptedDiskList = $this->commonfunc_obj->get_encrypted_disks($sourceHostId);
					
				// Validate ProcessServer Parameter
				$ProcessServer = $this->validateParameter($sourceDetailsObj,'ProcessServer',FALSE);
				//Error: Raise Error if ProcessServer Host Id is not registered with CS
				$ps_info = $this->commonfunc_obj->get_host_info($ProcessServer);
				if(!$ps_info || !$ps_info['processServerEnabled']) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));

				if($ps_info['currentTime'] - $ps_info['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ps_info['name'], 'PsIP' => $ps_info['ipaddress'], 'Interval' => ceil(($ps_info['currentTime'] - $ps_info['lastHostUpdateTimePs'])/60)));
				
				// Validate MasterTarget Parameter
				$MasterTarget = $this->validateParameter($sourceDetailsObj,'MasterTarget',FALSE);
				//Error: Raise Error if MasterTarget Host Id is not registered with CS
				$mt_info = $this->commonfunc_obj->get_host_info($MasterTarget);
				if($mt_info['agentRole'] != 'MasterTarget') ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
				if($mt_info['currentTime'] - $mt_info['lastHostUpdateTime'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_HEART_BEAT', array('MTHostName' => $mt_info['name'], 'MTIP' => $mt_info['ipaddress'], 'Interval' => ceil(($mt_info['currentTime'] - $mt_info['lastHostUpdateTime'])/60), 'OperationName' => $function_name_user_mode));
				
				$isMTSame = $this->volObj->isMasterTargetDifferent($ProtectionPlanId, $MasterTarget);
				if($isMTSame) ErrorCodeGenerator::raiseError($functionName, 'EC_DIFFERENT_MT_FOUND', array('PlanID' => $ProtectionPlanId));
                if ($protection_direction != 'A2E')
				{
                    if(!$cfs_mt_details[$MasterTarget]) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PUBLIC_IP', array('MTHostName' => $mt_info['name']));
                }
				$target_id_list[] = $MasterTarget;
				
				//Error: Raise error if already Source and Target are of different OS Platform
				$tgtOS = $this->commonfunc_obj->get_osFlag($MasterTarget);
				if($src_details['osFlag'] != $tgtOS) ErrorCodeGenerator::raiseError($functionName, 'EC_OS_CHECK');//,"HostId $sourceHostId and MasterTarget $MasterTarget");
				
				
				//Error: Raise error if already protection scenario exists for given source and target
				$count = $this->conn->sql_get_value("applicationScenario", "count(*)", "sourceId = ? AND targetId = ? AND scenarioType = ?",array($sourceHostId,$MasterTarget,'DR Protection'));
					
				//Retry the prepare target failed scenarios
				$policyState = 0;
				if($count > 0) {
					$scenarioId = $this->conn->sql_get_value("applicationScenario", "scenarioId", "sourceId = ? AND targetId = ? AND scenarioType = ?",array($sourceHostId,$MasterTarget,'DR Protection'));
					$policySql = "SELECT b.policyState AS policyState FROM policy a, policyInstance b WHERE a.policyId = b.policyId AND a.policyType = '5' AND a.planId = ? AND a.scenarioId = ? AND a.hostId = ?";
					$rs = $this->conn->sql_query($policySql,array($ProtectionPlanId,$scenarioId,$MasterTarget));
					for($i=0;isset($rs[$i]);$i++) {
						$policyState = $rs[$i]['policyState'];
					}
				}
				if($policyState!=2) {
					if($count > 0) ErrorCodeGenerator::raiseError($functionName, 'EC_SRC_TGT_PROTECTED', array('SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress'], 'MTHostName' => $mt_info['name'], 'MTIP' => $mt_info['ipaddress']));
				}
				
				// Validate RetentionDrive Parameter
				if($tgtOS == 1) $RetentionDrive = 'R';
				else $RetentionDrive = '/mnt/retention';
				
				if(isset($sourceDetailsObj['RetentionDrive'])) {
					$RetentionDrive = trim($sourceDetailsObj['RetentionDrive']->getValue());
					if(!$RetentionDrive) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RetentionDrive"));
				}
				//$RetentionDrive = $this->validateParameter($sourceDetailsObj,'RetentionDrive',TRUE);				
				//$ret_vol_list[$MasterTarget] = $RetentionDrive;
				
				$retention_details = $this->retention_obj->getValidRetentionVolumes($MasterTarget,$RetentionDrive,$sourceHostId);
				if (! is_array($retention_details) && ! $retention_details)	ErrorCodeGenerator::raiseError($functionName, 'EC_NO_RETENTION_VOLUMES', array('RetentionDrive' => $RetentionDrive));
				if (! is_array($retention_details) && $retention_details)	ErrorCodeGenerator::raiseError($functionName, 'EC_NO_SUFFICIENT_SPACE_FOR_RETENTION', array('RetentionDrive' => $RetentionDrive));
				
				//if(count($retention_details) && array_key_exists($MasterTarget, $retention_details)) $retention_details = array_keys($retention_details[$MasterTarget]);			
				//if(!in_array($RetentionDrive,$retention_details))  ErrorCodeGenerator::raiseError($functionName, EC_NO_RETENTION_VOLUMES,$RetentionDrive);
				
				
				// Validate UseNatIPFor Parameter
				$UseNatIPFor = $this->validateParameter($sourceDetailsObj,'UseNatIPFor',TRUE);
				if($UseNatIPFor) {
					//Validate Value for UseNatIPFor
					$validValues = array('source','target','both','none');
					if((in_array(strtolower($UseNatIPFor),$validValues)) === FALSE) {
						ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "UseNatIPFor"));
					}
				}

				//Error: DiskMapping Parameter is missed
				if((isset($sourceDetailsObj['DiskMapping'])) === FALSE) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "DiskMapping"));
				
				//Validate DiskMapping Details
				$diskMapObj = $sourceDetailsObj['DiskMapping']->getChildList();
				$SrcDeviceList = array();
				if((is_array($diskMapObj) === FALSE) || (count($diskMapObj) == 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'Disk'));
				unset($encryptedStr);
				foreach ($diskMapObj as $diskData)
				{
					$diskInfoObj = $diskData->getChildList();
					// Validate SourceDiskName Parameter
					$SourceDiskName = $this->validateParameter($diskInfoObj,'SourceDiskName',FALSE);
					$SrcDeviceList[] = $SourceDiskName;
					
					// Validate TargetLUNId Parameter
					$TargetLUNId = trim($diskInfoObj['TargetLUNId']->getValue()); 
                    if($TargetLUNId == "") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "TargetLUNId"));
					
					if(in_array("$SourceDiskName", $encryptedDiskList)) 
				    {
						$encryptedDisks = array_keys($encryptedDiskList, "$SourceDiskName");
						$encryptedStr .= implode(",", $encryptedDisks);
						$encryptedStr = $encryptedStr.",";
					}
				}
				
				//Validating source disk encryption.
			    // Protection needs to block if disk is encrypted.
			    if(isset($encryptedStr))
			    {
					ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_ENCRYPTED', array('ListVolumes' => rtrim($encryptedStr,","), 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
			    }	
					   
				//Validating source disks for UEFI partitions/dynamic disk
				$src_vols_str = implode(",", $SrcDeviceList);						
				
                //Validating source disks for UEFI partition/dynamic disks. This is applicable only for windows.
                if ($src_details['osFlag'] == 1)
                {    
                    $validation_status = $this->volObj->check_disk_signature_validation($sourceHostId, $SrcDeviceList, $src_vols_str, $protection_direction);
                    
                    if ($validation_status["validation"] == FALSE)
                    {
                        if ($validation_status["disk_ref"] == "SIGNATURE")
                        {
                            ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_SIGNATURE_INPUT_INVALID', array('ListVolumes' => $src_vols_str, 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
                        }
                        elseif($validation_status["disk_ref"] == "NAME")
                        {
                            ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_NAME_INPUT_INVALID', array('ListVolumes' => $src_vols_str, 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
                        }
                    }
                    
                    list($uefi_disk_arr, $dynamic_disk_arr) = $this->volObj->check_disk_eligibility($sourceHostId, $src_vols_str);
                    
                    //For E2A, it is forward protection and is volume based protection
                    if ($protection_unit == $VOLUME_PROTECTION_UNIT)
                    {
                        /*
                            For V1 block any dynamic disk whether it is data or boot dynamic disk.
                        */
                        if(count($dynamic_disk_arr) >= 1) 
                        {
                            ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_DYNAMIC', array('ListVolumes' => implode(",", $dynamic_disk_arr), 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
                        }
                    }
                    //For A2E, it is failback protection and it is disk based protection.
                    elseif ($protection_unit == $DISK_PROTECTION_UNIT)
                    { 
                        /*
                            Boot Dynamic disk:- V2 does not support
                            Data Dynamic disk:- V2 can support this
                        */
                        $dynamic_boot_disk_status = $this->volObj->is_dynamic_boot_disk_exists($sourceHostId, $src_vols);
                        
                        if($dynamic_boot_disk_status == TRUE) 
                        {
                            ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_DYNAMIC', array('ListVolumes' => implode(",", $dynamic_disk_arr), 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
                        }
                    }
                }
				
				if ($src_details['osFlag'] != 1)
				{
					$multi_path_status = $this->volObj->isMultiPathTrue($sourceHostId);
				
					if ($multi_path_status == TRUE)
					{
						ErrorCodeGenerator::raiseError($functionName, 'EC_MULTI_PATH_FOUND', array('ListVolumes' => implode(",", $SrcDeviceList),'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
					}
				}	
   
				//Error: Raise Error if the specified source devices are not registered with CS
				$unregisteredSRCDiskList = $this->volObj->checkDeviceexists($sourceHostId,$SrcDeviceList,$src_details['osFlag'],1,'azure');
				if(count($unregisteredSRCDiskList)) {
					$unregisteredSRCDiskListStr = implode(",",$unregisteredSRCDiskList);
					ErrorCodeGenerator::raiseError($functionName, 'EC_IS_SOURCE_VOLUME_ELIGIBLE', array('ListVolumes' => $unregisteredSRCDiskListStr, 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
				}
			}	
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
		}
		catch(SQLException $sql_excep)
		{
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
	
	/*
		Function to Add Server to Existing Protection Plan
	*/
	public function ModifyProtection ($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			/*
				ApplicationScenario
					- Get the Protection Plan Properties from already configured scenarios
					- Insert into applicationScenario
				srcmtdiskmapping 
					- Insert into srcmtdiskmapping for each source
				Policy
					- Insert PrepareTarget policy for each scenario
					- Update consistencyPolicy
				API Request
					- Insert into apirequest/apirequestdetails tables
				Response
					- return the new response Id
			*/
			
			/*
				ApplicationScenario
			*/
			global $PREPARE_TARGET_PENDING,$requestXML,$SCENARIO_DELETE_PENDING, $activityId,$clientRequestId,$CLOUD_PLAN_AZURE_SOLUTION,$CLOUD_PLAN_VMWARE_SOLUTION;
			
			$ProtectionPlanId = trim($parameters['ProtectionPlanId']->getValue());
			$planName = $this->conn->sql_get_value("applicationPlan", "planName", "planId = ?",array($ProtectionPlanId));
						
			$sql = "SELECT sourceId,scenarioDetails FROM applicationScenario WHERE planId = ? AND scenarioType = ? AND executionStatus != ?";
			$rs = $this->conn->sql_query($sql,array($ProtectionPlanId,'DR Protection',$SCENARIO_DELETE_PENDING));
			$protectedSrcIds = array();
			for($i=0;isset($rs[$i]);$i++) {
				$protectedSrcIds[] = $rs[$i]['sourceId'];
				$existingScenarioDetails = $rs[$i]['scenarioDetails'];
				
			}
			
			//Assign to default values in case if no scenario exists for that plan
			$defaults = $this->_get_defaults();
			$globalOption = $defaults['globalOption'];
			$retentionOptions = $defaults['retentionOptions'];
			
			$distributedVacp = 'off';
			if($existingScenarioDetails) {
				$existingScenarioDetailsArr = unserialize($existingScenarioDetails);
				$globalOption = $existingScenarioDetailsArr['globalOption'];
				$distributedVacp = strtolower($globalOption['vacp']);
				
				foreach($existingScenarioDetailsArr['pairInfo'] as $k=>$v) {
					$retentionOptions = $v['retentionOptions'];
				}
			}
			
			$planDetails = $this->conn->sql_get_value("applicationPlan","planDetails","planId=?", array($ProtectionPlanId));
			$planDetailsArr = unserialize($planDetails);
			$consistencyInterval = $planDetailsArr['ConsistencyInterval'];
			$crashConsistencyInterval = $planDetailsArr['CrashConsistencyInterval'];
			
			$prpTrgPolicyIdList = array();
			$srcScenarioList = array();
			$sourceServerObj = $parameters['Servers']->getChildList();
			            
            $cfs_arr = $this->conn->sql_get_array("SELECT id, (CASE WHEN connectivityType = 'VPN' THEN privateIpAddress  ELSE publicIpAddress  END) as ipAddress from cfs", "id", "ipAddress");                      

			foreach($sourceServerObj as $v)
			{
				$scenarioData=array();
				$sourceDetailsObj = $v->getChildList();
				$srcHostId = trim($sourceDetailsObj['HostId']->getValue());
				$psId = trim($sourceDetailsObj['ProcessServer']->getValue());
				$destId = trim($sourceDetailsObj['MasterTarget']->getValue());
				
				//Retry the prepare target failed scenarios
				$count = $this->conn->sql_get_value("applicationScenario", "count(*)", "sourceId = ? AND targetId = ? AND scenarioType = ? AND planId = ?",array($srcHostId,$destId,'DR Protection',$ProtectionPlanId));
				$policyState = 0;
				if($count > 0) {
					$scenarioId = $this->conn->sql_get_value("applicationScenario", "scenarioId", "sourceId = ? AND targetId = ? AND scenarioType = ? AND planId = ?",array($srcHostId,$destId,'DR Protection',$ProtectionPlanId));
					$policySql = "SELECT a.policyId AS policyId, b.policyState AS policyState FROM policy a, policyInstance b WHERE a.policyId = b.policyId AND a.policyType = '5' AND a.planId = ? AND a.scenarioId = ? AND a.hostId = ?";
					$rs = $this->conn->sql_query($policySql,array($ProtectionPlanId,$scenarioId,$destId));
					for($i=0;isset($rs[$i]);$i++) {
						$policyId = $rs[$i]['policyId'];
						$policyState = $rs[$i]['policyState'];
					}
					if($policyState == 2) {
						//Delete the existing scenario
						$delSrcMT = "DELETE FROM srcMtDiskMapping WHERE scenarioId = ?";
						$rsSrcMT = $this->conn->sql_query($delSrcMT, array($scenarioId));
						
						$delPrepareTrg = "DELETE FROM policy WHERE scenarioId = ?";
						$rsPrepareTrg = $this->conn->sql_query($delPrepareTrg, array($scenarioId));
						
						$delConsistency = "DELETE FROM policy WHERE planId = ? AND hostId = ?";
						$rsPrepareTrg = $this->conn->sql_query($delConsistency, array($ProtectionPlanId,$srcHostId));
						
						$delScenario = "DELETE FROM applicationScenario WHERE scenarioId = ?";
						$rsScenario = $this->conn->sql_query($delScenario, array($scenarioId));
					}
				}
				
				$tgtOS = $this->commonfunc_obj->get_osFlag($destId);
				if($tgtOS == 1) $retentionOptions['retentionPathVolume'] = 'R';
				else $retentionOptions['retentionPathVolume'] = '/mnt/retention';
				
				if($sourceDetailsObj['RetentionDrive']) 
				{
					$RetentionDrive = $sourceDetailsObj['RetentionDrive']->getValue(); 
					$retentionOptions['retentionPathVolume'] = $RetentionDrive;
				}
				$protectionDirection = "E2A";
				if($sourceDetailsObj['ProtectionDirection']) $protectionDirection = trim($sourceDetailsObj['ProtectionDirection']->getValue()); 
				
				if(isset($sourceDetailsObj['SecureSourcetoPS']))
				{
					$secureSourcetoPS = trim($sourceDetailsObj['SecureSourcetoPS']->getValue()); 
					$arr_global_options['secureSrctoCX'] = ($secureSourcetoPS == "yes") ? 1 : 0;
				}	
				
				$UseNatIPFor = 'none';
				if($sourceDetailsObj['UseNatIPFor']) {
					$UseNatIPFor = trim($sourceDetailsObj['UseNatIPFor']->getValue());
				}
				
				$globalOption['processServerid'] = $psId;
				switch(strtolower($UseNatIPFor))
				{
					case 'source':
						$globalOption['srcNATIPflag'] = '1';
						$globalOption['trgNATIPflag'] = '0';
						break;
					
					case 'target':
						$globalOption['srcNATIPflag'] = '0';
						$globalOption['trgNATIPflag'] = '1';
						break;
						
					case 'both':
						$globalOption['srcNATIPflag'] = '1';
						$globalOption['trgNATIPflag'] = '1';
						break;
						
					default:
						$globalOption['srcNATIPflag'] = '0';
						$globalOption['trgNATIPflag'] = '0';
				}
				
				if($protectionDirection == "E2A" && $cfs_arr[$destId]) 
                {
                    $globalOption['use_cfs']  = 1;
                }
                else
                {
                    $globalOption['use_cfs']  = 0;
                }

				$sourceHostName = $this->commonfunc_obj->getHostName($srcHostId);
				$targetHostName = $this->commonfunc_obj->getHostName($destId);
				
				$pairInfo = array($srcHostId.'='.$destId => array(
																'sourceId' => $srcHostId,
																'sourceName' => $sourceHostName,
																'targetId' => $destId,
																'targetName' => $targetHostName,
																'retentionOptions' => $retentionOptions
																));
                $solution_type = ucfirst(strtolower($CLOUD_PLAN_AZURE_SOLUTION));
                if ($protectionDirection == "A2E") 
                {
                    $solution_type = ucfirst(strtolower($CLOUD_PLAN_VMWARE_SOLUTION));
                }
				$scenarioDetails = array("globalOption" => $globalOption, 
										 "pairInfo" => $pairInfo, 
										 "planId" => $ProtectionPlanId,
										 "planName" => $PlanName,
										 "isPrimary" => 1,
										 "protectionType" => "BACKUP PROTECTION",
                                         "protectionPath" => $protectionDirection,
										 "solutionType" => $solution_type,
										 "sourceHostId" => $srcHostId,
										 "targetHostId" => $destId
										);
				$scenarioDetailsStr = serialize($scenarioDetails);
				
				$scenarioData['planId'] = $ProtectionPlanId;
				$scenarioData['scenarioType'] = 'DR Protection';
				$scenarioData['scenarioName'] = '';
				$scenarioData['scenarioDescription'] = '';
				$scenarioData['scenarioVersion'] = '';
				$scenarioData['scenarioCreationDate'] = '';
				$scenarioData['scenarioDetails'] = $scenarioDetailsStr;
				$scenarioData['validationStatus'] = '';
				$scenarioData['executionStatus'] = $PREPARE_TARGET_PENDING;
				$scenarioData['applicationType'] = $solution_type;
				$scenarioData['applicationVersion'] = '';
				$scenarioData['srcInstanceId'] = '';
				$scenarioData['trgInstanceId'] = '';
				$scenarioData['referenceId'] = '';
				$scenarioData['resyncLimit'] = '';
				$scenarioData['autoProvision'] = '';
				$scenarioData['sourceId'] = $srcHostId;
				$scenarioData['targetId'] = $destId;
				$scenarioData['sourceVolumes'] = '';
				$scenarioData['targetVolumes'] = '';
				$scenarioData['retentionVolumes'] = '';
				$scenarioData['reverseReplicationOption'] = '';
				$scenarioData['protectionDirection'] = 'forward';
				$scenarioData['currentStep'] = '';
				$scenarioData['creationStatus'] = '';
				$scenarioData['isPrimary'] = '';
				$scenarioData['isDisabled'] = '';
				$scenarioData['isModified'] = '';
				$scenarioData['reverseRetentionVolumes'] = '';
				$scenarioData['volpackVolumes'] = '';
				$scenarioData['volpackBaseVolume'] = '';
				$scenarioData['isVolumeResized'] = '';
				$scenarioData['sourceArrayGuid'] = '';
				$scenarioData['targetArrayGuid'] = '';
				$scenarioData['rxScenarioId'] = '';
				
				// insert scenario details
				$scenarioId = $this->protectionObj->insertScenarioDetails($scenarioData);
				$srcScenarioList[$srcHostId] = $scenarioId;
				
				if($sourceDetailsObj['DiskMapping'])
				{
					$diskMapObj = $sourceDetailsObj['DiskMapping']->getChildList();
					foreach ($diskMapObj as $diskData)
					{
					  $diskInfoObj = $diskData->getChildList();
					  $SourceDiskName = $diskInfoObj['SourceDiskName']->getValue();
					  $TargetLUNId = $diskInfoObj['TargetLUNId']->getValue();
					  $diskMappingList[] = array("sourceHostId" => $srcHostId, "targetHostId" => $destId, "SourceDiskName" => $SourceDiskName,
									"TargetDiskName" => '', "TargetLUNId" => $TargetLUNId , "status" => "Disk Layout Pending");						  
					}
					// insert source and target disk details
					$this->protectionObj->insert_src_mt_disk_mapping($diskMappingList, $ProtectionPlanId, $scenarioId);
				}
				
				// insert Policy Details
				$policyId = $this->protectionObj->insert_prepare_target($ProtectionPlanId, $destId, $scenarioId);
				$prpTrgPolicyIdList[] = $policyId;
				$referenceIds = $referenceIds . $policyId . ",";
				
				$srcOS = $this->commonfunc_obj->get_osFlag($srcHostId);
				
				// insert consistency policy
				if(($distributedVacp == 'off') && $consistencyInterval) 
				{
					// insert normal consistency policy
					$policyId = $this->protectionObj->insert_consistency_policy($ProtectionPlanId, $srcHostId, $scenarioId, $planDetailsArr);
					$referenceIds = $referenceIds . $policyId . ",";
				} 
				
				if(($distributedVacp == 'off')  &&  $crashConsistencyInterval)
				{
					#print "entered for dvcap consistency policy.\n";
					$policyId = $this->protectionObj->insert_crash_consistency_policy($ProtectionPlanId, $srcHostId, $scenarioId, $planDetailsArr);
					$referenceIds = $referenceIds . $policyId . ",";
				}
				
			}
						
			if($distributedVacp == 'on')
			{
				$srcHostIdAry = array_keys($srcScenarioList);
				$allSrcIds = array_merge($srcHostIdAry,$protectedSrcIds);
				$allSrcIds = array_unique($allSrcIds);
				
				$sql = "SELECT policyId,policyParameters,policyRunEvery FROM policy WHERE planId = ? AND policyType = '4'";
				$rs = $this->conn->sql_query($sql,array($ProtectionPlanId));
				
				if(is_array($rs) && (count($rs)>0)) 
				{
					foreach($rs as $k=>$v) 
					{
						$policyId = $v['policyId'];
						
						$del_dvap = "DELETE FROM policy WHERE policyId = ?";
						$rs = $this->conn->sql_query($del_dvap,array($policyId));
						
						$del_dvap_grp = "DELETE FROM consistencyGroups WHERE policyId = ?";
						$rs = $this->conn->sql_query($del_dvap_grp,array($policyId));
					}
				}
                
                #print_r($allSrcIds);
                
				if ($consistencyInterval) 
				{	
					$policyId = $this->protectionObj->insert_dvacp_consistency_policy_v2($allSrcIds,$ProtectionPlanId, $planDetailsArr);	
					$referenceIds = $referenceIds . $policyId . ",";
				}
				
				if($crashConsistencyInterval) 
				{
					$policyId = $this->protectionObj->insert_dvacp_crash_consistency_policy($allSrcIds,$ProtectionPlanId, $planDetailsArr);
					$referenceIds = $referenceIds . $policyId . ",";
				}  
			}
			$referenceIds = "," . $referenceIds;
			

			$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $referenceIds);
			$request_id = $api_ref['apiRequestId'];
			$requestedData = array("planId" => $ProtectionPlanId, "scenarioId" => $srcScenarioList, "policyId" => $prpTrgPolicyIdList);
			$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($requestedData, $request_id);
			
			$response  = new ParameterGroup ("Response");
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
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}	
	}
	
	/*
		Assign Defaults for Scenario Details
	*/
	private function _get_defaults()
	{
		$defaults = array();
		$globalOptions = array();
		$defaults = array();
		
		//Defaults for Global Options
		$globalOptions = array('secureSrctoCX' => 1,'secureCXtotrg' => 1,'vacp' => 'On','syncOptions' => 2,'syncCopyOptions' => 'full','resynThreshold' => 8192,'rpo' => 30,'batchResync' => 3,'autoResync' => array('isEnabled' => 1,'start_hr' => '12','start_min' => '00','stop_hr' => '18','stop_min' => '00','wait_min' => '30'),'diffThreshold' => 8192,'compression' => 'Enabled','processServerid' => '','srcNATIPflag' => 0,'trgNATIPflag' => 0,'use_cfs' => 0);
		
		//Defaults for Retention Options always maintaining 72 hours(3 days).
		$retentionOptions['time_based'] = '72';
        $retentionOptions['timewindow_count'] = '5';
        $retentionOptions['time_based_interval'] = '';
        $retentionOptions['hid_configure_capacity'] = '';
        $retentionOptions['hid_application0'] = 'all';
        $retentionOptions['hid_bookmark0'] = '0';
        $retentionOptions['hid_userDefined0'] = '';
        $retentionOptions['hid_granularity1'] = '0';
        $retentionOptions['hid_granularity_range1'] = 'hour';
        $retentionOptions['SparseEnable'] = '1';
        $retentionOptions['hid_seltime_range1'] = 'hour';
        $retentionOptions['hid_seltime1'] = '72';
        $retentionOptions['hid_add_remove1'] = 'Enable';
        $retentionOptions['seltime_range1'] = 'hour';
        $retentionOptions['hid_application1'] = 'File System';
        $retentionOptions['hid_bookmark1'] = '1';
        $retentionOptions['hid_userDefined1'] = '';
		
		//Considering the first retention window value provided from ASR.
        $retentionOptions['hid_granularity2'] = '1';
        $retentionOptions['hid_granularity_range2'] = 'hour';
        $retentionOptions['hid_seltime_range2'] = 'hour';
        $retentionOptions['hid_seltime2'] = "0";
        $retentionOptions['hid_add_remove2'] = 'Disable';
        $retentionOptions['seltime_range2'] = 'hour';
        $retentionOptions['hid_application2'] = 'File System';
        $retentionOptions['hid_bookmark2'] = '1';
        $retentionOptions['hid_userDefined2'] = '';
		
        $retentionOptions['hid_granularity3'] = '1';
        $retentionOptions['hid_granularity_range3'] = 'day';
        $retentionOptions['hid_seltime_range3'] = 'week';
        $retentionOptions['hid_seltime3'] = '0';
        $retentionOptions['hid_add_remove3'] = 'Disable';
        $retentionOptions['seltime_range3'] = '';
        $retentionOptions['hid_application3'] = 'File System';
        $retentionOptions['hid_bookmark3'] = '1';
        $retentionOptions['hid_userDefined3'] = '';
        $retentionOptions['hid_granularity4'] = '1';
        $retentionOptions['hid_granularity_range4'] = 'week';
        $retentionOptions['hid_seltime_range4'] = 'month';
        $retentionOptions['hid_seltime4'] = '0';
        $retentionOptions['hid_add_remove4'] = 'Disable';
        $retentionOptions['seltime_range4'] = '';
        $retentionOptions['hid_application4'] = 'File System';
        $retentionOptions['hid_bookmark4'] = '1';
        $retentionOptions['hid_userDefined4'] = '';
        $retentionOptions['hid_granularity5'] = '1';
        $retentionOptions['hid_granularity_range5'] = 'month';
        $retentionOptions['hid_seltime_range5'] = 'year';
        $retentionOptions['hid_seltime5'] = '0';
        $retentionOptions['hid_add_remove5'] = 'Disable';
        $retentionOptions['seltime_range5'] = '';
        $retentionOptions['time_based_flag'] = '1';
        $retentionOptions['space_based'] = '';
        $retentionOptions['change_by_size'] = '';
        $retentionOptions['selUnits'] = '-1';
        $retentionOptions['unlimited_space_hid'] = '1';
        $retentionOptions['hid_unit'] = '';
        $retentionOptions['logExceedPolicy'] = '0';
        $retentionOptions['txtdiskthreshold'] = '80';
        $retentionOptions['hid_retention_text'] = '72';
        $retentionOptions['hid_retention_sel'] = 'hour';
        $retentionOptions['retentionPathVolume'] = '';
        $retentionOptions['logpath'] = '';
		
		$defaults = array('globalOption'=>$globalOptions,'retentionOptions'=>$retentionOptions);
		return $defaults;
		
	}
	
	public function StopProtection_validate($identity, $version, $functionName, $parameters)
	{
        try
		{
			//Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			//Collect ProtectionPlanId and validate for its presence and also its value
			if(isset($parameters['ProtectionPlanId']))
			{
				$planObj = $parameters['ProtectionPlanId'];
				if(!is_numeric(trim($planObj->getValue()))) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionPlanId"));
			}
			else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ProtectionPlanId"));
			
			if(isset($parameters['Servers']))
			{
				$serverObj = $parameters['Servers']->getChildList();
				if(is_array($serverObj) && count($serverObj))
				{
					foreach ($serverObj as $key => $server)
					{
						$server_host_id = $server->getValue();
						if(!trim($server_host_id)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $key));
					}
				}
				else ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Servers"));
			}
			
			//Collect ForceDelete and validate its value
			if(isset($parameters['ForceDelete']))
			{
				$deleteObj = $parameters['ForceDelete'];
				$deleteValue = array('0', '1');
				if(trim($deleteObj->getValue()) == '' || !in_array(trim($deleteObj->getValue()), $deleteValue)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ForceDelete"));
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
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
	
	public function StopProtection($identity, $version, $functionName, $parameters)
	{
        try
		{
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();			
			
			global $requestXML,$SCENARIO_DELETE_PENDING, $activityId, $clientRequestId;
			$planObj = $parameters['ProtectionPlanId'];
			$protectionPlanId = trim($planObj->getValue());

			//Check for the availability of the provided protectionPlanId in applicationPlan table
			$plan_exists = $this->volObj->planExists($protectionPlanId);
			
            #print $plan_exists;
            
			/*$plan_details = $this->conn->sql_get_value("applicationPlan","planDetails","planId = ?", array($protectionPlanId));
			$consistency_info = unserialize($plan_details);*/
            $sql = "SELECT 
						planDetails,
                        TargetDataPlane 
					FROM 
						applicationPlan
					WHERE 
						planId = ?";
                        
			$data = $this->conn->sql_query($sql,array($protectionPlanId));
            foreach ($data as $key=>$value)
            {
                $plan_details = $value['planDetails'];
                $target_data_plane = $value['TargetDataPlane'];
            }
            
            $consistency_info = unserialize($plan_details);
            $consistency_interval = "";
            if ($consistency_info["ConsistencyInterval"])
            {
                $consistency_interval = $consistency_info["ConsistencyInterval"];
            }
            $crash_consistency_interval = "";
            if ($consistency_info["CrashConsistencyInterval"])
            {
                $crash_consistency_interval = $consistency_info["CrashConsistencyInterval"];
            }
            
            #print "$consistency_interval,$crash_consistency_interval,$target_data_plane";
    
			$plan_name = '';
			$arr_source_host_ids_provided_scenario_id = array();			
			if(isset($plan_exists) && $plan_exists)
			{
				$plan_name = $this->protectionObj->getPlanName($protectionPlanId);
				//Check for the availability of the provided source server hostids in applicationScenario table
				$arr_source_host_ids_provided = array();
				if(isset($parameters['Servers']))
				{
					$serverObj = $parameters['Servers']->getChildList();
					foreach ($serverObj as $key => $server)
					{
						$srcId = trim($server->getValue());
						
						//Removed the hostid registration validation, In case agent unregistration is done then the disable protection will not work from ASR as we are throwing error.
						//$srcCount = $this->conn->sql_get_value("hosts ", "count(*)", "id = ? AND sentinelEnabled = '1' AND outpostAgentEnabled = '1'",array($srcId));
						//if($srcCount == 0) ErrorCodeGenerator::raiseError($functionName,EC_NO_AGENTS,"$srcId");
						
						$arr_source_host_ids_provided[] = $srcId;
						$sql = "SELECT scenarioId FROM applicationScenario WHERE planId = ? AND scenarioType = ? AND sourceId = ?";
						$rs = $this->conn->sql_query($sql,array($protectionPlanId,'DR Protection',$srcId));
						
						if(isset($rs[0]['scenarioId']) && $rs[0]['scenarioId']) $arr_source_host_ids_provided_scenario_id[$srcId] = $rs[0]['scenarioId'];
						//Error: Src Not Protected
						//else ErrorCodeGenerator::raiseError($functionName, EC_INVALID_HOSTS_FOR_PLAN, $srcId);
					}
				}else 
                {
					$sql0 = "SELECT sourceId, scenarioId FROM applicationScenario WHERE planId = ? AND scenarioType = 'DR Protection'";
					$rs_all_scenarios = $this->conn->sql_query($sql0,array($protectionPlanId));
					if(is_array($rs_all_scenarios) && (count($rs_all_scenarios) > 0)) 
                    {
						for($i=0;isset($rs_all_scenarios[$i]);$i++) 
                        {
							$sourceId = $rs_all_scenarios[$i]['sourceId'];
							$arr_source_host_ids_provided_scenario_id[$sourceId] = $rs_all_scenarios[$i]['scenarioId'];
						}
					}
				}
				
				$forceDelete = 0;
				if(isset($parameters['ForceDelete']))
				{
					$deleteObj = $parameters['ForceDelete'];
					$forceDelete = trim($deleteObj->getValue());
				}

				//Fetch all the protected source host ids of the provided protectionPlanId
				$arr_source_host_ids = array();
				$arr_source_host_ids = $this->volObj->getSourceHostIdsByPlanId($protectionPlanId);
            
				$this->volObj->updatePairForDeletion($protectionPlanId, $arr_source_host_ids_provided, $forceDelete,$target_data_plane);
   #print_r($arr_source_host_ids_provided);
   
				if(is_array($arr_source_host_ids_provided))
				{
					$sql_scenario_details = "SELECT scenarioDetails FROM applicationScenario WHERE planId = ? AND scenarioType = 'DR Protection' AND executionStatus != ? LIMIT 1";
					$rs_scenario_details = $this->conn->sql_query($sql_scenario_details,array($protectionPlanId,$SCENARIO_DELETE_PENDING));
					if(is_array($rs_scenario_details) && (count($rs_scenario_details) > 0)) 
                    {
						$scenario_details = $rs_scenario_details[0]['scenarioDetails'];
						$scenario_details = unserialize(htmlspecialchars_decode($scenario_details));
                        
						if(strtolower(trim($scenario_details['globalOption']['vacp'])) == 'on')
						{
							$arr_source_host_ids_remaining = array_diff($arr_source_host_ids, $arr_source_host_ids_provided);
                            #print_r($arr_source_host_ids_remaining);

                            if ( $target_data_plane ==2)
                            {
                                $sql = "SELECT policyId,policyParameters,policyRunEvery FROM policy WHERE planId = ? AND policyType = '4'";
                                $rs = $this->conn->sql_query($sql,array($protectionPlanId));
                                
                                if(is_array($rs) && (count($rs)>0)) 
                                {
                                    foreach($rs as $k=>$v) 
                                    {
                                        $policyId = $v['policyId'];
                                        
                                        $del_dvap = "DELETE FROM policy WHERE policyId = ?";
                                        $rs = $this->conn->sql_query($del_dvap,array($policyId));
                                        
                                        $del_dvap_grp = "DELETE FROM consistencyGroups WHERE policyId = ?";
                                        $rs = $this->conn->sql_query($del_dvap_grp,array($policyId));
                                    }
                                }
                                if(count($arr_source_host_ids_remaining) > 1)
                                {
                                    if ($consistency_interval) 
                                    {	
                                        $this->protectionObj->insert_dvacp_consistency_policy_v2($arr_source_host_ids_remaining,$protectionPlanId, $consistency_info);	
                                    }
                                    
                                    if($crash_consistency_interval) 
                                    {
                                        $this->protectionObj->insert_dvacp_crash_consistency_policy($arr_source_host_ids_remaining,$protectionPlanId, $consistency_info);
                                    }
                                 }
                                 elseif(count($arr_source_host_ids_remaining) == 1)
                                 {
                                        $remained_host_id = array_pop($arr_source_host_ids_remaining);
                                       #print "$protectionPlanId,$remained_host_id";
                                      
                                       $sql = "SELECT scenarioId FROM applicationScenario WHERE planId = ? AND scenarioType = ? AND sourceId = ?";
                                        $rs = $this->conn->sql_query($sql,array($protectionPlanId,'DR Protection',$remained_host_id));
                                        $scenario_id = $rs[0]['scenarioId'];
                                        #print "scenario id is $scenario_id";
                                        
                                        if (isset($scenario_id))
                                        {
                                        if(isset($consistency_interval))
                                        {
                                            #print "entered for crash consistency policy.\n";
                                            #print "$protectionPlanId, $sourceHostId, $scenarioId, $consistencyOptions";
                                           $policyId = $this->protectionObj->insert_consistency_policy($protectionPlanId, $remained_host_id, $scenario_id, $consistency_info);
                                        }
                                        #print "application : $policyId";
                                        #print_r($consistencyOptions);
                                          
                                        // insert crash consistency policy
                                        if (isset($crash_consistency_interval))
                                        {
                                            #print "entered for dvcap consistency policy.\n";
                                            $policyId = $this->protectionObj->insert_crash_consistency_policy($protectionPlanId, $remained_host_id, $scenario_id, $consistency_info);
                                        }
                                        }
                                    }
                            }
                            else
                            {
                                if(count($arr_source_host_ids_remaining) > 1)
                                {
                                    $this->protectionObj->update_distributed_consistency_policy($protectionPlanId, $arr_source_host_ids_remaining, $consistency_interval);
                                }
                                elseif(count($arr_source_host_ids_remaining) == 1)
                                {
                                    $this->protectionObj->update_consistency_policy($protectionPlanId, $arr_source_host_ids_remaining, $consistency_interval);
                                }
                            }
						}
                        
                        
					}
				}
			}
			
			// clean the stale discovered VMs if any
			$this->discovery_obj->cleanStaleDiscoveredVMs($arr_source_host_ids_provided);
			
			// insert API request details
			$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId);
			$request_id = $api_ref['apiRequestId'];
			
			$requestedData = array('planId' => $protectionPlanId, 'sourceHostIdsScenarioIds' => $arr_source_host_ids_provided_scenario_id, 'PlanName' => $plan_name);
			$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($requestedData, $request_id);
				
			$response = new ParameterGroup("Response");
			if(!is_array($api_ref) || !$request_id || !$apiRequestDetailId)
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Failed due to internal error");
			}
			else
			{
				$response->setParameterNameValuePair('RequestId', $request_id);
			}
			
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
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
		}
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}	
	}

    /*
    * Function Name: GetProtectionDetails
    *
    * Description: 
    * This function is to retrieve the protection related information
    *
    * Parameters:
    *     Param 1 [IN]: ProtectionPlanId (and /or)
    *     Param 2 [IN]: Servers 
    *     Param 3 [OUT]: Protection information based on the filters
    *     
    * Exceptions:
    *     NONE
    */	
	
	public function GetProtectionDetails_validate ($identity, $version, $functionName, $parameters)
	{
        try
		{
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			/* If ProtectionPlanId is passed */
			if(isset($parameters['ProtectionPlanId']))
			{
				$plan_id = $parameters['ProtectionPlanId']->getValue();
				if(!$plan_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "ProtectionPlanId"));
				// Validate if the planId passed belongs to a protection plan
				$plan_name = $this->recovery_obj->validate_protection_plan($plan_id,1);
				if(!$plan_name) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionPlanId"));
				if(isset($parameters['Servers'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_MTAL_EXCLSIVE_ARGS', array(), "Expected either ProtectionPlanId/Servers");
			}
			
			/* If Servers are passed */
			if(isset($parameters['Servers']))
			{
				$source_host_ids = array();
				$servers = $parameters['Servers']->getChildList();
				if(!is_array($servers)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Servers"));
				foreach($servers as $server_name => $server_obj)
				{
					$host_id = $server_obj->getValue();
					if(!$host_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Servers - ".$server_name));
					if(in_array($host_id, $source_host_ids)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Servers. Duplicate ServerHostId - ".$server_name));
					$source_host_ids[] = $host_id;
				}
				// Validate if the hostIds belongs to protected servers
				$hosts_not_protected = $this->volObj->validateProtectedHosts($source_host_ids);
				if($hosts_not_protected) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PROTECTION_SERVER', array('SourceHostName' => $hosts_not_protected));
				if(isset($parameters['ProtectionPlanId'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_MTAL_EXCLSIVE_ARGS', array(), "Expected either ProtectionPlanId/Servers");
			}

			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
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
    
	public function GetProtectionDetails ($identity, $version, $functionName, $parameters)
	{
		try
		{
			global $protection_health_factors; 
			global $SLOW_PROGRESS_FORWARD, $SLOW_PROGRESS_REVERSE, $NO_PROGRESS, $IR_STATS_SUPPORTED_VERSION, $ZERO, $ONE, $TWO, $THREE, $FOUR, $MINUS_ONE;
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions(); 
			
			if(isset($parameters['ProtectionPlanId']))
			{
				$plan_id = $parameters['ProtectionPlanId']->getValue();
				$input_plan_id = $plan_id;
			}
			
			$server_ids = array();
			if(isset($parameters['Servers']))
			{
				$servers = $parameters['Servers']->getChildList();
				foreach($servers as $server_name => $server_obj)
				{
					$host_id = $server_obj->getValue();
					$server_ids[] = $host_id;
				}
			}
			
			/* Set health factor for Protection as red
				*   1. Data Flow Controlled
				*   2. Data Upload Blocked
				*   3. Pause Pairs
				*   4. Target Cache Volume less than 256 MB
				*   5. No Communication from Source/Target
				*/
				
			$protection_stage_array = array('Resync Step I', 'Resync Step II', 'Differential Sync', 'Queued');
			$health_array = array('Protection', 'DataConsistency', 'RPO', 'Retention', 'CommonConsistency', 'VolumeResize' );
			$cs_health_type = 'VM';
			
			$protection_health_factors = $this->volObj->get_protection_health_factors($plan_id, $server_ids);
			$protection_details = $this->volObj->get_pair_details($plan_id, $server_ids, $protection_health_factors);
			$profiler_details = $this->volObj->getProfilerDetails($plan_id, $server_ids);

			$response = new ParameterGroup("Response");
			$plan_count = 1;
	
			if($protection_details and is_array($protection_details))
			{
			if (count($protection_details) > 0)
			{
				$get_vm_id_sql = "SELECT
						h.id as hostId,
						inv.infrastructureVMId
					FROM
						hosts h						
					LEFT JOIN
							infrastructureVMs inv
						ON
							h.id = inv.hostId";
				$vm_id_arr = $this->conn->sql_get_array($get_vm_id_sql, "hostId", "infrastructureVMId", array());
				
				$host_vm_id_sql = "SELECT
										id,
										originalVMId
									FROM
										hosts
									WHERE
										originalVMId != ''";
				$host_id_arr = $this->conn->sql_get_array($host_vm_id_sql, "id", "originalVMId", array());
				
				$volume_details = $this->volObj->get_pair_volume_details();
				
                // Plan level details
				foreach ($protection_details as $plan_id => $details)
				{
					
					$plan_health = "";
					$source_servers = array_keys($details['servers']);
					$target_servers = $details['target_servers'];			
					$source_servers = array_unique(array_filter($source_servers));
					$target_servers = array_unique(array_filter($target_servers));
					$servers = array_merge($source_servers, $target_servers);
					$server_host_details = $this->volObj->get_server_details($servers);
                    
                    // Set no agent communication error in case if timestamp is greater than 15 mins.
					foreach($source_servers as $host_id)
					{
						$src_host_info = $server_host_details[$host_id];
						
						// Assuming that the source will always be protected using one target irrespective of number of volumes.
						$dest_id = $details['servers'][$host_id]['destHostId'];
						$dest_host_info = $server_host_details[$dest_id];
						
						// If source or destination, either of them have heartbeat issue.
                        // ECH00021 - InMageVmVxAgentDown
                        // ECH00022 - InMageVmAppAgentDown
                        // ECH00023 - InMageVmVxandAppAgentDown
						
						$source_app_heartbeat_time = $src_host_info['lastUpdateTimeApp']?$src_host_info['lastUpdateTimeApp']: $src_host_info['creationTime'];
						$dest_app_heartbeat_time = $dest_host_info['lastUpdateTimeApp']?$dest_host_info['lastUpdateTimeApp']: $dest_host_info['creationTime'];
						
						$min_val_hearetbeat = min($src_host_info['lastHostUpdateTime'],$source_app_heartbeat_time);
						$min_val_dest_hearetbeat = min($dest_host_info['lastHostUpdateTime'],$dest_app_heartbeat_time);
						
						$vx_heartbeat = $src_host_info['lastHostUpdateTime'];
						$app_heartbeat = $src_host_info['lastUpdateTimeApp']?$src_host_info['lastUpdateTimeApp']:$src_host_info['creationTime'];
						
						if(($src_host_info['currentTime'] - $src_host_info['lastHostUpdateTime'] > 900) && ($src_host_info['currentTime'] - $src_host_info['lastUpdateTimeApp'] > 900))
						{
							$details['health']['Protection'] .= "^ECH00023##$min_val_hearetbeat##$cs_health_type##";
							$details['servers'][$host_id]['health']['Protection'] .= "^ECH00023##$min_val_hearetbeat##$cs_health_type##";							
							$server_volumes = array_keys($details['servers'][$host_id]['volumes']);
							foreach($server_volumes as $volume_name)
							{
								$details['servers'][$host_id]['volumes'][$volume_name]['health']['Protection'] .= "^ECH00023##$min_val_hearetbeat##$cs_health_type##";
							}
						}
                        elseif($src_host_info['currentTime'] - $src_host_info['lastHostUpdateTime'] > 900)
                        {
                            $details['health']['Protection'] .= "^ECH00021##$vx_heartbeat##$cs_health_type##";
                            $details['servers'][$host_id]['health']['Protection'] .= "^ECH00021##$vx_heartbeat##$cs_health_type##";							
                            $server_volumes = array_keys($details['servers'][$host_id]['volumes']);
                            foreach($server_volumes as $volume_name)
                            {
                                $details['servers'][$host_id]['volumes'][$volume_name]['health']['Protection'] .= "^ECH00021##$vx_heartbeat##$cs_health_type##";
                            }
                        }
                        elseif($src_host_info['currentTime'] - $src_host_info['lastUpdateTimeApp'] > 900)
                        {
				
                            $details['health']['Protection'] .= "^ECH00022##$app_heartbeat##$cs_health_type##";
                            $details['servers'][$host_id]['health']['Protection'] .= "^ECH00022##$app_heartbeat##$cs_health_type##";							
                            $server_volumes = array_keys($details['servers'][$host_id]['volumes']);
                            foreach($server_volumes as $volume_name)
                            {
                                $details['servers'][$host_id]['volumes'][$volume_name]['health']['Protection'] .= "^ECH00022##$app_heartbeat##$cs_health_type##";
                            }
                        }
						
                        if(($dest_host_info['currentTime'] - $dest_host_info['lastHostUpdateTime'] > 900) || ($dest_host_info['currentTime'] - $dest_host_info['lastUpdateTimeApp'] > 900))
                        {
                            $details['health']['Protection'] .= "^ECH0001##$min_val_dest_hearetbeat";
							$details['servers'][$host_id]['health']['Protection'] .= "^ECH0001##$min_val_dest_hearetbeat##$cs_health_type##";							
							$server_volumes = array_keys($details['servers'][$host_id]['volumes']);
							foreach($server_volumes as $volume_name)
							{
								$details['servers'][$host_id]['volumes'][$volume_name]['health']['Protection'] .= "^ECH0001##$min_val_dest_hearetbeat##$cs_health_type##";
							}
                        }
					}
					$plan_grp = new ParameterGroup("Plan".$plan_count);			
					$plan_grp->setParameterNameValuePair("ProtectionPlanId", $plan_id);
					$plan_grp->setParameterNameValuePair("PlanName", $details['PlanName']);
					$plan_grp->setParameterNameValuePair("ConsistencyInterval", $details['ConsistencyInterval']);
					$plan_grp->setParameterNameValuePair("CrashConsistencyInterval", $details['CrashConsistencyInterval']);
					$plan_grp->setParameterNameValuePair("SolutionType", $details['solution_type']);
					$plan_grp->setParameterNameValuePair("ServersProtected", count($source_servers));
					$plan_grp->setParameterNameValuePair("VolumesProtected", $details['volumes_protected']);
					
                    // Server level details
					$protected_server_grp = new ParameterGroup("ProtectedServers");
					$server_count = 1;
					
					foreach($details['servers'] as $server_id => $server_details)
					{
						$volumes = array_keys($server_details['volumes']);						
						$total_capacity = $server_details['total_capacity'];
						
						if(!is_array($volume_details[$server_id])) continue;
						
						if($server_host_details[$server_id]['osFlag'] == "1") $os_name = "WINDOWS";
						elseif($server_host_details[$server_id]['osFlag'] == "2") $os_name = "LINUX";
						
						// Resyncing Details
						$resync_progress = "N/A";                       
                        
                        $resourceId = $server_host_details[$server_id]['resourceId'];                        
                        
						$server_grp = new ParameterGroup("Server".$server_count);
						$vmid= '';

						if($vm_id_arr[$server_id])
						{
							$vmid = $vm_id_arr[$server_id];
						}
						elseif($host_id_arr[$server_id])
						{
							$vmid = $host_id_arr[$server_id];
						}
						
						$server_grp->setParameterNameValuePair("InfrastructureVMId", $vmid);                        
                        $server_grp->setParameterNameValuePair("ResourceId", $resourceId);  
                        $server_grp->setParameterNameValuePair("ServerHostId", $server_id);
						$server_grp->setParameterNameValuePair("ServerHostName", $server_host_details[$server_id]['name']);
						$server_grp->setParameterNameValuePair("ServerHostIp", $server_host_details[$server_id]['ipAddress']);
						$server_grp->setParameterNameValuePair("ProcessServerId", $server_details['psId']);
						$server_grp->setParameterNameValuePair("ServerOS", $os_name);
						$server_grp->setParameterNameValuePair("RPO", $server_details['rpo']);
						$server_grp->setParameterNameValuePair("ProtectionStage", $protection_stage_array[$server_details['protection_stage']]);
						$server_grp->setParameterNameValuePair("ResyncDuration", $server_details['resync_duration']);
						$server_grp->setParameterNameValuePair("LastRpoCalculatedTime", $server_details['lastRpoCalculatedTime']);
						if(count($profiler_details[$server_id]))
						{
							$data_change_grp = new ParameterGroup("DailyDataChangeRate");
							$data_change_grp->setParameterNameValuePair("Compression", ($profiler_details[$server_id]['compression']));
							$data_change_grp->setParameterNameValuePair("UnCompression", ($profiler_details[$server_id]['unCompression']));
							$server_grp->addParameterGroup($data_change_grp);
						}
						
						// Health factors
						$server_health_array = $server_details['health'];
						$healthType = 'VM';
						if($server_health_array)
						{
							$server_health_grp = $this->GenerateHealthFactorsGroup($server_health_array, $healthType);
							if($server_health_grp) $server_grp->addParameterGroup($server_health_grp);
						}
                        
                        
                        
						if ($details['TargetDataPlane']!=2)
                        {
						// Retention window group
						$retention_window_grp = new ParameterGroup("RetentionWindow");
						$retention_window_grp->setParameterNameValuePair("From", $server_details['logsFrom']);
						$retention_window_grp->setParameterNameValuePair("To", $server_details['logsTo']);
						$server_grp->addParameterGroup($retention_window_grp);
						}
                        // Volume level details
						$pairs = $server_details['volumes'];
						$protected_volumes_grp = new ParameterGroup("ProtectedDevices");
						$vol_count = 1;
						$vm_stats_available = "No";
						$vm_total_data_transferred = array();
						$vm_progress_health = array();
						$vm_progress_health[] = $FOUR;
						$vm_total_data_transferred_exists = FALSE;
						foreach($pairs as $volume_name => $next_level_details)
						{
							$pair_details = $next_level_details['pair_details'];
							$resync_progress = 'N/A';
							
							$volume_grp = new ParameterGroup("Device".$vol_count);					
							
                            if ($os_name == "WINDOWS")
                            {
                                $match_status = preg_match("/DRIVE(.*)/", $pair_details['Display_Name'], $match_res);
                                if ($match_status)
                                {
                                    $disk_number = $match_res[1];
                                }
								else
								{
									$disk_number = $pair_details['Display_Name'];
								}
                                $volume_grp->setParameterNameValuePair("DeviceName", "Disk".$disk_number);
                            }
                            else
                            {
                                $volume_grp->setParameterNameValuePair("DeviceName", $pair_details['Display_Name']);
                            }
							$volume_grp->setParameterNameValuePair("DiskId", $pair_details['sourceDeviceName']);
							$volume_grp->setParameterNameValuePair("ProtectionStage", $protection_stage_array[$pair_details['protection_stage']]);
							$volume_grp->setParameterNameValuePair("RPO", $pair_details['rpo']);
							$volume_grp->setParameterNameValuePair("ResyncProgress", $pair_details['resync_progress']);
							$volume_grp->setParameterNameValuePair("ResyncDuration", $pair_details['resyncDuration']);
							$volume_grp->setParameterNameValuePair("ResyncRequired", $pair_details['resync_required']);
							$volume_grp->setParameterNameValuePair("DeviceCapacity", $volume_details[$server_id][$volume_name]['capacity']);
							$volume_grp->setParameterNameValuePair("FileSystemCapacity", $volume_details[$server_id][$volume_name]['capacity']-$volume_details[$server_id][$volume_name]['freeSpace']);
							$volume_grp->setParameterNameValuePair("VolumeResized", $pair_details['volume_resized']);
							$volume_grp->setParameterNameValuePair("LastRpoCalculatedTime", $pair_details['lastRpoCalculatedTime']);
							
							// Health factors
							$pair_health_array = $next_level_details['health'];
							$healthType = 'DISK';
							if($pair_health_array)
							{
								$pair_health_grp = $this->GenerateHealthFactorsGroup($pair_health_array, $healthType);
								if($pair_health_grp) $volume_grp->addParameterGroup($pair_health_grp);
							}
							
							// Data in transit
							$data_transit = $next_level_details['pair_details']['DataTransit'];
							if($data_transit)
							{
								$data_transit_grp = new ParameterGroup("DataInTransit");
								foreach($data_transit as $param_name => $param_value)
								{
									$data_transit_grp->setParameterNameValuePair($param_name, $param_value);
								}
								$volume_grp->addParameterGroup($data_transit_grp);
							}
							
							if(count($profiler_details[$server_id][$volume_name]))
							{
								$data_change_grp = new ParameterGroup("DailyDataChangeRate");
								$data_change_grp->setParameterNameValuePair("Compression", ($profiler_details[$server_id][$volume_name]['compression']));
								$data_change_grp->setParameterNameValuePair("UnCompression", ($profiler_details[$server_id][$volume_name]['unCompression']));
								$volume_grp->addParameterGroup($data_change_grp);
							}
							
							//Setup disk level properties for IR Stats.
							$disk_progress_health = $FOUR;
							//Only for step0 active(non queued) disks stats applicable.
							if (($pair_details['isQuasiflag'] == $ZERO) &&
								($pair_details['executionState'] == $ONE))
							{
								$default_stats = true;
								$vm_stats_available = "Yes";
								$stats_grp = new ParameterGroup("Stats");
								if ($pair_details['stats'])
								{
									$decoded_json_stats = json_decode($pair_details['stats'],TRUE);
									if (json_last_error() === JSON_ERROR_NONE) 
									{
										$default_stats = false;
										//print "valid json.";
										
										$stats_grp ->setParameterNameValuePair("ResyncProcessedBytes", $decoded_json_stats['ResyncProcessedBytes']);
										$stats_grp ->setParameterNameValuePair("ResyncTotalTransferredBytes", $decoded_json_stats['ResyncTotalTransferredBytes']);
										$stats_grp ->setParameterNameValuePair("ResyncLast15MinutesTransferredBytes", $decoded_json_stats['ResyncLast15MinutesTransferredBytes']);
										$stats_grp ->setParameterNameValuePair("ResyncLastDataTransferTimeUTC", $decoded_json_stats['ResyncLastDataTransferTimeUTC']);
										$stats_grp ->setParameterNameValuePair("ResyncStartTime", $decoded_json_stats['ResyncStartTimeUTC']);
										$vm_total_data_transferred[] = $decoded_json_stats['ResyncTotalTransferredBytes'];
										$vm_total_data_transferred_exists = TRUE;
									}
								}
								if ($default_stats)
								{
									$stats_grp ->setParameterNameValuePair("ResyncProcessedBytes", $MINUS_ONE);
									$stats_grp ->setParameterNameValuePair("ResyncTotalTransferredBytes", $MINUS_ONE);
									$stats_grp ->setParameterNameValuePair("ResyncLast15MinutesTransferredBytes", $MINUS_ONE);
									$stats_grp ->setParameterNameValuePair("ResyncLastDataTransferTimeUTC", "");
									$stats_grp ->setParameterNameValuePair("ResyncStartTime", "");
								}
								
								$volume_grp->addParameterGroup($stats_grp);
							
								if (count($protection_health_factors) > $ZERO)
								{	
									if (array_key_exists($pair_details['pairId'], $protection_health_factors))
									{
										if ($pair_details['TargetDataPlane'] == $TWO)
										{
											$SLOW_PROGRESS = $SLOW_PROGRESS_FORWARD;
										}
										else
										{
											$SLOW_PROGRESS = $SLOW_PROGRESS_REVERSE;
										}
										if (array_key_exists($SLOW_PROGRESS, $protection_health_factors[$pair_details['pairId']]))
										{
											$disk_progress_health = $TWO;
											$vm_progress_health[] = $TWO;
										}
										if (array_key_exists($NO_PROGRESS, $protection_health_factors[$pair_details['pairId']]))
										{
											$disk_progress_health = $THREE;
											$vm_progress_health[] = $THREE;
										}
									}
								}
								//If disk progress health empty, then marking healthy progress.
								if ($disk_progress_health == $FOUR)
								{
									$disk_progress_health = $ONE;
									$vm_progress_health[] = $ONE;
								}
							}
							
							if (($pair_details['isQuasiflag'] == $ZERO) &&
								($pair_details['executionState'] == $TWO))
							{
								//Queued
								$volume_grp->setParameterNameValuePair("ProgressHealth", $ZERO);
							}
							else
							{
								$volume_grp->setParameterNameValuePair("ProgressHealth", $disk_progress_health);
							}
							$volume_grp->setParameterNameValuePair("ProgressStatus", $disk_progress_health);
							
							$protected_volumes_grp->addParameterGroup($volume_grp);
							$vol_count++;
						}
						
						// calculating the weighted average for the server. (all disks resync_progress/ total capacity of all disks)
						if($server_details['protection_stage'] == $ZERO) 
						{
							if ((is_array($server_details['total_resync_offset'])) && 
								(is_array($server_details['total_capacity'])) )
							{
								
								$total_of_disk_resync_progress_values = implode("+",$server_details['total_resync_offset']);
								$total_of_disk_capacity_values = implode("+",$server_details['total_capacity']);
								$vm_total_data_transferred_cond = '';
								if ($vm_total_data_transferred_exists == TRUE)
								{
									$vm_total_data_transferred = implode("+",$vm_total_data_transferred);
									$vm_total_data_transferred_cond .= ', '.$vm_total_data_transferred.' as vmTotalDataTransferred';
								}
								$vm_stats_sql = "SELECT case when $total_of_disk_resync_progress_values=$ZERO then $ZERO else Round(((($total_of_disk_resync_progress_values)/($total_of_disk_capacity_values))*100),2) end as vmResyncProgress ".$vm_total_data_transferred_cond;
								 
								$vm_stats_data = $this->conn->sql_query($vm_stats_sql, array());
								foreach ($vm_stats_data as $key=>$value)
								{
									$resync_progress = $value['vmResyncProgress'];
									if (array_key_exists("vmTotalDataTransferred",$value))
										$total_data_transferred = $value['vmTotalDataTransferred'];
								}
							}	
						}
						$server_grp->setParameterNameValuePair("ResyncProgress", $resync_progress);
						//Setup server level properties for IR Stats.
						$server_grp->setParameterNameValuePair("IsAdditionalStatsAvailable", $vm_stats_available);
						if ($vm_total_data_transferred_exists == TRUE)
						{
							$server_grp->setParameterNameValuePair("TotalDataTransferred", $total_data_transferred);
						}
						else
						{
							$server_grp->setParameterNameValuePair("TotalDataTransferred", $MINUS_ONE);
						}
						if (in_array($THREE,$vm_progress_health))
							$vm_progress_health_val = $THREE;
						else if (in_array($TWO,$vm_progress_health))
							$vm_progress_health_val = $TWO;
						else if (in_array($ONE,$vm_progress_health))
							$vm_progress_health_val = $ONE;
						else if (in_array($FOUR,$vm_progress_health))
							$vm_progress_health_val = $FOUR;
						$server_grp->setParameterNameValuePair("TotalProgressHealth", $vm_progress_health_val);

						$server_grp->addParameterGroup($protected_volumes_grp);				
						$protected_server_grp->addParameterGroup($server_grp);
						$server_count++;
					}
					$plan_grp->addParameterGroup($protected_server_grp);
					$response->addParameterGroup($plan_grp);
					$plan_count++;
				}	
			}
			}
			$un_protection_details = $this->volObj->get_plans_info_without_protection_details($input_plan_id, $server_ids);
			if ($un_protection_details and is_array($un_protection_details))
			{
				if (count($un_protection_details) > 0)
				{
					foreach ($un_protection_details as $value)
					{
						$cons_intervals = unserialize($value['planDetails']);
						$plan_grp = new ParameterGroup("Plan".$plan_count);			
						$plan_grp->setParameterNameValuePair("ProtectionPlanId", $value['planId']);
						$plan_grp->setParameterNameValuePair("PlanName", $value['planName']);
						$plan_grp->setParameterNameValuePair("ConsistencyInterval", $cons_intervals['ConsistencyInterval']);
						$plan_grp->setParameterNameValuePair("CrashConsistencyInterval", $cons_intervals['CrashConsistencyInterval']);
						$plan_grp->setParameterNameValuePair("SolutionType", $value['solutionType']);
						$plan_grp->setParameterNameValuePair("ServersProtected", "0");
						$plan_grp->setParameterNameValuePair("VolumesProtected", "0");
						$plan_count++;
						$response->addParameterGroup($plan_grp);
					}
				}
			}
			

			
			if ((count($protection_details) == 0) && (count($un_protection_details) == 0))
			{
				$plan_grp = new ParameterGroup("Plans");	                     
                $response->addParameterGroup($plan_grp);
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
			
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
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
    
    public function GenerateHealthFactorsGroup($health_array, $healthType='VM')
    {
		global $protection_health_factors;
        $health_message = $this->volObj->get_health_message();
        $full_health_grp = new ParameterGroup("HealthFactors");
        $health_counter = 1;
        foreach ($health_array as $health_code => $health_str)
        {
            $health_details = explode("^", $health_str);
            $health_details = array_unique(array_filter($health_details));
			
			foreach($health_details as $key=>$value)
			{
				$health_data = explode("##", $value);
				if($health_data[2] == $healthType)
				{
					$health_detection_time = $health_data[1];
					$health_grp = new ParameterGroup("HealthFactor".$health_counter);
					$health_grp->setParameterNameValuePair("Code", $health_data[0]);
					$health_grp->setParameterNameValuePair("Type", $health_code);
					$health_grp->setParameterNameValuePair("Impact", "");
					$health_grp->setParameterNameValuePair("Description", "");
					$health_grp->setParameterNameValuePair("DetectionTime", $health_detection_time);
					
					if($health_data[3] != '')
					{
						$healthPlaceHolders = json_decode($health_data[3]);
						$ph_grp = new ParameterGroup("PlaceHolders");
						foreach ($healthPlaceHolders as $pkey => $pval)
						{
							$ph_grp->setParameterNameValuePair($pkey,$pval); 
						}
						$health_grp->addParameterGroup($ph_grp);	
					}
					$full_health_grp->addParameterGroup($health_grp);
					$health_counter++;
				}
			}
		 }
        return $full_health_grp;
    }
 
	public function CreateProtectionV2_validate ($identity, $version, $functionName, $parameters)
	{	
		global $REPLICATION_ROOT;
		global $BATCH_SIZE;
		global $is_same_request_id;
		global $activityId;
		$is_same_request_id = 0;
		$function_name_user_mode = 'Create Protection V2';
		try
		{
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			/*
				*Idempotency validation
				*If same createProtectionV2 request reaches second time to configuration server in the same workflow, then CS should send response as success with the given request id.
				*If no matches found in the apiRequest table allow further for createProtectionV2 validations.
				*If protection configurations exists, and apirequest gets pruned, then allow further for createProtectionV2 validations.
			*/
			if($parameters['SourceServers'])
			{
				$sourceServerListObj = $parameters['SourceServers']->getChildList();
				if(!is_array($sourceServerListObj)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "SourceServers"));
				
				foreach($sourceServerListObj as $val)
				{
					// validate source server
					$sourceDetailsObj = $val->getChildList();
					$sourceHostId = trim($sourceDetailsObj['HostId']->getValue()); 
					if(!$sourceHostId) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HostId"));
					$pair_info = $this->volObj->GetConfiguredPairInfo($sourceHostId);
					if (count($pair_info) > 0)
					{
						$api_req_res = $this->conn->sql_query("select max(apiRequestId) as maxRequestId from apirequest where requestType = \"$functionName\" and requestXml like '%Name=\"HostId\" Value=\"$sourceHostId\"%' and activityId='$activityId'");
						if($this->conn->sql_num_row($api_req_res))
						{
							while($row = $this->conn->sql_fetch_object($api_req_res))
							{
								$is_same_request_id = $row->maxRequestId;
								return;	
							}
						}
					}
				}
			}

			if(isset($parameters['PlanId'])) 
			{
				$PlanId = trim($parameters['PlanId']->getValue());
				if($PlanId != '')
				{
					// request for retry
					if(!is_numeric($PlanId))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "PlanId"));
					$planStatus = $this->protectionObj->getPlanStatus($PlanId);

					if($planStatus == 'Prepare Target Failed') return;
					else ErrorCodeGenerator::raiseError($functionName, 'EC_INVALID_PLAN_STATE', array('ProtectionState' => "$planStatus"));
				}
			}
			//validate plan name
			if(isset($parameters['PlanName'])) 
			{
				$planName = trim($parameters['PlanName']->getValue());
				$existingPlanId = $this->protectionObj->getPlanId($planName);
				
				if($existingPlanId) ErrorCodeGenerator::raiseError($functionName, 'EC_PLAN_EXISTS', array('PlanType' => 'Protection', 'PlanName' => $planName));
			}
			
			//validate plan properties
			if(isset($parameters['ProtectionProperties']))
			{
				$Obj = $parameters['ProtectionProperties']->getChildList();
				
				// if amethyst.conf having BATCH_SIZE value then overriding with ASR passed BatchResync value.
				if(isset($BATCH_SIZE))
				{
					if((!is_numeric($BATCH_SIZE)) || $BATCH_SIZE <= 0) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "BATCH_SIZE"));
				}
				else
				{
					$BatchResync = trim($Obj['BatchResync']->getValue());
					if((!is_numeric($BatchResync)) || $BatchResync < 0) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "BatchResync"));
				}
				
				$MultiVmSync = trim($Obj['MultiVmSync']->getValue());
				if((strtolower($MultiVmSync) != 'on') && (strtolower($MultiVmSync) != 'off'))  ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "MultiVmSync"));
				
				$RPOThreshold = trim($Obj['RPOThreshold']->getValue());
				if(!is_numeric($RPOThreshold)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RPOThreshold"));
					
					
				$ConsistencyInterval = trim($Obj['ConsistencyInterval']->getValue());
				if((!is_numeric($ConsistencyInterval)) || ($ConsistencyInterval < 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ConsistencyInterval"));
				
				if (isset($Obj['CrashConsistencyInterval']))
				{
					$CrashConsistencyInterval = trim($Obj['CrashConsistencyInterval']->getValue());	 
					if((!is_numeric($CrashConsistencyInterval)) || ($CrashConsistencyInterval < 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "CrashConsistencyInterval"));
				}
				
				$Compression = trim($Obj['Compression']->getValue());
				if((strtolower($Compression) != 'enabled') && (strtolower($Compression) != 'disabled'))  ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Compression"));
				
				// validate auto resync option 
				if(isset($Obj['AutoResyncOptions']))
				{
					$autoResyncObj = $Obj['AutoResyncOptions']->getChildList();
					$patternMatch = '/^([01]?[0-9]|2[0-3]):[0-5][0-9]$/';
					if(isset($autoResyncObj['StartTime']))
					{
						$StartTime = trim($autoResyncObj['StartTime']->getValue());
						preg_match($patternMatch, $StartTime, $StartTimeMatches);
						if(!$StartTimeMatches[0]) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "StartTime"));
					}
					else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "StartTime"));
				  
					if(isset($autoResyncObj['EndTime']))
					{
						$EndTime = trim($autoResyncObj['EndTime']->getValue()); 	
						preg_match($patternMatch, $EndTime, $EndTimeMatches);
						if(!$EndTimeMatches[0]) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "EndTime"));
					}
					else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "EndTime"));
				  
					if(isset($autoResyncObj['Interval'])){
						$Interval = trim($autoResyncObj['Interval']->getValue()); 
						if($Interval < 10) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Interval"));
					}
					else ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Interval"));
				}
			}
			#$cfs_mt_details = $this->conn->sql_get_array("SELECT id, (CASE WHEN connectivityType = 'VPN' THEN privateIpAddress  ELSE publicIpAddress  END) as ipAddress from cfs", "id", "ipAddress");          
			//validate global options
			if($parameters['GlobalOptions'])
			{
				$globalOptObj = $parameters['GlobalOptions']->getChildList();
				$ProcessServer = trim($globalOptObj['ProcessServer']->getValue()); 
				if(!$ProcessServer)ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProcessServer"));

				$ProcessServerInfo = $this->commonfunc_obj->get_host_info($ProcessServer);
				if((!$ProcessServerInfo) || (!$ProcessServerInfo['processServerEnabled'])) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
				
				if($ProcessServerInfo['currentTime'] - $ProcessServerInfo['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ProcessServerInfo['name'], 'PsIP' => $ProcessServerInfo['ipaddress'], 'Interval' => ceil(($ProcessServerInfo['currentTime'] - $ProcessServerInfo['lastHostUpdateTimePs'])/60)));
				
				$UseNatIPFor = 'None';
				if($globalOptObj['UseNatIPFor'])  $UseNatIPFor = trim($globalOptObj['UseNatIPFor']->getValue());
				
				if((strtolower($UseNatIPFor) != 'source') && (strtolower($UseNatIPFor) != 'target') && (strtolower($UseNatIPFor) != 'both') && (strtolower($UseNatIPFor) != 'none'))
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "UseNatIPFor"));
					
				$MasterTarget = trim($globalOptObj['MasterTarget']->getValue()); 
				if(!$MasterTarget) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "MasterTarget"));

				$MasterTargetInfo = $this->commonfunc_obj->get_host_info($MasterTarget);

				if(!$MasterTargetInfo) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
				elseif(strtolower($MasterTargetInfo['agentRole']) != 'mastertarget') ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
				
				if($MasterTargetInfo['currentTime'] - $MasterTargetInfo['lastHostUpdateTime'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_HEART_BEAT', array('MTHostName' => $MasterTargetInfo['name'], 'MTIP' => $MasterTargetInfo['ipaddress'], 'Interval' => ceil(($MasterTargetInfo['currentTime'] - $MasterTargetInfo['lastHostUpdateTime'])/60), 'OperationName' => $function_name_user_mode));
				
						
				if(isset($globalOptObj['ProtectionDirection']))
				{
					$protection_direction = trim($globalOptObj['ProtectionDirection']->getValue());
					if($protection_direction != "E2A" && $protection_direction != "A2E") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionDirection"));
				}
				#if($MasterTarget && !$cfs_mt_details[$MasterTarget]) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PUBLIC_IP', array('MTHostName' => $MasterTargetInfo['name']));
			}
			
			//validate source server details 
			$target_id_list = array();
			
			if($parameters['SourceServers'])
			{
				$sourceServerObj = $parameters['SourceServers']->getChildList();
				if(!is_array($sourceServerObj)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "SourceServers"));
				$cluster_hosts = $this->commonfunc_obj->getAllClusters();
				
				foreach($sourceServerObj as $v)
				{
					// validate source server
					$pairs_to_configure = array();
					$encryptedDiskList = array();
					$encryptedDisks = array();
					$sourceDetailsObj = $v->getChildList();
					$sourceHostId = trim($sourceDetailsObj['HostId']->getValue()); 
					if(!$sourceHostId) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "HostId"));
					$sourceIdInfo = $this->commonfunc_obj->get_host_info($sourceHostId);
						
					if(!$sourceIdInfo) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
					elseif(strtolower($sourceIdInfo['agentRole']) != 'agent') ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));

						

					foreach($cluster_hosts as $cluster_id => $cluster_data)
					{
						if(in_array($sourceHostId, $cluster_data)) ErrorCodeGenerator::raiseError($functionName, 'EC_CLUSTER_SERVER', array('SourceIP' => $sourceIdInfo['ipaddress'], 'ErrorCode' => '5117'));
					}
					
					// collect all the encrypted disks based upon hostid.	
					$encryptedDiskList = $this->commonfunc_obj->get_encrypted_disks($sourceHostId);
					
					// validate process server
					if(!isset($sourceDetailsObj['ProcessServer']) && !$ProcessServer) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProcessServer"));
					if(isset($sourceDetailsObj['ProcessServer']))
					{
						$ProcessServer = trim($sourceDetailsObj['ProcessServer']->getValue());
						if($ProcessServer) 
						{
							$ProcessServerInfo = $this->commonfunc_obj->get_host_info($ProcessServer);							
							if((!$ProcessServerInfo) || (!$ProcessServerInfo['processServerEnabled'])) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));
							
							if($ProcessServerInfo['currentTime'] - $ProcessServerInfo['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ProcessServerInfo['name'], 'PsIP' => $ProcessServerInfo['ipaddress'], 'Interval' => ceil(($ProcessServerInfo['currentTime'] - $ProcessServerInfo['lastHostUpdateTimePs'])/60)));
						}
					}

					if(!isset($sourceDetailsObj['UseNatIPFor']) && !$UseNatIPFor) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "UseNatIPFor"));				
					if(isset($sourceDetailsObj['UseNatIPFor']))  $UseNatIPFor = trim($sourceDetailsObj['UseNatIPFor']->getValue());				
					if((strtolower($UseNatIPFor) != 'source') && (strtolower($UseNatIPFor) != 'target') && (strtolower($UseNatIPFor) != 'both') && (strtolower($UseNatIPFor) != 'none')) 
						ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "UseNatIPFor"));


					// validate target server
					if(isset($sourceDetailsObj['MasterTarget']))
					{
						$MasterTarget = trim($sourceDetailsObj['MasterTarget']->getValue());
						if($MasterTarget) {
							$MasterTargetInfo = $this->commonfunc_obj->get_host_info($MasterTarget);
								
							if(!$MasterTargetInfo){
								ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
							}
							elseif(strtolower($MasterTargetInfo['agentRole']) != 'mastertarget'){
								ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
							}
							
							if($MasterTargetInfo['currentTime'] - $MasterTargetInfo['lastHostUpdateTime'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_HEART_BEAT', array('MTHostName' => $MasterTargetInfo['name'], 'MTIP' => $MasterTargetInfo['ipaddress'], 'Interval' => ceil(($MasterTargetInfo['currentTime'] - $MasterTargetInfo['lastHostUpdateTime'])/60), 'OperationName' => $function_name_user_mode));
							#if(!$cfs_mt_details[$MasterTarget]) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PUBLIC_IP', array('MTHostName' => $MasterTargetInfo['name']));
						}
					}
					

					// validate source and target already protected 
					$existingPlanDetails = $this->protectionObj->isPlanExist($sourceHostId, 'DR Protection');
						
					if(count($existingPlanDetails) > 0)
					{
						$existing_target_host = $this->commonfunc_obj->getHostName($existingPlanDetails[0]['targetId']);
						$existing_target_host_ip = $this->commonfunc_obj->getHostIpaddress($existingPlanDetails[0]['targetId']);

						ErrorCodeGenerator::raiseError($functionName, 'EC_SRC_TGT_PROTECTED', array('SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress'], 'MTHostName' => $existing_target_host, 'MTIP' => $existing_target_host_ip));
					}

					

					if(isset($sourceDetailsObj['ProtectionDirection']))
                    {
						$protection_direction = trim($sourceDetailsObj['ProtectionDirection']->getValue());
                        if($protection_direction != "E2A" && $protection_direction != "A2E") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionDirection"));
                    }
					
					

					if(!isset($sourceDetailsObj['DiskDetails'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "DiskDetails"));
					if($sourceDetailsObj['DiskDetails'])
					{
					   $diskMapObj = $sourceDetailsObj['DiskDetails']->getChildList();
					   unset($encryptedStr);
					   foreach ($diskMapObj as $diskData)
					   {
						  $diskInfoObj = $diskData->getChildList();
						  $SourceDiskName = trim($diskInfoObj['SourceDiskName']->getValue());
						  if(!$SourceDiskName)ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "SourceDiskName"));
						  
						  
						  $pairs_to_configure[$SourceDiskName] = $SourceDiskName;
						  
						  if(in_array("$SourceDiskName", $encryptedDiskList)) 
						  {
							$encryptedDisks = array_keys($encryptedDiskList, "$SourceDiskName");
							$encryptedStr .= implode(",", $encryptedDisks);
							$encryptedStr = $encryptedStr.",";
						  }
					   }
					  					   
					   
					   //Validating source disk encryption.
					   // Protection needs to block if disk is encrypted.
					   if(isset($encryptedStr))
					   {
							ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_ENCRYPTED', array('ListVolumes' => rtrim($encryptedStr,","), 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
							
					   }					   
					   
					   //Validating source disks for UEFI partition/dynamic disks.
					   //UEFI :- It does not support as Azure does not support
						$src_vols = array_keys($pairs_to_configure);
						$src_vols_str = implode(",", $src_vols);
						list($uefi_disk_arr, $dynamic_disk_arr) = $this->volObj->check_disk_eligibility($sourceHostId, $src_vols_str);						

						/*
						Doing it for windows only to identify boot dynamic disk. No dynamic disk concept for Linux.
						Boot Dynamic disk:- V2 does not support
						Data Dynamic disk:- V2 can support this
						*/
						if ($sourceIdInfo['osFlag'] == 1)
						{							
							/*
								Boot Dynamic disk:- V2 does not support
								Data Dynamic disk:- V2 can support this
							*/
							$dynamic_boot_disk_status = $this->volObj->is_dynamic_boot_disk_exists($sourceHostId, $src_vols);
							if($dynamic_boot_disk_status == TRUE) 
							{
								ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_DYNAMIC', array('ListVolumes' => implode(",", $dynamic_disk_arr), 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
							}
						}
						
						if ($sourceIdInfo['osFlag'] != 1)
						{
							$multi_path_status = $this->volObj->isMultiPathTrue($sourceHostId);
							if ($multi_path_status == TRUE)
							{
								ErrorCodeGenerator::raiseError($functionName, 'EC_MULTI_PATH_FOUND', array('ListVolumes' => implode(",", $src_vols),'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
							}
						}
						//Checks Given Source Volumes
						$src_vol_not_registered_arr = $this->volObj->checkDeviceexists($sourceHostId,$src_vols,$sourceIdInfo['osFlag'],1,'azure');
						$dest_device_array = array(); // as we are not getting target device list so passing as blank array.
							
						if(count($src_vol_not_registered_arr)) 
						{
							$src_vol_not_registered_str = implode(",",$src_vol_not_registered_arr);
							ErrorCodeGenerator::raiseError($functionName, 'EC_IS_SOURCE_VOLUME_ELIGIBLE', array('ListVolumes' => $src_vol_not_registered_str, 'SourceHostName' => $sourceIdInfo['name'], 'SourceIP' => $sourceIdInfo['ipaddress']));
						}
					}
				}	
			}
			
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
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
	
	public function CreateProtectionV2($identity, $version, $functionName, $parameters)
	{
		$retry = 0;
		$done = false;
		global $DB_TRANSACTION_RETRY_LIMIT, $DB_TRANSACTION_RETRY_SLEEP;
		global $is_same_request_id;
		global $MYSQL_FOREIGN_KEY_VIOLATION, $API_ERROR_LOG, $logging_obj;
		$function_name_user_mode = 'CreateProtectionV2';
		if ($is_same_request_id)
		{
			$response  = new ParameterGroup ( "Response" );
			$response->setParameterNameValuePair('RequestId', "$is_same_request_id");
			return $response->getChildList();
		}
		
		
		while (!$done and ($retry < $DB_TRANSACTION_RETRY_LIMIT)) 
		{
			try
			{
				global $BATCH_SIZE;
				
				// Enable exceptions for DB.
				$this->conn->enable_exceptions();
				$this->conn->sql_begin();
				global $requestXML, $activityId, $clientRequestId, $PREPARE_TARGET_DONE, $AZURE_DATA_PLANE, $CLOUD_PLAN_TYPE, $target_data_plane,$CRASH_CONSISTENCY_INTERVAL,$DVACP_CRASH_CONSISTENCY_INTERVAL;
				$scenarioData = array();
				$cfs_arr = array();
				$target_data_plane = $AZURE_DATA_PLANE;
				if($parameters['ProtectionProperties'])
				{
					$Obj = $parameters['ProtectionProperties']->getChildList();
				   
					if (isset($Obj['MultiVmSync']))
						$MultiVmSync = trim($Obj['MultiVmSync']->getValue());
					 
					// if amethyst.conf having BATCH_SIZE value then overriding with ASR passed BatchResync value.
					if(isset($BATCH_SIZE))
					{
						$BatchResync = $BATCH_SIZE;
					}
					else if (isset($Obj['BatchResync']))
					{
						$BatchResync = trim($Obj['BatchResync']->getValue());
					}
					
					if (isset($Obj['RPOThreshold']))
						$RPOThreshold = trim($Obj['RPOThreshold']->getValue());
					
					if (isset($Obj['ConsistencyInterval']))
						$ConsistencyInterval = trim($Obj['ConsistencyInterval']->getValue());
				 
					if (strtolower($MultiVmSync) == 'on')
					{
						$CrashConsistencyInterval = $DVACP_CRASH_CONSISTENCY_INTERVAL;
					}
					else
					{
						$CrashConsistencyInterval = $CRASH_CONSISTENCY_INTERVAL;
					}
					
					if (isset($Obj['CrashConsistencyInterval']))
						$CrashConsistencyInterval = trim($Obj['CrashConsistencyInterval']->getValue());
				
					if (isset($Obj['Compression']))
						$Compression = trim($Obj['Compression']->getValue());
					
					if($Obj['AutoResyncOptions'])
					{
						$autoResyncObj = $Obj['AutoResyncOptions']->getChildList();
						$StartTime = trim($autoResyncObj['StartTime']->getValue()); 
						$EndTime = trim($autoResyncObj['EndTime']->getValue()); 
						$Interval = trim($autoResyncObj['Interval']->getValue());
					}
				}
		 
				$PlanName = uniqid('Protection_', false);
			   
				if($parameters['PlanName'])
				{
					$PName = trim($parameters['PlanName']->getValue());
					if ($PName != "")
					{
						$PlanName = $PName;
					}
				}
				
				 
				$consistencyOptions = array("ConsistencyInterval" => $ConsistencyInterval,"CrashConsistencyInterval" => $CrashConsistencyInterval);
				
				$planDetails = serialize($consistencyOptions);
				/*Sample plan insert data
				INSERT INTO
							applicationPlan 
							(planName, planType,planCreationTime,batchResync,solutionType,applicationPlanStatus,planDetails,TargetDataPlane)
						VALUES
							('Protection_555c61cb00892', 'CLOUD',UNIX_TIMESTAMP(now()),'3','AZURE','Active', 'a:2:{s:19:"ConsistencyInterval";s:2:"30";s:24:"CrashConsistencyInterval";s:2:"30";}',2)*/
						   
				$planId = $this->protectionObj->createPlan($PlanName,$CLOUD_PLAN_TYPE,$BatchResync,0,$planDetails,'Active',$AZURE_DATA_PLANE);
			  
				$protectionDirection = "E2A";
				if($parameters['GlobalOptions'])
				{
					$globalOptObj = $parameters['GlobalOptions']->getChildList();
					if (isset($globalOptObj['ProcessServer']))
					$globalProcessServer = trim($globalOptObj['ProcessServer']->getValue()); 

					$UseNatIPFor = 'None';
					if (isset($globalOptObj['UseNatIPFor'])) 
					$globalUseNatIPFor = trim($globalOptObj['UseNatIPFor']->getValue());				
					
					if (isset($globalOptObj['MasterTarget']))
					$globalMasterTarget = trim($globalOptObj['MasterTarget']->getValue()); 
					
					if(isset($globalOptObj['ProtectionDirection']))				
					$globalprotectionDirection = trim($globalOptObj['ProtectionDirection']->getValue());   	
				}

				/*
				Sample CFS array
				Array
				(
					[B71F5598-08CA-7A47-9B39CBB26F9C67C1] => Array
						(
							[ipAddress] => 10.10.10.10
							[connectivityType] => NON VPN
						)

				)
				*/
				$cfs_rs = $this->conn->sql_query("SELECT id, (CASE WHEN connectivityType = 'VPN' THEN privateIpAddress  ELSE publicIpAddress  END) as ipAddress,connectivityType from cfs");
				if($this->conn->sql_num_row($cfs_rs))
				{
					while($row = $this->conn->sql_fetch_object($cfs_rs))
					{
						$cfs_arr[$row->id] = array("ipAddress"=>$row->ipAddress,"connectivityType"=>$row->connectivityType);
					}
				}
								
				if($parameters['SourceServers'])
				{
					$sourceServerObj = $parameters['SourceServers']->getChildList();
				  
				   $serverCount = count($sourceServerObj);			   
				   $dvacp_consistency_set = 0;
				   if($serverCount > 1) $dvacp_consistency_set = 1;
					
					foreach($sourceServerObj as $v)
					{
						$scenarioData = array();
						$scenario_arr = array();
						$sourceProtectionDirection = $protectionDirection;
						$sourceDetailsObj = $v->getChildList();
						
						if(isset($sourceDetailsObj['DiskDetails']))
						{
							$diskObj = $sourceDetailsObj['DiskDetails']->getChildList();
							$source_disk_names = array();
							$target_disk_names = array();
							$SourceDiskName = "";
							foreach ($diskObj as $diskData)
							{
							  $diskInfoObj = $diskData->getChildList();
							  if (isset($diskInfoObj['SourceDiskName']))
							  $SourceDiskName = trim($diskInfoObj['SourceDiskName']->getValue());
							  $source_disk_names[] = $SourceDiskName;
							}
							// insert target disk details in LV table
						}
						/*Sample source disk names structure
						Array
						(
							[0] => \\.\PHYSICALDRIVE1
							[1] => \\.\PHYSICALDRIVE0
							[2] => \\.\PHYSICALDRIVE2
						) */       
						 #print_r($source_disk_names);
						#exit;
						
						if (isset($sourceDetailsObj['HostId']))
						$sourceHostId = trim($sourceDetailsObj['HostId']->getValue());
						  /*Sample datum for $protection_disk_details
						Array
						(
							[0] => Array
								(
									[sourceDiskName] => \\.\PHYSICALDRIVE1
									[capacity] => 64424509440
									[targetDiskName] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1
								)

							[1] => Array
								(
									[sourceDiskName] => \\.\PHYSICALDRIVE0
									[capacity] => 42949672960
									[targetDiskName] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0
								)

							[2] => Array
								(
									[sourceDiskName] => \\.\PHYSICALDRIVE2
									[capacity] => 5368709120
									[targetDiskName] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2
								)

						)
						*/
					 
						if ($sourceDetailsObj['ProcessServer'] && isset($sourceDetailsObj['ProcessServer'])) 
						{
							$ProcessServer = trim($sourceDetailsObj['ProcessServer']->getValue());
						}
						else
						{
							$ProcessServer = $globalProcessServer;
						}
						if($sourceDetailsObj['UseNatIPFor'] && isset($sourceDetailsObj['UseNatIPFor'])) 
						{
							$UseNatIPFor = trim($sourceDetailsObj['UseNatIPFor']->getValue());
						}
						else
						{
							 $UseNatIPFor = $globalUseNatIPFor;
						}
						if($sourceDetailsObj['MasterTarget'] && isset($sourceDetailsObj['MasterTarget'])) 
						{
							$MasterTarget = trim($sourceDetailsObj['MasterTarget']->getValue());
						}
						else
						{
							$MasterTarget = $globalMasterTarget;
						}
						if($sourceDetailsObj['ProtectionDirection'] && isset($sourceDetailsObj['ProtectionDirection'])) 
						{
							$sourceProtectionDirection = trim($sourceDetailsObj['ProtectionDirection']->getValue()); 
						}
						else
						{
							$sourceProtectionDirection = $globalProtectionDirection;
						}
						#print "$ProcessServer, $UseNatIPFor, $MasterTarget";
						#print "$sourceProtectionDirection";
						
						$defaults = $this->_get_defaults();
						$arr_global_options = $defaults['globalOption'];
						
						$protection_disk_details = $this->protectionObj->insert_target_disk_details_in_lv_table($sourceHostId, $source_disk_names, $MasterTarget);
						
						$arr_global_options['vacp'] = $MultiVmSync;
						$arr_global_options['rpo'] = $RPOThreshold;
						$arr_global_options['batchResync'] = $BatchResync;
						$arr_global_options['compression'] = ($Compression == "Enabled") ? 1 : 0;
						$arr_global_options['processServerid'] = $ProcessServer;
						
						
						if(($StartTime) && ($EndTime)) 
						{
							$StartTimeArr = explode(":", $StartTime);
							$EndTimeArr = explode(":", $EndTime);
							
							$arr_global_options['autoResync']['start_hr'] = $StartTimeArr[0];
							$arr_global_options['autoResync']['start_min'] = $StartTimeArr[1];
							$arr_global_options['autoResync']['stop_hr'] = $EndTimeArr[0];
							$arr_global_options['autoResync']['stop_min'] = $EndTimeArr[1];
							$arr_global_options['autoResync']['wait_min'] = $Interval;
						}
						
						if(strtolower($UseNatIPFor) == 'source') $arr_global_options['srcNATIPflag'] = '1';
						if(strtolower($UseNatIPFor) == 'target') $arr_global_options['trgNATIPflag'] = '1';
						if(strtolower($UseNatIPFor) == 'both')
						{
							$arr_global_options['srcNATIPflag'] = '1';
							$arr_global_options['trgNATIPflag'] = '1';
						}
						
						foreach ($protection_disk_details as $key=>$value)
						{
							$srcVol = $value['sourceDiskName'];
							$trgVol = $value['targetDiskName'];
							$target_disk_names[] = $trgVol;
							$capacity = $value['capacity'];
							$scenario_arr[$srcVol.'='.$trgVol]['srcVol'] = $srcVol;
							$scenario_arr[$srcVol.'='.$trgVol]['trgVol'] = $trgVol;
							$scenario_arr[$srcVol.'='.$trgVol]['srcCapacity'] = $capacity;
							$scenario_arr[$srcVol.'='.$trgVol]['processServerid'] = $arr_global_options['processServerid'];
							$scenario_arr[$srcVol.'='.$trgVol]['srcNATIpflag'] = $arr_global_options['srcNATIPflag'];
							$scenario_arr[$srcVol.'='.$trgVol]['trgNATIPflag'] = $arr_global_options['trgNATIPflag'];
						}
						
						/*Sample scenario array
						Array
						(
							[pairDetails] => Array
								(
									[\\.\PHYSICALDRIVE1=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1] => Array
										(
											[srcVol] => \\.\PHYSICALDRIVE1
											[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1
											[srcCapacity] => 64424509440
											[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
											[srcNATIpflag] => 
											[trgNATIPflag] => 1
										)

									[\\.\PHYSICALDRIVE0=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0] => Array
										(
											[srcVol] => \\.\PHYSICALDRIVE0
											[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0
											[srcCapacity] => 42949672960
											[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
											[srcNATIpflag] => 
											[trgNATIPflag] => 1
										)

									[\\.\PHYSICALDRIVE2=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2] => Array
										(
											[srcVol] => \\.\PHYSICALDRIVE2
											[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2
											[srcCapacity] => 5368709120
											[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
											[srcNATIpflag] => 
											[trgNATIPflag] => 1
										)

								)

						)
						*/
						#print_r($scenario_arr);
						
						
						if ($cfs_arr[$MasterTarget]) 
						{
							if ($cfs_arr[$MasterTarget]["connectivityType"] == "VPN")
							{
								$arr_global_options['use_cfs']  = 0;
							}
							elseif ($cfs_arr[$MasterTarget]["connectivityType"] == "NON VPN")
							{
								$arr_global_options['use_cfs']  = 1;
							}
						}
						else
						{
							$arr_global_options['use_cfs']  = 0;
						}
						
						/*Sample datum for array global operations
						Array
						(
							[vacp] => On
							[rpo] => 30
							[batchResync] => 3
							[compression] => 1
							[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
							[autoResync] => Array
								(
									[start_hr] => 12
									[start_min] => 00
									[stop_hr] => 18
									[stop_min] => 00
									[wait_min] => 30
								)

							[trgNATIPflag] => 1
							[use_cfs] => 1
						)
						print_r($arr_global_options);
						exit;
						*/
						
						$sourceHostName = $this->commonfunc_obj->getHostName($sourceHostId);
						$targetHostName = $this->commonfunc_obj->getHostName($MasterTarget);
						$arr_pair_info = array($sourceHostId.'='.$MasterTarget => array(
																					'sourceId' => $sourceHostId,
																					'sourceName' => $sourceHostName,
																					'targetId' => $MasterTarget,
																					'targetName' => $targetHostName,
																					'pairDetails' => $scenario_arr
																					) );
						
						/*      Sample datum of arr_pair_info             
					   Array
						(
							[9F755DE0-AD01-9841-B6EB63AA5C269F58761=B71F5598-08CA-7A47-9B39CBB26F9C67C1] => Array
								(
									[sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58761
									[sourceName] => WIN-CFV0Q93KEAI
									[targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
									[targetName] => WIN-T36T0AUAC0F
									[pairDetails] => Array
										(
											[pairDetails] => Array
												(
													[\\.\PHYSICALDRIVE1=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE1
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1
															[srcCapacity] => 64424509440
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

													[\\.\PHYSICALDRIVE0=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE0
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0
															[srcCapacity] => 42949672960
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

													[\\.\PHYSICALDRIVE2=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE2
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2
															[srcCapacity] => 5368709120
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

												)

										)

								)

						)
						*/
						#print_r($arr_pair_info);
						
						$unserializeScenarioDetails = array("globalOption" => $arr_global_options, 
												 "pairInfo" => $arr_pair_info, 
												 "planId" => $planId,
												 "planName" => $PlanName,
												 "isPrimary" => 1,
												 "protectionType" => "BACKUP PROTECTION",
												 "protectionPath" => $sourceProtectionDirection,		
												 "solutionType" => "Azure",
												 "sourceHostId" => $sourceHostId,
												 "targetHostId" => $MasterTarget
												 );
						$scenarioDetails = serialize($unserializeScenarioDetails);
						
						if (count($source_disk_names) > 0 )
							$source_all_disks_details = implode(",",$source_disk_names);
						if (count($target_disk_names) > 0)
							$target_all_disks_details = implode(",",$target_disk_names);
						
						$scenarioData['planId'] = $planId;
						$scenarioData['scenarioType'] = 'DR Protection';
						$scenarioData['scenarioName'] = '';
						$scenarioData['scenarioDescription'] = '';
						$scenarioData['scenarioVersion'] = '';
						$scenarioData['scenarioCreationDate'] = '';
						$scenarioData['scenarioDetails'] = $scenarioDetails;
						$scenarioData['validationStatus'] = '';
						$scenarioData['executionStatus'] = $PREPARE_TARGET_DONE;
						$scenarioData['applicationType'] = 'Azure';
						$scenarioData['applicationVersion'] = '';
						$scenarioData['srcInstanceId'] = '';
						$scenarioData['trgInstanceId'] = '';
						$scenarioData['referenceId'] = '';
						$scenarioData['resyncLimit'] = '';
						$scenarioData['autoProvision'] = '';
						$scenarioData['sourceId'] = $sourceHostId;
						$scenarioData['targetId'] = $MasterTarget;
						$scenarioData['sourceVolumes'] = $source_all_disks_details;
						$scenarioData['targetVolumes'] = $target_all_disks_details;
						$scenarioData['retentionVolumes'] = '';
						$scenarioData['reverseReplicationOption'] = '';
						$scenarioData['protectionDirection'] = 'forward';
						$scenarioData['currentStep'] = '';
						$scenarioData['creationStatus'] = '';
						$scenarioData['isPrimary'] = '';
						$scenarioData['isDisabled'] = '';
						$scenarioData['isModified'] = '';
						$scenarioData['reverseRetentionVolumes'] = '';
						$scenarioData['volpackVolumes'] = '';
						$scenarioData['volpackBaseVolume'] = '';
						$scenarioData['isVolumeResized'] = '';
						$scenarioData['sourceArrayGuid'] = '';
						$scenarioData['targetArrayGuid'] = '';
						$scenarioData['rxScenarioId'] = '';
						/*Sample scenario datum
						Array
						(
							[globalOption] => Array
								(
									[vacp] => On
									[rpo] => 30
									[batchResync] => 3
									[compression] => 1
									[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
									[autoResync] => Array
										(
											[start_hr] => 12
											[start_min] => 00
											[stop_hr] => 18
											[stop_min] => 00
											[wait_min] => 30
										)

									[trgNATIPflag] => 1
									[use_cfs] => 1
								)

							[pairInfo] => Array
								(
									[9F755DE0-AD01-9841-B6EB63AA5C269F58761=B71F5598-08CA-7A47-9B39CBB26F9C67C1] => Array
										(
											[sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58761
											[sourceName] => WIN-CFV0Q93KEAI
											[targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
											[targetName] => WIN-T36T0AUAC0F
											[pairDetails] => Array
												(
													[\\.\PHYSICALDRIVE1=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE1
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1
															[srcCapacity] => 64424509440
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

													[\\.\PHYSICALDRIVE0=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE0
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0
															[srcCapacity] => 42949672960
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

													[\\.\PHYSICALDRIVE2=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE2
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2
															[srcCapacity] => 5368709120
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

												)

										)

								)

							[planId] => 62
							[planName] => Protection_55487fea897ab
							[isPrimary] => 1
							[protectionType] => BACKUP PROTECTION
							[solutionType] => Azure
							[sourceHostId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58761
							[targetHostId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
						)
						*/
						#print_r($scenarioData);
						
						// insert scenario details
						$scenarioId = $this->protectionObj->insertScenarioDetails($scenarioData);	
						#print $scenarioId;
						

						$scenarioIdArr["$sourceHostId"] = $scenarioId;
						
						// insert consistency policy
						if(((!$dvacp_consistency_set) || (($dvacp_consistency_set) && (strtolower($MultiVmSync) == 'off')) ) && ($ConsistencyInterval)) 
						{
							#print "entered for crash consistency policy.\n";
						   $policyId = $this->protectionObj->insert_consistency_policy($planId, $sourceHostId, $scenarioId, $consistencyOptions);
						   $referenceIds = $referenceIds . $policyId . ",";
						}
						#print "application : $MultiVmSync, $policyId";
						#print_r($consistencyOptions);
				   
						// insert crash consistency policy
						if(((!$dvacp_consistency_set) || (($dvacp_consistency_set) && (strtolower($MultiVmSync) == 'off')) ) && ($CrashConsistencyInterval)) 
						{
							#print "entered for dvcap consistency policy.\n";
							$policyId = $this->protectionObj->insert_crash_consistency_policy($planId, $sourceHostId, $scenarioId, $consistencyOptions);
							$referenceIds = $referenceIds . $policyId . ",";
						}
						#print "Crash: $MultiVmSync, $policyId";
						#exit;
						$this->protectionObj->insertCloudProtectionPlan($scenarioId);
						
					}
				}
				#print_r($scenarioIdArr);
			
				//For version 2 the dvcap policy is applicable for both windows & linux
				if(($dvacp_consistency_set) && (strtolower($MultiVmSync) == 'on')  && ($ConsistencyInterval))
				{
					#print "entered for dvacp policy.\n";
				   // insert dvacp policy
				   $srcIdsList = array_keys($scenarioIdArr);
				   #print_r($srcIdsList);
				   
				   $policyId = $this->protectionObj->insert_dvacp_consistency_policy_v2($srcIdsList, $planId, $consistencyOptions);
				   $referenceIds = $referenceIds . $policyId . ",";
				}
				 #print "dvacp  policy id is $policyId ";
				//For version 2 the dvcap policy is applicable for both windows & linux
				if(($dvacp_consistency_set) && (strtolower($MultiVmSync) == 'on')  && ($CrashConsistencyInterval))
				{
					#print "entered for dvacp crash policy.\n";
				   // insert dvacp policy
				   $srcIdsList = array_keys($scenarioIdArr);
				   $policyId = $this->protectionObj->insert_dvacp_crash_consistency_policy($srcIdsList, $planId, $consistencyOptions);
				   $referenceIds = $referenceIds . $policyId . ",";
				}
				#print "dvacp crash policy id is $policyId ";
				
				$referenceIds = "," . $referenceIds;
				
				// insert API request details
				$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $referenceIds);
				$request_id = $api_ref['apiRequestId'];
				$requestedData = array("planId" => $planId, "scenarioId" => $scenarioIdArr);
				$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($requestedData, $request_id);
				$response  = new ParameterGroup ( "Response" );
				$response->setParameterNameValuePair('RequestId', $request_id);
				
				// Disable Exceptions for DB
				$this->conn->sql_commit();
				$this->conn->disable_exceptions();
				$done = true;
				return $response->getChildList();
			}
			catch(SQLException $sql_excep)
			{
				$sql_error_code = $sql_excep->getCode();
				
				$this->conn->sql_rollback();
				$this->conn->disable_exceptions();
				
				switch($sql_error_code)
				{
					case $MYSQL_FOREIGN_KEY_VIOLATION:
					$MasterTargetInfo = $this->commonfunc_obj->get_host_info($MasterTarget);
					if(!$MasterTargetInfo)
					{
						ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
					}
					break;
					default:
				}				
				
				if ($retry < $DB_TRANSACTION_RETRY_LIMIT)
				{
					sleep($DB_TRANSACTION_RETRY_SLEEP);
					$retry++;
				}
				
				if ($retry == $DB_TRANSACTION_RETRY_LIMIT)
				{
					$logging_obj->my_error_handler("INFO",array("RetryCount"=>$retry),Log::APPINSIGHTS);
					debug_access_log($API_ERROR_LOG, "CreateProtectionV2 configuration failed: Retry count = $retry, ClientRequestId = $clientRequestId, ActivityId = $activityId \n");
					$this->commonfunc_obj->captureDeadLockDump($requestXML, $activityId, "Protection");
					
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
				// Disable Exceptions for DB
				$this->conn->disable_exceptions();
				$done = true;
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
			}
		}
	} 
	
	
	public function ModifyProtectionV2_validate ($identity, $version, $functionName, $parameters)
	{	
		global $REPLICATION_ROOT;
		$function_name_user_mode = 'Modify Protection V2';
		
		try
		{
			// Enable Exceptions for DB.
			$this->conn->enable_exceptions();
			
			//Validate ProtectionPlanId
			$ProtectionPlanId = $this->validateParameter($parameters,'ProtectionPlanId',FALSE,'NUMBER');
			//Error: Raise error if PlanId does not exists
			$planName = $this->protectionObj->getPlanName($ProtectionPlanId, "CLOUD");
			if(!$planName) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $ProtectionPlanId));
			
			//Error: Servers Parameter is missed
			if((isset($parameters['Servers'])) === FALSE) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Servers"));
			
			//Validate source server details 
			$sourceServerObj = $parameters['Servers']->getChildList();
			if((is_array($sourceServerObj) === FALSE) || (count($sourceServerObj) == 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'Server'));
			
			$target_id_list = array();
			//$ret_vol_list = array();
			
			#$cfs_mt_details = $this->conn->sql_get_array("SELECT id, (CASE WHEN connectivityType = 'VPN' THEN privateIpAddress  ELSE publicIpAddress  END) as ipAddress from cfs", "id", "ipAddress");
			$cluster_hosts = $this->commonfunc_obj->getAllClusters();
			
			foreach($sourceServerObj as $v)
			{
				$encryptedDiskList = array();
				$encryptedDisks = array();
				//Validate Source Server Details
				$sourceDetailsObj = $v->getChildList();

				// Validate HostId Parameter
				$sourceHostId = $this->validateParameter($sourceDetailsObj,'HostId',FALSE);
				//Error: Raise Error if Source Host Id is not registered with CS
				$srcCount = $this->conn->sql_get_value("hosts ", "count(*)", "id = ? AND sentinelEnabled = '1' AND outpostAgentEnabled = '1' AND agentRole = 'Agent'",array($sourceHostId));
				if($srcCount == 0) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_AGENT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
				
				$src_details = $this->commonfunc_obj->get_host_info($sourceHostId);
				
				
				foreach($cluster_hosts as $cluster_id => $cluster_data)
				{
					if(in_array($sourceHostId, $cluster_data)) ErrorCodeGenerator::raiseError($functionName, 'EC_CLUSTER_SERVER', array('SourceIP' => $src_details['ipaddress'], 'ErrorCode' => '5117'));
				}	
				
				// collect all the encrypted disks based upon hostid.	
				$encryptedDiskList = $this->commonfunc_obj->get_encrypted_disks($sourceHostId);
					
				// Validate ProcessServer Parameter
				$ProcessServer = $this->validateParameter($sourceDetailsObj,'ProcessServer',FALSE);
				//Error: Raise Error if ProcessServer Host Id is not registered with CS
				$ps_info = $this->commonfunc_obj->get_host_info($ProcessServer);
				if(!$ps_info || !$ps_info['processServerEnabled']) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_PS_FOUND));

				if($ps_info['currentTime'] - $ps_info['lastHostUpdateTimePs'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PS_HEART_BEAT', array('PsHostName' => $ps_info['name'], 'PsIP' => $ps_info['ipaddress'], 'Interval' => ceil(($ps_info['currentTime'] - $ps_info['lastHostUpdateTimePs'])/60)));
				
				// Validate MasterTarget Parameter
				$MasterTarget = $this->validateParameter($sourceDetailsObj,'MasterTarget',FALSE);
				//Error: Raise Error if MasterTarget Host Id is not registered with CS
				$mt_info = $this->commonfunc_obj->get_host_info($MasterTarget);
				if($mt_info['agentRole'] != 'MasterTarget') ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
				if($mt_info['currentTime'] - $mt_info['lastHostUpdateTime'] > 900) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_HEART_BEAT', array('MTHostName' => $mt_info['name'], 'MTIP' => $mt_info['ipaddress'], 'Interval' => ceil(($mt_info['currentTime'] - $mt_info['lastHostUpdateTime'])/60), 'OperationName' => $function_name_user_mode));
				#if(!$cfs_mt_details[$MasterTarget]) ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PUBLIC_IP', array('MTHostName' => $mt_info['name']));
				$target_id_list[] = $MasterTarget;
				
				//Error: Raise error if already Source and Target are of different OS Platform
				/*$tgtOS = $this->commonfunc_obj->get_osFlag($MasterTarget);
				if($src_details['osFlag'] != $tgtOS) ErrorCodeGenerator::raiseError($functionName, 'EC_OS_CHECK');//,"HostId $sourceHostId and MasterTarget $MasterTarget");*/
				
				
				//Error: Raise error if already protection scenario exists for given source and target
				$count = $this->conn->sql_get_value("applicationScenario", "count(*)", "sourceId = ? AND targetId = ? AND scenarioType = ?",array($sourceHostId,$MasterTarget,'DR Protection'));
					
				
				
				if(isset($sourceDetailsObj['ProtectionDirection']))
				{
					$protection_direction = trim($sourceDetailsObj['ProtectionDirection']->getValue());
					if($protection_direction != "E2A" && $protection_direction != "A2E") ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionDirection"));
				}
				// Validate UseNatIPFor Parameter
				$UseNatIPFor = $this->validateParameter($sourceDetailsObj,'UseNatIPFor',TRUE);
				if($UseNatIPFor) {
					//Validate Value for UseNatIPFor
					$validValues = array('source','target','both','none');
					if((in_array(strtolower($UseNatIPFor),$validValues)) === FALSE) {
						ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "UseNatIPFor"));
					}
				}

				//Error: DiskDetails Parameter is missed
				if((isset($sourceDetailsObj['DiskDetails'])) === FALSE) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "DiskDetails"));
				
				//Validate DiskDetails Details
				$diskMapObj = $sourceDetailsObj['DiskDetails']->getChildList();
				$SrcDeviceList = array();
				if((is_array($diskMapObj) === FALSE) || (count($diskMapObj) == 0)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => 'Disk'));
				unset($encryptedStr);
				foreach ($diskMapObj as $diskData)
				{
					$diskInfoObj = $diskData->getChildList();
					// Validate SourceDiskName Parameter
					$SourceDiskName = $this->validateParameter($diskInfoObj,'SourceDiskName',FALSE);
					$SrcDeviceList[] = $SourceDiskName;
					
					if(in_array("$SourceDiskName", $encryptedDiskList)) 
					{
						$encryptedDisks = array_keys($encryptedDiskList, "$SourceDiskName");
						$encryptedStr .= implode(",", $encryptedDisks);
						$encryptedStr = $encryptedStr.",";
					}
				}
				
				//Validating source disk encryption.
			    // Protection needs to block if disk is encrypted.
			    if(isset($encryptedStr))
			    {
					ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_ENCRYPTED', array('ListVolumes' => rtrim($encryptedStr,","), 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
			    }
					   
				//Validating source disks for UEFI partitions/dynamic disk
				$src_vols_str = implode(",", $SrcDeviceList);						
				list($uefi_disk_arr, $dynamic_disk_arr) = $this->volObj->check_disk_eligibility($sourceHostId, $src_vols_str);
				
				/*
				Doing it for windows only to identify boot dynamic disk. No dynamic disk concept for Linux.
				Boot Dynamic disk:- V2 does not support
				Data Dynamic disk:- V2 can support this
				*/
				if ($src_details['osFlag'] == 1)
				{							
					/*
						Boot Dynamic disk:- V2 does not support
						Data Dynamic disk:- V2 can support this
					*/
					$dynamic_boot_disk_status = $this->volObj->is_dynamic_boot_disk_exists($sourceHostId, $SrcDeviceList);
					if($dynamic_boot_disk_status == TRUE) 
					{
						ErrorCodeGenerator::raiseError($functionName, 'EC_DISK_IS_DYNAMIC', array('ListVolumes' => implode(",", $dynamic_disk_arr), 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
					}
				}				
				
				if ($src_details['osFlag'] != 1)
				{
					$multi_path_status = $this->volObj->isMultiPathTrue($sourceHostId);
					if ($multi_path_status == TRUE)
					{
						ErrorCodeGenerator::raiseError($functionName, 'EC_MULTI_PATH_FOUND', array('ListVolumes' => implode(",", $SrcDeviceList),'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
					}
				}	
				
				//Error: Raise Error if the specified source devices are not registered with CS
				$unregisteredSRCDiskList = $this->volObj->checkDeviceexists($sourceHostId,$SrcDeviceList,$src_details['osFlag'],1,'azure');
				if(count($unregisteredSRCDiskList)) {
					$unregisteredSRCDiskListStr = implode(",",$unregisteredSRCDiskList);
					ErrorCodeGenerator::raiseError($functionName, 'EC_IS_SOURCE_VOLUME_ELIGIBLE', array('ListVolumes' => $unregisteredSRCDiskListStr, 'SourceHostName' => $src_details['name'], 'SourceIP' => $src_details['ipaddress']));
				}
			}	
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
		}
		catch(SQLException $sql_excep)
		{
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
	
	/*
		Function to Add Server to Existing Protection Plan
	*/
	public function ModifyProtectionV2 ($identity, $version, $functionName, $parameters)
	{
		$retry = 0;
		$done = false;
		global $DB_TRANSACTION_RETRY_LIMIT, $DB_TRANSACTION_RETRY_SLEEP;
		global $requestXML, $activityId,$clientRequestId,$PREPARE_TARGET_DONE,$SCENARIO_DELETE_PENDING,$AZURE_DATA_PLANE,$target_data_plane;
		global $MYSQL_FOREIGN_KEY_VIOLATION,$API_ERROR_LOG,$logging_obj;
		while (!$done and ($retry < $DB_TRANSACTION_RETRY_LIMIT)) 
		{
			try
			{   
				// Enable exceptions for DB.
				$this->conn->enable_exceptions();
				$this->conn->sql_begin();
				$function_name_user_mode = 'Modify Protection V2';
				$scenarioData = array();
				$target_data_plane = $AZURE_DATA_PLANE;
				$cfs_arr = array();
				
				if ($parameters['ProtectionPlanId'] && isset($parameters['ProtectionPlanId']))
				{
					$planId = trim($parameters['ProtectionPlanId']->getValue());
					$planName = $this->conn->sql_get_value("applicationPlan", "planName", "planId = ?",array($ProtectionPlanId));
				
				
				
				$sql = "SELECT sourceId,scenarioDetails FROM applicationScenario WHERE planId = ? AND scenarioType = ? AND executionStatus != ?";
				$rs = $this->conn->sql_query($sql,array($planId,'DR Protection',$SCENARIO_DELETE_PENDING));
				$protectedSrcIds = array();
				for($i=0;isset($rs[$i]);$i++) 
				{
					$protectedSrcIds[] = $rs[$i]['sourceId'];
					$existingScenarioDetails = $rs[$i]['scenarioDetails'];	
				}
				
				#print_r($protectedSrcIds);
				
				/*Sample datum of scenario details a:9:{s:12:"globalOption";a:15:{s:13:"secureSrctoCX";i:0;s:13:"secureCXtotrg";i:1;s:4:"vacp";s:2:"On";s:11:"syncOptions";i:2;s:15:"syncCopyOptions";s:4:"full";s:14:"resynThreshold";i:2048;s:3:"rpo";s:2:"30";s:11:"batchResync";i:3;s:10:"autoResync";a:6:{s:9:"isEnabled";i:1;s:8:"start_hr";s:2:"12";s:9:"start_min";s:2:"00";s:7:"stop_hr";s:2:"18";s:8:"stop_min";s:2:"00";s:8:"wait_min";s:2:"30";}s:13:"diffThreshold";i:8192;s:11:"compression";i:1;s:15:"processServerid";s:35:"C0F2DE66-CB2A-F148-89FA41C60024D9C8";s:12:"srcNATIPflag";i:0;s:12:"trgNATIPflag";s:1:"1";s:7:"use_cfs";i:0;}s:8:"pairInfo";a:1:{s:74:"9F755DE0-AD01-9841-B6EB63AA5C269F58991=B71F5598-08CA-7A47-9B39CBB26F9C67C1";a:5:{s:8:"sourceId";s:38:"9F755DE0-AD01-9841-B6EB63AA5C269F58991";s:10:"sourceName";s:15:"WIN-CFV0Q93KEAI";s:8:"targetId";s:35:"B71F5598-08CA-7A47-9B39CBB26F9C67C1";s:10:"targetName";s:15:"WIN-T36T0AUAC0F";s:11:"pairDetails";a:3:{s:56:"10000000=9F755DE0-AD01-9841-B6EB63AA5C269F58991_10000000";a:6:{s:6:"srcVol";s:8:"10000000";s:6:"trgVol";s:47:"9F755DE0-AD01-9841-B6EB63AA5C269F58991_10000000";s:11:"srcCapacity";s:11:"64424509440";s:15:"processServerid";s:35:"C0F2DE66-CB2A-F148-89FA41C60024D9C8";s:12:"srcNATIpflag";i:0;s:12:"trgNATIPflag";s:1:"1";}s:56:"30000000=9F755DE0-AD01-9841-B6EB63AA5C269F58991_30000000";a:6:{s:6:"srcVol";s:8:"30000000";s:6:"trgVol";s:47:"9F755DE0-AD01-9841-B6EB63AA5C269F58991_30000000";s:11:"srcCapacity";s:11:"42949672960";s:15:"processServerid";s:35:"C0F2DE66-CB2A-F148-89FA41C60024D9C8";s:12:"srcNATIpflag";i:0;s:12:"trgNATIPflag";s:1:"1";}s:56:"20000000=9F755DE0-AD01-9841-B6EB63AA5C269F58991_20000000";a:6:{s:6:"srcVol";s:8:"20000000";s:6:"trgVol";s:47:"9F755DE0-AD01-9841-B6EB63AA5C269F58991_20000000";s:11:"srcCapacity";s:10:"5368709120";s:15:"processServerid";s:35:"C0F2DE66-CB2A-F148-89FA41C60024D9C8";s:12:"srcNATIpflag";i:0;s:12:"trgNATIPflag";s:1:"1";}}}}s:6:"planId";i:15;s:8:"planName";s:24:"Protection_5552ef68e7442";s:9:"isPrimary";i:1;s:14:"protectionType";s:17:"BACKUP PROTECTION";s:12:"solutionType";s:5:"Azure";s:12:"sourceHostId";s:38:"9F755DE0-AD01-9841-B6EB63AA5C269F58991";s:12:"targetHostId";s:35:"B71F5598-08CA-7A47-9B39CBB26F9C67C1";}
				*/
				#print $existingScenarioDetails;
				
				$defaults = $this->_get_defaults();
				$arr_global_options = $defaults['globalOption'];
				/*Sample datum of arr_global_options
				Array
	(
		[secureSrctoCX] => 0
		[secureCXtotrg] => 1
		[vacp] => On
		[syncOptions] => 2
		[syncCopyOptions] => full
		[resynThreshold] => 2048
		[rpo] => 30
		[batchResync] => 3
		[autoResync] => Array
			(
				[isEnabled] => 1
				[start_hr] => 12
				[start_min] => 00
				[stop_hr] => 18
				[stop_min] => 00
				[wait_min] => 30
			)

		[diffThreshold] => 8192
		[compression] => Enabled
		[processServerid] => 
		[srcNATIPflag] => 0
		[trgNATIPflag] => 0
		[use_cfs] => 0
	)
	*/
				#print_r($arr_global_options);
				
				$planDetails = $this->conn->sql_get_value("applicationPlan","planDetails","planId=?", array($planId));
				$planDetailsArr = unserialize($planDetails);
				$consistencyInterval = $planDetailsArr['ConsistencyInterval'];
				$crashConsistencyInterval = $planDetailsArr['CrashConsistencyInterval'];
			   
				
				#print "$consistencyInterval,$crashConsistencyInterval";
				
				/*
				Sample CFS array
				Array
				(
					[B71F5598-08CA-7A47-9B39CBB26F9C67C1] => Array
						(
							[ipAddress] => 10.10.10.10
							[connectivityType] => NON VPN
						)

				)
				*/
				$cfs_rs = $this->conn->sql_query("SELECT id, (CASE WHEN connectivityType = 'VPN' THEN privateIpAddress  ELSE publicIpAddress  END) as ipAddress,connectivityType from cfs");
				if($this->conn->sql_num_row($cfs_rs))
				{
					while($row = $this->conn->sql_fetch_object($cfs_rs))
					{
						$cfs_arr[$row->id] = array("ipAddress"=>$row->ipAddress,"connectivityType"=>$row->connectivityType);
					}
				}
				#print_r($cfs_arr);
				  
				if($parameters['Servers'])
				{
			   
				   $sourceServerObj = $parameters['Servers']->getChildList();
				  
				   $serverCount = count($sourceServerObj);			   
				   $dvacp_consistency_set = 0;
				   if($serverCount > 1) $dvacp_consistency_set = 1;
					  
					foreach($sourceServerObj as $v)
					{
						$scenarioData = array();
						$scenario_arr = array();
						
						$sourceDetailsObj = $v->getChildList();
					
						if($sourceDetailsObj['DiskDetails'] && isset($sourceDetailsObj['DiskDetails']))
						{
							$diskObj = $sourceDetailsObj['DiskDetails']->getChildList();
							$source_disk_names = array();
							$target_disk_names = array();
							$SourceDiskName = "";
							foreach ($diskObj as $diskData)
							{
							  $diskInfoObj = $diskData->getChildList();
							  if ($diskInfoObj['SourceDiskName'] && isset($diskInfoObj['SourceDiskName']))
							  {
							  $SourceDiskName = trim($diskInfoObj['SourceDiskName']->getValue());
							  $source_disk_names[] = $SourceDiskName;
							  }
							}
							
						}
						/*Sample source disk names structure
						Array
						(
							[0] => \\.\PHYSICALDRIVE1
							[1] => \\.\PHYSICALDRIVE0
							[2] => \\.\PHYSICALDRIVE2
						) */       
						 #print_r($source_disk_names);
						#exit;
						
						if ($sourceDetailsObj['HostId'] && isset($sourceDetailsObj['HostId']))
						$sourceHostId = trim($sourceDetailsObj['HostId']->getValue());
						  
					 
						if ($sourceDetailsObj['ProcessServer'] && isset($sourceDetailsObj['ProcessServer'])) 
						{
							$ProcessServer = trim($sourceDetailsObj['ProcessServer']->getValue());
						}
						
						if($sourceDetailsObj['UseNatIPFor'] && isset($sourceDetailsObj['UseNatIPFor'])) 
						{
							$UseNatIPFor = trim($sourceDetailsObj['UseNatIPFor']->getValue());
						}
						
						if($sourceDetailsObj['MasterTarget'] && isset($sourceDetailsObj['MasterTarget'])) 
						{
							$MasterTarget = trim($sourceDetailsObj['MasterTarget']->getValue());
						}
						
						if($sourceDetailsObj['ProtectionDirection'] && isset($sourceDetailsObj['ProtectionDirection'])) 
						{
							$sourceProtectionDirection = trim($sourceDetailsObj['ProtectionDirection']->getValue()); 
						}
						
						#print "$ProcessServer, $UseNatIPFor, $MasterTarget";
						#print "$sourceProtectionDirection";
						
						$distributedVacp = 'off';
						if($existingScenarioDetails) 
						{
							$existingScenarioDetailsArr = unserialize($existingScenarioDetails);
							$arr_global_options = $existingScenarioDetailsArr['globalOption'];
							$distributedVacp = strtolower($arr_global_options['vacp']);
						}
						
						/*Inserting taget disks to LV table.
						Sample datum for $protection_disk_details
						Array
						(
							[0] => Array
								(
									[sourceDiskName] => \\.\PHYSICALDRIVE1
									[capacity] => 64424509440
									[targetDiskName] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1
								)

							[1] => Array
								(
									[sourceDiskName] => \\.\PHYSICALDRIVE0
									[capacity] => 42949672960
									[targetDiskName] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0
								)

							[2] => Array
								(
									[sourceDiskName] => \\.\PHYSICALDRIVE2
									[capacity] => 5368709120
									[targetDiskName] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2
								)

						)
						*/
						$protection_disk_details = $this->protectionObj->insert_target_disk_details_in_lv_table($sourceHostId, $source_disk_names, $MasterTarget);
	 
						#print_r( $protection_disk_details);
						
						$arr_global_options['processServerid'] = $ProcessServer;
						
						if(strtolower($UseNatIPFor) == 'source') $arr_global_options['srcNATIPflag'] = '1';
						if(strtolower($UseNatIPFor) == 'target') $arr_global_options['trgNATIPflag'] = '1';
						if(strtolower($UseNatIPFor) == 'both')
						{
							$arr_global_options['srcNATIPflag'] = '1';
							$arr_global_options['trgNATIPflag'] = '1';
						}
						
						foreach ($protection_disk_details as $key=>$value)
						{
							$srcVol = $value['sourceDiskName'];
							$trgVol = $value['targetDiskName'];
							$target_disk_names[] = $trgVol;
							$capacity = $value['capacity'];
							$scenario_arr[$srcVol.'='.$trgVol]['srcVol'] = $srcVol;
							$scenario_arr[$srcVol.'='.$trgVol]['trgVol'] = $trgVol;
							$scenario_arr[$srcVol.'='.$trgVol]['srcCapacity'] = $capacity;
							$scenario_arr[$srcVol.'='.$trgVol]['processServerid'] = $arr_global_options['processServerid'];
							$scenario_arr[$srcVol.'='.$trgVol]['srcNATIpflag'] = $arr_global_options['srcNATIPflag'];
							$scenario_arr[$srcVol.'='.$trgVol]['trgNATIPflag'] = $arr_global_options['trgNATIPflag'];
						}
						
						/*Sample scenario array
						Array
						(
							[pairDetails] => Array
								(
									[\\.\PHYSICALDRIVE1=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1] => Array
										(
											[srcVol] => \\.\PHYSICALDRIVE1
											[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1
											[srcCapacity] => 64424509440
											[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
											[srcNATIpflag] => 
											[trgNATIPflag] => 1
										)

									[\\.\PHYSICALDRIVE0=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0] => Array
										(
											[srcVol] => \\.\PHYSICALDRIVE0
											[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0
											[srcCapacity] => 42949672960
											[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
											[srcNATIpflag] => 
											[trgNATIPflag] => 1
										)

									[\\.\PHYSICALDRIVE2=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2] => Array
										(
											[srcVol] => \\.\PHYSICALDRIVE2
											[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2
											[srcCapacity] => 5368709120
											[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
											[srcNATIpflag] => 
											[trgNATIPflag] => 1
										)

								)

						)
						*/
						#print_r($scenario_arr);
						
						
						if ($cfs_arr[$MasterTarget]) 
						{
							if ($cfs_arr[$MasterTarget]["connectivityType"] == "VPN")
							{
								$arr_global_options['use_cfs']  = 0;
							}
							elseif ($cfs_arr[$MasterTarget]["connectivityType"] == "NON VPN")
							{
								$arr_global_options['use_cfs']  = 1;
							}
						}
						else
						{
							$arr_global_options['use_cfs']  = 0;
						}
						
						/*Sample datum for array global operations
						Array
						(
							[vacp] => On
							[rpo] => 30
							[batchResync] => 3
							[compression] => 1
							[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
							[autoResync] => Array
								(
									[start_hr] => 12
									[start_min] => 00
									[stop_hr] => 18
									[stop_min] => 00
									[wait_min] => 30
								)

							[trgNATIPflag] => 1
							[use_cfs] => 1
						)
						print_r($arr_global_options);
						exit;
						*/
						
						$sourceHostName = $this->commonfunc_obj->getHostName($sourceHostId);
						$targetHostName = $this->commonfunc_obj->getHostName($MasterTarget);
						$arr_pair_info = array($sourceHostId.'='.$MasterTarget => array(
																					'sourceId' => $sourceHostId,
																					'sourceName' => $sourceHostName,
																					'targetId' => $MasterTarget,
																					'targetName' => $targetHostName,
																					'pairDetails' => $scenario_arr
																					) );
						
						/*      Sample datum of arr_pair_info             
					   Array
						(
							[9F755DE0-AD01-9841-B6EB63AA5C269F58761=B71F5598-08CA-7A47-9B39CBB26F9C67C1] => Array
								(
									[sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58761
									[sourceName] => WIN-CFV0Q93KEAI
									[targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
									[targetName] => WIN-T36T0AUAC0F
									[pairDetails] => Array
										(
											[pairDetails] => Array
												(
													[\\.\PHYSICALDRIVE1=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE1
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1
															[srcCapacity] => 64424509440
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

													[\\.\PHYSICALDRIVE0=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE0
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0
															[srcCapacity] => 42949672960
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

													[\\.\PHYSICALDRIVE2=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE2
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2
															[srcCapacity] => 5368709120
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

												)

										)

								)

						)
						*/
						#print_r($arr_pair_info);
						
						$unserializeScenarioDetails = array("globalOption" => $arr_global_options, 
												 "pairInfo" => $arr_pair_info, 
												 "planId" => $planId,
												 "planName" => $PlanName,
												 "isPrimary" => 1,
												 "protectionType" => "BACKUP PROTECTION",
												 "protectionPath" => $sourceProtectionDirection,											 
												 "solutionType" => "Azure",
												 "sourceHostId" => $sourceHostId,
												 "targetHostId" => $MasterTarget
												 );
						$scenarioDetails = serialize($unserializeScenarioDetails);
						
						if (count($source_disk_names) > 0 )
							$source_all_disks_details = implode(",",$source_disk_names);
						if (count($target_disk_names) > 0)
							$target_all_disks_details = implode(",",$target_disk_names);
						
						$scenarioData['planId'] = $planId;
						$scenarioData['scenarioType'] = 'DR Protection';
						$scenarioData['scenarioName'] = '';
						$scenarioData['scenarioDescription'] = '';
						$scenarioData['scenarioVersion'] = '';
						$scenarioData['scenarioCreationDate'] = '';
						$scenarioData['scenarioDetails'] = $scenarioDetails;
						$scenarioData['validationStatus'] = '';
						$scenarioData['executionStatus'] = $PREPARE_TARGET_DONE;
						$scenarioData['applicationType'] = 'Azure';
						$scenarioData['applicationVersion'] = '';
						$scenarioData['srcInstanceId'] = '';
						$scenarioData['trgInstanceId'] = '';
						$scenarioData['referenceId'] = '';
						$scenarioData['resyncLimit'] = '';
						$scenarioData['autoProvision'] = '';
						$scenarioData['sourceId'] = $sourceHostId;
						$scenarioData['targetId'] = $MasterTarget;
						$scenarioData['sourceVolumes'] = $source_all_disks_details;
						$scenarioData['targetVolumes'] = $target_all_disks_details;
						$scenarioData['retentionVolumes'] = '';
						$scenarioData['reverseReplicationOption'] = '';
						$scenarioData['protectionDirection'] = 'forward';
						$scenarioData['currentStep'] = '';
						$scenarioData['creationStatus'] = '';
						$scenarioData['isPrimary'] = '';
						$scenarioData['isDisabled'] = '';
						$scenarioData['isModified'] = '';
						$scenarioData['reverseRetentionVolumes'] = '';
						$scenarioData['volpackVolumes'] = '';
						$scenarioData['volpackBaseVolume'] = '';
						$scenarioData['isVolumeResized'] = '';
						$scenarioData['sourceArrayGuid'] = '';
						$scenarioData['targetArrayGuid'] = '';
						$scenarioData['rxScenarioId'] = '';
						/*Sample scenario datum
						Array
						(
							[globalOption] => Array
								(
									[vacp] => On
									[rpo] => 30
									[batchResync] => 3
									[compression] => 1
									[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
									[autoResync] => Array
										(
											[start_hr] => 12
											[start_min] => 00
											[stop_hr] => 18
											[stop_min] => 00
											[wait_min] => 30
										)

									[trgNATIPflag] => 1
									[use_cfs] => 1
								)

							[pairInfo] => Array
								(
									[9F755DE0-AD01-9841-B6EB63AA5C269F58761=B71F5598-08CA-7A47-9B39CBB26F9C67C1] => Array
										(
											[sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58761
											[sourceName] => WIN-CFV0Q93KEAI
											[targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
											[targetName] => WIN-T36T0AUAC0F
											[pairDetails] => Array
												(
													[\\.\PHYSICALDRIVE1=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE1
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE1
															[srcCapacity] => 64424509440
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

													[\\.\PHYSICALDRIVE0=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE0
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE0
															[srcCapacity] => 42949672960
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

													[\\.\PHYSICALDRIVE2=B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2] => Array
														(
															[srcVol] => \\.\PHYSICALDRIVE2
															[trgVol] => B71F5598-08CA-7A47-9B39CBB26F9C67C1_\\.\PHYSICALDRIVE2
															[srcCapacity] => 5368709120
															[processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
															[srcNATIpflag] => 
															[trgNATIPflag] => 1
														)

												)

										)

								)

							[planId] => 62
							[planName] => Protection_55487fea897ab
							[isPrimary] => 1
							[protectionType] => BACKUP PROTECTION
							[solutionType] => Azure
							[sourceHostId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58761
							[targetHostId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
						)
						*/
						#print_r($scenarioData);
						
						// insert scenario details
						$scenarioId = $this->protectionObj->insertScenarioDetails($scenarioData);
						$scenarioIdArr["$sourceHostId"] = $scenarioId;
						
						#print $distributedVacp;
						
						// insert consistency policy
						if(($distributedVacp == 'off')  && ($consistencyInterval)) 
						{
							#print "entered for crash consistency policy.\n";
							#print "$planId, $sourceHostId, $scenarioId, $consistencyOptions";
						   $policyId = $this->protectionObj->insert_consistency_policy($planId, $sourceHostId, $scenarioId, $planDetailsArr);
						   $referenceIds = $referenceIds . $policyId . ",";
						}
						#print "application : $policyId";
						#print_r($consistencyOptions);
						  
						// insert crash consistency policy
						if(($distributedVacp == 'off')  &&  ($crashConsistencyInterval)) 
						{
							#print "entered for dvcap consistency policy.\n";
							$policyId = $this->protectionObj->insert_crash_consistency_policy($planId, $sourceHostId, $scenarioId, $planDetailsArr);
							$referenceIds = $referenceIds . $policyId . ",";
						}
						#print "Crash:  $policyId";
						
						$this->protectionObj->insertCloudProtectionPlan($scenarioId);    
					}
				}
				#print_r($scenarioIdArr);		
				//update distributed consistency policy
				if($distributedVacp == 'on')
				{
					$srcHostIdAry = array_keys($scenarioIdArr);
					$allSrcIds = array_merge($srcHostIdAry,$protectedSrcIds);
					$allSrcIds = array_unique($allSrcIds);
					
					$sql = "SELECT policyId,policyParameters,policyRunEvery FROM policy WHERE planId = ? AND policyType = '4'";
					$rs = $this->conn->sql_query($sql,array($planId));
					
					if(is_array($rs) && (count($rs)>0)) 
					{
						foreach($rs as $k=>$v) 
						{
							$policyId = $v['policyId'];
							
							$del_dvap = "DELETE FROM policy WHERE policyId = ?";
							$rs = $this->conn->sql_query($del_dvap,array($policyId));
							
							$del_dvap_grp = "DELETE FROM consistencyGroups WHERE policyId = ?";
							$rs = $this->conn->sql_query($del_dvap_grp,array($policyId));
						}
					}
					
					#print_r($allSrcIds);
					
					if ($consistencyInterval) 
					{	
						$policyId = $this->protectionObj->insert_dvacp_consistency_policy_v2($allSrcIds,$planId, $planDetailsArr);	
						$referenceIds = $referenceIds . $policyId . ",";
					}
					
					if($crashConsistencyInterval) 
					{
						$policyId = $this->protectionObj->insert_dvacp_crash_consistency_policy($allSrcIds,$planId, $planDetailsArr);
						$referenceIds = $referenceIds . $policyId . ",";
					}  
				}
				}
				$referenceIds = "," . $referenceIds;
				
				$api_ref = $this->commonfunc_obj->insertRequestXml($requestXML, $functionName, $activityId, $clientRequestId, $referenceIds);
				$request_id = $api_ref['apiRequestId'];
				$requestedData = array("planId" => $planId,"scenarioId" => $scenarioIdArr);
				$apiRequestDetailId = $this->commonfunc_obj->insertRequestData($requestedData, $request_id);
				
				$response  = new ParameterGroup ("Response");
				$response->setParameterNameValuePair('RequestId', $request_id);

				// Disable Exceptions for DB
				$this->conn->sql_commit();
				$this->conn->disable_exceptions();
				$done = true;
				return $response->getChildList();
			}
			catch(SQLException $sql_excep)
			{
				$sql_error_code = $sql_excep->getCode();
				
				$this->conn->sql_rollback();
				$this->conn->disable_exceptions();
				switch($sql_error_code)
				{
					case $MYSQL_FOREIGN_KEY_VIOLATION:
						$MasterTargetInfo = $this->commonfunc_obj->get_host_info($MasterTarget);
						if(!$MasterTargetInfo)
						{
							ErrorCodeGenerator::raiseError($functionName, 'EC_NO_MT_FOUND', array('OperationName' => $function_name_user_mode, 'ErrorCode' => ErrorCodes::EC_NO_MT_FOUND));
						}
					break;
					default:
				}				
				
				if ($retry < $DB_TRANSACTION_RETRY_LIMIT)
				{
					sleep($DB_TRANSACTION_RETRY_SLEEP);
					$retry++;
				}
				
				if ($retry == $DB_TRANSACTION_RETRY_LIMIT)
				{
					$logging_obj->my_error_handler("INFO",array("RetryCount"=>$retry),Log::APPINSIGHTS);
					debug_access_log($API_ERROR_LOG, "ModifyProtectionV2 configuration failed: ClientRequestId = $clientRequestId, ActivityId = $activityId, Retry count = $retry \n");
					$this->commonfunc_obj->captureDeadLockDump($requestXML, $activityId, "Protection");
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
				// Disable Exceptions for DB
				$this->conn->disable_exceptions();	
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
			}
		}
	}
	
	public function ModifyProtectionPropertiesV2_validate ($identity, $version, $functionName, $parameters)
	{
		try
		{
			global $BATCH_SIZE;
			
			$this->conn->enable_exceptions();
					
			if(isset($parameters['ProtectionPlanId'])){
				$ProtectionPlanId = trim($parameters['ProtectionPlanId']->getValue());
				// request for retry
				if(!is_numeric($ProtectionPlanId))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ProtectionPlanId"));
				
				// get plan to verify if plan is protected or not
				$getPlanType = $this->protectionObj->getPlanType($ProtectionPlanId);
				// get scenario details to check provided source ids are protected or not
				$scenarioArr = $this->recoveryPlanObj->getScenarioDetails($ProtectionPlanId, 'DR Protection');
				
				if(!$getPlanType)ErrorCodeGenerator::raiseError($functionName, 'EC_NO_PLAN_FOUND', array('PlanID' => $ProtectionPlanId));
				
				if(!count($scenarioArr)){
					ErrorCodeGenerator::raiseError($functionName, 'EC_NO_SCENARIO_EXIST', array('PlanID' => $ProtectionPlanId));
				}
			}
			else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ProtectionPlanId"));
			}
			
			$param = false;
			// if amethyst.conf having BATCH_SIZE value then overriding with ASR passed BatchResync value.
			if(isset($BATCH_SIZE))
			{
				$param = true;
				if((!is_numeric($BATCH_SIZE)) || $BATCH_SIZE <= 0) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "BATCH_SIZE"));
			}
			else if(isset($parameters['BatchResync'])){
				$param = true;
				$BatchResync = trim($parameters['BatchResync']->getValue());
				if((!is_numeric($BatchResync)) || ($BatchResync < 0))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "BatchResync"));
			}
			
			if(isset($parameters['MultiVmSync'])){
				$param = true;
				$MultiVmSync = trim($parameters['MultiVmSync']->getValue());
				if((strtolower($MultiVmSync) != 'on') && (strtolower($MultiVmSync) != 'off')) {
					ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "MultiVmSync"));
				}
			}
			
			if(isset($parameters['RPOThreshold'])){
				$param = true;
				$RPOThreshold = trim($parameters['RPOThreshold']->getValue());
				if(!is_numeric($RPOThreshold))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "RPOThreshold"));
			}
			
			if(isset($parameters['ConsistencyPoints'])){
				$param = true;
				$ConsistencyPoints = trim($parameters['ConsistencyPoints']->getValue());
				if(!is_numeric($ConsistencyPoints))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ConsistencyPoints"));
			}
			
			
			if(isset($parameters['ApplicationConsistencyPoints'])){
				$param = true;
				$ApplicationConsistencyPoints = trim($parameters['ApplicationConsistencyPoints']->getValue());
				if(!is_numeric($ApplicationConsistencyPoints))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "ApplicationConsistencyPoints"));
			}
			
			if(isset($parameters['CrashConsistencyInterval']))
			{
				$param = true;
				$CrashConsistencyInterval = trim($parameters['CrashConsistencyInterval']->getValue());
				if(!is_numeric($CrashConsistencyInterval))ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "CrashConsistencyInterval"));
			}
			
			if(isset($parameters['Compression'])){
			$param = true;	
			$Compression = trim($parameters['Compression']->getValue());
			if((strtolower($Compression) != 'enabled') && (strtolower($Compression) != 'disabled')) {
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Compression"));
			}
			}
			
			// validate auto resync option 
			if(isset($parameters['AutoResyncOptions'])){
				$param = true;	
			  $autoResyncObj = $parameters['AutoResyncOptions']->getChildList();
			  $patternMatch = '/^([01]?[0-9]|2[0-3]):[0-5][0-9]$/';
			  if(isset($autoResyncObj['StartTime'])){
				$StartTime = trim($autoResyncObj['StartTime']->getValue());
				preg_match($patternMatch, $StartTime, $StartTimeMatches);
				if(!$StartTimeMatches[0]){
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "StartTime"));
				}
			  }
			  else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "StartTime"));
			  }
			  
			  if(isset($autoResyncObj['EndTime'])){
				$EndTime = trim($autoResyncObj['EndTime']->getValue()); 	
				preg_match($patternMatch, $EndTime, $EndTimeMatches);
				if(!$EndTimeMatches[0]){
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "EndTime"));
				}
			  }
			  else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "EndTime"));
			  }
			  
			  if(isset($autoResyncObj['Interval'])){
				$Interval = trim($autoResyncObj['Interval']->getValue()); 
				if($Interval < 10){
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Interval"));
				}	
			  }
			   else{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Interval"));
			  }
			}
			if(!$param){
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "ModifyProtectionProperties"));
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
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
	
	public function ModifyProtectionPropertiesV2 ($identity, $version, $functionName, $parameters)
	{
		try
		{
            global $CRASH_CONSISTENCY_INTERVAL, $DVACP_CRASH_CONSISTENCY_INTERVAL;
			global $BATCH_SIZE;
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			if ($parameters['ProtectionPlanId'] && isset($parameters['ProtectionPlanId']))
            {
			
			$ProtectionPlanId = trim($parameters['ProtectionPlanId']->getValue());
            $planDetails = $this->conn->sql_get_value("applicationPlan","planDetails","planId=?", array($ProtectionPlanId));
			$planDetailsArr = unserialize($planDetails);
			$consistencyInterval = $planDetailsArr['ConsistencyInterval'];
			$crashConsistencyInterval = $planDetailsArr['CrashConsistencyInterval'];
            
			if($parameters['RPOThreshold'] && isset($parameters['RPOThreshold']))
			{
				$RPOThreshold = trim($parameters['RPOThreshold']->getValue());
			}
			#print $RPOThreshold;
            
			if($parameters['MultiVmSync'] && isset($parameters['MultiVmSync']))
			{
				$MultiVmSync = trim($parameters['MultiVmSync']->getValue());
			}
			#print $MultiVmSync;
            
			if($parameters['Compression'] && isset($parameters['Compression']))
			{
				$Compression = trim($parameters['Compression']->getValue());
			}
			#print $Compression;
            
            
			if(isset($parameters['ApplicationConsistencyPoints']))
			{
				$consistencyInterval = trim($parameters['ApplicationConsistencyPoints']->getValue());
			}
			#print $ConsistencyInterval
			
			if ($parameters['CrashConsistencyInterval'] && isset($parameters['CrashConsistencyInterval']))
			{
				$crashConsistencyInterval = trim($parameters['CrashConsistencyInterval']->getValue());
			}
			
            $consistencyPolicyOptions["ConsistencyInterval"] = $consistencyInterval;
            $consistencyPolicyOptions["CrashConsistencyInterval"]= $crashConsistencyInterval;
			#print $CrashConsistencyInterval;
            /*
            Array
            (
                [ConsistencyInterval] => 30
                [CrashConsistencyInterval] => 30
            )
            */

			if($parameters['AutoResyncOptions'] && isset($parameters['AutoResyncOptions']))
			{
				$autoResyncObj = $parameters['AutoResyncOptions']->getChildList();
				$StartTime = trim($autoResyncObj['StartTime']->getValue()); 
				$EndTime = trim($autoResyncObj['EndTime']->getValue()); 
				$Interval = trim($autoResyncObj['Interval']->getValue()); 
				
				$StartTimeArr = explode(":", $StartTime);
				$EndTimeArr = explode(":", $EndTime);
			}
               
               /*
               12:00, 18:00, 600Array
                (
                    [0] => 12
                    [1] => 00
                )
                Array
                (
                    [0] => 18
                    [1] => 00
                )
                */
            #print "$StartTime, $EndTime, $Interval";
            #print_r($StartTimeArr);
            #print_r($EndTimeArr);
           
		    if(isset($BATCH_SIZE))
			{
				$BatchResync = $BATCH_SIZE;
				$updateBatch = $this->protectionObj->updateBatchResync($ProtectionPlanId,$BatchResync);
			}
			else if($parameters['BatchResync'] && isset($parameters['BatchResync']))
			{
				$BatchResync = trim($parameters['BatchResync']->getValue());
				$updateBatch = $this->protectionObj->updateBatchResync($ProtectionPlanId,$BatchResync);
			}
			
            #print "$BatchResync, $updateBatch";
            
			$sourceIdList = array();
			
			// get scenario details to check provided source ids are protected or not
			$scenarioArr = $this->recoveryPlanObj->getScenarioDetails($ProtectionPlanId, 'DR Protection');
            /*
            Array
(
    [0] => Array
        (
            [scenarioId] => 14
            [scenarioType] => DR Protection
            [sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58981
            [targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
            [applicationVersion] => 
            [reverseReplicationOption] => 0
            [executionStatus] => 92
            [scenarioCreationDate] => 0000-00-00 00:00:00
            [srcInstanceId] => 
            [trgInstanceId] => 
            [applicationType] => Azure
            [isDisabled] => 
            [isModified] => 0
            [currentStep] => 
            [referenceId] => 0
            [planId] => 14
        )

    [1] => Array
        (
            [scenarioId] => 15
            [scenarioType] => DR Protection
            [sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58991
            [targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
            [applicationVersion] => 
            [reverseReplicationOption] => 0
            [executionStatus] => 92
            [scenarioCreationDate] => 0000-00-00 00:00:00
            [srcInstanceId] => 
            [trgInstanceId] => 
            [applicationType] => Azure
            [isDisabled] => 
            [isModified] => 0
            [currentStep] => 
            [referenceId] => 0
            [planId] => 14
        )

    [2] => Array
        (
            [scenarioId] => 16
            [scenarioType] => DR Protection
            [sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58521
            [targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
            [applicationVersion] => 
            [reverseReplicationOption] => 0
            [executionStatus] => 92
            [scenarioCreationDate] => 0000-00-00 00:00:00
            [srcInstanceId] => 
            [trgInstanceId] => 
            [applicationType] => Azure
            [isDisabled] => 
            [isModified] => 0
            [currentStep] => 
            [referenceId] => 0
            [planId] => 14
        )

    [3] => Array
        (
            [scenarioId] => 17
            [scenarioType] => DR Protection
            [sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58511
            [targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
            [applicationVersion] => 
            [reverseReplicationOption] => 0
            [executionStatus] => 92
            [scenarioCreationDate] => 0000-00-00 00:00:00
            [srcInstanceId] => 
            [trgInstanceId] => 
            [applicationType] => Azure
            [isDisabled] => 
            [isModified] => 0
            [currentStep] => 
            [referenceId] => 0
            [planId] => 14
        )

)
*/
            #print_r($scenarioArr);
            
			if(count($scenarioArr) > 0)
			{
				foreach($scenarioArr as $scenarioData)
				{
					$updateColumnStr = '';
					$parameterArr = array();
					$scenarioId = $scenarioData['scenarioId'];
					$sourceId = $scenarioData['sourceId'];
					$targetId = $scenarioData['targetId'];
					$scenarioDetails = $this->conn->sql_get_value('applicationscenario', 'scenarioDetails','scenarioId=?', array($scenarioId));
					$plan_properties = unserialize($scenarioDetails);
                    /*
                    Array
(
    [globalOption] => Array
        (
            [secureSrctoCX] => 0
            [secureCXtotrg] => 1
            [vacp] => Off
            [syncOptions] => 2
            [syncCopyOptions] => full
            [resynThreshold] => 2048
            [rpo] => 30
            [batchResync] => 3
            [autoResync] => Array
                (
                    [isEnabled] => 1
                    [start_hr] => 12
                    [start_min] => 00
                    [stop_hr] => 18
                    [stop_min] => 00
                    [wait_min] => 30
                )

            [diffThreshold] => 8192
            [compression] => 1
            [processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
            [srcNATIPflag] => 0
            [trgNATIPflag] => 1
            [use_cfs] => 0
        )

    [pairInfo] => Array
        (
            [9F755DE0-AD01-9841-B6EB63AA5C269F58981=B71F5598-08CA-7A47-9B39CBB26F9C67C1] => Array
                (
                    [sourceId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58981
                    [sourceName] => WIN-CFV0Q93KEAI
                    [targetId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
                    [targetName] => WIN-T36T0AUAC0F
                    [pairDetails] => Array
                        (
                            [10000000=9F755DE0-AD01-9841-B6EB63AA5C269F58981_10000000] => Array
                                (
                                    [srcVol] => 10000000
                                    [trgVol] => 9F755DE0-AD01-9841-B6EB63AA5C269F58981_10000000
                                    [srcCapacity] => 64424509440
                                    [processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
                                    [srcNATIpflag] => 0
                                    [trgNATIPflag] => 1
                                )

                            [30000000=9F755DE0-AD01-9841-B6EB63AA5C269F58981_30000000] => Array
                                (
                                    [srcVol] => 30000000
                                    [trgVol] => 9F755DE0-AD01-9841-B6EB63AA5C269F58981_30000000
                                    [srcCapacity] => 42949672960
                                    [processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
                                    [srcNATIpflag] => 0
                                    [trgNATIPflag] => 1
                                )

                            [20000000=9F755DE0-AD01-9841-B6EB63AA5C269F58981_20000000] => Array
                                (
                                    [srcVol] => 20000000
                                    [trgVol] => 9F755DE0-AD01-9841-B6EB63AA5C269F58981_20000000
                                    [srcCapacity] => 5368709120
                                    [processServerid] => C0F2DE66-CB2A-F148-89FA41C60024D9C8
                                    [srcNATIpflag] => 0
                                    [trgNATIPflag] => 1
                                )

                        )

                )

        )

    [planId] => 14
    [planName] => Protection_555348de577ba
    [isPrimary] => 1
    [protectionType] => BACKUP PROTECTION
    [solutionType] => Azure
    [sourceHostId] => 9F755DE0-AD01-9841-B6EB63AA5C269F58981
    [targetHostId] => B71F5598-08CA-7A47-9B39CBB26F9C67C1
)
*/
					#print_r($plan_properties);
                    
					if(isset($BatchResync))
					{
						$plan_properties['globalOption']['batchResync'] = $BatchResync;
					}
					
					if(isset($MultiVmSync))
					{
						if(strtolower($MultiVmSync) == 'on')
						{
							// insert dvacp
							$dvacpSourceIdList[] = $sourceId;
						}
						elseif(strtolower($MultiVmSync) == 'off')
						{
							
							// insert normal consistency
							$normalConsistencySourceList[] = $sourceId;
						}
						$plan_properties['globalOption']['vacp'] = $MultiVmSync;
					}
					
					$scenario_ids[] = $scenarioId;
					
					if(isset($RPOThreshold))
					{
						$plan_properties['globalOption']['rpo'] = $RPOThreshold;
						$updateColumnStr .= 'rpoSLAThreshold = ?';
						array_push($parameterArr, $RPOThreshold);
					}
					
					if(isset($Compression))
					{
						$plan_properties['globalOption']['compression'] = $Compression;
						$CompressionVal = 0;
						if(strtolower($Compression) == 'enabled')
						{
							$CompressionVal = 1;
						}
					
						if(isset($updateColumnStr) && $updateColumnStr != '')
                        {
							$updateColumnStr .= ',';
						}
						
						$updateColumnStr .= 'compressionEnable = ?';
						array_push($parameterArr, $CompressionVal);
					}
					
                    
					if($parameters['AutoResyncOptions'] && isset($parameters['AutoResyncOptions']))
                    {
						$plan_properties['globalOption']['autoResync']['isEnabled'] = 1;
						$plan_properties['globalOption']['autoResync']['start_hr'] = $StartTimeArr[0];
						$plan_properties['globalOption']['autoResync']['start_min'] = $StartTimeArr[1];
						$plan_properties['globalOption']['autoResync']['stop_hr'] = $EndTimeArr[0];
						$plan_properties['globalOption']['autoResync']['stop_min'] = $EndTimeArr[1];
						$plan_properties['globalOption']['autoResync']['wait_min'] = $Interval;
						
						if(isset($updateColumnStr) && $updateColumnStr != '')
                        {
							$updateColumnStr .= ',';
						}
						
						$updateColumnStr .= "autoResyncStartTime = ?, autoResyncStartHours = ?, autoResyncStartMinutes = ?, autoResyncStopHours = ?, autoResyncStopMinutes = ?";
						array_push($parameterArr, $Interval, $StartTimeArr[0], $StartTimeArr[1], $EndTimeArr[0], $EndTimeArr[1]);
					
					}
					
                    /*
                    rpoSLAThreshold = ?,compressionEnable = ?,autoResyncStartTime = ?, autoResyncStartHours = ?, autoResyncStartMinutes = ?, autoResyncStopHours = ?, autoResyncStopMinutes = ?
                    */
                    #print $updateColumnStr;
                    
					if((isset($RPOThreshold)) || (isset($Compression)) || (isset($AutoResyncOptions)))
					{
						array_push($parameterArr, $sourceId, $targetId);
						$updateDb = $this->protectionObj->update_protection_properties($updateColumnStr, $parameterArr);
					}
					
                    /*
                    a:9:{s:12:"globalOption";a:15:{s:13:"secureSrctoCX";i:0;s:13:"secureCXtotrg";i:1;s:4:"vacp";s:2:"On";s:11:"syncOptions";i:2;s:15:"syncCopyOptions";s:4:"full";s:14:"resynThreshold";i:2048;s:3:"rpo";s:2:"20";s:11:"batchResync";s:1:"4";s:10:"autoResync";a:6:{s:9:"isEnabled";i:1;s:8:"start_hr";s:2:"11";s:9:"start_min";s:2:"00";s:7:"stop_hr";s:2:"20";s:8:"stop_min";s:2:"00";s:8:"wait_min";s:4:"1200";}s:13:"diffThreshold";i:8192;s:11:"compression";s:8:"Disabled";s:15:"processServerid";s:35:"C0F2DE66-CB2A-F148-89FA41C60024D9C8";s:12:"srcNATIPflag";i:0;s:12:"trgNATIPflag";s:1:"1";s:7:"use_cfs";i:0;}s:8:"pairInfo";a:1:{s:74:"9F755DE0-AD01-9841-B6EB63AA5C269F58981=B71F5598-08CA-7A47-9B39CBB26F9C67C1";a:5:{s:8:"sourceId";s:38:"9F755DE0-AD01-9841-B6EB63AA5C269F58981";s:10:"sourceName";s:15:"WIN-CFV0Q93KEAI";s:8:"targetId";s:35:"B71F5598-08CA-7A47-9B39CBB26F9C67C1";s:10:"targetName";s:15:"WIN-T36T0AUAC0F";s:11:"pairDetails";a:3:{s:56:"10000000=9F755DE0-AD01-9841-B6EB63AA5C269F58981_10000000";a:6:{s:6:"srcVol";s:8:"10000000";s:6:"trgVol";s:47:"9F755DE0-AD01-9841-B6EB63AA5C269F58981_10000000";s:11:"srcCapacity";s:11:"64424509440";s:15:"processServerid";s:35:"C0F2DE66-CB2A-F148-89FA41C60024D9C8";s:12:"srcNATIpflag";i:0;s:12:"trgNATIPflag";s:1:"1";}s:56:"30000000=9F755DE0-AD01-9841-B6EB63AA5C269F58981_30000000";a:6:{s:6:"srcVol";s:8:"30000000";s:6:"trgVol";s:47:"9F755DE0-AD01-9841-B6EB63AA5C269F58981_30000000";s:11:"srcCapacity";s:11:"42949672960";s:15:"processServerid";s:35:"C0F2DE66-CB2A-F148-89FA41C60024D9C8";s:12:"srcNATIpflag";i:0;s:12:"trgNATIPflag";s:1:"1";}s:56:"20000000=9F755DE0-AD01-9841-B6EB63AA5C269F58981_20000000";a:6:{s:6:"srcVol";s:8:"20000000";s:6:"trgVol";s:47:"9F755DE0-AD01-9841-B6EB63AA5C269F58981_20000000";s:11:"srcCapacity";s:10:"5368709120";s:15:"processServerid";s:35:"C0F2DE66-CB2A-F148-89FA41C60024D9C8";s:12:"srcNATIpflag";i:0;s:12:"trgNATIPflag";s:1:"1";}}}}s:6:"planId";i:14;s:8:"planName";s:24:"Protection_555348de577ba";s:9:"isPrimary";i:1;s:14:"protectionType";s:17:"BACKUP PROTECTION";s:12:"solutionType";s:5:"Azure";s:12:"sourceHostId";s:38:"9F755DE0-AD01-9841-B6EB63AA5C269F58981";s:12:"targetHostId";s:35:"B71F5598-08CA-7A47-9B39CBB26F9C67C1";}
                    */
                    #print_r($plan_properties);
                    #exit;
					$str = serialize($plan_properties);
                    #print $str;
                    
					$sql = "UPDATE
									applicationScenario
							SET
									scenarioDetails = ? 
							WHERE
									scenarioId = ? AND
									planId = ?";

					$rs = $this->conn->sql_query($sql, array($str, $scenarioId, $ProtectionPlanId));
				}
			}
			
            /*a:2:{s:19:"ConsistencyInterval";s:2:"20";s:24:"CrashConsistencyInterval";s:2:"10";}*/
            if (isset($consistencyPolicyOptions))
            {
                $planDetails = serialize($consistencyPolicyOptions);
                #print $planDetails;
                
                $this->conn->sql_query("UPDATE applicationPlan SET planDetails = ? WHERE planId = ?", array($planDetails,$ProtectionPlanId));
            }
			/*
            Array
            (
                [0] => 9F755DE0-AD01-9841-B6EB63AA5C269F58981
                [1] => 9F755DE0-AD01-9841-B6EB63AA5C269F58991
                [2] => 9F755DE0-AD01-9841-B6EB63AA5C269F58521
                [3] => 9F755DE0-AD01-9841-B6EB63AA5C269F58511
            )*/
            #print_r($dvacpSourceIdList);
            #print $ProtectionPlanId;
            /*Array
            (
                [0] => 14
                [1] => 15
                [2] => 16
                [3] => 17
            )
            */
            #print_r($scenario_ids);
            if (isset($consistencyInterval) && isset($crashConsistencyInterval) && isset($MultiVmSync))
			{
                
				$sql = "SELECT policyId,policyParameters,policyRunEvery FROM policy WHERE planId = ? AND policyType = '4'";
				$rs = $this->conn->sql_query($sql,array($ProtectionPlanId));
				
				if(is_array($rs) && (count($rs)>0)) 
				{
					foreach($rs as $k=>$v) 
					{
						$policyId = $v['policyId'];
						
						$del_dvap = "DELETE FROM policy WHERE policyId = ?";
						$rs = $this->conn->sql_query($del_dvap,array($policyId));
						
						$del_dvap_grp = "DELETE FROM consistencyGroups WHERE policyId = ?";
						$rs = $this->conn->sql_query($del_dvap_grp,array($policyId));
					}
				}
				
				if(count($dvacpSourceIdList) > 1)
				{       
						if ($consistencyPolicyOptions["ConsistencyInterval"] > 0)
						{
							$this->protectionObj->insert_dvacp_consistency_policy_v2($dvacpSourceIdList,$ProtectionPlanId, $consistencyPolicyOptions);	
						}
						if ( $consistencyPolicyOptions["CrashConsistencyInterval"] > 0)
						{
							$this->protectionObj->insert_dvacp_crash_consistency_policy($dvacpSourceIdList,$ProtectionPlanId, $consistencyPolicyOptions);  
						}
				}
				elseif((count($normalConsistencySourceList) > 0 || count($dvacpSourceIdList) == 1) && $consistencyPolicyOptions)
				{
					$sourceServerList = $dvacpSourceIdList;
					if(count($normalConsistencySourceList) > 0)
					{
						$sourceServerList = $normalConsistencySourceList;
					}
					
					for($i=0;$i<count($sourceServerList);$i++)
					{
						if ($consistencyPolicyOptions["ConsistencyInterval"] > 0)
						{
							$policyId = $this->protectionObj->insert_consistency_policy($ProtectionPlanId, $sourceServerList[$i], $scenario_ids[$i], $consistencyPolicyOptions);
						}
						if ( $consistencyPolicyOptions["CrashConsistencyInterval"] > 0)
						{ 
							$policyId = $this->protectionObj->insert_crash_consistency_policy($ProtectionPlanId, $sourceServerList[$i], $scenario_ids[$i], $consistencyPolicyOptions);
						}
					}
				}
			}
			}			
            $response  = new ParameterGroup ("Response");
            $response->setParameterNameValuePair('Status', "Success");

			// Disable Exceptions for DB
			$this->conn->sql_commit();
			$this->conn->disable_exceptions();
			
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
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
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
		
	}
}
?>