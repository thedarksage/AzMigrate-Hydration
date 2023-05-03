<?

function vxagent_create( $vxstub_obj, $host_id ) {
    $vxagent_obj = obj_new( "vxagent" );
    $vxagent_obj[vxstub] = $vxstub_obj;
    $vxagent_obj[host_id] = $host_id;

    debugLog("vxagent_create: vxstub obj is ".print_r($vxstub_obj,TRUE));
    debugLog("vxagent_create: vxagent obj is ".print_r($vxagent_obj,TRUE));
    return $vxagent_obj;
}
?>
