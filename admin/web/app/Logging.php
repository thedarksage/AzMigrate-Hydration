<?php
/*
 *$Header: /src/admin/web/app/Logging.php,v 1.4 2008/05/05 10:45:27 Pawan
 *======================================================================== 
 * FILENAME
 *  Logging.php
 *
 * DESCRIPTION
 *  This script contains logging class which is useful to write the log entries.
 *
 *=======================================================================
 *                 Copyright (c) InMage Systems                    
 *=======================================================================
*/
class Logging
{
	private $log_level;
	private $debug_file;
	private $is_stub_call;
	private $unmarshal_log;
	public $global_log_file;
	public $appinsights_log_file;
	/*
	* Function Name: Logging
	*
	* Description:
	*    This is the constructor of the class.Invokes the set error handler 
	*    method to the call.
	*
	*/
	function __construct($default_log_file, $appinsights_default_log_file="phpdebug")
	{
		/*Gets the global variable values, which is defined in config.php*/
		global $LOG_LEVEL;
		global $PHPDEBUG_FILE;
		global $UNMARSHAL_FILE;
		global $is_stub_call;
		
		$this->log_level = $LOG_LEVEL;
		$this->debug_file = $PHPDEBUG_FILE;
		$this->unmarshal_log = $UNMARSHAL_FILE;
		$this->is_stub_call = $is_stub_call;
		$this->global_log_file = $default_log_file;
		$this->appinsights_log_file = $appinsights_default_log_file;

		$this->setErrorHandler();
	}

	/*
	* Function Name: setErrorHandler
	*
	* Description:
	*    Sets the error hanlder to the script.
	*
	*/
	private function setErrorHandler()
	{
		set_error_handler(array(&$this, 'my_error_handler'));
	}
	
	private function do_telemetry($current_folder, $each_log_chunk,$appinsights_log_file, $file_extension)
	{
		global $TELEMETRY_ROOT,$TELEMETRY_FILE_SIZE, $SLASH, $UNDER_SCORE,$commonfunctions_obj, $APPINSIGHTS_WRITE_DIR_COUNT_LIMIT, $CS_LOGS_ROOT,$REPLICATION_ROOT,$TELEMETRY_FOLDER_COUNT_FILE;
		global $AUDIT_APPINSIGHTS_LOG;
		
		$digits = 6;
		$telmetry_root_status = 1;
		$log_root_status = 1;
		$current_folder_output = 1;
		
		if (!file_exists($CS_LOGS_ROOT))
		{
			$log_root_status = mkdir($CS_LOGS_ROOT);
		}
		if (!file_exists($TELEMETRY_ROOT))
		{
			$telmetry_root_status = mkdir($TELEMETRY_ROOT);
		}

		if($GLOBALS['auditLog'])
		{
			$appinsights_log_file = $AUDIT_APPINSIGHTS_LOG;
		}
				
		$telemetry_folder_exhausted_file_path = $REPLICATION_ROOT.$TELEMETRY_FOLDER_COUNT_FILE;
		
		if (!file_exists($telemetry_folder_exhausted_file_path) && ($log_root_status) && ($telmetry_root_status))
		{
		
			if (!file_exists($CS_LOGS_ROOT.$current_folder))
			{
				$current_folder_output = mkdir($CS_LOGS_ROOT.$current_folder);
			}
			
			$current_file_path = $CS_LOGS_ROOT.$current_folder.$SLASH.$appinsights_log_file.$file_extension;
			
			if (file_exists($current_file_path) && ($current_folder_output))
			{	
				clearstatcache();
				//Do this only for summary contents
				if ($file_extension == Extension::JSON) 
				{				
					//If file size exceeds the size limit, rename the existing file.
					if((filesize($current_file_path)+ strlen($each_log_chunk)) >= $TELEMETRY_FILE_SIZE)
					{
						//Get unix time stamp
						$current_timestamp = time();
						//Get 6 digits random number.
						$random = rand(pow(10, $digits-1), pow(10, $digits)-1);
						$backup_file_path = $CS_LOGS_ROOT.$current_folder.$SLASH.$appinsights_log_file.$UNDER_SCORE.$current_timestamp.$UNDER_SCORE.$random.$file_extension;
						clearstatcache();
						if (file_exists($current_file_path))
							$rename_status = rename($current_file_path,$backup_file_path);
					}
					clearstatcache();
				}
			}
			$log_write_status = error_log($each_log_chunk,3,$CS_LOGS_ROOT.$current_folder.$SLASH.$appinsights_log_file.$file_extension);
		}
		if($GLOBALS['auditLog'])
		{
			$GLOBALS['auditLog'] = false;
		}
		/*
			When you use stat(), lstat(), or any of the other functions listed in the affected functions list (below), PHP caches the information those functions return in order to provide faster performance. However, in certain cases, you may want to clear the cached information. For instance, if the same file is being checked multiple times within a single script, and that file is in danger of being removed or changed during that script's operation, you may elect to clear the status cache. In these cases, you can use the clearstatcache() function to clear the information that PHP caches about a file.

			You should also note that PHP doesn't cache information about non-existent files. So, if you call file_exists() on a file that doesn't exist, it will return FALSE until you create the file. If you create the file, it will return TRUE even if you then delete the file. However unlink() clears the cache automatically.
			Note:
			This function caches information about specific filenames, so you only need to call clearstatcache() if you are performing multiple operations on the same filename and require the information about that particular file to not be cached.

			Affected functions include stat(), lstat(), file_exists(), is_writable(), is_readable(), is_executable(), is_file(), is_dir(), is_link(), filectime(), fileatime(), filemtime(), fileinode(), filegroup(), fileowner(), filesize(), filetype(), and fileperms(). In this case, filesize used multiple times in a single script execution, hence, clearstatcahe required. Otherwise file size results are weird and causing unexpected functionality. 
			
			Conclusion:
			PHP caches data for some functions for better performance. If a file is being checked several times in a script, you might want to avoid caching to get correct results. To do this, use the clearstatcache() function.
		*/
	}
	
	private function get_current_folder()
	{
		global $UNDER_SCORE;
		//Get Unix time stamp. Folder creations are based on Unix time stamp value input across PHP & PERL.
		$timestamp = time();
		$date = date("Y_m_d",$timestamp); 
		$current_hour = date("H",$timestamp);
		$next_hour = $current_hour+1;
		$next_hour = str_pad($next_hour,2,"0",STR_PAD_LEFT);
		$log_dir = $date.$UNDER_SCORE.$current_hour.$UNDER_SCORE.$next_hour;
		return $log_dir;
	}
	
	/*
	* Function Name: my_error_handler
	*
	* Description:
	*   a) This is the call back function do the actual logging
	*   in phpdebug.txt based on the user configuration.
	*   b) If any error or warning occurs for the agent cx communication call
	*   it logs the entry and do the 500 header and prints the debug back 
	*   trace.    
	*    
	* Parameters:
	*   error number, error message, file name as default, line number as 
	*   default, empty context as default.
	*/ 
	public function my_error_handler(
	$errno="INFO", 
	$errmsg, 
	$is_telemetry=Log::MDS,
	$filename=__FILE__,
	$linenum=__LINE__,
	$context=''
	)
	{
		global $IS_APPINSIGHTS_LOGGING_ENABLED, $SLASH, $HOST_GUID,$CS_IP, $TELEMETRY_FILE_SIZE, $logging_obj, $APPINSIGHTS_RECORD_SEPARATOR, $IS_MDS_LOGGING_ENABLED,$commonfunctions_obj;

		/*Categorising error, warning and notice constants.*/
		$error_list =  array(
		E_ERROR,
		E_PARSE,
		E_CORE_ERROR,
		E_COMPILE_ERROR,
		E_USER_ERROR);
		
		$warning_list = array(
		E_WARNING,
		E_CORE_WARNING,
		E_COMPILE_WARNING,
		E_USER_WARNING);

		$notice_list = array(E_NOTICE,E_USER_NOTICE,E_STRICT);
		$log_config = explode(",",$this->log_level);
		$date_time = date('Y-m-d H:i:s (T)');

		$errortype = array (
		E_ERROR => 'Error',
		E_WARNING => 'Warning',
		E_PARSE => 'Parsing Error',
		E_NOTICE => 'Notice',
		E_CORE_ERROR => 'Core Error',
		E_CORE_WARNING => 'Core Warning',
		E_COMPILE_ERROR => 'Compile Error',
		E_COMPILE_WARNING => 'Compile Warning',
		E_USER_ERROR => 'User Error',
		E_USER_WARNING => 'User Warning',
		E_USER_NOTICE => 'User Notice',
		E_STRICT => 'Strict Notice'
		);
		
		if (!is_array($errmsg))
		{
			$errmsg = array("LogString"=>$errmsg);
		}
		$trace = debug_backtrace();
		$caller = array_shift($trace);
		//Caller file name and line no to capture.
		$errmsg["File"] = basename($caller['file']);
		$errmsg["Line"] = $caller['line'];
		
		/*Log the messages.*/
		if(	(in_array('ERROR',$log_config) && in_array($errno,$error_list))
		  ||(in_array('WARNING',$log_config) && in_array($errno,$warning_list))
		  ||(in_array('NOTICE',$log_config) && in_array($errno,$notice_list))
		  ||(in_array('DEBUG',$log_config) )
		  ||($errno == "INFO") )
		{
			/*
				MDS = Logging caller doesn't have parameter and picks default value MDS and should go to only var folder logs.
				BOTH = Logging caller has parameter value BOTH and should go to var and telemetry folder logs.
				APPINSIGHTS = Logging call has parameter value APPINSIGHTS and should go to only telemetry folder logs.
				Either telemetry configuration on or off, Either logging caller pass with value 0 or 1, write it into current log files under var path.
			*/
			if (!$GLOBALS['record_meta_data'])
			{
				$cs_version_array = $commonfunctions_obj->get_inmage_version_array();
				$GLOBALS['record_meta_data'] = array("CSGUID"=>$HOST_GUID, "CSIP"=>$CS_IP, "CSVERSION"=>$cs_version_array[0]);
			}
			$GLOBALS['record_meta_data']["DateTime"] = $date_time;
			if 	(
					( $is_telemetry == Log::MDS || $is_telemetry == Log::BOTH) 
					&& ($IS_MDS_LOGGING_ENABLED == "1")
					&& is_array($errmsg)
				) 
				
			{
				$log_data_string = print_r(array_merge($GLOBALS['record_meta_data'],$errmsg),TRUE);
				$log_write_status = error_log($log_data_string,3,$this->global_log_file);	
			}
			/*
				If telemetry configuration is on in amethyst configuration, then only write it into telemetry folder logs.
				If telemetry configuration is off in amethyst configuration, stop the telemetry completely.
			*/
			if (	( $is_telemetry == Log::APPINSIGHTS || $is_telemetry == Log::BOTH) 
					&& 
					($IS_APPINSIGHTS_LOGGING_ENABLED == "1") 
					&& 
					is_array($errmsg) 
				)
			{
				//Get record Meta data json encoding.
				$record_meta_data_json = json_encode($GLOBALS['record_meta_data']);
				
				//Get actual log content json encoding.
				$log_data_string_json = json_encode($errmsg);				
				
				//Compute record length
				$record_json_length = strlen($record_meta_data_json) + strlen($APPINSIGHTS_RECORD_SEPARATOR) + strlen($log_data_string_json);

				//Get the current fodler with required format.	
				$current_folder = $this->get_current_folder();
				
				
				if ($record_json_length <= $TELEMETRY_FILE_SIZE)
				{
					//If it is only one chunk, then merge the record meta data and log content structures and convert to json serialize format.
					$summary_chunk = json_encode(array_merge($GLOBALS['record_meta_data'],$errmsg)).$APPINSIGHTS_RECORD_SEPARATOR;
					
					//Write to summary json file.
					$this->do_telemetry($current_folder,$summary_chunk,$this->appinsights_log_file,Extension::JSON);
				}
				else
				{
					//Generate correlation id to form a relation between summary and verbose log.
					$guid = sprintf('%04X%04X-%04X-%04X-%04X-%04X%04X%04X', mt_rand(0, 65535), mt_rand(0, 65535), mt_rand(0, 65535), mt_rand(16384, 20479), mt_rand(32768, 49151), mt_rand(0, 65535), mt_rand(0, 65535), mt_rand(0, 65535));
					$GLOBALS['record_meta_data']["CorrelationId"] = $guid;

					//Get record Meta data json encoding.
					$record_meta_data_json = json_encode($GLOBALS['record_meta_data']);
					
					//Write the summary
					$summary_chunk = $record_meta_data_json.$APPINSIGHTS_RECORD_SEPARATOR;
					$this->do_telemetry($current_folder,$summary_chunk,$this->appinsights_log_file,Extension::JSON);
					
					//Write to verbose
					$this->do_telemetry($current_folder,$record_meta_data_json.$log_data_string_json,$guid,Extension::LOG);
				}
			}
		}
			
		/*
			If the call is from agent-cx communication call & is not a developer
			debug message, then do the following.If the script generates fatal error or warning, 
			then as a response it sends HTTP 500 header to all clients.
		*/
		if (	($this->is_stub_call == 1) 
			&&	($errno != 'INFO') && ($errno != 'DEBUG' ))
		{
			if(		(E_NOTICE != $errno) 
				&&	(E_USER_NOTICE != $errno) 
				&&      (E_STRICT != $errno)
				 &&      (E_DEPRECATED != $errno))
			{
				/*
				Purpose of $http_header_500 global: This variable sets to TRUE whenever Script throws Warning or Fatal error in Logging class my_error_handler method. Though Script generates warning for any reason of failure and it is continue to execute till end. On Error or Warning the custom error handler takes control and sends HTTP 500 header response to clients. Hence, Script should know the based on this global variable value at the end  to write as "Success" or "Fail" in app insights log content record for the client call execution status.
				*/
				$GLOBALS['http_header_500'] = TRUE;
				$trace = debug_backtrace();
				$caller = array_shift($trace);
				$caller_string = basename($caller['file'])."::".$caller['function']."::".$caller['line'];
				$logging_obj->my_error_handler("INFO",array("Result"=>500,"Status"=>"Fail","Reason"=>print_r($errmsg,TRUE),"ErrorType"=>$errno,"Trace"=>$caller_string),Log::BOTH);
				$log_write_status = error_log(
							$log_data_string,3,$this->unmarshal_log);
				header('HTTP/1.0 500 InMage method call returned an error',
				TRUE,500 );
				header('Content-Type: text/plain' );
				
				print $log_data_string;

			}
		}
	}
}
?>