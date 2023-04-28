<?php
/*
 *=================================================================
 * FILENAME
 *    VolumeProtection.php
 *
 * DESCRIPTION
 *    This script contains Volume Replication Related Functions.
 *    Earlier these functions were defined in functions.php,
 *    conn.php,pair_settings.php.
 *
 *
 * HISTORY
 *     <Date 1>  Author    Created.Pawan / Shrinivas .
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems
 *=================================================================
 */

class VolumeProtection
{
   /*
     $db_handle: database connection object
   */

    private $commonfunctions_obj;
    private $conn;
    private $db_handle;
	private $policy_obj;
	private $protection_plan_obj;
    /*
     * Function Name: VolumeProtection
     *
     * Description:
     *               Constructor
     *               It initializes the private variable
    */
    function __Construct()
    {
		global $conn_obj;
        $this->commonfunctions_obj = new CommonFunctions();
		$this->policy_obj =  new Policy();
		$this->protection_plan_obj =  new ProtectionPlan();
		$this->conn = $conn_obj;
        $this->db_handle = $this->conn->getConnectionHandle();
    }

    /*
     * Function Name: get_ret_log_policy_ruleids_for_deletion
     *
     * Description:
     *    gets all ruleId that are in retLogPolicy but not in srcLogicalVolumeDestLogicalVolume
     *	  as those can be deleted (kind of like a cascade delete)
     *
     * Parameters: N/A
     *
     * Return Value:
     *       ruleIds found: Returns string of ruleIds common separated
     *       ruleIds not found: returns empty string
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function get_ret_log_policy_ruleids_for_deletion()
    {
        $ruleIds = "";
        $rs = $this->conn->sql_query("select ".
                                     " r.ruleId ".
                                     "from  ".
                                     " retLogPolicy r  ".
                                     "left join (srcLogicalVolumeDestLogicalVolume sd)  ".
                                     "  on (r.ruleId = sd.ruleId)  ".
                                     "where ".
                                     " sd.ruleId is NULL ");
        if (!$rs) {
            // report error
            return $ruleIds;
        }

        if ($this->conn->sql_num_row($rs) > 0) {
            $row = $this->conn->sql_fetch_row($rs);        
            $ruleIds = "'$row[0]'";
            
            while ($row = $this->conn->sql_fetch_row($rs)) {
                $ruleIds = $ruleIds . ", '$row[0]'";
            }
        }

        return $ruleIds;
    }

    /*
     * Function Name: get_rules_ruleids_for_deletion
     *
     * Description:
     *    gets all ruleId that are in rules but not in srcLogicalVolumeDestLogicalVolume
     *	  as those can be deleted (kind of like a cascade delete)
     *
     * Parameters: N/A
     *
     * Return Value:
     *    Ret Value:
     *       ruleIds found: Returns string of ruleIds common separated
     *       ruleIds not found: returns empty string
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function get_rules_ruleids_for_deletion()
    {
        $ruleIds = "";
        $rs = $this->conn->sql_query("select ".
                                     " r.ruleId ".
                                     "from  ".
                                     " rules r  ".
                                     "left join (srcLogicalVolumeDestLogicalVolume sd)  ".
                                     "  on (r.ruleId = sd.ruleId)  ".
                                     "where ".
                                     " sd.ruleId is NULL ");
        if (!$rs) {
            // report error
            return $ruleIds;
        }

        $row = $this->conn->sql_fetch_row($rs);

        $ruleIds = "'$row[0]'";

        while ($row = $this->conn->sql_fetch_row($rs)) {
            $ruleIds = $ruleIds . ", '$row[0]'";
        }
        return $ruleIds;
    }
    
    
    /*
     * Function Name: get_all_ruleids_for_dest_as_array
     *
     * Description:
     *    Retrieve all ruleId from srcLogicalVolumeDestLogicalVolume table.
     *	 based upon the conditions.
     *
     * Parameters:
     *    Param 1 [IN]:$id
     *    Param 2 [IN]:$device
     *
     * Return Value:
     *    Ret Value: Returns Array
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function get_all_ruleids_for_dest_as_array($id, $device)
    {
        $ruleIds = array();
	 $query="SELECT
		    ruleId "."
                FROM
		    srcLogicalVolumeDestLogicalVolume "."
                WHERE
		    destinationHostId = '$id' AND
		    destinationDeviceName = '$device'";

        $result_set = $this->conn->sql_query($query);
        if (!$result_set)
	{
        /*
	    report error
	*/

            return $ruleIds;
        }

        while ($row = $this->conn->sql_fetch_object($result_set))
	{
            array_push($ruleIds, $row->ruleId);
        }

        return $ruleIds;
    }

    /*
     * Function Name: get_clusterids_as_array
     *
     * Description:
     *    Retrieve all clusterId from clusters table
     *	 based upon the conditions.
     *
     * Parameters:
     *    Param 1 [IN]:$id
     *    Param 2 [IN]:$device
     *
     * Return Value:
     *    Ret Value: Returns Array.
     *
     * Exceptions:
     *    DataBase Connection fails.
   */
    public function get_clusterids_as_array($id, $device)
    {

        $clusterIds = array();
	#6919
	$device = trim($device);
	#BUG 7192
	/* Construct proper windows path */
    $device =$this->get_unformat_dev_name($id,$device);
	$device = $this->commonfunctions_obj->verifyDatalogPath($device);

	/*Escape the win slashes*/
	$device = $this->conn->sql_escape_string($device);
        #BUG END 7192


	$query = "SELECT
	            c2.hostId " . "
                FROM
		    clusters c1,
		    clusters c2 " . "
                WHERE
		    c1.hostId = '$id' "."AND
                    c1.clusterGroupId = c2.clusterGroupId "."AND
                    c1.deviceName = c2.deviceName "."AND
                    c1.deviceName = '$device'";


        $result_set = $this->conn->sql_query($query);
		while ($row = $this->conn->sql_fetch_object($result_set))
	    {
			array_push($clusterIds, $row->hostId);
		}
        return $clusterIds;
    }

    /*
     * Function Name: current_tmid
     *
     * Description:
     *    Retrieve all tmId from logicalVolumes table
     *    based upon the conditions.
     *
     * Parameters:
     *     Param 1 [IN]:$id
     *     Param 2 [IN]:$device
     *
     * Return Value:
     *     Ret Value: Returns Scalar value.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function current_tmid($id, $device)
    {
        $query="SELECT
		    tmid "."
		FROM
		    logicalVolumes "."
		WHERE
		    hostId = '$id' AND
		    deviceName = '$device'";

	$result_set = $this->conn->sql_query($query);

        if (!$result_set || 0 == $this->conn->sql_num_row($result_set))
	{
	    /*
	    report error
	    */
	    return -1; // force the tmid to be updated
	}

	$row = $this->conn->sql_fetch_object($result_set);
	return $row->tmid;
    }

    /*
     * Function Name: will_be_one_to_n
     *
     * Description:
     *    Retrieve count from srcLogicalVolumeDestLogicalVolume table
     *    based upon the conditions.
     *
     * Parameters:
     *    Param 1 [IN]:$srcId
     *    Param 2 [IN]:$srcDevice
     *
     * Return Value:
     *    Ret Value: Returns Scalar value.
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
    public function will_be_one_to_n($srcId, $srcDevice)
    {
	$query="SELECT
		    count(*)
		FROM
		    srcLogicalVolumeDestLogicalVolume "."
                WHERE
		    sourceHostId = '$srcId' ". "AND
		    sourceDeviceName = '$srcDevice' ";

	$result_set = $this->conn->sql_query($query);
	if (!$result_set )
	{
	    /*
	    report error
	    */
	    return false;
	}
	else
	{
	    $row = $this->conn->sql_fetch_row($result_set );
	    return (0 != $row[0]);
	}
    }

    /*
     * Function Name: pair_exists
     *
     * Description:
     *    Retrieve count from srcLogicalVolumeDestLogicalVolume table
     *    based upon the conditions.
     *
     * Parameters:
     *    Param 1 [IN]:$srcId
     *    Param 2 [IN]:$srcDevice
     *    Param 3 [IN]:$dstId
     *    Param 4 [IN]:$dstDevice
     *
     * Return Value:
     *    Ret Value: Returns boolean value.
     *
    */
    public function pair_exists($source_host_id, $source_device_name, $destination_host_id, $destination_device_name)
    {
		$return_status = false;
		
		$sql = "SELECT
						count(*) as pairCount
					FROM
						srcLogicalVolumeDestLogicalVolume 
					WHERE
						sourceHostId = ? 
					AND
						sourceDeviceName = ? 
					AND
						destinationHostId = ? 
					AND
						destinationDeviceName = ?";
		$sql_params = array($source_host_id, $source_device_name, $destination_host_id, $destination_device_name);
		$result_set = $this->conn->sql_query($sql, $sql_params);
		
		if (count($result_set) > 0)
		{
			$row = $result_set[0];
			if ($row['pairCount'] > 0)
			{
				$return_status = true;
			}
		}
		
		return $return_status;
    }

    /*
     * Function Name: set_cluster_rule_ids
     *
     * Description:
     *    Retrieve count from srcLogicalVolumeDestLogicalVolume table
     *    based upon the conditions.
     *
     * Parameters:
     *    Param 1 [IN]:$clusterIds
     *    Param 2 [IN]:$srcDevice
     *
     * Return Value:
     *    Ret Value: Returns TRUE if pair exists else Returns FALSE.
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
    private function set_cluster_rule_ids($clusterIds, $srcDevice)
    {
		
       $cnt = count($clusterIds);	 
		if ($cnt > 0)
    	{
            $hostIdsIn = "'$clusterIds[0]'";
            for ($i = 1; $i < $cnt; $i++)
	    {
			$hostIdsIn = $hostIdsIn . ", '$clusterIds[$i]'";
	    }

	    $query="SELECT
			clusterruleId "."
		    FROM
			clusters "."
		    WHERE
			hostId
		    IN
			($hostIdsIn) AND
			deviceName = '$srcDevice' "."AND
			clusterruleId != 0";

	    $result_set = $this->conn->sql_query($query);
            if (!$result_set)
	    {
		/*
		report error
		*/
		return false;
	    }

	    $row = $this->conn->sql_fetch_object($result_set);
	
	    if ($row->clusterruleId != 0)
	    {
	        $clusterId = $row->clusterruleId;
	    }
	    else
	    {
	        $clusterId =$this->get_new_clusterruleId();
	    }

	    $query="UPDATE
		        clusters "."
		    SET
			clusterruleId = $clusterId "."
		    WHERE
			hostId
		    IN
			($hostIdsIn) AND
		        deviceName = '$srcDevice'";

	    if (!$this->conn->sql_query($query))
	    {
		/*
		report error
		*/
	        return false;
	    }
	}

	return true;
    }

    /*
     * Function Name: remove_files
     *
     * Description:
     *    Removes all files except those ending in .log for the given
     *    id and device.
     *
     * Parameters:
     *    Param 1 [IN]:$id
     *    Param 2 [IN]:$device
     *
     * Return Value:
     *    Ret Value: No return value.
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
    private function remove_files($id, $device)
    {
		global $ORIGINAL_REPLICATION_ROOT;
		global $FIND;
		global $XARGS;

		/*
		if id and and device are not null only this will execute
		*/

		$device = str_replace("\\", "/", $device);
		$device = str_replace(":", "", $device);

		if (($id) AND ($device))
		{
			$dir =  $ORIGINAL_REPLICATION_ROOT .  '/' .  $id. '/' . $device. '/';

			$this->delete_files( $dir, "*.dat.gz" ) ;
			$this->delete_files( $dir, "*.dat" ) ;
			$this->delete_files( $dir, "*.Map" ) ;

			############### 9448 REMOVED dirty_block_cache.txt file WHEN PAIR GETS RESYNC RESTART ##########
			$source_compatibilityno =  $this->commonfunctions_obj->get_compatibility($id);

			if($source_compatibilityno >= 550000)
			{
				$this->delete_files( $dir, "dirty_block_cache.txt" ) ;
			}
			############### 9448 ##########
		}
    }

    /*
     * Function Name: delete_files
     *
     * Description:
     *    Deletes the file.
     *
     * Parameters:
     *     Param 1 [IN]:$path
     *     Param 2 [IN]:$match
     *
     * Return Value:
     *     Ret Value: No return value.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function delete_files($path, $match)
    {
	global $logging_obj;
	$logging_obj->my_error_handler("DEBUG","delete_files:: path: $path\n");
	$dirs = glob($path."*");
	/*
	$dirs_data = var_export($dirs,true);
	debugLog("\n dirs data is::".$dirs_data."\n");
	*/

	$files = glob($path.$match);
	/*
	$files_data = var_export($files,true);
	debugLog("\n files data::".$files_data."\n");
	*/

	/*
	Below is the type cast instruction added by Srinivas Patnana
	to fix #3051.
	foreach control structure is not interpreting $files as an array,
	which is returned from glob function.
	*/
	$files = (array) $files;
	foreach($files as $file)
	{
	    if(is_file($file))
	    {
			$logging_obj->my_error_handler("DEBUG","delete_files:: file: $file\n");
		@unlink($file);
	    }
	}

	/*
	Below is the type cast instruction added by Srinivas Patnana
	to fix #3051.
	foreach control structure is not interpreting $dirs as an array,
	which is returned from glob function.
	*/
	$dirs = (array) $dirs;
	foreach($dirs as $dir)
	{
	    if(is_dir($dir))
	    {
		$dir = basename($dir) . "/";
		$this->delete_files($path.$dir,$match);
	    }
	}
    }

    /*
     * Function Name: remove_source_files
     *
     * Description:
     *    Removes all files except those ending in .log for the given
     *    id and device name including any cluster (does not deal with 1-to-N)
     *
     * Parameters:
     *    Param 1 [IN]:$id
     *    Param 2 [IN]:device
     *
     * Return Value:
     *    Ret Value: No return value.
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
    private function remove_source_files($id, $device)
    {
       $clusterIds =$this->get_clusterids_as_array($id, $device);

       if (count($clusterIds) > 0)
    	{
            foreach ($clusterIds as $id)
	    {
                $this->remove_files($id, $device);
	    }
	}
	else
	{
	    $this->remove_files($id, $device);
	}
    }

    /*
     * Function Name: remove_one_to_one_files
     *
     * Description:
     *    This function is responsible for forking multiple processes that
     *    performs the following actions.
     *
     *    Removes all files for a 1-to-1 setup except those ending in .log
     *	  including any cluster clusters.
     *
     * Parameters:
     *    Param 1 [IN]:$srcId
     *    Param 2 [IN]:$srcDevice
     *    Param 3 [IN]:$dstId
     *    Param 4 [IN]:$dstDevice
     *
     * Return Value:
     *    Ret Value: No return value.
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
    public function remove_one_to_one_files($srcId, $srcDevice, $dstId,
				     $dstDevice)
    {
		global $logging_obj;
		#$logging_obj->my_error_handler("DEBUG","remove_one_to_one_files:: srcId: $srcId, srcDevice: $srcDevice \n");
		#Fix for #13609 
		$is1toN = $this->is_one_to_many_by_source($srcId, $srcDevice);
		#$logging_obj->my_error_handler("DEBUG","remove_one_to_one_files:: is1toN: $is1toN \n");
		if(!$is1toN)
		{
			#$logging_obj->my_error_handler("DEBUG","remove_one_to_one_files:: callg remove_source_files \n");
			$this->remove_source_files($srcId, $srcDevice);
		}
        $this->remove_files($dstId, $dstDevice);
    }

    /*
     * Function Name: remove_one_to_n_files
     *
     * Description:
     *    Removes all files for all cluster 1-to-N pairs except those
     *    ending in .log including any cluster clusters
     *
     * Parameters:
     *     Param 1 [IN]:$srcId
     *     Param 2 [IN]:$srcDevice
     *
     * Return Value:
     *     Ret Value: No return value.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function remove_one_to_n_files($srcId, $srcDevice)
    {
        /*
	need to get all sources and targets and remove all files
        that maybe present
	*/
        $this->remove_source_files($srcId, $srcDevice);

        /*
	now get dest
	*/

	$query="SELECT
		    destinationHostId, destinationDeviceName ". "
		FROM
		    srcLogicalVolumeDestLogicalVolume "."
		WHERE
		    sourceHostId='$srcId' "."AND
		    sourceDeviceName='$srcDevice'";

	$result_set = $this->conn->sql_query ($query);

	if (!$result_set)
	{
	    /*
	    report error
	    */
	}
	else
	{
	    while ($row = $this->conn->sql_fetch_object($result_set))
	    {
                $this->remove_files($row->destinationHostId,
		                    $row->destinationDeviceName);
            }
	}
    }

    /*
     * Function Name: update_one_to_n_resync
     *
     * Description:
     *    This function is responsible for forking multiple processes that
     *    performs the following actions.
     *
     *    Updates all cluster 1-to-N pairs to resync as needed
     *
     * Parameters:
     *     Param 1 [IN]:$srcId
     *     Param 2 [IN]:$srcDevice
     *     Param 3 [IN]:$dstId
     *     Param 4 [IN]:$dstDevice
     *     Param 5 [IN]:$fast
     *
     * Return Value:
     *     Ret Value: Returns boolean value.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function update_one_to_n_resync($srcId, $srcDevice, $dstId,
				    $dstDevice, $fast)
    {
		global $logging_obj;
		#$logging_obj->my_error_handler("DEBUG","update_one_to_n_resync:: resyncversion value getting is $fast\n");
        /*
	get cluster ids if any
	*/
        $clusterIds =$this->get_clusterids_as_array($srcId, $srcDevice);

	/*
	set up all the source ids
	*/
	$cnt = count($clusterIds);

	if ($cnt > 0)
	{
	    $srcIdsIn = "'$clusterIds[0]'";
	    $whereSrcForLogicalVolumes ="(hostId = '$clusterIds[0]' and
		                          deviceName = '$srcDevice')";
	    for ($i = 1; $i < $cnt; $i++)
	    {
	        $srcIdsIn = $srcIdsIn . ", '$clusterIds[$i]'";
	        $whereSrcForLogicalVolumes =$whereSrcForLogicalVolumes . " or
			                    (hostId = '$clusterIds[$i]'
					     and
					     deviceName = '$srcDevice')";
	    }
	}
	else
	{
	    $srcIdsIn = "'$srcId'";
	    $whereSrcForLogicalVolumes ="(hostId = '$srcId' and
		                          deviceName = '$srcDevice')";
	}

	if (!$fast)
	{
	    /*
	    need to clean all files that are in transit as we are
	    resetting all pairs to resync
	    */
	    $this->remove_one_to_n_files($srcId, $srcDevice);

	    /*
	    the rule is if one 1-to-N pair wants to use offload,
	    then all pairs must be resynced from the start in offload
	    together no matter what state they are currently in.using
	    just the source ids here will cover all cases: 1-to-1,
	    1-to-N, cluster, and cluster 1-to-N.
	    */

	    $query="UPDATE
			srcLogicalVolumeDestLogicalVolume " . "
		    SET
		    	resyncVersion = 0,
			shouldResync = 1,
			remainingDifferentialBytes = 0,
			remainingResyncBytes = 0, "."
			currentRPOTime = 0,
			isResync = 1,
			resyncStartTime = now(),
			resyncEndTime = 0, " . "
			lastResyncOffset = 0,
			fullSyncBytesSent = 0,
			fastResyncMatched = 0,
			fastResyncUnmatched=0, "."
			remainingQuasiDiffBytes = 0,
			quasidiffStarttime = 0,
			quasidiffEndtime = 0, "."
			resyncStartTagtime = 0,
			resyncEndTagtime = 0,
			isQuasiflag = 0 "."
		    WHERE
			sourceHostId
		    IN
			($srcIdsIn) and
			sourceDeviceName = '$srcDevice'";

	    if (!$this->conn->sql_query($query))
	    {
	        /*
		report error
		*/
		return false;
	    }

	    /*
	    get destinations
	    */

	    $query="SELECT
		        destinationHostId,
			destinationDeviceName "."
		    FROM
			srcLogicalVolumeDestLogicalVolume "."
		    WHERE
			sourceHostId='$srcId' "."AND
			sourceDeviceName='$srcDevice'";

	    $result_set_destIds = $this->conn->sql_query ($query);

	    if (!$result_set_destIds)
	    {
	        /*
		report error
		*/
	        return false;
	    }

	    if ($this->conn->sql_num_row($result_set_destIds) == 0)
	    {
		/*
		report error
		*/
		return false;
	    }

	    $row = $this->conn->sql_fetch_object($result_set_destIds);
	    $whereDstForLogicalVolumes = "(hostId = '$row->destinationHostId'
					   and  deviceName = '".
					   $this->conn->sql_escape_string
					   ($row->destinationDeviceName)."')";

	    while ($row = $this->conn->sql_fetch_row($result_set_destIds))
	    {
	        $whereDstForLogicalVolumes = $whereDstForLogicalVolumes .
				              "or (hostId =
					      '$row->destinationHostId'
			                      and  deviceName = '".
					      $this->conn->sql_escape_string
					      ($row->destinationDeviceName)
					      ."')";
	    }

	    /*
	    now set all volumes for all host ids to do reysnc
	    need both source and dest ids to cover all cases:
	    1-to-1, 1-to-N, cluster, and cluster 1-to-N
	    */

	    $query="UPDATE
		        logicalVolumes " . "
		    SET
			doResync = 1 " . "
		    WHERE
			$whereSrcForLogicalVolumes or
			$whereDstForLogicalVolumes";

	    if (!$this->conn->sql_query($query))
	    {
		/*
		report error
		*/
		return false;
	    }
	}
	else
	{
	    /*
	    adding a fast sync job, make sure the other 1-to-N jobs are
	    set to offload note we just need to reset the resyncVersion
	    we don't want them to actually resync. we should only get
	    here if no other 1-to-N is resyncing using just the source
	    ids here will cover all cases: 1-to-1, 1-to-N, cluster,
	    and cluster 1-to-N make sure not to reset dest device
	    that wants to do fast sync
	    */
		$source_compatibilityno =  $this->commonfunctions_obj->get_compatibility($srcId);
		$target_compatibilityno =  $this->commonfunctions_obj->get_compatibility($dstId);

		/* 4369
		 * Added check since for 5.5 version onwards adding fast sync job
		 * should not switch other 1-N jobs to offload  */
		if ( ($source_compatibilityno < 550000) ||
                        	($target_compatibilityno < 550000))
		{
			#$logging_obj->my_error_handler("DEBUG","update_one_to_n_resync:: switching fast to offload for lower version agents when another 1-N fast pair is configured \n");
	    $query="UPDATE
			srcLogicalVolumeDestLogicalVolume "."
		    SET
	                resyncVersion = 0 "."
		    WHERE
			sourceHostId
		    IN
			($srcIdsIn) AND
			sourceDeviceName = '$srcDevice' "."AND
			destinationDeviceName != '$dstDevice'";

	    if (!$this->conn->sql_query ($query))
	    {
		/*
		report error
		*/
		return false;
	    }
		}
	}
	return true;
    }

    /*
     * Function Name: delete_ruleids
     * 
     * Description:
     *    Deletes ruleids for the Configured Pair.
     *
     * Parameters:
     *
     * Return Value:
     *     Ret Value: No return value.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function delete_ruleids()
    {
        $ruleIds = $this->get_rules_ruleids_for_deletion($db);
        if (strlen($ruleIds) > 0) {
            $this->conn->sql_query("delete from ".
                                   "  rules ".
                                   "where ".
                                   "  ruleId in ($ruleIds)");
        }
    }

	
	/*
     * Function Name: delete_retention
     *
     * Description:
     *    Removes all retention information as needed
     *
     * Parameters:
     *     Param 1 [IN]:$dstId
     *
     * Return Value:
     *     Ret Value: No return value.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function delete_retention($dstId,$pairs="")
    {
        global $logging_obj , $sparseretention_obj;
        if($pairs != "")
        {
            $ruleIds = $this->get_retention_ruleids($pairs);
        }
        else
        {
            $ruleIds = $this->get_ret_log_policy_ruleids_for_deletion();
        }
       
        if ("" == $ruleIds) 
        {
            return;
        }
        /*
                        see if retention is setup we can just get one
                        of the entries even if it is cluster because they
                        will have the same meta  and log dirs
                    */
        $query="SELECT
                    metafilepath,
                    retPolicyType
                FROM
                    retLogPolicy 
                WHERE
                    ruleId in ($ruleIds)";
        
        #$logging_obj->my_error_handler("DEBUG","delete_retention ruleIds :: $query \n");
        $result_set = $this->conn->sql_query($query);
        if (!$result_set)
        {
            /*
                                report error
                            */
            return;
        }
        if ($this->conn->sql_num_row($result_set) > 0)
        {
            $row = $this->conn->sql_fetch_object($result_set);
            $retPolicyType = $row->retPolicyType;
            #$logging_obj->my_error_handler("DEBUG","delete_retention policy Type :: $retPolicyType \n");
            /*
                            FIXME: this is drive letter specific code
                            */
            $metaParts = explode(":",$row->metafilepath);
            if($retPolicyType == 0 || $retPolicyType == 2)
            {
                $sql = "SELECT
                            DISTINCT ret_space.storagePath,
                            ret_space.storageDeviceName
                        FROM
                            retentionSpacePolicy ret_space ,
                            retLogPolicy ret_pol
                        WHERE
                            ret_space.retId = ret_pol.retId
                        AND
                            ret_pol.ruleid IN($ruleIds)";
            }
            else
            {
                $sql = "SELECT
                            DISTINCT ret_event.storagePath,
                            ret_event.storageDeviceName
                        FROM
                            retentionEventPolicy ret_event ,
                            retentionWindow ret_win,
                            retLogPolicy ret_pol
                        WHERE
                            ret_event.Id = ret_win.Id
                        AND
                            ret_win.retId = ret_pol.retId
                        AND
                            ret_pol.ruleid IN($ruleIds)";
            }
            #$logging_obj->my_error_handler("DEBUG","delete_retention sql:: $sql \n");
            $sql_result_set = $this->conn->sql_query($sql);
            $fetch_data = $this->conn->sql_fetch_array($sql_result_set);
            $metaVolume = $fetch_data['storagePath'];
            $logParts = $fetch_data['storageDeviceName'];
            $logParts = explode(":",$logParts);
            $logVolume = $fetch_data['storageDeviceName'];
            /*
                                have retention
                                delete from retError
                            */

            $query="DELETE
                    FROM
                        retError "."
                    WHERE
                        ruleId
                    IN
                    ($ruleIds)";
            #$logging_obj->my_error_handler("DEBUG","delete_retention query:: $query \n");

            if (!$this->conn->sql_query($query))
            {
            /*
                                    report error
                                */
            }

            /*
                            delete from retLogPolicy
                            */

            $query="DELETE
                    FROM
                        retLogPolicy "."
                    WHERE
                    ruleid
                    IN
                    ($ruleIds)";
            #$logging_obj->my_error_handler("DEBUG","delete_retention query:: $query \n");
            if (!$this->conn->sql_query($query))
            {
                /*
                                        report error
                                        */
            }
			
        $log_device_name = array();

        $query ="SELECT
                    DISTINCT ret_event.storagePath,
                    ret_event.storageDeviceName
                FROM
                    retentionEventPolicy ret_event ,
                    retentionWindow ret_win,
                    retLogPolicy ret_pol,
                    srcLogicalVolumeDestLogicalVolume src
                WHERE
                    ret_event.Id = ret_win.Id
                AND
                    ret_win.retId = ret_pol.retId
                AND
                    ret_pol.ruleid = src.ruleId
                AND
                    src.destinationHostId='$dstId'";

        #$logging_obj->my_error_handler("DEBUG","delete_retention query::$query\n");

	    $event_result_set =  $this->conn->sql_query($query);
        while ($event_row = $this->conn->sql_fetch_object($event_result_set))
        {
            $log_device_name[] = $event_row->storageDeviceName;
        }

        $query ="SELECT
                    DISTINCT ret_space.storagePath,
                    ret_space.storageDeviceName
                FROM
                    retentionSpacePolicy ret_space ,
                    retLogPolicy ret_pol,
                    srcLogicalVolumeDestLogicalVolume src
                WHERE
                    ret_space.retId = ret_pol.retId
                AND
                    ret_pol.ruleid = src.ruleId
                AND
                    src.destinationHostId='$dstId'";

        #$logging_obj->my_error_handler("DEBUG","delete_retention query::$query\n");
	    $space_result_set =  $this->conn->sql_query($query);
        while ($space_row = $this->conn->sql_fetch_object($space_result_set))
        {
            $log_device_name[] = $space_row->storageDeviceName;
        }

	    if (!$result_set)
	    {
	        /*
                                report error
                                */
	        return;
	    }

        for($i=0;$i<count($log_device_name);$i++)
        {
			$otherMetaVolumes = $log_device_name[$i];
			if ($metaVolume == $otherMetaVolumes)
			{
			    $metaVolume = "";
			}
            $otherLogParts = $log_device_name[$i];
			if ($logVolume == $otherLogParts)
			{
			    $logVolume = "";
			}
	    }


	    $whereClause = "";
	    if (0 != strlen($metaVolume))
	    {

		$whereClause = "(hostId = '$dstId' and (
		                  deviceName = '".
				 $this->conn->sql_escape_string ($metaVolume).
				 "' or mountPoint = '".
				 $this->conn->sql_escape_string ($logVolume)."'))";
	    }

	    if (0 != strlen($logVolume) && $logVolume != $metaVolume)
	    {
		    if (0 != strlen($whereClause))
			{
	            $whereClause = $whereClause . " or ";
	        }

	        $whereClause = "(hostId = '$dstId' and (
		                  deviceName = '".
				 $this->conn->sql_escape_string ($logVolume).
				 "' or mountPoint = '".
				 $this->conn->sql_escape_string ($logVolume)."'))";
	    }

        if($logVolume != "" && $metaVolume != "")
		{
	    $query="UPDATE
		        logicalVolumes "."
		    SET
			deviceFlagInUse = 0 "."
		    WHERE
			$whereClause";
		}
	    if (!$this->conn->sqL_query($query))
	    {
	        /*
		report error
		*/
	    }
        
        if($pairs != "")
        {
            $this->reset_media_retention($ruleIds);  
        }
	}
	}

	/*Get the ruleids for configured pairs*/
    private function get_retention_ruleids($pairs)
    {
        $str="";
        $varPair = explode("=", $pairs);
        $varHost = explode(",", $varPair[0]);
        $is_device_clustered = $this->is_device_clustered($varHost[0], $varPair[1]);
        if($is_device_clustered)
        {
            $source_host_ids =$this->get_clusterids_as_array($varHost[0],$varPair[1]);
        }
        else
        {   
            $source_host_ids = array_unique (explode (',', $varHost[0]));
        }
        foreach($source_host_ids as $key=>$value)
        {	
            $source_device_name =$this->get_unformat_dev_name($value,$varPair[1]);
            $dest_device_name =$this->get_unformat_dev_name($varPair[2],$varPair[3]);	    
            $ruleIdArray[] =$this->get_ruleId($value, $source_device_name, $varPair[2],$dest_device_name);
        }
        $str=implode(",",$ruleIdArray);
        return ($str);
    }
	
	/*Reset media retention on disable of media retention*/
    private function reset_media_retention($ruleids)
    {
        $sql = "UPDATE 
					srcLogicalVolumeDestLogicalVolume
				SET 
					mediaretent = 0									
				WHERE 
					ruleId IN($ruleids)";
        $result_set = $this->conn->sql_query($sql);
    }
	
    /*
     * Function Name: clear_logicalVolume_pair_settings
     *
     * Description:
     *    Clears settings in logicalVolume that only need
     *    to be set when the volume is part of a pair
     *
     * Parameters:
     *    Param 1 [IN]:$srcId
     *    Param 2 [IN]:$srcDevice
     *    Param 3 [IN]:$dstId
     *    Param 4 [IN]:$dstDevice
     *    Param 5 [IN]:$clearDps
     *
     * Return Value:
     *    Ret Value: No returns value.
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
    private function clear_logicalVolume_pair_settings($srcId, $srcDevice, $dstId,
						      $dstDevice, $clearDps,
                                                      $action)
    {
	global $MAX_DIFF_FILES_THRESHOLD,$logging_obj;
	$logging_obj->my_error_handler("DEBUG","clear_logicalVolume_pair_settings:: srcId: $srcId, srcDevice: $srcDevice, dstId: $dstId, dstDevice: $dstDevice, clearDps:$clearDps, action: $action \n");
	$updateWhereClause = "";

	/*
	this pair was resyncing check which doResync flags need to be
	cleared note we don't have to worry about checking for clusters at
	this point as the check is for 0 pairs. if there is a cluster
	involved then at least 1 of the nodes will show up if it
	meets these conditions and what ever 1 is doing the other
	is doing too.
	*/

	$query="SELECT count(*) FROM srcLogicalVolumeDestLogicalVolume ".
                      "WHERE sourceHostId = '$srcId' AND ".
                      " sourceDeviceName = '$srcDevice'";

	$rs = $this->conn->sql_query($query);

	$row = $this->conn->sql_fetch_row($rs);

	if (0 == $row[0])
	{
	    /*
	    no other resync jobs using the same source so clear source
	    and target doResync
	    */
	    $clusterIds =$this->get_clusterids_as_array($srcId, $srcDevice);
	    $cnt = count($clusterIds);
	    if ($cnt > 0)
	    {
			/*
				 * #6861 - Added the if condition to get that cluster node
				 *         hostId, which we have deleted from clusters.
				 *		This bolck only executes whenever we delete the cluster node.
				 */
				if (0 == strcmp($action, 'node_deleted'))
				{
					$clusterIdsIn = "'$srcId'";
				}
				else
				{
					$clusterIdsIn = "'$clusterIds[0]'";
					for ($i = 1; $i < $cnt; $i++)
					{
						$clusterIdsIn = $clusterIdsIn . ", '$clusterIds[$i]'";
					}
				}
				$srcWhereClause="hostId in ($clusterIdsIn) and
							     deviceName = '$srcDevice'";
		}
	    else
	    {
		$srcWhereClause = "hostId = '$srcId' and
					     deviceName = '$srcDevice'";
	    }

	    $setValues="doResync = 0, tmId = 0, deviceFlagInUse = 0,
			readwritemode = 0, farLineProtected = 0,
			maxDiffFilesThreshold = NULL ";

	    if ($clearDps)
	    {
	        $setValues = $setValues . ", dpsId = ''";
	    }

	    $query="UPDATE
			logicalVolumes "."
		    SET
			$setValues "."
                    WHERE
			$srcWhereClause";
		$logging_obj->my_error_handler("DEBUG","clear_logicalVolume_pair_settings:: query: $query \n");
	    if (!$this->conn->sql_query($query))
	    {
	    }
	}
        else
	{
	    /*
	    other resync jobs using the same source (1-to-N) only clear
	    the destination flags check if source doResync needs to be
	    cleared in case the deleted pair was resyncing and none of the
	    remaining pairs are resyncing note we don't have to worry about
	    checking for clusters at this point as the check is for 0
            pairs. if there is a cluster involved then at least 1 of the
            nodes will show up if it meets these conditions and what ever 1
            is doing the other is doing too.
	    */

	    $query="SELECT
			count(*)
		    FROM
			srcLogicalVolumeDestLogicalVolume "."
                    WHERE
			sourceHostId = '$srcId' AND
			sourceDeviceName = '$srcDevice' AND
			isResync = 1";

	    $rs = $this->conn->sql_query($query);
	    if (!$rs)
	    {
            }
	    else
	    {
		$row = $this->conn->sql_fetch_row($rs);
		if (0 == $row[0])
		{
		    /*
		    need to clear source doResync as no remaining pairs are
		    resyncing note here we need to deal with clusters to make
		    sure both nodes get there source volume doResync
		    flag cleared
		    */
		    $clusterIds=$this->get_clusterids_as_array($srcId,
							       $srcDevice,
							       $this->
							       db_handle);
		    $cnt = count($clusterIds);

		    if ($cnt > 0)
		    {
		        $clusterIdsIn = "'$clusterIds[0]'";
		        for ($i = 1; $i < $cnt; $i++)
		        {
		            $clusterIdsIn = $clusterIdsIn . ",
				               '$clusterIds[$i]'";
		        }
		        $srcWhereClause="hostId in ($clusterIdsIn) and
				       deviceName = '$srcDevice'";
		    }
		    else
		    {
		        $srcWhereClause = "hostId = '$srcId' and
					       deviceName = '$srcDevice'";
		    }

		    $query="UPDATE
		                logicalVolumes "."
		            SET
		        	    doResync = 0 "."
		            WHERE $srcWhereClause";
			$logging_obj->my_error_handler("DEBUG","clear_logicalVolume_pair_settings:: in else blk query: $query \n");
		    if (!$this->conn->sql_query($query))
		    {
		    }
		}
	    }
	}
    /*
	  * This is target pool
	  */
	$tgtIdsIn = "'$dstId'";
	if($check_xen = $this->check_xen_server($dstId))
	{
		$clusterTgtIds =$this->get_clusterids_as_array($dstId, $dstDevice);
		$countTgtIds = count($clusterTgtIds);
		if($countTgtIds >0)
		{
			$tgtIdsIn = "'$clusterTgtIds[0]'";
			for($i=1;$i<$cnt;$i++)
			{
				$tgtIdsIn = $tgtIdsIn . ", '$clusterTgtIds[$i]'";
			}
		}
		else
		{
			$tgtIdsIn = "'$dstId'";
		}

	}
	$dstWhereClause = "hostId in ($tgtIdsIn) and
			    deviceName = '$dstDevice'";
	/*$dstWhereClause = "hostId = '$dstId' and
			    deviceName = '$dstDevice'";*/

	$setValues = "doResync = 0, tmId = 0, deviceFlagInUse = 0, 
			dataSizeOnDisk = 0, diskSpaceSavedByThinprovision = 0, 
			diskSpaceSavedByCompression = 0";

	$query="SELECT
		    visible,
		    readwritemode "."
		FROM
		    logicalVolumes "."
		WHERE
		    $dstWhereClause";

	$rs = $this->conn->sql_query($query);
	if (!$rs)
	{
	}
	else
	{
	    $row = $this->conn->sql_fetch_row($rs);
	    if (1 == $row[0] && 2 == $row[1])
	    {
	        $setValues = $setValues . ", readwritemode = 0,
	                                     farLineProtected = 0";
	    }
	}

	if ($clearDps)
	{
	    $setValues = $setValues . ", dpsId = ''";
	}

	$query="UPDATE
		    logicalVolumes "."
		SET
		    $setValues "."
		WHERE
		    $dstWhereClause";
	$logging_obj->my_error_handler("DEBUG","clear_logicalVolume_pair_settings:: at end  query: $query \n");
	if (!$this->conn->sql_query($query))
	{
	}
	
	$query="DELETE 
			FROM 
				logicalVolumes 
			WHERE 
				isImpersonate = 1 
			AND 
				$dstWhereClause";
	$logging_obj->my_error_handler("INFO",array("Query"=>$query),Log::BOTH);
	if (!$this->conn->sql_query($query))
	{
		$logging_obj->my_error_handler("INFO","clear_logicalVolume_pair_settings:: at end  query: $query execution failed. \n");
	}
    }

    /*
     * Function Name: reassign_primary_pair
     *
     * Description:
     *    This function is responsible for forking multiple processes that
     *    performs the following actions.
     *
     *    Sets a pair to be the primary pair
     *
     * Parameters:
     *     Param 1 [IN]:$id
     *     Param 2 [IN]:$device
     *
     * Return Value:
     *     Ret Value: No returns value.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function reassign_primary_pair($id, $device)
    {
        /*
		check if there is a primary pair already set
		*/

		$query="SELECT
					count(*) "."
			FROM
				srcLogicalVolumeDestLogicalVolume "."
			WHERE
				sourceHostId = '$id' AND
				sourceDeviceName = '$device' "."AND
				oneToManySource = 1 "."limit 1";

		$result_set = $this->conn->sql_query($query);
		if (!$result_set)
		{
			return;
		}

		$row = $this->conn->sql_fetch_row($result_set);
		if (0 == $row[0])
		{
			$query="SELECT
				destinationHostId,
				destinationDeviceName "."
				FROM
					srcLogicalVolumeDestLogicalVolume "."
				WHERE
				sourceHostId = '$id' AND
				sourceDeviceName = '$device' "."AND
				oneToManySource = 0 "."limit 1";

			$result_set = $this->conn->sql_query($query);
			if (!$result_set)
			{
					return;
			}
			$row = $this->conn->sql_fetch_object($result_set);

			$query="UPDATE
					srcLogicalVolumeDestLogicalVolume "."
				SET
				oneToManySource = 1 "."
				WHERE
				destinationHostId = '$row->destinationHostId'
				AND
				destinationDeviceName = '".
				$this->conn->sql_escape_string
				($row->destinationDeviceName)."' ";

			$this->conn->sql_query($query);

		}
    }

    /*
     * Function Name: restart_resync
     *
     * Description:
     *    Restarts resync for the pair matching the ruleId using the resyncVer
     *    handles all cases 1-to-1, 1-to-n, cluster, cluster 1-to-n
     *    (incluindg forcing all pairs in 1-to-n to resync if needed)
     *
     * Parameters:
     *    Param 1 [IN]:$ruleId
     *    Param 2 [IN]:$resyncVer
     *
     * Return Value:
     *    Ret Value: Returns boolean value.
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
    private function restart_resync($ruleId, $resyncVer=0)
    {
		global $RESTART_RESYNC_CLEANUP;
		
		global $RESYNC_PENDING;
		global $RESYNC_NEEDED_UI,$MIRROR_SETUP_PENDING_RESYNC_CLEARED;
		global $RESYNC_MUST_REQUIRED, $RESYNC_NEEDED;
		global $PROTECTED, $REBOOT_PENDING, $logging_obj;
		
		$sql = "SELECT
					sourceHostId,
					sourceDeviceName,
					destinationHostId,
					destinationDeviceName,
					isResync,
					shouldResync,
					Phy_Lunid,
					isQuasiflag,
					planId,
					pairId,
					executionState,
					restartResyncCounter
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					ruleId = ? ";
		
		$rs=$this->conn->sql_query($sql, array($ruleId));
		$src_id_list = array();
		foreach($rs as $key=>$val)
		{
			$src_id_list[] = $val['sourceHostId'];
            $src_id = $val['sourceHostId'];
			$src_device = $val['sourceDeviceName'];
			$dest_id = $val['destinationHostId'];
			$dest_device = $val['destinationDeviceName'];
			$sanLunId = $val['Phy_Lunid'];
			$pairId = $val['pairId'];
			$execution_state = $val['executionState'];
			$shouldResyncFlag = $val['shouldResync'];
			$restartResyncCounter = $val['restartResyncCounter'];
			$restartResyncCounter = ($restartResyncCounter + 1);
		}
		
		$clusterid_sql = "select DISTINCT clusterId from clusters where hostId = ? AND deviceName = ?";
		$clusterid_result = $this->conn->sql_query($clusterid_sql, array($src_id, $src_device));							
		if (count($clusterid_result) > 0)
		{
			$cluster_id = $clusterid_result[0]['clusterId'];
			$host_sql = "select DISTINCT hostId from clusters where clusterId = ? and hostId != ?";
			$hostid_result = $this->conn->sql_query($host_sql, array($cluster_id, $src_id));
			foreach($hostid_result as $key => $value)
			{
				$src_id_list[] = $value['hostId'];
			}			 
		}				
		$src_id_list = array_unique($src_id_list);		
		$src_id_list_str = implode(",",$src_id_list);
		
		$setValues = "jobId='0'";
		if($execution_state == 4)
		{
			$setValues = $setValues.", executionState = 1";
		}
		
		$pairType = 0;
		if($sanLunId != "" AND $sanLunId != '0')
		{
           	$pairType = $this->get_pair_type($sanLunId); // pairType 0=host,1=fabric,2=prism, 3=array
			
			# For UI action, control comes here for Fabric
			$logging_obj->my_error_handler("DEBUG","resyncUI".$resync_ui."\n");

			
			$sqlSrc="SELECT lun_state 
					 FROM 
							srcLogicalVolumeDestLogicalVolume 
					  WHERE
							pairId = ?";
			$resultSqlSrc = $this->conn->sql_query($sqlSrc, array($pairId));
			$logging_obj->my_error_handler('INFO',"update resync-->$sqlSrc");
			
			foreach($resultSqlSrc as $$resultSqlSrcKey => $resultSqlSrcVal)
			{
				$lun_state = $resultSqlSrcVal['lun_state'];
				$logging_obj->my_error_handler('INFO',"update resync-->lun_state:$lun_state");	
				if($lun_state != $PROTECTED)
				{
					return;
				}
			}
			$logging_obj->my_error_handler('INFO',"update resync-->lun_state:$lun_state");	
			/*
			* this condition to check if srcLogicalVolumeDestLogicalVolume's pair was
			* in diif sync and resync set from the UI  after PS reported reboot event 
			* we need to reset pair's isResync,isQuasiflag and resyncStartTime.
			*/

			$updateResyncInfo="UPDATE
						srcLogicalVolumeDestLogicalVolume
					   SET
							isResync = 1,
						isQuasiflag  = 0,
						resyncStartTime = now()
					   WHERE
							pairId = ? AND
						lun_state IN ($REBOOT_PENDING)";

			$updateResyncInfoResult = $this->conn->sql_query($updateResyncInfo, array($pairId));
					

			#BUG 6853 #13434 Fix 
			#10334 Fix, reseting resyncSetCxtimestamp on restart of the pair
			  $setValues =  $setValues . ", shouldResync = 3,
							 resyncCounter = 0";

			

			if($pairType == 1)
			{
				# Don't set Quasi values here
				$update_sanLuns = "UPDATE sanLuns SET resyncRequired = $RESYNC_NEEDED_UI WHERE sanLunId = ? AND NOT resyncRequired IN ($RESYNC_NEEDED_UI, $RESYNC_MUST_REQUIRED)";
				if (!$this->conn->sql_query($update_sanLuns, array($sanLunId)))
				{
					return false;
				}

				$update_frBindingNexus = "UPDATE frBindingNexus SET state = $RESYNC_PENDING WHERE sanLunId = ? AND state = $PROTECTED AND processServerId IN (SELECT sourceHostId FROM srcLogicalVolumeDestLogicalVolume WHERE Phy_Lunid = ?)";
				if (!$this->conn->sql_query($update_frBindingNexus, array($sanLunId, $sanLunId)))
				{
					return false;
				}
			}
			else if($pairType == 2 || $pairType == 3)
			{
				$setValues =  $setValues . ", lun_state = 1";
				$updateHostAppTarLunMap = "UPDATE hostApplianceTargetLunMapping SET state = $RESYNC_PENDING WHERE sharedDeviceId = ? AND state = $PROTECTED AND processServerId IN (SELECT sourceHostId FROM srcLogicalVolumeDestLogicalVolume WHERE Phy_Lunid = ?)";
				if (!$this->conn->sql_query($updateHostAppTarLunMap, array($sanLunId, $sanLunId)))
				{
					return false;
				}
				$logging_obj->my_error_handler("DEBUG","resyncUI::".$updateHostAppTarLunMap."\n");
			
				$updateSharedDevice = "UPDATE hostSharedDeviceMapping SET resyncRequired = $RESYNC_MUST_REQUIRED WHERE sharedDeviceId = ?";
				if (!$this->conn->sql_query($updateSharedDevice, array($sanLunId)))
				{
					return false;
				}
				$logging_obj->my_error_handler("DEBUG","resyncUI::".$updateSharedDevice."\n");

				if ($pairType == 3)
				{
					$sql = "DELETE FROM policy WHERE policyType='19' AND policyParameters = ?";
					if (!$this->conn->sql_query($sql, array($sanLunId)))
					{
						return false;
					}
				}
			}
		}
		else
		{
			$setValues =  $setValues . ", shouldResync = 1,
									  isResync = 1,
									  resyncStartTime = now(),
									  remainingQuasiDiffBytes = 0,
									  resyncStartTagtime = 0,
									  resyncEndTagtime = 0";
		}
		
		$setValues =  $setValues . ", resyncSetCxtimestamp = 0,
									  isQuasiflag = 0,
									  throttleresync = 0,
									  throttleDifferentials = 0,
		                              differentialStartTime = 0,
									  fullSyncBytesSent = 0,
									  fastResyncMatched = 0,
									  fastResyncUnmatched = 0,
									  fastResyncUnused = 0,
									  quasidiffStarttime = 0,
                                      quasidiffEndtime = 0,
									  resyncEndTime = 0,
									  resyncStartTagtime=0,
									  remainingResyncBytes = 0,
									  lastResyncOffset = 0,
									  remainingDifferentialBytes = 0,
									  currentRPOTime = NOW(),
									  statusUpdateTime = NOW(),									  
									  ShouldResyncSetFrom = 0,
									  src_replication_status = 0,
									  tar_replication_status = 0,
									  restartPause = 0,
									  pauseActivity = NULL,
									  restartResyncCounter = ?,
									  lastTMChange = NOW(),
									  stats = '',
									  replication_status = $RESTART_RESYNC_CLEANUP";
		
		$sql = "UPDATE
				 srcLogicalVolumeDestLogicalVolume
			    SET
				 $setValues
				WHERE
				 pairId = ?" ;
				 
		$rs = $this->conn->sql_query($sql, array($restartResyncCounter, $pairId));
		
		//Update "doResync" flag for both Src and Trg
		$sql_lv = "UPDATE
						logicalVolumes
				   SET
						doResync = '1'
				   WHERE
						(FIND_IN_SET(hostId, ?) and deviceName = ?) OR
						(hostId = ? and deviceName = ?)";
		$rs_lv = $this->conn->sql_query($sql_lv, array($src_id_list_str, $src_device, $dest_id, $dest_device));
		
		//Update "doResync" flag for both Src and Trg
		$sql_vrh = "UPDATE
						volumeResizeHistory 
				   SET
						isValid = '0'
				   WHERE
						(FIND_IN_SET(hostId, ?) and deviceName = ?)";
		$rs_vrh = $this->conn->sql_query($sql_vrh, array($src_id_list_str, $src_device));
		
		//Clear resync reported health codes as part of restart resync of disk.
		$sql_resyncerror = "DELETE 
							FROM 
								resyncErrorCodesHistory 
							WHERE
								(FIND_IN_SET(sourceHostId, ?) and sourceDeviceName = ?)";
		$rs_resyncerror = $this->conn->sql_query($sql_resyncerror, array($src_id_list_str, $src_device));
		
		//Audit Log
		$query="SELECT
					ipaddress
				FROM
					hosts
				WHERE
					FIND_IN_SET(id, ?)
					";
		$iter = $this->conn->sql_query($query, array($src_id_list_str));
		$srcIp = array();
		foreach($iter as $iterKey=>$iterVal)
		{
			$srcIp[] = $iterVal['ipaddress'];
		}
		
		$srcIpStr = implode(",",$srcIp);

		$query="SELECT
					ipaddress
				FROM
					hosts
				WHERE
					id = ?";
		$iter = $this->conn->sql_query($query, array($dest_id));
		$destIp = $iter[0]['ipaddress'];
		$this->commonfunctions_obj->writeLog($_SESSION['username'],$this->commonfunctions_obj->customer_branding_names("Restart resync has been triggered for ($srcIpStr($src_device),$destIp($dest_device))"));
		
		return true;
    }

    /*
     * Function Name: update_rpo_thresholds
     *
     * Description:
     *    It updates rpo thresholds for every pair that was selected
     *    NOTE: for 1-to-N only updates the specific pair
     *    that was requested as rpo threshold is a per pair setting.
     *
     * Parameters:
     *    Param 1 [IN]:$pairsettings
     *
     * Return Value:
     *    Ret Value: No returns value
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
    public function update_rpo_thresholds($pairsettings)
    {

        foreach ($pairsettings as $pairSetting)
	{
            if (0 != strcmp($pairSetting, ''))
    	    {
		#5172:-Corrected the condition so that the rpo is getting updated into the database
			$pairSetting = str_replace("="," ", $pairSetting);
		list ($srcId, $srcDevice, $dstId, $dstDevice, $rpo,
		      $unused) = explode (" ", $pairSetting, 6);

			/*
			for clusters
			*/
		$srcIdList = explode (",", $srcId);
		$cnt = count($srcIdList);
		if (0 == $cnt)
		{
		    return false;
		}
		$srcIdsIn = "'$srcIdList[0]'";
		for ($i = 1; $i < $cnt; $i++)
		{
		    $srcIdsIn = $srcIdsIn . ", '$srcIdList[$i]'";
		}
		$dstDevice     = $this->get_unformat_dev_name($dstId,$dstDevice);
		$srcDevice     = $this->get_unformat_dev_name($srcId,$srcDevice);
		$query="UPDATE
		            srcLogicalVolumeDestLogicalVolume "."
			SET
			    rpoSLAThreshold = $rpo "."
			WHERE
			    sourceHostId
		        IN
		    	    ($srcIdsIn) AND
			    sourceDeviceName = '".
				$this->conn->sql_escape_string ($srcDevice).
				"' "."AND
	         	    destinationHostId = '$dstId' AND
			    destinationDeviceName = '".
				$this->conn->sql_escape_string($dstDevice)."'";
		if (!$this->conn->sql_query($query))
		{
                }
	    }
	}
    }

    /*
     * Function Name: get_Indiff_replication_pair
     *
     * Description:
     *    Retrieve the Replication pair details from hosts,logicalVolumes,
     *    srcLogicalVolumeDestLogicalVolume,retLogPolicy table
     *    based upon the conditions.
     *
     * Parameters:
     *     Param 1 [IN/OUT]:
     *     Param 2 [IN/OUT]:
     *
     * Return Value:
     *     Ret Value: Returns Result from database.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function get_Indiff_replication_pair($limit="" , $from="", $scenario_id="")
    {
        if ($arrVal[1] == 1)
        {
            $source_device_name=$arrVal[3];
			$modified_ruleId = $ruleId.":1:".$source_device_name;
        }
        else
        {
            $modified_ruleId = $ruleId.":0";
        }

        $query="SELECT
					lh.name,
					lhv.deviceName,
					rh.name,
					rhv.deviceName,
					rhv.tmId,
					map.ruleId,
					map.mediaretent,
					lh.id,
					ret.retState,
					rh.id,
					lhv.Phy_Lunid,
					map.pairId
				FROM
					hosts lh,
					hosts rh,
					logicalVolumes lhv,
					logicalVolumes rhv,
					srcLogicalVolumeDestLogicalVolume map,
					retLogPolicy ret ";

		$query = $this->commonfunctions_obj->addcond("lh.id = lhv.hostId ", $query);

		$query = $this->commonfunctions_obj->addcond("rh.id = rhv.hostId ", $query);

		$query = $this->commonfunctions_obj->addcond("lh.id = map.sourceHostId ", $query);

		$query = $this->commonfunctions_obj->addcond("rh.id = map.destinationHostId ", $query);

		$query = $this->commonfunctions_obj->addcond("lhv.deviceName = map.sourceDeviceName ",$query);

		$query = $this->commonfunctions_obj->addcond("rhv.deviceName = map.destinationDeviceName ", $query);

		$query = $this->commonfunctions_obj->addcond(" UNIX_TIMESTAMP(map.resyncStartTime) != 0", $query);

		// $query = $this->commonfunctions_obj->addcond(" UNIX_TIMESTAMP(map.
														 // resyncEndTime) != 0",
											 // $query);

		// $query = $this->commonfunctions_obj->addcond(" map.
														 // differentialStartTime > 0 ",
														// $query);

		$query = $this->commonfunctions_obj->addcond(" map.ruleId = ret.ruleid ",$query);

		$query = $this->commonfunctions_obj->addcond(" (map.isQuasiflag = 2 or map.isQuasiflag = 0 or map.isQuasiflag = 1) ", $query);

		if($scenario_id)
		{
			$query = $this->commonfunctions_obj->addcond(" map.scenarioId = '$scenario_id'",$query);
		}
		if($_GET['frm_src_host'])
		{
			$query = $this->commonfunctions_obj->addcond(" lh.id = '".$_GET['frm_src_host']."' ",$query);
		}
		if($_GET['frm_tgt_host'])
		{
			$query = $this->commonfunctions_obj->addcond(" rh.id = '".$_GET['frm_tgt_host']."' ",$query);
		}
		if($_GET['frm_device'])
		{
			if(get_magic_quotes_gpc() == 0)
			{
				$device_name = addslashes($_GET['frm_device']);
				$device_name = str_replace('\\', '\\\\', $device_name);
			}
			else
			{
				$device_name = str_replace('\\', '\\\\', $_GET['frm_device']);
			}

			$query = $this->commonfunctions_obj->addcond(" (lhv.deviceName  like'%".trim($device_name)."%'   or rhv.deviceName  like'%".trim($device_name)."%'   or lhv.mountPoint like '%".trim($device_name)."%'  or rhv.mountPoint like '%".trim($device_name)."%'   ) ",$query);
		}

		if ( $from)
		{
			$query = $query . " AND map.scenarioId=0 ";
		}
		$query = $query . " GROUP BY map.destinationHostId,map.destinationDeviceName ";
		$query = $query . "ORDER BY lh.name, lhv.deviceName $limit";
		// echo "<br>query :: $query <br>";
		$repRs = $this->conn->sql_query($query);

		$arrClusterId = array();

		while( $myrow = $this->conn->sql_fetch_row($repRs) )
		{
			if (($myrow[6] == 1)&&($myrow[8] != 1))
			{
				if (strpos($myrow[1],'\\'))
				{
					$myrow[1] = addslashes($myrow[1]);
				}
				$query1="SELECT
							 clusterGroupName,
							 clusterName,
							 clusterruleId
						 FROM
							 clusters
						 WHERE
							 hostId='$myrow[7]' and
							 deviceName='$myrow[1]'";
				#echo "query1 :: $query1 <br>";
				$repRs1 = $this->conn->sql_query($query1);
				$myrow2 = $this->conn->sql_fetch_row ($repRs1);

				if($myrow2[2])
				{
					#ZFS
					$qry = "SELECT hostId FROM clusters WHERE clusterruleId = $myrow2[2]";
					$rs = $this->conn->sql_query($qry);


					while( $iter = $this->conn->sql_fetch_row ($rs))
					{
						$sel_qry = "SELECT name FROM hosts WHERE id = '$iter[0]'";
						$res_set = $this->conn->sql_query($sel_qry);
						$iter1 = $this->conn->sql_fetch_row ($res_set);
						$result[] = array('server' => $iter1[ 0 ],
									  'pvol' => $myrow[ 1 ],
									  'rserver' => $myrow[2],
									  'rvol' => $myrow[3],
									  'tmid' => $myrow[4],
									  'ruleId'=> $myrow[5],
									  'clusterGroupName'=>$myrow2[0],
									  'clusterName'=>$myrow2[1],
									  'clusterruleId'=>$myrow2[2],
									  'srcId'=>$myrow[ 7 ],
									  'destId'=>$myrow[ 9 ],
									  'lun_id'=>$myrow[ 10 ],
									  'pairId'=>$myrow[ 11 ]);
					}
				}
				else
				{
					$result[] = array('server' => $myrow[ 0 ],
									  'pvol' => $myrow[ 1 ],
									  'rserver' => $myrow[2],
									  'rvol' => $myrow[3],
									  'tmid' => $myrow[4],
									  'ruleId'=> $myrow[5],
									  'clusterGroupName'=>$myrow2[0],
									  'clusterName'=>$myrow2[1],
									  'clusterruleId'=>$myrow2[2],
									  'srcId'=>$myrow[ 7 ],
									  'destId'=>$myrow[ 9 ],
									  'lun_id'=>$myrow[ 10 ],
									  'pairId'=>$myrow[ 11 ]);
				}


			}
		}
		return( $result);
    }

	public function get_count_replication_pair( $from = '')
	{
		$query="SELECT
		    map.destinationDeviceName
		FROM
		    hosts lh,
		    hosts rh,
		    logicalVolumes lhv,
		    logicalVolumes rhv,
		    srcLogicalVolumeDestLogicalVolume map,
		    retLogPolicy ret ";

	$query = $this->commonfunctions_obj->addcond("lh.id = lhv.hostId ",
                                                     $query);

	$query = $this->commonfunctions_obj->addcond("rh.id = rhv.hostId ",
                                                     $query);

	$query = $this->commonfunctions_obj->addcond("lh.id = map.
                                                      sourceHostId ",
                                                      $query);

	$query = $this->commonfunctions_obj->addcond("rh.id = map.
                                                     destinationHostId ",
                                                     $query);

	$query = $this->commonfunctions_obj->addcond("lhv.deviceName = map.
                                                     sourceDeviceName ",$query);

	$query = $this->commonfunctions_obj->addcond("rhv.deviceName = map.
                                                     destinationDeviceName ",
			                             $query);

	$query = $this->commonfunctions_obj->addcond(" UNIX_TIMESTAMP(map.
                                                     resyncStartTime) != 0",
			                             $query);

	// $query = $this->commonfunctions_obj->addcond(" UNIX_TIMESTAMP(map.
                                                     // resyncEndTime) != 0",
			                             // $query);

	// $query = $this->commonfunctions_obj->addcond(" map.
                                                     // differentialStartTime > 0 ",
                                                     // $query);

	$query = $this->commonfunctions_obj->addcond(" map.ruleId = ret.
                                                     ruleid ",$query);

	$query = $this->commonfunctions_obj->addcond(" (map.isQuasiflag = 2 or map.
                                                     isQuasiflag = 0 or isQuasiflag = 1) ",
			                             $query);

	if($_GET['frm_src_host'])
	{
		$query = $this->commonfunctions_obj->addcond(" lh.id = '".$_GET['frm_src_host']."' ",$query);
	}
	if($_GET['frm_tgt_host'])
	{
		$query = $this->commonfunctions_obj->addcond(" rh.id = '".$_GET['frm_tgt_host']."' ",$query);
	}
	if($_GET['frm_device'])
	{
		if(get_magic_quotes_gpc() == 0)
		{
			$device_name = addslashes($_GET['frm_device']);
			$device_name = str_replace('\\', '\\\\', $device_name);
		}
		else
		{
			$device_name = str_replace('\\', '\\\\', $_GET['frm_device']);
		}


		$query = $this->commonfunctions_obj->addcond(" (lhv.deviceName  like'%".trim($device_name)."%'   or rhv.deviceName  like'%".trim($device_name)."%'   or lhv.mountPoint like '%".trim($device_name)."%'  or rhv.mountPoint like '%".trim($device_name)."%'   ) ",$query);

	}

		if ( $from)
		{
			$query = $query . " AND map.scenarioId=0 ";
		}
		$query = $query." GROUP BY map.destinationHostId,map.destinationDeviceName";
		// echo "query : $query <br>";
		$result = $this->conn->sql_query($query);
		if ($result === false)
		{
			return 0;
		}
		else
		{
			$count = $this->conn->sql_num_row($result);
			return $count;
		}
	}
	public function get_snapshot_replication_pairs($profiler="",$limit="")
	{
		global $IN_PROFILER;
        $result = array();

		if($_GET['frm_src_host'])
		{
			$sql .= " AND lh.id = '".$_GET['frm_src_host']."' ";
		}
		if($_GET['frm_tgt_host'])
		{
			$sql .= " AND rh.id = '".$_GET['frm_tgt_host']."' ";
		}
		if($_GET['frm_device'])
		{
			if(get_magic_quotes_gpc() == 0)
			{
				$device_name = addslashes($_GET['frm_device']);
				$device_name = str_replace('\\', '\\\\', $device_name);
			}
			else
			{
				$device_name = str_replace('\\', '\\\\', $_GET['frm_device']);
			}
			$sql .= " AND (lhv.deviceName  like'%".trim($device_name)."%' or rhv.deviceName  like'%".trim($device_name)."%' or lhv.mountPoint like '%".trim($device_name)."%' or rhv.mountPoint like '%".trim($device_name)."%')";
		}

         $query="SELECT
                    lh.name,
                    lhv.deviceName,
                    rh.name,
                    rhv.deviceName," . "
                    map.lastOutpostAgentChange,
                    lh.sentinelEnabled,
                    " . "lh.id,
                    rh.id,
                    map.shouldResync,
                    map.profilingMode,
                    " .  "map.rpoSLAThreshold,
                    UNIX_TIMESTAMP(map.resyncStartTime),
                    " . "UNIX_TIMESTAMP(map.resyncEndTime),
                    FROM_UNIXTIME(rh.lastHostUpdateTime),
                    " . "map.volumeGroupId,
                    lhv.volumeLabel,
                    rhv.volumeLabel,
                    map.isResync,
                    map.isQuasiflag,
                    map.maxCompression ,
                    map.maxUnCompression,
                    map.profilingId,
                    lhv.capacity " . ",
					lhv.Phy_Lunid,
		    		lh.alias,
		    		rh.alias,
					map.pauseActivity,
					map.ruleId
                FROM
                    hosts lh,
                    hosts rh,
                    logicalVolumes lhv,
                    " . "logicalVolumes rhv,
                    srcLogicalVolumeDestLogicalVolume map " . "
                WHERE
                    lh.id = lhv.hostId AND
                    rh.id = rhv.hostId AND" . "
					lh.id = map.sourceHostId AND
                    rh.id = map.destinationHostId " . "AND
                    lhv.deviceName = map.sourceDeviceName AND " . "
                    rhv.deviceName = map.destinationDeviceName " . " AND
					map.destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'
					 $sql
				GROUP BY
					map.destinationHostId,
					map.destinationDeviceName
				ORDER BY
                    lh.name,
                    lhv.deviceName $limit";
		#echo "query :: $query <br>";
        $iter = $this->conn->sql_query($query);

        while( $myrow = $this->conn->sql_fetch_row( $iter ) )
        {
            $query_clus="SELECT
                            clusterGroupName,
                            clusterName,
							clusterruleId
                        FROM
                            clusters
                        WHERE
                            hostId='$myrow[6]' AND
                            deviceName='".$this->conn->sql_escape_string ($myrow[1])."'" ;
            $iter2 = $this->conn->sql_query ($query_clus);
            $myrow2 = $this->conn->sql_fetch_row ($iter2);
			if ($profiler==1)
			{
				#echo "IF <br>";
				if ($myrow[2]==$IN_PROFILER)
				{
					#echo "IFFFF <br>";
				}
				else
				{
					#ZFS
					#echo "ELSEEE <br>";
					$qry = "SELECT hostId FROM clusters WHERE clusterruleId = '$myrow2[2]'";
					$rs = $this->conn->sql_query($qry);

					if($myrow2[2])
					{
						while( $iter3 = $this->conn->sql_fetch_row ($rs))
						{
							$sel_qry = "SELECT name FROM hosts WHERE id = '$iter3[0]'";
							$res_set = $this->conn->sql_query($sel_qry);
							$iter4 = $this->conn->sql_fetch_row ($res_set);
							$result[] = array( 'server' => $iter4[ 0 ],
										   'pvol' => $myrow[ 1 ],
										   'rserver' => $myrow[2],
										   'rvol' => $myrow[3],
										   'lr_time' => $myrow[4],
										   'r_state' => $myrow[8],
										   'server_id' => $myrow[6],
										   'rserver_id' => $myrow[7],
										   'profiling' => $myrow[9],
										   'rpo' => $myrow[10],
										   'resyncStartTime' => $myrow[11],
										   'resyncEndTime' => $myrow[12],
										   'lastSentinelChange' => $myrow[13],
										   'clusterGroupName' => $myrow2[0],
										   'clusterName' => $myrow2[1],
										   'volumeGroupId' => $myrow[14],
										   'srcVolLabel' => $myrow[15],
										   'tgtVolLabel' => $myrow[16],
										   'isResync' => $myrow[17],
										   'isQuasiFlag' => $myrow[18],
										   'maxCompression' => $myrow[19],
										   'maxUnCompression' => $myrow[20],
										   'profilingId' => $myrow[21],
										   'capacity' => $myrow[22],
										   'lun_id' => $myrow[23],
										   'lalias' => $myrow[24],
										   'ralias' => $myrow[25],
										   'pauseActivity' => $myrow[26],
										   'ruleId' => $myrow[27]);
						}
					}
					else
					{
						$result[] = array( 'server' => $myrow[ 0 ],
										   'pvol' => $myrow[ 1 ],
										   'rserver' => $myrow[2],
										   'rvol' => $myrow[3],
										   'lr_time' => $myrow[4],
										   'r_state' => $myrow[8],
										   'server_id' => $myrow[6],
										   'rserver_id' => $myrow[7],
										   'profiling' => $myrow[9],
										   'rpo' => $myrow[10],
										   'resyncStartTime' => $myrow[11],
										   'resyncEndTime' => $myrow[12],
										   'lastSentinelChange' => $myrow[13],
										   'clusterGroupName' => $myrow2[0],
										   'clusterName' => $myrow2[1],
										   'volumeGroupId' => $myrow[14],
										   'srcVolLabel' => $myrow[15],
										   'tgtVolLabel' => $myrow[16],
										   'isResync' => $myrow[17],
										   'isQuasiFlag' => $myrow[18],
										   'maxCompression' => $myrow[19],
										   'maxUnCompression' => $myrow[20],
										   'profilingId' => $myrow[21],
										   'capacity' => $myrow[22],
										   'lun_id' => $myrow[23],
										   'lalias' => $myrow[24],
										   'ralias' => $myrow[25],
										   'pauseActivity' => $myrow[26],
										   'ruleId' => $myrow[27]);
					}
				}
			}
			else
			{
				#echo "ELSE <br>";
				$result[] = array( 'server' => $myrow[ 0 ],
								   'pvol' => $myrow[ 1 ],
								   'rserver' => $myrow[2],
								   'rvol' => $myrow[3],
								   'lr_time' => $myrow[4],
								   'r_state' => $myrow[8],
								   'server_id' => $myrow[6],
								   'rserver_id' => $myrow[7],
								   'profiling' => $myrow[9],
								   'rpo' => $myrow[10],
								   'resyncStartTime' => $myrow[11],
								   'resyncEndTime' => $myrow[12],
								   'lastSentinelChange' => $myrow[13],
								   'clusterGroupName' => $myrow2[0],
								   'clusterName' => $myrow2[1],
								   'volumeGroupId' => $myrow[14],
								   'srcVolLabel' => $myrow[15],
								   'tgtVolLabel' => $myrow[16],
								   'isResync' => $myrow[17],
								   'isQuasiFlag' => $myrow[18],
								   'maxCompression' => $myrow[19],
								   'maxUnCompression' => $myrow[20],
								   'profilingId' => $myrow[21],
								   'capacity' => $myrow[22],
								   'lun_id' => $myrow[23],
								   'lalias' => $myrow[24],
								   'ralias' => $myrow[25],
								   'pauseActivity' => $myrow[26]);
			}
        }

        return( $result );
	}
	public function get_wan_rep_count()
	{

		if($_GET['frm_src_host'])
		{
			$sql .= " AND lh.id = '".$_GET['frm_src_host']."' ";
		}
		if($_GET['frm_tgt_host'])
		{
			$sql .= " AND rh.id = '".$_GET['frm_tgt_host']."' ";
		}
		if($_GET['frm_device'])
		{
			if(get_magic_quotes_gpc() == 0)
			{
				$device_name = addslashes($_GET['frm_device']);
				$device_name = str_replace('\\', '\\\\', $device_name);
			}
			else
			{
				$device_name = str_replace('\\', '\\\\', $_GET['frm_device']);
			}
			$sql .= " AND (lhv.deviceName  like'%".trim($device_name)."%' or rhv.deviceName  like'%".trim($device_name)."%' or lhv.mountPoint like '%".trim($device_name)."%' or rhv.mountPoint like '%".trim($device_name)."%')";
		}

        $query="SELECT
                    map.destinationDeviceName
                FROM
                    hosts lh,
                    hosts rh,
                    logicalVolumes lhv,
                    logicalVolumes rhv,
                    srcLogicalVolumeDestLogicalVolume map
                WHERE
                    lh.id = lhv.hostId AND
                    rh.id = rhv.hostId AND
                    lh.id = map.sourceHostId AND
                    rh.id = map.destinationHostId AND
                    lhv.deviceName = map.sourceDeviceName AND
                    rhv.deviceName = map.destinationDeviceName AND
					map.destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'
					$sql";
					
		$query = $query." GROUP BY map.destinationHostId,map.destinationDeviceName";
		$result = $this->conn->sql_query($query);

        if ($result === false)
		{
			return 0;
		}
		else
		{
			$row = $this->conn->sql_num_row($result);
			return $row;
		}
	}

    /*
     * Function Name: get_lun_state
     *
     * Description:
     *    gets the lun_state value that should be used when updating/inserting
     *    a lun into srcLogicalVolumeDestLogical volume
     *
     *    Handles 1-to-1, 1-to-N, cluster, and 1-to-N cluster
     *
     * Parameters:
     *     Param 1 [IN]:$lunId - set to the Phy_Lunid value to search for
     *
     * Return Value:
     *     Ret Value: $START_PROTECTION_PENDING, or the lun_state of one of the
     *     existing 1-to-n pairs for the given Phy_Lunid
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function get_lun_state($lunId)
    {

        global $START_PROTECTION_PENDING,
            $STOP_PROTECTION_PENDING;

        $rs = $this->conn->sql_query("select distinct ".
                          " lun_state ".
                          "from   ".
                          " srcLogicalVolumeDestLogicalVolume  ".
                          "where  ".
                          " Phy_Lunid = '$lunId' and".
                          " lun_state != $START_PROTECTION_PENDING",
                          $this->db_handle);

        if (!$rs) {
            //report error
            return $START_PROTECTION_PENDING;
        }

        while ($row = $this->conn->sql_fetch_row($rs)) {
            // 1-to-n
            if ($STOP_PROTECTION_PENDING != $row[0]) {
                // have an existing entry that is not in
                // stop protection pending just return its state
                return $row[0];
            }
        }

        // at this point either 1-to-n paris with all existing pairs in
        // $STOP_PROTECTION_PENDING or just a 1-to-1 pair (possibly clusterd)
        // need to let ITLProtector figure the rest out
        return $START_PROTECTION_PENDING;
    }


    /*
     * Function Name: add_pair
     *
     * Description:
     *    Add the New Replication pair
     *    Handles 1-to-1, 1-to-N, cluster, and 1-to-N cluster
     *
     * Parameters:
     *     Param 1 [IN]:$srcId
     *     Param 2 [IN]:$srcDevice
     *     Param 3 [IN]:$dstId
     *     Param 4 [IN]:$dstDevice
     *     Param 5 [IN]:$dpsId
     *     Param 6 [IN]:$medRet
     *     Param 7 [IN]:$fast
     *     Param 8 [IN]:$compEnable
     *     Param 9 [IN]:$groupId
     *     Param 10 [IN]:$ftpsSrc
     *     Param 11 [IN]:$ftpsDst
     *     Param 12 [IN]:$logType
     *     Param 13 [IN]:$logPolicy
     *     Param 14 [IN]:$retSizeInBytes
     *     Param 15 [IN]:$metaFilePath
     *     Param 16 [IN]:$logDataDir
     *     Param 17 [IN]:$volume1
     *     Param 18 [IN]:$volume2
     *     Param 19 [IN]:$timeRecFlag
     *     Param 20 [IN]:$hidTime
     *     Param 21 [IN]:$change_by_time_day
     *     Param 22 [IN]:$change_by_time_hr
     *     Param 23 [IN]:$txtlocdir_move
     *     Param 24 [IN]:$txtdiskthreshold
     *     Param 25 [IN]:$txtlogsizethreshold
     *     Param 26 [IN]:$auto_resync_time_lapse
     *     Param 27 [IN]:$window_start_hours
     *     Param 28 [IN]:$window_start_minutes
     *     Param 29 [IN]:$window_stop_hours
     *     Param 30 [IN]:$window_stop_minutes
     *     Param 31 [IN]:$logExceedPolicy
     *     Param 32 [IN]:$lun_process_server_info
     *
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function add_pair($srcId, $srcDevice, $dstId, $dstDevice, $dpsId, $medRet,
                      $fast, $compEnable, $groupId, $ftpsSrc, $ftpsDst,
                      $logType, $logPolicy, $retSizeInBytes, $metaFilePath,
                      $logDataDir, $volume1, $volume2, $timeRecFlag,$hidTime,
                      $change_by_time_day,$change_by_time_hr,$txtlocdir_move,
                      $txtdiskthreshold,$txtlogsizethreshold,
                      $auto_resync_time_lapse,$window_start_hours,
                      $window_start_minutes, $window_stop_hours,
                      $window_stop_minutes,$logExceedPolicy,
                      $lun_process_server_info,$process_server,
                      $ps_natip_source,$ps_natip_target,$sparse_enable = 0,
					  $batch_resync_val=0,$execution_state=0,$scenario_id=0,$plan_id=0,$app_type=0,$ret_arr=array())

    {
		global $START_PROTECTION_PENDING,$PROTECTED,$INITIAL_LUN_DEVICE_NAME;
		global $logging_obj;
		global $IN_PROFILER;
        global $MAX_TMID;
        global $INMAGE_PROFILER_HOST_ID;
		
		global $newdb;
        global $retention_obj;
        global $system_obj;
		global $commonfunctions_obj,$logging_obj,$sparseretention_obj;

	    global $VOLUME_SYNCHRONIZATION_PROGRESS,$VIRTUAL_CREATE_PENDING;
	    global $MIRROR_CREATE_PENDING, $MIRROR_DELETE_FAIL,$MIRROR_CREATE_FAIL;
        global $REPLICATION_ROOT;
		global $MAX_RESYNC_FILES_THRESHOLD,$MAX_DIFF_FILES_THRESHOLD;
		global $target_data_plane;
        
        if (!isset($target_data_plane))
        {
            $target_data_plane = 1;
        }
		$return_result = TRUE;

		$Phy_Lunid = NULL;
		$lun_state = $PROTECTED;
		$srcFabric = FALSE;
		
		
		if($lun_process_server_info!="")
		{
			$srcFabric = TRUE;
			$Phy_Lunid = $lun_process_server_info;
			$lun_state = $START_PROTECTION_PENDING;
		}
		
		#$target_compatibilityno =  $this->commonfunctions_obj->get_compatibility($dstId);
		
		$solution_type = $ret_arr['solution_type'];
		$plan_type =  $ret_arr['plan_type'];
		
		$escSrcDevice = $this->conn->sql_escape_string($srcDevice);
        $escDstDevice = $this->conn->sql_escape_string($dstDevice);
		$escDstDevice =$this->commonfunctions_obj->decodePath($escDstDevice);
		
		// Assign Defaults
		$willBeOneToN = FALSE;
		$syncCopy = "full";
		$default_rpo_threshold = 30;
		$compressionSetting = 0;
		$oneToManyPrimary = 1; //(0 == 1-N) ; (1 == 1-1)
		$jobId = 0;
		$src_replication_status = 0;
		$tar_replication_status = 0;
		$pauseActivity = NULL;
		$replication_status = NULL;
		$current_now = 0;
		$srcCapacity = $ret_arr['srcCapacity'];
		$profilingId = mt_rand (1, mt_getrandmax ());
		$maxDiffFilesThreshold = $MAX_DIFF_FILES_THRESHOLD;
		$profilingMode = 0;
		$isResync = 1;
		$shouldResync = 1;
		$diffStartTime = 0;
		$resyncEndTime = 0;
		$inUse = 1;
		$doResync = 1;
		$resyncVer = 0;
		if ($fast)
		{
			$resyncVer = $fast;
		}
		
		$isProfiler = ($INMAGE_PROFILER_HOST_ID == $dstId);
		if ($isProfiler)
        {
            $profilingMode = 1;
            $isResync = 0;
            $shouldResync = 0;
            $diffStartTime = "now()";
            $resyncEndTime = "now()";
            $inUse = 0;
            $doResync = 0;			
			#11909 fix			
			$resyncVer = 2;
			$jobId = time();
        }
       		
		if (!$ftpsSrc)
        {
            $ftpsSrc = 0;
        }

        if (!$ftpsDst)
        {
            $ftpsDst = 0;
        }
		
		if ($compEnable)
        {
            $compressionSetting = $compEnable;
        }
		
		// NOTE: protocol to use should not be set in logicalVolume
        // it should be determined based on host and its process server
        // and if the host and process server are on the same machine or not
        // until that has been changed we still set it here        
        if ($fast == 4)
		{ //in case the Sync options is 'Direct apply', transport protocol will be 3.
            $dstTransportProtocol = 3;
			$srcTransportProtocol = 3;
        }
        else
		{
			#$amethyst_values = $commonfunctions_obj->amethyst_values();
			$srcTransportProtocol = $dstTransportProtocol = 1;
            
			#if(($dstId == $process_server) || ($srcId == $process_server) || $srcFabric)
			if(($srcId == $process_server) || $srcFabric)
			{
				$srcTransportProtocol = 2;
			}
			
			if(($dstId == $process_server)) 
			{
				$dstTransportProtocol = 2;
			}
		}
		
		$query = "SELECT compatibilityNo FROM hosts WHERE id = '$srcId'";
        $iter = $this->conn->sql_query($query);
        $myrow = $this->conn->sql_fetch_row($iter);
		$source_compatibilityno = $myrow[0];
 		
		/* 
			Check If Pair already exists or not
			-- Return True if exists
			-- Return False if Error
			-- Continue if no Pair exists
		*/
		$query="SELECT count(*) AS ROWS FROM srcLogicalVolumeDestLogicalVolume 
					WHERE
					sourceHostId = '$srcId' AND
					sourceDeviceName = '$escSrcDevice' AND
					destinationHostId = '$dstId' AND
					destinationDeviceName = '$escDstDevice'";
		$result_set = $this->conn->sql_query($query);
		$row = $this->conn->sql_fetch_row($result_set);
		if($row[0] != 0) 
		{
			return $return_result;
		}
		
		/* 
			Check If its 1-N
			-- ($oneToManyPrimary = 0 if 1-N) ; ($oneToManyPrimary = 1 if 1-1)
		*/
       	$query="SELECT
					resyncStartTagtime,
					restartResyncCounter,
					profilingId,
					replication_status,
					src_replication_status,
					pauseActivity,
					lun_state
				FROM
					srcLogicalVolumeDestLogicalVolume 
				WHERE
					sourceHostId = '$srcId' AND
					sourceDeviceName = '$escSrcDevice' AND
					oneToManySource = 1";
		$result_set = $this->conn->sql_query($query);
		
		$count_rows = $this->conn->sql_num_row($result_set);
		if($count_rows != 0) 
		{
			$oneToManyPrimary = 0;
			$willBeOneToN = TRUE;
						
			#11790 Fix
			/*a.  Before inserting the replication pair and is part of 1toN, check whether
			the primary pair has got RST or not, and restart resync counter.  If RST is
			recieved or restart resync counter is set, then batch condtion functions as it
			is and that means configures the pair.
			b. If RST is not recieved and restart resync counter is not set, then mark the
			replication pair as with execution state 3(which is unconfigured+waiting for an
			RST)*/
			$row = $this->conn->sql_fetch_row($result_set);
			$primary_resyncStartTagTime = $row[0];
			$primary_restartResyncCounter = $row[1];
			$profilingId = $row[2];
			if(($primary_resyncStartTagTime == 0) && ($primary_restartResyncCounter == 0))
			{					
				$execution_state = 3;
			}
			
			if($row[4] == 1) {
				$src_replication_status = 1;
				$tar_replication_status = 1;
				$pauseActivity = $row[5];
				$replication_status = $row[3];
			}
			
			if($srcFabric)
			{
				$lun_state = $row[6];
			}
		}
		
		if($execution_state == 1 or $execution_state == 0)
		{
			$sel_qry = "SELECT now()";
			$result = $this->conn->sql_query($sel_qry);
			
			$row = $this->conn->sql_fetch_row($result);
			$current_now = $row[0];
		}
				
		$maxDiffFilesThreshold_query = "SELECT
											maxDiffFilesThreshold,
											fileSystemType
										FROM
											logicalVolumes
										WHERE
											hostId = '$srcId' AND
											deviceName = '$escSrcDevice'";
		$result = $this->conn->sql_query($maxDiffFilesThreshold_query);
		
		$diffObj = $this->conn->sql_fetch_object($result);	
		$file_system_type = $diffObj->fileSystemType;
		if(($diffObj->maxDiffFilesThreshold != NULL) && $willBeOneToN)			
		{
			$maxDiffFilesThreshold = $diffObj->maxDiffFilesThreshold;
		}
		
		if($source_compatibilityno >= 650000 && $file_system_type == "NTFS")
		{
			$syncCopy = $ret_arr['syncCopy'];
		}
		
		$cnt = 0;
		$sql = "select DISTINCT clusterId from clusters where hostId = '$srcId' AND deviceName='$escSrcDevice'";
		$result_set = $this->conn->sql_query($sql);
		
		$clusterIds = array();
		$clusterIds[] = $srcId;
		$row_cnt = $this->conn->sql_num_row($result_set);
		if ($row_cnt > 0)
		{
			$row_val = $this->conn->sql_fetch_row($result_set);
			$cluser_id = $row_val[0];
			$sql = "select DISTINCT hostId from clusters where clusterId = '$cluser_id' and hostId != '$srcId'";
			$result_set = $this->conn->sql_query($sql);
			
			while($row = $this->conn->sql_fetch_row($result_set))
			{
				$clusterIds[] = $row[0];
			}			 
			$this->set_cluster_rule_ids($clusterIds, $escSrcDevice);
		}
		$cnt = count($clusterIds);
		
		$all_src_host_Ids = implode("','",$clusterIds);
		#13214 Fix -- added srcCapacity col into pair table
		if($scenario_id == 0)
		{
			$sql_capacity = "SELECT max(capacity) AS capacity FROM logicalVolumes WHERE hostId IN ('$all_src_host_Ids') AND deviceName = '$escSrcDevice'";
			$result_set = $this->conn->sql_query($sql_capacity);
			
			$row = $this->conn->sql_fetch_object($result_set);
			$srcCapacity = $row->capacity;
		}
		
		//create new volume group if needed
		$volumeGroup = $groupId;
		if (0 == $groupId)
		{
			$sql_group_Id = "INSERT INTO volumeGroups (displayName) values('Volume')";
			$result_group_Id = $this->conn->sql_query($sql_group_Id);
			$volumeGroup = $this->conn->sql_insert_id();
		}
		
		$pairId =$this->get_new_pairId();
		
		$newRuleIds = array();
		$vals = "";
        $use_cfs = 0;
        if(isset($ret_arr['use_cfs']))
		{
			if($ret_arr['use_cfs'] == 1) $use_cfs = 1;
			else $use_cfs = 0;
		}
		/* 
			-- START
			Code to Insert into srcLogicalVolumeDestLogicalVolume table
		*/
		for ($i = 0; $i < $cnt; $i++)
		{
			$ruleId =$this->get_new_ruleId();
			array_push($newRuleIds, $ruleId);
			
			if($vals)
			{
			$vals = $vals . ",('$clusterIds[$i]',
							   '$escSrcDevice',
							   '$dstId',
							   '$escDstDevice', ".
							   "'$current_now',
							   $resyncEndTime,
							   $diffStartTime,
							   '0',
							   '$ruleId',
							   '$medRet',".
							   "'$shouldResync',
							   '$isResync',
							   '$resyncVer',
							   '$volumeGroup',
							   '$compressionSetting',".
							   "'$profilingMode',
							   '$oneToManyPrimary',
							   '$jobId',
							   '$ftpsSrc',
							   '$ftpsDst',
							   '$auto_resync_time_lapse',".
							   "'$window_start_hours',
							   '$window_start_minutes',
							   '$window_stop_hours',
							   '$window_stop_minutes',
                               '$Phy_Lunid',
                               '$lun_state',
                                               '$process_server',
                               now(),'$default_rpo_threshold',
                              '$batch_resync_val',
                              '$execution_state',
                              '$scenario_id',
                              '$plan_id',
                              '$app_type',
                              '$srcCapacity',
                              '$syncCopy',
                              '$profilingId',
                              '$pairId',
                              '$MAX_RESYNC_FILES_THRESHOLD',
                              '$src_replication_status',
                              '$tar_replication_status',
                              '$replication_status',
                              '$pauseActivity',
                              '$ps_natip_source',
                              '$ps_natip_target',
                               $use_cfs,
                              $target_data_plane,
							  now()
                            )";
			}
			else
			{
				$vals = "('$clusterIds[$i]',
						   '$escSrcDevice',
						   '$dstId',
						   '$escDstDevice', ".
						   "'$current_now',
						   $resyncEndTime,
						   $diffStartTime,
						   '0',
						   '$ruleId',
						   '$medRet',".
						   "'$shouldResync',
						   '$isResync',
						   '$resyncVer',
						   '$volumeGroup',
						   '$compressionSetting',".
						   "'$profilingMode',
						   '$oneToManyPrimary',
						   '$jobId',
						   '$ftpsSrc',
						   '$ftpsDst',
						   '$auto_resync_time_lapse',".
						   "'$window_start_hours',
						   '$window_start_minutes',
						   '$window_stop_hours',
						   '$window_stop_minutes',
						   '$Phy_Lunid',
						   '$lun_state',
						   '$process_server',
							now(),'$default_rpo_threshold',
						   '$batch_resync_val',
						   '$execution_state',
						  '$scenario_id',
						  '$plan_id',
						  '$app_type',
						  '$srcCapacity',
						  '$syncCopy',
						  '$profilingId',
						  '$pairId',
						  '$MAX_RESYNC_FILES_THRESHOLD',
						  '$src_replication_status',
						  '$tar_replication_status',
						  '$replication_status',
						  '$pauseActivity',
						  '$ps_natip_source',
						  '$ps_natip_target',
                          $use_cfs,
                          $target_data_plane,
						  now())";
			}
		}
       
       
		#13214 Fix -- added srcCapacity col into pair table
		$insert_sql = "INSERT
                            INTO
                            srcLogicalVolumeDestLogicalVolume " .
                            "(sourceHostId,
                              sourceDeviceName,
                              destinationHostId,
                              destinationDeviceName, ".
                              "resyncStartTime,
                              resyncEndTime,
                              differentialStartTime,
                              lastResyncOffset,
                              ruleId,
                              mediaretent, ".
                              "shouldResync,
                              isResync,
                              resyncVersion,
                              volumeGroupId,
                              compressionEnable, ".
                              "profilingMode,
                              oneToManySource,
                              jobId,
                              ftpsSourceSecureMode,
                              ftpsDestSecureMode,
                              autoResyncStartTime, ".
                              "autoResyncStartHours,
                              autoResyncStartMinutes,
                              autoResyncStopHours,
                              autoResyncStopMinutes,
			      Phy_Lunid,
			      lun_state,
                              processServerId,
			      pairConfigureTime,
				  rpoSLAThreshold,
				  resyncOrder,
				  executionState,
				  scenarioId,
				  planId,
				  applicationType,
				  srcCapacity,
				  syncCopy,
				  profilingId,
				  pairId,
				  maxResyncFilesThreshold,
				  src_replication_status,
                  tar_replication_status,
				  replication_status,
				  pauseActivity,
				  usePsNatIpForSource,
				  usePsNatIpForTarget,
                  useCfs,
                  TargetDataPlane,
				  lastTMChange				  
				  ) " .
                          "values $vals";
		$rs_insert_sql = $this->conn->sql_query($insert_sql);
				
		/* 
			-- END
			Code to Insert srcLogicalVolumeDestLogicalVolume table for SRC
		*/
		
		/* 
			Code to update srcLogicalVolumeDestLogicalVolume table for SRC
			-- START
		*/
		if ($willBeOneToN)
		{
			$sql_src_compression = "UPDATE
						srcLogicalVolumeDestLogicalVolume
						SET   
						compressionEnable = '$compressionSetting'
						WHERE sourceHostId in ('$all_src_host_Ids')
						AND
						 sourceDeviceName = '$escSrcDevice'";
			$result_src_compression = $this->conn->sql_query($sql_src_compression);
						
			if ($ftpsSrc)
			{
				$sql_src_ftp = "UPDATE
									srcLogicalVolumeDestLogicalVolume ".
							 "SET   ftpsSourceSecureMode = '$ftpsSrc' ".
							 "WHERE sourceHostId in ('$all_src_host_Ids')
									AND
									 sourceDeviceName = '$escSrcDevice'";
				$result_src_ftp = $this->conn->sql_query($sql_src_ftp);
			}
		}
		
		$tmId = $this->get_tmid($srcId,$escSrcDevice,$willBeOneToN,$process_server);
		/* 
			-- END
			Code to update srcLogicalVolumeDestLogicalVolume table for SRC
		*/
		
		/* 
			Code to update LogicalVolume table for SRC and TRG
			-- START
		*/
		$query_src = "UPDATE
					logicalVolumes
				  SET
					maxDiffFilesThreshold = '$maxDiffFilesThreshold',
					farLineProtected = 1,
					dpsId = '$dpsId',
					doResync = '$doResync',
					transProtocol = '$srcTransportProtocol',
					tmId = '$tmId'
				  WHERE
					hostId IN ('$all_src_host_Ids') AND deviceName='$escSrcDevice'";
		$result_src = $this->conn->sql_query($query_src);
				
		$clusterTgtIds = array();
		$clusterTgtIds[] = $dstId;
		$check_xen = $this->check_xen_server($dstId);
		
		if($check_xen)
		{
			$clusterTgtIds = $this->get_clusterids_as_array($dstId, $escDstDevice);
		}
		$clusterTgtIds_str = implode("','",$clusterTgtIds);
		
		$query_trg = "UPDATE
					logicalVolumes
				  SET
					maxDiffFilesThreshold = '$maxDiffFilesThreshold',
					doResync = '$doResync',
					visible = 0,
					isVisible = 0,
					deviceFlagInUse = '$inUse',
					transProtocol = '$dstTransportProtocol',
					tmId = '$tmId'
				  WHERE
					hostId IN ('$clusterTgtIds_str') AND deviceName = '$escDstDevice'";
		$result_trg = $this->conn->sql_query($query_trg);
		
		/* 
			-- END
			Code to update LogicalVolume table for SRC and TRG
		*/
		
		/* 
			Code to update hosts table for SRC and TRG
			-- START
		*/
		if (1 == $ps_natip_source)
		{
			$sql1 = "UPDATE hosts SET usepsnatip=1
					 WHERE id in ('$all_src_host_Ids')";
			$result_host_src = $this->conn->sql_query($sql1);
		}
		
		if (1 == $ps_natip_target)
		{
			$sql1 = "UPDATE hosts SET usepsnatip=1
					 WHERE id='$dstId'";
			$result_host_trg = $this->conn->sql_query($sql1);
		}
		/* 
			-- END
			Code to update hosts table for SRC and TRG
		*/
		$src_os_flag = $this->commonfunctions_obj->get_osFlag($srcId);
				
		$os_flag = $this->commonfunctions_obj->get_osFlag($dstId);
				
        if (1 == $medRet)
		{
			$retention_obj->insert_retLogPolicy($newRuleIds, $logType,
                                                $logPolicy, $retSizeInBytes,
                                                $metaFilePath, $logDataDir,
                                                $volume1, $volume2,$timeRecFlag,
                                                $hidTime,$change_by_time_day,
                                                $change_by_time_hr,
                                                $txtlocdir_move,$txtdiskthreshold,
                                                $txtlogsizethreshold,
                                                $logExceedPolicy,$sparse_enable);
            
            $uniqueID = $sparseretention_obj->get_uniqueId($newRuleIds[0]);
            			
            $retention_dir = $logDataDir;
            //$previous_ret_path = $ret_arr['previous_ret_path'];
            
            $logDeviceName =  $ret_arr['logDeviceName'];
            
            $policy_type = $ret_arr['policy_type'];
            
			$retention_path = $retention_obj->get_catalogPath($srcId,$escSrcDevice,$dstId,$escDstDevice,$retention_dir,$os_flag,$uniqueID,$sparse_enable);
			
	        $window_count = 1;
	        for($i=1;$i<=5;$i++)
			{
				if($ret_arr['hid_add_remove'.$i] == 'Disable')
				{
					continue;
				}
				else
				{
					if($i == 1)
					{
						$application_name = 'all';
						$tag_count = 0;
						$user_defined_tag = "";
					}
					else
					{
						$application_name = $ret_arr['hid_application'.($i-1)];
						$tag_count = $ret_arr['hid_bookmark'.($i-1)];
						$user_defined_tag = $ret_arr['hid_userDefined'.($i-1)];
					}
					$retention_time_windows[$window_count] = array( 'windowId'=>$i,
																	'application_name'=>$application_name,
																	'granularity'=>$ret_arr['hid_granularity'.$i],
																	'granularity_unit'=>$ret_arr['hid_granularity_range'.$i],
																	'time_interval'=>$ret_arr['hid_seltime'.$i],
																	'time_interval_unit'=>$ret_arr['hid_seltime_range'.$i],
																	'tag_count'=>$tag_count,
																	'user_defined_tag'=>$user_defined_tag
																	);
					$log_management[$window_count] = array( 'windowId'=>$i,
															'on_insufficient_disk_space'=>$ret_arr['logExceedPolicy'],
															'alert_threshold'=>$ret_arr['txtdiskthreshold'],
															'storage_path'=>$retention_dir,
															'storage_device_name'=>$logDeviceName,
															'retentionPath'=>$retention_path
															); 
					$window_count++;
				}
			}
            
			if(($policy_type == 1) || ($policy_type == 2))
	        {
	            $sparseretention_obj->insert_timewindow($dstId,$dstDevice,$retention_time_windows,$log_management,$this);
			}
	        if(($policy_type == 0) || ($policy_type == 2))
	        {
				$sparseretention_obj->insert_spacepolicy($dstId,$dstDevice,$volume1, $volume2,$retSizeInBytes,$retention_dir,$logExceedPolicy,$txtdiskthreshold,$retention_path,'',$this); 
			}
        }
		
		/* Code to Create Src/Trg Cache Dir*/
		
		$input_dir = $this->commonfunctions_obj->make_path($REPLICATION_ROOT."/".$srcId."/".$escSrcDevice);
        $output_dir = $this->commonfunctions_obj->make_path($REPLICATION_ROOT."/".$dstId."/".$escDstDevice);
		
		/* Collection of source ids for cluster nodes*/
		$source_ids = $clusterIds;
		/*Creating the source host id folders with device names in the configuration server*/
		foreach ($source_ids as $key=>$source_id)
		{
			$formatted_device = $this->commonfunctions_obj->verifyDatalogPath($escSrcDevice);

			if ($src_os_flag != 3)
			{
				$formatted_device = str_replace(':', '', $formatted_device);
			}
			$formatted_device = str_replace('\\', '/', $formatted_device);

			$input_dir = $REPLICATION_ROOT."/".$source_id."/".$formatted_device;
			if(!file_exists($input_dir))
			{
				$mkdir_output = mkdir($input_dir, 0777, true);
				if($this->commonfunctions_obj->is_linux())
				{
					$guid_folder = $this->commonfunctions_obj->make_path($REPLICATION_ROOT."/".$source_id);
					$iterator = new RecursiveIteratorIterator(
								new RecursiveDirectoryIterator($guid_folder), RecursiveIteratorIterator::SELF_FIRST); 
					foreach($iterator as $item) 
					{ 
						$chmod_status = chmod($item, 0777); 
					}     
				}
			}
		}
		
		$formatted_device = $this->commonfunctions_obj->verifyDatalogPath($escDstDevice);
		if ($os_flag != 3)
		{
			$formatted_device = str_replace(':', '', $formatted_device);
		}
		$formatted_device = str_replace('\\', '/', $formatted_device);

		$output_dir = $REPLICATION_ROOT."/".$dstId."/".$formatted_device;
		if(!file_exists($output_dir))
		{
			$mkdir_output = mkdir($output_dir, 0777, true);
			if($this->commonfunctions_obj->is_linux())
			{
				$guid_folder = $this->commonfunctions_obj->make_path($REPLICATION_ROOT."/".$dstId);
				$iterator = new RecursiveIteratorIterator(
							new RecursiveDirectoryIterator($guid_folder), RecursiveIteratorIterator::SELF_FIRST); 
				foreach($iterator as $item) 
				{ 
					$chmod_status = chmod($item, 0777); 
				}
			}
		}
		
		$flag = $this->create_protection_rep_data($srcId, $escSrcDevice, $dstId, $escDstDevice, $willBeOneToN);
		
		$logging_obj->my_error_handler("DEBUG","add_pair final point");
	}

    /*
     * Function Name: update_do_resync
     *
     * Description:
     *    It updates the doResync field
     *    handles 1-to-1, 1-to-N, cluster, and cluster 1-to-N
     *
     * Parameters:
     *     Param 1 [IN/OUT]:$rs
     *     Param 2 [IN/OUT]:$doResync
     *     Param 1 [IN/OUT]:$srcDevice
     *     Param 2 [IN/OUT]:$dstId
     *     Param 1 [IN/OUT]:$dstDevice
     *
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    private function update_do_resync($rs, $doResync, $srcDevice, $dstId,
			      $dstDevice)
    {
	$this->conn->sql_data_seek($rs,0);
        $rowArray = $this->conn->sql_fetch_array($rs);
        $srcIdsIn = "'$rowArray[sourceHostId]'";
        while ($rowArray = $this->conn->sql_fetch_array($rs))
        {
            $srcIdsIn = $srcIdsIn . ", '$rowArray[sourceHostId]'";
        }

	$query="UPDATE
		    logicalVolumes " . "
		SET
		    doResync = $doResync " . "
		WHERE
		    (hostId IN ($srcIdsIn) AND
		    deviceName = '".$srcDevice."') or "."
		    (hostId = '$dstId' AND
		    deviceName = '".$dstDevice."')";

	if (!$this->conn->sql_query($query))
	{
	    return false;
	}
    }

    /*
     * Function Name: resync_now
     *
     * Description:
     *    This function is responsible for forking multiple processes that
     *    performs the following actions.
     *
     *   forces a resync when a user requests it after hiding a trager volume
     *   handles 1-to-1, 1-to-N, cluster, and cluster 1-to-N
     *
     * Parameters:
     *     Param 1 [OUT]:$ruleId
     *
     * Return Value:
     *     Ret Value: Returns value of $ruleId, $resyncVer & $db_handle .
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function resync_now($ruleId)
    {
        $result = $this->restart_resync($ruleId);
		return $result;
    }

    /*
     * Function Name: reset_cluster_rule_id
     *
     * Description:
     *    resets clusters.clusterRuleId to 0 for idsIn and deviceName
     *
     * Parameters:
     *     Param 1 [IN/OUT]:$idsIn - string of hostIds common separated to reset
     *     Param 2 [IN/OUT]:$deviceName - deviceName to reset
     *
     * Return Value:
     *     Ret Value: No returns value .
     *
     * Exceptions:
     *      DataBase Connection fails.
    */
    private function reset_cluster_rule_id($idsIn, $deviceName)
    {
        if (!$this->conn->sql_query("update ".
                                    "  clusters ".
                                    "set ".
                                    "  clusterRuleId = 0 ".
                                    "where ".
                                    "  hostId in ($idsIn) ".
                                    "  and deviceName = '$deviceName'")) {
            // report error
        }
    }


    /*
     * Function Name: delete_pair
     *
     * Description:
     *    It deletes a pair
     *    handles 1-to-1, 1-to-N, cluster, and cluster 1-to-N
     *
     * Parameters:
     *     Param 1 [IN]:$srcId
     *     Param 2 [IN]:$srcDevice
     *     Param 3 [IN]:$dstId
     *     Param 4 [IN]:$dstDevice
     *     Param 5 [IN]:$deleteSnapShots=1
     *     Param 6 [IN]:$deleteAllClusterNodes=1 - 0: only delete the pair for the given srcId
     *                                             1: delete all related cluster pairs
     *
     * Return Value:
     *     Ret Value: No returns value .
     *
     * Exceptions:
     *      DataBase Connection fails.
    */
    public function delete_pair($srcId, $srcDevice, $dstId, $dstDevice, $deleteSnapShots=1, $deleteAllClusterNodes=1,$action = '')
    {
	global $system_obj,$app_obj,$logging_obj;
	$escSrcDevice = $this->conn->sql_escape_string($srcDevice);
	$escDstDevice = $this->conn->sql_escape_string($dstDevice);

	$oneToN = $this->is_one_to_many_by_source($srcId, $srcDevice);
	$isPrimary = $this->is_primary_pair($srcId, $srcDevice, $dstId, $dstDevice);

	
    $idsIn = "'$srcId'";    
	$query="SELECT
				profilingId,
				pairId
			FROM
				srcLogicalVolumeDestLogicalVolume " . "
			WHERE
				sourceHostId in ($idsIn)
				AND
				 sourceDeviceName = '$escSrcDevice' " . "
				AND
				destinationHostId = '$dstId'
				AND  destinationDeviceName = '$escDstDevice'";
        $result_set_profile = $this->conn->sql_query($query);

        if ($this->conn->sql_num_row($result_set_profile) >= 1)
        {
            $profile_object = $this->conn->sql_fetch_object($result_set_profile);
			$pair_id = $profile_object->pairId;
			if(!$oneToN)
			{
				$query="DELETE
				FROM
					profilingDetails
				WHERE
					profilingId = '$profile_object->profilingId'";

				$this->conn->sql_query($query);
			}
        }
		
		#Fix for #13609
		 /*
         only delete files for this pair even if it is part of 1-to-N
        */
        $this->remove_one_to_one_files($srcId,
                                       $escSrcDevice,
                                       $dstId,
                                       $escDstDevice);
									   								
		#Fix for #13609 changed idsIn to srcId							   
		$query="DELETE
		FROM
		    srcLogicalVolumeDestLogicalVolume " . "
		WHERE
		    sourceHostId
		IN
		    ('$srcId') AND
		    sourceDeviceName = '$escSrcDevice' "." AND
		    destinationHostId = '$dstId' AND
		    destinationDeviceName = '$escDstDevice'";


        if (!$this->conn->sql_query($query))
        {
        }

        if ($oneToN)
        {
            $this->reassign_primary_pair($srcId,
                                         $escSrcDevice);
            $clearDps = false;
        }
        else
        {
            $clearDps = true;
        }

        /*
         only delete files for this pair even if it is part of 1-to-N
        */
		$logging_obj->my_error_handler("DEBUG","delete_pair:: callg clear_logicalVolume_pair_settings function with srcId: $srcId, escSrcDevice: $escSrcDevice, dstId: $dstId, escDstDevice: $escDstDevice, clearDps:$clearDps, action: $action \n"); 
        $this->clear_logicalVolume_pair_settings($srcId,
                                                 $escSrcDevice,
                                                 $dstId,
                                                 $escDstDevice,
                                                 $clearDps,
												 $action);

        $this->remove_protection_rep_data($srcId, $escSrcDevice,
                                          $dstId, $escDstDevice, $oneToN, $isPrimary);

        // must be called after remove_protection_rep_data
        $this->delete_retention($dstId);
        $this->delete_ruleids();

		$this->resetVolResizeHistory($idsIn, $escSrcDevice, $pair_id);
		#13214 Fix
		$dst_reset_id="'$dstId'";
		$this->resetVolResizeHistory($dst_reset_id, $escDstDevice, $pair_id);

        $query="SELECT
		    ipaddress
		FROM
		    hosts
		WHERE
		    id='$srcId' AND
		    (sentinelEnabled='1' or outpostAgentEnabled='1')";

        $result_set = $this->conn->sql_query($query);

        $myrow = $this->conn->sql_fetch_object($result_set);
        $srcIp = $myrow->ipaddress;

        $query="SELECT
		    ipaddress
		FROM
		    hosts
		WHERE
		    id='$dstId' AND
		    (sentinelEnabled='1' or outpostAgentEnabled='1')";

        $result_set = $this->conn->sql_query($query);
        $myrow = $this->conn->sql_fetch_object($result_set);
        $destIp = $myrow->ipaddress;
		$user_name = 'System';
		if($_SESSION['username'])
		{
			$user_name = $_SESSION['username'];
		}
        $this->commonfunctions_obj->writeLog($user_name,"Replication pair deleted for pair($srcIp($escSrcDevice),$destIp($escDstDevice))");

    }
	
		/*
     * Function Name: resetVolResizeHistory
     *
     * Description:
     *    To reset the volume resize history as already processed.
     *
     * Parameters:
     *     Param 1 [IN/OUT]:$source_host_id (source host identity)
     *     Param 2 [IN/OUT]:$source_device_name (source device name)
     *
     * Return Value:
     *
     * Exceptions:
	 *
    */
	private function resetVolResizeHistory($source_host_ids, $source_device_name, $pair_id)
	{
			global $logging_obj;
			$sql = "UPDATE
						volumeResizeHistory
					SET
						isValid = 0
					WHERE
					hostId
					IN ($source_host_ids)
					AND
						deviceName = '$source_device_name'
					AND 
					    pairId = '$pair_id'";

			$query_status = $this->conn->sql_query($sql);
			
			// need to reset the node devices volumeResize history also since 
			// they are part of volumeResize.
			$sqlStmnt = "select 
								Phy_Lunid 
						from 
								srcLogicalVolumeDestLogicalVolume 
						where 
								sourceHostId = $source_host_ids 
						and 
								sourceDeviceName = '$source_device_name'
						and 
								Phy_Lunid != ''";
			$logging_obj->my_error_handler("DEBUG","resetVolResizeHistory:: $sqlStmnt\n");						
			$resStmnt = $this->conn->sql_query($sqlStmnt);
			$num_rows = $this->conn->sql_num_row($resStmnt);
			if($num_rows>0)
			{
				$row = $this->conn->sql_fetch_object($resStmnt);
				$scsiId = $row->Phy_Lunid;
				$logging_obj->my_error_handler("DEBUG","resetVolResizeHistory:: scsiId:$scsiId\n");
				$query ="update 	
							volumeResizeHistory vrh, 
							srcLogicalVolumeDestLogicalVolume sldl, 
							logicalVolumes lv 
						set 
							isValid = 0 
						where 
							lv.hostId = vrh.hostId 
						and 
							lv.deviceName = vrh.deviceName 
						and 
							lv.Phy_Lunid = sldl.Phy_Lunid 
						and 
							vrh.pairId = '$pair_id'
						and 
							vrh.isValid = 1 
						and 
							lv.Phy_Lunid='$scsiId'";
				$logging_obj->my_error_handler("DEBUG","resetVolResizeHistory:: $query\n");			
				$result_set = $this->conn->sql_query($query);
			}			
			
	}
	
	private function create_protection_rep_data	($srcId, $srcDevice, $dstId, $dstDevice, $oneToN, $call_from='')
	{

		global $INSTALLATION_DIR;
		global $commonfunctions_obj, $REPLICATION_ROOT;
		
		$src_os_flag = $this->commonfunctions_obj->get_osFlag($srcId);
		
		if(!$oneToN)
		{			
			if(!$this->commonfunctions_obj->is_linux())
			{
				$amethyst_details = $this->commonfunctions_obj->amethyst_values();
				$rrd_install_path = $this->amethyst_details['RRDTOOL_PATH'];
				$rrd_install_path = addslashes($rrd_install_path);
				$rrdcommand = $rrd_install_path."\\rrdtool\\Release\\rrdtool.exe";
			}
	        else
	        {
	            $rrdcommand = "rrdtool";
	        }

			$list = $this->getProtectionHosts($srcId, $srcDevice);
			
			foreach ($list as $srcId => $srcDevice)
			{
				$formatted_device = $this->commonfunctions_obj->verifyDatalogPath($srcDevice);
				if($src_os_flag != 3)
				{
					$formatted_device = str_replace(':', '', $formatted_device);
				}
				$formatted_device = str_replace('\\', '/', $formatted_device);

		        if(!$this->commonfunctions_obj->is_linux())
		        {
		            $rrd_filepath = "$REPLICATION_ROOT/".$srcId."/".$formatted_device;
		        }
		        else
		        {
		            $rrd_filepath = $INSTALLATION_DIR."/".$srcId."/".$formatted_device;
		        }
				
				$rpo_rrd = $rrd_filepath."/rpo.rrd";
				$retention_rrd = $rrd_filepath."/retention.rrd";
				$health_rrd = $rrd_filepath."/health.rrd";
				$datamode_rrd = $rrd_filepath."/datamode.rrd";

				$perf_txt = $rrd_filepath."/perf.txt";
				$rpo_txt = $rrd_filepath."/rpo.txt";
				$datamode_txt = $rrd_filepath."/datamode.txt";
				$dm_txt = $rrd_filepath."/dm.txt";

				if(!file_exists($rrd_filepath))
				{
					$mkdir_output = mkdir($rrd_filepath, 0777, true);

					if($this->commonfunctions_obj->is_linux())
					{
						chmod ($rrd_filepath, 0777);
					}
				}

				if(file_exists($rrd_filepath))
				{					
					if (file_exists($perf_txt)) @unlink($perf_txt);
					if (file_exists($rpo_txt)) @unlink($rpo_txt);

					if($call_from != 'remove_protection_rep_data')
					{
						if (file_exists($datamode_rrd)) @unlink($datamode_rrd);
						if (file_exists($datamode_txt)) @unlink($datamode_txt);
						if (file_exists($dm_txt)) @unlink($dm_txt);
					}

					#Bug:-6920:- Fix for RPO misreporting
					$monitor_file = $rrd_filepath."/diffs/monitor.txt";
					if (file_exists($monitor_file)) @unlink($monitor_file);
				}
			}
		}
	}
    /*
     * Function Name: remove_protection_rep_data
     *
     * Description:
     *    It removes the protection report data when a pair got deleted
     *
     * Parameters:
     *     Param 1 [IN]: Source Host Id
     *     Param 2 [IN]: Source Device Name
     *     Param 3 [IN]: Dest Host ID
     *     Param 4 [IN]: Dest Device Name
     *     Param 5 [IN]: One to N flag
     *
     * Return Value: none
    */
	public function remove_protection_rep_data($sourceId, $sourceDevice, $dstId, $dstDevice, $oneToN, $isPrimary)
    {
		global $INSTALLATION_DIR;
		global $commonfunctions_obj;
		if(!$isPrimary) return;

		$list = $this->getProtectionHosts($sourceId, $sourceDevice);
        $src_os_flag = $commonfunctions_obj->get_osFlag($sourceId);
		$dstDevice = addslashes($this->commonfunctions_obj->verifyDatalogPath($dstDevice));
		foreach ($list as $srcId => $srcDevice)
		{
			$formatted_device = $this->commonfunctions_obj->verifyDatalogPath($srcDevice);
            if($src_os_flag != 3)
            {
			    $formatted_device = str_replace(':', '', $formatted_device);
            }
			$formatted_device = str_replace('\\', '/', $formatted_device);

			if(!$this->commonfunctions_obj->is_linux())
			{
				$rrd_filepath = $REPLICATION_ROOT.$srcId."/".$formatted_device;
				$sentinel_file = $REPLICATION_ROOT.$srcId."\\".$formatted_device."\\"."sentinel.txt";
			}
			else
			{
				$rrd_filepath = $INSTALLATION_DIR."/".$srcId."/".$formatted_device;
				$sentinel_file = $REPLICATION_ROOT.$srcId."/".$formatted_device."/sentinel.txt";
			}

			$rpo_rrd = $rrd_filepath."/rpo.rrd";
			$retention_rrd = $rrd_filepath."/retention.rrd";
			$health_rrd = $rrd_filepath."/health.rrd";
			$datamode_rrd = $rrd_filepath."/datamode.rrd";

			$perf_txt = $rrd_filepath."/perf.txt";
			$perf_cs_txt = $rrd_filepath."/perf_cs.txt";
			$perf_ps_txt = $rrd_filepath."/perf_ps.txt";
			$rpo_txt = $rrd_filepath."/rpo.txt";
			$datamode_txt = $rrd_filepath."/datamode.txt";
			$dm_txt = $rrd_filepath."/dm.txt";

			$perf_log = $rrd_filepath."/perf.log";
			$rpo_log = $rrd_filepath."/rpo.log";
			$monitor_file = $rrd_filepath."/diffs/monitor.txt";

			if (file_exists($rpo_rrd)) @unlink($rpo_rrd);
			if (file_exists($retention_rrd)) @unlink($retention_rrd);
			if (file_exists($health_rrd)) @unlink($health_rrd);

			if (file_exists($perf_txt)) @unlink($perf_txt);
			if (file_exists($rpo_txt)) @unlink($rpo_txt);

			if(!$oneToN)
			{
				if (file_exists($datamode_rrd)) @unlink($datamode_rrd);
				if (file_exists($datamode_txt)) @unlink($datamode_txt);
				if (file_exists($dm_txt)) @unlink($dm_txt);
				if (file_exists($monitor_file)) @unlink($monitor_file);
				if (file_exists($sentinel_file)) @unlink($sentinel_file);
				if (file_exists($perf_cs_txt)) @unlink($perf_cs_txt);
				if (file_exists($perf_ps_txt)) @unlink($perf_ps_txt);
			}

			if (file_exists($perf_log)) @rename($perf_log, $perf_log.'_'.time());
			if (file_exists($rpo_log)) @rename($rpo_log, $rpo_log.'_'.time());

			$srcDevice = addslashes($this->commonfunctions_obj->verifyDatalogPath($srcDevice));

			$sql = "DELETE
					FROM
						policyViolationEvents
					WHERE
						sourceHostId='$srcId' AND
						 sourceDeviceName='$srcDevice' AND
						destinationHostId='$dstId' AND
						 destinationDeviceName='$dstDevice'";
			$this->conn->sql_query($sql);

			$delete_sql = "DELETE
					       FROM
						       healthReport
					       WHERE
								sourceHostId='$srcId' AND
								 sourceDeviceName='$srcDevice' AND
								destinationHostId='$dstId' AND
								 destinationDeviceName='$dstDevice'";
			#$this->conn->sql_query($delete_sql);
			
			$delete_trending_data_sql = "
										DELETE
											FROM
												trendingData 
											WHERE
												sourceHostId='$srcId' AND
												sourceDeviceName='$srcDevice' AND
												destinationHostId='$dstId' AND
												destinationDeviceName='$dstDevice'";
			
			#$this->conn->sql_query($delete_trending_data_sql);
		}

		if($oneToN)
		{
			$sourceDevice = addslashes($this->commonfunctions_obj->verifyDatalogPath($sourceDevice));
			$sql = "SELECT
						destinationHostId,
						destinationDeviceName
					FROM
						srcLogicalVolumeDestLogicalVolume
					WHERE
						sourceHostId = '$sourceId' AND
						 sourceDeviceName = '$sourceDevice' AND
						oneToManySource = '1'";

			$result = $this->conn->sql_query($sql);
			$row = $this->conn->sql_fetch_array($result);

			$new_dstId = $row['destinationHostId'];
			$new_dstDevice = $row['destinationDeviceName'];

			if($new_dstId && $new_dstDevice)
			{
				$this->create_protection_rep_data ($sourceId, $sourceDevice, $new_dstId, $new_dstDevice, 0, 'remove_protection_rep_data');
			}
		}
	}

    /*
     * Function Name: getProtectionHosts
     *
     * Description:
     *    It collects all the node information for Cluster case
     *
     * Parameters:
     *     Param 1 [IN]: Source Host ID
     *     Param 2 [IN]: Source Volume
     *
     * Return Value:
     *     Ret Value: Array of all nodes
     *
     * Exceptions:
     *     no
    */
	private function getProtectionHosts($srchostid, $srcvol)
	{
		$source_vol = addslashes($this->commonfunctions_obj->verifyDatalogPath($srcvol));
		$host_list = array();

		$sql = "SELECT
					count(*) as num_entries
				FROM
					clusters
				WHERE
					hostId='$srchostid' AND
					 deviceName='$source_vol'" ;

		$result = $this->conn->sql_query($sql);
		
		$row = $this->conn->sql_fetch_array($result);

		if($row['num_entries'] > 0)
		{
			$sql = "SELECT
						clusterId
					FROM
						clusters
					WHERE
						hostId='$srchostid' AND
						 deviceName='$source_vol'";

			$result = $this->conn->sql_query($sql);
			
			$row = $this->conn->sql_fetch_array($result);
			$clusterId = $row['clusterId'];

			$sql = "SELECT
						hostId,
						deviceName
					FROM
						clusters
					WHERE
						 deviceName='$source_vol' AND
						clusterId = '$clusterId'";

			$result = $this->conn->sql_query($sql);
			
		    while($row = $this->conn->sql_fetch_array($result))
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

    /*
     * Function Name: get_format_dev_name
     *
     * Description:
     *    It gives the format device name.
     *
     * Parameters:
     *     Param 1 [IN]:$hostId
     *     Param 2 [IN]:$devName
     *
     * Return Value:
     *     Ret Value: Returns value of $devName.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function get_format_dev_name($hostId,$devName)
    {
        $devName = str_replace("\\","/",$devName);

	return $devName;
    }

   /*
    * Function Name: is_one_to_many_by_source
    *
    * Description:
    *    This function is responsible for forking multiple processes that
    *    performs the following actions.
    *
    *
    *
    *
    * Parameters:
    *     Param 1 [IN]:$src_id
    *     Param 2 [IN]:$src_dev
    *
    * Return Value:
    *     Ret Value: Returns Boolean value .
    *
    * Exceptions:
    *     DataBase Connection fails.
   */
    public function is_one_to_many_by_source ($src_id, $src_dev)
    {
        global $commonfunctions_obj;
        $src_dev   = $this->commonfunctions_obj->verifyDatalogPath($src_dev);
        $src_dev = $this->conn->sql_escape_string ($src_dev);

	$query="SELECT
	            count(*)
		FROM
		    srcLogicalVolumeDestLogicalVolume "."
		WHERE
		    sourceHostId = '$src_id' AND
		    sourceDeviceName='$src_dev'";

	$result_set = $this->conn->sql_query ($query);
	$row = $this->conn->sql_fetch_row ($result_set);
        if ($row[0] > 1)
	{
	    return 1;
	}
	else
	{
	    return 0;
	}
    }

   /*
    * Function Name: is_primary_pair
    *
    * Description:
    *    This function is checks if the replication pair is primary
    *
    * Parameters:
    *     Param 1 [IN]: Source ID
    *     Param 2 [IN]: Source Device
    *     Param 3 [IN]: Destination ID
    *     Param 4 [IN]: Destination Device
    *
    * Return Value:
    *     Ret Value: Returns Boolean value .
    *
    * Exceptions:
    *     DataBase Connection fails.
   */
	public function is_primary_pair($srcId, $srcDevice, $dstId, $dstDevice)
	{
		$srcDevice = $this->conn->sql_escape_string ($srcDevice);
		$dstDevice = $this->conn->sql_escape_string ($dstDevice);

		$sql = "SELECT
					oneToManySource
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					sourceHostId='$srcId' AND
					 sourceDeviceName='$srcDevice' AND
					destinationHostId='$dstId' AND
					 destinationDeviceName='$dstDevice'";

		$result_set = $this->conn->sql_query ($sql);
		$row = $this->conn->sql_fetch_row ($result_set);
	    if ($row[0] == 1)
		{
		    return 1;
		}
		else
		{
		    return 0;
		}
	}
	
   /*
    * Function Name: is_device_clustered
    *
    * Description:
    *    This function is responsible for forking multiple processes that
    *    performs the following actions.
    *
    *    Checking, device is clustered or not?
    *
    *
    * Parameters:
    *     Param 1 [IN]:$hostid
    *     Param 2 [IN]:$deviceName
    *
    * Return Value:
    *     Ret Value: Returns Boolean value .
    *
    * Exceptions:
    *     DataBase Connection fails.
   */
    public function is_device_clustered ($hostid, $deviceName)
    {

            # Cluster mount point change
        $deviceName =$this->get_unformat_dev_name($hostid,$deviceName);
        $deviceName   = $this->commonfunctions_obj->verifyDatalogPath($deviceName);
        $deviceName = $this->conn->sql_escape_string ($deviceName);

        $iter = $this->conn->sql_query ("select * from clusters where hostId='$hostid' and  deviceName='$deviceName'");
    //	debugLogC ("Is device clustered: select * from clusters where hostId='$hostid' and deviceName='$deviceName'\n");

        if ($this->conn->sql_num_row($iter) > 0)
        {
            return TRUE;
        }
        else
        {

            return FALSE;
        }
    }

   /*
    * Function Name: get_unformat_dev_name
    *
    * Description:
    *    This function is responsible for forking multiple processes that
    *    performs the following actions.
    *
    *
    *
    *
    * Parameters:
    *     Param 1 [IN]:$hostId
    *     Param 2 [IN]:$devName
    *
    * Return Value:
    *     Ret Value: Returns Device name.
    *
    * Exceptions:
    *     DataBase Connection fails.
   */
    public function get_unformat_dev_name($hostId,$devName)
    {
        /*
	Cluster mount point change
	*/
	$host_id_array = explode(",",$hostId);
	$hostId = implode("','",$host_id_array);
	$sql = "SELECT
				osFlag
			FROM
				hosts
			WHERE
				id IN ('$hostId')" ;

	$iter = $this->conn->sql_query ($sql );
	$myrow = $this->conn->sql_fetch_row ($iter);
	$osFlag =$myrow[0];
	if($osFlag == 1 )
	{
	    $devName = str_replace("/","\\",$devName);
	}
	else
	{
	    $devName = str_replace("\\","/",$devName);
	}

	return $devName ;
    }

    /*
     * Function Name: get_volumegroup_id
     *
     * Description:
     *  Gets the volumeGroupId from srcLogicalVolumeDestLogicalVolume.
     *
     * Parameters:
     *     Param 1 [IN]:$vardb
     *     Param 2 [IN]:$dest_guid
     *     Param 3 [IN]:$dest_dev
     *     Param 4 [OUT]:
     *
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function get_volumegroup_id($dest_guid, $dest_dev)
    {
        $sql = "SELECT
                    volumeGroupId
                FROM
                    srcLogicalVolumeDestLogicalVolume
                WHERE
                    destinationHostId='$dest_guid'
                    AND
                     destinationDeviceName='$dest_dev'";
        $rs = $this->conn->sql_query($sql);
        $ret_nrows = $this->conn->sql_num_row($rs);
        $volumeGroupId="";
        if ($ret_nrows != 0)
        {
            $fetch_res     = $this->conn->sql_fetch_object($rs);
            $volumeGroupId = $fetch_res->volumeGroupId;
        }
        return $volumeGroupId;
    }

    /*
     * Function Name: <clear_replication>
     *
     * Description:
     *  Clears the Replication pair.
     *
     *
     * Parameters:
     *     Param 1 [IN]:$newdb
     *     Param 2 [IN]:$pairs
     *     Param 3 [IN]:$flag=1
     *     Param 2 [OUT]:
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
     */
    public function clear_replication($pairs,$flag=1)
    {
        global $recovery_obj;
        global $retention_obj;
        global $bandwidth_obj,$logging_obj,$LOG_ROOT;
		$logging_obj->global_log_file = $LOG_ROOT . 'clear_replication.log';
		$logging_obj->my_error_handler("INFO",array("ReplicationPairs"=>$pairs),Log::BOTH);
        $varWan = "WAN";
        $pairs_list = explode (";", $pairs);

        foreach ($pairs_list as $pair)
        {
            if (strcmp($pair, ''))
            {
                list ($source_id, $source_device_name, $dest_id,
                      $dest_device_name)  = explode ("=", $pair, 4);
                /* For clusters*/
                $source_id_list = explode (",", $source_id);
				
                $ifCluster = count($source_id_list);

				$dest_device_name   = $this->commonfunctions_obj->verifyDatalogPath($dest_device_name);
				$source_device_name = $this->commonfunctions_obj->verifyDatalogPath($source_device_name);

                foreach ($source_id_list as $s)
                {
                    if (strcmp($s, ""))
                    {
                        if ($this->is_device_clustered ($s, $source_device_name))
                        {   

							$cRuleId =$this->get_cluster_details ($s,
                                                           $source_device_name);
                            $rId = $cRuleId['clusterruleId']."#!#".$dest_id.
                                    "#!#".$dest_device_name;

							#$recovery_obj->del_snapshot($rId, $dest_id,
                              #                          $dest_device_name,$flag);


                            $rId =$this->get_cluster_pairs_ruleId($s,
                                                                $source_device_name,
                                                                $dest_id,
                                                                $dest_device_name);
                            $rId = $rId."#!#".$dest_id."#!#".$dest_device_name;

							

                        }
                        else
                        {
                            $rId =$this->get_ruleId ($s, $source_device_name,
                                                    $dest_id,
                                                    $dest_device_name);
                            #$recovery_obj->del_snapshot($rId, $dest_id,
                             #                       $dest_device_name,$flag);
                        }


                        $retention_obj->del_tags($rId);
						$retention_obj->del_timeranges($rId);
						$retention_obj->delete_from_pairCatLogDetails($s, $source_device_name, $dest_id, $dest_device_name);

                        $retention_obj->change_rep_history_status($s,
                                                            $source_device_name,
                                                            $dest_id,
                                                            $dest_device_name);
                        $retention_obj->setRetLogHistoryFlag($s,
                                                            $source_device_name,
                                                            $dest_id,
                                                            $dest_device_name);
                        if(isset($_POST['ids']) && (strlen($ids)>0))
                        {
                            setDeleteFlag($ids);
                        }
						##8546-start
                        $sql = "Select
						            processServerId
								From
								    srcLogicalVolumeDestLogicalVolume
								Where
  								    sourceHostId = '$s'
								AND
								    destinationHostId='$dest_id'
								AND
								    sourceDeviceName = '".$this->conn->sql_escape_string($source_device_name)." '
								AND
								    destinationDeviceName = '".$this->conn->sql_escape_string($dest_device_name)."'";

						$result_set = $this->conn->sql_query($sql);
						$row = $this->conn->sql_fetch_row($result_set);



                        #$bandwidth_obj->recomputeAndUpdatePoliciesAllocation($varWan,1,$row[0],$dest_id);
						##8546-End
                        $this->stop_replication($s, $source_device_name,
                                                $dest_id, $dest_device_name,$flag);

                        $logging_obj->my_error_handler("INFO",array("HostId"=>$s),Log::BOTH);
                         $this->cleanupPolicy($s);
                    }
                }
            }
        }
    }

    /*
     * Function Name: get_new_ruleId
     *
     * Description:
     * Returns a random number that is not in use as a ruleId
     *
     *
     * Parameters:
     *     Param 1 [OUT]:
     *
     * Return Value:
     *     Ret Value: Returns scalar value .
     *
     * Exceptions:
     *     DataBase Connection fails.
     */
    public function get_new_ruleId ()
    {

        $eids = array ();
        $query = "SELECT
                    ruleId
                  FROM
                    srcLogicalVolumeDestLogicalVolume";
        $result_set = $this->conn->sql_query ($query );
		
        while ($row = $this->conn->sql_fetch_object ($result_set))
        {
            $eids [] = $row-> ruleId;
        }

        $random = mt_rand (1, mt_getrandmax ());
        while (in_array ($random, $eids))
        {
            $random = mt_rand (1, mt_getrandmax ());
        }

        return $random;
    }

    /*
     * Function Name: get_new_clusterruleId
     *
     * Description:
     *
     *   Gets new Cluster ID.
     *
     * Parameters:
     *     Param 1 [OUT]:$random
     *
     *
     * Return Value:
     *     Ret Value: Returns Scalar value .
     *
     * Exceptions:
     *     DataBase Connection fails.
     */
    private function get_new_clusterruleId ()
    {
        $eids = array ();
        $query = "SELECT
                    clusterruleId
                  FROM
                    clusters";
        $result_set = $this->conn->sql_query ($query);

        while ($row = $this->conn->sql_fetch_object ($result_set))
        {
            $eids [] = $row->clusterruleId;
        }

        $random = mt_rand (1, mt_getrandmax ());
        while (in_array ($random, $eids))
        {
            $random = mt_rand (1, mt_getrandmax ());
        }
        return $random;
    }

    /*
     * Function Name:protect_drive
     *
     * Description:
     *
     *
     *
     *
     *
     * Parameters:
     *     Param 1 [IN]:$source_id
     *     Param 2 [IN]:$source_device_name
     *     Param 3 [IN]:$dest_id
     *     Param 4 [IN]:$dest_device_name
     *     Param 5 [IN]:$dpsId
     *     Param 6 [IN]:$medret
     *     Param 7 [IN]:$fast_resync
     *     Param 8 [IN]:$comp_enable
     *     Param 9 [IN]:$volumegroup
     *     Param 10 [IN]:$ftpsSrc
     *     Param 11 [IN]:$ftpsDst
     *     Param 12 [IN]:$type_log
     *     Param 13 [IN]:$log_policy_rad
     *     Param 14 [IN]:$hidUnit
     *     Param 15 [IN]:$txtlocmeta
     *     Param 16 [IN]:$txtlocdir
     *     Param 17 [IN]:$hdrive1
     *     Param 18 [IN]:$hdrive2
     *     Param 19 [IN]:$timeRecFlag
     *     Param 20 [IN]:$hidTime
     *     Param 21 [IN]: $change_by_time_day
     *     Param 22 [IN]:$change_by_time_hr
     *     Param 23 [IN]:$txtlocdir_move
     *     Param 24 [IN]:$txtdiskthreshold
     *     Param 25 [IN]:$txtlogsizethreshold
     *     Param 26 [IN]:$time_lapse
     *     Param 27 [IN]:$window_start_hours
     *     Param 28 [IN]:$window_start_minutes
     *     Param 29 [IN]:$window_stop_hours
     *     Param 30 [IN]:$window_stop_minutes
     *     Param 31 [IN]:$logExceedPolicy
     *
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
     */
    public function protect_drive($source_id, $source_device_name, $dest_id,
                           $dest_device_name, $dpsId, $medret, $fast_resync,
                           $comp_enable, $volumegroup, $ftpsSrc, $ftpsDst,
                           $type_log, $log_policy_rad, $hidUnit, $txtlocmeta,
                           $txtlocdir, $hdrive1, $hdrive2, $timeRecFlag,
                           $hidTime,$change_by_time_day, $change_by_time_hr,
                           $txtlocdir_move,$txtdiskthreshold,
                           $txtlogsizethreshold, $time_lapse,
                           $window_start_hours, $window_start_minutes,
                           $window_stop_hours, $window_stop_minutes,
                           $logExceedPolicy,$lun_process_server_info,$process_server,
                           $ps_natip_source,$ps_natip_target,$sparse_enable = 0,
						   $batch_resync_val=0,$execution_state=0,$scenario_id=0,$plan_id=1,$app_type=0,$ret_arr=array())
    {
		global $logging_obj;
		$logging_obj->my_error_handler("DEBUG","protect_drive called ");
		
		if($ret_arr['exist_plan_name'] )
		{		
			$exist_plan_name = $ret_arr['exist_plan_name'];
			$plan_id = $protection_plan_obj->createPlan($exist_plan_name,3);
		}
		
		if($ret_arr['create_plan'] )
		{
			$create_plan = $ret_arr['create_plan'];
			$plan_id = $protection_plan_obj->createPlan($create_plan,3);
		}
		
		if(empty($plan_id) || (!$plan_id))  $planId=1;
		
		if($lun_process_server_info) 
		{
			// $this->fabricDataInsertion($source_id, $source_device_name, $dest_id, $dest_device_name,$lun_process_server_info,$process_server,$ret_arr);
			// $solution_type = $ret_arr['solution_type'];
			// if (($solution_type == 'PRISM') || $solution_type == 'ARRAY')
			// {
				// $source_id = $process_server;
			// }
		}
		
		$this->add_pair( $source_id, $source_device_name, $dest_id,
							   $dest_device_name, $dpsId, $medret,
							   $fast_resync,$comp_enable, $volumegroup,
							   $ftpsSrc,$ftpsDst, $type_log,
							   $log_policy_rad,$hidUnit, $txtlocmeta,
							   $txtlocdir, $hdrive1, $hdrive2,
							   $timeRecFlag, $hidTime, $change_by_time_day,
							   $change_by_time_hr,$txtlocdir_move,
							   $txtdiskthreshold,$txtlogsizethreshold,
							   $time_lapse,  $window_start_hours,
							   $window_start_minutes, $window_stop_hours,
							   $window_stop_minutes,$logExceedPolicy,
							   $lun_process_server_info,$process_server,
							   $ps_natip_source,$ps_natip_target,$sparse_enable,
							   $batch_resync_val,$execution_state,$scenario_id,$plan_id,$app_type,$ret_arr);
	}

    /*
     * Function Name: get_volume_type
     *
     * Description:
     *    Gets the VolumeType for the given hostId, volume.
     *
     *
     * Parameters:
     *     Param 1 [IN]:$hostid
     *     Param 2 [IN]:$vol
     *
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
     */
    public function get_volume_type($hostid,$vol)
    {
        $query = "SELECT
                    volumeType
                  FROM
                    logicalVolumes
                  WHERE
                    hostId ='".$hostid."'
                    AND
                     deviceName='".$this->conn->sql_escape_string ($vol)."'" ;
        $result_set = $this->conn->sql_query ($query  );
        $row = $this->conn->sql_fetch_object ($result_set);

        return $row->volumeType;

    }

    /*
     * Function Name: get_cluster_pairs_ruleId
     *
     * Description:
     *   Gets the cluster RuleId.
     *
     *
     *
     *
     * Parameters:
     *     Param 1 [IN/OUT]:$src_guid
     *     Param 2 [IN/OUT]:$src_dev
     *     Param 3 [IN/OUT]:$dest_guid
     *     Param 4 [IN/OUT]:$dest_dev
     *
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function get_cluster_pairs_ruleId ($src_guid, $src_dev, $dest_guid, $dest_dev)
    {

        $hostIds = "";
        $ids = array();
        $src_dev = $this->conn->sql_escape_string($src_dev);
        $dest_dev = $this->conn->sql_escape_string($dest_dev);
        if (!$this->db_handle)
        {
            #die ($this->conn->sql_error ());
        }

        $query = "SELECT
                    ruleId
                  FROM
                    srcLogicalVolumeDestLogicalVolume " . "
                  WHERE
                     sourceDeviceName=\"$src_dev\" " . "
                    AND
                    destinationHostId=\"$dest_guid\"
                    AND
                     destinationDeviceName=\"$dest_dev\"";

        $result_set = $this->conn->sql_query ($query );
        if (!$result_set)
        {
            #die ($this->conn->sql_error ());
        }

        if ($this->conn->sql_num_row ($result_set) == 0)
        {
            return false;
        }
        while ($row = $this->conn->sql_fetch_object ($result_set))
        {
            $ids[] = $row->ruleId;
        }
        if (count($ids) > 0)
        {
            $hostIds = implode(",",$ids);
        }

        return $hostIds;

    }

    /*
     * Function Name: is_fabric_lun
     *
     * Description:
     *    Is source a fabric LUN
     *
     * Parameters:
     *     Param 1 [IN]:$sourceId
     *     Param 2 [IN]:$sourceDeviceName
     *     Param 3 [IN]:$destId
     *     Param 4 [IN]:$destDeviceName
     *
     * Return Value:
     *     Ret Value: Returns 1 if Fabric LUN else 0
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function is_fabric_lun($sourceId, $sourceDeviceName,
                           $destId, $destDeviceName)
    {
        $rs =  $this->conn->sql_query("select Phy_lunid from ".
                          "srcLogicalVolumeDestLogicalVolume ".
                          "where ".
                          " sourceHostId = '$sourceId' and ".
                          "  sourceDeviceName = '".
                          $this->conn->sql_escape_string($sourceDeviceName) . "'".
                          " and destinationHostId = '$destId' ".
                          " and   destinationDeviceName = '" .
                         $this->conn->sql_escape_string($destDeviceName) . "'");
        if (!$rs) {
            // report error
            return 0;
        }

        $row = $this->conn->sql_fetch_row($rs);
        if (isset($row[0]) && $row[0] != 0) {
            return 1;
        }
        return 0;
    }
	/*
     * Function Name: is_array_lun
     * Description:
     *    Is source a fabric LUN
     *
     * Parameters:
     *     Param 1 [IN]:$sourceId
     *     Param 2 [IN]:$sourceDeviceName
     *     Param 3 [IN]:$destId
     *     Param 4 [IN]:$destDeviceName
     *
     * Return Value:
     *     Ret Value: Returns 1 if Fabric LUN else 0
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function is_array_lun($sourceId, $sourceDeviceName,
                           $destId, $destDeviceName)
    {
		global $logging_obj;
		$sql = "SELECT  
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
					slvdlv.sourceDeviceName like '%$sourceDeviceName%'
				AND 
					slvdlv.destinationDeviceName like '%$destDeviceName%'
				AND
					slvdlv.scenarioId = appsc.scenarioId
				AND 	
					slvdlv.planId = appsc.planId 
                AND
                    slvdlv.planId = ap.planId";
        $logging_obj->my_error_handler("DEBUG","is_array_lun ::  $sql\n");
		$rs = $this->conn->sql_query($sql);
        if (!$rs) {
            // report error
			$logging_obj->my_error_handler("DEBUG","is_array_lun :: no rs so zero\n");
            return 0;
        }

        $row = $this->conn->sql_fetch_row($rs);
		$logging_obj->my_error_handler("DEBUG","is_array_lun ::  type value = $row[0]\n");
        if (isset($row[0]) && $row[0] == "ARRAY") {
            return 1;
        }
        return 0;
    }
    /*
     * Function Name: stop_protecting_fabric_lun
     *
     * Description:
     *    sets fabric lun state to STOP_PROTECTION_PENDING
     *
     * Parameters:
     *     Param 1 [IN]:$sourceId
     *     Param 2 [IN]:$sourceDeviceName
     *     Param 3 [IN]:$destId
     *     Param 4 [IN]:$destDeviceName
     *
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
     */
    public function stop_protecting_fabric_lun($sourceId,$sourceDeviceName,
                                        $destId,$destDeviceName, $state_field, $state)
    {
        global $STOP_PROTECTION_PENDING;
		global $logging_obj;
		$test="update srcLogicalVolumeDestLogicalVolume ".
                          " set $state_field = $state ".
                          " where ".
                          " sourceHostId = '$sourceId' ".
                          " and  sourceDeviceName = '" .
                          $this->conn->sql_escape_string($sourceDeviceName) . "'" .
                          " and destinationHostId = '$destId' " .
                          "and  destinationDeviceName = '" .
                          $this->conn->sql_escape_string($destDeviceName) . "'";



        $rs = $this->conn->sql_query("update srcLogicalVolumeDestLogicalVolume ".
                          " set $state_field = $state ".
                          " where ".
                          " sourceHostId = '$sourceId' ".
                          " and  sourceDeviceName = '" .
                          $this->conn->sql_escape_string($sourceDeviceName) . "'" .
                          " and destinationHostId = '$destId' " .
                          "and  destinationDeviceName = '" .
                          $this->conn->sql_escape_string($destDeviceName) . "'");
        if (!$rs) {
            // report error
            return 1;
        }

        return 0;
    }
	
	public function stop_protecting_prism_lun($sharedDeviceId,$state_field,$state)
    {
        global $SOURCE_DELETE_PENDING;
		global $logging_obj;
		
        $rs = $this->conn->sql_query("update srcLogicalVolumeDestLogicalVolume ".
                          " set $state_field = $state ".
						  " where ".
                          " Phy_Lunid  = '$sharedDeviceId' " 
                          );
        if (!$rs) {
            // report error
            return 1;
        }

        return 0;
    }
	
    /*
     * Function Name: stop_replication
     *
     * Description:
     *    Stops inprogress replication
     *
     *
     *
     *
     * Parameters:
     *     Param 1 [IN]:$sourceId
     *     Param 2 [IN]:$sourceDeviceName
     *     Param 2 [IN]:$destId
     *     Param 2 [IN]:$destDeviceName
     *     Param 2 [IN]:$deleteSnapshots=1
     *
     * Return Value:
     *     Ret Value: Returns Boolean value .
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function stop_replication($sourceId,$sourceDeviceName,$destId,
                              $destDeviceName,$deleteSnapshots=1)
    {

        return $this->delete_pair($sourceId, $sourceDeviceName, $destId,
                                  $destDeviceName, $deleteSnapshots);

    }

    /*
     * Function Name:get_ruleId
     *
     * Description:
     *    Returns the ruleId of the pair described by the guid/drive
     *    letter combinations
     *
     * Parameters:
     *     Param 1 [IN]:$src_guid
     *     Param 2 [IN]:$src_dev
     *     Param 3 [IN]:$dest_guid
     *     Param 4 [IN]:$dest_dev
     *     Param 5 [OUT]:$row->ruleId
     * Return Value:
     *     Ret Value: Returns ResultSet value .
     *
     * Exceptions:
     *     DataBase Connection fails.
     */
    public function get_ruleId ($src_guid, $src_dev, $dest_guid, $dest_dev)
    {
		global $logging_obj;
        $src_dev   = $this->commonfunctions_obj->verifyDatalogPath($src_dev);
        $src_dev = $this->conn->sql_escape_string($src_dev);

		$dest_dev   = $this->commonfunctions_obj->verifyDatalogPath($dest_dev);
        $dest_dev = $this->conn->sql_escape_string($dest_dev);

        $query="SELECT
                    ruleId
                FROM
                    srcLogicalVolumeDestLogicalVolume 
                WHERE
                    sourceHostId='$src_guid'
                    AND
                     sourceDeviceName='$src_dev' 
                    AND
                    destinationHostId IN ('$dest_guid')
                    AND
                     destinationDeviceName='$dest_dev'";
		$logging_obj->my_error_handler("INFO",array("SQL"=>$query),Log::BOTH);
		
        $result_set = $this->conn->sql_query ($query); 

		$number_of_rows = $this->conn->sql_num_row ($result_set);	
        if ($number_of_rows != 1)
        {
			$logging_obj->my_error_handler("INFO",array("NumberOfRows"=>$number_of_rows),Log::BOTH);
            return false;
        }

        $row =  $this->conn->sql_fetch_object($result_set);
		$return_rule_id = $row->ruleId;
		
		
		$logging_obj->my_error_handler("INFO",array("ReturnRuleId"=>$return_rule_id),Log::BOTH);
        return $return_rule_id;
    }

    /*
     * Function Name:get_cluster_details
     *
     * Description:
     *
     * Gets the Cluster Details form cluster table.
     *   e.g:: clusterName,clusterGroupName,clusterGroupId,clusterruleId.
     *
     * Parameters:
     *     Param 1 [IN]:$vardb.
     *     Param 2 [IN]:$serverid
     *     Param 3 [IN]:$vol
     *
     *     Param 1 [OUT]:$result
     *
     * Return Value:
     *     Ret Value: Returns An Array.
     *
     * Exceptions:
     *     DataBase Connection fails.
    */
    public function get_cluster_details ($serverid, $vol)
    {
        $vol = $this->conn->sql_escape_string($vol);
        $CDetail = "SELECT
                        clusterName,
                        clusterGroupName,
                        clusterGroupId,
                        clusterruleId
                    FROM
                        clusters
                    WHERE
                        hostId='$serverid'
                        AND deviceName='$vol'";
        $CRs = $this->conn->sql_query($CDetail);
        $myrow = $this->conn->sql_fetch_row ($CRs);
        $result = array ();
        $result = array ('clusterName' => $myrow[0],
                         'clusterGroupName' => $myrow[1],
                         'clusterGroupId' => $myrow[2],
                         'clusterruleId' => $myrow[3]);
        return $result;
    }

	 /*
     * Function Name: get_qausi_flag
     *
     * Description:
     *        This function returns the replication status of a pair, whether the pair is in
     *         Resync step I or II or Diff sync,by quering the database for isQuasiflag
     *
     * Parameters:
     *     Param 1 [IN]: $src_id
     *     Param 1 [IN]: $src_dev
     *     Param 1 [IN]: $dest_id
     *     Param 1 [IN]: $dest_dev
     *
     * Return Value:
     *     Ret Value: Status
     *
     * Exceptions:
     *
   */
   public function get_qausi_flag( $src_id, $src_dev, $dest_id, $dest_dev )
   {
      $src_dev = $this->conn->sql_escape_string($src_dev);
      $dest_dev = $this->conn->sql_escape_string($dest_dev);
      $query = "SELECT
                  isQuasiflag
               FROM
                  srcLogicalVolumeDestLogicalVolume " . "
               WHERE
                  sourceHostId = '" . $src_id . "'
                  AND " . "
                   sourceDeviceName = '" . $src_dev . "'
                  AND "."
                  destinationHostId ='" . $dest_id . "'
                  AND "."
                   destinationDeviceName ='". $dest_dev ."'";
      $iter = $this->conn->sql_query( $query);
      $myrow = $this->conn->sql_fetch_object( $iter );
      $isQuasiflag = $myrow->isQuasiflag;
      return ($isQuasiflag);
   }

   /*
     * Function Name: set_unregister_host
     *
     * Description:
     *   This function update the unistall state in host table during the unregister fabric Vxagent .
     *
     * Parameters:
     *     Param 1 [IN]: $id
     *     Param 2 [OUT]:$status
     * Return Value:
     *     Ret Value: boolean
     *
     * Exceptions:
     *
     */

	public function set_unregister_host($id)
	{

		global $UNINSTALL_PENDING;
		global $logging_obj;
	
		$status = FALSE;
		$update_host="
						UPDATE
							  hosts
						SET
							  agent_state='$UNINSTALL_PENDING'
						WHERE
							  id='$id'
						";
		$host_exec = $this->conn->sql_query($update_host);
		$logging_obj->my_error_handler('INFO','set_unregister_host:::'.$update_host);
		
		if($host_exec)
		{
			$status = TRUE;
		}

		return ($status);

	}
/*
* Function Name: pause_replication
*
* Description:
*   This function update the pause state for selected pairs.
*
* Parameters:
*     Param 1 [IN]: $source_id
*     Param 2 [IN]: $source_device_name
*     Param 3 [IN]: $dest_id
*     Param 4 [IN]: $dest_device_name
*     Param 5 [IN]: $onetoNcase
* Return Value:
*     Ret Value: boolean
*
* Exceptions:
*
*/
  public function pause_replication($source_id, $source_device_name, $dest_id, $dest_device_name,$onetoNcase,$flag=0,$pause_from="user")
  {
		global $logging_obj;
		global $PAUSED_PENDING,$PAUSE_COMPLETED;
		global $PAUSE_BY_USER, $PAUSE_BY_RESTART_RESYNC;
		$cond = " ";
		
		if ($pause_from == "disk resize")
		{
			$cond = ", restartPause = 1 ";
		}
		elseif ($pause_from == "general pause")
		{
			$cond = ",restartPause = 0 ";
		}
		elseif ($pause_from == "resync required")
		{
			$cond = ",restartPause = 1 ";
		}
		else
		{
			$cond = ",src_replication_status = 1";
		}
		
		$source_device_name = $this->conn->sql_escape_string($source_device_name);
		$dest_device_name = $this->conn->sql_escape_string($dest_device_name);
    	
		$src_hostids = $source_id;

		if($onetoNcase == 1)
		{
			/*Update pause for all 1-N pairs*/
			$query="UPDATE
						srcLogicalVolumeDestLogicalVolume
					SET
						replication_status = $PAUSED_PENDING, 
						tar_replication_status = 1,
						pauseActivity = '$PAUSE_BY_USER' ,
						autoResume    = 0 $cond
					WHERE
						sourceHostId IN('$src_hostids')
					AND
						sourceDeviceName ='$source_device_name'
					AND
						tar_replication_status = 0
					AND
						src_replication_status = 0 
					AND 
							deleted != 1";
		}
		else if($onetoNcase == 2)
		{
			/*Update only specific targets for pause in 1-N pairs*/
			$query="UPDATE
						srcLogicalVolumeDestLogicalVolume
					SET
						replication_status = $PAUSED_PENDING,
						tar_replication_status = 1,
						pauseActivity = '$PAUSE_BY_USER',
						autoResume    = 0 $cond 
					WHERE
						sourceHostId IN('$src_hostids')
					AND
						sourceDeviceName ='$source_device_name'
					AND
						destinationHostId = '$dest_id'
					AND
						destinationDeviceName = '$dest_device_name'
					AND
						tar_replication_status = 0 
					AND 
							deleted != 1";
						
		}
		else
		{
			if ($pause_from == "general pause")
			{
				/*Re-sync required sent YES, as part of pause-pending initiated, later general pause request reached, and the pause pending state should overwrite with general pause. This is applicable only in forward protection.*/
				$query="UPDATE
							srcLogicalVolumeDestLogicalVolume
						SET
							replication_status = $PAUSE_COMPLETED,
							src_replication_status = 1,
							tar_replication_status = 1,
							pauseActivity = '$PAUSE_BY_USER',
							autoResume    = 0 $cond 
							
						WHERE
							sourceHostId IN('$src_hostids')
						AND
							sourceDeviceName ='$source_device_name'
						AND
							destinationHostId = '$dest_id'
						AND
							destinationDeviceName = '$dest_device_name' 
						AND 
							deleted != 1";
						
			}
			elseif($flag) 
			{
				/*Added for Recovery during Resync Time for *ONLY* HA-DR plans*/
				$query="UPDATE
							srcLogicalVolumeDestLogicalVolume
						SET
							replication_status = $PAUSE_COMPLETED,
							src_replication_status = 1,
							tar_replication_status = 1,
							pauseActivity = '$PAUSE_BY_USER',
							autoResume    = 0 
							
						WHERE
							sourceHostId IN('$src_hostids')
						AND
							sourceDeviceName ='$source_device_name'
						AND
							destinationHostId = '$dest_id'
						AND
							destinationDeviceName = '$dest_device_name'
						AND
							tar_replication_status = 0
						AND
							src_replication_status = 0 
						AND 
							deleted != 1";

			
			} else{
			
				/*Update source & target as pause  1-1*/
				$query="UPDATE
							srcLogicalVolumeDestLogicalVolume
						SET
							replication_status = $PAUSED_PENDING,
							src_replication_status = 1,
							tar_replication_status = 1,
							pauseActivity = '$PAUSE_BY_USER',
							autoResume    = 0 $cond 
						WHERE
							sourceHostId IN('$src_hostids')
						AND
							sourceDeviceName ='$source_device_name'
						AND
							destinationHostId = '$dest_id'
						AND
							destinationDeviceName = '$dest_device_name'
						AND
							tar_replication_status = 0
						AND
							src_replication_status = 0 
						AND 
							deleted != 1";
			}
			
		}
		
		$logging_obj->my_error_handler("INFO",array("Query"=>$query),Log::BOTH);
		$host_exec = $this->conn->sql_query($query);
		
		/*make audit log entry */
		$iter = $this->conn->sql_query("SELECT
											ipaddress
										FROM
											hosts
										WHERE
											id='$source_id'
										AND (sentinelEnabled='1'
											or
											outpostAgentEnabled='1')");
		$myrow = $this->conn->sql_fetch_object($iter);
		$srcIp = $myrow->ipaddress;
		/*Audit log entry for 1-N case*/
		if($onetoNcase == 1)
		{
			$iter = $this->conn->sql_query("SELECT
												destinationDeviceName,
												destinationHostId
											FROM
												srcLogicalVolumeDestLogicalVolume
											WHERE
												sourceHostId='$source_id'
											AND
												sourceDeviceName='$source_device_name'");
			while($row = $this->conn->sql_fetch_row($iter))
			{
				$dest_drive_name .=$row[0].",";
				$dest_host_id .=$row[1]."','";
			}
			$dest_device_name = $dest_drive_name;
			$iter = $this->conn->sql_query("SELECT
											ipaddress
										FROM
											hosts
										WHERE id IN('$dest_host_id')
										AND
											(sentinelEnabled='1'
											or
											outpostAgentEnabled='1')");
			while($row = $this->conn->sql_fetch_object($iter))
			{
				$destinationIp .= $row->ipaddress.",";
			}
			$destIp = $destinationIp;
		}
		else
		{
			$iter = $this->conn->sql_query("SELECT
												ipaddress
											FROM
												hosts
											WHERE id='$dest_id'
											AND
												(sentinelEnabled='1'
												or
												outpostAgentEnabled='1')");
			$myrow = $this->conn->sql_fetch_object($iter);
			$destIp = $myrow->ipaddress;
		}
		$source_device_name   = $this->commonfunctions_obj->verifyDatalogPath($source_device_name);
		$dest_device_name     = $this->commonfunctions_obj->verifyDatalogPath($dest_device_name);
		$this->commonfunctions_obj->writeLog($_SESSION['username'],"Replication pair has been paused manually for ($srcIp($source_device_name),$destIp($dest_device_name))");
}
	/*
	* Function Name: get_pause_status
	*
	* Description:
	*   This function gets the pause state for selected pairs.
	*
	* Parameters:
	*     Param 1 [IN]: $source_id
	*     Param 2 [IN]: $source_device_name
	*     Param 3 [IN]: $dest_id
	*     Param 4 [IN]: $dest_device_name
	*     Param 1 [OUT] :$pausereplication
	* Return Value:
	*     Ret Value: boolean
	*
	* Exceptions:
	*
	*/
	public function get_pause_status($source_id, $source_device_name, $dest_id, $dest_device_name)
	{
		/*validate windows & linux slashes*/
		$source_device_name =$this->get_unformat_dev_name($source_id,$source_device_name);
		$dest_device_name =$this->get_unformat_dev_name($dest_id,$dest_device_name);

		$source_device_name =$this->commonfunctions_obj->verifyDatalogPath($source_device_name);
		  $source_device_name = $this->conn->sql_escape_string($source_device_name);
		$dest_device_name =$this->commonfunctions_obj->verifyDatalogPath($dest_device_name);
		  $dest_device_name = $this->conn->sql_escape_string($dest_device_name);
		$src_hostid_arr = explode(",",$source_id);
		$is_device_clustered = $this->is_device_clustered($src_hostid_arr[0], $source_device_name);
		/*check if device is clustered*/
		if($is_device_clustered)
		{
			$clusterIds_arr =$this->get_clusterids_as_array($src_hostid_arr[0],$source_device_name);
			$src_hostids = implode("','",$clusterIds_arr);
		}
		else
		{
			$src_hostids = $source_id;
		}
		/*1-1,1-N,Cluster & Non Cluster cases are considered*/
		$query ="SELECT
                replication_status
            FROM
                srcLogicalVolumeDestLogicalVolume
            WHERE
                sourceHostId IN('$src_hostids')
            AND
                sourceDeviceName ='$source_device_name'
            AND
                destinationHostId = '$dest_id'
            AND
                destinationDeviceName = '$dest_device_name'";
      $host_exec = $this->conn->sql_query($query);
	  $myrow = $this->conn->sql_fetch_object( $host_exec );
	  $pausereplication = $myrow->replication_status;
	  return ($pausereplication);
	}
	/*
	* Function Name: resume_replication
	*
	* Description:
	*   This function updates the resume state for selected pairs.
	*
	* Parameters:
	*     Param 1 [IN]: $source_id
	*     Param 2 [IN]: $source_device_name
	*     Param 3 [IN]: $dest_id
	*     Param 4 [IN]: $dest_device_name
	*     Param 5 [IN] :$onetoNcase
	* Return Value:
	*     Ret Value: boolean
	*
	* Exceptions:
	*
	*/
	public function resume_replication($source_id, $source_device_name, $dest_id, $dest_device_name, $onetoNcase,$flag=0)
	{
		global $logging_obj;
		global $PAUSED_PENDING;
		global $PAUSE_COMPLETED;

		$src_hostid_arr = explode(",",$source_id);
		$is_device_clustered = $this->is_device_clustered($src_hostid_arr[0], $source_device_name);
		/*check if device is clustered*/
		if($is_device_clustered)
		{
			$clusterIds_arr =$this->get_clusterids_as_array($src_hostid_arr[0],$source_device_name);
			$src_hostids = implode("','",$clusterIds_arr);
		}
		else
		{
			$src_hostids = $source_id;
		}

		$src_compatibilty = $this->commonfunctions_obj->get_compatibility($source_id);
		$trg_compatibilty = $this->commonfunctions_obj->get_compatibility($dest_id);
		$pause_state = $this->get_pause_status($source_id, $source_device_name, $dest_id, $dest_device_name);
		$source_device_name = $this->conn->sql_escape_string($source_device_name);
		$dest_device_name = $this->conn->sql_escape_string($dest_device_name);

		if(($pause_state == $PAUSED_PENDING) || ($pause_state == $PAUSE_COMPLETED))
		  {
			if($src_compatibilty == 420000)
			{
				/*BUG 10097*/
				if($onetoNcase == 1)
				{
					$query="UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status = '0',
								throttleDifferentials ='0'
							WHERE
								sourceHostId IN('$src_hostids')
							AND
								sourceDeviceName ='$source_device_name'";
				}
				else
				{
					$query="UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status = '0',
								throttleDifferentials ='0'
							WHERE
								sourceHostId IN('$src_hostids')
							AND
								sourceDeviceName ='$source_device_name'
							AND
								destinationHostId = '$dest_id'
							AND
								destinationDeviceName = '$dest_device_name'";
				}

			}
			/*resume case for 5.1*/
			else if(($src_compatibilty >= 510000) && ($trg_compatibilty >=510000))
			{
				/*Update resume for all 1-N pairs*/
				if($onetoNcase == 1)
				{

					$query="UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status = 0,
								src_replication_status = 0,
								tar_replication_status = 0,
								pauseActivity = NULL,
								autoResume    = 0
							WHERE
								sourceHostId IN('$src_hostids')
								AND
								sourceDeviceName ='$source_device_name'
								AND
								tar_replication_status = 1
								AND
								src_replication_status = 1";
				}
				/*Update only specific targets for resume in 1-N pairs*/
				else if($onetoNcase == 2)
				{
					$sql = "SELECT
								tar_replication_status
							FROM
								srcLogicalVolumeDestLogicalVolume
							WHERE
								sourceHostId IN('$src_hostids')
								AND
								sourceDeviceName ='$source_device_name'
								AND 
								tar_replication_status = 1";
					$result = $this->conn->sql_query($sql);
					while($row = $this->conn->sql_fetch_object($result))
					{
						$tar_replication_status[] = $row->tar_replication_status;
					}	
					$one_to_n_value = count($tar_replication_status);
					
					if($one_to_n_value == 1)
					{
						$query="UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status = 0,
								src_replication_status = 0,
								tar_replication_status = 0,
								pauseActivity = NULL,
								autoResume    = 0
							WHERE
								sourceHostId IN('$src_hostids')
								AND
								sourceDeviceName ='$source_device_name'
								AND
								(tar_replication_status = 1 OR src_replication_status = 1)";
								//exit();
					}
					elseif($one_to_n_value > 1)
					{
						$query="UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status = 0,
								tar_replication_status = 0,
								pauseActivity = NULL,
								autoResume    = 0
							WHERE
								sourceHostId IN('$src_hostids')
								AND
								sourceDeviceName ='$source_device_name'
								AND
								destinationHostId = '$dest_id'
								AND
								destinationDeviceName = '$dest_device_name'
								AND
								tar_replication_status = 1";
								
					}					
				}
				else
				{
					if($flag) {

						$query="UPDATE
									srcLogicalVolumeDestLogicalVolume
								SET
									replication_status = 0,
									src_replication_status= 0,
									tar_replication_status= 0,
									pauseActivity = NULL,
									autoResume    = 0
								WHERE
									sourceHostId IN('$src_hostids')
									AND
									sourceDeviceName ='$source_device_name'
									AND
									destinationHostId = '$dest_id'
									AND
									destinationDeviceName = '$dest_device_name' AND
									replication_status = $PAUSE_COMPLETED";

					
					} else {
						/*Update source & target as resume  1-1*/
						$query="UPDATE
									srcLogicalVolumeDestLogicalVolume
								SET
									replication_status = 0,
									src_replication_status= 0,
									tar_replication_status= 0,
									pauseActivity = NULL,
									autoResume    = 0
								WHERE
									sourceHostId IN('$src_hostids')
									AND
									sourceDeviceName ='$source_device_name'
									AND
									destinationHostId = '$dest_id'
									AND
									destinationDeviceName = '$dest_device_name'";
					}
						
				}

			}
			/*resume case for 5.0*/
			else
			{
				/*Update resume for all 1-N pairs*/
				if($onetoNcase == 1)
				{
					$query ="UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status = 0,
								src_replication_status = 0,
								tar_replication_status = 0
							where
								sourceHostId IN('$src_hostids')
								and
								sourceDeviceName ='$source_device_name'";
				}
				/*Update only specific targets for resume in 1-N pairs*/
				else if($onetoNcase == 2)
				{
					$query ="UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status = 0,
								tar_replication_status =0
							where
								sourceHostId IN('$src_hostids')
								and
								sourceDeviceName ='$source_device_name'
								and
								destinationHostId = '$dest_id'
								and
								destinationDeviceName = '$dest_device_name'";
				}
				/*resume source & target for  1-1*/
				else
				{
					$query ="UPDATE
								srcLogicalVolumeDestLogicalVolume
							SET
								replication_status = 0,
								src_replication_status=0,
								tar_replication_status=0
							where
								sourceHostId IN('$src_hostids')
								and
								sourceDeviceName ='$source_device_name'
								and
								destinationHostId = '$dest_id'
								and
								destinationDeviceName = '$dest_device_name'";
				}
			}
		//exit();
			$host_exec = $this->conn->sql_query($query);
			
			/*make audit log entry*/
			$iter = $this->conn->sql_query("SELECT
												ipaddress
											FROM
												hosts
											WHERE
												id='$source_id'
												AND (sentinelEnabled='1'
												or
												outpostAgentEnabled='1')");
			$myrow = $this->conn->sql_fetch_object($iter);
			$srcIp = $myrow->ipaddress;
			/*Audit log entry for 1-N case*/
			if($onetoNcase == 1)
			{
				$iter = $this->conn->sql_query("SELECT
													destinationDeviceName,
													destinationHostId
												FROM
													srcLogicalVolumeDestLogicalVolume
												WHERE
													sourceHostId='$source_id'
													AND
													sourceDeviceName='$source_device_name'");
				while($row = $this->conn->sql_fetch_row($iter))
				{
					$dest_drive_name .=$row[0].",";
					$dest_host_id .=$row[1]."','";
				}
				$dest_device_name = $dest_drive_name;
				$iter = $this->conn->sql_query("SELECT
													ipaddress
												FROM
													hosts
												WHERE id IN('$dest_host_id')
												AND
												(sentinelEnabled='1'
												or
												outpostAgentEnabled='1')");
				while($row = $this->conn->sql_fetch_object($iter))
				{
					$destinationIp .= $row->ipaddress.",";
				}
				$destIp = $destinationIp;
			}
			else
			{
				$iter = $this->conn->sql_query("SELECT
													ipaddress
												FROM
													hosts
												WHERE id='$dest_id'
												AND
												(sentinelEnabled='1'
												or
												outpostAgentEnabled='1')");
				$myrow = $this->conn->sql_fetch_object($iter);
				$destIp = $myrow->ipaddress;
			}
			$source_device_name   = $this->commonfunctions_obj->verifyDatalogPath($source_device_name);
			$dest_device_name     = $this->commonfunctions_obj->verifyDatalogPath($dest_device_name);
			$this->commonfunctions_obj->writeLog($_SESSION['username'],"Replication pair has been resumed manually for ($srcIp($source_device_name),$destIp($dest_device_name))");
		  }

	}

    /*
	 * This function will the os type of the host
	 */
	public function os_type($dest_id)
	{
		$os_type_sql = "SELECT
		                    osFlag
					    FROM
						    hosts
					    WHERE
						    id = '$dest_id'";
		$os_type_set = $this->conn->sql_query($os_type_sql);
		$row = $this->conn->sql_fetch_row($os_type_set );
		return $row[0];

	}

	/*
     * Function Name: check_xen_server
     *
     * Description:
     *    This function check that the given hostId is related to xen or not
     *
     *
     *
     * Parameters:
     *    Param 1 [IN]:$host_Id
     *
     * Return Value:
     *      Returns the count
     *
     * Exceptions:
     *    DataBase Connection fails.
    */
	public function check_xen_server($hostId)
	{
		$poolid_sql = "SELECT
		                   count(*)
					   FROM
					       hostXsResourcePools
					   WHERE
					       hostId='$hostId'";
		$poolid_result = $this->conn->sql_query($poolid_sql);
		$poolid_row = $this->conn->sql_fetch_row($poolid_result);
		return $poolid_row[0];
	}

	/*
     * Function Name: is_volume_resize_done
     *
     * Description:
     *    To get the status of volume resize.
     *
     * Parameters:
     *     Param 1 [IN/OUT]:$source_host_id (source host identity)
     *     Param 2 [IN/OUT]:$source_device_name (source device name)
     *
     * Return Value:
	 *		TRUE/FALSE
     *
     * Exceptions:
	 *
    */
	public function is_volume_resize_done($source_id, $source_device_name, $pair_id)
	{

		$source_device_name = $this->conn->sql_escape_string($source_device_name);

		$query =	"select
						*
					from
						volumeResizeHistory
					where
						hostId = '$source_id'
					and
						deviceName = '$source_device_name'
					and
						pairId = '$pair_id'
					and
						isValid = 1";
		$result_set = $this->conn->sql_query($query);
		$num_rows = $this->conn->sql_num_row($result_set);
		if ($num_rows > 0)
		{
			$return_status = TRUE;
		}
		else
		{
			#15385 Fix
			$sqlStmnt = "select 
								Phy_Lunid 
						from 
								srcLogicalVolumeDestLogicalVolume 
						where 
								sourceHostId = '$source_id' 
						and 
								sourceDeviceName = '$source_device_name'
						and
								((Phy_Lunid != '') AND (Phy_Lunid IS NOT NULL))";
						
			$resStmnt = $this->conn->sql_query($sqlStmnt);
			$num_rows = $this->conn->sql_num_row($resStmnt);
			if($num_rows>0)
			{
				$row = $this->conn->sql_fetch_object($resStmnt);
				$scsiId = $row->Phy_Lunid;
				$query =	"select 
										lv.deviceName 
								from 
									volumeResizeHistory vrh, 
									srcLogicalVolumeDestLogicalVolume sldl, 
									logicalVolumes lv 
								where 
									lv.hostId = vrh.hostId 
								and 
									lv.deviceName = vrh.deviceName 
								and 
									lv.Phy_Lunid = sldl.Phy_Lunid
								and 
									vrh.pairId = '$pair_id'
								and 
									vrh.isValid = 1 
								and 
									lv.Phy_Lunid='$scsiId'";
				$result_set = $this->conn->sql_query($query);
				$num_rows1 = $this->conn->sql_num_row($result_set);
				if($num_rows1>0)
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
				$return_status = FALSE;
			}
			//$return_status = FALSE;
		}
		return $return_status;
	}

	/*
     * Function Name: get_agent_ip_address
     *
     * Description:
     *    getting agent ip address.
     *
     * Parameters:
     * 1[IN]: hostId
     *
     * Return Value:
     * Scalar
     *
     * Exceptions:
     *
     */
	public function get_agent_ip_address($hostId)
	{
		$query = "SELECT
					ipaddress
				FROM
					hosts
				WHERE
					id='$hostId' AND
					(sentinelEnabled='1' or outpostAgentEnabled='1') ";
		$result_set = $this->conn->sql_query($query);
        $myrow = $this->conn->sql_fetch_object($result_set);
        $ip_address = $myrow->ipaddress;

		return $ip_address;
	}

	public function get_source_details($destinaton_host_id , $destination_device_name)
	{
		 $source_details = array();
		 $destination_device_name =$this->commonfunctions_obj->verifyDatalogPath($destination_device_name);
		 $destination_device_name = $this->conn->sql_escape_string($destination_device_name);

		 $query = "SELECT
						 sourceHostId,
						 sourceDeviceName
				   FROM
						 srcLogicalVolumeDestLogicalVolume
				   WHERE
						 destinationHostId = '$destinaton_host_id' AND
						 destinationDeviceName = '$destination_device_name'";
		 $result = $this->conn->sql_query($query);
		 $myrow = $this->conn->sql_fetch_object($result);
		 $source_details[0] = $myrow->sourceHostId;
		 $source_details[1] = $myrow->sourceDeviceName;
		 return $source_details;
	}
	
	public function configure_mediaRetention($ret_arr,$common_func_obj="",$sparse_ret_obj="",$ret_obj="",$protectionPlanObj="",$loggingObj="")
	{
		global $sparseretention_obj,$retention_obj,$commonfunctions_obj,$logging_obj,$protection_plan_obj;
		if(!$commonfunctions_obj)
		$commonfunctions_obj = $common_func_obj;
		if(!$sparseretention_obj)
		$sparseretention_obj = $sparse_ret_obj;
		if(!$retention_obj)
		$retention_obj = $ret_obj;
		if(!$logging_obj)
		$logging_obj = $loggingObj;
		if(!$protection_plan_obj)
		$protection_plan_obj = $protectionPlanObj;	
		
		
	   $logging_obj->my_error_handler("DEBUG","configure_mediaRetention (Parameters): ".print_r($ret_arr,TRUE)."\n");
		
		
		$dest = $ret_arr['dest'];
		$id = $ret_arr['id'];
		$deviceName = $ret_arr['deviceName'];
		$select_resync = $ret_arr['select_resync'];
		$hidMediaret = $ret_arr['hidMediaret'];
		$hcompenable = $ret_arr['hcompenable'];
		$volumegroup = $ret_arr['volumegroup'];
		$hsecure_transport_src = $ret_arr['hsecure_transport_src'];
		$hsecure_transport = $ret_arr['hsecure_transport'];
		$log_policy_rad = $ret_arr['log_policy_rad'];
		$hidUnit = $ret_arr['hidUnit'];
		$txtlocmeta = $ret_arr['txtlocmeta'];
		$hdrive1 = $ret_arr['hdrive1']; 
		$hdrive2 = $ret_arr['hdrive2']; 
		$timeRecFlag = $ret_arr['timeRecFlag']; 
		$hidTime = $ret_arr['hidTime'];
		$change_by_time_day = $ret_arr['change_by_time_day']; 
		$change_by_time_hr = $ret_arr['change_by_time_hr'];
		$txtlocdir_move = $ret_arr['txtlocdir_move'];
		$txtdiskthreshold = $ret_arr['txtdiskthreshold'];
		$htimelapse = $ret_arr['htimelapse'];
		$hwindowstarthours = $ret_arr['hwindowstarthours'];
		$hwindowstartminutes = $ret_arr['hwindowstartminutes'];
		$hwindowstophours = $ret_arr['hwindowstophours'];
		$hwindowstopminutes = $ret_arr['hwindowstopminutes'];
		$logExceedPolicy = $ret_arr['logExceedPolicy'];
		$lun_process_server_info = $ret_arr['lun_process_server_info'];
		$process_server = $ret_arr['process_server'];
		$hid_ps_natip_source = $ret_arr['hid_ps_natip_source'];
		$hid_ps_natip_target = $ret_arr['hid_ps_natip_target'];
		$sparse_enable = $ret_arr['hid_sparse_enable'];
		
		$logDeviceName =  $ret_arr['logDeviceName'];
		$policy_type = $ret_arr['policy_type'];
		$retention_dir = $ret_arr['retention_dir'];
		
		$previous_ret_path = $ret_arr['previous_ret_path'];
		
		
		list( $destId, $destDeviceName ) = explode( ",", $dest, 2 );
	    $deviceName =$this->get_unformat_dev_name($id,$deviceName);
		$destDeviceName =$this->get_unformat_dev_name($destId,$destDeviceName);
		
		#$destDeviceName =$this->get_base_volume_name($destId,$destDeviceName);
		
	    /*
	            removing Alert when log size reaches 
	            instead 0f $txtlogsizethreshold sending value 0
	        */
	    /*
	            *5546-Assign the selected resync value to the hfastresync variable to 
	            *insert into the database based on the select_resync existence.(#5778)
	        */
	    if (isset($select_resync) && ($select_resync != ''))
	    {
	        $hfastresync = $select_resync;
	    }

						
		if($ret_arr['batch_resync_val']) $batch_resync_val = $ret_arr['batch_resync_val'];
			else $batch_resync_val = 0;
		if($ret_arr['execution_state'])  $execution_state = $ret_arr['execution_state'];
			else $execution_state = 0;
		if($ret_arr['scenario_id'])  $scenario_id = $ret_arr['scenario_id'];
			else $scenario_id = 0;
		if($ret_arr['app_type']) $app_type = $ret_arr['app_type'];
			else $app_type = 0;
		if($ret_arr['plan_id']) $plan_id = $ret_arr['plan_id'];
	 
		$logging_obj->my_error_handler("DEBUG","protect_drive (Parameters): ");
		$logging_obj->my_error_handler("DEBUG","$id, $deviceName, $destId, $destDeviceName, 1, $hidMediaret, $hfastresync, $hcompenable, $volumegroup,
	                       $hsecure_transport_src, $hsecure_transport, $policy_type, $log_policy_rad, $hidUnit,$txtlocmeta,
	                       $retention_dir, $hdrive1, $hdrive2, $timeRecFlag, $hidTime, $change_by_time_day, $change_by_time_hr,
	                       $txtlocdir_move, $txtdiskthreshold, 0, $htimelapse, 
	                       $hwindowstarthours, $hwindowstartminutes, $hwindowstophours, $hwindowstopminutes,$logExceedPolicy,$lun_process_server_info,
						   $process_server,$hid_ps_natip_source,$hid_ps_natip_target,$sparse_enable,$batch_resync_val,$execution_state,$scenario_id,$plan_id,$app_type");
		$this->protect_drive( $id, $deviceName, $destId, $destDeviceName, 1, $hidMediaret, $hfastresync, $hcompenable, $volumegroup,
	                       $hsecure_transport_src, $hsecure_transport, $policy_type, $log_policy_rad, $hidUnit,$txtlocmeta,
	                       $retention_dir, $hdrive1, $hdrive2, $timeRecFlag, $hidTime, $change_by_time_day, $change_by_time_hr,
	                       $txtlocdir_move, $txtdiskthreshold, 0, $htimelapse, 
	                       $hwindowstarthours, $hwindowstartminutes, $hwindowstophours, $hwindowstopminutes,$logExceedPolicy,$lun_process_server_info,$process_server,$hid_ps_natip_source,$hid_ps_natip_target,$sparse_enable,$batch_resync_val,$execution_state,$scenario_id,$plan_id,$app_type,$ret_arr);
	}	
	
	/*
	* Function Name: get_pair_type
    *
	* Description:
    * This function will return pair type(PRISM/FABRIC) based on SCSI ID exist in respective table.
	*  
	* Parameters: 
	*   $fb_wwn[IN]:
	*   $result[OUT]:
    *	
	* Return Value:
	*   0 if pair type is host,1 for fabric and 2 for prism
	*
	*/
	
	public function get_pair_type($scsi_id)
	{
 		$lun_id_arr = array();
		$pair_type = 0;
		$sql = "SELECT Phy_Lunid, destinationHostId, destinationDeviceName, planId FROM srcLogicalVolumeDestLogicalVolume WHERE planId != '1'";
		$res = $this->conn->sql_query($sql);
		if($this->conn->sql_num_row($res) > 0)
		{
			while($row = $this->conn->sql_fetch_object($res))
			{
				$lun_id_arr[$row->planId]["srcLun"][$row->Phy_Lunid] = $row->Phy_Lunid;
				$dest_lun_id = $this->get_phy_lun_id($row->destinationDeviceName,$row->destinationHostId);
				if($dest_lun_id)
				{
					$lun_id_arr[$row->planId]["destLun"][$dest_lun_id] = $dest_lun_id;
				}
			}
			if(count($lun_id_arr) > 0)
			{
				foreach($lun_id_arr as $plan_id => $value)
				{
					if((isset($value["srcLun"]) && is_array($value["srcLun"]) && in_array($scsi_id,$value["srcLun"])) || (isset($value["destLun"]) && is_array($value["destLun"]) && in_array($scsi_id,$value["destLun"])))
					{
						$sol_type_sql = "SELECT solutionType FROM applicationPlan WHERE planId = '$plan_id'";
						$sol_type_res = $this->conn->sql_query($sol_type_sql);
						if($this->conn->sql_num_row($sol_type_res) > 0)
						{
							$sol_type_row = $this->conn->sql_fetch_object($sol_type_res);
							if($sol_type_row->solutionType == 'PRISM')
							{
								$pair_type = 2;
							}
							elseif($sol_type_row->solutionType == 'ARRAY')
							{
								$pair_type = 3;
							}
						}
					}
				}
			}
		}
		
		return $pair_type;
    }
	public function get_pair_type_new($host_id,$logical_volumes_arary)
	{
 		$lun_id_arr = array();
		$dest_lun_id_array = array();
		$pair_type = 0;
		$sql = "SELECT Phy_Lunid, destinationHostId, destinationDeviceName, planId FROM srcLogicalVolumeDestLogicalVolume WHERE planId != '1' AND (sourceHostId = '$host_id' OR destinationHostId = '$host_id')";
	
		$res = $this->conn->sql_query($sql);
		$pair_type_array = $this->get_pair_type_array();
		if($this->conn->sql_num_row($res) > 0)
		{
			while($row = $this->conn->sql_fetch_object($res))
			{			
				if($row->Phy_Lunid)
				$lun_id_arr[$row->planId]["srcLun"][] = $row->Phy_Lunid;	
				$dest_lun_id = $logical_volumes_arary[$row->destinationDeviceName]['Phy_Lunid'];
				if($dest_lun_id)
				$lun_id_arr[$row->planId]["destLun"][] = $dest_lun_id;
				if($row->Phy_Lunid || $dest_lun_id)
				$lun_id_arr[$row->planId]['pair_type'] = $pair_type_array[$row->planId];
			}
		}
		return $lun_id_arr;	
    }
	
	public function get_pair_type_array()
	{
		global $logging_obj;
		$sol_type = array();
		$sol_type_sql = "SELECT solutionType , planId FROM applicationPlan";
		$sol_type_res = $this->conn->sql_query($sol_type_sql);
		if( !$sol_type_res ) 
		{
			trigger_error( $conn_obj->sql_error() );
		}
		if($this->conn->sql_num_row($sol_type_res) > 0)
		{
			while($sol_type_row = $this->conn->sql_fetch_object($sol_type_res))
			{
				$pair_type = 0;	
				if($sol_type_row->solutionType == 'PRISM')
				{
					$pair_type = 2;
				}
				elseif($sol_type_row->solutionType == 'ARRAY')
				{
					$pair_type = 3;
				}
				$sol_type[$sol_type_row->planId] = $pair_type;
			}
		}
		return $sol_type;
	}


	public function get_file_system_type($host_id,$device_name)
    {
		$result = explode("\\\\",$device_name);
		$count = count($result);
		if($count == 1)
		{
			$device_name = $this->conn->sql_escape_string($device_name);
		}
       $query = "SELECT
					fileSystemType
				FROM
					logicalVolumes
				WHERE
					deviceName = '$device_name'
				AND
					hostId = '$host_id'";
		$result = $this->conn->sql_query($query);
		if($result)
		{	
			if($this->conn->sql_num_row($result)>0)
			{	
				while($row = $this->conn->sql_fetch_object($result))
				{
					$file_system_type = $row->fileSystemType;				
				}
			}
		}	
		return $file_system_type;
    }
	
	public function get_phy_lun_id($device_name,$host_id)
	{
		$sql = "SELECT
					Phy_Lunid 
				FROM	
					logicalVolumes
				WHERE
					hostId = '$host_id' AND
					deviceName = '$device_name'";
		$result = $this->conn->sql_query($sql);
		if($result)
		{	
			if($this->conn->sql_num_row($result)>0)
			{
				$row = $this->conn->sql_fetch_object($result);
				$phy_lun_id = $row->Phy_Lunid;
			}
			else
			{
				$phy_lun_id = 0;
			}
		}
		return $phy_lun_id;
	}
	public function get_application_type_by_hostid($host_id)
	{
		$sql = "SELECT
					applicationType
				From
					applicationHosts
				WHERE
					hostId = '$host_id'";
		$result = $this->conn->sql_query($sql);
		while($row = $this->conn->sql_fetch_object($result))
		{
			$applicationType[] = $row->applicationType;			
		}
		return $applicationType;
	}

	public function RestartResyncReplicationPairsWithPause($src_host_id , $dest_host_id, $resync_required)
	{
		global $logging_obj;
		$result = array();
		$condition = "";
		if($resync_required == "YES")$condition = "AND ShouldResyncSetFrom !=0";
		
		$sql = "SELECT
									sourceHostId,
									sourceDeviceName,
									destinationHostId,
									destinationDeviceName,
									deleted ,
									replication_status, 
									jobId, 
									restartPause,
									ruleId 
							  FROM
							        srcLogicalVolumeDestLogicalVolume
							  WHERE
							        sourceHostId = ?
							  AND 
									destinationHostId = ? 
							  $condition
							  AND
								executionState IN (1,0)";

		$result = $this->conn->sql_query($sql, array($src_host_id, $dest_host_id));
		if(count($result) > 0)
		{
			foreach($result as $key=>$val)
			{
				$source_id = $val['sourceHostId'];
				$source_device_name = $val['sourceDeviceName'];
				$destHostId = $val['destinationHostId'];
				$destDev = $val['destinationDeviceName'];
				$deleted = $val['deleted'];
				$job_id = $val['jobId'];
				$replication_status = $val['replication_status'];
				$restartPause = $val['restartPause'];
				$ruleId = $val['ruleId'];
				$is_protection_rollback_progress = $this->commonfunctions_obj->is_protection_rollback_in_progress($source_id);	
				//General pause pending or paused pair got restart re-sync request, allow directly to PS/Target cleanup pending with immediate re-start re-sync.
				/*if (($restartPause == 0) && ($replication_status == 41 || $replication_status == 26) && ($deleted == 0) && ($is_protection_rollback_progress == 0))
				{
					$res = $this->resync_now($ruleId);
				}*/
				//If jobid = 0 or PS/Target cleanup pending is in progress or pair marked for deletion or restart pause marked, ignore re-start re-sync.
				//Re-start pause finally goes to re-start re-sync
				//Pair marked for deletion, not required to send to pause state.
				//PS/Target cleanup pending or jobid is 0 means already initiated for re-start re-sync. Ignore if we get re-start re-sync request again.
				//Ignore rollback in progress time.
				if (($job_id == 0) || ($replication_status == 42) ||  ($deleted == 1) || ($restartPause == 1) || ($is_protection_rollback_progress == 1) || (($restartPause == 0) && ($replication_status == 41 || $replication_status == 26)))
				{
					//Ignore restart re-sync
					$logging_obj->my_error_handler("INFO",array("SourceHostId"=>$source_id, "SourceDeviceName"=>$source_device_name, "DestinationHostId"=>$destHostId, "DestinationDeviceName"=>$destDev, "RuleId"=>$ruleId), Log::BOTH);
				}
				else 
				{
					$this->pause_replication($source_id, $source_device_name, $destHostId, $destDev,1,0,"resync required");
				}
				$logging_obj->my_error_handler("INFO",array("SourceHostId"=>$source_id, "SourceDeviceName"=>$source_device_name, "DestinationHostId"=>$destHostId, "DestinationDeviceName"=>$destDev, "RuleId"=>$ruleId, "JobId"=>$job_id, "ReplicationStatus"=>$replication_status, "Deleted"=>$deleted, "RestartPause"=>$restartPause, "RollbackState"=>$is_protection_rollback_progress), Log::BOTH);
			}
		}
		else
		{
			$logging_obj->my_error_handler("INFO",array("Reason"=>"No pairs found", "SourceHostId"=>$src_host_id, "DestinationHostId"=>$dest_host_id , "Sql"=>$sql),Log::BOTH);
		}
		//All updates are under transaction, directly returning TRUE. 
		return true;
	}
	
	public function ResumeReplicationPairs($src_host_id,$tgt_host_id, $pause_retention = 0)
	{
		global $PAUSE_COMPLETED,$logging_obj;
		
		if(is_array($src_host_id)) $src_host_id = implode("','",$src_host_id);
		else
		{
			$srcIp = $this->commonfunctions_obj->getHostIpaddress($src_host_id);
			$destIp = $this->commonfunctions_obj->getHostIpaddress($tgt_host_id);
		}
		
		$query="SELECT
					pairId,
					ruleId,
					sourceDeviceName,
					destinationDeviceName
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					sourceHostId IN ('$src_host_id')
				AND
					destinationHostId = '$tgt_host_id'
				AND
					replication_status = '$PAUSE_COMPLETED'
				AND					
					tar_replication_status = 1";
		$logging_obj->my_error_handler("INFO",array("SourceHostId"=>$src_host_id ,"DestinationHostId"=>$tgt_host_id ,"Query"=>$query),Log::BOTH);
		$result = $this->conn->sql_query($query);	
		$ruleId = $pairId = $paireArr = array();
		while($data = $this->conn->sql_fetch_object($result))
		{
			$pairId[] = $data->pairId;
			$ruleId[] = $data->ruleId;
			$paireArr[] = $data->sourceDeviceName."=".$data->destinationDeviceName;
		}
		$pairId = implode("','",$pairId);
		$pairInfo = implode("','",$paireArr);
		
		$sql = "UPDATE
					srcLogicalVolumeDestLogicalVolume
				SET
					replication_status = 0,
					tar_replication_status = 0,
					pauseActivity = NULL,
					autoResume    = 0,
					deleted = 0
				WHERE
					pairId IN('$pairId')";
		$logging_obj->my_error_handler("INFO",array("Query"=>$sql), Log::BOTH);
		$result = $this->conn->sql_query($sql);
		$this->commonfunctions_obj->writeLog($_SESSION['username'],"Replication pairs has been resumed from vCon for (($srcIp => $destIp), ('$pairInfo'))");
		
		if($pause_retention)
		{
			$this->updateRetentionPolicy($ruleId, 0);
		}
		return true;
	}
	
	public function get_pair_status($srcHostId, $destHostId)
	{
		global $PAUSE_COMPLETED,$PAUSED_PENDING,$logging_obj;
		$sql = "SELECT
					replication_status
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					sourceHostId = '$srcHostId'
				AND
					destinationHostId = '$destHostId'";
		$result = $this->conn->sql_query($sql);
		$pair_count = $this->conn->sql_num_row($result);
		$flag = 0;
		while($rs_obj = $this->conn->sql_fetch_object($result))
		{
			$logging_obj->my_error_handler("INFO",array("ReplicationStatus"=>$rs_obj->replication_status), Log::BOTH);
			if($rs_obj->replication_status == $PAUSE_COMPLETED)
			{
				$flag++;
			}
		}
		$logging_obj->my_error_handler("INFO",array("PairCount"=>$pair_count , "Flag"=>$flag , "Sq1"=>$sql), Log::BOTH);
		return ($pair_count == $flag)?3:1;				
	}
 
	public function unregister_vx_agent ($id, $flag='')
	{
		global $logging_obj,$MDS_ACTIVITY_ID,$WEB_PATH;
		global $STOP_PROTECTION_PENDING,$bandwidth_obj,$ps_obj, $commonfunctions_obj;
		$status_flag = FALSE;
		if(!is_object($bandwidth_obj))$bandwidth_obj = new BandwidthPolicy();
		if(!is_object($ps_obj))$ps_obj = new ProcessServer();
		
		$fabric_flag =FALSE;
		
		$host_info = $this->commonfunctions_obj->get_host_info($id);
		$logging_obj->my_error_handler("INFO", array("HostId"=>$id,"HostInfo"=>$host_info), Log::BOTH); 
		$hostid = $host_info['id'];
		$hostname = $host_info['name'];
		$ipaddress = $host_info['ipaddress'];
		$sentinelEnabled = $host_info['sentinelEnabled'];
		$outpostAgentEnabled = $host_info['outpostAgentEnabled'];
		
		try
		{
			$this->conn->sql_query("BEGIN");
			$query="SELECT
						sourceHostId,
						sourceDeviceName, 
						destinationHostId,
						destinationDeviceName,
						pairId
					FROM 
						srcLogicalVolumeDestLogicalVolume
					WHERE
						sourceHostId='".$id."'
						OR
						destinationHostId='".$id."'";
			$logging_obj->my_error_handler('INFO','unregister_vxagent Select after volume_obj '.$query); 
			$iter = $this->conn->sql_query($query);
			if(!$iter)
			{
				throw new Exception("Unable to fetch the data from pair table $query");
			}
			
			$num_rows = $this->conn->sql_num_row($iter);
			if ($num_rows > 0)
			{
				$pair_ids = array();
				while ($myrow = $this->conn->sql_fetch_array($iter, MYSQL_NUM))
				{
					$pair_ids[] = $myrow[4];
					$includePath = $WEB_PATH."/om_functions.php";
					include_once("$includePath");
					
						$logging_obj->my_error_handler("INFO",array("Control"=>"Entered"), Log::BOTH);
						$this->stop_replication($myrow[0],$myrow[1],$myrow[2],$myrow[3]);
						
						//Cleaning up the applicationScenario table information.
						
						$delete_app_scenario = "DELETE aps FROM
													  applicationScenario aps, applicationPlan ap 
													 WHERE 
													 (aps.sourceId LIKE '%$id%' OR
													  aps.targetId LIKE '%$id%') AND ap.solutionType =  'HOST' 
											  AND aps.planId = ap.planId";								
						$app_scenario_result = $this->conn->sql_query($delete_app_scenario);
						$logging_obj->my_error_handler("INFO",array("Sql"=>$delete_app_scenario), Log::BOTH);
				}
				$pair_id = implode("','",$pair_ids);
				$tag_sql = "DELETE FROM retentionTag WHERE pairId IN ('$pair_id')";
				$tag_rs = $this->conn->sql_query($tag_sql);
				$ts_sql = "DELETE FROM retentionTimeStamp WHERE pairId IN ('$pair_id')";
				$ts_rs = $this->conn->sql_query($ts_sql);
			}
				
			if($fabric_flag)
			{
				$logging_obj->my_error_handler('INFO','Returnig back'); 
				$this->conn->sql_query("COMMIT");
				RETURN;	
			}			
			
			//Cleaning up the applicationScenario table information.
			$delete_app_scenario = "DELETE FROM
											applicationScenario
										WHERE
											sourceId LIKE '%$id%' OR
											targetId LIKE '%$id%'";
			$app_scenario_result = $this->conn->sql_query($delete_app_scenario);
			$logging_obj->my_error_handler("INFO",array("Sql"=>$delete_app_scenario), Log::BOTH);

			if(!$app_scenario_result)
			{
				throw new Exception("Unable to delete the entries from applicationScenario table delete_app_scenario::$delete_app_scenario");
			}
			
			//Fetching the policy id to delete the polcies from the policy table.
			$policy_sql = "SELECT
								policyId
							FROM
								policy
							WHERE
								hostId LIKE '%$id%'";
			$policy_result = $this->conn->sql_query($policy_sql);
			$logging_obj->my_error_handler("INFO",array("Sql"=>$policy_sql), Log::BOTH);

			if(!$policy_result)
			{
				throw new Exception("unable to fetch the data from policy table");
			}
			
			$policy_id_list = array();
			while($row = $this->conn->sql_fetch_object($policy_result))
			{
				$policy_id_list[] = $row->policyId;
			}
			$policy_id_list = implode("','",$policy_id_list);
			
			//Cleaning up the policy table information.
			$policy_del_sql = "DELETE FROM
									policy
								WHERE
									policyId IN ('$policy_id_list')";
			$del_policy_result = $this->conn->sql_query($policy_del_sql);
			$logging_obj->my_error_handler("INFO",array("Sql"=>$policy_del_sql), Log::BOTH);
			
			if(!$del_policy_result)
			{
				throw new Exception("unable to delete data from policy table policy_del_sql::$policy_del_sql");
			}
			
			if($flag !=1)
			{
				//Updating the Sentinal & outpost version to empty.
				$update_sql = "UPDATE 
										hosts 
								   SET 
										sentinelEnabled='0',
										outpostAgentEnabled='0',
										sentinel_version='',
										outpost_version='',
										involflt_version='',
										PatchHistoryVX='' 
								   WHERE id='$id'";
				
				$result_update_sql = $this->conn->sql_query ($update_sql);
				$logging_obj->my_error_handler("INFO",array("Sql"=>$update_sql), Log::BOTH);
				if(!$result_update_sql)
				{
					throw new Exception("unable to update the hosts table update_sql::$update_sql");
				}
				
				//Cleaning up the logical volumes table information.
				$delete_lv_sql = "DELETE
										FROM 
											logicalVolumes
										WHERE hostId='$id'";
				$delete_lv = $this->conn->sql_query ($delete_lv_sql);
				$logging_obj->my_error_handler("INFO",array("Sql"=>$delete_lv_sql), Log::BOTH);
				if(!$delete_lv)
				{
					throw new Exception("unable to delete from logical volume table delete_lv_sql::$delete_lv_sql");
				}
				
				//Finally cleaning up the hosts table information.
				$delete_query = "DELETE 
										FROM
											hosts
										WHERE 
											sentinelEnabled='0' 
											AND
											outpostAgentEnabled='0' " .
											"AND 
											filereplicationAgentEnabled='0'
								AND 
									(processServerEnabled = '0'
										OR 
									processServerEnabled IS NULL)    
								AND 
									id='$id'";
									
									
				$delete_hosts = $this->conn->sql_query ($delete_query);
				$logging_obj->my_error_handler("INFO",array("Sql"=>$delete_query), Log::BOTH);
				if(!$delete_hosts)
				{
					throw new Exception("unable to delete from hosts table delete_query::$delete_query");
				}
				
				$hs_nt_sql = "DELETE FROM
									hostNetworkAddress
								WHERE
									hostId='$id'";
				$hs_nt_result = $this->conn->sql_query($hs_nt_sql);
				$logging_obj->my_error_handler("INFO",array("Sql"=>$hs_nt_sql), Log::BOTH);
				if(!$hs_nt_result)
				{
					throw new Exception("unable to delete from hostNetworkAddress table hs_nt_sql::$hs_nt_sql");
				}	
                $infraHostId = "SELECT infrastructureHostId FROM infrastructurevms WHERE hostId = '$id'";
                $infraHostIdRes = $this->conn->sql_query($infraHostId);
                if(!$infraHostIdRes)
                {
                    throw new Exception("Unable to fetch the infrastructure HostId for given hostId $id. \n");
                }
                $num_rows = $this->conn->sql_num_row($infraHostIdRes);
               
                if ($num_rows > 0)
                {		
                    $row = $this->conn->sql_fetch_object($infraHostIdRes);
                    $infrastructureHostId = $row->infrastructureHostId;
                    $invId = "SELECT inventoryId FROM infrastructureHosts WHERE infrastructureHostId = '$infrastructureHostId'";
                    $invIdRes = $this->conn->sql_query($invId);
                    if(!$invIdRes)
                    {
                        throw new Exception("Unable to fetch the infrastructure inventory id for given hostId $id with infrastructure host id $infrastructureHostId. \n");
                    }
                    $num_rows = $this->conn->sql_num_row($invIdRes);
                    if ($num_rows > 0)
                    {				
                        $row = $this->conn->sql_fetch_object($invIdRes);
                        $inventoryId = $row->inventoryId;
                         $infraInvSql = "DELETE FROM
                            infrastructureInventory
                        WHERE
                            inventoryId='$inventoryId'
                            AND 
                            hostType = 'Physical'";
                        $infraInvSqlResult = $this->conn->sql_query($infraInvSql);
						$logging_obj->my_error_handler("INFO",array("Sql"=>$infraInvSql), Log::BOTH);

						
                        if(!$infraInvSqlResult)
                        {
                            throw new Exception("unable to delete from InfraStructureInventory table infraInvSql::$infraInvSql");
                        }   
                     }
                }
				$infrastruture_vm_sql = "UPDATE 
									infrastructureVMs
								SET
									hostId = '',
									replicaId = ''
								WHERE
									hostId='$id'";
				$infrastruture_vm_result = $this->conn->sql_query($infrastruture_vm_sql);
				$logging_obj->my_error_handler("INFO",array("Sql"=>$infrastruture_vm_sql), Log::BOTH);
				if(!$infrastruture_vm_result)
				{
					throw new Exception("unable to update infrastructureVMs table infrastruture_vm_sql :: $infrastruture_vm_sql");
				}
			}	
			
			$user_name = 'System';
			if($_SESSION['username'])
			{
				$user_name = $_SESSION['username'];
			}
			
			$msg = $this->commonfunctions_obj->customer_branding_names("VX agent unregistered from CX with details (hostName::$hostname, ipaddress::$ipaddress, hostid::$hostid, sentinelEnabled::$sentinelEnabled, outpostAgentEnabled::$outpostAgentEnabled)");
			$this->commonfunctions_obj->writeLog($user_name,$msg);
			
			$mds_obj = new MDSErrorLogging();
			$details_arr = array(
								'activityId' 	=> $MDS_ACTIVITY_ID,
								'jobType' 		=> 'Unregistration',
								'errorString'	=> $msg,
								'eventName'		=> 'CS'
								);
			$mds_obj->saveMDSLogDetails($details_arr);
			$logging_obj->my_error_handler("INFO",array("Details"=>$details_arr), Log::BOTH);			
			$this->conn->sql_query("COMMIT");
			$status_flag = TRUE;
		}
		catch(Exception $e)
		{
			$exception_message = $e->getMessage();	
			$logging_obj->my_error_handler("INFO",array("ExceptionMessage"=>$exception_message), Log::BOTH);			
			$this->conn->sql_query("ROLLBACK");
		}
		return $status_flag;
	}
	
	/*
     * Function Name: get_new_pairId
     *
     * Description:
     * Returns a random number that is not in use as a pairId
     *
     *
     * Parameters:
     *     Param 1 [OUT]:
     *
     * Return Value:
     *     Ret Value: Returns scalar value .
     *
     * Exceptions:
     *     DataBase Connection fails.
     */
    private function get_new_pairId ()
    {

        $eids = array ();
        $query = "SELECT
                    pairId
                  FROM
                    srcLogicalVolumeDestLogicalVolume";
        $result_set = $this->conn->sql_query ($query );
		
        while ($row = $this->conn->sql_fetch_object ($result_set))
        {
            $eids [] = $row-> pairId;
        }

        $random = mt_rand (1, mt_getrandmax ());
        while (in_array ($random, $eids))
        {
            $random = mt_rand (1, mt_getrandmax ());
        }

        return $random;
    }
		
	public function unregister_ps($host_id)
	{
		global $UNINSTALL_PENDING, $commonfunctions_obj, $logging_obj;
		$host_info = $commonfunctions_obj->get_host_info($host_id);
		
		$sql = "UPDATE hosts SET agent_state='$UNINSTALL_PENDING' WHERE id='$host_id' AND processServerEnabled=1";
		$this->conn->sql_query($sql);
		$logging_obj->my_error_handler("INFO",array("Id"=>$id,"Sql"=>$sql, "HostInfo"=>$host_info), Log::BOTH);			
		$hostid = $host_info['id'];
		$hostname = $host_info['name'];
		$ipaddress = $host_info['ipaddress'];
		
		$user_name = 'System';
		if($_SESSION['username'])
		{
			$user_name = $_SESSION['username'];
		}		
		$commonfunctions_obj->writeLog($user_name,$commonfunctions_obj->customer_branding_names("PS is unregistered from CX with details (hostName::$hostname, ipaddress::$ipaddress, hostid::$hostid)"));		
	}
	
	public function delete_MBR_files($host_id)
	{
		global $REPLICATION_ROOT,$SLASH,$RM,$logging_obj;
		
		$mbr_file_path = $REPLICATION_ROOT.$SLASH."admin".$SLASH."web".$SLASH."SourceMBR".$SLASH."$host_id";
		$logging_obj->my_error_handler('INFO',"MBR PATH:: $mbr_file_path"); 
		
		if(file_exists($mbr_file_path)) {
			$cmd = "$RM -rf $mbr_file_path";
			$logging_obj->my_error_handler('INFO',"CMD:: $cmd");
			
			$ret = system("$cmd");
			if(!$ret) {
				$logging_obj->my_error_handler('INFO',"Failed to delete MBR Path :: $mbr_file_path");
			}
		}
	}
	
	private function get_tmid($sourceId, $sourceDeviceName,$oneToN,$process_server='')
    {
        global $MAX_TMID,$logging_obj;
		$tmId = 0;
		if($process_server)
		{
			$ps_os_flag = $this->commonfunctions_obj->get_osFlag($process_server);
			/*
			* In case of Windows PS max volsync count should not exceed 12,that is the reason assigning MAX_TMID to 12 
			* For Linux it will be 24
			*/
			if($ps_os_flag == 1)
			{
				$MAX_TMID = 12;
			}
 		}
        if ($oneToN)
        {
        /* 1-to-N set all to the same tmId*/
            $query = "SELECT
                        tmId "."
                      FROM
                        logicalVolumes "."
                      WHERE
                        hostId = '$sourceId'
                        AND
                         deviceName='$sourceDeviceName'";
            $result_set = $this->conn->sql_query ($query);
			$row = $this->conn->sql_fetch_object ($result_set);
            $tmId = $row->tmId;
        }
        
        if (0 == $tmId)
        {
			/* no tmid found or some error getting it*/
            $query="SELECT
                        ValueData
                    FROM
                        transbandSettings "."
                    WHERE
                        ValueName = 'NEXTTMID'";
            $result_set = $this->conn->sql_query($query );
			$row = $this->conn->sql_fetch_object ($result_set);
            $tmId = $row->ValueData;
			
            if ($tmId < $MAX_TMID)
            {
                $tmId++;
            }
            else
            {
                $tmId = 1;
            }
			
            $query = "UPDATE
                        transbandSettings "."
                      SET
                        ValueData = '".$tmId."'
                      WHERE
                        "."ValueName = 'NEXTTMID'";
            $result = $this->conn->sql_query($query);
		}
        return $tmId;
	}
	
	public function GetConfiguredPairInfo($src_host_id,$dest_host_id="")
	{	
		$target_host_cond = "";
		$result = array();
		if($dest_host_id)
		{
			$target_host_cond = " AND destinationHostId = '$dest_host_id'";
		}
		$select_sql = "SELECT
						sourceDeviceName,
						destinationDeviceName
					FROM
						srcLogicalVolumeDestLogicalVolume
					WHERE
						sourceHostId = '$src_host_id'
					$target_host_cond";
		$select_rs = $this->conn->sql_query($select_sql);
		$i=1;
		while($row  = $this->conn->sql_fetch_object( $select_rs ))
		{
			$srcDeviceName = $row->sourceDeviceName;			
			$destDeviceName = $row->destinationDeviceName;	
			$result[$i]["sourceDeviceName"] = $srcDeviceName;
			$result[$i]["destDeviceName"] = $destDeviceName;
			$i++;
		}
		return $result;
	}
	
	public function checkDeviceexists($host_id,$device_arr,$os_type,$src_or_trg,$apiFor='cisco')
	{
		$result = array();
		$esc_device_arr = array();
		$cond = '';
		
		//Common Conditions
		if(($os_type == 1) && ($apiFor == 'azure')) {
			
			$cond .= " AND volumeType='0'";
		}
		if(($os_type == 1) && ($apiFor == 'cisco')) {
			
			$cond .= " AND volumeType='5'";
		}
		
		//conditions specific for target
		if($src_or_trg == 2) {
			$cond .= " AND deviceFlagInUse='0' AND isUsedForPaging='0' AND systemVolume='0' AND cacheVolume='0'";
		}
		elseif($src_or_trg == 1) //conditions specific for source
		{
			$cond .= " AND farLineProtected='0' AND deviceFlagInUse='0'";
		}
		
		foreach($device_arr as $v) {
			$esc_device_arr[] = $this->conn->sql_escape_string($v);
		}
		$esc_device_str = implode("','",$esc_device_arr);
		
		$sql = "SELECT deviceName,mountPoint FROM logicalVolumes where hostId ='$host_id' AND ((deviceName IN('$esc_device_str')) OR (mountPoint IN('$esc_device_str'))) $cond";
		$rs = $this->conn->sql_query($sql);
		$registered_devices = array();
		while($row  = $this->conn->sql_fetch_object( $rs )) {
			$registered_devices[] = $row->deviceName;
			$registered_devices[] = $row->mountPoint;
		}
		
		foreach($device_arr as $v) {
			if((in_array($v,$registered_devices)) === false) {
				$result[] = $v;
			}
		}
		return $result;
	}
	
		/*
	* GetPairDeleteStatus function is used to tell whether pairs are deleted or not	
	*/
	
	public function GetPairDeleteStatus($planId, $srcHostId, $destHostId, $job_type)
	{
		global $logging_obj;
		if($job_type == 1)//1.VX pairs 2. FX pairs		
		{
			$sql = "SELECT
					deleted
				FROM
					srcLogicalVolumeDestLogicalVolume
				WHERE
					sourceHostId = '$srcHostId'
				AND
					destinationHostId = '$destHostId'
				AND
					planId = '$planId'";
			$result = $this->conn->sql_query($sql);		
			$logging_obj->my_error_handler("INFO",array("PairCount"=>$pair_count,"Sql"=>$sql), Log::BOTH);			
		}
		else if($job_type == 2)
		{
			$sql = "SELECT
				deleted
			FROM
				frbJobConfig
			WHERE
				sourceHost = '$srcHostId' AND
				destHost = '$destHostId' AND
				planId = '$planId'";
			$result = $this->conn->sql_query ($sql);	
		}
		$delete_flag = 0;
		$pair_count = $this->conn->sql_num_row($result);
		if($pair_count)
		{
			while($pair_info = $this->conn->sql_fetch_object($result))
			{	
				if($pair_info->deleted == 1)
				{
					$delete_flag = 1;
					break;
				}
			}			
		}
		return array("pair_count" => $pair_count, "delete_flag" => $delete_flag);		
	}
	
    private function cleanupPolicy($src_host_id)
    {
        global $logging_obj;
        $logging_obj->my_error_handler("DEBUG","cleanupPolicy:: src_host_id:$src_host_id \n");
        
        $pair_exists = $this->conn->sql_get_value("srcLogicalVolumeDestLogicalVolume","sourceHostId","sourceHostId='$src_host_id'");
        $logging_obj->my_error_handler("DEBUG","cleanupPolicy:: pair_exists:$pair_exists \n");
        if(!$pair_exists)
        {
            $sql = "SELECT p.hostId as hostId, p.policyId as policyId, p.planId as planId FROM policy p, consistencyGroups cg WHERE p.policyId=cg.policyId AND p.hostId LIKE '%$src_host_id%'";
            $policy_data = $this->conn->sql_exec($sql);
            $logging_obj->my_error_handler("DEBUG","cleanupPolicy:: policy_data: \n".print_r($policy_data));
            if(count($policy_data))
            {
                foreach($policy_data as $policy_info)
                {
                    $policy_id = $policy_info["policyId"];
                    $hostId = $policy_info["hostId"];
                    $planId = $policy_info["planId"];
                }
                $existing_host_ids = explode(",",$hostId);
                $logging_obj->my_error_handler("DEBUG","cleanupPolicy:: policy_id:$policy_id \n hostId::$hostId \n existing_host_ids::$existing_host_ids");
                $this->conn->sql_query("DELETE FROM policyInstance WHERE policyId=$policy_id AND hostId='$src_host_id'");
                 
                if(count($existing_host_ids) == 1 && $hostId == $src_host_id)
                {
                    $this->conn->sql_query("DELETE FROM consistencyGroups WHERE policyId=$policy_id");
                    $this->conn->sql_query("DELETE FROM policy WHERE policyId=$policy_id");
                }
                else
                {
                    #$pattern = '/(,?)' . $src_host_id . '(,?)/';
                    #$new_host_id = preg_replace_callback($pattern, create_function('$matches', 'return (empty($matches[1]) || empty($matches[3])) ? null : ",";'), $hostId);
					$plan_details = $this->conn->sql_get_value("applicationPlan","planDetails","planId = ?", array($planId));
					$plan_info = unserialize($plan_details);
					$consistency_interval = $plan_info['ConsistencyInterval'];
					
					foreach($existing_host_ids as $key=>$host_id)
					{
							if ($host_id == $src_host_id)
							{
									unset($existing_host_ids[$key]);
							}
					}
                    $logging_obj->my_error_handler("INFO",array("ExistingHostIds"=>$existing_host_ids), Log::BOTH);
					
                    $this->protection_plan_obj->update_distributed_consistency_policy($planId, $existing_host_ids, $consistency_interval);
                }
            }
			else
			{
				$this->conn->sql_query("DELETE FROM policy WHERE hostId='$src_host_id' and policyType='4'"); // PolicyType 4 : ConsistencyPolicy
			}
		}
    }

	/*
	* Function Name: planExists
    *
	* Description:
    * Check if the protection plan exists
	*  
	* Parameters: 
	*   $plan_id: plan id (primary key id) of applicationPlan table
    *	
	* Return Value:
	*   Boolean
	*/
	public function planExists($plan_id)
	{
		$sql = "SELECT 
                    count(*) AS no_of_plans
                FROM 
                    applicationPlan  
                WHERE 
                    planId = ? AND planType = 'CLOUD'";
        $rs = $this->conn->sql_query($sql, array($plan_id));
        $plan_exists = ($rs[0]['no_of_plans'] > 0) ? 1 : 0;

        return $plan_exists;
	}
    
	/*
	* Function Name: getSourceHostIdsByPlanId
    *
	* Description:
    * Fetch all the source host ids in a protection plan
	*  
	* Parameters: 
	*   $plan_id: plan id (primary key id) of applicationPlan table
    *	
	* Return Value:
	*   Array
	*/
	public function getSourceHostIdsByPlanId($plan_id)
	{
		global $SCENARIO_DELETE_PENDING;
		$sql = "SELECT 
                    DISTINCT sourceId 
                FROM 
                    applicationScenario 
                WHERE 
                    planId = ? AND scenarioType = 'DR Protection' AND executionStatus != ?";
        $rs = $this->conn->sql_query($sql, array($plan_id,$SCENARIO_DELETE_PENDING));
        
        $arr_source_host_ids = array();
		if(count($rs) > 0)
		{
			foreach($rs as $data)
			{
				$arr_source_host_ids[] = $data['sourceId'];
            }
        }
        return $arr_source_host_ids;
	}
    
	/*
	* Function Name: updatePairForDeletion
    *
	* Description:
    * Set the replication pairs for deletion by updating the relevant columns
    * in applicationScenario and srcLogicalVolumeDestLogicalVolume tables
	*  
	* Parameters: 
	*   $plan_id: plan id (primary key id) of applicationPlan table
	*   $arr_source_host_ids: array of hostids
	*   $force_delete: whether the pairs are to be forcefully deleted. values can be 0 or 1 only
    *	
	* Return Value:
	*   None
	*/
    public function updatePairForDeletion($plan_id, $arr_source_host_ids, $force_delete, $target_data_plane=1)
    {
        global $SCENARIO_DELETE_PENDING, $RELEASE_DRIVE_ON_TARGET_DELETE_PENDING, $PS_DELETE_DONE;
        global $SOURCE_DELETE_DONE;
        $arr_query_params = array($plan_id);
        $source_host_ids = (count($arr_source_host_ids)) ? implode(',', $arr_source_host_ids) : '';
        $cond_sql1 = '';
        $cond_sql2 = '';
        $cond_sql3 = '';
        if($source_host_ids) 
        {
            $cond_sql1 = "AND FIND_IN_SET(sourceId, ?)";
            $cond_sql2 = "AND FIND_IN_SET(sourceHostId, ?)";
            $cond_sql3 = "AND FIND_IN_SET(srcHostId, ?)";
            array_push($arr_query_params, $source_host_ids);
        }
        
        $sql1 = "UPDATE applicationScenario SET executionStatus = '$SCENARIO_DELETE_PENDING' WHERE planId = ? AND scenarioType = 'DR Protection' $cond_sql1";
        $rs1 = $this->conn->sql_query($sql1, $arr_query_params);
        if($force_delete == 0)
        {
            $sql4 = "SELECT sourceHostId,sourceDeviceName,destinationHostId,destinationDeviceName,processServerId,deleted FROM srcLogicalVolumeDestLogicalVolume WHERE planId = ? $cond_sql2";
			$rs4 = $this->conn->sql_query($sql4, $arr_query_params);
			
			for($i=0;isset($rs4[$i]);$i++) 
            {
				$delete_array = array();
				$cond = '';
				$sourceHostId = $rs4[$i]['sourceHostId'];
				$sourceDeviceName = $rs4[$i]['sourceDeviceName'];
				$destinationHostId = $rs4[$i]['destinationHostId'];
				$destinationDeviceName = $rs4[$i]['destinationDeviceName'];
				$processServerId = $rs4[$i]['processServerId'];				
				$deleted = $rs4[$i]['deleted'];
				
				if($deleted == 0) 
				{
					$get_sync_state = $this->get_qausi_flag($sourceHostId,$sourceDeviceName,$destinationHostId,$destinationDeviceName);
					$replication_state = "$RELEASE_DRIVE_ON_TARGET_DELETE_PENDING";
					$cond = ", replicationCleanupOptions = ?";
					
                    if ($target_data_plane == 2)
                    {
                        if(($get_sync_state == 0) || ($processServerId == $destinationHostId))	
                        {
                            $rep_options = "0";
                            $replication_state = "$SOURCE_DELETE_DONE";
                        }
                        else 
                        {
                            $rep_options = "1000000000";
                        }
                        #print "$replication_state,$rep_options";
                    }
                    else
                    {
                        if(($get_sync_state == 0) || ($processServerId == $destinationHostId))	
                            $rep_options = "0010101000";
                        else 
                            $rep_options = "1010101000";
                    }
					array_push($delete_array, $rep_options);
				}	
				else 
                    $replication_state = "$PS_DELETE_DONE";
				
				$sql2 = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status = '$replication_state', deleted = '1' $cond WHERE sourceHostId=? AND destinationHostId = ? AND destinationDeviceName = ? AND planId = ? ";
                #print $sql2."\n";
				array_push($delete_array,$sourceHostId,$destinationHostId,$destinationDeviceName,$plan_id);
				$rs2 = $this->conn->sql_query($sql2, $delete_array);
			}
		}
        else
        {
           $sql2 = "UPDATE srcLogicalVolumeDestLogicalVolume SET replication_status = '$PS_DELETE_DONE', deleted = '1' WHERE planId = ? $cond_sql2";
             $rs2 = $this->conn->sql_query($sql2, $arr_query_params);
        }
        
        if ($target_data_plane != 2)
        {
            $sql3 = "DELETE FROM srcMtDiskMapping WHERE planId = ? $cond_sql3";
            $rs3 = $this->conn->sql_query($sql3, $arr_query_params);
        }
        #exit;
    }
	
	//Added this function for premium storage support.
	function get_plans_info_without_protection_details($plan_id=NULL, $src_host_ids=NULL)
	{
		$sql_cond_one = "";
		$plan_ids = array();
		$sql_args = array();
		$plan_details = array();
		if($plan_id)
		{
			$plan_ids[] = $plan_id;
		}
		
		if($src_host_ids)
		{
			$host_ids_str = implode(",", $src_host_ids);
			$sql  = "select planId from applicationScenario where sourceId in (?)";
			$plan_id_details = $this->conn->sql_query($sql ,array($host_ids_str));
			if (isset($plan_id_details))
			{
				foreach ($plan_id_details as $value)
				{
					$plan_ids[] = $value['planId'];
				}
			}			
		}	

		$plan_ids = array_unique($plan_ids);
		if (count($plan_ids) > 0)
		{
			$plan_ids = implode(",",$plan_ids);
			$sql_cond_one =" and ap.planId in (?) ";
			$sql_args[] = $plan_ids;
		}
		
		$sql_cond_two =" ap.planId not in (select planId from srcLogicalVolumeDestLogicalVolume) ";
		
		$sql = "select 
					ap.planId, 
					ap.planDetails, 
					ap.planName,
					ap.solutionType 
				from 
					applicationPlan ap  
				where 
				$sql_cond_two $sql_cond_one and ap.planType = ?";
		$sql_args[] = 'CLOUD';
		$plan_details = $this->conn->sql_query($sql ,$sql_args);
		return $plan_details;
	}
	
	function get_protection_health_factors($plan_id=NULL, $src_host_ids=NULL)
	{
		global $DETECTION_TIME_INCLUSION_HF_LIST;
		$health_factors = array();
		$sql_cond = "";
		$sql_args = array();
		
		if($plan_id)
		{
			$sql_cond .= " AND p.planId = ?";
			$sql_args[] = $plan_id;
		}
		
        
		if($src_host_ids)
		{
			$host_ids_str = implode(",", $src_host_ids);
			$sql_cond .= " AND FIND_IN_SET(p.sourceHostId, ?)";
			$sql_args[] = $host_ids_str;
		}
		
		$sql = "SELECT
					h.pairId,
					h.healthFactorCode,
					h.healthFactor,
					h.healthDescription,
					h.healthFactorType,
					h.healthPlaceHolders,
					unix_timestamp(h.healthTime) as healthDetectionTime					
				FROM 
					healthfactors h,
					srcLogicalVolumeDestLogicalVolume p 
				WHERE	
					p.destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' 
					AND 
					p.pairId = h.pairId ".
					$sql_cond;					
        $pairs_details = $this->conn->sql_query($sql, $sql_args);         

		foreach($pairs_details as $pair)
        {
            $pair_id = $pair['pairId'];
            $hf_code = $pair['healthFactorCode'];
			$hf = $pair['healthFactor'];
			$hf_description = $pair['healthDescription'];
			$hf_type = $pair['healthFactorType'];
			$hf_place_holder = $pair['healthPlaceHolders'];
			$hf_detection_time = $pair['healthDetectionTime'];						
			
			$health_factors[$pair_id][$hf_code] = array("healthfactor"=>$hf, "description"=>$hf_description, "detectiontime"=>$hf_detection_time, "healthfactortype" => $hf_type, "healthPlaceHolders" => $hf_place_holder);
		}
		return $health_factors;
	}
	 /*
    * Function Name: get_pair_details
    *
    * Description: This function is used to get the pair details
    *   Plan, Server and Pair level information
    * 
    * Parameters:
    *     Param 1 [IN]:planId
    *     Param 2 [IN]:src_host_ids
    *     Param 6 [OUT]:pair details
    *
    * Return Value:
    *     Ret Value: Array
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    function get_pair_details($plan_id=NULL, $src_host_ids=NULL, $protection_health_factors=array())
    {
		global $DEFAULT_HF_DETECTION_TIME, $RESYNC_HEALTH_CODE_FOR_BACKWARD_AGENT;
		$sql_cond = "";
		$sql_args = array();
		$vm_health_factor = 'VM';
		$disk_health_factor = 'DISK';
		
		if($plan_id)
		{
			$sql_cond .= " AND p.planId = ?";
			$sql_args[] = $plan_id;
		}
		
        
		if($src_host_ids)
		{
			$host_ids_str = implode(",", $src_host_ids);
			$sql_cond .= " AND FIND_IN_SET(p.sourceHostId, ?)";
			$sql_args[] = $host_ids_str;
		}
        $get_pairs_sql = "SELECT
                                p.sourceHostId,
                                p.sourceDeviceName,
                                p.destinationHostId,
                                p.destinationDeviceName,
                                p.ruleId,
                                p.pairId,
                                p.planId,
                                p.scenarioId,
								p.volumeGroupId,
								p.currentRPOTime,
								UNIX_TIMESTAMP(p.statusUpdateTime) as statusUpdateTime,
								p.isResync,
								p.isQuasiflag,
								p.shouldResync,
								UNIX_TIMESTAMP(p.resyncEndTime) as resyncEndTime,
								(UNIX_TIMESTAMP(p.statusUpdateTime) - UNIX_TIMESTAMP(p.currentRPOTime)) as currRpo,
								(UNIX_TIMESTAMP(now()) - UNIX_TIMESTAMP(p.resyncStartTime)) as currRpoAtIR,
								p.lastResyncOffset,
                                p.rpoSLAThreshold,
                                (SUBSTRING(p.logsFromUTC, 1, 11) - 11644473600) as logsFrom,
                                (SUBSTRING(p.logsToUTC, 1, 11) - 11644473600) as logsTo,
                                p.processServerId,
                                p.executionState,
                                p.replication_status,
                                p.isCommunicationfromSource,
                                p.throttleresync,
                                p.throttleDifferentials,
								p.remainingResyncBytes as resyncBytes,
								p.remainingQuasiDiffBytes as quasiDiffBytes,
                                ps.cummulativeThrottle,
								(CASE WHEN UNIX_TIMESTAMP(resyncEndTime) = 0 THEN 'N/A' ELSE (UNIX_TIMESTAMP(resyncEndTime) - UNIX_TIMESTAMP(resyncStartTime)) END) resyncDuration,
                                dev.freeSpace,
								dev.fileSystemType,
								dev.cacheVolume,
								p.differentialPendingInSource as srcTransit,
								p.remainingDifferentialBytes as psTransit,
								p.differentialPendingInTarget as trgTransit,
                                ap.solutionType,
								ap.planDetails,
								ap.planName,
                                p.TargetDataPlane,
								p.restartResyncCounter,
								p.stats 
                           FROM 
                                srcLogicalVolumeDestLogicalVolume p,
                                processServer ps,
                                logicalVolumes dev,
                                applicationPlan ap
                           WHERE
                                p.destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' AND
                                p.processServerId = ps.processServerId AND
                                p.destinationHostId = dev.hostId AND
                                p.destinationDeviceName = dev.devicename AND
                                p.planId = ap.planId ".
								$sql_cond." group by p.pairId";		
        $pairs_details = $this->conn->sql_query($get_pairs_sql, $sql_args);
         
		if(!$pairs_details) return;
        $retention_details = array();
        $retention_pairs_sql = "SELECT
                                    P.ruleId,
                                    ret.currLogsize
                                FROM
                                    srcLogicalVolumeDestLogicalVolume p,
                                    retLogPolicy ret
                                WHERE
                                     p.ruleId = ret.ruleId".
                                     $sql_cond;
             
		// get all the vm deleted states for protected VMs
		$deleted_sql_args = array();
		$deleted_and_protected_vms = "SELECT 
										hostId, 
										isVMDeleted 
									FROM 
										infrastructureVMs 
									where 
									isVMDeleted = '1' AND 
									hostId IN (select distinct sourceHostId FROM srcLogicalVolumeDestLogicalVolume)";
		$deleted_and_protected_vm_details = $this->conn->sql_get_array($deleted_and_protected_vms, "hostId", "isVMDeleted", $deleted_sql_args);
		
        $retention_pairs_details = $this->conn->sql_query($retention_pairs_sql,$sql_args);
        if (isset($retention_pairs_details))
        {
            foreach ($retention_pairs_details as $value)
            {
                $retention_details[$value['ruleId']] = $value['currLogsize'];
            }
        }
		
		$resync_codes_available = array();
		$resync_codes_sql = "SELECT  
								min(Id), 
								sourceHostId, 
								sourceDeviceName, 
								resyncErrorCode, 
								detectedTime,
								placeHolders 
							FROM
								resyncErrorCodesHistory 
							GROUP BY 
								sourceHostId, sourceDeviceName";
		$result = $this->conn->sql_query ($resync_codes_sql);	
		$resync_code_count = $this->conn->sql_num_row($result);
		if($resync_code_count)
		{
			while($resync_codes = $this->conn->sql_fetch_object($result))
			{	
				$resync_codes_available[$resync_codes->sourceHostId][$resync_codes->sourceDeviceName]['resyncErrorCode'] = 
																							$resync_codes->resyncErrorCode;
				$resync_codes_available[$resync_codes->sourceHostId][$resync_codes->sourceDeviceName]['detectedTime'] = 
																							$resync_codes->detectedTime;
				$resync_codes_available[$resync_codes->sourceHostId][$resync_codes->sourceDeviceName]['placeHolders'] = 
																							$resync_codes->placeHolders;
			}			
		}
        
        $protection_details = array();
		$volume_details = $this->get_pair_volume_details();
        foreach($pairs_details as $pair)
        {
            $plan_id = $pair['planId'];
            $server_id = $pair['sourceHostId'];
            $src_device = $pair['sourceDeviceName'];
            $dest_host_id = $pair['destinationHostId'];
            $ps_id = $pair['processServerId'];
          	$target_data_palne = $pair['TargetDataPlane'];
			$protection_details[$plan_id]['PlanName'] = $pair['planName'];
            $protection_details[$plan_id]['solution_type'] = $pair['solutionType'];
			$consistency_intervals = unserialize($pair['planDetails']);
			$protection_details[$plan_id]['ConsistencyInterval'] = $consistency_intervals['ConsistencyInterval'];
			$protection_details[$plan_id]['CrashConsistencyInterval'] = $consistency_intervals['CrashConsistencyInterval'];
            $protection_details[$plan_id]['TargetDataPlane'] = $pair['TargetDataPlane'];
            // Collecting target servers perl plan
            $protection_details[$plan_id]['target_servers'][] = $dest_host_id;
			
			// Target host for the server
			$protection_details[$plan_id]['servers'][$server_id]['destHostId'] = $dest_host_id;
			$protection_details[$plan_id]['servers'][$server_id]['psId'] = $ps_id;
			
			// Setting RPO for server
			if($protection_details[$plan_id]['servers'][$server_id]['rpo'] < $pair['currRpo'])			
			{
				$protection_details[$plan_id]['servers'][$server_id]['rpo'] = $pair['currRpo'];
			}
			
			// Setting last RPO calculated time.
			if($protection_details[$plan_id]['servers'][$server_id]['lastRpoCalculatedTime'] < $pair['statusUpdateTime'])			
			{
				$protection_details[$plan_id]['servers'][$server_id]['lastRpoCalculatedTime'] = $pair['statusUpdateTime'];
			}
			
            $protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details'] = $pair;
			
			// Set the ProtectionStage
			$resync_progress = 'N/A';
			$disk_resync_progress = 0;
			
			if($pair['isResync'] == 1)
			{
				if($pair['executionState'] == 2 )
				{
					$state = 3; // Queued 
				}
				elseif($pair['isQuasiflag'] == 0 )
				{
					$state = 0; // Resyncing (Step I)
					// Get the ResyncProgress
					if (($pair['resyncEndTime'] == 0) && ($volume_details[$server_id][$src_device]['capacity'] != 0))
					{
						if ($pair['stats'])
						{
							$decoded_json_stats = json_decode($pair['stats'],TRUE);
							
							if (json_last_error() === JSON_ERROR_NONE) 
							{
								
								$resync_progress = $decoded_json_stats['ResyncPercentage'];
								$disk_resync_progress = round((($volume_details[$server_id][$src_device]['capacity']/100)*$decoded_json_stats['ResyncPercentage']),2);
							}
						}
						else
						{
							$resync_progress = round((($pair['lastResyncOffset'] * 100)/$volume_details[$server_id][$src_device]['capacity']), 2);
							$disk_resync_progress = $pair['lastResyncOffset'];
						}
					}
				}
				else if($pair['isQuasiflag'] == 1 ) 
					$state = 1;	// Resyncing (Step II)				
			}
			else
				$state = 2; // Differential Sync
			
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['protection_stage'] = $state;
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['resync_progress'] = $resync_progress;
			
			if($pair['isResync'] == 1) $resync_data_on_ps = $pair['resyncBytes']+$pair['quasiDiffBytes'];
			else $resync_data_on_ps = $pair['psTransit'];
   
			// Data in transit
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['DataTransit']['Source'] = round(($pair['srcTransit'] / 1024 / 1024), 2);// Converting to MB
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['DataTransit']['PS'] = round(($resync_data_on_ps / 1024 / 1024), 2);// Converting to MB
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['DataTransit']['Target'] = round(($pair['trgTransit'] / 1024 / 1024), 2);// Converting to MB

			if($pair['executionState'] == 2)			
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['rpo'] = "N/A";
			}
			elseif ($pair['isQuasiflag'] == 0  and $pair['restartResyncCounter'] == 0)
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['rpo'] = $pair['currRpoAtIR'];
			}
			else
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['rpo'] = $pair['currRpo'];
			}
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['lastRpoCalculatedTime'] = $pair['statusUpdateTime'];
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['stats'] = $pair['stats'];
            $dev_properties = $volume_details[$server_id][$src_device]['deviceProperties'];
            $display_name = "";
            if (isset($dev_properties) && $dev_properties!="")
            {
                $dev_properties = unserialize($dev_properties);
                $display_name = $dev_properties['display_name'];
            }
            $protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['Display_Name'] = $display_name ;
             
			
			// Set server level replication state
			if((!in_array('protection_stage',$protection_details[$plan_id]['servers'][$server_id])) || ($protection_details[$plan_id]['servers'][$server_id]['protection_stage'] > $state))
			{
				$protection_details[$plan_id]['servers'][$server_id]['protection_stage'] = $state;
			}	
			
			// Set server level resync progress
			//Step2 and Diffsync
			if($state == '2' || $state == '1')
			{
				$protection_details[$plan_id]['servers'][$server_id]['total_resync_offset'][] = $volume_details[$server_id][$src_device]['capacity'];
				
			}
			else //Queued and InProgress 
			{
				$protection_details[$plan_id]['servers'][$server_id]['total_resync_offset'][] = $disk_resync_progress;
			}
			
			$protection_details[$plan_id]['servers'][$server_id]['total_capacity'][]= $volume_details[$server_id][$src_device]['capacity'];
			
			// Set server level resync duration
			if((!$protection_details[$plan_id]['servers'][$server_id]['resync_duration']) || ($protection_details[$plan_id]['servers'][$server_id]['resync_duration'] > $pair['resyncDuration']))
			$protection_details[$plan_id]['servers'][$server_id]['resync_duration'] = $pair['resyncDuration'];
			
			// Set server level recoverability time
            if ($target_data_palne != 2)
            {
			if((!$protection_details[$plan_id]['servers'][$server_id]['logsFrom']) || ($protection_details[$plan_id]['servers'][$server_id]['logsFrom'] < $pair['logsFrom']))
			$protection_details[$plan_id]['servers'][$server_id]['logsFrom'] = ($pair['logsFrom'] < 0) ? '0' : $pair['logsFrom'];
			if((!$protection_details[$plan_id]['servers'][$server_id]['logsTo']) || ($protection_details[$plan_id]['servers'][$server_id]['logsTo'] > $pair['logsTo']))
			$protection_details[$plan_id]['servers'][$server_id]['logsTo'] = ($pair['logsTo'] < 0) ? '0' : $pair['logsTo'];
		    }
			// Set VolumeResize flag
			$is_volume_resized = 'No';
			if(($this->is_volume_resize_done($server_id,$src_device,$pair['pairId'])) === TRUE) 
			{
				$is_volume_resized = 'Yes';
			}
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['volume_resized'] = $is_volume_resized;
			
			// Set resync required only if there is no volume resize
			$resync_required = "No";
			
			/// Consider only if state is in diff sync
			if ($state == 2 && ($pair['isResync'] == 1 || $pair['shouldResync']))	$resync_required = "Yes";
					
			$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['pair_details']['resync_required'] = $resync_required;
			
            // Set health factor for RPO as red
            if(($pair['rpoSLAThreshold']*60) < $pair['currRpo'] && $pair['isResync'] != 1)
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['RPO'] = "^ECH0007##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['RPO'] .= "^ECH0007##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['health']['RPO'] .= "^ECH0007##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
			}
			
			// Set health factor for Resync Required as red only if there is no volume resize
            if($pair['shouldResync'] && $state == 2)
			{
				$resync_hf_code = $RESYNC_HEALTH_CODE_FOR_BACKWARD_AGENT;
				$resync_detection_time = $DEFAULT_HF_DETECTION_TIME;
				$place_holders = "";
				if ((count($resync_codes_available) > 0) && ($resync_codes_available[$server_id][$src_device]))
				{
					$resync_hf_code = $resync_codes_available[$server_id][$src_device]['resyncErrorCode'];
					$resync_detection_time = $resync_codes_available[$server_id][$src_device]['detectedTime'];
					$place_holders = $resync_codes_available[$server_id][$src_device]['placeHolders'];
				}
				if ($place_holders)
				{
					$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['DataConsistency'] = "^$resync_hf_code##$resync_detection_time##$disk_health_factor##$place_holders";
					$protection_details[$plan_id]['servers'][$server_id]['health']['DataConsistency'] .= "^$resync_hf_code##$resync_detection_time##$disk_health_factor##$place_holders";
					$protection_details[$plan_id]['health']['DataConsistency'] .= "^$resync_hf_code##$resync_detection_time##$disk_health_factor##$place_holders";
				}
				else
				{
					$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['DataConsistency'] = "^$resync_hf_code##$resync_detection_time##$disk_health_factor##";
					$protection_details[$plan_id]['servers'][$server_id]['health']['DataConsistency'] .= "^$resync_hf_code##$resync_detection_time##$disk_health_factor##";
					$protection_details[$plan_id]['health']['DataConsistency'] .= "^$resync_hf_code##$resync_detection_time##$disk_health_factor##";
				}
			}
			
            // Set health factor for Retention as red
            if ($target_data_palne != 2)
            {
			 if(($state == 2) && !$pair['currLogsize'] && (empty($pair['logsFrom']) or empty($pair['logsTo'])))
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Retention'] = "^ECH0008##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['Retention'] .= "^ECH0008##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['health']['Retention'] .= "^ECH0008##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
			}
		    }
            /* Set health factor for Protection as red
            *   1. Data Flow Controlled
            *   2. Data Upload Blocked
            *   3. Pause Pairs
            *   4. Target Cache Volume less than 256 MB
            *   5. No Communication from Source/Target
			*	6. If the On-Premises VM is deleted
            *   7. ECH00018 - Resync Throttle.
            *   8. ECH00019 - Differential Throttle.
            *   9. ECH00020 - Cummulative Throttle.
            */
            if($pair['isCommunicationfromSource'])
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Protection'] .= "^ECH0004##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['Protection'] .= "^ECH0004##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['health']['Protection'] .= "^ECH0004:$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
			}
            if($pair['replication_state'] == 26)
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Protection'] .= "^ECH0002##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['Protection'] .= "^ECH0002##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['health']['Protection'] .= "^ECH0002:$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
			}
			if($pair['cacheVolume'] && $pair['fileSystemType'] && ($pair['freeSpace'] < (256*1024*1024)))
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Protection'] .= "^ECH0006##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['Protection'] .= "^ECH0006##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['health']['Protection'] .= "^ECH0006:$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
			}
			if($deleted_and_protected_vm_details[$server_id])
			{
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Protection'] .= "^ECH00011##$DEFAULT_HF_DETECTION_TIME##$vm_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['Protection'] .= "^ECH00011##$DEFAULT_HF_DETECTION_TIME##$vm_health_factor##";
				$protection_details[$plan_id]['health']['Protection'] .= "^ECH00011:$DEFAULT_HF_DETECTION_TIME##$vm_health_factor##";
			}
            if($pair['throttleresync'])
			{            
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Protection'] .= "^ECH00018##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['Protection'] .= "^ECH00018##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['health']['Protection'] .= "^ECH00018:$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
			}
            if($pair['throttleDifferentials'] && ($pair['isQuasiflag'] != 0))
			{            
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Protection'] .= "^ECH00019##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['Protection'] .= "^ECH00019##$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
				$protection_details[$plan_id]['health']['Protection'] .= "^ECH00019:$DEFAULT_HF_DETECTION_TIME##$disk_health_factor##";
			}
            if($pair['cummulativeThrottle'])
			{            
				$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Protection'] .= "^ECH00020##$DEFAULT_HF_DETECTION_TIME##$vm_health_factor##";
				$protection_details[$plan_id]['servers'][$server_id]['health']['Protection'] .= "^ECH00020##$DEFAULT_HF_DETECTION_TIME##$vm_health_factor##";
				$protection_details[$plan_id]['health']['Protection'] .= "^ECH00020:$DEFAULT_HF_DETECTION_TIME##$vm_health_factor##";
			}
			
			if (array_key_exists($pair['pairId'],$protection_health_factors))
			{
				$hf_data = $protection_health_factors[$pair['pairId']];
				foreach ($hf_data as $hf_code=>$hf_description)
				{
					$hf_detection_time = $hf_description["detectiontime"];	
					$healthfactortype = $hf_description["healthfactortype"];	
					$healthPlaceHolders = $hf_description["healthPlaceHolders"];	
					$protection_details[$plan_id]['servers'][$server_id]['volumes'][$src_device]['health']['Protection'] .= "^".$hf_code."##$hf_detection_time##$healthfactortype##$healthPlaceHolders";
					$protection_details[$plan_id]['servers'][$server_id]['health']['Protection'] .= "^".$hf_code."##$hf_detection_time##$healthfactortype##$healthPlaceHolders";
					$protection_details[$plan_id]['health']['Protection'] .= ",".$hf_code.":$hf_detection_time:$healthfactortype:$healthPlaceHolders";
				}
			}
			
			
            // Volumes protected under the plan
            $protection_details[$plan_id]['volumes_protected'] += 1;                    
        }
        
		return $protection_details;
    }
	
	public function get_health_message()
	{
		$health_message = array(
								'Protection'	=> 
								array( 'ECH0001' => array( 'Impact' => 'Critical',
																				'Description' => 'No Communication from Source/Target' ),
									'ECH0002' => array( 'Impact' => 'Critical',
																				'Description' => 'Pause Pairs' ),
									'ECH0004' => array( 'Impact' => 'Critical',
																				'Description' => 'Data Upload Blocked' ),																				
																  'ECH0005' => array( 'Impact' => 'Critical',
																				'Description' => 'Data Flow Controlled' ),
																  'ECH0006' => array( 'Impact' => 'Critical',
																				'Description' => 'Target Cache Volume freespace less than 256 MB' ),
																  'ECH00011' => array( 'Impact' => 'Critical',
																				'Description' => 'VM Deleted'),
																  'ECH00012' => array( 'Impact' => 'Critical',
																				'Description' => 'Storage account deleted or inaccessible.'),
																  'ECH00013' => array( 'Impact' => 'Critical',
																				'Description' => 'Replication blocked because base blob deleted.'),
																   'ECH00015' => array( 'Impact' => 'Warning',
																				'Description' => 'Protection progress stuck'),
																   'ECH00014' => array( 'Impact' => 'Critical',
																				'Description' => 'Protection progress stuck')
																),
										'DataConsistency' 	=> array( 'ECH0003' => array( 'Impact' => 'Critical',
																				'Description' => 'Resync required yes' )
																),						
										'RPO' 			=> array( 'ECH0007' => 
																		array( 'Impact' => 'Critical',
																			   'Description' => 'RPO threshold exceeded' )
																),
										'Retention' 	=> array( 'ECH0008' => array( 'Impact' => 'Warning',
																				'Description' => 'No retention available' )
																),										
										'CommonConsistency' => array( 'ECH0009' => array('Impact' => 'Critical',
																					'Description' => 'Common consistency not available.')
																	),
										'VolumeResize' => array( 'ECH00010' => array('Impact' => 'Critical',
																					'Description' => 'Source Volume capacity got resized')
																	)
								);
		return 	$health_message;					
	}
	
    function get_server_details($server_ids)
    {
        $server_id_array = array();
        if(!is_array($server_ids)) $server_id_array[] = $server_ids;
        else $server_id_array = $server_ids;
        
        $servers_host_id_str = implode(",", $server_id_array);
        if(!$servers_host_id_str) return;
        
        $server_info_sql = "SELECT
                                h.id,
                                h.name,
                                h.ipAddress,
                                h.osFlag,
                                h.osCaption,
								h.lastHostUpdateTime,
								UNIX_TIMESTAMP(h.lastHostUpdateTimeApp) as lastUpdateTimeApp, 
                                h.resourceId,
								UNIX_TIMESTAMP(now()) as currentTime,
								UNIX_TIMESTAMP(creationTime) as creationTime 
                            FROM
                                hosts h 
							WHERE
								FIND_IN_SET(h.id, ?)";
        $server_info = $this->conn->sql_query($server_info_sql, array($servers_host_id_str));
        $server_host_details = array();
        foreach($server_info as $detail)
        {
            $server_host_details[$detail['id']] = $detail;
        }
        
        return $server_host_details;
    }
	
	function get_pair_volume_details()
	{
		$sql_args = array();
		$volumes_sql = "SELECT
							hostId,
							deviceName,
							capacity,
							freeSpace,
                             deviceProperties 
						FROM
							logicalVolumes
						WHERE
							volumeType != 2";
		$volume_info = $this->conn->sql_query($volumes_sql, $sql_args);
		if(!is_array($volume_info)) return;
		$volume_details = array();
		foreach($volume_info as $volume)
		{
			$volume_details[$volume['hostId']][$volume['deviceName']] = $volume;
		}
		
		return $volume_details;
	}
	
	function validateProtectedHosts($host_ids)
    {
        $sql_cond = "";
        $sql_args = array();
        $host_ids_array = array();;
        if(!is_array($host_ids)) $host_ids_array[] = $host_ids;
        else $host_ids_array = $host_ids;
        
        $host_ids_str = implode(",", $host_ids_array);
        $sql_cond = " AND FIND_IN_SET(sourceHostId, ?)";
        $sql_args[] = $host_ids_str;
        
        $protection_check_sql = "SELECT
                                DISTINCT
                                    sourceHostId
                                FROM
                                    srcLogicalVolumeDestLogicalVolume
                                WHERE
                                    destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A'".
                                $sql_cond;
        $array_servers = $this->conn->sql_get_array($protection_check_sql, "sourceHostId", "sourceHostId", $sql_args);
        $protected_servers = array_keys($array_servers);
        
        $not_protected_hosts = array();
        foreach($host_ids_array as $server)
        {
            if(!in_array($server, $protected_servers)) $not_protected_hosts[] = $server;
        }
        $not_protected_hosts_str = implode(",", $not_protected_hosts);
        
        return $not_protected_hosts_str;
    }
	
	public function getProfilerDetails($plan_id=NULL, $src_host_ids=NULL)
    {
		$sql_cond = "";
		$sql_args = array();
		
		if($plan_id)
		{
			$sql_cond = " AND s.planId = ?";
			$sql_args[] = $plan_id;
		}
		
		if($src_host_ids)
		{
			$host_ids_str = implode(",", $src_host_ids);
			$sql_cond = " AND FIND_IN_SET(s.sourceHostId, ?)";
			$sql_args[] = $host_ids_str;
		}
		
		$profiler_sql = "SELECT
                               avg(p.cumulativeCompression) as cumulativeCompression,
							   avg(p.cumulativeUnCompression) as cumulativeUnCompression,
							   s.sourceHostId,
							   s.sourceDeviceName
                           FROM 
                                srcLogicalVolumeDestLogicalVolume s,
                                profilingDetails p
                           WHERE
                                s.destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' AND
                                s.profilingId = p.profilingId $sql_cond
							GROUP BY sourceDeviceName, sourceHostId";
		$profiling_details = $this->conn->sql_query($profiler_sql, $sql_args);
        if(!count($profiling_details)) return;
		$final_output = array();
		foreach($profiling_details as $profiler_info)
		{
			$src_id = $profiler_info["sourceHostId"];	
			$src_device_name = $profiler_info["sourceDeviceName"];	
			$compression = $profiler_info["cumulativeCompression"];	
			$un_compression = $profiler_info["cumulativeUnCompression"];
			
			$final_output[$src_id]['compression'] +=  $compression;
			$final_output[$src_id]['unCompression'] +=  $un_compression;
			$final_output[$src_id][$src_device_name]['compression'] +=  $compression;
			$final_output[$src_id][$src_device_name]['unCompression'] +=  $un_compression;
		}
		return $final_output;
	}
    
    function check_disk_signature_validation($hostId, $disks_arr, $disks_str, $protection_direction)
    {
        $number_of_disks = count($disks_arr);
        
        $disk_properties = $this->conn->sql_query("SELECT deviceProperties, deviceName FROM logicalVolumes WHERE hostId = ? AND volumeType=?", array($hostId, 0)); 
       
        $disk_match_count = 0;
        foreach($disk_properties as $disk_info) 
		{
           $disk_properties = unserialize($disk_info['deviceProperties']);
           $disk_name = $disk_info['deviceName'];  
           
           if ($protection_direction ==  "A2E")
           {
                if (in_array($disk_name, $disks_arr))
                {
                    $disk_ref = "SIGNATURE";
                    $disk_match_count++;
                }
           }
           else
           {
                if (in_array($disk_properties['display_name'],$disks_arr))
                {
                    $disk_ref = "NAME";
                    $disk_match_count++;
                }
           }
        }
        if ($disk_match_count == $number_of_disks)
        {
            $validation = TRUE;
        }
        else
        {
            $validation = FALSE;
        }
        return array("disk_ref"=>$disk_ref, "validation"=>$validation);
    }
    
	/*
	* This function is used check whether the disk is having UEFI partition or if the disk type is dynamic disk
	*/
	public function check_disk_eligibility($hostId, $disks_str)
	{	
		$device_properties = $uefi_disk_arr = $dynamic_disk_arr = array();
		$disk_properties = $this->conn->sql_query("SELECT deviceProperties, deviceName FROM logicalVolumes WHERE hostId = ? AND FIND_IN_SET(deviceName, ?)", array($hostId, $disks_str));
	
		foreach($disk_properties as $disk_info) 
		{
			$has_uefi = $isDynamicDisk = 0;
			$device_properties = unserialize($disk_info['deviceProperties']);			
			if(array_key_exists('has_uefi',$device_properties)) $has_uefi = 1;		
			if($device_properties['storage_type'] == "dynamic") $isDynamicDisk = 1; 
			if($has_uefi)
			{
				$uefi_disk_arr[] = $disk_info['deviceName'];
			}
			else if($isDynamicDisk)
			{
				$dynamic_disk_arr[] = $disk_info['deviceName'];
			}
		}
		return array($uefi_disk_arr, $dynamic_disk_arr);
	}
	
    /*
        This function is used to check whether the boot dynamic disk exists or not. If exists, then throw the boolean flag with TRUE.
	*/
    public function is_dynamic_boot_disk_exists($hostId, $disks_str) //hostId-->Host identity guid. disks_str-->ASR input disks
	{
		$dynamic_boot_disk_status = FALSE;
					
        $result_set = $this->conn->sql_query("select 
                                                    dg.diskGroupName 
                                                from 
                                                    diskGroups dg, 
                                                    logicalVolumes lv 
                                                where 
                                                    dg.hostId=lv.hostId 
                                                and  
                                                    lv.hostId = ? 
                                                and 
                                                    dg.deviceName=lv.deviceName 
                                                and 
                                                    lv.volumeType=5 
                                                and 
                                                    lv.SystemVolume=1", array($hostId));
        
  /* 
Sample output for the above query
  Array
(
    [0] => Array
        (
            [diskGroupName] => __INMAGE__DYNAMIC__DG__
        )

)*/
        $result_set = array_unique($result_set);
		foreach($result_set as $disk_group_name_array) 
		{
            $disk_group_name = $disk_group_name_array['diskGroupName'];
            $properties_result_set = $this->conn->sql_query("select 
                                                                dg.deviceName, 
                                                                lv.deviceProperties 
                                                            from 
                                                                diskGroups dg, 
                                                                logicalVolumes lv 
                                                            where 
                                                                dg.hostId=lv.hostId 
                                                            and 
                                                                lv.hostId= ? 
                                                            and 
                                                                dg.diskGroupName=? 
                                                            and 
                                                                dg.deviceName=lv.deviceName 
                                                            and 
                                                                lv.volumeType=0", array($hostId,$disk_group_name));
                                                                
             /*
             Sample output for the above query
             Array
(
    [0] => Array
        (
            [deviceName] => {3459418242}
            [deviceProperties] => a:15:{s:12:"display_name";s:18:"\\.\PHYSICALDRIVE0";s:22:"has_bootable_partition";s:4:"true";s:14:"interface_type";s:3:"ide";s:12:"manufacturer";s:22:"(Standard disk drives)";s:10:"media_type";s:21:"Fixed hard disk media";s:5:"model";s:32:"WDC WD5000AAKS-75V0A0 ATA Device";s:8:"scsi_bus";s:1:"0";s:17:"scsi_logical_unit";s:1:"0";s:9:"scsi_port";s:1:"0";s:14:"scsi_target_id";s:1:"0";s:17:"sectors_per_track";s:2:"63";s:12:"storage_type";s:7:"dynamic";s:15:"total_cylinders";s:5:"60801";s:11:"total_heads";s:3:"255";s:13:"total_sectors";s:9:"976768065";}
        )

)*/
            foreach ($properties_result_set as $key=>$value)
            { 
                $device_properties = unserialize($value['deviceProperties']);
                $disk_name = $value['deviceName'];            
                if (($device_properties['storage_type'] == "dynamic") && (strtolower($device_properties['has_bootable_partition']) == "true"))
                {
                    //If such dynamic bootable disk exists in ASR sent disks input, then mark flag as TRUE and break.
                    if (in_array($disk_name, $disks_str))
                    {
                        $dynamic_boot_disk_status = TRUE;
                        break;
                    }                    					
                }
            }
            if ($dynamic_boot_disk_status == TRUE)  
            break;
		}		
		return $dynamic_boot_disk_status;
	}
    
	public function pausePairs($pair_id, $rule_id = '')
	{
		global $PAUSED_PENDING, $PAUSE_BY_USER;
		
		if(is_array($pair_id)) $pair_id = implode("','",$pair_id);
		
		// Pause all the pairs of server for which rollback is initiated.
		$pair_sql = "UPDATE
							srcLogicalVolumeDestLogicalVolume
						SET
							replication_status = $PAUSED_PENDING,
							tar_replication_status = 1,	
							pauseActivity = '$PAUSE_BY_USER',
							autoResume    = 0,
							restartPause = 0 
						WHERE
							pairId IN ('$pair_id') 
						AND
							(tar_replication_status = '0' OR (tar_replication_status = '1' AND restartPause = '1'))";
		$result = $this->conn->sql_query($pair_sql);
		
		// Pause retention only in case if call is from Rollback API.
		if(is_array($rule_id) && $result)
		{
			$this->updateRetentionPolicy($rule_id, 1);
		}
	}
	
	private function updateRetentionPolicy($rule_id, $pause_retention = 0)
	{
		global $logging_obj;
		$ret_id = $ret_event_id = array();
		
		if($pause_retention == 1) $policy_type = 1;
		else $policy_type = 2;
		
		if(is_array($rule_id)) $rule_id = implode("','",$rule_id);
		
		$ret_sql = "SELECT 
						event.Id as Id, 
						log.retId as retId
					FROM
						retLogPolicy log, 
						retentionWindow window, 
						retentionEventPolicy event 
					WHERE
						log.retId = window.retId AND
						window.Id = event.Id AND
						log.ruleId in ('$rule_id')";
		$retention_details = $this->conn->sql_query($ret_sql);
		while ($row = $this->conn->sql_fetch_object($retention_details))
		{
			$ret_id[] = $row->retId;
			$ret_event_id[] = $row->Id;
        }
		
		$ret_id = implode("','",$ret_id);
		$ret_event_id = implode("','",$ret_event_id);
		
		$this->conn->sql_query("UPDATE retLogPolicy SET retPolicyType = 1 WHERE ruleId in ('$rule_id')");
		$this->conn->sql_query("UPDATE retentionEventPolicy  SET storagePruningPolicy = 1 WHERE Id in ('$ret_event_id')");
		$this->conn->sql_query("UPDATE retLogHistory SET type_of_policy = '1', logPruningPolicy = '1' WHERE retId in ('$ret_id')");
	}
	
  
    function getDiskGroups($all_source_host_ids)
    {
        $disk_group_array = array();
        if (is_array($all_source_host_ids))
        {
            $source_host_ids_str = implode(',', $all_source_host_ids);
            $disk_group_result_set = $this->conn->sql_query("SELECT hostId, deviceName, diskGroupName FROM diskGroups WHERE FIND_IN_SET(hostId, ?)", array($source_host_ids_str));
            foreach($disk_group_result_set as $disk_group_data) 
            {
                $disk_group_array[$disk_group_data['hostId']][$disk_group_data['deviceName']] = $disk_group_data['diskGroupName'];
            }
        }
        return $disk_group_array;
    }
	
	//It checks, if the given host id is having multi path enabled or not. This function usage applicable for only Linux OS. 
	function isMultiPathTrue($host_id)
	{
		$is_multi_path = FALSE;
		$multi_path_result_set = $this->conn->sql_query("SELECT isMultipath FROM logicalVolumes WHERE hostId = ? and isMultipath =  ? limit 1", array($host_id,1));
		foreach($multi_path_result_set as $multi_path_data) 
		{
			$is_multi_path = TRUE;
		}
		return $is_multi_path;
	}
	
	//Function to check, all the VMs protected for fail back should go to same MT.
	function isMasterTargetDifferent($plan_id, $mt_id)
	{
		global $CLOUD_PLAN_VMWARE_SOLUTION;
		$status_flag = FALSE;
		$cloud_plan_solution  = ucfirst(strtolower($CLOUD_PLAN_VMWARE_SOLUTION));
		$query = "select scenarioDetails from applicationScenario where applicationType = ? and planId = ? limit 1";
		$result_set = $this->conn->sql_query ($query, array($cloud_plan_solution, $plan_id));
		
		foreach($result_set as $data) 
		{
			$scenario_data = $data['scenarioDetails'];
			$scenario_data = unserialize($scenario_data);
			$multi_vm_sync = $scenario_data['globalOption']['vacp'];
			if ($multi_vm_sync == 'On')
			{
				$query = "select planId from applicationScenario where applicationType = ? and targetId != ? and planId = ? limit 1";
				$result_set = $this->conn->sql_query ($query,array($cloud_plan_solution,$mt_id,$plan_id));
				if (count($result_set) > 0)
				{
					$status_flag = TRUE;
				}
			}
		}
		return $status_flag;
    }
	
	# Function is set resync for the required pair.
	# $updateAgentLog is update the Agent logging. if 1 then it will update the logging.
	public function setResyncRequiredToPair($destHostId, $destDev, $isResyncRequired ,$origin,$err, $updateAgentLog = 1,$resync_reason_details='', $component = '')
	{
		/* Sample data target sends to CX:
		vxstub.log:13:2015-12-12 11:55:50 (UTC) CX :INFO:setResyncRequiredToPair:: destHo
		stId: 736808ED-70DB-2840-96ACEA5C16B915A8, destDev: 36000c295048ba9c1a5af1cc33e0
		cc09a, isResyncRequired: 1, err: Few files are missing for 36000c295048ba9c1a5af
		1cc33e0cc09a
		vxstub.log:20:2015-12-12 11:55:50 (UTC) CX :INFO:setResyncRequiredToPair:: update
		_should_resync: UPDATE srcLogicalVolumeDestLogicalVolume SET shouldResync = 1 ,
		ShouldResyncSetFrom = 1 , resyncSetCxtimestamp=1449921350  WHERE destinationHost
		Id='736808ED-70DB-2840-96ACEA5C16B915A8' AND  destinationDeviceName='36000c29504
		8ba9c1a5af1cc33e0cc09a' AND destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C
		79236A' AND shouldResync != 3
		vxstub.log:23:2015-12-12 11:55:53 (UTC) CX :INFO:setResyncRequiredToPair:: update
		d agent log
		*/

		global $logging_obj, $RESYNC_REQD, $FATAL, $MASTER_TARGET, $ZERO, $TARGET_DATA;
		$result = TRUE;
		$logging_obj->my_error_handler("INFO",array("DestinationHostId"=>$destHostId, "DestinationDeviceName"=>$destDev, "IsResyncRequired"=> $isResyncRequired, "Data"=>$err, "Origin"=>$origin, "Reasoncode_details"=>print_r($resync_reason_details,TRUE)), Log::BOTH);
		
		$valid_protection = $this->commonfunctions_obj->isValidProtection($destHostId, $destDev, $TARGET_DATA);
		if (!$valid_protection)
		{
			$logging_obj->my_error_handler("INFO","Invalid post data $destDev hence returning from setResyncRequiredToPair.", Log::BOTH);
			//In case the protection configuration cleaned from CS database because of disable protection, hence, by considering such race condition returning API success status as TRUE for agent to avoid re-try the same call again and again to CS.
			return $result;
		}
		
		if($updateAgentLog == 1)
		{
			$logging_obj->my_error_handler("INFO",array("DestinationHostId"=>$destHostId, "DestinationDeviceName"=>$destDev, "IsResyncRequired"=> $isResyncRequired, "Data"=>$err, "Origin"=>$origin, "Reasoncode_details"=>print_r($resync_reason_details,TRUE)), Log::BOTH);
				
			$dest_host_name = $this->commonfunctions_obj->getHostName($destHostId);
			$dest_ip_address = $this->commonfunctions_obj->getHostIpaddress($destHostId);
				
			$err = $err."<br> Destination Host : $dest_host_name <br>";
			$err = $err."Destination IP : $dest_ip_address";
			$this->commonfunctions_obj->update_agent_log($destHostId, $destDev,$err,0,$origin);	
		}
		
		$optional = "";
		$destDevNoEscape = $destDev;
		$destDev = $this->conn->sql_escape_string ($destDev);
		$isResyncRequired = ($isResyncRequired ? '1' : '0');
		/*
			fix #4053	  	4.1Beta1.1-AutoResync is not working
			in 4.0 we have to update  with resyncSetCxtimestamp then only auto resync will trigger 
			In 3.5.2 resyncSettimestamp field is using and time stamp values is sending by agent
			Problem Description: This issue is due to the wrong update. Instead  resyncSetCxtimestamp, we are updating  resyncSettimestamp filed
		*/
		$timestamp = time ();
		$sql = "SELECT
						resyncSetCxtimestamp, 
						autoResyncStartTime,
						isQuasiFlag,
						sourceHostId,
						sourceDeviceName,
						deleted,
						replication_status, 
						jobId, 
						restartPause,
						src_replication_status,
						tar_replication_status 
					FROM
						srcLogicalVolumeDestLogicalVolume
					WHERE
						destinationHostId='$destHostId' 
					AND
						destinationDeviceName='$destDev'";
					
		$auto_details = $this->conn->sql_query($sql);
		$auto_details_obj = $this->conn->sql_fetch_object($auto_details);
		$resyncSetCxtimestamp = $auto_details_obj->resyncSetCxtimestamp;
		$autoResyncStartTime = $auto_details_obj->autoResyncStartTime;
		$source_id = $auto_details_obj->sourceHostId; 
		$source_device_name = $auto_details_obj->sourceDeviceName; 		
		$pair_resync_status = $auto_details_obj->isQuasiFlag;
		$deleted = $auto_details_obj->deleted;
		$replication_status = $auto_details_obj->replication_status;
		$job_id = $auto_details_obj->jobId;
		$restartPause = $auto_details_obj->restartPause;
		$src_replication_status = $auto_details_obj->src_replication_status;
		$tar_replication_status = $auto_details_obj->tar_replication_status;
		$source_device_escaped = $this->conn->sql_escape_string ($source_device_name);
		
		if ($resync_reason_details)
		{
			$resync_reason_code = $resync_reason_details['resyncReasonCode'];
			$resync_reason_code_detected_time = $resync_reason_details['detectionTime'];
			$host_info = $this->commonfunctions_obj->get_host_info($source_id);
			if ($host_info)
			{
				$placeholders = array();
				$placeholders['SourceHostName'] = $host_info['name'];
				$placeholders['SourceIpAddress'] = $host_info['ipaddress'];
				$placeholders['DiskId'] = $source_device_name;
				$placeholders['detectionTime'] = $resync_reason_code_detected_time;
				$disk_name = $this->commonfunctions_obj->get_disk_name($source_id, $source_device_escaped, $host_info['osFlag']);
				$placeholders['DiskName'] = $disk_name;
				$placeholders_data = (count($placeholders)) ? serialize($placeholders) : '';
			}
		}

		$optional = ", ShouldResyncSetFrom = 1 ";
		if(($autoResyncStartTime == 0) || ($resyncSetCxtimestamp == 0))
		{
			$optional = $optional.", resyncSetCxtimestamp=$timestamp ";
		}
		$restartResyncFlag = 3;
		$update_should_resync = "UPDATE srcLogicalVolumeDestLogicalVolume " .
			"SET shouldResync = $isResyncRequired $optional WHERE destinationHostId=" .
			"'$destHostId' AND  destinationDeviceName='$destDev' AND destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' AND shouldResync != $restartResyncFlag";
		$is_protection_rollback_progress = $this->commonfunctions_obj->is_protection_rollback_in_progress($source_id);
		if ( ($pair_resync_status == 2 && $deleted == 0) 
		|| 
		($restartPause == 0 && ($replication_status == 41 || $replication_status == 26) && ($deleted == 0) && $pair_resync_status != 2) 
		|| 
		($is_protection_rollback_progress == 1 && $deleted == 0)
		)
		{
			$iter =  $this->conn->sql_query( $update_should_resync);
			//Raise event and add the resync code to resync code history table, if disk is in diff sync.
			if ($resync_reason_details)
			{
				$this->resync_error_code_action($source_id,$source_device_name,$destHostId, $destDevNoEscape, $job_id, $MASTER_TARGET, $resync_reason_code,$resync_reason_code_detected_time);
				$this->commonfunctions_obj->updateAgentAlertsDbV2($RESYNC_REQD, $source_id, $resync_reason_code.$source_device_name, $resync_reason_code, $resync_reason_code, $placeholders_data, $FATAL, $ZERO, $component);
			}
			$logging_obj->my_error_handler("INFO",array("Sql"=> $update_should_resync), Log::BOTH);
		}
		//If jobid = 0 or PS/Target cleanup pending is in progress or pair marked for deletion or restart pause marked, ignore re-start re-sync.
		//Re-start pause finally goes to re-start re-sync
		//Pair marked for deletion, not required to send to pause state.
		//PS/Target cleanup pending or jobid is 0 means already initiated for re-start re-sync. Ignore if we get re-sync required marker again in this state.
		//Ignore re-sync required marker, when protection rollback is in progress.
		elseif (($job_id == 0) || ($replication_status == 42) ||  ($deleted == 1) || ($restartPause == 1))
		{
			$do_pause = FALSE;
		}
		else
		{
			$do_pause = TRUE;
		}
		$logging_obj->my_error_handler("INFO",array("SourceHostId"=>$source_id, "SourceDeviceName"=>$source_device_name, "DestinationHostId"=> $destHostId, "DestinationDeviceName"=>$destDev, "DoPause"=>$do_pause, "JobId"=> $job_id, "ReplicationStatus"=>$replication_status, "Deleted"=>  $deleted, "RestartPause"=>$restartPause, "PairResyncStatus"=>$pair_resync_status, "SrcReplicationStatus"=>$src_replication_status, "TarReplicationStatus" =>$tar_replication_status, "RollbackState"=>$is_protection_rollback_progress), Log::BOTH);
		if ($do_pause)
		{
			$result = $this->pause_replication($source_id, $source_device_name, $destHostId, $destDevNoEscape,2,0,"resync required");
			//If disk is in IR/Resync and means not in diff sync, then raise only event
			if ($resync_reason_details)
			{
				$this->commonfunctions_obj->updateAgentAlertsDbV2($RESYNC_REQD, $source_id, $resync_reason_code.$source_device_name, $resync_reason_code, $resync_reason_code, $placeholders_data, $FATAL, $ZERO, $component);
			}
		}
		return $result;
	}
	
	#Method to insert the resync reason code details in resyncErrorCodesHistory table by the reported component(PS/Source/Mt)
	public function resync_error_code_action($source_host_id,$source_device_name,$destination_host_id, $destination_device_name, $jobid, $reported_component, $resync_reason_code, $resync_reason_code_detected_time, $place_holders='')
	{
		global $ZERO, $ONE;
		$place_holders = (trim($place_holders)) ? trim($place_holders) : '';
		
		$sql = "SELECT
					Id,
					counter
				FROM 
					resyncErrorCodesHistory
				WHERE
					sourceHostId=? AND sourceDeviceName=? AND resyncErrorCode=? AND reprotedComponent=?";
		$sql_params = array($source_host_id, $source_device_name, $resync_reason_code, $reported_component);
		$resultset = $this->conn->sql_query ($sql, $sql_params);
		
		if (! empty($resultset))
		{
			$data = $resultset[0];
			$count = $data['counter'] + 1;
			$sql = "UPDATE 
						resyncErrorCodesHistory
					SET
						counter = ?,
						updatedTime = now(),
						placeHolders = ? 		
					WHERE
						Id = ?";
			$args = array($count, $place_holders, $data['Id']);
		}
		else
		{
			$sql = "INSERT 
					INTO 
						resyncErrorCodesHistory(
						sourceHostId,
						sourceDeviceName,
						destinationHostId,
						destinationDeviceName,
						jobId,
						resyncErrorCode,
						detectedTime,
						isActionRequired,
						firstReportedTime,
                        updatedTime,
                        counter,
						reprotedComponent,
						placeHolders
						)
					VALUES (
						?,
						?,
						?,
						?,
						?,
						?,
						?,
						?,
						now(),
						now(),
						?,
                        ?,
						?
						)";
			$args = array($source_host_id, $source_device_name, $destination_host_id, $destination_device_name, $jobid, $resync_reason_code, $resync_reason_code_detected_time, $ZERO, $ONE, $reported_component, $place_holders );
		}
		$rs = $this->conn->sql_query ($sql, $args);
	}
};
?>