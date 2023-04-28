<?

include_once("conn.php");
include_once('bpm.php');
if (!$functions_included) { include_once( 'functions.php' ); }

function setDeleteFlag($ids){
	global $conn_obj;
	if(strlen($ids)>0){
	
		$query="update retLogHistory set deleted=2,modifiedDate=now() where id IN (".$ids.")";

		$set = $conn_obj->sql_query ($query);
		if (!$set)
	       {
		      die ($conn_obj->sql_error());
		}
	}
}
?>
