<?php
/* 
 *================================================================= 
 * FILENAME
 *    System.php
 *
 * DESCRIPTION
 *    This script contains functions related to administrator part
 *
 * HISTORY
 *     20-Feb-2008  Author    Created - Chandra Sekhar Padhi
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
Class System
{
    
    private $commonfunctions_obj;
    private $conn;
	private $uid;
	private $encryption_key;
	private $logging_obj;
    
    /*    
    * Function Name: System
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
 	public function __construct()
    {
		global $conn_obj, $logging_obj;
        $this->commonfunctions_obj = new CommonFunctions();
        $this->conn = $conn_obj;
		$this->encryption_key = $this->commonfunctions_obj->getCSEncryptionKey();
		$this->logging_obj = $logging_obj;
	}     
    
    /*
    * Function Name: is_UID
    *
    * Description:
    *    This is the function checks that there is any admin entry
    *    in table users or not for the given UID     
    *
    *    <Give more details here>    
    *
    * Parameters:
    *     Param 1 [IN]:$UID
    *     Param 2 [OUT]:True or False
    *
    * Return Value:
    *     Ret Value: True or False
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function is_UID ($UID)
    {
        $UID = intval ($UID);

        if (!is_int ($UID) || $UID <= 0)
        {
        return false;
        }

        $result = $this->conn->sql_query ("SELECT 
                                    UID 
                                FROM 
                                    users 
                                WHERE 
                                    UID='$UID'");
        if (!$result) 
        {
            #die ($this->conn->sql_error ()); 
        }


        if ($this->conn->sql_num_row ($result) >= 1)
        {
            return true;
        }
        return false;
    }
    
    /*
    * Function Name: getPath
    *
    * Description:
    *    This  function returns the path ,by fetching 
    *    from 'transbandSettings' table
    *
    *    <Give more details here>    
    *
    * Parameters:
    *     Param 1 [OUT]:$myrow[0] (contains the path) or 0
    *
    * Return Value:
    *     Ret Value: $myrow[0] (contains the path) or 0
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */ 
    public function getPath()
    {
        $sql="SELECT
                  ValueData 
              FROM 
                  transbandSettings
              WHERE
                  ValueName ='PATH';";
        $rs = $this->conn->sql_query($sql);
        if( $myrow = $this->conn->sql_fetch_row($rs) ) 
        {
            return $myrow[0];
        }
        else
        {
            return 0;
        }
    }

    /// \brief copies files to be archived in $fromName to $toDir
    public function copyFilesToTmpDir(&$files,     ///< receives list of files in tmp dir to be archived
                                      $fromName,  ///< dir/file to copy/move
                                      $toDir,     ///< dir to copy/move files to
                                      $move = 0)  ///< indicates if move files or copy them 0: copy, non 0: move
    {
        if (is_file($fromName)) 
		{
            $tmp_path = $toDir.'/'.basename($fromName);
			if(!in_array($tmp_path, $files))
			{
				copy($fromName, $tmp_path);
				$files[] = $tmp_path;
			}
			else
			{
				$cnt_val = 1;
				while($cnt_val <= count($files))
				{
					if(!in_array($tmp_path.$cnt_val, $files))
					{
						copy($fromName, $tmp_path.$cnt_val);
						$files[] = $tmp_path.$cnt_val;
						break;
					}
					$cnt_val++;
				}
			}
			
            if (0 != $move) {
                unlink($fromName);
            }
        } elseif (is_dir($fromName)) 
		{
			$cnt = 1;
            $fromDir = opendir($fromName);
            do {
                $dirEnt = readdir($fromDir);
                if (false === $dirEnt) {
                    break;
                }
                if ('.' != $dirEnt && '..' != $dirEnt) {
                    if (is_dir($fromName . '/' . $dirEnt)) {
                        mkdir($toDir . '/' . $dirEnt);
                        $this->copyFilesToTmpDir($files, $fromName . '/' . $dirEnt, $toDir . '/' . $dirEnt, $move);
                    } else {
                        copy($fromName . '/' . $dirEnt, $toDir . '/' . $dirEnt . $cnt);
                        $files[] =$toDir . '/' . $dirEnt . $cnt;
                        if (0 != $move) {
                            unlink($fromName . '/' . $dirEnt);
                        }
                        $cnt++;
                    }
                }
            } while (false !== $dirEnt);
            if (0 != $move) {
                rmdir($fromName);
            }
        }
    }

    /// collects files in $list and copies or moves them to $tmpDir
    public function collectFiles($list,     ///< string, array or array of array (fullpath, basename) that has files/dirs to be archived
                                 $tmpDir,   ///< tmp dir to place the files to be archvied
                                 $move = 0) ///< indicates if files should be copied or moved 0: copy, non 0: move
    {
        $files = array();
        if (is_array($list)) {
            foreach ($list as $name) {
                if (is_array($name)) {
                    $fileName = $name[0];
                } else {
                    $fileName = $name;
                }
                // convert \ to / and remove any repeating \ or /
                $fileName = preg_replace(array('/\\\\+/', '/\/{2,}/'), '/', $fileName);
                $this->copyFilesToTmpDir($files, $fileName, $tmpDir, $move);
            }
        } else {
            $this->copyFilesToTmpDir($files, $list, $tmpDir, $move);
        }
        return $files;
    }

    /// brief creates and adds files to the archive file
    public function addFilesToArchive($name,              ///< name of archive file (full path)
                                      $parentTmpDir,
                                      $files,             ///< array of file names (each file name  should have full path)
                                      $archiveType)       ///< type of archive tocreate 1: zip, non 1: tar.gz
    {
        $dirPrefixLen = strlen($parentTmpDir) + 1;
        $cwd = getcwd();
        chdir($parentTmpDir);
        $zip = new ZipArchive();
		$archName = $name . '.zip';
		$rc = $zip->open($archName, ZipArchive::OVERWRITE);
		if (true !== $rc) {
			return null;
		}
		foreach ($files as $file) {
			$zip->addFile(substr($file, $dirPrefixLen ));
		}
		$zip->close();
		chdir($cwd);
        $archName = preg_replace(array('/\\\\+/', '/\/{2,}/'), '/', $archName);
        if (preg_match('/Windows/i', php_uname())) {
            $archName =  preg_replace(array('/\//'), '\\', $archName);
        }
        return $archName;

    }

    /// \brief cleans all files and dirs including $name if needed
    public function cleanDirIfNeeded($name) ///< directory to clean
    {
        if (is_file($name)) {
            unlink($name);
        } elseif (is_dir($name)) {
            $dir = opendir($name);
            do {
                $dirEnt = readdir($dir);
                if (false === $dirEnt) {
                    break;
                }
                if ('.' != $dirEnt && '..' != $dirEnt) {
                    if (is_dir($name . '/' . $dirEnt)) {
                        $this->cleanDirIfNeeded($name . '/' . $dirEnt);
                    } else {
                        unlink($name . '/' . $dirEnt);
                    }
                }
            } while (false !== $dirEnt);
            rmdir($name);
        }
    }

    public function cleanTmpArchDirsIfNeeded($tmpDir, $tmpArch)
    {
        $dir = opendir($tmpDir);
        if (false !== $dir) {
            do {
                $dirEnt = readdir($dir);
                if (false === $dirEnt) {
                    break;
                }
                if (is_dir($tmpDir . '/' . $dirEnt) && preg_match('/^' . $tmpArch . '/', $dirEnt)) {
                    $this->cleanDirIfNeeded($tmpDir . '/' . $dirEnt);
                }
            } while (false !== $dirEnt);
        }
    }
    
    /// \brief creates archive (tar or zip) all all files in list
    public function archive_log_files($name,                  ///< name of archive
                                      $list,                  ///< files/dirs to archive
                                      $archiveType = '1', ///< '1' = zip, non '1' = tar)
                                      $logType = '1',         ///< ? used by ArchiveFile_Zip
                                      $label = 'cx',          ///< ? used by ArchiveFile_Zip
                                      $move = 0)              ///< indicaes if move or copy files being archived 0: copy, non 0: move
    {
        $tmpArch = 'tmp-arch';
        global $AUDIT_LOG_DIRECTORY;
        $tmpDir = $AUDIT_LOG_DIRECTORY . '/';
        $this->cleanTmpArchDirsIfNeeded($tmpDir, $tmpArch);
        $tmpDir .= uniqid($tmpArch);
        mkdir($tmpDir);
        $name = $tmpDir . '/' . $name;
        $tmpDir = preg_replace(array('/\\\\+/', '/\/{2,}/'), '/', $tmpDir);
        $files = $this->collectFiles($list, $tmpDir, $move);

        // TODO: for now override $archiveType and base it on current platform
        // windows: .zip, linux: tar.gz
        // just delete this comment and line below to stop overriding
        $archiveType = preg_match('/Windows/i', php_uname()) ? 1 : 0;
        return $this->addFilesToArchive($name, dirName($tmpDir), $files, $archiveType);
    }           
    
    /*
    * Function Name:get_agent_heartbeat
    *
    * Description:
    *    This  function Returns array of hashes (name,sentinel,outpost,filerep)
    *    containing hearbeat
    *
    *    Heartbeat is the last time that the agent on the host contacted the CX    
    *
    * Parameters:
    *     Param 1 [OUT]:$result 
    *
    * Return Value:
    *     Ret Value: 
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */ 
    public function get_agent_heartbeat()
    {
     
        global $INMAGE_PROFILER_HOST_ID;
    
        $result = array();
		
		$sql ="SELECT
				  id,
				  name,
				  FROM_UNIXTIME(lastHostUpdateTime) , 
				  ipaddress ,
				  lastHostUpdateTimeFx ,
				  sentinelEnabled,
				  outpostAgentEnabled,
				  filereplicationAgentEnabled,
				  lastHostUpdateTimePs,
				  lastHostUpdateTimeApp,
				  hypervisorState,
				  hypervisorName,
				  UNIX_TIMESTAMP(lastHostUpdateTimeFx) as lastHostUpdateTimeFx_Unix,
				  UNIX_TIMESTAMP(lastHostUpdateTimePs) as lastHostUpdateTimePs_Unix,
				  UNIX_TIMESTAMP(lastHostUpdateTimeApp) as lastHostUpdateTimeApp_Unix,
				  lastHostUpdateTime as lastHostUpdateTime_Unix,
				  outpost_version,
				  fr_version,
				  agentRole,
				  psVersion
			  FROM 
				  hosts 
			  WHERE
				   id <> '$INMAGE_PROFILER_HOST_ID' and type <> 'Switch'
			  GROUP BY
				  id
				ORDER BY name";
        $query = $this->conn->sql_query($sql);
        while( $row = $this->conn->sql_fetch_row( $query ) ) 
        {
			
            $entries = array();
            $entries['hostname'] = $row[1];
            $entries['ip'] = $row[3];
            $entries['sentinel'] = '';
            $entries['outpost'] = '';
            $entries['filerep'] = '';
	
			$hyper_version   = $row[10];
			$sentinelEnabled = $row[5];
			if($sentinelEnabled == 0)
			{
				$vm_name = "Unknown";
				if($row[21])
				{
					$vm_name = "Physical";
				}
			}
			else
			{
				if($hyper_version == 0)
				{
					$vm_name = "Unknown"; 
				}
				else if($hyper_version == 1)
				{	
					$vm_name = "Physical"; 
				}
				else
				{
					$vm_name = "Virtual";
				}
			}
			$hyper_version_name = $row[11];

			$entries['vm'] = $vm_name;
            if($row[5] == 1)
            {
                $entries['sentinel'] = $row[2];
				$entries['sentinel_ts'] = $row[15];
            }
            if($row[6] == 1)
            {
                $entries['outpost'] = $row[2];
				$entries['outpost_ts'] = $row[15];
            }
            if($row[7] == 1)
            {
                $entries['filerep'] = $row[4];
				$entries['filerep_ts'] = $row[12];
            }
			if($row[8])
            {
                $entries['ps_heartbeat'] = $row[8];
				$entries['ps_heartbeat_ts'] = $row[13]; 
            }
			if($row[9])
            {
                $entries['app_heartbeat'] = $row[9];
				$entries['app_heartbeat_ts'] = $row[14];
            }
			if($row[16])
			{
				$entries['outpost_version'] = $row[16];
			}
			if($row[17])
			{
				$entries['fr_version'] = $row[17];
			}
			$entries['agent_role'] = 'Unknown';
			if($row[18])
			{
				$entries['agent_role'] = $row[18];
			}
			if($row[19])
			{
				$entries['ps_version'] = $row[19];
			}				
            $result[$row[0]] = $entries;
        }
        return( $result );
    } 

    /*
    * Function Name: remove_dir
    *
    * Description:
    *    Remove the directory
    *
    * Parameters:
    *     Param 1 [IN]: Directory
    *
    * Return Value:
    * 
    * Exceptions:
    *     none
    */
    private function remove_dir($dir)
    {
        if(is_dir($dir))
        {
            $dir = (substr($dir, -1) != "/")? $dir."/":$dir;
            $openDir = opendir($dir);
            while($file = readdir($openDir))
            {
                if(!in_array($file, array(".", "..")))
                {
                    if(!is_dir($dir.$file))
                        @unlink($dir.$file);
                    else
                        remove_dir($dir.$file);
                }
            }
            closedir($openDir);
            @rmdir($dir);
        }
    } 
	
	public function ListHosts($condition,$flag=0)
    {
		$select_value = "";
		if(!$flag)
		$select_value = ",systemId";
		$sql = "SELECT
					name,
					id,
					ipAddress $select_value
				FROM 
					hosts	
				WHERE 
					vxAgentPath IS NOT NULL
				AND
					$condition";
		$query = $this->conn->sql_query($sql);

        while( $row = $this->conn->sql_fetch_object( $query ) ) 
        {
			$entries['HostIP'] = $row->ipAddress;
			$entries['HostName'] = $row->name;
			$entries['HostAgentGUID'] = $row->id;
			if(!$flag)
			$entries['SystemGUID'] = $row->systemId;
            $result[] = $entries;
        }

        return $result;
	}
	
	public function ListHostInfo($host_id,$informationType)
	{
		global $REPLICATION_ROOT, $ROOT_ON_MULTIPLE_DISKS_SUPPORT_VERSION;
		$cluster_node_mbr_arr = array();
		$nicinfo_array = array();
		$cpu_sql = "SELECT
							count(*) as cpuCount,
							architecture
						FROM
							cpuInfo
						WHERE						
							hostId = '$host_id'";
		$cpuinfo_result = $this->conn->sql_query($cpu_sql);
		$cpu_info = $this->conn->sql_fetch_object($cpuinfo_result);
		$cpu_count = $cpu_info->cpuCount;
		$cpu_architecture = $cpu_info->architecture;
		
		$cores_sql = "SELECT 
						max(noOfCores) as noOfCores 
					FROM 
						CPUInfo 
					WHERE 
						hostId = '$host_id'";
		
		$coreinfo_result = $this->conn->sql_query($cores_sql);
		$cores_info = $this->conn->sql_fetch_object($coreinfo_result);
		$no_of_cores = $cores_info->noOfCores;

		$host_sql = "SELECT
					id,
					name,
					memory,
					hypervisorState,
					osFlag,
					osCaption,
					systemType,
					PatchHistoryVX,
					prod_version,
					vxAgentPath,
					compatibilityNo,
					FirmwareType 			
					FROM
					hosts
				WHERE 
					id = '$host_id'";
		$result_set = $this->conn->sql_query($host_sql);
		while($data = $this->conn->sql_fetch_object($result_set))
		{
			if($data->hypervisorState == 1)
				$machineType = "PhysicalMachine";
			elseif($data->hypervisorState == 2)
				$machineType = "VirtualMachine";
				
			if($data->osFlag == 1)
				$osType = "Windows";
			elseif($data->osFlag == 2)
				$osType = "Linux";
			elseif($data->osFlag == 3)
				$osType = "Solaris";
			$hostName = $data->name;
			$arr['HostGUID'] = $data->id;
			$arr['HostName'] = $hostName;
			$arr['MemSize'] = $data->memory;
			$arr['MachineType'] = $machineType;
			$arr['CPUCount'] = $cpu_count;
			$arr['NUMCores'] = $no_of_cores;	
			$arr['CPUArchitecture'] = $cpu_architecture;	
			$arr['OsType'] = $osType;
			$arr['Caption'] = str_replace("\"","&quot;",$data->osCaption);
			$arr['SystemType'] = $data->systemType;
            $arr['VxAgentPath'] = $data->vxAgentPath;
			$patchHistoryVx = $data->PatchHistoryVX;
			$patchHistoryArray = explode("|",$patchHistoryVx);
			$latest_update = $patchHistoryArray[count($patchHistoryArray)-1];
			$update_array = explode(",",$latest_update);
			$build_name = $update_array[0];
			$build_array = explode("_",$build_name);
			if($build_array[2])			
			$arr['inmage_version'] = $build_array[2];
			else
			$arr['inmage_version'] = $data->prod_version;
			$compatibility_no = $data->compatibilityNo;
			$arr['FirmwareType'] = $data->FirmwareType;
		}
		$host_ids[] = $host_id;
		list($disk_group_struct) =  $this->get_disk_volume_count($host_ids);
		$system_obj = new System();
		$disk_groups_data = $system_obj->getDiskGroupsData($host_ids);
		$vendor_info = $this->get_vendor_info();
		$disk_sql = "SELECT
						deviceName,
						capacity,
						formatLabel,
						deviceProperties,
						volumeType,
						fileSystemType,
						mountPoint,
						Phy_Lunid,
						vendorOrigin,
						systemVolume  
					FROM 
						logicalVolumes 
					WHERE
						hostId = '$host_id' 
					AND
						volumeType = 0
					AND
						isImpersonate = 0 "; 
						
		$diskinfo_result = $this->conn->sql_query($disk_sql);		
		
		$arr['NumberOfDisks'] = $this->conn->sql_num_row($diskinfo_result);
		$mbrPath = $cluster_nodes_mbr_arr[] = $this->GetHostMBRPath($host_id);
		
		
		$cluster_sql = "SELECT
							clusterName,
							clusterId							
						FROM
							clusters
						WHERE
							hostId = '$host_id'";
		$clusterinfo_result = $this->conn->sql_query($cluster_sql);
		$cluster_count = $this->conn->sql_num_row($clusterinfo_result);
		$cluster_data = $this->conn->sql_fetch_object($clusterinfo_result);
		
		if($cluster_count)
		{			
			$arr['Cluster'] = "true";
			$arr['ClusterName'] = $cluster_data->clusterName;
			$node_select_sql = "SELECT
									distinct hostId
								FROM
									clusters
								WHERE
									hostId != '$host_id'
								AND
									clusterId = '".$cluster_data->clusterId."'";
			$node_result = $this->conn->sql_query($node_select_sql);
			$cluster_node_id = $host_id;
			while($node_info = $this->conn->sql_fetch_object($node_result))
			{
				$cluster_nodes_mbr_arr[] = $this->GetHostMBRPath($node_info->hostId);				
				$cluster_node_id = $cluster_node_id.",".$node_info->hostId;
				$hostName = $hostName.",".$this->commonfunctions_obj->getHostName($node_info->hostId);
			}
			$arr['clusternodesmbrfiles'] = implode(",",$cluster_nodes_mbr_arr); 
			$arr['ClusterNodesInmageuuid'] = $cluster_node_id;
			$arr['ClusterNodes'] = $hostName;
		}
		else
		{
			$arr['Cluster'] = "false";
		}
	
		$arr['MBRPath'] = $mbrPath;
		
			
		if($informationType == "ALL" || $informationType == "STORAGE")
		{
			$system_disk_set = 0;
			$i = 0;
			while($disk_data = $this->conn->sql_fetch_object($diskinfo_result))
			{	
				$formatLabel = $disk_data->formatLabel;
				$diskLayout = array(0=>"LABEL_UNKNOWN" , 1=>"SMI" , 2=>"EFI" , 3=>"MBR" , 4=>"GPT" , 5=>"LABEL_RAW");
				$device_properties = array();
				if (isset($device_properties))
				{
					$device_properties = unserialize($disk_data->deviceProperties);
				}
				
				if(strpos(strtolower($device_properties['interface_type']), "usb") !== false)
					continue;				
				else if(strtolower(trim($device_properties['interface_type'])) == 'scsi')
					$IsIscsiDevice = "true";
				else
					$IsIscsiDevice = "false";
				
				
				$has_uefi = "false";
				if(array_key_exists('has_uefi',$device_properties)) {
					$has_uefi = "true";
				}
				
					
				$isDiskShared = "false";
				if(array_key_exists('is_shared',$device_properties)) {
					$isDiskShared = $device_properties['is_shared'];
				}
				
				$compareToDiskName = $disk_data->deviceName;
				if ($device_properties['display_name'])
				{
					$disk_name = $device_properties['display_name'];
				}
				else
				{
					$disk_name = $disk_data->deviceName;
				}
				
				if ($device_properties['disk_guid'])
				{
					$disk_guid = $device_properties['disk_guid'];
				}
				else
				{
					$disk_guid = $disk_data->deviceName;
				}
				
				$is_system_disk = "false";
				if ($osType == "Windows")
				{
					if (array_key_exists($host_id, $disk_groups_data))
					{
						if (array_key_exists($disk_guid, $disk_groups_data[$host_id]))
						{
							$disk_group_name_for_disk = $disk_groups_data[$host_id][$disk_guid][0];
							if(strtolower($device_properties['has_bootable_partition']) == "true" && in_array($disk_group_name_for_disk, $disk_group_struct[$host_id])) 
							$is_system_disk = "true";
						}
					}
				}
				else
				{
					if ($compatibility_no >= $ROOT_ON_MULTIPLE_DISKS_SUPPORT_VERSION)
					{
						if ($disk_data->systemVolume == "1")
						{
							$is_system_disk = "true";
						}
					}
					else
					if(strtolower($device_properties['has_bootable_partition']) == "true" && $disk_data->systemVolume == "1" && !$system_disk_set) 
					{
						$is_system_disk = "true";
						$system_disk_set = 1;
					}	
				}
							
				$partitions_sql = "SELECT 
									pt.partitionName,
									pt.capacity,
									lv.systemVolume,
									lv.fileSystemType,
									lv.mountPoint,
									lv.Phy_Lunid AS scsiId,
									lv.vendorOrigin AS vendor
								  FROM
									partitions pt,
									logicalVolumes lv
								  WHERE
									pt.diskId = '".$this->conn->sql_escape_string($compareToDiskName)."'
								  AND
									pt.hostId = '$host_id'
								  AND
									pt.partitionName = lv.deviceName
								  AND 
									pt.hostId = lv.hostId";
				$result_partitions = $this->conn->sql_query($partitions_sql);	
				$partition_count = $this->conn->sql_num_row($result_partitions);
				$partition = array();
				$diskGroupArr = array();
				
				if($partition_count)
				{
					while($partition_object = $this->conn->sql_fetch_object($result_partitions))
					{
						$systemValue = ($partition_object->systemVolume == 0)?"false":"true";
						$partitionName = $partition_object->partitionName;						
						$logicalVolumes = $this->getLogicalVolumeInfo($host_id,$partitionName);
						$partition[$partitionName] = array("Name" => $partition_object->partitionName , 'DiskGroup'=>$logicalVolumes["diskGroup"], 'Size' =>$partition_object->capacity , 'IsSystemVolume' => $systemValue,'MountPoint'=>$partition_object->mountPoint,'FileSystemType' => $partition_object->fileSystemType,'ScsiId'=>$partition_object->scsiId,'VendorInfo'=>$vendor_info[$partition_object->vendor]);
						if($logicalVolumes["logicalVolume"])
						$partition[$partitionName]['logicalVolume'] = $logicalVolumes["logicalVolume"];
						if($logicalVolumes["diskGroup"])
						$diskGroupArr[] = $logicalVolumes["diskGroup"];
					} 
				}
				else
				{		
					$logicalVolumes = $this->getLogicalVolumeInfo($host_id,$compareToDiskName);										
					if($logicalVolumes["logicalVolume"])
					$partition[$disk_name]['logicalVolume'] = $logicalVolumes["logicalVolume"];
					if($logicalVolumes["diskGroup"])
					$diskGroupArr[] = $logicalVolumes["diskGroup"];
				}
				
				$diskGroupArr = array_unique($diskGroupArr);
				$diskGroupName = implode(",",$diskGroupArr);
				if ($device_properties['is_resource_disk'] == "true")
				{
					$is_resource_disk = $device_properties['is_resource_disk'];
				}
				else
				{
					$is_resource_disk = "false";
				}
				$arr['disk'][] = array('DiskName'=>$disk_name , "DiskGroup"=>$diskGroupName , 'Size' =>$disk_data->capacity ,'FileSystemType' => $disk_data->fileSystemType,'MountPoint'=>$disk_data->mountPoint ,'IdeOrScsi' =>$device_properties['interface_type'] , 'ScsiPort' =>$device_properties['scsi_port'] , 'ScsiTargetId'=>$device_properties['scsi_target_id'],'ScsiBusNumber'=>$device_properties['scsi_bus'] , 'ScsiLogicalUnit'=>$device_properties['scsi_logical_unit'] ,'IsResourceDisk'=>$is_resource_disk, 'SystemDisk'=>$is_system_disk, 'IsIscsiDevice' =>$IsIscsiDevice, 'DiskType'=>$device_properties['storage_type'] , 'DiskLayout'=>$diskLayout[$formatLabel], 'Heads'=>$device_properties['total_heads'],'Sectors'=>$device_properties['total_sectors'],'SectorsPerTrack'=>$device_properties['sectors_per_track'], 'Bootable'=>$device_properties['has_bootable_partition'],'ScsiControllerModel'=>trim($device_properties['model']),'MediaType'=>$device_properties['media_type'],'DiskGuid'=>$disk_guid,'DriverVersion'=>"",'efi'=>$has_uefi,'ScsiId'=>$disk_data->Phy_Lunid,'VendorInfo'=>$vendor_info[$disk_data->vendorOrigin],"IsShared"=>$isDiskShared,"partition"=>$partition);					
			}
		}
		
		if($informationType == "ALL" || $informationType == "NETWORK")
		{
			if($arr['OsType'] == 'Windows') {
				$vendor = array();
				$check_ms_nic = "SELECT DISTINCT manufacturer,macAddress FROM hostNetworkAddress WHERE hostId = '$host_id' AND 				manufacturer != 'Microsoft'
								 UNION
								 SELECT DISTINCT manufacturer,macAddress FROM hostNetworkAddress WHERE hostId = '$host_id' AND manufacturer='Microsoft' AND macAddress NOT IN (SELECT DISTINCT macAddress FROM hostNetworkAddress WHERE hostId = '$host_id' AND manufacturer!='Microsoft')";
				$check_ms_nic_rs = $this->conn->sql_query($check_ms_nic);
				$num_rows = $this->conn->sql_num_row($check_ms_nic_rs);
				while($check_ms_nic_row = $this->conn->sql_fetch_object($check_ms_nic_rs)) {
					$vendor[$check_ms_nic_row->macAddress] = $check_ms_nic_row->manufacturer;
				}
			}
			
			$ip_type_list = array();		
			$sql = "SELECT id,type from ipType";
			$rs = $this->conn->sql_query($sql); 
			while($row = $this->conn->sql_fetch_object($rs))
			{
				$ip_type_list[$row->id] = $row->type;
			}
			
			$nicinfo_sql = "SELECT
							    deviceName,
								macAddress,
								gateway,
								dnsServerAddresses,
								isDhcpEnabled,
								manufacturer,							
								ipAddress,
								subnetMask,
								ipType
							FROM
								hostNetworkAddress
							WHERE
								hostId = '$host_id'";
			$nicinfo_result = $this->conn->sql_query($nicinfo_sql);
			while($nic_info = $this->conn->sql_fetch_object($nicinfo_result))
			{
				$nicinfo_array[] = $nic_info;
			}
			
			foreach($nicinfo_array as $key => $value)
			{
				$macAddress = $value->macAddress;
				if($arr['OsType'] == 'Windows') {
					$arr['nicinfo'][$macAddress][0] = $vendor[$macAddress];
				}
				else {
					$arr['nicinfo'][$macAddress][0] = $value->manufacturer;
				}
				$isDhcp = ($value->isDhcpEnabled == 1)?'true':'false';	
				if($value->ipType)
				{
					$arr['nicinfo'][$macAddress][] = array('Ip' => $value->ipAddress,'SubnetMask' => $value->subnetMask , 'GateWay' => $value->gateway , 'DnsIp' => $value->dnsServerAddresses, 'IsDhcp' => $isDhcp, 'DeviceName' => $value->deviceName, 'IpType' => $ip_type_list[$value->ipType]);
				}
				else
				{
					$arr['nicinfo'][$macAddress][] = array('Ip' => $value->ipAddress,'SubnetMask' => $value->subnetMask , 'GateWay' => $value->gateway , 'DnsIp' => $value->dnsServerAddresses, 'IsDhcp' => $isDhcp, 'DeviceName' => $value->deviceName);		
				}
			}
		}
	
		$result[] = $arr;
		return $result;
	}
	
	public function getAllPartitions($host_ids)
	{
		$sql_args = array();	
		$partitions = array();
		$cond_hosts = "";
		if($host_ids)
		{
			$host_ids_str = implode(",", $host_ids);
			$cond_hosts .= " AND FIND_IN_SET(lv.hostId, ?)";
			$sql_args[] = $host_ids_str;
		}
	/*
	mysql> explain SELECT
    ->         pt.hostId,
    ->         pt.diskId,
    ->         pt.partitionName,
    ->         pt.capacity,
    ->         lv.systemVolume,
    ->         lv.fileSystemType,
    ->         lv.mountPoint
    ->          FROM
    ->         partitions pt,
    ->         logicalVolumes lv
    ->          WHERE
    ->         pt.hostId = lv.hostId\G;
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: pt
         type: ALL
possible_keys: hostId_indx
          key: NULL
      key_len: NULL
          ref: NULL
         rows: 133
        Extra:
*************************** 2. row ***************************
           id: 1
  select_type: SIMPLE
        table: lv
         type: ref
possible_keys: hostId
          key: hostId
      key_len: 162
          ref: svsdb1.pt.hostId
         rows: 13
        Extra:
2 rows in set (0.00 sec)
*/
		$partitions_sql = "SELECT
								pt.hostId,
								pt.diskId,
								pt.partitionName,
								pt.capacity,
								lv.systemVolume,
								lv.fileSystemType,
								lv.mountPoint
							  FROM 
								partitions pt,
								logicalVolumes lv 
							  WHERE 
								pt.partitionName = lv.deviceName 
							  AND 
								pt.hostId = lv.hostId ".$cond_hosts;
		
#print 	$partitions_sql;
#print_r($sql_args);

		$result_partitions = $this->conn->sql_query($partitions_sql,$sql_args);
#print_r($result_partitions);
#exit;
		
		$partition_count = count($result_partitions);
		#echo "partition count:".$partition_count;
	
		if($partition_count)
		{	
				foreach ($result_partitions as $key=>$value)
				{
					$partitions[$value['hostId']][$value['diskId']][]= array("Name" => $value['partitionName'], 'Size' =>$value['capacity'] ,'systemVolume'=>$value['systemVolume'],'MountPoint'=>$value['mountPoint'],'FileSystemType' => $value['fileSystemType']);
				}
		}
		return $partitions;			
	}
	
	public function getAllRelatedStorageDevices($host_id,$disk_name,$partitions,$dg_data)
	{
		$partition = array();
		$diskGroupArr = array();
		$arr = array();
		if ($partitions[$host_id][$disk_name])
		{
			
			if (array_key_exists($disk_name, $partitions[$host_id]))
			{
				
				$get_partitions_data = $partitions[$host_id][$disk_name];
				
				foreach ($get_partitions_data as $pkey=>$pvalue)
				{
					$systemValue = ($pvalue['systemVolume'] == 0)?"false":"true";
					$partitionName = $pvalue['Name'];
					$logicalVolumes = $this->getLogicalVolumeData($host_id,$partitionName,$dg_data);
					$partition[$partitionName] = array("Name" => $pvalue['Name'] , 'DiskGroup'=>$logicalVolumes["diskGroup"], 'Size' =>$pvalue['Size'] , 'IsSystemVolume' =>$systemValue,'MountPoint'=>$pvalue['MountPoint'],'FileSystemType' => $pvalue['FileSystemType']);
					if($logicalVolumes["logicalVolume"])
					$partition[$partitionName]['logicalVolume'] = $logicalVolumes["logicalVolume"];
					if($logicalVolumes["diskGroup"])
					$diskGroupArr[] = $logicalVolumes["diskGroup"];
				}
			}
		}
		else
		{
			#print "$host_id,$disk_name";
			#exit;
			$logicalVolumes = $this->getLogicalVolumeData($host_id,$disk_name,$dg_data);
			if($logicalVolumes["logicalVolume"])
			$partition[$disk_name]['logicalVolume'] = $logicalVolumes["logicalVolume"];
			if($logicalVolumes["diskGroup"])
			$diskGroupArr[] = $logicalVolumes["diskGroup"];
		}
			$diskGroupArr = array_unique($diskGroupArr);
			$diskGroupName = implode(",",$diskGroupArr);
			$arr = array("DiskGroup"=>$diskGroupName ,"partition"=>$partition);
		return $arr;
	}
	
	public function getDiskGroupsData($host_ids)
	{
		$sql_args = array();
		$dg_data = array();
		$cond_hosts = "";
		if($host_ids)
		{
			$host_ids_str = implode(",", $host_ids);
			$cond_hosts .= " AND FIND_IN_SET(h.id, ?)";
			$sql_args[] = $host_ids_str;
		}
	/*
	Query explain plan
		mysql> explain select diskGroupName from diskGroups dg, hosts h where h.id = dg.
hostId and h.sentinelEnabled=1\G;
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: h
         type: ref
possible_keys: PRIMARY,hosts_sentinelEnabled
          key: hosts_sentinelEnabled
      key_len: 1
          ref: const
         rows: 249
        Extra: Using index
*************************** 2. row ***************************
           id: 1
  select_type: SIMPLE
        table: dg
         type: ref
possible_keys: PRIMARY
          key: PRIMARY
      key_len: 162
          ref: svsdb1.h.id
         rows: 4
        Extra: Using index
2 rows in set (0.00 sec)

ERROR:
No query specified

mysql> explain select diskGroupName from diskGroups dg, hosts h where h.id = dg.
hostId and h.sentinelEnabled=1 and FIND_IN_SET(h.id, "9F755DE0-AD01-9841-B6EB63A
A5C269F5881,DE9FFF98-EE2A-AB47-81BFB419946E3CCD")\G;
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: h
         type: ref
possible_keys: PRIMARY,hosts_sentinelEnabled
          key: hosts_sentinelEnabled
      key_len: 1
          ref: const
         rows: 249
        Extra: Using where; Using index
*************************** 2. row ***************************
           id: 1
  select_type: SIMPLE
        table: dg
         type: ref
possible_keys: PRIMARY
          key: PRIMARY
      key_len: 162
          ref: svsdb1.h.id
         rows: 4
        Extra: Using index
2 rows in set (0.00 sec)

ERROR:
No query specified
*/
		$diskgroup_sql = "select 
								dg.hostId,
								dg.deviceName,
								dg.diskGroupName 
							from 
								diskGroups dg, 
								hosts h 
							where 
								h.id = dg.hostId 
							and 
								h.sentinelEnabled=1 ".$cond_hosts;
	
		$result_diskgroup = $this->conn->sql_query($diskgroup_sql,$sql_args);	
		
		if(count($result_diskgroup))
		{	
			foreach($result_diskgroup as $key=>$value)
			{
				$dg_data[$value['hostId']][$value['deviceName']][] = $value['diskGroupName'];	
			}
		}
		return $dg_data;
	}
	
	private function getLogicalVolumeData($hostId,$deviceName,$dg_data)
	{	
		$logicalVolumes = array();
		
		if ($dg_data[$hostId][$deviceName])
		{
			#print_r($dg_data[$hostId][$deviceName]);
			#exit;
			foreach ($dg_data[$hostId][$deviceName] as $key=>$value)
			{
				$diskGroup = $value;
				$lv_sql = "SELECT
								lv.deviceName,
								lv.capacity,
								lv.systemVolume,
								lv.fileSystemType,
								lv.mountPoint,
								lv.Phy_Lunid AS scsiId,
								lv.vendorOrigin AS vendor,
								lv.volumeLabel 
							FROM
								diskGroups dg,
								logicalVolumes lv
							WHERE
								dg.diskGroupName = '".$this->conn->sql_escape_string($diskGroup)."'
							  AND 
								 dg.deviceName = lv.deviceName
							  AND
								lv.volumeType = 5
							  AND
								lv.hostId = '$hostId'
							  AND 
								lv.hostId = dg.hostId";
				$result_lv = $this->conn->sql_query($lv_sql);
			
				while($lv_object = $this->conn->sql_fetch_object($result_lv))
				{
					$isSystemValue = ($lv_object->systemVolume == 0)?"false":"true";
					$logicalVolumes[] = array("Name" => $lv_object->deviceName , 'DiskGroup'=>$diskGroup, 'Size' =>$lv_object->capacity , 'IsSystemVolume' => $isSystemValue,"FileSystemType" => $lv_object->fileSystemType,'MountPoint'=>$lv_object->mountPoint,'VolumeLabel'=>$lv_object->volumeLabel);
				} 
				$result = array("diskGroup"=>$diskGroup , "logicalVolume"=>$logicalVolumes);
				return $result;
			} 
		}
	}
	
	
	private function getLogicalVolumeInfo($hostId,$deviceName)
	{
		$vendor_info = $this->get_vendor_info();
		$diskgroup_sql = "SELECT
										diskGroupName
									  FROM
										diskGroups
									  WHERE
										deviceName = '".$this->conn->sql_escape_string($deviceName)."'
										AND
											hostId = '$hostId'";
		
		$result_diskgroup = $this->conn->sql_query($diskgroup_sql);	
		$diskgroup_count = $this->conn->sql_num_row($result_diskgroup);
		$result = array();
		$logicalVolumes = array();
		
		if($diskgroup_count)
		{
			while($diskgroup_object = $this->conn->sql_fetch_object($result_diskgroup))
			{
				$diskGroup = $diskgroup_object->diskGroupName;
				$lv_sql = "SELECT
								lv.deviceName,
								lv.capacity,
								lv.systemVolume,
								lv.fileSystemType,
								lv.mountPoint,
								lv.Phy_Lunid AS scsiId,
								lv.vendorOrigin AS vendor,
								lv.volumeLabel 
							FROM
								diskGroups dg,
								logicalVolumes lv
							WHERE
								dg.diskGroupName = '".$this->conn->sql_escape_string($diskGroup)."'
							  AND 
								 dg.deviceName = lv.deviceName
							  AND
								lv.volumeType = 5
							  AND
								lv.hostId = '$hostId'
							  AND 
								lv.hostId = dg.hostId";
				$result_lv = $this->conn->sql_query($lv_sql);
			
				while($lv_object = $this->conn->sql_fetch_object($result_lv))
				{
					$isSystemValue = ($lv_object->systemVolume == 0)?"false":"true";
					$logicalVolumes[] = array("Name" => $lv_object->deviceName , 'DiskGroup'=>$diskGroup, 'Size' =>$lv_object->capacity , 'IsSystemVolume' => $isSystemValue,"FileSystemType" => $lv_object->fileSystemType,'MountPoint'=>$lv_object->mountPoint,'ScsiId'=>$lv_object->scsiId,'VendorInfo'=>$vendor_info[$lv_object->vendor],'VolumeLabel'=>$lv_object->volumeLabel);
					//$partition[$deviceName]['logicalVolume'] = $logicalVolumes;
				} 
				$result = array("diskGroup"=>$diskGroup , "logicalVolume"=>$logicalVolumes);				
			} 
		}
		return $result;
	}
	public function GetHostMBRPath($host_id,$restore_point=0)
    {
		
		global $SLASH , $REPLICATION_ROOT;		
		$recovery_obj = new Recovery();
		$mbr_file = '';
		if (preg_match('/Windows/i', php_uname()))
		{
			$mbr_path = $REPLICATION_ROOT."admin\\web\\SourceMBR\\$host_id\\";
		}
		else
		{
			$mbr_path = $REPLICATION_ROOT."admin/web/SourceMBR/$host_id/";
		}

		$links = array();
		$file_ts = array();
		$hash = array();
		if(file_exists($mbr_path))
		{
			
			$sql = "SELECT
						latestMBRFileName
					FROM
						hosts
					WHERE
						id='$host_id'";
			$rs = $this->conn->sql_query($sql);
			while($row = $this->conn->sql_fetch_object($rs)) {
				$mbr_file_name = $row->latestMBRFileName;
				if($mbr_file_name) {
					if(file_exists($mbr_path.$mbr_file_name)) {
						$mbr_file = $mbr_path.$mbr_file_name;
					}
				}
			}
		}
		return $mbr_file;
	}
	public function updateHostRefreshStatus($host_id,$requestApiId)
	{
		$update_refresh_status = "UPDATE
									hosts
								  SET
									refreshStatus = '$requestApiId'
								  WHERE
									id = '$host_id'";
		$update_status = $this->conn->sql_query($update_refresh_status);
		if(!$update_status)
		{
			throw new Exception('updating refreshStatus in host table');
		}
	}
	
	public function getRequestedHostInfo($request_id)
	{
		$request_query = "SELECT
							requestdata
						FROM
							apiRequestDetail
						WHERE
							apiRequestId = $request_id";
		$query_status = $this->conn->sql_query($request_query);
		$request_object =  $this->conn->sql_fetch_object($query_status);		
		$requestdata = unserialize($request_object->requestdata);
		return array("requestdata"=>$requestdata);
	}
	
	public function get_passwd ($user_name)
    {
        $user_name = $this->conn->sql_escape_string($user_name);
		$sql = "SELECT
					passwd
				FROM
					users
				WHERE
					uname = ?";
		$result = $this->conn->sql_query($sql, array($user_name));
		if(!count($result)) {
			return 0;
		}
		return $result[0]['passwd'];
    }
	
	function get_vendor_info()
	{
		$result = array();
		$sql = "SELECT
					type,
					name
				FROM
					vendorInfo";
		$rs = $this->conn->sql_query($sql);
		while($row = $this->conn->sql_fetch_object($rs)) {
			$result[$row->type] = $row->name;
		}
		return $result;
	}
	
    function getScoutComponents($component_type=NULL, $os_type=NULL, $host_ids=NULL)
	{
		$host_cond = "";
		if($component_type)
		{
			$host_cond .= ($component_type == "ProcessServers") ? " AND processServerEnabled = '1'" : "";
			$host_cond .= ($component_type == "MasterTargets") ? " AND (outpostAgentEnabled = '1' OR sentinelEnabled = '1') AND agentRole = 'MasterTarget'" : "";
			$host_cond .= ($component_type == "SourceServers") ? "AND (outpostAgentEnabled = '1' OR sentinelEnabled = '1') AND agentRole != 'MasterTarget'" : "";
		}
		else
		{
			$host_cond .= " AND (h.processServerEnabled = '1' OR h.csEnabled = '1' OR h.outpostAgentEnabled = '1' OR h.sentinelEnabled = '1')";			
		}
		
		if($os_type)
		{
			$host_cond .= ($os_type == "Windows")  ? " AND osFlag = '1'" : "";
			$host_cond .= ($os_type == "Linux")  ? " AND osFlag != '1'" : "";
		}
		if($host_ids && is_array($host_ids))
		{
			$host_ids_str = implode(",", $host_ids);
			$host_arg_cond .= " AND FIND_IN_SET(id, ?)";
		}
		
		$get_hosts_sql = "SELECT
					DISTINCT
					h.id as hostId, 
					h.name as hostName, 
					h.ipaddress as ipAddress, 
					h.csEnabled as isCs,
					h.processServerEnabled as isPs,
					h.agentRole,					
					h.lastHostUpdateTime as lastUpdateTimeVx,
					UNIX_TIMESTAMP(h.lastHostUpdateTimeFx) as lastUpdateTimeFx,
					UNIX_TIMESTAMP(h.lastHostUpdateTimeApp) as lastUpdateTimeApp,
					UNIX_TIMESTAMP(h.lastHostUpdateTimePs) as lastUpdateTimePs,
					h.osFlag as osType,
					h.osCaption as osName,
					h.outpost_version as vxVersion,
                    h.PatchHistoryVX as vxPatchVersion,	                    
					h.psVersion as psVersion,
                    h.psPatchVersion as psPatchVersion, 
                    h.patchInstallTime as patchInstallTime,
                    h.memory as memory,
                    h.hypervisorState,
                    h.hypervisorName,
					h.resourceId,
					iv.infrastructureVMId,
                    iv.hostUuid,
					iv.instanceUuid,
					iv.infrastructureHostId,
					iv.validationErrors,
					IF (ISNULL(iv.diskCount) OR iv.diskCount = '', 0, iv.diskCount) as diskCount,
					h.TargetDataPlane,
					h.systemDrive,
					h.systemDirectory,
					h.systemDriveDiskExtents,
					h.certExpiryDetails as certExpiryDetails,
					h.marsVersion,
					h.compatibilityNo,
					h.FirmwareType,
					h.involflt_version,
					h.AgentBiosId,
					h.AgentFqdn 
				FROM 
					hosts h				
				LEFT JOIN
					infrastructureVMs iv
				ON				
					iv.hostId = h.id
				WHERE 					
					h.id != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'
					$host_cond
					$host_arg_cond";
		$component_details = ($host_arg_cond) ? $this->conn->sql_query($get_hosts_sql, array($host_ids_str)) : $this->conn->sql_exec($get_hosts_sql);
		if(!is_array($component_details)) return 0;		
		return $component_details;
	}

	function getInventoryIdsOfInfrastructureHosts()
	{
		$inventory_ids = array();
		$get_inventory_sql = "SELECT
								infrastructureHostId,
								inventoryId
							FROM
								infrastructurehosts";
		$inventory_ids_result = $this->conn->sql_query($get_inventory_sql);
		while( $row = $this->conn->sql_fetch_object( $inventory_ids_result ) ) 
        {
			$inventory_ids[$row->infrastructureHostId] = $row->inventoryId;
			
        }
		return $inventory_ids;
	}
	
	function getDevices($host_ids=NULL, $get_retention = 0)
	{	
		$sql_args = array();
        $cond_hosts = "";
		if($host_ids)
        {
            $host_ids_str = implode(",", $host_ids);
            $cond_hosts .= " AND FIND_IN_SET(hostId, ?)";
			$sql_args[] = $host_ids_str;
        }
		$get_devices_sql = "SELECT
                                hostId,
                                deviceName,
                            (CASE WHEN systemVolume = 1 THEN 'Yes' ELSE 'No' END) as systemVolume,
                                mountPoint,
                                capacity,
                                freespace,
                                volumeType,
                                deviceId,
                            	farLineProtected,
								deviceProperties,
								fileSystemType
                            FROM
                                logicalVolumes 
                            WHERE
                                volumeType != '2' AND
                                deviceFlagInUse NOT IN ('1') "
                                .$cond_hosts;
        $devices = $this->conn->sql_query($get_devices_sql, $sql_args);
		if(!is_array($devices)) return;
        $device_details = array();
		$retention_volumes = array();
        foreach($devices as $device)
        {
            $device_details[$device['hostId']][] = $device;
        }
		return $device_details;
	}						
  
	function getDisks($host_ids=NULL)
	{	
		$sql_args = array();
		$cond_hosts = "";
		if($host_ids)
		{
			$host_ids_str = implode(",", $host_ids);
			$cond_hosts .= " AND FIND_IN_SET(hostId, ?)";
			$sql_args[] = $host_ids_str;
		}
		$get_devices_sql = "SELECT
								hostId,
								deviceName,
							(CASE WHEN systemVolume = 1 THEN 'Yes' ELSE 'No' END) as systemVolume,
								mountPoint,
								capacity,
								freespace,
								volumeType,
								deviceId,
								farLineProtected,
								deviceProperties,
								fileSystemType
							FROM
								logicalVolumes 
							WHERE
								volumeType = '0' AND
								deviceFlagInUse NOT IN ('1') AND 
								isImpersonate = 0 "
								.$cond_hosts;
		$devices = $this->conn->sql_query($get_devices_sql, $sql_args);
		if(!is_array($devices)) return;
		$device_details = array();
		$retention_volumes = array();
		foreach($devices as $device)
		{
			$device_details[$device['hostId']][] = $device;
		}
		return $device_details;
	}
	
	public function get_disk_volume_count($host_ids=NULL)
	{
		$sql_args = array();
        $cond_diskgrp_hosts = "1";
        $cond_hosts = "";
		if(is_array($host_ids))
        {
            $host_ids_str = implode(",", $host_ids);
            $cond_hosts = " AND FIND_IN_SET(dg.hostId, ?)";
            $cond_diskgrp_hosts = " FIND_IN_SET(hostId, ?)";
			$sql_args[] = $host_ids_str;
        }
		$diskgroup_sql = "SELECT 
							dg.diskGroupName as diskGroupName, 
							dg.deviceName as diskName, 
							dg.hostId as hostId ,
							lv.systemVolume as systemVolume,
							lv.volumeType as volumeType
						FROM 
							diskGroups dg, 
							logicalVolumes lv 
						WHERE 
							lv. volumeType IN  (0 ,5)
						AND 
							lv.hostId = dg.hostId 
						AND 
							lv.deviceName = dg.deviceName 
						$cond_hosts";
							
		$diskgroup_details = $this->conn->sql_query($diskgroup_sql, $sql_args);	
		$disk_info = $lv_info = $system_disks = array();
		if(!count($diskgroup_details)) return $disk_info;
		
		foreach($diskgroup_details as $disk_key => $disk_data)
		{
			if($disk_data['systemVolume'] == 1)
			{
				$system_disks[$disk_data['hostId']][] = $disk_data['diskGroupName'];
				$system_disks[$disk_data['hostId']] = array_unique($system_disks[$disk_data['hostId']]);
			}			
		}
		return array($system_disks);
	}
	
    function getHardwareConfiguration($host_ids=NULL)
    {
        $cond_hosts = "";
		$sql_args = array();
		if($host_ids)
        {
            $host_ids_str = implode(",", $host_ids);
            $cond_hosts .= " WHERE FIND_IN_SET(hostId, ?)";
			$sql_args[] = $host_ids_str;
        }
        $get_hardware_sql = "SELECT
                                hostId,
                                noOfCores
                             FROM   
                                cpuInfo
                             ".$cond_hosts;
        $cpus = $this->conn->sql_query($get_hardware_sql, $sql_args);
		if(!is_array($cpus)) return;
        $cpu_details = array();
        foreach($cpus as $cpu)
        {
            $cpuCount = $cpu['noOfCores'];
			$cpu_details[$cpu['hostId']] += $cpuCount;
        }
        
        return $cpu_details;
    }
    
    function getNetworkConfiguration($host_ids=NULL)
    {
        $cond_hosts = "";
		if($host_ids)
        {
            $host_ids_str = implode(",", $host_ids);
            $cond_hosts .= " WHERE FIND_IN_SET(hostId, ?)";
        }
        $get_network_sql = "SELECT
                                hostId,
                                deviceName,
                                macAddress,
                                ipAddress,                                
                                subnetMask,
                                gateway,
                                dnsServerAddresses,
                                isDhcpEnabled,
                                ipType
                             FROM   
                                hostNetworkAddress
							 ".$cond_hosts;
        $nics = ($cond_hosts) ? $this->conn->sql_query($get_network_sql, array($host_ids_str)) : $this->conn->sql_exec($get_network_sql);
		if(!is_array($nics)) return;
        $network_details = array();
        foreach($nics as $nic)
        {
            if(preg_match('/VPN\s+Adapter/', $nic['deviceName'])) continue;
			# For the IPV6 additions
            if(filter_var($nic['ipAddress'], FILTER_VALIDATE_IP, FILTER_FLAG_IPV6)) $network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['IPV6Address'] = $nic['ipAddress']; 
			else
			{
				$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['hostId'] = $nic['hostId'];
				if(filter_var($nic['ipAddress'], FILTER_VALIDATE_IP, FILTER_FLAG_IPV4))
				{
					$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['ipType'] = "1";
				}
				else
				{
					$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['ipType'] = $nic['ipType'];
				}
				$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['deviceName'] = $nic['deviceName'];
				$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['macAddress'] = $nic['macAddress'];
				$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['ipAddress'] = $nic['ipAddress'];
				$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['subnetMask'] = $nic['subnetMask'];
				$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['gateway'] = $nic['gateway'];
				$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['dnsServerAddresses'] = $nic['dnsServerAddresses'];
				$network_details[$nic['hostId']][$nic['macAddress']."_".$nic['deviceName']]['isDhcpEnabled'] = $nic['isDhcpEnabled'];
			}
        }
        return $network_details;
    }
    
    function getPsNatIps($host_ids=NULL)
    {
        $cond_hosts = "";
		if($host_ids)
        {
            $host_ids_str = implode(",", $host_ids);
            $cond_hosts .= " WHERE FIND_IN_SET(processServerId, ?)";
        }
        $get_nat_sql = "SELECT
                                processServerId,
                                ps_natip,
								psCertExpiryDetails 								
                             FROM   
                                processServer
                             ".$cond_hosts;
        $nats = ($cond_hosts) ? $this->conn->sql_query($get_nat_sql, array($host_ids_str)) : $this->conn->sql_exec($get_nat_sql);
		if(!is_array($nats)) return;
        $nat_details = array();
		
        foreach($nats as $nat)
        {
            $nat_details[$nat['processServerId']]['ps_natip'] = $nat['ps_natip'];
			$nat_details[$nat['processServerId']]['psCertExpiryDetails'] = $nat['psCertExpiryDetails'];
        }
       
        return $nat_details;
    }
	
	function updateInfraHostId($host_ip, $host_id, $resource_id = '')
	{
		global $logging_obj, $INFRASTRUCTURE_VM_ID_DEFAULT;		
		
		// Fetching original host id
		$org_host_id = $this->conn->sql_get_value("hosts", "id", "resourceId = ? and id != ? and (originalVMId = ? OR originalVMId = '')", array($resource_id, $host_id,$INFRASTRUCTURE_VM_ID_DEFAULT));
		$logging_obj->my_error_handler("INFO","configurator_register_host_static_info org_host_id::$org_host_id");
		if($org_host_id)
		{
			$infra_vm_id = $this->conn->sql_get_value("infrastructureVMs","infrastructureVMId","hostId = ?", array($org_host_id));
			$orig_infra_vm_id = $this->conn->sql_get_value("infrastructureVMs","infrastructureVMId","hostId = ?", array($host_id));
			$logging_obj->my_error_handler("INFO","configurator_register_host_static_info infra_vm_id::$infra_vm_id");
			
			// Updating the vmId for failed over VM in hosts table in order to send as part of GetProtectionDetails API in case of Failback
			if($infra_vm_id && !$orig_infra_vm_id)
			{	
				$update_vm_id_sql = "UPDATE hosts SET originalVMId = ? WHERE id = ? AND resourceId = ? AND agentRole = 'Agent' AND (originalVMId = ? OR originalVMId = '')";
				$sql_args = array($infra_vm_id, $host_id ,$resource_id, $INFRASTRUCTURE_VM_ID_DEFAULT);
				$this->conn->sql_query($update_vm_id_sql, $sql_args);
				$logging_obj->my_error_handler("DEBUG","configurator_register_host_static_info \n update_vm_id_sql  : $update_vm_id_sql\n : sql_args : ".print_r($sql_args,true));
			}
			elseif($orig_infra_vm_id)
			{
				$reset_vm_id_sql = "UPDATE hosts SET originalVMId = ? WHERE id = ? AND agentRole = 'Agent'";
				$reset_vm_id_sql_args = array($INFRASTRUCTURE_VM_ID_DEFAULT, $host_id);
				$this->conn->sql_query($reset_vm_id_sql, $reset_vm_id_sql_args);
				$logging_obj->my_error_handler("DEBUG","configurator_register_host_static_info \n reset_vm_id_sql  : $reset_vm_id_sql\n : reset_vm_id_sql_args : ".print_r($reset_vm_id_sql_args,true));
			}
		}
	}
    
    public function getRetentionDetails($hostIds=NULL)
    {
        $cond_hosts = "";
        $sql_args = array();
		if($hostIds)
        {
            $host_ids_str = implode(",", $hostIds);
            $cond_hosts .= " FIND_IN_SET(src.destinationHostId, ?) AND ";
			$sql_args[] = $host_ids_str;
        }
        $retention_data = array();
		$select_space_retention = "SELECT 
                                src.destinationHostId,
                                r.storageDeviceName,
                                r.alertThreshold
                            FROM 
                                retentionSpacePolicy r,                                
                                retLogPolicy ret                           
                             LEFT JOIN
                                srcLogicalVolumeDestLogicalVolume src
                            ON
                                src.ruleId = ret.ruleid
                            WHERE
                                ".$cond_hosts."                            
                                r.retId = ret.retId
								GROUP BY src.destinationDeviceName, r.storageDeviceName";
         
        $retention_data[] = $this->conn->sql_query($select_space_retention, $sql_args);  
        $select_event_retention = "SELECT 
                            src.destinationHostId,
                            r.storageDeviceName,
                            r.alertThreshold
                        FROM 
                            retentionEventPolicy r,                                     
                            retentionWindow rw,
                            retLogPolicy ret
                         LEFT JOIN
                            srcLogicalVolumeDestLogicalVolume src
                        ON
                            src.ruleId = ret.ruleid
                        WHERE
                            ".$cond_hosts."
                            rw.retId = ret.retId
                        AND
                            rw.Id = r.Id
						GROUP BY src.destinationDeviceName, r.storageDeviceName";
        $retention_data[] = $this->conn->sql_query($select_event_retention, $sql_args); 
	
        foreach($retention_data as $key => $retention_array)
        {
            if(is_array($retention_array))
            {
                foreach($retention_array as $k => $value)
                {
                    $result[$value['destinationHostId']."!!".$value['storageDeviceName']] = $value['alertThreshold'];
                }
            }
        }	
        return $result;
    }
	
	public function renewCertificate($hostId)
    {
        $ps_data = $policy_id = $sql_args = array();
		$policy_obj = new Policy();
		
		$select_ps = "SELECT
							processServerId
						FROM 
							processServer";
        $ps_data = $this->conn->sql_query($select_ps, $sql_args);  
		
		// policyType - 50 - RenewCertificate at CS
		// policyType - 51 - RenewCertificate at PS
        
		foreach($ps_data as $key => $ps_db_data)
        {
            $execution_log = array();
			$host_id = $ps_db_data["processServerId"];
			
			if($ps_db_data["processServerId"] == $hostId)
			{
				// insert CS renew certificate policy.
				$cs_policy_id = $policy_obj->insert_policy(
									$ps_db_data["processServerId"], 
									"50", 
									"2", 
									"0", 
									"0", 
									"0", 
									"0",
									"0", 
									"CSRenewCertificate");
				array_push($policy_id, $cs_policy_id);
			}
			
			// insert PS renew certificate policy.
			$ps_policy_id = $policy_obj->insert_policy(
								$ps_db_data["processServerId"], 
								"51", 
								"2", 
								"0", 
								"0", 
								"0", 
								"0",
								"0", 
								"PSRenewCertificate");
			
			array_push($policy_id, $ps_policy_id);
        }
        return $policy_id;
    }
	
	public function insertJobForAgent($host_id, $job_type="MTPrepareForAadMigrationJob")
	{
		global $activityId, $clientRequestId;
		$job_id = $this->agentHasJobs($host_id, $job_type);
		if(!$job_id) {
			$insert_job = "INSERT INTO agentJobs (hostId, jobType, jobCreationTime, activityId, clientRequestId)
							VALUES
							(?, ?, now(), ?, ?)";
			$job_id = $this->conn->sql_query($insert_job, array($host_id, $job_type, $activityId, $clientRequestId));
		}
		
		return $job_id;
	}
	
	public function agentHasJobs($host_id, $job_type=NULL) 
	{
		$sql_args = array("InProgress", $host_id);
		$job_type_condition = "";
		
		if($job_type) {
			$job_type_condition = "AND jobType = ?";
			$sql_args[] = $job_type;
		}

		$job_exists = $this->conn->sql_get_value("agentJobs", "jobId", "FIND_IN_SET(jobStatus, ?) AND hostId = ? $job_type_condition", $sql_args);
		
		return $job_exists;
	}
	
	public function getAgentJobStatus($job_ids) 
	{
		if(!$job_ids || !is_array($job_ids)) return array();
		
		$sql_args = array(implode(",", $job_ids));
		
		$get_jobs_details_sql = "SELECT jobId,
										hostId,
										jobStatus,
										errorDetails
								FROM
										agentJobs
								WHERE
										FIND_IN_SET(jobId, ?)";
		$job_details = $this->conn->sql_query($get_jobs_details_sql, $sql_args, "jobId");
		
		return $job_details;
	}
    
    public function getAgentJobs($host_id) 
	{
		$sql_args = array("InProgress", $host_id);
        
		$get_jobs_sql = "SELECT * FROM agentJobs WHERE FIND_IN_SET(jobStatus, ?) AND hostId = ?";
		$jobs = $this->conn->sql_query($get_jobs_sql, $sql_args);
        $agent_jobs = array();
        foreach($jobs as $job)
        {			
			// Context for the job
			$job_context = array(0);
            $job_context[] = $job['activityId'];
            $job_context[] = $job['clientRequestId'];
            
			// Error Messages
            $jobErrorDetails = ($job['errorDetails'] != NULL || $job['errorDetails'] != "") ? unserialize($job['errorDetails']) : "";
            $job_error = (is_array($jobErrorDetails)) ? $jobErrorDetails : array();
            
            $agent_job["Id"] = $job['jobId'];
            $agent_job["JobType"] = $job['jobType'];
            $agent_job["JobStatus"] = $job['jobStatus'];
            $agent_job["InputPayload"] = $job['inputPayload'];
            $agent_job["OutputPayload"] = ($job['outputPayload'] != NULL) ? $job['outputPayload'] : "";
            $agent_job["Errors"] = $job_error;
            $agent_job["Context"] = $job_context;
            
            $agent_jobs[] = $agent_job;
        }
		return $agent_jobs;
	}
	
	public function updateJobStatus($host_id, $agent_job)
	{
		global $logging_obj;
		$logging_obj->my_error_handler("INFO",array("HostId"=>$host_id,"AgentJobs"=>$agent_job),Log::BOTH);
        
		$update_job = "UPDATE agentJobs 
						SET
							jobStatus = ?,
							jobUpdateTime = now(),
							errorDetails = ?
						WHERE
							jobId = ?
						AND	hostId = ?";
		$sql_args = array($agent_job["JobStatus"], $agent_job['Errors'], $agent_job['Id'], $host_id);
		$logging_obj->my_error_handler("INFO",array("Sql"=>$update_job,"Arguments"=>$sql_args),Log::BOTH);
		$job_id = $this->conn->sql_query($update_job, $sql_args);
	}
	
	public function getMigrationJobDetails($job_id)
	{
        global $logging_obj;
        
        $get_request_sql = "SELECT apiRequestId FROM apiRequest WHERE requestType = 'MigrateACSToAAD' AND referenceId LIKE '%,".$job_id.",%' ORDER BY requestTime DESC LIMIT 0, 1";
        $request_details = $this->conn->sql_exec($get_request_sql);
        $apiRequestId = $request_details[0]['apiRequestId'];
        $apiRequestDetailId = $this->conn->sql_get_value("apiRequestDetail", "apiRequestDetailId", "apiRequestId = ?", array($apiRequestId));        
        
        return array("apiRequestId" => $apiRequestId, "apiRequestDetailId" => $apiRequestDetailId);
	}
	
	function updateHostIdInDiscovery($bios_id, $host_id, $nic_info, $ip_address_list, $host_name, $os_flag)
	{
		global $logging_obj, $CLOUD_PLAN_AZURE_SOLUTION;
		$mac_address = array();
		$ip_address_list = array();
		$bios_id_check = $log_data = "";
		
		$log_data = "updateHostIdInDiscovery bios_id::$bios_id, host_id::$host_id, host_name::$host_name, os_flag::$os_flag, nic_info::".print_r($nic_info, true);
		$logging_obj->my_error_handler("INFO",array("UpdateHostIdInDiscoveryNics"=>$nic_info),Log::BOTH);
		
		if(is_array($nic_info))
		{
			// Getting all the mac address list.
			foreach( $nic_info as $nicinfo_inner_array ) 
			{
				list(   
						$ver1,
						$deviceName,
						$ipAddress,
						$dnsHostName,
						$macAddress,
						$defaultIpGateways,
						$dnsServerAddresses,
						$index,
						$isDhcpEnabled,
						$manufacturer,
						$nicSpeed,
						$ipType
					) = $nicinfo_inner_array;
				
				if($os_flag == 1) 
				{
					if(preg_match('/Miniport/i',$deviceName)) 
					{
						continue;
					}
				}
				else if($os_flag == 2) 
				{
					if(preg_match('/^sit/i',$deviceName)) 
					{
						continue;
					}
				}
				
					
				if(!is_array($ipAddress)) 
				{
					$ipAddress_tmp = array();
					if($ipAddress)
					{ 
						$ipAddress_tmp[$ipAddress] = '';
					}
					else
					{
						//Incase if IPAddress is blank
						$ipAddress_tmp[0] = '';
					}
					$ipAddress = $ipAddress_tmp;
				}
				
				foreach($ipAddress as $ip=>$subnetMask) 
				{
					if ($ip) 
					{
						if (filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4))
						{
							$ip_address_list[] = trim($ip);
							$mac_address[] = trim($macAddress);
						}
					}
				}
			}
		}
		$mac_address = array_unique($mac_address);
		$mac_address = implode(",", $mac_address);
		$ip_address_list = array_unique($ip_address_list);
		$merge_ip_address = implode(",", $ip_address_list);
				
		if($bios_id)
		{
			$bios_id_check = " AND hostUuid = ? ";
		}
		
		$infra_sql = "SELECT
							infrastructureVMId,
							hostDetails,
							hostId,
							macAddressList,
							hostIp
						FROM
							infrastructureVMs
						WHERE
							macAddressList REGEXP REPLACE (?,',','|')
							$bios_id_check";
		if($bios_id)
		{						
			$infra_vm_data = $this->conn->sql_query($infra_sql, array($mac_address, $bios_id));
		}
		else
		{
			$infra_vm_data = $this->conn->sql_query($infra_sql, array($mac_address));
		}
		$log_data = $log_data." infra_sql::$infra_sql, mac_address::$mac_address, infra_vm_data::".print_r($infra_vm_data, true);
			
		$logging_obj->my_error_handler("INFO",array("InfraSql"=>$infra_sql,"MacAddress"=>$mac_address,"IpAddressList"=>$ip_address_list,"mergeip"=>$merge_ip_address,"InfraVmData"=>$infra_vm_data),Log::BOTH);
	
		// With bios id & mac address list combination if record exist update hostId always.		
		if($infra_vm_data)
		{
			$host_details_array = ($infra_vm_data[0]["hostDetails"]) ? unserialize($infra_vm_data[0]["hostDetails"]) : array();
			$infra_vm_host_id = $infra_vm_data[0]["hostId"];
			$vm_mac_address = $infra_vm_data[0]["macAddressList"];
			$vm_infra_id = $infra_vm_data[0]["infrastructureVMId"];
			$host_ip = $infra_vm_data[0]["hostIp"];
			$vmware_tools_installed = 1;
			$logging_obj->my_error_handler("INFO",array("ToolsStatus"=>$host_ip),Log::BOTH);
			
			if(!$host_ip)
			{
				$vmware_tools_installed = 0;
				
				// In case if the record does not exist then check with the list of ip addrress any record exist or not.
				$physical_infra_vm_id = $this->conn->sql_get_value("infrastructureVMs","infrastructureVMId", "FIND_IN_SET(hostIp, ?) AND hostDetails = '' AND discoveryType != ?", array($merge_ip_address, $CLOUD_PLAN_AZURE_SOLUTION));
				$log_data = $log_data." physical_infra_vm_id::$physical_infra_vm_id, vmware_tools_installed::$vmware_tools_installed";
				
				if($physical_infra_vm_id)
				{
					$physical_mac_address = $this->conn->sql_get_value("infrastructureVMs","macAddressList", "infrastructureVMId = ?", array($physical_infra_vm_id));
				}

				$logging_obj->my_error_handler("INFO",array("PhysicalInfraVmId"=>$physical_infra_vm_id,"MergeIpAddress"=>$merge_ip_address,"MacAddress"=>$physical_mac_address,"AgentReportedMac"=>$physical_mac_address,"VmwareToolsInstalled"=>$vmware_tools_installed,"Solution"=>$CLOUD_PLAN_AZURE_SOLUTION),Log::BOTH);
				
				$log_data = $log_data." physical_infra_vm_id::$physical_infra_vm_id, merge_ip_address::$merge_ip_address mac_address::$physical_mac_address, agent reported mac::$mac_address";

				// In case record exist with the ipaddress then that is a physical vm go and update the bios id , hostid & mac address list.
				// In case if mac address is not set, or same mac address is reported with the ip then update else add as physical.
				// in case of TFo/UFO VMs report the ip same as on prem so in that case we need to add it as physical so added mac address check.
				if($physical_infra_vm_id && (!$physical_mac_address || (strtolower($physical_mac_address) == strtolower($mac_address))))
				{
					$infra_ids_for_host_id = $this->commonfunctions_obj->get_infrastructure_id_for_hostid($host_id);
					//As of now host id mapping doesn't exist (or) 
					//Host id mapping exists and for same infrastructure vm id allow to update required properties.
					if (count($infra_ids_for_host_id) == 0 || in_array($physical_infra_vm_id, $infra_ids_for_host_id))
					{
						$update_physical_sql = "UPDATE
													infrastructureVMs
												SET
													hostId = ?,
													hostUuid = ?,
													macAddressList = ?,
													isPersonaChanged = '1'
												WHERE
													infrastructureVMId = ?";
						$sql_args = array($host_id, $bios_id, $mac_address, $physical_infra_vm_id);
						$this->conn->sql_query($update_physical_sql, $sql_args);
						
						$logging_obj->my_error_handler("INFO",array("UpdateSQL"=>$update_physical_sql, "UpdateHostIdArgs"=>$sql_args),Log::BOTH);
						
						$log_data = $log_data." Updating host id for physical VM update_physical_sql::$update_physical_sql, sql_args::".print_r($sql_args, true);
					}
					
					$protection_exist_sql = "SELECT count(*) as pairCount FROM applicationScenario, srcLogicalVolumeDestLogicalVolume WHERE sourceId = sourceHostId AND sourceId = ?";
					$protection_data = $this->conn->sql_query($protection_exist_sql, array($host_id));
					
					if($protection_data[0]["pairCount"] <= 0)
					{
						$delete_sql = "DELETE FROM infrastructureVMs WHERE infrastructureVMId = ?";
						$this->conn->sql_query($delete_sql, array($infra_vm_data[0]["infrastructureVMId"]));
						
						$logging_obj->my_error_handler("INFO",array("DeleteSql"=>$delete_sql, "DeleteNoVmWareToolsInfrastructureVmId"=>$infra_vm_data[0]["infrastructureVMId"]),Log::BOTH);

						$log_data = $log_data." Deleting vmware vm delete_sql::$delete_sql, sql_args::".$infra_vm_data[0]["infrastructureVMId"];
					}
				}
			}
			
			$logging_obj->my_error_handler("INFO",array("previousHostId"=>$infra_vm_host_id,"currentHostId"=> $host_id,"vmwareToolsInstalled"=>$vmware_tools_installed),Log::BOTH);
									
			$log_data = $log_data." infra_vm_host_id::$infra_vm_host_id, host_id::$host_id vmware_tools_installed::$vmware_tools_installed";
			
			// In case if host id is not set or the for the same vm if new host id is reported update the new host id.
			// By updating the hostId every time, in case if the vm agent is reinstalled then those hostid problems will be resolved.
			if(!$infra_vm_host_id || ($infra_vm_host_id != $host_id))
			{
				$infra_ids_for_host_id = $this->commonfunctions_obj->get_infrastructure_id_for_hostid($host_id);
				if (count($infra_ids_for_host_id) == 0 || in_array($vm_infra_id, $infra_ids_for_host_id))
				{
					$update_vm_sql = "UPDATE
										infrastructureVMs
									SET
										hostId = ?
									WHERE
										infrastructureVMId = ?";
					$sql_args = array($host_id, $vm_infra_id);
					$this->conn->sql_query($update_vm_sql, $sql_args);
					
					$logging_obj->my_error_handler("INFO",array("UpdateVmSql"=>$update_vm_sql,"UpdateHostIdForVm"=>$sql_args),Log::BOTH);

					$log_data = $log_data." update_vm_sql::$update_vm_sql, sql_args".print_r($sql_args, true);
				}
			}
			//If IP exists and mapping already done and later in case if there is a mac address changed like nic addition case then update for physical as there is no discovery 
			// going to happen in backend, leaving for vmware as discovery updates it.
			else if((strtolower($vm_mac_address) != strtolower($mac_address)) && !$infra_vm_data[0]["hostDetails"])
			{
				$infra_ids_for_host_id = $this->commonfunctions_obj->get_infrastructure_id_for_hostid($host_id);
				//As of now host id mapping doesn't exist (or) 
				//Host id mapping exists and for same infrastructure vm id allow to update required properties.
				if (count($infra_ids_for_host_id) == 0 || in_array($physical_infra_vm_id, $infra_ids_for_host_id))
				{
					$update_vm_sql = "UPDATE
										infrastructureVMs
									SET
										macAddressList = ?
									WHERE
										infrastructureVMId = ?";
					$sql_args = array($mac_address, $vm_infra_id);
					$this->conn->sql_query($update_vm_sql, $sql_args);
					$logging_obj->my_error_handler("INFO",array("UpdateVmSql"=>$update_vm_sql,"UpdateMacAddress"=>$sql_args),Log::BOTH);
					$log_data = $log_data."Updating mac address for physical as MAC changed update_vm_sql::$update_vm_sql, sql_args".print_r($sql_args, true);
				}
			}
		}
		// 1. if vm macAddress got changed.
		// 2. First time agent discovery for vmware without vcenter discovery.
		// 3. First time agent discovery for physical
		// 3a. manual Physical discovery and then push installation.
		// 3b. agent installation done first.
		else if(!$infra_vm_data)
		{
			// In case if the record does not exist then check with the list of ip addrress any record exist or not.
			$physical_infra_vm_id = $this->conn->sql_get_value("infrastructureVMs","infrastructureVMId", "FIND_IN_SET(hostIp, ?) AND hostDetails = '' AND discoveryType != ?", array($merge_ip_address, $CLOUD_PLAN_AZURE_SOLUTION));
			
			if($physical_infra_vm_id)
			{
				$physical_mac_address = $this->conn->sql_get_value("infrastructureVMs","macAddressList", "infrastructureVMId = ?", array($physical_infra_vm_id));
			}
			
			$logging_obj->my_error_handler("INFO",array("PhysicalInfraVmId"=>$physical_infra_vm_id,"MergeIpAddress"=>$merge_ip_address,"MacAddress"=>$physical_mac_address, "AgentReportedMac"=>$mac_address),Log::BOTH);

			$log_data = $log_data." physical_infra_vm_id::$physical_infra_vm_id, merge_ip_address::$merge_ip_address mac_address::$physical_mac_address, agent reported mac::$mac_address";
			
			// In case record exist with the ipaddress then that is a physical vm go and update the bios id , hostid & mac address list.
			// In case if mac address is not set, or same mac address is reported with the ip then update else add as physical.
			// in case of TFo/UFO VMs report the ip same as on prem so in that case we need to add it as physical so added mac address check.
			if($physical_infra_vm_id && (!$physical_mac_address || (strtolower($physical_mac_address) == strtolower($mac_address))))
			{
				$infra_ids_for_host_id = $this->commonfunctions_obj->get_infrastructure_id_for_hostid($host_id);
				//As of now host id mapping doesn't exist (or) 
				//Host id mapping exists and for same infrastructure vm id allow to update required properties.
				if (count($infra_ids_for_host_id) == 0 || in_array($physical_infra_vm_id, $infra_ids_for_host_id))
				{
					$update_physical_sql = "UPDATE
												infrastructureVMs
											SET
												hostId = ?,
												hostUuid = ?,
												macAddressList = ?
											WHERE
												infrastructureVMId = ?";
					$sql_args = array($host_id, $bios_id, $mac_address, $physical_infra_vm_id);
					$this->conn->sql_query($update_physical_sql, $sql_args);
					$logging_obj->my_error_handler("INFO",array("UpdatePhysicalSql"=>$update_physical_sql,"updateInfrastructureVms"=>$sql_args),Log::BOTH);
					$log_data = $log_data." update_physical_sql::$update_physical_sql, sql_args".print_r($sql_args, true);
				}
			}
			else
			{
			
				//Get the hostDetails value from infrastructure vms table. For physical machines, hostDetails property is empty value. For discovered vmware vm's hostdetails property has properties of vm.
				$record_exist_sql = "SELECT infrastructureVMId, hostDetails FROM infrastructureVMs WHERE hostUuid = ? AND hostId = ?";
				$record_data = $this->conn->sql_query($record_exist_sql, array($bios_id, $host_id));
				if (is_array($record_data ))
				{
					$matched_infra_id = $record_data[0]["infrastructureVMId"];
					$physical_host_details = $record_data[0]["hostDetails"];
					$hostdetailstype = gettype($physical_host_details);
					$rec_count = count($record_data);
					$logging_obj->my_error_handler("INFO",array("physical_host_details_type"=>$hostdetailstype, "physical_host_details"=>$physical_host_details,"BIOSID"=>$bios_id,"HoSTID"=>$host_id, "recordCount"=>$rec_count),Log::BOTH);	
					if(count($record_data) > 0)
					{
						$logging_obj->my_error_handler("INFO",array("physical_host_details"=>$physical_host_details),Log::BOTH);
						if($physical_host_details == '')
						{
							// physical, Only update it for physical discovery records.
							//If BIOS id changes for any reason, agent treats as clone VM, it reports with different host id and biosid. Hence, no need to worry about hostUuid update here.
							$update_vm_sql = "UPDATE
													infrastructureVMs              
												SET
													macAddressList = ?
												WHERE
													infrastructureVMId = ?";
							$sql_args = array($mac_address, $matched_infra_id);
							$this->conn->sql_query($update_vm_sql, $sql_args);
							$logging_obj->my_error_handler("INFO",array("UpdateVmSql"=>$update_vm_sql,"UpdateMacAddress"=>$sql_args),Log::BOTH);
							$log_data = $log_data."Updating mac address for physical as MAC changed update_vm_sql::$update_vm_sql, sql_args".print_r($sql_args, true);
							return;
						}
						else
						{
							//vmware discovery updates the actual mac address for vmware vm's which are vCenter discovered ones.
							$logging_obj->my_error_handler("INFO","Trying to do duplicate mapping because of mac address mismatch or biosid mismatch, hence returned",Log::BOTH);
							return;
						}
					}
				}
				// in case no record exist with the bios id + mac combination & ipaddress then insert new record in discovery as physical VM.
				$esx_discovery_obj = new ESXDiscovery();
				$post_details = array();
				
				$post_details['inventoryid'] = "";
				$post_details['new_site'] = 'VDC1';
				$post_details['servertype'] = "Physical";
				$post_details['ipaddress'] = $ip_address_list[0];
				$post_details['host_name'] = $host_name;
				$post_details['operating_system'] = ($os_flag == 1) ? "Windows" : "Linux";
				$post_details['input_host_id'] = $host_id;
				$post_details['bios_id'] = $bios_id;
				$post_details['mac_address_list'] = $mac_address;

				$_POST = $post_details;
				$esx_discovery_obj->saveInventoryPhysical("Physical");
				$log_data = $log_data." post_details::".print_r($post_details, true);
			}
		}
		
		if ($log_data != "")
		{
			$mds_data_array = array();
			$mds_data_array["activityId"] = "";
			$mds_data_array["jobType"] = "Registration";
			$mds_data_array["errorString"] = "Register Host static info host id updation, LogMessage - $log_data";
			$mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
			$mds_data_array["errorType"] = "ERROR";
			$mds_obj = new MDSErrorLogging();
			$mds_obj->saveMDSLogDetails($mds_data_array);
		}
	}
};
?>