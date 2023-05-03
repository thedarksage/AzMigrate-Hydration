<?php
extract($_REQUEST);
extract($_ENV);
extract($_SERVER);
include_once 'config.php';

$fx_obj = new FileProtection();
$commonfunctions_obj->conn = $conn_obj;
$host_info = $commonfunctions_obj->isValidComponent($id);
if ($host_info)
{
	$fx_obj->unregister_fr_agent ($id);
}
?>
