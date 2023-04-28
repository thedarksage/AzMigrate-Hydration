<?
include_once("om_functions.php");

function vxstub_create( $cfg, $host_id ) 
{
    # to do: verify that host_id is valid
    $obj = obj_new( "vxstub" );
    $obj['host_id'] = $host_id;
    return $obj;
}

function vxstub_get_snapshot_manager( $obj ) {
    return vxsnapshotmanager_create( $obj );
}

function vxstub_get_vxagent( $obj, $host_id ) {
    return vxagent_create( $obj, $host_id );
}

#
# The below function is implemented by Srinivas Patnana to construct the proper data log path before 
# sending the data log path to the database, based on the given input and is for windows. This function 
# is using at the time of snapshots(schedule and recovery) creation.
#
function verifyDatalogPath($dPath)
{
        $folderCollection = array();
        $dPathList = explode(":",$dPath);
        $count =  count($dPathList);
        #echo $dPathList[1];
        if ($count > 1)
        {
                $dPathTerms = explode("\\",$dPathList[1]);
                if (count($dPathTerms) > 1)
                {
                        #print_r($dPathTerms);
                        for($i=0;$i<count($dPathTerms);$i++)
                        {
                                if ($dPathTerms[$i]!='')
                                {
                                        $folderCollection[] = $dPathTerms[$i];
                                }
                        }
                        #print_r($folderCollection);
                        if (count($folderCollection) > 0)
                        {
                                $dPath = implode("\\",$folderCollection);
                                $dPath = $dPathList[0].':\\'.$dPath;
                        }// This is the else block if the path doesn't contain any sub folders and the path contains more slashes.
						else //If folder collection is zero
                        {
                                $dPath = $dPathList[0].':\\';
                        }

                }
        }
        #print_r($dPathTerms);
        return $dPath;
}

function vxstub_get_initial_settings( $obj,$arg=0 ) 
{
    global $logging_obj,$HOST_GUID;
	global $conn_obj;
	$BatchSqls_obj = new BatchSqls();
	global $batch_query;
	$batch_query = array();
	$final_hash = array();
	$retVal = array();
	
	if($arg == 2)
	{				
		$hostinfo = get_host_info($obj['host_id']);
		$hostCompatibilityNum = $hostinfo['compatibilityNo'];
		/*
		* This hash gives pair,LogicalVolumes,sharedDevices,processServer,hosts,
		* hatlm,networkAddress table info realted to calling hostId
		* Along with the above table info it also gives retention info 
		* and one to many details related calling hostid pairs
		*/
		$final_hash = hash_construct($obj['host_id'],$hostCompatibilityNum);	
	
		if(($hostCompatibilityNum == '') || ($hostCompatibilityNum == 0) ||($hostCompatibilityNum >= 710000))
		{
			$sql = "UPDATE 
			              hosts 
				    SET 
					      lastHostUpdateTime = UNIX_TIMESTAMP(now())
				    WHERE
					      id = '$obj[host_id]'";
			$batch_query['update'][] = $sql;
			
			$isTransportSecure = 0;
			$refresh_host_request_id = get_refresh_host_status($obj['host_id'],$hostCompatibilityNum,$final_hash);
			$retVal = array( 0,
				get_host_volume_group_settings( $obj, 2, $final_hash),				
				get_cdp_settings( $obj , $final_hash),
				get_timeout(),
				get_protections_data(),
				prism_settings($obj,$final_hash),
				get_cs_transport_connection_settings($obj['host_id']),
                get_cs_transport_protocol(),
                $isTransportSecure,
				$refresh_host_request_id);        
        }
		$result = (int)$BatchSqls_obj->filterAndUpdateBatches($batch_query,"om_vxstub :: vxstub_get_initial_settings");
		$logging_obj->my_error_handler("DEBUG","get_initial_setting:: when host is <5.5 return value is for host ".$obj['host_id']."----".print_r($retVal,TRUE)."\n".print_r($final_hash,TRUE)."\n");
		return $retVal;
	}
	else
	{
		#Returning 500 response error if agent sends argument value as "0".So that agent will retry.
		$GLOBALS['http_header_500'] = TRUE;
		$logging_obj->my_error_handler("INFO",array("Reason"=>"Arguments zero","Status"=>"Fail","Result"=>500), Log::BOTH);
		header('HTTP/1.0 500 InMage method call returned an error',TRUE,500);
		header('Content-Type: text/plain');
	}
}
/*
 * Function to get the ip address
 */
function getHostIPAddr()
{
	$ipaddress = $_SERVER['SERVER_ADDR'];
	if ($ipaddress == null || (strcasecmp($ipaddress, "127.0.0.1") == 0))
	{
		if($_SERVER['COMPUTERNAME']!=null)
		{
			$ipaddress = gethostbyname($_SERVER['COMPUTERNAME']);
		}
		else if(getenv("HOSTNAME")!=null)
		{
			$ipaddress = gethostbyname(getenv("HOSTNAME"));
		}	
	}
	return $ipaddress;
}

function get_timeout()
{
	/* For time being, lets hard quote that as 10 minutes */

	global  $conn_obj;
    global $HOST_GUID, $logging_obj, $CLUSTER_NAME;
	
	$logging_obj->my_error_handler("DEBUG","get_timeout:: Entered \n");
	$local_ip = getHostIPAddr();
	
    	$domain_sql = "SELECT 
                        timeout
                    FROM
                        standByDetails
                    WHERE
                        appliacneClusterIdOrHostId != '".$HOST_GUID."'
					AND
						appliacneClusterIdOrHostId != '".$CLUSTER_NAME."'";
	
	$result_domain = $conn_obj->sql_query($domain_sql);
	//$logging_obj->my_error_handler("DEBUG","get_timeout:: domain_sql: $domain_sql \n");
	if($conn_obj->sql_num_row($result_domain) > 0)
	{
		$domain_value = $conn_obj->sql_fetch_row( $result_domain);
		$timeout = $domain_value[0];
		$logging_obj->my_error_handler("DEBUG","get_timeout:: timeout: $timeout \n");
		return $timeout;
	}
	else
	{
		$timeout = "0";
		$logging_obj->my_error_handler("DEBUG","get_timeout:: timeout: $timeout \n");
		return $timeout;
	}
	
}

/*
* This function is responsible to give information about passive CX
*/
function get_protections_data()
{
	global  $conn_obj;
    global $HOST_GUID,$logging_obj, $CLUSTER_NAME;
	
	
	$result = array('all'=>"");
	$local_ip = getHostIPAddr();
	
	$domain_sql = "SELECT
                        ip_address,
                        port_number,
                        timeout,
                        nat_ip
                    FROM
                        standByDetails
                    WHERE
                        appliacneClusterIdOrHostId != '".$HOST_GUID."'
					AND
						appliacneClusterIdOrHostId != '".$CLUSTER_NAME."'";
	
	$result_domain = $conn_obj->sql_query($domain_sql);
	//$logging_obj->my_error_handler("DEBUG","get_protections_data:: domain_sql: $domain_sql \n");
	$temp_arr = array();
	if($conn_obj->sql_num_row($result_domain) > 0)
	{	
	    while($domain_value = $conn_obj->sql_fetch_row( $result_domain)) 
	    {
	       $port_number = intval("$domain_value[1]");
	       $temp = array(0,"$domain_value[3]","$domain_value[0]",$port_number);
	       $temp_arr[] = $temp;
		   #$logging_obj->my_error_handler("DEBUG","get_protections_data:: temp_arr: ".print_r($temp_arr,TRUE)." \n");
	    }
	    
	    $result['all'] = $temp_arr;
	}
	else
	{
		$result = array();
	}
	$logging_obj->my_error_handler("DEBUG","get_protections_data:: result: ".print_r($result,TRUE)." \n");
	return $result;
}
    
// This function convert the string time stamp to a numeric value
function convert_retention_win_time($str)
{
    $time = explode(",",$str);
    $year    = str_pad($time[0],4,"0",STR_PAD_LEFT);
    $month   = str_pad($time[1],2,"0",STR_PAD_LEFT);
    $day     = str_pad($time[2],2,"0",STR_PAD_LEFT);
    $hour    = str_pad($time[3],2,"0",STR_PAD_LEFT);
    $minute  = str_pad($time[4],2,"0",STR_PAD_LEFT);
    $second  = str_pad($time[5],2,"0",STR_PAD_LEFT);
    $msec    = str_pad($time[6],3,"0",STR_PAD_LEFT);
    $nsec    = str_pad($time[7],3,"0",STR_PAD_LEFT);
    $misec   = str_pad($time[8],3,"0",STR_PAD_LEFT);
    $total_time = (($year*100000000)+($month*10000000)+($day*1000000)+($hour*100000)
                   +($minute*10000)+($second*1000)+($msec*100)+($nsec*10)+($misec*1));
    return $total_time;
}

function get_host_volume_group_settings($obj,$arg=0, $final_hash=array()) 
{
    $vgs = array();
    global $logging_obj;
    global $conn_obj;
    global $PAUSED_PENDING;
    # add source volumes to list of volume groups
    $volume_groups = array();
	
    $src_vols = get_source_volumes( $obj, $final_hash);
    #debugLog("Got sourceVols as ".print_r($src_vols, TRUE));
	$hst_array = $final_hash[2];
	$id = $obj['host_id'];
    $hostCompatibilityNum = $hst_array[$id]['compatibilityNo'];
		
	/*if ($hostCompatibilityNum >= 500001)         // 500001 is added for 5.0_Sp1 Vx agent.
	{*/		
		if($hostCompatibilityNum >= 510000)
		{
			## 4369 ##
			# Throttle settings are added at the end			
			if($hostCompatibilityNum >= 630050)
			{
				$volumeGroupIdIndex = 39;				
			}
			else if($hostCompatibilityNum >= 560000)
			{
				$volumeGroupIdIndex = 38;				
			}
			else if($hostCompatibilityNum >= 550000)
			{
				## 9448 ##
				#pair level resyncCounter is sent as part of get_initial_settings
				#$volumeGroupIdIndex = 33;
				$volumeGroupIdIndex = 35;				
			}
			else
			{
				/*Added a field for move retention*/
				$volumeGroupIdIndex = 32;
			}
		}
		/*else
		{
			$volumeGroupIdIndex = 31;
		}*/
	//}
	elseif ($hostCompatibilityNum >= 430000 )
	{
		$volumeGroupIdIndex = 29;
	}
	elseif ($hostCompatibilityNum >= 420000 )
    {
		$volumeGroupIdIndex = 23;
    }
    else
    {
		$volumeGroupIdIndex = 22;
    }	
    foreach( $src_vols as $src_vol ) 
	{
	    if( !array_key_exists( $src_vol[$volumeGroupIdIndex], $volume_groups ) ) 
		{
			// here $volumeGroupIdIndex is the index of volumeGroupId
            $volume_groups[ (int)$src_vol[$volumeGroupIdIndex] ] = array();
        }
        $volume_groups[ $src_vol[$volumeGroupIdIndex] ] []= $src_vol;
    }
    foreach( $volume_groups as $vgId => $vg ) 
	{
        if($arg == 2)
        {
            # Assuming source as '1'
            $host_type = 1;
            if ($hostCompatibilityNum >= 550000 )
            {
                $vgs []= array( 0, $vgId, 1, 0, array(), get_host_transport_connection_settings($id, $vgId, $host_type,$final_hash));
            }	
        }
        else
        {
			$vgs []= array( 0, $vgId, 1  , 0 , array() );
        }
        foreach( $vg as $sv ) 
		{   
			$src_state = 0;
			
            array_pop( $sv ); 
            array_unshift( $sv, 0 );
			$vgs[count($vgs)-1][ 4 ] [$sv[1]]= $sv;
        }
    }
    # add target volumes to list of volume groups
    $target_vols = get_target_volumes( $obj, $final_hash);

    $volume_groups = array();

    foreach( $target_vols as $target_vol )
	{
       # index 19 is for volume groupid
       # volume groupid is the last element in the target/source volumes array
       # then no need of  $tv[7] = $tv[9] i.e copying last element into the index of volume groupid

        if( !array_key_exists( $target_vol[$volumeGroupIdIndex], $volume_groups ) ) 
		{
            $volume_groups[ (int)$target_vol[$volumeGroupIdIndex] ] = array();
        }

        $volume_groups[ (int)$target_vol[$volumeGroupIdIndex] ] []= $target_vol;
    }

    foreach( $volume_groups as $vgId => $vg )
	{
	    if($arg == 2)
        {
            # Assuming target as '2'
            $host_type = 2;
            if ($hostCompatibilityNum >= 550000 )
            {
                $vgs []= array( 0, $vgId, 2, 0, array(),get_host_transport_connection_settings($id, $vgId, $host_type,$final_hash));
            }            
        }
        else
        {
            $vgs []= array( 0, $vgId, 2 , 0 , array() );
        }
        foreach( $vg as $tv ) 
		{
			$state = 0;    
			  
            array_pop( $tv ); // pops the last element from array
            array_unshift( $tv, 0 );
			$vgs[count($vgs)-1][ 4 ] [ $tv[1] ]= $tv;           
        }
    }	
	#$logging_obj->my_error_handler("DEBUG","get_initial_setting -->$arg".print_r($vgs,TRUE));
	return array( 0, $vgs );
}

function is_fabric_lun_valid($sourceId, $sourceDeviceName,$destId, $destDeviceName)
  {
	global $conn_obj;
        $rs =  $conn_obj->sql_query("select Phy_lunid from ".
                          "srcLogicalVolumeDestLogicalVolume ".
                          "where ".
                          " sourceHostId = '$sourceId' and ".
                          "  sourceDeviceName = '".
                          $conn_obj->sql_escape_string($sourceDeviceName) . "'".
                          " and destinationHostId = '$destId' ".
                          " and   destinationDeviceName = '" .
                         $conn_obj->sql_escape_string($destDeviceName) . "'");
        if (!$rs) {
            // report error
            return 0;
        }

        $row = $conn_obj->sql_fetch_row($rs);
        if (isset($row[0]) && $row[0] != 0) {
            return 1;
        }
        return 0;
   }

function is_array_lun_valid($sourceId, $sourceDeviceName,$destId, $destDeviceName)
  {
        global $conn_obj;
        global $logging_obj;
		
    	$logging_obj->my_error_handler("DEBUG","is_array_lun_valid Called\n");
        $sql =  "SELECT  
					ap.solutionType,                                         
					appsc.applicationType,                                         
					ap.planType     
				FROM	
					applicationScenario appsc, 
					applicationPlan ap,
					srcLogicalVolumeDestLogicalVolume slvdlv
				WHERE
					slvdlv.sourceHostId = '$sourceId'
				AND                                         
					slvdlv.destinationHostId = '$destId'
				AND
					slvdlv.sourceDeviceName like '%".$conn_obj->sql_escape_string($sourceDeviceName)."%'
				AND 
					slvdlv.destinationDeviceName like '%".$conn_obj->sql_escape_string($destDeviceName)."%'
				AND 	
					slvdlv.planId = appsc.planId 
                AND
                    slvdlv.planId = ap.planId
				AND
					slvdlv.scenarioId = appsc.scenarioId";
    	$logging_obj->my_error_handler("DEBUG","is_array_lun_valid :: SQL :: $sql\n");
        $rs =  $conn_obj->sql_query($sql);  
        if (!$rs) {
    	    $logging_obj->my_error_handler("ERROR","is_array_lun_valid :: $sql ::Error in above SQL \n");
            // report error
            return 0;
        }

        $row = $conn_obj->sql_fetch_row($rs);
        if (isset($row[0]) && $row[0] == "ARRAY") {
            return 1;
        }
        return 0;
   }

function update_replication_state_status($obj,$device_name,$pairState)
{
	
	global $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING ,$RELEASE_DRIVE_ON_TARGET_DELETE_DONE,$RELEASE_DRIVE_PENDING;
	global $conn_obj,$SOURCE_DELETE_PENDING,$SOURCE_DELETE_DONE;
	global $logging_obj;
	$dest_id = $obj['host_id'];
	$logging_obj->my_error_handler("INFO",array("DeviceName"=>$device_name,"PairState"=>$pairState),Log::BOTH);

	$device_name = $conn_obj->sql_escape_string($device_name);
	$replication_state = $pairState;
	$select_src_sql = "SELECT 
	                       sourceHostId,
						   sourceDeviceName
					   FROM 
					       srcLogicalVolumeDestLogicalVolume
					   WHERE
					       destinationHostId = '$dest_id' 
					   AND 
					       destinationDeviceName='$device_name'";
	$logging_obj->my_error_handler("DEBUG","update_replication_state_status :: SQL as $select_src_sql\n");
	$result_set = $conn_obj->sql_query($select_src_sql);
	$src_row = $conn_obj->sql_fetch_row($result_set);
	$source_id = $src_row[0];
	$source_device_name = $src_row[1];
	$logging_obj->my_error_handler("INFO",array("SourceHostId"=>$source_id,"SourceDeviceName"=>$source_device_name),Log::BOTH);
	$is_fabric = is_fabric_lun_valid($source_id,$source_device_name,$dest_id,$device_name);
	$is_array = is_array_lun_valid($source_id,$source_device_name,$dest_id,$device_name);
	$logging_obj->my_error_handler("DEBUG","update_replication_state_status :: is array lun: $is_array\n");
	if(!$is_fabric || $is_array)
	{
		if($replication_state == $RELEASE_DRIVE_ON_TARGET_DELETE_DONE)
		{
				$update_src_sql = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status = '$SOURCE_DELETE_DONE'
				WHERE destinationHostId = '$dest_id' AND destinationDeviceName='$device_name' AND replication_status = '$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING'";
		}
		else
		{
			$update_src_sql = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status = '$replication_state'
			WHERE destinationHostId = '$dest_id' AND destinationDeviceName='$device_name' AND replication_status = '$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING'";
		}
	   	
	}
	else
	{
	   
	   if($replication_state == $RELEASE_DRIVE_ON_TARGET_DELETE_DONE)
		{
			$update_src_sql = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status = '$SOURCE_DELETE_PENDING'
	WHERE destinationHostId = '$dest_id' AND destinationDeviceName='$device_name' AND replication_status = '$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING'";
		}
		else
		{
		$update_src_sql = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status = '$replication_state'
	WHERE destinationHostId = '$dest_id' AND destinationDeviceName='$device_name' AND replication_status = '$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING'";
	   }
	}
	

	$result = $conn_obj->sql_query($update_src_sql);
	
	if (!$GLOBALS['http_header_500'])
		$logging_obj->my_error_handler("INFO",array("Result"=>$result,"Status"=>"Success"),Log::BOTH);
	else
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
	return TRUE;
	
}
/*
 * Function Name: vxstub_set_pause_replication_status
 *
 * Description:
 *  Agent will call this API for pause replication pair.
 *  
 * Parameters:
 *     [IN]: HostID                 $obj
 *              DeviceName           $device_name
 *              HostType              $host_type
 *              Response Sting       $str
 *
 */
function vxstub_set_pause_replication_status($obj,$device_name,$host_type,$str)
{
    global $conn_obj,$logging_obj,$commonfunctions_obj,$ABHAI_MIB, $TRAP_AMETHYSTGUID, $TRAP_SEVERITY, $TRAP_CS_HOSTNAME, $TRAP_SOURCE_HOSTNAME, $TRAP_SOURCE_DEVICENAME, $TRAP_TARGET_HOSTNAME, $TRAP_TARGET_DEVICENAME, $TRAP_RETENTION_DEVICENAME, $TRAP_AGENT_ERROR_MESSAGE, $TRAP_INSUFFICIENT_RETENTION_SPACE, $TRAP_MOVE_RETENTION_LOG,$HOST_GUID, $TARGET_DATA, $SOURCE_DATA;
	$result = TRUE;    
	try
	{
		if (!in_array($host_type, array(1,2))) 
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for host type $host_type in vxstub_set_pause_replication_status.");
		}
		
		if($host_type == 1)
		{
			$valid_protection = $commonfunctions_obj->isValidProtection($obj['host_id'], $device_name, $TARGET_DATA);
			if (!$valid_protection)
			{
				$logging_obj->my_error_handler("INFO","Invalid post data for target details $device_name in vxstub_set_pause_replication_status.",Log::BOTH);
				//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
				return $result;
			}	
		}
		
		if($host_type == 2)
		{
			$valid_protection = $commonfunctions_obj->isValidProtection($obj['host_id'], $device_name, $SOURCE_DATA);
			if (!$valid_protection)
			{
				$logging_obj->my_error_handler("INFO","Invalid post data for source details $device_name in vxstub_set_pause_replication_status.",Log::BOTH);
				//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
				return $result;
			}	
		}
		
	$conn_obj->enable_exceptions();
	$conn_obj->sql_begin();
	$logging_obj->my_error_handler("INFO",array("DeviceName"=>$device_name,"HostType"=>host_type,"Data"=>$str),Log::BOTH);

	$sparseretention_obj = new SparseRetention();
	/*
	User pause response sample:: 2015-12-14 13:09:55 (UTC) CX :INFO:vxstub_set_pausereplication_status  API 36000c295048ba9c1a5af1cc33e0cc09a,1,pause_components=yes;components=all;pause_components_status=completed;pause_components_message=;
	*/
    $trap_info_str = '';    
    $cx_hostid = $HOST_GUID;
    $cx_hostname = $commonfunctions_obj->getHostName($cx_hostid);    
    $cx_os_flag = $commonfunctions_obj->get_osFlag($cx_hostid);
	
    $host_id = $obj['host_id'];
    $str_status = explode(";",$str);
    $status = array();
	$message = array();
    /*pause status variables */
    $pause_components_status="";
    $move_retention_status= "";
    $components="";
    $pause_components="";
    $move_retention="";
    $error_code="";
    $error_placeholders="";
    /*Contruct key value pair from response string*/
    for($temp = 0; $temp < count($str_status); $temp++)
    {
    
        $result_arr = explode("=",trim($str_status[$temp]));
        $key = $result_arr[0];
		$value = $result_arr[1];
        $status[$key] = $value ;
    }
    /*Collect required string values*/
    /*general pause components*/
    if (array_key_exists('components', $status))
    {
        $components = $status['components'];
    }
    if (array_key_exists('pause_components_status', $status))
    {
        $pause_components_status = $status['pause_components_status'];
    }
    if (array_key_exists('pause_components', $status))
    {
        $pause_components = $status['pause_components'];
    }
    /*move retention pause components*/
    if (array_key_exists('move_retention', $status))
    {
        $move_retention = $status['move_retention'];
    }
    if (array_key_exists('move_retention_status', $status))
    {
        $move_retention_status = $status['move_retention_status'];
    }
    /*insufficient disk space pause components*/
    if (array_key_exists('insufficient_space_components', $status))
    {
        $insufficient_space_components = $status['insufficient_space_components'];
    }
   
    if (array_key_exists('insufficient_space_status', $status))
    {
        $insufficient_space_status = $status['insufficient_space_status'];
    }
    /*Host_type is 2 for source agents */ 
    if($host_type == 2)
    {    
        /*check pause request */
        if($pause_components =='yes')
        {
            /*PAUSE BY USER
                            Update pause state for source*/
            if($pause_components_status == 'completed')
            {
                $rs = update_pause_state($host_id,$device_name,1);
            }
            /*PAUSE BY USER failed*/
            if($pause_components_status == 'failed')
            {
				$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Activity"=>"User"),Log::BOTH);
            }
        }
    }
    /*Host_type is 1 for Target agents
            So perform required operations for target*/
    if($host_type == 1)
    {
        /*Get sourechostid & source device name to update agent log*/
        $dest_device_name = $conn_obj->sql_escape_string($device_name);
        $select_sql="SELECT 
                        sourceHostId,
                        sourceDeviceName,
                        ruleId,
                        autoResume
                    FROM
                        srcLogicalVolumeDestLogicalVolume
                    WHERE
                        destinationHostId = '$host_id'
                        AND
                        destinationDeviceName = '$dest_device_name'";
        $result_set = $conn_obj->sql_query($select_sql);
        $row_object = $conn_obj->sql_fetch_object($result_set);
        $source_hostid = $row_object->sourceHostId;
        $source_devicename = $row_object->sourceDeviceName;
        $ruleId            = $row_object->ruleId;
        $autoResume        = $row_object->autoResume;
        /*Get old & new retention log path*/
        $hostDetails = $commonfunctions_obj->getTargetDeviceDetails($ruleId);  
        # $logging_obj->my_error_handler("DEBUG","Host details in vxstub_set_pause_replication_status -->".print_r($hostDetails,TRUE)."\n");
        $log_details =$sparseretention_obj-> get_logging_details($hostDetails['destinationHostId'], $hostDetails['destinationDeviceName']);
        #$logging_obj->my_error_handler("DEBUG","log details in vxstub_set_pause_replication_status -->".print_r($log_details,TRUE)."\n");
        
        $old_retlog_path = $log_details[1]['storage_path'];
        $new_retlog_path = $log_details[1]['moveRetentionPath'];
		
        /*Get host name & ip details of source & target*/
        $source_host_name = $commonfunctions_obj->getHostName($source_hostid);
        $source_ip_address = $commonfunctions_obj->getHostIpaddress($source_hostid);
        $dest_host_name = $commonfunctions_obj->getHostName($host_id);
        $dest_ip_address = $commonfunctions_obj->getHostIpaddress($host_id);
        /*Contruct email body*/
        $pair_details = "";
        $pair_details = $pair_details."<br>Pair Details:<br>";
        $pair_details = $pair_details."Source Host    : $source_host_name <br>";
        $pair_details = $pair_details."Source IP      : $source_ip_address <br>";
        $pair_details = $pair_details."Source Volume  : $source_devicename <br>";
        $pair_details = $pair_details."Target Host    : $dest_host_name <br>";
        $pair_details = $pair_details."Target IP      : $dest_ip_address <br>";
        $pair_details = $pair_details."Target Volume  : $device_name <br>";
        /*check pause request */
        if($pause_components =='yes')
        {
            $error_source="";
            /*Mail details*/
            $error_id = "";
            $error_template_id = "";
            $summary = "";
            $message="";
            /* Update pause state for target*/
            if($pause_components_status == 'completed')
            {
                /*check if activity is move retention*/
                if($components =='affected' && $move_retention == 'yes')
                {
                   # $logging_obj->my_error_handler("DEBUG","vxstub_set_pausereplication_status  move retention \n");
                    $error_source="move_retention_log"; 
                    $trap_source = $TRAP_MOVE_RETENTION_LOG; 
                    $error_id = "MOVE_RETENTION_LOG_WARNING";
                    $error_template_id = "MOVE_RETLOG";
                    $summary = "move retention log warning";
                    if($move_retention_status =='requested')
                    {

                    #     $logging_obj->my_error_handler("DEBUG","vxstub_set_pausereplication_status  move retention requested \n");
                        /*Move pair from pause pending to paused state for move retention activity*/
                        if(array_key_exists('move_retention_message', $status))
                        {
                            $move_retention_message = $status['move_retention_message'];
                        }
                        /*update pause state for move retention activity
                                                      The pair would move from pause pending to pause by maintenance activity*/
                        $rs = update_pause_state($host_id,$device_name,2);
                        if($rs)
                        {
                            /*add mail to be sent*/ 
                            $message = "The following pair has been paused for maintenance as retention logs are being moved from $old_retlog_path to $new_retlog_path .The pair would resume once retention logs are successfully moved to different location.";
                            $message = $message." ".$pair_details;
                        }
                        
                    }
                    /*Move retention completed successfully*/
                    else if($move_retention_status =='completed')
                    {
                     #   $logging_obj->my_error_handler("DEBUG","vxstub_set_pausereplication_status  move retention completed \n");
                        /*Move pair from paused state to automatic resume*/
                        if(array_key_exists('move_retention_message', $status))
                        {
                            $move_retention_message = $status['move_retention_message'];
                        }
                        /*update new location for retention logs as pair is resumed successfully*/
                        $ret = update_new_retention_path($host_id,$device_name);
                        if($ret)
                        {
                            /*Resume pair as maintenance activity is done*/
                            $state = update_pause_state($host_id,$device_name,0);
                            if($state)
                            {
                                /*Add mail for move retention logs*/
                                if(!$autoResume)
                                {
                                    $message = "The retention maintenance activity is completed successfully.";
                                }
                                else
                                {
                                    $message = "The following pair is resumed as retention maintenance activity is completed successfully.";
                                }
                                $message = $message." ".$pair_details;
                            }
                        }
                    }
                    else 
                    {   
                        /*As move retention activity failed now resume the pair with out moving retention path*/
                        if(array_key_exists('move_retention_message', $status))
                        {
                            $move_retention_message = $status['move_retention_message'];
                        }
                        /*resume replication pair*/
                        $state = update_pause_state($host_id,$device_name,0);                        
                        /*Add mail to be sent*/
                        if($state)
                        {
                            $message = "We would resume the pair despite of the retention logs are not being moved due to following reason.$move_retention_message<br>";
                            $message = $message." ".$pair_details;
                        }
                    }
                }
                else if($components =='affected' && $insufficient_space_components == 'yes')
                { 
                    $error_source="insufficient_retention_space"; 
                    $trap_source = $TRAP_INSUFFICIENT_RETENTION_SPACE;
                    $error_id = "INSUFFICIENT_RETENTION_SPACE_WARNING";
                    $error_template_id = "INSUFFICIENT_RET_SPACE";
                    $summary = "insufficient retention space warning";
                    $message="";
                    if(array_key_exists('insufficient_space_message', $status))
                    {
                        $insufficient_space_message = $status['insufficient_space_message'];
                    }
                    if($insufficient_space_status == 'requested')
                    {   
                        $state = update_pause_state($host_id,$device_name,2);
                        if($state)
                        {
                            $message = "The Pair moved to pause state due to insufficient disk space in retention volume ".$old_retlog_path."<br>";
                            $message = $message." ".$pair_details;
                            $error_code = "EC0121";
                            $arr_error_placeholders = array();
                            $arr_error_placeholders['VolumeName'] = $old_retlog_path;
                            $arr_error_placeholders['SrcHostName'] = $source_host_name;
                            $arr_error_placeholders['SrcIPAddress'] = $source_ip_address;
                            $arr_error_placeholders['SrcVolume'] = $source_devicename;
                            $arr_error_placeholders['DestHostName'] = $dest_host_name;
                            $arr_error_placeholders['DestIPAddress'] = $dest_ip_address;
                            $arr_error_placeholders['DestVolume'] = $device_name;
                            $error_placeholders = serialize($arr_error_placeholders);
							                   
                            if($cx_os_flag == 1)
							{
								$trap_info_str = " -trapvar ".$TRAP_SEVERITY."::s::Major";
								$trap_info_str .= " -trapvar ".$TRAP_CS_HOSTNAME."::s::\"".$cx_hostname."\"";
								$trap_info_str .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."::s::\"".$source_host_name."\"";
								$trap_info_str .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."::s::\"".$source_devicename."\"";
								$trap_info_str .= " -trapvar ".$TRAP_TARGET_HOSTNAME."::s::\"".$dest_host_name."\"";
								$trap_info_str .= " -trapvar ".$TRAP_TARGET_DEVICENAME."::s::\"".$device_name."\"";
								$trap_info_str .= " -trapvar ".$TRAP_RETENTION_DEVICENAME."::s::\"".$old_retlog_path."\"";
							}
							else
							{
								$trap_info_str = "Abhai-1-MIB-V1::trapSeverity s Major ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapCSHostname s \"$cx_hostname\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapSourceHostName s \"$source_host_name\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapSourceDeviceName s \"$source_devicename\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapTargetHostName s \"$dest_host_name\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapTargetDeviceName s \"$device_name\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapRetentionDeviceName s \"$old_retlog_path\" ";
							}
                        }
                    }
                }
                else
                {
                    /*Update pause state for pause by user target*/
                    update_pause_state($host_id,$device_name,2);
                }
            }
            else if($pause_components_status == 'requested')
            {
                if($components =='affected' && $insufficient_space_components == 'yes')
                {
                    $error_source="insufficient_retention_space"; 
                    $trap_source = $TRAP_INSUFFICIENT_RETENTION_SPACE;
                    $error_id = "INSUFFICIENT_RETENTION_SPACE_WARNING";
                    $error_template_id = "INSUFFICIENT_RET_SPACE";
                    $summary = "insufficient retention space warning";
                    $message="";
                    if(array_key_exists('insufficient_space_message', $status))
                    {
                        $insufficient_space_message = $status['insufficient_space_message'];
                    }
                    if($insufficient_space_status == 'requested')
                    {
                        $ret = update_pause_pending_state($host_id,$device_name,2);
                        if($ret)
                        {
                            $message = "The Pair moved to paused pending state as $insufficient_space_message<br>";
                            $message = $message." ".$pair_details;
                            
							if($cx_os_flag == 1)
							{
								$trap_info_str = " -trapvar ".$TRAP_SEVERITY."::s::Major";
								$trap_info_str .= " -trapvar ".$TRAP_CS_HOSTNAME."::s::\"".$cx_hostname."\"";
								$trap_info_str .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."::s::\"".$source_host_name."\"";
								$trap_info_str .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."::s::\"".$source_devicename."\"";
								$trap_info_str .= " -trapvar ".$TRAP_TARGET_HOSTNAME."::s::\"".$dest_host_name."\"";
								$trap_info_str .= " -trapvar ".$TRAP_TARGET_DEVICENAME."::s::\"".$device_name."\"";
								$trap_info_str .= " -trapvar ".$TRAP_RETENTION_DEVICENAME."::s::\"".$old_retlog_path."\"";
							}
							else
							{
								$trap_info_str = "Abhai-1-MIB-V1::trapSeverity s Major ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapCSHostname s \"$cx_hostname\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapSourceHostName s \"$source_host_name\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapSourceDeviceName s \"$source_devicename\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapTargetHostName s \"$dest_host_name\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapTargetDeviceName s \"$device_name\" ";
								$trap_info_str .= "Abhai-1-MIB-V1::trapRetentionDeviceName s \"$old_retlog_path\" ";
							}
                        }
                    }
                }
            }
            else
            {
                /*Pause pending acknowlegement failed*/
                /*need to disscuss*/
				$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Activity"=>"User"),Log::BOTH);
            }
            if($message !="")
            {
                $trap_cmdline = '';
				if($cx_os_flag == 1)
				{
					$trap_cmdline .= " -traptype ".$trap_source;
					$trap_cmdline .= " -trapvar ".$TRAP_AMETHYSTGUID."::s::\"".$cx_hostid."\"";
					$trap_cmdline .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."::s::\"".$message."\"";
				}
				else
				{
					$trap_cmdline   .= "Abhai-1-MIB-V1::$trap_source ";
					$trap_cmdline   .= "Abhai-1-MIB-V1::trapAmethystGuid ";
					$trap_cmdline   .= "s \"$cx_hostid\" ";
					$trap_cmdline   .= "Abhai-1-MIB-V1::$TRAP_AGENT_ERROR_MESSAGE ";
					$trap_cmdline   .= "s \"$message \" ";
				}				
				$trap_cmdline   .= $trap_info_str;
				$commonfunctions_obj->send_trap($trap_cmdline, $error_template_id);
                
                /*update pause maintenance activity to agent logs*/
                update_agent_log ($source_hostid, $source_devicename,$message,0,$error_source);
                $commonfunctions_obj->add_error_message($error_id, $error_template_id, $summary, $message, $source_hostid, '', $error_code, $error_placeholders);
            }
            
        }
     }
	 $conn_obj->sql_commit();
	}
	catch(SQLException $e)
	{
		$conn_obj->sql_rollback();
		$result = FALSE;
		$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>$result),Log::BOTH);
	}
	$conn_obj->disable_exceptions();
	if (!$GLOBALS['http_header_500'])
		$logging_obj->my_error_handler("INFO",array("Result"=>$result,"Status"=>"Success"),Log::BOTH);
	else
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
	 return $result;
}

function vxstub_update_replication_cleanup_status($obj,$device_name,$str)
{
	global $conn_obj;
	global $logging_obj;
	global $CACHE_CLEANUP_COMPLETED,$CACHE_CLEANUP_FAILED;
	global $PENDING_FILES_CLEANUP_COMPLETED,$PENDING_FILES_CLEANUP_FAILED;
	global $VNSAP_CLEANUP_COMPLETED,$VNSAP_CLEANUP_FAILED;
	global $LOG_CLEANUP_COMPLETE,$LOG_CLEANUP_FAILED;
	global $UNLOCK_COMPLETE,$UNLOCK_FAILED;
	global $RELEASE_DRIVE_ON_TARGET_DELETE_DONE;
	global $RELEASE_DRIVE_ON_TARGET_DELETE_FAIL;
	global $TARGET_DATA, $commonfunctions_obj;
	$dest_id = $obj['host_id'];
	$dest_device_name = $device_name;
	$valid_protection = $commonfunctions_obj->isValidProtection($dest_id, $dest_device_name, $TARGET_DATA);
	if (!$valid_protection)
	{
		$logging_obj->my_error_handler("INFO","Invalid post data $dest_device_name in method vxstub_update_replication_cleanup_status.",Log::BOTH);
		//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
		return TRUE;
	}
	$logging_obj->my_error_handler("INFO",array("HostId"=>$dest_id,"DestinationDeviceName"=>$dest_device_name,"String"=>$str),Log::BOTH);
	$conn_obj->enable_exceptions();
	try
	{
		$conn_obj->sql_begin();
		$device_name = $conn_obj->sql_escape_string($device_name);
		$cleanup_others = "10100000";
		$str_status = explode(";",$str);
		$status = array();
		$message = array();
		$chk_status = "_status";
		$chk_message = "_message";
		for($temp = 0; $temp < count($str_status); $temp++)
		{
			$result_arr = explode("=",trim($str_status[$temp]));
			$key = $result_arr[0];
			$value = $result_arr[1];
			if((strpos($key,$chk_status))!== false)
			{
				$value = intval($value);
				if($value!=0)
				{
					$status[] = $value;
				}
			}
			if((strpos($key,$chk_message))!== false)
			{
				$msg = $value;
				if($msg)
				{
					$sql = "SELECT
								sourceHostId,
								sourceDeviceName
							FROM
								srcLogicalVolumeDestLogicalVolume
							WHERE
								destinationHostId = '$dest_id' AND 
								destinationDeviceName='$device_name'";
					$logging_obj->my_error_handler("DEBUG","vxstub_update_replication_cleanup_status::$sql \n");			
					$rs = $conn_obj->sql_query($sql);
					while($row = $conn_obj->sql_fetch_row($rs))
					{
						update_agent_log($row[0],$row[1],$value);
					}
				}
			}
		}
		$logging_obj->my_error_handler("INFO",array("StringStatus"=>$status),Log::BOTH);
		if(count($status) > 0)
		{
			for($temp = 0; $temp < count($status); $temp++)
			{
				if($status[$temp] == $CACHE_CLEANUP_COMPLETED || $status[$temp] == $CACHE_CLEANUP_FAILED)
				{
					$cache_cleanup = "01";
					$update_sql = "UPDATE
									srcLogicalVolumeDestLogicalVolume
								   SET 
									replicationCleanupOptions = INSERT(replicationCleanupOptions,1,2,'$cache_cleanup')
								   WHERE 
									destinationHostId = '$dest_id' AND 
									destinationDeviceName='$device_name'";
					$logging_obj->my_error_handler("DEBUG","vxstub_update_replication_cleanup_status::CACHE_CLEANUP_COMPLETED,$update_sql \n");				
					$result = $conn_obj->sql_query($update_sql);
				}
				
				if($status[$temp] == $PENDING_FILES_CLEANUP_COMPLETED || $status[$temp] == $PENDING_FILES_CLEANUP_FAILED)
				{
					$pending_files_cleanup = ($status[$temp] == $PENDING_FILES_CLEANUP_COMPLETED) ? "01" : "02";
					$update_sql = "UPDATE
									srcLogicalVolumeDestLogicalVolume
								   SET 
									replicationCleanupOptions = INSERT(replicationCleanupOptions,3,2,'$pending_files_cleanup')
								   WHERE 
									destinationHostId = '$dest_id' AND 
									destinationDeviceName='$device_name'";
					$logging_obj->my_error_handler("DEBUG","vxstub_update_replication_cleanup_status::PENDING_FILES_CLEANUP_COMPLETED,$update_sql \n");					
					$result = $conn_obj->sql_query($update_sql);
				}
				
				if($status[$temp] == $VNSAP_CLEANUP_COMPLETED || $status[$temp] == $VNSAP_CLEANUP_FAILED)
				{
					$vsnap_cleanup = ($status[$temp] == $VNSAP_CLEANUP_COMPLETED) ? "01" : "02";
					$update_sql = "UPDATE
									srcLogicalVolumeDestLogicalVolume
								   SET 
									replicationCleanupOptions = INSERT(replicationCleanupOptions,5,2,'$vsnap_cleanup')
								   WHERE 
									destinationHostId = '$dest_id' AND 
									destinationDeviceName='$device_name'";
					$logging_obj->my_error_handler("DEBUG","vxstub_update_replication_cleanup_status::VNSAP_CLEANUP_COMPLETED,$update_sql \n");					
					$result = $conn_obj->sql_query($update_sql);
				}
				
				if($status[$temp] == $LOG_CLEANUP_COMPLETE || $status[$temp] == $LOG_CLEANUP_FAILED)
				{
					$ret_log_cleanup = ($status[$temp] == $LOG_CLEANUP_COMPLETE) ? "01" : "02";
					$update_sql = "UPDATE
									srcLogicalVolumeDestLogicalVolume
								   SET 
									replicationCleanupOptions = INSERT(replicationCleanupOptions,7,2,'$ret_log_cleanup')
								   WHERE 
									destinationHostId = '$dest_id' AND 
									destinationDeviceName='$device_name'";
					$logging_obj->my_error_handler("DEBUG","vxstub_update_replication_cleanup_status::LOG_CLEANUP_COMPLETE,$update_sql \n");					
					$result = $conn_obj->sql_query($update_sql);
				
				}
				
				if($status[$temp] == $UNLOCK_COMPLETE || $status[$temp] == $UNLOCK_FAILED)
				{
					$unlock_drive = ($status[$temp] == $UNLOCK_COMPLETE) ? "01" : "02";
					$update_sql = "UPDATE
									srcLogicalVolumeDestLogicalVolume
								   SET 
									replicationCleanupOptions = INSERT(replicationCleanupOptions,9,2,'$unlock_drive')
								   WHERE 
									destinationHostId = '$dest_id' AND 
									destinationDeviceName='$device_name'";
					$logging_obj->my_error_handler("DEBUG","vxstub_update_replication_cleanup_status::UNLOCK_COMPLETE,$update_sql \n");					
					$result = $conn_obj->sql_query($update_sql);
				}
			}
		}
		$flag = check_replication_cleanup_status($dest_id,$dest_device_name,$chk_rep_options);
		$logging_obj->my_error_handler("DEBUG","vxstub_update_replication_cleanup_status::flag:$flag \n");					
		if($flag == 2)
		{
			$pairState = $RELEASE_DRIVE_ON_TARGET_DELETE_DONE;
			update_replication_state_status($obj,$dest_device_name,$pairState);
		}
		if($flag == 1)
		{
			$pairState = $RELEASE_DRIVE_ON_TARGET_DELETE_FAIL;
			update_replication_state_status($obj,$dest_device_name,$pairState);
		}
		$conn_obj->sql_commit();
		$conn_obj->disable_exceptions();
	}
	catch(SQLException $e)
	{
		$conn_obj->sql_rollback();
		$conn_obj->disable_exceptions();
		$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>FALSE),Log::BOTH);
		return FALSE;
	}
	$logging_obj->my_error_handler("DEBUG","vxstub_update_replication_cleanup_status::return value  :true \n");	
	
	if (!$GLOBALS['http_header_500'])
		$logging_obj->my_error_handler("INFO",array("Result"=>TRUE,"Status"=>"Success"),Log::BOTH);
	else
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
	return TRUE;
}

function get_source_volumes( $obj, $final_hash=array()) 
{
    global $REPLICATION_ROOT, $ORIGINAL_REPLICATION_ROOT;
    global  $conn_obj;
	global $PROTECTED,$REBOOT_PENDING;
	global 	$logging_obj;
	global $PAUSED_PENDING,$PAUSE_COMPLETED,$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;
	global $FLUSH_AND_HOLD_WRITES_PENDING,$FLUSH_AND_HOLD_WRITES,$FLUSH_AND_HOLD_RESUME_PENDING;
	global $RESTART_RESYNC_CLEANUP;

	$commonfunctions_obj = new CommonFunctions();
	global $batch_query;
	
	$result = array();
	
    $targets = array();
	$pause_state = array();
	$rpo_array = array();
	$cmp_array = array();
	
	$pair_array = $final_hash[0];
	$lv_array   = $final_hash[1]; 
	$hst_array   = $final_hash[2];
	$sd_array   = $final_hash[3];
	$hatlm_array   = $final_hash[4];
	$retention_array   = $final_hash[7];
	$ps_array   = $final_hash[5];
	$one_to_many   = $final_hash[8]; 
	$license_array   = $final_hash[9]; 
	$pair_type_array   = $final_hash[11];
	
	$cnt =0;
	
    foreach($pair_array as $pair_id => $source_info)
    {
        /*source & target replication state values*/
	
		if($source_info['sourceHostId'] == $obj['host_id'])
		{
			$src_replication_state = 0;	  
			$tar_replication_state = 0;
			$src_lun_id = $source_info['Phy_Lunid'];
			$lun_state = $source_info['lun_state'];
			$sourceDevice = $source_info['sourceDeviceName'];
			$target_data_plane = $source_info['TargetDataPlane'];
					
			$src_lv_data = $lv_array[$source_info['sourceHostId']."!!".$source_info['sourceDeviceName']];
			$tgt_lv_data = $lv_array[$source_info['destinationHostId']."!!".$source_info['destinationDeviceName']];        
			$src_host_data = $hst_array[$source_info['sourceHostId']];
			$tgt_host_data = $hst_array[$source_info['destinationHostId']];		
			
			$license_expired_source = $license_array[$source_info['sourceHostId']]['expired'];
			$license_expired_target = $license_array[$source_info['destinationHostId']]['expired'];
			
			$pair_key = $source_info['sourceHostId']."!!".$source_info['sourceDeviceName']."!!".$source_info['Phy_Lunid'];
			$throttle_diff = $source_info['throttleDifferentials'];
			if(array_key_exists($pair_key, $one_to_many))
			{
				if($one_to_many[$pair_key]['throttleDifferentials'])
				$throttle_diff = $one_to_many[$pair_key]['throttleDifferentials'];
				$rpoThreshold = $one_to_many[$pair_key]['rpoSLAThreshold'];		
				$min_target_compatibility = $one_to_many[$pair_key]['compatibilityNo'];
			}
			##updating sentinel time in srcLogicalVolumeDestLogicalVolume and logicalVolumes.
			$current_time = time ();
			/*$lv_sql = "update logicalVolumes set lastSentinelChange=now(), lastDeviceUpdateTime=$current_time where hostId = '".$source_info['sourceHostId']."' and deviceName = '".$conn_obj->sql_escape_string($source_info['sourceDeviceName'])."'";
			$batch_query['update'][] = $lv_sql;
			$pair_sql = "update srcLogicalVolumeDestLogicalVolume set lastSentinelChange = now() where sourceHostId = '".$source_info['sourceHostId']."'";
			$batch_query['update'][] = $pair_sql;*/		

			$readwritemode = $src_lv_data['visible'] ? $src_lv_data['readwritemode'] : 0;
		
			$targethostname      =  $tgt_host_data['name'];
			$tarCompatibilityNum = $tgt_host_data['compatibilityNo'];
			$tarOsFlag           = (int)$tgt_host_data['osFlag']; //  OSFlag 0-UNKNOWN 1-WINDOWS 2-LINUX

			$hostname      =  $src_host_data['name'];
			$srcCompatibilityNum = $src_host_data['compatibilityNo'];
			$srcOsFlag           = (int)$src_host_data['osFlag'];			
			
			## source volume info
			$srctransProtocol = $src_lv_data['transProtocol'];
			$srcMountPoint    = $src_lv_data['mountPoint'];
			$srcfileSystem    = $src_lv_data['fileSystemType'];
			$srcRawSize       = $src_lv_data['rawSize'];
			$sourcevolumeType = (int)$src_lv_data['volumeType'];
			
			## target volume info
			$tartransProtocol = $tgt_lv_data['transProtocol'];
			$tarcapacity      = $tgt_lv_data['capacity'];
			$tarMountPoint    = $tgt_lv_data['mountPoint'];
			$tarfileSystem    = $tgt_lv_data['fileSystemType'];
			$tgtRawSize       = $tgt_lv_data['rawSize'];
			$targetvolumeType = (int)$tgt_lv_data['volumeType'];	
			$tgt_lun_id 	  = $tgt_lv_data['Phy_Lunid'];
			$pairType = 0;
			if($source_info['planId'] != 1)
			{
				if((isset($src_lun_id) && $src_lun_id !='') || (isset($tgt_lun_id) && $tgt_lun_id != ''))
				{
					$pairType = $pair_type_array[$source_info['planId']];
				}
			}
			if($source_info['Phy_Lunid'])
			{			
				$applianceTargetLunMappingId = $hatlm_array[$source_info['Phy_Lunid']]['applianceTargetLunMappingId'];
			}		

			$startingPhysicalOffset = "0";	
			if($pairType == 2) // pair type is prism
			{
				$prism_obj = new PrismConfigurator();
				$lunCapacity = $prism_obj->get_lun_size($source_info['Phy_Lunid']);
				$srccapacity = $tarcapacity = $lunCapacity;
				
				$sync_device = $prism_obj->get_sync_device($source_info['Phy_Lunid']);
				if($sync_device)
				{
					$option = 1; // this option for sending request from AT to get physical offset.
					$startingPhysicalOffset = $prism_obj->get_volume_level_info($obj['host_id'],$source_info['Phy_Lunid'],$sync_device,$option);
				}
			}
				

			$atLunStatsRequest = array(0, intval(0), "", array());
			
			if($srcCompatibilityNum >= 630050)
			{
				$volume_settings['resync_copy_mode'] = $source_info['sync_copy'];
				$end_point_settings['resync_copy_mode'] = $source_info['sync_copy'];
			}
			
			if($srcCompatibilityNum >= 710000)
			{
				$protection_plan_obj = new ProtectionPlan();
				$end_point_settings['rawsize'] = (string)$tgtRawSize;
				$volume_settings['rawsize'] = (string)$srcRawSize;
				$scenarioId = $source_info['scenarioId'];
				$end_point_settings['protection_direction'] = "forward";
				$volume_settings['protection_direction'] = "forward";
				if($scenarioId)
				{
					$protectionDirection = $protection_plan_obj->getProtectionDirection($scenarioId);
					if($protectionDirection)
					{
						$end_point_settings['protection_direction'] = $protectionDirection;			
						$volume_settings['protection_direction'] = $protectionDirection;
					}	
				}
			}
			
            if ($target_data_plane == 0)
			{
				$end_point_settings['target_data_plane'] = 'UNSUPPORTED_DATA_PLANE';
                $volume_settings['target_data_plane'] = 'UNSUPPORTED_DATA_PLANE';
			}
			elseif ($target_data_plane == 1)
			{
				$end_point_settings['target_data_plane'] = 'INMAGE_DATA_PLANE';
                $volume_settings['target_data_plane'] = 'INMAGE_DATA_PLANE';
			}
			elseif ($target_data_plane == 2)
			{
				$end_point_settings['target_data_plane'] = 'AZURE_DATA_PLANE';
                $volume_settings['target_data_plane'] = 'AZURE_DATA_PLANE';
			}
				
						
			$compressionEnable = (int) $source_info['compressionEnable'];
			
			if($source_info['isQuasiflag'] != 2)
			{
				$resyncRequiredFlag = false;
			}
			else
			{
			   if($source_info['shouldResync'] == 1 or $source_info['shouldResync'] == 8 or $source_info['shouldResync'] == 2)
				{
					$resyncRequiredFlag = true;
				}
				else
				{
					$resyncRequiredFlag = false;
				}
			}
			$srcResyncStarttime = $source_info['resyncStartTagtime'];
			$srcResyncEndtime   =$source_info['resyncEndTagtime'];      
			$resyncRequiredCause =(int) $source_info['ShouldResyncSetFrom'];
			$resyncRequiredTimestamp = $source_info['resyncSettimestamp'];
			$job_id = $source_info['jobId'];
			$resyncStartTagtimeSequence = $source_info['resyncStartTagtimeSequence'];
			$resyncEndTagtimeSequence = $source_info['resyncEndTagtimeSequence'];
			$resync_counter = $source_info['resyncCounter'];		
		
			/*get the pause related flags*/
			$replication_cleanup_trg = get_rep_cleanup_info($tgt_lv_data['fileSystemType'],$tgt_lv_data['mountPoint'],$source_info['replicationCleanupOptions'],1);
			$replication_cleanup_src = get_rep_cleanup_info('','',$source_info['replicationCleanupOptions'],0);
			$srccapacity = $source_info['srcCapacity'];
			/*get the source pause information*/
			$pause_pending_maintenance_src = get_pause_pending_maintenance_src_info($source_info['replication_status'],$source_info['pauseActivity'],$source_info['src_replication_status']);

			/*get the Target pause information*/
			$pause_pending_maintenance_trg = get_pause_pending_maintenance_trg_info($source_info['tar_replication_status'],$source_info['pauseActivity'],$source_info['ruleId'],$source_info['destinationDeviceName'], $source_info['destinationHostId'], $tgt_host_data['osFlag'], $retention_array,$source_info['replication_status']);   


		   $apply_rate_trg = "$source_info[differentialApplyRate]";
		   $pending_diffs_cx_trg = "$source_info[remainingDifferentialBytes]";
		   
			$currentRpotime = $source_info['rpotime'];
			$currentTime = $source_info['statusupdatetime'];
			$currentRPO_trg = "0";
			if($currentRpotime == 0)
			{
				$currentRPO_trg = "0";
			}
			else
			{
				$rpo = ($currentTime - $currentRpotime);
				$currentRPO_trg = ($rpo < 0) ?"0":"$rpo";				
			}

		   $apply_rate_src = "0";
		   $pending_diffs_cx_src = "0";
		   $currentRPO_src = "0";
		   if ($source_info['replication_status'] == $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING)
		   {            
				$tar_replication_state = $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;			
		   }
		   if ($source_info['replication_status'] == $RESTART_RESYNC_CLEANUP)
		   {
				$tar_replication_state = $RESTART_RESYNC_CLEANUP;			
		   }
		   if(($source_info['replication_status'] == $PAUSED_PENDING) ||($source_info['replication_status'] == $PAUSE_COMPLETED))
		   {
				/*check if source & target components are paused*/
				$is_src_paused = intval($source_info['src_replication_status']);
				$is_tar_paused = intval($source_info['tar_replication_status']);
				/*check if source & target components are paused Here "OR" condition is for backward compatibility in case of upgrade */
				if(($is_src_paused == 1) || ($is_src_paused == $PAUSE_COMPLETED))
				{
					$src_replication_state = intval($source_info['replication_status']);
				}
				if(($is_tar_paused == 1) || ($is_tar_paused == $PAUSE_COMPLETED))
				{
					$tar_replication_state = intval($source_info['replication_status']);    
				}
		   }
		   if(($source_info['replication_status'] == $FLUSH_AND_HOLD_WRITES_PENDING) ||($source_info['replication_status'] == $FLUSH_AND_HOLD_WRITES) || ($source_info['replication_status'] == $FLUSH_AND_HOLD_RESUME_PENDING))
			{
				$tar_replication_state = intval($source_info['replication_status']);    
			}
			
			#New fast sync holds the mode as 6 for step1 transition
			#0-- SYNC_DIFF, 1--SYNC_OFFLOAD, 2--SYNC_FAST, 3--SYNC_SNAPSHOT, 4--SYNC_DIFF_SNAPSHOT, 5--SYNC_QUASIDIFF, 6 -- NEW FAST RESYNC VERSION
			#7--Local copy
			$syncMode =  ($source_info['isQuasiflag'] == 0) ? ($source_info['resyncVersion']==0 ? 1 : ($source_info['resyncVersion']==2 ? 6 : ($source_info['resyncVersion']==3 ? 7 : ($source_info['resyncVersion']==4 ? 7 : 2)))) : (($source_info['isQuasiflag'] == 1)  ? 5 : 0 );
			
			$san_volume_info = get_san_volume_info_settings($source_info['Phy_Lunid'], $hatlm_array, $source_info['sourceDeviceName'], $applianceTargetLunMappingId);
			$tarCompatibilityNum = $min_target_compatibility;
			
			$targets = array();

			## added to support Windows mountPoint Support
			if($src_host_data['osFlag'] == 3)
			{
				$device_path = $source_info[1];
			}
			else
			{
				$device_path = str_replace(":", "", $source_info['sourceDeviceName']);
			}
			$device_path = str_replace("\\", "/", $device_path);
			$resyncDirectory = $ORIGINAL_REPLICATION_ROOT."/".$obj['host_id']."/$device_path/resync"; // for Source

			if( $tgt_host_data['osFlag'] == 3)
			{
				$device =  $source_info['destinationDeviceName'];
			}
			else
			{
				$device = str_replace(":", "", $source_info['destinationDeviceName']);
			}

			$device = str_replace("\\", "/", $device);
			$resyncDirectory_target = $ORIGINAL_REPLICATION_ROOT."/".$source_info['destinationHostId']."/$device/resync"; // for Target
		
			 
			$targets []= array( 0,$source_info['destinationDeviceName'],$tarMountPoint,$tarfileSystem, $targethostname /* to do: hostname */, $source_info['destinationHostId'], intval($source_info['ftpsDestSecureMode']),intval($source_info['ftpsSourceSecureMode']),$syncMode,intval($tartransProtocol),(int)$readwritemode,$tarcapacity, $resyncDirectory,$rpoThreshold,$srcResyncStarttime,$srcResyncEndtime,$srcCompatibilityNum,$resyncRequiredFlag,$resyncRequiredCause,$resyncRequiredTimestamp,$tarOsFlag,$compressionEnable);

			$result []= array( $source_info['sourceDeviceName'],$srcMountPoint,$srcfileSystem,$hostname /* hostname */, $obj['host_id'],intval($source_info['ftpsDestSecureMode']),intval($source_info['ftpsSourceSecureMode']), $syncMode,intval($srctransProtocol),(int)$readwritemode,$srccapacity, $resyncDirectory_target, $rpoThreshold,$srcResyncStarttime,$srcResyncEndtime,$tarCompatibilityNum,$resyncRequiredFlag,$resyncRequiredCause,$resyncRequiredTimestamp,$srcOsFlag,$compressionEnable);
			
			array_push($targets[count($targets)-1], $job_id);
			array_push($result[count($result)-1], $job_id);
			array_push($targets[count($targets)-1], $san_volume_info);
			array_push($result[count($result)-1], $san_volume_info);
			array_push($targets[count($targets)-1], array());
			$throttle_settings = array();
			$ps_throttle = $ps_array[$source_info['processServerId']]['cummulativeThrottle'];
			$throttle_settings = vxstub_should_throttle_fast( $source_info['resyncVersion'], $source_info['throttleresync'], $throttle_diff, $lun_state, $ps_throttle , $license_expired_source, $license_expired_target,$source_info['sourceDeviceName']); 
			
			if($srcCompatibilityNum >= 630050)
			{
				array_push($targets[count($targets)-1], $tar_replication_state,$replication_cleanup_trg,$pending_diffs_cx_trg,$currentRPO_trg,$apply_rate_trg,$pause_pending_maintenance_trg,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[1],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$targetvolumeType,$end_point_settings);
			}
			else if($srcCompatibilityNum >= 560000)
			{
				array_push($targets[count($targets)-1], $tar_replication_state,$replication_cleanup_trg,$pending_diffs_cx_trg,$currentRPO_trg,$apply_rate_trg,$pause_pending_maintenance_trg,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[1],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$targetvolumeType);
			}
			else if($srcCompatibilityNum >= 550000) ##### 9448 ######
			{
				#pair resyncCounter is part of get_initial_settings
				array_push($targets[count($targets)-1], $tar_replication_state,$replication_cleanup_trg,$pending_diffs_cx_trg,$currentRPO_trg,$apply_rate_trg,$pause_pending_maintenance_trg,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[1],$resync_counter,$srcRawSize);
			}
			
			//Mark Active Node's Src volumes as PAUSED_PENDING while sending their settings to Passive Node				
			if ($source_info['sourceHostId'] != $obj['host_id'])
			{	
				$src_replication_state = $PAUSED_PENDING;
			}
			
			if ($src_lun_id != NULL && $lun_state != $PROTECTED && $src_replication_state == 0)
			{
				$src_replication_state = $PAUSED_PENDING;
			}
			if($srcCompatibilityNum >= 630050)
			{
				array_push($result[count($result)-1], $targets, $src_replication_state,$replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$sourcevolumeType,$volume_settings,$source_info['volumeGroupId']);
			}
			else if($srcCompatibilityNum >= 560000)
			{
				#pair resyncCounter is part of get_initial_settings
				array_push($result[count($result)-1], $targets, $src_replication_state,$replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$sourcevolumeType,$source_info['volumeGroupId']);
			}
			else if($srcCompatibilityNum >= 550000) ###### 9448 ######
			{
				#pair resyncCounter is part of get_initial_settings
				array_push($result[count($result)-1], $targets, $src_replication_state,$replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize,$source_info['volumeGroupId']);
			} 
		$cnt++;
		}
				
    }

    return $result;
}

function get_target_volumes( $obj, $final_hash=array())
{
    global $REPLICATION_ROOT, $ORIGINAL_REPLICATION_ROOT,$PROTECTED,$REBOOT_PENDING ;
    global $conn_obj;
	global 	$logging_obj;
	global $PAUSED_PENDING,$PAUSE_COMPLETED,$DELETE_PENDING, $RELEASE_DRIVE_ON_DELETE_PENDING, $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;
    global $SOURCE_DELETE_DONE,$PS_DELETE_DONE;
	global $FLUSH_AND_HOLD_WRITES_PENDING,$FLUSH_AND_HOLD_WRITES,$FLUSH_AND_HOLD_RESUME_PENDING;
    global $RESTART_RESYNC_CLEANUP;
	global $batch_query;
	$volume_obj = new VolumeProtection();
	$commonfunctions_obj = new CommonFunctions();
	$BatchSqls_obj = new BatchSqls();
	
	$result = array();
    $lastDevice = NULL;
	
	$pair_array = $final_hash[0];
	$lv_array   = $final_hash[1]; 
	$hst_array   = $final_hash[2];
	$sd_array   = $final_hash[3]; 
	$hatlm_array   = $final_hash[4]; 
	$retention_array   = $final_hash[7];
	$ps_array   = $final_hash[5];
	$one_to_many   = $final_hash[8];
	$license_array   = $final_hash[9];
	$pair_type_array   = $final_hash[11];

	$sanVolumeInfo = array(0, FALSE, "", "", "", "",""); //initialize default value for san.
	$cnt = 0;

	foreach($pair_array as $pair_id => $target_info)
	{
        if($target_info['destinationHostId'] == $obj['host_id'])
		{
			/*source & target default replication state values*/
			$src_replication_state = 0;	  
			$tar_replication_state = 0;
			
			$src_lun_id = $target_info['Phy_Lunid'];
			$lun_state = $target_info['lun_state'];
			$target_data_plane = $target_info['TargetDataPlane'];

			
			$src_lv_data = $lv_array[$target_info['sourceHostId']."!!".$target_info['sourceDeviceName']];
			$tgt_lv_data = $lv_array[$target_info['destinationHostId']."!!".$target_info['destinationDeviceName']]; 
			
			$src_host_data = $hst_array[$target_info['sourceHostId']];
			$tgt_host_data = $hst_array[$target_info['destinationHostId']];
			
			$license_expired_source = $license_array[$target_info['sourceHostId']]['expired'];
			$license_expired_target = $license_array[$target_info['destinationHostId']]['expired'];
			
			$replication_cleanup_trg = get_rep_cleanup_info($tgt_lv_data['fileSystemType'],$tgt_lv_data['mountPoint'],$target_info['replicationCleanupOptions'],1);
			$replication_cleanup_src = get_rep_cleanup_info('','',$target_info['replicationCleanupOptions'],0);
			
			$key = $target_info['sourceHostId']."!!".$target_info['sourceDeviceName']."!!".$target_info['Phy_Lunid'];
			
			$throttle_diff = $target_info['throttleDifferentials'];
			if(array_key_exists($key, $one_to_many))
			{
				#if($key['throttleDifferentials'])
				#$throttle_diff = $one_to_many[$key]['throttleDifferentials'];
				$rpoThreshold = $one_to_many[$key]['rpoSLAThreshold'];				
			}			
			
			$apply_rate_trg = "$target_info[differentialApplyRate]";
			$pending_diffs_cx_trg = "$target_info[remainingDifferentialBytes]";
			$currentRpotime = $target_info['rpotime'];
			$currentTime = $target_info['statusupdatetime'];
			$currentRPO_trg = "0";
			if($currentRpotime == 0)
			{
				$currentRPO_trg = "0";
			}
			else
			{
				$rpo = ($currentTime - $currentRpotime);
				if ($rpo < 0) 
				{
					$currentRPO_trg = "0";
				}
			}
			$apply_rate_src = "0";
			$pending_diffs_cx_src = "0";
			$currentRPO_src = "0";
			
			$tmp_host_id = $obj['host_id'];
			$rep_state = $target_info['replication_status'];
			$logging_obj->my_error_handler("DEBUG","HOSTID::$tmp_host_id REPLICATION STATUS::$rep_state");
			
			if ($target_info['replication_status'] == $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING)
			{
				$tar_replication_state = $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;			
			}
			if ($target_info['replication_status'] == $RESTART_RESYNC_CLEANUP)
			{
				$tar_replication_state = $RESTART_RESYNC_CLEANUP;			
			}
			if(($target_info['replication_status'] == $PAUSED_PENDING) ||($target_info['replication_status'] == $PAUSE_COMPLETED))
			{
				$is_src_paused = intval($target_info['src_replication_status']);
				$is_tar_paused = intval($target_info['tar_replication_status']);
				/*check if source & target components are paused Here "OR" condition is for backward compatibility in case of upgrade */                   
				if(($is_src_paused == 1) || ($is_src_paused == $PAUSE_COMPLETED))
				{
					$src_replication_state = intval($target_info['replication_status']);
				}
				if(($is_tar_paused == 1) || ($is_tar_paused == $PAUSE_COMPLETED))
				{
					$tar_replication_state = intval($target_info['replication_status']);    
				}
			}
			if(($target_info['replication_status'] == $FLUSH_AND_HOLD_WRITES_PENDING) ||($target_info['replication_status'] == $FLUSH_AND_HOLD_WRITES) || ($target_info['replication_status'] == $FLUSH_AND_HOLD_RESUME_PENDING))
			{
				$tar_replication_state = intval($target_info['replication_status']);    
			}	
		  
			##updating outpost time in srcLogicalVolumeDestLogicalVolume and logicalVolumes.
			/*$lv_sql = "update logicalVolumes set lastDeviceUpdateTime=UNIX_TIMESTAMP(), lastOutpostAgentChange=now() " .
                 "where hostId = '" . $obj['host_id'] . "' and  deviceName = '" . $conn_obj->sql_escape_string( $target_info['destinationDeviceName'] ) . "'";
			$batch_query['update'][] = $lv_sql;*/				
			
			$readwritemode = $tgt_lv_data['visible'] ? $tgt_lv_data['readwritemode'] : 0;

			## source volume info
			$srctransProtocol = $src_lv_data['transProtocol'];
			$srcMountPoint    = $src_lv_data['mountPoint'];
			$srcfileSystem    = $src_lv_data['fileSystemType'];
			$srcRawSize       = $src_lv_data['rawSize'];
			$sourcevolumeType = (int)$src_lv_data['volumeType'];
			
			## target volume info
			$tartransProtocol = $tgt_lv_data['transProtocol'];
			$tarcapacity      = $tgt_lv_data['capacity'];
			$tarMountPoint    = $tgt_lv_data['mountPoint'];
			$tarfileSystem    = $tgt_lv_data['fileSystemType'];
			$tgtRawSize       = $tgt_lv_data['rawSize'];
			$targetvolumeType = (int)$tgt_lv_data['volumeType'];
			$tgt_lun_id 	  = $tgt_lv_data['Phy_Lunid'];
			$targethostname      =  $tgt_host_data['name'];
			$tarCompatibilityNum = $tgt_host_data['compatibilityNo'];
			$tarOsFlag           = (int)$tgt_host_data['osFlag'];		
			
			$srchostname      =  $src_host_data['name'];
			$srcCompatibilityNum = $src_host_data['compatibilityNo'];
			$srcOsFlag           = (int)$src_host_data['osFlag'];
			$pairType = 0;
			if($target_info['planId'] != 1)
			{
				if((isset($src_lun_id) && $src_lun_id !='') || (isset($tgt_lun_id ) && $tgt_lun_id  != ''))
				{
					$pairType = $pair_type_array[$target_info['planId']];
				}
			}
			$startingPhysicalOffset = "0";
			if($pairType == 2) // pair type is prism
			{
				$prism_obj = new PrismConfigurator();
				$lunCapacity = $prism_obj->get_lun_size($source_info['Phy_Lunid']);
				$srccapacity = $tarcapacity = $lunCapacity;
				
				$sync_device = $prism_obj->get_sync_device($source_info['Phy_Lunid']);
				if($sync_device)
				{
					$option = 1; // this option for sending request from AT to get physical offset.
					$startingPhysicalOffset = $prism_obj->get_volume_level_info($obj['host_id'],$source_info['Phy_Lunid'],$sync_device,$option);
				}
			}
			$atLunStatsRequest = array(0, intval(0), "", array());

			if($tarCompatibilityNum >= 630050)
			{
				$volume_settings['resync_copy_mode'] = $target_info['sync_copy'];		
				$end_point_settings['resync_copy_mode'] = $target_info['sync_copy'];			
			}
			
			if($tarCompatibilityNum >= 710000)
			{
				$protection_plan_obj = new ProtectionPlan();
				$end_point_settings['rawsize'] = (string)$srcRawSize;
				$volume_settings['rawsize'] = (string)$tgtRawSize;
				$scenarioId =  $target_info['scenarioId'];
				$end_point_settings['protection_direction'] = "forward";
				$volume_settings['protection_direction'] = "forward";
				if($scenarioId)
				{
					$protectionDirection = $protection_plan_obj->getProtectionDirection($scenarioId);
					if($protectionDirection)
					{
							$end_point_settings['protection_direction'] = $protectionDirection;
							$volume_settings['protection_direction'] = $protectionDirection;
					}
				}
			}		
			
			if($tarCompatibilityNum >= 820000)
			{
				$end_point_settings['useCfs'] = $target_info['useCfs'];
				$end_point_settings['psId'] = $target_info['processServerId'];
				$volume_settings['useCfs'] = $target_info['useCfs'];
				$volume_settings['psId'] = $target_info['processServerId'];
			}
			
			if ($target_data_plane == 0)
			{
				$end_point_settings['target_data_plane'] = 'UNSUPPORTED_DATA_PLANE';
                $volume_settings['target_data_plane'] = 'UNSUPPORTED_DATA_PLANE';
			}
			elseif ($target_data_plane == 1)
			{
				$end_point_settings['target_data_plane'] = 'INMAGE_DATA_PLANE';
                $volume_settings['target_data_plane'] = 'INMAGE_DATA_PLANE';
			}
			elseif ($target_data_plane == 2)
			{
				$end_point_settings['target_data_plane'] = 'AZURE_DATA_PLANE';
                $volume_settings['target_data_plane'] = 'AZURE_DATA_PLANE';
			}
			
			$source = array();
			$pause_pending_maintenance_src = get_pause_pending_maintenance_src_info($target_info['replication_status'],$target_info['pauseActivity'],$target_info['src_replication_status']);
			$pause_pending_maintenance_trg = get_pause_pending_maintenance_trg_info($target_info['tar_replication_status'],$target_info['pauseActivity'],$target_info['ruleId'],$target_info['destinationDeviceName'], $target_info['destinationHostId'], $tgt_host_data['osFlag'], $retention_array,$target_info['replication_status']); 
			
			$srccapacity = $target_info['srcCapacity'];
			$compressionEnable = (int) $target_info['compressionEnable'];
			$sourceDevice = $target_info['sourceDeviceName'];
			
			if($target_info['shouldResync'] != 2)
			{
				$resyncRequiredFlag = false;
			}
			else
			{
				if($target_info['shouldResync'] == 1 or $target_info['shouldResync'] == 8 or $target_info['shouldResync'] == 2)
				{
					$resyncRequiredFlag = true;
				}
				else
				{
					$resyncRequiredFlag = false;
				}
			}

			$srcResyncStarttime = $target_info['resyncStartTagtime'];
			$srcResyncEndtime   =  $target_info['resyncEndTagtime'];
			$resyncRequiredCause = (int) $target_info['ShouldResyncSetFrom'];
			$resyncRequiredTimestamp = $target_info['resyncSettimestamp'];
			$job_id = $target_info['jobId'];
			$resyncStartTagtimeSequence = $target_info['resyncStartTagtimeSequence'];
			$resyncEndTagtimeSequence = $target_info['resyncEndTagtimeSequence'];
			$resync_counter = $target_info['resyncCounter'];			
			
			#New fast sync holds the mode as 6 for step1 transition
			#0-- SYNC_DIFF, 1--SYNC_OFFLOAD, 2--SYNC_FAST, 3--SYNC_SNAPSHOT, 4--SYNC_DIFF_SNAPSHOT, 5--SYNC_QUASIDIFF, 6 -- NEW FAST RESYNC VERSION 7 -- Local copy
			$syncMode =  ($target_info['isQuasiflag'] == 0) ? ($target_info['resyncVersion']==0 ? 1 : ($target_info['resyncVersion']==2 ? 6 : ($target_info['resyncVersion']==3 ? 7 : ($target_info['resyncVersion']==4 ? 7 : 2)))) : (($target_info['isQuasiflag'] == 1)  ? 5 : 0 );			

			if( $lastDevice === $sourceDevice ) 
			{
				$source[]= array( 0,$target_info['sourceDeviceName'], $srcMountPoint,$srcfileSystem,$srchostname /* to do: hostname */, $target_info['sourceHostId'], intval($target_info['ftpsDestSecureMode']),intval($target_info['ftpsSourceSecureMode']), $syncMode,intval($srctransProtocol),(int)$readwritemode,$srccapacity, $resyncDirectory,$rpoThreshold,$srcResyncStarttime,$srcResyncEndtime,$tarCompatibilityNum,$resyncRequiredFlag,$resyncRequiredCause,$resyncRequiredTimestamp,$srcOsFlag ,$compressionEnable );
				
				array_push($source[count($source)-1], $job_id);
				array_push($source[count($source)-1], $sanVolumeInfo);
				array_push($source[count($source)-1], array());
				$ps_throttle = $ps_array[$target_info['processServerId']]['cummulativeThrottle'];				
				/* 4369:: sending throttle values per pair level for 5.5 agents*/
				$throttle_settings = array();
				$throttle_settings = vxstub_should_throttle_fast( $target_info['resyncVersion'], $target_info['throttleresync'], $throttle_diff, $lun_state, $ps_throttle , $license_expired_source, $license_expired_target, $target_info['sourceDeviceName']) ;
				#$throttle_settings = vxstub_should_throttle_fast( $obj, $row[1], $row[2], $obj[host_id], $row[0] );			
				# move retention + resyncStartTagtimeSequence and resyncEndTagtimeSequence.
				if($tarCompatibilityNum >= 630050)
				{
					array_push($source[count($source)-1],$src_replication_state,$replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$sourcevolumeType,$end_point_settings);
				}
				else if($tarCompatibilityNum >= 560000)
				{
					#resync_counter is  sent as part of initial_settings
					#$startingPhysicalOffset = $row[19];
					
					array_push($source[count($source)-1],$src_replication_state,$replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$sourcevolumeType);
				}
				else if($tarCompatibilityNum >= 550000)
				{
					array_push($source[count($source)-1],$src_replication_state,$replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize);
				}
				$lastDevice = $sourceDevice;
			}
			else 
			{
				$targets = array();
				
				if( $src_host_data['osFlag'] == 3)
				{
					$device_path = $target_info['destinationDeviceName'];
				}
				else
				{
					$device_path = str_replace(":", "", $target_info['destinationDeviceName']);
				}
				
				$device_path = str_replace("\\", "/", $device_path);
				$resyncDirectory = $ORIGINAL_REPLICATION_ROOT."/".$obj[host_id]."/".$device_path."/resync";
				 
				$source[]= array( 0,$target_info['sourceDeviceName'],$srcMountPoint,$srcfileSystem, $srchostname /* to do: hostname */, $target_info['sourceHostId'], intval($target_info['ftpsDestSecureMode']),intval($target_info['ftpsSourceSecureMode']), $syncMode,intval($srctransProtocol),(int)$readwritemode,$srccapacity, $resyncDirectory,$rpoThreshold,$srcResyncStarttime,$srcResyncEndtime,$tarCompatibilityNum,$resyncRequiredFlag,$resyncRequiredCause,$resyncRequiredTimestamp,$srcOsFlag ,$compressionEnable);
				
				array_push($source[count($source)-1], $job_id);
				array_push($source[count($source)-1], $sanVolumeInfo);
				array_push($source[count($source)-1], array());
				if ($src_lun_id != NULL && $lun_state != $PROTECTED && $src_replication_state == 0)
				{
					$src_replication_state = $PAUSED_PENDING;
				}
									
				/* 4369:: sending throttle values per pair level for 5.5 agents */
				$throttle_settings = array();
				$throttle_settings = vxstub_should_throttle_fast( $target_info['resyncVersion'], $target_info['throttleresync'], $throttle_diff, $lun_state, $ps_throttle , $license_expired_source, $license_expired_target, $target_info['sourceDeviceName']) ;	
								
				if($tarCompatibilityNum >= 630050)
				{
					array_push($source[count($source)-1],$src_replication_state, $replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$sourcevolumeType,$end_point_settings);
				}
				else if($tarCompatibilityNum >= 560000)
				{
					# resync_counter is sent as part of initial_settings
					array_push($source[count($source)-1],$src_replication_state, $replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$sourcevolumeType);
				}
				else if($tarCompatibilityNum >= 550000)
				{
					array_push($source[count($source)-1],$src_replication_state, $replication_cleanup_src,$pending_diffs_cx_src,$currentRPO_src,$apply_rate_src,$pause_pending_maintenance_src,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[0],$resync_counter,$srcRawSize);
				}
				$lastDevice = $sourceDevice;
			}

			if( $tgt_host_data['osFlag'] == 3)
			{
				$device_path = $target_info['sourceDeviceName'];
			}
			else
			{
				$device_path = str_replace(":", "", $target_info['sourceDeviceName']);
			}

			$device_path = str_replace("\\", "/", $device_path);
			$resyncDirectory_target = $ORIGINAL_REPLICATION_ROOT."/".$target_info['sourceHostId']."/$device_path/resync" ; 

			$result [] = array($target_info['destinationDeviceName'],$tarMountPoint,$tarfileSystem, $targethostname /* hostname */, $obj['host_id'],intval($target_info['ftpsDestSecureMode']), intval($target_info['ftpsSourceSecureMode']), $syncMode,intval($tartransProtocol) /*transport*/,(int)$readwritemode,$srccapacity,$resyncDirectory_target ,$rpoThreshold,$srcResyncStarttime,$srcResyncEndtime,$srcCompatibilityNum,$resyncRequiredFlag,$resyncRequiredCause,$resyncRequiredTimestamp,$tarOsFlag,$compressionEnable);
		
			array_push($result[count($result)-1], $job_id);
			array_push($result[count($result)-1], $sanVolumeInfo);
			if ($src_lun_id != NULL && $lun_state != $PROTECTED && $tar_replication_state == 0)
			{
				$tar_replication_state = $PAUSED_PENDING;
			}

			if($tarCompatibilityNum >=630050)
			{
				array_push($result[count($result)-1],$source, $tar_replication_state, $replication_cleanup_trg,$pending_diffs_cx_trg,$currentRPO_trg,$apply_rate_trg,$pause_pending_maintenance_trg,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[1],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$targetvolumeType,$volume_settings,$target_info['volumeGroupId']);
			}
			else if($tarCompatibilityNum >=560000)
			{
				#resync_counter is sent as part of initial_settings
				array_push($result[count($result)-1],$source, $tar_replication_state, $replication_cleanup_trg,$pending_diffs_cx_trg,$currentRPO_trg,$apply_rate_trg,$pause_pending_maintenance_trg,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[1],$resync_counter,$srcRawSize,$atLunStatsRequest,$startingPhysicalOffset,$targetvolumeType,$pair_info['volumeGroupId']);
			}
			else if($tarCompatibilityNum >=550000)
			{
				#resync_counter is sent as part of initial_settings
				array_push($result[count($result)-1],$source, $tar_replication_state, $replication_cleanup_trg,$pending_diffs_cx_trg,$currentRPO_trg,$apply_rate_trg,$pause_pending_maintenance_trg,$resyncStartTagtimeSequence,$resyncEndTagtimeSequence,$throttle_settings[1],$resync_counter,$srcRawSize,$pair_info['volumeGroupId']);
			}
			$cnt ++;
		}	
	}
    return $result;
}
#################### 9448 ######################

function vxstub_is_resync_required( $obj ) { return TRUE; } # to do

function vxstub_should_throttle( $obj ) 
{   
    global $REPLICATION_ROOT, $ORIGINAL_REPLICATION_ROOT;
    global $PROTECTED,$REBOOT_PENDING;
    global $conn_obj;
    global 	$logging_obj;
    $result = array();
    $hostId = $obj[host_id];	
	$pslevel_cummulative_throttling = 0;
	$delete_pending = 0;	
	$hostinfo = get_host_info($obj[host_id]);
	$hostCompatibilityNum = $hostinfo['compatibilityNo'];
	# license expiry check
    $license_expired = FALSE;
    $throttle_settings = "";
	if ($hostCompatibilityNum >= 430000 )
	{
        $query="SELECT
                src.sourceDeviceName,
                src.destinationHostId,
                src.destinationDeviceName,
                src.ruleId,
                src.throttleresync,
                src.throttleDifferentials,
                ps.cummulativeThrottle
            FROM
                srcLogicalVolumeDestLogicalVolume src,
                processServer ps                       
            WHERE
                src.sourceHostId='$hostId'
            AND
                (src.lun_state = '$PROTECTED' 
                OR
                src.lun_state = '$REBOOT_PENDING')
            AND src.processServerId = ps.processServerId"; 
        $iter = $conn_obj->sql_query($query);
        //$iter = $conn_obj->sql_query ("select sourceDeviceName, destinationHostId, destinationDeviceName, ruleId,throttleresync, throttleDifferentials from srcLogicalVolumeDestLogicalVolume where sourceHostId='$hostId' AND (lun_state = '$PROTECTED' OR lun_state = '$REBOOT_PENDING') "); 
    }
	else
	{
        $iter = $conn_obj->sql_query ("select sourceDeviceName, destinationHostId, destinationDeviceName, ruleId,throttleresync, throttleDifferentials, deleted from srcLogicalVolumeDestLogicalVolume where sourceHostId='$hostId'");
    }
	while ($row = $conn_obj->sql_fetch_row ($iter))
	{
        $file_count_check = $row[4] ; #check_resync_files($hostId, $row[0]);
		$diff_file_count_check = $row[5];
        if ($hostCompatibilityNum >= 430000 )
        {
            $pslevel_cummulative_throttling = $row[6];
        }
        else
        {
            $delete_pending = $row[6]?1:0;
        }
		#$logging_obj->my_error_handler("DEBUG","vxstub_should_throttle:: filecount check = $file_count_check, diffcount check = $diff_file_count_check, pslevelcummulativethrottling = $pslevel_cummulative_throttling, delete pending = $delete_pending");
        //$delete_pending = $row[6]?1:0;
        //$resync_throttle = $free_space_check | $file_count_check | $license_expired | $delete_pending;
        $resync_throttle   =  $pslevel_cummulative_throttling  | $file_count_check | $license_expired | $delete_pending;
        //$sentinel_throttle = $free_space_check | $license_expired | $diff_file_count_check | $delete_pending;	
        $sentinel_throttle =  $pslevel_cummulative_throttling  | $license_expired | $diff_file_count_check | $delete_pending;	
 		$throttle_settings .= $row[0].":,"."$resync_throttle,$sentinel_throttle,0;";
		#$logging_obj->my_error_handler("DEBUG","vxstub_should_throttle:: resync throttle = $resync_throttle,sentinel throttle =  $sentinel_throttle, throttle settings = $throttle_settings");
        $result [$row[0]] = array(0,(bool)$resync_throttle,(bool)$sentinel_throttle,(bool)0);
		#$logging_obj->my_error_handler("DEBUG","vxstub_should_throttle:: result: ".print_r($result,TRUE));
    }
    if ($hostCompatibilityNum >= 430000 )
	{
        $iter = $conn_obj->sql_query ("select destinationDeviceName, sourceHostId, sourceDeviceName, ruleId from srcLogicalVolumeDestLogicalVolume where destinationHostId='$hostId' AND (lun_state = '$PROTECTED' OR lun_state = '$REBOOT_PENDING')");
    }
	else
	{
        $iter = $conn_obj->sql_query ("select destinationDeviceName, sourceHostId, sourceDeviceName, ruleId, deleted from srcLogicalVolumeDestLogicalVolume where destinationHostId='$hostId'");
    }
	while ($row = $conn_obj->sql_fetch_row ($iter))
	{ 
        $delete_pending = $row[4]?1:0;
 		$rule_check = (can_replicate ($row[3], $date) ? 0 : 1);
        $outpost_throttle = $license_expired | $rule_check | $delete_pending;
 		$throttle_settings .= $row[0].":,"."0,0,$outpost_throttle;";
		#$logging_obj->my_error_handler("DEBUG","vxstub_should_throttle:: rulecheck = $rule_check, outpost throttle = $outpost_throttle, throttle settings = $throttle_settings");
        $result [$row[0]] = array(0,(bool)0,(bool)0,(bool)$outpost_throttle);
    }
	#$logging_obj->my_error_handler("DEBUG","vxstub_should_throttle: vxstub obj   returng value:".print_r($result,TRUE));
    return $result;
                
 } 

function get_cdp_settings( $obj , $final_hash=array())
{

	global $PROTECTED,$REBOOT_PENDING;
    $cdpsettings = array();
    global $conn_obj;
    global $logging_obj;

   # $logging_obj->my_error_handler("DEBUG","get_cdp_settings::".print_r($obj,TRUE)."\n ");
    $hostinfo = get_host_info($obj['host_id']);
	$hostId   = $obj['host_id'];
    $os_flag = get_osFlag($obj['host_id']);
	
	$ruleid_hash = $final_hash[6];
	$retention_array = $final_hash[7];
	
    $setting_reserved= $conn_obj->sql_query("select ValueData from transbandSettings where ValueName = 'RETENTION_RESERVE_SPACE'");
    if( !$setting_reserved )
    {
        $min_reserved ="1048576"; 
    }
	else
	{
       $retReserveSpace = $conn_obj->sql_fetch_row ($setting_reserved);
       $min_reserved = $retReserveSpace[0];
	}
	$rule_id_array = array();
	$rule_ids = array();
	foreach( $ruleid_hash as $rule_id => $dest_info)
	{
		//Constructiong rule id array incase if the calling host id is destination host id
		if($hostId == $dest_info['destinationHostId'])
		{
			$rule_id_array[$rule_id]['destinationDeviceName'] = $dest_info['destinationDeviceName'];
			$rule_id_array[$rule_id]['mediaretent'] = $dest_info['mediaretent'];
			$rule_ids[] = $rule_id;
		}
	}
	list($cdp_settings_array,$retentionWindowArray,$retentionEventPolicyArray,$retentionSpacePolicyArray,$tag_type_array) = $retention_array;
	foreach($rule_id_array as $ruleId => $cdp_info)
	{
        $mediaRetention="";
		$ret_array = array();
        # $logging_obj->my_error_handler("DEBUG","get_cdp_settings:: before initialise::".print_r($cdpsettings,TRUE)."\n ");
        if( !array_key_exists( $cdp_info['destinationDeviceName'], $cdpsettings ) ) 
        {
			//If retention is not configured for the pair
           if($cdp_info['mediaretent'] == 0) 
            {
				$cdpsettings[$cdp_info['destinationDeviceName']] =array(0,0,"",3,"",80,"0",0,"0","0",array(array(0,"0","0",0,0,"0","",0,array(""))));
            }
            else //If retention configured for the pair
            {	
                $storagePath ="";
                $storagePrunningPolicy =0;
                $alertThreshold=0;
                $storageSpace=0;
                $total_duration=0;
                $retTimeInterval = 0;
                $cdpSettingsArray = $cdp_settings_array[$ruleId];
                $uniqueId = $cdpSettingsArray['uniqueId'];
                $application_array = array(); 
                if(intval($cdpSettingsArray['retState']) == 0)
                {
                    $mediaRetention = 1;
                }
                else if(intval($cdpSettingsArray['retState']) == 1)
                {
                    $mediaRetention = 0;
                }
                else
                {
                    $mediaRetention = intval($cdpSettingsArray['retState']);
                }
				//If the policy type is time based or both(time and space based)
                if(($cdpSettingsArray['retPolicyType'] == 1) || ($cdpSettingsArray['retPolicyType'] == 2))
                {					
					$retWindowArray = $retentionWindowArray[$cdpSettingsArray['retId']];
					$startTime = 0;
                    $i =0;
					foreach($retWindowArray as $Id =>$windowValue)
					{
						$arr =array();
						$startTime = $windowValue['startTime'];
						$duration = ($windowValue['endTime'])-($windowValue['startTime']);
						$retTimeInterval = $windowValue['endTime'];
						$total_duration = ($total_duration + $duration);        
						$interval = ($windowValue['timeGranularity']);
					   
						$retEventPolicyArray = $retentionEventPolicyArray[$windowValue['Id']];
						$storagePath=$retEventPolicyArray['storagePath'];
						if($os_flag == 1)
						{
							$content_store_arr = explode(':',$storagePath);
							if(count($content_store_arr) == 1)
							{   
							   $storagePath = $storagePath.":";
							}                             
							$content_store = "$storagePath\\contentstore";
						}
						else
						{
							$content_store = "$storagePath/contentstore";
						}
						
						$tag_value=0;
						$arr[0]=0;
						$arr[1]="$startTime";
						$arr[2]="$duration" ;
						$arr[3]=(int)$interval;
						$arr[4]=(int) ($retEventPolicyArray['tagCount']);
						$arr[5]="0";//For now send blank
						$arr[6]="$content_store";//for now send contentstore blank
						$tag_type = $tag_type_array[$retEventPolicyArray['consistencyTag']];
						#$logging_obj->my_error_handler("DEBUG","get_cdp_settings::tag_type:: $tag_type \n");
						$tag_value = AppNameToTagType($tag_type);
						#$logging_obj->my_error_handler("DEBUG","get_cdp_settings::tag_value:: $tag_value \n");
						$arr[7]=$tag_value;
						$user_defined = explode(',',$retEventPolicyArray['userDefinedTag']);
						$arr[8]= $user_defined;
						$ret_array[$i]=$arr;
						$i++;
						$storagePath = $retEventPolicyArray['storagePath'];
						$storagePrunningPolicy = $retEventPolicyArray['storagePruningPolicy'];
						$alertThreshold = $retEventPolicyArray['alertThreshold'];
						$catalog_path = $retEventPolicyArray['catalogPath'];
					}
                }
				//If the policy type space based or both(time and space based)
                if(($cdpSettingsArray['retPolicyType'] == 0) || ($cdpSettingsArray['retPolicyType'] == 2))
                {                    
                    $retSpacePolicyArray = $retentionSpacePolicyArray[$cdpSettingsArray['retId']];
                    $storagePrunningPolicy = $retSpacePolicyArray['storagePruningPolicy'];
                    $alertThreshold = $retSpacePolicyArray['alertThreshold'];
                    $storageSpace = $retSpacePolicyArray['storageSpace'];
                    $catalog_path = $retSpacePolicyArray['catalogPath'];
                }
                $deviceName = $cdp_info['destinationDeviceName'];
				$metadataPath =$catalog_path; 
				$cdpVersion = (($cdpSettingsArray['sparseEnable'])!=0)?3:0;
				$cdpsettings[$deviceName]=  array(0,(int)$mediaRetention,"$uniqueId",(int)$cdpVersion,"$metadataPath",(int)$alertThreshold,"$storageSpace",(int)$storagePrunningPolicy,"$total_duration","$min_reserved",$ret_array);
            }
        }
    }
    return $cdpsettings;
}

function vxstub_update_agent_log ( $obj,$ttime,$log_level,$agent_info,
	                               $error_msg,$module = '',$alert_type = '',$error_code = '', $error_placeholders = '') 
{

	global $HOST_LOG_DIRECTORY;
	global $CHMOD;
	global $SLASH;
	global $commonfunctions_obj;
	global $logging_obj;

	$hostid_log =$obj[host_id];
	$logging_obj->my_error_handler("INFO","vxstub_update_agent_log :: $hostid_log, $ttime,$log_level,$agent_info,
	                               $error_msg,$module ,$alert_type,$error_code, $error_placeholders \n");
	if(!file_exists($HOST_LOG_DIRECTORY))
    {
		mkdir($HOST_LOG_DIRECTORY);
		$iterator = new RecursiveIteratorIterator(
					new RecursiveDirectoryIterator($HOST_LOG_DIRECTORY), RecursiveIteratorIterator::SELF_FIRST); 
		foreach($iterator as $item) 
		{ 
			$status = chmod($item, 0777); 
		} 
    }
		
	if($error_msg !='')
	{
		$hostid =$obj[host_id];
		$is_valid_host_id = $commonfunctions_obj->isValidComponent($hostid);
		if ($is_valid_host_id)
		{
			$fName=$obj[host_id].".log";
			$data=$error_msg."\n";
			$ts=$ttime; 
			$elevel=$log_level;
			$agent = $agent_info; 
				 
			if ($module == '')
				$module=0;

			if ($alert_type == '')
				$alert_type=0;

			$msg = $data;
			$data=$ts."\t".$elevel."\t".$agent." : ".$data;
			$fPath=$HOST_LOG_DIRECTORY.$SLASH.$fName;
			$fp=$commonfunctions_obj->file_open_handle($fPath,'a');

			if(!$fp){
				
				return false;
					
			}

			if(!fwrite($fp,$data)){
				return false;

			}

			if(!fclose($fp)){

				return false;

			}

			//if (($elevel == 'ERROR')||($elevel == 'FATAL')||($elevel == 'SEVERE')||($elevel == 'ALERT'))
			if($elevel == 'ALERT')
			{
				global $CXLOGGER_MULTI_QUERY;
				$CXLOGGER_MULTI_QUERY = array();
				$commonfunctions_obj = new CommonFunctions();
				$batch_sqls_obj = new BatchSqls();
				$cx_logger = 1;
				$commonfunctions_obj->process_agent_error($hostid,$elevel,$msg,$ts,$module,$alert_type,$cx_logger,$error_code,$error_placeholders);
				$batch_sqls_obj->sequentialBatchUpdates($CXLOGGER_MULTI_QUERY);
			}
		}
	}
	
	return TRUE;
}

## This function is to update resync status from targetside
function vxstub_set_target_resync_required($obj,$vol,$err='') 
{	
    $hostid = $obj[host_id];
	$origin = "target_agent";
    $result = @set_resync_required_oa( $hostid, $vol, 1,$origin,$err);
    return (bool)$result;
}

## This function is to update resync reason code from Master Target
function vxstub_set_target_resync_required_with_reason_code($obj,$disk,$error_string='',$error_code,$resync_reason_details)
{	
    $hostid = $obj[host_id];
	$origin = "target_agent";
    $result = @set_resync_required_oa($hostid, $disk, 1,$origin, $error_string, $resync_reason_details);
    return (bool)$result;
}

## This function is to update resync status from sourceSide
function vxstub_set_source_resync_required($obj,$vol,$err) 
{
	$hostid = $obj[host_id];
	$timestamp = time ();
	$result = @set_resync_required( $hostid, $vol, $err, 2,$timestamp );
	return (bool)$result;
}

## This function is to update resync reason code from Source mobility agent
function vxstub_set_source_resync_required_with_reason_code($obj,$disk,$error_string,$resync_reason_details)
{
	$source_hostid = $obj[host_id];
	$timestamp = time ();
	$result = @set_resync_required($source_hostid, $disk, $error_string, 2, $timestamp, $resync_reason_details);
	return (bool)$result;	
}

/*
 * Function Name: vxstub_update_volume_attribute
 *
 * Description: 
 *    Update the status of drive.
 *
 * Parameters:
 *     Param 1 [IN]: object
 *     Param 2 [IN]: request
 *     Param 3 [IN]: mode
 *     Param 3 [IN]: volume name 
 *     Param 3 [IN]: mount point volume
 *
 * Return Value:
 *     Ret Value: Boolean value
 *
 * Exceptions:
 *     <Specify the type of exception caught>
 */
function vxstub_update_volume_attribute($obj,
                                        $request,
                                        $mode,
                                        $vol,
                                        $mountPoint='') 
{

	global $conn_obj, $vol_obj , $logging_obj, $commonfunctions_obj;
	global $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING ,$RELEASE_DRIVE_ON_TARGET_DELETE_DONE,$RELEASE_DRIVE_PENDING,$RELEASE_DRIVE_ON_DELETE_FAIL,$SOURCE_DELETE_DONE;
	
	$mode_str = (string)$mode;
	//If the string contains all digits, then only allow the request, otherwise return from here.
	if (isset($mode) && $mode != '' && (!preg_match('/^\d+$/',$mode_str))) 
	{
		//As the post data of mode data is invalid, hence returning from here
		$commonfunctions_obj->bad_request_response("Invalid post data mode $mode in vxstub_update_volume_attribute.");
	}

	$hostid = $obj[host_id];
	$hostinfo = get_host_info($obj[host_id]);
    $hostCompatibilityNum = $hostinfo['compatibilityNo'];
	$visible = $mode ? 1 : 0;
	$unlock =  $mode ? 1 : 0;
	
	$dDname=$vol;
	$vol=verifyDatalogPath($vol);
	$vol = $conn_obj->sql_escape_string( $vol ) ;
	$logging_obj->my_error_handler("INFO","vxstub_update_volume_attribute :: request :: $request , mode :: $mode , vol :: $vol , mountPoint :: $mountPoint \n");
	$query="SELECT
				readwritemode ,
				isVisible 
			FROM
				logicalVolumes 
			WHERE
				hostId  = ? AND  
				deviceName = ?";
	$lv_result = $conn_obj->sql_query($query, array($hostid, $dDname));
	if (is_array($lv_result) && count($lv_result) == 0)
	{
		//As the post data of given devicename not exists, hence returning from here.
		$commonfunctions_obj->bad_request_response("Invalid post data devicename $dDname in vxstub_update_volume_attribute.");
	}
	foreach($lv_result as $key=>$row)
	{
		$readwriteMode_exist = $row['readwritemode'];
		$isVisible_exist = $row['isVisible'];
	}
	$dhId=$hostid;

	/*
	 * FIX for BUG #4077
	 * Added mysql_escape_string to the query to fix the issue for
	 * unhiding the target volume for mount points using cdpcli.            
	 */
	$sql="SELECT
			sourceHostId,
			sourceDeviceName 
		  FROM  
			srcLogicalVolumeDestLogicalVolume 
		  WHERE 
			destinationHostId = '$dhId' AND	 
			destinationDeviceName = '" . $conn_obj->sql_escape_string($dDname)."'";
			
	$qry=$conn_obj->sql_query($sql);
	$myRow=$conn_obj->sql_fetch_row ($qry);
	$sHid=$myRow[0];
	$sDname= $myRow[1];
	$getRepStat=get_replication_state3($sHid,$conn_obj->sql_escape_string($sDname),$dhId,$vol);
	$diffFlag=0;
	if ($getRepStat=="Differential Sync")
	{
		$diffFlag=1;
	}
	else
	{
		$diffFlag=0;
	}
	#11599 Fix
	$is_clustered = $vol_obj->is_device_clustered($hostid, $vol);
	if($is_clustered)
	{

		$cluster_host_ids_array = $vol_obj->get_clusterids_as_array($hostid, $vol);
		$cluster_host_ids = implode("','",$cluster_host_ids_array);
		$condition = " hostId IN('$cluster_host_ids') AND deviceName ='$vol'";
	}
	else
	{
		$condition = " hostId='$hostid' AND deviceName = '$vol'";
	}
	/*
	 * cx is updating only when the pair in diff sync     
	 */
	if ($diffFlag==1)
	{
		/*
		 * If the action is from CDPCLI($request = 1) & drive visbility($unlock = 1) 
		 * then do the below.
		 */
		if ($request && $unlock)
		{
			/*
			 * Updates the mount point information.
			 */
			if($mountPoint != '' )
			{
				updateMountPoint($hostid, $dDname, $mountPoint);
			}
			/*
			 * FIX for BUG #4077
			 * Added mysql_escape_string to the query to fix the issue for
			 * unhiding the target volume for mount points using cdpcli            
			 */
			/*
			 * This is to update the drive visibility to one based on the user 
			 * request. In this case read write mode will be either in ready only
			 * or readwrite mode and the sent flag value will be updated in the
			 * database.
			 */
			$query ="UPDATE 
						logicalVolumes 
					 SET
						isVisible='1', 
						visible='1', 
						readwritemode='$mode' 
					 WHERE
						$condition";
		}
		/*
		 * If the request is from the CDPCLI($request = 1) & 
		 * drive hide($unlock = 0)
		 */
		else if ($request && (!$unlock))
		{
			/*
			 * FIX for BUG #4077
			 * Added mysql_escape_string to the query to fix the issue for
			 * unhiding the target volume for mount points using cdpcli            
			 */
			/*
			 * Set all the database flags with hide state.
			 */
			$query ="UPDATE
						logicalVolumes 
					 SET 
						isVisible='1', 
						visible='0',
						readwritemode='0' 
					 WHERE
						$condition";
		}
		else if (!$request)
		{
			$query ="UPDATE 
						logicalVolumes 
					 SET
						isVisible='2', 
						visible='$visible', 
						readwritemode='$mode' 
					 WHERE 
						$condition";
			if ($hostCompatibilityNum >= 430000 )
			{
			$src_update_sql1 = "UPDATE 
									srcLogicalVolumeDestLogicalVolume
								SET 
									replication_status = $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING 
								WHERE 
									destinationHostId = '$dhId'
								AND
									replication_status = $RELEASE_DRIVE_PENDING
								AND	
								 
								destinationDeviceName = '" .$conn_obj->sql_escape_string($dDname)."'";
				
				$result = $conn_obj->sql_query($src_update_sql1);
			}
			else
			{
				$src_update_sql1 = "UPDATE 
									srcLogicalVolumeDestLogicalVolume
								SET 
									replication_status = $SOURCE_DELETE_DONE 
								WHERE 
									destinationHostId = '$dhId'
								AND
									(replication_status = $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING or
									replication_status = $RELEASE_DRIVE_PENDING)
								AND	
								 
								destinationDeviceName = '" .$conn_obj->sql_escape_string($dDname)."'";
				$result = $conn_obj->sql_query($src_update_sql1);
				
			}
				
				$src_update_sql2 = "UPDATE 
									   srcLogicalVolumeDestLogicalVolume
								   SET 
									   replication_status = $SOURCE_DELETE_DONE 
								   WHERE 
									   destinationHostId = '$dhId'
								   AND
										replication_status = $RELEASE_DRIVE_ON_DELETE_FAIL
								   AND	
								    destinationDeviceName = '" .$conn_obj->sql_escape_string($dDname)."'";
			$result2 = $conn_obj->sql_query($src_update_sql2);
			$logging_obj->my_error_handler("DEBUG","vxstub_update_volume_attribute ::  src_update_sql1 :: $src_update_sql1  , src_update_sql2 :: $src_update_sql2 \n");

		}
		
		$logging_obj->my_error_handler("DEBUG","vxstub_update_volume_attribute :: query :: $query  \n");
		$iter = $conn_obj->sql_query($query);

		if( !$iter ) 
		{
			trigger_error( $conn_obj->sql_error() );
		}
	}
	return TRUE;
}

## function to send current visibility status 0-hide, 1-visible RO, 2- visible RW 3- processing
function vxstub_get_current_volume_attribute($obj,$deviceName) 
{
    global $conn_obj;
    $hostid = $obj[host_id];
    
    $query = "SELECT
                readwritemode ,
                visible,
                isVisible
            FROM
                logicalVolumes
            WHERE
                hostId  = ? 
                AND
                 deviceName = ?";
				 
	

    $hosts = $conn_obj->sql_query($query, array($hostid,$deviceName));
	
	foreach($hosts as $host) 
	{
		$row = $host;
		break;
	}

    $readwritemode = ((int)$row['isVisible']==1) ? 3 : (((int)$row['isVisible'] == 3) ? 4 : ($row['visible']?$row['readwritemode']:0));

    #debugLog("readwritemode :: $readwritemode ...  and isVisible = $row[2]  ".time());
    return intval($readwritemode) ;

}


## function to get can_clear_diffs return value boolean
function vxstub_get_clear_diffs($obj,$vol)
{
	global $logging_obj, $commonfunctions_obj, $SOURCE_DATA;
	$hostid = $obj[host_id];
	global $conn_obj;
	
	$valid_protection = $commonfunctions_obj->isValidProtection($hostid, $vol, $SOURCE_DATA);
	if (!$valid_protection)
	{
		$logging_obj->my_error_handler("INFO","Invalid post data $vol, hence, returning from vxstub_get_clear_diffs." ,Log::BOTH);
		//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
		return TRUE;
	}
	
	$compatibility_no =  $commonfunctions_obj->get_compatibility($hostid);
	if (is_one_to_many_by_source ($hostid, $vol))
	{		
		if($compatibility_no < 550000)
		{
			$iter = $conn_obj->sql_query("select distinct(isResync),deviceName,oneToManySource,resyncVersion from srcLogicalVolumeDestLogicalVolume, logicalVolumes where sourceHostId = '$hostid' and  sourceDeviceName = '".$conn_obj->sql_escape_string( $vol )."' and sourceHostId = hostId and  sourceDeviceName = deviceName  and resyncVersion=0 and isResync=1");
			$logging_obj->my_error_handler("INFO","vxstub_get_clear_diffs:: select_query: select distinct(isResync),deviceName,oneToManySource,resyncVersion from srcLogicalVolumeDestLogicalVolume, logicalVolumes where sourceHostId = '$hostid' and  sourceDeviceName = '".$conn_obj->sql_escape_string( $vol )."' and sourceHostId = hostId and  sourceDeviceName = deviceName  and resyncVersion=0 and isResync=1 & iter: $iter\n"); 			
		}
		else
		{
						
			#11790 Fix
			/*How CX can send clear diffs?
			a. Check the RST (resync start tag time) for 1toN, if exists send the clear
			diffs as zero, otherwise check for restart re-sync counter.
			b. Get the max restart resync counter value for all 1toN pairs, if the max is >
			0, then send it as zero, otherwise 1.*/

			$rst_query = "SELECT
								resyncStartTagtime
							FROM
								srcLogicalVolumeDestLogicalVolume
							WHERE
								sourceHostId = '$hostid' 
							AND
								sourceDeviceName = '".$conn_obj->sql_escape_string( $vol )."'
							AND
								resyncStartTagtime != 0
							LIMIT 1";
			
			$result_set = $conn_obj->sql_query($rst_query);
			$num_rows = $conn_obj->sql_num_row($result_set);
			//$logging_obj->my_error_handler("DEBUG","vxstub_get_clear_diffs:: rst_query: $rst_query & num_rows: $num_rows \n");		
			if($num_rows == 1)
			{
				$logging_obj->my_error_handler("INFO","vxstub_get_clear_diffs:: entered if when num_rows is 1 & so returning false \n");
				return FALSE;
			}
			else
			{
				$rrc_query = "SELECT
									max(restartResyncCounter)
								FROM
									srcLogicalVolumeDestLogicalVolume
								WHERE
									sourceHostId = '$hostid' 
								AND
									sourceDeviceName = '".$conn_obj->sql_escape_string( $vol )."'";
									
                $result = $conn_obj->sql_query($rrc_query);									
				$row = $conn_obj->sql_fetch_row( $result);
				$max_rrc = $row[0];
				$logging_obj->my_error_handler("DEBUG","vxstub_get_clear_diffs:: rrc_query: $rrc_query,max_rrc: $max_rrc \n");	
				if($max_rrc > 0)
				{
					$logging_obj->my_error_handler("INFO","vxstub_get_clear_diffs:: entered if when max_rrc is > 0 so, returning false \n");
					return FALSE;
				}
				else
				{
					$logging_obj->my_error_handler("INFO","vxstub_get_clear_diffs:: entered else when max_rrc is <= 0 so, returning true \n");
					return TRUE;
				}
			}
		}
	}
	else
	{
		$iter = $conn_obj->sql_query( "select distinct (deviceName),oneToManySource,isResync from srcLogicalVolumeDestLogicalVolume, logicalVolumes where sourceHostId = '$hostid' and  sourceDeviceName = '".$conn_obj->sql_escape_string( $vol )."' and sourceHostId = hostId and  sourceDeviceName = deviceName and isResync=1");
		$logging_obj->my_error_handler("DEBUG","vxstub_get_clear_diffs:: select distinct (deviceName),oneToManySource,isResync from srcLogicalVolumeDestLogicalVolume, logicalVolumes where sourceHostId = '$hostid' and  sourceDeviceName = '".$conn_obj->sql_escape_string( $vol )."' and sourceHostId = hostId and  sourceDeviceName = deviceName and isResync=1\n"); 
	}

		$rowcnt = $conn_obj->sql_num_row($iter);
		$logging_obj->my_error_handler("DEBUG","vxstub_get_clear_diffs:: rowcnt: $rowcnt \n");
		if ($rowcnt>0)
		{
			$logging_obj->my_error_handler("INFO",array("Result"=>TRUE,"Status"=>"Success"),Log::BOTH);
			return TRUE;
		}
		else
		{
			$logging_obj->my_error_handler("INFO",array("Result"=>FALSE,"Status"=>"Fail"),Log::BOTH);
			return FALSE;
		}

}


function vxstub_get_srr_jobs($obj,$version=0,$targetDeviceName='') 
{

/*
snaptype::::	
	
0-Schedule snapShot on Physical drive
1-recovery on physical drive
2-roll back
3-
4-schedule snapshot on virtual drive
5-recovery on virtual drive
6-event based 

EXECUTION STATE::

For Physical drives
0-queued 
1-Ready
2-Snapshot Started
3-Snapshot Inprogress
4-Complete
5-Failed
6-Snapshot Completed, Recovery Started
7-Snapshot Completed, Recovery Inprogress


For Virtual drives
0-queued 
1-Ready
2-Map Generation Started
3-Map Generation Inprogress
4-Complete
5-Failed
6-Map Generation Completed, Mount Started
7-Map Generation Completed, Mount Inprogress

VOLUME TYPE :::

0-Physical
2-virtual
1-virtual mounted

*/
        $hostid = $obj[host_id];
	$result = array();	 
    return $result;
}

       
  /*
     * Function Name: vxstub_set_end_quasi_state_request
     *
     * Description:
     *
     *    This function updates quasi flag from resync step(II) to diff sync
     *	 
     * Parameters:
     *    Param 1 [IN]:$obj
     *    Param 2 [IN]:$destDeviceName
     *
     * Return Value:
     *    Ret Value: Returns boolean value
     *    
    */

function vxstub_set_end_quasi_state_request($obj,$destDeviceName)
{
    global $conn_obj,$logging_obj,$commonfunctions_obj,$TARGET_DATA;
	$result=0;
	$hostid = $obj[host_id];
	$valid_protection = $commonfunctions_obj->isValidProtection($hostid, $destDeviceName, $TARGET_DATA);
	if (!$valid_protection)
	{
		$logging_obj->my_error_handler("INFO","Invalid post data $destDeviceName in method vxstub_set_end_quasi_state_request.",Log::BOTH);
		//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
		return (int)$result;
	}
	
	$escape_device_name=$conn_obj->sql_escape_string ($destDeviceName);
	$logging_obj->my_error_handler("INFO",array("DestinationDeviceName"=>$destDeviceName),Log::BOTH);
	$conn_obj->enable_exceptions();
	try
	{
		$conn_obj->sql_begin();
		$sql_quasi = "SELECT 
					  isQuasiflag,
					  scenarioId,
					  planId				  
				FROM 
					  srcLogicalVolumeDestLogicalVolume 
				WHERE 
					  destinationHostId= '$hostid' AND 
					  destinationDeviceName = '$escape_device_name'";
			
		$result = $conn_obj->sql_query($sql_quasi); 
		$row = $conn_obj->sql_fetch_object($result);
		$quasi_flag = $row->isQuasiflag;
		$scenario_id = $row->scenarioId;
		$plan_id = $row->planId;
	
		if($quasi_flag == 1)
		{
			#$logging_obj->my_error_handler("DEBUG","quasiFlag--update_stats_quasi--$hostid,$destDeviceName,$quasi_flag \n ");
			$result = @update_stats_quasi($hostid,$destDeviceName);
		}
		
		$sql_isquasi = "SELECT 
					isQuasiflag,
					destinationHostId,
					sourceHostId
				FROM
					srcLogicalVolumeDestLogicalVolume 
				WHERE 
					scenarioId = '$scenario_id' AND 
					planId = '$plan_id'";
			
		$result0 = $conn_obj->sql_query($sql_isquasi); 
		$isDiff = 1;
		
		while($row0 = $conn_obj->sql_fetch_object($result0))
		{
			$quasi = $row0->isQuasiflag;
			$destinationHostId = $row0->destinationHostId;
			$sourceHostId = $row0->sourceHostId;
			if($quasi != 2)
			{
				$isDiff = 0;
				break;
			}
		}
		$conn_obj->sql_commit();
	}
	catch(SQLException $e)
	{
		$conn_obj->sql_rollback();
		$result = 1;
		$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>$result),Log::BOTH);
	}
	$conn_obj->disable_exceptions();	
	if (!$GLOBALS['http_header_500'])
		$logging_obj->my_error_handler("INFO",array("Result"=>(int)$result,"Status"=>"Success"),Log::BOTH);
	else
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
	return (int)$result;
}


function vxstub_update_outpost_agent_status($obj,$vol,$apply_rate=0)
{
	  $hostid = $obj[host_id];
	 $result = @update_stats_outpost( $hostid, $vol,0, 0,$apply_rate);
        return (int)$result;
}

## CDPSTOP REplication
function vxstub_cdp_stop_replication($obj,$destDeviceName,$cleanupAction="")
{
	$hostid = $obj[host_id];
	global  $conn_obj, $logging_obj, $app_obj, $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING, $STOP_PROTECTION_PENDING, $SOURCE_DELETE_DONE, $SOURCE_DELETE_PENDING, $commonfunctions_obj, $TARGET_DATA;
	$flag = TRUE;
	$conn_obj->enable_exceptions();
	try
	{
		$valid_protection = $commonfunctions_obj->isValidProtection($hostid, $destDeviceName, $TARGET_DATA);
		if (!$valid_protection)
		{
			$logging_obj->my_error_handler("INFO","Invalid post data $destDeviceName in method vxstub_cdp_stop_replication.",Log::BOTH);
			//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
			return TRUE;
		}
		
		$conn_obj->sql_begin();
		$volume_obj = new VolumeProtection();
		$process_obj = new ProcessServer();
		$logging_obj->my_error_handler("INFO",array("DestinationHostId"=>$hostid,"DestinationDeviceName"=>$destDeviceName,"CleanUpAction"=>$cleanupAction),Log::BOTH);
		
		$sql = "select 
					sourceHostId, 
					sourceDeviceName, 
					Phy_Lunid 
				from 
					srcLogicalVolumeDestLogicalVolume 
				where 
					destinationHostId = ? 
				and  
					destinationDeviceName = ?";
		$master_target_details = $conn_obj->sql_query($sql, array($hostid, $destDeviceName));

		if(is_array($master_target_details) and count($master_target_details) > 0)
		{
			foreach($master_target_details as $key=>$value)
			{
				$srcHostId = $value['sourceHostId'];
				$srcDevName = $value['sourceDeviceName'];
				$phy_lunid = $value['Phy_Lunid'];
			}

			if(isset($phy_lunid) && ($phy_lunid != ''))
			{
				$pair_type = $volume_obj->get_pair_type($phy_lunid);
			}
			else
			{
				$pair_type = 0;
			}
			$logging_obj->my_error_handler("INFO","Source Host Id  :: $srcHostId  Source Device Name :: $srcDevName \n");
			$isFabricLun = $volume_obj->is_fabric_lun($srcHostId, $srcDevName, $hostid, $destDeviceName);
			if ($volume_obj->is_fabric_lun($srcHostId, $srcDevName, $hostid, $destDeviceName))
			{
				$state_field = "lun_state";
				$state = $STOP_PROTECTION_PENDING;
				$volume_obj->stop_protecting_fabric_lun($srcHostId, $srcDevName, $hostid, $destDeviceName, $state_field, $state);
			}
			
			$retentionlogsCleanup = 0;
			if((strpos($cleanupAction,'deleteretentionlog=yes'))!== false)
			{
				$retentionlogsCleanup = 1;
			}
			$logging_obj->my_error_handler("INFO",array("RetentionLogsCleanUp"=>$retentionlogsCleanup),Log::BOTH);

			if($retentionlogsCleanup)
			{
				/*
				* Call from CDP CLI Rollback
				*/
				$retentionlogsCleanup = '10';
			}
			else
			{
				$sql = "SELECT 
							retentionlogsCleanup 
						FROM 
							snapShots 
						WHERE 
							srcHostid='$hostid' AND 
							 srcDeviceName='".$conn_obj->sql_escape_string($destDeviceName)."' AND			
							snaptype='2'";
				$rs = $conn_obj->sql_query($sql);
				
				if($rs)
				{
					$fetch_data = $conn_obj->sql_fetch_array($rs);
					$retentionlogsCleanup = ($fetch_data['retentionlogsCleanup']) ? '10' : '00';
				}
				else
				{
					$retentionlogsCleanup = '00';
				}
			}
		
			$processServerId = $process_obj->getProcessServerId($srcHostId, $srcDevName, $hostid, $destDeviceName);
			$get_sync_state = $volume_obj->get_qausi_flag($srcHostId, $srcDevName, $hostid, $destDeviceName);
			if(($processServerId == $hostid) || ($get_sync_state == 0))
			{
				$rep_options = "001010".$retentionlogsCleanup."00";
			}
			else
			{
				$rep_options = "101010".$retentionlogsCleanup."00";
			}
			
			$hostinfo = get_host_info($obj[host_id]);
			$hostCompatibilityNum = $hostinfo['compatibilityNo'];
		
			if ($hostCompatibilityNum >= 430000 )
			{
				$replication_status = ($pair_type == 3) ? $SOURCE_DELETE_PENDING : $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING;
			}
			else
			{
				$replication_status = $SOURCE_DELETE_DONE;
				$rep_options = "0";
			}
								
			$dest_device_name = $conn_obj->sql_escape_string($destDeviceName);
			$query = "SELECT scenarioId FROM srcLogicalVolumeDestLogicalVolume WHERE destinationHostId='$hostid' AND destinationDeviceName='$dest_device_name'";
			$rs1 = $conn_obj->sql_query($query);
			$fetch_data1 = $conn_obj->sql_fetch_array($rs1);
			$scenarioId = $fetch_data1['scenarioId'];
			if($scenarioId)
			{
				$query2 = "SELECT referenceId FROM applicationScenario WHERE scenarioId='$scenarioId' AND scenarioType = 'Data Validation and Backup'";
				$rs2 = $conn_obj->sql_query($query2);
				$fetch_data2 = $conn_obj->sql_fetch_array($rs2);
				$referenceId = $fetch_data2['referenceId'];
				if($referenceId)
				{
					$query3 = "DELETE FROM applicationScenario WHERE scenarioId='$referenceId'";
					$conn_obj->sql_query($query3);
				}
			}
			
			$sql = "UPDATE 
						srcLogicalVolumeDestLogicalVolume
					SET 
						replication_status = '$replication_status',
						replicationCleanupOptions = '$rep_options',
						deleted = '1'
					WHERE 
						destinationHostId ='$hostid' AND
						 destinationDeviceName = '$dest_device_name'";
			$logging_obj->my_error_handler("INFO",array("Sql"=>$sql,"SourceHostId"=>$srcHostId,"SourceDeviceName"=>$srcDevName),Log::BOTH);
			
			$conn_obj->sql_query($sql);
		
			if($scenarioId)
			{
				$logging_obj->my_error_handler("DEBUG","vxstub_cdp_stop_replication:: destDeviceName ::$destDeviceName In Loop\n");
				$app_obj->vxstubUpdateStatusForManualFailoverfailbackPlans($scenarioId);
			}
			
			$commonfun_obj = new CommonFunctions();
							
			$trg_agent_type = $commonfun_obj->get_host_type($hostid);
			
			if(strtolower($trg_agent_type) == 'fabric')
			{
				delete_unexported_records($hostid,$destDeviceName);
			}
		
			$src_host_ipaddress = $volume_obj->get_agent_ip_address($srcHostId);
			$dest_host_ipaddress = $volume_obj->get_agent_ip_address($hostid);
			$commonfunctions_obj->writeLog($_SESSION['username'],"Deletion initiated for the pair ($src_host_ipaddress($srcDevName),$dest_host_ipaddress($destDeviceName)) from rollback request.");
			$conn_obj->sql_commit();
		}
		else
		{
			$flag = FALSE;
		}	

	}
	catch(Exception $e)
	{
		$conn_obj->sql_rollback();
		$logging_obj->my_error_handler("INFO",array("DeviceName"=>$destDeviceName,"TransactionStatus"=>"ROLLBACK","Result"=>FALSE),Log::BOTH);
		$flag = FALSE;
	}
	$conn_obj->disable_exceptions();	
	if (!$GLOBALS['http_header_500'])
		$logging_obj->my_error_handler("INFO",array("DeviceName"=>$destDeviceName, "Result"=>$result,"Status"=>"Success"),Log::BOTH);
	else
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
	return $flag;
	//return TRUE; ## todo exact success/failure status 
}

/*
 * Function Name: delete_unexported_records
 *
 * Description:
 *
 *    This function is used to cleanup the unexported Vsnaps where the
 *     vsnap itself is not started.
 *	 
 * Parameters:
 *    Param 1 [IN]: hostid
 *    Param 2 [IN]: destDeviceName 
 *
 * Return Value:
 *    Ret Value: Returns boolean value
 *
 * Exceptions:
 */

function delete_unexported_records($hostid,$destDeviceName)
{
	global $conn_obj;
	global $logging_obj;
	$dest_device_name = $conn_obj->sql_escape_string($destDeviceName);
	$vsnap_devices = array();
	$sql = "SELECT
				b.exportedDeviceName,
				b.exportedFCLunId
			FROM
				snapShots a,exportedFCLuns b
			WHERE
				a.srcHostid = '$hostid' AND
				a.srcDeviceName = '$dest_device_name' AND 
				a.executionState!='4' AND
				a.snaptype = '5' AND
				a.destDeviceName = b.exportedDeviceName";
	$rs = $conn_obj->sql_query($sql);
	if($conn_obj->sql_num_row($rs))
	{
		while($row = $conn_obj->sql_fetch_row($rs))
		{
			$vsnap_devices[] = $row[0];
		}
	}
	
	$sql = "SELECT
				b.exportedDeviceName,
				b.exportedFCLunId
			FROM
				snapshotMain a,exportedFCLuns b
			WHERE
				a.srcHostid = '$hostid' AND
				a.srcDeviceName = '$dest_device_name' AND
				a.status != '100' AND
				a.volumeType = '2' AND
				a.Deleted = '0' AND
				a.destDeviceName = b.exportedDeviceName";
	$rs = $conn_obj->sql_query($sql);
	if($conn_obj->sql_num_row($rs))
	{
		while($row = $conn_obj->sql_fetch_row($rs))
		{
			$vsnap_devices[] = $row[0];
		}
	}
	/*
	* Delete all the pending vsnap entries from exported tables.
	*/
	if(count($vsnap_devices)>0)
	{
		$vsnap_devices = implode("','",$vsnap_devices);
		
		$sql = "DELETE FROM
					exportedDevices
				WHERE
					hostId = '$hostid' AND
					exportedDeviceName IN ('$vsnap_devices')";
		$rs = $conn_obj->sql_query($sql);
	}
}

function get_agent_version_from_hostid ($host_id)
{
  
   global  $conn_obj;
   //debugLog("In agent_version function wit host id : $host_id\n");
   /* Get the sentinel and outpost version from host id */
   
   $agent_version_query = "SELECT sentinel_version, outpost_version 
							FROM hosts 
							WHERE 
							id='$host_id'";
	$iter = $conn_obj->sql_query($agent_version_query, $db );

    if( !$iter ) 
	{
        trigger_error( $conn_obj->sql_error( $db ) );
    }
	
	$outpost_version = "";
	$sentinel_version = "";
	
	while( $row = $conn_obj->sql_fetch_row( $iter ) ) 
	{
	     $sentinel_version = $row[0];
		 $outpost_version = $row[1];
	}
	return $outpost_version;
}

/*
 * Function Name: vxstub_convert_string_to_wwn
 *
 * Description:
 *     This function is to convert string vlaue into wwn formate. - Prakash Goyal
 *  
 * Parameters:
 *     [IN]: port or node wwwn in ":" format
 * Return Value:
 *    port or node wwwn in "0x" format
 *
 * Exceptions:
 *     
 */

function vxstub_convert_string_to_wwn($wwn_str)
{
   	$wwn_array = explode(":", $wwn_str);
	$hexwwn = "0x";//prefix with 0x
	foreach($wwn_array as $i)
	{
		$hexwwn = $hexwwn.$i; 
	}
	return $hexwwn;
}


function vxstub_send_event_notify_info($obj,$notify_info)
{
	#debugLog("\n vxstub_send_event_notify_info = ".print_r($notify_info, TRUE));
	return true;
}

/*
* 1toN fast implementation specific calls
*/
/*
 * Function Name: vxstub_set_resync_start_time_stamp_fastsync
 *
 * Description:
 * This function is the interface call to update the resync start time stamp.
 *
 * Parameters: 
 * Initiator Id, source host identity, source device name, destination host 
 * identity, destination device name, resync tag time stamp, sequence time, 
 * type
 *
 * Return Value:
 * This function returns the 0(success) or 4(if update fails)
 *
*/

function vxstub_set_resync_start_time_stamp_fastsync(
$obj,
$src_host_id,
$source_device_name,
$destination_host_id,
$destination_device_name,
$jobid,
$tag_timestamp,
$seq)
{
    $fast_sync_obj = new Fastsync();
    if (is_array($obj))
    {
        $initiator_id = $obj[host_id];
    }
    $result = $fast_sync_obj->updateResyncTimes(
        $initiator_id,
        $src_host_id,
        $source_device_name,
        $destination_host_id,
        $destination_device_name,
        $tag_timestamp,
        $seq,
		$jobid,
        "starttime");
    return (int)$result;
}

/*
 * Function Name: vxstub_set_resync_end_time_stamp_fastsync
 *
 * Description:  
 * This function is the interface call to update the resync end time stamp.
 *
 * Parameters: 
 * Initiator Id, source host identity, source device name, destination host 
 * identity, destination device name, resync tag time stamp, sequence time, 
 * type
 *
 * Return Value:
 * This function returns the 0(success) or 4(if update fails)
 *
 * Exceptions:
 *     <Specify the type of exception caught>
*/
function vxstub_set_resync_end_time_stamp_fastsync(
$obj,
$src_host_id,
$source_device_name,
$destination_host_id,
$destination_device_name,
$jobid,
$tag_timestamp,
$seq)
{
    $fast_sync_obj = new Fastsync();
    if (is_array($obj))
    {
        $initiator_id = $obj[host_id];
    }
    $result = $fast_sync_obj->updateResyncTimes(
        $initiator_id,
        $src_host_id,
        $source_device_name,
        $destination_host_id,
        $destination_device_name, 
        $tag_timestamp,
        $seq,
		$jobid,
        "endtime");
    return (int)$result;
}


/*
* Function Name: vxstub_set_resync_transition_step_one_to_two
*
* Description:
* This is the interface to change resync transition state from step1 to step2. 
*  
*
* Parameters: 
* Initiator identity, source host identity, source device name, destination * host 
* identity,destination device name and sync no more file path.
*    
* Return Value:
*     Retruns zero on success and returns non zero on failure. 
*
*/
function vxstub_set_resync_transition_step_one_to_two(
$obj,
$src_host_id,
$source_device_name,
$destination_host_id,
$destination_device_name,
$jobid,
$sync_nomore_file_path)
{
    $fast_sync_obj = new Fastsync();
    if (is_array($obj))
    {
        $initiator_id = $obj[host_id];
    }
    $return_status = (int) $fast_sync_obj->setResyncTransitionStepOneToTwo(
                    $initiator_id,
                    $src_host_id,
                    $source_device_name,
                    $destination_host_id,
                    $destination_device_name,
                    $sync_nomore_file_path);
    return $return_status;
}

/*
* Function Name: vxstub_get_stats_resync_time_tag_fastsync
*
* Description:
* This is the inteface to get the resync times.
*  
*
* Parameters: 
* Initiator identity, source host identity, source device name, destination host 
* identity, destination device name and sync no more file path.
*    
* Return Value:
*     Returns array of values.
*
*/
function vxstub_get_stats_resync_time_tag_fastsync( 
$obj,
$src_host_id,
$source_device_name,
$destination_host_id,
$destination_device_name,
$job_id,$field)
{
    $fast_sync_obj = new Fastsync();
    if (is_array($obj))
    {
        $initiator_id = $obj[host_id];
    }
    $retrun_status = $fast_sync_obj->getResyncTagsTime(
                    $initiator_id,
                    $src_host_id,
                    $source_device_name,
                    $destination_host_id,
                    $destination_device_name,
                    $job_id,$field);
    return $retrun_status;
}


function vxstub_get_upgrade_url( $obj ) 
{
	return "";
}

##
## Returns an array of command line arguments that are passed to the
## upgrade executable once it is executed. This is used to specify
## whether or not it should reboot.
#
function vxstub_get_upgrade_arguments( $obj ) {
	//return "-silent";
	return "";
}

/*
* Function Name: vxstub_resync_progress_update_v2
*
* Description:
* This is the interface to receive applied data and partially matched bytes from target.
* This new API introduced as part of IR/Resync stats implementation for MT to send stats along with progress. 
*
* Parameters: 
* Initiator identity, source host identity, source device name, destination host 
* identity, destination device name, unmatched bytes, partially matched bytes,
* job id and IR/Resync stats.
*    
* Return Value:
*     Returns values depending upon the outcome.
*
*/
function vxstub_resync_progress_update_v2(
$obj,
$src_host_id,
$src_device_name,
$destination_host_id,
$destination_device_name,
$unmatched_bytes,
$partial_matched_bytes,
$jobid,
$stats)
{
	global $logging_obj;
	if (is_array($obj))
	{
		$initiator_id = $obj[host_id];
	}
	#$logging_obj->my_error_handler("INFO","vxstub_resync_progress_update_v2:: Caller id: $initiator_id, SourceHostId: src_host_id, SrcDeviceName: $src_device_name, DestDeviceName: $destination_device_name, UnmatchedBytes: $unmatched_bytes,  PartialMatchedBytes: $partial_matched_bytes, Jobid: $jobid, Stats: $stats",Log::BOTH);

	$fast_sync_obj = new Fastsync();
	$return_status =  $fast_sync_obj->updateStatsResyncFromTarget(
		$src_host_id,
		$src_device_name,
		$destination_host_id,
		$destination_device_name,
		$unmatched_bytes,
		$partial_matched_bytes,
		$jobid,
		$stats);
	return (int) $return_status;
}

/*
* Function Name: vxstub_set_resync_matched_bytes_fast
*
* Description:
* This is the interface to receive fully matched bytes from source.
*  
*
* Parameters: 
* Initiator identity, source host identity, source device name, destination host 
* identity, destination device name, bytes matched, isBeforeResyncTransistion flag and job id.
*    
* Return Value:
*     Returns values depending upon the outcome.
*
*/
function vxstub_set_resync_matched_bytes_fast(
$obj, 
$src_host_id,
$src_device_name,
$destination_host_id,
$destination_device_name, 
$bytes_matched,
$is_before_resync_transition, 
$jobid)
{
	#debugLogFastSync("vxstub_set_resync_matched_bytes_fast::SrcDeviceName: $src_device_name, DestDeviceName: $destination_device_name, BytesMatched: $bytes_matched, IsBeforeResyncTransition: $is_before_resync_transition");
    if (is_array($obj))
	{
		$initiator_id = $obj[host_id];
	}

	$fast_sync_obj = new Fastsync();
	$return_status = $fast_sync_obj->updateStatsResyncFromSource(
		$src_host_id,
		$src_device_name,
		$destination_host_id,
		$destination_device_name, 
		$bytes_matched,
		$is_before_resync_transition ,
		$jobid);
	return (int) $return_status;	
}

/*
* Function Name: vxstub_set_hcd_progress_bytes_fast
*
* Description:
* This is the interface to receive size of hcd files sent from source / target.
*  
*
* Parameters: 
* Initiator identity, source host identity, source device name, destination host 
* identity, destination device name, volume offset and job id.
*    
* Return Value:
*     Returns values depending upon the outcome.
*
*/
function vxstub_set_hcd_progress_bytes_fast(
$obj,
$src_host_id,
$src_device_name,
$destination_host_id,
$destination_device_name,
$vol_offset,
$jobid)
{
	global $logging_obj, $commonfunctions_obj;
	if (is_array($obj))
	{
			$initiator_id = $obj[host_id];
	}
	
	$vol_offset_str = (string)$vol_offset;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($vol_offset) && $vol_offset != '' && (!preg_match('/^\d+$/',$vol_offset_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for vol offset $vol_offset in vxstub_set_hcd_progress_bytes_fast.");
	}
	
	$fast_sync_obj = new Fastsync();
	$return_status = $fast_sync_obj->updateHcdProgressFrom43(
		$initiator_id,
		$src_host_id,
		$src_device_name,
		$destination_host_id,
		$destination_device_name,
		$vol_offset,
		$jobid);
	
	return (int) $return_status;
}

/*
* Function Name: vxstub_set_resync_fully_unused_bytes_fast
*
* Description:
* This is the interface to receive unused bytes from target.
*  
*
* Parameters: 
* Initiator identity, source host identity, source device name, destination host 
* identity, destination device name, unused bytes and job id.
*    
* Return Value:
*     Returns values depending upon the outcome.
*
*/
function vxstub_set_resync_fully_unused_bytes_fast(
$obj,
$src_host_id,
$src_device_name,
$destination_host_id,
$destination_device_name,
$bytes_unused,
$jobid)
{
	global $logging_obj, $commonfunctions_obj;
    $obj_print = print_r($obj, TRUE);
    #debugLogFastSync("vxstub_set_resync_fully_unused_bytes_fast ::Obj: $obj_print, SrcHostId: $src_host_id, SrcDeviceName: $src_device_name, DestHostId: $destination_host_id, DestDeviceName: $destination_device_name, BytesUnused: $bytes_unused, JobId: $jobid");
    
	$bytes_unused_str = (string)$bytes_unused;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($bytes_unused) && $bytes_unused != '' && (!preg_match('/^\d+$/',$bytes_unused_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for bytes unused $bytes_unused in vxstub_set_resync_fully_unused_bytes_fast."); 
	}

	$fast_sync_obj = new Fastsync();
	$return_status = $fast_sync_obj->updateUnusedBytesFromTarget(
		$src_host_id,
		$src_device_name,
		$destination_host_id,
		$destination_device_name, 
		$bytes_unused,
		$jobid);
	return (int) $return_status;    
}

/*
* Function Name: get_lun_id
*
* Description:
* This Function will return lun id on the basis of given id and device  name.
*
* Parameters: 
* source host identity, source device name,
*    
* Return Value:
*     integer.
*/
function get_lun_id($srcId, $srcDevice, $dstId, $dstDevice)
{
	global $conn_obj;
	$result = array();
	$sql_lun_info="SELECT 
						Phy_Lunid, lun_state 
				   FROM 
						srcLogicalVolumeDestLogicalVolume 
				   WHERE 
						sourceHostId = '$srcId' 
				   and 
						sourceDeviceName = '$srcDevice' 
				   and 
						destinationHostId = '$dstId'
				   and
						destinationDeviceName = '$dstDevice'					
				   limit 0,1";
		   
	debugLog("sql_lun_info----> ". $sql_lun_info."\n");			   
	$result_info=$conn_obj->sql_query($sql_lun_info);
	$row = $conn_obj->sql_fetch_object($result_info);
	$result = array("Phy_Lunid" => $row->Phy_Lunid, "lun_state" => $row->lun_state);
	return $result;
}

/*
 * Function Name: vxstub_update_pending_data_on_target
 *
 * Description:
 *
 *    This function updates the pending amount of data on target 
 *	 
 * Parameters:
 *    Param 1 [IN]:$obj
 *    Param 2 [IN]:$result
 *
 * Return Value:
 *    Ret Value: Returns boolean value
 *
 * Exceptions:
 *    DataBase Connection fails.
*/
function vxstub_update_pending_data_on_target($obj,$result)
{
	global $conn_obj;
	global $logging_obj;
	$host_Id = $obj['host_id'];
	
	if(count($result) > 0)
	{
		foreach($result as $drive => $data)
		{
			$update_sql = "UPDATE 
							srcLogicalVolumeDestLogicalVolume 
						   SET 	
							differentialPendingInTarget = ? 
					       WHERE 
							destinationHostId = ? AND 
							destinationDeviceName = ?";
			$flag = $conn_obj->sql_query($update_sql, array($data, $host_Id, $drive));
		}
	}
	
	return TRUE;
}

function vxstub_pause_replication_from_host($obj, $deviceName, $type)
{
	global $conn_obj;
	global 	$logging_obj;
	global $vol_obj, $MARS_NON_RETRYABLE_ERRORS_BUCKET, $MARS_NON_RETRYABLE_ERRORS_HF_MAP;
	global $commonfunctions_obj, $SOURCE_DATA, $TARGET_DATA;
	$return_status = FALSE;
	$logging_obj->my_error_handler("INFO","vxstub_pause_replication_from_host: ".array("HostId"=>$obj['host_id'],"DeviceName"=>$deviceName,"type"=>$type),Log::BOTH);
	if($type) {
		/*
			$type = id=80A3E1CE-23CD-DD4D-BC15F1DEE6B968EA&vol=F&hosttype=1&err=Replication is paused to allow snapshot/recovery operation
		*/
		/* Target sample sending string:
		DEVICENAME::C:\a81fd777-47bd-406c-983f-9afaa8ae4fcc\{1495818192}, type: id=8A3EBDBD-6042-AF43-BA0D051CD617B5D6&vol=C:\a81fd777-47bd-406c-983f-9afaa8ae4fcc\{1495818192}&hosttype=1&err=Replication for C:\a81fd777-47bd-406c-983f-9afaa8ae4fcc\{1495818192}is paused due to unrecoverable error(-2139552509).&errCode=-2139552509
		*/
		$str = explode('&',$type);
		$hosttype = $str[2];
		$hosttype = explode('=',$hosttype);
		$type = $hosttype[1];
		$err_code_data = $str[4];
		if ($err_code_data)
		{
			$err_code = explode('=',$err_code_data);
			$errorCode = dechex($err_code[1]);
		}
	} else {
		return (bool)$return_status;
	}

	if (!in_array($type, array(1,2))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for host type $type in vxstub_pause_replication_from_host.");
	}
	
	if($type == 1)
	{
		$valid_protection = $commonfunctions_obj->isValidProtection($obj['host_id'], $deviceName, $TARGET_DATA);
		if (!$valid_protection)
		{
			$logging_obj->my_error_handler("INFO","Invalid post data for target details $deviceName in vxstub_pause_replication_from_host.",Log::BOTH);
			//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
			return TRUE;
		}	
	}
	
	if($type == 2)
	{
		$valid_protection = $commonfunctions_obj->isValidProtection($obj['host_id'], $deviceName, $SOURCE_DATA);
		if (!$valid_protection)
		{
			$logging_obj->my_error_handler("INFO","Invalid post data for source details $deviceName in vxstub_pause_replication_from_host.",Log::BOTH);
			//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
			return TRUE;;
		}	
	}
	
	$source_device_name = $conn_obj->sql_escape_string ($deviceName);
	if($type == 2) {
		$host_id = $obj['host_id'];
		$query =	"SELECT
							destinationHostId,
							destinationDeviceName
						FROM
							srcLogicalVolumeDestLogicalVolume
						WHERE
							sourceHostId = '$host_id'
						AND
							 sourceDeviceName = '$source_device_name'"; 			

		$dest_result_set = $conn_obj->sql_query($query);
		$dest_details_obj = $conn_obj->sql_fetch_object($dest_result_set);

		$dest_host_id = $dest_details_obj->destinationHostId;
		$dest_device = $dest_details_obj->destinationDeviceName;
		$dest_device_name = $conn_obj->sql_escape_string ($dest_device);
	} else {
		$dest_host_id = $obj['host_id'];
		$dest_device_name = $conn_obj->sql_escape_string ($deviceName);
	}
	$query =	"SELECT
					sourceHostId,
					sourceDeviceName,
					destinationHostId,
					destinationDeviceName,
					pairId 
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					destinationHostId = '$dest_host_id'
				AND
					 destinationDeviceName = '$dest_device_name'"; 							
	$result_set = $conn_obj->sql_query($query);
	
	while ($row_object = $conn_obj->sql_fetch_object($result_set))
	{
		$source_host_id = $row_object->sourceHostId;
		$source_device = $row_object->sourceDeviceName;
		$dest_host_id = $row_object->destinationHostId;
		$dest_device = $row_object->destinationDeviceName;
		$pair_id = $row_object->pairId;
		if($type == 2) 
		{
			$vol_obj->pause_replication(
			$source_host_id,
			$source_device,
			$dest_host_id,
			$dest_device,
			1);
		} 
		else 
		{
			$vol_obj->pause_replication(
			$source_host_id,
			$source_device,
			$dest_host_id,
			$dest_device,
			3,1,"general pause");
			
			if ($errorCode)
			{
				if (array_key_exists($errorCode,$MARS_NON_RETRYABLE_ERRORS_BUCKET))
				{						
					//Add the alert generation code block here
					/*a:10:{i:0;s:78:"a:2:{i:0;s:12:"::getVxAgent";i:1;s:36:"d6276c79-f32a-4475-9345-0ca5f74711b0";}";i:1;s:14:"updateAgentLog";i:2;s:22:"(2016-11-11 16:08:26):";i:3;s:5:"ALERT";i:4;s:2:"VX";i:5;s:242:"consistency command failed because of driver in non data mode. Command="/usr/local/ASR/Vx///bin/vacp" -systemlevel  -distributed -mn 10.150.209.247 -cn 10.150.209.249 -dport 20004 -prescript "/usr/local/ASR/Vx//scripts/vCon/AzureDiscovery.sh"";i:6;i:10;i:7;i:2;i:8;s:6:"EA0429";i:9;a:2:{s:11:"FailedNodes";s:158:" nichougu-0624-lin-1, 10.150.209.247, VACP_E_DRIVER_IN_NON_DATA_MODE [10003]#!# nichougu-0624-lin-2, 10.150.209.249, VACP_E_DRIVER_IN_NON_DATA_MODE [10003]#!#";s:8:"PolicyId";s:3:"120";}}*/
					
					$cx_logger = 0;
					$hostid = $obj['host_id'];
					$log_level = "ALERT";
					$agent_info = "VX";
					$error_msg = "";
					$module = 10;
					$alert_type = 2;
					$error_code = $MARS_NON_RETRYABLE_ERRORS_BUCKET[$errorCode];
					$error_placeholders['SourceVmHostId'] = $source_host_id;
					$ttime = "(".date("Y-m-d H:i:s")."):"; 
					$hf_description = "RESYNC PROGRESS STUCK";
					$hf_code = $MARS_NON_RETRYABLE_ERRORS_HF_MAP[$errorCode];
					switch ($error_code) 
					{						
						case "EA0432":
							$error_msg = "MT_E_STORAGE_NOT_FOUND";
							break;
						case "EA0433":
							$error_msg = "MT_E_REPLICATION_BLOCKED";
							break;
						default:
					}
					$commonfunctions_obj->process_agent_error($hostid,$log_level,$error_msg,$ttime,$module,$alert_type,$cx_logger,$error_code,$error_placeholders);
					$sql = "REPLACE INTO healthfactors(sourceHostId, destinationHostId, pairId, errCode, healthFactorCode, healthFactor, priority, component, healthDescription, healthTime) VALUES ('$source_host_id', '$dest_host_id', $pair_id,'$error_code','$hf_code',2,0,'MT', '$hf_description',now())";
					$query_status = $conn_obj->sql_query($sql);
				}
			}
		}
		if (isset($update_return_status) and $query_status)
		{
			if ($update_return_status and $query_status)
			{
				$return_status = TRUE;
			}
			else
			{
				$return_status = FALSE;
			}
		}
		else
		{
			$return_status = TRUE;
		}
	}
	return (bool)$return_status;
}

function vxstub_resume_replication_from_host($obj, $deviceName, $type)
{
	global $conn_obj;
	global $logging_obj;
	global $vol_obj;
	global $commonfunctions_obj, $TARGET_DATA, $SOURCE_DATA; 
	$return_status = FALSE;
	$logging_obj->my_error_handler("INFO",array("HostId"=>$obj['host_id'],"SourceDeviceName"=>$deviceName,"Type"=>$type),Log::BOTH);
	
	if($type) {
		/*
			$type = id=80A3E1CE-23CD-DD4D-BC15F1DEE6B968EA&vol=F&hosttype=1&err=Replication is paused to allow snapshot/recovery operation
		*/
		$str = explode('&',$type);
		$hosttype = $str[2];
		$hosttype = explode('=',$hosttype);
		$type = $hosttype[1];
	}
	else {
		return (bool)$return_status;
	}
		
	$logging_obj->my_error_handler("INFO","Entered vxstub_resume_replication_from_host TYPE MODIFIED::$type");
	if (!in_array($type, array(1,2))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for host type $type in vxstub_resume_replication_from_host.");
	}
	
	if($type == 1)
	{
		$valid_protection = $commonfunctions_obj->isValidProtection($obj['host_id'], $deviceName, $TARGET_DATA);
		if (!$valid_protection)
		{
			$logging_obj->my_error_handler("INFO","Invalid post data for target details $deviceName in vxstub_resume_replication_from_host.",Log::BOTH);
			//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
			return TRUE;
		}	
	}
	
	if($type == 2)
	{
		$valid_protection = $commonfunctions_obj->isValidProtection($obj['host_id'], $deviceName, $SOURCE_DATA);
		if (!$valid_protection)
		{
			$logging_obj->my_error_handler("INFO","Invalid post data for source details $deviceName in vxstub_resume_replication_from_host.",Log::BOTH);
			//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
			return TRUE;
		}	
	}
	if($type == 2) {
		$host_id = $obj['host_id'];
		
		$query =	"SELECT
							destinationHostId,
							destinationDeviceName
						FROM
							srcLogicalVolumeDestLogicalVolume
						WHERE
							sourceHostId = ?
						AND
							 sourceDeviceName = ?"; 			

		$dest_result_set = $conn_obj->sql_query($query, array($host_id, $deviceName));
		foreach($dest_result_set as $rs)
		{
			$dest_host_id = $rs['destinationHostId'];
			$dest_device = $rs['destinationDeviceName'];
			$dest_device_name = $conn_obj->sql_escape_string($dest_device);
            break;
		}
	}
	else {
		$dest_host_id = $obj['host_id'];
		$dest_device_name = $conn_obj->sql_escape_string($deviceName);
	}
			
	$query =	"SELECT
					sourceHostId,
					sourceDeviceName,
					destinationHostId,
					destinationDeviceName
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					destinationHostId = '$dest_host_id'
				AND
					 destinationDeviceName = '$dest_device_name'"; 							
	$result_set = $conn_obj->sql_query($query);
	
	while ($row_object = $conn_obj->sql_fetch_object($result_set))
	{
		$source_host_id = $row_object->sourceHostId;
		$source_device = $row_object->sourceDeviceName;
		$dest_host_id = $row_object->destinationHostId;
		$dest_device = $row_object->destinationDeviceName;
		if($type == 2) {
			$vol_obj->resume_replication(
			$source_host_id,
			$source_device,
			$dest_host_id,
			$dest_device,
			1);
		} else {
			$vol_obj->resume_replication(
			$source_host_id,
			$source_device,
			$dest_host_id,
			$dest_device,
			3,1);
		}
		$return_status = TRUE;
	}
	return (bool)$return_status;
}

/*
 * Function Name: vxstub_get_install_details
 *
 * Description:
 *
 *    This function returns the installation details for a 
 *    push install job.     
 *	 
 * Parameters:
 *    Param 1 [IN]:vxstub object
 *    Param 2 [IN]:job id 
 *
 * Return Value:
 *    Ret Value: Returns a structure of installation 
 *    details for a given host.
 *
 * Exceptions:
 */
function vxstub_get_install_details($obj,$job_id,$host_id,$os_type) {
    
    global $logging_obj;
	$pushinstall_obj = new PushInstall();
    
    $telemetry_data = $pushinstall_obj->get_telemetry_flag($job_id);	
	
	$telemetry_log_string["JobId"]=$job_id;
	$telemetry_log_string["HostId"]=$host_id;
	$telemetry_log_string["OsType"]=$os_type;
	$telemetry_log_string["StartTime"]=$telemetry_data['startTime'];
	$telemetry_log_string["LastUpdatedTime"]=$telemetry_data['lastUpdatedTime'];
	$telemetry_log_string["StatusMessage"]=$telemetry_data['statusMessage'];
	$logging_obj->my_error_handler("INFO",$telemetry_log_string,Log::APPINSIGHTS);
	
    $str = $pushinstall_obj->get_install_details($job_id,$host_id,$os_type);
    
	$logging_obj->my_error_handler("INFO",array("VxstubGetInstallDetails"=>$str,"Status"=>"Success"),Log::APPINSIGHTS);
	    
    return $str;
}

/*
 * Function Name: vxstub_update_agent_installation_status
 *
 * Description:
 *
 *    This function updates the installation status of agent
 *    via the push proxy host
 *	 
 * Parameters:
 *    Param 1 [IN]:vxstub object
 *    Param 2 [IN]:job id 
 *    Param 3 [IN]:state
 *    Param 4 [IN]:status message of installation
 *    Param 5 [IN]:installation log
 *
 * Return Value:
 *    Ret Value: Returns boolean value
 *
 * Exceptions:
 */
function vxstub_update_agent_installation_status ( $obj,
						   $job_id,
						   $state,
						   $install_status_message,
						   $host_id="",
						   $log_message="",
						   $error_code="",
						   $error_place_holders=array(),
						   $installer_extended_errors=""
					   	  )
{
	global $logging_obj;
	global $conn_obj;
	$flag = 0;	
	$pushinstall_obj = new PushInstall();
    $conn_obj->enable_exceptions();
	try
	{
		$conn_obj->sql_begin();	
		
    	$str = $pushinstall_obj->update_agent_installation_status($job_id,
								  $state,
								  $install_status_message,
								  $host_id,
								  array(
									'message' => $log_message,
									'errorCode' => $error_code,
									'placeHolders' => $error_place_holders,
									'installerExtendedErrors' => $installer_extended_errors
								  ));
		$conn_obj->sql_commit();
	}
	catch(SQLException $e)
	{
		$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>$result),Log::BOTH);
		$conn_obj->sql_rollback();
		$flag = 1;
	}
	$conn_obj->disable_exceptions();
	$logging_obj->my_error_handler("DEBUG","vxstub_update_agent_installation_status return string:: $str ");
	if($flag == 1)
	{
		$GLOBALS['http_header_500'] = TRUE;
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Result"=>500),Log::BOTH);
		header('HTTP/1.0 500 InMage method call returned an error');
		exit;
	}
	return $str;
}
/*
 * Function to return push install settings to agent
 */
function vxstub_get_install_hosts ($obj)
{
    global $logging_obj;
	$jobIds = array();
	$pushinstall_obj = new PushInstall();
    //$pushinstall_obj->HGPLog("vxstub_get_install_hosts call");
    $result = $pushinstall_obj->get_install_hosts($obj);
    //$pushinstall_obj->HGPLog(print_r($result, TRUE));
	$output = $result;
	if(is_array($output[3]))
	{
		if(count($output[3] > 0))
		{
			foreach($output[3] as $hostsData)
			{
				if(is_array($hostsData[5]))
				{
					foreach($hostsData[5] as $info)
					{
						$telemetry_data = $pushinstall_obj->get_telemetry_flag($info[4]);	
						if((!$telemetry_data['telemetry']) && 
							(strtotime($telemetry_data['startTime']) == strtotime($telemetry_data['lastUpdatedTime'])) && 
							($telemetry_data['statusMessage'] == ""))
						{
							array_push($jobIds,$info[4]);
							
							$retArr[] = array($hostsData[0],$hostsData[1],"*****","*****",$hostsData[4],
							$info[0],$info[1],$info[2],$info[3],$info[4],$info[5],$info[6],"*****","*****",
							$info[9],$info[10],$info[11]);
						}
					}
				}
		   }
	   }
	}
	if(is_array($jobIds) && (count($jobIds) > 0))
	{
		$pushinstall_obj->update_telemetry_flag($jobIds);
		$logging_obj->my_error_handler("INFO",array("VxstubGetInstallHosts"=>$retArr,"Status"=>"Success"),Log::APPINSIGHTS);	
	}
	
    return $result;
}

function vxstub_get_push_client_path($obj,$osName,$jobid)
{
    global $logging_obj;
	$pushinstall_obj = new PushInstall();
    $result = $pushinstall_obj->get_push_client_path($osName,$jobid);
	$logging_obj->my_error_handler("INFO",array("LogMessage"=>$result,"Status"=>"Success"),Log::APPINSIGHTS);
	
    return $result;
}

function vxstub_should_throttle_fast( $resyncVersion, $throttle_resync, $throttle_diff, $lun_state, $ps_throttle , $license_expired_source, $license_expired_target, $src_device) 
{   
	global $REPLICATION_ROOT, $ORIGINAL_REPLICATION_ROOT;
	global $PROTECTED,$REBOOT_PENDING;
	global $conn_obj;
	global 	$logging_obj;
	$result = array();
	$pslevel_cummulative_throttling = 0;
	$delete_pending = 0;			
	$throttle_settings = "";	

	$file_count_check = $throttle_resync;		
	$diff_file_count_check = $throttle_diff;

	$pslevel_cummulative_throttling = $ps_throttle;	
	
	$resync_throttle   =  $pslevel_cummulative_throttling  | $file_count_check | $license_expired_source | $delete_pending;        
	$sentinel_throttle =  $pslevel_cummulative_throttling  | $license_expired_source | $diff_file_count_check | $delete_pending;	
	$throttle_settings .= $src_device.":,"."$resync_throttle,$sentinel_throttle,0;";		
	$outpost_throttle = $license_expired_target | $delete_pending;
	$result[0] = array(0,(bool)$resync_throttle,(bool)$sentinel_throttle,(bool)0);
	$result[1] = array(0,(bool)0,(bool)0,(bool)$outpost_throttle);
			
	//$logging_obj->my_error_handler("DEBUG","vxstub_should_throttle_fast:: return value is".print_r($result,TRUE)."\n");
	return $result;
				
}

function vxstub_restart_replication_update($obj, $devicename, $error_status, $error_message, $action="stopdownload")
{	
	global $conn_obj, $logging_obj, $RESTART_RESYNC_CLEANUP;
	$update_status = 0;
	$hostid= $obj['host_id'];
	
	$logging_obj->my_error_handler("INFO",array("HostId"=>$hostid, "DestinationDeviceName"=>$devicename,"ErrorStatus"=>$error_status, "ErrorMessage"=>$error_message, "Action"=>$action),Log::BOTH);
	
	if($error_status == TRUE)
	{		
		$rep_options = "1";
		$sql = "UPDATE 
					srcLogicalVolumeDestLogicalVolume
				SET 					
					replicationCleanupOptions = '$rep_options'				
				WHERE 
					destinationHostId = ? 
				AND
					destinationDeviceName = ?
				AND 
					replication_status = ?";
		
		$update_status = $conn_obj->sql_query($sql, array($hostid,$devicename,$RESTART_RESYNC_CLEANUP));
		
		$logging_obj->my_error_handler("DEBUG","vxstub_restart_replication_update:: update query is $sql & its status returned as $update_status \n");
	}
	
	$logging_obj->my_error_handler("INFO",array("Result"=>TRUE,"Status"=>"Success"),Log::BOTH);
	return TRUE;
}

function get_cs_transport_protocol()
{
    global $conn_obj;
    global $logging_obj;
    
    $sql = "SELECT id FROM transportProtocol WHERE name='HTTP'";
    $rs = $conn_obj->sql_query($sql);
    
    if (!$rs) {
        $logging_obj->my_error_handler("ERROR", "get_cs_transport_protocol failed".$conn_obj->sql_error($rs)."\n");
        return array(0);
    }
    
    if (0 == $conn_obj->sql_num_row($rs)) {
        $logging_obj->my_error_handler("ERROR", "get_cs_transport_protocol cs information not found\n");
        return array(0);                
    }
    
    $result_set = $conn_obj->sql_fetch_array($rs);
    
    return intval($result_set['id']);
}

/*
 * Function Name: get_cs_transport_connection_settings
 *
 * Description:
 *     this function returns the transport connection settings of the cs
 *  
 * Parameters:
 *     none
 *     
 * Return Value:
 *     array with transport connection details like ip, port, user credentials etc.
 *
 * Exceptions:
 *
 */
function get_cs_transport_connection_settings($host_id="")
{
    global $conn_obj;
    global $logging_obj;
    global $CXPS_USER_WINDOWS;
    global $CXPS_PASSWORD_WINDOWS;
    global $CXPS_USER_LINUX;
    global $CXPS_PASSWORD_LINUX;
    global $CXPS_CONNECTTIMEOUT;
    global $CXPS_RESPONSETIMEOUT;   
    global $commonfunctions_obj;
	global $CS_IP;
    
    $cxps_values = $commonfunctions_obj->cxpsconf_values();
    
    $cs_ip = $CS_IP;
    
    $port = $cxps_values['port'];
    $port = trim(str_replace('"','',$port));
    
    $ssl_port = $cxps_values['ssl_port'];
    $ssl_port = trim(str_replace('"','',$ssl_port));
    $select_sql = "SELECT
						usecxnatip,
						cx_natip
					FROM
						hosts
					WHERE
						id = '$host_id'";
	$rs = $conn_obj->sql_query($select_sql);
	$rs_obj = $conn_obj->sql_fetch_object($rs);
	$cs_ip = ($rs_obj->usecxnatip)?$rs_obj->cx_natip:$CS_IP;	
	
    if (preg_match('/Windows/i', php_uname())) {
        $user = $CXPS_USER_WINDOWS;
        $password = $CXPS_PASSWORD_WINDOWS;
    } else {
        $user = $CXPS_USER_LINUX;
        $password = $CXPS_PASSWORD_LINUX;
    }    
    
    return array (0,
                  $cs_ip,              // ip address
                  $port,               // port
                  $ssl_port,           // sslPort
                  "0",            
                  $user,
                  $password,
                  $CXPS_CONNECTTIMEOUT, 
                  $CXPS_RESPONSETIMEOUT,
                  (bool)1);    
}

function get_host_transport_connection_settings($id, $vol_grp_id, $host_type,$final_hash=array())
{ 
    global $conn_obj;
    global $logging_obj;
    global $CXPS_USER_WINDOWS;
    global $CXPS_PASSWORD_WINDOWS;
    global $CXPS_USER_LINUX;
    global $CXPS_PASSWORD_LINUX;
    global $CXPS_CONNECTTIMEOUT;
    global $CXPS_RESPONSETIMEOUT;    
	
	$ps_nic_ip = '';
	
	$pair_array = $final_hash[0];
	$host_array = $final_hash[2];
	$ps_array = $final_hash[5];
	$nic_array = $final_hash[10];
	
	if (1 == $host_type) {
        $hostIdColName = "sourceHostId";
    } else {
        $hostIdColName = "destinationHostId";
    } 
	foreach($pair_array as $ruleId =>$pairArray)
	{
		if(($pairArray[$hostIdColName] == $id) && ($pairArray['volumeGroupId'] ==  $vol_grp_id))
		{
			$processServerId = $pairArray['processServerId'];
			$use_ps_nat_source = $pairArray['usePsNatIpForSource'];
			$use_ps_nat_target = $pairArray['usePsNatIpForTarget'];
		}
	}
	
	#13656 Fix

    if (1 == $host_array[$processServerId]['osFlag']) {
        $user = $CXPS_USER_WINDOWS;
        $password = $CXPS_PASSWORD_WINDOWS;
    } else {
        $user = $CXPS_USER_LINUX;
        $password = $CXPS_PASSWORD_LINUX;
    }
	if(($use_ps_nat_source && $host_type == 1) || ($use_ps_nat_target && $host_type == 2))
	{
		$use_ps_nat = $ps_array[$processServerId]['ps_natip'];
	}
	else
	{
		$use_ps_nat = $ps_array[$processServerId]['ipaddress'];
	}
	
    return array (0,
                  $use_ps_nat,							  // ip address
                  $ps_array[$processServerId]['port'],                                // port
                  $ps_array[$processServerId]['sslPort'],                                // sslPort
                  "0",                               
                  $user,
                  $password,
                  $CXPS_CONNECTTIMEOUT, 
                  $CXPS_RESPONSETIMEOUT,
                  (bool)1);    
}

function vxstub_get_application_settings($obj,$policy_list)
{
	global $logging_obj;
	global $conn_obj;
	global $commonfunctions_obj;
	$host_id = $obj['host_id'];
	
	$result = array();
	$app_obj = new Application();
	$policy_obj = new Policy();
	
	if($host_id == NULL)
	{
		$logging_obj->my_error_handler("INFO",array("HostId"=>NULL),Log::BOTH);
		return $result;
	}
	
	$host_id_exists = $commonfunctions_obj->isValidComponent($host_id);	
	if($host_id_exists == FALSE)
	{
		$logging_obj->my_error_handler("INFO",array("HostId"=>"Not registered"),Log::BOTH);
		return $result;
	}
	
	$passive_cx = array();
	$time_out = "0";
	
	$result =$app_obj->vxstubGetApplicationSettings($host_id,$policy_list,$passive_cx,$time_out);
	$update_app_agent_hreatbeat_sql = "UPDATE 
											hosts 
										SET 
											lastHostUpdateTimeApp = now(),
											appAgentEnabled = '1'
										WHERE 
											id = '$host_id' AND 
											sentinelEnabled = 1 AND
											outpostAgentEnabled = 1";
	$conn_obj->sql_query( $update_app_agent_hreatbeat_sql );
    
	return $result;
}

/*
Error code:-
POLICY_UPDATE_COMPLETED = 0,
POLICY_UPDATE_FAILED = 1,
POLICY_UPDATE_DELETED = 2,
POLICY_UPDATE_DUPLICATE = 3
*/
function vxstub_update_policy($obj,$policy_id,$policy_instance_id,$status,$log)
{
	global $logging_obj, $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
	global $conn_obj;
	$app_obj = new Application();
	$commonfunctions_obj = new CommonFunctions();
	$flag = 0;
	$host_id = $obj['host_id'];
	$status = trim($status);
	$policy_instance_id = trim($policy_instance_id);
	$policy_id = trim($policy_id);
	$status = strtolower($status);
	if($status == "success")
	{
		$status = 1; //success
	}
	elseif($status == "inprogress")
	{
		$status = 4; //inprogress
	}
	elseif($status == "skipped")
	{
		$status = 5; //Skipped
	}
	else
	{
		$status = 2; //failed
	}
	
	$policy_id_str  = (string)$policy_id;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($policy_id) && $policy_id != '' && (!preg_match('/^\d+$/',$policy_id_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for policy id $policy_id in vxstub_update_policy.");
	}
	
	$policy_instance_id_str  = (string)$policy_instance_id;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($policy_instance_id) && $policy_instance_id != '' && (!preg_match('/^\d+$/',$policy_instance_id_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for policy instance id $policy_instance_id in vxstub_update_policy.");
	}
	
	$sql = "SELECT
				policyType,
				policyScheduledType,
				scenarioId
			FROM
				policy
			WHERE
				policyId = '$policy_id'";
	$rs = $conn_obj->sql_query($sql);
	$row = $conn_obj->sql_fetch_object($rs);
	$policy_exists = $conn_obj->sql_num_row($rs);
	$policy_type = $row->policyType;
	$policy_sceduled_type = $row->policyScheduledType;
	$scenarioId = $row->scenarioId;
	if ( $policy_type != 4)
	{
		$logging_obj->my_error_handler("INFO",array("PolicyId"=>$policy_id,"PolicyInstanceId"=>$policy_instance_id,"Status"=>$status,"log"=>$log),Log::BOTH);
	}
	$sel_solution_type = "SELECT
								solutionType,
								aps.scenarioDetails,
								aps.protectionDirection,
								aps.targetVolumes
							FROM
								applicationPlan ap,
								applicationScenario aps
							WHERE
								aps.scenarioId = '$scenarioId'
								AND
								aps.planId = ap.planId";
	$logging_obj->my_error_handler("DEBUG","vxstub_update_policy sql ::$sel_solution_type");	
	$result_solution_type = $conn_obj->sql_query($sel_solution_type);
	$result_row = $conn_obj->sql_fetch_object($result_solution_type);
	$solution_type = $result_row->solutionType;
	$scenario_details = $result_row->scenarioDetails;
	if($solution_type != $CLOUD_PLAN_AZURE_SOLUTION && $solution_type != $CLOUD_PLAN_VMWARE_SOLUTION) {
		$cdata_arr = $commonfunctions_obj->getArrayFromXML($scenario_details);
		$scenario_details = unserialize($cdata_arr['plan']['data']['value']);
	}else {
		$scenario_details = unserialize($scenario_details);
	}
	$forward_solution_type = $scenario_details['forwardSolutionType'];
	$logging_obj->my_error_handler("DEBUG","vxstub_update_policy sql ::$solution_type \n status::$status");	
	/*	
	 *Below if conditions is to send fail response to app agent with 500 exception eventhough app agent sends prepare target status as success when source volumes are not registered to CX
	 *This if will executed when the below conditions matches
	 *1)current protection direction is reverse
	 *2)policy type is prepare target
	 *3)solution type is not prism
	 *4)current target is clusterd in forward direction
	*/
	if(($status == 1) && ($policy_type == 5) && isset($scenario_details['srcIsClustered']) && ($scenario_details['srcIsClustered']) && ($scenario_details['applicationType'] != 'ORACLE') && ($result_row->protectionDirection == 'reverse') )
	{
		$targetVolumes = $conn_obj->sql_escape_string($result_row->targetVolumes);
		$targetVolumesArray = explode(",",$targetVolumes);
		$target_devices = implode("','",$targetVolumesArray);
		
		$select_sql = "SELECT
							hostId
						FROM
							logicalVolumes
						WHERE
							hostId = '$host_id'
						AND
							deviceName IN('$target_devices')";
							
		$result = $conn_obj->sql_query($select_sql);	
		$count = $conn_obj->sql_num_row($result);
		$logging_obj->my_error_handler("INFO",array("Sql"=>$select_sql,"policy_type"=>$policy_type, "TargetVolumes"=>$targetVolumes, "Count"=>$count, "TargetVolumesArray"=>$targetVolumesArray),Log::BOTH);
		if(count($targetVolumesArray) != $count)	
		{
			$logging_obj->my_error_handler("INFO",array("Status"=>"Fail", "Result"=>500, "Reason"=>"Prepare Target Failed"),Log::BOTH);
			$GLOBALS['http_header_500'] = TRUE;
			header('HTTP/1.0 500 InMage method call returned an error',TRUE,500 );
			header('Content-Type: text/plain');
			print "Volumes are not registred , hence sending prepare target as failed";
			return 1;
		}
	}
	
	if($policy_exists)
	{
		//policy inprogress
		if($status == 4)
		{
			if ($solution_type == 'PRISM' || $forward_solution_type == 'PRISM')
			{
				if ($policy_type == 2  || $policy_type == 16 || $policy_type == 3 || $policy_type == 5)
				{
					$cond = " AND hostId = '$host_id'";
				}	
			}
			else
			{
				$cond = "";
				if($policy_type == 4) 
				{
					$cond = " AND hostId = '$host_id'";
				}
			}
			$sql = "SELECT
						policyInstanceId
					FROM
						policyInstance
					WHERE
						policyId = '$policy_id' AND
						uniqueId = '$policy_instance_id' AND
						(policyState = '3' OR policyState = '4') $cond";
			if ( $policy_type != 4)
			{
				$logging_obj->my_error_handler("INFO",array("Sql"=>$sql),Log::BOTH);
			}
			$rs = $conn_obj->sql_query($sql);
			$num_rows = $conn_obj->sql_num_row($rs);
			if(!$num_rows)
			{
				if($policy_sceduled_type == 1)
				{
					if ((($solution_type == 'PRISM' || $forward_solution_type == 'PRISM') && (($policy_type == 2 ) || ($policy_type == 3 )))  || ($policy_type == 4))
					{
						$sql = "INSERT INTO
									policyInstance
									(
										policyId,
										lastUpdatedTime,
										policyState,
										uniqueId,
										hostId
									)
									VALUES
									(
										'$policy_id',
										now(),
										'$status',
										'$policy_instance_id',
										'$host_id'
									)";
							
							
					}
					else
					{
						$sql = "INSERT INTO
								policyInstance
								(
									policyId,
									lastUpdatedTime,
									policyState,
									uniqueId
								)
								VALUES
								(
									'$policy_id',
									now(),
									'$status',
									'$policy_instance_id'
							)";	
							
					}
					$rs = $conn_obj->sql_query($sql);
				}	
				else
				{
					$conn_obj->enable_exceptions();
					try
					{
						$conn_obj->sql_begin();
						$consistency_group_exists = $conn_obj->sql_get_value("consistencyGroups","count(*)","policyId='$policy_id'");
						if($consistency_group_exists && ($policy_type == 45 || $policy_type == 4))
						{
							$cond = "AND hostId='$host_id'";
						}
						$logging_obj->my_error_handler("INFO",array("HostId"=>$host_id,"PolicyId"=>$policy_id,"PolicyInstanceId"=>$policy_instance_id,"Status"=>$status,"Log"=>$log, "ConsistencyGroupExists"=>$consistency_group_exists, "PolicyType"=>$policy_type),Log::BOTH);
						$sql = "SELECT
									policyInstanceId
								FROM
									policyInstance
								WHERE
									policyId = '$policy_id' AND
									(policyState = '3' OR policyState = '4') $cond";
						$logging_obj->my_error_handler("DEBUG","vxstub_update_policy sql ::$sql");			
						$rs = $conn_obj->sql_query($sql);
						while($row = $conn_obj->sql_fetch_object($rs))
						{
							$instance_id = $row->policyInstanceId;
						}					
							
						$sql = "UPDATE
									policyInstance
								SET
									policyState = '$status',
									uniqueId = '$policy_instance_id',
									lastUpdatedTime = now()
								WHERE
									policyId = '$policy_id' AND
									policyInstanceId = '$instance_id' $cond";
						$rs = $conn_obj->sql_query($sql);			
						$logging_obj->my_error_handler("DEBUG","SQL2::$sql");
						$logging_obj->my_error_handler("INFO","vxstubUpdatePolicy :: $sql");
						$conn_obj->sql_commit();
					}	
					catch(Exception $e)
					{
						$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK", "HostId"=>$host_id, "PolicyId"=>$policy_id, "PolicyInstanceId"=>$policy_instance_id,"Status"=>$status, "Log"=>$log,"Result"=>1),Log::BOTH);
						$conn_obj->sql_rollback();
						$flag = 1;
					}
					$conn_obj->disable_exceptions();	
				}
			}
			else
			{
				if($policy_sceduled_type == 1)
				{
					$sql = "UPDATE
								policyInstance
							SET
								policyState = '$status',
								uniqueId = '$policy_instance_id',
								lastUpdatedTime = now()
							WHERE
								policyId = '$policy_id' AND
								uniqueId = '$policy_instance_id'";
					$rs = $conn_obj->sql_query($sql);
				}
				else
				{
					$flag = 3;
				}
			}
		}
		else
		{
			$cond = "";
			if($policy_type == 4) 
			{
				$cond = " AND hostId = '$host_id'";
			}
			
			$sql = "SELECT
						policyInstanceId,
						policyState 
					FROM
						policyInstance
					WHERE
						policyId = '$policy_id' AND
						uniqueId = '$policy_instance_id' 
						 $cond";
			
			$rs = $conn_obj->sql_query($sql);
			$num_rows = $conn_obj->sql_num_row($rs);
			$policy_instance_id_row = $conn_obj->sql_fetch_object($rs);
			$policy_instance_id_primary = $policy_instance_id_row->policyInstanceId;
			$policy_state = $policy_instance_id_row->policyState;
			
			//Only policies from state inProgress to update to "Success/Failed".
			if ($policy_state == 4)
			{
				$app_obj = new Application();
				$recovery_plan_obj = new RecoveryPlan();
								
				if($policy_type == 12 || $policy_type == 13)
				{
					$log_arr = array();
					if($policy_type == 12)
					{
						$new_volpack_volumes = '';
						$log_arr = unserialize($log);
						if($log != '')
						{
							$sql = "SELECT
										DISTINCT
										scenarioId
									FROM
										policy
									WHERE
										policyId='$policy_id'";
							
							
							$rs = $conn_obj->sql_query($sql);
							$row = $conn_obj->sql_fetch_object($rs);
							$scenario_id = $row->scenarioId;
							
							$sql = "SELECT
										DISTINCT
										scenarioDetails,
										sourceVolumes
									FROM
										applicationScenario
									WHERE
										scenarioId='$scenario_id'";
							
							$rs = $conn_obj->sql_query($sql);
							$row = $conn_obj->sql_fetch_object($rs);
							$xml_data = $row->scenarioDetails;	
							$sourceVolumes = $row->sourceVolumes;
							$cdata = $commonfunctions_obj->getArrayFromXML($xml_data);
							$plan_properties = unserialize($cdata['plan']['data']['value']);
							
							$new_device_list_arr = array();					
							$prism_info_arr = array();
							$prism_specific_info = array();
							$src_capacity_arr = array();
							
							foreach($log_arr[2] as $keyA => $valA)
							{
								$volpack_vol = $conn_obj->sql_escape_string($valA);
								$new_volpack_volumes .= $volpack_vol.",";
								foreach($plan_properties['tgt_vol_pack_details'] as $key => $val)
								{									
									if($keyA == $key)
									{
										$plan_properties['tgt_vol_pack_details'][$key]['new_device_name'] = $valA;										
									}
								}
							}
							foreach($plan_properties['pairInfo'] as $key=>$val)
							{
								foreach($val as $key1=>$val1)
								{
									if(!$val1['processServerid']) 
									{
										if($key1 == 'pairDetails')
										{
											foreach($val1 as $key2 => $val2)
											{
												if($solution_type == "PRISM")
												{
													$phy_lun_id = $val2['srcVol'];
													$prism_info_arr = $val2['prismSpecificInfo'];
													$prism_specific_info[$phy_lun_id] = $prism_info_arr;
												}
												$src_capacity_arr[] = $val2['srcCapacity'];
											}
										}
										unset($plan_properties['pairInfo'][$key]['pairDetails']);
									}
								}
							}
							$volpack_base_vol_arr = explode("(",$plan_properties['repositoryVol']);
							$volpack_base_volume = $volpack_base_vol_arr[0];
							$src_vols_arr = explode(",",$sourceVolumes);
							foreach($plan_properties['pairInfo'] as $key=>$val)
							{
								$i = 0;
								foreach($plan_properties['tgt_vol_pack_details'] as $keyA => $valA)
								{				
									$key_info2 			= $src_vols_arr[$i]."=".$valA['new_device_name'];
									$plan_properties['pairInfo'][$key]['pairDetails'][$key_info2]['srcVol'] = $src_vols_arr[$i];
									$plan_properties['pairInfo'][$key]['pairDetails'][$key_info2]['trgVol'] = $valA['new_device_name'];
									$plan_properties['pairInfo'][$key]['pairDetails'][$key_info2]['srcCapacity'] = $src_capacity_arr[$i];
									if($solution_type == "PRISM")
									{
										$plan_properties['pairInfo'][$key]['pairDetails'][$key_info2]['prismSpecificInfo'] = $prism_specific_info[$src_vols_arr[$i]];
									}
									$i++;
								}
							}
														
							$cdata = serialize($plan_properties);
							$str = "<plan><header><parameters>";
							$str.= "<param name='name'>".$plan_properties['planName']."</param>";
							$str.= "<param name='type'>Protection Plan</param>";
							$str.= "<param name='version'>1.1</param>";
							$str.= "</parameters></header>";
							$str.= "<data><![CDATA[";
							$str.= $cdata;
							$str.= "]]></data></plan>";
							$str = $conn_obj->sql_escape_string($str);
							$new_volpack_volumes = substr_replace($new_volpack_volumes ,"",-1);
							
							
							$sql1 = "UPDATE
										applicationScenario
									SET
										volpackVolumes='$new_volpack_volumes',
										volpackBaseVolume='$volpack_base_volume',
										scenarioDetails = '$str'
									WHERE
										scenarioId='$scenario_id'";
							
							$rs = $conn_obj->sql_query($sql1);
							
						}
					}
					
					$app_obj->vxstubUpdatePolicy($host_id,$policy_id,$status,$log);
				}
				elseif($policy_type == 4)
				{
					$app_obj->vxstubUpdateConsistencyPolicy($host_id,$policy_id,$policy_instance_id,$status,$log, $policy_instance_id_primary);
				}
				elseif($policy_type == 5)
				{
					$conn_obj->enable_exceptions();
					try
					{
						$logging_obj->my_error_handler("INFO",array("HostId"=>$host_id, "PolicyId"=>$policy_id, "Status"=>$status,"Log"=>$log),Log::BOTH);
						$conn_obj->sql_begin();
						$return_result = $app_obj->vxstubUpdatePrepareTarget($host_id,$policy_id,$status,$log);
						$conn_obj->sql_commit();
					}
					catch(Exception $e)
					{
						$conn_obj->sql_rollback();
						$flag = 1;
					}
					$conn_obj->disable_exceptions();
				}
				elseif($policy_type == 45)
				{
					$logging_obj->my_error_handler("INFO",array("HostId"=>$host_id, "PolicyId"=>$policy_id, "PolicyInstanceId"=>$policy_instance_id,"Status"=>$status,"Log"=>$log),Log::BOTH);

					$app_obj->vxstubUpdateRescanScriptPolicy($host_id,$policy_id,$policy_instance_id,$status,$log);
				}
				elseif($policy_type == 61 || $policy_type == 62 || $policy_type == 63 || $policy_type == 64)
				{
					$logging_obj->my_error_handler("INFO",array("HostId"=>$host_id, "PolicyId"=>$policy_id, "PolicyInstanceId"=>$policy_instance_id,"Status"=>$status,"Log"=>$log),Log::BOTH);

					$app_obj->vxstubUpdateV2AMigrationPolicy($host_id,$policy_id,$policy_instance_id,$status,$log);
				}
				elseif($policy_type == 48 || $policy_type == 49)
				{
					$conn_obj->enable_exceptions();
					try
					{
						$logging_obj->my_error_handler("INFO",array("HostId"=>$host_id, "PolicyId"=>$policy_id, "PolicyInstanceId"=>$policy_instance_id,"Status"=>$status,"Log"=>$log),Log::BOTH);
						$conn_obj->sql_begin();
						$flag = $app_obj->vxstubUpdateRollbackPolicy($host_id,$policy_id,$policy_instance_id,$status,$log);
						$conn_obj->sql_commit();
						
					}
					catch(Exception $e)
					{
						$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>1),Log::BOTH);
						$conn_obj->sql_rollback();
						$flag = 1;
					}
					$conn_obj->disable_exceptions();	
				}
			}
			else
			{
				$flag = 3;
			}
		}
	}
	else
	{
		$flag = 2;
	}
	$logging_obj->my_error_handler("DEBUG","FLAG:::$flag");
	return $flag;
}

/*
    * Function Name: vxstub_update_cdp_information_v2
    *
    * Description:
    * This function will capture all cdp information except NoOfFiles and DiskSpace. It is called every 3 minutes.
    *  
    * Parameters:
    *   Param 1 [IN]: hostid
    *	Param 2 [IN]: multi-dimensional array with volume/device name as keys and other values like starttime, endtime, freespace etc. in the inner array as value.
    *
    * Return Value:
    *     Returns boolean
    *
    * Exceptions:
    *     Database connection failure
    */
function vxstub_update_cdp_information_v2($obj,$arg)
{
    global $logging_obj;
	global $conn_obj;
	global $multi_query;
	global $multi_query_delete;	
	global $commonfunctions_obj;
	global $src_updates;
	
	$multi_query = array();
	$multi_query_delete = array();
	$multi_query_update = array();
	$src_updates = array();
	$batchsql_obj = new BatchSqls();
	$logging_obj->my_error_handler("DEBUG","vxstub_update_cdp_information_v2 :: arg - ".print_r($arg,true));	

    $hostid = $obj['host_id'];
    foreach($arg as $key=>$value)
    {   /*total volumes*/
        $total_vol_name[] = $key;
    }
		
    for($vol_num=0;$vol_num<count($total_vol_name);$vol_num++)
	{
        /*Target volumes*/
        $src = $total_vol_name[$vol_num];   
        /*cdp summary containing total retention time ranges*/
        $cdp_summary = $arg["$src"][1];
        /*cdp summary containing particular retention time ranges*/
        $cdp_timeranges = $arg["$src"][2];
        /*cdp events containing details of retention tag events*/
        $cdp_events = $arg["$src"][3];
        /*Update the recovery ranges*/
        $result = update_recovery_ranges_v2($hostid,$src,$cdp_summary,$cdp_timeranges,$cdp_events,$conn_obj);
        if(!$result)
        {
            return false;
        }
    }
	$multi_query_update = array_merge($src_updates,$multi_query_delete, $multi_query);
	
	$logging_obj->my_error_handler("DEBUG","vxstub_update_cdp_information_v2 ::multi_query_update - ".print_r($multi_query_update,true));
	$result_set = $batchsql_obj->sequentialBatchUpdates($multi_query_update, "om_vxstub :: vxstub_update_cdp_information_v2");	
	if($result_set)
	{
		$logging_obj->my_error_handler("DEBUG","vxstub_update_cdp_information_v2 ::return true");				
		return true;
	}
	else
	{		
		$logging_obj->my_error_handler("DEBUG","vxstub_update_cdp_information_v2 ::return false");
		return false;
	}
}

/*
    * Function Name: vxstub_update_cdp_disk_usage
    *
    * Description:
    * This function will capture the NoOfFiles and DiskSpace sent to it from API. It is called every 1 hour.
    *  
    * Parameters:
    *   Param 1 [IN]: hostid
    *	Param 2 [IN]: multi-dimensional array with volume/device name as keys and other values like starttime, endtime, nooffiles etc. in the inner array as value.
    *
    * Return Value:
    *     Returns boolean
    *
    * Exceptions:
    *     Database connection failure
    */
function vxstub_update_cdp_disk_usage($obj,$retention_disk_usage,$target_disk_usage)
{
    global $logging_obj;    
    $hostid = $obj['host_id'];
    
    foreach($retention_disk_usage as $key=>$value)
    {   /*total volumes*/
        $total_vol_name[] = $key;		
    }
    for($vol_num=0;$vol_num<count($total_vol_name);$vol_num++)
	{
        /*Target volumes*/
        $src = $total_vol_name[$vol_num];        
        /*start_timestamp*/
        $start_timestamp = $retention_disk_usage["$src"][1];        
        /*end_timestamp*/
        $end_timestamp = $retention_disk_usage["$src"][2];        
        /*no of retention files*/
        $no_of_files = $retention_disk_usage["$src"][3];        
        /*disk space used retention data*/
        $disk_space = $retention_disk_usage["$src"][4];        
        /*space saved by compression*/
        $space_saved_by_compression = $retention_disk_usage["$src"][5];        
        /*space saved by sparse policy*/
        $space_saved_by_sparsepolicy = $retention_disk_usage["$src"][6];		
        /*Update the recovery ranges*/
        $result = update_recovery_ranges_disk_usage($hostid,$src,$start_timestamp,$end_timestamp,$no_of_files,$disk_space,$space_saved_by_compression,$space_saved_by_sparsepolicy);
        if(!$result)
        {
            return false;
        }
    }
	
	foreach($target_disk_usage as $key=>$value)
    {   /*total volumes*/
        $total_tar_vol_name[] = $key;
    }
	for($tar_vol_num=0;$tar_vol_num<count($total_tar_vol_name);$tar_vol_num++)
	{
		/*Target volumes*/
        $src = $total_tar_vol_name[$tar_vol_num];
		/*target disk space */
		$target_disk_space = $target_disk_usage["$src"][1];
        /*target disk space saved by thinprovisioning */
        $target_space_saved_by_thinprovision = $target_disk_usage["$src"][2];
        /*target disk space saved by compression */	
        $target_space_saved_by_compression = $target_disk_usage["$src"][3];        
		$result = update_target_disk_usage($hostid,$src,$target_disk_space,$target_space_saved_by_thinprovision,$target_space_saved_by_compression);
		if(!$result)
        {
            return false;
        }
	}
	
    return TRUE;
}

function debughost($host_id,$debugString,$method=0)
{
  //global $PHPDEBUG_FILE;
  global $REPLICATION_ROOT;
  $PHPDEBUG_FILE = "$REPLICATION_ROOT/".$host_id."_log";
  if($method)
  {
	$PHPDEBUG_FILE = "$REPLICATION_ROOT/".$host_id."___".$method."_log";
  }
  
  # Some debug
  $fr = fopen($PHPDEBUG_FILE, 'a+');
  if(!$fr) {
	echo "Error! Couldn't open the file.";
  }
  if (!fwrite($fr, $debugString . "\n")) {
	print "Cannot write to file ($filename)";
  }
  if(!fclose($fr)) {
	echo "Error! Couldn't close the file.";
  }
}

/*
 * Function Name: vxstub_update_source_pending_changes
 *
 * Description:
 *     This function update RPO and Pending changes at source
 *  
 * Parameters:
 *     [IN]: $obj, $pending_changes_info_at_source
  *	   [IN]: 
  * Return Value:
 *    boolean
 *
 * Exceptions:
 *     
 */
function vxstub_update_source_pending_changes($obj, $pending_changes_info_at_source)
{
	global $logging_obj, $conn_obj, $vol_obj; 
	$host_id = $obj[host_id];
	
	foreach($pending_changes_info_at_source as $key => $value)
	{
		$total_changes_pending = $value['1'];
		
		#
		# Bug 3302769 : Seeing RPO violation of 16610 days.
		# Fix as driver is sending sudden RPO violation because of known issue
		#
		if(empty($value['2']) || empty($value['3']))
		{
			$message = 'Invalid RPO Reporting by Source -  Previous: ' . $value['2'] . '; New: ' . $value['3'] . PHP_EOL;
			$logging_obj->my_error_handler("INFO","vxstub_update_source_pending_changes: ".$message);
			
			$current_rpo_at_source = $conn_obj->sql_get_value("srcLogicalVolumeDestLogicalVolume", "currentRPOTimeAtSource", "sourceHostId = ? AND sourceDeviceName = ?", array($host_id, $key));
		}
		else
		{
			$hundred_of_nano_unit = 10000000;
			$oldest_ts = $value['2']/$hundred_of_nano_unit;
			$driver_ts = $value['3']/$hundred_of_nano_unit;
			$current_rpo_at_source = $driver_ts - $oldest_ts;
		}		
			
		$query="UPDATE
					srcLogicalVolumeDestLogicalVolume "."
				SET
					currentRPOTimeAtSource = ?,
					differentialPendingInSource = ?
				WHERE
					sourceHostId IN (?)
				AND 
					sourceDeviceName = ?";
					
		$conn_obj->sql_query($query,array($current_rpo_at_source,$total_changes_pending,$host_id,$key));	
		
	}
	
	$logging_obj->my_error_handler("DEBUG","vxstub_update_source_pending_changes".print_r($pending_changes_info_at_source,TRUE)."\n");
	return 0;
}

/* Function to handle application agent registration */
function vxstub_register_application_agent($obj, $host_id, $application_version,  $name, $ip_address, $app_agent_install_path, $compatability_num, $time_zone, $app_patch_version, $product_version, $os_val, $os_info, $endianess)
{
	global $conn_obj, $commonfunctions_obj, $logging_obj;
    $id = $obj['host_id'];
	$auditFlag = false;
	$return_value = TRUE;
	$logging_obj->my_error_handler("DEBUG","vxstub_register_application_agent ::  host_id::$host_id, application_version::$application_version, name:: $name, ip_address::$ip_address, install_path::$app_agent_install_path, compatability_num::$compatability_num, time_zone::$time_zone, app_patch_version::$app_patch_version, product_version::$product_version,endianess=$endianess,osType=$os_info,osFlag=$os_val");

    $result_set = $conn_obj->sql_query("SELECT count(*) as recCount, appAgentEnabled FROM hosts WHERE id = ? GROUP BY ipaddress", array($id));

	$app_patch_version = preg_replace('/\\\\+/', '\\', $app_patch_version);
	$os_type = $os_info['name'];
	$os_caption = $os_info['caption'];
	
	$rec_count = 0;
	if (count($result_set) > 0)
	{
		$rec_count = $result_set[0]['recCount'];
		if ($result_set[0]['appAgentEnabled'] == 0)
		{
			$auditFlag = true;
		}
	} 
	else
	{
		$auditFlag = true;
	}

    if($rec_count > 0) 
    {
        $sql =  "UPDATE 
					hosts 
				SET 
					appAgentEnabled = ?, 
					prod_version = ?, 
					appAgentVersion = ?, 
					name = ?, 
					ipaddress = ?, 
					compatibilityNo = ?,  
					appAgentPath = ?, 
					agentTimeZone = ? , 
					PatchHistoryAppAgent = ?, 
					endianess = ?, 
					osType = ?, 
					osFlag = ?, 
					osCaption = ? 
				WHERE 
					id = ?";
		$update_status = $conn_obj->sql_query($sql,array(1,$product_version,$application_version,$name,$ip_address,$compatability_num,$app_agent_install_path,$time_zone,$app_patch_version,$endianess,$os_type,$os_val,$os_caption,$id));
    }
    else
    {
       
		$sql = "INSERT 
					INTO 
				hosts 
					(id, 
					name, 
					ipaddress, 
					agentTimeZone, 
					appAgentEnabled, 
					compatibilityNo, 
					prod_version, 
					appAgentVersion, 
					PatchHistoryAppAgent, 
					appAgentPath, 
					endianess, 
					osType, 
					osFlag, 
					osCaption) 
				VALUES 
					(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
		$insert_status = $conn_obj->sql_query($sql, array($id,$name,$ip_address,$time_zone,1,$compatability_num,$product_version,$application_version,$app_patch_version,$app_agent_install_path,$endianess,$os_type,$os_val,$os_caption));
    }
    $logging_obj->my_error_handler("DEBUG","vxstub_register_application_agent sql::$sql");
	
	if ($auditFlag && $return_value)
	{	
		include_once("writeFile.php");
		$auditPatch='';
		if ($app_patch_version)
		{
			$auditPatch = "PatchHistoryAppAgent :: $app_patch_version";
		}
		writeLog("admin","App agent registered to CX with details (hostName::$name, ipaddress:$ip_address, hostid::$id, prod_version::$product_version , appAgentEnabled::1, HostUpdateTime::$my_current_time, Version::$compatability_num, vxAgentPath::$app_agent_install_path , $auditPatch)");
	}
	
    return $return_value;
}


function vxstub_get_build_path($obj,$osName,$jobid)
{
	global $logging_obj;
    $pushinstall_obj = new PushInstall();
    $result = $pushinstall_obj->get_build_path($osName,$jobid);
    return $result;
}

function vxstub_get_mtregistration_details() 
{
	$commonfun_obj = new CommonFunctions();
	
	global $CLOUD_CONTAINER_CERT, $CLOUD_CONTAINER_KEY, $PRIVATE_FOLDER_NAME, $SLASH, $logging_obj;	
	$folder_location = $commonfun_obj->getCertRootDir().$SLASH.$PRIVATE_FOLDER_NAME.$SLASH;
	$cert_file_path = $folder_location.$CLOUD_CONTAINER_CERT;
	$key_file_path = $folder_location.$CLOUD_CONTAINER_KEY;
	$file_mode = "r";
	$cert_reg_data = array(0);
	try
	{
		if (is_dir($folder_location))
		{			
			if(! is_readable($cert_file_path))
			{
				throw new Exception ("File is not readable: ".$cert_file_path);
			}	
			if(! is_readable($key_file_path))
			{
				throw new Exception ("File is not readable: ".$key_file_path);
			}			
			if(!$cert_file_handle = $commonfun_obj->file_open_handle($cert_file_path, $file_mode))	
			{
				throw new Exception ("File open failed: $cert_file_handle and file is: ".$cert_file_path);
			}
			if(!$key_file_handle = $commonfun_obj->file_open_handle($key_file_path, $file_mode))	
			{
				throw new Exception ("File open failed: $key_file_handle and file is: ".$key_file_path);
			}
			
			$cert_file_size = filesize($cert_file_path);
			if ($cert_file_size == 0)
			{
				throw new Exception ("File size is zero: ".$cert_file_path);
			}
			else
			{
				$cert_file_contents = fread($cert_file_handle, filesize($cert_file_path));
				fclose($cert_file_handle);
				$cert_reg_data[] = base64_encode($cert_file_contents);
			}
			$key_file_size = filesize($key_file_path);
			if ($key_file_size == 0)
			{
				throw new Exception ("File size is zero: ".$key_file_path);
			}
			else
			{
				$key_file_contents = fread($key_file_handle, filesize($key_file_path));
				fclose($key_file_handle);	
				$cert_reg_data[] = base64_encode($key_file_contents);
			}			
		}
		else
		{
			throw new Exception ("Folder not exists: ".$folder_location);
		}
	}
	catch (Exception $excep)
	{
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Result"=>500,"Reason"=>$excep->getMessage()),Log::BOTH);
		$GLOBALS['http_header_500'] = TRUE;	
		header('HTTP/1.0 500 InMage method call returned an error',TRUE,500);
		header('Content-Type: text/plain');		
	}
	$logging_obj->my_error_handler("DEBUG","vxstub_get_mtregistration_details::cert_reg_data ".print_r($cert_reg_data, true));	
	return $cert_reg_data;
}

function vxstub_get_jobs_to_process($obj) 
{
	global $logging_obj, $INSTALLATION_DIR, $CLOUD_CONTAINER_REGISTRY_HIVE_FILE;
	$CLOUD_CONTAINER_REGISTRY_HIVE_FILE_PATH = $INSTALLATION_DIR."/var/".$CLOUD_CONTAINER_REGISTRY_HIVE_FILE;
	
	$commonfun_obj = new CommonFunctions();
	$system_obj = new System();
    $mds_obj = new MDSErrorLogging();
	
    $jobs = $system_obj->getAgentJobs($obj['host_id']);
    
	$cert_reg_data = vxstub_get_mtregistration_details();
	
	// execute the perl command to get the registration data.
	$reg_data = "";
	if(file_exists($CLOUD_CONTAINER_REGISTRY_HIVE_FILE_PATH) && filesize($CLOUD_CONTAINER_REGISTRY_HIVE_FILE_PATH) > 0)
	{
		$reg_key_file_handle = $commonfun_obj->file_open_handle($CLOUD_CONTAINER_REGISTRY_HIVE_FILE_PATH, "rb");
		$reg_data = fread($reg_key_file_handle, filesize($CLOUD_CONTAINER_REGISTRY_HIVE_FILE_PATH));
		fclose($reg_key_file_handle);
	}
	
    $agent_jobs = array();
    foreach($jobs as $job_details) 
    {
        $agent_job = array(0);
        if($job_details['JobType'] == "MTPrepareForAadMigrationJob")
        {
            $logging_obj->my_error_handler("DEBUG","vxstub_get_jobs_to_process::job_details ".print_r($job_details, true));
            $agent_job[] = $job_details['Id'].'';
            $agent_job[] = $job_details['JobType'];
            $agent_job[] = $job_details['JobStatus'];
            $agent_job[] = serialize(array(0, $cert_reg_data[1], base64_encode($reg_data)));
            $agent_job[] = ($job_details['outputPayload'] != NULL) ? $job_details['outputPayload'] : "";
            $agent_job[] = $job_details['Errors'];
            $agent_job[] = $job_details['Context'];
        }
        else continue;
        $agent_jobs[] = $agent_job;
        
        // update to MDS that the jobs are being sent to agent.
        $mds_data_array = array();
        $mds_data_array["activityId"] = $job_details['Context'][0];
        $mds_data_array["jobType"] = $job_details['JobType'];
        $mds_data_array["errorString"] = "MigrateACSToAAD job with ID : ".$job_details['Id']." is being sent to MT ".$obj['host_id']." with ClientRequestId : ".$job_details['Context'][1].", Activity id: ".$job_details['Context'][0].", Data - ".print_r($job_details, true)."\n";
        $mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
        $mds_data_array["errorType"] = "ERROR";        
        $mds_obj->saveMDSLogDetails($mds_data_array);
    }
    
	$logging_obj->my_error_handler("DEBUG","vxstub_get_jobs_to_process::job_details ".print_r($agent_jobs, true));
    
	return $agent_jobs;
}

function vxstub_update_job_status($obj, $job_details) 
{
	global $logging_obj;
	
	$system_obj = new System();
	
	$logging_obj->my_error_handler("DEBUG","vxstub_update_job_status::job_details ".print_r($job_details, true));
	
	// errors structure format
    $update_job_error_array = array();
	$agent_job_error = $job_details[6];
    // trimming so that the unserialize doesn't fail
    foreach($agent_job_error as $job_error)
    {
        $loop_error = array();
        // trim errorcode, errormessage, possible causes, recommended action.
        for($i = 0; $i < 4; $i ++)
        {
            $loop_error[] = trim($job_error[$i]);
        }
        
        $loop_error_params = array();
        // trim data in params
        foreach($job_error[5] as $param)
        {
            $loop_error_params[] = trim($param);
        }
        $loop_error[] = $loop_error_params;
        
        $update_job_error_array[] = $loop_error;
    }
	
	$update_job = array();
	$update_job['Id'] = $job_details[1];
	$update_job['JobStatus'] = $job_details[3];	
	$update_job['Errors'] = serialize($update_job_error_array);
    
    // update to MDS the job status from agent.
    $mds_data_array = array();
    $mds_data_array["activityId"] = $job_details[7][0];
    $mds_data_array["jobType"] = "MigrateACSToAAD";
    $mds_data_array["errorString"] = "MigrateACSToAAD job with ID ".$job_details[1]." is updated by MT ".$obj['host_id']." with ClientRequestId : ".$job_details[7][1].", Activity id: ".$job_details[7][0].", Data - ".print_r($update_job, true)."\n";
    $mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
    $mds_data_array["errorType"] = "ERROR";
    $mds_obj = new MDSErrorLogging();
    $mds_obj->saveMDSLogDetails($mds_data_array);
	
    $system_obj->updateJobStatus($obj['host_id'], $update_job);
}

function vxstub_update_agent_health($obj, $health_errors)
{
    global $logging_obj, $conn_obj, $MOBILITY_AGENT_HF_LIST, $VM_LEVEL_HF_LIST, $DISK_LEVEL_HF_LIST;
	
	$agent_health_update = false;
    
    $source_host_id = $obj['host_id'];
	//This call do every 30mins. Hence, it is safe to log to identify what health errors reported by agent.
	$logging_obj->my_error_handler("INFO",array("HostId"=>$source_host_id,"HealthErrors"=>$health_errors),Log::BOTH);
	$prot_obj = new ProtectionPlan();
    
    $health_details = array();
    foreach($health_errors as $health_error)
    {
        $health_code = $health_error[1];
        $context = $health_error[2];
        $health_error_text = $health_error[3];
        
        if($context == "") $health_details[$health_code] = array();
        else $health_details[$health_code][$context] = $health_error_text;
    }
    
    foreach($health_details as $health_code => $details)
    {
        $params = array_keys($details);
         
        $health_error_text = $pairs = array();
		
        if(in_array(strtoupper($health_code),$VM_LEVEL_HF_LIST)) // VM level health factors should process in below block.
        {
            $pairs = $prot_obj->get_pair_id_by_source($params);
            foreach($pairs as $pair)
            {
                $health_error_text[$pair['pairId']] = $details[$source_host_id];
            }
        }
        
        if(in_array(strtoupper($health_code),$DISK_LEVEL_HF_LIST)) // Disk level health factors should process in below block.
        {
            $pairs = $prot_obj->get_pair_id_by_disk_id($source_host_id, $params);
            foreach($details as $src_device_name => $text)
            {
                foreach($pairs as $pair)
                {
                    if($pair['sourceDeviceName'] == $src_device_name)
                    $health_error_text[$pair['pairId']] = $text;
                }
            }
        }
        
        foreach($pairs as $pair)
        {
            if(is_array($pair) && $pair['sourceHostId'] != "" && $pair['destinationHostId'] != "" && $pair['pairId'] != "")
            {
                $sql = "REPLACE INTO healthfactors(sourceHostId, destinationHostId, pairId, errCode, healthFactorCode, healthFactor, priority, component, healthDescription, healthTime, updated) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, now(), ?)";
				$sql_args = array($pair['sourceHostId'], $pair['destinationHostId'], $pair['pairId'], $health_code, $health_code, 2, 0, 'Source', $health_error_text[$pair['pairId']], 'Y');
				$logging_obj->my_error_handler("DEBUG","vxstub_update_agent_health :: SQL : $sql, Args : ".print_r($sql_args,true));
				$logging_obj->my_error_handler("INFO",array("Sql"=>$sql, "Args"=>$sql_args),Log::BOTH);
                $query_status = $conn_obj->sql_query($sql, $sql_args);
            }
            else
            {
                $mds_data_array = array();
                $mds_data_array["activityId"] = "";
                $mds_data_array["errorString"] = "Failed to update the health code ".$health_code." for source : ".$source_host_id.", parameters : ".print_r($params, true)." text : ".$health_error_text[$pair['pairId']];
                $mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
                $mds_data_array["errorType"] = "ERROR";
				$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>$mds_data_array),Log::BOTH);
                $mds_obj = new MDSErrorLogging();
                $mds_obj->saveMDSLogDetails($mds_data_array);
            }
        }
    }
	
	$mobility_agent_hfs = implode(",", $MOBILITY_AGENT_HF_LIST);
	// If no context of the health code exists, it mean the health error has been corrected.
	// so remove the health error from the pair/source
    // Delete all the health entries for this health code and source host that are not updated.
	$sql = "DELETE FROM healthfactors WHERE sourceHostId = ? AND updated = ? AND FIND_IN_SET(errCode, ?)";
	$query_status = $conn_obj->sql_query($sql, array($source_host_id, 'N', $mobility_agent_hfs));
        
    // update the updated column to N for all the health entries for the source host
    $sql = "UPDATE healthfactors SET updated = ? WHERE sourceHostId = ? AND FIND_IN_SET(errCode, ?)";
    $query_status = $conn_obj->sql_query($sql, array('N', $source_host_id, $mobility_agent_hfs));
	
	$agent_health_update = true;
	
	return $agent_health_update;
}

/*
 * Function Name: prism_settings
 *
 * Description:
 *     This API will send prism settings to svagent.
 *  
 * Parameters:
 *     [IN]: $obj
 * Return Value:
 *    Array
 *
 * Exceptions:
 *     
 */
function prism_settings($obj,$final_hash=array())
{
	global $logging_obj; 
	#$logging_obj->my_error_handler("DEBUG","prism_settings");
	$prismConf = new PrismConfigurator();
	#$logging_obj->my_error_handler("DEBUG","prism_settings>>>>>> ".print_r($prismConf->get_prism_settings($obj),TRUE));
	return $prismConf->get_prism_settings($obj,$final_hash);

}

/*
 * Function Name: vxstub_source_agent_protection_pair_health_issues
 *
 * Description:
 *     This API receive agent reported health issues and insert into DB.
 *  
 * Parameters:
 *     [IN]: $obj
 *     [IN]: $health_errors
 * Return Value:
 *    Bool
 *
 * Exceptions:
 *     
 */
function vxstub_source_agent_protection_pair_health_issues($obj, $health_errors)
{
    global $logging_obj, $conn_obj;
	$prot_obj = new ProtectionPlan();
	$result=true;
	$health_description  = "";
	$agent_health_update = false;
	$reportedHealthIssues = array();
    $existing_health_issues = array();
	$source = "SourceAgent";
	$backward_compatible_component_type = "Source";
	
    $source_host_id = $obj['host_id'];
	$logging_obj->my_error_handler("INFO",array("HostId"=>$source_host_id,"HealthErrors"=>$health_errors),Log::BOTH);
	$health_errors_info = json_decode($health_errors);

	$conn_obj->enable_exceptions();
	try
	{
		$conn_obj->sql_begin();
		if(count($health_errors_info->HealthIssues) > 0)
		{
			$pairs = $prot_obj->get_pair_id_by_source($source_host_id);
			$healthFactorType = 'VM';
			foreach($health_errors_info->HealthIssues as $serverIssue)
			{
				$health_code = $serverIssue->IssueCode;
				$severity = $serverIssue->Severity;
				$source = $serverIssue->Source;
				$messageParams = json_encode($serverIssue->MessageParams);
				
				$logging_obj->my_error_handler("INFO",array("health_code"=>$health_code, "messageParams"=>$messageParams),Log::BOTH);
				array_push($reportedHealthIssues, $health_code);
				
				foreach($pairs as $pair)
				{
					if(is_array($pair) && $pair['sourceHostId'] != "" && $pair['destinationHostId'] != "" && $pair['pairId'] != "")
					{
						$sql = "INSERT INTO healthfactors (sourceHostId, destinationHostId, pairId, errCode, healthFactorCode, healthFactor, priority, component, healthDescription, healthTime, updated, healthUpdateTime, healthFactorType, healthPlaceHolders) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, now(), ?, now(), ?, ?) on duplicate key update healthUpdateTime = now()";
						$sql_args = array($pair['sourceHostId'], $pair['destinationHostId'], $pair['pairId'], $health_code, $health_code, 2, 0, $source, $health_description, 'Y', $healthFactorType, $messageParams);
						
						$logging_obj->my_error_handler("INFO",array("Sql"=>$sql, "Args"=>$sql_args),Log::BOTH);
						$query_status = $conn_obj->sql_query($sql, $sql_args);
					}
					else
					{
						$result = false;
						$mds_data_array = array();
						$mds_data_array["activityId"] = "";
						$mds_data_array["errorString"] = "Failed to update the health code ".$health_code." for source : ".$source_host_id.", parameters : ".$messageParams;
						$mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
						$mds_data_array["errorType"] = "ERROR";
						$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>$mds_data_array),Log::BOTH);
						$mds_obj = new MDSErrorLogging();
						$mds_obj->saveMDSLogDetails($mds_data_array);
					}
				}
			}
		}
		
		if(count($health_errors_info->DiskLevelHealthIssues) > 0)
		{
			$healthFactorType = 'DISK';
			foreach($health_errors_info->DiskLevelHealthIssues as $serverIssue)
			{
				$health_code = $serverIssue->IssueCode;
				$diskId = $serverIssue->DiskContext;
				$severity = $serverIssue->Severity;
				$source = $serverIssue->Source;
				$messageParams = json_encode($serverIssue->MessageParams);
				array_push($reportedHealthIssues, $health_code);
				
				$pairs = $prot_obj->get_pair_id_by_disk_id($source_host_id, $diskId);
				
				foreach($pairs as $pair)
				{
					if(is_array($pair) && $pair['sourceHostId'] != "" && $pair['destinationHostId'] != "" && $pair['pairId'] != "")
					{
						$sql = "INSERT INTO healthfactors (sourceHostId, destinationHostId, pairId, errCode, healthFactorCode, healthFactor, priority, component, healthDescription, healthTime, updated, healthUpdateTime, healthFactorType, healthPlaceHolders) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, now(), ?, now(), ?, ?) on duplicate key update healthUpdateTime = now()";
						$sql_args = array($pair['sourceHostId'], $pair['destinationHostId'], $pair['pairId'], $health_code, $health_code, 2, 0, $source, $health_description, 'Y', $healthFactorType, $messageParams);
						
						$logging_obj->my_error_handler("INFO",array("Sql"=>$sql, "Args"=>$sql_args),Log::BOTH);
						$query_status = $conn_obj->sql_query($sql, $sql_args);
					}
					else
					{
						$result = false;
						$mds_data_array = array();
						$mds_data_array["activityId"] = "";
						$mds_data_array["errorString"] = "Failed to update the health code ".$health_code." for source : ".$source_host_id.", diskId : ".$diskId.",parameters : ".$messageParams;
						$mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
						$mds_data_array["errorType"] = "ERROR";
						$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>$mds_data_array),Log::BOTH);
						$mds_obj = new MDSErrorLogging();
						$mds_obj->saveMDSLogDetails($mds_data_array);
					}
				}
			}
		}
		
		$reported_health_issues = array_unique($reportedHealthIssues);
		
		$heatlh_sql = "SELECT distinct errCode FROM healthfactors WHERE sourceHostId = ? AND (Component = ? OR Component = ?)";
		$health_query_status = $conn_obj->sql_query($heatlh_sql, array($source_host_id, $source, $backward_compatible_component_type));
		foreach($health_query_status as $key=>$value)
		{
			array_push($existing_health_issues, $value['errCode']);
		}
		
		$diff_health_issues=array_diff($existing_health_issues,$reported_health_issues);
		$logging_obj->my_error_handler("INFO",array("existing_health_issues"=>$existing_health_issues, "reported_health_issues"=>$reported_health_issues, "diff_health_issues"=>$diff_health_issues),Log::BOTH);
		
		if(count($diff_health_issues) > 0)
		{
			$diff_health_issues_str = implode(",",$diff_health_issues);
			$sql = "DELETE FROM healthfactors WHERE sourceHostId = ? AND FIND_IN_SET(errCode, ?) AND (Component = ? OR Component = ?)";
			$query_status = $conn_obj->sql_query($sql, array($source_host_id, $diff_health_issues_str, $source, $backward_compatible_component_type));	
		}
		$conn_obj->sql_commit();
	}
	catch(SQLException $e)
	{
		$conn_obj->sql_rollback();
		$result = false;
		$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>$result),Log::BOTH);
	}
	$conn_obj->disable_exceptions();	
	
	if (!$GLOBALS['http_header_500'])
		$logging_obj->my_error_handler("INFO",array("Result"=>$result,"Status"=>"Success"),Log::BOTH);
	else
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
	return $result;
}



/*
 * Function Name: vxstub_target_agent_protection_pair_health_issues
 *
 * Description:
 *     This API receive agent reported health issues and insert into DB.
 *  
 * Parameters:
 *     [IN]: $obj
 *     [IN]: $health_errors
 * Return Value:
 *    Bool
 *
 * Exceptions:
 *     
 */
function vxstub_target_agent_protection_pair_health_issues($obj, $health_errors)
{
    global $logging_obj, $conn_obj, $MARS_RETRYABLE_ERRORS_FABRIC_HF_MAP, $MARS_RETRYABLE_ERRORS_PROTECTION_HF_MAP;
	global $MARS_HEALTH_FACTOR_SOURCE_FOR_FABRIC, $MARS_HEALTH_FACTOR_SOURCE_FOR_PROTECTION;
	$prot_obj = new ProtectionPlan();
	$result = true;
	$fabricReportedHealthIssues = array();
	$protectionReportedHealthIssues = array();
	$commonfun_obj = new CommonFunctions();
    $destination_host_id = $obj['host_id'];
	$logging_obj->my_error_handler("INFO", "HostId = $destination_host_id, HealthErrors = ".print_r($health_errors,TRUE),Log::BOTH);

	$conn_obj->enable_exceptions();
	try
	{
		$conn_obj->sql_begin();
		$get_ps_details = $commonfun_obj->getAllPsDetails();
		if (is_array($health_errors) && 
			(count($health_errors) > 0))
		{
			$pairs = $prot_obj->get_pair_id_by_destination($destination_host_id);
			$healthFactorType = 'VM';
			foreach ($health_errors as $key=>$health_error)
			{
				//Fabric health factors processing block.
				$health_error = dechex($health_error);
				$logging_obj->my_error_handler("INFO", "HealthErrors = $health_error",Log::BOTH);
				if (array_key_exists($health_error,$MARS_RETRYABLE_ERRORS_FABRIC_HF_MAP))
				{
					$fabric_health_code = $MARS_RETRYABLE_ERRORS_FABRIC_HF_MAP[$health_error];
					$fabric_place_holders = "";
					$health_description  = "";
					$logging_obj->my_error_handler("INFO",array("fabric_health_code"=>$fabric_health_code),Log::BOTH);
					array_push($fabricReportedHealthIssues, $fabric_health_code);
					switch ($health_error) 
					{						
						case "80790305":
							$health_description = "MT_E_REGISTRATION_FAILED";
						break;
						case "80790100":
							$health_description = "MT_E_SERVICE_COMMUNICATION_ERROR";
							$ps_name = $get_ps_details[$destination_host_id]["name"];
							$fabric_place_holders = serialize(array("ProcessServerName"=>$ps_name));
						break;
						case "80790300":
							$health_description = "MT_E_SERVICE_AUTHENTICATION_FAILED";
						break;
						case "800706ba":
							$health_description = "MT_E_MARS_SERVICE_STOPPPED";
						break;
						default:
					}
					
					$sql = "insert into infrastructurehealthfactors(hostId, healthFactorCode, healthFactor, priority, component, healthDescription,  healthCreationTime, healthUpdateTime, placeHolders) values (?,?,?,?,?,?,now(), now(), ?) on duplicate key update healthUpdateTime = now()";
					$sql_args = array($destination_host_id,$fabric_health_code,2,0,$MARS_HEALTH_FACTOR_SOURCE_FOR_FABRIC,$health_description, $fabric_place_holders);
					$logging_obj->my_error_handler("DEBUG",array("Sql"=>$sql, "Args"=>$sql_args),Log::BOTH);
					$query_status = $conn_obj->sql_query($sql, $sql_args);
				}
				
				//Protection health factors processing block.
				if (array_key_exists($health_error,$MARS_RETRYABLE_ERRORS_PROTECTION_HF_MAP))
				{
					$protection_health_code = $MARS_RETRYABLE_ERRORS_PROTECTION_HF_MAP[$health_error];
					$protection_place_holders = "";
					$logging_obj->my_error_handler("INFO",array("protection_health_code"=>$protection_health_code),Log::BOTH);
					array_push($protectionReportedHealthIssues, $protection_health_code);
					switch ($health_error) 
					{						
						case "80790100":
							$health_description = "MT_E_SERVICE_COMMUNICATION_ERROR";
							$ps_name = $get_ps_details[$destination_host_id]["name"];
							$protection_place_holders = json_encode(array("ProcessServerName"=>$ps_name));
						break;
						case "800706ba":
							$health_description = "MT_E_MARS_SERVICE_STOPPPED";
						break;
						default:
					}
				
					foreach($pairs as $pair)
					{
						if(is_array($pair) && $pair['sourceHostId'] != "" && $pair['destinationHostId'] != "" && $pair['pairId'] != "")
						{
							$sql = "INSERT INTO healthfactors (sourceHostId, destinationHostId, pairId, errCode, healthFactorCode, healthFactor, priority, component, healthDescription, healthTime, updated, healthUpdateTime, healthFactorType, healthPlaceHolders) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, now(), ?, now(), ?, ?) on duplicate key update healthUpdateTime = now()";
							$sql_args = array($pair['sourceHostId'], $pair['destinationHostId'], $pair['pairId'], $protection_health_code, $protection_health_code, 2, 0, $MARS_HEALTH_FACTOR_SOURCE_FOR_PROTECTION, $health_description, 'Y', $healthFactorType, $protection_place_holders);
							$logging_obj->my_error_handler("DEBUG",array("Sql"=>$sql, "Args"=>$sql_args),Log::BOTH);
							$query_status = $conn_obj->sql_query($sql, $sql_args);
						}
					}
				}
				
			}
			
		}
		
		//Fabric health factors clean up
		$existing_health_issues = array();
		$reported_health_issues = array_unique($fabricReportedHealthIssues);
		$heatlh_sql = "SELECT distinct healthFactorCode FROM infrastructurehealthfactors WHERE hostId = ? AND component = ?";
		$health_query_status = $conn_obj->sql_query($heatlh_sql, array($destination_host_id, $MARS_HEALTH_FACTOR_SOURCE_FOR_FABRIC));
		foreach($health_query_status as $key=>$value)
		{
			if (in_array($value['healthFactorCode'],$MARS_RETRYABLE_ERRORS_FABRIC_HF_MAP))
			{
				array_push($existing_health_issues, $value['healthFactorCode']);
			}
		}
		
		$diff_health_issues=array_diff($existing_health_issues,$reported_health_issues);
		if(count($diff_health_issues) > 0)
		{
			$logging_obj->my_error_handler("INFO",array("existing_health_issues"=>$existing_health_issues, "reported_health_issues"=>$reported_health_issues, "diff_health_issues"=>$diff_health_issues),Log::BOTH);
			$diff_health_issues_str = implode(",",$diff_health_issues);
			$sql = "DELETE FROM infrastructurehealthfactors WHERE hostId = ? AND FIND_IN_SET(healthFactorCode, ?) AND component = ?";
			$query_status = $conn_obj->sql_query($sql, array($destination_host_id, $diff_health_issues_str, $MARS_HEALTH_FACTOR_SOURCE_FOR_FABRIC));	
		}
		
		//Protection health factors clean up 
		$existing_health_issues = array();
		$reported_health_issues = array_unique($protectionReportedHealthIssues);		
		$heatlh_sql = "SELECT distinct errCode FROM healthfactors WHERE destinationHostId = ? AND component = ?";
		$health_query_status = $conn_obj->sql_query($heatlh_sql, array($destination_host_id, $MARS_HEALTH_FACTOR_SOURCE_FOR_PROTECTION));
		foreach($health_query_status as $key=>$value)
		{
			if (in_array($value['errCode'],$MARS_RETRYABLE_ERRORS_PROTECTION_HF_MAP))
			{
				array_push($existing_health_issues, $value['errCode']);
			}
		}
		
		$diff_health_issues=array_diff($existing_health_issues,$reported_health_issues);
		if(count($diff_health_issues) > 0)
		{
			$logging_obj->my_error_handler("INFO",array("existing_health_issues"=>$existing_health_issues, "reported_health_issues"=>$reported_health_issues, "diff_health_issues"=>$diff_health_issues),Log::BOTH);
			$diff_health_issues_str = implode(",",$diff_health_issues);
			$sql = "DELETE FROM healthfactors WHERE destinationHostId = ? AND FIND_IN_SET(errCode, ?) AND component = ?";
			$query_status = $conn_obj->sql_query($sql, array($destination_host_id, $diff_health_issues_str, $MARS_HEALTH_FACTOR_SOURCE_FOR_PROTECTION));	
		}
		
		$conn_obj->sql_commit();
	}
	catch(SQLException $e)
	{
		$conn_obj->sql_rollback();
		$result = false;
		$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>$result),Log::BOTH);
	}
	$conn_obj->disable_exceptions();	
	
	if (!$GLOBALS['http_header_500'])
		$logging_obj->my_error_handler("INFO",array("Result"=>$result,"Status"=>"Success"),Log::BOTH);
	else
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"500"),Log::BOTH);
	return $result;
}
?>