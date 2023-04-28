<?php
ob_start();
#error_reporting(E_ALL);
#ini_set('display_errors', '1');
include_once("../config.php");
include_once("../app/VolumeProtection.php");
include_once("../app/CommonFunctions.php");
include_once("../app/ProtectionPlan.php");
include_once("../app/Application.php");
include_once("../app/ProcessServer.php");
include_once("../config/TemplateFactory.php");
include_once("../config/Template.php");
include_once("../config/ReplicationOptions.php");
include_once '../app/System.php';
include_once '../app/UI.php';
include_once '../app/Recovery.php';
include_once('../app/SparseRetention.php');
include_once '../app/Retention.php';
include_once("../app/Export.php");
include_once("../app/SRM.php");
include_once("../app/PushInstall.php");
include_once("../app/FileProtection.php");
include_once("../app/Policy.php");
include_once("../app/ESXDiscovery.php");
include_once('csapijson.php');

global $requestXML, $activityId, $clientRequestId, $srsActivityId;
global $REPLICATION_ROOT , $SLASH, $CXAPI_LOG, $API_MANAGER_LOG, $API_ERROR_LOG, $commonfunctions_obj, $HOST_GUID, $logging_obj,$CS_IP;
global $ENABLE_API_PROFILE, $ASRAPI_APPINSIGHTS_LOG;
$reporting_api_list = array('ListInfrastructure','ListScoutComponents','ProtectionReadinessCheck','GetProtectionState','GetProtectionDetails','ListConsistencyPoints','RollbackReadinessCheck','GetRollbackState','ListEvents','GetCSPSHealth','ListPlans','NotifyError','GetHostInfo','RefreshHostInfo','ListAccounts');
$ux_event_api_list = array('InfrastructureDiscovery','UpdateInfrastructureDiscovery','RemoveInfrastructure','InstallMobilityService','UpdateMobilityService','UnInstallMobilityService','SetPSNatIP','CreateProtection','RestartResync','CreateRollback','CleanRollbackPlans','ModifyProtection','StopProtection','ModifyProtectionProperties','SetVPNDetails','PSFailover','CreateProtectionV2','ModifyProtectionV2','ModifyProtectionPropertiesV2','RenewCertificate','GetCsRegistry','GetCxPsConfiguration','CreateAccount','UpdateAccount','RemoveAccount','UnregisterAgent',"MachineRegistrationToRcm","FinalReplicationCycle","MobilityAgentSwitchToRcm","ResumeReplicationToCs");
$status_api_list = array('GetRequestStatus');
$telemetry_api_list = array('InfrastructureDiscovery','UpdateInfrastructureDiscovery','RemoveInfrastructure','InstallMobilityService','UpdateMobilityService','UnInstallMobilityService','CreateProtection','RestartResync','CreateRollback','CleanRollbackPlans','ModifyProtection','StopProtection','ModifyProtectionProperties','PSFailover','CreateProtectionV2','ModifyProtectionV2','ModifyProtectionPropertiesV2','RenewCertificate','GetCsRegistry','GetCxPsConfiguration','CreateAccount','UpdateAccount','RemoveAccount','RollbackReadinessCheck','GetRollbackState','UnregisterAgent','GetHostInfo','RefreshHostInfo','ListAccounts','MigrateACSToAAD','InsertScriptPolicy');

$logging_obj->appinsights_log_file = $ASRAPI_APPINSIGHTS_LOG;
	
$no_logging_apis = array("UpdateSql", "SchedulerUpdate");
if($_SERVER['HTTP_ACTIVITYID']) $activityId = $_SERVER['HTTP_ACTIVITYID'];
else $activityId = '';

if($_SERVER['HTTP_CLIENTREQUESTID']) $clientRequestId = $_SERVER['HTTP_CLIENTREQUESTID'];
else $clientRequestId = '';

if($_SERVER['HTTP_SERVICEACTIVITYID']) $srsActivityId = $_SERVER['HTTP_SERVICEACTIVITYID'];
else $srsActivityId = '';

$cs_version_array = $commonfunctions_obj->get_inmage_version_array();
$GLOBALS['record_meta_data']	= array("CSGUID"=>$HOST_GUID,
										"CSIP"=> $CS_IP,
										"CSVERSION"=>$cs_version_array[0],
										"CLIENTREQUESTID"=>$clientRequestId,
										"ActivityId"=>$activityId,
										"SrsActivityId"=>$srsActivityId);
// Set the XML internal errors
libxml_use_internal_errors(true);

if (false !== strpos($_SERVER['HTTP_ACCEPT'], 'json')) 
{
	ob_end_clean();
    $csApiJson = @new csApiJson();
    $csApiJson->process_request();
} 
else 
{
	$start_time = time();
    $requestXML = file_get_contents ( "php://input" );
	$log_api_name = get_api_name($requestXML);
    $GLOBALS['record_meta_data']["RecordType"]=$log_api_name;
    if(!$requestXML && $_POST['Content']) $requestXML = $_POST['Content'];
    if (strlen ( $requestXML ) == 0) $requestXML = getTestXMLRequest ();
    
    // Process Request
    set_error_handler ( 'errorHandler' );

	// Validate IPAddress for DRA API requests when DRA_ALLOW_LOCALHOST_COMMUNICATION flag is set in the amethyst.conf
    if (isset($DRA_ALLOW_LOCALHOST_COMMUNICATION) && $DRA_ALLOW_LOCALHOST_COMMUNICATION == "1" && in_array($log_api_name, $DRA_API_LIST))
    {
        // Check if the request is from localhost or not
        if(!in_array(strtolower(trim($_SERVER['REMOTE_ADDR'])), $WHITE_LIST_IP_ARR))
		{
            $error_msg = "Unauthorized IPAddress ".$_SERVER['REMOTE_ADDR']." is found in the ".$log_api_name." request";
            // Throw 401 error, if above validation fails.
            $logging_obj->my_error_handler("INFO",array("Status"=>"Fail", "Result"=>401, "Reason"=>$error_msg),Log::BOTH);
            header("HTTP/1.1 401 Unauthorized : ".$error_msg, TRUE, 401);
            header('Content-Type: text/plain');
            exit(0);
        }
    }

	$api_name = null;
    $xml = getResponseXML ( $requestXML, $api_name );
	$end_time = time();
	$diff_timestamp = $end_time - $start_time;
	$buffer_data = ob_get_contents();
	if(trim($buffer_data)) 
	{
		$logging_obj->my_error_handler("INFO",array("BufferData"=>$buffer_data),Log::APPINSIGHTS);	
		debug_access_log($API_ERROR_LOG, "Buffer data::$buffer_data");
	}
	ob_end_clean();
	$api_log_string = "API: $api_name, Start: $start_time, End: $end_time, Duration: $diff_timestamp";
	
	if ($ENABLE_API_PROFILE == "1")
	{	
		$mem_consumed = memory_get_peak_usage(true);
		$mem_consumed = $mem_consumed / (1024 *1024);
		$mem_consumed = round($mem_consumed, 2);
		$api_log_string = $api_log_string.", Memory: $mem_consumed MB";
	}
	debug_access_log($API_MANAGER_LOG, $api_log_string);
    print_r ( $xml );
}

/**
 * Returns a sample RequestXML for Test
 * @return string
 */
function getTestXMLRequest() {
	
	// Retrieve Request XML form _SERVER
	$requestXML = "<Request Id=\"XXXXX\" Version=\"1.0\">
	    	 <Header>
    	  			<Authentication>
	    		 		<AccessKeyID>DDC9525FF275C104EFA1DFFD528BD0145F903CB1</AccessKeyID>
             			<AccessSignature>Yzk0YjE0YzUwZGE1MmE1YWViYjFjMWQ5YWIzNzEwNjgyOGFhYWE4NQ==</AccessSignature>
	     				<Parameter Name=\"Test\" Value=\"TestValue\"/>
	    			</Authentication>
					<ParameterGroup Id=\"XYZE\">
	    		 		<Parameter Name=\"123\" Value=\"TestValue123\"/> 
	    		 	</ParameterGroup>	       
     		</Header>
			<Body>
				<FunctionRequest Name=\"ListVolumeMonitoring\" Id=\"1\" Include=\"Y\">
					<Parameter Name=\"423\" Value=\"TestValue4444\"/> 
					<Parameter Name=\"111\" Value=\"TestValue1111\"/>
					<ParameterGroup Id=\"XYZ\">
						<Parameter Name=\"abc\" Value=\"Value555\"/>  
						<ParameterGroup Id=\"XYZ1\">
							<Parameter Name=\"abc1\" Value=\"Value666\"/>  
						</ParameterGroup>
					</ParameterGroup>
				</FunctionRequest> 
				<FunctionRequest Name=\"ListVolumes\" Id=\"2\" Include=\"Y\">
					<Parameter Name=\"423\" Value=\"TestValue4444\"/> 
					<Parameter Name=\"111\" Value=\"TestValue1111\"/> 
					<ParameterGroup Id=\"XYZ\">
						<Parameter Name=\"abc\" Value=\"Value555\"/>  
						<ParameterGroup Id=\"XYZ1\">
							<Parameter Name=\"abc1\" Value=\"Value666\"/>  
						</ParameterGroup>
					</ParameterGroup>
				</FunctionRequest> 				
			</Body>
		</Request>";
	
	return $requestXML;

}

/**
 * Returns Response XML for the corresponding Request XML
 * @param String $requestXML
 * @return string 
 */
function getResponseXML($requestXML, &$api_name) {
	// Call Main Controller
	global $MDS_ACTIVITY_ID, $activityId, $logging_obj, $commonfunctions_obj;
	$api_name = $error_message = '';
	$write_to_api_log = true;
	$write_to_mds_log = false;
	$controller = new APIController ();	

	try {
		$responseXML = $controller->doProcess ( $requestXML);
	}
	catch ( XMLException $xml_exception ) {
		$write_to_api_log = false;
		$write_to_mds_log = true;
		$responseXML = APIController::ProcessRuntimeError ( $xml_exception->getCode (), $xml_exception->getMessage () );
		$error_message = "Request XML Error:" . PHP_EOL . getXMLErrors() . PHP_EOL;
	}
	catch ( SecurityException $security_exception ) {
		$write_to_api_log = false;
		$write_to_mds_log = true;	
		$responseXML = $controller->processError ( $security_exception->getCode (), $security_exception->getMessage () );
	}
	catch ( Exception $e ) {
	
		$trace_string = $e->getTraceAsString();
		$get_file = $e->getFile();
		$get_line = $e->getLine();
		$get_msg = $e->getMessage();
		$get_code = $e->getCode();
		$error_message = "Trace string::".$trace_string."---File::".$get_file."---Line::".$get_line."---Message:".$get_msg."Exception Code:".$get_code;
		$write_to_api_log = $write_to_mds_log = true;
		$responseXML = APIController::ProcessRuntimeError ( ErrorCodes::EC_INTERNAL_SERVER_ERROR, $e->getMessage ());
	}
	
	try{
		/// Verifying for valid XML response
		if (! simplexml_load_string ( $responseXML )){
			$write_to_api_log = $write_to_mds_log = true;
			$error_message .= "Response XML Error:" . PHP_EOL . getXMLErrors(). PHP_EOL;
		}

		/// Push to MDS
		if (ErrorCodeGenerator::getPreCheckErrorCounter() > 0 || $write_to_mds_log || ErrorCodeGenerator::getWorkFlowErrorCounter() > 0)
		{
			global $asycRequestXML;
			$mds_obj = new MDSErrorLogging();
			if($activityId)
			{
				$activity_id_to_log = $activityId;
			}
			else
			{
				$activity_id_to_log = $MDS_ACTIVITY_ID;
			}
			$log_string = $commonfunctions_obj->mask_authentication_headers($requestXML);
			$log_string = $commonfunctions_obj->mask_credentials($log_string);
			$details_arr = array(
									'activityId' 	=> $activity_id_to_log,
									'jobType' 		=> 'APIErrors',
									'errorString'	=> "Request XML -- ".($asycRequestXML ? $asycRequestXML : $log_string) . PHP_EOL . "Response XMl -- ". $responseXML . PHP_EOL . $error_message,
									'eventName'		=> 'CS'
								);
			$caller_info = $commonfunctions_obj->get_debug_back_trace();
			$logging_obj->my_error_handler("INFO",array("RequestXML"=>$log_string,"ResponseXML"=>$responseXML,"ErrorString"=>$error_message,"CallerInfo"=>$caller_info),Log::BOTH);	
			$mds_obj->saveMDSLogDetails($details_arr);
		}
		
		/// Finally write to api log file
		if ($write_to_api_log)
		{
			$api_names_array = $controller->getAPIRequestObj()->getFunctionNames();
			$api_name = $api_names_array[0];
			log_asr_api($requestXML, $responseXML, $api_name);
		}
	}
	catch ( Exception $e ) {
		/// All exceptions will be logged to API_ERROR_LOG by errorHandler. So nothing to do here
	}
	
	return $responseXML;
}

/**
 * Returns XML error string captured during parsing
 * @return string 
 */
function getXMLErrors(){
	
	$error_log = "";
	$xml_errors = libxml_get_errors();
	
	// Capturing only the error object, rather traversing to the parent
	$error = current($xml_errors);
	$error_log .= "Error: " . trim($error->message) . "; on Line: " . $error->line . " at Column: " . $error->column . PHP_EOL;

	libxml_clear_errors();
	return $error_log;	
}

/**
 * Callback handler to handle errors
 * @param String $number
 * @param String $string
 * @param String $file
 * @param String $line
 * @param String $context
 * @throws Exception
 * @return boolean
 */
function errorHandler($number, $string, $file = 'Unknown', $line = 0, $context = array()) 
{
	global $API_ERROR_LOG, $commonfunctions_obj, $logging_obj, $requestXML;
	
	if (($number == E_NOTICE) || ($number == E_STRICT))
		return false;
	
	if(defined('E_DEPRECATED'))
	{
		if ($number == E_DEPRECATED)
		return false;
	}
	
	if (! error_reporting ())
		return false;
	
	$caller_info = $commonfunctions_obj->get_debug_back_trace();
	$log_string = $commonfunctions_obj->mask_authentication_headers($requestXML);
	$log_string = $commonfunctions_obj->mask_credentials($log_string);
	$logging_obj->my_error_handler("INFO",array("requestXML"=>$log_string,"ErrorNumber"=>$number,"ErrorString"=>$string,"File"=>$file,"Line"=>$line,"CallerInfo"=>$caller_info),Log::BOTH);	
	debug_access_log($API_ERROR_LOG, "Error Number::$number \n Error String::$string \n File Name:: $file \n Line Number:: $line \n caller_info:: $caller_info \n");
    $number = "-6"; 
	$string = "Internal Server Error";
	throw new Exception ( $string, $number );
	
	return true;
}

function log_asr_api($requestXML='', $responseXML='', $functionName='', $isError = 0)
{
    global $ASRAPI_EVENTS_LOG, $ASRAPI_REPORT_LOG, $ASRAPI_STATUS_LOG, $CXAPI_LOG, $activityId, $SCOUT_API_PATH, $SLASH, $API_ERROR_LOG, $ux_event_api_list, $reporting_api_list, $status_api_list, $logging_obj,$telemetry_api_list;
   
    if ($isError)	$log_name = $API_ERROR_LOG;
	elseif (in_array($functionName, $ux_event_api_list)) $log_name = $ASRAPI_EVENTS_LOG;
    elseif (in_array($functionName, $reporting_api_list)) $log_name = $ASRAPI_REPORT_LOG;
	elseif (in_array($functionName, $status_api_list)) $log_name = $ASRAPI_STATUS_LOG;
    else return;	/// As CXAPI_LOG is no more using and any error is going to MDS, disabling the log.	
	
	//else $log_name = $CXAPI_LOG;
    
    /*
    Not to log the passwords for following APIs
    1. InfrastructureDiscovery
    2. UpdateInfrastructureDiscovery
    3. InstallMobilityService
    4. UninstallMobilityService
    */
    $request_password_xml_pattern = '/(Name=\"Password\"\s+Value=\")(.*)(\")|(Parameter\s+Value=\")(.+)("\s+Name=\"Password\")/';
    $requestXML = preg_replace_callback($request_password_xml_pattern, function($matches) { return isset($matches[4]) ? $matches[4].'*****'.$matches[6] : $matches[1].'*****'.$matches[3]; } , $requestXML);
	
	$request_user_xml_pattern = '/(Name=\"UserName\"\s+Value=\")(.*)(\")|(Parameter\s+Value=\")(.+)("\s+Name=\"UserName\")/';
	$requestXML = preg_replace_callback($request_user_xml_pattern, function($matches) { return isset($matches[4]) ? $matches[4].'*****'.$matches[6] : $matches[1].'*****'.$matches[3]; } , $requestXML);
	
	$response_password_xml_pattern = '/(Name=\"Password\"\s+Value=\")(.*)(\")|(Parameter\s+Value=\")(.+)("\s+Name=\"Password\")/';
	$responseXML = preg_replace_callback($response_password_xml_pattern, function($matches) { return isset($matches[4]) ? $matches[4].'*****'.$matches[6] : $matches[1].'*****'.$matches[3]; } , $responseXML);
    
	$response_user_xml_pattern = '/(Name=\"UserName\"\s+Value=\")(.*)(\")|(Parameter\s+Value=\")(.+)("\s+Name=\"UserName\")/';
	$responseXML = preg_replace_callback($response_user_xml_pattern, function($matches) { return isset($matches[4]) ? $matches[4].'*****'.$matches[6] : $matches[1].'*****'.$matches[3]; } , $responseXML);
    
	$delimiter = str_repeat('=', 30);
	if($activityId) debug_access_log($log_name, "activityId: $activityId \n");
	$log_string = "";
	preg_match('/\<AccessKeyID\>(.*?)\<\/AccessKeyID\>/', $requestXML, $match);
	if (count($match) > 0)
	{
		$actual_string = $match[1];
		$full_string = substr_replace($actual_string, "#####",-5, 5);
		$log_string = preg_replace("/$actual_string/sm", $full_string, $requestXML);
	}
	else
	{
		$log_string = $requestXML;
	}
	preg_match('/\<AccessSignature\>(.*?)\<\/AccessSignature\>/', $log_string, $match);
	if (count($match) > 0)
	{
		$actual_string = $match[1];
		$log_string = preg_replace("/$actual_string/sm", "#####", $log_string);
	}
	if (in_array( $functionName, $telemetry_api_list))
	{
		$logging_obj->my_error_handler("INFO",array("FunctionName"=>$functionName,"REQUEST"=>$log_string,"RESPONSE"=>$responseXML),Log::APPINSIGHTS);
	}
    debug_access_log($log_name, "REMOTE_ADDR : " . $_SERVER['REMOTE_ADDR'] . ", functionName: " . $functionName . PHP_EOL . PHP_EOL . "REQUEST" . PHP_EOL . $delimiter . PHP_EOL . $log_string . PHP_EOL . PHP_EOL . "RESPONSE" . PHP_EOL . $delimiter . PHP_EOL . $responseXML . PHP_EOL . PHP_EOL );
}

function get_api_name($requestXML)
{
    $request_data = simplexml_load_string($requestXML);
    $data = (array) $request_data;
    $data = json_decode(json_encode($data), 1);
	$functionName = $data["Body"]["FunctionRequest"]["@attributes"]["Name"];
	if(!$functionName) $functionName = $data["Body"][0]["@attributes"]["Name"];
	return $functionName;
}

function debug_access_log($log_name, $debugString)
{
	global $commonfunctions_obj;
	//if($log_name == $CXAPI_LOG) return;
	$fr = $commonfunctions_obj->file_open_handle($log_name, 'a+');
	$debugString = date('Y-m-d H:i:s') .":". substr(microtime(), 2, 6)."  ".$debugString . PHP_EOL;
	if($fr != FALSE) 
	{
		 #echo "Error! Couldn't open the file.";
	
		if (!fwrite($fr, $debugString . PHP_EOL)) 
		{
			 #print "Cannot write to file ($filename)";
		}
		if(!fclose($fr)) 
		{
			 #echo "Error! Couldn't close the file.";
		}
	}
}

?>