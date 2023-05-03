<?php

include_once "config.php";
$commonfunctions_obj = new CommonFunctions();

try
{
	// Command line arguments not set
	if (! isset($argc) || $argc == 0)	throw new Exception ("Arguments not set");

	$post_content  = $argv[1];
	$post_array = explode('!##!', $post_content);
	$post_data = array();

	for($i=0;$i<count($post_array);$i++)
	{
		$complete_post = explode("##",$post_array[$i]);
		if($complete_post[0] != null && $complete_post[1] != null)
		{
			$post_data[$complete_post[0]] = $complete_post[1];
		}
	}

	if(empty($post_data) || !$post_data['HOST_GUID'] || !$post_data['IP'] || !$post_data['PORT'] || !$post_data['PROTOCOL'] || !$post_data['OPTION'])	throw new Exception ("Parameters not set");

	if ($post_data['OPTION'] == 'getPsConfiguration')
	{
		if (empty($post_data['PAIRID'])) throw new Exception ("Parameters not set");
		$pair_id = $post_data['PAIRID'];
	}
	if ($post_data['OPTION'] == 'getRollbackStatus')
	{
		if (empty($post_data['SOURCEID'])) throw new Exception ("Parameters not set");
		$source_id = $post_data['SOURCEID'];
	}
	if ($post_data['OPTION'] == 'setResyncFlag')
	{
		if (empty($post_data['PAIRINFO'])) throw new Exception ("Parameters not set");
		$pair_info = $post_data['PAIRINFO'];
	}
	
	$cs_ip = $post_data['IP'];
	$cs_port = $post_data['PORT'];
	$http = $post_data['PROTOCOL'];	
	$option = $post_data['OPTION'];			
	$cs_url = $http."://".$cs_ip.":".$cs_port."/ScoutAPI/CXAPI.php";

	$requestDetails_arr = get_request_xml($post_data['HOST_GUID'], $cs_ip, $cs_port, $http, $post_data, $option);

	// Check for success state
	if (! $requestDetails_arr)	throw new Exception ("Request details not set");
	
	$request_xml = $requestDetails_arr['Requestxml'];
	$content_type = "text/xml";
	$headers = array(
					'Content-Type: ' . $content_type . '; charset=utf-8',
					'HTTP-AUTH-VERSION: '. $requestDetails_arr['Version'] ,
					'HTTP-AUTH-SIGNATURE: ' . $requestDetails_arr['Signature'],
					'HTTP-AUTH-REQUESTID: ' . $requestDetails_arr['RequestID'],
					'HTTP-AUTH-PHPAPI-NAME: ' . $requestDetails_arr['Function']
				);
	$cert_path = $http == 'https' ? $commonfunctions_obj->getCertPath($cs_ip,$cs_port) : null;
	if ($http == 'https' && ! $cert_path)	throw new Exception ("Certificate path not set");

	$curl_obj = curl_init($cs_url); 
	curl_setopt($curl_obj, CURLOPT_HTTPHEADER, $headers);
	curl_setopt($curl_obj, CURLOPT_URL, $cs_url); 
	curl_setopt($curl_obj, CURLOPT_POST, true); 
	curl_setopt($curl_obj, CURLOPT_POSTFIELDS, $request_xml);

	if ($http == 'https')
	{
		curl_setopt($curl_obj, CURLOPT_SSL_VERIFYHOST, false);
		curl_setopt($curl_obj, CURLOPT_SSL_VERIFYPEER, true);
		curl_setopt($curl_obj, CURLOPT_CAINFO, $cert_path);
	}

	curl_setopt($curl_obj, CURLOPT_RETURNTRANSFER, true);
	curl_setopt($curl_obj, CURLOPT_TIMEOUT, 120);
	curl_setopt($curl_obj, CURLOPT_CONNECTTIMEOUT, 120);
	
	//Execute http request
	$response_xml = curl_exec($curl_obj);
	
	if(curl_errno($curl_obj))	throw new Exception ("Curl error: " . curl_error($ch));
	curl_close($curl_obj);
	
	$response_array = parse_xml($response_xml,$option);
	if (isset($response_array))	echo $response_array;
	exit(0);
}
catch(Exception  $exception)
{
	$logging_obj->my_error_handler("INFO", "get_ps_settings: ".$exception->getMessage()."\n");
	exit(1);
}

/// Functions ///

function parse_xml($response_xml,$option)
{   
	$reponse_xml = str_replace("\n","",$response_xml);
	$reponse_xml = str_replace("\r","",$response_xml);
    $xml = simplexml_load_string ($response_xml);    
	if (! is_object($xml))	throw new Exception ("Error in parse response xml");
	
    $data = (array) $xml;
    $data = json_decode(json_encode($data), 1);
    
	if (($option == 'getPsConfiguration') || ($option == 'getRollbackStatus') || ($option == 'setResyncFlag'))
	{		
		if($data['Body']['Function']['FunctionResponse']['Parameter'])
		{
			$root = convertIntoIndexArr($data['Body']['Function']['FunctionResponse']['Parameter']);
			foreach($root as $v) 
			{
				$finalOutput = $v['@attributes']['Value'];				
			}
		}	
	}   
    
	return $finalOutput;
}

function convertIntoIndexArr($arr)
{
	$result = array();
	if(array_key_exists(0,$arr) === FALSE) {
		$result[0] = $arr;
	}
	else {
		$result = $arr;
	}
	
	return $result;
}

function get_request_xml($HOST_GUID, $cs_ip, $cs_port, $http, $post_data, $option)
{
    global $commonfunctions_obj;
	global $logging_obj;
	
	$passphrase =  $commonfunctions_obj->getPassphrase();
	$fingerprint = '';
	
	if(! $passphrase)
	{
		$logging_obj->my_error_handler("INFO", "Passphrase not found, http call not initiated for get ps settings"."\n");
		return false;
	}
	
	if ($http == 'https')
	{
		$fingerprint = $commonfunctions_obj->getServerFingerPrint($cs_ip,$cs_port);
		if(! $fingerprint)
		{
			$logging_obj->my_error_handler("INFO", "Finger print not found, http call not initiated for get ps settings"."\n");
			return false;
		}
	}

	$request_id = $commonfunctions_obj->genRandNonce(32, "false");
	$auth_method = 'ComponentAuth_V2015_01';
	$access_file = "/ScoutAPI/CXAPI.php";
	if ($option == 'getPsConfiguration')
	{
		$function_name = "GetDBData";
	}
	
	if ($option == 'getRollbackStatus')
	{
		$function_name = "IsRollbackInProgress";
	}
	if ($option == 'setResyncFlag')
	{
		$function_name = "MarkResyncRequiredToPair";
	}
	$request_method = "POST";
	$version = "1.0";
	
	$params = array ('RequestID'=> $request_id,
					 'Version'	=> $version,								 
					 'Function' => $function_name
					);
								
	$access_signature = ComputeSignatureComponentAuth($passphrase, $request_method, $access_file, $params, null, null, $fingerprint);
	$function_request = '';
    
    
	if ($option == 'getPsConfiguration')
	{
        
		$pair_id = $post_data['PAIRID'];
		$function_request = '<FunctionRequest Name="'.$function_name.'">
									<Parameter Name="HostId" Value="'.$HOST_GUID.'" />
									<Parameter Name="PairId" Value="'.$pair_id.'" />
                            </FunctionRequest>';
	}
    
	if ($option == 'getRollbackStatus')
	{
        
		$source_id = $post_data['SOURCEID'];
		$function_request = '<FunctionRequest Name="'.$function_name.'">
									<Parameter Name="HostId" Value="'.$HOST_GUID.'" />
									<Parameter Name="SourceId" Value="'.$source_id.'" />
                            </FunctionRequest>';
	}
	
	if ($option == 'setResyncFlag')
	{
        
		$pair_info = $post_data['PAIRINFO'];
		$pairinfo_array = explode('!@!@!', $pair_info);
		#Do we need a empty value check for the parameters over here
		
		$function_request = '<FunctionRequest Name="'.$function_name.'">
									<Parameter Name="HostId" Value="'.$HOST_GUID.'" />
									<Parameter Name="SourceHostId" Value="'.$pairinfo_array[0].'" />
									<Parameter Name="SourceVolume" Value="'.$pairinfo_array[1].'" />
									<Parameter Name="DestHostId" Value="'.$pairinfo_array[2].'" />
									<Parameter Name="DestVolume" Value="'.$pairinfo_array[3].'" />
									<Parameter Name="HardlinkFromFile" Value="'.$pairinfo_array[4].'" />
									<Parameter Name="HardlinkToFile" Value="'.$pairinfo_array[5].'" />
                            </FunctionRequest>';
	}
	
	$request_xml = '<Request Id="'.$request_id.'" Version="'.$version.'">
                    <Header>
                            <Authentication>
								<AuthMethod>'.$auth_method.'</AuthMethod>
								<AccessKeyID>'.$HOST_GUID.'</AccessKeyID>
								<AccessSignature>'.$access_signature.'</AccessSignature>
                            </Authentication>
                    </Header>
                    <Body>'
					.$function_request.
                    '</Body>
					</Request>';
			
	$response = array('RequestID'=> $request_id,
					 'Version'	=> $version,								 
					 'Function' => $function_name,
					 'Signature' => $access_signature,
					 'Requestxml' => $request_xml);		
    return $response;
}

function ComputeSignatureComponentAuth($passphrase, // passphrase used to sign request
												  $method,     // HTTP method (e.g. GET, POST)
												  $resource,   // HTTP resource (i.e. the php script)
												  $params,     // HTTP array for http paramaters and values (i.e. the param=value) this should have version and nonce if being used plus all other params
												  $headers,    // array of speical headers specific to the request (i.e. not general http headers, they should have a uniquen prefix e.g. IMS_ (InMage Miscrosoft)
												  $content,     // content being sent
												  $fingerprint) // server certificate fingerprint

{
	$delimiter = ':';

	$passphraseHash = hash('sha256', $passphrase, false);
	$fingerprintHMAC = $fingerprint ? hash_hmac('sha256', $fingerprint, $passphraseHash, false) : '';					
	
	$stringtosign = $method . $delimiter. $resource . $delimiter. $params['Function'] . $delimiter. $params['RequestID'] . $delimiter. $params['Version'];
	$stringtosignHMAC = hash_hmac('sha256', $stringtosign, $passphraseHash, false);
	
	$signature = $fingerprintHMAC . $delimiter . $stringtosignHMAC;		
	$signatureHMAC = hash_hmac('sha256', $signature, $passphraseHash, false);	
	
	// $headers, $content will be implemented at the time when needed.
	
	return $signatureHMAC;
}	
?>
