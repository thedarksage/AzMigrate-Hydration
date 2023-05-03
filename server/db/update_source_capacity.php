<?php
error_reporting(E_ALL & ~E_NOTICE);

if (preg_match('/Windows/i', php_uname()))
{
  $constants_file = "..\\admin\\web\\app\\Constants.php";
}
else
{
  $constants_file = "/home/svsystems/admin/web/app/Constants.php";
}
include_once $constants_file;
$config_file = $REPLICATION_ROOT.'admin'.$SLASH.'web'.$SLASH.'config.php';
include_once $config_file;

mysql_connect($DB_HOST, $DB_USER, $DB_PASSWORD) or  newdebugLog("Connection Establishment failed  ".mysql_error());
mysql_select_db($DB_DATABASE_NAME) or  newdebugLog("Database selection failed  ".mysql_error());

/*
 *  Following block is to delete junk information from nports table.
 */
$sql = "SHOW tables LIKE 'nports'";
$result = mysql_query($sql) or  newdebugLog("SHOW tables LIKE nports query execution failed  ".mysql_error());
if(mysql_num_rows($result) > 0)
{
	$sql = "SELECT portWwn FROM nports";
	$result = mysql_query($sql) or newdebugLog("SELECT portWwn FROM nports query failed ".mysql_error());
	if(mysql_num_rows($result) > 0)
	{
		$sql = "DELETE FROM nports WHERE portWwn NOT LIKE '%:%:%:%:%:%:%:%' AND portWwn NOT LIKE 'iqn%'";
		$rs = mysql_query($sql) or newdebugLog("DELETE FROM nports query failed ".mysql_error());
	}
}

$sql = "SELECT 
			deviceName
		FROM
		exportImportDetails where deviceName NOT IN (select exportedDeviceName from exportedFCLuns);
		";
$resSql = mysql_query($sql);					
if(mysql_num_rows($resSql) > 0 )
{				
	while ($data_object1 = mysql_fetch_object($resSql))
	{
		$device_name[] = $data_object1->deviceName;
	}
		
	foreach ($device_name as $value)
	{
		$delSql = "DELETE 
			FROM
			exportedDevices
			WHERE
				exportedDeviceName = '$value'";
		$result = mysql_query($delSql) or newdebugLog("DELETE FROM  exportImportDetails query failed ".mysql_error());				  
	}
}					
$delSql = "DELETE 
				FROM
				exportImportDetails
			";
$result = mysql_query($delSql) or newdebugLog("DELETE FROM  exportImportDetails query failed ".mysql_error());				  
$delPolicy = "DELETE 
				FROM
				policy
			WHERE
				policyName IN ('ExportDevice','UnExportDevice')";
$result = mysql_query($delPolicy) or newdebugLog("DELETE FROM  policy query failed ".mysql_error());


//Query added to populate solution-type(for ESX plans) as part of upgrade to 7.1
$update_solution_type = "UPDATE 
								applicationPlan 
							SET 
								solutionType='ESX' 
							WHERE 
								planId IN(SELECT DISTINCT planId FROM frbJobConfig WHERE alert_category <>0)";

$update_solution_type_rs = mysql_query($update_solution_type) or newdebugLog("UPDATE applicationPlan SET solutionType='ESX' query failed ".mysql_error());

//Query added to reset the 	dispatch_interval in error_policy dbtable(from minutes to days)
$update_dispatch_interval = "UPDATE 
									error_policy 
								SET 
									dispatch_interval = dispatch_interval/1440 
								WHERE 
									dispatch_interval >=1440 
								AND 
									error_template_id='PROTECT_REP'";

$update_solution_type_rs = mysql_query($update_dispatch_interval) or newdebugLog("UPDATE error_policy SET dispatch_interval query failed ".mysql_error());


$delTemplates = "DELETE FROM frbJobTemplates WHERE id <= 87 AND id NOT IN(7,8,10,13,14,24,31,32,33,34,35,38,47,65,66,78,79,81,82,83,86,87)";
$templates_result = mysql_query($delTemplates) or newdebugLog("DELETE FROM  policy query failed ".mysql_error());


function newdebugLog ($debugString)
{
	if (eregi('Windows', php_uname()))
	{
	  $version_file_path = "c:\\home\\svsystems\\etc\\version";
	  $log_path = "c:\\home\\svsystems\\var\\";
	}
	else
	{ 
	  $version_file_path = "/home/svsystems/etc/version";
	  $log_path = "/home/svsystems/var/";
	}

	$version = `cat $version_file_path | grep -i inmage`;
	$PHPDEBUG_FILE_PATH =  $log_path."upgrade_log_from_".trim($version).".log";
	# Some debug
	$fr = fopen($PHPDEBUG_FILE_PATH, 'a+');
	if(!$fr) {
	#echo "Error! Couldn't open the file.";
	}
	if (!fwrite($fr, $debugString . "\n")) {
	#print "Cannot write to file ($filename)";
	}
	if(!fclose($fr)) {
	#echo "Error! Couldn't close the file.";
	}

	$return_check_one = strripos($debugString, 'ADD CONSTRAINT');
	$return_check_two = strripos($debugString, 'dpsLogicalVolumes');
	if(!$return_check_one AND !$return_check_two)
	{
	  exit(111);
	}
}
?>