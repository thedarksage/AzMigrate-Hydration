<?php
class PolicyHandler extends ResourceHandler 
{
	private $commonObj;
	private $conn;
	private $policyObj;
	private $SRMObj;
	
	public function PolicyHandler() 
	{
		$this->commonObj = new CommonFunctions();
		$this->conn = new Connection();
		$this->policyObj = new Policy();
		$this->SRMObj = new SRM();
	}
	
	public function InsertScriptPolicy_validate($identity, $version, $functionName, $parameters)
	{
		if(!isset($parameters['HostID'])) $this->raiseException(ErrorCodes::EC_INSFFCNT_PRMTRS,"Missing HostID Parameter");
		$hostId = trim($parameters['HostID']->getValue());
		if(!$hostId) {
			$this->raiseException(ErrorCodes::EC_NO_DATA,"No Data found for HostID Parameter");
		}
		
		if(!isset($parameters['ScriptPath'])) $this->raiseException(ErrorCodes::EC_INSFFCNT_PRMTRS,"Missing ScriptPath Parameter");
		$path = trim($parameters['ScriptPath']->getValue());
		if(!$path) {
			$this->raiseException(ErrorCodes::EC_NO_DATA,"No Data found for ScriptPath Parameter");
		}
	}
	
	public function InsertScriptPolicy($identity, $version, $functionName, $parameters)
	{
		global $requestXML;
		$response = new ParameterGroup("Response");
		$path = trim($parameters['ScriptPath']->getValue());
		$path = str_replace("'",'"',$path);
		$hostId = trim($parameters['HostID']->getValue());
		if(!($this->commonObj->getHostName($hostId))) {
			$this->raiseException(ErrorCodes::EC_NO_AGENTS,"No Agent found with the given HostID");
		}
		$policyType = 45;
		$policyScheduledType = 2;
		$policyRunEvery = 0;
		$policyParameters = serialize(array('ScriptCmd'=>$path));
		$policyName = "Script";
				
		$policyId = $this->policyObj->insert_script_policy($hostId,$policyType,$policyScheduledType,$policyRunEvery,'',0,0,$policyParameters,$policyName);
		
		if($policyId) {
			$md5str = $hostId.rand();
			$md5CheckSum = md5($md5str);
			$result = $this->SRMObj->insert_request_xml($requestXML,$functionName,$md5CheckSum);
			$this->SRMObj->insert_request_data($policyId,$result['apiRequestId'],$functionName);
			$response->setParameterNameValuePair('RequestId',$result['apiRequestId']);
		}
		return $response->getChildList();
	}
}
?>