<?php
class APIRequest {
	private $data;
	private $parmCounter;
	public $parsed = false;
	
	/*
	 * Function: RequestObj from request XML
	 */
	public function build($requestXML) {
		
		/*if (get_magic_quotes_gpc()==1)
		{
			$requestXML = stripslashes ( $requestXML );
		}*/	
		
		$xml = XMLUtil::parseXML ( $requestXML );
		if ($xml == false)
			return;
		$this->data = $xml;
		$this->parsed = true;
	}
	
	/*
	 * Returns Associate Array represention of XML
	 */
	public function getData() {
		return $this->data;
	}
	
	/*
	 * Returns Request ID
	 */
	public function getRequestID() {
		return $this->data ["attr"] ["Id"];
	}
	
	/*
	 *  Returns Version
	 */
	public function getVersion() {
		return $this->data ["attr"] ["Version"];
	}
	
	/*
	 *  Returns Authentication Block
	 */
	public function getAuthenticationBlock() {
		return $this->data ["children"] ["Header"] ["children"] ["Authentication"] ["children"];
	}
	
	/*
	 *  Returns Function Request Objects
	 */
	public function getFunctionRequestObjects() {
		return $parry = $this->data ["children"] ["Body"] ["children"];
	}
	
	/*
	 * Return AccessKey
	 */
	public function getAccessKey() {
		$authenticationblock = $this->getAuthenticationBlock ();
		$accesskey = $authenticationblock ["Accesskeyid"] ["value"];
		return $accesskey;
	
	}
	
	/*
	 * Return RequestSignature
	 */
	public function getRequestSignature() {
		$authenticationblock = $this->getAuthenticationBlock ();
		$requestSignature =  $authenticationblock ["Accesssignature"] ["value"];
		return $requestSignature;
	
	}
	
	/*
	 * Return RequestAuthMethod
	 */
	public function getRequestAuthMethod() {
		$authenticationblock = $this->getAuthenticationBlock ();
		$authMethod =  isset($authenticationblock ["Authmethod"] ["value"]) ? trim($authenticationblock ["Authmethod"] ["value"]) : '';

		return $authMethod;	
	}
	
	/*
	 * Return RequestCXUserName
	 */
	public function getRequestUserName() {
		$authenticationblock = $this->getAuthenticationBlock ();
		$userName =  trim($authenticationblock ['Cxusername'] ['value']);
		return $userName;
	
	}
	
	/*
	 * Return RequestPassword
	 */
	public function getRequestPassword() {
		$authenticationblock = $this->getAuthenticationBlock ();
		$passwd =  trim($authenticationblock ["Cxpassword"] ["value"]);
		return $passwd;
	
	}
	
	/*
	 *  Return Function Names
	 */
	public function getFunctionNames() {
		$funcObjs = $this->getFunctionRequestObjects ();
		$funcNames = array ();
		foreach ( $funcObjs as $funcObj ) {
			$funcNames [] = $funcObj->getName ();
		}
		return $funcNames;
	}
	
	/*
	 *  Return array representing Response XML Fragment from a file
	 */
	public function readParameterXML($filename) {
		$xml = simplexml_load_file ( $filename );
		if ($xml == false)
			return;
		return $this->xml2array ( $xml );
	}
}

?>