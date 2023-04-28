<?php

include_once 'config.php';

$options = getopt("h:c:");

// Exit if no arguments are passed
if(!$options['h'] || !$options['c']) exit(1);

$unregister_host_id = $options['h'];
$unregister_component = $options['c'];

if($unregister_host_id && $unregister_component)
{
	try
	{
		$conn_obj->enable_exceptions();
		
		$volume_obj = new VolumeProtection();
		$commonfunctions_obj = new CommonFunctions(); 
		$logging_obj = new Logging($PHPDEBUG_FILE);
		
		if($unregister_component == "Source" || $unregister_component == "MT")
		{
			$fx_obj = new FileProtection();
			$volume_obj->unregister_vx_agent( $unregister_host_id, 2);
			$fx_obj->unregister_fr_agent ($unregister_host_id , 2);
		} 
		elseif($unregister_component == "PS")
		{
			$volume_obj->unregister_ps( $unregister_host_id);
		}
		$conn_obj->disable_exceptions();
	}
	catch(SQLException $sql_excep)
	{
		// Disable Exceptions for DB
		$conn_obj->disable_exceptions();
		exit(1);
	}	
	exit(0);
}

exit(1);

?>