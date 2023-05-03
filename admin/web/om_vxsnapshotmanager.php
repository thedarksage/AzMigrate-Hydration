<?

function vxsnapshotmanager_create( $vxstub ) {
    $obj = obj_new( "vxsnapshotmanager" );
    $obj[vxstub] = $vxstub;
    return $obj;
}

function vxsnapshotmanager_get_snapshot_count( $obj ) {
    return 303; # testing
}

?>
