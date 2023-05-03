<?php
include_once('config.php');
$commonfunctions_obj = new CommonFunctions();

function writeLog($userName=NULL,$actionMsg=NULL,$filePath=NULL)
{
	global $commonfunctions_obj;
	$commonfunctions_obj->writeLog("System",$actionMsg,$filePath);
}
?>