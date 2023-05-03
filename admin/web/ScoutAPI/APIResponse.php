<?php
class APIResponse {
	
	/**
	 * Create Response XML
	 * @param String $request_id
	 * @param String $version
	 * @param Integer $returnCode
	 * @param String $message
	 * @param array $resultList
	 * @return string
	 */
	public function createResponseXML($request_id, $version, $returnCode, $message, $resultList = array()) {
		$message = strip_tags($message);
		$response1 = "<Response ID=\"" . $request_id . "\" Version=\"" . $version . "\"" . " Returncode=\"" . $returnCode . "\" Message=\"" . $message . "\">";
		if (count ( $resultList ) > 0) {
			// Do something
			$response1 .= "<Body>";
			foreach ( $resultList as $result ) {
				$response1 .= $this->responseXML ( $result );
			}
			
			$response1 .= "</Body>";
		}
		
		$response1 = $response1 . "\n</Response>";
		return $response1;
	}
	
	/**
	 * Return Function XML for each Request Object
	 * @param String $request_id
	 * @param String $version
	 * @param String $returnCode
	 * @param String $message
	 * @param String data corresponding data $data
	 * @param String $reqobj
	 * @return string
	 */
	private function responseXML($result) {
		
		$functionRequestObj = $result->getRequest ();
		$functionResponseObj = $result->getResponse ();
		
		$functionName = $functionRequestObj->getName ();
		$functionRequestid = $functionRequestObj->getID ();
		$returnCode = $result->getReturnCode ();
		$message = $result->getMessage ();
		$data = $functionResponseObj;
		
		// Commented the below lines and merged Response and return codes in a single element
		$response1 = "<Function Name=\"$functionName\" Id=\"$functionRequestid\" Returncode=\"$returnCode\" Message=\"$message\">";
		
		// Check if function request is required
		

		if ($functionRequestObj->getInclude ()) {
			//$response1 = $response1 . $this->functionRequestXML ( $functionRequestObj );
		}
		
		if (isset ( $data ) && count ( $data ) > 0) {
			$response1 = $response1 . $this->functionResponseXML ( $data, $returnCode );
		}
		
		$response1 .= "\n</Function>";
		
		return $response1;
	}
	
	/*
	 * Return XML Response fragement for given data
	 */
	private function functionResponseXML($data, $returnCode) 
	{
		global $requestXML, $commonfunctions_obj, $logging_obj;
		// If returnCode is non-zero (failure state), do not add FunctionResponse tag.
		$xml = "\n" . ($returnCode ? "" : "<FunctionResponse>");
		global $API_ERROR_LOG;
		if (is_array ( $data )) 
		{
			foreach ( $data as $key => $val ) 
			{
				if (get_class ( $val ) == "ParameterGroup" || get_class ( $val ) == "Parameter") 
				{
					$xml = $xml . $val->getXMLString ();
				} 
				else
				{
					//Stop logging for complete response XML for credentials related output, which is ListAccounts. CreateAccount, ModifyAccount responses doesn't contain any credentials and always response XML output as success/failure.
					if (preg_match("/Name=\"ListAccounts\"/i", $requestXML) !== 1)
					{
						$log_string = $commonfunctions_obj->mask_authentication_headers($requestXML);
						$log_string = $commonfunctions_obj->mask_credentials($log_string);
						debug_access_log($API_ERROR_LOG, "Wrong datatype passed by Handler: request XML: $log_string, data = $data, return code = $returnCode\n".print_r($data,TRUE));
						
						$logging_obj->my_error_handler("INFO",array("RequestXml"=>$log_string, "Data"=>print_r($data,TRUE), "ReturnCode"=>$returnCode, "Reason"=>"Wrong data type passed by handler"),Log::APPINSIGHTS);
					}
					throw new Exception ( "Wrong datatype passed by Handler: " . get_class ( $val ), ErrorCodes::EC_INTERNAL_SERVER_ERROR );
				}
			}
		} 
		else if ((get_class ( $data ) == "ParameterGroup") || (get_class ( $data ) == "Parameter")) 
		{
			$xml = $xml . $data->getXMLString ();
		} 
		else
		{
			if (preg_match("/Name=\"ListAccounts\"/i", $requestXML) !== 1)
			{
				$log_string = $commonfunctions_obj->mask_authentication_headers($requestXML);
				$log_string = $commonfunctions_obj->mask_credentials($log_string);
				debug_access_log($API_ERROR_LOG, "Wrong datatype passed by Handler: request XML: $log_string, data = $data, return code = $returnCode\n".print_r($data,TRUE));		
				$logging_obj->my_error_handler("INFO",array("RequestXml"=>$log_string, "Data"=>print_r($data,TRUE), "ReturnCode"=>$returnCode, "Reason"=>"Wrong data type passed by handler"),Log::APPINSIGHTS);
			}
			throw new Exception ( "Wrong datatype passed by Handler : " . get_class ( $data ), ErrorCodes::EC_INTERNAL_SERVER_ERROR );
		}
		
		$xml = $xml . "\n" . ($returnCode ? "" : "</FunctionResponse>");
		return $xml;
	}
	
	/*
	 * Returns the function response Data XML
	 */
	private function functionRequestXML($functionRequestObj) {
		
		$data = $functionRequestObj->getChildList ();
		// Create a Function Request Object
		

		$functionName = $functionRequestObj->getName ();
		$id = $functionRequestObj->getID ();
		$xml = "<FunctionRequest Name=\"$functionName\" Id=\"$id\" include=\"true\">";
		if (is_array ( $data )) {
			foreach ( $data as $key => $val ) {
				if (get_class ( $val ) == "ParameterGroup" || get_class ( $val ) == "Parameter") {
					$xml = $xml . $val->getXMLString ();
				} else {
					throw new Exception ( "Wrong datatype passed by Handler: " . get_class ( $val ), ErrorCodes::EC_INTERNAL_SERVER_ERROR );
				}
			}
		} else if ((get_class ( $data ) == "ParameterGroup") || (get_class ( $data ) == "Parameter")) {
			$xml = $xml . $data->getXMLString ();
		} else {
			throw new Exception ( "Wrong datatype passed by Handler : " . get_class ( $data ), ErrorCodes::EC_INTERNAL_SERVER_ERROR );
		}
		$xml = $xml . "\n</FunctionRequest>";
		return $xml;
	
		//"IncludeInResponse";
	}

	/**
	 * Return Function XML for each Request Object
	 * @param String $request_id
	 * @param String $version
	 * @param String $returnCode
	 * @param String $message
	 * @param String data corresponding data $data
	 * @param String $reqobj
	 * @return string
	 */
	public function getResponseXML($request_id, $version, $functionName, $returnCode, $message, $resultList = array()) 
	{	
		$message = strip_tags($message);
		if (count ( $resultList ) > 0) {
			// Do something
			foreach ( $resultList as $result ) {
				$functionResponseObj = $result->getResponse ();		
				$returnCode = $result->getReturnCode ();
				$message = $result->getMessage ();
				$data = $functionResponseObj;
				
				if (isset ( $data ) && count ( $data ) > 0) {
					$response1 = $response1 . $this->functionResponseXML ( $data, $returnCode );
				}
			}
		}
		return $response1;
	}
}
?>