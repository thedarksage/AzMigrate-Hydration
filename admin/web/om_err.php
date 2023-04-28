<?
$INMAGE_ERRNO = array();

function err_set( $err ) { 
    global $INMAGE_ERRNO;
    if( NULL == $err || $err == "" ) { return; }
    if( function_exists( 'debug_backtrace' ) ) {
        $err = debug_backtrace();
    }
    $INMAGE_ERRNO []= $err;
}

function err_is_set() { 
    global $INMAGE_ERRNO; 
    return count($INMAGE_ERRNO) > 0;
}

function err_clear(){ 
    global $INMAGE_ERRNO;
    $INMAGE_ERRNO = array();
}

function err_get() { 
    global $INMAGE_ERRNO; 
    return $INMAGE_ERRNO;
}

?>
