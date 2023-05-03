<?php

	/// Get the headers from the HTTP request
	/// Key => header key
	/// Custom => is it custom key
	function getHeader($key, $custom = 0)
	{
		$key = $custom ? 'HTTP_' . $key : $key;
		return isset($_SERVER[$key]) ? $_SERVER[$key]: null;
	}

	/// Is HTTPS enabled?
	function isHTTPS()
	{
		return (isset($_SERVER['HTTPS']) && strcasecmp($_SERVER['HTTPS'], 'on') == 0) ? true : false;
	}

	/// Is the script invoke from command line?
	function isCommandLineCall()
	{
		// Compatible for both windows and linux
		if (isset($_SERVER['argc']) && $_SERVER['argc'] > 0)	return true;
		return false;
	}

	/// Verify authentication signature sent by the client.
	function verifySignature()
	{
		global $logging_obj;

		$header_signature = getHeader('HTTP_AUTH_SIGNATURE', 1);

        // Muliple api names in a single call are seperated with delimiter ','
        $header_apiname = getHeader('HTTP_AUTH_PHPAPI_NAME', 1);

        $header_reqid = getHeader('HTTP_AUTH_REQUESTID', 1);

        // Get Request method: GET/POST
        $header_reqmethod = getHeader('REQUEST_METHOD');
		$file_callee = getHeader('PHP_SELF');
        $version = '1.0';

		// If any of the header not set, throw 403 error
		if (! ($header_signature && $header_apiname && $header_reqid))
        {
            $logging_obj->my_error_handler("INFO", "Headers not set - header_signature: $header_signature, header_apiname: $header_apiname, header_reqid: $header_reqid\n");
            //$logging_obj->my_error_handler("INFO", "POST DATA - " . file_get_contents('php://input') . "\n");
        	throw new Exception('Headers not set', 403);
        }

		$common_obj = new CommonFunctions();
		$passphrase =  $common_obj->getPassphrase();
		$fingerprint = isHTTPS() ? $common_obj->getCSFingerPrint() : '';

		if ($passphrase === false || $fingerprint === false)	throw new Exception('Internal server error - Passphrase/Fingerprint', 500);


		$request_params = array(
								'Function' => str_replace(',', '', $header_apiname),
								'RequestID' => $header_reqid,
								'Version' => $version
							);
		if ($header_signature != SignatureGenerator::ComputeSignatureComponentAuth($passphrase, $header_reqmethod, $file_callee, $request_params, null, null, $fingerprint))
		{
			$logging_obj->my_error_handler("INFO", "Signature mismatch - Received signature: $header_signature. \n");
			throw new Exception('Signature/Password mismatch', 403);
		}

		return true;
	}

	#### END FUNCTIONS	####

	// If command line call, return to parent file and continue execution. Enable below statement if included this file as header
    //if (isCommandLineCall())	return;

	//$current_dir = dirname(__FILE__);
	//include_once $current_dir . DIRECTORY_SEPARATOR . 'config.php';
	//include_once $current_dir . DIRECTORY_SEPARATOR . 'ScoutAPI/SignatureGenerator.php';

	include_once 'config.php';
	include_once 'ScoutAPI/SignatureGenerator.php';    

    // Setting new log file, saving the original log file to reset later
    $default_log_file = $logging_obj->global_log_file;
    $logging_obj->global_log_file = $LOG_ROOT . 'authentication.log';
    
    $client_ip = getHeader('REMOTE_ADDR');
    $file_callee = getHeader('PHP_SELF');

	try
	{
        if (empty($_GET['page_name']))
        {
			$logging_obj->my_error_handler("INFO", "File called with 'page_name' empty.\n");
			throw new Exception('File not found', 404);
        }
        
        // Skip page_name == SELF. These should be different as this file will be invovked only by the server
		if (basename($file_callee) == basename(__FILE__))
		{
			//Log the exception
			$logging_obj->my_error_handler("INFO", "Invalid file invoked\n");
			throw new Exception('File calling forbidden', 403);
		}

		// Check for Signature
		if (! isset($CLIENT_AUTHENTICATION) || $CLIENT_AUTHENTICATION != 0)	verifySignature();
	}
	catch(Exception $excep)
	{
		$logging_obj->my_error_handler("INFO", "Client IP: $client_ip. URL: $file_callee. " . $excep->getMessage() . "\n");
		header('HTTP/1.1 ' . $excep->getCode() . ' ' . $excep->getMessage());

		exit(0);
	}

    // Reset log file to original
    $logging_obj->global_log_file = $default_log_file;

	// Enable if the file is routed through rewrite url
	include_once ".$file_callee";
?>