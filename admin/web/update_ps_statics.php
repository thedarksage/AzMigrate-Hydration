<?php
include_once ("config.php");
global $REPLICATION_ROOT, $commonfunctions_obj;
$commonfunctions_obj->conn = $conn_obj;

if(isset($_REQUEST['perf_params']))
{
    $perf_params = $_REQUEST['perf_params'];
	$xml=simplexml_load_string($perf_params);
	foreach($xml->children() as $xmlData)
	{
		if($xmlData['Name'] == 'PSGuid')
		{
			$perf_ps_guid = (string)$xmlData['Value'];
		}
	}
}

if(isset($_REQUEST['serv_params']))
{
    $serv_params = $_REQUEST['serv_params'];
	$xml=simplexml_load_string($serv_params);
	foreach($xml->children() as $xmlData)
	{
		if($xmlData['Name'] == 'PSGuid')
		{
			$serv_ps_guid = (string)$xmlData['Value'];
		}
	}
}

/*
Do the validation here to know valid PS guid or not.
If it is valid guid, then only allow the control to process further.
*/
$is_valid_perf_ps_guid = FALSE;
$is_valid_serv_ps_guid = FALSE;
if ($perf_ps_guid == $serv_ps_guid)
{
	$is_valid_ps_guid = $commonfunctions_obj->isValidComponent($perf_ps_guid);	
	if ($is_valid_ps_guid == TRUE)
	{
		$is_valid_perf_ps_guid = TRUE;
		$is_valid_serv_ps_guid = TRUE;
	}
}
else
{
	if ($perf_ps_guid)
	{
		$is_valid_ps_guid = $commonfunctions_obj->isValidComponent($perf_ps_guid);
		if ($is_valid_ps_guid == TRUE)
		{
			$is_valid_perf_ps_guid = TRUE;
		}
	}
	if ($serv_ps_guid)
	{
		$is_valid_ps_guid = $commonfunctions_obj->isValidComponent($serv_ps_guid);
		if ($is_valid_ps_guid == TRUE)
		{
			$is_valid_serv_ps_guid = TRUE;
		}
	}
}

if(isset($_REQUEST['perf_params']))
{	
	if ($is_valid_perf_ps_guid)
	{
		$param_file = $REPLICATION_ROOT."/SystemMonitorRrds/".$perf_ps_guid."-perf.xml";
		echo "creating file at $param_file\n";
		$rc = false;
		do 
		{
		  if (!($f = $commonfunctions_obj->file_open_handle($param_file, "wa+"))) 
		  {
			$rc = 1; break;
		  }
		  if (!fwrite($f, $perf_params)) 
		  {
			$rc = 2; break;
		  }
		  $rc = true;
		} while (0);
		if ($f) 
		{
		  fclose($f);
		}
	}
}

if(isset($_REQUEST['serv_params']))
{	
	if ($is_valid_serv_ps_guid)
	{
		$serv_file = $REPLICATION_ROOT."/SystemMonitorRrds/".$serv_ps_guid."-service.xml";
		$rc = false;
		do 
		{
		  if (!($f = $commonfunctions_obj->file_open_handle($serv_file, "wa+"))) 
		  {
			$rc = 1; break;
		  }
		  if (!fwrite($f, $serv_params)) 
		  {
			$rc = 2; break;
		  }
		  $rc = true;
		} while (0);
		if ($f) 
		{
		  fclose($f);
		}
	}
}
?>