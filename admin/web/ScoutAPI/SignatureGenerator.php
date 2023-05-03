<?php

class SignatureGenerator {
	
	/*
	 *  Get Signature
	 */

	public static  function ComputeSignatureCXAuth($userName)
	{
		$systemObj = new System();
		$signature = $systemObj->get_passwd($userName);	
		
		return $signature;
	}
	
	// Input Parameters
	// params: array for http parameters and values (i.e. the param=value)
	public static  function ComputeSignatureMessageAuth($params)
	{
		// Create PlainText 
		$plainText = $params['RequestID'] . $params['Version'] . $params['Function']. $params['AccessKey'] . 'scout';
		
		// get Passphrase
		$apiProcessData = new APIProcessData ();
		$pp = $apiProcessData->getPP ( $accesskey );
		// Create HMAC_SH1 Key
		$signature = base64_encode ( hash_hmac ( "sha1", $plainText, $pp ) );
		return $signature;
	}

	public static function ComputeSignatureComponentAuth($passphrase, // passphrase used to sign request
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
}
?>