<?php
/*
 *================================================================= 
 * FILENAME
 *    PSAPI.php
 *
 * DESCRIPTION
 *    This script contains functions related to Process Server
 *
 * HISTORY
 *     Date         Author          Comments
 *     28-Aug-2008  Akshay Hegde    Created
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
Class PSAPI
{
	private $commonfunctions_obj;
    private $conn;
	
	/*
    * Function Name: PSAPI
    *
    * Description:
    *    This is the constructor of the class 
    *    performs the following actions.
    *
    *    <Give more details here>    
    *
    * Parameters:
    *     Param 1 [IN/OUT]:
    *     Param 2 [IN/OUT]:
    *
    * Return Value:
    *     Ret Value: <Return value, specify whether Array or Scalar>
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */    
    function __construct()
    {
		global $conn_obj;
        $this->commonfunctions_obj = new CommonFunctions();
        $this->conn_obj = $conn_obj;
		$this->batchsql_obj = new BatchSqls();
    }
	
	public function getPlanDetails($plan_ids) 
	{
		$plan_sql = "SELECT
						solutionType,
						planId,
						planName
					FROM
						applicationPlan
					WHERE
						planId IN ('$plan_ids')";
		$plan_data_res = $this->conn_obj->sql_exec($plan_sql);
		
		$plan_list = array();
		$plan_data = array();
		foreach($plan_data_res as $plan_info) {
			$plan_list['solutionType'] = $plan_info['solutionType'];
			$plan_list['planName'] = $plan_info['planName'];
			
			$plan_data[$plan_info['planId']] = $plan_list;	
		}
		return $plan_data;
	}
	
	public function getNTWAddDetails($host_id) 
	{
		$ntw_add_sql = "SELECT
							ipAddress,
							hostId,
							deviceName
						FROM
							networkAddress
						WHERE 
							hostId = '$host_id'";
		$ntw_data_res = $this->conn_obj->sql_exec($ntw_add_sql);
		
		$ntw_list = array();
		$ntw_add_data = array();
		foreach($ntw_data_res as $ntw_info) {
			$ntw_list['ipAddress'] = $ntw_info['ipAddress'];
			$ntw_list['hostId'] = $ntw_info['hostId'];
			
			$ntw_add_data[$ntw_info['deviceName']."!!".$ntw_info['ipAddress']] = $ntw_list;	
		}
		return 	$ntw_add_data;
	}
	
	public function getStandbyDetails() 
	{	
		$standby_sql = "SELECT
							ip_address,
							timeout,
							port_number,
							role,
							appliacneClusterIdOrHostId
						FROM
							standByDetails";
		$stand_by_data_res = $this->conn_obj->sql_exec($standby_sql);
			
		$stand_by_list = array();
		$stand_by_data = array();
		foreach($stand_by_data_res as $stand_by_info) {
			$stand_by_list['timeout'] = $stand_by_info['timeout'];
			$stand_by_list['port_number'] = $stand_by_info['port_number'];
			$stand_by_list['role'] = $stand_by_info['role'];
			$stand_by_list['ip_address'] = $stand_by_info['ip_address'];
			
			$stand_by_data[$stand_by_info['appliacneClusterIdOrHostId']] = $stand_by_list;	
		}
		return 	$stand_by_data;
	}
	
	public function getClusterHA() 
	{		
		$app_node_sql = "SELECT
					nodeId,
					nodeRole,
					nodeIp
				FROM
					applianceNodeMap";
		$app_node_data_res = $this->conn_obj->sql_exec($app_node_sql);
		
		$app_node_list = array();
		$app_node_data = array();
        
		foreach($app_node_data_res as $app_node_info) {
			$app_node_list['nodeIp'] = $app_node_info['nodeIp'];
			$app_node_list['nodeRole'] = $app_node_info['nodeRole'];
			$app_node_data[$app_node_info['nodeId']] = $app_node_list;
		}
		
		$app_clus_sql = "SELECT
					applianceId,
					applianceIp
				FROM 
					applianceCluster";
		$app_clus_data_res = $this->conn_obj->sql_exec($app_clus_sql);
		
		$app_clus_list = array();
		$app_clus_data = array();
		foreach($app_clus_data_res as $app_clus_info){
			$app_clus_list['applianceIp'] = $app_clus_info['applianceIp'];
			$app_clus_data[$app_clus_info['applianceId']] = $app_clus_list;	
		}
		return array($app_node_data, $app_clus_data);
	}
	
	public function getPSDetails($host_id) 
	{
		$ps_sql = "SELECT 
						processServerId,
						cummulativeThrottle,
						port,
						ps_natip,
						ipaddress						
					FROM
						processServer
					WHERE
						processServerId='$host_id'";
		$ps_data_res = $this->conn_obj->sql_exec($ps_sql);
		
		$ps_list = array();
		$psdata = array();
		foreach($ps_data_res as $ps_info){
			$ps_list['cummulativeThrottle'] = $ps_info['cummulativeThrottle'];
			$ps_list['port'] = $ps_info['port'];
			$ps_list['ps_natip'] = $ps_info['ps_natip'];
			$ps_list['ipaddress'] = $ps_info['ipaddress'];
			$psdata[$ps_info['processServerId']] = $ps_list;	
		}
		
		return $psdata;
	}
	
	public function getLogDetails() 
	{	
		$tbs_sql = "SELECT
						ValueName,
						ValueData,
						id
					FROM
						transbandSettings";
		$tbs_data = $this->conn_obj->sql_exec($tbs_sql);
		
		
		// $tbs_data_res = $this->conn_obj->sql_exec($tbs_sql);
		
		// $tbs_list = array();
		// foreach($tbs_data_res as $tbs_info){
			// $tbs_list[$tbs_info['ValueName']] = $tbs_info['ValueData'];
			// $tbs_data[$tbs_info['ValueName']] = $tbs_list;	
		// }	
		
		$log_sql = "SELECT
						id,
						logName,
						logPathLinux,
						logPathWindows, 
						logSizeLimit,
						logTimeLimit,
						unix_timestamp(startTime) as startTime,
						unix_timestamp(now()) as presentTime								
					FROM 
						logRotationList";
		$log_data_res = $this->conn_obj->sql_exec($log_sql);
		
		$log_data_list = array();
		$log_data = array();
		foreach($log_data_res as $log_info){
			$log_data_list['logName'] = $log_info['logName'];
			$log_data_list['logPathLinux'] = $log_info['logPathLinux'];
			$log_data_list['logPathWindows'] = $log_info['logPathWindows'];
			$log_data_list['logSizeLimit'] = $log_info['logSizeLimit'];
			$log_data_list['logTimeLimit'] = $log_info['logTimeLimit'];
			$log_data_list['startTime'] = $log_info['startTime'];
			$log_data_list['presentTime'] = $log_info['presentTime'];
			
			$log_data[$log_info['id']] = $log_data_list;	
		}
			
		return array($tbs_data,$log_data);		
	}

	public function getPairData($host_id)	
	{		
		$pair_sql = "SELECT							
						sourceHostId,							
						sourceDeviceName,							
						destinationHostId,							
						destinationDeviceName,							
						shouldResync,							
						currentRPOTime,							
						statusUpdateTime,							
						resyncVersion,							
						ruleId,							
						isResync,							
						isQuasiflag,							
						resyncSetCxtimestamp,							
						throttleresync,							
						throttleDifferentials,							
						autoResyncStartType,							
						autoResyncStartTime, 							
						autoResyncStartHours,
						autoResyncStartMinutes,
						autoResyncStopHours,
						autoResyncStopMinutes,
						oneToManySource,
						jobId,
						compressionEnable,
						pairId,
						maxResyncFilesThreshold,
						resyncStartTagtime,
						resyncEndTagtime,							
						Phy_Lunid,							
						planId,							
						processServerId,														
						UNIX_TIMESTAMP(now()) as unix_time, 							
						resyncOrder, 							
						executionState,							
						scenarioId,
						replication_status,						
						HOUR(now()) as hour, 
						MINUTE(now()) as minute,
						isCommunicationfromSource,
						rpoSLAThreshold,
						lun_state,
						profilingMode,
						replicationCleanupOptions,
						restartResyncCounter,
						UNIX_TIMESTAMP(resyncStartTime) as resyncStartTime,
						UNIX_TIMESTAMP(resyncEndTime) as resyncEndTime,
						UNIX_TIMESTAMP(lastTMChange) as lastTMChange,
						deleted,
						src_replication_status,	
						tar_replication_status,
						UNIX_TIMESTAMP(statusUpdateTime) as statusUpdateTime_unix_time,
						TargetDataPlane 		
					FROM							
						srcLogicalVolumeDestLogicalVolume						
					WHERE							
						processServerId='$host_id'";				
		$pair_data_res = $this->conn_obj->sql_exec($pair_sql);				
			
		$pair_list = array();
		$pair_data = array();
		$protected_host_id = array();
		$protected_src_vols = array();
		$protected_tgt_vols = array();
		$phy_lun_ids = array();
		$plan_ids = array();
		$one_to_many_targets = array();
		$count = 1;
		foreach($pair_data_res as $pair_info){
			$pair_list['sourceHostId'] = $pair_info['sourceHostId'];
			$pair_list['sourceDeviceName'] = $pair_info['sourceDeviceName'];
			$pair_list['destinationDeviceName'] = $pair_info['destinationDeviceName'];
			$pair_list['destinationHostId'] = $pair_info['destinationHostId'];
			$pair_list['isQuasiflag'] = $pair_info['isQuasiflag'];
			$pair_list['throttleresync'] = $pair_info['throttleresync'];
			$pair_list['throttleDifferentials'] = $pair_info['throttleDifferentials'];
			$pair_list['maxResyncFilesThreshold'] = $pair_info['maxResyncFilesThreshold'];
			$pair_list['resyncStartTagtime'] = $pair_info['resyncStartTagtime'];
			$pair_list['resyncEndTagtime'] = $pair_info['resyncEndTagtime'];
			$pair_list['Phy_Lunid'] = $pair_info['Phy_Lunid'];
			$pair_list['planId'] = $pair_info['planId'];
			$pair_list['processServerId'] = $pair_info['processServerId'];
			$pair_list['replication_status'] = $pair_info['replication_status'];
			$pair_list['jobId'] = $pair_info['jobId'];
			$pair_list['isCommunicationfromSource'] = $pair_info['isCommunicationfromSource'];
			$pair_list['shouldResync'] = $pair_info['shouldResync'];
			$pair_list['oneToManySource'] = $pair_info['oneToManySource'];
			$pair_list['resyncVersion'] = $pair_info['resyncVersion'];
			$pair_list['ruleId'] = $pair_info['ruleId'];
			$pair_list['compressionEnable'] = $pair_info['compressionEnable'];
			$pair_list['autoResyncStartType'] = $pair_info['autoResyncStartType'];
			$pair_list['autoResyncStartTime'] = $pair_info['autoResyncStartTime'];
			$pair_list['autoResyncStartHours'] = $pair_info['autoResyncStartHours'];
			$pair_list['autoResyncStartMinutes'] = $pair_info['autoResyncStartMinutes'];
			$pair_list['autoResyncStopHours'] = $pair_info['autoResyncStopHours'];
			$pair_list['autoResyncStopMinutes'] = $pair_info['autoResyncStopMinutes'];
			$pair_list['resyncOrder'] = $pair_info['resyncOrder'];
			$pair_list['executionState'] = $pair_info['executionState'];
			$pair_list['scenarioId'] = $pair_info['scenarioId'];
			$pair_list['hour'] = $pair_info['hour'];
			$pair_list['minute'] = $pair_info['minute'];
			$pair_list['unix_time'] = $pair_info['unix_time'];
			$pair_list['isResync'] = $pair_info['isResync'];
			$pair_list['currentRPOTime'] = $pair_info['currentRPOTime'];
			$pair_list['statusUpdateTime'] = $pair_info['statusUpdateTime'];
			$pair_list['resyncSetCxtimestamp'] = $pair_info['resyncSetCxtimestamp'];
			$pair_list['lunState'] = $pair_info['lun_state'];
			$pair_list['profilingMode'] = $pair_info['profilingMode'];
			$pair_list['replicationCleanupOptions'] = $pair_info['replicationCleanupOptions'];
			$pair_list['restartResyncCounter'] = $pair_info['restartResyncCounter'];
			$pair_list['resyncStartTime'] = $pair_info['resyncStartTime'];
			$pair_list['resyncEndTime'] = $pair_info['resyncEndTime'];
			$pair_list['rpoSLAThreshold'] = $pair_info['rpoSLAThreshold'];
			$pair_list['lastTMChange'] = $pair_info['lastTMChange'];
			$pair_list['deleted'] = $pair_info['deleted'];
			$pair_list['src_replication_status'] = $pair_info['src_replication_status'];
			$pair_list['tar_replication_status'] = $pair_info['tar_replication_status'];
			$pair_list['statusUpdateTime_unix_time'] = $pair_info['statusUpdateTime_unix_time'];
			$pair_list['TargetDataPlane'] = $pair_info['TargetDataPlane'];
			
			$pair_data[$pair_info['pairId']."!!".$pair_info['sourceHostId']] = $pair_list;	
			
			if($pair_info['executionState'] != 2 && $pair_info['executionState'] != 3)
			{
				$one_to_many_targets[$pair_info['sourceHostId']."!!".$pair_info['sourceDeviceName']][$count] = $pair_info['destinationHostId']."!!".$pair_info['destinationDeviceName']."!!".$pair_info['resyncStartTagtime']."!!".$pair_info['autoResyncStartTime']."!!".$pair_info['resyncSetCxtimestamp']."!!".$pair_info['jobId'];
				$count++;
			}
			array_push($protected_host_id,$pair_info['sourceHostId']);
			array_push($protected_host_id,$pair_info['destinationHostId']);
			array_push($protected_src_vols,$this->conn_obj->sql_escape_string($pair_info['sourceDeviceName']));
			array_push($protected_tgt_vols,$this->conn_obj->sql_escape_string($pair_info['destinationDeviceName']));
			array_push($phy_lun_ids,$pair_info['Phy_Lunid']);
			array_push($plan_ids,$pair_info['planId']);	
		}	
		$protected_host_id = array_unique($protected_host_id);
		$protected_src_vols = array_unique($protected_src_vols);
		$protected_tgt_vols = array_unique($protected_tgt_vols);
		$phy_lun_ids = array_unique($phy_lun_ids);
		$plan_ids = array_unique($plan_ids);
		
		return array($pair_data, $protected_host_id, $protected_src_vols, $protected_tgt_vols, $phy_lun_ids, $plan_ids, $one_to_many_targets);	
	}		
	
	public function getHostData() {						
		$host_sql = "SELECT							
						id,
						name,
						ipaddress,
						osFlag,
						compatibilityNo,
						sentinelEnabled,
						outpostAgentEnabled,
						filereplicationAgentEnabled,
						processServerEnabled,
						csEnabled,
						lastHostUpdateTime,
						UNIX_TIMESTAMP(lastHostUpdateTimeApp) as lastHostUpdateTimeApp,
						prod_version
					FROM
						hosts";
		$host_data_res = $this->conn_obj->sql_exec($host_sql);		
				
		$host_data_list = array();
		$host_data = array();
		foreach($host_data_res as $host_info){
			$host_data_list['id'] = $host_info['id'];
			$host_data_list['name'] = trim($host_info['name']);
			$host_data_list['ipaddress'] = $host_info['ipaddress'];
			$host_data_list['osFlag'] = $host_info['osFlag'];
			$host_data_list['compatibilityNo'] = $host_info['compatibilityNo'];
			$host_data_list['sentinelEnabled'] = $host_info['sentinelEnabled'];
			$host_data_list['outpostAgentEnabled'] = $host_info['outpostAgentEnabled'];
			$host_data_list['filereplicationAgentEnabled'] = $host_info['filereplicationAgentEnabled'];
			$host_data_list['processServerEnabled'] = $host_info['processServerEnabled'];
			$host_data_list['csEnabled'] = $host_info['csEnabled'];
			$host_data_list['lastHostUpdateTime'] = $host_info['lastHostUpdateTime'];
			$host_data_list['lastHostUpdateTimeApp'] = $host_info['lastHostUpdateTimeApp'];
			$host_data_list['prod_version'] = $host_info['prod_version'];
			
			$host_data[$host_info['id']] = $host_data_list;	
		}
		
		return $host_data;
	}		
	
	public function getLVData($protected_host_id, $protected_src_vols, $protected_tgt_vols)	{
		$lv_sql = "SELECT						
						hostId,						
						deviceName,						
						doResync,						
						Phy_Lunid,
						farLineProtected,
						visible,
						tmId,
						maxDiffFilesThreshold,
						deviceProperties 
					FROM						
						logicalVolumes					
					WHERE												
						hostId in ('$protected_host_id')					
					AND						
						deviceName IN ('$protected_src_vols','$protected_tgt_vols')
					AND tmId > 0";		
		$lv_data_res = $this->conn_obj->sql_exec($lv_sql);
			
		$lv_list = array();
		$lv_data = array();
		foreach($lv_data_res as $lv_info){
			$lv_list['doResync'] = $lv_info['doResync'];
			$lv_list['Phy_Lunid'] = $lv_info['Phy_Lunid'];
			$lv_list['farLineProtected'] = $lv_info['farLineProtected'];
			$lv_list['visible'] = $lv_info['visible'];
			$lv_list['tmId'] = $lv_info['tmId'];
			$lv_list['maxDiffFilesThreshold'] = $lv_info['maxDiffFilesThreshold'];
			$lv_list['display_name'] = "";
			if ($lv_info['deviceProperties'])
			{
				$device_properties = $lv_info['deviceProperties'];
				$device_properties = unserialize($device_properties);
				if (is_array($device_properties))
					$lv_list['display_name'] = $device_properties['display_name'];
			}
			$lv_data[$lv_info['hostId']."!!".$lv_info['deviceName']] = $lv_list;	
		}
		
		return $lv_data;
	}		
	
	public function getClustersData($flag = 0)
	{				
		$clus_sql = "SELECT					
						hostId , 					
						deviceName , 					
						clusterId				
					FROM 					
						clusters order by clusterId";		
		$clus_data_res = $this->conn_obj->sql_exec($clus_sql);
		
		$clus_list = array();
		$clus_data = array();
		foreach($clus_data_res as $clus_info)	{		
			if($flag == 1)
			{
				$clus_list[] = $clus_info['hostId']."!&*!".$clus_info['deviceName'];
				$clus_data[$clus_info['clusterId']] = $clus_list;				
			}
			else
			{
				$clus_list['cluster'] = $clus_info['hostId']."!!".$clus_info['deviceName'];
				$clus_data[$clus_info['clusterId']."!!".$clus_info['deviceName']."!!".$clus_info['hostId']] = $clus_list;	
			}			
		}		
		return $clus_data;	
	}		
	
	public function getHostAppLunMap($phy_lun_ids)
	{		
		$at_lun_sql = "SELECT 								
							distinct applianceTargetLunMappingId,								
							processServerId,								
							sharedDeviceId							
						FROM 								
							applianceTargetLunMapping  							
						WHERE 								
							sharedDeviceId IN ('$phy_lun_ids')";		
		$at_lun_res = $this->conn_obj->sql_exec($at_lun_sql);
		
		$at_lun_list = array();
		$at_lun_data = array();
		foreach($at_lun_res as $at_lun_info){
			$at_lun_list['applianceTargetLunMappingId'] = $at_lun_info['applianceTargetLunMappingId'];
			
			$at_lun_data[$at_lun_info['processServerId']."!!".$at_lun_info['sharedDeviceId']] = $at_lun_list;	
		}
		
		return $at_lun_data;
	}
	
	public function unregister_host($ps_id)
	{
		global $UNINSTALL_PENDING;
		$update_hosts_query = "UPDATE
                                 hosts
                              SET
                                 agent_state = $UNINSTALL_PENDING
                              WHERE
                                 id='$ps_id'";
		
		$this->conn_obj->sql_exec($update_hosts_query);
	
		#deleting standby cx information from applianceNodeMap, applianceCluster, standByDetails tables
		#query to check if the ps uninstalled is part of ha cluster
		$cluster_name = '';
		$cluster_name = $this->conn_obj->sql_get_value('applianceNodeMap', 'applianceId', "nodeId = '$ps_id'");
		
		if($cluster_name)
		{    
			$count_cluster_node = $this->conn_obj->sql_get_value('applianceNodeMap', 'count(applianceId)', "nodeId != '$ps_id' AND applianceId = '$cluster_name'");
			if($count_cluster_node > 0)
			{
				$sql_delete_cluster_node = "DELETE FROM applianceNodeMap WHERE nodeId = '$ps_id'";
				$this->conn_obj->sql_exec($sql_delete_cluster_node);
			}
			else
			{
				$sql_delete_appliancenodemap = "DELETE FROM applianceNodeMap WHERE applianceId = '$cluster_name'";
				$this->conn_obj->sql_exec($sql_delete_appliancenodemap);
				
				$sql_delete_appliancecluster = "DELETE FROM applianceCluster WHERE applianceId = '$cluster_name'";
				$this->conn_obj->sql_exec($sql_delete_appliancecluster);
				
				$count_select_standby = $this->conn_obj->sql_get_value('standByDetails', 'count(appliacneClusterIdOrHostId)', "appliacneClusterIdOrHostId = '$cluster_name'");
				if($count_select_standby)
				{
					$sql_delete_standby = "DELETE FROM standByDetails";
					$this->conn_obj->sql_exec($sql_delete_standby);
				}
			}
		}
		else
		{
			$count_select_standby = $this->conn_obj->sql_get_value('standByDetails', 'count(appliacneClusterIdOrHostId)', "appliacneClusterIdOrHostId = '$ps_id'");
			
			if($count_select_standby)
			{
				$sql = "DELETE FROM standByDetails";
				$this->conn_obj->sql_exec($sql);
			}
		}
		
		$process_server_details = $this->commonfunctions_obj->get_host_info($ps_id);
		
		$unregister_time = date("Y-m-d H:i:s"); 	
			
		$psname = $process_server_details['name'];
		$psipaddress = $process_server_details['ipaddress'];
		$psostype = $process_server_details['osType'];
		$psenabled = $process_server_details['processServerEnabled'];
		$psregistertime = $process_server_details['creationTime'];
		$psid = $process_server_details['id'];
		
		$msg = "Process Server Unregistered to CX with details (hostName::$psname, ipaddress:$psipaddress, operatingSystem::$psostype,hostid::$psid,processServerEnabled::$psenabled,PSUnRegisteredTime::$unregister_time)";
		
		$this->commonfunctions_obj->writeLog("System",$msg,"");
	}
	
	public function registerHost($host_guid,$host_info)
	{
		global $AGENTROLE;
		if(!$host_guid)
		{
			return;
		}		
		
		/*
		1. Deleting process server passphrase update health, once the control path communication establishes between process server and configuration server.
		2. Though delete query fails for any reason of database error or database server down, the next registration call on retry ensures to get success.
		*/
		$pass_phrase_update_health_sql = 	"DELETE 
											FROM 
												infrastructurehealthfactors 
											WHERE 
												`hostId` ='$host_guid' AND 
												`healthFactorCode` = 'EPH00021'";
		$this->conn_obj->sql_query($pass_phrase_update_health_sql);	
		
		$host_exists = $this->conn_obj->sql_get_value("hosts","id","id = '$host_guid'");
		$agent_role = $this->conn_obj->sql_get_value("hosts","agentRole","id = '$host_guid'");

		if (!$host_exists)
		{
			$register_ps_query = "INSERT INTO
									hosts (
										accountId,
										psInstallTime,
										csEnabled,
										hypervisorName,
										psPatchVersion,
										processServerEnabled,
										patchInstallTime,
										osType,
										id,
										hypervisorState,
										name,
										psVersion,
										ipaddress,
										accountType,
										osFlag,
										lastHostUpdateTimePs)
									VALUES (
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										?,
										now())";

			$args = array($host_info['accountId'], $host_info['psInstallTime'], $host_info['csEnabled'],
							$host_info['hypervisorName'], $host_info['psPatchVersion'], $host_info['processServerEnabled'],
							$host_info['patchInstallTime'], $host_info['osType'], $host_info['id'], $host_info['hypervisorState'],
							$host_info['name'], $host_info['psVersion'], $host_info['ipaddress'], $host_info['accountType'],
							$host_info['osFlag']);
		}
		else
		{
			$register_ps_query = "UPDATE
									hosts
								  SET
									accountId = ?,
									psInstallTime = ?,
									csEnabled = ?,
									psPatchVersion = ?,
									processServerEnabled = ?,
									patchInstallTime = ?,
									id = ?,
									name = ?,
									psVersion = ?,
									ipaddress = ?,
									accountType = ?,
									osFlag = ?,
									lastHostUpdateTimePs = now()
								  WHERE
									id = ?";

			$args = array($host_info['accountId'], $host_info['psInstallTime'], $host_info['csEnabled'],
							$host_info['psPatchVersion'], $host_info['processServerEnabled'],
							$host_info['patchInstallTime'], $host_info['id'], $host_info['name'],
							$host_info['psVersion'], $host_info['ipaddress'], $host_info['accountType'],
							$host_info['osFlag'], $host_guid);
		}
		$this->conn_obj->sql_query($register_ps_query, $args);

		if($host_info['processServerEnabled'] == '1')
		{
			$ps_info['processServerId'] = $host_guid;
			$ps_info['ipaddress'] = $host_info['ipaddress'];
            $ps_info['port'] = $host_info['port'];
            $ps_info['sslPort'] = $host_info['sslPort'];
			$this->registerPs($host_guid,$ps_info);
		}
	}
	
	public function registerPs($host_guid,$ps_info)
	{
		$ps_registered = $this->conn_obj->sql_get_value("processServer","processServerId","processServerId = '$host_guid'");
		
		if (!$ps_registered)
		{
			$register_ps_query = "INSERT INTO
									processServer (
										processServerId,
										ipaddress,
										port,
										sslPort)
									VALUES (
										?,
										?,
										?,
										?)";
			$args = array($host_guid, $ps_info['ipaddress'], $ps_info['port'], $ps_info['sslPort']);
		}
		else
		{
			$register_ps_query = "UPDATE
									processServer
								  SET
								    processServerId = ?,
									ipaddress = ?,
									port = ?,
									sslPort = ?
								  WHERE
									processServerId = ?";
			$args = array($host_guid, $ps_info['ipaddress'], $ps_info['port'], $ps_info['sslPort'], $host_guid);
		}
		$this->conn_obj->sql_query($register_ps_query, $args);

		if(!$ps_registered)
		{
			$process_server_details = $this->commonfunctions_obj->get_host_info($ps_info['processServerId']);
			
			$psname = $process_server_details['name'];
			$psipaddress = $process_server_details['ipaddress'];
			$psostype = $process_server_details['osType'];
			$psenabled = $process_server_details['processServerEnabled'];
			$psregistertime = $process_server_details['creationTime'];
			$psid = $process_server_details['id'];
			
			$msg = "Process Server Registered to CX with details (hostName::$psname, ipaddress:$psipaddress, operatingSystem::$psostype, hostid::$psid,processServerEnabled::$psenabled,PSRegisteredTime::$psregistertime)";
			$this->commonfunctions_obj->writeLog("System" ,$msg,"");
		}
		
	}
		
	public function registerNetworkDevices($network_info)
	{
		foreach($network_info as $nic_name => $network_details)			
		{
			$host_id = $network_details['hostId'];
			$nic_reported = $this->conn_obj->sql_get_value("networkAddress","ipAddress","hostId = ? AND deviceName = ?", array($host_id, $nic_name));

			foreach($network_details['ipAddress'] as $index_key => $nic_ip_address)
			{
				$dns = $network_details['dns'];
				$nicSpeed = $network_details['nicSpeed'];
				$gateway = $network_details['gateway'];
				$deviceType = $network_details['deviceType'];
				$deviceName = $network_details['deviceName'];
				if ($nic_reported)
				{
					$nic_sql = "UPDATE
									networkAddress
								SET
									dns = ?,
									nicSpeed = ?,
									gateway = ?,
									ipAddress = ?,
									deviceType = ?,
									deviceName = ?,
									hostId = ?,
									netmask = ?
								WHERE
									hostId = ? AND
									deviceName = ?";
					$args = array($dns, $nicSpeed, $gateway, $nic_ip_address, $deviceType, $deviceName,
									$host_id, $network_details['netmask'][$nic_ip_address], $host_id, $deviceName);
				}
				else
				{
					if ($nic_ip_address)
					{
						$nic_sql = "INSERT INTO
										networkAddress(
													dns,
													nicSpeed,
													gateway,
													ipAddress,
													deviceType,
													deviceName,
													hostId,
													netmask)
										VALUES (
												?,
												?,
												?,
												?,
												?,
												?,
												?,
												?)";
						$args = array($dns, $nicSpeed, $gateway, $nic_ip_address, $deviceType,
										$deviceName, $host_id, $network_details['netmask'][$nic_ip_address]);
					}
					else
					{
						$nic_sql = "DELETE FROM
										networkAddress
									WHERE
										hostId = ? AND
										deviceName = ?";
						$args = array($host_id, $deviceName);
					}
				}
				$this->conn_obj->sql_query($nic_sql, $args);
			}			
			$nic_name_list[] = $nic_name;
		}
		
		$nic_count = count($nic_name_list);
		if ($nic_count > 0) 
		{			
			$nic_names = implode(",", $nic_name_list);
			
			$stale_nic_arr = array();
			$stale_nic_sql = "SELECT deviceName FROM networkAddress WHERE hostId = ? AND NOT FIND_IN_SET(deviceName, ?)";
			$stale_nic_rs = $this->conn_obj->sql_query($stale_nic_sql, array($host_id, $nic_names));

			if($this->conn_obj->sql_num_row($stale_nic_rs) > 0)
			{
				while($stale_nic_row = $this->conn_obj->sql_fetch_object($stale_nic_rs))
				{
					$stale_nic_arr[] = $stale_nic_row->deviceName;
				}
			}
			
			if(count($stale_nic_arr) > 0)
			{
				$stale_nic_name_list = implode(",", $stale_nic_arr);
				
				#
				# Delete from the networkAddressMap
				#
				$delete_nw_map = "DELETE FROM networkAddressMap WHERE processServerId = ? AND FIND_IN_SET(nicDeviceName, ?)";
				$this->conn_obj->sql_query($delete_nw_map, array($host_id, $stale_nic_name_list));
				
				#
				# SET deletion flag for BPMPolicies
				#
				$policies_sql = "UPDATE BPMPolicies SET isBPMEnabled = 2 WHERE sourceHostId = ? AND FIND_IN_SET(networkDeviceName, ?)";
				$this->conn_obj->sql_query($policies_sql, array($host_id, $stale_nic_name_list));
			}
							
			$delete_nic_sql = "DELETE FROM networkAddress WHERE hostId = ? AND NOT FIND_IN_SET(deviceName, ?)";
			$this->conn_obj->sql_query($delete_nic_sql, array($host_id, $nic_names));
		}
	}
	
	public function getPolicyData($host_id)
	{
		$policy_sql = "SELECT 
							p.hostId,
							p.policyId
						FROM
							policy p,	
							policyInstance pi
						WHERE
							p.policyId = pi.policyId
						AND
							p.policyType IN ('51')
						AND
							pi.policyState = '3'
						AND 
							p.hostId = '$host_id'";
		$policy_data_res = $this->conn_obj->sql_exec($policy_sql);
		
		$policy_list = array();
		$policy_data = array();
		foreach($policy_data_res as $policy_data1) {
			$policy_list['hostId'] = $policy_data1['hostId'];
			$policy_list['policyId'] = $policy_data1['policyId'];
			
			$policy_data[$policy_data1['policyId']] = $policy_list;	
		}
		return $policy_data;
	}
};
?>