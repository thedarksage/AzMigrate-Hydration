<?php
header('Cache-control: no-cache, no-store');
error_reporting(E_ALL & ~E_NOTICE);
ini_set('display_errors', '1');

include_once("../config.php");

$requestXML = file_get_contents ( "php://input" );
global $REPLICATION_ROOT , $SLASH, $commonfunctions_obj;

debug_access_log($_SERVER['REMOTE_ADDR']."	".$_SERVER['SCRIPT_FILENAME']);
debug_access_log("REQUEST \n=============================\n".$requestXML);


if (strlen ( $requestXML ) == 0) {
	exit;
}
// Process Request
set_error_handler ( 'errorHandler' );
$signature = getSignature ( $requestXML );
debug_access_log("RESPONSE \n=============================\n".$signature);
print( $signature );

/**
 * Returns Response XML for the corresponding Request XML
 * @param String $requestXML
 * @return string 
 */
function getSignature($requestXML) {
	// Call Main Controller
	$controller = new APIController ();
	$requestObj = new APIRequest ();
	$common_obj = new CommonFunctions ();
	try {
		$requestObj->build ( $requestXML );
		$requestid = $requestObj->getRequestID();
		if(!$requestid) return "Request ID not provided";
		$version = $requestObj->getVersion();
		if(!$version) return "Version not provided";
		$accesskey = $requestObj->getAccessKey();
		if(!$accesskey) return "Access Key not provided";
		$funAry = $requestObj->getFunctionNames();	
		if(!$funAry[0]) return "Request Function Name not provided";
		$authMethod = $requestObj->getRequestAuthMethod ();
		switch($authMethod)
		{
			case APIController::AUTHMETHOD_CXAUTH:
				$sig = SignatureGenerator::ComputeSignatureCXAuth($requestObj->getRequestUserName());
				break;
			
			case  APIController::AUTHMETHOD_COMPAUTH:
				$passphrase =  $common_obj->getPassphrase();
				$fingerprint = $common_obj->getCSFingerPrint();
				
				// Check passphrase and fingerprint exits
				if (! $passphrase)	return ErrorCodes::EC_PASSPHRASE_EMPTY;				
				if (isset($_SERVER['HTTPS']) && ! $fingerprint)	return ErrorCodes::EC_FINGERPRINT_EMPTY;

				$params = array ('RequestID'=> $requestObj->getRequestID(),
								 'Version'	=> $requestObj->getVersion(),								 
								 'Function' => implode('', $requestObj->getFunctionNames())
								);
				$sig = SignatureGenerator::ComputeSignatureComponentAuth($passphrase, $_SERVER['REQUEST_METHOD'], '/ScoutAPI/CXAPI.php', $params, null, null, $fingerprint);
				break;
			
			default:
				$params = array ('RequestID'=> $requestObj->getRequestID(),
								'Version'	=> $requestObj->getVersion(),								 
								'Function'  => implode('', $requestObj->getFunctionNames()),
								'AccessKey' => $requestObj->getAccessKey()
						);				
				$sig = SignatureGenerator::ComputeSignatureMessageAuth($params);
		}
		
		return $sig;
	} catch ( Exception $e ) {
		return APIController::ProcessRuntimeError ( "-6", $e->getMessage () );
	}	
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
function errorHandler($number, $string, $file = 'Unknown', $line = 0, $context = array()) {
	if (($number == E_NOTICE) || ($number == E_STRICT))
		return false;
	
	if (! error_reporting ())
		return false;
	
	throw new Exception ( $string, $number );
	
	return true;
}

function debug_access_log($debugString)
{
	global $CXAPI_LOG, $commonfunctions_obj;
    $fr = $commonfunctions_obj->file_open_handle($CXAPI_LOG, 'a+');
	$debugString = date("Y-m-d H:i:s")."  ".$debugString."\n";
	if(!$fr) 
	{
        return;
		 #echo "Error! Couldn't open the file.";
	}
	if (!fwrite($fr, $debugString . "\n")) 
	{
		 #print "Cannot write to file ($filename)";
	}
	if(!fclose($fr)) 
	{
		 #echo "Error! Couldn't close the file.";
	}

}
?>