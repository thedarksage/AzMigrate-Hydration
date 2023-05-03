<?php
/*
 *================================================================= 
 * FILENAME
 *    ProcessServer.php
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
Class ProcessServer
{
    
    private $commonfunctions_obj;
    private $system_obj;
    private $trending_obj;
    private $volume_obj;
    private $bandwidth_obj;
    private $conn;
    
    /*    
    * Function Name: ProcessServer
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
    *     Ret Value: 
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    function __construct()
    {
		global $conn_obj, $RRDTOOL_PATH;
		
		$this->commonfunctions_obj = new CommonFunctions();
		$this->system_obj = new System();
		$this->trending_obj = new Trending();
		$this->volume_obj = new VolumeProtection();
		$this->bandwidth_obj = new BandwidthPolicy();
        $this->conn = $conn_obj;
		
		if(!$this->commonfunctions_obj->is_linux())
        {
			$this->rrd_install_path = $RRDTOOL_PATH;
			$this->rrd_install_path = addslashes($this->rrd_install_path);
			$this->rrdcommand = $this->rrd_install_path."\\rrdtool\\Release\\rrdtool.exe";
        }
		else
        {
            $this->rrdcommand = "rrdtool";
        }
    }

   /*		
    * Function Name: getPSHosts
    *
    * Description:
    *   Returns the array of ps host id, host name,
    *   and OS flag, OS type from table hosts.				
    *
    * Parameters:
    *     Param 1 [IN/OUT]:
    *     Param 2 [IN/OUT]:
    *
    * Return Value:
    *     Ret Value: Array 
    *
    * Exceptions:
    *     <Specify the type of exception caught>
   */
    public function getPSHosts($host_id)
    {
	    $sql_ps_details = "SELECT 
	    			  name,
				  id,
				  ipaddress,
				  osFlag,
				  osType
			       FROM
			       	  hosts
			       WHERE 
			          processServerEnabled=1
			       AND 
			          id='$host_id'";
	    
		$rs = $this->conn->sql_query($sql_ps_details);
	    $row = $this->conn->sql_fetch_assoc ($rs);
        return $row;
    }
 
    
   /*
    * Function Name: getProcessServerId
    *
    * Description:
    *   Returns the process server id given a source
    *   and destination device name and id combination
    *
    * Parameters:
    *     Param 1 [IN]: $srcid
    *     Param 2 [IN]: $srcdev
    *     Param 3 [IN]: $destid
    *     Param 4 [IN]: $destdev
    *
    * Return Value:
    *     Ret Value: Array
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function getProcessServerId ($srcid, $srcdev, $destid, $destdev)
    {
		$srcdev = $this->conn->sql_escape_string($srcdev);
		$destdev = $this->conn->sql_escape_string($destdev);
	   
		$get_ps_id_query = "SELECT 
		        processServerId 
		   FROM 
                        srcLogicalVolumeDestLogicalVolume 
	           WHERE 
			sourceHostId='$srcid' AND 
			 sourceDeviceName='$srcdev' AND
	        	destinationHostId ='$destid' AND
			 destinationDeviceName='$destdev'";
      
        $rs = $this->conn->sql_query($get_ps_id_query);
        $row = $this->conn->sql_fetch_object ($rs);
		$ps_id = $row->processServerId;
		return $ps_id;
    }
	

   /*
    * Function Name: getProcessServerPairs
    *
    * Description:
    *     Prepares the details of replication pairs 
    *     associated with the process server
    *
    * Parameters:
    *     Param 1 [IN]: process server id
    *
    * Return Value:
    *     array with the associated replication 
    *
    * Exceptions:
    *     N/A
    */
	public function getProcessServerPairs($id)
	{
		global $logging_obj;
		$result_set = array();
		
		#14404 Fix
		#non-profiler pairs
		$sql = "SELECT
					a.sourceDeviceName, 
					a.destinationDeviceName,
					b.name as srcName,
					b.compatibilityNo as srcCompatibilityNo,
					c.name as tgtName,
					c.compatibilityNo as tgtCompatibilityNo,
					a.sourceHostId,
					a.destinationHostId,
					a.profilingId
				from 
					srcLogicalVolumeDestLogicalVolume a,
					hosts b,
					hosts c		
				WHERE
				    a.processServerId='$id' AND
					a.sourceHostId = b.id AND
					a.destinationHostId = c.id AND 
					a.destinationHostId !='5C1DAEF0-9386-44a5-B92A-31FE2C79236A'
				GROUP BY 
					a.destinationHostId, a.destinationDeviceName";							
		
		#non-cluster profiler pairs
		$host_ids = array();
		$sql1 = "SELECT distinct(hostId) FROM clusters";
		$sth1 = $this->conn->sql_query($sql1);
		while($row =  $this->conn->sql_fetch_row($sth1))
		{
		   $host_ids[] = $row[0];
		}
		$distinct_cluster_host_ids = implode("','",$host_ids);
		
		$sql_profiler = "SELECT
					a.sourceDeviceName, 
					a.destinationDeviceName,
					b.name as srcName,
					b.compatibilityNo as srcCompatibilityNo,
					c.name as tgtName,
					c.compatibilityNo as tgtCompatibilityNo,
					a.sourceHostId,
					a.destinationHostId,
					a.profilingId
				from 
					srcLogicalVolumeDestLogicalVolume a,
					hosts b,
					hosts c					
				WHERE
				    a.processServerId='$id' AND
					a.sourceHostId = b.id AND
					a.destinationHostId = c.id AND
					a.sourceHostId NOT IN ('$distinct_cluster_host_ids') AND
					a.destinationHostId ='5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";
		
		#cluster profiler pairs
		$cluster_ids = $this->commonfunctions_obj->getAllClusters();
		$eliminated_cluster_ids = $this->commonfunctions_obj->getEliminatedClusterIds($cluster_ids);
		
		$host_names = array();
		$distinct_cluster_host_ids = implode("','",$host_ids);
		$sql2 = "SELECT distinct(deviceName) FROM clusters";
		$sth2 = $this->conn->sql_query($sql2);
		while($row =  $this->conn->sql_fetch_row($sth2))
		{
		   $host_names[] = $row[0];
		}
        $distinct_device_names = implode("','",$host_names);
		
		$sql_profiler_cluster = "SELECT
					a.sourceDeviceName, 
					a.destinationDeviceName,
					b.name as srcName,
					b.compatibilityNo as srcCompatibilityNo,
					c.name as tgtName,
					c.compatibilityNo as tgtCompatibilityNo,
					a.sourceHostId,
					a.destinationHostId,
					a.profilingId
				from 
					srcLogicalVolumeDestLogicalVolume a,
					hosts b,
					hosts c					
				WHERE
				    a.processServerId='$id' AND
					a.sourceHostId = b.id AND
					a.destinationHostId = c.id AND
					a.sourceHostId NOT IN ('$eliminated_cluster_ids') AND
					a.sourceDeviceName IN ('$distinct_device_names') AND
					a.destinationHostId ='5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";
					
		#cluster non-shared profiler pairs
		$sql_profiler_non_shared = "SELECT
					a.sourceDeviceName, 
					a.destinationDeviceName,
					b.name as srcName,
					b.compatibilityNo as srcCompatibilityNo,
					c.name as tgtName,
					c.compatibilityNo as tgtCompatibilityNo,
					a.sourceHostId,
					a.destinationHostId,
					a.profilingId
				from 
					srcLogicalVolumeDestLogicalVolume a,
					hosts b,
					hosts c					
				WHERE
				    a.processServerId='$id' AND
					a.sourceHostId = b.id AND
					a.destinationHostId = c.id AND
					a.sourceHostId IN ('$distinct_cluster_host_ids') AND
					a.sourceDeviceName NOT IN ('$distinct_device_names') AND
					a.destinationHostId ='5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";
						
		$sql = $sql." UNION ".$sql_profiler." UNION ".$sql_profiler_cluster." UNION ".$sql_profiler_non_shared;
		$rs = $this->conn->sql_query($sql);
		while($row = $this->conn->sql_fetch_array($rs))
		{
			$result_set[] = $row;
		}
		
		return $result_set;
                                                                
}
   /*
    * Function Name: getAgentsOfPS
    *
    * Description:
    *     Prepares to get agents details
    *     associated with the process server
    *
    * Parameters:
    *     Param 1 [IN]: process server id
    *
    * Return Value:
    *     array with the associated agents
    *
    * Exceptions:
    *     N/A
    */
 	public function getAgentsOfPS($id)
	{
		$server_count = $this->conn->sql_get_value("srcLogicalVolumeDestLogicalVolume", "count(DISTINCT sourceHostId)", "processServerId = '".$id."'");
		return $server_count;
	}

	public function validatePS($ps_host_id)
	{
		global $UNINSTALL_PENDING;
		
		$sqlProcesServer = "SELECT 								 
								h.id,
								h.name,
								h.ipaddress,
								UNIX_TIMESTAMP(now()) `currentTime`,
								UNIX_TIMESTAMP(h.lastHostUpdateTimePs) `lastHostUpdateTimePs`
							FROM 
								hosts h,
								processServer p
							WHERE 
								p.processServerId = h.id AND
								h.id = ? AND
								h.agent_state != ?";
		$ps_details = $this->conn->sql_query($sqlProcesServer, array($ps_host_id, $UNINSTALL_PENDING));
		return isset($ps_details[0]) ? $ps_details[0] : array();
	}
	
	
	public function getPerfAndServiceStatus()
	{
		global $REPLICATION_ROOT, $SLASH;
	
		$cs_guid = $this->amethyst_details['HOST_GUID'];
		
		$cs_ps_guids = array();
		$ps_guids = array();
		$ps_data = array();
		$hosts_data = array();
		$ps_guids_sql = "SELECT processServerId, ipaddress, cummulativeThrottle FROM processServer";		
		$ps_result = $this->conn->sql_query($ps_guids_sql);
		while($data = $this->conn->sql_fetch_object($ps_result))
		{
			$ps_guids[] = $data->processServerId;
			$ps_data[$data->processServerId]["ipaddress"] =  $data->ipaddress;
			$ps_data[$data->processServerId]["cumulativeThrottle"] =  $data->cummulativeThrottle;
		}		
		foreach( $ps_guids as $line ) 
		{ 
			$cs_ps_guids[] = rtrim( $line ); 
		}
		
		$hosts_sql = "SELECT id, name, ipaddress, lastHostUpdateTime as vxheartbeat, UNIX_TIMESTAMP(now()) - lastHostUpdateTime as vxheartbeathealth,lastHostUpdateTimeApp as appheartbeat, UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(lastHostUpdateTimeApp) as appheartbeathealth, UNIX_TIMESTAMP(lastHostUpdateTimeApp) as appheartbeatutc, UNIX_TIMESTAMP(lastHostUpdateTimePs) as psheartbeat, UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(lastHostUpdateTimePs) as psheartbeathealth, processServerEnabled, agentRole,UNIX_TIMESTAMP(now()) as currentTime, UNIX_TIMESTAMP(creationTime) as creationTime FROM hosts where processServerEnabled = ? or agentRole = ?";
		$hosts_result = $this->conn->sql_query($hosts_sql, array(1,"MasterTarget"));
		foreach ($hosts_result as $data)
		{			
			$hosts_data[$data["id"]] = array("name"=>$data["name"],
			"ipaddress"=>$data["ipaddress"],
			"vxheartbeat"=>$data["vxheartbeat"],
			"vxheartbeathealth"=>$data["vxheartbeathealth"],
			"appheartbeat"=>$data["appheartbeat"],
			"appheartbeathealth"=>$data["appheartbeathealth"],
			"appheartbeatutc"=>$data["appheartbeatutc"],
			"psheartbeat"=>$data["psheartbeat"],
			"psheartbeathealth"=>$data["psheartbeathealth"],
			"processServerEnabled"=>$data["processServerEnabled"],
			"agentRole"=>$data["agentRole"],
			"currentTime"=>$data["currentTime"],
			"creationTime"=>$data["creationTime"]
			);			
		}
				
		if(!in_array($cs_guid, $cs_ps_guids)) array_push($cs_ps_guids, $cs_guid);
		
		$data = array();
		foreach ($cs_ps_guids as $guid)
		{
			$is_cs = $this->conn->sql_get_value("hosts", "csEnabled", "id = ?", array($guid));
			$is_ps = $this->conn->sql_get_value("hosts", "processServerEnabled", "id = ?", array($guid));
			
			$cs_details = array();
			$ps_details = array();
			if($is_cs)
			{
				$cs_ps_id_str = implode(',', $cs_ps_guids);
				
				$cs_details['HostId'] = $guid;
				$cs_details['ProtectedServers'] = $this->conn->sql_get_value("srcLogicalVolumeDestLogicalVolume", "count(distinct sourceHostId)", "destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' AND FIND_IN_SET(processServerId, ?)", array($cs_ps_id_str));
				$cs_details['ReplicationPairCount'] =  $this->conn->sql_get_value("srcLogicalVolumeDestLogicalVolume", "count(*)", "destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' AND FIND_IN_SET(processServerId, ?)", array($cs_ps_id_str));
				$cs_details['ProcessServerCount'] = $this->conn->sql_get_value("hosts", "count(*)", "processServerEnabled = '1'");
				$cs_details['AgentCount'] = $this->conn->sql_get_value("hosts", "count(*)", "(outpostAgentEnabled = '1' OR sentinelEnabled = '1' OR filereplicationAgentEnabled = 1) AND id != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' AND agentRole != 'MasterTarget'");
				$cs_details['SystemPerformance'] = $this->getPerfStatistics($guid, 0);
				$cs_details['CSServiceStatus'] = $this->getServiceHealth($guid, 0);
				
				$data["ConfigServer"] = $cs_details;
			}
			
			//Process PS servers data.
			if($is_ps)
			{
				$PSPerformanceStateTimeStamp = 0;
				$rrd_filepath = $REPLICATION_ROOT.$SLASH."SystemMonitorRrds".$SLASH;
				$param_xml_file = $rrd_filepath.$guid."-perf.xml";
				$param_file = $rrd_filepath.$guid."-perf.txt";
				
				if(file_exists($param_xml_file))
				{
					$PSPerformanceStateTimeStamp = filemtime("$param_xml_file");
				}
				elseif(file_exists($param_file))
				{
					$PSPerformanceStateTimeStamp = filemtime("$param_file");
				}
				
				$pair = $this->getProcessServerPairs($guid);
				
				$data_change = 0;
				$avg_uncom_value = 0;
				
				for($i=0;isset($pair[$i]);$i++)
				{
					#These calculations similar to average data change in analyzer details
					$profiling_id = $pair[$i]['profilingId'];
					$cumulative_data = $this->trending_obj->getProfilingSummary($profiling_id);
					$sum_cumulative_uncomp = $cumulative_data['sumCumulativeUnComp'];
					$days = $cumulative_data['recordCount'];
					if($days)
					{
						$avg_uncom_value =  intval(($sum_cumulative_uncomp*1024*1024)/($days*24*60*60));
						$data_change += $avg_uncom_value;
					}
				}	
				
				$c = 0;
				while ($data_change >= 1024) {
						$c++;
						$data_change = $data_change / 1024;
				}
				$data_change = number_format($data_change,($c ? 2 : 0),'.','');	
				
				$ps_details['HostId'] = $guid;
				$ps_details['PSServerIP'] = $ps_data[$guid]["ipaddress"];				
				$cumulative_throttle = $ps_data[$guid]["cumulativeThrottle"];
				$ps_details['Heartbeat'] = $hosts_data[$guid]['psheartbeat'];
				$ps_details['ServerCount'] = $this->getAgentsOfPS($guid);
				$ps_details['ReplicationPairCount'] = count($pair);	
				$ps_details['DataChangeRate'] = $data_change;
				$ps_details['PSPerformanceStateTimeStamp'] = $PSPerformanceStateTimeStamp;
				$ps_details['SystemPerformance'] = $this->getPerfStatistics($guid, 1);
				$ps_details['PSServiceStatus'] = $this->getServiceHealth($guid, 1);
				$ps_heartbeat_status = $this->conn->sql_get_value("hosts", "UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(lastHostUpdateTimePs)", "id = ?", array($guid));
				$ps_hf_data = $this->getPsHealthFactors($guid, $hosts_data, $ps_data);
				if (count($ps_hf_data) > 0)
				{
					$ps_details['HealthFactors'] = $ps_hf_data;
				}
				$data["ProcessServers"][$guid] = $ps_details;
			}
		}
		
		//Process MT servers data.
		foreach ($hosts_data as $host_id=>$val)
		{
			$mt_details = array();
			if ($val["agentRole"] == "MasterTarget")
			{				
				$mt_details['HostId'] = $host_id;
				$mt_details['MTServerIP'] = $val["ipaddress"];								
				$mt_details['Heartbeat'] = $val["vxheartbeat"];
				$mt_hf_data = $this->getMtHealthFactors($host_id, $hosts_data);
				if (count($mt_hf_data) > 0)
				{
					$mt_details['HealthFactors'] = $mt_hf_data;
				}
				$data["MasterTargets"][$host_id] = $mt_details;		
			}			
		}
		
		ksort($data);
		return $data;
	}    
	
	public function getPsHealthFactors($guid, $hosts_data, $ps_data)
	{
		global $PS_HEART_BEAT_HF,$PS_CUMULATIVE_THROTTLE_HF;
		$ps_health = $placeHoldersArr = array();
		$healthEvents = $this->listHealthEvents($guid,"Ps");
		
		//Health factor for process server heartbeat.
		if ($hosts_data[$guid]["currentTime"] - $hosts_data[$guid]["psheartbeat"] > 900)
		{
			$ps_heart_beat = $hosts_data[$guid]["psheartbeat"]?$hosts_data[$guid]["psheartbeat"]:$hosts_data[$guid]["creationTime"];
			$ps_health[]=array("Code"=>$PS_HEART_BEAT_HF,
			"Severity"=>"Critical",
			"Source"=>"Ps",
			"DetectionTime"=>$ps_heart_beat,
			"PlaceHolders"=>array("name"=>$hosts_data[$guid]["name"],"ipaddress"=>$hosts_data[$guid]["ipaddress"])
			);
		}
		
		//Health factor for process server cumulative throttle.
		if ($ps_data[$guid]["cumulativeThrottle"] == 1)
		{
			$ps_health[]=array("Code"=>$PS_CUMULATIVE_THROTTLE_HF,
			"Severity"=>"Warning",
			"Source"=>"Ps",
			"DetectionTime"=>"-1",
			"PlaceHolders"=>array("name"=>$hosts_data[$guid]["name"],"ipaddress"=>$hosts_data[$guid]["ipaddress"])
			);
		}
		
		if(count($healthEvents) > 0)
		{
			foreach($healthEvents as $hf)
			{
				if (!empty($healthEvents[$hf['healthFactorCode']]['placeHolders']))
				{
					$placeHoldersArr = unserialize(htmlspecialchars_decode($healthEvents[$hf['healthFactorCode']]['placeHolders']));
				}
				else
				{
					$placeHoldersArr = array();
				}
				$ps_health[]=array("Code"=>$hf['healthFactorCode'],
				"Severity"=>$hf['severity'],
				"Source"=>$hf['component'],
				"DetectionTime"=>$hf['healthCreationTime'],
				"PlaceHolders"=>$placeHoldersArr
				);
			}
		}
		return $ps_health;
	}
	
	public function getMtHealthFactors($guid, $hosts_data)
	{		
		global $MT_VX_SERVICE_HEART_BEAT_HF,$MT_APP_SERVICE_HEART_BEAT_HF,$MT_RETENTION_VOLUMES_LOW_SPACE_HF;
		$mt_health = array();
		$healthEvents = $this->listHealthEvents($guid,"Mt");

		//Health factor for MT disk agent vx service herartbeat.
		if ($hosts_data[$guid]["vxheartbeathealth"] > 900)
		{
			$mt_health[]=array("Code"=>$MT_VX_SERVICE_HEART_BEAT_HF,
			"Severity"=>"Critical",
			"Source"=>"Mt",
			"DetectionTime"=>$hosts_data[$guid]["vxheartbeat"],
			"PlaceHolders"=>array("name"=>$hosts_data[$guid]["name"],"ipaddress"=>$hosts_data[$guid]["ipaddress"])
			);
		}
		
		//Health factor for MT application service herartbeat.
		if ($hosts_data[$guid]["currentTime"] - $hosts_data[$guid]["appheartbeatutc"] > 900)
		{
			$app_heart_beat = $hosts_data[$guid]["appheartbeatutc"]?$hosts_data[$guid]["appheartbeatutc"]:$hosts_data[$guid]["creationTime"];
			$mt_health[]=array("Code"=>$MT_APP_SERVICE_HEART_BEAT_HF,
			"Severity"=>"Critical",
			"Source"=>"Mt",
			"DetectionTime"=>$app_heart_beat,
			"PlaceHolders"=>array("name"=>$hosts_data[$guid]["name"],"ipaddress"=>$hosts_data[$guid]["ipaddress"])
			);
		}
		
		//Health factor for low space on retention volumes
		$ret_sql = "select hostId, 
			mountPoint, 
			capacity, 
			freespace 
		from 
			logicalVolumes 
		where 
		(	
			ROUND(freeSpace/?,?)<=? 
			OR 
			((freespace/capacity)*?<=? AND ROUND(freeSpace/?,?)<?)
		) 
		and 
		deviceFlagInUse=?
		and
		hostId = ?";
		$ret_vol_result = $this->conn->sql_query($ret_sql, array(1073741824,2,1,100,5,1073741824,2,4,3,$guid));		
		$list_of_ret_vols = array();
		foreach ($ret_vol_result as $data)
		{						
			$list_of_ret_vols[] = $data["mountPoint"];
		}
		if (count($list_of_ret_vols) > 0)
		{
			$list_of_ret_vols = implode(",",$list_of_ret_vols);
			$mt_health[]=array("Code"=>$MT_RETENTION_VOLUMES_LOW_SPACE_HF,
			"Severity"=>"Warning",
			"Source"=>"Mt",
			"DetectionTime"=>"-1",
			"PlaceHolders"=>array("retentionVols"=>$list_of_ret_vols,"name"=>$hosts_data[$guid]["name"],"ipaddress"=>$hosts_data[$guid]["ipaddress"])
			);
		}

		if(count($healthEvents) > 0)
		{
			foreach($healthEvents as $hf)
			{
				$placeHoldersArr = unserialize(htmlspecialchars_decode($healthEvents[$hf['healthFactorCode']]['placeHolders']));
				
				$mt_health[]=array("Code"=>$hf['healthFactorCode'],
				"Severity"=>$hf['severity'],
				"Source"=>$hf['component'],
				"DetectionTime"=>$hf['healthCreationTime'],
				"PlaceHolders"=>$placeHoldersArr
				);
			}
		}	
		return $mt_health;
	}

	public function getServiceHealth($host, $is_ps)
	{
		global $REPLICATION_ROOT, $MAX_TMID, $SLASH;
		
		$rrd_filepath = $REPLICATION_ROOT.$SLASH."SystemMonitorRrds".$SLASH;
		
		$service_file = $rrd_filepath.$host."-service.txt";
		$service_xml_file = $rrd_filepath.$host."-service.xml";
		
		if(file_exists($service_xml_file)) 
		{
            $service_health_flag = $this->getPSXMLServiceData($service_xml_file,$host,$is_ps);
        }
		else if(file_exists($service_file)) 
		{
           $service_health_flag = $this->getPSTXTSeviceData($service_file,$host,$is_ps);
        }
		else
		{
			$service_health_flag = "Running";
		}
		
		return $service_health_flag;
	}
	
	public function getPSXMLServiceData($service_file, $host, $is_ps)
	{
		global $MAX_TMID;
		$serviceStatus = array();
		
		$xml = simplexml_load_file ( $service_file );
		if ($xml === false)
		{
			return;
		}
		
		foreach($xml->children() as $xmlData)
		{
			if($xmlData['Name'] == 'PSGuid')
			{
				$PSGuid = $xmlData['Value'];
			}
			if($xmlData['Name'] == 'PSVersion')
			{
				$PSVersion = $xmlData['Value'];
			}
			if($xmlData['Id'] == 'SystemServices')
			{
				foreach($xmlData->children() as $xmlDataChildren)
				{
					$key = $xmlDataChildren['Name'];
					$serviceStatus["$key"] = $xmlDataChildren['Value'];
				}
			}
		}
		
		$tman_services = $this->commonfunctions_obj->tman_services_values();
		array_push($tman_services, "cxps", "W3SVC", "MySQL", "ProcessServer");
		
		$service = array();
		foreach ($tman_services as $tman_service)
		{
			if($tman_service == "monitor") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
			elseif($tman_service == "monitor_disks") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
			elseif($tman_service == "cxps") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
			
			if($is_ps)
			{
				if($tman_service == "monitor_ps") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
				elseif($tman_service == "monitor_protection") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
				elseif($tman_service == "esx") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
				elseif($tman_service == "volsync") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
				elseif($tman_service == "pushinstalld") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
				elseif($tman_service == "ProcessServer") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
			}
			else
			{
				if($tman_service == "MySQL") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
				elseif($tman_service == "W3SVC") $service[$tman_service] = isset($serviceStatus["$tman_service"]) ? $serviceStatus["$tman_service"] : "0";
			}
		}
		
		$overall_flag = 1;
		foreach ($service as $service_key => $service_val)
		{
			$health_flag = 1;
			
			if($service_key == "volsync")
			{
				if($service_val == "") $health_flag = 4;
				elseif($service_val < 1 || $service_val < $MAX_TMID) $health_flag = 3;
			}
			else
			{
				if($service_val == "") $health_flag = 4;
				elseif ($service_val != 1) $health_flag = 4;
			}
			$overall_flag = ($overall_flag > $health_flag) ? $overall_flag : $health_flag;
		}
		
		if($overall_flag == 1) $service_health = "Running";
		elseif($overall_flag == 3) $service_health = "Stopped";
		elseif($overall_flag == 4) $service_health = "Warning";
		
		return $service_health;
	}
	
	public function getPSTXTSeviceData($service_file, $host, $is_ps)
	{
		global $MAX_TMID;
		$file_handle = $this->commonfunctions_obj->file_open_handle($service_file);
        if(!$file_handle)
        {
		  return;
        }
        $service_str = fread($file_handle, filesize($service_file));
		
        fclose($file_handle);
        
		$service_arr = explode(":",$service_str);
		
		$tman_services = $this->commonfunctions_obj->tman_services_values();
		array_push($tman_services, "cxps", "httpd", "mysqld", "ProcessServer", "ProcessServerMonitor");
		
		$service = array();
		foreach ($tman_services as $tman_service)
		{
			if($tman_service == "monitor") $service[$tman_service] = $service_arr[1];
			elseif($tman_service == "monitor_disks") $service[$tman_service] = $service_arr[3];
			elseif($tman_service == "cxps") $service[$tman_service] = $service_arr[12];
			
			if($is_ps)
			{
				if($tman_service == "monitor_ps") $service[$tman_service] = $service_arr[2];
				elseif($tman_service == "volsync") $service[$tman_service] = $service_arr[4];
				elseif($tman_service == "pushinstalld") $service[$tman_service] = $service_arr[7];
				elseif($tman_service == "bpm") $service[$tman_service] = $service_arr[11];
				elseif($tman_service == "ProcessServer") $service[$tman_service] = $service_arr[14];
				elseif($tman_service == "ProcessServerMonitor") $service[$tman_service] = $service_arr[15];
			}
			else
			{
				if($tman_service == "scheduler") $service[$tman_service] = $service_arr[5];
				elseif($tman_service == "httpd") $service[$tman_service] = $service_arr[6];
				elseif($tman_service == "gentrends") $service[$tman_service] = $service_arr[9];
				elseif($tman_service == "mysqld") $service[$tman_service] = $service_arr[10];
			}
		}
		
		$overall_flag = 1;
		foreach ($service as $service_key => $service_val)
		{
			$health_flag = 1;
			
			if($service_key == "volsync")
			{
				if($service_val == "") $health_flag = 4;
				elseif($service_val < 1 || $service_val < $MAX_TMID) $health_flag = 3;
			}
			else
			{
				if($service_val == "") $health_flag = 4;
				elseif ($service_val != 1) $health_flag = 3;
			}
			$overall_flag = ($overall_flag > $health_flag) ? $overall_flag : $health_flag;
		}
		
		if($overall_flag == 1) $service_health = "Running";
		elseif($overall_flag == 3) $service_health = "Stopped";
		elseif($overall_flag == 4) $service_health = "Warning";
		
		return $service_health;
	}
	
	public function getPerfStatistics($host, $ps)
	{
		global $REPLICATION_ROOT, $SLASH, $MYSQL_DATA_PATH;
		$rrd_filepath = $REPLICATION_ROOT.$SLASH."SystemMonitorRrds".$SLASH;
		
		$param_file = $rrd_filepath.$host."-perf.txt";
		$param_xml_file = $rrd_filepath.$host."-perf.xml";
		
		if (file_exists($param_xml_file)) 
		{
            $perf_arr = $this->getPSXMLPerfData($param_xml_file,$host,$ps);
        }
		else if (file_exists($param_file)) 
		{
           $perf_arr = $this->getPSTXTPerfData($param_file,$host,$ps);
        }
		else
		{
			$perf_arr = $this->defaultPsPerfData();
		}
		
		return $perf_arr;
	}
	
	//Default values for process server statistics data
	function defaultPsPerfData()
	{
		global $MINUS_ONE, $FOUR;
		$perf = array();
		$perf['SystemLoad']['SystemLoad'] = $MINUS_ONE;		
		$perf['SystemLoad']['Status'] = $FOUR;		
		$perf['CPULoad']['CPULoad']= $MINUS_ONE;		
		$perf['CPULoad']['Status']= $FOUR;		
		$perf['MemoryUsage']['TotalMemory'] =  $MINUS_ONE;
		$perf['MemoryUsage']['UsedMemory'] =  $MINUS_ONE;
		$perf['MemoryUsage']['AvailableMemory'] =  $MINUS_ONE;
		$perf['MemoryUsage']['MemoryUsage'] =  $MINUS_ONE;
		$perf['MemoryUsage']['Status'] = $FOUR;
		$perf['FreeSpace']['FreeSpace']=$MINUS_ONE;
		$perf['FreeSpace']['TotalSpace']= $MINUS_ONE;
		$perf['FreeSpace']['AvailableSpace']= $MINUS_ONE;
		$perf['FreeSpace']['UsedSpace']= $MINUS_ONE;
		$perf['FreeSpace']['Status']= $FOUR;
		$perf['Throughput']['UploadPendingData']= $MINUS_ONE;
		$perf['Throughput']['Throughput']= $MINUS_ONE;	
		$perf['Throughput']['ThroughputInBytes']= $MINUS_ONE;	
		$perf['Throughput']['Status']= $FOUR;
		$perf['WebServer']['WebLoad']= $MINUS_ONE;
		$perf['DatabaseServer']['SqlLoad'] = $MINUS_ONE;
		$perf['WebServer']['Status']= $FOUR;
		$perf['DatabaseServer']['Status'] = $FOUR;
		return $perf;
	}
	
	function getPSXMLPerfData($param_file,$host,$ps)
	{
		global $REPLICATION_ROOT, $SLASH, $MYSQL_DATA_PATH;
		
		$xml = simplexml_load_file ( $param_file );
		if ($xml === false)
		{
			return;
		}
		
		foreach($xml->children() as $xmlData)
		{
			if($xmlData['Name'] == 'PSGuid')
			{
				$PSGuid = $xmlData['Value'];
			}
			if($xmlData['Name'] == 'PSVersion')
			{
				$PSVersion = $xmlData['Value'];
			}
			if($xmlData['Id'] == 'SystemPerformance')
			{
				foreach($xmlData->children() as $xmlDataChildren)
				{
					if($xmlDataChildren['Id'] == 'SystemLoad')
					{
						foreach($xmlDataChildren->Parameter as $val)
						{
							if($val['Name'] == 'SystemLoad')
							{
								$SystemLoadValue = $val['Value'];
							}
							if($val['Name'] == 'Status')
							{
								$SystemLoadStatus = $val['Value'];
							}
						}
					}
					if($xmlDataChildren['Id'] == 'CPULoad')
					{
						foreach($xmlDataChildren->Parameter as $val)
						{
							if($val['Name'] == 'CPULoad')
							{
								$CPULoadValue = $val['Value'];
							}
							if($val['Name'] == 'Status')
							{
								$CPULoadStatus = $val['Value'];
							}
						}
					}
					if($xmlDataChildren['Id'] == 'MemoryUsage')
					{
						foreach($xmlDataChildren->Parameter as $val)
						{
							if($val['Name'] == 'TotalMemory')
							{
								$TotalMemoryValue = $val['Value'];
							}
							if($val['Name'] == 'UsedMemory')
							{
								$UsedMemoryValue = $val['Value'];
							}
							if($val['Name'] == 'AvailableMemory')
							{
								$AvailableMemoryValue = $val['Value'];
							}
							if($val['Name'] == 'MemoryUsage')
							{
								$MemoryUsageValue = $val['Value'];
							}
							if($val['Name'] == 'Status')
							{
								$MemoryUsageStatus = $val['Value'];
							}
						}
					}
					if($xmlDataChildren['Id'] == 'FreeSpace')
					{
						foreach($xmlDataChildren->Parameter as $val)
						{
							if($val['Name'] == 'FreeSpace')
							{
								$FreeSpaceValue = $val['Value'];
							}
							if($val['Name'] == 'TotalSpace')
							{
								$TotalSpaceValue = $val['Value'];
							}
							if($val['Name'] == 'AvailableSpace')
							{
								$AvailableSpaceValue = $val['Value'];
							}
							if($val['Name'] == 'UsedSpace')
							{
								$UsedSpaceValue = $val['Value'];
							}
							if($val['Name'] == 'Status')
							{
								$FreeSpaceStatus = $val['Value'];
							}
						}
					}
					if($xmlDataChildren['Id'] == 'Throughput')
					{
						foreach($xmlDataChildren->Parameter as $val)
						{
							if($val['Name'] == 'UploadPendingData')
							{
								$UploadPendingDataValue = $val['Value'];
							}
							if($val['Name'] == 'Throughput')
							{
								$ThroughputValue = $val['Value'];
							}
							if($val['Name'] == 'Status')
							{
								$ThroughputStatus = $val['Value'];
							}
						}
					}
				}
			}
		}
		
		$perf = array();
		$perf['SystemLoad']['SystemLoad'] = $SystemLoadValue;		
		$perf['SystemLoad']['Status'] = $SystemLoadStatus;		
		$perf['CPULoad']['CPULoad']= $CPULoadValue;		
		$perf['CPULoad']['Status']= $CPULoadStatus;		
		$perf['MemoryUsage']['TotalMemory'] = $TotalMemoryValue;
		$perf['MemoryUsage']['UsedMemory'] = $UsedMemoryValue;
		$perf['MemoryUsage']['AvailableMemory'] = $AvailableMemoryValue;
		$perf['MemoryUsage']['MemoryUsage'] = $MemoryUsageValue;
		$perf['MemoryUsage']['Status'] = $MemoryUsageStatus;
		$perf['FreeSpace']['FreeSpace']= $FreeSpaceValue;
		$perf['FreeSpace']['TotalSpace']= $TotalSpaceValue;
		$perf['FreeSpace']['AvailableSpace']= $AvailableSpaceValue;
		$perf['FreeSpace']['UsedSpace']= $UsedSpaceValue;
		$perf['FreeSpace']['Status']= $FreeSpaceStatus;
		if($ps)
		{
			$perf['Throughput']['UploadPendingData']= $UploadPendingDataValue;
			if($ThroughputValue == "-1")
			{
				$perf['Throughput']['Throughput']= $ThroughputValue;
				$perf['Throughput']['ThroughputInBytes']= $ThroughputValue;	
			}
			else
			{
				$ThroughputInMbps = ($ThroughputValue/(1024*1024));
				$perf['Throughput']['Throughput']= round($ThroughputInMbps);	
				$perf['Throughput']['ThroughputInBytes']= $ThroughputValue;	
			}
			$perf['Throughput']['Status']= $ThroughputStatus;
		}
		
		if(!$ps)
		{
			$web_load = $sql_load = 0;
			
			$one_more_apache = exec("tasklist | findstr /I 'w3sp'");
			
			$per_apache_load = 0;
			
			if(!preg_match ('/ERROR/i', $one_more_apache))
			{
				$apache_vals = preg_split('/\s+/', $one_more_apache);
				$per_apache_load = $apache_vals[2];
			}
			$one_more_apache = exec("tasklist | findstr /I 'php-cgi'");
			if(!preg_match ('/ERROR/i', $one_more_apache))
			{
				$apache_vals = preg_split('/\s+/', $one_more_apache);
				foreach ($apache_vals as $php_key)
				{
					$php_loads = preg_split('/s+/', $php_key);
					$apa_load += $php_loads[2];
				}
				$per_apache_load += $apa_load;
			}
			if($per_apache_load > 0) $per_apache_load = round($per_apache_load, 2);
			$web_load = $per_apache_load;
			
			$mysql_file_to_read = $MYSQL_DATA_PATH."my.ini";
			
			$sql_threads = 0;
            $mysql_file_handle = $this->commonfunctions_obj->file_open_handle($mysql_file_to_read);
            if(!$mysql_file_handle)
            {
                return;
            }
            $mysql_str = fread($mysql_file_handle, filesize($mysql_file_to_read));
            fclose($mysql_file_handle);
			preg_match('/max_connections=(\d+)/', $mysql_str, $line);
			$sql_threads = $line[1];
			
			if($this->conn)
			{
				$rs = $this->conn->sql_query("show status where Variable_name = 'Threads_connected'");
				$row = $this->conn->sql_fetch_object ($rs);
				$mysql_load_check = $row->Value;
				$sql_load = ($mysql_load_check / $sql_threads) * 100;
			}
			
			$perf['WebServer']['WebLoad']= $web_load;
			$perf['DatabaseServer']['SqlLoad'] = $sql_load;
			
			if($web_load <= 50) $WebLoadStatus = 1;
			elseif($web_load <= 70) $WebLoadStatus = 2;
			elseif($web_load == "") $WebLoadStatus = 4;
			
			if($sql_load == "") $DBStatus = 4;
			elseif($sql_load <= 30) $DBStatus = 1;
			elseif($sql_load <= 60) $DBStatus = 2;
				
			$sql_load = round($sql_load, 2); 
			
			$perf['WebServer']['WebLoad']= $web_load;
			$perf['DatabaseServer']['SqlLoad'] = $sql_load;
			$perf['WebServer']['Status']= $WebLoadStatus;
			$perf['DatabaseServer']['Status'] = $DBStatus;
		}
		
		return $perf;
	}
	
	function getPSTXTPerfData($param_file,$host,$ps)
	{
		global $REPLICATION_ROOT, $SLASH, $MYSQL_DATA_PATH;
		
		$file_handle = $this->commonfunctions_obj->file_open_handle($param_file);
        if(!$file_handle)
        {
            return;
        }
        $perf_str = fread($file_handle, filesize($param_file));
        fclose($file_handle);
		
		list($cs_ps_guid, $sys_load, $cpu_load, $free_space, $tot_freespace, $avail_freespace, $mem_usage, $tot_memusage, $used_memusage, $disk_io) = explode(":",$perf_str);
		
		$perf = array();		
		$perf['SystemLoad'] = $sys_load;		
		$perf['CPULoad']= $cpu_load;		
		$perf['MemoryUsage'] = $mem_usage;		
		$perf['FreeSpace']= $free_space;	
		$host_os_type = $this->commonfunctions_obj->get_osFlag($host);	
		if ($host_os_type != 1) $perf['DiskActivity'] = $disk_io;
		
		if(!$ps)
		{
			$web_load = $sql_load = 0;
			
			if($this->commonfunctions_obj->is_linux())
			{
				$apache_file = $REPLICATION_ROOT."var/apache.txt";
                $apache_file_handle = $this->commonfunctions_obj->file_open_handle($apache_file);
                if(!$apache_file_handle)
                {
                    return;
                }
                $apache_str = fread($apache_file_handle, filesize($apache_file));
                fclose($apache_file_handle);
				preg_match('/CPULoad:\s+(\d*\.\d+)/', $apache_str, $apache_arr);
				$web_load = $apache_arr[1];
				$mysql_file_to_read = "/etc/my.cnf";	
			}
			else
			{	
				$one_more_apache = exec("tasklist | findstr /I 'w3sp'");
				
				$per_apache_load = 0;
				
				if(!preg_match ('/ERROR/i', $one_more_apache))
				{
					$apache_vals = preg_split('/\s+/', $one_more_apache);
					$per_apache_load = $apache_vals[2];
				}
				$one_more_apache = exec("tasklist | findstr /I 'php-cgi'");
				if(!preg_match ('/ERROR/i', $one_more_apache))
				{
					$apache_vals = preg_split('/\s+/', $one_more_apache);
					foreach ($apache_vals as $php_key)
					{
						$php_loads = preg_split('/s+/', $php_key);
						$apa_load += $php_loads[2];
					}
					$per_apache_load += $apa_load;
				}
				if($per_apache_load > 0) $per_apache_load = round($per_apache_load, 2);
				$web_load = $per_apache_load;
				
				$mysql_file_to_read = $MYSQL_DATA_PATH."my.ini";
			}
			
			$sql_threads = 0;
            $mysql_file_handle = $this->commonfunctions_obj->file_open_handle($mysql_file_to_read);
            if(!$mysql_file_handle)
            {
                return;
            }
            $mysql_str = fread($mysql_file_handle, filesize($mysql_file_to_read));
            fclose($mysql_file_handle);
			preg_match('/max_connections=(\d+)/', $mysql_str, $line);
			$sql_threads = $line[1];
			
			if($this->conn)
			{
				$rs = $this->conn->sql_query("show status where Variable_name = 'Threads_connected'");
				$row = $this->conn->sql_fetch_object ($rs);
				$mysql_load_check = $row->Value;
				$sql_load = ($mysql_load_check / $sql_threads) * 100;
			}
			
			$perf['WebServer']= $web_load;
			$perf['DatabaseServer'] = $sql_load;
		}
		
		$perf_arr = array();
		foreach($perf as $key => $value)
		{
			if($key == "SystemLoad" || $key == "CPULoad" || $key == "MemoryUsage" || $key == "FreeSpace" || $key == "DiskActivity" || $key == "WebServer" || $key == "DatabaseServer")
			{
				$health_flag = 3;				
				if($key == "SystemLoad")
				{
					if($value == "") $health_flag = 4;
					elseif($value <= 6) $health_flag = 1;
					elseif($value <= 12) $health_flag = 2;
					$name = "SystemLoad";
				}
				elseif($key == "CPULoad")
				{
					if($value == "") $health_flag = 4;
					elseif($value <= 80) $health_flag = 1;
					elseif($value <= 95) $health_flag = 2;
					$value = ($value) ? $value."%" : $value;	
					$name = "CpuLoad";	
				}
				elseif($key == "MemoryUsage")
				{
					if($value == "") $health_flag = 4;
					elseif($value <= 80) $health_flag = 1;
					elseif($value > 80 && $value < 95) $health_flag = 2;
					
					$perf_arr[$key]['TotalMemory'] = $tot_memusage;				
					$perf_arr[$key]['UsedMemory'] = $used_memusage;
					$perf_arr[$key]['AvailableMemory'] = $tot_memusage - $used_memusage;
					$name = "MemoryUsage";
					$value = ($value) ? $value."%" : $value;
				}
				elseif($key == "FreeSpace")
				{					
					if($value == "") $health_flag = 4;
					elseif($value >= 30) $health_flag = 1;
					elseif($value >= 25) $health_flag = 2;
					
					$perf_arr[$key]['TotalSpace'] = $tot_freespace;				
					$perf_arr[$key]['AvailableSpace'] = $avail_freespace;				
					$perf_arr[$key]['UsedSpace'] = $tot_freespace - $avail_freespace;	
					$name = "FreeSpace";
					$value = ($value) ? $value."%" : $value;
				}
				elseif($key == "DiskActivity" && $host_os_type != 1)
				{	
					if($value != "") $health_flag = 1;
					else $health_flag = 4;
					$name = "Activity";
				}
				elseif($key == "WebServer")
				{
					if($value <= 50) $health_flag = 1;
					elseif($value <= 70) $health_flag = 2;
					elseif($value == "") $health_flag = 4;
					$name = "WebLoad";
				}
				elseif($key == "DatabaseServer")
				{
					if($value == "") $health_flag = 4;
					elseif($value <= 30) $health_flag = 1;
					elseif($value <= 60) $health_flag = 2;
					
					$value = round($value, 2); 
					$name = "SqlLoad";
				}
				$perf_arr[$key][$name] = $value;
				$perf_arr[$key]['Status'] = $health_flag;			
			}
		}
		
		return $perf_arr;
	}
	
	
	/*
    * Function Name: getPSSourceHosts
    *
    * Description: 
    * This function is to get the source server ids protected
	*	using this Process Server
    *
    * Parameters:
    *     Param 1 [IN]: PS ID
    *     Param 2 [OUT]: SOURCE IDS (Those that are protected)
    *     
    * Exceptions:
    *     SQL exception on transaction failure
    */
	
	public function getPSSourceHosts($ps_id)
	{
		global $INMAGE_PROFILER_HOST_ID;
		
		$sql = "SELECT DISTINCT
					sourceHostId,
					processServerId
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					processServerId = ? AND
					destinationHostId != ?";
		$result = array();
		$result = $this->conn->sql_get_array($sql, "sourceHostId", "processServerId", array($ps_id, $INMAGE_PROFILER_HOST_ID));
		return array_keys($result);	
	}
	
	/*
    * Function Name: failoverPs
    *
    * Description: 
    * This function will change the process server from old PS to new PS
    *
    * Parameters:
    *     Param 1 [IN]: SOURCE PS ARRAY, NEW PS ID
    *     Param 2 [OUT]: 
    *     
    * Exceptions:
    *     SQL exception on transaction failure
    */
	public function psFailover($server_ps_array, $old_ps_id, $call_from_api=NULL)
	{
		global $INMAGE_PROFILER_HOST_ID, $ABHAI_MIB, $TRAP_AMETHYSTGUID, $TRAP_AGENT_ERROR_MESSAGE, $TRAP_PROCESS_SERVER_FAILOVER, $A2E, $E2A,$REPLICATION_ROOT, $HOST_GUID;
		global $MARS_HEALTH_FACTOR_SOURCE_FOR_PROTECTION, $MARS_RETRYABLE_ERRORS_PROTECTION_HF_MAP, $PS_FAIL_OVER ;
		
		// Get all the Process Servers Name and IPAddress
		$all_ps_details = array();		
		$ps_ids = array_values($server_ps_array);
		$ps_ids[] = $old_ps_id;
		$ps_ids = array_unique($ps_ids);
		$ps_ids_str = implode(",", $ps_ids);
		$scenario_details = array();
		$ps_details_sql = "SELECT id, name, ipaddress FROM hosts WHERE FIND_IN_SET(id, ?)";
		$ps_details = $this->conn->sql_query($ps_details_sql, array($ps_ids_str));
		foreach($ps_details as $detail)
		{
			$all_ps_details[$detail['id']]['name'] = $detail['name'];
			$all_ps_details[$detail['id']]['ipaddress'] = $detail['ipaddress'];
		}
		$old_ps_name = $all_ps_details[$old_ps_id]['name'];		
		$old_ps_ip = $all_ps_details[$old_ps_id]['ipaddress'];
		
		$pair_str = '';
		$agent_log = array();
		if($call_from_api)
		{
			$update_fields = "";
		}
		
		// Get pair destination hosts and device names
		$pair_details = array();
		$source_ids = array_keys($server_ps_array);
		$source_ids = array_unique($source_ids);
		$source_ids_str = implode(",", $source_ids);
		
		$source_hosts_str = implode(",", $source_ids);
		$get_scenario_details_sql = "SELECT
										scenarioId,
										scenarioDetails,
										sourceId,
										targetId 
									FROM
										applicationScenario
									WHERE
										scenarioType = 'DR Protection' AND
										FIND_IN_SET(sourceId, ?)";
		$scenario_details = $this->conn->sql_query($get_scenario_details_sql, array($source_hosts_str));
			
		$pair_details_sql = "SELECT
								sourceHostId,
								sourceDeviceName,
								destinationHostId,
								destinationDeviceName,
								isResync,
								ruleId 
							FROM
								srcLogicalVolumeDestLogicalVolume
							WHERE
								FIND_IN_SET(sourceHostId, ?) AND
								processServerId = ? AND
								destinationHostId != ?
							";
		$pair_details_db = $this->conn->sql_query($pair_details_sql, array($source_ids_str, $old_ps_id, $INMAGE_PROFILER_HOST_ID));
		$dest_hosts = array();
		foreach($pair_details_db as $pair)
		{
			$dest_hosts[$pair['destinationHostId']][] = $pair['destinationDeviceName'];
			$pair_details[$pair['sourceHostId']][$pair['ruleId']] = $pair;
			$source_dest_details[$pair['sourceHostId']] = $pair['destinationHostId'];
		}		
		
		// Get name for all the hosts
		$host_ids = array();
		$dest_ids = array_keys($dest_hosts);
		$host_ids = array_unique(array_filter(array_merge($dest_ids, $source_ids)));
		$host_ids_str = implode(",", $host_ids);
		$hosts_sql = "SELECT id, name FROM hosts WHERE FIND_IN_SET(id, ?)";
		$host_names = $this->conn->sql_get_array($hosts_sql, "id", "name", array($host_ids_str));
		
		$per_server_pair_details = array();
		// Set the New Process Server and restart resync
		foreach($server_ps_array as $source_id => $new_ps_id)
		{
			$rule_ids = array_keys($pair_details[$source_id]);
			$rule_ids_str = implode(",", $rule_ids);
			$dest_cond = "";
			$pair_str = "";
			
			foreach($scenario_details as $details)
			{
				if ($source_id == $details['sourceId'])
				{
					$scenario_data = $details['scenarioDetails'];
					$scenario = unserialize(htmlspecialchars_decode($scenario_data));
					$protection_path = $scenario['protectionPath'];
					if ($protection_path != $A2E)
					{
						$dest_cond = ",destinationHostId = ? ";
					}
					break;
				}
			}
		
			
			$sql = "UPDATE 
						srcLogicalVolumeDestLogicalVolume
					SET 
						processServerId = ?,
						shouldResync = '1',
						ShouldResyncSetFrom = '1',
						resyncSetCxtimestamp = unix_timestamp(now()) ".$dest_cond.
					"WHERE 
						sourceHostId = ? AND
						FIND_IN_SET(ruleId, ?) AND
						destinationHostId != ?
						";
			
			if ($dest_cond!="")
			{
				$this->conn->sql_query($sql, array($new_ps_id, $new_ps_id, $source_id, $rule_ids_str, $INMAGE_PROFILER_HOST_ID));
			}
			else
			{
				$this->conn->sql_query($sql, array($new_ps_id, $source_id, $rule_ids_str, $INMAGE_PROFILER_HOST_ID));
			}
			
			$errors_protection_str = implode(",",$MARS_RETRYABLE_ERRORS_PROTECTION_HF_MAP);
			$sql = "DELETE 
					FROM
						healthfactors
					WHERE 
						sourceHostId = ? 
					AND
						component = ?
					AND
						FIND_IN_SET(healthFactorCode, ?)
					";
			$this->conn->sql_query($sql, array($source_id, $MARS_HEALTH_FACTOR_SOURCE_FOR_PROTECTION, $errors_protection_str));
			
			foreach($pair_details[$source_id] as $rule_id => $details)
			{
				//Change target host id of target devices with new process server id in case of on-premise protection.
				if (($protection_path != $A2E) && ($source_dest_details[$source_id] == $old_ps_id))
				{		
					$dest_device = $details['destinationDeviceName'];
					$sql = "UPDATE 
						logicalVolumes 
					SET 
						hostId = ? 
					WHERE 
						hostId = ? 
					AND 
						deviceName = ? 
					AND
						isImpersonate = ?";
					$this->conn->sql_query($sql, array($new_ps_id, $old_ps_id, $dest_device,1));

					$formatted_device = $this->commonfunctions_obj->verifyDatalogPath($dest_device);
					$formatted_device = str_replace(':', '', $formatted_device);
					$formatted_device = str_replace('\\', '/', $formatted_device);
					$output_dir = $REPLICATION_ROOT."/".$new_ps_id."/".$formatted_device;
					if(!file_exists($output_dir))
					{
						$mkdir_output = mkdir($output_dir, 0777, true);
					}		
					#print "$new_ps_id, $old_ps_id, $dest_device,1 \n";
				}
				
				//Failback protection PS failover from PS+MT to PS (or) PS to PS+MT.
				if ($new_ps_id != $source_dest_details[$source_id])
				{
					$transprotocol = 1;
				}
				else
				{
					$transprotocol = 2;
				}
				/* 
				Transprotocol is update is applicable only for Azure to On-prem protection. PS+MT to PS (or) PS to PS+MT.
				*/
				if ($protection_path == $A2E)
				{					
					$destination_host_id = $source_dest_details[$source_id];
					$dest_device = $details['destinationDeviceName'];
					$sql = "UPDATE 
						logicalVolumes 
					SET 
						transProtocol = ? 
					WHERE 
						hostId = ? 
					AND 
						deviceName = ?"; 
					
					$this->conn->sql_query($sql, array($transprotocol, $destination_host_id, $dest_device));
							
				}
				
				$new_ps = $all_ps_details[$server_ps_array[$source_id]]['name'];
				$new_ps_ip = $all_ps_details[$server_ps_array[$source_id]]['ipaddress'];
				
				$resync_flag = $details['isResync'];
				/* 
				* For Pair in resync or called from API
				*/
				if($resync_flag == 1 || $call_from_api)
				{
					$this->volume_obj->resync_now($rule_id);
					$agent_log[$rule_id] = "Process server failover occured from ".trim($old_ps_name)." [$old_ps_ip] to ".trim($new_ps)." [$new_ps_ip]. Resync restarted.";
				}
				else
				{
					$agent_log[$rule_id] = "Process server failover occured from $old_ps_name [$old_ps_ip] to $new_ps [$new_ps_ip]. Resync is recommended for this pair.";
				}
				$server_str = "<br>".$host_names[$source_id]." [".
							$details['sourceDeviceName']."] -> ".
							$host_names[$details['destinationHostId']]." [".
							$details['destinationDeviceName']."]";	
				$pair_str .= $server_str;
				/* 
				* Update Agent Log
				*/
				$log_msg = $agent_log[$rule_id]." The following replication pairs have been affected.".$server_str;
				$this->commonfunctions_obj->update_agent_log($source_id, $details['sourceDeviceName'], $log_msg,0,"process_server_failover");
			}
			$per_server_pair_details[$source_id] = $pair_str;
		}
		
		reset($server_ps_array);
		// Process Server Failover Trap, Email and Alert
		$pair_msg = "";
		foreach($server_ps_array as $source_id => $new_ps_id)
		{	
			$new_ps = $all_ps_details[$new_ps_id]['name'];
			$new_ps_ip = $all_ps_details[$new_ps_id]['ipaddress'];
			
			$summary = "Process server Failover";
			$message = "Process server failover occured from $old_ps_name [$old_ps_ip] to $new_ps [$new_ps_ip].";
			$pair_msg = $pair_msg.$per_server_pair_details[$source_id];
			
			/* 
			* Send Email
			*/
			$error_id = "PS_FAILOVER_NOTICE";
			/*
			 * #8199 : Change the error_template_id for PS Failover.Earlier,it categorized under CXDEBUG_INF template.
			 */
			$error_template_id = "PS_FAILOVER";
			
			$placeholders['oldPsName'] = $old_ps_name;
			$placeholders['oldPsIp'] = $old_ps_ip;
			$placeholders['newPsName'] = $new_ps;
			$placeholders['newPsIp'] = $new_ps_ip;
			$placeholders['detectionTime'] = time();
			$placeholders_data =  serialize($placeholders);
			$this->commonfunctions_obj->updateAgentAlertsDb($error_template_id, $source_id,  $summary, $message, '', $PS_FAIL_OVER, $placeholders_data, '');
		}
		
		// Set scenario details if call from API
		if($call_from_api)
		{		
			reset($scenario_details);
			// Update Process Server Id in Scenario data for all the source hosts
			foreach($scenario_details as $details)
			{
				$cond = "";
				$scenario_id = $details['scenarioId'];
				$scenario_data = $details['scenarioDetails'];
				$source_id = $details['sourceId'];
				$target_id = $details['targetId'];
				// Get New PS ID
				$new_ps_id = $server_ps_array[$source_id];
				$scenario = unserialize(htmlspecialchars_decode($scenario_data));
				// Change the PS in globalOptions in session data
				$scenario['globalOption']['processServerid'] = $new_ps_id;
				$protection_path = $scenario['protectionPath'];
				$pair_data = $scenario['pairInfo'];
				foreach($pair_data as $pair_key => $pair_info)
				{
					if (($target_id == $old_ps_id) && ($protection_path != $A2E))
					{
						$scenario['pairInfo'][$pair_key]['targetId'] = $new_ps_id;
					}	
					$pair_details = $pair_info['pairDetails'];
					// Change the PS at pair level info in session data
					foreach($pair_details as $vol_key => $vol_info)
					{
						$scenario['pairInfo'][$pair_key]['pairDetails'][$vol_key]['processServerid'] = $new_ps_id;
					}
					
					$pair_key_data = $scenario['pairInfo'][$pair_key];
					
					if (($target_id == $old_ps_id) && ($protection_path != $A2E))
					{
						$old_pair_key_data = explode("=",$pair_key);
						$new_pair_key_data = $old_pair_key_data[0]."=".$new_ps_id;
						$scenario['pairInfo'][$new_pair_key_data] = $pair_key_data;
						unset($scenario['pairInfo'][$pair_key]);
					}
					
				}
				if (($target_id == $old_ps_id) && ($protection_path != $A2E))
				{
						$scenario['targetHostId'] = $new_ps_id;
						$cond = ",targetId = ? ";
				}
				
				// Updating the modified scenario data
				$modified_scenario_details = serialize($scenario);	
				$update_scenario_sql = "UPDATE applicationScenario SET scenarioDetails = ? ".$cond." WHERE scenarioId = ?";
				if ($cond != "" )
				{
					$this->conn->sql_query($update_scenario_sql, array($modified_scenario_details, $new_ps_id, $scenario_id));
				}
				else
				{
					$this->conn->sql_query($update_scenario_sql, array($modified_scenario_details, $scenario_id));
				}
			}
		}
	}
    
    /*
    * Function Name: validatePsHosts
    *
    * Description: 
    * This function validate the following:
	*	1. Validate the old Process Server protection
    *   2. Validate the source servers are protected
    *   3. Validate the target heartbeat for all the servers
    *   4. Validate the source servers attached to the new process server
    *
    * Parameters:
    *     Param 1 [IN]: OLD PS ID, NEW PS ID
    *     Param 2 [OUT]: 
    *     
    * Exceptions:
    *     SQL exception on transaction failure
    */
    
    public function validatePsHosts($server_ps_array, $old_ps_id)
    {
		global $INMAGE_PROFILER_HOST_ID;
		$old_ps_name = $this->commonfunctions_obj->getHostName($old_ps_id);
		
        $source_ids = array_keys($server_ps_array);
        $source_ids_str = implode(",", $source_ids);
        
        $get_pair_details_sql = "SELECT
                                    p.ruleId,
                                    p.sourceHostId,
                                    p.sourceDeviceName,
                                    p.destinationHostId,
                                    p.destinationDeviceName,
                                    p.processServerId,
									UNIX_TIMESTAMP(now()) `currentTime`,
									h.lastHostUpdateTime `lastHostUpdateTime`,
									h.ipaddress									
                                FROM
                                    srcLogicalVolumeDestLogicalVolume p,
                                    hosts h
                                WHERE
                                    h.id = p.destinationHostId AND                                    
                                    FIND_IN_SET(p.sourceHostId, ?) AND
									p.destinationHostId != ?
                                ";
        $pair_details = $dest_ids = array();
        $pair_details_db = $this->conn->sql_query($get_pair_details_sql, array($source_ids_str, $INMAGE_PROFILER_HOST_ID));
		if(!$pair_details_db) return array(0, '');
        foreach($pair_details_db as $pair)
        {
            $pair_details[$pair['sourceHostId']] = $pair;
            if(!in_array($pair['destinationHostId'], $dest_ids)) $dest_ids[] = $pair['destinationHostId'];
        }
        
        // Get ID and Names of Source Servers
        $all_host_ids = array();
        $all_host_ids = array_merge($source_ids, $dest_ids);
        $all_host_ids_str = implode(",", $all_host_ids);
        $id_name_array_sql = "SELECT id, name FROM hosts WHERE FIND_IN_SET(id, ?)";
        $host_id_names = $this->conn->sql_get_array($id_name_array_sql, "id", "name", array($all_host_ids_str));
        
        foreach($server_ps_array as $source_id => $new_ps_id)
        {
            if(!array_key_exists($source_id, $pair_details)) return array(1, array('SourceHostName' => $host_id_names[$source_id]));
            $server_pairs = $pair_details[$source_id];
            if($server_pairs['currentTime'] - $server_pairs['lastHostUpdateTime'] >= 900) return array(2, array('MTHostName' => $host_id_names[$server_pairs['destinationHostId']], 'MTIP' => $server_pairs['ipaddress'], 'Interval' => ceil(($server_pairs['currentTime'] - $server_pairs['lastHostUpdateTime'])/60)));            
        }
        return array(0, '');
    }
    
    public function getPsForSource($source_id)
    {
        return $this->conn->sql_get_array("SELECT processServerId, sourceHostId FROM srcLogicalVolumeDestLogicalVolume WHERE FIND_IN_SET(sourceHostId, ?)", "sourceHostId", "processServerId", array(implode(",", $source_id)));
    }
    
    public function psUpdateDetails($ps_host_id, $update_details)
    {
        $check_update_sql = "SELECT patchFilename FROM patchDetails WHERE processServerId = ?";
        $check_updates = $this->conn->sql_get_array($check_update_sql, "patchFilename", "patchFilename", array($ps_host_id));
        $check_updates = array_keys($check_updates);
        
        $update_sql_args = array();
        $update_patch_details_sql = "INSERT INTO patchDetails (processServerId, patchFilename, osType, targetOs, updateType, patchVersion, description, rebootRequired) VALUES";
        foreach($update_details as $update)
        {
            // Skip if the entry is already made to the DB
            if(in_array($update['patchFilename'], $check_updates)) continue;
            
            // SQL
            $update_values .= ($update_values) ? ", " : "";
            $update_values .= "(?, ?, ?, ?, ?, ?, ?, ?)";
            
            // SQL arguments
            array_push($update_sql_args, $ps_host_id, $update['patchFilename'], $update['osType'], $update['targetOs'], $update['updateType'], $update['patchVersion'], $update['description'], $update['rebootRequired']);
        }
		if($update_values) $this->conn->sql_query($update_patch_details_sql.$update_values, $update_sql_args);
    }
	
	/*
    * Function Name: listHealthEvents
    *
    * Description: 
    * This function will return list of the health events based upon
	* component guid and type
    *
    * Parameters:
    *     Param 1 [IN]: component guid and type
    *     Param 2 [OUT]: List of the health events
    *     
    * Exceptions:
    *     N/A
    */
	
	public function listHealthEvents($guid,$component)
	{
		$event_details = array();
		$health_events_sql = "SELECT healthFactorCode,healthFactor,component,UNIX_TIMESTAMP(healthCreationTime) as healthCreationTime,healthUpdateTime,placeHolders FROM infrastructurehealthfactors WHERE hostId = ? and component = ?";
        $health_events_db = $this->conn->sql_query($health_events_sql, array($guid, $component));
		
		if(!$health_events_db) return array();
        foreach($health_events_db as $events)
        {
            $severity = "Warning";
			if($events['healthFactor'] == 2)
			{
				$severity = "Critical";
			}
			$event_details[$events['healthFactorCode']]['severity'] = $severity;
			$event_details[$events['healthFactorCode']]['healthCreationTime'] = $events['healthCreationTime'];
			$event_details[$events['healthFactorCode']]['healthUpdateTime'] = $events['healthUpdateTime'];
			$event_details[$events['healthFactorCode']]['healthFactorCode'] = $events['healthFactorCode'];
			$event_details[$events['healthFactorCode']]['placeHolders'] = $events['placeHolders'];
			$event_details[$events['healthFactorCode']]['component'] = $events['component'];
	   }
	   return $event_details;
	}
	
	/*
    * Function Name: checkMarsComponentHealth
    *
    * Description: 
    * This function will return list of the MARS health status for every PS.
    *
    * Parameters:
    *   	N/A
    *     
    * Exceptions:
    *     	N/A
    */
	public function checkMarsComponentHealth()
	{
		global $MARS_RETRYABLE_ERRORS_FABRIC_HF_MAP;
		$mars_health_status = array();
		$errors_fabric_hf_map = implode(",",$MARS_RETRYABLE_ERRORS_FABRIC_HF_MAP);
		$sql = "select hostId, healthFactorCode from infrastructurehealthfactors where FIND_IN_SET(healthFactorCode, ?)";
		$health_status_details = $this->conn->sql_query($sql, array($errors_fabric_hf_map));
		if(!$health_status_details) return array();

		foreach ($health_status_details as $key=>$value)
		{
			switch ($value['healthFactorCode']) 
			{						
				case "EPH00022":
					$mars_health_status[$value['hostId']]['marsCommunicationStatus'] = 0;
				break;
				case "EPH00023":
					$mars_health_status[$value['hostId']]['marsRegistrationStatus'] = 0;
				break;
				case "EPH00024":
					$mars_health_status[$value['hostId']]['marsRegistrationStatus'] = 0;
				break;
				default:
			}
		}
		return $mars_health_status;
	}
};
?>