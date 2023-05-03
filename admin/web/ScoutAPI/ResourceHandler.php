<?php
/**
 * Base case for Resource Handler ...
 */
abstract class ResourceHandler extends FunctionHandler {
	
	/*
 *  Base Function Implementation (FunctionHandler)
 *  Returns true if the ParameterGroup are sufficient 
 */
	public function validate($identity, $version, $ParameterGroup) {
		return true;
	}
	
/*
 *  Base Function Implementation (ResourceHandler)
 *  Returns true if the function is implemented 
 */
	public function isFunctionSupported($functionName) {
		$found = true;
		$class_methods = get_class_methods ( get_class ( $this ) );
		if (array_search ( $functionName, $class_methods ) === false) {
			$found = false;
		}
		return $found;
	}
	
	/*
	 *  Base Function Implementation (Handler)
	 *  Implements the function show return Associative Array or throw an Exception with ID and Message
	 *
	 */
	public function process($identity, $version, $functionName, $ParameterGroup) {
		
		$validate = $functionName.'_validate';
		$this->$validate( $identity, $version, $functionName, $ParameterGroup );
		return $this->$functionName ( $identity, $version, $functionName, $ParameterGroup );
	}

	/*
	 * @Description: checkSystemGUID  returns hostId
	 * @PartnerLogic: None
	 */
	public function checkSystemGUID($conn,$systemGUID)
	{
		$sql = "SELECT
					id
				FROM
					hosts
				WHERE
					systemId='$systemGUID'";
		$rs = $conn->sql_query($sql);
		if($conn->sql_num_row($rs))
		{
			$row = $conn->sql_fetch_object($rs);
			$hostId = $row->id;
			return $hostId;
		}
		else
		{
			$this->raiseException(ErrorCodes::EC_NO_HST_FND_WTH_SYSTM_GUID,"No host found with the system GUID");
		}
	}
	
	/*
	 * @Description: checkSystemGUID  returns hostId
	 * @PartnerLogic: None
	 */
	public function checkAgentGUID($conn,$agentGUID)
	{
		global $log_api_name;
		$sql = "SELECT
					id
				FROM
					hosts
				WHERE
					id = ?";
		$rs = $conn->sql_query($sql, array($agentGUID));
		if(count($rs) > 0)
		{
			return $rs[0]['id'];
		}
		else
		{
			ErrorCodeGenerator::raiseError(COMMON, 'EC_NO_AGENT_FOUND', array("OperationName"=>$log_api_name, 'ErrorCode' => ErrorCodes::EC_NO_AGENT_FOUND));
		}
	}
	/*
	 * @Description: checkAgentIP checks wheather the host with given IP exists or not
	 *               If yes returns hostId else throws exception
	 * @PartnerLogic: None
	 */
	public function checkAgentIP($conn,$agentIP)
	{
		$sql = "SELECT
					id
				FROM
					hosts
				WHERE
					ipAddress = ?";
		$result = $conn->sql_query($sql, array($agentIP));
        if(count($result) > 0)
        {
			$hostId = $result[0]['id'];
			return $hostId;
		}
		else
		{
			$this->raiseException(ErrorCodes::EC_NO_HOSTS_IP,"No Agent found with the given HostIP $agentIP");
		}
	}
	
	/*
	 * @Description: checkAgentName checks wheather the host with given name exists or not
	 *               If yes returns hostId else throws exception
	 * @PartnerLogic: None
	 */
	public function checkAgentName($conn,$hostName)
	{
		$sql = "SELECT
					id
				FROM
					hosts
				WHERE
					name = ?";
		$result = $conn->sql_query($sql, array($hostName));
        if(count($result) > 0)
        {
			$hostId = $result[0]['id'];
			return $hostId;
		}
		else
		{
			$this->raiseException(ErrorCodes::EC_NO_HOSTS_NAME,"No Agent found with the given HostName");
		}
	}
	
	/*
	 * @Description: Throws an Invalid Data Exception
	 * @PartnerLogic: None
	 */
	public function raiseException($errorCode,$message = "Unknown Exception")
	{
		throw new Exception($message,$errorCode);
	}
	
	/*
	 * @Description: Common Function to validate Parameter Values
	 * @PartnerLogic: None
	*/
	protected function validateParameter($obj,$parameterName,$optionalParameter=FALSE,$validate='')
	{
		$value = '';
		//Validate Parameter
		if(isset($obj[$parameterName])) 
		{
			$value = trim($obj[$parameterName]->getValue());
			if($value != '')
			{
				if(trim($validate)) {
					$validate = strtoupper($validate);
					switch($validate)
					{
						case 'NUMBER':
							if(!is_numeric($value)) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => $parameterName));
						break;
					}
				}
			}else {
				//Error: No value Present for ParameterName
				ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $parameterName));
			}
			
		}else {
			if($optionalParameter === FALSE) {
				//Error: ParameterName Parameter is missed from XML
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $parameterName));
			}
		}
		return $value;
	}
}
?>