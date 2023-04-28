<?php
/* 
 *================================================================= 
 * FILENAME
 *    Recovery.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    schedule/recovery snapshot
 *
 * HISTORY
 *     20-Feb-2008  Reena    Created by pulling functions from 
 *			     functions.php and conn.php
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/

class Recovery
{
    /*
     * $db: database connection object
    */
    
    private $conn;
    
    /*
     * $commonfunctions_obj: common functions object
    */
    private $commonfunctions_obj;
    
    
    /*
    * Function Name: Recovery
    *
    * Description:   
    *    This is the constructor of the class
    *
    * Return Value:
    *    Ret Value:
    */
    function __construct()
    {
		global $conn_obj;
        $this->commonfunctions_obj = new CommonFunctions();
        $this->conn = $conn_obj;
    }       
   
   /*
    * Function Name: update_device_flag
    *
    * Description: 
    *    Update the deviceFlagInUse=0 in logicalVolumes
    *
    * Parameters:
    *     Param 1 [IN/OUT]:$id
    *     Param 2 [IN/OUT]:$flag
    *
    * Return Value:
    *     Ret Value: No Value
    *
    * Exceptions:
    *     <Specify the type of exception caught>
   */
   private function update_device_flag($id , $flag)
   {
        if($id!=NULL)
        {
            if($flag ==1)
           {
                    
                $sql = "SELECT
                        destHostid,
                        destDeviceName
                      FROM
                        snapshotMain
                      WHERE
                        snapshotId =$id";
                        
                $rs = $this->conn->sql_query($sql );
                
                while ($row = $this->conn->sql_fetch_row($rs))
                {
                    $hostId    = $row[0];
                    $deviceName = $row[1];      
                }
                $sql1 = "SELECT
                            count(*)
                        FROM
                            snapShots
                        WHERE
                            snapshotId =$id";
                            
                $rs1 = $this->conn->sql_query($sql1 );
                $row = $this->conn->sql_fetch_row($rs1);
                $num_instances = $row[0];
                if($num_instances == 0)
                {
                    $qry="UPDATE
                            logicalVolumes
                          SET
                            deviceFlagInUse=0
                          WHERE
                            deviceName= '$deviceName' AND
                            hostId = '$hostId'";
                            
                    $mainUpdate=$this->conn->sql_query($qry );
                }
           }
           else if($flag ==2)
           {
    
                $query = "SELECT
                            destHostid,
                            destDeviceName,
                            snaptype,
                            executionState,
                            snapshotId,
                            errMessage,
                            pending
                          FROM
                            snapShots
                          WHERE
                            snapshotId IN ($id) AND
                            executionState IN (0,4,5)";
                            
                $rs = $this->conn->sql_query($query );
                
                while ($row=$this->conn->sql_fetch_row($rs))
                {
                    $hostId    = $row[0];
                    $deviceName = $row[1];
                    $sql1="SELECT
                                count(*)
                           FROM
                                snapShots
                           WHERE
                                destDeviceName='$deviceName' AND
                                destHostid = '$hostId' AND
                                executionState IN (1,2,3,6,7) ";
                                
                    $rs1=$this->conn->sql_query($sql1 );
                    $row=$this->conn->sql_fetch_row($rs1);
                    
                    $sql2="SELECT
                                count(*)
                            FROM
                                snapshotMain
                            WHERE
                                destDeviceName='$deviceName' AND
                                destHostid = '$hostId' AND
                                Deleted <> 1 ";
                                
                    $rs2=$this->conn->sql_query($sql2 );
                    $row1=$this->conn->sql_fetch_row($rs2);
                    $deleted_flag = $row1[0];
                    $num_instances = $row[0];
                    if($num_instances == 0 && $deleted_flag ==0 )
                    {
                        $qry="UPDATE
                                logicalVolumes
                              SET
                                deviceFlagInUse=0
                              WHERE
                                deviceName= '$deviceName' AND
                                hostId = '$hostId'";
                                
                        $mainUpdate=$this->conn->sql_query($qry );
                    }
                }
           }
        }  
    }
    
	
    /*
    * Function Name: get_search_tag
    *
    * Description: 
    *
    * 
    * Parameters:
    *     Param 1 [IN]:$vardb
    *     Param 2 [IN]:$varPost
    *     Param 3 [IN]:$versionno
    *     Param 4 [IN]:$tgtId,$
    *     Param 5 [IN]:$tgtName
    *     Param 6 [OUT]:$result
    *
    * Return Value:
    *     Ret Value: Array
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function get_search_tag($varPost,$versionno=NULL,$tgtId,$tgtName,$fs_flag=0)
    {       
        $volume_obj = new VolumeProtection();
		
		$sql = "SELECT
				planId
			 FROM
				srcLogicalVolumeDestLogicalVolume
			 WHERE
				destinationHostId = '$tgtId'
			 AND
				destinationDeviceName = '$tgtName'";
		$rs = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_object($rs);
		$plan_id = $row->planId;
		
		$solution_type = $this->commonfunctions_obj->get_solution_type($plan_id);
		
        extract($varPost);
		$arrVal = explode("||",$hidRuleId);
		$ruleId = $arrVal[0];
		$pairId = $arrVal[sizeof($arrVal) -1];
		$arrVal[2] = $volume_obj->get_unformat_dev_name($arrVal[4],$arrVal[2]);
		$arrVal[3] = $volume_obj->get_unformat_dev_name($arrVal[4],$arrVal[3]);
		$arrClusterruleids = array();
		// $arrVal[1] is for is_src_cluster  flag  it has only values 0 or 1 so removing the if ( arrVal[1]==0 )  block and add that code block to else part of it
		if ($arrVal[1]==1 && ($solution_type != 'PRISM'))
        {
            $clusterDetailSql = "SELECT
                                    map.ruleId,
                                    map.destinationHostId
                                 FROM
                                    hosts lh,
                                    hosts rh,
                                    logicalVolumes lhv,
                                    logicalVolumes rhv,
                                    srcLogicalVolumeDestLogicalVolume map,
                                    clusters as cus";
                                    
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "lh.id = lhv.hostId",$clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "rh.id = rhv.hostId",$clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "lh.id = map.sourceHostId",$clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "rh.id = map.destinationHostId",
                                $clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "lhv.deviceName = map.sourceDeviceName",
                                $clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "rhv.deviceName = map.destinationDeviceName",
                                $clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "map.sourceHostId = cus.hostId",$clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "map.sourceDevicename = '".
                                $this->conn->sql_escape_string($arrVal[3])."'",
                                $clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "map.destinationDevicename = '".
                                $this->conn->sql_escape_string($arrVal[2])."'",
                                $clusterDetailSql);
            
            $clusterDetailSql = $this->commonfunctions_obj->addcond(
                                "cus.clusterruleId='$ruleId'
                                ORDER BY lh.name, lhv.deviceName",
                                $clusterDetailSql);
            
            $rsCluster = $this->conn->sql_query($clusterDetailSql);
            
            while($clusterRuleIds = $this->conn->sql_fetch_array($rsCluster))
            {
                $arrClusterruleids[] = $clusterRuleIds[0];
                $destid = $tgtId;
            }
            
            $ruleId = implode(",",$arrClusterruleids);
            $dest_info = array($destid,$tgtName);		
        }
        else
        {
            $query_nor = "SELECT
                            destinationHostId,
                            destinationDeviceName
                          FROM
                            srcLogicalVolumeDestLogicalVolume
                          WHERE
                            ruleId = '$ruleId'";
                            
            $rs_nor = $this->conn->sql_query($query_nor);
            $dest_inf = $this->conn->sql_fetch_array($rs_nor);
            $dest_info = array($dest_inf[0],$dest_inf[1]);
		}
        $arrApplicationTag = $this->get_transbandsetting_for('ACM');
        /*10480  fetch tarGuid column for vacp tagbased multi-volume recovery*/
        if ($hidRadio==0)
        {
            $searchQuery = "SELECT
                                t.tagTimeStamp,
                                s.ValueName,
                                t.userTag,
                                t.paddedTagTimeStamp,
                                t.accuracy,
                                t.tagGuid,
								t.tagVerfiication,
								t.comment,
								t.tagTimeStampUTC,								
								s.valueData,
								t.hostId,
								UNIX_TIMESTAMP(concat_ws(' ', REPLACE(SUBSTRING_INDEX(t.tagTimeStamp, ',', 3), ',', '-'), REPLACE(substr(t.tagTimeStamp FROM 12 FOR 8), ',', ':'))) tagTimeStampUnix
                            FROM
                                retentionTag t,
                                consistencyTagList s
                            WHERE
                                s.valueType='ACM' AND
                                s.valueData=t.appName";
        }
        else
        {
            $searchQuery = "SELECT
                                t.tagTimeStamp,
                                s.ValueName,
                                t.userTag,
                                t.paddedTagTimeStamp,
                                t.accuracy,
                                t.tagGuid,
								t.tagVerfiication,
								t.comment,
								t.tagTimeStampUTC,
								s.valueData,
								t.hostId,
								UNIX_TIMESTAMP(concat_ws(' ', REPLACE(SUBSTRING_INDEX(t.tagTimeStamp, ',', 3), ',', '-'), REPLACE(substr(t.tagTimeStamp FROM 12 FOR 8), ',', ':'))) tagTimeStampUnix
                            FROM
                                retentionTag t,
                                consistencyTagList s
                            WHERE
                            s.valueType='ACM' AND
                            s.valueData=t.appName";
                            
            /*
            * #5455: removed the gab between dates, due to which search on date
            * was failing.
            */
            if ($chkSC1==1)
            {
                if(strlen($tag_from_selday) == 1)
				{
					$tag_from_selday = '0'.$tag_from_selday;
				}
				if(strlen($tag_from_selMonth) == 1)
				{
					$tag_from_selMonth = '0'.$tag_from_selMonth;
				}
				
				if(strlen($tag_to_selday) == 1)
				{
					$tag_to_selday = '0'.$tag_to_selday;
				}
				if(strlen($tag_to_selMonth) == 1)
				{
					$tag_to_selMonth = '0'.$tag_to_selMonth;
				}
				
				$dayforafter = $tag_from_selday + 1;
                
                if ($tag_datespecific==1)
                {
                    $searchQuery = $this->commonfunctions_obj->addcond2(
                            "t.tagTimeStamp >= '".$tag_from_selYear.",".$tag_from_selMonth.",".$dayforafter.",00,00,00,00,00,00'",
                            $searchQuery,
                            0);			                    
                }
                    
                if ($tag_datespecific==2)
                {
                    $searchQuery = $this->commonfunctions_obj->addcond2(
                            "t.tagTimeStamp < '".$tag_from_selYear.",".$tag_from_selMonth.",".$tag_from_selday.",00,00,00,00,00,00'",
                            $searchQuery,
                            0);
                }   
                if ($tag_datespecific==3)
                {
                    $searchQuery = $this->commonfunctions_obj->addcond2(
                        "t.tagTimeStamp >= '".$tag_from_selYear.",".$tag_from_selMonth.",".$tag_from_selday.",00,00,00,00,00,00' and t.tagTimeStamp <= '".$tag_to_selYear.",".$tag_to_selMonth.",".$tag_to_selday.",24,00,00,00,00,00'",
                        $searchQuery,
                        0);
                }
                if ($tag_datespecific==4)
                {
                    /*
                    * #5455: placed comma in from of % sign, so that when we
                    * search for tags on date 1-July , it should not get the
                    * tags dated 12-Jul.
                    */
                    $searchQuery = $this->commonfunctions_obj->addcond2(
                        "t.tagTimeStamp like '".$tag_from_selYear.",".$tag_from_selMonth.",".$tag_from_selday.",%'",$searchQuery,0); 
                } 
			
            }
            if ($chkSC2==2)
            {
                $arrtagValues = explode(",",$hidApplicationtag);
                $a=0;
                
                foreach ($arrtagValues as $k=>$v)
                {
                    $arrStr[$a] = $arrApplicationTag[valueName][$v];
                    $a++;
                }
                $str = implode(",",$arrStr);

                $sQuery1 = "SELECT
                                ValueData
                            FROM
                                consistencyTagList
                            WHERE
                                FIND_IN_SET(ValueName,'$str')";

                $rs1 = $this->conn->sql_query($sQuery1);
                
                while($fetch_tag = $this->conn->sql_fetch_row($rs1))
                {
                    $result1[]= $fetch_tag[0];
                }
                $str = implode(",",$result1);
            }
            
            if (($chkSC4==4)&&($chkSC2==2))
            {
                $searchQuery = $this->commonfunctions_obj->addcond2(
                    "((FIND_IN_SET(t.appName,'$str'))||(t.appName = 'USERDEFINED'))",$searchQuery,0);
            }
            elseif($chkSC4==4)
            {
                $searchQuery = $this->commonfunctions_obj->addcond2(
                                "t.appName = 'USERDEFINED'",$searchQuery,0);
            }
            elseif($chkSC2==2)
            {
                $searchQuery = $this->commonfunctions_obj->addcond2(
                        "FIND_IN_SET(t.appName,'$str')",$searchQuery,0);
            }
            
            if($chkSC5==5)
            {
                $searchQuery = $this->commonfunctions_obj->addcond2(
                                "t.accuracy = $tag_accuracy",$searchQuery,0);    
            }
            if($chkSC3==3)
            {
                $searchQuery = $this->commonfunctions_obj->addcond2(
                "UPPER(t.userTag) like '%".strtoupper($txtEvent)."%'",$searchQuery,0);	
            }
			if($api_calling == 1)
			{
				if(!$filter_tag_application)
				$searchQuery = $this->commonfunctions_obj->addcond2("s.valueData IN ('FS', 'SystemLevel')", $searchQuery, 0);
				if(!$filter_tag_accuracy)
				$searchQuery = $this->commonfunctions_obj->addcond2("t.accuracy = '0'", $searchQuery, 0);
				$filter_tag = explode(",", $filter_tag_str);
				if(in_array("accuracy", $filter_tag))
				$searchQuery = $this->commonfunctions_obj->addcond2("t.accuracy = $filter_tag_accuracy", $searchQuery, 0);
				
				if($filter_tag_end_time && $filter_tag_start_time)
				{
					//select UNIX_TIMESTAMP(concat_ws(' ', REPLACE(SUBSTRING_INDEX(t.tagTimeStamp, ',', 3), ',', '-'), REPLACE(substr(t.tagTimeStamp FROM 12 FOR 8), ',', ':'))) FROM retentionTag;
					$searchQuery = $this->commonfunctions_obj->addcond2("UNIX_TIMESTAMP(concat_ws(' ', REPLACE(SUBSTRING_INDEX(t.tagTimeStamp, ',', 3), ',', '-'), REPLACE(substr(t.tagTimeStamp FROM 12 FOR 8), ',', ':'))) >= '".$filter_tag_start_time."' AND UNIX_TIMESTAMP(concat_ws(' ', REPLACE(SUBSTRING_INDEX(t.tagTimeStamp, ',', 3), ',', '-'), REPLACE(substr(t.tagTimeStamp FROM 12 FOR 8), ',', ':'))) <= '".$filter_tag_end_time."'", $searchQuery, 0);
				}
				
				if(in_array("application", $filter_tag))
				$searchQuery = $this->commonfunctions_obj->addcond2("s.valueData = '".$filter_tag_application."'", $searchQuery, 0);
				if(in_array("tagname", $filter_tag))
				$searchQuery = $this->commonfunctions_obj->addcond2("t.userTag = '".$filter_tag_name."'", $searchQuery, 0);
			}
			
            if ($arrVal[1]==1)
            {
                $searchQuery = $this->commonfunctions_obj->addcond2(
                                " FIND_IN_SET(t.pairId,'$pairId')",$searchQuery,0);		
            }
            else
            {
                $searchQuery = $this->commonfunctions_obj->addcond2(
                                " t.pairId=$pairId",$searchQuery,0);
            }       
        }
		
		if($fs_flag) {
			$searchQuery .= " and s.valueData='FS' and t.accuracy='0' ";
		}
		
        $searchQuery .= " and t.HostId = '$dest_info[0]'
                    and t.deviceName = '".$this->conn->sql_escape_string($dest_info[1])."'";
        if($api_calling == 1) $searchQuery .= " GROUP BY t.userTag";		
        $searchQuery .= " order by $hidsort";
        
        /*
         * fix for timestamp display with latest at the top.
        */
        if ( 0 == strcmp($hidsort,'paddedTagTimeStamp'))
        {
            $searchQuery .= " DESC";
        }
        $rs = $this->conn->sql_query($searchQuery);

        /*
        * Consider one rec set only as even for cluster case also,
        * tgt is same, hence logsFrom & logsTo will be be same
        * for all the pairs.
        */
        $sqlSrcL = "SELECT
                        logsFrom,
                        logsTo
                    FROM
                        srcLogicalVolumeDestLogicalVolume
                    WHERE
                        ruleId = '$ruleId' OR 
                        (destinationHostId = '$dest_info[0]' AND
                        destinationDeviceName = '".$this->conn->sql_escape_string($dest_info[1])."') 
                        LIMIT 0,1";
        $resSrcL = $this->conn->sql_query($sqlSrcL);
        $rowSrcL = $this->conn->sql_fetch_row($resSrcL);

        $tgtTmStampArr = explode(",",$rowSrcL[0]);
		$arrTo = explode(",",$rowSrcL[1]);

        $flag = array('../inmages/white_green.PNG',
                      '../inmages/white_yellow.PNG',
                      '../inmages/white_red.PNG');
        
        $result = array();
        while($fetch_tag = $this->conn->sql_fetch_row($rs))
        {
            $includeFlag = false;
            $arrFrom = explode(",",$fetch_tag[0]);

            if(intval($arrFrom[0]) > intval($tgtTmStampArr[0]))
            {
                /* Check for Year */
                $includeFlag = true;
            }
            elseif(intval($arrFrom[0]) == intval($tgtTmStampArr[0]) AND intval($arrFrom[1])>intval($tgtTmStampArr[1]))
            {
                /* Check for month, if years are same */
                $includeFlag = true;
            }
            elseif(intval($arrFrom[0]) == intval($tgtTmStampArr[0]) AND intval($arrFrom[1])== intval($tgtTmStampArr[1]) AND intval($arrFrom[2])> intval($tgtTmStampArr[2]))
            {
                /* Check for day, if years & months are same */
                $includeFlag = true;
            }
            elseif(intval($arrFrom[0]) == intval($tgtTmStampArr[0]) AND intval($arrFrom[1])== intval($tgtTmStampArr[1]) AND intval($arrFrom[2]) == intval($tgtTmStampArr[2]) AND intval($arrFrom[3]) > intval($tgtTmStampArr[3]))
            {
               /* Check for hour, if years, months & days are same */
                $includeFlag = true;
            }
            elseif(intval($arrFrom[0]) == intval($tgtTmStampArr[0]) AND intval($arrFrom[1])== intval($tgtTmStampArr[1]) AND intval($arrFrom[2]) == intval($tgtTmStampArr[2]) AND intval($arrFrom[3]) == intval($tgtTmStampArr[3]) AND intval($arrFrom[4]) > intval($tgtTmStampArr[4]))
            {
                /* Check for minutes, if years, months, days & hours are same */
                $includeFlag = true;
            }
            elseif(intval($arrFrom[0]) == intval($tgtTmStampArr[0]) AND intval($arrFrom[1])== intval($tgtTmStampArr[1]) AND intval($arrFrom[2]) == intval($tgtTmStampArr[2]) AND intval($arrFrom[3]) == intval($tgtTmStampArr[3]) AND intval($arrFrom[4]) == intval($tgtTmStampArr[4]) AND intval($arrFrom[5]) > intval($tgtTmStampArr[5]))
            {
                /* Check for seconds, if years, months, days, hours & minutes are same */
                $includeFlag = true;
            }
            elseif(intval($arrFrom[0]) == intval($tgtTmStampArr[0]) AND intval($arrFrom[1])== intval($tgtTmStampArr[1]) AND intval($arrFrom[2]) == intval($tgtTmStampArr[2]) AND intval($arrFrom[3]) == intval($tgtTmStampArr[3]) AND intval($arrFrom[4]) == intval($tgtTmStampArr[4]) AND intval($arrFrom[5]) == intval($tgtTmStampArr[5]) AND intval($arrFrom[6]) > intval($tgtTmStampArr[6]))
            {
                /* Check for mili-seconds, if years, months, days, hours, minutes & seconds are same */
                $includeFlag = true;
            }
			elseif(intval($arrFrom[0]) == intval($tgtTmStampArr[0]) AND intval($arrFrom[1])== intval($tgtTmStampArr[1]) AND intval($arrFrom[2]) == intval($tgtTmStampArr[2]) AND intval($arrFrom[3]) == intval($tgtTmStampArr[3]) AND intval($arrFrom[4]) == intval($tgtTmStampArr[4]) AND intval($arrFrom[5]) == intval($tgtTmStampArr[5]) AND intval($arrFrom[6]) == intval($tgtTmStampArr[6]))
            {
                /* Check for all, if years, months, days, hours, minutes, seconds & mili-seconds are same */
                $includeFlag = true;
            }
			// To get the timestamp in UnixTimeStamp from the query itself
			$fetch_tag[12]   = $fetch_tag[11];
			$fetch_tag[11]   = $fetch_tag[10];
			$fetch_tag[10]   = $fetch_tag[9];
			$fetch_tag[9]   = $fetch_tag[8];
			$fetch_tag[8]   = $fetch_tag[7];
			$fetch_tag[7]   = $fetch_tag[6];
            $fetch_tag[6]   = $fetch_tag[5];
            $fetch_tag[5]   = $fetch_tag[4];
            
            $accuracy = $flag[$fetch_tag[4]];
            $fetch_tag[4] = $accuracy;
            if($includeFlag === true)
            {
                $result[]= $fetch_tag;
            }
        }

        return $result;
    }
	
	
	
    public function get_common_tag($arr1,$destId,$destDev,$flag=0)
	{
        $result = array();
        $tmp_arr = array();
		
		for($i=0;$i<count($arr1);$i++)
        {
          	$appName = $arr1[$i][10];
			$userTag = $arr1[$i][2];
			$accuracy = $arr1[$i][5];
			$tagGuid = $arr1[$i][6];
			$str = $appName."#$#".$userTag."#$#".$accuracy."#$#".$tagGuid;
			$tmp_arr[$str] = $arr1[$i];
        }
		$keys = implode("','",array_keys($tmp_arr));
		if($flag==0)
		{
			$destDev = $this->conn->sql_escape_string($destDev);
		}
		$sql = "SELECT CONCAT_WS('#$#',appName,userTag,accuracy,tagGuid) AS value from retentionTag where CONCAT_WS('#$#',appName,userTag,accuracy,tagGuid) IN ('$keys') AND HostId='$destId' AND deviceName='$destDev' group by userTag ORDER BY tagTimeStampUTC DESC";
		$rs = $this->conn->sql_query($sql);
		while($row = $this->conn->sql_fetch_row($rs)) 
		{
			if((array_key_exists($row[0],$tmp_arr)))
			{
				$result[] = $tmp_arr[$row[0]];
			}
		}
			
        return $result;		
    }

    /*
    * Function Name: get_transbandsetting_for
    *
    * Description: 
    *
    * 
    * Parameters:
    *     Param 1 [IN]:$vardb
    *     Param 2 [IN]:$varPost
    *     Param 3 [IN]:$versionno
    *     Param 4 [IN]:$tgtId,$
    *     Param 5 [IN]:$tgtName
    *     Param 6 [OUT]:$result
    *
    * Return Value:
    *     Ret Value: Array
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function get_transbandsetting_for($varType)
    {
        $result=array();
        $querySettings = "SELECT
                            ValueName,ValueData
                          FROM
                            consistencyTagList
                          WHERE
                            ValueType='$varType'
                          ORDER BY
                            ValueName";
        $rs = $this->conn->sql_query($querySettings);
        
        while ($fetch_data = $this->conn->sql_fetch_array($rs))
        {
            $result['valueName'][]= $fetch_data['ValueName'];
			$result['valueData'][]= $fetch_data['ValueData'];
        }
        
        return $result;
    }
	
	public function check_common_time_exists($pair_ids,$from_capi=0)
	{
		$pair_id_str = implode("','",$pair_ids);
		
		$sql = "SELECT
					pairId,
					count(*) AS count,
					min(StartTimeUTC) AS min_st,
					max(EndTimeUTC) AS max_et
				FROM
					retentionTimeStamp
				WHERE
					accuracy = 0 AND
					pairId IN ('$pair_id_str')
				GROUP BY
					pairId";
		$rs = $this->conn->sql_query ($sql);
		if(count($pair_ids) != $this->conn->sql_num_row($rs)) {
			return false;
		}
		$tmp_st = array();
		$tmp_et = array();
		while($row = $this->conn->sql_fetch_object($rs)) {
			$tmp_st[] = $row->min_st;
			$tmp_et[] = $row->max_et;
		}
		$max_st = max($tmp_st);
		$min_et = min($tmp_et);
		
		$sql = "SELECT
					pairId,
					StartTimeUTC,
					EndTimeUTC,
					StartTime
				FROM
					retentionTimeStamp
				WHERE
					pairId IN ('$pair_id_str') AND
					accuracy = '0' AND 
					(($max_st BETWEEN StartTimeUTC AND EndTimeUTC) OR ($min_et BETWEEN StartTimeUTC AND EndTimeUTC))
                ORDER BY StartTimeUTC DESC, EndTimeUTC DESC";
		$rs = $this->conn->sql_query ($sql);
		if(!$this->conn->sql_num_row($rs)){
			return false;
		}
		$times = array();
		while($row = $this->conn->sql_fetch_object($rs)) {
			$times[$row->pairId][] = array('StartTime'=>$row->StartTimeUTC,'EndTime'=>$row->EndTimeUTC, 'RStartTime'=>$row->StartTime);
		}
		if(count($pair_ids)!=count(array_keys($times))) {
			return false;
		}

		$result = array();
		$tmp_arr = array();
		$tmp_arr = array_values($times);
		$result = $tmp_arr[0];
		$flag = false;
		for($i=1;$i<count($tmp_arr);$i++) {
			$pair2 = $tmp_arr[$i];
			$result = $this->get_common_time($result,$pair2);
			if(!count($result)) {
				break;
			}
		}
		
		if($from_capi)
		{
			return $result;
		}
		else
		{
			if(count($result)) 
			{
				$flag = true;
				return $flag;
			}
		}
	}
	
	private function get_common_time($pair1,$pair2)
	{
		$result = array();
		foreach($pair1 as $k=>$v) {
			$st = $v['StartTime'];
			$et = $v['EndTime'];
			$rst = $v['RStartTime'];
			foreach($pair2 as $k1=>$v1) {
				$st1 = $v1['StartTime'];
				$et1 = $v1['EndTime'];
				$rst1 = $v1['RStartTime'];
				if(($et<$st1)||($st>$et1)) {
					continue;
				}elseif((($st>=$st1)&&($st<=$et1))&&($et>$et1) ) {
					$result[] = array('StartTime'=>$st,'EndTime'=>$et1,'RStartTime'=>$rst);
				}elseif((($et>=$st1)&&($et<=$et1))&&($st<$st1)) {
					$result[] = array('StartTime'=>$st1,'EndTime'=>$et,'RStartTime'=>$rst1);
				}elseif((($st1>=$st)&&($st1<=$et))&&(($et1>=$st)&&($et1<=$et))) {
					$result[] = array('StartTime'=>$st1,'EndTime'=>$et1,'RStartTime'=>$rst1);
				}elseif((($st>=$st1)&&($st<=$et1))&&(($et>=$st1)&&($et<=$et1))) {
					$result[] = array('StartTime'=>$st,'EndTime'=>$et,'RStartTime'=>$rst);
				}
			}
		}
		return $result;
	}
	
	
	public function check_specified_time_exists($pair_ids,$time)
	{
		$pair_id_str = implode("','",$pair_ids);
		
		$sql = "SELECT
					pairId,
					count(*) AS count
				FROM
					retentionTimeStamp
				WHERE
					accuracy = '0' AND
					pairId IN ('$pair_id_str') AND
					$time BETWEEN StartTimeUTC AND EndTimeUTC
				GROUP BY
					pairId";
		$rs = $this->conn->sql_query ($sql);
		if(count($pair_ids) != $this->conn->sql_num_row($rs)) {
			return false;
		}
		return true;
	}
   
	
    function getPairsFromPlanId($plan_id, $level=NULL)
    {
        $get_source_ids_sql = "SELECT
                                p.sourceHostId,
                                p.sourceDeviceName,
                                p.ruleId,
                                p.destinationHostId,
                                p.destinationDeviceName,
                                p.pairId,
                                p.planId,
                                p.scenarioId,
                                (SELECT count(*) FROM srcLogicalVolumeDestLogicalVolume WHERE sourceHostId = p.sourceHostId) as pairCount
                               FROM
                                srcLogicalVolumeDestLogicalVolume p
                               WHERE
                                p.planId = ? AND
                                (p.isQuasiflag = '2' OR 
								p.restartResyncCounter > 0)";
		
		$pairs_details = $this->conn->sql_query($get_source_ids_sql, array($plan_id));
		if(!$pairs_details) return;
        $protection_details = array();
        $target_hosts = array();
        foreach($pairs_details as $pair)
        {
			if(strtolower($level) == "plan")
            $protection_details[$pair['planId']][$pair['sourceHostId']][] = $pair;
			else
            $protection_details[$pair['sourceHostId']][] = $pair;
        }
        return $protection_details;
    }
    
    function get_pair_details($src_host_ids=NULL, $level=NULL)
    {
		$sql_args = array();
		if($src_host_ids)
		{
			$host_ids_str = "";
			if(is_array($src_host_ids)) $host_ids_str = implode(",", $src_host_ids);
			else $host_ids_str = $src_host_ids;
			
			$sql_cond = " AND FIND_IN_SET(p.sourceHostId, ?)";
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
                                (SELECT count(*) FROM srcLogicalVolumeDestLogicalVolume WHERE sourceHostId = p.sourceHostId) as pairCount
                           FROM 
                                srcLogicalVolumeDestLogicalVolume p
                           WHERE
                                p.destinationHostId != '5C1DAEF0-9386-44a5-B92A-31FE2C79236A' AND
                                ((p.isResync = '0' AND p.isQuasiflag = '2') OR 
                                restartResyncCounter > 0)".
							$sql_cond;
        $pairs_details = $this->conn->sql_query($get_pairs_sql, $sql_args);
		
		if(empty($pairs_details)) return array();
        $protection_details = array();
        $target_hosts = array();
		
        foreach($pairs_details as $pair)
        {
			if(strtolower($level) == "plan")
            $protection_details[$pair['planId']][$pair['sourceHostId']][] = $pair;
			else
            $protection_details[$pair['sourceHostId']][] = $pair;
        }
        return $protection_details;
    }
	
	public function get_plan_id($plan_name)
    {
        return $this->conn->sql_get_value("applicationPlan", "planName", "planName = ?", array($plan_name));
    }
    
    public function validate_protection_plan($plan_id, $flag=0)
    {
        global $CLOUD_PLAN_AZURE_SOLUTION,$CLOUD_PLAN_VMWARE_SOLUTION;
		$condition = ($flag) ? "(solutionType = '$CLOUD_PLAN_AZURE_SOLUTION' OR solutionType = '$CLOUD_PLAN_VMWARE_SOLUTION' OR solutionType = 'ESX')" : "planType = 'CLOUD'";		
        return $this->conn->sql_get_value("applicationPlan", "planName", "planId = ? AND $condition", array($plan_id));
    }
    
    public function validate_migration_plan($plan_id)
    {
         global $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
        return $this->conn->sql_get_value("applicationPlan", "planName", "planId = ? AND planType = ? AND (solutionType = ? OR solutionType = ?)", array($plan_id,'CLOUD_MIGRATION',$CLOUD_PLAN_AZURE_SOLUTION,$CLOUD_PLAN_VMWARE_SOLUTION));
    }
    
    public function get_common_tag_across_pairs($pair_details, $data_type_to_send=0, $tag_query_data=NULL)
    {
        $host_tag_details = array();
        $common_tag_details = array();
		$tag_counter = 0;
        foreach($pair_details as $src_host_id => $pairs)
        {
			$server_tag_details = array();
			$pair_counter = 0;		
            foreach ($pairs as $pair)
            {
                $destId = $pair['destinationHostId'];                
                $ruleId = $pair['ruleId']."||".$pair['pairId'];
                $destDeviceName = $pair['destinationDeviceName'];
                
				$all = array();
				if($tag_query_data)	$all = $tag_query_data;
				
                $all = array_merge($all, array('radSC'=>0,'hidRuleId'=>$ruleId,'hidsort'=>'paddedTagTimeStamp'));
                if($pair_counter == 0)
                {
                    $server_tag_details = $this->get_search_tag($all,NULL,$destId,$destDeviceName);
					if($tag_counter == 0) $common_tag_details = $server_tag_details;
                }
                else
                {				
                    $common_tag_details = $this->get_common_tag($common_tag_details,$destId,$destDeviceName);
                    $server_tag_details = $this->get_common_tag($server_tag_details,$destId,$destDeviceName);
                }
				$pair_counter++;				
            }
			if(count($server_tag_details)) $host_tag_details[$src_host_id] = $server_tag_details;
			$tag_counter = 1;
        }
        if(!$data_type_to_send) return $host_tag_details;
        elseif($data_type_to_send == 1) return $common_tag_details;
        else return array($common_tag_details, $host_tag_details);
    }	
    
    public function insert_rollback_plan($planName, $migration_plan_id, $solution_type)
    {
		global $log_rollback_string;
        if(empty($migration_plan_id) == false)
		{
			$sql = "UPDATE applicationPlan SET planName = ? , applicationPlanStatus = 'Readiness Check Pending', errorMessage = '' WHERE planId = ? ";
			$updateStatus = $this->conn->sql_query($sql, array($planName, $migration_plan_id));
			$log_rollback_string = $log_rollback_string."updateStatus = $updateStatus,migration_plan_id=$migration_plan_id ".",";			
			return $migration_plan_id;
		}
		else
		{
			$sql = "INSERT INTO applicationPlan
							(planName, planType, solutionType, applicationPlanStatus, planCreationTime)
					VALUES
							(?, 'CLOUD_MIGRATION', '$solution_type' , 'Readiness Check Pending', UNIX_TIMESTAMP(now()))";
			$planId = $this->conn->sql_query($sql, array($planName));  
			$log_rollback_string = $log_rollback_string."sql = $sql,planId=$planId ".",";			
			return $planId;
		}
    }
    
    function build_scenario_host_details($details)
    {
        $source = $details['SRCHOST'];
        $target =  $details['TGTHOST'];

        $protection_scenarioId = $this->conn->sql_get_value("applicationScenario", "scenarioId", "planId = ? AND `sourceId` LIKE ? AND `targetId` LIKE ?", array($details['PLANID'], "%$source%", "%$target%"));

        $source_hostname = $this->conn->sql_get_value("hosts", "name", "id = ?", array($source));
        $target_hostname = $this->conn->sql_get_value("hosts", "name", "id = ?", array($target));
        
        if($details['RECOVERYPOINT'] == "LatestTag" || $details['RECOVERYPOINT'] == "MultiVMLatestTag")
        {
           $tag_name = "LATEST";
           $tag_type = "FS";
        }
        elseif($details['RECOVERYPOINT'] == "LatestTime")
        {
           $tag_name = "LATESTTIME";
           $tag_type = "FS";
        }
        elseif($details['RECOVERYPOINT'] == "Custom")
        {
           $tag_name = $details['RECOVERY_CUSTOM_TAG'];
           $tag_type = "FS";
        }

        $scenario_hostDetails = array(
                                'tagName'               => $tag_name,
                                'tagType'               => $tag_type,
                                'sourcehostId'          => $source,
                                'sourceHostname'        => $source_hostname,
                                'targethostId'          => $target,
                                'targetHostName'        => $target_hostname,
                                'protectionScenarioId'  => $protection_scenarioId,
								'protectionPlanId'		=> $details['PLANID'],
                            );        
        return $scenario_hostDetails;
    }
    
    function create_migration_scenario($planId, $scenarioDetails, $migrationScenarioId,$solution_type)
    {
        $csId       = $scenarioDetails['CSID'];
        $srcHost    = $scenarioDetails['SRCHOST'];
        $tgtHost    = $scenarioDetails['TGTHOST'];
        $details    = serialize($scenarioDetails['DETAILS']);
        $protection_planid = $scenarioDetails['PROTECTIONID'];
        $protection_scenario_id = $scenarioDetails['PROTECTIONSCENARIOID'];
        $exe_status = $scenarioDetails['STATUS'];
        $solution_type = ucfirst(strtolower($solution_type));
		$sql = "INSERT INTO
						applicationScenario
						(
							scenarioType,
							scenarioCreationDate,
							planId,
							executionStatus,
							applicationType,
							sourceId,
							targetId,
                            scenarioDetails,
							referenceId
						)
						VALUES
						(
							'Rollback',
							now(),
							?,
							?,
							'$solution_type',
							?,
							?,
                            ?,
							?
						)";
		$scenarioId = $this->conn->sql_query($sql, array($planId, $exe_status, $srcHost, $tgtHost, $details, $protection_scenario_id));
		return $scenarioId;
    }
    
    public function get_migration_scenario_id($plan_id, $source, $target)
    {
        return $this->conn->sql_get_value("applicationScenario", "scenarioId", "planId = ? AND sourceId = ? AND targetId = ?", array($plan_id, $source, $target));
    }    
    
    public function insert_rollback_policy($rollback_plan_id, $target_ids)
    {        
        $rollback_policy_type = '48';
        $rollback_policy_name = 'Rollback';
		$policy_id_arr = array();
        
        foreach($target_ids as $target)
        {
            $policy_sql = "INSERT INTO 
                            policy
                                (policyType,
                                policyName,
                                policyParameters,
                                policyScheduledType,		
                                policyRunEvery,
                                nextScheduledTime,
                                policyExcludeFrom,
                                policyExcludeTo,
                                applicationInstanceId,
                                hostId,	
                                scenarioId,
                                dependentId,
                                policyTimestamp,
                                planId) 
                        VALUES
                                (?,
                                ?,
                                '0',
                                2,
                                0,
                                0,
                                0,
                                0,
                                0,
                                ?,
                                0,
                                0,
                                now(),
                                ?)";
            $policy_id = $this->conn->sql_query($policy_sql, array($rollback_policy_type, $rollback_policy_name, $target, $rollback_plan_id));
            $policy_id_arr[$target] = $policy_id;
            $policy_instance_id_sql = "INSERT INTO 
                            policyInstance
                                (policyId,
                                lastUpdatedTime,
                                policyState	,
                                executionLog,
                                outputLog,
                                policyStatus,
                                uniqueId,
                                dependentInstanceId,
                                hostId) 
                            VALUES
                                (?,
                                now(),
                                3,
                                '',
                                '',
                                'Active',
                                0,
                                0,
                                ?)"; 
            $policy_instance_id = $this->conn->sql_query($policy_instance_id_sql, array($policy_id, $target));
        }
		return $policy_id_arr;
    }
    
    public function create_rollback_plan($vm_migration_details, $solution_type, $edit_plan_id=NULL, $plan_name=NULL)
	{
		global $log_rollback_string;
        $plan_name = ($plan_name) ? $plan_name : 'RollbackPlan_'.time();

        $migration_planid = $this->insert_rollback_plan($plan_name, $edit_plan_id, $solution_type);			

        $target_ids = array();
		$scenario_id_arr = $plan_id_arr = array();
		
        foreach ($vm_migration_details as $source => $migrationOptons_obj)    //$source
        {
            $migrationScenarioId = $this->get_migration_scenario_id($migration_plan_id, $source, $target);
            $target     = $migrationOptons_obj['targethostid'];
            $protection_planid = $migrationOptons_obj['planid'];
            $protection_scenario_id = $migrationOptons_obj['scenarioId'];
            $recovery_custom_tag = $migrationOptons_obj['tagname'];
            $recovery_point = $migrationOptons_obj['recoveryoption'];
            $execution_status = $migrationOptons_obj['executionstatus'];

            $scenario_hostDetails = $this->build_scenario_host_details(array(
                                                                            'PLANID'        => $protection_planid,
                                                                            'SRCHOST'       => $source,
                                                                            'TGTHOST'       => $target,
                                                                            'RECOVERYPOINT' => $recovery_point,
                                                                            'RECOVERY_CUSTOM_TAG' => $recovery_custom_tag
                                                                            ));
            
            $scenarioDetails = array(
                                        'SRCHOST'   => $source,
                                        'TGTHOST'   => $target,
                                        'PROTECTIONID' => $protection_planid,
                                        'PROTECTIONSCENARIOID' => $protection_scenario_id,
                                        'DETAILS'   => array('host_Details'  => $scenario_hostDetails),
                                        'STATUS'   => $execution_status
                                    );
		
			$scenario_hostDetails_log = serialize($scenario_hostDetails);
			$scenarioDetails_log = serialize($scenarioDetails);			
			$log_rollback_string = $log_rollback_string."scenario_hostDetails_log = $scenario_hostDetails_log, scenarioDetails_log = $scenarioDetails".",";	
			
            $scenarioId = $this->create_migration_scenario($migration_planid, $scenarioDetails, $migrationScenarioId, $solution_type);
			$log_rollback_string = $log_rollback_string."scenarioId = $scenarioId".",";	
            $target_ids[] = $target;
			$scenario_id_arr[$source] = $scenarioId;
			$plan_id_arr[$source] = $migration_planid;
        }
		
        $target_ids = array_unique($target_ids);
		$target_ids_log = serialize($target_ids);
		$log_rollback_string = $log_rollback_string."scenario_hostDetails_log = $scenario_hostDetails_log, scenarioDetails_log = $scenarioDetails".",";	
        $policy_id_arr = $this->insert_rollback_policy($migration_planid, $target_ids);		
		$rollback_data = array("planId" => $plan_id_arr, "scenarioId" => $scenario_id_arr, "policyId" => $policy_id_arr);
		$rollback_data_log = serialize($plan_id_arr);
		$log_rollback_string = $log_rollback_string."rollback_data = $rollback_data_log".",";	
		return $rollback_data;
	}
    
    public function check_rollback_exists($src_host_ids)
    {
        $src_host_ids_str = implode(",", $src_host_ids);
        // ROLLBACK_INPROGRESS
        return $this->conn->sql_get_value("applicationScenario", "scenarioId", "scenarioType = 'Rollback' AND (FIND_IN_SET(sourceId, ?) OR sourceId LIKE ?) AND executionStatus IN ('116')", array($src_host_ids_str, "%src_host_ids_str%"));        
    }
    
    public function retry_rollback($recovery_plan_id)
    {
        global $CLOUD_PLAN_AZURE_SOLUTION, $CLOUD_PLAN_VMWARE_SOLUTION;
		global $log_rollback_string;
        
        $azure_sol_type = ucfirst(strtolower($CLOUD_PLAN_AZURE_SOLUTION));
        $vmware_sol_type = ucfirst(strtolower($CLOUD_PLAN_VMWARE_SOLUTION));
        $get_all_scenarios_sql = "SELECT * FROM applicationScenario WHERE planId = ? AND scenarioType = 'Rollback' AND (applicationType = '$azure_sol_type' OR applicationType = '$vmware_sol_type') AND executionStatus IN ('118')";
        $rollback_scenarios = $this->conn->sql_query($get_all_scenarios_sql, array($recovery_plan_id));
		
		$rollback_scenarios_log = serialize($rollback_scenarios);
		$log_rollback_string = $log_rollback_string."Rollback Scenarios: SQL = $get_all_scenarios_sql, ".$rollback_scenarios_log.",";
		
		$sql_policy = "SELECT b.policyInstanceId AS policyInstanceId, a.policyId as policyId FROM policy a, policyInstance b WHERE a.planId = ? AND a.policyId = b.policyId AND b.policyState = '2' AND a.policyType = '48'";
		$policy_details = $this->conn->sql_query($sql_policy, array($recovery_plan_id));
		
		$policy_details_log = serialize($policy_details);
		$log_rollback_string = $log_rollback_string."Policy details: SQL = $sql_policy, ".$policy_details_log.",";
		
		$scenario_failure_exists = 0;
		
		$scenario_id_arr = array();
		$policy_id_arr = array();
		foreach ($rollback_scenarios as $scenario)
        {
            $id = $scenario['scenarioId'];
            $scenario_status = $scenario['executionStatus'];
			$scenario_id_arr[$scenario['sourceId']] = $id;
			$policy_id_arr[$scenario['targetId']] = $policy_details[0]['policyId'];
            ##
            # Retry Scenario from RX if Rollback failed
            #   ROLLBACK_FAILED
            ##
            if($scenario_status == '118') 
			{               
			    if($policy_details[0]['policyInstanceId']) 
				{
                    $scenario_failure_exists = 1;
					$update_scenario = "UPDATE applicationScenario SET executionStatus = '116' WHERE scenarioId = ?";
                    $this->conn->sql_query($update_scenario, array($id));
					
					$log_rollback_string = $log_rollback_string."SQL = $update_scenario,id=$id".",";
                
                    $planStatus = 'Rollback InProgress';
                    $update_plan = "UPDATE applicationPlan SET applicationPlanStatus = ? WHERE planId = ?";
                    $this->conn->sql_query($update_plan, array($planStatus, $recovery_plan_id));
					
					$log_rollback_string = $log_rollback_string."SQL = $update_plan, planStatus = $planStatus, recovery_plan_id =$recovery_plan_id ".",";
                }
            }
        }
		
		if($scenario_failure_exists == 1)
		{
			$update_plan = "UPDATE policy SET policyTimestamp = now() WHERE planId = ?";
			$this->conn->sql_query($update_plan, array($recovery_plan_id));
			
			
			$log_rollback_string = $log_rollback_string."SQL = $update_plan, recovery_plan_id =$recovery_plan_id ".",";
			
			$policyInstanceId = $policy_details[0]['policyInstanceId'];
			$update_policy = "UPDATE policyInstance SET policyState = '3' WHERE policyInstanceId = ?";
			$this->conn->sql_query($update_policy, array($policyInstanceId));
			$log_rollback_string = $log_rollback_string."SQL = $update_policy, policyInstanceId =$policyInstanceId ".",";
		}
		$rollback_data = array("planId" => $recovery_plan_id, "scenarioId" => $scenario_id_arr, "policyId" => $policy_id_arr);
		$rollback_data_log = serialize($rollback_data);
		$log_rollback_string = $log_rollback_string."$rollback_data_log".",";
        return $rollback_data;
    }
    
    public function getRollbackState($recovery_plan_id=NULL, $servers=NULL)
    {
        $sql_args = $source_status_arr = array();
        
        if($recovery_plan_id)
        {
            $sql_cond = " AND p.planId = ?";
            $sql_args[] = $recovery_plan_id;
        }
        
        if($servers)
        {
            $server_ids = array();
            if(!is_array($servers)) $server_ids[] = $servers;
            else $server_ids = $servers;            
            $server_ids_str = implode(",", $server_ids);
            
            $sql_cond = " AND FIND_IN_SET(sc.sourceId, ?)";
            $sql_args[] = $server_ids_str;
        }

        //$get_scenarios_sql = "SELECT sourceId,executionStatus FROM applicationScenario WHERE scenarioType='Rollback' $sql_cond";
  
		$get_scenarios_sql = "SELECT 
                                sc.sourceId as sourceId,
                                pi.executionLog as errorLog,
								sc.executionStatus as executionStatus
                              FROM 
                                applicationScenario sc,
                                applicationPlan ap,
                                policy p,
                                policyInstance pi
                              WHERE 
                                sc.planId = ap.planId AND
                                p.planId = ap.planId AND
                                p.policyId = pi.policyId AND
								sc.scenarioType = 'Rollback' $sql_cond";
        
		$policy_instance_arr = $this->conn->sql_query($get_scenarios_sql, $sql_args);
		foreach ($policy_instance_arr as $details_arr)
		{
			$source_host_id = $details_arr['sourceId'];
			$tmp_source_log_arr = $details_arr['errorLog'] ? unserialize($details_arr['errorLog']) : array();
			$source_status_arr[$source_host_id]['executionStatus'] = $details_arr['executionStatus'];
			
			if (! empty($tmp_source_log_arr))
			{
				if (! empty($tmp_source_log_arr[3]))
				{
					foreach ($tmp_source_log_arr[3] as $source_host_id => $details)
					{
						$source_status_arr[$source_host_id]['recovery_status'] = $details[2]['RecoveryStatus'];
						$source_status_arr[$source_host_id]['Tag'] = $details[2]['Tag'];
						$source_status_arr[$source_host_id]['error_code'] = isset($details[2]['ErrorCode']) ? $details[2]['ErrorCode'] : '';
						$source_status_arr[$source_host_id]['error_message'] = isset($details[2]['ErrorMessage']) ? $details[2]['ErrorMessage'] : '';
						$source_status_arr[$source_host_id]['error_place_holder'] = empty($details[3]['PlaceHolder']) ? array() : $details[3]['PlaceHolder'][2];
					}
				}
				else if (! empty($tmp_source_log_arr[2]))
				{
					$source_status_arr[$source_host_id]['recovery_status'] = $tmp_source_log_arr[2]['RecoveryStatus'];
					$source_status_arr[$source_host_id]['Tag'] = $tmp_source_log_arr[2]['Tag'];
					$source_status_arr[$source_host_id]['error_code'] = isset($tmp_source_log_arr[2]['ErrorCode']) ? $tmp_source_log_arr[2]['ErrorCode'] : '';
					$source_status_arr[$source_host_id]['error_message'] = isset($tmp_source_log_arr[2]['ErrorMessage']) ? $tmp_source_log_arr[2]['ErrorMessage'] : '';
					$source_status_arr[$source_host_id]['error_place_holder'] = empty($tmp_source_log_arr[4]['PlaceHolder']) ? array() : $tmp_source_log_arr[4]['PlaceHolder'][2];
				}
			}
		}
		return $source_status_arr;
    }
    
    public function get_rollback_server_details($servers)
    {
        $server_ids = array();
        if(!is_array($servers)) $server_ids[] = $servers;
        else $server_ids = $servers;
        
        $server_ids_str = implode(",", $server_ids);
        
        $validate_servers_sql = "SELECT
                                    sourceId,
                                    planId
                                 FROM
                                    applicationScenario
                                 WHERE
                                    scenarioType = 'Rollback' AND
                                    FIND_IN_SET(sourceId, ?)";
        $scenario_details = $this->conn->sql_get_array($validate_servers_sql, "sourceId", "sourceId", array($server_ids_str));
        $no_rollback_source_ids = array();
        foreach($server_ids as $server)
        {
            if(!array_key_exists($server, $scenario_details)) $no_rollback_source_ids[] = $server;
        }
        $no_rollback_source_ids_str = implode(", ", $no_rollback_source_ids);
        
        return $no_rollback_source_ids_str;
    }
	
	public function getProtectedSourceHostId($plan_id)
	{
		$sql = "SELECT sourceId FROM applicationScenario WHERE planId = ?";
		$source_id_list = $this->conn->sql_get_array($sql, "sourceId", "sourceId", array($plan_id));
		$source_id = array_keys($source_id_list);
		return $source_id;
	}
	
	public function getTagTimeStamp($tag_name)
	{
		$tag_time = $this->conn->sql_get_value("retentionTag", "concat_ws(' ', REPLACE(SUBSTRING_INDEX(tagTimeStamp, ',', 3), ',', '/'), REPLACE(substr(tagTimeStamp FROM 12 FOR 8), ',', ':'))", "userTag = ?", array($tag_name));
		return $tag_time;
	}
	
	public function check_common_tag_exists($pair_details, $recovery_type, $custom_tag=NULL)
	{
		if($recovery_type == "LatestTime" || ($recovery_type == "Custom" && preg_match("/:/",$custom_tag)))
		{
			$pair_ids = array();
			foreach($pair_details as $host_id => $details)
			{
				foreach($details as $pair)
				{
					$pair_ids[] = $pair["pairId"];
				}
			}
			if($custom_tag)
			{
				$time = (strtotime($custom_tag)+11644473600)*10000000;
				$common_recovery_details = $this->check_specified_time_exists($pair_ids, $time);
				if(!$common_recovery_details) return array(0, $custom_tag);
			}
			else
			{
				$common_recovery_details = $this->check_common_time_exists($pair_ids, 1);
				if(!$common_recovery_details) return array(0, 'Common Consistency Time');	
			}
		}
		else
		{
			$common_recovery_details = $this->get_common_tag_across_pairs($pair_details, 1);
			if(!$common_recovery_details) return array(0, 'Common Consistency Tag');
			
			if($recovery_type == "Custom")
			{
				$common_tag_flag = False;
				foreach($common_recovery_details as $common_tag_details)
				{								
					if($common_tag_details[2] == $custom_tag)
					{
						$common_tag_flag = True;
						break;
					}
				}
				if(!$common_tag_flag) return array(0, $custom_tag);
			}
		}
		
		return array(1, $common_recovery_details);
	}
}
?>