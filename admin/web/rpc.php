<?
ob_start();
$is_stub_call = 1;
include_once('om.php');

/* 
* Function Name: inmage_rpc 
*  
* Description:
* This function is invoked from request_handler.php which receives the call from agent's side.
* This is the first of the five functions defined in rpc.php that the code flow passes through.
*        
* Parameters:
* Param 1 [IN]: $str 
*     - This holds the serialized string that agent has sent to cx. 
*     The string contains name of the function that agent wants to get invoked and the arguments as needed by the function.
*     
* Return Value:
* $serial_string - serialized string returned by the function that agent wants to get invoked
*/
function inmage_rpc( $str ) 
{
	global $logging_obj,$HOST_GUID, $LOG_ROOT,$UNMARSHAL_FILE,$REPLICATION_ROOT;
	
	
    $call_start_time = microtime(true);   
	$serial_string = serialize( dispatch_cx_call( $str ) );
    if(preg_match ("/Linux/i", php_uname())) 
    {
        $agentLogFile = "/home/svsystems/var/enableAgentCallLogs.txt";
    }
    else
    {
        $agentLogFile = "$REPLICATION_ROOT\\var\\enableAgentCallLogs.txt";
    }
	
    
    if (file_exists($agentLogFile))
    {
        $call_duration = microtime(true) - $call_start_time; 
		populateAgentCallsDBTable($str, $serial_string, $call_duration);
    }
	
	/*5600-Capturing the agent sent string and data string, which is having
	*null data in the serailzed form(N;)
	*/
	$null_string = 'N;';
	if (	($serial_string != $null_string) 
		&&	(strpos($serial_string,$null_string) != FALSE))
	{
		$logging_obj->global_log_file = $UNMARSHAL_FILE;
	}
	
	/* Remove buffer content */
	$buffer_contents = trim(ob_get_contents());
	ob_end_clean();
	if($buffer_contents)
	{
		unmarshalLog("$buffer_contents");
	}
	
	print_r($serial_string);
}

/*
 * Function Name: dispatch_cx_call
 *
 * Description: This API will unserialize the string which the agent sends, 
 *    and checks if it is a stub or a configurator call and directs it to the 
 *    function accordingly.
 * 
 * Parameters:
 *    Param 1 [IN]:$dpc 
 * 
 * Return Value:
 *    Ret Value: No return value
 *    
*/
function dispatch_cx_call( $dpc ) 
{

    $arg = unserialize( $dpc );
	$obj = $arg[0];
    $func = $arg[1];
	
    if( NULL == $arg ) 
	{
        trigger_error( "couldn't unserialize" );
		header('HTTP/1.0 500 InMage method call returned an error with data unserialize failure.',TRUE,500 );
		header('Content-Type: text/plain' );
		exit(0);		
    }
    if( !is_array( $arg ) ) 
	{ 
        trigger_error( "unserialized results not an array" );
		header('HTTP/1.0 500 InMage method call returned an error with post data conversion to array failed after unserialize.',TRUE,500 );
		header('Content-Type: text/plain' );
		exit(0);
    }

    # configurator methods begin with :: all have object as first arg
    if( strpos( $arg[0], "::" ) === 0 ) 
	{
        $arg[0] = substr( $arg[0], 2 );
        return inmage_configurator_dispatch( $arg );
    }
	
	return inmage_method_dispatch( $obj, $func, array_slice( $arg, 2 ) );
}

/* 
* Function Name: inmage_configurator_dispatch 
*  
* Description:
* This function converts the function name from camel case format to underscore format.
* It prepares the function name as per configurator call and arguments required for it, and invokes inmage_call_func with the same.
*        
* Parameters:
* Param 1 [IN]: $args 
*     - This holds an array containing the function name and arguments sent by agent
*     
* Return Value:
* $ret_val: the return value from inmage_call_func function is returned

Example of a configurator call:
	Agent sent serialized string:
a:5:{i:0;s:19:"::registerPushProxy";i:1;s:36:"7cae63d2-8754-4e69-aca4-3ce882c7b053";i:2;s:11:"10.0.168.20";i:3;s:9:"CentOS5u5";i:4;i:2;}	
	Structure: 
Array
(
    [0] => ::registerPushProxy
    [1] => 7cae63d2-8754-4e69-aca4-3ce882c7b053
    [2] => 10.0.168.20
    [3] => CentOS5u5
    [4] => 2
)
 */
function inmage_configurator_dispatch( $args = array() ) {
	global $logging_obj;
	
	
	$func= camel_case_to_underscore('Configurator'.ucfirst(array_shift($args)));
    $obj = inmage_get_configurator();
    array_unshift( $args, $obj );
	$start_time = (round(microtime(true),4));
    $ret_val = inmage_call_func( $func, $args );
	$end_time =  (round(microtime(true),4));
	$execution_time = (round((($end_time)-($start_time)),4));
	$logging_obj->my_error_handler("DEBUG",$_SERVER['REMOTE_ADDR'],"$func","$start_time","$end_time","$execution_time");
    return $ret_val;
}

/* 
* Function Name: inmage_method_dispatch 
*
* Description:
* This function converts the function name from camel case format to underscore format.
* It prepares the function name as per stub call and also the arguments required for it, and invokes inmage_call_func with the same.
*        
* Parameters:
* Param 1 [IN]: $dpc - This string holds the zeroth element of unserialized string that is sent by agent
* Param 2 [IN]: $method - This string contains the function name
* Param 3 [IN]: $args - This array contains the arguments that are needed for the function name in $method in Param 2 above
*     
* Return Value:
* $ret_val: the return value from inmage_call_func function is returned

Example of a stub call:
	Agent sent serialized string:
a:3:{i:0;s:77:"a:2:{i:0;s:12:"::getVxAgent";i:1;s:35:"FFC514A0-3884-1A43-871996D4F467E7EE";}";i:1;s:18:"getInitialSettings";i:2;i:2;}
	structure:
Array
(
    [0] => a:2:{i:0;s:12:"::getVxAgent";i:1;s:35:"FFC514A0-3884-1A43-871996D4F467E7EE";}
    [1] => getInitialSettings
    [2] => 2
)
*/

function inmage_method_dispatch( $dpc, $method, $args = array() ) {
	global $logging_obj;
	

    # get $obj from the deferred procedure call
    $obj = dispatch_cx_call( $dpc );
    if( !obj_is_valid( $obj ) ) 
	{
        trigger_error( "not a valid object: $obj" );
		header('HTTP/1.0 500 InMage method call returned an error with invalid object',TRUE,500 );
		header('Content-Type: text/plain' );
		exit(0);
    }

    # call $method on $obj with $args agument array
	
	$func = camel_case_to_underscore(obj_class_name( $obj ) . ucfirst($method));
    array_unshift( $args, $obj );
	$start_time = (round(microtime(true),4));
	$ret_val = inmage_call_func( $func, $args );
	$end_time =  (round(microtime(true),4));
	$execution_time = (round(($end_time-$start_time),4));
	$logging_obj->my_error_handler("DEBUG",$_SERVER['REMOTE_ADDR'],"$func","$start_time","$end_time","$execution_time");
	return $ret_val;
}

/*
 * Function Name: requestLog
 *
 * Description:
 *    Function to log the http request functions with execution times.
 *
 * Parameters:
 *    Param 1 [IN]:$ip_address (ip address)
 *    Param 2 [IN]:$func_name (function name)
 *    Param 3 [IN]:$start (start execution time)
 *    Param 4 [IN]:$end (end execution time)
 *    Param 5 [IN]:$finish (total execution time)
 *
 * Return Value:
 *    Ret Value: No return value
 *
 * Exceptions:
 *    
*/

function requestLog($ip_address,$func_name,$start,$end,$finish)
{
	global $REPLICATION_ROOT;
	$filename = "$REPLICATION_ROOT/var/request_enable.log";

	if (preg_match('/Windows/i', php_uname()))
			$filename="$REPLICATION_ROOT\\var\\request_enable.log";
	if (file_exists($filename)) 
	{
		global $REQUESTS_FILE;
		$string = $ip_address.",".$func_name.",".$start.",".$end.",".$finish;
		$fr = fopen($REQUESTS_FILE, 'a+');
		if(!$fr) 
		{
			#echo "Error! Couldn't open the file.";
		}
		if (!fwrite($fr, $string . "\n")) 
		{
			#print "Cannot write to file ($filename)";
		}
		if(!fclose($fr)) 
		{
			#echo "Error! Couldn't close the file.";
		} 
	}
}


/* 
* Function Name: inmage_call_func 
*  
* Description:
* This function invokes those user-defined functions whose names the agent sends in its backend call.
* These functions are defined in om_vxstub.php and om_configurator.php files.
*     
* Parameters:
* Param 1 [IN]: $func - This string contains the function name
* Param 2 [IN]: $args - This array contains the arguments that are needed for the function name contained in $func in Param 1 above
*     
* Return Value:
* $ret_val: the return value from inmage_call_func function is returned
*/
function inmage_call_func( $func, $args ) 
{
	global $logging_obj, $LOG_ROOT, $record_meta_data, $HOST_GUID, $CS_IP, $commonfunctions_obj;
	
	if ($func == 'vxstub_get_initial_settings' || 
		$func == 'vxstub_get_application_settings' || 
		$func == 'vxstub_update_discovery_info' || 
		$func == 'vxstub_update_policy' || 
		$func == 'vxstub_update_cdp_information' || 
		$func == 'vxstub_update_cdp_information_v2' || 
		$func == 'vxstub_register_cluster_info' || 
		$func == 'vxstub_failover_commands_update' || 
		$func == 'vxstub_update_agent_installation_status' || 
		$func == 'configurator_register_host_static_info' || 
		$func == 'configurator_register_host_dynamic_info' || 
		$func == 'configurator_register_host' || 
		$func == 'configurator_unregister_host')
	{
		$log_file = "{$func}.log";
	}
	elseif (strpos($func, 'configurator') === False) 
	{
		$log_file = "vxstub.log";
	}
	else 
	{
		$log_file = "configurator.log";
	}
	
	if (strpos($func, 'configurator') === False)
	{
		$caller_host_id = $args[0]['host_id'];
	}
	else
	{
		$caller_host_id = $args[1];
	}
	$logging_obj->global_log_file = $LOG_ROOT.$log_file;
	$logging_obj->appinsights_log_file = "{$func}";
	
    if( !function_exists( $func ) ) {
        trigger_error( "not a function: $func" );
		header('HTTP/1.0 500 InMage method call returned an error with invalid function $func.',TRUE,500 );
		header('Content-Type: text/plain' );
		exit(0);
    }
	$cs_version_array = $commonfunctions_obj->get_inmage_version_array();
	$GLOBALS['record_meta_data']	= array("CSGUID"=>$HOST_GUID,
								"CSIP"=> $CS_IP,
								"CSVERSION"=>$cs_version_array[0],
								"RecordType"=>$func,
								"HostId"=>$caller_host_id);	
    if( function_exists( 'call_user_func_array' ) ) 
	{
		//Allows only functions to execute, which are start with vxstub or configurator
		if ((preg_match("/\A(vxstub)/i", $func)) or 
			(preg_match("/\A(configurator)/i", $func)))
		{
			if ((is_array($args) and count($args) > 0))
			{
				//Mobility agent calls any configurator or stub call with _inmage_class value as configurator.
				if ($args[0][_inmage_class] == "configurator")
				{
					$guid_format_status = $commonfunctions_obj->is_guid_format_valid($args[1]);
					
					if ($guid_format_status == false)
					{
						$logging_obj->my_error_handler("INFO","Invalid guid format for host id $caller_host_id for function call $func");
						header('HTTP/1.0 500 InMage method call returned an error with invalid guid format.',TRUE,500 );
						header('Content-Type: text/plain' );
						exit(0);
					}
				}
			}

			return call_user_func_array( $func, $args );			
		}
		else
		{
			$logging_obj->my_error_handler("INFO","Invalid function call $func for host id $caller_host_id");
			header('HTTP/1.0 500 InMage method call returned an error with invalid function call.',TRUE,500 );
			header('Content-Type: text/plain' );
			exit(0);
		}
    }

    switch( count( $args ) ) {
    case 0: return call_user_func($func); break;
    case 1: return call_user_func($func,$args[0]); break;
    case 2: return call_user_func($func,$args[0],$args[1]); break;
    case 3: return call_user_func($func,$args[0],$args[1],$args[2]);break;
    case 4: return call_user_func($func,$args[0],$args[1], $args[2],$args[3]);
            break;
    case 5: return call_user_func($func,$args[0],$args[1],$args[2],$args[3],
                $args[4]); break;
    case 6: return call_user_func($func,$args[0],$args[1],$args[2],$args[3],
                $args[4],$args[5]); break;
    case 7: return call_user_func($func,$args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6]); break;
    case 8: return call_user_func($func,$args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6],$args[7]); break;
    case 9: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6],$args[7],$args[8]); break;
    case 10: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6],$args[7],$args[8],$args[9]); break;
    case 11: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6],$args[7],$args[8],$args[9],$args[10]); break;
    case 12: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6],$args[7],$args[8],$args[9],$args[10],$args[11]); break;
    case 13: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6],$args[7],$args[8],$args[9],$args[10],$args[11],$args[12]); break;
    case 14: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6],$args[7],$args[8],$args[9],$args[10],$args[11],$args[12],$args[13]); break;
    case 15: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
                $args[4],$args[5],$args[6],$args[7],$args[8],$args[9],$args[10],$args[11],$args[12],$args[13],$args[14]); break;
	#Backported patch history changes from 3.5.2 to 4.1,added to handle register host call.
    case 19: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
	        $args[4],$args[5],$args[6],$args[7],$args[8],$args[9],$args[10],$args[11],$args[12],$args[13],$args[14],$args[15],$args[16],$args[17],$args[18]); break;
	case 20: return call_user_func($func, $args[0],$args[1], $args[2],$args[3],
	        $args[4],$args[5],$args[6],$args[7],$args[8],$args[9],$args[10],$args[11],$args[12],$args[13],$args[14],$args[15],$args[16],$args[17],$args[18], $args[19]); break;

    default:trigger_error("can handle up to ..15 and 20 arguments, not ".count($args));
             break;
    }
    return NULL;
}

/* 
* Function Name: unmarshalLog 
*  
* Description:
* This function writes the logs to $UNMARSHAL_FILE file
*        
* Parameters:
* Param 1 [IN]: $debug_string - This string contains text that would be writen to the $UNMARSHAL_FILE file
*     
* Return Value:
* None
*/
function unmarshalLog ( $debug_string)
{
    global $UNMARSHAL_FILE, $commonfunctions_obj;
    # Some debug
    $date_time = date('Y-m-d H:i:s (T)');
    $debug_string = $date_time."  ".$debug_string."\n";
    $fr = $commonfunctions_obj->file_open_handle($UNMARSHAL_FILE, 'a+');
    if(!$fr) 
    {
        // return if file handle not available.
        return;
        #echo "Error! Couldn't open the file.";
    }
    if (!fwrite($fr, $debug_string . "\n")) 
    {
        #print "Cannot write to file ($filename)";
    }
    if(!fclose($fr)) 
    {
        #echo "Error! Couldn't close the file.";
    }
}

function populateAgentCallsDBTable($request, $response, $call_duration)
{
	global $conn_obj;
	$arg = unserialize($request);
	$host_id = '';
	$function_name = '';
	if(strpos($arg[0], "::") === 0) 
	{
		$host_id = $arg[1];
		$function_name = camel_case_to_underscore('Configurator'.ucfirst(str_replace("::","",$arg[0])));
	}
	else
	{
		$host_id_arr = unserialize($arg[0]);
		$host_id = $host_id_arr[1];
		$function_name = camel_case_to_underscore('Vxstub'.ucfirst($arg[1]));
	}
	
	$sql_delete = "DELETE FROM agentCalls WHERE (UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(callTime)) > 86400";
	$conn_obj->sql_query($sql_delete);
	
	$sql_select = "SELECT (TIME_TO_SEC(now()) - TIME_TO_SEC(callTime)) AS timePassedSinceLastCall FROM agentCalls WHERE hostId='$host_id' AND callName='$function_name' ORDER BY id DESC LIMIT 0,1";
	$rs_select = $conn_obj->sql_query($sql_select);
	$row_select = $conn_obj->sql_fetch_assoc($rs_select);
	$time_passed = ($row_select['timePassedSinceLastCall']) ? $row_select['timePassedSinceLastCall'] : 0;
	
	$sql_insert = "INSERT INTO agentCalls 
					   (hostId, callName, payload, callResponse, callTime, callDuration, timePassedSinceLastCall) 
				   VALUES 
					   ('$host_id', '$function_name', '$request', '$response', now(), '$call_duration', '$time_passed')";
	$conn_obj->sql_query($sql_insert);
}
?>
