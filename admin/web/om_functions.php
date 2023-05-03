<?php
#RHEL5
$functions_included = 1;

include_once( 'config.php' );
include_once( 'pair_settings.php' );
include_once( 'om_conn.php' );
$retention_obj = new Retention(); 
$commonfunctions_obj = new CommonFunctions();
$vol_obj = new VolumeProtection();
$sparseretention_obj = new SparseRetention();
$bandwidth_obj = new BandwidthPolicy();
$app_obj = new Application();

# Constants
$FRLOG_PATH = '/tmp/frlog';

/*The bleow logical block is added by Srinivas Patnana to allow default time zone settings code block for RHEL5.
The PHP_OS constant will contain the short descritpion of the operating system PHP was built on.*/

$php_version = phpversion();
$get_OS=$_ENV["OS"];
if (($php_version >= 5) AND ($get_OS!="Windows_NT"))
{
   #debugLog("Setting time zone\n");
	$TIME_ZONE = date_default_timezone_get();
   #
   #Set the default timezone explicitly - BKNATHAN
   #
   date_default_timezone_set("$TIME_ZONE");
   #debugLog("Timezone : $TIME_ZONE\n");
}

function get_host_info( $id )
{
    global $conn_obj;
  
    $iter = $conn_obj->sql_query( "select a.id, a.name, a.sentinelEnabled, a.outpostAgentEnabled, a.filereplicationAgentEnabled,a.compatibilityNo,a.osFlag from hosts a where a.id = '$id'");

    $myrow = $conn_obj->sql_fetch_row( $iter );
    $result = array( 'id' => $myrow[ 0 ], 'name' => $myrow[ 1 ], 'sentinelEnabled' => $myrow[ 2 ], 'outpostAgentEnabled' => $myrow[ 3 ], 'filereplicationAgentEnabled' => $myrow[4],'compatibilityNo' => $myrow[5],'osFlag' => $myrow[6]  );

    return( $result );
}

function is_one_to_many_by_source ($src_id, $src_dev)
{
  global $conn_obj;
  $src_dev = $conn_obj->sql_escape_string ($src_dev);
  $iter = $conn_obj->sql_query ("select count(*) from srcLogicalVolumeDestLogicalVolume ".
            "where sourceHostId = '$src_id' and  sourceDeviceName='$src_dev'");
  $row = $conn_obj->sql_fetch_row ($iter);
  if ($row[0] > 1)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

function is_device_clustered ($hostid, $deviceName)
{

    global $conn_obj;
        
    #bug 6030
    $deviceName = verifyDatalogPath($deviceName);
    #End bug 6030   
        
    
     $deviceName = $conn_obj->sql_escape_string ($deviceName);
    $iter = $conn_obj->sql_query ("select * from clusters where hostId='$hostid' and  deviceName='$deviceName'");
    if ($conn_obj->sql_num_row($iter) > 0)
    {
        return TRUE;
    }
    else
    {

        return FALSE;
    }
}

/*
	This function is added for voiding hide and undhide call from cdpcli for handling the  pair when it os in resynjc 
*/
function get_replication_state3( $src_id, $src_dev, $dest_id, $dest_dev )
{
	 global $conn_obj;
    $iter = $conn_obj->sql_query( "select isResync from srcLogicalVolumeDestLogicalVolume " .
                "where sourceHostId = '" . $src_id . "' and " .
                " sourceDeviceName = '" . $src_dev . "' and ".
                "destinationHostId ='" . $dest_id . "' and ".
                " destinationDeviceName ='". $dest_dev ."'");

    $iter1 = $conn_obj->sql_query( "select visible,readwritemode from logicalVolumes " .
                "where hostId ='" . $dest_id . "' and ".
                " deviceName ='". $dest_dev ."'");


    $myrow = $conn_obj->sql_fetch_row( $iter );
    $myrow1 = $conn_obj->sql_fetch_row( $iter1 );

    $result = 'Inactive';
    if( $myrow[ 0 ] == 1 ) {
        $result = 'Resyncing';
    }
    else if( $myrow[ 0 ] == 0 ) {
        $result = 'Differential Sync';

	 if($myrow1[0]==1){
		if($myrow1[1]==2)
			$result = $result;
		else if($myrow1[1]==1)
			$result = $result;

	 }
    }

    return $result;
}

function set_resync_required( $id, $vol, $err, $isResyncRequired, $timestamp="", $resync_reason_details="")
{

	/*
	2015-12-16 18:33:59 (UTC) CX :INFO:set_resync_required:: id=f25121f9-142e-4f27-a27b-1042e591d877, vol={2908697911},err= id=f25121f9-142e-4f27-a27b-1042e591d877&vol={2908697911}&timestamp=130947643577115977&err=The system crashed or experienced a non-controlled shutdown. Replication sync on
device {2908697911} (DEVICE ID = {2908697911}) will need to be reestablished.
&errcode=2702376978, isResyncRequired=2,timestamp=1450290839
*/
    global $conn_obj, $SOURCE_DATA;
	global $logging_obj,$vol_obj, $commonfunctions_obj, $ROLLBACK_INPROGRESS, $SOURCE_AGENT, $SOURCE_AGENT_COMPONENT,$RESYNC_REQD, $FATAL, $ZERO;
    $optionals = "";
    $cond = "";
	$write_agent_log = TRUE;
	$agent_timestamp = $timestamp;
	$do_pause = FALSE;
	$result = TRUE;
	try
	{
		$logging_obj->my_error_handler("INFO",array("HostId"=>$id, "SourceDeviceName"=>$vol, "Reason"=>$err,"IsResyncRequired"=>$isResyncRequired,"Timestamp"=>$timestamp, "Origin"=>"Source", "Reason details"=>print_r($resync_reason_details, TRUE)),Log::BOTH);
		
		$valid_protection = $commonfunctions_obj->isValidProtection($id, $vol, $SOURCE_DATA);
		if (!$valid_protection)
		{
			$logging_obj->my_error_handler("INFO","Invalid post data $vol, hence returning from set_resync_required.", Log::BOTH);
			//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
			return $result;
		}
		
		$conn_obj->enable_exceptions();
		$conn_obj->sql_begin();
		/*
			in 4.0 we have to update  with resyncSetCxtimestamp then only auto resync will trigger 
			In 3.5.2 resyncSettimestamp field is using and time stamp values is sending by agent
		*/
		$timestamp=time();
		$src_vol_without_escape = $vol;
		$vol = $conn_obj->sql_escape_string ($vol);
		#4021 -- The resync required flag is setting to yes only if agent resync start time <= resyncRequestTimeStamp
	
		$sql = "select resyncStartTagtime, destinationHostId, destinationDeviceName, isQuasiFlag, deleted, replication_status, jobId, restartPause, src_replication_status, tar_replication_status from srcLogicalVolumeDestLogicalVolume where sourceHostId='$id' and sourceDeviceName='$vol' and destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";
		$resultSet = $conn_obj->sql_query($sql);
		$rowObject = $conn_obj->sql_fetch_object ($resultSet);
		$dest_host_id = $rowObject->destinationHostId;
		$dest_device_name = $rowObject->destinationDeviceName;
		$is_quasi_flag = $rowObject->isQuasiFlag;
		$deleted = $rowObject->deleted;
		$replication_status = $rowObject->replication_status;
		$job_id = $rowObject->jobId;
		$restartPause = $rowObject->restartPause;
		$src_replication_status = $rowObject->src_replication_status;
		$tar_replication_status = $rowObject->tar_replication_status;
		if ($resync_reason_details)
		{
			$resync_reason_code = $resync_reason_details['resyncReasonCode'];
			$resync_reason_code_detected_time = $resync_reason_details['detectionTime'];
			$host_info = $commonfunctions_obj->get_host_info($id);
			if ($host_info)
			{
				$placeholders = array();
				$placeholders['SourceHostName'] = $host_info['name'];
				$placeholders['SourceIpAddress'] = $host_info['ipaddress'];
				$placeholders['DiskId'] = $src_vol_without_escape;
				$placeholders['detectionTime'] = $resync_reason_code_detected_time;
				$disk_name = $commonfunctions_obj->get_disk_name($id, $vol, $host_info['osFlag']);
				$placeholders['DiskName'] = $disk_name;
				$placeholders_data = (count($placeholders)) ? serialize($placeholders) : '';
			}
		}
		
		
		$errorList = explode("&",$err);
		//Getting the resync request time stamp from the error message.
		if (is_array($errorList))
		{
			$requestTimeStamp = explode("=",$errorList[2]);
			if (is_array($requestTimeStamp))
			{
				$resyncRequestTimeStamp = $requestTimeStamp[1];
			}
			/*
			*a) The error code is sending as the last parameter as part of the error string and is "errcode = <code>".
			*b) The lun resize notification at CX will be taken based on the error code from 5.0.
			*/
			if(isset($errorList[4]) && $errorList[4]!=NULL)
			{
				$error_code_list = explode("=",$errorList[4]);
				/* For fabric lun resize the error code is defined as 6 .*/
				if ($error_code_list[1] == 6)
				{
					$query = "select * from logicalVolumes where hostId = '$id' and  deviceName = '$vol'";
					$deviceData = $conn_obj->sql_query($query);
					$device_properties_obj = $conn_obj->sql_fetch_object($deviceData);
					/*
					* In case of fabric as soon as the lun resize done, it won't send the new capacity. 
					*The new_capacity_send set as false in case of fabric, true for host volume.
					*/
					$new_capacity_send = FALSE;
					$write_agent_log = FALSE;
					
					$phy_lun_id = $device_properties_obj->Phy_Lunid;
					$pairType = $vol_obj->get_pair_type($phy_lun_id);
					if($pairType != 3)
					{
						volumeResizeNotification($device_properties_obj, 0, $new_capacity_send);
					}
				}
			}
			
		}

		if (($rowObject->resyncStartTagtime!=0) && ($resyncRequestTimeStamp!=0)) 
		{
			if ($rowObject->resyncStartTagtime <= $resyncRequestTimeStamp)
			{
				$optionals = ", resyncSetCxtimestamp=$timestamp,ShouldResyncSetFrom = 2";
				$cond = " and resyncSetCxtimestamp = 0 and ShouldResyncSetFrom <> 1 and destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'";
	
				$err = $err."(Resync required time stamp at $resyncRequestTimeStamp)";
				
				$update_should_resync = "update srcLogicalVolumeDestLogicalVolume " .
					"set shouldResync = $isResyncRequired $optionals where sourceHostId = '" .
					$id . "' and  sourceDeviceName = '" . $vol . "' $cond";
				$is_protection_rollback_progress = $commonfunctions_obj->is_protection_rollback_in_progress($id);
				if (($is_quasi_flag == 2 and $deleted == 0) 
				|| ($restartPause == 0 && ($replication_status == 41 || $replication_status == 26) && ($deleted == 0) && $is_quasi_flag != 2)
				|| ($is_protection_rollback_progress == 1 && $deleted == 0)
				)
				{
					$conn_obj->sql_query( $update_should_resync);
					if ($resync_reason_details)
					{
						//Raise event and add the resync code to resync code history table, if disk is in diff sync.
						$vol_obj->resync_error_code_action($id,$src_vol_without_escape,$dest_host_id, $dest_device_name, $job_id, $SOURCE_AGENT, $resync_reason_code,$resync_reason_code_detected_time);
						$commonfunctions_obj->updateAgentAlertsDbV2($RESYNC_REQD, $id, $resync_reason_code.$src_vol_without_escape, $resync_reason_code, $resync_reason_code, $placeholders_data, $FATAL, $ZERO, $SOURCE_AGENT_COMPONENT);
					}
					$logging_obj->my_error_handler("INFO",array("Sql"=>$update_should_resync),Log::BOTH);
				}
				elseif (($job_id == 0) || ($replication_status == 42) ||  ($deleted == 1) || ($restartPause == 1))
				{
					$do_pause = FALSE;
				}
				else
				{
					$do_pause = TRUE;
				}
				if ($write_agent_log == TRUE)
				{
					update_agent_log($id, $vol, $err, $agent_timestamp);
				}	
				$logging_obj->my_error_handler("INFO",array("HostId"=>$id, "SourceDeviceName"=>$src_vol_without_escape,"DestinationHostId"=>$dest_host_id,"DestinationDeviceName"=>$dest_device_name,"DoPause"=>$do_pause, "JobId"=>$job_id, "ReplicationStatus"=>$replication_status, "Deleted"=>$deleted, "PairStatus"=>$is_quasi_flag, "SrcReplicationStatus"=>$src_replication_status, "TarReplicationStatus"=>$tar_replication_status, "RestartPause"=>$restartPause, "RollbackState"=>$is_protection_rollback_progress),Log::BOTH);
	
				if ($do_pause)
				{					
					$result = $vol_obj->pause_replication($id, $src_vol_without_escape, $dest_host_id, $dest_device_name,1,0,"resync required");
					if ($resync_reason_details)
					{
						//If disk is in IR/Resync and means not in diff sync, then raise only event
						$commonfunctions_obj->updateAgentAlertsDbV2($RESYNC_REQD, $id, $resync_reason_code.$src_vol_without_escape, $resync_reason_code, $resync_reason_code, $placeholders_data, $FATAL, $ZERO, $SOURCE_AGENT_COMPONENT);
					}
				}
			}
		}
		$conn_obj->sql_commit();
	}
	catch(SQLException $e)
	{
		$conn_obj->sql_rollback();
		$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>FALSE),Log::BOTH);
		$result = FALSE;
	}
	$conn_obj->disable_exceptions();
	return $result;
}

function set_resync_required_oa ($destHostId, $destDev, $isResyncRequired ,$origin,$err, $resync_reason_details="")
{
/* Sample data target sends to CX:
vxstub.log:13:2015-12-12 11:55:50 (UTC) CX :INFO:set_resync_required_oa:: destHo
stId: 736808ED-70DB-2840-96ACEA5C16B915A8, destDev: 36000c295048ba9c1a5af1cc33e0
cc09a, isResyncRequired: 1, err: Few files are missing for 36000c295048ba9c1a5af
1cc33e0cc09a
vxstub.log:20:2015-12-12 11:55:50 (UTC) CX :INFO:set_resync_required_oa:: update
_should_resync: UPDATE srcLogicalVolumeDestLogicalVolume SET shouldResync = 1 ,
ShouldResyncSetFrom = 1 , resyncSetCxtimestamp=1449921350  WHERE destinationHost
Id='736808ED-70DB-2840-96ACEA5C16B915A8' AND  destinationDeviceName='36000c29504
8ba9c1a5af1cc33e0cc09a' AND destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C
79236A' AND shouldResync != 3
vxstub.log:23:2015-12-12 11:55:53 (UTC) CX :INFO:set_resync_required_oa:: update
d agent log
*/
global $conn_obj;
global $commonfunctions_obj, $vol_obj;
global $logging_obj, $MASTER_TARGET;
$result = TRUE;
try
{
		$conn_obj->enable_exceptions();
		$logging_obj->my_error_handler("INFO",array("DestinationHostId"=>$destHostId,"DestinationDeviceName"=>$destDev, "IsResyncRequired"=>$isResyncRequired, "Reason"=>$err, "Origin"=>$origin,"Reasoncode_details"=>print_r($resync_reason_details,TRUE)),Log::BOTH);
		$dest_host_name = $commonfunctions_obj->getHostName($destHostId);
		$dest_ip_address = $commonfunctions_obj->getHostIpaddress($destHostId);
		$err = $err."<br> Destination Host : $dest_host_name <br>";
		$err = $err."Destination IP : $dest_ip_address";
		update_agent_log($destHostId, $destDev,$err,0,$origin);	
		$conn_obj->sql_begin();
		$updateAgentLog = 0;

		$result = $vol_obj->setResyncRequiredToPair($destHostId, $destDev, $isResyncRequired ,$origin,$err, $updateAgentLog, $resync_reason_details, $MASTER_TARGET);
			
		$conn_obj->sql_commit();
	}
	catch(SQLException $e)
	{
		$conn_obj->sql_rollback();
		$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK","Result"=>FALSE),Log::BOTH);
		$result = FALSE;
	}
	$conn_obj->disable_exceptions();		
    return $result;
}

function update_stats_resync( $id, $deviceName, $vol_offset, $length, $matched, $jobid, $action1, $file1, $file2, $action2, $file3, $file4, $callfrom=0)
{
   	global $REPLICATION_ROOT;
	global $commonfunctions_obj;
	global $conn_obj;
	global $vol_obj,$logging_obj;
	
	$stats_update_flag = FALSE;
   # debugLog("$id, $deviceName, $vol_offset, $length, $matched, $jobid, $action1, $file1, $file2, $action2, $file3, $file4, $callfrom");
    update_last_outpostagent_update_timestamp( $id, $deviceName);

    # Return if job id does not match
    $iter = $conn_obj->sql_query("select jobId from srcLogicalVolumeDestLogicalVolume map ".
            "where destinationHostId ='$id' and  destinationDeviceName='".$conn_obj->sql_escape_string ($deviceName)."'");
    if ($myrow = $conn_obj->sql_fetch_row ($iter))
    {
      if (($myrow[0] != $jobid) && ($jobid))
      {
        
        return 4;
      }
    }

    $iter = $conn_obj->sql_query(
	"select resyncEndTime, sourceHostId, sourceDeviceName,lastResyncOffset " .
	"from srcLogicalVolumeDestLogicalVolume " .
	"where destinationHostId = '" . $id . "' " .
	"and  destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName) . "'");

	$rename_delete_done = false;
    $renamed_or_deleted = FALSE;

    while( $myrow = $conn_obj->sql_fetch_row($iter) ) {
		# While() will handle clustered case.
		#if( $myrow = $conn_obj->sql_fetch_row( $iter ) ) {
		$stats_update_flag = TRUE;
		$lastresyncoffset=$myrow[3];
		# This is not a good enough test looks like
		$compatiblityNo = get_host_compatibility($myrow[1],$id);
		$source_compatibilityno =  $commonfunctions_obj->get_compatibility($myrow[1]);
		$target_compatibilityno =  $commonfunctions_obj->get_compatibility($id);
		if (( 0 == $myrow[ 0 ] )||($callfrom == 1)) 		
		{			
			$logging_obj->my_error_handler("DEBUG","Length->$length");			
			$sql2 = "select capacity " .
			"from logicalVolumes " .
			"where hostId = '" . $myrow[1] . "' " .
			"and  deviceName = '".$conn_obj->sql_escape_string($myrow[2])."'";

			$iter2 = $conn_obj->sql_query( $sql2 );
			#debugLog("-----\n $sql2 \n");
			$myrow2 = $conn_obj->sql_fetch_row( $iter2 );
			$srcDeviceCapacity = $myrow2[0];

	  		$sql3 = "select lastResyncOffset,resyncEndTagtime " .
		  	"from srcLogicalVolumeDestLogicalVolume " .
		  	"where destinationHostId = '" . $id . "' and " .
		  	" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName) . "' and " .
                  	"sourceHostId = '" . $myrow[1] . "' and " .
                  	" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]) . "'";
          
			$iter3 = $conn_obj->sql_query( $sql3 );
	  		$myrow3 = $conn_obj->sql_fetch_row( $iter3 );
			$lastResyncOffset = $myrow3[0];
			$resyncEndTagtime = $myrow3[1];
			#5068:-Added condition $resyncEndTagtime!=0 so that resyncEndTime is updated only when resyncEndTagTime is received by the CX from the agent
	  		if ($lastResyncOffset == $srcDeviceCapacity and $lastResyncOffset != 0 and $srcDeviceCapacity!=0 and $resyncEndTagtime!=0) 
			{
				# Now turn off resync for this replication pair and start
				# differential mode
	        		$sql4 = "update logicalVolumes " .
	          		"set doResync = 0 " .
	          		"where (hostId = '" . $myrow[1]. "' and " .
	          		" deviceName = '" . $conn_obj->sql_escape_string($myrow[2]). "') or " .
	          		"(hostId = '" . $id. "' and " .
	          		" deviceName = '" . $conn_obj->sql_escape_string ($deviceName). "')";

                		$sql8 =
                  		"update srcLogicalVolumeDestLogicalVolume " .
                  		"set shouldResync = 0 " .
                  		"where sourceHostId = '" . $myrow[1]. "' and " .
                  		" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
                  		"destinationHostId = '" . $id. "' and " .
                  		" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "' and ".
                  		"shouldResync <> 2 and isResync = 1";

                		$sql9 =
                  		"update srcLogicalVolumeDestLogicalVolume " .
                  		"set shouldResync = 1 " .
                  		"where sourceHostId = '" . $myrow[1]. "' and " .
                  		" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
                  		"destinationHostId = '" . $id. "' and " .
                  		" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "' and ".
                  		"shouldResync = 2 and isResync = 1";

                		$sql7 =
                  		"update srcLogicalVolumeDestLogicalVolume " .
                  		"set isResync = 0 " .
                  		"where sourceHostId = '" . $myrow[1]. "' and " .
                  		" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
                  		"destinationHostId = '" . $id. "' and " .
                  		" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";

                		$sql6 =
                  		"update logicalVolumes " .
                  		"set capacity = '" . $srcDeviceCapacity. "'".
                  		"where hostId = '" . $id. "' and ".
                  		" deviceName ='" . $conn_obj->sql_escape_string ($deviceName). "'";

	        		$sql5 =
	          		"update srcLogicalVolumeDestLogicalVolume " .
	          		"set lastResyncOffset = 0, " .
	          		"differentialStartTime = now(), " .
	          		"resyncEndTime = now() " .
                  		"where sourceHostId = '" . $myrow[1]. "' and " .
	          		" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
	          		"destinationHostId = '" . $id. "' and " .
	          		" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";
					#Checking for backward compatiblity and changing the pair state to resync step1 to resync step2
				//$compatiblityNo = get_host_compatibility($myrow[1],$id);
				if($compatiblityNo >= 351000)
				{
					# Moving the replication pair from Resync Step I to Resync Step II	
					#The below if block is added by Srinivas Patnana to fix #4075. Cluster passive node pair is not getting resyncEndTime. So, to do equal updates for all the cluster nodes, we are going to get all the cluster source node pairs and trying to update each node pair in the database.
					if ( is_device_clustered($myrow[1], $myrow[2]) )
					{
						$query = "select sourceHostId,sourceDeviceName from srcLogicalVolumeDestLogicalVolume where destinationHostId = '" . $id. "' and " ." destinationDeviceName = '" .$conn_obj->sql_escape_string ($deviceName). "'";	
						$resultSet = $conn_obj->sql_query($query);
						$nRows = $conn_obj->sql_num_row($resultSet);
						while ($myObject = $conn_obj->sql_fetch_object($resultSet))
						{
							$sql5_1 =
	       		   	"update srcLogicalVolumeDestLogicalVolume " .
			          	"set resyncEndTime = now() " .
              		   	"where sourceHostId = '" . $myObject->sourceHostId. "' and " .
			          	" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myObject->sourceDeviceName). "' and " .
	       		   	"destinationHostId = '" . $id. "' and " .
			          	" destinationDeviceName = '" .$conn_obj->sql_escape_string ($deviceName). "'";
							$conn_obj->sql_query($sql5_1);
						}
					}
					else  //non cluster pair update
					{
						$sql5_1 =
						"update srcLogicalVolumeDestLogicalVolume " .
							"set resyncEndTime = now() " .
							"where sourceHostId = '" . $myrow[1]. "' and " .
							" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
						"destinationHostId = '" . $id. "' and " .
							" destinationDeviceName = '" .$conn_obj->sql_escape_string ($deviceName). "'";
							$conn_obj->sql_query($sql5_1);
					}				

	     		}
				else	# Moving the pair from Resync state to Differential sync if not Backward compatible.
				{
	                	if (is_one_to_many_by_source ($myrow[1], $myrow[2]))	
       	         		{
              	      			$conn_obj->sql_query ("update srcLogicalVolumeDestLogicalVolume ".
                     	            	"set resyncVersion = 0 where ".
                           		      	"sourceHostId = '" . $myrow[1]. "' and " .
                                 		" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
	                                 	"destinationHostId = '" . $id. "' and " .
       	                          	" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'");
              	  		}
        	 			$conn_obj->sql_query($sql4);
	 	       		$conn_obj->sql_query($sql5);
	       			$conn_obj->sql_query($sql8);
					$conn_obj->sql_query($sql9);
	      	       		$conn_obj->sql_query($sql7);
				}
	              		$conn_obj->sql_query($sql6);
	  		}
			
			// To change state from resync step I to step II

					$qry = "select resyncEndTime,resyncEndTagtime from srcLogicalVolumeDestLogicalVolume ".
              		   	"where sourceHostId = '" . $myrow[1]. "' and " .
			          	" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
	       		   	"destinationHostId = '" . $id. "' and " .
			          	" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";
                  			$qry_rs = $conn_obj->sql_query($qry);
			        	$resyncET = 	$conn_obj->sql_fetch_row( $qry_rs );
					//$compatiblityNo = get_host_compatibility($myrow[1],$id);
					if (($resyncET[0]!=0)&&($resyncET[1]!=0)&& $compatiblityNo >= 351000)
					{

					# Moving the replication pair from Resync Step I to Resync Step II	
					#The below if block is added by Srinivas Patnana to fix #4156 & #4075. Cluster passive node pair is not getting lastResyncOffset, isQuasiflag and quasidiffStarttime. So, to do equal updates for all the cluster nodes, we are going to get all the cluster source node pairs and trying to update each node pair in the database.
					if ( is_device_clustered($myrow[1], $myrow[2]) )
					{
						$query = "select sourceHostId,sourceDeviceName from srcLogicalVolumeDestLogicalVolume where destinationHostId = '" . $id. "' and " ." destinationDeviceName = '" .$conn_obj->sql_escape_string ($deviceName). "'";	
						$resultSet = $conn_obj->sql_query($query);
						$nRows = $conn_obj->sql_num_row($resultSet);
						while ($myObject = $conn_obj->sql_fetch_object($resultSet))
						{
							$sql5_2 =
		       		   	"update srcLogicalVolumeDestLogicalVolume " .
				          	"set lastResyncOffset = 0 " .
								"where sourceHostId = '" . $myObject->sourceHostId. "' and " .
			          	" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myObject->sourceDeviceName). "' and " .
	       		   		"destinationHostId = '" . $id. "' and " .
				          	" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";
		
			           		$conn_obj->sql_query($sql5_2);

				
				   			$query5_1 = "update srcLogicalVolumeDestLogicalVolume set isQuasiflag = 1, quasidiffStarttime = now() ".
								"where sourceHostId = '" . $myObject->sourceHostId. "' and " .
								" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myObject->sourceDeviceName). "' and " .
	       					      "destinationHostId = '" . $id. "' and " .
			       				" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";

					   		$conn_obj->sql_query($query5_1);

						}
					}
					else  //non cluster pair update
					{
						$sql5_2 =
		       		   	"update srcLogicalVolumeDestLogicalVolume " .
				          	"set lastResyncOffset = 0 " .
              			   	"where sourceHostId = '" . $myrow[1]. "' and " .
			       	   	" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
	       		   		"destinationHostId = '" . $id. "' and " .
				          	" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";
		
			           		$conn_obj->sql_query($sql5_2);

				
				   		$query5_1 = "update srcLogicalVolumeDestLogicalVolume set isQuasiflag = 1, quasidiffStarttime = now() ".
		                   			      "where sourceHostId = '" . $myrow[1]. "' and " .
		  			       	      " sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
	       					      "destinationHostId = '" . $id. "' and " .
			       				" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";

					   	 $conn_obj->sql_query($query5_1);

					}
		   		  }
			// End of code to change state from resync step I to step II
	      	}
		else 
		{
	  		# if callfrom = 2, the replication pair moves to Resync Step II to Differential Sync	
	  		if($callfrom == 2)
	  		{

	        		$sql4 =
        	  		"update logicalVolumes " .
	          		"set doResync = 0 " .
	          		"where (hostId = '" . $myrow[1]. "' and " .
	          		" deviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "') or " .
	          		"(hostId = '" . $id. "' and " .
	          		" deviceName = '" . $conn_obj->sql_escape_string ($deviceName). "')";
						
						/* 
						*This is the fix to keep the resync required as YES, after the pair
						* moved to diff sync, if the volume resize is done on resyncing time.
						*/
						//$vol_resize_status = $vol_obj->is_volume_resize_done($myrow[1], $myrow[2]);
						#11777 Fix
						$sql = "SELECT
										ShouldResyncSetFrom
									FROM
										srcLogicalVolumeDestLogicalVolume
									WHERE
										sourceHostId = '" . $myrow[1]. "'
									AND
										sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "'
									AND
										destinationHostId = '" . $id. "'
									AND
										destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";
									
						$should_resyncquery = $conn_obj->sql_query($sql);
						$resync_row = $conn_obj->sql_fetch_row($should_resyncquery);
						$ShouldResyncSetFrom = $resync_row[0];			
						//$logging_obj->my_error_handler("DEBUG","update_stats_resync:: sql: $sql, resync_row: $resync_row & ShouldResyncSetFrom: $ShouldResyncSetFrom \n");
						//if ($vol_resize_status != TRUE)
						if(!$ShouldResyncSetFrom)
						{
							$sql8 =
							"update srcLogicalVolumeDestLogicalVolume " .
							"set shouldResync = 0 , ShouldResyncSetFrom = 0, resyncSetCxtimestamp = 0 " .
							"where sourceHostId = '" . $myrow[1]. "' and " .
							" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
							"destinationHostId = '" . $id. "' and " .
							" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "' and ".
							"shouldResync <> 2 and isResync = 1";
						}

                		$sql9 =
                  		"update srcLogicalVolumeDestLogicalVolume " .
                  		"set shouldResync = 1 " .
                  		"where sourceHostId = '" . $myrow[1]. "' and " .
                  		" sourceDeviceName = '" . $conn_obj->sql_escape_string($myrow[2]). "' and " .
                  		"destinationHostId = '" . $id. "' and " .
                  		" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "' and ".
                  		"shouldResync = 2 and isResync = 1";

                		$sql7 =
                  		"update srcLogicalVolumeDestLogicalVolume " .
                  		"set isResync = 0, differentialStartTime = now() " .
                  		"where sourceHostId = '" . $myrow[1]. "' and " .
                  		" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
                  		"destinationHostId = '" . $id. "' and " .
                  		" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'";
						
						#$logging_obj->my_error_handler("DEBUG","update_stats_resync:: src & target compatibilityno.'s are $source_compatibilityno and $target_compatibilityno \n");	

						/*  4369:: For 1-N fast/direct sync case we are
						 * not switching the pair to offload
						 * once pair reaches diff sync
						 * since at any point of time any number 
						 * of pairs can run with fast/direct sync */
						if(($source_compatibilityno < 550000) || ($target_compatibilityno < 550000))
						{
	                		if (is_one_to_many_by_source ($myrow[1], $myrow[2]))	
	                		{
								#$logging_obj->my_error_handler("DEBUG","update_stats_resync:: updating resyncVersion to 0 while switching from stepII to diff \n");
	                    			$conn_obj->sql_query ("update srcLogicalVolumeDestLogicalVolume ".
	                                 	"set resyncVersion = 0 where ".
	                                 	"sourceHostId = '" . $myrow[1]. "' and " .
	                                 	" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' and " .
	                                 	"destinationHostId = '" . $id. "' and " .
	                                 	" destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'");
	                		}
						}
				#debugLog("$sql4 \n $sql8 \n $sql9 \n $sql7 \n ");
				
				/* 4369:: For 1-N fast/direct sync case we are
				 * updating doResync value of logicalVolumes table
				 * only when all 1-N pairs reach diff sync */
				if(is_one_to_many_by_source ($myrow[1], $myrow[2]))
				{										
					$count = 0;
					$isresync_sql = "SELECT
										count(*)
									FROM 
										srcLogicalVolumeDestLogicalVolume ".
									"WHERE sourceHostId = '" . $myrow[1]. "' AND " .
									" sourceDeviceName = '" . $conn_obj->sql_escape_string ($myrow[2]). "' AND ".
									"isQuasiFlag != 2";
					$count_query = $conn_obj->sql_query($isresync_sql);
					$row = $conn_obj->sql_fetch_row($count_query);
					$count = $row[0];
					//$logging_obj->my_error_handler("DEBUG","update_stats_resync:: count query is $isresync_sql & count value is $count, $row[0]\n");					
					if($count == 0)
					{
						$conn_obj->sql_query($sql4);
					}
					$conn_obj->sql_query ("update logicalVolumes " .
	          		"set doResync = 0 " .
	          		"where hostId = '" . $id. "' and " .
	          		" deviceName = '" . $conn_obj->sql_escape_string ($deviceName). "'");
								
				}
				else
				{
					$conn_obj->sql_query($sql4);
				}				
				$conn_obj->sql_query($sql8);
				$conn_obj->sql_query($sql9);
   	       		$conn_obj->sql_query($sql7);
	  		}
			else
			{
	  			$conn_obj->sql_query( "update srcLogicalVolumeDestLogicalVolume " .
			       "set lastDifferentialOffset = '" . ($vol_offset + $length) . "' " .
			       "where destinationHostId = '" . $id . "' " .
			       "and  destinationDeviceName = '" . $conn_obj->sql_escape_string ($deviceName) . "'");
			}
		}
    	}
    	if(!$stats_update_flag)
    	{
       	  	return 4;
    	}
       return 0;
}

function update_stats_outpost( $id, $deviceName, $vol_offset, $length ,$apply_rate )
{
    global $conn_obj;

    update_apply_rate($id, $deviceName,$apply_rate);
      return 0;
}

/*
 * Function Name: update_apply_rate
 *
 * Description:
 *
 *    This function updates theapply rate sent by the target if its non
 *    zero	 
 *
 * Parameters:
 *    Param 1 [IN]:$id
 *    Param 2 [IN]:$deviceName
 *    Param 3 [IN]:$apply_rate
 *
 * Return Value:
 *    Ret Value: Returns boolean
 *
 * Exceptions:
 *    DataBase Connection fails.
*/
function update_apply_rate($id, $deviceName, $apply_rate)
{
	global $conn_obj;
	global $logging_obj;
	
	$query = "SELECT 
				differentialApplyRate,
				differentialPendingInTarget
			  FROM  
				srcLogicalVolumeDestLogicalVolume  
			  WHERE 
				destinationHostId = ? AND 
				destinationDeviceName = ?";
	$pair_result = $conn_obj->sql_query($query, array($id, $deviceName));
	if (is_array($pair_result) && count($pair_result) == 0)
	{
		//As the post data of given devicename not exists, hence returning from here.
		$logging_obj->my_error_handler("INFO","Invalid post data $deviceName in method vxstub_update_outpost_agent_status(update_apply_rate).", Log::BOTH);
		//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
		return true;
	}
	
	foreach($pair_result as $key=>$row)
	{
		$applyRate_old = intval($row['differentialApplyRate']);
		$trgDiffs = intval($row['differentialPendingInTarget']);
	}
	$applyRate_new = $apply_rate;
	if($apply_rate == 0 && $trgDiffs == 0)
	{
		$applyRate_new = 0;
	}
	elseif($apply_rate == 0 && $trgDiffs != 0)
	{
		$applyRate_new = $applyRate_old;
	}
	#$logging_obj->my_error_handler('DEBUG',"applyRate_new::$applyRate_new");
	if ($applyRate_new != $applyRate_old)
	{
		$sql = "UPDATE 
					srcLogicalVolumeDestLogicalVolume 
				SET 	
					differentialApplyRate = ?
				WHERE 
					destinationHostId = ? AND 
					destinationDeviceName = ?";
		#$logging_obj->my_error_handler('DEBUG',"sql ::$sql ");
		$conn_obj->sql_query($sql, array($applyRate_new,$id,$deviceName));
	}
	return true;
}

function update_last_outpostagent_update_timestamp( $id, $deviceName)
{
	#1toN fast change
	global $conn_obj;

//    $conn_obj->sql_query( "update srcLogicalVolumeDestLogicalVolume " .
//                 "set lastOutpostAgentChange = now() " .
//                 "where destinationHostId = '" . $id . "'");

    $conn_obj->sql_query( "update logicalVolumes set lastDeviceUpdateTime=UNIX_TIMESTAMP(), lastOutpostAgentChange=now() " .
                 "where hostId = '" . $id . "' and  deviceName = '" . $conn_obj->sql_escape_string( $deviceName ) . "'");
    
	$my_current_time = time();
$conn_obj->sql_query( "update hosts set lastHostUpdateTime = $my_current_time where id = '" . $id . "'");

}

##
# Logs a debug string to a file.
##
function debugLog ( $debugString)
{
  /*global $PHPDEBUG_FILE;
  # Some debug
  $fr = fopen($PHPDEBUG_FILE, 'a+');
  if(!$fr) {
    #echo "Error! Couldn't open the file.";
  }
  if (!fwrite($fr, $debugString . "\n")) {
    #print "Cannot write to file ($filename)";
  }
  if(!fclose($fr)) {
    #echo "Error! Couldn't close the file.";
  }*/
}

##
# Logs a debug string to a file.
##
function debugLogFastSync($debugString)
{
  global $REPLICATION_ROOT;
  if (preg_match('/Windows/i', php_uname()))
  {
      $PHPDEBUG_FILE = "$REPLICATION_ROOT\\var\\fastsyncinfo.log";
  }
  else{
      $PHPDEBUG_FILE = "$REPLICATION_ROOT/var/fastsyncinfo.log";
  }  
  $date_time = date('Y-m-d H:i:s (T)');
  $debugString = $date_time .' '. $debugString;
  # Some debug
  $fr = fopen($PHPDEBUG_FILE, 'a+');
  if(!$fr) {
    #echo "Error! Couldn't open the file.";
  }
  if (!fwrite($fr, $debugString . "\n\n")) {
    #print "Cannot write to file ($filename)";
  }
  if(!fclose($fr)) {
    #echo "Error! Couldn't close the file.";
  }
}

$days = array ("SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT", "WKD", "WKE", "ALL", "ONE", "MTH", "YRL");
$daysn = array_flip ($days);

$run = array ("ALLDAY", "NEVER", "DURING", "NOTDUR");
$runn = array_flip ($run);

##
# Returns true if the given number is a listed ruleId / replication pair.  The default (0) is included

function is_ruleId ($ruleId)
{
  if ($ruleId == "0")
  {
    return true;
  }

  global $conn_obj;

  $result = $conn_obj->sql_query ("SELECT ruleId FROM srcLogicalVolumeDestLogicalVolume WHERE ruleId=\"$ruleId\"");

  if ($conn_obj->sql_num_row ($result) != 0)
  {
    return true;
  }

  return false;
}

##
# Returns true if the given rep pair can replicate at the given time (as a string "YYYY-MM-DD HH:MM:SS")
# An empty $datetime argument assumes the current time and date
# An invalid $datetime argument returns false

function can_replicate ($ruleId, $datetime="now")
{
  $date = time ();
  if (isset ($datetime) && strtotime ($datetime) == -1)
  {
    return false;
  }
  elseif (isset ($datetime))
  {
    $date = strtotime ($datetime);
  }

  if (!is_ruleId ($ruleId))
  {
    return false;
  }

  $result = check_pair_rules ($ruleId, $date);
  if ($result == 1)
  {
    return true;
  }
  elseif ($result == 0)
  {
    return false;
  }

  if ($ruleId > 0)
  {
    $result = check_pair_rules (0, $date);
    if ($result == 1)
    {
      return true;
    }
    elseif ($result == 0)
    {
      return false;
    }
  }

  return true;
}

##
# Checks a specific rep pair and time for replication permission.  Only to be called by can_replicate ();
# Returns 1 if defined and okay
# Returns 0 if defined and not okay
# Returns -1 if not defined

function check_pair_rules ($ruleId, $date)
{
  global $conn_obj;

  $rules = $conn_obj->sql_query ("SELECT * FROM rules WHERE ruleID=\"$ruleId\" ORDER BY rule DESC");

  while ($row = $conn_obj->sql_fetch_assoc ($rules))
  {
    $afd = affects_today ($row, $date);
    $aft = affects_time_of_day ($row, $date);
    if ($afd == 1)
    {
      if ($aft != -1)
      {
	return $aft;
      }
    }
  }

  return -1;
}

##
# Checks if the rule allows replication on the given day.  Only to be called by check_pair_rules ();
# Returns 1 if defined and okay
# Returns 0 if defined and not okay
# Returns -1 if not defined

function affects_today ($rule_row, $date)
{
  $today = strtotime (date ("Y-m-d", $date));
  $sd = strtotime ($rule_row ["startDate"]);
  $ed = strtotime ($rule_row ["endDate"]);
  if ($rule_row ["perpetual"] == "1" || ($sd <= $today && $today <= $ed))
  {
    if ($rule_row ["dayType"] == "ALL")
    {
      return 1;
    }
    elseif ($rule_row ["dayType"] == "ONE")
    {
      $date1 = strtotime ($rule_row ["singleDate"]);
      if ($date1 == strtotime (date ("Y-m-d", $date)))
      {
	return 1;
      }
    }
    elseif ($rule_row ["dayType"] == "YRL")
    {
      $date1 = strtotime ($rule_row ["singleDate"]);
      if ($date1 == strtotime (date ("0000-m-d", $date)))
      {
	return 1;
      }
    }
    elseif ($rule_row ["dayType"] == "MTH")
    {
      $date1 = strtotime ($rule_row ["singleDate"]);
      if ($date1 == strtotime (date ("0000-00-d", $date)))
      {
	return 1;
      }
    }
    elseif ($rule_row ["dayType"] == "WKD" && (1 <= date ("w", $date) && date ("w", $date) <= 5))
    {
      return 1;
    }
    elseif ($rule_row ["dayType"] == "WKE" && (0 == date ("w", $date) || date ("w", $date) == 6))
    {
      return 1;
    }
    elseif ($rule_row ["dayType"] == strtoupper (date ("D", $date)))
    {
      return 1;
    }
  }

  return -1;
}

##
# Checks if the rule allows replication for the given time of day.  Only to be called by check_pair_rules ();
# Returns 1 if defined and okay
# Returns 0 if defined and not okay
# Returns -1 if not defined

function affects_time_of_day ($rule_row, $date)
{
  $now = strtotime (date ("H:i:s", $date));
  $start = strtotime ($rule_row ["startTime"]);
  $end = strtotime ($rule_row ["endTime"]);

  if ($rule_row ["runType"] == "ALLDAY") { return 1; }
  elseif ($rule_row ["runType"] == "NEVER") { return 0; }
  elseif ($rule_row ["runType"] == "DURING")
  {
    if ($start > $end)
    {
      if (($now <= $end) || ($start <= $now))
      {
	return 1;
      }
    }
    elseif ($start < $end)
    {
      if (($now <= $end) && ($start <= $end))
      {
        return 1;
      }
    }
  }
  elseif ($rule_row ["runType"] == "NOTDUR")
  {
    if ($start > $end)
    {
      if (($now <= $end) || ($start <= $now))
      {
        return 0;
      }
    }
    elseif ($start <= $end)
    {
      if (($start <= $now) && ($now <= $end))
      {
        return 0;
      }
    }
  }

  return -1;
}

##
# Debugging function that writes a line to a temporary log (/tmp/mytrace.log).
# The file is used to see the sequence of events between PHP and the TM.
# This function prepends line with "php: " and appends a linefeed.

function mylog($line)
{
    #$file = fopen( "/tmp/mytrace.log", "ab" );
    #fputs( $file, "php: " . $line . "\n" );
    #fclose( $file );
}

##
# Record what version of the agent is being used

function update_agent_version( $host_id, $field, $version ) 
{
    global $conn_obj;
	global $multi_query_static;
    $sql = "update hosts set $field = '" .$conn_obj->sql_escape_string( $version ) . "' where id = '" . $conn_obj->sql_escape_string( $host_id ) . "'";
	$multi_query_static["update"][] = $sql;
}

function is_windows ()
{
    if (preg_match('/Windows/i', php_uname())) { return true; }
    else { return false; }
}

function is_linux ()
{
    if (preg_match('/Linux/i', php_uname())) { return true; }
    else { return false; }
}

function get_osFlag($hostId)
{
     global $conn_obj;
     $sql = " select osFlag from hosts where id ='".$hostId."'" ;
	 $iter = $conn_obj->sql_query ($sql);
     $myrow = $conn_obj->sql_fetch_row ($iter);
     $osFlag =$myrow[0];
	 return $osFlag;

}


function get_host_compatibility($srcid,$destid)
{
   global $conn_obj;
   $ids="";
   $version = 0;	
   if($srcid!='')
   {
	$ids = "'".$srcid."'";
   }
   if($destid!='')
   {
	if($srcid!="")
	{ $ids = $ids.","."'".$destid."'"; }
	else
	{ $ids = "'".$destid."'"; }
   }	
		
  $query = "select compatibilityNo from hosts where id in ($ids)";
  //debugLog($query)	;
  $iter = $conn_obj->sql_query($query);
  while ($myrow = $conn_obj->sql_fetch_row($iter))
  { $host_version[] = $myrow[0]; }

  $cnt = count($host_version);

  if($cnt == 1)
  {
	if($host_version[0] >= 351000)
		$version = 351000;
  }
  else
  {
	if($host_version[0] >= 351000 && $host_version[1] >= 351000)
		$version = 351000;
  }			 	
  return $version;
}

// to support full resync feature

function update_stats_resyncStartTag($id, $deviceName, $jobid, $tag_timestamp,$seq='')
{
   global $conn_obj,$logging_obj;
   
	$agentVersion = get_agent_version_from_hostid($id);

   $query = "select destinationHostId, destinationDeviceName from srcLogicalVolumeDestLogicalVolume where sourceHostId ='". $id ."'".
           " and  sourceDeviceName = '". $conn_obj->sql_escape_string ($deviceName) ."' and jobId = $jobid and isResync = 1";
  // debugLog($query );
   $rs1 = $conn_obj->sql_query($query);
   	if(!$rs1)
       {
		return 4;
       }
	while($myrow = $conn_obj->sql_fetch_row($rs1))
	{
		#3253
		if (!preg_match('/4-1-0_GA_/i', $agentVersion))
		{
			$iter = "update srcLogicalVolumeDestLogicalVolume set resyncSetCxtimestamp = 0 , ShouldResyncSetFrom = 0 , resyncStartTagtime = $tag_timestamp , resyncStartTagtimeSequence = $seq  ".
            		"where destinationHostId= '" . $myrow[0] . "' " ."and  destinationDeviceName = '" . $conn_obj->sql_escape_string ($myrow[1]) . "'";
			$logging_obj->my_error_handler("DEBUG","start Tag::".$iter);
			//debugLog("start Tag::".$iter);
		}
		else
		{
			 $iter = "update srcLogicalVolumeDestLogicalVolume set resyncStartTagtime = $tag_timestamp ". 	
            		"where destinationHostId= '" . $myrow[0] . "' " ."and  destinationDeviceName = '" . $conn_obj->sql_escape_string ($myrow[1]) . "'";
		}
	    $rs=  $conn_obj->sql_query($iter);
           if(! $rs)
          {
		return 4;
          }
		
	}	
       return 0;			
}

//Bug fix 3846, Updating the resync end time when the agent sends the resync end timestamp.
function update_stats_resyncEndTag($id, $deviceName, $jobid, $tag_timestamp,$seq='')
{

   global $conn_obj,$logging_obj;
  
   $agentVersion = get_agent_version_from_hostid($id);
  // $result = 4;
   $query1 = "select destinationHostId, destinationDeviceName from srcLogicalVolumeDestLogicalVolume where sourceHostId ='". $id ."'".
           " and  sourceDeviceName = '". $conn_obj->sql_escape_string ($deviceName) ."' and jobId = $jobid and isResync = 1 and resyncEndTagtime = 0";
   $rs = $conn_obj->sql_query($query1);
	while($myrow = $conn_obj->sql_fetch_row($rs))
	{
		#3253
		if (!preg_match('/4-1-0_GA_/i', $agentVersion))
		{
			$query2 = "update srcLogicalVolumeDestLogicalVolume set resyncEndTagtime = $tag_timestamp,resyncEndTagtimeSequence = $seq ".
     			   "where destinationHostId= '" . $myrow[0] . "' and resyncEndTagtime = 0 and   destinationDeviceName = '" . $conn_obj->sql_escape_string ($myrow[1]) . "'";
			$logging_obj->my_error_handler("DEBUG","End Tag".$query2);
			//debugLog("End Tag".$query2);
		}
		else
		{
			$query2 = "update srcLogicalVolumeDestLogicalVolume set resyncEndTagtime = $tag_timestamp ".
     			   "where destinationHostId= '" . $myrow[0] . "' and resyncEndTagtime = 0 and   destinationDeviceName = '" . $conn_obj->sql_escape_string ($myrow[1]) . "'";
		}
	
	    	$iter = $conn_obj->sql_query($query2);
		if ($tag_timestamp != 0 && $iter)
		{
			$result = update_stats_resync($myrow[0], $myrow[1],'','','',$jobid,'','','','','','',1);	
		}	
    }	
    
    return $result;
}

  /*
     * Function Name: update_stats_quasi
     *
     * Description:
     *
     *    This function updates quasi flag from resync step(II) to diff sync.
     *	 
     * Parameters:
     *    Param 1 [IN]:$id
     *    Param 2 [IN]:$deviceName
     *
     * Return Value:
     *    Ret Value: Returns boolean value
     *    
    */

function update_stats_quasi($id, $deviceName)
{
    global $conn_obj;

    $query = "UPDATE
                   srcLogicalVolumeDestLogicalVolume
               SET
                   isQuasiflag = 2,
                   quasidiffEndtime = now(),
				   throttleresync = 0,
				   statusUpdateTime = now(),
				   remainingResyncBytes = 0,
				   remainingQuasiDiffBytes = 0					
               WHERE
                   destinationHostId = '" . $id . "' and
                    destinationDeviceName = '" .$conn_obj->sql_escape_string ($deviceName) . "' and 
				   isQuasiflag = 1";
    $conn_obj->sql_query($query);
    $result_update_stats_resync = update_stats_resync($id, $deviceName,
										'','','','','','','','','','',2);    

    /* 
     * Update resync duration in policy violation table 
     */
	$query = "SELECT
                 sourceHostId,
                 sourceDeviceName,
                 resyncStartTime,
                 quasidiffEndtime,
				 oneToManySource
              FROM
                 srcLogicalVolumeDestLogicalVolume
              WHERE
                 destinationHostId = '" . $id . "' and 
                  destinationDeviceName = '" .$conn_obj->sql_escape_string ($deviceName) . "' 
              LIMIT 1";
    $result = $conn_obj->sql_query($query);
    $row = $conn_obj->sql_fetch_array($result);
    $sourceHostId = $row['sourceHostId'];
    $sourceDeviceName = $row['sourceDeviceName'];
    $resyncStartTime = $row['resyncStartTime'];
    $quasidiffEndtime = $row['quasidiffEndtime'];
	$oneToManySource = $row['oneToManySource'];
	
    $deviceName = $conn_obj->sql_escape_string($deviceName);
    
	if($oneToManySource)
	{
		$list = getProtectionHosts($sourceHostId, $sourceDeviceName);
		
		foreach ($list as $sourceHostId => $sourceDeviceName)
		{	    
			$sourceDeviceName = $conn_obj->sql_escape_string($sourceDeviceName);
		    $days = getDays($resyncStartTime,$quasidiffEndtime);
		   
		    $eventType = 'resync';
		    foreach($days as $day)
		    {
		        $tmp_st_time = $day[0];
		        $tmp_en_time = $day[1];
				
				$tmp_arr = explode(' ', $tmp_st_time);
				$evt_day = $tmp_arr[0];
		        
		        $sql = "SELECT 
		                    count(*) as num_entries 
		                FROM 
		                    policyViolationEvents
		                WHERE
		                    sourceHostId='$sourceHostId' AND
		                     sourceDeviceName='$sourceDeviceName' AND
		                    destinationHostId='$id' AND
		                     destinationDeviceName='$deviceName' AND
		                    eventDate='$evt_day' AND
		                    eventType='$eventType'" ;
							
				
		        $result = $conn_obj->sql_query($sql);
		        $row = $conn_obj->sql_fetch_array($result);
		        if($row['num_entries'])
		        {
		            $sql = "UPDATE
		                        policyViolationEvents
		                    SET
		                        duration=(duration+(unix_timestamp('$tmp_en_time') 
		                        - unix_timestamp('$tmp_st_time'))), 
		                        startTime='0000-00-00 00:00:00',
		                        numOccurrence=numOccurrence+1 
		                    WHERE
		                        sourceHostId='$sourceHostId' AND
		                         sourceDeviceName='$sourceDeviceName' AND
		                        destinationHostId='$id' AND
		                         destinationDeviceName='$deviceName' AND
		                        eventDate='$evt_day' AND
		                        eventType='$eventType'";
		        }
		        else
		        {
		            $sql = "INSERT 
		                    INTO 
		                        policyViolationEvents (
		                        sourceHostId,
		                        sourceDeviceName,
		                        destinationHostId,
		                        destinationDeviceName,
		                        eventDate,
		                        startTime,
		                        duration,
		                        eventType,
		                        numOccurrence)
		                    VALUES (
		                        '$sourceHostId',
		                        '$sourceDeviceName',
		                        '$id',
		                        '$deviceName',
		                        '$evt_day',
		                        '0000-00-00 00:00:00', 
		                        (unix_timestamp('$tmp_en_time') 
		                        - unix_timestamp('$tmp_st_time')),
		                        '$eventType',
		                        '1')";
		        }
		        
		        $conn_obj->sql_query($sql);
		    }
		}
	}
  send_mail_trap_for_diffsync($id, $deviceName); 
  
  return $result_update_stats_resync;
}

function getProtectionHosts($srchostid, $srcvol)
{
	global $conn_obj;
	$source_vol = $conn_obj->sql_escape_string($srcvol);
	$host_list = array();
	
	$sql = "SELECT 
				count(*) as num_entries 
			FROM 
				clusters
			WHERE
				hostId='$srchostid' AND
				 deviceName='$source_vol'" ;

	$result = $conn_obj->sql_query($sql);
	$row = $conn_obj->sql_fetch_array($result);       
	
	if($row['num_entries'] > 0)
	{
		$sql = "SELECT 
					clusterId 
				FROM 
					clusters 
				WHERE 
					hostId='$srchostid' AND
					 deviceName='$source_vol'";
					
		$result = $conn_obj->sql_query($sql);
		$row = $conn_obj->sql_fetch_array($result);
		$clusterId = $row['clusterId'];
			
		$sql = "SELECT 
					hostId,
					deviceName 
				FROM
					clusters
				WHERE
					 deviceName='$source_vol' AND
					clusterId = '$clusterId'";

		$result = $conn_obj->sql_query($sql);

		while($row = $conn_obj->sql_fetch_array($result))
		{
			$host_list[$row['hostId']] = $row['deviceName'];
		}
	}
	else
	{
		$host_list[$srchostid] = $srcvol;
	}

	return $host_list;
}

function getDays($dt1,$dt2)
{
	$dt_arr =  preg_split("/[-:\s]/",$dt1);
	$st_dt = mktime($dt_arr[3],$dt_arr[4],$dt_arr[5],$dt_arr[1],$dt_arr[2],$dt_arr[0]);
	
	$dt_arr =  preg_split("/[-:\s]/",$dt2);
	$en_dt = mktime($dt_arr[3],$dt_arr[4],$dt_arr[5],$dt_arr[1],$dt_arr[2],$dt_arr[0]);	
	
	if($en_dt < $st_dt)
	{
		$tmp_dt = $en_dt;
		$en_dt = $st_dt;
		$st_dt = $tmp_dt;
	}

	for($i = $st_dt; $i <= $en_dt; $i += 43200)
	{
		$tmp_dt = date('Y-m-d', $i);
		$dates[$tmp_dt] = 1;
	}
	
	$tmp_dt = date('Y-m-d', $en_dt);
	$dates[$tmp_dt] = 1;
	
	$new_dt = '';
	foreach($dates as $date => $val)
	{
		if(date('Y-m-d', $st_dt) == $date && date('Y-m-d', $en_dt) == $date)
		{
			$start = date('Y-m-d H:i:s', $st_dt);
			$end = date('Y-m-d H:i:s', $en_dt);
		}		
		elseif(date('Y-m-d', $st_dt) == $date)
		{
			$start = date('Y-m-d H:i:s', $st_dt);
			$end = date('Y-m-d 23:59:59', $st_dt);
		}
		elseif(date('Y-m-d', $en_dt) == $date)
		{
			$start = date('Y-m-d 00:00:00', $en_dt);
			$end = date('Y-m-d H:i:s', $en_dt);
		}
		else
		{
			$start = $date.' 00:00:00';
			$end = $date.' 23:59:59';
		}
		
		//echo("$date: $start, $end<br>");
		
		//$new_dt[] = array($start, $end);
		$new_dt = array(array($start, $end));
	}
	
	return $new_dt;
}

//Below is the function added by Srinivas Patnana to fix #3035  and the function do the job of coverting the complete array data to lower case.
function arrayToLower($arr)
{
	if (is_array($arr))
	{
		foreach($arr as $key=>$val)
		{
			$lowArr[] = strtolower($val);
		}
	}
	return $lowArr;
}
//End of the function.


function updateMountPoint($destHostId,$destDev,$mountpoint)
{
	global $conn_obj;
	
	$iter = "update logicalVolumes set mountPoint = ? where hostId = ? and deviceName = ?";
	$rs=  $conn_obj->sql_query($iter, array($mountpoint, $destHostId, $destDev));		
}

#Duplicate function of change_rep_history_status from retentionFunctions.php( to fix #2700)
function change_rep_history_status_alias ($src_guid, $src_dev, $dest_guid, $dest_dev)
{
    global $conn_obj;
    # make sure to handle cluster so get all ruleids and matching source ids
    $sqlStmt = "select ruleId, sourceHostId ".
        "from srcLogicalVolumeDestLogicalVolume ".
        "where destinationHostId = '$dest_guid' and  destinationDeviceName = '$dest_dev'";
    
    $rs = $conn_obj->sql_query($sqlStmt);
    if (!$rs) {
        # report error
        return;
    }

    if ($conn_obj->sql_num_row($rs) == 0) {
        # most likely trying to update histry for a cluster node that was
        # already done 
        return;
    }
    
    $volumeGroupId = get_volumegroup_id_alias($dest_guid, $dest_dev);
    if (!$volumeGroupId) {
        # report error
        return;
    }
    
    $escSrcDev = $conn_obj->sql_escape_string( $src_dev );
    $escDstDev = $conn_obj->sql_escape_string( $dest_dev );

    $row = $conn_obj->sql_fetch_row($rs);
    $values = "($row[0], '$row[1]', '$escSrcDev', '$dest_guid', '$escDstDev', now(), '$volumeGroupId')";
    while ($row = $conn_obj->sql_fetch_row($rs)) {
        $values = $values . ", ($row[0], '$row[1]', '$escSrcDev', '$dest_guid', '$escDstDev', now(), '$volumeGroupId')";
    }

    $sqlret = "insert into replicationHistory " .
        "(`ruleId`,`sourceHostId`, `sourceDeviceName`, `destinationHostId`, " .
        "`destinationDeviceName`, `createdDate`,`volumeGroupId`) " .
        "values $values";

    $conn_obj->sql_query($sqlret);
}

#Duplicate function of get_volumegroup_id_alias from retentionFunctions.php( to fix #2700))
function get_volumegroup_id_alias($dest_guid, $dest_dev)
{
	global $conn_obj;
	$sql = "select volumeGroupId from srcLogicalVolumeDestLogicalVolume where destinationHostId='$dest_guid' and  destinationDeviceName='$dest_dev'";
       $rs = $conn_obj->sql_query($sql);

       $ret_nrows = $conn_obj->sql_num_row($rs);
	$volumeGroupId="";
       if ($ret_nrows != 0){
	       $fetch_res     = $conn_obj->sql_fetch_array($rs);
    		$volumeGroupId = $fetch_res['volumeGroupId']; 
	}	
	return $volumeGroupId;
}


/*
     * Function Name: get_pause_pending_mentainance_trg_info
     *
     * Description:
     *
     *    This function is used to construct the string,which is sent to 
     *	 target agent for pause. 
     *    
     * Parameters:
     *    Param 1 [IN]:$source_id
     *    Param 2 [IN]:$source_device_name
     *	Param 3 [IN]:$dest_id
     *	Param 4 [IN]:$dest_device_name
     *    Param 1 [OUT]:$result
     *
     * Return Value:
     *    Ret Value: Returns String
     *
     * Exceptions:
     *    DataBase Connection fails.
*/
function get_pause_pending_maintenance_trg_info($tar_replication_status,$pauseActivity,$ruleId,$dest_device_name, $dest_id, $osFlag, $retention_array, $pause_status)
{
    /*define global parameters*/
    
    global $conn_obj,$logging_obj,$commonfunctions_obj,$vol_obj,$sparseretention_obj,$retention_obj;
    global $PAUSED_PENDING,$PAUSE_COMPLETED,$PAUSE_BY_RETENTION,$PAUSE_BY_USER,$PAUSE_BY_INSUFFICIENT_SPACE;
    $storagePath = "";
    $storageDeviceName = "";
    $moveRetentionPath = "";
    $result="";

    /*default values*/
    $pause_components         ="no";
    $components               = "";
    $pause_components_status  = "";
    $pause_components_message = "";    $result="pause_components=$pause_components;components=$components;pause_components_status=$pause_components_status;pause_components_message=$pause_components_message;";

    $pauseActivity_arr = explode(',',$pauseActivity);
    $log_details = get_retention_log_details($retention_array, $ruleId);    

    $storagePath = $log_details['storage_path'];
    $storageDeviceName = $log_details['storage_device_name'];
    $moveRetentionPath = $log_details['moveRetentionPath'];
    $catalogPath = $log_details['catalogPath'];
	$sparseEnable = $log_details['sparse_enable'];
	$uniqueId = $log_details['unique_id'];

    if(($tar_replication_status) == 1)
    {
        if(($pause_status == $PAUSED_PENDING) || ($pause_status == $PAUSE_COMPLETED))
        { 
            $pause_components         ="yes";
            if(in_array($PAUSE_BY_USER,$pauseActivity_arr))
            {
				$components           = "all";
            }
            if(($pauseActivity == $PAUSE_BY_RETENTION)||($pauseActivity == $PAUSE_BY_INSUFFICIENT_SPACE))
            {
                $components           = "affected";
            }
        }
        if($pause_status == $PAUSED_PENDING)
        {
            $pause_components_status  = "requested";
        }
        if($pause_status == $PAUSE_COMPLETED)
        {
            $pause_components_status  = "completed";
        }
        /*contruct the request string*/    
		$result="pause_components=$pause_components;components=$components;pause_components_status=$pause_components_status;pause_components_message=$pause_components_message;";
		
        /*Check if move retention is applicable*/
        
        if((($moveRetentionPath !='') || ($moveRetentionPath != 'NULL')) 
         && (in_array($PAUSE_BY_RETENTION,$pauseActivity_arr)))
         {
            $move_retention          ="yes";
            
            if($osFlag == 1)
            {
                $catalogPath_arr = explode ('\\',$catalogPath);
                array_pop($catalogPath_arr);
                $catalogPath = implode('\\',$catalogPath_arr);
            }
            else
            {
                $catalogPath_arr = explode('/',$catalogPath);
                array_pop($catalogPath_arr);
                $catalogPath = implode('/',$catalogPath_arr);
            }
            $new_catalog_path = substr($catalogPath,(strlen($storagePath)));
            
            $move_retention_old_path = $catalogPath;
			$move_retention_path = $retention_obj->get_new_retention_path($dest_id,$dest_device_name,$moveRetentionPath,$osFlag,$uniqueId,$sparseEnable);            

            if(($pause_status == $PAUSED_PENDING) || ($pause_status == $PAUSE_COMPLETED) )
            {
                $move_retention_status   =  "requested";
            }
            $move_retention_message  ="";
            /*Adding move retention parameter to request string*/
            $result .= "move_retention=$move_retention;move_retention_old_path=$move_retention_old_path;move_retention_path=$move_retention_path;move_retention_status=$move_retention_status;move_retention_message=$move_retention_message;";
        }
        if(in_array($PAUSE_BY_INSUFFICIENT_SPACE,$pauseActivity_arr))
        {
             $insufficient_space_components="yes";
             if(($pause_status == $PAUSED_PENDING)||($pause_status == $PAUSE_COMPLETED))
             {
                $insufficient_space_status  = "requested";
             }
             $insufficient_space_message="";   
             $result .= "insufficient_space_components=$insufficient_space_components;insufficient_space_status=$insufficient_space_status;insufficient_space_message=$insufficient_space_message;";
        }
    }    
    return $result;
}


/*
    * Function Name: get_pause_pending_maintenance_src_info
     *
     *  Description:
     *
     *    This function is used to construct the string,which is sent to 
     *	 source agent for pause maintenance pending 
     *    
     * Parameters:
     *    Param 1 [IN]:$source_id
     *    Param 2 [IN]:$source_device_name
     *	Param 3 [IN]:$dest_id
     *	Param 4 [IN]:$dest_device_name
     *    Param 1 [OUT]:$result
     *
     * Return Value:
     *    Ret Value: Returns String
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
function get_pause_pending_maintenance_src_info($pause_status,$pause_activity, $src_replication_status)
{
    global $conn_obj,$logging_obj,$commonfunctions_obj,$vol_obj;
    global $PAUSED_PENDING,$PAUSE_COMPLETED;
    global $PAUSE_BY_RETENTION,$PAUSE_BY_USER;

    /*default values*/
    $pause_components         ="no";
    $components               = "";
    $pause_components_status  = "";
    $pause_components_message = "";
    
    $pauseActivity_arr = explode(',',$pause_activity);
    /*Check is the pair is in pending state*/

    if(in_array($PAUSE_BY_USER,$pauseActivity_arr) && (($src_replication_status) == 1))
    {
        if(($pause_status == $PAUSED_PENDING) || ($pause_status == $PAUSE_COMPLETED) ) 
        {
            $pause_components         ="yes";
            $pause_components_status  = "requested";
            $components           = "all";
        }
    }
    /*Source request string*/       
    $result ="pause_components=$pause_components;components=$components;pause_components_status=$pause_components_status;pause_components_message=$pause_components_message;";
    return $result;
}
/*
     * Function Name: update_pause_state
     *
     * Description:
     *
     *    This function is used to update the paused state of the pair
     *    
     * Parameters:
     *    Param 1 [IN]:$hostId
     *    Param 2 [IN]:$device_name
     *    Param 3 [IN]:$state	
     *	 Param 1 [OUT]:$query_status
     *
     * Return Value:
     *    Ret Value: Returns flag
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
function update_pause_state($host_id,$device_name,$state)
{
    global $conn_obj,$logging_obj;
    global $PAUSE_COMPLETED,$PAUSE_ALL,$PAUSE_BY_USER,$PAUSED_PENDING;
    global $commonfunctions_obj,$vol_obj;
    $ruleId = array();
    $device_name =$commonfunctions_obj->verifyDatalogPath($device_name);
    $device_name = $conn_obj->sql_escape_string($device_name);
	$query = " ";
	if($state == 1)
    {
		$logging_obj->my_error_handler("INFO",array("SubFunction"=>"update_pause_state","HostId"=>$host_id,"DeviceName"=>$device_name,"State"=>$state),Log::BOTH);
		/*Check cluster case*/
        if($vol_obj->is_device_clustered($host_id,$device_name))
        {
            $clusterIds =$vol_obj->get_clusterids_as_array($host_id, $device_name);
            $cluster_src_hostids = implode("','",$clusterIds);
        }
        else
        {
            $cluster_src_hostids = $host_id;
        }
		$select_sql="SELECT 
							restartPause,
							ruleId,
							jobId,
							replication_status  			
						FROM
							srcLogicalVolumeDestLogicalVolume 
						WHERE 
							sourceHostId IN('$cluster_src_hostids') 
						AND 
							sourceDeviceName ='$device_name'";
							
	}
	elseif ($state == 2)
	{
		$select_sql="SELECT 
							restartPause,
							ruleId,
							jobId,
							replication_status  	
						FROM
							srcLogicalVolumeDestLogicalVolume 
						WHERE 
							destinationHostId ='$host_id' 
						AND 
							destinationDeviceName ='$device_name'";
	}
	if (($state == 1) || ($state == 2))
	{
		$logging_obj->my_error_handler("INFO",array("SubFunction"=>"update_pause_state","HostId"=>$host_id,"DeviceName"=>$device_name,"State"=>$state,"Sql"=>$select_sql),Log::BOTH);
		$result_set = $conn_obj->sql_query($select_sql);
		while($pair_rs = $conn_obj->sql_fetch_object($result_set))
		{
			$restart_pause = $pair_rs->restartPause;
			$job_id = $pair_rs->jobId;
			$rule_id = $pair_rs->ruleId;
			$replication_status = $pair_rs->replication_status;
			$ruleId[$rule_id] = array("restartPause"=>$restart_pause ,"jobId"=>$job_id,"replication_status"=>$replication_status);
		}
		
		foreach ($ruleId as $key=>$value)
		{
			if ($value['replication_status'] == $PAUSED_PENDING)
			{
				//If the pause done for restart re-sync, initiate re-start re-sync on status of pause completion.
				if ($value['restartPause'] == 1)
				{				
					$restart_resync_status = $vol_obj->resync_now($key);
					$logging_obj->my_error_handler("INFO",array("SubFunction"=>"update_pause_state","HostId"=>$host_id,"DeviceName"=>$device_name,"State"=>$state,"Message"=>"Resync now executed for restart resync"),Log::BOTH);
				}			
				elseif ($value['restartPause'] == 0)
				{
							/*update pause state for source or target*/
					$query ="UPDATE 
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status     = $PAUSE_COMPLETED
							WHERE
								ruleId = $key 
							AND
								replication_status = $PAUSED_PENDING";
					$logging_obj->my_error_handler("INFO",array("SubFunction"=>"update_pause_state","HostId"=>$host_id,"DeviceName"=>$device_name,"State"=>$state,"Sql"=>$query),Log::BOTH);
					$status = $conn_obj->sql_query($query);
				}
			}
			else
			{
				//Duplicate acknowledgement, finally it returns TRUE.
			}
		}
    }
    /*resume pair on completion for maintenance activity*/
    else
    {
        $select_sql="SELECT 
                        pauseActivity,
                        autoResume
                    FROM
                        srcLogicalVolumeDestLogicalVolume
                    WHERE
                        destinationHostId ='$host_id'
                    AND
                        destinationDeviceName ='$device_name'";
                        
        $result_set = $conn_obj->sql_query($select_sql);
        $row = $conn_obj->sql_fetch_row($result_set);
        if($row[1] != 1)                
        {
            /*since user choose manully resume after maintenance activity
                               need to  switch to pause by user */ 
            $query ="UPDATE 
                        srcLogicalVolumeDestLogicalVolume
                    SET
                        pauseActivity         = '$PAUSE_BY_USER',
                        autoResume            = 0
                    WHERE
                        destinationHostId ='$host_id'
                    AND
                        destinationDeviceName ='$device_name'
                    AND
                    replication_status = $PAUSE_COMPLETED";
            $status = $conn_obj->sql_query($query);
       }
       else
       {
            /*since maintenance activity is done successfully just resume pair */ 
            $query ="UPDATE 
                        srcLogicalVolumeDestLogicalVolume
                    SET
                        src_replication_status = 0,
                        tar_replication_status = 0,
                        replication_status     = 0,
                        pauseActivity          = NULL,
                        autoResume             = 0
                    WHERE
                        destinationHostId ='$host_id'
                    AND
                        destinationDeviceName ='$device_name'
                    AND
                        replication_status = $PAUSE_COMPLETED";
			$status = $conn_obj->sql_query($query);
       }  
    }
    return TRUE;
}
/*
     * Function Name: update_new_retention_path
     *
     * Description:
     *
     *    This function is used to update new retention path.
     *    
     * Parameters:
     *    Param 1 [IN]:$host_id
     *    Param 2 [IN]:$device_name
     * Return Value:
     *    Ret Value: Returns flag
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
function update_new_retention_path($host_id,$device_name)
{
    global $conn_obj,$sparseretention_obj;
    global $logging_obj;
    global $commonfunctions_obj,$vol_obj;
    $old_log_devicename;
    $device_name =$commonfunctions_obj->verifyDatalogPath($device_name);
    $device_name = $conn_obj->sql_escape_string($device_name);
    
    $compatibilityNo = $commonfunctions_obj->get_compatibility($host_id);
    $osFlag = $commonfunctions_obj->get_osFlag($host_id);
    $source_details=$vol_obj->get_source_details($host_id,$device_name);
    $ruleId =$vol_obj->get_ruleId($source_details[0],$source_details[1],$host_id,$device_name);
    $uniqueId =$sparseretention_obj->get_uniqueId($ruleId);
    
    $sparse_enable =$sparseretention_obj->get_sparse_enable($ruleId);
    $select_query ="SELECT
                        src.ruleId
                    FROM
                        srcLogicalVolumeDestLogicalVolume src,
                        retLogPolicy ret
                    WHERE
                        src.destinationHostId = '$host_id'
                    AND
                        src.destinationDeviceName = '$device_name'
                    AND
                        src.ruleId = ret.ruleid";
    $result_set = $conn_obj->sql_query($select_query);
    
    $log_details =$sparseretention_obj->get_logging_details($host_id,$device_name); 
    $storageDeviceName = $log_details[1]['storage_device_name'];
    $moveRetentionPath = $log_details[1]['moveRetentionPath'];
    /*Cluster case also handled*/
    $query_status;
	while($row = $conn_obj->sql_fetch_row($result_set))
    {
        $old_log_devicename = $storageDeviceName;        
        if((($row[0] != '') ||($row[0] != 0)) && ($moveRetentionPath !=''))
        {
            $ruleid = $row[0];
            $user_path = $moveRetentionPath;
            $retention_path = $conn_obj->sql_escape_string($moveRetentionPath);
            $osFlag =$commonfunctions_obj->getAgentOSType($host_id);
            if($osFlag == 1)
            {
                /*check is new log device name is valid one*/
                $log_device = get_matched_device($host_id,$retention_path,1);
                
            }
            else
            {
                /*linux agents
                                    used new retention path to construct metfilepath &
                                    new log device name */            
                /*check is new log device name is valid one*/
                $log_device = get_matched_device($host_id,$retention_path,2); 
            }
            $log_device =$commonfunctions_obj->verifyDatalogPath($log_device);
            $log_device   = $conn_obj->sql_escape_string($log_device);
            $old_log_devicename =$commonfunctions_obj->verifyDatalogPath($old_log_devicename);
            $old_log_devicename = $conn_obj->sql_escape_string($old_log_devicename);
            $result_set = $sparseretention_obj->get_retentionId($host_id,$device_name);
            $i=0;
            /*collect the retention ids in string*/
            while($row = $conn_obj->sql_fetch_array($result_set))
            {
                if ($i == 0)
                {
                    $strRetId = $row['retId'];  
                }
                else
                {
                    $strRetId = $strRetId.",".$row['retId'];
                }
                $i++;
            }
            $policy_type = $sparseretention_obj->get_policytype($strRetId);
            
            if($osFlag == 1)
            {
                $content_store_arr = explode(':',$user_path);
                if(count($content_store_arr) == 1)
                {   
                    $retention_log = $retention_path.":";
                }
                else
                {
                    $retention_log = $retention_path;
                }
                $deviceName = str_replace(":", "", $device_name);         
            }
            else
            {
                $retention_log = $retention_path;
                $deviceName = str_replace("\\", "/", $device_name);
            }
            
            
            if($compatibilityNo <= 510000)
            {
                if($osFlag == 1)
                {
                    $catalog_path = "$retention_log\\$uniqueId\\$host_id\\$deviceName\\cdpv1.db";
                }
                else
                {
                    $catalog_path = "$retention_log/$uniqueId/$host_id/$deviceName/cdpv1.db";
                }
            }
            else
            {
                if($sparse_enable == 1)
                {
                    $cdp ="cdpv3.db"; 
                }
                else
                {
                    $cdp = "cdpv1.db";
                }
                if($osFlag == 1)
                {
                    $catalog_path = "$retention_log\\catalogue\\$host_id\\$deviceName\\$uniqueId\\$cdp";
                }
                else
                {
                    $catalog_path = "$retention_log/catalogue/$host_id/$deviceName/$uniqueId/$cdp";
                }
            }
            #$logging_obj->my_error_handler("DEBUG","catalog_path :$catalog_path");
            $catalog_path =$commonfunctions_obj->verifyDatalogPath($catalog_path);
            #$logging_obj->my_error_handler("DEBUG","catalog_path :$catalog_path");
            $catalog_path = $conn_obj->sql_escape_string($catalog_path);
            #$logging_obj->my_error_handler("DEBUG","catalog_path :$catalog_path");
           
            
            if($policy_type == 1)
            {
                $query_status = $sparseretention_obj->update_timeBased_moveRetention($strRetId,$retention_path,$catalog_path,$log_device);
            }   
            else if($policy_type == 0)
            {
                $query_status = $sparseretention_obj->update_spaceBased_moveRetention($strRetId,$retention_path,$catalog_path,$log_device);
                
            }
            else
            {
                $query_status = $sparseretention_obj->update_timeBased_moveRetention($strRetId,$retention_path,$catalog_path,$log_device);
                $query_status = $sparseretention_obj->update_spaceBased_moveRetention($strRetId,$retention_path,$catalog_path,$log_device);
            }
            if($query_status)
            {
                $ret = $sparseretention_obj->reset_device_flag_in_use($host_id,$old_log_devicename);
                $query="UPDATE
                        logicalVolumes 
                    SET
                        deviceFlagInUse = 3 
                    WHERE
                        hostId = '$host_id' 
                    AND
                        (deviceName = '$log_device' OR mountPoint = '$log_device')";
                $ret = $conn_obj->sql_query($query);
            }
        }
    }
    
    return  $query_status; 
}

/*
     * Function Name: update_pause_pending_state
     *
     * Description:
     *
     *    This function is used to update the pause pending state of the pair
     *    
     * Parameters:
     *    Param 1 [IN]:$hostId
     *    Param 2 [IN]:$device_name
     *    Param 3 [IN]:$state
     *	 Param 1 [OUT]:$query_status
     *
     * Return Value:
     *    Ret Value: Returns flag
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
function update_pause_pending_state($host_id,$device_name,$state)
{
    global $conn_obj,$logging_obj;
    global $PAUSE_COMPLETED,$PAUSE_ALL,$PAUSE_BY_USER,$PAUSED_PENDING,$PAUSE_BY_INSUFFICIENT_SPACE;
    global $commonfunctions_obj,$vol_obj;
    
    $device_name =$commonfunctions_obj->verifyDatalogPath($device_name);
    $device_name = $conn_obj->sql_escape_string($device_name);
    /*update target state for pause by user & maintenance*/
    if($state == 2)
    {
        $query ="UPDATE 
                    srcLogicalVolumeDestLogicalVolume
                SET
                    replication_status     = $PAUSED_PENDING,
                    tar_replication_status = 1,
                    pauseActivity          = '$PAUSE_BY_INSUFFICIENT_SPACE',
                    autoResume             = 0
                WHERE
                    destinationHostId ='$host_id'
                AND
                    destinationDeviceName ='$device_name'
                AND
                    replication_status = 0";
    }
    $query_status = $conn_obj->sql_query($query);
    return ($query_status);
}

/*
     * Function Name: check_replication_cleanup_status
     *
     * Description:
     *
     *    This function is used to construct the string,which is sent to 
     *	 source agent for cleanup 
     *    
     * Parameters:
     *    Param 1 [IN]:$destId
     *    Param 2 [IN]:$device_name
     *	Param 3 [IN]:$rep_options_exiting
     *	Param 1 [OUT]:$flag
     *
     * Return Value:
     *    Ret Value: Returns flag
     *
     * Exceptions:
     *    DataBase Connection fails.
    */

function check_replication_cleanup_status($destId,$device_name,$rep_options_exiting)
{
	global $conn_obj;
	global $logging_obj;
	$device_name = $conn_obj->sql_escape_string($device_name);
	$flag = 0;
	$sql = "SELECT 
				replicationCleanupOptions
			FROM 
				srcLogicalVolumeDestLogicalVolume
			WHERE
				destinationHostId = '$destId' 
				AND 
				destinationDeviceName='$device_name'";
	$result_set = $conn_obj->sql_query($sql);
	$row = $conn_obj->sql_fetch_row($result_set);
	$rep_options = "$row[0]";
	$chk_exiting = $rep_options_exiting[0].$rep_options_exiting[2].$rep_options_exiting[4];
	$chk_new = $rep_options[0].$rep_options[2].$rep_options[4].$rep_options[6].$rep_options[8];
	$result = $rep_options[1].$rep_options[3].$rep_options[5].$rep_options[7].$rep_options[9];
	if($chk_new === "00000")
	{
		//Sending success in all cases. This is to handle caspian delete stuck issue.
		$flag = 2;
		
		/* if($result === "11111" || $result === "11100" || 
			$result ==="11110" || $result === "11101" || 
			$result === "01101" || $result === "01111" || 
			$result === "01110" || $result === "01100" || $result === "11000" || $result === "01000")
		{
			$flag = 2; //Success
		}
		else
		{
			$flag = 1; //Failed
		} */
	}
	else
	{
		$flag = 3; //Not Received
	}
	return $flag;
}

/*
 * Function Name: volumeResizeNotification
 *
 * Description:
 *		The responsiblities of the function are as below.
 *		a) Inserting the source volume resize history into the volume resize history table.
 *		b) Pause the replication pair on source volume resize identified.
 *		c) Send an email to the admins about the source volume resize.
 *		d) Send a source volume resize trap to the trap reciever.
 *    
 * Parameters:
 *		Param 1 [IN]: source volume device properties object
 *		Param 2 [IN]: source volume capacity
 *
 * Return Value:
 *
 * Exceptions:
*/
function volumeResizeNotification($device_properties_obj,$capacity,$new_capacity_send=TRUE , $logical_volumes_arary = 1 , $volume_resize_arary = 1 , $application_scenario_array = 1, $host_ids_array = 1 , $deviceName = 1, $src_volumes_arary,$pair_type_array='',$host_array='',$cluster_volumes_array='', $rawSize)
{
    global $conn_obj;
    global 	$logging_obj;
    global $vol_obj,$app_obj;
    global $commonfunctions_obj;
    global $CAPACITY_CHANGE_RESET;
    global $CAPACITY_CHANGE_REPORTED,$CAPACITY_CHANGE_UPDATED;
    global $CAPACITY_CHANGE_RESCANNED;
    global $CAPACITY_CHANGE_PROCESSING;
    global $INMAGE_PROFILER_HOST_ID;
    global $PAUSED_PENDING, $PAUSE_COMPLETED;
	global $multi_query_static, $CONFIGURATION_SERVER, $CONFIGURATION_SERVER_DISK_RESIZE;
	global $ABHAI_MIB, $TRAP_AMETHYSTGUID, $TRAP_AGENT_ERROR_MESSAGE, $TRAP_SEVERITY, $TRAP_CS_HOSTNAME, $TRAP_SOURCE_HOSTNAME, $TRAP_SOURCE_DEVICENAME, $TRAP_TARGET_HOSTNAME, $TRAP_TARGET_DEVICENAME, $TRAP_VOLUME_RESIZE;
	
	$dest_detas = array();
	$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Device properties =  ".print_r($device_properties_obj,TRUE)." \n capacity = $capacity \n\n New capacity send = $new_capacity_send \n application scenario array = ".print_r($application_scenario_array,true)."\n");
	$cx_hostname = '';
	$trap_info_str = '';  
    $error_code = '';
    $arr_error_placeholders = array();
	foreach($host_array as $id => $hostInfo)
	{
		if($hostInfo['csEnabled'])
		{			
			$cx_hostname = $hostInfo['name'];
			$cx_hostid   = $id;
			break;
		}
	}
	$cx_os_flag = $commonfunctions_obj->get_osFlag($cx_hostid);
	
	$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n cx_hostid = $cx_hostid \n cx_hostname = $cx_hostname \n");	
    $new_capacity = $capacity;
    $source_host_ids = array();
	if(is_array($logical_volumes_arary))
	{
		$device_escaped = $deviceName;
		$device_name = $logical_volumes_arary['deviceName'];
		$old_capacity = $logical_volumes_arary["capacity"];
		$is_protected = $logical_volumes_arary["farLineProtected"];
		$device_flag_inuse = $logical_volumes_arary["deviceFlagInUse"];
		$host_id = $logical_volumes_arary["hostId"];
		$phy_lun_id = $logical_volumes_arary["Phy_Lunid"];
		
		$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Device name = $device_name \n\n Device escaped = $device_escaped \n\n Old capacity = $old_capacity \n\n Is protected = $is_protected \n\n Device flag inuse = $device_flag_inuse \n\n Host id = $host_id \n\n Physical lun id = $phy_lun_id \n");
	}
	else
	{
		$old_capacity = $device_properties_obj->capacity;
		$is_protected = $device_properties_obj->farLineProtected;
		$device_flag_inuse = $device_properties_obj->deviceFlagInUse;
		
		$device_name = $device_properties_obj->deviceName;
		$host_id = $device_properties_obj->hostId;
		$device_escaped = $conn_obj->sql_escape_string($device_name);
		$phy_lun_id = $device_properties_obj->Phy_Lunid;
	}
	
    $pairType = 0;
	$flag = 0;
	if(isset($phy_lun_id) && $phy_lun_id !='')
	{
		foreach($pair_type_array as $planId => $value)
		{
			if($value['srcLun'])
			{
				foreach($value['srcLun'] as $key => $src_lun_id)
				{
					if($phy_lun_id == $src_lun_id)
					{
						$pairType= $value['pair_type'];//0=host,1=fabric,2=prism,3=array
						$flag = 1;						
						break;
					}
				}
			}
			if($value['destLun'])
			{
				foreach($value['destLun'] as $key => $dst_lun_id)
				{
					if($phy_lun_id == $dst_lun_id)
					{
						$pairType= $value['pair_type'];	//0=host,1=fabric,2=prism,3=array
						$flag = 1;
						break;
					}
				}
			}
			if($flag)break;			
		}
	}
	$logging_obj->my_error_handler("INFO","volumeResizeNotification pairType :: $pairType \n");
	if($pairType == 3)
	{
		if($old_capacity > $new_capacity)
		{
			$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Getting shrink capacity \n\n Returning new capacity = $new_capacity \n\n Old capacity = $old_capacity\n");
			return;
		}
	}
	if(!is_array($logical_volumes_arary))
	{
		$query =	"SELECT
						count(*) as count
					FROM
						volumeResizeHistory
					WHERE
						hostId = '$host_id' AND 
						deviceName = '$device_escaped' AND
						isValid = '1' AND
						newCapacity = '$new_capacity' AND
						oldCapacity = '$old_capacity'";	
		$logging_obj->my_error_handler("DEBUG","volumeResizeNotification\n Query = $query \n");
		$dest_result_set = $conn_obj->sql_query($query);
		$dest_details_obj = $conn_obj->sql_fetch_object($dest_result_set);
		$count = $dest_details_obj->count;	
	}
	else
	{
		$count = 0;
		if(is_array($volume_resize_arary))
		{
			if(array_key_exists($device_name , $volume_resize_arary) && $volume_resize_arary[$device_name]['isValid'] == 1 && $volume_resize_arary[$device_name]['newCapacity'] == $new_capacity && $volume_resize_arary[$device_name]['oldCapacity'] == $old_capacity)
			{
				$count++;
			}
		}

	}

	$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Count = $count \n");
	$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Application scenario array = ".print_r($application_scenario_array,true)."\n");
	$protectionDirection='';
	$scenarioId = 0;	
	if(!$count)
	{		
		if($pairType == 2 || $pairType == 3)
		{
			if(is_array($logical_volumes_arary))
			{
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Application scenario array = ".print_r($application_scenario_array,true)."\n");
				foreach ($application_scenario_array as $key => $application_details)
				{	
					$srcHostList = explode(",",$application_details['sourceId']);
					$srcVolList = explode(",",$application_details['sourceVolumes']);
					$tgtHostList = explode(",",$application_details['targetId']);
					$tgtVolList = explode(",",$application_details['targetVolumes']);
					
					if(((in_array($host_id, $srcHostList ) &&  in_array($phy_lun_id,$srcVolList )) || ((in_array($host_id,$tgtHostList) && ((($pairType == 2) && in_array($device_name,$tgtVolList)) || (($pairType == 3) && in_array($phy_lun_id,$tgtVolList)))))))
					
					{
						$scenarioId = $application_details['scenarioId'];
						$protectionDirection = $application_details['protectionDirection'];
						
						$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n scenarioId = $scenarioId \n\n protectionDirection = $protectionDirection \n");
					}
				}
			}
			else
			{
				$target_vol_cond = ($pairType == 2) ? "targetVolumes LIKE '%$device_escaped%'" : "targetVolumes LIKE '%$phy_lun_id%'";
								
				$query2 =	"SELECT
							scenarioId,
							protectionDirection						
						FROM
							applicationScenario
						WHERE
							(sourceId LIKE '%$host_id%' AND 
							sourceVolumes LIKE '%$phy_lun_id%') OR
							(targetId LIKE '%$host_id%' AND $target_vol_cond)";
			}
		}
		else
		{	
			if(is_array($logical_volumes_arary))
			{
				foreach ($application_scenario_array as $key => $application_details)
				{	
					$srcHostList = explode(",",$application_details['sourceId']);
									$srcVolList = explode(",",$application_details['sourceVolumes']);
									$tgtHostList = explode(",",$application_details['targetId']);
									$tgtVolList = explode(",",$application_details['targetVolumes']);
									
					if((in_array($host_id,$srcHostList ) &&  in_array($device_name,$srcVolList)) || (in_array($host_id,$tgtHostList) && in_array($device_name,$tgtVolList)))
					
					{
						$scenarioId = $application_details['scenarioId'];
						$protectionDirection = $application_details['protectionDirection'];
						$scenario_dets = unserialize($application_details['scenarioDetails']);
						$protection_path = $scenario_dets['protectionPath'];
					}
				}
			}
			else
			{
				$query2 =	"SELECT
							scenarioId,
							protectionDirection						
						FROM
							applicationScenario
						WHERE
							(sourceId LIKE '%$host_id%' AND 
							sourceVolumes LIKE '%$device_escaped%') OR
							(targetId LIKE '%$host_id%' AND 
							targetVolumes LIKE '%$device_escaped%')";
			}

		}
		if(!is_array($logical_volumes_arary))
		{
			$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Query = $query2 \n");
			$dest_result_set2 = $conn_obj->sql_query($query2);
			$dest_details_obj2 = $conn_obj->sql_fetch_object($dest_result_set2);
			$scenarioId = $dest_details_obj2->scenarioId;
			$protectionDirection = $dest_details_obj2->protectionDirection;
		}
	}
    	
	$logging_obj->my_error_handler("DEBUG","volumeResizeNotification\n Device properties new capacity send = $new_capacity_send \n\n new capacity = $new_capacity\n\n Old capacity = $old_capacity \n\n ScenarioId = $scenarioId \n\n protection path = $protection_path \n");
    /* For cluster passive node as capacity is zero, it is not allowing to check the volume resize part*/
	#as part of 13214 Fix --added device_flag_inuse to allow for tgt vol resize

	if($pairType == 3)
	{
		#
		# if pair is Array type then only consider the /dev/mapper for lun resize exclude
		# all the scsi devices.
		#
		$count = 0;
		if(is_array($logical_volumes_arary))
		{
			if(is_array($src_volumes_arary))
			{
				foreach($src_volumes_arary["pair_details"] as $key => $pair_details)
				{
					$logging_obj->my_error_handler("DEBUG","volumeResizeNotification::checking device,".$pair_details['sourceDeviceName'], $device_name."\n");
					if(($pair_details['sourceDeviceName'] == $device_name) || ($pair_details['destinationDeviceName'] == $device_name))
					{
						$count++;
					}
				}
			}
		}
						
		if($count == 0)
		{
			return;
		}
	}
	$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Device match count = $count \n old_capacity = $old_capacity \n new_capacity = $new_capacity \n is_protected = $is_protected \n device_flag_inuse = $device_flag_inuse \n");
	
    if ( (($new_capacity_send == TRUE)
        && ($new_capacity > 0)
        && ($old_capacity > 0)
        && ($is_protected == 1 || $scenarioId || $device_flag_inuse == 1)
        && (($old_capacity > $new_capacity) || ($old_capacity < $new_capacity)))
         || ($new_capacity_send == FALSE))
    {	
		
		$source_vol_resize = FALSE;
		$dest_vol_resize = FALSE;
		$ps_volume_resize = false;
		
		$logging_obj->my_error_handler("INFO","volumeResizeNotification	entered inside if pairType = $pairType \n");
		
        $is_cluster = FALSE;
       

        $is_cluster = array_key_exists ($device_name,$cluster_volumes_array);
		$logging_obj->my_error_handler("INFO","volumeResizeNotification \n device = $device_name \n is_cluster = $is_cluster \n"); 

		if($pairType == 2 || $pairType == 3)
		{
			if(is_array($logical_volumes_arary))
			{
				$i = 0;	
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n  Source volumes data ".print_r($src_volumes_arary,TRUE)."\n");
				
				if(is_array($src_volumes_arary) && array_key_exists('pair_details',$src_volumes_arary))
				{
					foreach($src_volumes_arary["pair_details"] as $key => $pair_details)
					{
						if(($pair_details['Phy_Lunid'] == $phy_lun_id) || ($pair_details['destinationHostId'] == $host_id && $pair_details['destinationDeviceName'] == $device_name))
						{
							$source_host_id = $pair_details['sourceHostId'];
							$source_device_name =  $pair_details['sourceDeviceName'];			
							$source_device = $pair_details['sourceDeviceName'];
							$source_device_escaped = $conn_obj->sql_escape_string ($source_device);			
							$dest_host_id = $pair_details['destinationHostId'];
							$dest_device = $pair_details['destinationDeviceName'];
							$lun_id = $pair_details['Phy_Lunid'];		
							$dest_device_name = $conn_obj->sql_escape_string($dest_device);
							#$dest_detas[$i][$dest_host_id] = $dest_device;	
							$i++;
							
							if($pairType == 2)
							{
								if($pair_details['destinationHostId'] == $host_id && $pair_details['destinationDeviceName'] == $device_name)
								{
									$dest_detas[$i][$dest_host_id] = $dest_device;	
									$seek_status = TRUE;
									$dest_vol_resize = TRUE;
								}
								else
								{
									if($source_device_name == $device_name && $source_host_id == $host_id)
									{
										/* in case of prism, if sync device reports volume resize then we have to only change the AT lun capacity
										* with sharedDevice flag update.
										*/
										$ps_volume_resize = TRUE;
									}
								}
							}
							else if($pairType == 3)
							{
								$dest_detas[$i][$dest_host_id] = $dest_device;
								$seek_status = TRUE;	
							}
						}
					}
				}
				else
				{
					/* This will handle PRISM source node volume resize as src_volumes_arary will not set for source node */
					$src_sql ="SELECT
						sourceHostId,
						sourceDeviceName,
						destinationHostId,
						destinationDeviceName 
					FROM
						srcLogicalVolumeDestLogicalVolume
					WHERE
						Phy_Lunid = '$phy_lun_id'";
					$src_rs = $conn_obj->sql_query($src_sql);        
					$i = 0;		
					$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n PRISM source node sql = $src_sql \n\n dest_result_set = $src_rs \n");
					while($rs_obj = $conn_obj->sql_fetch_object($src_rs))
					{
						$source_host_id = $rs_obj->sourceHostId;
						$source_device_name = $rs_obj->sourceDeviceName;			
						$source_device = $rs_obj->sourceDeviceName;
						$source_device_escaped = $conn_obj->sql_escape_string ($source_device);			
						$dest_host_id = $rs_obj->destinationHostId;
						$dest_device = $rs_obj->destinationDeviceName;
						$lun_id = $phy_lun_id;		
						$dest_device_name = $conn_obj->sql_escape_string($dest_device);
						$logging_obj->my_error_handler("DEBUG","volumeResizeNotification::   PRISM source node  Source hostid = $source_host_id \n\n Source device name = $source_device_name \n\n Source device = $source_device \n\n Source device escaped = $source_device_escaped \n\n Destination hostid = $dest_host_id \n\n Destination device = $dest_device \n\n Destination device name = $dest_device_name \n\n Destination deta = ".print_r($dest_detas,TRUE)." \n");
						
						if($pairType == 2)
						{
							if($source_host_id != $host_id)
							{
								/* in case of prism, if node reports source volume resize 
								* then onky we have to consider as volume resize
								*/
								$source_vol_resize = TRUE;
								$dest_detas[$i][$dest_host_id] = $dest_device;	
								$seek_status = TRUE;
							}
						}
						$i++;
					}
				}
			}
			else
			{
				$query ="SELECT
						sourceHostId,
						sourceDeviceName,
						destinationHostId,
						destinationDeviceName,
						Phy_Lunid
					FROM
						srcLogicalVolumeDestLogicalVolume
					WHERE
						(Phy_Lunid = '$phy_lun_id') OR
						(destinationHostId= '$host_id' AND 
						destinationDeviceName = '$device_escaped')";
			}
		}
		else
		{
			if(is_array($logical_volumes_arary))
			{
				$i = 0;	
				if(is_array($src_volumes_arary))
				{
					foreach($src_volumes_arary["pair_details"] as $key => $pair_details)
					{
						if(($pair_details['sourceHostId'] == $host_id && $pair_details['sourceDeviceName'] == $device_name) || ($pair_details['destinationHostId'] == $host_id && $pair_details['destinationDeviceName'] == $device_name))
						{

							$source_host_id = $pair_details['sourceHostId'];
							$source_device_name =  $pair_details['sourceDeviceName'];			
							$source_device = $pair_details['sourceDeviceName'];
							$source_device_escaped = $conn_obj->sql_escape_string ($source_device);			
							$dest_host_id = $pair_details['destinationHostId'];
							$dest_device = $pair_details['destinationDeviceName'];
							$lun_id = $pair_details['Phy_Lunid'];
							$job_id = $pair_details['jobId'];
							$dest_device_name = $conn_obj->sql_escape_string($dest_device);
							$dest_detas[$i][$dest_host_id] = $dest_device;	
							$i++;
							$seek_status = TRUE;
						}
					}
				}
			}
			else
			{
				$query ="SELECT
						sourceHostId,
						sourceDeviceName,
						destinationHostId,
						destinationDeviceName,
						Phy_Lunid
					FROM
						srcLogicalVolumeDestLogicalVolume
					WHERE
						(sourceHostId = '$host_id' AND 
						sourceDeviceName = '$device_escaped') OR
						(destinationHostId= '$host_id' AND 
						destinationDeviceName = '$device_escaped')";
			}
		
		}
		
		
				
		if(!is_array($logical_volumes_arary))
		{
			$dest_result_set = $conn_obj->sql_query($query);        
			$i = 0;		
			$logging_obj->my_error_handler("INFO","volumeResizeNotification \n query = $query \n\n dest_result_set = $dest_result_set \n");
			while($dest_details_obj = $conn_obj->sql_fetch_object($dest_result_set))
			{
				$source_host_id = $dest_details_obj->sourceHostId;
				$source_device_name = $dest_details_obj->sourceDeviceName;			
				$source_device = $dest_details_obj->sourceDeviceName;
				$source_device_escaped = $conn_obj->sql_escape_string ($source_device);			
				$dest_host_id = $dest_details_obj->destinationHostId;
				$dest_device = $dest_details_obj->destinationDeviceName;
				$lun_id = $dest_details_obj->Phy_Lunid;		
				$dest_device_name = $conn_obj->sql_escape_string($dest_device);
				$dest_detas[$i][$dest_host_id] = $dest_device;			
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification::   Source hostid = $source_host_id \n\n Source device name = $source_device_name \n\n Source device = $source_device \n\n Source device escaped = $source_device_escaped \n\n Destination hostid = $dest_host_id \n\n Destination device = $dest_device \n\n Destination device name = $dest_device_name \n\n Destination deta = ".print_r($dest_detas,TRUE)." \n");
				$i++;
			}
			$seek_status = $conn_obj->sql_data_seek($dest_result_set,0);
		}
	
		$source_host_name1 = ($source_host_id) ? $host_array[$source_host_id]['name'] : 'N/A';
        $destination_host_name1 = ($dest_host_id) ? $host_array[$dest_host_id]['name'] : 'N/A';
		
		$logging_obj->my_error_handler("INFO","volumeResizeNotification \n\n Source hostid = $source_host_id \n\n Source device name = $source_device_name\n\n Source device escaped = $source_device_escaped \n\n Protection direction = $protectionDirection \n\n Lun id = $lun_id \n source_host_name1 = $source_host_name1 \n destination_host_name1 = $destination_host_name1 \n");
		$dest_hostids[] = array();
		$dest_devicenames[] = array();
						
		#src vol resize
		if($pairType !=2)
		{
			if((($source_host_id == $host_id ) && (!strcmp($source_device_name,$device_name))) || ($dest_host_id != $host_id))
			{
				$source_vol_resize = TRUE;				
			}
			else
			{
				$dest_vol_resize = TRUE;								
							
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Destination volume resize = $dest_vol_resize \n\n Destination hostid = $dest_host_id \n\n Destination device = $dest_device, Destination device name = $dest_device_name \n");		
			}
		}		
		$logging_obj->my_error_handler("INFO","volumeResizeNotification:: Source volume resize = $source_vol_resize \n\n Destination volume resize =  $dest_vol_resize \n");
		
		$oneToMany = $vol_obj->is_one_to_many_by_source($source_host_id, $source_device_name);		
		$logging_obj->my_error_handler("INFO","volumeResizeNotification \n oneToMany = $oneToMany \n");
        
		//$pairType = $vol_obj->get_pair_type($lun_id); // pairType 0=host,1=fabric,2=prism
		$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Pair type = ".$pairType."\n");
        if($pairType == 1 || $pairType == 2 || $pairType == 3)
        {
            $is_lun_pair = true;
        }
        else
        {
            $is_lun_pair = false;
        }		
		$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Is lun pair = $is_lun_pair \n");
        
		$host_ids = array();
        $host_ids[] = $host_id;
		$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Is cluster = $is_cluster \n");
        if ($is_cluster)
        {
			if(is_array($logical_volumes_arary))
			{
				$host_ids = $host_ids_array;
			}
			else
			{
				$clusterid_array = array();
				$cluster_id_sql = "SELECT 
										clusterId
									FROM 
										clusters 
									WHERE 
										hostId = '$host_id'";				
				$clusterid_result = $conn_obj->sql_query($cluster_id_sql);
				while($clusterid_object = $conn_obj->sql_fetch_object($clusterid_result))
				{
					$clusterid_array[] = $clusterid_object->clusterId;
				}
				$clusterid_array = array_unique($clusterid_array);
				$logging_obj->my_error_handler("INFO","volumeResizeNotification \n cluster_id_sql = $cluster_id_sql , clusterid_array = ".print_r($clusterid_array,true)."\n");
				if($clusterid_array)
				{
					$cluster_hostids = array();
					$query = "SELECT 
								hostId
							FROM 
								clusters  
							WHERE clusterId = '".$clusterid_array[0]."'";
					$result_set = $conn_obj->sql_query($query);			
					$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Query for fetching cluster hostId = $query \n");
					while ($row_object = $conn_obj->sql_fetch_object($result_set))
					{
						$cluster_hostids[] = $row_object->hostId;
					}
					$cluster_hostids = array_unique($cluster_hostids);
					$logging_obj->my_error_handler("INFO","volumeResizeNotification \n after  array_unique cluster_hostids = ".print_r($cluster_hostids,true)."\n");
					foreach($cluster_hostids as $key => $clus_host_id)
					{
						if (!in_array($clus_host_id,$host_ids))
						{
							$host_ids[] = $clus_host_id;
						}
					}				
				}			
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification final  hostids = ".print_r($host_ids,true)."\n");
				
			}
        }        
        
        $source_host_name = '';
        $source_ip_address = '';
        $source_host_name_list = array();
        $source_ip_address_list = array();
		$pair_details_additional_data = array();
        foreach ($host_ids as $key=>$value)
        {
            $host_id = $value;
			$is_valid = 0;
			if(($source_vol_resize == TRUE) || (($dest_vol_resize == TRUE) && ($new_capacity > $old_capacity)))
			#if(($dest_vol_resize == TRUE) && ($new_capacity > $old_capacity))
			{
				$is_valid = 1;
			}
			
			if($source_vol_resize == TRUE)
			{
				$pair_id_sql = "SELECT pairId, sourceHostId, sourceDeviceName, destinationHostId, destinationDeviceName, deleted, replication_status, jobId, isQuasiFlag, restartPause FROM srcLogicalVolumeDestLogicalVolume WHERE sourceHostId = '$host_id' AND sourceDeviceName = '$device_escaped'";
			}

			if(($dest_vol_resize == TRUE) && ($new_capacity > $old_capacity))
			{
				$pair_id_sql = "SELECT pairId, sourceHostId, sourceDeviceName, destinationHostId, destinationDeviceName, deleted, replication_status, jobId, isQuasiFlag, restartPause FROM srcLogicalVolumeDestLogicalVolume WHERE destinationHostId= '$host_id' AND destinationDeviceName = '$device_escaped'";
			}

			if($pair_id_rs = $conn_obj->sql_query($pair_id_sql))
			{
				$temp_count = 0;
				$vals = '';
				while ($pair_id_obj = $conn_obj->sql_fetch_object($pair_id_rs))
				{
					$deleted = $pair_id_obj->deleted;
					//If protection marked for deletion, just return.
					if ($deleted == 1)
					{
						$logging_obj->my_error_handler("DEBUG","volumeResizeNotification: As protection marked for deletion, returned from volume resize. \n");
						return;
					}
					$src_host_id = $pair_id_obj->sourceHostId;
					$src_device_name = $pair_id_obj->sourceDeviceName;
					#$src_device_name = $conn_obj->sql_escape_string ($src_device_name);
					$dst_host_id = $pair_id_obj->destinationHostId;
					$dst_device_name = $pair_id_obj->destinationDeviceName;
					#$dst_device_name = $conn_obj->sql_escape_string ($dst_device_name);
					$replication_status = $pair_id_obj->replication_status;
					$job_id = $pair_id_obj->jobId;
					$is_quasi_flag = $pair_id_obj->isQuasiFlag;
					$restart_pause = $pair_id_obj->restartPause;
					$pair_details_additional_data[$src_host_id][$src_device_name] = array("deleted"=>$deleted, "replication_status"=>$replication_status, "job_id"=>$job_id, "is_quasi_flag"=>$is_quasi_flag, "restart_pause"=>$restart_pause, "destination_host_id"=>$dst_host_id,"destination_device_name"=>$dst_device_name);
					if ($protection_path == 'E2A')
					{
					
						$dst_device_name = $conn_obj->sql_escape_string ($dst_device_name);
						$query = "UPDATE
							logicalVolumes
						SET								
							capacity = '$new_capacity',
							rawSize = '$rawSize' 
						WHERE
							hostId = '$dst_host_id'
						AND
							deviceName = '$dst_device_name'
						AND 
							isImpersonate = 1 ";
						$multi_query_static["update"][] = $query;
					}
					if($temp_count == 0)
					{
						$vals = "('$host_id',
								  '$device_escaped',
								   now(),
								   $old_capacity,
								   $new_capacity,
								   $is_valid,
								   $pair_id_obj->pairId)";
					}
					else
					{
						$vals = $vals . ",('$host_id',
											   '$device_escaped',
												now(),
												$old_capacity,
												$new_capacity,
												$is_valid,
												$pair_id_obj->pairId)";
					}
					$temp_count++;
				}
				
				$query =	"INSERT 
								INTO 
									volumeResizeHistory
									(hostId,
									deviceName,
									resizeTime,
									oldCapacity,
									newCapacity,
									isValid,
									pairId)
							  VALUES $vals";
							  
				if(is_array($volume_resize_arary))
				{
					$multi_query_static["insert"][] = $query;
				}
				else
				{
					$query_status = $conn_obj->sql_query($query);
				}
			}
			
			$logging_obj->my_error_handler("DEBUG","volumeResizeNotification\n Insertion into volumeResizeHistory query = $query \n");
            $source_host_name_list[] = $host_array[$host_id]['name'];
            $source_host_name = implode(",",$source_host_name_list);
            $source_ip_address_list[] = $host_array[$host_id]['ipAddress'];
            $source_ip_address = implode(",",$source_ip_address_list);
			
			/*
				for Application Specific
			*/
			
			$scenario_type_array =  array('DR Protection','Backup Protection');
			$count = 0;
			if($pairType == 2 || $pairType == 3)
			{
				if(is_array($logical_volumes_arary))
				{
					if(is_array($application_scenario_array))
					{
						foreach ($application_scenario_array as $key => $application_details)
						{	
							$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Application details = ".$application_details['sourceId'].",$host_id==\n");
							$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Application details = ".$application_details['sourceVolumes'].",$phy_lun_id==\n");
							$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Application details = ".$application_details['scenarioType']."==\n");
							$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Scenario type data = ".print_r($scenario_type_array,true)."\n");
							
							$srcHostList = explode(",",$application_details['sourceId']);
									$srcVolList = explode(",",$application_details['sourceVolumes']);
									$tgtHostList = explode(",",$application_details['targetId']);
									$tgtVolList = explode(",",$application_details['targetVolumes']);
									
									if(((in_array($host_id,$srcHostList) &&  in_array($phy_lun_id,$srcVolList)) || (in_array($host_id,$tgtHostList) && ((($pairType == 2) && in_array($device_name,$tgtVolList)) || (($pairType == 3) && in_array($phy_lun_id,$tgtVolList))))) && in_array($application_details['scenarioType'], $scenario_type_array))
							
							{
								$scenario_array[$count]['scenarioId'] = $application_details['scenarioId'];
								$scenario_array[$count]['scenarioDetails'] = $application_details['scenarioDetails'];
								$count++;
							}
						}
					}
				}
				else
				{
					$target_vol_cond = ($pairType == 2) ? "targetVolumes LIKE '%$device_escaped%'" : "targetVolumes LIKE '%$phy_lun_id%'";
					
					$sql = "SELECT
							scenarioId,
							scenarioDetails
						FROM
							applicationScenario
						WHERE
							(sourceId LIKE '%$host_id%'
						AND
							sourceVolumes LIKE '%$phy_lun_id%')
						OR
							(targetId LIKE '%$host_id%'	AND	$target_vol_cond)
						AND
							scenarioType IN('DR Protection','Backup Protection')";
				}
			
			}
			else
			{
				if(is_array($logical_volumes_arary))
				{
					if(is_array($application_scenario_array))
					{
						foreach ($application_scenario_array as $key => $application_details)
						{	
							$srcHostList = explode(",",$application_details['sourceId']);
									$srcVolList = explode(",",$application_details['sourceVolumes']);
									$tgtHostList = explode(",",$application_details['targetId']);
									$tgtVolList = explode(",",$application_details['targetVolumes']);
									
									if(((in_array($host_id,$srcHostList) &&  in_array($device_name,$srcVolList)) || (in_array($host_id,$tgtHostList) && in_array($device_name,$tgtVolList))) && in_array($application_details['scenarioType'] , $scenario_type_array ))
							
							
							{
								$scenario_array[$count]['scenarioId'] = $application_details['scenarioId'];
								$scenario_array[$count]['scenarioDetails'] = $application_details['scenarioDetails'];
								$count++;
							}
						}
					}
				}
				else
				{
					$sql = "SELECT
							scenarioId,
							scenarioDetails
						FROM
							applicationScenario
						WHERE
							(sourceId LIKE '%$host_id%'
						AND
							sourceVolumes LIKE '%$device_escaped%')
						OR
							(targetId LIKE '%$host_id%'
						AND
							targetVolumes LIKE '%$device_escaped%')
						AND
							scenarioType IN('DR Protection','Backup Protection')";
				}
			}

			if(!is_array($logical_volumes_arary))
			{
				$count = 0;
				$rs = $conn_obj->sql_query($sql);			
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n  Select query from applicationScenario table = $sql \n");
				if($conn_obj->sql_num_row($rs)>0)
				{
					while($row = $conn_obj->sql_fetch_object($rs))
					{			
						$scenario_array[$count]['scenarioId'] = $row->scenarioId;
						$scenario_array[$count]['scenarioDetails'] = $row->scenarioDetails;
						$count++;
					}
				}
			}
			
			$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n application scenario updation block :: count :: $count \n");		
			
			if($count>0)
			{
				foreach ($scenario_array as $key => $scenario_details)
				{			
					$scenarioId = $scenario_details['scenarioId'];
					/*$xml_data = $scenario_details['scenarioDetails'];
					$cdata = $commonfunctions_obj->getArrayFromXML($xml_data);
					$plan_properties = unserialize($cdata['plan']['data']['value']);*/
					$plan_properties = unserialize($scenario_details['scenarioDetails']);
					if($plan_properties['backupProvisioning'] == 'auto')
					{					
						$sql_update_sanLuns = "UPDATE
													applicationScenario
												SET
													isVolumeResized = '1'
												WHERE
													scenarioId = '$scenarioId'";						
						
						if(is_array($volume_resize_arary))
						{
							$multi_query_static["update"][] = $sql_update_sanLuns;
						}
						else
						{
							$sql_update_sanLuns_res = $conn_obj->sql_query($sql_update_sanLuns);
						}
					}
					foreach($plan_properties['pairInfo'] as $key=>$val)
					{												
						$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n application scenario updation block :: pair details :: ".print_r($val['pairDetails'],true)."\n");
						foreach($val['pairDetails'] as $keyA => $valA)
						{														
							if($pairType == 2 || $pairType == 3)
							{							
								if($valA['srcVol'] == $phy_lun_id)
								{								
									if($pairType == 2)
									{
										$plan_properties['pairInfo'][$key]['pairDetails'][$keyA]['srcCapacity'] = $new_capacity;
									}
									else
									{
										if($valA['trgCapacity'] < $new_capacity)
										{
											$plan_properties['pairInfo'][$key]['pairDetails'][$keyA]['srcCapacity'] = $valA['trgCapacity'];
										}
										else
										{
											$plan_properties['pairInfo'][$key]['pairDetails'][$keyA]['srcCapacity'] = $new_capacity;
										}
									}
								}
								if($pairType == 3)
								{
									if($valA['trgVol'] == $phy_lun_id)
									{
										$plan_properties['pairInfo'][$key]['pairDetails'][$keyA]['trgCapacity'] = $new_capacity;
										if($new_capacity < $valA['srcCapacity'])
										{
											$plan_properties['pairInfo'][$key]['pairDetails'][$keyA]['srcCapacity'] = $new_capacity;
										}
									}
								}
							}
							else
							{
								if($valA['srcVol']== $device_name)
								{								
									$plan_properties['pairInfo'][$key]['pairDetails'][$keyA]['srcCapacity'] = $new_capacity;
								}
							}
						}
					}
					foreach($plan_properties['pairInfo'] as $key=>$val)
					{						
						foreach($val['pairDetails'] as $keyA => $valA)
						{							
							$pair_size_arr[] = $plan_properties['pairInfo'][$key]['pairDetails'][$keyA]['srcCapacity'];	
							if($pairType == 3)
							{
								$trg_size_arr[] = $plan_properties['pairInfo'][$key]['pairDetails'][$keyA]['trgCapacity'];
							}
							if($pairType == 2 || $pairType == 3)
							{
								$src_vendor_origin = $valA['prismSpecificInfo']['vendorOrigin'];
								$src_volume_type = $valA['prismSpecificInfo']['volumeType'];
								if($plan_properties['planType'] == 'BULK')
								{
									$src_app_type = "";
								}
								else
								{
									$src_app_type = $plan_properties['applicationType'];
								}
							}
							
						}
					}
					if(count($plan_properties['reversepairInfo']) > 0)
					{
						foreach($plan_properties['reversepairInfo'] as $key=>$val)
						{						
							$i = 0;
							foreach($val['pairDetails'] as $keyA => $valA)
							{							
								$plan_properties['reversepairInfo'][$key]['pairDetails'][$keyA]['srcCapacity'] = $pair_size_arr[$i];
								if($pairType == 3)
								{
									$plan_properties['reversepairInfo'][$key]['pairDetails'][$keyA]['trgCapacity'] = $trg_size_arr[$i];
								}
								$i++;
							}
						}
					}
					$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n application scenario updation block :: plan properties array :: ".print_r($plan_properties,true)."\n");
					$str = serialize($plan_properties);
					/*$str = "<plan><header><parameters>";
					$str.= "<param name='name'>".$plan_properties['planName']."</param>";
					$str.= "<param name='type'>Protection Plan</param>";
					$str.= "<param name='version'>1.1</param>";
					$str.= "</parameters></header>";
					$str.= "<data><![CDATA[";
					$str.= $cdata;
					$str.= "]]></data></plan>";*/
					$str = $conn_obj->sql_escape_string($str);					
										
					$sql_app_sce = "UPDATE
								applicationScenario
							SET								
								scenarioDetails = '$str'
							WHERE
								scenarioId='$scenarioId'";
					
					if($pairType == 2 || $pairType == 3) //0=host,1=fabric,2=prism
					{
						// need to update session only if resize notification got from the node, incase 
						// of PS /dev/mapper device avoid it.
						if($source_vol_resize || $dest_vol_resize)
						{							
							$rs_app_sce = $conn_obj->sql_query($sql_app_sce);
						}
					}
					else
					{
						if(is_array($logical_volumes_arary))
						{
							
							$multi_query_static["update"][] = $sql_app_sce;
						}
						else
						{
							$rs_app_sce = $conn_obj->sql_query($sql_app_sce);
						}
					}
					
					$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Update query for applicationScenario table = $sql_app_sce \n");
				}
			}
						
			#in case of src vol resize, pause that pair if its target size is lessthan the new capacity of src 
			if($oneToMany)
			{
				$set = 2;
			}
			else
			{
				$set = 1;
			}
			$dst_device_str = array();
			if(($source_vol_resize == TRUE) && (($protectionDirection == '') || (!strcmp($protectionDirection,'forward'))))
			{
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n dest_detas =".print_r($dest_detas,TRUE)." \n");
				$pause_count = 0;
				foreach ($dest_detas as $key=>$value)
				{
					foreach  ($value as $key2=>$value2)
					{
						$dst_hostid = $key2;
						$dst_device = $value2;						
						$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Destination hostId = $dst_hostid \n\n Destination device = $dst_device \n\n key = $key \n\n value = ".print_r($value,TRUE)." \n");
												
						$logging_obj->my_error_handler("INFO","volumeResizeNotification \n calling get_pause_status function with parameters as Source hostid = $source_host_id \n\n Source device = $source_device \n\n Destination hostid = $dst_hostid & Destinaton device = $dst_device \n");
						$pair_pause_staus = $vol_obj->get_pause_status(
												$source_host_id,
												$source_device,
												$dst_hostid,
												$dst_device);							
						$logging_obj->my_error_handler("INFO","volumeResizeNotification \n  Pair pause staus =  $pair_pause_staus \n");
						$dst_device_str[$pause_count][$dst_hostid] = $dst_device;
						
						#$pair_details_additional_data[$src_host_id][$src_device_name] = array("deleted"=>$deleted, "replication_status"=>$replication_status, "job_id"=>$job_id, "is_quasi_flag"=>$is_quasi_flag, "restart_pause"=>$restart_pause, "destination_host_id"=>$dst_host_id,"destination_device_name"=>$dst_device_name);
						
						$pair_state = $pair_details_additional_data[$source_host_id][$source_device]["is_quasi_flag"];
						
						if ($pair_pause_staus == 0)
						{								
							//$dst_device_str[$pause_count][$dst_hostid] = $dst_device;
							/*
							  * Pause pair provided it's not paused already
							  */								
							$logging_obj->my_error_handler("INFO","volumeResizeNotification \n calling pause_replication function with parameters as Source hostid = $source_host_id\n\n Source device = $source_device \n\n Destination hostid = $dst_hostid \n\n Destination device = $dst_device \n\n set = $set, pair state = $pair_state, protection path = $protection_path \n");
							
							if (($pair_state != 2) && ($protection_path == 'E2A'))
							{
								$vol_obj->pause_replication(
												$source_host_id,
												$source_device,
												$dst_hostid,
												$dst_device,1,0,"disk resize");
							}
							elseif ($protection_path != 'E2A')
							{
								$vol_obj->pause_replication(
												$source_host_id,
												$source_device,
												$dst_hostid,
												$dst_device,
												$set);
							}
							$pause_count++;
						}
						
					}
				}				
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n  Destination device string = ".print_r($dst_device_str,TRUE)." \n");
			}
			
            if($is_lun_pair === true && $pairType == 1)
            {
                /*
				* It is a LUN resize notification for Fabric 
				* So, set capacityChanged flag in sanLuns
				* provided another scan is NOT already in progress
				*/

         		$sql_update_sanLuns = "UPDATE
                                            sanLuns
                                        SET
                                            capacityChanged = $CAPACITY_CHANGE_REPORTED
                                        WHERE
                                            sanLunId = '$lun_id' AND
                                            capacityChanged in ($CAPACITY_CHANGE_RESET, $CAPACITY_CHANGE_PROCESSING)";
               
				
				if(is_array($logical_volumes_arary))
				{
					$multi_query_static["update"][] = $sql_update_sanLuns;
				}
				else
				{
					$sql_update_sanLuns_res = $conn_obj->sql_query($sql_update_sanLuns);
				}
            }
			else if($is_lun_pair === true && ($pairType == 2 || $pairType == 3))
            {
                /*
				* It is a LUN resize notification for Prism
				* So, set volumeResize flag in sharedDevices.
				* we should update capacity only if existing capacity is 
				* less than reported capcity since we don't support
				* shrinking of lun and for prism if all the node keep
				* reporting lun resize then also we need to perform only
				* one time based on scsi id.
				*/
				
				$count = 0;
				if(is_array($logical_volumes_arary))
				{
					if(is_array($src_volumes_arary))
					{
						foreach($src_volumes_arary["pair_details"] as $key => $pair_details)
						{
							if($pair_details['sourceDeviceName'] == $device_name)
							{
								$count++;
							}
						}
					}
				}
				else
				{

					$sqlStmnt = "SELECT 
										sourceHostId 												
								FROM
									srcLogicalVolumeDestLogicalVolume 
								WHERE
									sourceDeviceName = '$device_escaped'
								";
					$resStmnt = $conn_obj->sql_query($sqlStmnt);
					$row = $conn_obj->sql_fetch_object($resStmnt);
					$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n check /dev/mapper device = $sqlStmnt \n\n Count = $conn_obj->sql_num_row($resStmnt) \n");
					$count = $conn_obj->sql_num_row($resStmnt);
				}
				if($count > 0)
				{
					// need to update capacity only if resize notification got from the PS /dev/mapper device, incase 
					// of node devices need to avoid it.
					$sql_update_sharedDevice = "UPDATE
												sharedDevices
											SET
												volumeResize = $CAPACITY_CHANGE_UPDATED,
												capacity = $new_capacity 
											WHERE
												sharedDeviceId = '$lun_id' AND
												capacity != $new_capacity AND 
												capacity < $new_capacity AND 
												volumeResize in ($CAPACITY_CHANGE_RESET, $CAPACITY_CHANGE_PROCESSING)";

					$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n update sharedDevice = $sql_update_sharedDevice \n\n");
					if(is_array($logical_volumes_arary))
					{
						$multi_query_static["update"][] = $sql_update_sharedDevice;
					}
					else
					{
						$sql_update_sharedDevice_res = $conn_obj->sql_query($sql_update_sharedDevice);
					}
					
				}
				else
				{

					$count = 0;
					if(is_array($logical_volumes_arary))
					{
						if(is_array($src_volumes_arary))
						{
							foreach($src_volumes_arary["pair_details"] as $key => $pair_details)
							{
								if($pair_details['srcCapacity'] == $new_capacity)
								{
									$count++;
								}
							}
						}
					}
					else
					{
						// control will come only if node device resized 
						$sqlStmnt = "SELECT 
											srcCapacity  												
									FROM
										srcLogicalVolumeDestLogicalVolume 
									WHERE
										srcCapacity = '$new_capacity'
									";
						$resStmnt = $conn_obj->sql_query($sqlStmnt);
						$row = $conn_obj->sql_fetch_object($resStmnt);
						$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Capacity already updated = $sqlStmnt \n\n Count = $conn_obj->sql_num_row($resStmnt)\n");
						$count = $conn_obj->sql_num_row($resStmnt);
					}
					if($count>0)
					{
						// need to return if any of the node already updated the capacity for pair 
						// and we should not write the message again in agent log.
						$logging_obj->my_error_handler("INFO","volumeResizeNotification \n Capacity already updated, returnning\n");
						return;
					}
				}
					
				
            }
        }

        /* 
                    *Re-sync required set to YES from CX on source volume resize.
                    *This is applicable only for host based volume resize, 
                    *not for fabric lun resize. For fabric on lun resize, the 
                    *resync required set from set_resync_required(), 
                    *which is source called.
		*/		
		$logging_obj->my_error_handler("INFO","volumeResizeNotification \n new_capacity_send = $new_capacity_send \n\n Is lun pair = $is_lun_pair \n\n Source volume resize = $source_vol_resize\n");
        if (($new_capacity_send == TRUE) or ($is_lun_pair == true))
        {
            $timestamp = time ();
			if($ps_volume_resize == TRUE)
			{
				if($pairType == 2)
				{
					$query ="UPDATE 
	                            srcLogicalVolumeDestLogicalVolume
	                        SET 
	                            shouldResync = 1, 
	                            ShouldResyncSetFrom = 1, 
	                            resyncSetCxtimestamp = $timestamp 
							WHERE 
	                            Phy_Lunid ='$lun_id' AND 
	                            destinationHostId != '".$INMAGE_PROFILER_HOST_ID."'";
					if(is_array($logical_volumes_arary))
					{
						$multi_query_static["update"][] = $query;
					}
					else
					{
						$query_status = $conn_obj->sql_query($query);
					}			
				}
			}
			
			if($source_vol_resize == TRUE)
			{
				if($pairType == 2)
				{
					$query =	"UPDATE 
	                            srcLogicalVolumeDestLogicalVolume
	                        SET 
	                            srcCapacity = $new_capacity 
	                        WHERE 
	                            Phy_Lunid ='$lun_id' AND 
	                            destinationHostId != '".$INMAGE_PROFILER_HOST_ID."'";
				}
				else if($pairType == 3)
				{
					$query =	"UPDATE 
	                            srcLogicalVolumeDestLogicalVolume
	                        SET 
	                            shouldResync = 1, 
	                            ShouldResyncSetFrom = 1, 
	                            resyncSetCxtimestamp = $timestamp,
								srcCapacity = $new_capacity
	                        WHERE 
	                            Phy_Lunid ='$lun_id' AND 
	                            destinationHostId != '".$INMAGE_PROFILER_HOST_ID."'";
				}
				else
				{
					$source_host_ids_string = implode(",",$host_ids); 
					$query =	"UPDATE 
	                            srcLogicalVolumeDestLogicalVolume
	                        SET 
								srcCapacity = $new_capacity
	                        WHERE 
	                            FIND_IN_SET(sourceHostId,'$source_host_ids_string') AND 
	                             sourceDeviceName='$source_device_escaped' AND 
	                            destinationHostId != '".$INMAGE_PROFILER_HOST_ID."'";
					$multi_query_static["update"][] = $query;
					$pair_state = $pair_details_additional_data[$host_id][$device_name]["is_quasi_flag"];
					if ( ($protection_path == 'E2A') && ($pair_state == 2) )
					{
						$query =	"UPDATE 
	                            srcLogicalVolumeDestLogicalVolume
	                        SET 
	                            shouldResync = 1, 
	                            ShouldResyncSetFrom = 1, 
	                            resyncSetCxtimestamp = $timestamp
	                        WHERE 
	                            FIND_IN_SET(sourceHostId,'$source_host_ids_string') AND 
	                             sourceDeviceName='$source_device_escaped' AND 
	                            destinationHostId != '".$INMAGE_PROFILER_HOST_ID."'";
						$CURRENT_TIME = time();
						$healthfactor_placeholders['OldVolumeSize']=$old_capacity;
						$healthfactor_placeholders['NewVolumeSize']=$new_capacity;
						$healthfactor_placeholders = json_encode($healthfactor_placeholders);
						$vol_obj->resync_error_code_action($source_host_id,$source_device_name,$dest_host_id, $dest_device, $job_id, $CONFIGURATION_SERVER, $CONFIGURATION_SERVER_DISK_RESIZE,$CURRENT_TIME,$healthfactor_placeholders);
					}
					else
					{
					$query =	"UPDATE 
	                            srcLogicalVolumeDestLogicalVolume
	                        SET 
	                            shouldResync = 1, 
	                            ShouldResyncSetFrom = 1, 
	                            resyncSetCxtimestamp = $timestamp,
								srcCapacity = $new_capacity
	                        WHERE 
	                            FIND_IN_SET(sourceHostId,'$source_host_ids_string') AND 
	                             sourceDeviceName='$source_device_escaped' AND 
	                            destinationHostId != '".$INMAGE_PROFILER_HOST_ID."'";
					}
				}
			   				

				if(is_array($logical_volumes_arary))
				{
					$multi_query_static["update"][] = $query;
				}
				else
				{
					$query_status = $conn_obj->sql_query($query);
				}
			}
			
			if($dest_vol_resize == true)
			{
				if(($new_capacity > $old_capacity) && ($pairType != 3))
				{
					$query =	"UPDATE 
		                            srcLogicalVolumeDestLogicalVolume
		                        SET 
		                            shouldResync = 1, 
		                            ShouldResyncSetFrom = 1, 
		                            resyncSetCxtimestamp = $timestamp
		                        WHERE 
		                            destinationHostId='$host_id' AND 
		                            destinationDeviceName='$device_escaped'";					

					if(is_array($logical_volumes_arary))
					{
						$multi_query_static["update"][] = $query;
					}
					else
					{
						$query_status = $conn_obj->sql_query($query);
					}
				}
			}           
        }

        	
		$logging_obj->my_error_handler("INFO","volumeResizeNotification\n Seek status = $seek_status \n");
        if ($seek_status == TRUE)
        {
            $count=0;
            /*Assign default values */
            $dest_device_name_str = "";
            $dest_host_name_str = "";
            $dest_ip_address_str = "";
            $volume_label1 = "";
            $volume_label2 = "";
            $ip_label = "";
            $host_label = "";
            /*Get all the detination details to form the message */
            #while( $dest_details_obj = $conn_obj->sql_fetch_object($dest_result_set))
			foreach ($dst_device_str as $key=>$value)
			{
				foreach ($value as $key3=>$value3)
				{
					
					$dst_device = $value3;           
	                $error_id = "VOL_RESIZE_WARNING";
	                $error_template_id = "VOL_RESIZE";
	                $message = '';
	                
					$dest_device_name = $value3;
					$dest_device_name = $conn_obj->sql_escape_string ($dest_device_name);
	                    
	                
					$dest_id = $key3;
	                $dest_host_name = $host_array[$dest_id]['name'];
	                
	                $dest_ip_address = $host_array[$dest_id]['ipAddress'];
	                
	                if($count == 0)
	                {
	                    $dest_device_name_str = $dest_device_name;
	                    $dest_host_name_str   = $dest_host_name;
	                    $dest_ip_address_str   = $dest_ip_address;
	                    $volume_label1 = "volume";
	                    $volume_label2 = "Volume";
	                    $ip_label = "IP";
	                    $host_label = "Host";
	                }
	                else
	                {
	                    $dest_device_name_str = $dest_device_name_str.','.$dest_device_name;
	                    $dest_host_name_str   = $dest_host_name_str.','.$dest_host_name;
	                    $dest_ip_address_str  =$dest_ip_address_str.','.$dest_ip_address;
	                    $volume_label1 = "volumes";
	                    $volume_label2 = "Volumes";
	                    $ip_label = "IPs";
	                    $host_label = "Hosts";
	                }
	                $count++;
				}
            }			
			$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n  Destination device name str = ".print_r($dest_device_name_str,TRUE).", Destination hostname str = ".print_r($dest_host_name_str,TRUE).", Destination ip address str = ".print_r($dest_ip_address_str,TRUE)." \n");
            if ($is_lun_pair === true && $pairType == 1)	// Fabric Pair		   		
            {
                /*
                                    * The message for fabric based lun volume resize.
                                    * This is to shoot immediate notification soon as resize occurs.
                                    * This message does not contain any capcity of the lun volume
                                     */
				$message = "The source LUN of ID  $lun_id has been resized and the associated replication pair is paused. CX is about to rescan/reconfigure the resized LUN automatically. Another email notification would be sent after reconfiguration is completed.<br><br>";                
				$summary = "Source LUN Resize Warning";
                $error_code = "EC0122";
                $arr_error_placeholders['LunId'] = $lun_id;
				
				if($cx_os_flag == 1)
				{
					$trap_info_str = " -trapvar ".$TRAP_SEVERITY."::s::Major";
					$trap_info_str .= " -trapvar ".$TRAP_CS_HOSTNAME."::s::\"".$cx_hostname."\"";
					$trap_info_str .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."::s::\"".$source_host_name1."\"";
					$trap_info_str .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."::s::\"".$source_device_name."\"";
					$trap_info_str .= " -trapvar ".$TRAP_TARGET_HOSTNAME."::s::\"".$destination_host_name1."\"";
					$trap_info_str .= " -trapvar ".$TRAP_TARGET_DEVICENAME."::s::\"".$dest_device."\"";
				}
				else
				{
					$trap_info_str = "Abhai-1-MIB-V1::trapSeverity s Major ";
					$trap_info_str .= "Abhai-1-MIB-V1::trapCSHostname s \"$cx_hostname\" ";
					$trap_info_str .= "Abhai-1-MIB-V1::trapSourceHostName s \"$source_host_name1\" ";
					$trap_info_str .= "Abhai-1-MIB-V1::trapSourceDeviceName s \"$source_device_name\" ";
					$trap_info_str .= "Abhai-1-MIB-V1::trapTargetHostName s \"$destination_host_name1\" ";
					$trap_info_str .= "Abhai-1-MIB-V1::trapTargetDeviceName s \"$dest_device\" ";
				}
            }
			else
            {
                /* 
				 * The message for host based volume resize. 
				 * This message contains both the old and 
				 * new capcity of the source volume
				 */				
				$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n  Source volume resize = $source_vol_resize \n\n Protection direction = $protectionDirection \n");				
				if($source_vol_resize == TRUE)
				{
					$disk_os_flag =	$commonfunctions_obj->get_osFlag($host_id);
					$disk_name = $commonfunctions_obj->get_disk_name($host_id, $device_escaped, $disk_os_flag);
					
					if ($is_lun_pair === true && ($pairType == 2 || $pairType == 3))	// Prism Pair		   		
					{
						$count = 0;
						$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Logical volumes data = ".print_r($logical_volumes_arary,true)."\n");
						if(is_array($logical_volumes_arary))
						{
							$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Application scenario data = ".print_r($application_scenario_array,true)."\n");
							if(is_array($application_scenario_array))
							{
								foreach ($application_scenario_array as $key => $application_details)
								{	
									$srcHostList = explode(",",$application_details['sourceId']);
									$srcVolList = explode(",",$application_details['sourceVolumes']);
									$tgtHostList = explode(",",$application_details['targetId']);
									$tgtVolList = explode(",",$application_details['targetVolumes']);
									
									if(((in_array($host_id,$srcHostList) &&  in_array($lun_id,$srcVolList)) || (in_array($host_id,$tgtHostList) && in_array($device_name,$tgtVolList))) && in_array($application_details['scenarioType'] , $scenario_type_array ))
									
									{
										$scenario_array[$count]['sourceId'] = $application_details['sourceId'];
										$count++;
										$logging_obj->my_error_handler("DEBUG","volumeResizeNotification::source id>>".$application_details['sourceId']."\n");	
										
									}
								}
							}
						}
						else
						{
							$target_vol_cond = ($pairType == 2) ? "targetVolumes LIKE '%$device_escaped%'" : "targetVolumes LIKE '%$phy_lun_id%'";
							$sqlScenario = "SELECT
										sourceId,
										scenarioDetails 
									FROM
										applicationScenario
									WHERE
										(sourceId LIKE '%$host_id%'
									AND
										sourceVolumes LIKE '%$lun_id%')
									OR
										(targetId LIKE '%$host_id%'	AND	$target_vol_cond)
									AND
										scenarioType IN('DR Protection','Backup Protection')";			
							$rsScenario = $conn_obj->sql_query($sqlScenario);			
							$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n sql query = $sqlScenario \n");
							if($conn_obj->sql_num_row($rsScenario)>0)
							{
								while($rowScenario = $conn_obj->sql_fetch_object($rsScenario))
								{	
									$scenario_array[$count]['sourceId'] = $rowScenario->sourceId;
									$count++;
								}
							}
						}
						
						$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Count = $count \n\n Scenario array = ".print_r($scenario_array,true)."\n");	
						if($count>0)
						{
							foreach ($scenario_array as $key => $scenario_details)
							{			
								$sourceNodeIds = $scenario_details['sourceId'];		
								#$sourceNodeIds = $rowScenario->sourceId;
								$node_host_ids = explode(",",$sourceNodeIds);
								$source_host_name_list=array();
								$source_ip_address_list=array();
								
								$source_host_name = "";
								$source_ip_address = "";
								foreach($node_host_ids as $nodeId)
								{
									$source_host_name_list[] = $host_array[$nodeId]['name'];
									$source_host_name = implode(",",$source_host_name_list);
									$source_ip_address_list[] = $host_array[$nodeId]['ipAddress'];
									$source_ip_address = implode(",",$source_ip_address_list);
									//device_name
								}
							}
						}	
						
						$logging_obj->my_error_handler("DEBUG","volumeResizeNotification \n Get primary volume name : Source node ids = $sourceNodeIds \n\n Lun id = $lun_id \n\n Source vendor origin = $src_vendor_origin \n\n Source volume type = $src_volume_type \n\n Source application type = $src_app_type \n\n Device escaped = $device_escaped \n");
						$device_escaped = "";
						
						$message = "The following pair has been paused as the source volume($device_escaped) has been resized from  $old_capacity to $new_capacity. CX is about to reconfigure the device automatically. Another email notification would be sent after reconfiguration is completed.<br><br>";
						
						if($cx_os_flag == 1)
						{
							$trap_info_str = " -trapvar ".$TRAP_SEVERITY."::s::Major";
							$trap_info_str .= " -trapvar ".$TRAP_CS_HOSTNAME."::s::\"".$cx_hostname."\"";
							$trap_info_str .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."::s::\"".$source_host_name1."\"";
							$trap_info_str .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."::s::\"".$source_device_name."\"";
							$trap_info_str .= " -trapvar ".$TRAP_TARGET_HOSTNAME."::s::\"".$destination_host_name1."\"";
							$trap_info_str .= " -trapvar ".$TRAP_TARGET_DEVICENAME."::s::\"".$dest_device."\"";
						}
						else
						{
							$trap_info_str = "Abhai-1-MIB-V1::trapSeverity s Major ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapCSHostname s \"$cx_hostname\" ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapSourceHostName s \"$source_host_name1\" ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapSourceDeviceName s \"$source_device_name\" ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapTargetHostName s \"$destination_host_name1\" ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapTargetDeviceName s \"$dest_device\" ";
						}
					}
					else
					{
						$error_code = "EC0124";
						if ($protection_path == 'E2A')
						{
							$error_code = $CONFIGURATION_SERVER_DISK_RESIZE;
							$message = "Source disk $device_escaped $disk_name has been resized from  $old_capacity bytes to $new_capacity bytes."; 
						}
						else
						{
							$message = "The following pair has been paused as the source disk $device_escaped $disk_name has been resized from  $old_capacity bytes to $new_capacity bytes. Please resize your target disk ($dest_device_name_str) to greater than or equal to source disk and resume the replication pair. <br><br>";
						}
						
                        $arr_error_placeholders['OldVolumeSize'] = $old_capacity;
                        $arr_error_placeholders['NewVolumeSize'] = $new_capacity;
						
						if($cx_os_flag == 1)
						{
							$trap_info_str = " -trapvar ".$TRAP_SEVERITY."::s::Major";
							$trap_info_str .= " -trapvar ".$TRAP_CS_HOSTNAME."::s::\"".$cx_hostname."\"";
							$trap_info_str .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."::s::\"".$source_host_name1."\"";
							$trap_info_str .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."::s::\"".$source_device_name."\"";
							$trap_info_str .= " -trapvar ".$TRAP_TARGET_HOSTNAME."::s::\"".$destination_host_name1."\"";
							$trap_info_str .= " -trapvar ".$TRAP_TARGET_DEVICENAME."::s::\"".$dest_device."\"";
						}
						else
						{
							$trap_info_str = "Abhai-1-MIB-V1::trapSeverity s Major ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapCSHostname s \"$cx_hostname\" ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapSourceHostName s \"$source_host_name1\" ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapSourceDeviceName s \"$source_device_name\" ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapTargetHostName s \"$destination_host_name1\" ";
							$trap_info_str .= "Abhai-1-MIB-V1::trapTargetDeviceName s \"$dest_device\" ";
						}
					}
					
					
					                    
					$summary = "Source Disk ($device_escaped) $disk_name Resize Warning";					
					$logging_obj->my_error_handler("DEBUG","volumeResizeNotification::source resize,message: $message, summary: $summary \n");
				}
				elseif(($dest_vol_resize == TRUE) && ($new_capacity > $old_capacity))
				{
					$message = "Target volume($device_escaped) of the following pair has been resized from  $old_capacity to $new_capacity. <br><br>";                    
					$summary = "Target Volume Resize Warning";
					$dest_host_name = $host_array[$host_id]['name'];                
					$dest_ip_address = $host_array[$host_id]['ipAddress'];
					$dest_device_name_str = $device_escaped;
                    $dest_host_name_str   = $dest_host_name;
                    $dest_ip_address_str   = $dest_ip_address;
                    $volume_label1 = "volume";
                    $volume_label2 = "Volume";
                    $ip_label = "IP";
                    $host_label = "Host";
				}
				if(($pause_count == 0) && ($source_vol_resize == TRUE))
				{					
					if ($protection_path == 'E2A')
					{
						$error_code = $CONFIGURATION_SERVER_DISK_RESIZE;
						$message = "Source disk $device_escaped $disk_name has been resized from  $old_capacity bytes to $new_capacity bytes."; 
					}
					else
					{
						$message = "Source disk ($device_escaped) of the following pair has been resized from  $old_capacity to $new_capacity. <br><br>";
					}
					$summary = "Source Disk ($device_escaped) $disk_name Resize Warning";					
					$logging_obj->my_error_handler("DEBUG","volumeResizeNotification pause_count:: message: $message, summary: $summary \n");
				}
            }
			$message = $commonfunctions_obj->customer_branding_names($message);
			$summary = $commonfunctions_obj->customer_branding_names($summary);
			
            if($error_code == "EC0124" || $error_code == "EC0122" || $error_code == $CONFIGURATION_SERVER_DISK_RESIZE)
            {
                $arr_error_placeholders['SrcHostName'] = $source_host_name;
                $arr_error_placeholders['SrcIPAddress'] = $source_ip_address;
                $arr_error_placeholders['SrcVolume'] = ($pairType == 2 || $pairType == 3) ? $device_escaped : $source_device_name;
                $arr_error_placeholders['DestHostName'] = $dest_host_name_str;
                $arr_error_placeholders['DestIPAddress'] = $dest_ip_address_str;
                $arr_error_placeholders['DestVolume'] = $dest_device_name_str;
				$arr_error_placeholders['DiskName'] = $disk_name;
				$arr_error_placeholders['DiskId'] = $device_escaped;
				$arr_error_placeholders['DetectionTime'] = $CURRENT_TIME ? $CURRENT_TIME : time();
            }
            
            $ser_arr_error_placeholders = (count($arr_error_placeholders)) ? serialize($arr_error_placeholders) : '';
			$logging_obj->my_error_handler("INFO","calling error message:: $error_id, $error_template_id, $summary, $message, $host_id, '', $error_code");

            $commonfunctions_obj->add_error_message($error_id, $error_template_id, $summary, $message, $host_id, '', $error_code, $ser_arr_error_placeholders);
							
            $message = str_replace("<br>", " ", $message);
			$message = str_replace("\n", " ", $message);
            
			if($source_vol_resize == TRUE)
			{
	            if($pairType == 2 || $pairType == 3) // Prism pair
				{
					$device_name = $source_device_name;
					$host_ids = array();
					
					$host_ids[] = $source_host_id;
				}
				foreach($host_ids as $key=>$host_id)
	            {					
					$logging_obj->my_error_handler("INFO",array("sourceDiskResize"=>$source_vol_resize,"message"=>$message,"summary"=>$summary,"hostId"=>$host_id,"deviceName"=>$device_name),Log::BOTH);
	                update_agent_log ($host_id, $device_name, $message, 0,"vol_resize");
	            }
			}
			else
			{
				if(($dest_vol_resize == TRUE) && ($new_capacity > $old_capacity))
				{					
					$logging_obj->my_error_handler("INFO","volumeResizeNotification \n calling update_agent_log function with parameters as Host id = $host_id \n\n Device name = $device_name \n\n Message = $message \n\n Summary = $summary \n");
					update_agent_log ($host_id, $device_name, $message, 0,"vol_resize");
				}
			}
        }				
				
    }		 
}

/*
 * Function Name: get_matched_device
 *
 * Description:
 *		The responsiblities of the function are as below.
 *		 Search the device name used to store new retention log path is already existing in logicalVolumes
 *
 *    
 * Parameters:
 *		Param 1 [IN]: $host_id
 *		Param 2 [IN]: $retention_path
 *		Param 3 [IN]: $os_flag
 *
 * Return Value:
 *
 * Exceptions:
*/
function get_matched_device($host_id,$retention_path,$os_flag)
{
    global $conn_obj,$logging_obj,$commonfunctions_obj;
    $device_sql="";
    /*deviceName for windows agents
            mount for linux agents*/
    if($os_flag == 1)
    {
        $device_sql="SELECT
                        deviceName 
                    FROM
                        logicalVolumes
                    WHERE hostId = '$host_id'"; 
                    
    }
    else
    {
        $device_sql="SELECT
                        mountPoint 
                    FROM
                        logicalVolumes
                    WHERE hostId = '$host_id'"; 
    }
    $device_rs = $conn_obj->sql_query($device_sql);
    while($device_name = $conn_obj->sql_fetch_row($device_rs))
    {
        /*check if the log device for retention log is valid deviceName*/
        if($device_name[0] != "")
        {
            /*windows is not case sensitive*/
            if($os_flag == 1)
            {
                $path = strtoupper($retention_path);
                $drive =strtoupper($device_name[0]);
            }
            else
            {
                $path = $retention_path;
                $drive =$device_name[0];
            }            
			$arr = explode("$drive",$path);			
            if($arr[0] == "")
            {
                return $device_name[0];
            }
        }
        else
        {
            continue;
        }
    }
    return;
}

function register_network_devices($nicinfo , $host_id, $osFlag, $cpu_array="")
{
	global $conn_obj, $commonfunctions_obj;
	$delete_ips = '';
	$nics_list_to_del = array();
	$mac_address_list = array();
	$nic_multi_query_static = array();
	if (is_array($nicinfo))
	{
		$delete_query = "delete from hostNetworkAddress where hostId = '$host_id'";
		$nic_multi_query_static[] = $delete_query;
		
		$ip_type_list = array();		
		$sql = "SELECT id,type from ipType";
		$rs = $conn_obj->sql_query($sql); 
		while($row = $conn_obj->sql_fetch_object($rs))
		{
			$ip_type_list[$row->type] = $row->id;
		}
		
		foreach( $nicinfo as $nicinfo_inner_array ) 
		{
			list(   
					$ver1,
					$deviceName,
					$ipAddress,
					$dnsHostName,
					$macAddress,
					$defaultIpGateways,
					$dnsServerAddresses,
					$index,
					$isDhcpEnabled,
					$manufacturer,
					$nicSpeed,
					$ipType
				) = $nicinfo_inner_array;
			
			$index_str = (string)$index;
			//If the string contains all digits, then only allow, otherwise return.
			if (isset($index) && $index != '' && (!preg_match('/^\d+$/',$index_str))) 
			{
				$commonfunctions_obj->bad_request_response("Invalid post data for osFlag $index in register_network_devices."); 
			}
		
			if($osFlag == 1) {
				if(preg_match('/Miniport/i',$deviceName)) {
					continue;
				}
			}else if($osFlag == 2) {
				if(preg_match('/^sit/i',$deviceName)) {
					continue;
				}
			}
			
				
			if(!is_array($ipAddress)) {
				$ipAddress_tmp = array();
				if($ipAddress){ 
					$ipAddress_tmp[$ipAddress] = '';
				}else{
					//Incase if IPAddress is blank
					$ipAddress_tmp[0] = '';
				}
				$ipAddress = $ipAddress_tmp;
			}
			$mac_address_list[] = $macAddress;
			
			foreach($ipAddress as $ip=>$subnetMask) 
			{
				$id = 0;
				if(count($ipType))
				{
					$type = $ipType[$ip];
					$id = $ip_type_list[$type];
				}
				
				if($ip) {
									
					$cols = "`hostId`,`deviceName`,`ipAddress`,`deviceType`,`macAddress`,`nicSpeed`,`gateway`,`dnsServerAddresses`,`networkAddressIndex`,`isDhcpEnabled`,`manufacturer`,`subnetMask`,`dnsHostName`,`ipType`";
					$vals = "'$host_id','".$conn_obj->sql_escape_string($deviceName)."','".$conn_obj->sql_escape_string($ip)."','1','".$conn_obj->sql_escape_string($macAddress)."','".$conn_obj->sql_escape_string($nicSpeed)."','".$conn_obj->sql_escape_string($defaultIpGateways)."','".$conn_obj->sql_escape_string($dnsServerAddresses)."','$index','$isDhcpEnabled','".$conn_obj->sql_escape_string($manufacturer)."','".$conn_obj->sql_escape_string($subnetMask)."','".$conn_obj->sql_escape_string($dnsHostName)."','$id'";
				}
				else {
										
					$cols = "`hostId`,`deviceName`,`deviceType`,`macAddress`,`nicSpeed`,`gateway`,`dnsServerAddresses`,`networkAddressIndex`,`isDhcpEnabled`,`manufacturer`,`subnetMask`,`dnsHostName`,`ipType`";
					$vals = "'$host_id','".$conn_obj->sql_escape_string($deviceName)."','1','".$conn_obj->sql_escape_string($macAddress)."','".$conn_obj->sql_escape_string($nicSpeed)."','".$conn_obj->sql_escape_string($defaultIpGateways)."','".$conn_obj->sql_escape_string($dnsServerAddresses)."','$index','$isDhcpEnabled','".$conn_obj->sql_escape_string($manufacturer)."','".$conn_obj->sql_escape_string($subnetMask)."','".$conn_obj->sql_escape_string($dnsHostName)."','$id'";
				}
				
				$query = "INSERT INTO 
							hostNetworkAddress 
							($cols) 
						VALUES
							($vals)";
				$nic_multi_query_static[] = $query;
			}
		}
	}
	
	if(is_array($cpu_array))
	{
		$cpu_delete_query = "delete from cpuInfo where hostId = '$host_id'";
		$nic_multi_query_static[] = $cpu_delete_query;
		
		foreach($cpu_array as $CPU => $cpuInfo)
		{
			$architecture = $cpuInfo[1]['architecture'];
			$manufacturer = $cpuInfo[1]['manufacturer'];
			$name =  $cpuInfo[1]['name'];
			$number_of_cores = $cpuInfo[1]['number_of_cores'];
			$number_of_logical_processors = $cpuInfo[1]['number_of_logical_processors'];
			
			$number_of_cores_str = (string)$number_of_cores;
			//If the string contains all digits, then only allow, otherwise return.
			if (isset($number_of_cores) && $number_of_cores != '' && (!preg_match('/^\d+$/',$number_of_cores_str))) 
			{
				$commonfunctions_obj->bad_request_response("Invalid post data for osFlag $number_of_cores in register_network_devices."); 
			}
			
			$number_of_logical_processors_str = (string)$number_of_logical_processors;
			//If the string contains all digits, then only allow, otherwise return.
			if (isset($number_of_logical_processors) && $number_of_logical_processors != '' && (!preg_match('/^\d+$/',$number_of_logical_processors_str))) 
			{
				$commonfunctions_obj->bad_request_response("Invalid post data for osFlag $number_of_logical_processors in register_network_devices."); 
			}
			
			
			$insert_cpu_info =  "INSERT 
									INTO
										cpuInfo
										( `hostId`,
										  `cpuNumber`,
										  `architecture`,
										  `manufacturer`,
										  `name`,
										  `noOfCores`,
										  `noOfLogicalProcessors`
										 )
									VALUES
										('$host_id',
										 '".$conn_obj->sql_escape_string($CPU)."',
										 '".$conn_obj->sql_escape_string($architecture)."',
										 '".$conn_obj->sql_escape_string($manufacturer)."',
										 '".$conn_obj->sql_escape_string($name)."',
										 '$number_of_cores',
										 '$number_of_logical_processors'
										)";
			$nic_multi_query_static[] = $insert_cpu_info;		
		}
	}
	return $nic_multi_query_static;
}
	function send_mail_trap_for_diffsync($id, $deviceName)
	{
		global $conn_obj,$commonfunctions_obj, $ABHAI_MIB, $TRAP_AMETHYSTGUID, $TRAP_AGENT_ERROR_MESSAGE, $TRAP_VX_PAIR_INITIAL_DIFFSYNC, $HOST_GUID;
		 $sql = "SELECT 
						sourceHostId,
						sourceDeviceName,
						destinationHostId,
						destinationDeviceName,
						restartResyncCounter					
					FROM 
						srcLogicalVolumeDestLogicalVolume
					WHERE					
						destinationHostId='$id' AND
						destinationDeviceName='$deviceName'
						" ;
								
		
					$result = $conn_obj->sql_query($sql);
					$row = $conn_obj->sql_fetch_array($result);
				
		if(!$row['restartResyncCounter'])
		{
			
			$src_host_name = $commonfunctions_obj->getHostName($row['sourceHostId']);
			$dest_host_name = $commonfunctions_obj->getHostName($row['destinationHostId']);		
			#$dest_ip_address = $commonfunctions_obj->getHostIpaddress($host_id);
			
			$summary = "Volume pair reached to diff sync";
			$error_id = "VOL_DIFFSYNC_ALERT_".$row['destinationHostId']."_".$row['destinationDeviceName'];
			$error_template_id = "PROTECTION_ALERT";
			$message = "The following volume replication pair has reached diffsync. <br> $src_host_name (".$row['sourceDeviceName'].") -> $dest_host_name (".$row['destinationDeviceName'].")";
			$error_code = 'AC0007';
            $arr_error_placeholders = array();
            $arr_error_placeholders['SrcHostName'] = $src_host_name;
            $arr_error_placeholders['SrcVolume'] = $row['sourceDeviceName'];
            $arr_error_placeholders['DestHostName'] = $dest_host_name;
            $arr_error_placeholders['DestVolume'] = $row['destinationDeviceName'];
            $ser_arr_error_placeholders = serialize($arr_error_placeholders);

			$commonfunctions_obj->add_error_message($error_id, $error_template_id, $summary, $message, $row['sourceHostId'], '', $error_code, $ser_arr_error_placeholders);

            $cx_hostid = $HOST_GUID;
			$cx_os_flag = $commonfunctions_obj->get_osFlag($cx_hostid);
			
			$trap_cmdline = '';
			if($cx_os_flag == 1)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_VX_PAIR_INITIAL_DIFFSYNC;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."::s::\"".$cx_hostid."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."::s::\"".$message."\"";
			}
			else
			{
				$trap_cmdline   .= "Abhai-1-MIB-V1::trapVXPairInitialDiffSync ";
				$trap_cmdline   .= "Abhai-1-MIB-V1::trapAmethystGuid ";
				$trap_cmdline   .= "s \"$cx_hostid\" ";
				$trap_cmdline   .= "Abhai-1-MIB-V1::$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$message \" ";
			}
			$commonfunctions_obj->send_trap($trap_cmdline, $error_template_id);
		}
	}
	
function get_refresh_host_status($host_id,$compatibility_num,$final_hash=array())
{
	/*
		refreshStatus, will contain the requestID of on_demand registration.
		Return Value:-
			1) Will be empty array if no request Id is present.
			2) Will be array('on_demand_registration_request_id'=> "Id"), incase if request Id is present.
			3) Will be any_jobs_to_be_processed = yes / no based on the jobs to be processed.
		
	*/
	global $conn_obj,$logging_obj,$IP_ADDRESS_FOR_AZURE_COMPONENTS, $CS_IP;
	$srm_obj = new SRM();
	$system_obj = new System();
	$result = array();
	$host_hash = $final_hash[2];
	$refreshStatus = $host_hash[$host_id]['refreshStatus'];	
	if($refreshStatus) {
		if($compatibility_num >= 710000) {
			$request_id = $refreshStatus;
			if($request_id) {
				$option = '';
				$sql = "SELECT requestData FROM apiRequestDetail WHERE hostId='$host_id' AND apiRequestId='$request_id'";
				$rs = $conn_obj->sql_query($sql);
				$num_rows = $conn_obj->sql_num_row($rs);
				if($num_rows) {
					$row1 = $conn_obj->sql_fetch_object($rs);
					$data = $row1->requestData;
					$data = unserialize($data);
					if($data['Option']) {
						$option = $data['Option'];
					}
				}
				$result['on_demand_registration_request_id'] = $request_id."";
                $result['disks_layout_option'] = $option."";
			}
		}
		else 
        {
			if($refreshStatus) 
            {
				$result['on_demand_registration_request_id'] = $refreshStatus."";
			}
		}
	}
	if($compatibility_num > 9100000) 
	{
		if($system_obj->agentHasJobs($host_id)) 
		{
			$result['any_jobs_to_be_processed'] = 'yes';
		} else {
			$result['any_jobs_to_be_processed'] = 'no';
		}
		
		//Initializing to empty string to avoid unmarshals at agent processing time.
		$result['cs_address_for_azure_components'] = $CS_IP;
		if (isset($IP_ADDRESS_FOR_AZURE_COMPONENTS))
		{
			$result['cs_address_for_azure_components'] = $IP_ADDRESS_FOR_AZURE_COMPONENTS;
		}
	}
	$logging_obj->my_error_handler("DEBUG","inside get_refresh_host_status result :: ".print_r($result,true)."\n");
	return $result;
}

function update_api_request($host_id,$request_id,$status)
{
	global $conn_obj,$multi_query_static;
	
	$state = 4;
	if($status == "success") {
		$state = 3;
	}
			
	$sql = "UPDATE
				apiRequestDetail
			SET
				state = '$state'
			WHERE
				apiRequestId = '$request_id' AND
				hostId = '$host_id' AND
				state IN ('1','2')";
	#$multi_query_static["update"][] = $sql;
	$rs = $conn_obj->sql_query($sql);
	
	if($state == 4) {
		$sql = "UPDATE
					apiRequest
				SET
					state = '$state'
				WHERE
					apiRequestId = '$request_id' AND
					state IN ('1','2')";
		#$multi_query_static["update"][] = $sql;
		$rs1 = $conn_obj->sql_query($sql);
	}
	else {
		$sql_state = "SELECT state FROM apiRequest WHERE apiRequestId='$request_id' AND state IN ('1','2')";
		$result_set = $conn_obj->sql_query($sql_state);
		$num_rows = $conn_obj->sql_num_row($result_set);
		if($num_rows) {
			$get_state = "SELECT state 
						  FROM apiRequestDetail 
						  WHERE apiRequestId = '$request_id' AND state IN ('1','2')";
			$result_set = $conn_obj->sql_query($get_state);
			$count = $conn_obj->sql_num_row($result_set);
			if(!$count) {
				$sql = "UPDATE
							apiRequest
						SET
							state = '3'
						WHERE
							apiRequestId = '$request_id' AND
							state IN ('1','2')";
				$rs1 = $conn_obj->sql_query($sql);
			}
		}
	}
}
/*
* Function Name: cluster_logical_volumes_sync
*
* Description:
* This function is used to insert/update the volume info for passive node as part of active node VIC call 
* as passive is not capable of reporting mountpoints and capacity for normal volumes
*  
*
* Parameters: 
* All the activde node device properties, passive node hostid,device name , capacity array($cluster_device_array), 
*    
* Return Value:
*     Adds insert or update queries to batch queries. 
*
*/
 
function cluster_logical_volumes_sync($id,$scsi_id,$logicalVolume_name,$mount_point,$capacity,$physical_offset,$file_system,$freespace,$is_system_logicalVolume,$is_cache_logicalVolume,$my_current_time,$deviceLocked,$case_volume_type,$logicalVolumeLabel,$logicalVolumeId,$isUsedForPaging,$rawSize,$writecacheEnabled,$isMultipath,$vendorOrigin,$formatLabel,$cluster_device_array,$farline_protected, $device_flag_inuse= 0)
{
	global $multi_query_static,$logging_obj,$conn_obj,$capacity_array;
	
	$logicalVolume_name_escaped = $conn_obj->sql_escape_string($logicalVolume_name);
	$logging_obj->my_error_handler("INFO","inside cluster_logical_volumes_sync logicalVolume_name :: $logicalVolume_name \n");

	foreach($cluster_device_array as $host_id => $device_array)
	{
		if(!array_key_exists($logicalVolume_name,$device_array) && strlen($logicalVolume_name)>1) 
		{
			$insert_cluster_Volume= "INSERT 
									INTO
										logicalVolumes 
										(`hostId`,
										`deviceName`, 
										`mountPoint`, 
										`capacity`, 
										`lastSentinelChange`,
										`lastOutpostAgentChange`, 
										`dpsId`, 
										`farLineProtected`, 
										`startingPhysicalOffset`, 
										`fileSystemType`, 
										`freeSpace`, 
										`systemVolume`,
										`cacheVolume`, 
										`lastDeviceUpdateTime`,
										`deviceLocked`,
										`volumeType`,
										`volumeLabel` ,
										`deviceId`,
										`isUsedForPaging`,
										`rawSize`,
										`writecacheEnabled`,
										`Phy_Lunid`,
										`isMultipath`,
										`vendorOrigin`,
										`formatLabel`) 
									VALUES
										('$host_id', 
										'$logicalVolume_name_escaped',
										'$mount_point',
										$capacity,
										0,
										0,
										'',
										0,
										$physical_offset,
										'$file_system',
										$freespace,
										$is_system_logicalVolume,
										$is_cache_logicalVolume, 
										$my_current_time,
										$deviceLocked,
										$case_volume_type,
										'$logicalVolumeLabel',
										'$logicalVolumeId',
										'$isUsedForPaging',
										$rawSize,
										'$writecacheEnabled',
										'$scsi_id',
										'$isMultipath',
										'$vendorOrigin',
										'$formatLabel')";
			$logging_obj->my_error_handler("INFO","configurator_register_logicalvolumes \n insert_cluster_Volume query = $insert_cluster_Volume \n");
			$multi_query_static["insert"][] = $insert_cluster_Volume;				
		}
		elseif(array_key_exists($logicalVolume_name,$device_array))
		{
			
			$update_size = " ,rawSize = $rawSize";
			if(($farline_protected == 1 || $device_flag_inuse == 1)&& $rawSize == 0)$update_size = "";
			$update_cluster_volume = "UPDATE 
										logicalVolumes 
									SET 
										capacity = $capacity,
										volumeType = $case_volume_type,
										mountPoint = '$mount_point' $update_size
									WHERE
										hostId = '$host_id'
									AND
										deviceName = '$logicalVolume_name_escaped'";
										
			$capacity_array[$host_id][$logicalVolume_name]['capacity'] = $capacity;
			$capacity_array[$host_id][$logicalVolume_name]['systemVolume'] = $is_system_logicalVolume;
			$capacity_array[$host_id][$logicalVolume_name]['cacheVolume'] = $is_cache_logicalVolume;
			$capacity_array[$host_id][$logicalVolume_name]['lastDeviceUpdateTime'] = $my_current_time;
			$capacity_array[$host_id][$logicalVolume_name]['startingPhysicalOffset'] = $physical_offset;
			$capacity_array[$host_id][$logicalVolume_name]['volumeLabel'] = $logicalVolumeLabel;
			$capacity_array[$host_id][$logicalVolume_name]['mountPoint'] = $mount_point;
			$capacity_array[$host_id][$logicalVolume_name]['isUsedForPaging'] = $isUsedForPaging;
			$capacity_array[$host_id][$logicalVolume_name]['writecacheEnabled'] = $writecacheEnabled;
			$capacity_array[$host_id][$logicalVolume_name]['Phy_Lunid'] = $scsi_id;
			$capacity_array[$host_id][$logicalVolume_name]['isMultipath'] = $isMultipath;
			$capacity_array[$host_id][$logicalVolume_name]['fileSystemType'] = $file_system;
			$capacity_array[$host_id][$logicalVolume_name]['volumeType'] = $case_volume_type;	
			$capacity_array[$host_id][$logicalVolume_name]['deviceLocked'] = $deviceLocked;	
			$capacity_array[$host_id][$logicalVolume_name]['vendorOrigin'] = $vendorOrigin;	
			$capacity_array[$host_id][$logicalVolume_name]['formatLabel'] = $formatLabel;	
			if($update_size)
			$capacity_array[$host_id][$logicalVolume_name]['rawSize'] = $rawSize;
			
			$capacity_array[$id][$logicalVolume_name]['capacity'] = $capacity;
			$capacity_array[$id][$logicalVolume_name]['systemVolume'] = $is_system_logicalVolume;
			$capacity_array[$id][$logicalVolume_name]['cacheVolume'] = $is_cache_logicalVolume;
			$capacity_array[$id][$logicalVolume_name]['lastDeviceUpdateTime'] = $my_current_time;
			$capacity_array[$id][$logicalVolume_name]['startingPhysicalOffset'] = $physical_offset;
			$capacity_array[$id][$logicalVolume_name]['volumeLabel'] = $logicalVolumeLabel;
			$capacity_array[$id][$logicalVolume_name]['mountPoint'] = $mount_point;
			$capacity_array[$id][$logicalVolume_name]['isUsedForPaging'] = $isUsedForPaging;
			$capacity_array[$id][$logicalVolume_name]['writecacheEnabled'] = $writecacheEnabled;
			$capacity_array[$id][$logicalVolume_name]['Phy_Lunid'] = $scsi_id;
			$capacity_array[$id][$logicalVolume_name]['isMultipath'] = $isMultipath;
			$capacity_array[$id][$logicalVolume_name]['fileSystemType'] = $file_system;
			$capacity_array[$id][$logicalVolume_name]['volumeType'] = $case_volume_type;
			$capacity_array[$id][$logicalVolume_name]['deviceLocked'] = $deviceLocked;
			$capacity_array[$id][$logicalVolume_name]['vendorOrigin'] = $vendorOrigin;
			$capacity_array[$id][$logicalVolume_name]['formatLabel'] = $formatLabel;
			if($update_size)				
			$capacity_array[$id][$logicalVolume_name]['rawSize'] = $rawSize;
			
			$logging_obj->my_error_handler("INFO","configurator_register_logicalvolumes \n update_cluster_volume query = $update_cluster_volume \n");
			$multi_query_static["update"][] = $update_cluster_volume;
			
		}
	}
}
function get_cluster_peer_hostid($id)
{
	global $conn_obj;
	$cluster_ids = array();
	$sql_host_id = "SELECT
					c1.hostId as hostId
				FROM
					clusters c1,
					clusters c2
				WHERE
					c1.clusterId = c2.clusterId
				AND
					c1.hostId != c2.hostId
				AND
					c2.hostId = '$id'";
	$rs_host_id = $conn_obj->sql_query($sql_host_id);
	while($host_id_obj =  $conn_obj->sql_fetch_object($rs_host_id))
	{
		$cluster_ids[] = $host_id_obj->hostId;
	}
	$cluster_ids = array_unique($cluster_ids);
	return $cluster_ids;
}

function hash_construct($srcIdsIn, $srcCompatibilityNum)
{
	global $conn_obj;
	global $PS_DELETE_DONE,$SOURCE_DELETE_DONE;
	global $vol_obj,$logging_obj;
        $newColumns = '';
        if ($srcCompatibilityNum >= 630050)
        {
            $newColumns .= ', syncCopy';
        }
        if($srcCompatibilityNum >= 710000)
        {
            $newColumns .= ', scenarioId, usePsNatIpForSource, usePsNatIpForTarget';
        }
        if($srcCompatibilityNum >= 820000)
        {
            $newColumns .= ', useCfs ';
        }
		
		$newColumns .= ',  TargetDataPlane ';
	/*
	* This query is used to get all the pairs info related to calling hostid
	* We used UNION to join the pairs of the calling hostid as source 
	* to the pairs of the calling hostid as target instead of IN clause because UNION is less costly operation than IN clause
	*/
        $query = "(select sourceHostId, sourceDeviceName, destinationHostId, destinationDeviceName, ftpsSourceSecureMode, ftpsDestSecureMode, isResync, resyncVersion, volumeGroupId, resyncStartTagtime,resyncEndTagtime,ShouldResyncSetFrom,resyncSettimestamp,shouldResync , isQuasiflag, oneToManySource,compressionEnable,jobId,resyncStartTagtimeSequence,resyncEndTagtimeSequence, Phy_Lunid, lun_state, replication_status,src_replication_status,tar_replication_status , srcCapacity, replicationCleanupOptions, pauseActivity, differentialApplyRate, remainingDifferentialBytes ,UNIX_TIMESTAMP(currentRPOTime) as rpotime, UNIX_TIMESTAMP(statusUpdateTime) as statusupdatetime, executionState, rpoSLAThreshold, mediaretent, ruleId, processServerId , pairId, rpoSLAThreshold, oneToManySource, throttleresync, throttleDifferentials, planId, restartResyncCounter as resyncCounter $newColumns from srcLogicalVolumeDestLogicalVolume where sourceHostId = '$srcIdsIn' and replication_status NOT IN ('$SOURCE_DELETE_DONE','$PS_DELETE_DONE') and executionState NOT IN ('2','3') ORDER BY sourcedeviceName,oneToManySource desc )
	UNION 
	(select sourceHostId, sourceDeviceName, destinationHostId, destinationDeviceName, ftpsSourceSecureMode, ftpsDestSecureMode, isResync, resyncVersion, volumeGroupId, resyncStartTagtime,resyncEndTagtime,ShouldResyncSetFrom,resyncSettimestamp,shouldResync , isQuasiflag, oneToManySource,compressionEnable,jobId,resyncStartTagtimeSequence,resyncEndTagtimeSequence, Phy_Lunid, lun_state, replication_status,src_replication_status,tar_replication_status , srcCapacity, replicationCleanupOptions, pauseActivity, differentialApplyRate, remainingDifferentialBytes ,UNIX_TIMESTAMP(currentRPOTime) as rpotime, UNIX_TIMESTAMP(statusUpdateTime) as statusupdatetime, executionState, rpoSLAThreshold, mediaretent, ruleId, processServerId , pairId, rpoSLAThreshold, oneToManySource, throttleresync, throttleDifferentials, planId, restartResyncCounter as resyncCounter $newColumns from srcLogicalVolumeDestLogicalVolume where destinationHostId = '$srcIdsIn' and replication_status NOT IN ('$SOURCE_DELETE_DONE','$PS_DELETE_DONE') and executionState NOT IN ('2','3') ORDER BY sourcedeviceName,oneToManySource desc) ";
	$iter =  $conn_obj->sql_query($query);
	
    if( !$iter ) 
    {
        trigger_error( $conn_obj->sql_error() );
    }
    $host_id_array = array();
	$lv_array = array();
	$host_array = array();
	$phy_lun_id_array = array();
	$sd_array = array();
	$hatlm_array = array();
	$ret_array = array();
	$pair_array = array();
	$ps_id_array = array();
	$license_array = array();
	$nic_array = array();
	$plan_id_array = array();
	$psIdArray =  array();
	$nic_dev_array =  array();
	$ruleId_array = array();
	$pair_rule_id_array = array();
	$one_to_many_array = array();
	$cnt =0;
	
	while ($row = $conn_obj->sql_fetch_object( $iter ))
	{
		if($srcCompatibilityNum >= 630050) 
		{
			$pair_rule_id = $row->pairId;
			$sync_copy = $row->syncCopy;
		}	
		else 
		{
			$pair_rule_id = $row->ruleId;
			$sync_copy = '';
		}
		if(in_array($pair_rule_id,$pair_rule_id_array))
		continue;
		$pair_array[$pair_rule_id]['sourceHostId'] = $row->sourceHostId;
		$pair_array[$pair_rule_id]['sourceDeviceName'] = $row->sourceDeviceName;
		$pair_array[$pair_rule_id]['destinationHostId'] = $row->destinationHostId;
		$pair_array[$pair_rule_id]['destinationDeviceName'] = $row->destinationDeviceName;
		$pair_array[$pair_rule_id]['ftpsSourceSecureMode'] = $row->ftpsSourceSecureMode;
		$pair_array[$pair_rule_id]['ftpsDestSecureMode'] = $row->ftpsDestSecureMode;
		$pair_array[$pair_rule_id]['isResync'] = $row->isResync;
		$pair_array[$pair_rule_id]['resyncVersion'] = $row->resyncVersion;
		$pair_array[$pair_rule_id]['shouldResync'] = $row->shouldResync;
		$pair_array[$pair_rule_id]['isQuasiflag'] = $row->isQuasiflag;
		$pair_array[$pair_rule_id]['oneToManySource'] = $row->oneToManySource;
		$pair_array[$pair_rule_id]['compressionEnable'] = $row->compressionEnable;
		$pair_array[$pair_rule_id]['jobId'] = $row->jobId;
		$pair_array[$pair_rule_id]['resyncStartTagtimeSequence'] = $row->resyncStartTagtimeSequence;
		$pair_array[$pair_rule_id]['resyncEndTagtimeSequence'] = $row->resyncEndTagtimeSequence;
		$pair_array[$pair_rule_id]['Phy_Lunid'] = $row->Phy_Lunid;
		$pair_array[$pair_rule_id]['lun_state'] = $row->lun_state;
		$pair_array[$pair_rule_id]['replication_status'] = $row->replication_status;
		$pair_array[$pair_rule_id]['src_replication_status'] = $row->src_replication_status;
		$pair_array[$pair_rule_id]['tar_replication_status'] = $row->tar_replication_status;		
		$pair_array[$pair_rule_id]['srcCapacity'] = $row->srcCapacity;
		$pair_array[$pair_rule_id]['replicationCleanupOptions'] = $row->replicationCleanupOptions;
		$pair_array[$pair_rule_id]['pauseActivity'] = $row->pauseActivity;
		$pair_array[$pair_rule_id]['differentialApplyRate'] = $row->differentialApplyRate;
		$pair_array[$pair_rule_id]['remainingDifferentialBytes'] = $row->remainingDifferentialBytes;
		$pair_array[$pair_rule_id]['rpotime'] = $row->rpotime;
		$pair_array[$pair_rule_id]['statusupdatetime'] = $row->statusupdatetime;
		$pair_array[$pair_rule_id]['executionState'] = $row->executionState;
		$pair_array[$pair_rule_id]['mediaretent'] = $row->mediaretent;
		$pair_array[$pair_rule_id]['ruleId'] = $row->ruleId;
		$pair_array[$pair_rule_id]['processServerId'] = $row->processServerId;
		$pair_array[$pair_rule_id]['sync_copy'] = $sync_copy;
		$pair_array[$pair_rule_id]['rpoSLAThreshold'] = $row->rpoSLAThreshold; 
		$pair_array[$pair_rule_id]['oneToManySource'] = $row->oneToManySource;
		$pair_array[$pair_rule_id]['throttleresync'] = $row->throttleresync;
		$pair_array[$pair_rule_id]['throttleDifferentials'] = $row->throttleDifferentials;  
		$pair_array[$pair_rule_id]['volumeGroupId'] = $row->volumeGroupId;  
		$pair_array[$pair_rule_id]['resyncStartTagtime'] = $row->resyncStartTagtime;  
		$pair_array[$pair_rule_id]['resyncEndTagtime'] = $row->resyncEndTagtime;  
		$pair_array[$pair_rule_id]['planId'] = $row->planId;  
		$pair_array[$pair_rule_id]['resyncCounter'] = $row->resyncCounter;
		$pair_array[$pair_rule_id]['resyncSettimestamp'] = $row->resyncSettimestamp;
		$pair_array[$pair_rule_id]['ShouldResyncSetFrom'] = $row->ShouldResyncSetFrom;
		
		if($srcCompatibilityNum >= 710000)
		{
			$pair_array[$pair_rule_id]['scenarioId'] = $row->scenarioId;
			$pair_array[$pair_rule_id]['usePsNatIpForSource'] = $row->usePsNatIpForSource;
			$pair_array[$pair_rule_id]['usePsNatIpForTarget'] = $row->usePsNatIpForTarget;			
		}
                if($srcCompatibilityNum >= 820000)
                {
                    $pair_array[$pair_rule_id]['useCfs'] = $row->useCfs;
                }
		
		$pair_array[$pair_rule_id]['TargetDataPlane'] = $row->TargetDataPlane;
		
		$pair_rule_id_array[] = $pair_rule_id;
			
		if($row->throttleDifferentials)
		{
			$one_to_many_array[$row->sourceHostId ."!!". $row->sourceDeviceName."!!".$row->Phy_Lunid]['throttleDifferentials'] = $row->throttleDifferentials;
		}
		/* Collecting all the hostids */		
		$host_id_array[] = $row->sourceHostId;			
		$host_id_array[] = $row->destinationHostId;
		$host_id_array[] = $row->processServerId;
		
		$phy_lun_id_array[] = $row->Phy_Lunid;
		$ruleId_array[] = $row->ruleId;
		$ps_id_array[] = $row->processServerId;

		$ret_array[$row->ruleId]['destinationHostId'] = $row->destinationHostId;
		$ret_array[$row->ruleId]['destinationDeviceName'] = $row->destinationDeviceName;
		$ret_array[$row->ruleId]['mediaretent'] = $row->mediaretent;
		$cnt++;
	}
	$host_id = array_unique($host_id_array);
	$host_id = implode("','",$host_id);	
	
	
	$ps_id = array_unique($ps_id_array);
	$ps_id = implode("','",$ps_id);
	
	$ps_array = array();
	/*
	* This query is used to get all the process servers table data which are used by current calling hostid pairs
	* And this info is stored in array "$ps_array"
	*/
	$ps_sql = "SELECT cummulativeThrottle, processServerId, port, sslPort, ps_natip, ipaddress FROM processServer FORCE INDEX (PRIMARY) WHERE processServerId IN ('$ps_id')";
	$ps_rs =  $conn_obj->sql_query($ps_sql);
	if( !$ps_rs ) 
    {
        trigger_error( $conn_obj->sql_error() );
    }
	while($ps_data = $conn_obj->sql_fetch_object($ps_rs))
	{
		$ps_array[$ps_data->processServerId]['cummulativeThrottle'] = $ps_data->cummulativeThrottle;
		$ps_array[$ps_data->processServerId]['port'] = $ps_data->port;
		$ps_array[$ps_data->processServerId]['sslPort'] = $ps_data->sslPort;
		$ps_array[$ps_data->processServerId]['ps_natip'] = $ps_data->ps_natip;
		$ps_array[$ps_data->processServerId]['ipaddress'] = $ps_data->ipaddress;
	}
	/*
	* When we do prism protection pairs will be inserted with appliance host id instead of actual source node, and also in logicalVolumes * table only appliance host related devices are updated as protceted.So,when we get call from multipath source node as multipath     * source node devices are not updated as protected in data base even though it is protected we should not use tmId condition and also * we need to include calling host id in where condtion to get corresponding devices info
	*/
	
	if(!$host_id)
	{
		$host_id = $srcIdsIn;
	}
	/*
	* This query is used to get logicalVolumes table data for the 
	* source,target,processServer hostid of the current calling hostid pairs
	* Finally data will be stored in array $lv_array
	*/
	$lv_sql = "SELECT hostId, deviceName, capacity, lastSentinelChange, lastOutpostAgentChange, farLineProtected, fileSystemType, transProtocol,mountPoint , rawSize , volumeType, startingPhysicalOffset, Phy_Lunid, readwritemode ,visible  FROM logicalVolumes FORCE INDEX (hostId , logicalVolumes_tmId) WHERE hostId IN ('$host_id')";
	$lv_rs =  $conn_obj->sql_query($lv_sql);
	if( !$lv_rs ) 
    {
        trigger_error( $conn_obj->sql_error() );
    }
	while($lv_data = $conn_obj->sql_fetch_object($lv_rs))
	{
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['lastSentinelChange'] = $lv_data->lastSentinelChange;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['fileSystemType'] = $lv_data->fileSystemType;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['transProtocol'] = $lv_data->transProtocol;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['mountPoint'] = $lv_data->mountPoint;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['volumeType'] = $lv_data->volumeType;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['startingPhysicalOffset'] = $lv_data->startingPhysicalOffset;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['readwritemode'] = $lv_data->readwritemode;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['visible'] = $lv_data->visible;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['capacity'] = $lv_data->capacity;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['rawSize'] = $lv_data->rawSize;
		$lv_array[$lv_data->hostId ."!!".$lv_data->deviceName]['Phy_Lunid'] = $lv_data->Phy_Lunid;
		$phy_lun_id_array[] = $lv_data->Phy_Lunid;
	}
	
	$phy_lun_id = array_unique($phy_lun_id_array);
	$phy_lun_id = implode("','",$phy_lun_id);
	/*
	* This query is used to get host table info for source,target,processServer hostid of the current calling hostid pairs
	*/
	$hst_sql = "SELECT compatibilityNo, osFlag, name, id, usepsnatip, ipAddress , refreshStatus , latestMBRFileName , agentRole FROM hosts FORCE INDEX (PRIMARY) WHERE id IN ('$host_id')";
	$hst_rs =  $conn_obj->sql_query($hst_sql);
	if( !$hst_rs ) 
    {
        trigger_error( $conn_obj->sql_error() );
    }
	
	while($hst_data = $conn_obj->sql_fetch_object($hst_rs))
	{
		$host_array[$hst_data->id]['compatibilityNo'] = $hst_data->compatibilityNo;
		$host_array[$hst_data->id]['osFlag'] = $hst_data->osFlag;
		$host_array[$hst_data->id]['name'] = $hst_data->name;
		$host_array[$hst_data->id]['usepsnatip'] = $hst_data->usepsnatip;
		$host_array[$hst_data->id]['ipAddress'] = $hst_data->ipAddress;
		$host_array[$hst_data->id]['refreshStatus'] = $hst_data->refreshStatus;
		$host_array[$hst_data->id]['latestMBRFileName'] = $hst_data->latestMBRFileName;
		$host_array[$hst_data->id]['agentRole'] = $hst_data->agentRole;
	}
	
	foreach($pair_array as $pair_id => $pair_data)
	{

		$pair_key = $pair_data['sourceHostId']."!!".$pair_data['sourceDeviceName']."!!".$pair_data['Phy_Lunid'];
		if($one_to_many_array[$pair_key]['compatibilityNo'] || $one_to_many_array[$pair_key]['rpoSLAThreshold']) 
		{
			if($host_array[$pair_data['destinationHostId']]['compatibilityNo'] < $one_to_many_array[$pair_key]['compatibilityNo'])
			$one_to_many_array[$pair_key]['compatibilityNo'] = $host_array[$pair_data['destinationHostId']]['compatibilityNo'];
			if($pair_data['rpoSLAThreshold'] < $one_to_many_array[$pair_key]['rpoSLAThreshold'])
			$one_to_many_array[$pair_key]['rpoSLAThreshold'] =(int)$pair_data['rpoSLAThreshold'];
		}
		else
		{
			$one_to_many_array[$pair_key]['compatibilityNo'] = $host_array[$pair_data['destinationHostId']]['compatibilityNo'];
			$one_to_many_array[$pair_key]['rpoSLAThreshold'] = (int)$pair_data['rpoSLAThreshold'];
		}
	}
	
	/*
	* Setting default license expiry value (0) for each host 
	*/
	$host_id_arr = explode("','",$host_id);
	foreach($host_id_arr as $host_id_key => $host_id_val)
	{
		$license_array[$host_id_val]['expired'] = 0;
	}
	
	/*
	* This if condition will execute if pairs has Phy_lunid
	* $sd_array:: This array will give all shared device capacity inf
	* $hatlm_array:: This array contains source device name , applianceTargetLunMappingId of all the shared devices
	*/
	if($phy_lun_id)
	{
		$sd_sql = "select capacity, sharedDeviceId from sharedDevices FORCE INDEX(PRIMARY) WHERE sharedDeviceId IN ('$phy_lun_id')";
		$sd_rs =  $conn_obj->sql_query($sd_sql);
		if( !$sd_rs ) 
		{
			trigger_error( $conn_obj->sql_error() );
		}
		while($sd_data = $conn_obj->sql_fetch_object($sd_rs))
		{
			$sd_array[$sd_data->sharedDeviceId] = $sd_data->capacity;
		}	
		$hatlm_sql = "select distinct srcDeviceName, sharedDeviceId, applianceTargetLunMappingId from hostApplianceTargetLunMapping WHERE sharedDeviceId IN ('$phy_lun_id')";
		$hatlm_rs =  $conn_obj->sql_query($hatlm_sql);
		if( !$hatlm_rs ) 
		{
			trigger_error( $conn_obj->sql_error() );
		}
		while($hatlm_data = $conn_obj->sql_fetch_object($hatlm_rs))
		{
			$hatlm_array[$hatlm_data->sharedDeviceId]['srcDeviceName'] = $hatlm_data->srcDeviceName;
			$hatlm_array[$hatlm_data->sharedDeviceId]['applianceTargetLunMappingId'] = $hatlm_data->applianceTargetLunMappingId;
		}		
	}
    $sql = "SELECT nicDeviceName,processServerId FROM networkAddressMap  WHERE vxAgentId = '$srcIdsIn' AND processServerId in ('$ps_id')";
    $result = $conn_obj->sql_query($sql);
	if( !$result ) 
	{
		trigger_error( $conn_obj->sql_error() );
	}
	while($row = $conn_obj->sql_fetch_object($result))
	{
		$nic_dev_array[] = $row->nicDeviceName;
		$psIdArray[] = $row->processServerId;
	}
	$psIdArray = array_unique($psIdArray);
	$psId = implode("','",$psIdArray);
	$nic_dev_array =  array_unique($nic_dev_array);
	$nic_dev_str =  implode("','",$nic_dev_array);	
	$sql = "SELECT ipAddress,hostId,deviceName FROM networkAddress WHERE  hostId IN ('$psId') AND deviceName IN ('$nic_dev_str')";
	$rs = $conn_obj->sql_query($sql);
	if( !$rs ) 
	{
		trigger_error( $conn_obj->sql_error() );
	}
	while($data = $conn_obj->sql_fetch_object($rs))
	{
		/*
		*$nic_array :: This array will give nic ipAddress if we do mapping from agent to process server nic's
		*/
		$nic_array[$data->hostId]['ipAddress'] = $data->ipAddress;
	}	
	/*
	*$retention_array: This will give arrays of array as result 
	*which intern contains all the retention realted table info for the current calling host id pairs
	*/
	$retention_array = get_retention_settings($ruleId_array);
	/*
	* pair_type_array contains all pairs type if they are part of PRISM/PILLAR  plan
	*/
	$pair_type_array = $vol_obj->get_pair_type_array();
	
	return array($pair_array,$lv_array, $host_array, $sd_array, $hatlm_array, $ps_array, $ret_array, $retention_array, $one_to_many_array, $license_array,$nic_array,$pair_type_array);
}


function get_retention_settings($rule_ids)
{
	global $conn_obj;
	$rule_id_array = array();
	$rule_id = array();
	$retentionEventPolicyArray = array();
	$retentionSpacePolicyArray = array();
	$cdp_settings_array = array();
	$retentionWindowArray = array();
	$consistencyTagArray = array();
	$retIdArray = array();
	$Id = array();
	$rule_id = array_unique($rule_ids);
	$ruleId = implode("','",$rule_id);
	$cdpquery = "SELECT
                                retState,
                                metafilepath,
                                diskspacethreshold,
                                retPolicyType,
                                uniqueId,
                                sparseEnable,
								ruleId,
								retId
                            FROM
                                retLogPolicy
							FORCE INDEX (ruleid)
							WHERE 
								ruleId IN ('$ruleId')";
	$cdprs=$conn_obj->sql_query($cdpquery);   
	if( !$cdprs ) 
	{
		trigger_error( $conn_obj->sql_error() );
	}	
	while($cdpsettingrow = $conn_obj->sql_fetch_object( $cdprs))
	{
		$cdp_settings_array[$cdpsettingrow->ruleId]['retState'] = $cdpsettingrow->retState;
		$cdp_settings_array[$cdpsettingrow->ruleId]['metafilepath'] = $cdpsettingrow->metafilepath;
		$cdp_settings_array[$cdpsettingrow->ruleId]['diskspacethreshold'] = $cdpsettingrow->diskspacethreshold;
		$cdp_settings_array[$cdpsettingrow->ruleId]['retPolicyType'] = $cdpsettingrow->retPolicyType;
		$cdp_settings_array[$cdpsettingrow->ruleId]['uniqueId'] = $cdpsettingrow->uniqueId;
		$cdp_settings_array[$cdpsettingrow->ruleId]['sparseEnable'] = $cdpsettingrow->sparseEnable;
		$cdp_settings_array[$cdpsettingrow->ruleId]['retId'] = $cdpsettingrow->retId;
		$retIdArray[] = $cdpsettingrow->retId;
	}
	$retIdArray = array_unique($retIdArray);
	$ret_id_array = implode("','",$retIdArray);
	$query ="SELECT
				Id,
				startTime,
				endTime,
				timeGranularity,
				retId
			FROM
                retentionWindow
			FORCE INDEX(retLogPolicy_retentionWindow) 				
            WHERE 
                retId in ('$ret_id_array')
			ORDER BY startTime,endTime";//need to add  order by startTime, endTime logic
	$result_set=$conn_obj->sql_query($query);
	if( !$result_set ) 
	{
		trigger_error( $conn_obj->sql_error() );
	}	
	$startTime = 0;
	$i =0;
	while( $row = $conn_obj->sql_fetch_object( $result_set ) ) 
	{
		$Id[] = $row->Id;
		$retentionWindowArray[$row->retId][$row->Id]['startTime'] = $row->startTime;
		$retentionWindowArray[$row->retId][$row->Id]['endTime'] = $row->endTime;
		$retentionWindowArray[$row->retId][$row->Id]['timeGranularity'] = $row->timeGranularity;			
		$retentionWindowArray[$row->retId][$row->Id]['Id'] = $row->Id;			
	}	
	$Id = array_unique($Id);
	$Id = implode("','",$Id);
	$select_app =   "SELECT 
                                            storagePath,
                                            storageDeviceName,
                                            storagePruningPolicy,
                                            consistencyTag,
                                            alertThreshold,
                                            moveRetentionPath,
                                            tagCount,
                                            userDefinedTag,
                                            catalogPath,
											Id,
											retentionEventPolicyId
                                        FROM
                                            retentionEventPolicy
                                        FORCE INDEX (retentionWindow_retentionEventPolicy)    
                                        WHERE
											Id in ('$Id')";
	$result_app=$conn_obj->sql_query($select_app);
	if( !$result_app ) 
	{
		trigger_error( $conn_obj->sql_error() );
	}	
    while($row_app = $conn_obj->sql_fetch_object( $result_app))
	{
		$retentionEventPolicyArray[$row_app->Id]['storagePath'] = $row_app->storagePath;
		$retentionEventPolicyArray[$row_app->Id]['storageDeviceName'] = $row_app->storageDeviceName;
		$retentionEventPolicyArray[$row_app->Id]['storagePruningPolicy'] = $row_app->storagePruningPolicy;
		$retentionEventPolicyArray[$row_app->Id]['consistencyTag'] = $row_app->consistencyTag;
		$retentionEventPolicyArray[$row_app->Id]['alertThreshold'] = $row_app->alertThreshold;
		$retentionEventPolicyArray[$row_app->Id]['moveRetentionPath'] = $row_app->moveRetentionPath;
		$retentionEventPolicyArray[$row_app->Id]['tagCount'] = $row_app->tagCount;
		$retentionEventPolicyArray[$row_app->Id]['userDefinedTag'] = $row_app->userDefinedTag;
		$retentionEventPolicyArray[$row_app->Id]['catalogPath'] = $row_app->catalogPath;
		$retentionEventPolicyArray[$row_app->Id]['retentionEventPolicyId'] = $row_app->retentionEventPolicyId;
		$consistencyTagArray[] = $row_app->consistencyTag;
	}
	$consistencyTagArray = array_unique($consistencyTagArray);
	$consistencyTag = implode("','",$consistencyTagArray);
	
	$tag_type = get_tag_type($consistencyTag);
	$select_space =   "SELECT 
                                            storagePath,
                                            storageDeviceName,
                                            storagePruningPolicy,
                                            storageSpace,
                                            alertThreshold,
                                            moveRetentionPath,
                                            catalogPath,
											retId
                                        FROM
                                            retentionSpacePolicy
										FORCE INDEX (retLogPolicy_retentionSpacePolicy)
                                        WHERE
                                           retId IN ('$ret_id_array')";
	$result_space=$conn_obj->sql_query($select_space);
	if( !$result_space ) 
	{
		trigger_error( $conn_obj->sql_error() );
	}	
    while($row_space = $conn_obj->sql_fetch_object( $result_space))
	{
		$retentionSpacePolicyArray[$row_space->retId]['storagePath'] = $row_space->storagePath;
		$retentionSpacePolicyArray[$row_space->retId]['storageDeviceName'] = $row_space->storageDeviceName;
		$retentionSpacePolicyArray[$row_space->retId]['storagePruningPolicy'] = $row_space->storagePruningPolicy;
		$retentionSpacePolicyArray[$row_space->retId]['storageSpace'] = $row_space->storageSpace;
		$retentionSpacePolicyArray[$row_space->retId]['alertThreshold'] = $row_space->alertThreshold;
		$retentionSpacePolicyArray[$row_space->retId]['moveRetentionPath'] = $row_space->moveRetentionPath;
		$retentionSpacePolicyArray[$row_space->retId]['catalogPath'] = $row_space->catalogPath;
	}
	return(array($cdp_settings_array,$retentionWindowArray,$retentionEventPolicyArray,$retentionSpacePolicyArray,$tag_type));
}


function get_san_volume_info_settings($phy_lun_id, $hatlm_array, $srcDeviceName, $applianceTargetLunMappingId) 
{
    $sanVolumeInfo = array();
    $isSanVolume = FALSE;
    $physicalDeviceName = "";
    $mirrorDeviceName = "";
    $applianceDeviceName = "";
    $applianceLunName = "";
	$physicalLunId = "";

	if($phy_lun_id)
	{
		if($hatlm_array[$phy_lun_id])
		{
			$sanVolumeInfo = array(0, TRUE, $srcDeviceName, $srcDeviceName, $applianceDeviceName, $applianceTargetLunMappingId,$phy_lun_id);
             return $sanVolumeInfo;
		}
		else
		{
			$sanVolumeInfo = array(0, $isSanVolume, $physicalDeviceName, $mirrorDeviceName, $applianceDeviceName, $applianceLunName,$physicalLunId);
             return $sanVolumeInfo;
		}
	}
	else
	{
		$sanVolumeInfo = array(0, FALSE, "", "", "", "","");
        return $sanVolumeInfo;
	}    
}

function get_rep_cleanup_info($tgt_fs,$tgt_mntpnt,$replication_cleanup_options,$tgt = 0)
{
	global $conn_obj;
	global $LOG_CLEANUP_PENDING,$UNLOCK_PENDING,$CACHE_CLEANUP_PENDING;
	global $PENDING_FILES_CLEANUP_PENDING,$VNSAP_CLEANUP_PENDING;
	global $logging_obj;
		
	$log_cleanup = "no";
	$log_cleanup_status = 0;
	
	$unlock_volume = "no";
	$fileSystem = 0;
	$mntpoint = 0;
	$unlock_vol_status = 0;
	
	$cache_dir = "no";
	$cache_dir_del_status = 0;
	
	$pending_action = "no";
	$pending_action_cleanup_status = 0;
	
	$vsnap_cleanup_action = "no";
	$vsnap_cleanup_status = 0;
	
	$result = "log_cleanup=$log_cleanup;log_cleanup_status=$log_cleanup_status;log_cleanup_message=0;";
	$result .="unlock_volume=$unlock_volume;unlock_vol_mount_pt=$mntpoint;unlock_vol_fs=$fileSystem;unlock_vol_status=$unlock_vol_status;unlock_vol_message=0;";
	$result .="cache_dir_del=no;cache_dir_del_status=$cache_dir_del_status;cache_dir_del_message=0;";
	$result .="pending_action_folder_cleanup=no;pending_action_folder_cleanup_status=$pending_action_cleanup_status;pending_action_folder_cleanup_message=0;";
	$result .="vsnap_cleanup=no;vsnap_cleanup_status=$vsnap_cleanup_status;vsnap_cleanup_message=0";
	
	if($replication_cleanup_options != "0" && $tgt == 1)
	{
		$replication_options = $replication_cleanup_options;
		$cache_dir_del = intval($replication_options[0]);
		$pending_action_cleanup = intval($replication_options[2]);
		$vsnap_cleanup = intval($replication_options[4]);
		$clear_logs = intval($replication_options[6]);
		$unlock_drive = intval($replication_options[8]);
		if($clear_logs > 0)
		{
			$log_cleanup = "yes";
			$log_cleanup_status = $LOG_CLEANUP_PENDING;
		}
		if($unlock_drive > 0)
		{
			$fileSystem = $tgt_fs;
			if($tgt_fs == "")
			{
				$fileSystem = 0;
			}
			$mntpoint = $tgt_mntpnt;
			if($tgt_mntpnt == "")
			{
				$mntpoint = 0;
			}
			$unlock_volume = "yes";
			$unlock_vol_status = $UNLOCK_PENDING;
		}
		if($cache_dir_del > 0)
		{
			$cache_dir = "yes";
			$cache_dir_del_status = $CACHE_CLEANUP_PENDING;
		}
		if($pending_action_cleanup > 0)
		{
			$pending_action = "yes";
			$pending_action_cleanup_status = $PENDING_FILES_CLEANUP_PENDING;
		}
		if($vsnap_cleanup > 0)
		{
			$vsnap_cleanup_action = "yes";
			$vsnap_cleanup_status = $VNSAP_CLEANUP_PENDING;
		}
			
		$result = "log_cleanup=$log_cleanup;log_cleanup_status=$log_cleanup_status;log_cleanup_message=0;";
		$result .="unlock_volume=$unlock_volume;unlock_vol_mount_pt=$mntpoint;unlock_vol_fs=$fileSystem;unlock_vol_status=$unlock_vol_status;unlock_vol_message=0;";
		$result .="cache_dir_del=$cache_dir;cache_dir_del_status=$cache_dir_del_status;cache_dir_del_message=0;";
		$result .="pending_action_folder_cleanup=$pending_action;pending_action_folder_cleanup_status=$pending_action_cleanup_status;pending_action_folder_cleanup_message=0;";
		$result .="vsnap_cleanup=$vsnap_cleanup_action;vsnap_cleanup_status=$vsnap_cleanup_status;vsnap_cleanup_message=0";
	}
	return $result;
}

function get_retention_log_details($retention_array, $ruleId)
{
	list($ret_log_policy, $retention_window, $ret_event_policy, $ret_space_policy) = $retention_array;

	$retId = $ret_log_policy[$ruleId]['retId'];
	$policy_type = $ret_log_policy[$ruleId]['retPolicyType'];
	$sparse_enable = $ret_log_policy[$ruleId]['sparseEnable'];
	$unique_id = $ret_log_policy[$ruleId]['uniqueId'];
	foreach($retention_window as $retentionId => $retentionWindowData)
	{
		if($retId == $retentionId)
		{
			foreach($retentionWindowData as $window_id => $window_data)
			{
				$ret_window_id = $window_id;
				break;
			}
		}
	}	
	
	if($policy_type == 0 || $policy_type == 2)
	{
		$on_insufficient_disk_space = $ret_space_policy[$retId]['storagePruningPolicy'];
		$alert_threshold = $ret_space_policy[$retId]['alertThreshold'];
		$storage_path = $ret_space_policy[$retId]['storagePath'];
		$storage_device_name = $ret_space_policy[$retId]['storageDeviceName'];
		$storage_space = $ret_space_policy[$retId]['storageSpace'];
		$moveRetentionPath = $ret_space_policy[$retId]['moveRetentionPath'];
		$catalogPath = $ret_space_policy[$retId]['catalogPath'];
	}
	else
	{
		$on_insufficient_disk_space = $ret_event_policy[$ret_window_id]['storagePruningPolicy'];
		$alert_threshold = $ret_event_policy[$ret_window_id]['alertThreshold'];
		$storage_path = $ret_event_policy[$ret_window_id]['storagePath'];
		$storage_device_name = $ret_event_policy[$ret_window_id]['storageDeviceName'];
		$storage_space = $ret_event_policy[$ret_window_id]['storageSpace'];
		$moveRetentionPath = $ret_event_policy[$ret_window_id]['moveRetentionPath'];
		$catalogPath = $ret_event_policy[$ret_window_id]['catalogPath'];
	}
	return array("on_insufficient_disk_space" => $on_insufficient_disk_space,
					"alert_threshold" => $alert_threshold,		
                    "storage_path" => $storage_path,
                    "storage_device_name" => $storage_device_name,
                    "storage_space" => $storage_space,
                    "moveRetentionPath" =>$moveRetentionPath,
					"catalogPath" =>$catalogPath,
					"sparse_enable" => $sparse_enable,
					"unique_id" => $unique_id);
}

/*
 * Function Name: update_agent_log
 *
 * Description:
 * Logs a error string to a Sentinel log and creates a symbolic link
 *  
 *
 * Parameters: 
 * SRC host Id, Src Volume, Error String sent by agent
 *	
 * Return Value:
 *     nothing
 *
 */

function update_agent_log ($hostId, $volume, $errorString, $agent_timestamp=0, $error_source='', $ipAddress='')
{
   global $REPLICATION_ROOT;
   global $logging_obj, $commonfunctions_obj;
   global $LN;
   
   $logging_obj->my_error_handler("DEBUG","update_agent_log \n HostId = $hostId \n\n Volume = $volume \n\n Error string = $errorString \n\n Agent timestamp = $agent_timestamp \n\n Error source = $error_source \n\n IP address = $ipAddress \n");
   # Fix for 4455
   /*
	* Getting the resync request time stamp from the error message.
    */ 
    if($agent_timestamp!=0)
	{
		$errorList = explode("&",$errorString);
    
		/*
		* Getting the resync request time stamp from the error message.
		*/
		if (is_array($errorList))
		{
			$requestTimeStamp = explode("=",$errorList[2]);
			if(is_array($requestTimeStamp))
			{
				$resyncRequestTimeStamp = $requestTimeStamp[1];
			}
			$errorMsg = explode("=",$errorList[3],2);
			if (is_array($errorMsg))
			{
				$errStr = $errorMsg[1];
			}
		}
	}

    $current_day_time = getdate();
    $current_weekday = $current_day_time['weekday'];
    $current_month = $current_day_time['month'];
    $current_day = $current_day_time['mday'];
    $current_time = $current_day_time['hours'].":".$current_day_time['minutes'].":".$current_day_time['seconds'];
    $current_year = $current_day_time['year'];

    $TimeString = $current_weekday.' '.$current_month.' '.$current_day.' '.$current_time.' '.$current_year;

	$errStr = preg_replace("/[\n\r]/"," ",$errStr);

    if($agent_timestamp != 0 and $ipAddress != '') // this condition will satisfy only for Prism pair.
	{
		$commonfunctions_obj->writeLog($_SESSION['username'],$errStr);	
		$errorString = "$TimeString Resync detected at"." $resyncRequestTimeStamp (TimeStamp sent by S2):"." Error message sent by node $ipAddress S2: $errStr";
	}
	elseif($agent_timestamp != 0)
	{
		$commonfunctions_obj->writeLog($_SESSION['username'],$errStr);	
		$errorString = "$TimeString Resync detected at"." $resyncRequestTimeStamp (TimeStamp sent by S2):"." Error message sent by $hostId S2: $errStr";
	}
	elseif ($error_source == "pillar_mirror_status")
	{
		$errorString ="$TimeString $errorString";
	}
	elseif ($error_source == "vol_resize")
	{
		$errorString ="$TimeString $errorString";
	}
	/*move retention log agnet logs*/
    elseif($error_source == "move_retention_log")
    {
        $errorString ="$TimeString $errorString";
    }
    /*inSufficient Disk Space*/
    elseif($error_source == "insufficient_retention_space")
    {
        $errorString ="$TimeString $errorString";
    }
	elseif ($error_source == "reassign_primary")
	{
		$errorString ="$TimeString "."Error sent by $hostId for 1 to N pair with srcVolume $volume."."$errorString";
	}
	elseif ($error_source == "target_agent") // 9448 : Writing the error message when target agent set Resync Required as YES.
	{				
		#12179 Fix
		$errorString = str_replace("\n"," ",$errorString);		
		$errorString ="$TimeString $errorString";		
	}
	else
	{
		$errorString = "$TimeString "."Error sent by $hostId DP while $volume pair deletion "."$errorString";
	}
	
	$logging_obj->my_error_handler("DEBUG","update_agent_log \n errorString = $errorString \n");
	
	$volume_test = $volume;
    $volume = str_replace("\\", "/", $volume);
    $volume = str_replace(":", "", $volume);

	$filename = $REPLICATION_ROOT.'/'.$hostId.'/'.$volume.'/sentinel.txt';
	
    $logging_obj->my_error_handler("DEBUG","update_agent_log \n filename = $filename \n");
	
    $fr = $commonfunctions_obj->file_open_handle($filename, 'a+');
    if(!$fr) 
	{
		return;
       /*
	    * echo "Error! Couldn't open the file ($filename).";
		*/
    }
    if (!fwrite($fr, $errorString . "\n"))
	{
       /*
		* print "Cannot write to file ($filename)";
		*/
    }
    if(!fclose($fr))
	{
       /*
		* echo "Error! Couldn't close the file ($filename).";
		*/
    }
}


?>
