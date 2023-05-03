<?php
class ESXMasterConfigParser
{
	protected $obj;
	public $conn;
	private $comm_obj;
	
	function ESXMasterConfigParser()
    {	
		global $conn_obj;
		$this->conn = $conn_obj;
		$this->comm_obj = new CommonFunctions();
    }
	
	public function parseXML() {
		global $AUDIT_LOG_DIRECTORY;	
		$ESX_MASTER_CONFIG = $AUDIT_LOG_DIRECTORY.'cli/vcon/Esx_Master.xml';
		if(file_exists($ESX_MASTER_CONFIG))
		{
			$returnval = false;
			$xml = simplexml_load_file ( $ESX_MASTER_CONFIG );
			if ($xml === false) {
				return false;
			}
			$this->obj = $xml;
			return true;
		}
		return false;
	}
	
	public function getRoot($index=0) {
		return $this->obj->root[$index];
	}
	
	/*
		Return all attributes as an array of the root element of given index
	*/
	public function getRootAttributes($index=0) {
		return $this->obj->root[$index]->attributes();
	}
	
	/*
		Sets attributes of the root element of given index
	*/
	public function setRootAttributes($attList=array(),$index=0) {
		foreach($attList as $name=>$value) {
			$this->obj->root[$index][$name] = $value;
		}
	}
	
	public function getHostNetworkConf($hostId=0) {
		$result = array();
		$index = 0;
		while($this->obj->root[$index]) {
			$SrcObj = $this->obj->root[$index]->SRC_ESX;
			$TrgObj = $this->obj->root[$index]->TARGET_ESX;
			//SRC_ESX Tag Details
			if(is_object($SrcObj)) {
				$SrcHostIndex = 0;
				while($SrcObj->host[$SrcHostIndex]) {
					$hostObj = $SrcObj->host[$SrcHostIndex];
					$SrcHostAttributes = (array) $hostObj->attributes();
					$SrcHostAttributes = $SrcHostAttributes['@attributes'];												
					if($SrcHostAttributes['failover'] == "no" && (($hostId == 0) || ($hostId == $SrcHostAttributes['inmage_hostid']))) {						
						$nicIndex = 0;
						while($hostObj->nic[$nicIndex]) {
							$r = (array) $hostObj->nic[$nicIndex]->attributes();
							$result[$SrcHostAttributes['inmage_hostid']][] = $r['@attributes'];
							$nicIndex++;
						}
					}
					$SrcHostIndex++;
				}
			}
						
			//TARGET_ESX Tag Details
			if(is_object($TrgObj)) {
				$TrgHostIndex = 0;
				while($TrgObj->host[$TrgHostIndex]) {
					$hostObj = $TrgObj->host[$TrgHostIndex];
					$TrgHostAttributes = (array) $hostObj->attributes();
					$TrgHostAttributes = $TrgHostAttributes['@attributes'];					
					if($TrgHostAttributes['failover'] == "no" && (($hostId == 0) || ($hostId == $TrgHostAttributes['inmage_hostid']))) {
						$nicIndex = 0;
						while($hostObj->nic[$nicIndex]) {
							$r = (array) $hostObj->nic[$nicIndex]->attributes();
							$result[$TrgHostAttributes['inmage_hostid']][] = $r['@attributes'];
							$nicIndex++;
						}
					}
					$TrgHostIndex++;
				}
			}
			$index++;
		}
		return $result;
	}
	
	public function getHostHWConf($hostId=0) {
		$result = array();
		$index = 0;
		while($this->obj->root[$index]) {
			$SrcObj = $this->obj->root[$index]->SRC_ESX;
			$TrgObj = $this->obj->root[$index]->TARGET_ESX;
			//SRC_ESX Tag Details
			if(is_object($SrcObj)) {
				$SrcHostIndex = 0;
				while($SrcObj->host[$SrcHostIndex]) {
					$hostObj = $SrcObj->host[$SrcHostIndex];
					$SrcHostAttributes = (array) $hostObj->attributes();
					$SrcHostAttributes = $SrcHostAttributes['@attributes'];
					if($SrcHostAttributes['failover'] == "no" && (($hostId == 0) || ($hostId == $SrcHostAttributes['inmage_hostid']))) 
					{
						// Disk details to get the datastore selected
						$SrcHostDiskIndex = 0;
						$SrcDiskDetails = array();
						while($hostObj->disk[$SrcHostDiskIndex])
						{
							$DiskDetails = array();
							$hostDiskobj = $hostObj->disk[$SrcHostDiskIndex];
							$SrcHostDiskAttributes = (array) $hostDiskobj->attributes();
							$SrcHostDiskAttributes = $SrcHostDiskAttributes['@attributes'];
							foreach ($SrcHostDiskAttributes as $key => $value)
							{
								$DiskDetails[$key] = $value;
							}
							$SrcDiskDetails[] = $DiskDetails;
							$SrcHostDiskIndex++;
						}
						
						$result[$SrcHostAttributes['inmage_hostid']] = array('memsize'=>$SrcHostAttributes['memsize'],
																			'cpucount'=>$SrcHostAttributes['cpucount'],
																			'uuid'=>$SrcHostAttributes['source_uuid'],
																			'new_displayname'=>$SrcHostAttributes['new_displayname'],
																			'vmx_path' => $SrcHostAttributes['vmx_path'],
																			'folder_path' => $SrcHostAttributes['folder_path'],
																			'vmDirectoryPath' => $SrcHostAttributes['vmDirectoryPath'],
																			'alt_guest_name' => $SrcHostAttributes['alt_guest_name'],
																			'vmx_version' => $SrcHostAttributes['vmx_version'],
																			'hostversion' => $SrcHostAttributes['hostversion'], 
																			'efi' => $SrcHostAttributes['efi'],
																			'resourcepoolgrpname' => $SrcHostAttributes['resourcepoolgrpname'],
																			'ide_count' => $SrcHostAttributes['ide_count'],
																			'cluster' => $SrcHostAttributes['cluster'],
																			'floppy_device_count' => $SrcHostAttributes['floppy_device_count'],
																			'os_info' => $SrcHostAttributes['os_info'],
																			'hostname' => $SrcHostAttributes['hostname'],
																			'machinetype' => $SrcHostAttributes['machinetype'],
																			'isItVcenter' => $SrcHostAttributes['vCenterProtection'],
																			'disk_details' => $SrcDiskDetails);
					}					
					$SrcHostIndex++;					
				}
			}
			
			//TARGET_ESX Tag Details
			if(is_object($TrgObj)) {
				$TrgHostIndex = 0;
				while($TrgObj->host[$TrgHostIndex]) {
					$hostObj = $TrgObj->host[$TrgHostIndex];
					$TrgHostAttributes = (array) $hostObj->attributes();
					$TrgHostAttributes = $TrgHostAttributes['@attributes'];
					if($TrgHostAttributes['failover'] == "no" && (($hostId == 0) || ($hostId == $TrgHostAttributes['inmage_hostid']))) {
						$memSize = $TrgHostAttributes['memsize'];
						$cpuCount = $TrgHostAttributes['cpucount'];
						$uuid = $TrgHostAttributes['target_uuid'];
						$new_displayname = $TrgHostAttributes['new_displayname'];
						$result[$TrgHostAttributes['inmage_hostid']] = array('memsize'=>$memSize,'cpucount'=>$cpuCount,'uuid'=>$uuid,'new_displayname'=>$new_displayname);
					}
					$TrgHostIndex++;
				}
			}
			$index++;
		}
		return $result;
	}
	
	public function getESXHW() {
		$result = array();
		$index = 0;
		while($this->obj->root[$index]) {
			$TrgObj = $this->obj->root[$index]->TARGET_ESX;
						
			//TARGET_ESX Tag Details
			if(is_object($TrgObj)) {
				$TrgHostIndex = 0;
				while($TrgObj->host[$TrgHostIndex]) {
					$hostObj = $TrgObj->host[$TrgHostIndex];
					$configIndex = 0;
					while($hostObj->config[$configIndex]) {
						$r = (array) $hostObj->config[$configIndex]->attributes();
						$configAttributes = $r['@attributes'];
						$vSpherehostname = $configAttributes['vSpherehostname'];
						$result[$vSpherehostname] = $r['@attributes'];
						$configIndex++;
					}
					$nicIndex = 0;
					while($hostObj->network[$nicIndex]) {
						$r = (array) $hostObj->network[$nicIndex]->attributes();
						$networkAttributes = $r['@attributes'];
						$network_names[] = $networkAttributes['name'];
						$nicIndex++;
					}
					$network_names_array = array_unique($network_names);					
					$result['network_names'] = $network_names_array;
					$TrgHostIndex++;
				}
			}
			$index++;
		}
		return $result;
	}
	
	public function getESXDetails() {
		$result = array();
		$index = 0;
		while($this->obj->root[$index]) {
			$SrcObj = $this->obj->root[$index]->SRC_ESX;
			//SRC_ESX Tag Details
			if(is_object($SrcObj)) {
				$SrcHostIndex = 0;
				while($SrcObj->host[$SrcHostIndex]) {
					$hostObj = $SrcObj->host[$SrcHostIndex];
					$SrcHostAttributes = (array) $hostObj->attributes();
					$SrcHostAttributes = $SrcHostAttributes['@attributes'];
					$encryption_key = $this->comm_obj->getCSEncryptionKey();
					$host_info_sql = "SELECT IF(ISNULL(userName) OR userName = '', userName, AES_DECRYPT(UNHEX(userName), ?)) as userName, IF(ISNULL(password) OR password = '', password, AES_DECRYPT(UNHEX(password), ?)) as password FROM credentials WHERE serverId = ?";
					$host_credentials = $this->conn->sql_query($host_info_sql, array($encryption_key, $encryption_key, $SrcHostAttributes['targetesxip']));
					if($SrcHostAttributes['targetesxip'] && $host_credentials[0]['userName'] && $host_credentials[0]['password']) {
						$result[$SrcHostAttributes['inmage_hostid']] = array('ESXIP'=>$SrcHostAttributes['targetesxip'],'UserName'=>$host_credentials[0]['userName'],'Password'=>$host_credentials[0]['password']);
					}
					$SrcHostIndex++;
				}
			}
			$index++;
		}
		return $result;
	}
	
	public function getProtectedHostDetails($hostId=0) {
		$result = array();
		$index = 0;
		while($this->obj->root[$index]) {
			$SrcObj = $this->obj->root[$index]->SRC_ESX;
			//SRC_ESX Tag Details
			if(is_object($SrcObj)) {
				$SrcHostIndex = 0;
				while($SrcObj->host[$SrcHostIndex]) {
					$hostObj = $SrcObj->host[$SrcHostIndex];
					$SrcHostAttributes = (array) $hostObj->attributes();
					$SrcHostAttributes = $SrcHostAttributes['@attributes'];
					if($SrcHostAttributes['failover'] == "no" && (($hostId == 0) || ($hostId == $SrcHostAttributes['inmage_hostid']))) {
						if($SrcHostAttributes['targetesxip'] && $SrcHostAttributes['vconname'] && $SrcHostAttributes['mastertargethostname'] && $SrcHostAttributes['targetvSphereHostName']) {
							$result[$SrcHostAttributes['inmage_hostid']] = array('targetesxip'=>$SrcHostAttributes['targetesxip'],'mastertargethostname'=>$SrcHostAttributes['mastertargethostname'],'vconname'=>$SrcHostAttributes['vconname'], 'targetvSphereHostName'=>$SrcHostAttributes['targetvSphereHostName']);
						}
					}
					$SrcHostIndex++;
				}
			}
			$index++;
		}
		return $result;
	}
	
	public function getMasterTargetDetails($hostId=0) {
		$result = array();
		$index = 0;
		while($this->obj->root[$index]) {
			$TgtObj = $this->obj->root[$index]->TARGET_ESX;
			//TARGET_ESX Tag Details
			if(is_object($TgtObj)) {
				$TgtHostIndex = 0;
				while($TgtObj->host[$TgtHostIndex]) {
					$hostObj = $TgtObj->host[$TgtHostIndex];
					$TgtHostAttributes = (array) $hostObj->attributes();
					$TgtHostAttributes = $TgtHostAttributes['@attributes'];
					if(($hostId == 0) || ($hostId == $TgtHostAttributes['inmage_hostid'])) {						
						if($TgtHostAttributes['inmage_hostid']) {
							$result[$TgtHostAttributes['inmage_hostid']] = array('vSphereHostName'=>$TgtHostAttributes['vSpherehostname'],'datacenter' => $TgtHostAttributes['datacenter'],'source_uuid' => $TgtHostAttributes['source_uuid'], 'hostname' => $TgtHostAttributes['hostname'], 'vCenterProtection' => $TgtHostAttributes['vCenterProtection'], 'hostversion' => $TgtHostAttributes['hostversion'], 'resourcepoolgrpname' => $TgtHostAttributes['resourcepoolgrpname']) ;
						}
					}
					$TgtHostIndex++;
				}
			}
			$index++;
		}
		return $result;
	}
	
	public function getMBRPath() {
		$result = array();
		$index = 0;
		while($this->obj->root[$index]) {
			$SrcObj = $this->obj->root[$index]->SRC_ESX;
			//SRC_ESX Tag Details
			if(is_object($SrcObj)) {
				$SrcHostIndex = 0;
				while($SrcObj->host[$SrcHostIndex]) {
					$hostObj = $SrcObj->host[$SrcHostIndex];
					$SrcHostAttributes = (array) $hostObj->attributes();
					$SrcHostAttributes = $SrcHostAttributes['@attributes'];
					
					if($SrcHostAttributes['mbr_path']) {
						$result[$SrcHostAttributes['inmage_hostid']][] = basename($SrcHostAttributes['mbr_path']);
					}	
					
					$SrcHostIndex++;
				}
			}
			$index++;
		}
		return $result;
	}

}
?>
