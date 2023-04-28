<?php
/*
 *$Header: /src/admin/web/app/Retention.php,v 1.93.16.2 2017/08/16 15:57:42 srpatnan Exp $
 *================================================================= 
 * FILENAME
 *    Retention.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    Retention logs
 *
 * HISTORY
 *     *     20-Feb-2008  Varun    Created by pulling functions from 
 *                 functions.php and conn.php
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/ 
    
Class Retention
{
    private $commonfunctions_obj;
    private $conn;

    /*
    * Function Name: Retention
    *
    * Description:
    *    This is the constructor of the class 
    *    performs the following actions.
    *
    *    <Give more details here>    
    *
    * Parameters:
    *     Param 1 [IN/OUT]:
    *     Param 2 [IN/OUT]:
    *
    * Return Value:
    *     Ret Value: <Return value, specify whether Array or Scalar>
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */    
    function __construct()
    {
		global $conn_obj;
        $this->commonfunctions_obj = new CommonFunctions();
        $this->conn = $conn_obj;
    }
    
    /*
    * Function Name: change_rep_history_status
    *
    * Description: 
    *    This function insert some values in database table 'replicationHistory'  
    *
    * Parameters:
    *     Param 1 [IN]:$vardb
    *     Param 2 [IN]:$src_guid
    *     Param 3 [IN]:$src_dev
    *     Param 4 [IN]:$dest_guid
    *     Param 5 [IN]:$dest_dev       
    *
    * Return Value:
    *     Ret Value: 
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function change_rep_history_status ($src_guid, 
                                    $src_dev, $dest_guid, $dest_dev)
    {
        global  $volume_obj;

        $sqlStmt = "SELECT
                        ruleId,
                        sourceHostId ".
                        "from srcLogicalVolumeDestLogicalVolume ".
                        "where destinationHostId = '$dest_guid' 
                    AND
                         destinationDeviceName = '$dest_dev'";
        
        $rs = $this->conn->sql_query($sqlStmt);
        if (!$rs) 
        {
            return;
        }

        if ($this->conn->sql_num_row($rs) == 0) 
        {
            return;
        }
        
        $volumeGroupId = $volume_obj->get_volumegroup_id( $dest_guid,
                                                          $dest_dev);
        if (!$volumeGroupId) 
        {
            return;
        }
        
        $escSrcDev = $this->conn->sql_escape_string( $src_dev );
        $escDstDev = $this->conn->sql_escape_string( $dest_dev );

        $row = $this->conn->sql_fetch_row($rs);
        $values = "($row[0], '$row[1]', '$escSrcDev',
                     '$dest_guid', '$escDstDev', now(), '$volumeGroupId')";
        while ($row = $this->conn->sql_fetch_row($rs)) 
        {
            $values = $values . ", ($row[0], '$row[1]', '$escSrcDev',
                                    '$dest_guid', '$escDstDev',
                                      now(), '$volumeGroupId')";
        }

        $sqlret = "INSERT into 
                       replicationHistory
                       (`ruleId`,
                       `sourceHostId`,
                       `sourceDeviceName`,
                       `destinationHostId`,
                       `destinationDeviceName`,
                       `createdDate`,
                       `volumeGroupId`)
                  VALUES 
                       $values";

        $this->conn->sql_query($sqlret);
    }

    /*
    * Function Name: setRetLogHistoryFlag
    *
    * Description: 
    *    This function update the field deleted of  the table retLogHistory  
    *
    * Parameters:
    *     Param 1 [IN]:$vardb
    *     Param 2 [IN]:$sourceId
    *     Param 3 [IN]:$sourceDeviceName
    *     Param 4 [IN]:$destId
    *     Param 5 [IN]:$destDeviceName
    *
    * Return Value:
    *     Ret Value: 
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function setRetLogHistoryFlag ($sourceId,
                                $sourceDeviceName, $destId, $destDeviceName)
    {
        global $volume_obj;

        $ruleIds = $volume_obj->get_all_ruleids_for_dest_as_array($destId, 
                                                                $destDeviceName
                                                                );
        $cnt = count($ruleIds);
        if (0 == $cnt) 
        {
            return;
        }

        $ruleIdsIn = "$ruleIds[0]";
        for ($i = 1; $i < $cnt; $i++) 
        {
            $ruleIdsIn = $ruleIdsIn . ", $ruleIds[$i]";
        }
        $updateQuery = "UPDATE
                            retLogHistory 
                        SET
                            deleted=1 
                        WHERE
                            deleted=0 
                        AND
                            repId in ($ruleIdsIn)";
                            
        $this->conn->sql_query($updateQuery);
    }
    
    /*
    * Function Name: insert_retLogPolicy
    *
    * Description: 
    *    This function insert the retention log  policy in database table  
    *
    * Parameters:
    *     Param 1  [IN]:$vardb
    *     Param 2  [IN]:$ruleids
    *     Param 3  [IN]:$type_log
    *     Param 4  [IN]:$log_policy_rad
    *     Param 5  [IN]:$conUnit
    *     Param 6  [IN]:$locmeta
    *     Param 7  [IN]:$locdir
    *     Param 8  [IN]:$drive1
    *     Param 9  [IN]:$drive2
    *     Param 10 [IN]:$timeRecFlag=0
    *     Param 11 [IN]:$hidTime=0
    *     Param 12 [IN]:$change_by_time_day=0
    *     Param 13 [IN]:$change_by_time_hr=0
    *     Param 14 [IN]:$txtlocdir_move
    *     Param 15 [IN]:$txtdiskthreshold=0
    *     Param 16 [IN]:$txtlogsizethreshold=0
    *     Param 17 [IN]:$logExceedPolicy
    *
    * Return Value:
    *     Ret Value: 
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function insert_retLogPolicy($ruleids, $policy_type, 
                                $log_policy_rad,$conUnit, $locmeta,
                                $locdir, $drive1, $drive2,
                                $timeRecFlag=0, $hidTime=0,
                                $change_by_time_day=0, $change_by_time_hr=0,
                                $txtlocdir_move, $txtdiskthreshold=0,
                                $txtlogsizethreshold=0, $logExceedPolicy,$sparse_enable=0)
    {
        /*need to maintain backward compatibility*/
        global $system_obj,$logging_obj;
		
		$unqid = '';	
        $log_policy_rad = 0;
        $typeOfPolicy  = $policy_type;
        if (0 == count($ruleids)) 
        {
            return;
        }
				
		$logging_obj->my_error_handler("DEBUG","rule_ids: ".print_r($ruleids,TRUE)."\n");	
		
		$logging_obj->my_error_handler("DEBUG","insert_retLogPolicy (Parameters) :  $policy_type, 
                                $log_policy_rad,$conUnit, $locmeta,
                                $locdir, $drive1, $drive2,
                                $timeRecFlag=0, $hidTime=0,
                                $change_by_time_day=0, $change_by_time_hr=0,
                                $txtlocdir_move, $txtdiskthreshold=0,
                                $txtlogsizethreshold=0, $logExceedPolicy,$sparse_enable=0");
        if($timeRecFlag=='')
        {
            $timeRecFlag = 0;
            $hidTime = 0;
        }    

        if($hidTime == '') 
        {
            $hidTime = 0;
        }

        if ($conUnit == '')
        {
            $conUnit = 0;
        }
        $change_by_time_day=0;
        $change_by_time_hr=0;
        if(!isset($txtdiskthreshold) || $txtdiskthreshold == '')
        {
            $txtdiskthreshold=0;
        }
        if(!isset($txtlogsizethreshold) || $txtdiskthreshold == '')
        {
            $txtlogsizethreshold=0;
        } 
        $ruleIdsIn = "$ruleids[0]";

        $query1 = "SELECT 
                       sourceHostId,
					   sourceDeviceName,
					   destinationHostId,
                       destinationDeviceName,
                       scenarioId ".
                  "FROM 
                       srcLogicalVolumeDestLogicalVolume ".
                  "WHERE
                       ruleId in ($ruleIdsIn) limit 1";
		$logging_obj->my_error_handler("DEBUG","query1 : $query1");
		
        $rs = $this->conn->sql_query($query1); 
		
        $fetch_data = $this->conn->sql_fetch_array($rs);
		
		$srcId 		= $fetch_data['sourceHostId'];
		$srcDevice 	= $this->conn->sql_escape_string($fetch_data['sourceDeviceName']);
		$hostId     = $fetch_data['destinationHostId'];
        $dstDevice  = $this->conn->sql_escape_string($fetch_data['destinationDeviceName']);
		$scenario_id = $fetch_data['scenarioId'];
		
        #$osFlag = $this->commonfunctions_obj->getAgentOSType($hostId);
		$sql_host = "SELECT osFlag FROM hosts WHERE id = '$hostId'";
		$rs_host = $this->conn->sql_query($sql_host);
		$row = $this->conn->sql_fetch_object($rs_host);
		$osFlag = $row->osFlag;
		
		$sql_unique_id = "SELECT uniqueId FROM pairCatLogDetails WHERE destinationHostId = '$hostId' AND destinationDeviceName = '$dstDevice'";
		$rs = $this->conn->sql_query($sql_unique_id);
		$row = $this->conn->sql_fetch_array($rs);
		$unique_id = $row[0];
		
		if($unique_id != '')
		{
			#$is_ret_reuse = $this->commonfunctions_obj->is_retention_reuse($scenario_id);
			$sql_unique_id = "SELECT isRetentionReuse FROM applicationScenario WHERE scenarioId = '$scenario_id'";
			$rs = $this->conn->sql_query($sql_unique_id);
			$row = $this->conn->sql_fetch_array($rs);
			$is_ret_reuse = $row[0];
			
			if($is_ret_reuse == 1)
			{
				$unqid = $unique_id;
			}
			else
			{
				$unqid = $this->generateuniqueretid();
				$sql = "UPDATE pairCatLogDetails set uniqueId='$unqid' WHERE destinationHostId='$hostId' AND destinationDeviceName='$dstDevice'";
				$rs = $this->conn->sql_query($sql);
			}
		}
		else
		{
			$unqid = $this->generateuniqueretid();
			if($scenario_id)
			{
				$sql = "INSERT into pairCatLogDetails(sourceHostId, sourceDeviceName, destinationHostId, destinationDeviceName, uniqueId) values('$srcId', '$srcDevice', '$hostId', '$dstDevice', '$unqid')";
				$rs = $this->conn->sql_query($sql);
				$logging_obj->my_error_handler("DEBUG","pairCatLogDetails sql: $sql");
			}
		}
		
		
		if($osFlag == 1)
        {
			$locdir = $this->conn->sql_escape_string($locdir);
            if($sparse_enable == 1)
            {
                $metaFilePath = $locdir.':\\'.$locmeta;
            }
            else
            {
                $metaFilePath = $this->commonfunctions_obj->verifyDatalogPath(
                                                            $locdir."\\\\".$locmeta);
            }
            $logDataDir   = $this->commonfunctions_obj->verifyDatalogPath(
                                                            $locdir."\\\\".$unqid);
            
        }    
        else
        {
            $metaFilePath = $locdir."/".$locmeta;
            $logDataDir   = $locdir."/".$unqid;
        }
        #4848
        $drive1 = $this->commonfunctions_obj->verifyDatalogPath($drive1);        
	    $drive2 = $this->commonfunctions_obj->verifyDatalogPath($drive2);

	  #bug 5486  
        if ($drive1)
        {
			$drive1 = $this->conn->sql_escape_string($drive1);
            $logDeviceName = $drive1;
        }
           
        if ($drive2)
        {
			$drive2 = $this->conn->sql_escape_string($drive2);
            $logDeviceName = $drive2;
        }

		#End bug 5486 
          
        $values = "($ruleids[0],$typeOfPolicy,'".
                    $this->conn->sql_escape_string($metaFilePath)."', 
                    sysdate(), 
                    $txtdiskthreshold,
                    '$unqid',
                    $sparse_enable
                    )";

        $cnt = count($ruleids);
        for ($i = 1; $i < $cnt; $i++) 
        {
            $ruleIdsIn = $ruleIdsIn . ", $ruleids[$i]";
            $values = $values . ", ($ruleids[$i],$typeOfPolicy,
                                    '".$this->conn->sql_escape_string($metaFilePath)."', 
                                    sysdate(),
                                    $txtdiskthreshold,
                                    '$unqid',
                                    $sparse_enable
                                    )";
        }

	        
        $query = "INSERT INTO 
                      retLogPolicy 
                      (ruleid,
                       retPolicyType,
                       metafilepath,
                       createdDate,
                       diskspacethreshold,
                       uniqueId,
                       sparseEnable
                       )
                 VALUES
                      $values";
		$logging_obj->my_error_handler("DEBUG","insert retLogPolicy : $query");	
        $rs = $this->conn->sql_query($query);
		
        if ($drive1)
        {
            $query3 = "UPDATE
                           logicalVolumes
                       SET
                           deviceFlagInUse = 3 
                       WHERE
                           HostId = '".$hostId."'
                       AND
                           ( deviceName = '".$drive1."' 
                           or mountpoint ='".$drive1."') ";
                           
            $rs = $this->conn->sql_query($query3);
		}
            
        if ($drive2)
        {

            $query4 = "UPDATE
                           logicalVolumes
                       SET 
                           deviceFlagInUse = 3
                       WHERE 
                           HostId = '".$hostId."' 
                       AND
                           ( deviceName = '".$drive2."' 
                           or mountpoint ='".$drive2."' )"; 
                           
            $rs = $this->conn->sql_query($query4);
		}
            
        $query5 = "SELECT
                       retId,
                       ruleid
                   FROM
                       retLogPolicy 
                   WHERE
                       ruleid in ($ruleIdsIn)";
		$logging_obj->my_error_handler("DEBUG","query5 : $query5");	
        $rs5 = $this->conn->sql_query($query5);
		
        $myrow = $this->conn->sql_fetch_row( $rs5);
        $histValues = "($myrow[0], $myrow[1], '$hostId',
                        '".$this->conn->sql_escape_string($logDataDir)."',
                        $conUnit, now(), now(),$typeOfPolicy,
                        $change_by_time_day,$change_by_time_hr,
                        $txtdiskthreshold,$txtlogsizethreshold,
                        0,$logExceedPolicy)";
        while ($myrow = $this->conn->sql_fetch_row( $rs5)) 
        {
            $histValues = $histValues . ", ($myrow[0], $myrow[1],
                                             '$hostId','$logDataDir', 
                                            $conUnit, now(), now(), 
                                            $typeOfPolicy,$change_by_time_day,
                                            $change_by_time_hr,
                                            $txtdiskthreshold,
                                            $txtlogsizethreshold,0,
                                            $logExceedPolicy)";
        }
                
        $query6 = "INSERT into 
                       retLogHistory ".
                       "(retId,
                       repId,
                       HostId,
                       logdatadir,
                       retLogSize,
                       createdDate,
                       modifiedDate,
                       type_of_policy,
                       ret_logupto_days,
                       ret_logupto_hrs,
                       diskspacethreshold,
                       logsizethreshold,
                       deleted,
                       logPruningPolicy) ".
                  "VALUES
                       $histValues";
		$logging_obj->my_error_handler("DEBUG","insert retLogPolicy2 : $query6");	
                       
        $rs5 = $this->conn->sql_query($query6);
		
        $query = "SELECT
                      sourceHostId,
                      sourceDeviceName,
                      destinationHostId,
                      destinationDeviceName
                  FROM
                      srcLogicalVolumeDestLogicalVolume
                  WHERE
                      ruleId='$ruleids[0]'";
                      
        $hostResult = $this->conn->sql_query($query);
		
        $logString = '';
        while ($hostData = $this->conn->sql_fetch_object($hostResult))
        {
            $iter = $this->conn->sql_query("SELECT
                                          ipaddress
                                      FROM 
                                          hosts 
                                      WHERE
                                          id = '$hostData->sourceHostId'");
            if ($iter)
            {
                $myrow = $this->conn->sql_fetch_object($iter);
                $srcIp = $myrow->ipaddress;
            }
			
            $iter = $this->conn->sql_query("SELECT
                                          ipaddress
                                      FROM
                                          hosts 
                                      WHERE
                                          id = '$hostData->destinationHostId'");
            if ($iter)
            {
                $myrow = $this->conn->sql_fetch_object($iter);
                $destIp = $myrow->ipaddress;
            }
			
            $logString = $logString."(".$srcIp."(".$hostData->sourceDeviceName."),".$destIp."(".$hostData->destinationDeviceName."))";
        }


		  if($typeOfPolicy == 0)
		  {
		   
			$type_policy = "Space Based";

		  }
		  elseif($typeOfPolicy == 1)
		  {

			$type_policy = "Time Based";

		  }
		  else
		  {
			$type_policy = "Composite Based";
		  }
		  if($sparse_enable == 0)
		  {

			 $sparseEnable = "Not Enabled";
		  }

		  else
		  {
			  $sparseEnable = "Enabled";
		  }

		  $ret_detals =  " policy type :".$type_policy." metafilepath : ".$locdir." diskspacethreshold : ".$txtdiskthreshold." sparseRetention :". $sparseEnable;



        $this->commonfunctions_obj->writeLog($_SESSION['username'],"Retention settings configured for $logString"."  with details :: $ret_detals");
		$logging_obj->my_error_handler("DEBUG","insert_retLogPolicy final Point");	
    }     
    
    /*
    * Function Name: getRetentionReserveSpace
    *
    * Description: 
    *    This function gives  ValueData  from the table transbandSettings
    *
    * Parameters:
    *     Param 1 [IN]:$vardb
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function getRetentionReserveSpace()
    {
        $query = "SELECT
                      ValueData 
                  FROM
                      transbandSettings 
                  WHERE
                      ValueName = 'RETENTION_RESERVE_SPACE'";
        $resquery = $this->conn->sql_query($query);    
        $retReserveSpace = $this->conn->sql_fetch_row ($resquery);    
        return ($retReserveSpace[0]);        
    }
    
    /*
    * Function Name: getActualFreeSpace
    *
    * Description: 
    *    This function gives retSizeinbytes ,currLogsize from the table retLogPolicy
    *
    * Parameters:
    *     Param 1 [IN]:$vardb
    *     Param 2 [IN]:$destHostId
    *     Param 3 [IN]:$destVol
    *     Param 4 [IN]:$freeSpace
    *     Param 5 [IN]:$ruleid
    *     Param 6 [IN]:$mountpoint
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    private function getActualFreeSpace($destHostId, $destVol,
                                       $freeSpace,$ruleid,$mountpoint)
    {
        $osType = $this->commonfunctions_obj->getAgentOSType($destHostId);
        $currLogSize = 0;
        $storageSpace = 0;
        foreach($ruleid as $ruleId)
        {
            $policy_sql="SELECT 
                            retId,
                            retPolicyType
                        FROM
                            retLogPolicy
                        WHERE
                            ruleid = $ruleId";
            $policy_rs = $this->conn->sql_query($policy_sql);
            $retlog=$this->conn->sql_fetch_object($policy_rs);
            $vol="";
            if($osType == 1)
            {   
                if(strlen($destVol) > 1)
                {
                    $vol = str_replace("\\","\\\\",$destVol);
                    $vol = $vol."\\\%";
                }
                else
                {
                    $vol = $destVol.":\\\%";
                }
            }
            else
            {
                $vol = $mountpoint."/%" ;
            }
            /*space based & composite policies*/
            if(($retlog->retPolicyType == 0)||($retlog->retPolicyType == 2))    
            {
                $sqlstat=  "SELECT
                                    ret.currLogsize,
                                    retspace.storageSpace
                                FROM
                                    retLogPolicy ret,
                                    retentionSpacePolicy retspace    
                                WHERE
                                    ret.ruleid = $ruleId
                                AND
                                    retspace.storageSpace <> 0 
								AND
									ret.retId = retspace.retID
                                AND 
                                    retspace.storagePath like '".$this->conn->sql_escape_string ($vol)."'";
                $sql_rs = $this->conn->sql_query($sqlstat);         
                $fetch_data=$this->conn->sql_fetch_object($sql_rs);                    
                $currLogSize = $currLogSize + intval($fetch_data->currLogsize);
                $storageSpace= $storageSpace + intval($fetch_data->storageSpace);
            }
            /*Time based Policy*/
            else
            {
                $sqlstat=   "SELECT
                                DISTINCT(retwindow.retId) 
                            FROM
                                retentionWindow retwindow,
                                retentionEventPolicy retevent 
                            WHERE
                                retevent.storagePath like '".$this->conn->sql_escape_string ($vol)."'";
                $sql_rs = $this->conn->sql_query($sqlstat);                            
                $fetch_retId=$this->conn->sql_fetch_object($sql_rs);
                $retentionId = $fetch_retId->retId;
                if($retentionId != "")
                {
                    $sqlstat1 = "SELECT
                                    (ret.currLogsize)
                                FROM
                                    retLogPolicy ret
                                WHERE
                                    ret.retId =$retentionId
                                AND
                                    ret.ruleid = $ruleId";
                    $sql_rs1=$this->conn->sql_query($sqlstat1);                            
                    $fetch_currlog_size=$this->conn->sql_fetch_object($sql_rs1);
                    $current_log_size=$fetch_currlog_size->currLogsize;
                    $currLogSize = $currLogSize + $current_log_size;
                }
            }
        }
        /*BUG 10441*/
        $actualFreeSpace = ($freeSpace - $storageSpace + $currLogSize);
        return $actualFreeSpace;
    }

   /*
	* Function Name: getUsedMediaDeviceCount
	*
	* Description:
	*    Computes the media retention drive used count for the pairs.
	*
	* Parameters:
	*    Param 1 [IN]:Database connection resource.
	*    Param 2 [IN]:Destination host identity.
	*    Param 3 [IN]:Destination volume.
	*    Param 4 [IN]:Free space of the device.
	*    Param 5 [IN]:Rule id of the pair.
	*    Param 6 [IN]:Mount point.
	*
	* Return Value:
	*    Returns the media retention drive used count for computation.
	*
	*/
	private function getUsedMediaDeviceCount($destHostId,$destVol,$freeSpace,$ruleid,$mountpoint)
	{
		/*Get the destination host operating system identity*/
        $replication_count = 0;
        $osType = $this->commonfunctions_obj->getAgentOSType($destHostId);
        foreach($ruleid as $ruleId)
        {
            $policy_sql="SELECT 
                            retId,
                            retPolicyType
                        FROM
                            retLogPolicy
                        WHERE
                            ruleid = $ruleId";
            $policy_rs = $this->conn->sql_query($policy_sql);
            $retlog=$this->conn->sql_fetch_object($policy_rs);
            $vol="";
            
            if($osType == 1)
            {
                if(strlen($destVol) > 1)
                {
                    $vol = str_replace("\\","\\\\",$destVol);
                    $vol = $vol."\\\%";
                }
                else
                {
                    $vol = $destVol.":\\\%";
                }
            }
            else
            {		
                $vol = $mountpoint."/%" ;
            }
            
            if(($retlog->retPolicyType == 0)||($retlog->retPolicyType == 2))    
            {
                $sqlstat=  "SELECT
                                ret.retId
                            FROM
                                retLogPolicy ret,
                                retentionSpacePolicy retspace    
                            WHERE
                                ret.ruleid = $ruleId
                            AND
                                retspace.storageSpace <> 0 
							AND
								ret.retId = retspace.retID
                            AND 
                                retspace.storagePath like '".$this->conn->sql_escape_string ($vol)."'";
                $sql_rs = $this->conn->sql_query($sqlstat);         
                $fetch_data=$this->conn->sql_fetch_object($sql_rs);                    
                if($fetch_data->retId != "")
                {
                  $replication_count++;  
                }
                
            }
            else
            {
                $sqlstat=   "SELECT
                                DISTINCT(retwindow.retId) 
                            FROM
                                retentionWindow retwindow,
                                retentionEventPolicy retevent 
                            WHERE
                                retevent.storagePath like '".$this->conn->sql_escape_string ($vol)."'";
                $sql_rs = $this->conn->sql_query($sqlstat);                            
                $fetch_retId=$this->conn->sql_fetch_object($sql_rs);
                $retentionId = $fetch_retId->retId;
                if($retentionId != "")
                {
                    $sqlstat1 = "SELECT
                                    (ret.retId)
                                FROM
                                    retLogPolicy ret
                                WHERE
                                    ret.retId =$retentionId
                                AND 
                                    ret.ruleid = $ruleId";
                    $sql_rs1=$this->conn->sql_query($sqlstat1);                            
                    $fetch_currlog_size=$this->conn->sql_fetch_object($sql_rs1);
                    if($fetch_currlog_size->retId != "")
                    {
                        $replication_count++;  
                    }
                }
            }
        } 
        return $replication_count;
	}

    /*
    * Function Name: generateuniqueretid
    *
    * Description: 
    *    This function gives the uniqueId for logdatadir .
    *
    * Parameters:
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    private function generateuniqueretid()
    {
        $retid = substr(uniqid(md5(rand())), 0, 10);
        $result = $this->conn->sql_query("SELECT
                                   uniqueId 
                               FROM
                                   retLogPolicy 
                               WHERE
                                   uniqueId 
                               LIKE
                                   '%$retid'");                                    
        $row = $this->conn->sql_fetch_row($result);
        if(!$row)
        {
            return $retid;
        }
        else
        {
            return $this->generateuniqueretid();
        }
    }
    
    /*
    * Function Name: del_tags
    *
    * Description: 
    *    This function delete records from the retentionTag .
    *
    * Parameters:
    *     Param 1 [IN]:$vardb
    *     Param 2 [IN]:$rId
    *
    * Return Value:
    *     Ret Value: True or False
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function del_tags($rId)
    {
        global $logging_obj;
		
		$logging_obj->my_error_handler("DEBUG","del_tags called ");
		
		$data = explode("#!#",$rId);
        $rId=$data[0];
        if((isset($data[1]))&&(isset($data[2]))) 
        {
            $casQuery = "DELETE
                         FROM
                             retentionTag 
                         WHERE 
                             HostId = '".$data[1]."' 
                         AND 
                             deviceName = '".$data[2]."'";
        }
        else
        {
            $casQuery = "DELETE
                         FROM 
                             retentionTag 
                         WHERE
                             pairId IN (SELECT pairId FROM srcLogicalVolumeDestLogicalVolume WHERE ruleId='".$rId."')";
        }
        $logging_obj->my_error_handler("DEBUG","del_tags query :: $casQuery ");
		$rs = $this->conn->sql_query($casQuery);
        return true;
    }
    
    public function get_catalogPath($sourceHostId,$sourceDeviceName,$destinationHostId,$destinationDeviceName,$retentionDir,$osFlag,$uniqueId,$sparseEnabled)
    {
        /*collect the compatibility nos of source & target*/
        #$src_compatibility_no =$this->commonfunctions_obj->get_compatibility($sourceHostId);
        $tgt_compatibility_no =$this->commonfunctions_obj->get_compatibility($destinationHostId);
        /*osFlag*/
        $retention_path = "";        
        if($osFlag == 1)
        {
            $content_store_arr = explode(':',$retentionDir);
            if(count($content_store_arr) == 1)
            {   
                $retention_log = $retentionDir.":";
            }
            else
            {
                $retention_log = $retentionDir;
            }
            $deviceName = str_replace(":", "", $destinationDeviceName);            
        }
        else
        {
            $retention_log = $retentionDir;
            $deviceName = str_replace("\\", "/", $destinationDeviceName);
        }
        
        if($tgt_compatibility_no <= 510000)
        {
            if($osFlag == 1)
            {
                $retention_path = "$retention_log\\$uniqueId\\$destinationHostId\\$deviceName\\cdpv1.db";
            }
            else
            {
                $retention_path = "$retention_log/$uniqueId/$destinationHostId/$deviceName/cdpv1.db";
            }
        }
        else
        {
            if($sparseEnabled == 1)
            {
                $cdp ="cdpv3.db"; 
            }
            else
            {
                $cdp = "cdpv1.db";
            }
            if($osFlag == 1)
            {
                $retention_path = "$retention_log\\catalogue\\$destinationHostId\\$deviceName\\$uniqueId\\$cdp";
            }
            else
            {   
                $retention_path = "$retention_log/catalogue/$destinationHostId/$deviceName/$uniqueId/$cdp";
            }
        }
        return $retention_path;
    }

	public function get_new_retention_path($destinationHostId,$destinationDeviceName,$retentionDir,$osFlag,$uniqueId,$sparseEnabled)
    {
        /*collect the compatibility nos of source & target*/
        $src_compatibility_no =$this->commonfunctions_obj->get_compatibility($sourceHostId);
        $tgt_compatibility_no =$this->commonfunctions_obj->get_compatibility($destinationHostId);
        /*osFlag*/
        $retention_path = "";        
        if($osFlag == 1)
        {
            $content_store_arr = explode(':',$retentionDir);
            if(count($content_store_arr) == 1)
            {   
                $retention_log = $retentionDir.":";
            }
            else
            {
                $retention_log = $retentionDir;
            }
            $deviceName = str_replace(":", "", $destinationDeviceName);            
        }
        else
        {
            $retention_log = $retentionDir;
            $deviceName = str_replace("\\", "/", $destinationDeviceName);
        }
        
        if($tgt_compatibility_no <= 510000)
        {
            if($osFlag == 1)
            {
                $retention_path = "$retention_log\\$uniqueId\\$destinationHostId\\$deviceName";
            }
            else
            {
                $retention_path = "$retention_log/$uniqueId/$destinationHostId/$deviceName";
            }
        }
        else
        {
            if($osFlag == 1)
            {
                $retention_path = "$retention_log\\catalogue\\$destinationHostId\\$deviceName\\$uniqueId";
            }
            else
            {   
                $retention_path = "$retention_log/catalogue/$destinationHostId/$deviceName/$uniqueId";
            }
        }
        return $retention_path;
    }
	
	/*
    * Function Name: del_timeranges
    *
    * Description: 
    *    This function delete records from the retentionTimeStamp .
    *
    * Parameters:
    *     Param 1 [IN]:$vardb
    *     Param 2 [IN]:$rId
    *
    * Return Value:
    *     Ret Value: True or False
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function del_timeranges($rId)
    {
        global $logging_obj;
		
		$logging_obj->my_error_handler("DEBUG","del_timeranges called ");
		
		$data = explode("#!#",$rId);
        $rId=$data[0];
        if((isset($data[1]))&&(isset($data[2]))) 
        {
            $casQuery = "DELETE
                         FROM
                             retentionTimeStamp 
                         WHERE 
                             hostId = '".$data[1]."' 
                         AND 
                             deviceName = '".$data[2]."'";
        }
        else
        {
            $casQuery = "DELETE
                         FROM 
                             retentionTimeStamp 
                         WHERE
                             pairId IN (SELECT pairId FROM srcLogicalVolumeDestLogicalVolume WHERE ruleId='".$rId."')";
        }
        $logging_obj->my_error_handler("DEBUG","del_timeranges query :: $casQuery ");
		$rs = $this->conn->sql_query($casQuery);
        return true;
    }
	
	public function delete_from_pairCatLogDetails($source_id, $source_device_name, $dest_id, $dest_device_name)
	{
		global $app_obj,$SCENARIO_DELETE_PENDING,$DISABLE;
		$src_device = $this->conn->sql_escape_string($source_device_name);
        $dest_device = $this->conn->sql_escape_string($dest_device_name);
		
		$scenario_id = $this->conn->sql_get_value("srcLogicalVolumeDestLogicalVolume", "scenarioId","sourceHostId = '$source_id' AND sourceDeviceName = '$src_device' AND destinationHostId = '$dest_id' AND destinationDeviceName = '$dest_device'");
		
		$is_ret_reuse = $this->commonfunctions_obj->is_retention_reuse($scenario_id);
		$execution_status = $app_obj->getScenarioExecutionStatus($scenario_id);
		
		if($is_ret_reuse != 1 || $execution_status == $SCENARIO_DELETE_PENDING || $execution_status == $DISABLE)
		{
			$del_sql = "DELETE 
			             FROM pairCatLogDetails 
						WHERE
							sourceHostId='$source_id' AND 
							sourceDeviceName='$src_device' AND 
							destinationHostId='$dest_id' AND 
							destinationDeviceName='$dest_device'";
			$this->conn->sql_query($del_sql);
		}
	}
	
	public function getValidRetentionVolumes($dstId=NULL, $retVolume=NULL, $src_id=NULL)
	{
        global $logging_obj;
	
		$sql_args = $vol_args = $device_details = array();
		$vol_cond = '';
		
		$retReserveSpace = $this->conn->sql_get_value("transbandSettings","ValueData","ValueName = 'RETENTION_RESERVE_SPACE'");
		
		if($dstId)
		{
			if(is_array($dstId)) $dstId = implode(",",$dstId);
			$host_cond = "WHERE FIND_IN_SET(destinationHostId ,?)";
			$sql_args[] = $dstId;
		}
		
		$rs = "SELECT
                  ruleId,
				  destinationHostId
               FROM
                   srcLogicalVolumeDestLogicalVolume
				$host_cond
               GROUP BY
                   destinationDeviceName";
        
        $protected_vols = $this->conn->sql_query($rs, $sql_args);
		foreach($protected_vols as $key => $value)
        {
            $ruleid_array[$value['destinationHostId']][] = $value['ruleId'];
        }	
		if($retVolume)
		{
			if(is_array($dstId)) $dstId = implode(",",$dstId);
			$vol_cond = "AND (lv.mountPoint=? OR lv.deviceName = ?) AND FIND_IN_SET(lv.hostId,?)";
			$vol_args = array($retVolume, $retVolume, $dstId);
		}
		
		/*
		a. Volume shouldn't be in use for any other purpose (target of replication etc.)
		b. Volume shouldn't be in lock mode.
		c. Volume shouldn't be cache volume. (MT installation shouldn't exist on that volume. PS+MT custom installation volume is not eligible for retention volume. Here installed PS+MT volume is cache volume of MT.)
		d. Volume shouldn't be system volume.
		e. The Volume File system type shouldn't be FAT and FAT32.
		f. The volume capacity should be non-zero.
		*/		
		$device_sql = "SELECT 
								lv.freeSpace, 
								lv.deviceName, 
								lv.hostId, 
								lv.mountPoint,
								h.osFlag
							FROM 
								logicalVolumes lv,
								hosts h
							WHERE 
								h.id = lv.hostId AND
								(lv.deviceFlagInUse = 0 or lv.deviceFlagInUse = 3) AND
                                (lv.deviceLocked = 0 or lv.capacity != 0) AND
                                lv.volumeType NOT IN(1,2) AND
                                lv.cacheVolume = 0 AND
								lv.systemVolume = 0 AND
                                lv.farLineProtected!=1 AND
                                lv.fileSystemType NOT IN('FAT','FAT32') AND
                                lv.isProtectable = '1' 
                                $vol_cond";
		$device_data = $this->conn->sql_query($device_sql,$vol_args);
		$ret_volume_info = array();
		$device_data_count = count($device_data);
		
		if($device_data_count > 0)
		{
			foreach($device_data as $key1 => $value1)
			{
				if($value1['osFlag'] == 1) $device_name = $value1['deviceName'];
				else $device_name = $value1['mountPoint'];
				
				$device_details[$value1['hostId']][$device_name] = $value1['freeSpace'];
			}
			
			foreach($device_details as $host_id => $device_info)
			{
				foreach($device_info as $ret_device_name => $ret_free_space)
				{
					if(!is_null($ruleid_array[$host_id]))
					{
						if($src_id) $volume_count = $this->conn->sql_get_value("logicalVolumes","count(*)","hostId=? and volumeType=5",array($src_id));
						else $volume_count = 4;
						
						$availableSpace = $this->getActualFreeSpace($dstId,$ret_device_name,$ret_free_space,$ruleid_array[$host_id],$ret_device_name);			
						$device_used_for_pairs_count = $this->getUsedMediaDeviceCount($dstId,$ret_device_name,$ret_free_space,$ruleid_array[$host_id],$ret_device_name);			
						$reserve_space_total = $retReserveSpace * ($device_used_for_pairs_count + $volume_count);
					}
					else
					{
						$availableSpace = $ret_free_space;
						$reserve_space_total = $retReserveSpace;
					}
					$availabeFreeSpace = ($availableSpace) - ($reserve_space_total);
					$availableSpace1 = $this->commonfunctions_obj->ByteSize($availabeFreeSpace);
					
					if ($ret_free_space >= $retReserveSpace && $availabeFreeSpace >= $retReserveSpace)
					{
						if($availableSpace1 > 1)
						{
							$ret_volume_info[$host_id][$ret_device_name] = $ret_free_space;
						}						
					}
					elseif ($retVolume)
					{
						$logging_obj->my_error_handler("INFO", "getValidRetentionVolumes: Host id -> $host_id, Actual free space -> $availableSpace, Total retention reserve space -> $reserve_space_total, Retention reserve space -> $retReserveSpace, Retention free space -> $ret_free_space, Retention device name -> $ret_device_name");
					}
				}
			}
		}
		
		if (! $device_data_count)	return 0;	// no volumes available
		elseif ($retVolume && empty($ret_volume_info))	
		{
			return 1;	// not having available free space
		}
		else return $ret_volume_info;
	}

};
?>