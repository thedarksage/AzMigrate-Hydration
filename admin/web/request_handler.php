<?php
include_once 'rpc.php';
extract ($_REQUEST);
if( !strcmp( $action, 'send_data' ) ) 
{
	# Undo annoying PHP 'feature'
	if( get_magic_quotes_gpc() ) 
	{
		$_POST[message] = stripslashes( $_POST[message] );
	}
	inmage_rpc( $_POST[message] );
}
else
{
	print "invalid request: $action\n";
}

?>
