<?php
class ErrorCodeGenerator {

	private static $workflow_error_counter = 0;
	private static $precheck_error_counter = 0;

	private static $error_code = array(
		
		// range: -100 to 100
		'COMMON' => array(
			'EC_SUCCESS' 				=> array(0, 'Success'),
			'EC_INVALID_FORMAT' 		=> array(-1, 'Format Error'),
			'EC_AUTH_FAILURE' 			=> array(-2, 'Authentication Failure: #text#'),
			'EC_ATHRZTN_FAILURE' 		=> array(-3, 'Authorization Failure'),
			'EC_FUNC_NOT_SUPPORTED' 	=> array(-4, 'Function Not Supported'),
			'EC_UNKNWN' 				=> array(-6, '')
		)
	);
	
	public static function raiseError($api_name, $error_type, $place_holders = array(), $user_defined_msg = '') {
		
		self::$precheck_error_counter++;
		$commonfunctions_obj = new CommonFunctions();
		list($event_code, $err_code) = $commonfunctions_obj->getErrorCode($error_type);
		$err_response = self::frameErrorResponse($event_code, $user_defined_msg, $place_holders, 0);
		
		throw new APIException($err_response->getParameterValue("ErrorMessage"), $err_code, $err_response);
	}
	
	public static function getErrorCodeAndMessage($api_name, $error_type, $text='', $user_defined_msg='') {
		
		$api_name = strtoupper ($api_name);
		$error_type = strtoupper ($error_type);
		
		list($errCode, $errMessage) = ErrorCodeGenerator::$error_code[$api_name][$error_type];
		if($user_defined_msg)
		{
			$errMessage = $user_defined_msg;
		}
		elseif($text)
		{
			$errMessage = str_replace('#text#', $text, $errMessage);
		}
		return array($errCode, $errMessage);
	}	
	
	public static function getErrorCode($api_name, $error_type) {
		
		$api_name = strtoupper ($api_name);
		$error_type = strtoupper ($error_type);
		
		list($errCode, $errMessage) = ErrorCodeGenerator::$error_code[$api_name][$error_type];		
		return $errCode;
	}

	public static function frameErrorResponse($event_code, $error_message, $place_holders = array(), $error_type = null, $is_workflow_error = 1, $pgid = 0, $is_it_health = 0, $installer_extended_errors = null)
	{
		if ($is_workflow_error)	self::$workflow_error_counter++;
		
		$commonfunctions_obj = new CommonFunctions();
		$error_details = $commonfunctions_obj->getErrorDetails($event_code, $place_holders, $error_type);
	
		$response = new ParameterGroup ( "ErrorDetails" );
		if ($pgid)
		{
			$response = new ParameterGroup ( $event_code );
			$response->setParameterNameValuePair( "ErrorCode", empty($error_details['apiCode']) ?  $event_code : $error_details['apiCode']);
		}
		else
		{
			$response->setParameterNameValuePair( "ErrorCode", $event_code );
		}
		
		if($is_it_health)
		{
			$response->setParameterNameValuePair( "DetectionTime", "-1" );
		}
		
		$response->setParameterNameValuePair( "ErrorMessage", empty($error_details['errorMsg']) ? $error_message : $error_details['errorMsg'] );
		$response->setParameterNameValuePair( "PossibleCauses", isset($error_details['errPossibleCauses']) ? $error_details['errPossibleCauses'] : "" );
		$response->setParameterNameValuePair( "Recommendation", isset($error_details['errCorrectiveAction']) ? $error_details['errCorrectiveAction'] : "" );
		
		if(isset($installer_extended_errors))
		{
			$response->setParameterNameValuePair( "InstallerExtendedErrors", isset($installer_extended_errors) ? $installer_extended_errors : "" );
		}
		
		$place_holder_group= new ParameterGroup ( "PlaceHolders" );
		if (is_array($place_holders))
		{
			foreach($place_holders  as $key => $value)
			{
				$place_holder_group->setParameterNameValuePair($key, $value);
			}
		}
		
		$response->addParameterGroup($place_holder_group);		
		return $response;
	}
	
	public static function getWorkFlowErrorCounter()
	{
		return self::$workflow_error_counter;
	}

	public static function getPreCheckErrorCounter()
	{
		return self::$precheck_error_counter;
	}
}
?>