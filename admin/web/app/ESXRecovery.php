<?php
class ESXRecovery extends ESXMasterConfigParser
{
	private $recoveryXml;
	private $planName;
	private $batchCount;
	private $xmlDirectoryName;
	private $hostIdList = array();
	private $hostDetails;
	private $hostId;
	
	public function ESXRecovery() {
		parent::parseXML();
	}
	
	/*
		Sets attributes of the element
	*/
	public function addAttribute($obj,$attList=array()) {
		foreach($attList as $name=>$value) {
			if(!$value) $value="##BLANK_SPACE##";
			$obj->addAttribute($name,$value);
		}
	}
	
	public function addChild($obj,$name,$value='') {
		if($value) {
			$child = $obj->addChild($name,$value);
		}else {
			$child = $obj->addChild($name);
		}
		return $child;
	}
	
	/*
		Return all attributes as an array of the root element of given index
	*/
	public function getRootAttributes() {
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
					
					if((in_array($SrcHostAttributes['inmage_hostid'],$this->hostIdList)) && ($this->planName == $SrcHostAttributes['plan'])) {
						$result = (array) $this->obj->root[$index]->attributes();
						$result['@attributes']['batchresync'] = $this->batchCount;
						$result['@attributes']['xmlDirectoryName'] = $this->xmlDirectoryName;
						$SrcHostIndex++;
						return $result['@attributes'];
					}else {
						$SrcHostIndex++;
						continue;
					}
				}
			}
			$index++;
		}
		return $result;
	}
	
	public function getTRGHost() {
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
					
					if((in_array($SrcHostAttributes['inmage_hostid'],$this->hostIdList)) && ($this->planName == $SrcHostAttributes['plan'])) {
						$TrgObj = $this->obj->root[$index]->TARGET_ESX;
						if(is_object($TrgObj)) {
							$TgrHostIndex = 0;
							while($TrgObj->host[$TgrHostIndex]) {
								$insert_tgt_flag = 0;
								$tgthostObj = $TrgObj->host[$TgrHostIndex];
								$TgrHostAttributes = (array) $tgthostObj->attributes();
								$TgrHostAttributes = $TgrHostAttributes['@attributes'];
								$id = $TgrHostAttributes['inmage_hostid'];
								
								if($this->targetHostDetails[$id]['dummy_disk_details']) 
								{
									$DummyDiskObj = $this->addChild($TrgObj,'dummy_disk');
									foreach ($this->targetHostDetails[$id]['dummy_disk_details'] as $disk_detail)
									{
										$MT_DiskObj = $this->addChild($DummyDiskObj,'MT_Disk');
										foreach($disk_detail as $k=>$v) {
											$MT_DiskObj->addAttribute($k,$v);
										}									
									}
									if(is_object($DummyDiskObj)) {
										$insert_tgt_flag = 1;
										$result[] = $TrgObj->host[$TgrHostIndex];
										$result[] = $DummyDiskObj;
									}
								}
								if(!$insert_tgt_flag)
								{
									$result[] = $TrgObj->host[$TgrHostIndex];
									$insert_tgt_flag = 0;
								}
								$TgrHostIndex++;
							}
						}						
						//For now hard code as TRG_ESX has only one <host> element
						$SrcHostIndex++;
					}else {						
						$SrcHostIndex++;
						$TgrHostIndex++;
						continue;
					}					
				}
			}
			$index++;
		}
		return $result;
	}
		
	public function getSRCHost() {
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
						
					if((in_array($SrcHostAttributes['inmage_hostid'],$this->hostIdList)) && ($this->planName == $SrcHostAttributes['plan'])  && ($SrcHostAttributes['failover'] == "no")) {
						//Update H/W configuration
						$id = $SrcHostAttributes['inmage_hostid'];
						if(array_key_exists('config',$this->hostDetails[$id])) {
							foreach($this->hostDetails[$id]['config'] as $k=>$v) {
								$hostObj->attributes()->$k = $v;
							}
						}
						//Update Network configuration
						if(array_key_exists('host_network_details',$this->hostDetails[$id])) {
							$nicIndex = 0;
							while($hostObj->nic[$nicIndex]) {
								foreach($this->hostDetails[$id]['host_network_details'][$nicIndex] as $k=>$v) {
									$hostObj->nic[$nicIndex]->attributes()->$k = $v;
								}
								$nicIndex++;
							}
						}
						//Update Recovery configuration
						if(array_key_exists('host_tag_details',$this->hostDetails[$id])) {							
							foreach($this->hostDetails[$id]['host_tag_details'] as $k=>$v) {
								if($hostObj->attributes()->$k)
									$hostObj->attributes()->$k = $v;
								else
									$hostObj->attributes()->addAttribute($k,$v);
							}
						}
						$SrcHostIndex++;
						$result[] = $hostObj;
						//return $hostObj;
					}else {
						$SrcHostIndex++;
						continue;
					}
				}
			}
			$index++;
		}
		return $result;
	}
	
	public function createRecovery ($val) {
		foreach($val as $planName=>$plan_details) {
			$this->recoveryXml = new SimpleXMLElement("<root/>");
			$this->planName = $planName;			
			$this->batchCount = $plan_details['batch_count'];			
			$this->hostIdList = array_keys($plan_details['hosts_details']);
			$this->hostDetails = $plan_details['hosts_details'];
			$this->xmlDirectoryName = $plan_details['xmlDirectoryName'];
			$this->targetHostDetails = $plan_details['target_host_details'];
			$recovery_xml = $this->process();
			return $recovery_xml;
		}
	}
	
	public function process() {
		$att = $this->getRootAttributes();
		$this->addAttribute($this->recoveryXml,$att);
		$this->addSRCESX();
		$this->addTRGESX();
		$result = $this->createXML();
		return $result;
	}
	
	public function addSRCESX() {
		$srcObj = $this->addChild($this->recoveryXml,'SRC_ESX');
		$result = $this->getSRCHost();
		foreach($result as $hostObj) {
			if(is_object($hostObj)) {
				$this->addHost($srcObj,$hostObj);
			}
		}
		
	}
	
	public function addTRGESX() {
		$trgObj = $this->addChild($this->recoveryXml,'TARGET_ESX');
		$result = $this->getTRGHost();
		foreach($result as $hostObj) {
			if(is_object($hostObj)) {
				$this->addHost($trgObj,$hostObj);
			}
		}
	}
	
	public function addHost($recObj,$node) {
		$element = $node->getName();
		$childObj = $this->addChild($recObj,$element);
		$att = (array) $node->attributes();
		$this->addAttribute($childObj,$att['@attributes']);
		if(count($node->children())) {
			foreach($node->children() as $child) {
				$this->addHost($childObj,$child);
			}
		} else {
			return;
		}
		return;
	}
	
	public function createXML() {		
		$result = $this->recoveryXml->asXML();
		$result = str_replace("##BLANK_SPACE##", "", $result);
		$stripped_recovery_xml = explode("\n", $result);
		$recovery_xml = str_replace(">",">\n",$stripped_recovery_xml[1]);
		return $recovery_xml;
	}
}
?>
