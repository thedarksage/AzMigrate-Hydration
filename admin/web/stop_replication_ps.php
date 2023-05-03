<?php
if (preg_match('/Linux/i', php_uname())) 
{
	include_once 'config.php';
}
else
{
	include_once 'config.php';
}

$volume_obj= new VolumeProtection();
$commonfunctions_obj = new CommonFunctions();
global $conn_obj;

$srcId = $_REQUEST['sourceHostId'];
$srcDevice = $_REQUEST['sourceDeviceName'];
$dstId = $_REQUEST['destinationHostId'];
$dstDevice = $_REQUEST['destinationDeviceName'];

$is_valid_protection = $volume_obj->pair_exists($srcId,$srcDevice,$dstId,$dstDevice);

//If it is valid protection based on post data, then only allow script to execute for further.
if ($is_valid_protection)
{
	$oneToN = $volume_obj->is_one_to_many_by_source($srcId, $srcDevice);
	$isPrimary = $volume_obj->is_primary_pair($srcId, $srcDevice, $dstId, $dstDevice);

	$escSrcDevice = $conn_obj->sql_escape_string($srcDevice);
	$escDstDevice = $conn_obj->sql_escape_string($dstDevice);								  

	$volume_obj->remove_one_to_one_files($srcId, $escSrcDevice, $dstId, $escDstDevice);
	$volume_obj->remove_protection_rep_data($srcId, $escSrcDevice, $dstId, $escDstDevice, $oneToN, $isPrimary);
}
?>