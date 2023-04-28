<?
    if (!$functions_included) 
    {
        include_once 'config.php';
    }
    
	include_once("om_retentionFunctions.php");

  	$conn_included = 1;	
	
/*	
# Execution states
	$QUEUED = 0;
      $READY  = 1;
      $STARTED = 2;
   	$INPROGRESS = 3;
      $COMPLETED = 4;
      $FAILED    =5;
      $S_R_COMP_STARED =6;
      $S_R_COMP_PROGRESS =7;
       
# Snap Type
      $SCHED_SNAP_PHYSICAL = 0;
      $RECOVERY_PHYSICAL   = 1;
      $ROLLBACK            = 2;
      $SCHED_SNAP_VIRTUAL  = 4;
      $RECOVERY_VIRTUAL    = 5;
      $EVENT_BASED         = 6;

 VOLUME TYPE :::
	0-Physical
	2-virtual
	1-virtual mounted


*/
$conn_included = 1;

/*
    * Function Name: update_retentiontimestamp
    *
    * Description:
    * This function is used to update the recovery ranges(retentiontimestamp) from updateCdpInformationV2 function available in om_vxstub.php
    *
    * Parameters:
    *   Param 1 [IN]: logsFrom
    *	Param 2 [IN]: logsFromutc
    *	Param 3 [IN]: logsTo
    *	Param 4 [IN]: logsToutc
    *	Param 5 [IN]: hostid
	*	Param 6 [IN]: deviceName
	*	Param 7 [IN]: cdp_timeranges
	*	Param 8 [IN]: pairid
	*	Param 9 [IN]: conn_obj
    *
    * Return Value:
    *     Returns boolean
    *
    * Exceptions:
    *     NA
    */
function update_retentiontimestamp($logsFrom,$logsFromutc,$logsTo,$logsToutc,$hostid,$deviceName,$cdp_timeranges,$pairid,$conn_obj)
{
    global $conn_obj;
	global $logging_obj;
	global $multi_query;
	global $multi_query_delete;
	$logging_obj->my_error_handler("DEBUG",date('m-d-Y h:i:s')." - update_retentiontimestamp Entered");	
    /*
         cdp_timeranges sample structure
         {
                entryno          // unused by CS
                starttimeUTC //recovery range start time in UTC
                startseq       // unused by CS
                endtimeUTC //recovery range end time in UTC
                endseq       //unused by CS
                mode         // accuracy mode
                granularity// Any Point In Time(APIT) or only end points
                action// action to insert( 0)/delete (1)
         }
         */
//    $logging_obj->my_error_handler("DEBUG","update_retentiontimestamp: \n");
//    $logging_obj->my_error_handler("DEBUG","update_retentiontimestamp:cdp_timeranges".print_r($cdp_timeranges,TRUE)."\n");
/*	$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp logsFrom=$logsFrom ,logsFromutc=$logsFromutc , logsTo=$logsTo , logsToutc=$logsToutc , hostid=$hostid, deviceName=$deviceName , pairid::$pairid\n");
*/
    
    $commonfunctions_obj = new CommonFunctions();
    $sparseretention_obj = new SparseRetention();
	$update_ts = true;
	$hostid_condition = "";
    $hostid_condition .= " AND hostId = '$hostid' ";
    /*get the sparse enabled*/
    $sparse_enabled = $sparseretention_obj->get_sparse_enable_new($pairid);
    #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp sparse_enabled::$sparse_enabled\n");	
    
	$query ="SELECT 
				id,
				accuracy,
				EndTimeUTC
			FROM
				retentionTimeStamp
			WHERE
				pairId = $pairid $hostid_condition
			ORDER BY StartTimeUTC DESC LIMIT 1";
	$rsQuery = $conn_obj->sql_query($query);
    $recovery_count = $conn_obj->sql_num_row($rsQuery);
	
	if($recovery_count)
	{
		$db_latest_recovery_range_info = $conn_obj->sql_fetch_object($rsQuery);
		$db_accuracy = $db_latest_recovery_range_info->accuracy;
		$db_end_timestamp = $db_latest_recovery_range_info->EndTimeUTC;
		$db_recovery_range_id = $db_latest_recovery_range_info->id;
	}
	
	if(!$sparse_enabled)
	{
		if(sizeof($cdp_timeranges) > 1)
		{
			foreach ($cdp_timeranges as $key => $row) 
			{
				$v2[$key] = $row[2];
			}
			array_multisort($v2, SORT_ASC, $cdp_timeranges);
		}		
	}
	
	for($j=0;$j<count($cdp_timeranges);$j++)
    {
        /*unused by CS for now*/
        $entryno=$cdp_timeranges[$j][1];
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp entryno::$entryno \n");
        $starttimeUTC = $cdp_timeranges[$j][2];
		#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp starttimeUTC::$starttimeUTC \n");

		$start_time_utc_str = (string)$starttimeUTC;
		//If the string contains all digits, then only allow, otherwise return.
		if (isset($starttimeUTC) && $starttimeUTC != '' && (!preg_match('/^\d+$/',$start_time_utc_str))) 
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for starttimeUTC $starttimeUTC in update_retentiontimestamp."); 
		}

		/*convert unix time to readable string format*/
        $starttime= $commonfunctions_obj->getTimeFormat($starttimeUTC);
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp starttime::$starttime \n");
		
        /*unused by CS for now*/
        $startseq = $cdp_timeranges[$j][3];
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp startseq::$startseq \n");
		
        $endtimeUTC = $cdp_timeranges[$j][4];
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp endtimeUTC::$endtimeUTC \n");

		$end_time_utc_str = (string)$endtimeUTC;
		//If the string contains all digits, then only allow, otherwise return.
		if (isset($endtimeUTC) && $endtimeUTC != '' && (!preg_match('/^\d+$/',$end_time_utc_str))) 
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for endtimeUTC $endtimeUTC in update_retentiontimestamp.");
		}
        /*convert unix time to readable string format*/
        $endtime = $commonfunctions_obj->getTimeFormat($endtimeUTC);
		
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp endtime::$endtime \n");
        /*unused by CS for now*/
        $endseq = $cdp_timeranges[$j][5];
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp endseq::$endseq \n");
		
        /*accuracy point*/
        $mode = $cdp_timeranges[$j][6];
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp mode::$mode \n");
		$mode_str = (string)$mode;
		//If the string contains all digits, then only allow, otherwise return.
		if (isset($mode) && $mode != '' && (!preg_match('/^\d+$/',$mode_str)))
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for mode $mode in update_retentiontimestamp.");
		}
		
        /*APIT or only end points*/
        $granularity = $cdp_timeranges[$j][7];
        $action = $cdp_timeranges[$j][8];
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp granularity::$granularity \n");
        /*start transaction*/
        #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp:transaction begins \n");
       
        /*non - sparse block*/
		/*update datamode.txt for health.rrd*/
		#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp update rrd file:: $ruleid, $endtime, $mode\n");
		#updateDataMode($pairid, $endtime, $mode);
        if(!$sparse_enabled)
		{
			/* 
			*If agent sent record end time is greater than the database latest record end time, 
			*then do the below. This is the purpose of filtering the ranges which are already proces
			*sed by CX, but in any chance if agent send the similar range.
			*/
			#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp non sparse_enabled block\n");
			if($endtimeUTC >= $db_end_timestamp)
			{		   
				#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp query db_recovery_range_id::".$db_recovery_range_id."\n");
				/*if accuracy is same then update the end time*/
				
				#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp select query recovery_count::$recovery_count\n");
				
				/* 
				*if no recovery range records available in database till now, that means this is the
				*first recovery range record receieved from agent end and insert the first record in db.
				*/	
				if($recovery_count == 0)
				{
					$sql =  "INSERT
									INTO
									retentionTimeStamp
									(hostId,
									deviceName,
									StartTime,
									EndTime,
									StartTimeUTC,
									EndTimeUTC,
									accuracy,	
									pairId)
							VALUES ('$hostid',
									'$deviceName',
									'$starttime',
									'$endtime',
									$starttimeUTC,
									$endtimeUTC,
									$mode,
									$pairid)";
				}
				else
				{
					/* 
					*If database latest record accuracy matches with agent sent record, just appen
					*ding the endtime stamp of db record with agent sent end time stamp. That means
 					*the endtime is moving forward even though there is some time interval gap betwe
					*een the database record end time and agent sent record start time.
					*/
					if($db_accuracy == $mode && $update_ts)
					{
						$sql =  "UPDATE
										retentionTimeStamp
								SET 
									EndTime = '$endtime' ,
									EndTimeUTC = $endtimeUTC  
								WHERE 
									accuracy = $mode 
								AND
									pairId = $pairid
								AND 
									id = $db_recovery_range_id $hostid_condition";
						
					}
					/* Accuracy mis - match insert new record*/
					else
					{
						$sql =  "INSERT
									INTO
										retentionTimeStamp
										(hostId,
										deviceName,
										StartTime,
										EndTime,
										StartTimeUTC,
										EndTimeUTC,
										accuracy,
										pairId)
									VALUES 
										('$hostid',
										'$deviceName',
										'$starttime',
										'$endtime',
										$starttimeUTC,
										$endtimeUTC,
										$mode,
										$pairid)
									ON DUPLICATE KEY 
									UPDATE
										StartTime = '$starttime',
										EndTime = '$endtime',
										StartTimeUTC = $starttimeUTC,
										EndTimeUTC = $endtimeUTC,
										accuracy = $mode";
						$update_ts = false;
					}
				}
				#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp sql::$sql \n");
				$multi_query[] = $sql;
			}
		}
        /*sparse block*/
        else
        {
            #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp sparse_enabled block\n");
            /*
			* Delete action received for coalsed ranges. We need to delete
			* only the ranges which are between the agent sent range.
			* Perform a delete operation first. As the agent sends the 
			* recovery ranges in any order, if we do the records in the
			* agent sent order, then there is a chance that the inserted
			* range can delete again even though it is valid range. So let
			* us perform the delete operation first for the given ranges.
			* If an action is delete, then that means the deletion part of
			* all writes, which converted to coal esc points (or) some of
			* the coal-esc points deletion is happening and example like
			* when hourly points converted to day point. Collecting all 
			* delete queries into the structure finally to give it for batch
			* update. Even on http time outs, if we perform the same 
			* deletion, it couldn't be any issue, because agent posts the 
			* same input to CS.
			*/
            if($action == 1)
            {
                $sql =  "DELETE
                        FROM
                            retentionTimeStamp
                        WHERE 
                            StartTimeUTC >= $starttimeUTC
                        AND
                            EndTimeUTC <= $endtimeUTC
                        AND
                            pairId = $pairid $hostid_condition";
                #$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp delete the coalesced range ::$sql \n");
				$multi_query_delete[] = $sql;				
            }
            /*Insert action received for time ranges*/
            if($action == 0)
            {
				/* If agent records end time is greater than the latest db range end time and means the sent range is greater than database range, so we can process this
				10:00 - 10:30 - DB range
					10:40 - 11:00 - Agent range
					10:25 - 11:00 - Agent range
				*/
				/*Code block for all writes*/
				if(!$granularity)
				{
					/*
					* The first if condition is to filter the ranges ,
					* which are already processed(i.e., timeranges already
					* updated in the db). The second if condition is to
					* process the range which is near to append with that
					* latest db range or to do simple insert. The
					* remaining ranges will be inserted simply. Always the
					* agent sent ranges will be within a hour boundary for
					* sparse retention only.
					*/
					if($endtimeUTC >= $db_end_timestamp)
					{
						/*
						* If Accuracy matches and range is within the same * hour boundary of the latest db range, then 
						* update the endtimeUTC, else insert new record.
						*/
						if($starttimeUTC <= $db_end_timestamp && $db_accuracy == $mode)
						{
						
							$sql =  "UPDATE
									retentionTimeStamp
								SET 
									EndTime = '$endtime' ,
									EndTimeUTC = $endtimeUTC  
								WHERE 
									accuracy = $mode 
								AND
									pairId = $pairid
								AND 
									id = $db_recovery_range_id $hostid_condition";
							#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp Accuracy Matched and start_time less than db_end_timestamp ::\n");
							$multi_query[] = $sql;
						}
						else
						{
							/*
							*Insert new record as this is new hourly
							*boundary range (or) for accuracy changed
							*within same hourly boundary range.
							*/
							$sql =  "INSERT
										INTO
											retentionTimeStamp
											(hostId,
											deviceName,
											StartTime,
											EndTime,
											StartTimeUTC,
											EndTimeUTC,
											accuracy,
											pairId)
										VALUES 
											('$hostid',
											'$deviceName',
											'$starttime',
											'$endtime',
											$starttimeUTC,
											$endtimeUTC,
											$mode,
											$pairid)
										ON DUPLICATE KEY 
										UPDATE
											StartTime = '$starttime',
											EndTime = '$endtime',
											StartTimeUTC = $starttimeUTC,
											EndTimeUTC = $endtimeUTC,
											accuracy = $mode";

							#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp inserting new hourly time range ::$sql\n");
							$multi_query[] = $sql;
						}
					}
				}
				else
				{
					/*
					* Processing the coal-esc ranges.
					* Perform the coal-esc ranges(these are the 
					* hourly or daily or weekly or monthly points) 
					* operation. These ranges can be inserted directly. On
					* http time outs and if the same range inserts and if
					* agent sends the same range again to avoid the 
					* duplicates perform the delete operation first on
					* this range and then proceed for insertion with the
					* same range. This like insert if-not-exists (or)
					* delete+insert.
					*/
					#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp coal-esc ranges block\n");
					/*
					$delete_sql =  "DELETE
					FROM
						retentionTimeStamp
					WHERE 
						StartTimeUTC >= $starttimeUTC
					AND
						EndTimeUTC <= $endtimeUTC
					AND
						pairId = $pairid";
					
					$multi_query_delete[] = $delete_sql;

					$sql =  "INSERT
								INTO
									retentionTimeStamp
									(hostId,
									deviceName,
									StartTime,
									EndTime,
									StartTimeUTC,
									EndTimeUTC,
									accuracy,
									pairId)
								VALUES 
									('$hostid',
									'$deviceName',
									'$starttime',
									'$endtime',
									$starttimeUTC,
									$endtimeUTC,
									$mode,
									$pairid) 
								";
					#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp insert the coalesced range ::$sql \n");
					$multi_query[] = $sql;
				*/
				
				$sql =  "INSERT
								INTO
									retentionTimeStamp
									(hostId,
									deviceName,
									StartTime,
									EndTime,
									StartTimeUTC,
									EndTimeUTC,
									accuracy,
									pairId)
								VALUES 
									('$hostid',
									'$deviceName',
									'$starttime',
									'$endtime',
									$starttimeUTC,
									$endtimeUTC,
									$mode,
									$pairid) 
								ON DUPLICATE KEY 
								UPDATE
									StartTime = '$starttime',
									EndTime = '$endtime',
									StartTimeUTC = $starttimeUTC,
									EndTimeUTC = $endtimeUTC,
									accuracy = $mode";
					#$logging_obj->my_error_handler("DEBUG","update_retentiontimestamp insert the coalesced range ::$sql \n");
					$multi_query[] = $sql;
				}
			}
        } // Sparse block close
		unset($db_latest_recovery_range_info);
    }
    if(!$sparse_enabled)
	{
		/*update the retentionTimeStamp table to bring the entries in the proper range*/
		
		$update_retentionTimeStamp =    "UPDATE
											retentionTimeStamp 
											SET
												StartTime = '$logsFrom',
												StartTimeUTC = $logsFromutc 
											WHERE
												StartTimeUTC < $logsFromutc 
											AND
												EndTimeUTC > $logsFromutc  
											AND
												pairId = $pairid $hostid_condition";
		#$logging_obj->my_error_handler("DEBUG","prunning  update_retentiontimestamp update_retentionTimeStamp::$update_retentionTimeStamp \n");
		$multi_query[] = $update_retentionTimeStamp;
	}
	
	/*This is for pruning purpose*/
    $delete_sql =   "DELETE
						FROM
							retentionTimeStamp
						WHERE
							EndTimeUTC < $logsFromutc
						AND 
							pairId = $pairid $hostid_condition";
	$multi_query_delete[] = $delete_sql;
    #$logging_obj->my_error_handler("DEBUG"," prunning update_retentiontimestamp delete_sql::$delete_sql \n");
    return TRUE;
}

/*
    * Function Name: update_retentiontag
    *
    * Description:
    * This function is used to update the recovery tags from updateCdpInformationV2 function available in om_vxstub.php
    *
    * Parameters:
    *   Param 1 [IN]: logsFrom
    *	Param 2 [IN]: logsFromutc  
    *	Param 5 [IN]: hostid
	*	Param 6 [IN]: deviceName
	*	Param 7 [IN]: cdp_events
	*	Param 8 [IN]: pairid
	*	Param 9 [IN]: conn_obj
    *
    * Return Value:
    *     Returns boolean
    *
    * Exceptions:
    *     NA
    */
function update_retentiontag($logsFrom,$logsFromutc,$hostid, $deviceName, $cdp_events,$pairId,$conn_obj)
{
    global $conn_obj;
	global $logging_obj;
    global $commonfunctions_obj;
    global $multi_query;
	#$logging_obj->my_error_handler("INFO",date('m-d-Y h:i:s')." - update_retentiontag \n");
	#$multi_query = array();
    /*cdp_events Sample structure
            eventNo       // unused by CS
            eventType   //Application name
            eventTimeUTC //tag time in UTC
            eventSeq        //unused by CS
            eventValue     //user defined tag name
            eventMode   //tag accuracy
            eventIdentifier //tag guid;
            eventVerification //tag verification 
            eventComment  //comment;
            eventStatus     // if zero, it is an insert operation, if 1 – update operation
        */	
    #$logging_obj->my_error_handler("INFO","update_retentiontag cdp_events :: ".print_r($cdp_events,TRUE));

    $commonfunctions_obj = new CommonFunctions();
    $sparseretention_obj = new SparseRetention();
    
    if($pairId)
    {
		$hostid_condition = "";
        $hostid_condition .= " AND hostId = '$hostid'";
		for($j=0;$j<count($cdp_events);$j++)
		{
			/*unused by CS for now*/
			$eventNo =$cdp_events[$j][1];
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventNo::$eventNo\n");
			
			/*Application name*/
			$eventType = TagTypeToAppName($cdp_events[$j][2]);
			$eventType = $conn_obj->sql_escape_string ($eventType);
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventType::$eventType\n");
			
			/*tag timestamp in unix format*/
			$eventTimeUTC =$cdp_events[$j][3];
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventTimeUTC::$eventTimeUTC\n");
			$event_time_utc_str = (string)$eventTimeUTC;
			//If the string contains all digits, then only allow, otherwise return.
			if (isset($eventTimeUTC) && $eventTimeUTC != '' && (!preg_match('/^\d+$/',$event_time_utc_str))) 
			{
				$commonfunctions_obj->bad_request_response("Invalid post data for eventTimeUTC $eventTimeUTC in update_retentiontag."); 
			}
			
			$eventTime= $commonfunctions_obj->getTimeFormat($eventTimeUTC);
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventTime::$eventTime\n");
			
			/*unused by CS for now*/
			$eventSeq =$cdp_events[$j][4];
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventSeq::$eventSeq\n");
			
			/*user defined tag name*/
			$eventValue =$cdp_events[$j][5];
			$eventValue = $conn_obj->sql_escape_string ($eventValue);
			
			/*accuracy of the tag*/
			$eventMode =$cdp_events[$j][6];
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventMode::$eventMode\n");
			$event_mode_str = (string)$eventMode;
			//If the string contains all digits, then only allow, otherwise return.
			if (isset($eventMode) && $eventMode != '' && (!preg_match('/^\d+$/',$event_mode_str))) 
			{
				$commonfunctions_obj->bad_request_response("Invalid post data for eventMode $eventMode in update_retentiontag.");
			}
			
			/*tagGuid*/
			$eventIdentifier =$cdp_events[$j][7];
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventIdentifier::$eventIdentifier\n");
			if (isset($eventIdentifier) && $eventIdentifier != '')
			{
				$guid_format_status = $commonfunctions_obj->is_guid_format_valid($eventIdentifier);		
				if ($guid_format_status == false)
				{
					$commonfunctions_obj->bad_request_response("Invalid post data for eventIdentifier $eventIdentifier in update_retentiontag.");
				}
			}
						
			/*tag verification feature*/
			$eventVerification =$cdp_events[$j][8];
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventVerification::$eventVerification\n");
			$eventIdentifier = $conn_obj->sql_escape_string ($eventIdentifier);
			/*comment*/
			$eventComment =$cdp_events[$j][9];
			$eventComment = $conn_obj->sql_escape_string ($eventComment);
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventComment::$eventComment\n");
			/*if zero , it is insert operation,if 1 -update operation*/
			$eventStatus =$cdp_events[$j][10];
			#$logging_obj->my_error_handler("DEBUG","update_retentiontag eventStatus::$eventStatus\n");
			if($eventVerification == 1)
			{
				$update_flag = 'YES';
			}
			else
			{
				$update_flag = 'NO';
			}
            #$logging_obj->my_error_handler("DEBUG","update_retentiontag eventStatus::$eventStatus\n");
            if($eventStatus == 0)
            {
                $time    = explode(",",$eventTime);
                $year    = str_pad($time[0],4,"0",STR_PAD_LEFT);
                $month   = str_pad($time[1],2,"0",STR_PAD_LEFT);
                $day     = str_pad($time[2],2,"0",STR_PAD_LEFT);
                $hour    = str_pad($time[3],2,"0",STR_PAD_LEFT);
                $minute  = str_pad($time[4],2,"0",STR_PAD_LEFT);
                $second  = str_pad($time[5],2,"0",STR_PAD_LEFT);
                $msec    = str_pad($time[6],3,"0",STR_PAD_LEFT);
                $nsec    = str_pad($time[7],3,"0",STR_PAD_LEFT);
                $misec   = str_pad($time[8],3,"0",STR_PAD_LEFT);
                $padTime=$year.",".$month.",".$day.",".$hour.",".$minute.",".$second.",".$msec.",".$nsec.",".$misec;
                $tagQuery = "INSERT
								INTO
									retentionTag 
									(  hostId,
									deviceName,
									tagTimeStamp,
									appName,
									userTag,
									pairId,
									paddedTagTimeStamp,
									accuracy,
									tagGuid,
									tagVerfiication,
									comment,
									tagTimeStampUTC)
								VALUES 
									('$hostid',
									'$deviceName',
									'$eventTime',
									'$eventType',
									'$eventValue',
									$pairId,
									'$padTime',
									$eventMode,
									'$eventIdentifier',
									'$update_flag',
									'$eventComment',
									$eventTimeUTC) 
								ON DUPLICATE KEY 
								UPDATE 
									hostId='$hostid',
									deviceName='$deviceName',
									tagTimeStamp='$eventTime',
									userTag='$eventValue',
									paddedTagTimeStamp='$padTime',
									accuracy=$eventMode,
									tagVerfiication='$update_flag',
									comment='$eventComment',
									tagTimeStampUTC=$eventTimeUTC";
                #$logging_obj->my_error_handler("DEBUG","update_retentiontag tagAddQuery::$tagQuery\n");
				$multi_query[] = $tagQuery;
            }
			
			
				
            if($eventStatus == 1)
            {
                $condition = "";
                $condition .= " AND tagTimeStamp = '$eventTime'";
                if($eventType != "")
                {
                    $condition .= " AND appName = '$eventType'";
                }
                if($eventValue != "")
                {
                    $condition .= " AND userTag = '$eventValue'";
                }
                if($tagGuid != "")
                {
                    $condition .= " AND tagGuid = '$eventIdentifier'";
                }
                $tagQuery =   "UPDATE 
                                    retentionTag 
                                set 
                                    tagVerfiication = '$update_flag',
                                    comment = '$eventComment' 
                                WHERE
                                    pairId = $pairId $condition $hostid_condition";
									
				$multi_query[] = $tagQuery;
                #$logging_obj->my_error_handler("DEBUG","update_retentiontag tagUpdateQuery::$tagQuery\n");
            }
            if($eventStatus == 2)
            {
                $condition = "";
                $condition .= " AND tagTimeStamp = '$eventTime'";
                if($eventType != "")
                {
                    $condition .= " AND appName = '$eventType'";
                }
                if($eventValue != "")
                {
                    $condition .= " AND userTag = '$eventValue'";
                }
                if($tagGuid != "")
                {
                    $condition .= " AND tagGuid = '$eventIdentifier'";
                }
                $tagQuery = "DELETE 
								FROM
									retentionTag 
								WHERE
									pairId = $pairId $condition $hostid_condition 
							";
				$multi_query[] = $tagQuery;
				#$logging_obj->my_error_handler("INFO","update_retentiontag tagAddQuery::$tagQuery\n");
			}
		}
		$pruneTag_sql = "DELETE
							FROM
								retentionTag
							WHERE
								tagTimeStampUTC < $logsFromutc
							AND
								pairId = $pairId $hostid_condition";
		$multi_query[] = $pruneTag_sql;
		#$logging_obj->my_error_handler("INFO","update_retentiontag prunning::$pruneTag_sql\n");
	}
	return TRUE;
}
#function to convert tagtype to application name
function TagTypeToAppName($eventType)
{
    global $STREAM_REC_TYPE_ORACLE_TAG,$STREAM_REC_TYPE_SYSTEMFILES_TAG,$STREAM_REC_TYPE_REGISTRY_TAG;
    global $STREAM_REC_TYPE_SQL_TAG,$STREAM_REC_TYPE_EVENTLOG_TAG,$STREAM_REC_TYPE_WMI_DATA_TAG;
    global $STREAM_REC_TYPE_COM_REGDB_TAG,$STREAM_REC_TYPE_FS_TAG,$STREAM_REC_TYPE_USERDEFINED_EVENT;
    global $STREAM_REC_TYPE_EXCHANGE_TAG,$STREAM_REC_TYPE_IISMETABASE_TAG,$STREAM_REC_TYPE_CA_TAG;
    global $STREAM_REC_TYPE_AD_TAG,$STREAM_REC_TYPE_DHCP_TAG,$STREAM_REC_TYPE_BITS_TAG;
    global $STREAM_REC_TYPE_WINS_TAG,$STREAM_REC_TYPE_RSM_TAG,$STREAM_REC_TYPE_TSL_TAG;
    global $STREAM_REC_TYPE_FRS_TAG,$STREAM_REC_TYPE_BS_TAG,$STREAM_REC_TYPE_SS_TAG;
    global $STREAM_REC_TYPE_CLUSTER_TAG,$STREAM_REC_TYPE_SQL2005_TAG,$STREAM_REC_TYPE_EXCHANGEIS_TAG;
    global $STREAM_REC_TYPE_EXCHANGEREPL_TAG,$STREAM_REC_TYPE_SQL2008_TAG,$STREAM_REC_TYPE_SHAREPOINT_TAG;
    global $STREAM_REC_TYPE_OSEARCH_TAG,$STREAM_REC_TYPE_SPSEARCH_TAG,$STREAM_REC_TYPE_HYPERV_TAG;
    global $STREAM_REC_TYPE_ASR_TAG,$STREAM_REC_TYPE_SC_OPTIMIZATION_TAG,$STREAM_REC_TYPE_MSSEARCH_TAG;
    global $STREAM_REC_TYPE_REVOCATION_TAG,$STREAM_REC_TYPE_TAGGUID_TAG;
	global $STREAM_REC_TYPE_TASK_SCHEDULER_TAG,$STREAM_REC_TYPE_VSS_METADATA_STORE_TAG,$STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG,$STREAM_REC_TYPE_IIS_CONFIG_TAG, $STREAM_REC_TYPE_FSRM_TAG, $STREAM_REC_TYPE_ADAM_TAG, $STREAM_REC_TYPE_CLUSTER_DB_TAG, $STREAM_REC_TYPE_NPS_TAG, $STREAM_REC_TYPE_TSG_TAG, $STREAM_REC_TYPE_DFSR_TAG, $STREAM_REC_TYPE_VMWARESRV_TAG, $SYSTEM_LEVEL_TAG, $STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_TAG;
    $AppName = "";
    switch($eventType)
    {
        case $STREAM_REC_TYPE_ORACLE_TAG:
            $AppName = "ORACLE";
        break;
        case $STREAM_REC_TYPE_SYSTEMFILES_TAG:
            $AppName = "SystemFiles";
        break;
        case $STREAM_REC_TYPE_REGISTRY_TAG:
            $AppName = "Registry";
        break;
        case $STREAM_REC_TYPE_SQL_TAG:
            $AppName = "SQL";
        break;
        case $STREAM_REC_TYPE_EVENTLOG_TAG:
            $AppName = "EventLog";
        break;
        case $STREAM_REC_TYPE_WMI_DATA_TAG:
            $AppName = "WMI";
        break;
        case $STREAM_REC_TYPE_COM_REGDB_TAG:
            $AppName = "COM+REGDB";
        break;
        case $STREAM_REC_TYPE_FS_TAG:
            $AppName = "FS";
        break;
        case $STREAM_REC_TYPE_USERDEFINED_EVENT:
            $AppName = "USERDEFINED";
        break;
        case $STREAM_REC_TYPE_EXCHANGE_TAG:
            $AppName = "Exchange";
        break;
        case $STREAM_REC_TYPE_IISMETABASE_TAG:
            $AppName = "IISMETABASE";
        break;
        case $STREAM_REC_TYPE_CA_TAG:
            $AppName = "CA";
        break;
        case $STREAM_REC_TYPE_AD_TAG:
            $AppName = "AD";
        break;
        case $STREAM_REC_TYPE_DHCP_TAG:
            $AppName = "DHCP";
        break;
        case $STREAM_REC_TYPE_BITS_TAG:
            $AppName = "BITS";
        break;
        case $STREAM_REC_TYPE_WINS_TAG:
            $AppName = "WINS";
        break;
        case $STREAM_REC_TYPE_RSM_TAG:
            $AppName = "RSM";
        break;
        case $STREAM_REC_TYPE_TSL_TAG:
            $AppName = "TSL";
        break;
        case $STREAM_REC_TYPE_FRS_TAG:
            $AppName = "FRS";
        break;
        case $STREAM_REC_TYPE_BS_TAG:
            $AppName = "BootableState";
        break;
        case $STREAM_REC_TYPE_SS_TAG:
            $AppName = "ServiceState";
        break;
        case $STREAM_REC_TYPE_CLUSTER_TAG:
            $AppName = "ClusterService";
        break;
        case $STREAM_REC_TYPE_SQL2005_TAG:
            $AppName = "Sql2005";
        break;
        case $STREAM_REC_TYPE_EXCHANGEIS_TAG:
            $AppName = "ExchangeIS";
        break;
        case $STREAM_REC_TYPE_EXCHANGEREPL_TAG:
            $AppName = "ExchangeRepl";
        break;
        case $STREAM_REC_TYPE_SQL2008_TAG:
            $AppName = "Sql2008";
        break;
        case $STREAM_REC_TYPE_SHAREPOINT_TAG:
            $AppName = "SharePoint";
        break;
        case $STREAM_REC_TYPE_OSEARCH_TAG:
            $AppName = "OSearch";
        break;
        case $STREAM_REC_TYPE_SPSEARCH_TAG:
            $AppName = "SPSearch";
        break;
        case $STREAM_REC_TYPE_REVOCATION_TAG:
            $AppName = "RevocationTag";
        break;
        case $STREAM_REC_TYPE_HYPERV_TAG:
            $AppName = "HyperV";
        break;
        case $STREAM_REC_TYPE_ASR_TAG:
            $AppName = "ASR";
        break;
        case $STREAM_REC_TYPE_SC_OPTIMIZATION_TAG:
            $AppName = "ShadowCopyOptimization";
        break;
        case $STREAM_REC_TYPE_MSSEARCH_TAG:
            $AppName = "MSSearch";
        break;
		case $STREAM_REC_TYPE_TASK_SCHEDULER_TAG:
            $AppName = "TaskScheduler";
        break;
		case $STREAM_REC_TYPE_VSS_METADATA_STORE_TAG:
            $AppName = "VSSMetaDataStore";
        break;
		case $STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG:
            $AppName = "PerformanceCounter";
        break;
		case $STREAM_REC_TYPE_IIS_CONFIG_TAG:
            $AppName = "IISConfig";
        break;
		case $STREAM_REC_TYPE_FSRM_TAG:
            $AppName = "FSRM";
        break;
		case $STREAM_REC_TYPE_ADAM_TAG:
            $AppName = "ADAM";
        break;
		case $STREAM_REC_TYPE_CLUSTER_DB_TAG:
            $AppName = "ClusterDB";
        break;
		case $STREAM_REC_TYPE_NPS_TAG:
            $AppName = "NPS";
        break;
		case $STREAM_REC_TYPE_TSG_TAG:
            $AppName = "TSG";
        break;
		case $STREAM_REC_TYPE_DFSR_TAG:
            $AppName = "DFSR";
        break;
		case $STREAM_REC_TYPE_VMWARESRV_TAG:
            $AppName = "VMWare";
        break;
		case $SYSTEM_LEVEL_TAG:
            $AppName = "SystemLevel";
        break;
		case $STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_TAG:
            $AppName = "APPVSSWRITER";
        break;
        default:
            $AppName = "";
        break;
    }
    return $AppName;
}


/*get the tag name from tag value*/
function get_tag_type($tagName)
{
    global $conn_obj;
	$tagType = array();
	$query ="SELECT
                ValueData,
				ValueName
            FROM
                consistencyTagList
            WHERE
                ValueName IN ('$tagName')";
    $rs = $conn_obj->sql_query($query);  
    while($fetch_name = $conn_obj->sql_fetch_object($rs))
	{
		$tagType[$fetch_name->ValueName] = $fetch_name->ValueData;
	}
    return ($tagType);
}
/*get the tag type from tag value*/
function AppNameToTagType($eventName)
{
    global $STREAM_REC_TYPE_ORACLE_TAG,$STREAM_REC_TYPE_SYSTEMFILES_TAG,$STREAM_REC_TYPE_REGISTRY_TAG;
    global $STREAM_REC_TYPE_SQL_TAG,$STREAM_REC_TYPE_EVENTLOG_TAG,$STREAM_REC_TYPE_WMI_DATA_TAG;
    global $STREAM_REC_TYPE_COM_REGDB_TAG,$STREAM_REC_TYPE_FS_TAG,$STREAM_REC_TYPE_USERDEFINED_EVENT;
    global $STREAM_REC_TYPE_EXCHANGE_TAG,$STREAM_REC_TYPE_IISMETABASE_TAG,$STREAM_REC_TYPE_CA_TAG;
    global $STREAM_REC_TYPE_AD_TAG,$STREAM_REC_TYPE_DHCP_TAG,$STREAM_REC_TYPE_BITS_TAG;
    global $STREAM_REC_TYPE_WINS_TAG,$STREAM_REC_TYPE_RSM_TAG,$STREAM_REC_TYPE_TSL_TAG;
    global $STREAM_REC_TYPE_FRS_TAG,$STREAM_REC_TYPE_BS_TAG,$STREAM_REC_TYPE_SS_TAG;
    global $STREAM_REC_TYPE_CLUSTER_TAG,$STREAM_REC_TYPE_SQL2005_TAG,$STREAM_REC_TYPE_EXCHANGEIS_TAG;
    global $STREAM_REC_TYPE_EXCHANGEREPL_TAG,$STREAM_REC_TYPE_SQL2008_TAG,$STREAM_REC_TYPE_SHAREPOINT_TAG;
    global $STREAM_REC_TYPE_OSEARCH_TAG,$STREAM_REC_TYPE_SPSEARCH_TAG,$STREAM_REC_TYPE_HYPERV_TAG;
    global $STREAM_REC_TYPE_ASR_TAG,$STREAM_REC_TYPE_SC_OPTIMIZATION_TAG,$STREAM_REC_TYPE_MSSEARCH_TAG;
    global $STREAM_REC_TYPE_REVOCATION_TAG,$STREAM_REC_TYPE_TAGGUID_TAG;
	global $STREAM_REC_TYPE_TASK_SCHEDULER_TAG,$STREAM_REC_TYPE_VSS_METADATA_STORE_TAG,$STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG,$STREAM_REC_TYPE_IIS_CONFIG_TAG, $STREAM_REC_TYPE_FSRM_TAG, $STREAM_REC_TYPE_ADAM_TAG, $STREAM_REC_TYPE_CLUSTER_DB_TAG, $STREAM_REC_TYPE_NPS_TAG, $STREAM_REC_TYPE_TSG_TAG, $STREAM_REC_TYPE_DFSR_TAG, $STREAM_REC_TYPE_VMWARESRV_TAG, $SYSTEM_LEVEL_TAG, $STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_TAG,$STREAM_REC_TYPE_SQL2012_TAG;
    $AppType = 0 ;
    switch($eventName)
    {
        case "ORACLE": 
            $AppType = $STREAM_REC_TYPE_ORACLE_TAG;            
        break;
        case "SystemFiles":
            $AppType = $STREAM_REC_TYPE_SYSTEMFILES_TAG;            
        break;
        case "Registry":
            $AppType = $STREAM_REC_TYPE_REGISTRY_TAG;
        break;
        case "SQL":
            $AppType = $STREAM_REC_TYPE_SQL_TAG;
        break;
        case "EventLog":
            $AppType = $STREAM_REC_TYPE_EVENTLOG_TAG;
        break;
        case "WMI":
            $AppType = $STREAM_REC_TYPE_WMI_DATA_TAG ;
        break;
        case "COM+REGDB":
            $AppType = $STREAM_REC_TYPE_COM_REGDB_TAG;
        break;
        case "FS":
            $AppType = $STREAM_REC_TYPE_FS_TAG;
        break;
        case "USERDEFINED":
            $AppType = $STREAM_REC_TYPE_USERDEFINED_EVENT;
        break;
        case "Exchange":
            $AppType = $STREAM_REC_TYPE_EXCHANGE_TAG;
        break;
        case "IISMETABASE":
            $AppType = $STREAM_REC_TYPE_IISMETABASE_TAG;
        break;
        case "CA":
            $AppName = $STREAM_REC_TYPE_CA_TAG;
        break;
        case "AD":
            $AppType = $STREAM_REC_TYPE_AD_TAG;
        break;
        case "DHCP":
            $AppType = $STREAM_REC_TYPE_DHCP_TAG;
        break;
        case "BITS":
            $AppType = $STREAM_REC_TYPE_BITS_TAG ;
        break;
        case "WINS":
            $AppType = $STREAM_REC_TYPE_WINS_TAG;
        break;
        case "RSM":
            $AppType = $STREAM_REC_TYPE_RSM_TAG ;
        break;
        case "TSL":
            $AppType = $STREAM_REC_TYPE_TSL_TAG;
        break;
        case "FRS":
            $AppType = $STREAM_REC_TYPE_FRS_TAG ;
        break;
        case "BootableState":
            $AppType = $STREAM_REC_TYPE_BS_TAG;
        break;
        case "ServiceState":
            $AppType = $STREAM_REC_TYPE_SS_TAG;
        break;
        case "ClusterService":
            $AppType = $STREAM_REC_TYPE_CLUSTER_TAG;
        break;
        case "Sql2005":
            $AppType = $STREAM_REC_TYPE_SQL2005_TAG ;
        break;
        case "ExchangeIS":
            $AppType = $STREAM_REC_TYPE_EXCHANGEIS_TAG;
        break;
        case "ExchangeRepl":
            $AppType = $STREAM_REC_TYPE_EXCHANGEREPL_TAG;
        break;
        case "Sql2008":
            $AppType = $STREAM_REC_TYPE_SQL2008_TAG;
        break;
        case "SharePoint":
            $AppType = $STREAM_REC_TYPE_SHAREPOINT_TAG;
        break;
        case "OSearch":
            $AppType = $STREAM_REC_TYPE_OSEARCH_TAG ;
        break;
        case "SPSearch":
            $AppType = $STREAM_REC_TYPE_SPSEARCH_TAG;
        break;
        case "RevocationTag":
            $AppType = $STREAM_REC_TYPE_REVOCATION_TAG;
        break;
        case "HyperV":
            $AppType = $STREAM_REC_TYPE_HYPERV_TAG;
        break;
        case "ASR":
            $AppType = $STREAM_REC_TYPE_ASR_TAG;
        break;
        case "ShadowCopyOptimization":
            $AppType = $STREAM_REC_TYPE_SC_OPTIMIZATION_TAG;
        break;
        case "MSSearch":
            $AppType = $STREAM_REC_TYPE_MSSEARCH_TAG;
        break;
		case "TaskScheduler":
            $AppType = $STREAM_REC_TYPE_TASK_SCHEDULER_TAG;
        break;
		case "VSSMetaDataStore":
            $AppType = $STREAM_REC_TYPE_VSS_METADATA_STORE_TAG;
        break;
		case "PerformanceCounter":
            $AppType = $STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG;
        break;
		case "IISConfig":
            $AppType = $STREAM_REC_TYPE_IIS_CONFIG_TAG;
        break;
		case "FSRM":
            $AppType = $STREAM_REC_TYPE_FSRM_TAG;
        break;
		case "ADAM":
            $AppType = $STREAM_REC_TYPE_ADAM_TAG;
        break;
		case "ClusterDB":
            $AppType = $STREAM_REC_TYPE_CLUSTER_DB_TAG;
        break;
		case "NPS":
            $AppType = $STREAM_REC_TYPE_NPS_TAG;
        break;
		case "TSG":
            $AppType = $STREAM_REC_TYPE_TSG_TAG;
        break;
		case "DFSR":
            $AppType = $STREAM_REC_TYPE_DFSR_TAG;
        break;
		case "VMWare":
            $AppType = $STREAM_REC_TYPE_VMWARESRV_TAG;
        break;
		case "SystemLevel":
            $AppType = $SYSTEM_LEVEL_TAG;
        break;
		case "APPVSSWRITER":
            $AppType = $STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_TAG;
        break;
		case "Sql2012":
            $AppType = $STREAM_REC_TYPE_SQL2012_TAG;
        break;
        default:
            $AppType = 0;
        break;
    }
    return $AppType;
}

/*
    * Function Name: update_recovery_ranges_v2
    *
    * Description:
    * This function is used to update the recovery ranges from updateCdpInformationV2 function available in om_vxstub.php
    *
    * Parameters:
    *   Param 1 [IN]: hostid
    *	Param 2 [IN]: deviceName
    *	Param 3 [IN]: cdp_summary
    *	Param 4 [IN]: cdp_timeranges
    *	Param 5 [IN]: cdp_events
    *
    * Return Value:
    *     Returns boolean
    *
    * Exceptions:
    *     NA
    */
function update_recovery_ranges_v2($hostid,$deviceName,$cdp_summary,$cdp_timeranges,$cdp_events,$conn_obj)
{
    global $conn_obj;
    global $logging_obj;
	#$logging_obj->my_error_handler("INFO",date('m-d-Y h:i:s')." - update_recovery_ranges_v2");
    $commonfunctions_obj = new CommonFunctions();
    $deviceName = $commonfunctions_obj->verifyDatalogPath($deviceName);
    $deviceName = $conn_obj->sql_escape_string ($deviceName);
    /*update cdp details*/
    $result     = update_cdp_summary_v2($hostid,$deviceName,$cdp_summary,$cdp_timeranges,$cdp_events,$conn_obj);
    return (bool)$result;
}

/*
* Function Name: update_cdp_summary_v2
*
* Description:
* This function is used to update the cpd summary; this is part of updateCdpInformationV2 from om_vxstub.php
*  
* Parameters:
*   Param 1 [IN]: hostid
*	Param 2 [IN]: deviceName
*	Param 3 [IN]: cdp_summary
*	Param 4 [IN]: cdp_timeranges
*	Param 5 [IN]: cdp_events
*
* Return Value:
*     Returns boolean
*
* Exceptions:
*     NA
*/
function update_cdp_summary_v2($hostid,$deviceName,$cdp_summary,$cdp_timeranges,$cdp_events,$conn_obj)
{
    //global $conn_obj;
    global $logging_obj;
	global $multi_query;
	global $src_updates;
	#$logging_obj->my_error_handler("INFO",date('m-d-Y h:i:s')." - update_cdp_summary_v2 :: ");
    /*
        cdp_summary sample structure 
        cdp_summary
        {
                logsFromutc
                logsToutc
                freeSpace                              // free space on the volume
                version;                                 // retention version  (unused by CS for now)
                revision;                                // retention version  (unused by CS for now)
                logType;                              // undo/redo (unused by CS for now)
                eventSummary                    // bookmark summary (see below, unused by CS for  now)
        };
	*/
    $commonfunctions_obj = new CommonFunctions();
    $sparseretention_obj = new SparseRetention();    
    $logsFromutc = $cdp_summary[1];	
	$logs_from_utc_str = (string)$logsFromutc;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($logsFromutc) && $logsFromutc != '' && (!preg_match('/^\d+$/',$logs_from_utc_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for logsFromutc $logsFromutc in update_cdp_summary_v2");
	}
	/*Convert the time stamp to readable format*/
    $logsFrom = $commonfunctions_obj->getTimeFormat($logsFromutc);

    $logsToutc = $cdp_summary[2];
	$logs_to_utc_str = (string)$logsToutc;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($logsToutc) && $logsToutc != '' && (!preg_match('/^\d+$/',$logs_to_utc_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for logsToutc $logsToutc in update_cdp_summary_v2"); 
	}
    /*Convert the time stamp to readable format*/	
    $logsTo = $commonfunctions_obj->getTimeFormat($logsToutc);
	
    $freeSpace = $cdp_summary[3];
	$free_space_str = (string)$freeSpace;
	//If the string contains all digits, then only allow, otherwise return.
	if (isset($freeSpace) && $freeSpace != '' && (!preg_match('/^\d+$/',$free_space_str))) 
	{
		$commonfunctions_obj->bad_request_response("Invalid post data for freeSpace $freeSpace in update_cdp_summary_v2");
	}
	
    /*unused by CS for now*/
    $version = $cdp_summary[4];
    /*unused by CS for now*/
    $revision = $cdp_summary[5];
    /*unused by CS for now*/
    $logType = $cdp_summary[6];
    /*unused by CS for now*/
    $eventSummary = $cdp_summary[7];
	
	/*Get the ruleids*/
    $select_ruleid_sql ="SELECT
                            pairId 
                        FROM 
                            srcLogicalVolumeDestLogicalVolume
                        WHERE 
                            destinationHostId = ?
                        AND 
                            destinationDevicename = ?";
    $select_rs =$conn_obj->sql_query($select_ruleid_sql, array($hostid, $deviceName));
    if(!$select_rs) return false;
    /*For each ruleid update the currenLogSize*/
	foreach ($select_rs as $data)
	{
		$pairId = $data['pairId'];
		break;
	}
	
    /*update logfrom logsto in srcLogicalVolumes*/
    $update_logs_sql = "UPDATE 
                            srcLogicalVolumeDestLogicalVolume
                        SET logsFrom = '".$logsFrom."',
                            logsTo = '".$logsTo."',
                            logsFromUTC = ".$logsFromutc.",
                            logsToUTC = ".$logsToutc." 
                        WHERE 
                            destinationHostId = '".$hostid."' 
						AND
                            destinationDevicename = '".$deviceName."'";
    #$multi_query[]= $update_logs_sql;
	$src_updates[] = $update_logs_sql;
	
    /*update freespace */
    if($freeSpace != 0)
	{
        $hostDetails = $commonfunctions_obj->getTargetDeviceDetails_v2($pairId);
        $log_details = $sparseretention_obj->get_logging_details($hostDetails['destinationHostId'], $hostDetails['destinationDeviceName']);
        $retVol = $log_details[1]['storage_device_name'];
        $update_logicalVolume = "UPDATE 
                                    logicalVolumes 
                                SET 
                                    freeSpace = '$freeSpace' 
                                WHERE
                                    hostId = '".$hostid."' 
                                AND
                                    deviceName = '".$conn_obj->sql_escape_string($retVol)."'";
        $multi_query[]= $update_logicalVolume;		
	}

	/*update retention time stamp */
	$result = update_retentiontimestamp($logsFrom,$logsFromutc,$logsTo,$logsToutc,$hostid,$deviceName,$cdp_timeranges,$pairId,$conn_obj);
	if(!$result) return false;
	$k++;
	
	/*update retention tag */
	$tag_result = update_retentiontag($logsFrom,$logsFromutc,$hostid, $deviceName, $cdp_events,$pairId,$conn_obj);
	if(!$tag_result) return false;

    return TRUE;
}

/*
    * Function Name: update_recovery_ranges_disk_usage
    *
    * Description:
    * This function is used to update the recovery ranges; this is part of updateCdpDiskUsage from om_vxstub.php
    *  
    * Parameters:
    *   Param 1 [IN]: hostid
    *	Param 2 [IN]: deviceName
    *	Param 3 [IN]: start_timestamp
    *	Param 4 [IN]: end_timestamp
    *	Param 5 [IN]: no_of_files
    *	Param 6 [IN]: disk_space
    *	Param 7 [IN]: space_saved_by_compression
    *	Param 8 [IN]: space_saved_by_sparsepolicy
    *
    * Return Value:
    *     Returns boolean
    *
    * Exceptions:
    *     NA
    */
function update_recovery_ranges_disk_usage($hostid,$deviceName,$start_timestamp,$end_timestamp,$no_of_files,$disk_space,$space_saved_by_compression,$space_saved_by_sparsepolicy)
{
    global $conn_obj;
    global $logging_obj;
    $commonfunctions_obj = new CommonFunctions();
    $deviceName = $commonfunctions_obj->verifyDatalogPath($deviceName);
    $deviceName = $conn_obj->sql_escape_string($deviceName);
    /*update cdp details*/
    $result     = update_cdp_summary_disk_usage($hostid,$deviceName,$start_timestamp,$end_timestamp,$no_of_files,$disk_space,$space_saved_by_compression,$space_saved_by_sparsepolicy);
    return (bool)$result;
}

/*
    * Function Name: update_target_disk_usage
    *
    * Description:
    * This function is used to update the recovery ranges; this is part of updateCdpDiskUsage from om_vxstub.php
    *  
    * Parameters:
    *   Param 1 [IN]: hostid
    *	Param 2 [IN]: deviceName
    *	Param 3 [IN]: target_disk_space
    *	Param 4 [IN]: target_space_saved_by_thinprovision
    *	Param 5 [IN]: target_space_saved_by_compression
    *
    * Return Value:
    *     Returns boolean
    *
    * Exceptions:
    *     NA
    */
function update_target_disk_usage($hostid,$deviceName,$target_disk_space,$target_space_saved_by_thinprovision,$target_space_saved_by_compression)
{
    global $conn_obj;
    global $logging_obj;
    $commonfunctions_obj = new CommonFunctions();
    $deviceName = $commonfunctions_obj->verifyDatalogPath($deviceName);
    $deviceName = $conn_obj->sql_escape_string($deviceName);
    /*update cdp details*/
    $result     = update_target_summary_disk_usage($hostid,$deviceName,$target_disk_space,$target_space_saved_by_thinprovision,$target_space_saved_by_compression);
    return (bool)$result;
}


function update_cdp_summary_disk_usage($hostid,$deviceName,$start_timestamp,$end_timestamp,$no_of_files,$disk_space,$space_saved_by_compression,$space_saved_by_sparsepolicy)
{
    global $conn_obj;
    global $logging_obj;
 
    $commonfunctions_obj = new CommonFunctions();
    
    $numFiles = $no_of_files;
    $diskSpace = $disk_space;
    
    /*Get the ruleids*/
    $select_ruleid_sql ="select
                            ruleId 
                        from 
                            srcLogicalVolumeDestLogicalVolume
                        where 
                            destinationHostId = ?
                        and 
                            destinationDevicename = ?";
							
    $select_rs = $conn_obj->sql_query($select_ruleid_sql,array($hostid,$deviceName));

    if(!$select_rs) return false;
    /*For each ruleid update the currenLogSize*/
    foreach ($select_rs as $key=>$data)
    {
        $modifiedDate = date("Y-m-d H:i:s");
        $update_sql = "update 
                        retLogPolicy 
                    set 
                        currLogsize = ?,
                        modifiedDate = ?,
                        startTimeStamp = ?,
                        endTimeStamp = ?,
                        spaceSavedBySparsePolicy = ?,
						spaceSavedByCompression = ?
                    where
                        ruleid = ?";
		$update_ret_rs =$conn_obj->sql_query($update_sql, array($diskSpace,$modifiedDate,$start_timestamp,$end_timestamp,$space_saved_by_sparsepolicy,$space_saved_by_compression,$data['ruleId']));
		$ruleId[] = $data['ruleId'];
	}
    
    return TRUE;
}

/*
    * Function Name: update_target_summary_disk_usage
    *
    * Description:
    * This function is used to update the cdp summary; this is part of updateCdpDiskUsage from om_vxstub.php
    *  
    * Parameters:
    *   Param 1 [IN]: hostid
    *	Param 2 [IN]: deviceName
    *	Param 3 [IN]: target_disk_space
    *	Param 4 [IN]: target_space_saved_by_thinprovision
    *	Param 5 [IN]: target_space_saved_by_compression
    *
    * Return Value:
    *     Returns boolean
    *
    * Exceptions:
    *     NA
    */
function update_target_summary_disk_usage($hostid,$deviceName,$target_disk_space,$target_space_saved_by_thinprovision,$target_space_saved_by_compression)
{
	global $conn_obj;
	global $logging_obj;
 
    $commonfunctions_obj = new CommonFunctions();
    #$logging_obj->my_error_handler("DEBUG","update_cdp_summary_disk_usage \n");
    #$logging_obj->my_error_handler("DEBUG","update_cdp_summary_disk_usage ::cdp_events".print_r($cdp_events,TRUE)."\n");

	$modifiedDate = date("Y-m-d H:i:s");
	$update_sql = "update 
					logicalVolumes 
				set 
					diskSpaceSavedByCompression = ?,
					diskSpaceSavedByThinprovision = ?,
					dataSizeOnDisk = ?
				where 
					hostId = ? 
				and 
					deviceName = ?";
	$update_ret_rs =$conn_obj->sql_query($update_sql, array($target_space_saved_by_compression,$target_space_saved_by_thinprovision,$target_disk_space,$hostid, $deviceName));
		
    return TRUE;
}

?>
