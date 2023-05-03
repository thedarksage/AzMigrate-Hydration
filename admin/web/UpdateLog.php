<?php
include( 'functions.php' );
$lines = file( 'php://input' );
global $logging_obj;
$logging_obj->my_error_handler("DEBUG","value".print_r($lines,true));

if (isset($lines))
{
	$policy_id_array = explode(":",$lines[0]);
	$policy_id_string = $policy_id_array[1];
	$policy_id_string_array = explode("]",$policy_id_string);
	$policy_id = $policy_id_string_array[0];
	
	$group_id_array = explode(":",$lines[1]);
	$group_id_string = $group_id_array[1];
	$group_id_string_array = explode("]",$group_id_string);
	$group_id = $group_id_string_array[0];
	
	
	
	$instance_array = explode(":",$lines[2]);
	$instance_id_string = $instance_array[1];
	$instance_id_string_array = explode("]",$instance_id_string);
	$instance_id = $instance_id_string_array[0];
	
	$offset_array = explode(":",$lines[3]);
	$offset_id_string = $offset_array[1];
	$offset_id_string_array = explode("]",$offset_id_string);
	$offset_id = $offset_id_string_array[0];
	
	$length_array = explode(":",$lines[4]);
	$length_string = $length_array[1];
	$length_string_array = explode("]",$length_string);
	$length = $length_string_array[0];
	
	return $length;
	
}
?>
