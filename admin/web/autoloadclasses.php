<?php

include_once('config.php');
function &classload( $classname, $path, $params = null )
{
        global $REPLICATION_ROOT;
        if( !class_exists( $classname ))
        {
			#echo $REPLICATION_ROOT . '/' . str_replace( '.', '/', $path ) . '.php';
			require_once( $REPLICATION_ROOT.'/'. str_replace( '.', '/', $path ) . '.php' );
        }
        if( $params != null )
        {
                #echo "came here";
                $params = implode( ',', $params );
                eval( "\$object =& new $classname( $params );" );
                return $object;
        }
		#echo "came here";
        return new $classname;
}
?>