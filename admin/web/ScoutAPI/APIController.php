<?php
class APIController {
	private $requestid_i = "UNKNOWN";
	private $version_i = "1.0";
	private $requestObj;
	private $resultList = array ();
	public $call;
	
	const AUTHMETHOD_CXAUTH = 'CXAuth';
	const AUTHMETHOD_MESSAGEAUTH = 'MessageAuth';
	const AUTHMETHOD_COMPAUTH = 'ComponentAuth_V2015_01';
	
	/*
	 * Entry Method for Controller
	 */
	public function doProcess($requestXML) {
		
		return $this->process ( $requestXML );
	
	}
	/*
	 * Main Process Logic
	 */
	/*
	 * Main controller logic
	 * 1. Create Access Log
	 * 2. Create Request Object
	 * 3. Authenticate the request
	 * 4. For Each FunctionRequest do the following
	 * 		4.1 Authorize the request
	 * 		4.2 Resolve the handler
	 * 		4.3 Invoke the handler
	 * 		4.4 Construct the Function response XML
	 * 5. Create Respose XML
	 * 6. Create Request and Error Logs
	 */
	private function process($requestXML) {
		
		//1: Create Access Log
		// 1.1 Get the Referal URL and Log to a file
		

		//2: Create Request Object
		$message = "";
		$this->requestObj = new APIRequest ();		
		$this->requestObj->build ( $requestXML );
		
		if ($this->requestObj->parsed == false)
		{
			list($errCode, $message) = ErrorCodeGenerator::getErrorCodeAndMessage('COMMON', 'EC_INVALID_FORMAT');
			throw new XMLException($message, $errCode);
		}
		
		//Retrieve the Request and Version numbers
		$requestid = $this->requestObj->getRequestID ();
		$version = $this->requestObj->getVersion ();
		
		// 4. For Each FunctionRequest call process function Request
		$functionRequestObjects = $this->requestObj->getFunctionRequestObjects ();
		$processData = $this->getDataMethod ();
		foreach ( $functionRequestObjects as $functionRequestObj ) {
			$result = $this->processFunctionRequest ($this->requestObj, $functionRequestObj );
			$this->resultList [] = $result;
			if ($result->getReturnCode () > ErrorCodes::EC_SUCCESS && $processData->getHaltOnError ()) {
				break;
			}
		}
		
		// 5. Create a XML Response
		$response = new APIResponse ();
		$returnCode = ErrorCodes::EC_SUCCESS;
		$message = "Success";
		$xml = $response->createResponseXML ( $requestid, $version, $returnCode, $message, $this->resultList );
		return $xml;
	
		// 6. Create Request and Error Logs
	//TODO: Implement Access Log functionality
	}
	
	/*
	 * Process it for each of the functionRequest
	 */
	private function processFunctionRequest($requestObject, $functionRequestObj) 
	{

		global $REPLICATION_ROOT , $SLASH;
		$result = new Result ();
		$accesskey = $requestObject->getAccessKey();
		$version = $requestObject->getVersion();
		$result->setRequest ( $functionRequestObj );		
			
		// Request authentication
		$authCode = $this->authenticate ( $requestObject );
		if (! $authCode == 0) {
			// Create Response on Error and break it
			$returnCode = $authCode;
			$message = "Authentication Failure: ";
			if ($authCode == ErrorCodes::EC_AUTH_FAILURE) {
				$returnCode = ErrorCodes::EC_SIGNATURE_PASSWORD_MISMATCH;
				$message = $message . "Signature/Password mismatch";
			} elseif($authCode == ErrorCodes::EC_INVLD_AUTH_MTHD) {
				$returnCode = ErrorCodes::EC_INVALID_AUTHENTICATION_METHOD;
				$message = $message . "Invalid Auth Method";
			} elseif($authCode == ErrorCodes::EC_USRNME_NOT_FOUND) {
				$returnCode = ErrorCodes::EC_SIGNATURE_PASSWORD_MISMATCH;
				$message = $message . "Invalid User Name";
			} elseif($authCode == ErrorCodes::EC_PASSWD_NOT_FOUND) {
				$returnCode = ErrorCodes::EC_SIGNATURE_PASSWORD_MISMATCH;
				$message = $message . "Invalid Password";
			} elseif($authCode == ErrorCodes::EC_PASSPHRASE_EMPTY) {
				$returnCode = ErrorCodes::EC_PASSPHRASE_READ_FAILED;
				$message = $message . "Passphrase read failed - Internal Error";
			} elseif($authCode == ErrorCodes::EC_FINGERPRINT_EMPTY) {
				$returnCode = ErrorCodes::EC_FINGERPRINT_READ_FAILED;
				$message = $message . "Fingerprint read failed - Internal Error";
			}else {
				$returnCode = ErrorCodes::EC_ACCESS_KEY_NOT_FOUND ;
				$message = $message . "Access Key Not Found";
			}
			$result->setReturnCode ( $returnCode );
			$result->setMessage ( $message );
			$response = new ParameterGroup ( "ErrorDetails" );
			$response->setParameterNameValuePair( "ErrorCode", $returnCode );
			$response->setParameterNameValuePair( "ErrorMessage", "" );
			$response->setParameterNameValuePair( "PossibleCauses", "" );
			$response->setParameterNameValuePair( "Recommendation", "" );
			
			$result->setResponse ( $response);
			return $result;
		}
		
		//4.1: Authorization
		$functionName = $functionRequestObj->getName ();
		
		$authMethod = $requestObject->getRequestAuthMethod ();
		
		if(strcasecmp($authMethod,self::AUTHMETHOD_COMPAUTH) != 0)
		{
			$athrCode = $this->authorize ( $accesskey, $functionName );
			if (! $athrCode == 0) {
				
				if($athrCode == 2)
				{
				  $returnCode = ErrorCodes::EC_FUNC_NOT_SUPPORTED;
				  $message = "Function Not Supported";
				}
				else 
				{
					$returnCode = ErrorCodes::EC_ATHRZTN_FAILURE;
					$message = "Authorization Failure";
				}
				// Create Response on Error and break it
				$result->setReturnCode ( $returnCode );
				$result->setMessage ( $message );
				$result->setResponse ( array () );
				return $result;
			}
		}
		
		//5. Handle the Request
		$handler = HandlerFactory::getHandler ( $version, $functionName );
		$handlerName = "Unknown";
		if (isset ( $handler )) {
			$handlerName = get_class ( $handler );
			$paramList = $functionRequestObj->getChildList ();
			
			if ($handler->validate ( $accesskey, $version, $paramList )) {
				try {
				
					$appData = $this->getDataMethod ();
					$identityRecord = $appData->getIdentityRecord($accesskey);
					
					$data = $handler->process ( $identityRecord, $version, $functionName, $paramList );
					$result->setReturnCode ( ErrorCodes::EC_SUCCESS );
					$result->setMessage ( "Success" );
					$result->setResponse ( $data );
				} catch ( APIException $apiException ) {
					// Create Response on Error and break it
					$returnCode = $apiException->getCode ();
					$message = $apiException->getMessage ();
					$errorResponse = $apiException->getErrorResponse();
					
					$result->setReturnCode ( $returnCode );
					$result->setMessage ( $message );
					$result->setResponse ( $errorResponse );
				}
			} else {
				// Create Response on Error and break it
				$returnCode = ErrorCodes::EC_INSFFCNT_PRMTRS;
				$message = "Insufficient or Wrong ParameterGroup";
				$result->setReturnCode ( $returnCode );
				$result->setMessage ( $message );
				$result->setResponse ( array () );
			}
		} else {
			// Create Response on Error and break it
			$returnCode = ErrorCodes::EC_FUNC_NOT_SUPPORTED;
			$message = "Function Not Supported";
			$result->setReturnCode ( $returnCode );
			$result->setMessage ( $message );
			$result->setResponse ( array () );
		}
		
		// Create Request and Error Logs
		return $result;

	}
	
	/*
	 * Function to build Error Response XML
	 * 
	 */
	public function processError($returnCode, $message) {
		$response = new APIResponse ();
		$requestid = "UNKNOWN";
		$version = "UNKNOWN";
		if ((isset ( $this->requestObj )) && ($this->requestObj->parsed == TRUE)) {
			$requestid = $this->requestObj->getRequestID ();
			$version = $this->requestObj->getVersion ();
		}
		$resxml = $response->createResponseXML ( $requestid, $version, $returnCode, $message, $ary = array () );
		return $resxml;
	}
	
	/*
	 * Function to build Error Response XML
	 * 
	 */
	
	/*	function processError($requestid, $version, $returnCode, $message) {
		$response = new APIResponse ();
		$resxml = $response->responseXML ( $requestid, $version, $returnCode, $message, $ary = array () );
		return $resxml;
	}*/
	
	/*
	 * Function Runtime Error
	 */
	public static function ProcessRuntimeError($returnCode, $message) {
		$response = new APIResponse ();
		$resxml = $response->createResponseXML ( "UNKNOWN", "UNKNOWN", $returnCode, $message, $ary = array () );
		return $resxml;
	}
	
	/*
	 * Create Final Response XML
	 * 
	 */
	function createResponse($returnCode, $message, $data, $handlerName) {
		$response = new APIResponse ();
		try {
			$requestid = "UNKNOWN";
			$version = "UNKNOWN";
			if ((isset ( $this->requestObj )) && ($this->requestObj->parsed == TRUE)) {
				$requestid = $this->requestObj->getRequestID ();
				$version = $this->requestObj->getVersion ();
			}
			
			$resxml = $response->responseXML ( $requestid, $version, $returnCode, $message, $data, $this->requestObj );
		} catch ( Exception $e ) {
			$returnCode = $e->getCode ();
			$message = $e->getMessage ();
			if ($returnCode == ErrorCodes::EC_INTERNAL_SERVER_ERROR) {
				$message = $message . " " . $handlerName;
			}
			$response = $this->processError ( $returnCode, $message );
			return $response;
		}
		
		return $resxml;
	}
	
	/*
	 * Authenticate the request 
	 */
	function authenticate($requestObj) {
		$returnCode = ErrorCodes::EC_AUTH_FAILURE;
		
		$authMethod = $requestObj->getRequestAuthMethod ();
		if (empty($authMethod))	$authMethod = self::AUTHMETHOD_MESSAGEAUTH;
		
		switch($authMethod)
		{
			case self::AUTHMETHOD_COMPAUTH:
				$returnCode = $this->authenticateComponentAuth($requestObj);
				break;

			case self::AUTHMETHOD_MESSAGEAUTH:
				$returnCode = $this->authenticateMessageAuth($requestObj);
				break;

			default:
				$returnCode = ErrorCodes::EC_INVLD_AUTH_MTHD;
		}
		return $returnCode;
	}
	
	private function authenticateMessageAuth($requestObj)
	{
		$returnCode = ErrorCodes::EC_AUTH_FAILURE;
		$accesskey = $requestObj->getAccessKey ();
		$requestsignature = $requestObj->getRequestSignature ();
		$appdata = $this->getDataMethod ();
		
		// Check if authentication is required
		if ($appdata->isAccessKeyPresent ( $accesskey )) {
			if ($appdata->needAuthentication ( $accesskey )) {

				$params = array ('RequestID'=> $requestObj->getRequestID(),
								'Version'	=> $requestObj->getVersion(),								 
								'Function'  => implode('', $requestObj->getFunctionNames()),
								'AccessKey' => $requestObj->getAccessKey()
						);				
				$computedSignature = SignatureGenerator::ComputeSignatureMessageAuth($params);			
				
				if ($requestsignature == $computedSignature) {
					$returnCode = ErrorCodes::EC_SUCCESS;
				}
				else 
				{
					$returnCode = ErrorCodes::EC_AUTH_FAILURE;
				}
			}
			else 
			{
				$returnCode = ErrorCodes::EC_SUCCESS;
			}
		
		} else {
			$returnCode = ErrorCodes::EC_ACCESSKEY_NOT_FOUND;
		}
		return $returnCode;
	}
	
	private function authenticateComponentAuth($requestObj)
	{
		$httpsMode = 0;
		$returnCode = ErrorCodes::EC_AUTH_FAILURE;
		$common_obj = new CommonFunctions();
		$appdata = $this->getDataMethod ();
		$functionName = '';
		$fingerprint = '';
		
		// On windows, eventhough connecting with http mode, https will be set and will set to off mode
		if (isset($_SERVER['HTTPS']) && strcasecmp($_SERVER['HTTPS'], 'on') == 0)	$httpsMode = 1;
		
		$functionRequestObjects = $requestObj->getFunctionRequestObjects ();		
		foreach ( $functionRequestObjects as $functionRequestObj ) {
			$functionName = $functionRequestObj->getName ();
			break;
		}
		//For ComponentAuth accesskey == hostId
		$accesskey = trim($requestObj->getAccessKey ());
		
		//Check if accessKey(HostId) is present in DB
		if (! $appdata->checkFunctionExceptions ( $functionName )) {
			if(($common_obj->isValidComponent($accesskey)) === FALSE) {
				return ErrorCodes::EC_ACCESSKEY_NOT_FOUND;
			}
		}

		$requestsignature = $requestObj->getRequestSignature();
		$passphrase =  $common_obj->getPassphrase();
		if ($httpsMode)	$fingerprint = $common_obj->getCSFingerPrint();	
				
		// Check passphrase and fingerprint exits
		if (! $passphrase)	return ErrorCodes::EC_PASSPHRASE_EMPTY;				
		if ($httpsMode && ! $fingerprint)	return ErrorCodes::EC_FINGERPRINT_EMPTY;
		
		$params = array ('RequestID'=> $requestObj->getRequestID(),
						 'Version'	=> $this->version_i,								 
						 'Function' => implode('', $requestObj->getFunctionNames())
						);
		$computedSignature = SignatureGenerator::ComputeSignatureComponentAuth($passphrase, $_SERVER['REQUEST_METHOD'], '/ScoutAPI/CXAPI.php', $params, null, null, $fingerprint);

		if ($requestsignature == $computedSignature) {
			$returnCode = ErrorCodes::EC_SUCCESS;
		}
		else 
		{
			$returnCode = ErrorCodes::EC_AUTH_FAILURE;
		}
		
		return $returnCode;
	}
	
	/*
	 * Authorization
	 */
	function authorize($accesskey, $functionName) {
		$authorized = 1;
		$id = $accesskey;
		$appdata = $this->getDataMethod ();
		if ($appdata->isACLEnabled ()) {
			$auth_type = $appdata->getACLType ( $id );
			
			// Check Universal Authorization
			if (isset ( $auth_type ) && strcmp ( $auth_type, "A" ) == 0) {
				return 0;
			}
			// Check Resource Level Authorization
			$resourcecode = $appdata->getResource ( $functionName );
			if($resourcecode == null)
			{
				return 2;
			}
			$resourcepermissionList = $appdata->getResourcePermissions ( $id );
			if (isset ( $resourcecode ) && isset ( $resourcepermissionList ) && stripos ( $resourcepermissionList, $resourcecode ) !== false) {
				//do nothing
			}
            else{
                return 1;
            }
			
			// Check Function level Authorization
			$functionPermissions = $appdata->getFunctionPermissions ( $id );
			if (isset ( $functionPermissions )) {
				$functionList = explode ( ",", strtoupper ( $functionPermissions ) );
				$result = strlen(trim(array_search ( strtoupper ( $functionName ), $functionList)));
				//print_r($result);
				if ( $result >=1) {
					return 0;
				}
				else 
				{
					return 1;
				}
			}
		
		} else {
			$authorized = 0;
		}
		
		return $authorized;
	}
	
	function getDataMethod()
	{		
		if($this->call == "rx")
		{
			return new RXAPIProcessData ();
		}
		else
		{			
			return new APIProcessData ();
		}	
	}

	function getAPIRequestObj()
	{
		return $this->requestObj;
	}
}
?>