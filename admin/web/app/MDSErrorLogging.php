<?php
 /*
 *$Header: /src/admin/web/app/MDSErrorLogging.php,v 1.17.2.2 2017/06/27 13:44:29 srtulasi Exp $
 *================================================================= 
 * FILENAME
 *    MDSErrorLogging.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    MDS logging
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
class MDSErrorLogging
{
    private $conn_obj;
		
   	function __construct()
	{
		global $conn_obj;
		$this->conn_obj = $conn_obj;
		$this->mds_obj = new MDSErrors();
		$this->com_func = new CommonFunctions();
	}
	public function getErrorLogs($log_id ='')
	{
		$args = array();
		if (!$log_id)
		$log_id = 0;
		
		$log_limit = $this->conn_obj->sql_get_value('transbandSettings', 'ValueData', "ValueName = ?", array('MDS_LOG_LIMIT'));
		array_push($args, $log_id);
		
		$sql = "SELECT  
						activityId as ActivityId,
						clientRequestId as ClientId,
						eventName as EventName,
						errorCode as ErrorCode,
						errorCodeName as ErrorCodeName,
						errorType as ErrorType,        
						errorDetails as ErrorDetails,
                        componentName as ComponentName,
						callingFunction as CallingFunction    
					FROM
						MDSLogging
						WHERE logId > ? LIMIT $log_limit";
		$log_details = $this->conn_obj->sql_query($sql, $args);
		return $log_details;
	}
	
	public function removeErrorLogs($log_id)
	{
		$sql = "DELETE 
					FROM
				MDSLogging
					WHERE	
						logId <= ?";	
		$this->conn_obj->sql_query($sql, array($log_id));				
	}
	
	public function getToken($log_id ='')
	{
		$args = array();
		
		if (!$log_id)
		$log_id = 0;
		
		$log_limit = $this->conn_obj->sql_get_value('transbandSettings', 'ValueData', "ValueName = ?", array('MDS_LOG_LIMIT'));
		array_push($args, $log_id);
				
		$sql =  "SELECT  MAX(logId) as id FROM (select logId FROM MDSLogging WHERE logId > ? LIMIT $log_limit)as maxid";
		$log_id = $this->conn_obj->sql_query($sql, $args);
		return $log_id[0]['id'];
	}
	
	public function saveMDSLogDetails($mds_array = array())
	{
        global $CS_IP, $HOST_GUID;
		$CS_ID = substr_replace($HOST_GUID, '#####', -5, 5);
		
		$job_id			= isset($mds_array["jobId"]) ? $mds_array["jobId"] : '';
		$activity_id 	= isset($mds_array["activityId"]) ? $mds_array["activityId"] : '';
		$job_type 		= isset($mds_array["jobType"]) ? $mds_array["jobType"] : '';
		$error_string 	= isset($mds_array["errorString"]) ? $mds_array["errorString"] : '';
		$event_name 	= isset($mds_array["eventName"]) ? $mds_array["eventName"] : '';
		$error_type 	= isset($mds_array["errorType"]) ? $mds_array["errorType"] : 'ERROR';
		
		$client_request_id = $component_name = $calling_function = $error_code = $error_code_name = '';		
		$request_type = array();

        if($job_id && $job_type)
        {
			$request_type = $this->mds_obj->getAPINames($job_type);
            $result = $this->getClientId($job_id, $request_type);
            
			$activity_id = $result["activityId"];
			$client_request_id = $result["clientRequestId"];
        }
		
		if ($job_type)
		{
			$error_details = $this->getErrorDetails($job_type);
			$error_code = $error_details["ErrorCode"];
			$error_code_name = $error_details["ErrorName"];		
		}
		
		$log_string = "";
		preg_match('/\<AccessKeyID\>(.*?)\<\/AccessKeyID\>/', $error_string, $match);
		if (count($match) > 0)
		{
			$actual_string = $match[1];
			$full_string = substr_replace($actual_string, "#####",-5, 5);
			$log_string = preg_replace("/$actual_string/sm", $full_string, $error_string);
		}
		else
		{
			$log_string = $error_string;
		}
		
		preg_match('/\<AccessSignature\>(.*?)\<\/AccessSignature\>/', $log_string, $match);
		if (count($match) > 0)
		{
			$actual_string = $match[1];
			$log_string = preg_replace("/$actual_string/sm", "#####", $log_string);
		}
		
		# Removing the non-ascii characters from the updates		
		$error_string = preg_replace('/[^\x0A\x20-\x7E]/','',$log_string);
		$ver_arr = $this->com_func->get_inmage_version_array();
		$CS_VER = "";
		if (is_array($ver_arr))
		{
			if (count($ver_arr) > 0)
			{
				$CS_VER = $ver_arr[0];
			}
		}
		$num_chunks = $this->getChunks($error_string);
		
		for($i = 0; $i < $num_chunks; $i++)
		{
			$j = $i+1;
			$error_sub_string = substr($error_string , $i*8*1024 , 8*1024);
			$mds_error_log = "\n"."Sequence:" .$j. ", Date: (".date('Y-m-d H:i:s') .":". substr(microtime(), 2, 6)."), CS_IP: $CS_IP, CS_GUID - $CS_ID, CS_VER - $CS_VER\n" .$error_sub_string;
		    $sql_args = array($activity_id, $client_request_id, $event_name, $error_code, $error_code_name, $error_type, $mds_error_log, $component_name, $calling_function);
			
			$sql = "INSERT INTO  
						MDSLogging
						(
						activityId,
						clientRequestId,
						eventName,
						errorCode,
						errorCodeName,
						errorType,
						errorDetails, 
						componentName, 
						callingFunction,
						logCreationDate					
						)
						VALUES
						(
						?,
						?, 
						?, 
						?, 
						?, 
						?, 
						?,
						?,
						?,
						now()
						)";
			$this->conn_obj->sql_query($sql, $sql_args);
		}
	}
	
	public function getClientId($reference_id, $request_type)
	{
		$request_data = implode(",", $request_type);
		$sql = "SELECT activityId,clientRequestId FROM apiRequest  WHERE referenceId Like ? AND FIND_IN_SET(requestType, ?) ORDER BY apiRequestId DESC LIMIT 1"; 
		$result = $this->conn_obj->sql_query($sql, array("%,$reference_id,%", $request_data));
		return $result[0];
	}
	
	public function getChunks($error_string)
	{
		$error_string_length = strlen($error_string);
		$num_chunks = $error_string_length/(8 *1024);
		$num_chunks = ceil($num_chunks);
		
		return $num_chunks;
	}
	
	public function getErrorDetails($api_name)
	{
		global $DISCOVERY_FAILED_ERROR_CODE, $MOBILITY_SERVICE_FAILED_ERROR_CODE, $PROTECTION_FAILED_ERROR_CODE, $ROLLBACK_FAILED_ERROR_CODE, $DISCOVERY_FAILED_ERROR_CODE_NAME, $MOBILITY_SERVICE_FAILED_ERROR_CODE_NAME, $PROTECTION_FAILED_ERROR_CODE_NAME, $ROLLBACK_FAILED_ERROR_CODE_NAME, $API_FAILED_ERROR_CODE, $API_FAILED_ERROR_CODE_NAME, $PROTECTION_TIMEOUT_ERROR_CODE_NAME, $PROTECTION_TIMEOUT_ERROR_CODE, $UNREGISTRATION_ERROR_CODE, $UNREGISTRATION_ERROR_CODE_NAME, $REGISTRATION_ERROR_CODE, $REGISTRATION_ERROR_CODE_NAME, $MIGRATION_ERROR_CODE, $MIGRATION_ERROR_CODE_NAME;
		
		$api_codes = array(
							"Discovery" => array("ErrorCode" => $DISCOVERY_FAILED_ERROR_CODE, "ErrorName" => $DISCOVERY_FAILED_ERROR_CODE_NAME),
							"PushInstall" => array("ErrorCode" => $MOBILITY_SERVICE_FAILED_ERROR_CODE, "ErrorName" => $MOBILITY_SERVICE_FAILED_ERROR_CODE_NAME),
							"Protection" => array("ErrorCode" => $PROTECTION_FAILED_ERROR_CODE, "ErrorName" => $PROTECTION_FAILED_ERROR_CODE_NAME),
							"Rollback" => array("ErrorCode" => $ROLLBACK_FAILED_ERROR_CODE, "ErrorName" => $ROLLBACK_FAILED_ERROR_CODE_NAME),
							"APIErrors" => array("ErrorCode" => $API_FAILED_ERROR_CODE, "ErrorName" => $API_FAILED_ERROR_CODE_NAME),
							"ProtectionTimeout" => array("ErrorCode" => $PROTECTION_TIMEOUT_ERROR_CODE, "ErrorName" => $PROTECTION_TIMEOUT_ERROR_CODE_NAME),
							"Unregistration" => array("ErrorCode" => $UNREGISTRATION_ERROR_CODE, "ErrorName" => $UNREGISTRATION_ERROR_CODE_NAME),
							"Registration" => array("ErrorCode" => $REGISTRATION_ERROR_CODE, "ErrorName" => $REGISTRATION_ERROR_CODE_NAME),
                            "MigrateACSToAAD" => array("ErrorCode" => $MIGRATION_ERROR_CODE, "ErrorName" => $MIGRATION_ERROR_CODE_NAME)
						);
							
		return 	isset($api_codes[$api_name]) ? $api_codes[$api_name] : array('ErrorCode' => 'UNKNOWN', 'ErrorName' => 'Error not found for ' . $api_name);
	}
	
};	
?>	