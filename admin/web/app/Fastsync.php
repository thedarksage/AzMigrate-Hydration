<?
include_once("autoloadclasses.php");
if (preg_match('/Windows/i', php_uname()))
{
        include_once("$REPLICATION_ROOT\\admin\\web\\cs\\classes\\data\\LogicalVolumesVO.php");
        include_once("$REPLICATION_ROOT\\admin\\web\\cs\\classes\\data\\SrcLogicalVolumeDestLogicalVolumeVO.php");
		include_once("$REPLICATION_ROOT\\admin\\web\\cs\\GlobalConstants.php");		
}
else
{
        include_once("$REPLICATION_ROOT/admin/web/cs/classes/data/LogicalVolumesVO.php");
        include_once("$REPLICATION_ROOT/admin/web/cs/classes/data/SrcLogicalVolumeDestLogicalVolumeVO.php");
		include_once("$REPLICATION_ROOT/admin/web/cs/GlobalConstants.php");
}

class Fastsync
{
    /*
     *properties
     */
    private $src_dest_dao;
    private $src_dest_vo;
    private $lv_dao;
    private $lv_vo;


	/*
	 *    Constructor
	 */
	public function Fastsync()
	{
		$this->conn = & classload( 'Connection',
		'admin.web.cs.classes.conn.Connection');
		$this->src_dest_dao = & classload('SrcLogicalVolumeDestLogicalVolumeDAO', 
		'admin.web.cs.classes.data.SrcLogicalVolumeDestLogicalVolumeDAO');
		$this->lv_dao = & classload( 'LogicalVolumesDAO', 'admin.web.cs.classes.data.LogicalVolumesDAO');
	}

	/*
	* Function Name: updateResyncTimes
	*
	* Description:
	*    
	* This function updates the resync start time and end time along with the  
	* sequence times for the given host id and device name, which are sent by  
	* the agent. Both the requests are comes from source side.
	*
	* Parameters: 
	* Initiator Id, source host identity, source device name, destination host 
	* identity, destination device name, resync tag time stamp, sequence time, 
	* type
	*
	* Return Value:
	* This function returns the 0(success) or non zero for failure.
	*
	*/
	public function updateResyncTimes($initiator_id, $source_host_id, $source_device_name, $destination_host_id, $destination_device_name,	$tag_timestamp,	$seq,	$jobid,$type)
	{
		global $logging_obj;
		$commonfunctions_obj = new commonfunctions();
		$app_obj = new Application();
		$return_status = STATUS_SUCCESS;
		
		if (!in_array($type, array('starttime','endtime'))) 
		{
			$commonfunctions_obj->bad_request_response("Invalid post data for type $type in updateResyncTimes.");
		}
		
		$tag_timestamp_str = (string)$tag_timestamp;
		//If the string contains all digits, then only allow the request, otherwise return from here.
		if (isset($tag_timestamp_str) && $tag_timestamp_str != '' && (!preg_match('/^\d+$/',$tag_timestamp_str))) 
		{
			//As the post data of mode data is invalid, hence returning from here
			$commonfunctions_obj->bad_request_response("Invalid post data mode $tag_timestamp in updateResyncTimes for type $type.");
		}
		
		$seq_str = (string)$seq;
		//If the string contains all digits, then only allow the request, otherwise return from here.
		if (isset($seq_str) && $seq_str != '' && (!preg_match('/^\d+$/',$seq_str))) 
		{
			//As the post data of mode data is invalid, hence returning from here
			$commonfunctions_obj->bad_request_response("Invalid post data mode $seq in updateResyncTimes for type $type.");
		}
		
		$params = array(
		'srcid'=>$source_host_id,                        
		'srcdevicename'=>$source_device_name, 
		'destid'=>$destination_host_id,
		'destdevicename'=>$destination_device_name);
		$search_results = $commonfunctions_obj->find_by_pair_details($params);
		
		$source_device_name = $this->conn->sql_escape_string($source_device_name);
		$destination_device_name = $this->conn->sql_escape_string($destination_device_name);
		
		/*If data is not found, then return the status as data mismatch*/
		if (count($search_results) == 0)
		{
			/*Log the message here*/
			$return_status = AFFECTED_ROWS_FAILURE;
		}
		else
		{		
			/*Collect the required data from the value object*/
			foreach ($search_results as $key=>$obj)
			{
				$job_id = $obj['jobId'];
			}
			/*If jobid is not present in the database, then return the
			job id mismatch status*/
			if ($jobid && ($jobid != $job_id))
			{
				$return_status = JOBID_MISMATCH;
			}
			else
			{
				$this->src_dest_vo    = & classload(
				'SrcLogicalVolumeDestLogicalVolumeVO', 
				'admin.web.cs.classes.data.SrcLogicalVolumeDestLogicalVolumeVO');
			
				/*Construct the params array to use it in update query*/
				$params = array('destid'=>$destination_host_id,            
								'destdevicename'=>$destination_device_name);
				/*Constructing the value object for the start time or end time*/
				if ($type == 'starttime')
				{
					$value = 0;
					$this->src_dest_vo->set_resync_start_tagtime($tag_timestamp);
					$this->src_dest_vo->set_resync_start_tagtime_sequence($seq); 
					
					/* As part of resyncStartTagTime update we are resetting shouldresyncSetFrom and resyncSetCXtimestamp */
					
					$this->src_dest_vo->set_should_resync_set_from($value);
					$this->src_dest_vo->set_resync_set_cxtimestamp($value);
					$update_status = $this->src_dest_dao->
					update_by_agent_sync_start_time(
					$this->conn,$this->src_dest_vo,$params);
				}
				elseif ($type == 'endtime')
				{
					$this->src_dest_vo->set_resync_end_tagtime($tag_timestamp);
					$this->src_dest_vo->set_resync_end_tagtime_sequence($seq);
					$update_status = $this->src_dest_dao->
					update_by_agent_sync_end_time(
					$this->conn,$this->src_dest_vo,$params);
				}

				$affected_rows_count = $this->conn->sql_affected_row();
				if($update_status === FALSE) 
				{
					$return_status = QUERY_EXECUTION_FAILURE;
				}
				elseif ($affected_rows_count == -1)
				{
					$return_status = AFFECTED_ROWS_FAILURE;
				}			
				#11790 Fix
				/*Updating RST?
				a. On update of RST, if the source belongs to part of 1toN, then get all the
				secondary pairs whose execution state is 3.
				b. Get each pair batch resync value and if batch resync counter == no.of.pairs
				in resync step1, then move the pair to execution state  to 2, else move it to
				1.*/
				if (($type == 'starttime') && ($update_status === TRUE))
				{
					$is_primary_call = "SELECT
											oneToManySource
										FROM
											srcLogicalVolumeDestLogicalVolume
										WHERE
											sourceHostId = '$source_host_id'
										AND
											sourceDeviceName = '$source_device_name'
										AND							
											destinationHostId = '$destination_host_id'
										AND
											destinationDeviceName = '$destination_device_name'";

					$is_primary_result_set = $this->conn->sql_query($is_primary_call);				
					$is_primary_row = $this->conn->sql_fetch_row($is_primary_result_set);
					$primary_oneToManySource = $is_primary_row[0];
					if($primary_oneToManySource != 0)
					{														
						$destination_details = "SELECT
													destinationHostId,
													destinationDeviceName,
													scenarioId,
													planId
												FROM
													srcLogicalVolumeDestLogicalVolume
												WHERE
													sourceHostId = '$source_host_id'
												AND
													sourceDeviceName = '$source_device_name'
												AND
													executionState = 3";
						
						$destination_result_set = $this->conn->sql_query($destination_details);
						while($destination_row = $this->conn->sql_fetch_row($destination_result_set))
						{
							$dest_host_id = $destination_row[0];
							$dest_device_name = $destination_row[1];
							$dest_device_name = $this->conn->sql_escape_string($dest_device_name);
							$scenario_id = $destination_row[2];						
							#13167 Fix
							/* 
							For 1toN case at the time of RST update, we can get the batchResync value
							based on the planId and planType either from applicationScenario (or)
							applicationPlan table
							*/
							$plan_id = $destination_row[3];
							$plan_details = "SELECT
													planType
												FROM
													applicationPlan
												WHERE
													planId = '$plan_id'";
													
							$plan_rs = $this->conn->sql_query($plan_details);
							$plan_set = $this->conn->sql_fetch_row($plan_rs);
							$plan_type = $plan_set[0];
							$batch_resync;
							if(strcmp($plan_type,'OTHER') == 0 )
							{
								$sql = "SELECT
												batchResync
											FROM
												applicationPlan
											WHERE
												planId = '$plan_id'";	
								$appli_rs = $this->conn->sql_query($sql);
								$appli_set = $this->conn->sql_fetch_row($appli_rs);
								$batch_resync = $appli_set[0];
							}
							else
							{
								$result = array();		
								$sql = "SELECT
											scenarioDetails
										FROM
											applicationScenario
										WHERE
											scenarioId = '$scenario_id'";
								$scenario_rs = $this->conn->sql_query($sql);
								$res_set = $this->conn->sql_fetch_object($scenario_rs);
								$scenario_details = $res_set->scenarioDetails;
								$cdata_arr = $commonfunctions_obj->getArrayFromXML($scenario_details);
								$scenario_details = unserialize($cdata_arr['plan']['data']['value']);
								$result['globalOption'] = $scenario_details['globalOption'];
								$batch_resync = $result['globalOption']['batchResync'];
							}			
							$resync_pairs_query = "SELECT
														isQuasiflag
													FROM
														srcLogicalVolumeDestLogicalVolume
													WHERE
														scenarioId = '$scenario_id'
													AND
														planId = '$plan_id'
													AND
														isQuasiflag = 0
													AND
														executionState = 1
													GROUP BY 
														destinationHostId, destinationDeviceName";
															
							$resync_result_set = $this->conn->sql_query($resync_pairs_query);						
							$resync_count = $this->conn->sql_num_row($resync_result_set);
							if(($batch_resync > $resync_count) || ($batch_resync == '') || ($batch_resync == 0))
							{
								$configured_query = "UPDATE
														srcLogicalVolumeDestLogicalVolume 
													SET
														executionState = 1 ,
														resyncStartTime = now()
													WHERE
														destinationHostId= '$dest_host_id' 
													AND
														destinationDeviceName = '$dest_device_name'";
								$update_result = $this->conn->sql_query($configured_query);
							}				
							elseif($batch_resync == $resync_count)
							{
								$unconfigured_query = "UPDATE
															srcLogicalVolumeDestLogicalVolume 
														SET
															executionState = 2
														WHERE
															destinationHostId= '$dest_host_id' 
														AND
															destinationDeviceName = '$dest_device_name'";
								$update_execute_result = $this->conn->sql_query($unconfigured_query);
							}																
						}				
					}
				}
			}			
		}
		return $return_status;            
	}


	/*
	* Function Name: setResyncTransitionStepOneToTwo
	*
	* Description:
	* Updates all the necessary database flags from resync transition step1 to 
	* step2.  
	*  
	*
	* Parameters: 
	* Initiator identity, source host identity, source device name, destination
	* host identity,destination device name and sync no more file path.
	*    
	* Return Value:
	*     Retruns zero on success and returns non zero on failure. 
	*
	*/
	public function setResyncTransitionStepOneToTwo(
	$initiator_id, 
	$src_host_id,
	$src_device_name,
	$destination_host_id,
	$destination_device_name,
	$sync_nomore_file_path)
	{
		global $logging_obj;
		$commonfunctions_obj = new commonfunctions();
		$params = array(
		'srcid'=>$src_host_id, 
		'srcdevicename'=>$src_device_name,
		'destid'=>$destination_host_id, 
		'destdevicename'=>$destination_device_name);
		$search_results = $commonfunctions_obj->find_by_pair_details($params);
		
		/*Escaping the device names*/
		$src_device_name = $this->conn->sql_escape_string($src_device_name);
		$destination_device_name = $this->conn->sql_escape_string($destination_device_name);
		$return_status = STATUS_SUCCESS;
		
		if (count($search_results) == 0)
		{
			$return_status = AFFECTED_ROWS_FAILURE;
		}
		else
		{
			/*Get the required data from the value object*/
			foreach ($search_results as $key=>$obj)
			{
				$resync_end_tag_time = $obj['resyncEndTagtime'];
				$resync_offset       = $obj['lastResyncOffset'];
				$full_sync_bytes_sent = $obj['fullSyncBytesSent'];
				$is_quasi_flag = $obj['isQuasiflag'];
				$resync_version = $obj['resyncVersion'];
			}
			$params = array( 'destid'=>$destination_host_id,
							 'destdevicename'=>$destination_device_name);
			$pair_results = $this->src_dest_dao->        
							find_by_srcdestlv_destdetails($this->conn, $params);
			
			/* If the last resync offset reaches the full sync bytes sent, then
			change the resync transition from step1 to step2. Allow for direct
			sync if end tag time is present*/
			if (    ($resync_end_tag_time != 0) 
				&&    (    (      ($resync_offset == $full_sync_bytes_sent)
							&&($resync_version == 2)
						) 
					  ||(($resync_version == 3) || ($resync_version == 4))
					) 
				)
			{
				$params = array(
				'id'=>$src_host_id,    
				'devicename'=>$src_device_name);
				$search_results = $this->lv_dao->
				find_by_hostid_devicename($this->conn, $params);

				$params = array(
				'id'=>$destination_host_id, 
				'devicename'=>$destination_device_name);
				$destination_search_results = $this->lv_dao->
				find_by_hostid_devicename($this->conn, $params);

				if (count($search_results) == 0 
					|| count($destination_search_results) == 0)
				{
					$return_status = AFFECTED_ROWS_FAILURE;
				}
				else
				{
					foreach ($search_results as $key=>$obj)
					{
						$src_device_capacity = $obj->get_capacity();
					}

					foreach ($destination_search_results as $key=>$obj)
					{
						$target_device_capacity = $obj->get_capacity();
					}
					if ($src_device_capacity != $target_device_capacity)
					{
						#14710 Fix
						$dbupdate_status_flag = TRUE;
						$affected_rows_count = 1;
					}
					else
					{
						$dbupdate_status_flag = TRUE;
						$affected_rows_count = 1;
					}

					if( $dbupdate_status_flag === TRUE &&
						$affected_rows_count > 0 )
					{
						$params = array(
						'destid'=>$destination_host_id, 
						'destdevicename'=>$destination_device_name);
						$return_data = 
						$this->resyncTransitionUpdate($params,'update');
						$dbupdate_status_flag = $return_data['db_update_status'];
						$affected_rows_count = $return_data['affected_rows'];
					}
					/* If database update fails send the return value as 4*/
					if ($dbupdate_status_flag === FALSE)
					{
						$return_status = QUERY_EXECUTION_FAILURE;
					}
					/* If agent sent wrong data and database is not updated, then send the return value as 6*/
					elseif ($affected_rows_count == 0)
					{
						$return_status = AFFECTED_ROWS_FAILURE;
					}
					/*Update all the database flags to move the pair from step
					one to step two*/
					else
					{
						/*Delete the sync no more file*/
						if (file_exists($sync_nomore_file_path)) 
						{
							if (FALSE === unlink($sync_nomore_file_path))
							{
								$return_status = FILE_DELETION_FAILURE;
								$return_data =
								$this->resyncTransitionUpdate($params,'rollback');
							}
						}
					}				
				}
			}
			else
			{
				$return_status = AFFECTED_ROWS_FAILURE;
			}
		}
		return $return_status;
	}

	private function updateExecutionState($tid,$tdevice)
	{
		$volume_obj = new VolumeProtection();
		$this->src_dest_vo = & classload( 
		'SrcLogicalVolumeDestLogicalVolumeVO', 
		'admin.web.cs.classes.data.SrcLogicalVolumeDestLogicalVolumeVO');
		
		$param = array(
		'tdevice'=>$tdevice,
		'tid'=>$tid
		);
		$search_result = $this->src_dest_dao->        
		find_by_scenarioid_and_resyncorder($this->conn, $param);
		
		foreach ($search_result as $obj)
		{
			$scenario_id = $obj->ary['scenarioId'];
			$plan_id = $obj->ary['planId'];
		}
		
		if($plan_id > 1)
		{
			$param = array(
			'scenario_Id'=>$scenario_id,
			'plan_Id'=>$plan_id
			);
			$search_result = $this->src_dest_dao->        
			find_by_isquasiflag($this->conn, $param);
			
			foreach($search_result as $obj)
			{
				$flag = false;
				$execution_state = $obj->ary['executionState'];
				if($execution_state == 2 || $execution_state == 4)
				{
					$flag = true;
					break;
				}
			}
			
			if($flag)
			{
				$param = array(
				'scenario_Id'=>$scenario_id,
				'execution_state'=>$execution_state,
				'plan_Id'=>$plan_id
				);
				
				$search_result = $this->src_dest_dao->        
				find_by_executionState($this->conn, $param);
				
				foreach($search_result as $obj)
				{
					$dest_id = $obj->ary['destinationHostId'];
					$dest_dev = $obj->ary['destinationDeviceName'];
					
					$dest_dev = $this->conn->sql_escape_string($dest_dev);
				}
				/*
					If executionState is 4 means its auto resync case, call the 
					resync_now function
				*/
				if($execution_state == 4)
				{
					$param = array(
					'tdevice'=>$dest_dev,
					'tid'=>$dest_id
					);
					$search_result = $this->src_dest_dao->        
					find_by_scenarioid_and_resyncorder($this->conn, $param);
					foreach($search_result as $obj)
					{
						$rule_id = $obj->ary['ruleId'];
						$volume_obj->resync_now ($rule_id);
					}
				}
						
				$this->src_dest_vo->set_execution_state(1);
				
				$sel_qry = "SELECT now()";
				$result = $this->conn->sql_query($sel_qry);
				$row = $this->conn->sql_fetch_row($result);
				$current_now = $row[0];			
				$this->src_dest_vo->set_resync_start_time( $current_now );
				
				$param = array(
				'tdevice'=>$dest_dev,
				'tid'=>$dest_id
				);
				$dbupdate_status_flag = $this->src_dest_dao->
				update_by_executionstate(
				$this->conn,$this->src_dest_vo, $param);
			}
		}
	}

	private function resyncTransitionUpdate(
	$params,
	$action)
	{
		$this->src_dest_vo    = & classload( 
		'SrcLogicalVolumeDestLogicalVolumeVO', 
		'admin.web.cs.classes.data.SrcLogicalVolumeDestLogicalVolumeVO');
		
		/*Construct the value object*/
		if ($action != 'rollback')
		{
			$this->src_dest_vo->
					set_resync_end_time("now()");
			$this->src_dest_vo->
					set_is_quasiflag(1);
			$this->src_dest_vo->
					set_quasidiff_starttime("now()");
			$this->src_dest_vo->
					set_remaining_resync_bytes(0);
			$this->src_dest_vo->
					set_throttleresync(0);
			$this->src_dest_vo->
					set_status_update_time("now()");
			$this->src_dest_vo->
					set_last_resync_offset(0);
		}
		else
		{
			$this->src_dest_vo->
					set_resync_end_time(0);
			$this->src_dest_vo->
					set_is_quasiflag(0);
			$this->src_dest_vo->
					set_quasidiff_starttime(0);
			$this->src_dest_vo->
					set_remaining_resync_bytes(0);
			$this->src_dest_vo->
					set_throttleresync(0);
			$this->src_dest_vo->
					set_status_update_time("now()");
			$this->src_dest_vo->
					set_last_resync_offset(0);
		}

		$dbupdate_status_flag = $this->src_dest_dao->
		update_by_destination(
		$this->conn,$this->src_dest_vo, $params);
		if ($action != 'rollback')
		{
			$this->updateExecutionState($params['destid'],$params['destdevicename']);
		}
		$affected_rows_count = $this->conn->sql_affected_row();
		unset($this->src_dest_vo);

		$return_data = array(
		'db_update_status'=>$dbupdate_status_flag,
		'affected_rows'=>$affected_rows_count);
		return $return_data; 
	}

	/*
	* Function Name: getResyncTagsTime
	*
	* Description:
	*  This function gets the tag time and sequence for the given pair input. 
	*  
	*
	* Parameters: 
	* Initiator identity, source host identity, source device name, destination host 
	* identity, destination device name and sync no more file path.
	*    
	* Return Value:
	*     Returns array of tag time and sequence.
	*
	*/
	public function getResyncTagsTime(
	$initiator_id,
	$src_host_id,
	$src_device_name,
	$destination_host_id,
	$destination_device_name,
	$job_id, 
	$field)
	{
		$commonfunctions_obj = new commonfunctions();
		$params = array(
		'srcid'=>$src_host_id ,                        
		'srcdevicename'=>$src_device_name, 
		'destid'=>$destination_host_id , 
		'destdevicename'=>$destination_device_name);
		$search_results = $commonfunctions_obj->find_by_pair_details_in_resync($params);
		
		/* Get the times and assign it to variables*/
		foreach ($search_results as $obj)
		{
			if ($field == 'resyncStartTagtime')
			{
				$tag_time = $obj['resyncStartTagtime'];
				$tag_time_sequence = $obj['resyncStartTagtimeSequence'];

			}
			elseif ($field == 'resyncEndTagtime')
			{
				$tag_time = $obj['resyncEndTagtime'];
				$tag_time_sequence = $obj['resyncEndTagtimeSequence'];
			}
		}  	
		if(!isset($tag_time) || ($tag_time == "NULL"))
		{
				$tag_time = "0";
		}
		if(!isset($tag_time_sequence) || ($tag_time_sequence == "NULL"))
		{
				$tag_time_sequence = "0";
		}
		$return_status = array(0,$tag_time,$tag_time_sequence);
		return $return_status;
	}

	/*
	* Function Name: updateHcdProgressFrom43
	*
	* Description:
	* calls the function hcdProgressUpdatesFrom43 that involves updating fullSyncBytesSent field in scrLogicalVolumeDestLogicalVolume table
	*
	* Parameters: 
	* initiator info array, source host id, source device name, destination host identity, destination device name
	* capacity of hcd files sent and job id.
	*    
	* Return Value:
	*     Returns zero on success and non zero on failure. 
	*
	*/
	public function updateHcdProgressFrom43(
	$initiator_id,
	$src_host_id,
	$src_device_name,
	$destination_host_id,
	$destination_device_name,
	$vol_offset,
	$jobid)
	{
		debugLog("updateHcdProgressFrom43 $initiator_id, $src_host_id, $src_device_name, $destination_host_id, $destination_device_name, $vol_offset, $jobid");
		$commonfunctions_obj = new commonfunctions();
		$params = array('srcid'=>$src_host_id,
				'srcdevicename'=>$src_device_name,
				'destid'=>$destination_host_id,
				'destdevicename'=>$destination_device_name);
		$search_results = $commonfunctions_obj->find_by_pair_details($params);

		$return_status = STATUS_SUCCESS;
		$src_device_name = $this->conn->sql_escape_string($src_device_name);
		$destination_device_name = $this->conn->sql_escape_string($destination_device_name);

		if (count($search_results) == 0)
		{
			/*Log the message here*/
			$return_status = AFFECTED_ROWS_FAILURE;
		}
		else
		{
			foreach ($search_results as $key=>$obj)
			{
				$fullsync_start_time = $obj['fullSyncStartTime'];
				$fullsync_bytes_sent = $obj['fullSyncBytesSent'];
				$job_id = $obj['jobId'];
			}
			/*If jobid is not present in the database, then return*/
			if ( isset($jobid) && ($jobid != $job_id) )
			{
				$return_status = JOBID_MISMATCH;
		   }
			else
			{
				$params = array(
				'destid'=>$destination_host_id,
				'destdevicename'=>$destination_device_name);

				$dbupdate_status_flag = FALSE ;

				$dbupdate_status_flag = $this->hcdProgressUpdatesFrom43($vol_offset,
				$fullsync_start_time,$params);

				/*If database udpate success, then do the rename*/
				if  ($dbupdate_status_flag === TRUE)
				{
					/* If all the bytes of the source volume have been sent,
					then update the fullsync end time.*/
					$lv_params = array(
					'id'=>$src_host_id,
					'devicename'=>$src_device_name);
					$device_results = $this->lv_dao->
					find_by_hostid_devicename($this->conn, $lv_params);

					foreach ($device_results as $key=>$obj)
					{
							$source_device_capacity = $obj->get_capacity();
					}

					$dbupdate_status_flag = TRUE;
					reset($search_results);
					if($source_device_capacity)
					{
						/*If volume offset reaches the source device capcity,
						then do the update of full sync end time*/
						if($vol_offset == $source_device_capacity)
						{
								$this->src_dest_vo    = & classload(
								'SrcLogicalVolumeDestLogicalVolumeVO',
								'admin.web.cs.classes.data.
								SrcLogicalVolumeDestLogicalVolumeVO');

								$this->src_dest_vo->
								set_full_sync_end_time("now()");

								$dbupdate_status_flag = $this->src_dest_dao->
								update_by_destination(
								$this->conn,$this->src_dest_vo, $params);

								$affected_rows_count = $this->conn->sql_affected_row();
								unset($this->src_dest_vo);
						}
					}
					else
					{
						$return_status = QUERY_EXECUTION_FAILURE;
					}
				}
				/* If database update fails send the return value as 4*/
				elseif ($dbupdate_status_flag === FALSE)
				{
					$return_status = QUERY_EXECUTION_FAILURE;
				}
			}
		}
		return $return_status;
	}


	private function hcdProgressUpdatesFrom43
	($vol_offset,
	$fullsync_start_time,
	$params)
	{
		$this->src_dest_vo = & classload(
		'SrcLogicalVolumeDestLogicalVolumeVO',
		'admin.web.cs.classes.data.SrcLogicalVolumeDestLogicalVolumeVO');

		if(0 == $fullsync_start_time)
		{
				$this->src_dest_vo->set_full_sync_start_time("now()");
		}
		$this->src_dest_vo-> set_full_sync_bytes_sent($vol_offset);

		$dbupdate_status_flag = $this->src_dest_dao->
		update_by_destination(
		$this->conn,$this->src_dest_vo, $params);
		unset($this->src_dest_vo);

		return $dbupdate_status_flag;
	}


	/*
	* Function Name: updateStatsResyncFromSource
	*
	* Description:
	* calls the function resyncProgressUpdatesFromSource that involves calculation and db updates
	*
	* Parameters: 
	* source host identity, source device name, destination host identity, destination device name
	* matched bytes, is before resync transition flag and job id.
	*    
	* Return Value:
	*     Returns zero on success and non zero on failure. 
	*
	*/
	public function updateStatsResyncFromSource(
	$src_host_id,
	$src_device_name,
	$destination_host_id,
	$destination_device_name,
	$bytes_matched,
	$is_before_resync_transition, 
	$jobid)
	{
		global $REPLICATION_ROOT;
		
		$commonfunctions_obj = new commonfunctions();
		$bytes_matched_str = (string)$bytes_matched;
		//If the string contains all digits, then only allow the request, otherwise return from here.
		if (isset($bytes_matched_str) && $bytes_matched_str != '' && (!preg_match('/^\d+$/',$bytes_matched_str))) 
		{
			//As the post data of mode data is invalid, hence returning from here.
			$commonfunctions_obj->bad_request_response("Invalid post data bytes matched for $bytes_matched_str in updateStatsResyncFromSource.");
		}
		$params = array(
		'srcid'=>$src_host_id,
		'srcdevicename'=>$src_device_name,
		'destid'=>$destination_host_id,
		'destdevicename'=>$destination_device_name);
		$search_results = $commonfunctions_obj->find_by_pair_details($params);
		
		$src_device_name = $this->conn->sql_escape_string($src_device_name);
		$destination_device_name = $this->conn->sql_escape_string($destination_device_name);
		$return_status = STATUS_SUCCESS;

		/*If search result count as zero, then send the data mis match status*/
		if (count($search_results) == 0)
		{
			#Log the message here
			$return_status = AFFECTED_ROWS_FAILURE;
		}
		else
		{
			/*Collect the required data from the value object*/
			foreach ($search_results as $key=>$obj)
			{
				$resync_end_time = $obj['resyncEndTime'];
				$curr_vol_matched_bytes = $obj['fastResyncMatched'];
				$curr_vol_unmatched_bytes = $obj['fastResyncUnmatched'];
				$curr_last_resync_offset = $obj['lastResyncOffset'];
				$job_id = $obj['jobId'];
			}
			/*If jobid is not present in the database, then return the job id mismatch status*/
			if ($jobid && ($jobid != $job_id))
			{
				$return_status = JOBID_MISMATCH;
			}
			elseif  (0 == $resync_end_time)
			{
				$params = array(
						'destid'=>$destination_host_id,
						'destdevicename'=>$destination_device_name);


				$dbupdate_status_flag = $this->resyncProgressUpdatesFromSource(
				$bytes_matched,
				$is_before_resync_transition,
				$params,
				$curr_last_resync_offset);

				/*If database update fails send the return value as 4*/
				if ($dbupdate_status_flag === FALSE)
				{
						$return_status = QUERY_EXECUTION_FAILURE;
				}
			}
			else
			{
				$return_status = INVALID_DB_STATE;
			}
		}
		return $return_status;
	}

	/*
	* Function Name: resyncProgressUpdatesFromSource
	*
	* Description:
	* Updates lastResyncOffset and fastResyncMatched fields in srcLogicalVolumeDestLogicalVolume table
	* bytes matched value is send as part of the parameters. this is updated to fastResyncMatched without any modifications.
	* lastResyncOffset is updated of (fastResyncUnmatched field in tbl)+(fastResyncUnused field)+(bytes matched value) 
	* if $is_before_resync_transition is 1
	* $is_before_resync_transition variable tells if the call is made just before the transit to resync step II
	* if $is_before_resync_transition equals 1, then bytes matched is the sum of bytes matched in resync step I till now.
	*
	* Parameters: 
	* matched bytes, is before resync transition flag and params (params contain dest host id and dest device name).
	*    
	* Return Value:
	*     Returns zero on success and non zero on failure. 
	*
	*/
	public function resyncProgressUpdatesFromSource(
	$bytes_matched,
	$is_before_resync_transition,
	$params,
	$curr_last_resync_offset)
	{
		/*Construct the value object to do database update*/
		$this->src_dest_vo = & classload('SrcLogicalVolumeDestLogicalVolumeVO',
		'admin.web.cs.classes.data.SrcLogicalVolumeDestLogicalVolumeVO');

		if ($is_before_resync_transition)
		{
			$resync_progress_bytes = 
			"fastResyncUnmatched+fastResyncUnused+$bytes_matched";	
		}
		else
		{
			$resync_progress_bytes = "lastResyncOffset-fastResyncMatched+$bytes_matched";
		}

		$this->src_dest_vo->set_last_resync_offset($resync_progress_bytes);
		$this->src_dest_vo->set_fast_resync_matched($bytes_matched);
		/*Added this flag update for IR stuck alert purpose. If no resync progress in 15 minutes health factor is warning and if it is more than one hour stuck health factor is critical.*/
		if ($curr_last_resync_offset != $resync_progress_bytes)
		{	
			$this->src_dest_vo->set_last_t_m_change("now()");
		}
		
		$dbupdate_status_flag = $this->src_dest_dao->
		update_by_destination(
		$this->conn,$this->src_dest_vo, $params);

		unset($this->src_dest_vo);

		return $dbupdate_status_flag;
	}

	/*
	* Function Name: updateStatsResyncFromTarget
	*
	* Description:
	* calls the function updateStatsResyncFromTarget that involves calculation and db updates
	*
	* Parameters: 
	* source host identity, source device name, destination host identity, destination device name
	* unmatched bytes, partially matched bytes, job id and IR stats.
	*    
	* Return Value:
	*     Returns zero on success and non zero on failure. 
	*
	*/
	public function updateStatsResyncFromTarget(
	$src_host_id,
	$src_device_name,
	$destination_host_id,
	$destination_device_name,
	$unmatched_bytes,
	$partial_matched_bytes,
	$jobid,
	$stats='')
	{
		global $REPLICATION_ROOT;
		$commonfunctions_obj = new commonfunctions();
		$unmatched_bytes_str = (string)$unmatched_bytes;
		//If the string contains all digits, then only allow the request, otherwise return from here.
		if (isset($unmatched_bytes_str) && $unmatched_bytes_str != '' && (!preg_match('/^\d+$/',$unmatched_bytes_str))) 
		{
			//As the post data of mode data is invalid, hence returning from here
			$commonfunctions_obj->bad_request_response("Invalid post data unmatched bytes $unmatched_bytes in updateStatsResyncFromTarget .");
		}
		$partial_matched_bytes_str = (string)$partial_matched_bytes;
		//If the string contains all digits, then only allow the request, otherwise return from here.
		if (isset($partial_matched_bytes_str) && $partial_matched_bytes_str != '' && (!preg_match('/^\d+$/',$partial_matched_bytes_str))) 
		{
			//As the post data of mode data is invalid, hence returning from here
			$commonfunctions_obj->bad_request_response("Invalid post data partial matched bytes $partial_matched_bytes in updateStatsResyncFromTarget.");
		}
		$destination_device_name_without_escape = $destination_device_name;
		/*Construct the params array to pass it to DAO method*/
		$params = array(
		'srcid'=>$src_host_id,
		'srcdevicename'=>$src_device_name,
		'destid'=>$destination_host_id,
		'destdevicename'=>$destination_device_name);
		$search_results = $commonfunctions_obj->find_by_pair_details($params);

		$src_device_name = $this->conn->sql_escape_string($src_device_name);
		$destination_device_name = $this->conn->sql_escape_string
		($destination_device_name);
		$return_status = STATUS_SUCCESS;

		/*If search result count as zero, then send the data mis match status*/
		if (count($search_results) == 0)
		{
			#Log the message here
			$return_status = AFFECTED_ROWS_FAILURE;
		}
		else
		{
			/*Collect the required data from the value object*/
			foreach ($search_results as $key=>$obj)
			{
				$resync_end_time = $obj['resyncEndTime'];
				$job_id = $obj['jobId'];
				$db_resync_offset = $obj['lastResyncOffset'];
				$db_fast_resync_unused = $obj['fastResyncUnused'];
				$db_fast_resync_matched = $obj['fastResyncMatched'];									
			}

			/*If jobid is not present in the database, then return the
			job id mismatch status*/
			if ($jobid && ($jobid != $job_id))
			{
				$return_status = JOBID_MISMATCH;
			}
			elseif  (0 == $resync_end_time)
			{
				$params = array(
						'destid'=>$destination_host_id,
						'destdevicename'=>$destination_device_name_without_escape);

				$dbupdate_status_flag = $this->resyncProgressUpdatesFromTarget(
				$unmatched_bytes,
				$partial_matched_bytes,
				$params,
				$db_resync_offset,
				$db_fast_resync_unused,
				$db_fast_resync_matched,
				$stats);

				/*If database update fails send the return value as 4*/
				if ($dbupdate_status_flag === FALSE)
				{
						$return_status = QUERY_EXECUTION_FAILURE;
				}
			}
			else
			{
				$return_status = INVALID_DB_STATE;
			}
		}
		return $return_status;
	}


	/*
	* Function Name: updateUnusedBytesFromTarget
	*
	* Description:
	* Updates lastResyncOffset and fastResyncUnused fields in srcLogicalVolumeDestLogicalVolume table
	* bytes unused value is send as part of the parameters. this is updated to fastResyncUnused without any modifications.
	* lastResyncOffset is updated of (lastResyncOffset field in db tbl)-(fastResyncUnused field in db tbl)+(bytes unused value) 
	* as bytes unused value is cumulative
	*
	* Parameters: 
	* source host identity, source device name, destination
	* host identity,destination device name, bytes unused and job id.
	*    
	* Return Value:
	*     Returns zero on success and non zero on failure. 
	*
	*/
	public function updateUnusedBytesFromTarget(
	$src_host_id,
	$src_device_name,
	$destination_host_id,
	$destination_device_name,
	$bytes_unused,
	$jobid)
	{
		global $REPLICATION_ROOT;
		$commonfunctions_obj = new commonfunctions();
		$return_status = STATUS_SUCCESS;

		/*Construct the params array to pass it to DAO method*/
		$params = array(
		'srcid'=>$src_host_id,
		'srcdevicename'=>$src_device_name,
		'destid'=>$destination_host_id,
		'destdevicename'=>$destination_device_name);
		
		$search_results = $commonfunctions_obj->find_by_pair_details($params);
		
		$src_device_name = $this->conn->sql_escape_string($src_device_name);
		$destination_device_name = $this->conn->sql_escape_string($destination_device_name);

		/*If search result count as zero, then send the data mis match status*/
		if (count($search_results) == 0)
		{
			#Log the message here
			$return_status = AFFECTED_ROWS_FAILURE;
		}
		else
		{        
			/*Collect the required data from the value object*/
			foreach ($search_results as $obj)
			{
				$resync_end_time = $obj['resyncEndTime'];;
				$job_id = $obj['jobId'];
			}

			/*If jobid is not present in the database, then return the
			job id mismatch status*/
			if ($jobid && ($jobid != $job_id))
			{
				$return_status = JOBID_MISMATCH;
			}
			elseif  (0 == $resync_end_time)
			{
				$resync_progress_bytes = "lastResyncOffset-fastResyncUnused+$bytes_unused";
				$sql = "UPDATE 
							srcLogicalVolumeDestLogicalVolume 
						SET 
							lastResyncOffset = $resync_progress_bytes,
							fastResyncUnused = $bytes_unused
						WHERE
							destinationHostId = '$destination_host_id' 
						AND 
							destinationDeviceName = '$destination_device_name'";
							
				$dbupdate_status_flag = $this->conn->sql_query($sql);			
				
				/*If database update fails send the return value as 4*/
				if ($dbupdate_status_flag === FALSE)
				{
					$return_status = QUERY_EXECUTION_FAILURE;
				}
			}
			else
			{
				$return_status = INVALID_DB_STATE;
			}
		}
		return $return_status;
	}

	/*
	* Function Name: resyncProgressUpdatesFromTarget
	*
	* Description:
	* Updates lastResyncOffset and fastResyncUnmatched fields in srcLogicalVolumeDestLogicalVolume table
	* bytes unmatched value is send as part of the parameters. this is updated to fastResyncUnmatched without any modifications.
	* lastResyncOffset is updated of (fastResyncMatched field in tbl)+(bytes unmatched value)+(partially matched bytes value) 
	* as bytes unused value is cumulative
	*
	* Parameters: 
	* unmatched bytes, partially matched bytes and params (params contain dest host id and dest device name),
	* database last resync offset value and IR stats.
	*    
	* Return Value:
	*     Returns zero on success and non zero on failure. 
	*
	*/
	private function resyncProgressUpdatesFromTarget(
	$unmatched_bytes,
	$partial_matched_bytes,
	$params,
	$db_resync_offset,
	$db_fast_resync_unused,
	$db_fast_resync_matched,
	$stats)
	{				
		$resync_progress_bytes = $db_fast_resync_unused+$db_fast_resync_matched+$unmatched_bytes+$partial_matched_bytes;
		$last_tm_change_set  = "";
		if ($db_resync_offset != $resync_progress_bytes)
		{	
			$last_tm_change_set = ", lastTMChange = now() ";
		}

		$sql = "UPDATE 
					srcLogicalVolumeDestLogicalVolume 
				SET 
					lastResyncOffset = ?,
					fastResyncUnmatched = ?,
					stats = ? $last_tm_change_set
				WHERE
					destinationHostId = ?
				AND 
					destinationDeviceName = ?";
						
		$dbupdate_status_flag = $this->conn->sql_query($sql, array($resync_progress_bytes, $unmatched_bytes, $stats, $params['destid'], $params['destdevicename']));
		
		//The parameterized approach update query return status as null from connection class method sql_query, if update query execution fails.
		if (is_null($dbupdate_status_flag))
		{
			return FALSE;
		}
		else
			return TRUE;
	}
}
?>
