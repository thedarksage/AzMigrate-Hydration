<?php
error_reporting(E_ALL & ~E_NOTICE);
ini_set('memory_limit', '512M');
date_default_timezone_set(@date_default_timezone_get());

if (preg_match('/Windows/i', php_uname()))
{
  $log_dir = "..\\var\\";
}
else
{
  $log_dir = "/home/svsystems/var/";
}

// Upgrade Clean logs
$upgrade_clean_log_files = array(
		'unmarshal.txt', 
		'authentication.log', 
		'api_error.log', 
		'php_error.log', 
		'php_sql_error.log', 
		'perl_sql_error.log'
	);
	
// Cleaning log files
foreach($upgrade_clean_log_files as $log_file)
{
	$log_file_path =  $log_dir.$log_file;
	if (file_exists($log_file_path)) 
	{					
		@unlink($log_file_path);
	}
}
?>