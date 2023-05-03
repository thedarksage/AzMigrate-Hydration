<?

$OBJ_CLASS_FIELD = '_inmage_class';

include_once('om_err.php');
include_once('om_configurator.php');
include_once('om_vxstub.php');
include_once('om_vxagent.php');
include_once('om_vxsnapshotmanager.php');

function mylog2($line)
{
    $file = fopen( "/tmp/mytrace.log", "ab" );
    fputs( $file, "php: " . $line . "\n" );
    fclose( $file );
}   

function camel_case_to_underscore( $name ) {
    return strtolower( preg_replace('/(?<!^)([A-Z])/','_$1', $name ) );
}

function obj_is_valid($obj) { 
    global $OBJ_CLASS_FIELD; 
    return is_array( $obj ) && array_key_exists($OBJ_CLASS_FIELD, $obj );
}
function obj_class_name($obj){ 
    global $OBJ_CLASS_FIELD; 
    return $obj[$OBJ_CLASS_FIELD];
}

function obj_new($className) { 
    global $OBJ_CLASS_FIELD; 
    return array( $OBJ_CLASS_FIELD => $className );
}
    
?>
