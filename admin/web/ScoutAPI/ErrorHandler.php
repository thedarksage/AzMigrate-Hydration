<?php
class ErrorHandler extends ResourceHandler 
{

	private $volume_obj;
	private $conn;
	
	public function ErrorHandler() 
	{
		global $conn_obj;
	
		$this->volume_obj = new VolumeProtection();
		$this->conn = $conn_obj;
	}
	
	public function GetErrorLogPathForSystemGUID($identity, $version, $functionName, $parameters,$flag=0) 
	{
		if(!$flag)
		{
			$obj = $parameters['SystemGUID'];
			$sysGUID = trim($obj->getValue());
			$host_id = $this->checkSystemGUID($this->conn,$sysGUID);
		}
		else
		{
			$obj = $parameters['AgentGUID'];
			$host_id = trim($obj->getValue());
			$host_id = $this->checkAgentGUID($this->conn,$host_id);
		}
		$result = $this->volume_obj->GetErrorLogPathForSystemGUID($host_id);
		$response = new ParameterGroup ( "Response" );
		if($result)
		{
			$response->setParameterNameValuePair("ErrorLogPathName",$result);			
		}
		return $response->getChildList();
		
	}

	public function GetErrorLogPathForSystemGUID_validate($identity, $version, $functionName, $parameters,$flag=0) 
	{

		if(!$flag)
		{
			if(isset($parameters['SystemGUID']))
			{
				$obj = $parameters['SystemGUID'];
				$systemGUID = trim($obj->getValue());
				if(!trim($obj->getValue()))
				{
					$this->raiseException(ErrorCodes::EC_NO_DATA,"Invalid Value for SystemGUID");
				}
			}
			else
			{
				$this->raiseException(ErrorCodes::EC_INSFFCNT_PRMTRS,"Missing SystemGUID Parameter");
			}
		}
		else
		{
			if(isset($parameters['AgentGUID']))
			{
				$obj = $parameters['AgentGUID'];
				if(!trim($obj->getValue()))
				{
					$this->raiseException(ErrorCodes::EC_NO_DATA,"Invalid Value for AgentGUID");
				}
			}
			else
			{
				$this->raiseException(ErrorCodes::EC_INSFFCNT_PRMTRS,"Missing AgentGUID Parameter");
			}
		}
	}
	
	public function GetErrorLogPathForAgentGUID_validate($identity, $version, $functionName, $parameters)
	{
		$this->GetErrorLogPathForSystemGUID_validate($identity, $version, $functionName, $parameters,1);
	}
	
	public function GetErrorLogPathForAgentGUID($identity, $version, $functionName, $parameters)
	{
		return $this->GetErrorLogPathForSystemGUID($identity, $version, $functionName, $parameters,1);
	}
	
	public function NotifyError_validate($identity, $version, $functionName, $parameters)
	{
		global $SCOUT_API_PATH, $SLASH;
		
		if(!isset($parameters['APIName'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "APIName"));
		if(!$parameters['APIName']->getValue()) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "APIName"));
		$api_name = $parameters['APIName']->getValue();		
        if($api_name == "GetRequestStatus")
        {
            if(!isset($parameters['Context'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Context"));
            $context = $parameters['Context']->getValue();
            if(!$context) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => "Context"));
            /*
            $context_array = explode("/=>/", $context);
            $api_request_id = $context_array['RequestId'];
            if(!$api_request_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Context"));
            $valid_api_request_id = $this->conn->sql_get_value("apiRequest", "count(*)", "apiRequestId = ?", array($api_request_id));
            if(!$valid_api_request_id) ErrorCodeGenerator::raiseError('COMMON', 'EC_INVALID_DATA', array('Parameter' => "Context"));
            */
        }
	}
	
	public function NotifyError($identity, $version, $functionName, $parameters)
	{
		try
		{
			$api_name = $parameters['APIName']->getValue();
			
            // Return the success response
			$api_response = new ParameterGroup("Response");
            
            $valid_api_list = array("ListInfrastructure", "ListScoutComponents", "GetProtectionDetails", "GetRequestStatus");
            //$valid_api_list = array("ListInfrastructure", "ListScoutComponents", "GetProtectionDetails", "ListConsistencyPoints", "GetRequestStatus");
            if(!in_array($api_name, $valid_api_list)) return $api_response->getChildList();
            
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
            $request_list = array();
            if(isset($parameters['Context']))
            {
                $context = $parameters['Context']->getValue();
                list($key, $api_request_id) = explode("=>", $context);
				$req_grp = new ParameterGroup("RequestIdList");
                $req_grp->setParameterNameValuePair("RequestId", $api_request_id);				
				$request_list['RequestIdList'] = $req_grp;
            }
			
			$result = new Result ();			
			$handler = HandlerFactory::getHandler ( $version, $api_name );
			$handlerName = "Unknown";
			if (isset ( $handler )) {
				$handlerName = get_class ( $handler );
				$paramList = $request_list;
				if ($handler->validate ( $accesskey, $version, $paramList )) {
					try {					
						$data = $handler->process ( $identity, $version, $api_name, $paramList );
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
			$this->resultList [] = $result;
			$response = new APIResponse ();
			$returnCode = ErrorCodes::EC_SUCCESS;
			$message = "Success";
			$xml = $response->getResponseXML ( $requestid, $version, $api_name, $returnCode, $message, $this->resultList );
			
			// Update the XML to MDS table.
            $mds_data_array = array();
			$mds_obj = new MDSErrorLogging();
			
            $mds_data_array["jobId"] = '';
            $mds_data_array["jobType"] = "DRA Error Update";
            $mds_data_array["errorString"] = $xml;
            $mds_data_array["eventName"] = "DRA";
            $mds_data_array["errorType"] = "ERROR";
			
            $mds_obj->saveMDSLogDetails($mds_data_array);
			
			// Disable Exceptions for DB
			$this->conn->sql_commit();
			$this->conn->disable_exceptions();
			return $api_response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}		
	}
};
?>