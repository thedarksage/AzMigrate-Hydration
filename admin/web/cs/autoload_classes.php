<?php
//include("functions.php");
//global $VOLUME_SYNCHRONIZATION_PROGRESS,$REPLICATION_ROOT, $ORIGINAL_REPLICATION_ROOT; 

function __autoload($class_name) {
    global $VOLUME_SYNCHRONIZATION_PROGRESS,$REPLICATION_ROOT, $ORIGINAL_REPLICATION_ROOT; 
	$class_types = array ('conn', 'data', 'log');
	foreach ($class_types as $class_type) 
	{
		$class_file = $REPLICATION_ROOT.'admin/web/cs/classes/'.$class_type.'/'.$class_name.'.php';
        if (file_exists ($class_file)) {
           include_once $class_file;
            break;
        }
    }
$local_class_types = array ('app');
	foreach ($local_class_types as $local_class_type) 
	{
		$class_file = $REPLICATION_ROOT.'admin/web/'.$local_class_type.'/'.$class_name.'.php';
        if (file_exists ($class_file)) {
           include_once $class_file;
            break;
        }
    }
}



?>