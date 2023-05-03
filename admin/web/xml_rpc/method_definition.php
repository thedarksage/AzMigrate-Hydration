<?php
	include_once '../config.php';
	include_once '../app/VolumeProtection.php';
	include_once '../app/FileProtection.php';
	include_once '../app/CommonFunctions.php';
	include_once '../app/Recovery.php';
	include_once '../app/Trending.php';
	include_once '../app/SparseRetention.php';
	include_once '../app/CxReport.php';
	
	include_once '../app/Application.php';
	include_once '../app/ProtectionPlan.php';
	include_once '../app/RecoveryPlan.php';
	include_once '../app/System.php';
	include_once '../app/ProcessServer.php';
	include_once '../app/ESXMasterConfigParser.php';
	include_once '../app/ESXRecovery.php';


	$volume_obj = new VolumeProtection();
	$fx_obj = new FileProtection();
	$commonfunctions_obj = new CommonFunctions();
	$recovery_obj = new Recovery();
	$trending_obj = new Trending();
	$report_obj = new CxReport();
	$sparseretention_obj = new SparseRetention();
	$app_obj = new Application();
	$protection_plan_obj = new ProtectionPlan();
	$recovery_plan_obj = new RecoveryPlan();
	$vcon_obj = new ESXMasterConfigParser();
	$esx_rec_obj = new ESXRecovery();
	
	
	$findstate_sig = array(array($xmlrpcString, $xmlrpcInt));
	$findstate_doc = 'When passed an integer between 1 and 10 returns the name of a US state, where the integer is the index of that state name in an alphabetic order.';
	
	function findstate($m)
	{
		global $xmlrpcerruser;
		
		$stateNames = array(
			"Alabama", "Alaska", "Arizona", "Arkansas", "California",
			"Colorado", "Columbia", "Connecticut", "Delaware", "Florida"
		);
	
		$err = "";
		
		// get the first param
		$sno = $m->getParam(0);

		// extract the value of the state number
		$snv = $sno->scalarval();
		
		// look it up in our array (zero-based)
		if (isset($stateNames[$snv-1]))
		{
			$sname = $stateNames[$snv-1];
		}
		else
		{
			// not, there so complain
			$err = "I don't have a state for the index '" . $snv . "'";
		}

		// if we generated an error, create an error return response
		if ($err)
		{
			return new xmlrpcresp(0, $xmlrpcerruser, $err);
		}
		else
		{
			// otherwise, we create the right response
			// with the state name
			return new xmlrpcresp(new xmlrpcval($sname));
		}
	}
	
	$getHostDetails_sig = array(array($xmlrpcStruct));
	$getHostDetails_doc = 'It gives Host name, ethernet card details and Hardware/Software details.';
	function getHostDetails()
	{
		global $report_obj;
		$data = $report_obj->getHostDetails();
		
		return new xmlrpcresp(php_xmlrpc_encode($data));
	}
	
	$registerRx_sig = array(array($xmlrpcInt, $xmlrpcArray));
	$registerRx_doc = 'It registers RX with CX.';
	function registerRx($param)
	{
		global $report_obj;
		$data = $report_obj->registerRx($param);
		
		return new xmlrpcresp(php_xmlrpc_encode($data));
	}
	
	$unregisterRx_sig = array(array($xmlrpcInt, $xmlrpcArray));
	$unregisterRx_doc = 'It unregisters RX from CX.';
	function unregisterRx($param)
	{
		global $report_obj;
		$data = $report_obj->unregisterRx($param);
		
		return new xmlrpcresp(php_xmlrpc_encode($data));
	}	

	$getAlerts_sig = array(array($xmlrpcStruct));
	$getAlerts_doc = 'Returns Structure of CX Alerts';	
	function getAlerts()
	{
		global $report_obj;
		$data = $report_obj->getAlerts();
		
		return new xmlrpcresp(php_xmlrpc_encode($data));	
	}

	$getVxDetails_sig = array(array($xmlrpcStruct));
	$getVxDetails_doc = 'Return Structure of Volume Replication Pairs.';	
	function getVxDetails()
	{
		global $report_obj;
		$data = $report_obj->getVxDetails();
		
		return new xmlrpcresp(php_xmlrpc_encode($data));	
	}
	
	$getFxDetails_sig = array(array($xmlrpcStruct));
	$getFxDetails_doc = 'Return Structure of File Replication Pairs.';	
	function getFxDetails()
	{
		global $report_obj;
		$data = $report_obj->getFxDetails();
		
		return new xmlrpcresp(php_xmlrpc_encode($data));	
	}	
		
	$getReportDetails_sig = array(array($xmlrpcStruct, $xmlrpcArray));
	$getReportDetails_doc = 'Return Structure of Report Data.';
	function getReportDetails($param)
	{
		global $report_obj;
		$data = $report_obj->getReportDetails($param);
		
		return new xmlrpcresp(php_xmlrpc_encode($data));	
	}
	
	$pwdupdated_sig = array(array($xmlrpcInt, $xmlrpcArray));
	$pwdupdated_doc = 'It updates new password to CX ';
	function getPwDetails($param)
	{
		global $report_obj;
		$data = $report_obj->getPwDetails($param);
		
		return new xmlrpcresp(new xmlrpcval($data));
	}
	
	$checkpwd_sig = array(array($xmlrpcStruct, $xmlrpcArray));
	$checkpwd_doc = 'It verifies the CX authentication from RX.';
	function checkPwDetails($param)
	{
		global $report_obj;
		$data = $report_obj->checkPwDetails($param);
		
		return ($data['faultCode'])	? new xmlrpcresp(0,$data['faultCode'],$data['faultString']) : new xmlrpcresp(new xmlrpcval($data));
	}
	
	$getesxcred_sig = array(array($xmlrpcStruct));
	$getesxcred_doc = 'It will get the ESX credentials from CX (VCON).';
	function getEsxCredentials()
	{
		global $report_obj;
		$data = $report_obj->getEsxCredentials();
		
		return ($data['faultCode'])	? new xmlrpcresp(0,$data['faultCode'],$data['faultString']) : new xmlrpcresp( php_xmlrpc_encode($data));
	}
	
	$getesxhwconf_sig = array(array($xmlrpcStruct));
	$getesxhwconf_doc = 'It will get the ESX hardware configuration from CX (VCON).';
	function getEsxHwConf()
	{
		global $report_obj;
		$data = $report_obj->getEsxHwConf();
		
		return ($data['faultCode'])	? new xmlrpcresp(0,$data['faultCode'],$data['faultString']) : new xmlrpcresp( php_xmlrpc_encode($data));
	}
	
	$gethosthwconf_sig = array(array($xmlrpcStruct));
	$gethosthwconf_doc = 'It will get the hosts harware configuration from CX (VCON).';
	function getHostHwConf()
	{
		global $report_obj;
		$data = $report_obj->getHostHwConf();
		
		return ($data['faultCode'])	? new xmlrpcresp(0,$data['faultCode'],$data['faultString']) : new xmlrpcresp( php_xmlrpc_encode($data));
	}
	
	$gethostnwconf_sig = array(array($xmlrpcStruct));
	$gethostnwconf_doc = 'It will get the hosts network configuration from CX (VCON).';
	function getHostNwConf()
	{
		global $report_obj;
		$data = $report_obj->getHostNwConf();
		
		return ($data['faultCode'])	? new xmlrpcresp(0,$data['faultCode'],$data['faultString']) : new xmlrpcresp( php_xmlrpc_encode($data));
	}
	
	$createrecoveryxml_sig = array(array($xmlrpcStruct, $xmlrpcStruct));
	$createrecoveryxml_doc = 'It will create the recovery xml based on the parameters sent.';
	function createRecoveryXML($param)
	{
		global $report_obj;
		$data = $report_obj->createRecoveryXML($param);
		
		return ($data['faultCode'])	? new xmlrpcresp(0,$data['faultCode'],$data['faultString']) : new xmlrpcresp( php_xmlrpc_encode($data));
	}
	
	$updateLastResponse_sig = array(array($xmlrpcInt, $xmlrpcArray));
	$updateLastResponse_doc = 'It will update the last response time.';
	function updateLastResponse($param)
	{
		global $report_obj;
		$data = $report_obj->updateLastResponse($param);
		
		return new xmlrpcresp(new xmlrpcval($data));	
	}
	
	$a = array(
		"examples.getStateName" => array(
			"function" => "findstate",
			"signature" => $findstate_sig,
			"docstring" => $findstate_doc
		),
		"report.getHostDetails" => array(
			"function" => "getHostDetails",
			"signature" => $getHostDetails_sig,
			"docstring" => $getHostDetails_doc
		),
		"report.registerRx" => array(
			"function" => "registerRx",
			"signature" => $registerRx_sig,
			"docstring" => $registerRx_doc
		),	
		"report.unregisterRx" => array(
			"function" => "unregisterRx",
			"signature" => $unregisterRx_sig,
			"docstring" => $unregisterRx_doc
		),		
		"report.getAlerts" => array(
			"function" => "getAlerts",
			"signature" => $getAlerts_sig,
			"docstring" => $getAlerts_doc
		),
		"report.getVxDetails" => array(
			"function" => "getVxDetails",
			"signature" => $getVxDetails_sig,
			"docstring" => $getVxDetails_doc
		),
		"report.getFxDetails" => array(
			"function" => "getFxDetails",
			"signature" => $getFxDetails_sig,
			"docstring" => $getFxDetails_doc
		),
		"report.getReportDetails" => array(
			"function" => "getReportDetails",
			"signature" => $getReportDetails_sig,
			"docstring" => $getReportDetails_doc
		),
		"report.getPwDetails" => array(
			"function" => "getPwDetails",
			"signature" => $pwdupdated_sig,
			"docstring" => $pwdupdated_doc
		),
		"report.checkPwDetails" => array(
			"function" => "checkPwDetails",
			"signature" => $checkpwd_sig,
			"docstring" => $checkpwd_doc
		),
		"report.getEsxCredentials" => array(
			"function" => "getEsxCredentials",
			"signature" => $getesxcred_sig,
			"docstring" => $getesxcred_doc
		),
		"report.getEsxHwConf" => array(
			"function" => "getEsxHwConf",
			"signature" => $getesxhwconf_sig,
			"docstring" => $getesxhwconf_doc
		),
		"report.getHostHwConf" => array(
			"function" => "getHostHwConf",
			"signature" => $gethosthwconf_sig,
			"docstring" => $gethosthwconf_doc
		),
		"report.getHostNwConf" => array(
			"function" => "getHostNwConf",
			"signature" => $gethostnwconf_sig,
			"docstring" => $gethostnwconf_doc
		),
		"report.createRecoveryXML" => array(
			"function" => "getHostNwConf",
			"signature" => $createrecoveryxml_sig,
			"docstring" => $createrecoveryxml_doc
		),
		"report.updateLastResponse" => array(
			"function" => "updateLastResponse",
			"signature" => $updateLastResponse_sig,
			"docstring" => $updateLastResponse_doc
		)
	);
?>