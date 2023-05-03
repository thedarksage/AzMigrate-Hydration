<?

include_once("om_conn.php");
include_once('om_bpm.php');
if (!$functions_included) { include_once( 'om_functions.php' ); }

function setDeleteFlag($ids){
	global $conn_obj;
	if(strlen($ids)>0){
		
		$query="update retLogHistory set deleted=2,modifiedDate=now() where id IN (".$ids.")";

		$set = $conn_obj->sql_query ($query);		
	}
}

?>
