<?

	include_once ('config.php');
	
	global $HOST_LOG_DIRECTORY;
	global $SLASH, $commonfunctions_obj;
	if(!file_exists($HOST_LOG_DIRECTORY))
	{
		mkdir($HOST_LOG_DIRECTORY);
		$iterator = new RecursiveIteratorIterator(
					new RecursiveDirectoryIterator($HOST_LOG_DIRECTORY), RecursiveIteratorIterator::SELF_FIRST); 
		foreach($iterator as $item) 
		{ 
			$chmod_status = chmod($item, 0777); 
		} 
	}
	
	$module = '';
	$alert_type = '';
	
	if(isset($_POST[msg]))
	{
		$hostid = $_POST[id];
		$commonfunctions_obj->conn = $conn_obj;
		$is_valid_host_id = $commonfunctions_obj->isValidComponent($hostid);	
		if ($is_valid_host_id)
		{
			$fName=$_POST[id].".log";
			$data=$_POST[msg]."\n";
			$ts=$_POST[ttime];
			$ts_cx = "(".date("Y-m-d G:i:s").")"; 
			$elevel=$_POST[errlvl];
			$agent = $_POST[agent];
			$module = $_POST[module];

			if ($module == '')
				$module=0;
				$alert_type = $_POST[alert_type];
			if ($alert_type == '')
				$alert_type=0;

			$msg = $data;
			$data=$ts_cx."\t".$ts."\t".$elevel."\t".$agent." : ".$data;
			$fPath=$HOST_LOG_DIRECTORY.$SLASH.$fName;
			$fp = $commonfunctions_obj->file_open_handle($fPath,'a');
			if(!$fp){
				echo "Error! Couldn't open the file ($fPath).";
				return;
			}

			if(!fwrite($fp,$data)){
				echo "Cannot write to file ($fPath).";
			}

			if(!fclose($fp)){
				echo "Error! Couldn't close the file ($fPath).";
			}
			

			if($elevel == 'ALERT')
			{	
				$CXLOGGER_MULTI_QUERY = array();
				$commonfunctions_obj = new CommonFunctions();
				$batch_sqls_obj = new BatchSqls();
				$cx_logger = 1;
				$commonfunctions_obj->process_agent_error($hostid,$elevel,$msg,$ts,$module,$alert_type,$cx_logger);
				$batch_sqls_obj->sequentialBatchUpdates($CXLOGGER_MULTI_QUERY);
			}
		}
	}
?>
