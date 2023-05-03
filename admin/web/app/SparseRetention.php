<?php
/*
 *================================================================= 
 * FILENAME
 * SparseRetention.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    Sparse Retention
 *
 * HISTORY
 *     *     17-Feb-2008  Shrinivas  
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/ 

Class SparseRetention
{
    private $conn;
    private $commonfunctions_obj;
    function __construct()
    {
		global $conn_obj;
        $this->commonfunctions_obj = new CommonFunctions();
        $this->conn = $conn_obj;
    }
    /*
        * Function Name: insert_retLogPolicy
        *
        * Description: 
        *    This function insert the retention time windows in database table  
        *
        * Parameters:
        *     Param 1  [IN]:$id                                 Source Host Id
        *     Param 2  [IN]:$deviceName                   Source Device Name 
        *     Param 3  [IN]:$destId                         Target Host Id
        *     Param 4  [IN]:$destDeviceName            Target Device Name
        *     Param 5  [IN]:$retention_time_windows Array with all window details
        *     Param 6  [IN]:$log_management             Array for log policy of windows
        *
        * Return Value:
        *     Ret Value: 
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function insert_timewindow($destId,$destDeviceName,$retention_time_windows,$log_management,$vol_obj=0)
    {
        global $volume_obj,$logging_obj;
				
		$logging_obj->my_error_handler("DEBUG","insert_timewindow called ");
		$logging_obj->my_error_handler("DEBUG","destId : $destId :: destDeviceName : $destDeviceName ");
		if($vol_obj)
		{
			$volume_obj = $vol_obj;
		}
        $logDeviceName ="";
        /*Path validations*/
        $destDeviceName = $volume_obj->get_unformat_dev_name($destId,$destDeviceName);
		
        $destDeviceName = $this->commonfunctions_obj->verifyDatalogPath($destDeviceName);
        $destDeviceName = $this->conn->sql_escape_string($destDeviceName);
        /*Get the retention ID for a pair*/
        $result_set = $this->get_retentionId($destId,$destDeviceName);
		
        /*Loop the retention ID as for to satisfy cluster case*/
        while( $row = $this->conn->sql_fetch_object($result_set))
        {
            /*Insert data for each window*/
            $endTime = 0;
			$values = '';
            for($count=1;$count<=count($retention_time_windows);$count++)
            {
                /*Collecting all time parameters*/
                
                /*Converting the times into hours*/
                $time_granularity_hrs = $this->convertToHours($retention_time_windows[$count]['granularity'],$retention_time_windows[$count]['granularity_unit']);
                $time_interval_hrs    = $this->convertToHours($retention_time_windows[$count]['time_interval'],$retention_time_windows[$count]['time_interval_unit']);
                /*Start time is always  zero for first window*/
                /*Start time of next winodw is end time of previous window */
                if($count == 1)
                {
                    $startTime = 0;
                }
                else
                {
                    $startTime = $endTime;
                }
                $retention_window_id = $retention_time_windows[$count]['windowId'];
                $endTime = ($startTime + $time_interval_hrs );
                $time_interval_unit = $retention_time_windows[$count]['time_interval_unit'];
                $granularity = $time_granularity_hrs;
                $granularity_unit=$retention_time_windows[$count]['granularity_unit'];
                
				if($values)
				{
					$values = $values ." , ('$retention_window_id','$row->retId','$startTime','$endTime','$granularity',
								 '$time_interval_unit','$granularity_unit')";
				}
				else
				{
					$values = "('$retention_window_id','$row->retId','$startTime','$endTime','$granularity',
								 '$time_interval_unit','$granularity_unit')";
				}
			}
			if($values)
			{
				$insert_sql =   "INSERT 
                                    INTO
                                retentionWindow
                                    (retentionWindowId,
                                    retId,
                                    startTime,
                                    endTime,
                                    timeGranularity,
                                    timeIntervalUnit,
                                    granularityUnit
                                    )
                                    VALUES $values";
				$logging_obj->my_error_handler("DEBUG","insert_sql_in_time_window : $insert_sql ");
                $exec = $this->conn->sql_query($insert_sql);
			}
            /*Get the retentionWindowId of parent table
                               Adding the entries in retentionEventPolicy for each window 
                           */
            $rs = $this->get_windowId($row->retId);
			
            $i=1;
			$ret_event_policy_values = '';
            while($row = $this->conn->sql_fetch_object($rs))
            {
                /*Storage path device name*/
                $drive1 = $this->commonfunctions_obj->verifyDatalogPath($log_management[$i]['storage_device_name']);        
                $drive2 = $this->commonfunctions_obj->verifyDatalogPath($log_management[$i]['storage_device_name']);
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
                /*replace the encoded special character with space in application name*/
                /*application name is consistency tag name*/
                $applicationName = str_replace("%20"," ",$retention_time_windows[$i]['application_name']);
                $tagCount = $retention_time_windows[$i]['tag_count'];
                $user_defined_tag = $retention_time_windows[$i]['user_defined_tag'];
                $storagePrunning = $log_management[$i]['on_insufficient_disk_space'];
                $storagePath = $log_management[$i]['storage_path'];
				$storagePath  = $this->commonfunctions_obj->verifyDatalogPath($storagePath);
                $storagePath  = $this->conn->sql_escape_string($storagePath );
                $alertThreshold = $log_management[$i]['alert_threshold'];
                
                $catalogPath =  $log_management[$i]['retentionPath'];
                $catalogPath = $this->commonfunctions_obj->verifyDatalogPath($catalogPath);
                $catalogPath = $this->conn->sql_escape_string($catalogPath);
                
				
				if($ret_event_policy_values)
				{
					$ret_event_policy_values = $ret_event_policy_values . " , ('$row->Id','$row->retentionWindowId','$storagePath',
												 '$logDeviceName','$storagePrunning','$applicationName',
												 '$alertThreshold','$tagCount','$user_defined_tag',
												 '$catalogPath')";
				}
				else
				{
					$ret_event_policy_values = "('$row->Id','$row->retentionWindowId','$storagePath',
												 '$logDeviceName','$storagePrunning','$applicationName',
												 '$alertThreshold','$tagCount','$user_defined_tag',
												 '$catalogPath')";
				}
                $i++;
            }
			if($ret_event_policy_values)
			{
				/*Insert sql to add the retentionEventWindow*/
                $insert_sql =   "INSERT 
                                INTO
                                retentionEventPolicy
                                    (   Id,
                                        retentionWindowId,
                                        storagePath,
                                        storageDeviceName,
                                        storagePruningPolicy,
                                        consistencyTag,
                                        alertThreshold,
                                        tagCount,
                                        userDefinedTag,
                                        catalogPath
                                    )
                                    VALUES $ret_event_policy_values";
				$logging_obj->my_error_handler("DEBUG","insert_sql_in_retentionEventPolicy : $insert_sql ");
                $exec = $this->conn->sql_query($insert_sql);                
			}
        }
	}
    
    /*
        * Function Name:update_timewindow
        *
        * Description: 
        *    This function update  the retention time windows in database table  on edit of mediaretention policy 
        *
        * Parameters:
        *     Param 1  [IN]:$destId                          Target host id 
        *     Param 2  [IN]:$destDeviceName            Target Device name
        *     Param 3  [IN]:$retention_time_windows Array with all window details
        *     Param 4  [IN]:$log_management             Array for log policy of windows
        *     Param 5  [IN]:$deleteWindow                Window Ids to be deleted
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function update_timewindow($destId,$destDeviceName,$retention_time_windows,$log_management,$deleteWindow)
    {
        global  $volume_obj,$logging_obj;
		
		
		$logging_obj->my_error_handler("INFO","update_timewindow called ");
		$logging_obj->my_error_handler("INFO"," update_timewindow destId : $destId :: destDeviceName : $destDeviceName ");
        
        $windowIdArray = array();
        $deleteWindow_flag = 0;
        $logDeviceName ="";
        /*Path validations*/
        $destDeviceName = $volume_obj->get_unformat_dev_name($destId,$destDeviceName);
        $destDeviceName = $this->commonfunctions_obj->verifyDatalogPath($destDeviceName);
        $destDeviceName = $this->conn->sql_escape_string($destDeviceName);
        /*Get the retention ID for a pair*/
        $result_set = $this->get_retentionId($destId,$destDeviceName);
        /*Remove ',' at start & end of string*/
        $deleteWindow = trim($deleteWindow, ",");
        /*Loop the retention ID as to satisfy cluster case*/
        while( $row = $this->conn->sql_fetch_object($result_set))
        {
            $endTime = 0;
            /*Get the policy type*/
            $policy_type = $this->get_policytype($row->retId);
            /*policy_type = 0 --Space
                              policy_type = 1 --Time
                              policy_type = 2 --Composite
                           */
            if($policy_type == 1)
            {
                /*Remove space policy details*/
                $this->remove_spacePolicy($row->retId);
            }
            /*Check windows to be deleted*/
            if(($deleteWindow !="") ||($deleteWindow_flag != 0))
            {
                $deleteWindow_flag == 1;
                /*Remove time windows*/
                $delete_sql ="DELETE 
                            FROM
                                retentionWindow
                            WHERE 
                                retentionWindowId IN($deleteWindow)
                                AND 
                                retId = '$row->retId'";
                $delete_exec = $this->conn->sql_query($delete_sql);
            }
            $rs = $this->get_windowId($row->retId);
            $id_count=0;
            $strId="";
            while($id_row = $this->conn->sql_fetch_array($rs))
            {
                if ($id_count == 0)
                {
                    $strId = $id_row['Id'];  
                }
                else
                {
                    $strId = $strId.",".$id_row['Id'];
                }
                $id_count++;
            }
            /*Loop through each window*/
            for($count=1;$count<=count($retention_time_windows);$count++)
            {
                /*Collecting all time parameters*/
                /*Converting the times into hours*/
                $time_granularity_hrs = $this->convertToHours($retention_time_windows[$count]['granularity'],$retention_time_windows[$count]['granularity_unit']);
                $time_interval_hrs = $this->convertToHours($retention_time_windows[$count]['time_interval'],$retention_time_windows[$count]['time_interval_unit']);
                /*Start time is always  zero for first window*/
                /*Start time of next winodw is end time of previous window */
                if($count == 1)
                {
                    $startTime = 0;
                }
                else
                {
                    $startTime = $endTime;
                }
                $endTime = ($startTime + $time_interval_hrs);
                $time_interval_unit = $retention_time_windows[$count]['time_interval_unit'];
                $granularity = $time_granularity_hrs;
                $granularity_unit=$retention_time_windows[$count]['granularity_unit'];
                $window_id =$retention_time_windows[$count]['windowId'];
                $num_rows = 0;
                $window_sql = "SELECT 
                                    Id 
                                FROM
                                    retentionWindow 
                                WHERE
                                    retentionWindowId ='$window_id'
                                AND
                                    retId = '$row->retId'";
                $window_row = $this->conn->sql_query($window_sql);
                $num_rows = $this->conn->sql_num_row($window_row);
                /*window id will be blank for new windows*/
                if($num_rows == 0)
                {
                    /*Insert the new window*/
                    $update_sql =   "INSERT 
                                    INTO
                                    retentionWindow
                                        (
                                        retentionWindowId,
                                        retId,
                                        startTime,
                                        endTime,
                                        timeGranularity,
                                        timeIntervalUnit,
                                        granularityUnit
                                        )
                                    VALUES
                                        (
                                        '$window_id',
                                        '$row->retId',
                                        '$startTime',
                                        '$endTime',
                                        '$granularity',
                                        '$time_interval_unit',
                                        '$granularity_unit'
                                        )";
					$logging_obj->my_error_handler("INFO","update_timewindow called update_sql::$update_sql");					
										
                }
               
                else
                {
                    /*Update the existing window*/
                    $update_sql =   "UPDATE
                                        retentionWindow
                                    SET
                                        startTime        ='$startTime',
                                        endTime          ='$endTime',
                                        timeGranularity  ='$granularity',
                                        timeIntervalUnit ='$time_interval_unit',
                                        granularityUnit  ='$granularity_unit'
                                    WHERE 
                                        retentionWindowId ='$window_id'
                                    AND
                                        retId = '$row->retId'";
				$logging_obj->my_error_handler("INFO","update_timewindow called update_sql::$update_sql");					
                }
                $exec = $this->conn->sql_query($update_sql);
            }
            /*Loop through each window for retention logging Management*/
            for($count=1;$count<=count($log_management);$count++)
            {
                /*Path Validations for storage path device name valildations */
                $drive1 = $this->commonfunctions_obj->verifyDatalogPath($log_management[$count]['storage_device_name']);        
                $drive2 = $this->commonfunctions_obj->verifyDatalogPath($log_management[$count]['storage_device_name']);
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
                /*replace the encoded special character with space in application name*/
                /*application name is consistency tag name*/
                $applicationName = str_replace("%20"," ",$log_management[$count]['application_name']);
                $tagCount = $log_management[$count]['tag_count'];
                $userDefined =$log_management[$count]['user_defined_tag'];
                $storagePrunning = $log_management[$count]['on_insufficient_disk_space'];
                $storagePath     = $log_management[$count]['storage_path'];
                
                $storagePath = $this->commonfunctions_obj->verifyDatalogPath($storagePath);
                $storagePath = $this->conn->sql_escape_string($storagePath);
                
                $moveRetentionPath = $log_management[$count]['move_retention_path'];
                if($moveRetentionPath == "")
                {
                    $moveRetentionPath = 'NULL';
                }
                else
                {
                    $moveRetentionPath = $this->commonfunctions_obj->verifyDatalogPath($moveRetentionPath);
                    $moveRetentionPath = $this->conn->sql_escape_string($moveRetentionPath);
                }
                $alertThreshold = $log_management[$count]['alert_threshold'];
                
                $catalog_path = $log_management[$count]['retention_path'];
                $catalog_path = $this->commonfunctions_obj->verifyDatalogPath($catalog_path);
                $catalog_path =$this->conn->sql_escape_string($catalog_path);
                
                $window_id =$log_management[$count]['windowId'];
                $window_sql = "SELECT 
                                    retentionEventPolicyId 
                                FROM
                                    retentionEventPolicy 
                                WHERE
                                    retentionWindowId ='$window_id'
                                AND
                                    Id IN ($strId)";
                $window_row = $this->conn->sql_query($window_sql);
                $num_rows = $this->conn->sql_num_row($window_row);
                /*window id will be blank for new log windows*/
                if($num_rows == 0)
                {
          
                    $select_windowid = "SELECT
                                            ret1.Id
                                        FROM 
                                            retentionWindow ret1 
                                        WHERE
                                            ret1.Id
                                        NOT IN(SELECT Id FROM retentionEventPolicy)" ;
                    $exec_window = $this->conn->sql_query($select_windowid);                
                    $window = $this->conn->sql_fetch_row($exec_window);
                    $id = $window[0];
                    
                    /*Insert new record*/
                    $update_sql =   "INSERT 
                                    INTO
                                    retentionEventPolicy
                                    (
                                        Id,
                                        retentionWindowId,
                                        storagePath,
                                        storageDeviceName,
                                        storagePruningPolicy,
                                        consistencyTag,
                                        alertThreshold,
                                        tagCount,
                                        userDefinedTag,
                                        catalogPath,
                                        moveRetentionPath
                                    )
                                    VALUES
                                    (
                                        '$id',
                                        '$window_id',
                                        '$storagePath',
                                        '$logDeviceName',
                                        '$storagePrunning',
                                        '$applicationName',
                                        '$alertThreshold',
                                        '$tagCount',
                                        '$userDefined',
                                        '$catalog_path',
                                        '$moveRetentionPath'
                                    )";
                
                }
                else
                {
                    /*Update the existing records*/
					
					  $update_sql =   "UPDATE 
											retentionEventPolicy
										SET
											moveRetentionPath ='$moveRetentionPath',
											storagePruningPolicy ='$storagePrunning',
											consistencyTag ='$applicationName',
											alertThreshold='$alertThreshold',
											tagCount = '$tagCount',
											userDefinedTag='$userDefined'
										WHERE
											retentionWindowId ='$window_id'
										AND
											Id IN($strId)";

                }
                $exec = $this->conn->sql_query($update_sql);
            }
        }
    }   
    
    /*
        * Function Name: convertToHours
        *
        * Description: 
        *    Converts supplied time in hours
        *
        * Parameters:
        *     Param 1  [IN]:$time               Time Value
        *     Param 2  [IN]:$unit               Time Unit
        * Return Value:
        *     Ret Value: Time in hours $time
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    private function convertToHours($time,$unit)
    {
        switch($unit)
        {
            case 'day':
                $time = ($time * 24);
            break;
            case 'week':
                $time = ($time * 24 * 7);
            break;
            case 'month':
                $time = ($time * 24 * 30);
            break;
            case 'year':
                $time = ($time * 24 * 365);
            break;
            default:
                $time = ($time) ;
        }
        /*Time converted in terms of hours*/
        return ($time);
    }
    /*
        * Function Name: convertToTime
        *
        * Description: 
        *    Converts supplied  hours in time
        *
        * Parameters:
        *     Param 1  [IN]:$time               Time Value
        *     Param 2  [IN]:$unit               Time Unit
        * Return Value:
        *     Ret Value: Time in hours $time
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    private function convertToTime($time,$unit)
    {
        switch($unit)
        {
            case 'day':
                $time = (($time)/24);
            break;
            case 'week':
               $time = (($time)/(24 * 7));
            break;
            case 'month':
                $time = (($time)/ (24 * 30));
            break;
            case 'year':
                $time = (($time)/ (24 * 365));
            break;
            default:
                $time = ($time) ;
        }
        /*Time converted in days , week etc format*/
        return ($time);
    }
    
    /*
        * Function Name: insert_spacepolicy
        *
        * Description: 
        *    This function insert the retention space policy  in database table  
        *
        * Parameters:
        *     Param 1  [IN]:$destId                    Target Host Id
        *     Param 2  [IN]:$destDeviceName      Target Device Name
        *     Param 3 [IN]:$drive1                      Log device name
        *     Param 4 [IN]:,$drive2                     Log device name 
        *     Param 5 [IN]:$storageSpace            Log Storage space 
        *     Param 6 [IN]:$storagePath              Log Storage Path
        *     Param 7 [IN]:$storagePrunning         Log prunning Policy
        *     Param 8 [IN]:$alertThreshold          Alert threshold
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function insert_spacepolicy($destId,$destDeviceName,$drive1,$drive2,$storageSpace,$storagePath,$storagePrunning,$alertThreshold,$catalogPath,$moveRetentionPath="NULL",$vol_obj=0)
    {
        global  $volume_obj,$logging_obj;
				
		$logging_obj->my_error_handler("INFO","insert_spacepolicy called destId::$destId  :: destDeviceName::$destDeviceName  :: drive1::$drive1  :: drive2::$drive2  :: storageSpace::$storageSpace  :: storagePath::$storagePath :: storagePrunning::$storagePrunning  :: alertThreshold::$alertThreshold  :: catalogPath::$catalogPath");
        $logDeviceName = "";
		if($vol_obj)
		{
			$volume_obj = $vol_obj;
		}
        /*Path validation*/
        $drive1 = $this->commonfunctions_obj->verifyDatalogPath($drive1);        
        $drive2 = $this->commonfunctions_obj->verifyDatalogPath($drive2);
        $storagePath = $this->commonfunctions_obj->verifyDatalogPath($storagePath);
        $storagePath = $this->conn->sql_escape_string($storagePath);
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
        /*Path validation for destination device name*/
        $destDeviceName = $volume_obj->get_unformat_dev_name($destId,$destDeviceName);
		
        $destDeviceName = $this->commonfunctions_obj->verifyDatalogPath($destDeviceName);
        $destDeviceName = $this->conn->sql_escape_string($destDeviceName);
        
        $catalogPath =$this->commonfunctions_obj->verifyDatalogPath($catalogPath);
        $catalogPath = $this->conn->sql_escape_string($catalogPath);
        
        $moveRetentionPath =$this->commonfunctions_obj->verifyDatalogPath($moveRetentionPath);
        $moveRetentionPath =$this->conn->sql_escape_string($moveRetentionPath);
        
        /*Get the retention ID for a pair*/
        $result_set = $this->get_retentionId($destId,$destDeviceName);
		
        /*Loop the retention ID as to satisfy cluster case*/
		$values = '';
        while($row = $this->conn->sql_fetch_object($result_set))
        {
            if($values)
			{
				$values = $values . " , ('$row->retId','$storageSpace','$storagePath','$logDeviceName','$storagePrunning',
							'$alertThreshold','$catalogPath','$moveRetentionPath')";
			}
			else
			{
				$values = "('$row->retId','$storageSpace','$storagePath','$logDeviceName','$storagePrunning',
							'$alertThreshold','$catalogPath','$moveRetentionPath')";
			}
		}
		if($values)
		{
			/*Insert SQL for space policy*/
            $insert_sql =   "INSERT
                                INTO
                                retentionSpacePolicy
                                    (
                                        retId,
                                        storageSpace,
                                        storagePath,
                                        storageDeviceName,
                                        storagePruningPolicy,
                                        alertThreshold,
                                        catalogPath,
                                        moveRetentionPath                                        
                                    )
                                    VALUES $values";
			$exec = $this->conn->sql_query($insert_sql); 
			$logging_obj->my_error_handler("DEBUG","insert_spacepolicy called sql :: $insert_sql");
		}
	}
    
    /*
        * Function Name: update_spacepolicy
        *
        * Description: 
        *    This function updates the retention space policy  in database table  
        *
        * Parameters:
        *     Param 1  [IN]:$destId                    Target Host Id
        *     Param 2  [IN]:$destDeviceName      Target Device Name
        *     Param 3 [IN]:$drive1                      Log device name
        *     Param 4 [IN]:,$drive2                     Log device name
        *     Param 5 [IN]:$storageSpace            Log Storage space 
        *     Param 6 [IN]:$storagePath              Log Storage Path
        *     Param 7 [IN]:$storagePrunning        Log prunning Policy
        *     Param 8 [IN]:$alertThreshold        Alert threshold
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function update_spacepolicy($destId,$destDeviceName,$drive1,$drive2,$storageSpace,$storagePath,$moveRetentionPath,$storagePrunning,$alertThreshold,$catalogPath)
    {
        global  $volume_obj,$logging_obj;
		
		
		$logging_obj->my_error_handler("DEBUG","insert_spacepolicy called destId::$destId  :: destDeviceName::$destDeviceName  :: drive1::$drive1  :: drive2::$drive2  :: storageSpace::$storageSpace  :: storagePath::$storagePath :: storagePrunning::$storagePrunning  :: alertThreshold::$alertThreshold  :: catalogPath::$catalogPath , moveRetentionPath ::$moveRetentionPath");
        $logDeviceName = "";
        /*Path validation*/
        $drive1 = $this->commonfunctions_obj->verifyDatalogPath($drive1);        
        $drive2 = $this->commonfunctions_obj->verifyDatalogPath($drive2);
        if($moveRetentionPath == "")
        {
            $moveRetentionPath = 'NULL';
        }
        else
        {
            $moveRetentionPath = $this->commonfunctions_obj->verifyDatalogPath($moveRetentionPath);
            $moveRetentionPath = $this->conn->sql_escape_string($moveRetentionPath);
        }
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
        /*Path validations for destination device name*/
        $destDeviceName = $volume_obj->get_unformat_dev_name($destId,$destDeviceName);
        $destDeviceName = $this->commonfunctions_obj->verifyDatalogPath($destDeviceName);
        $destDeviceName = $this->conn->sql_escape_string($destDeviceName);
        /*Get the retention ID for a pair*/
        $result_set = $this->get_retentionId($destId,$destDeviceName);
        /*Loop the retention ID as to satisfy cluster case*/
        while($row = $this->conn->sql_fetch_object($result_set))
        {
            /*Get the policy type*/
            $policy_type = $this->get_policytype($row->retId);
            /*policy_type = 0 --Space
                              policy_type = 1 --Time
                              policy_type = 2 --Composite
                           */
                           
            if($policy_type == 0)
            {
                /*Remove time policy*/
                $this->remove_timePolicy($row->retId);
            }
            /*Check is the record already exists*/
            $select_sql =   "SELECT
                                retentionSpacePolicyId
                            FROM
                                retentionSpacePolicy
                            WHERE
                                retId = '$row->retId'";
            $select_result = $this->conn->sql_query($select_sql);                
            $fetch_row = $this->conn->sql_fetch_row($select_result);
            /*insert new record for space policy */
            if(count($fetch_row[0]) == 0)
            {
                $this->insert_spacepolicy($destId,$destDeviceName,$drive1,$drive2,$storageSpace,$storagePath,$storagePrunning,$alertThreshold,$catalogPath,$moveRetentionPath);
            }
            /*update the existing record for space policy */
            else
            {
                $catalogPath = $this->commonfunctions_obj->verifyDatalogPath($catalogPath);
                $catalogPath = $this->conn->sql_escape_string($catalogPath);
		
				$update_sql =   "UPDATE
										retentionSpacePolicy
									SET
										storageSpace         = '$storageSpace',
										moveRetentionPath    = '$moveRetentionPath',
										storagePruningPolicy = '$storagePrunning',
										alertThreshold       = '$alertThreshold'                                 
									WHERE
										retentionSpacePolicyId = '$fetch_row[0]'";

			
                $exec = $this->conn->sql_query($update_sql);      
$logging_obj->my_error_handler("INFO","insert_spacepolicy called update_sql::$update_sql");				
            }
        }
    }
    
    /*
        * Function Name: get_retentionId
        *
        * Description: 
        *    This function retrives  the retention Id 
        *
        * Parameters:
        *     Param 1  [IN]:$destId                       Target host id
        *     Param 2  [IN]:$destDeviceName         Target Device Name
        * Return Value:
        *     Ret Value: 
        *     Param [OUT]:result set     Retention  ID
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function get_retentionId($destId,$destDeviceName)
    {
		global $logging_obj;
        /*Sql to get the retention ID*/
		
        $sql =  "SELECT
                        ret.retId
                    FROM
                        retLogPolicy ret,
                        srcLogicalVolumeDestLogicalVolume src
                    WHERE
                        ret.ruleid = src.ruleId
                    AND
                        src.destinationHostId IN ('$destId')
                    AND
                        src.destinationDeviceName='$destDeviceName'";
						
		$logging_obj->my_error_handler("DEBUG","sql : $sql");
        $result_set = $this->conn->sql_query($sql);
		
		#12865 Fix
		return $result_set;
    }
    
   /*
        * Function Name: get_windowId
        *
        * Description: 
        *    This function retrives  the retention window Id 
        *
        * Parameters:
        *     Param 1  [IN]:$retId        Retention ID
        * Return Value:
        *     Param [OUT]:result set     Retention Window ID
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    private function get_windowId($retId)
    {
        /*Sql to get the retention Window ID for time policy*/
        $select_sql =   "SELECT 
                            Id,
                            retentionWindowId
                        FROM
                            retentionWindow
                        WHERE
                            retId IN('$retId')";
        $rs = $this->conn->sql_query($select_sql);
        return $rs;
    }
    /*
        * Function Name: get_sparse_details
        *
        * Description: 
        *    This function returns retention window details
        *
        * Parameters:
        *     Param 1  [IN]:$destHostId        
        *     Param 2  [IN]:$destDeviceName 
        * Return Value:
        *     Param [OUT]:$retention_time_windows
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function get_window_details($destHostId,$destDeviceName)
    {
        /*Array to hold the retention window time details*/
        $retention_time_windows = array();
        $strRetId="";
        /*Path validation for destination device name*/
        $destDeviceName = $this->commonfunctions_obj->verifyDatalogPath($destDeviceName);        
        $destDeviceName = $this->conn->sql_escape_string ($destDeviceName);
        /*Sql to fetch the retention ID for a pair*/
        /*collect the retention ids in string*/
        $result_set = $this->get_retentionId($destHostId,$destDeviceName);
        $i=0;
        while($row = $this->conn->sql_fetch_array($result_set))
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
        /*Select the window details*/
        $sql = "SELECT 
                    distinct startTime,
                    endTime,
                    timeGranularity,
                    timeIntervalUnit,
                    granularityUnit,
                    retentionWindowId,
					Id
                FROM 
                    retentionWindow 
                WHERE 
                    retId IN ($strRetId)
				GROUP BY
				     retentionWindowId";

        $result_set = $this->conn->sql_query($sql);  
        $count =1;
        /*Populate the details in arrays*/
        while($fetch_result = $this->conn->sql_fetch_array($result_set))
        {

            $application_name = $this->getApplicationName($fetch_result['retentionWindowId'],$fetch_result['Id']);
            $tagCount = $this->getTagCount($fetch_result['retentionWindowId'],$fetch_result['Id']);
            $userDefinedTag = $this->getUserDefinedTag($fetch_result['retentionWindowId'],$fetch_result['Id']);
            $retention_time_windows[$count]=array('retentionWindowId'=>$fetch_result['retentionWindowId'],
                                                  'application_name'=>$application_name,
                                                  'granularity'=>($fetch_result['timeGranularity']),
                                                  'granularity_unit'=>($fetch_result['granularityUnit']),
                                                  'time_interval'=>($fetch_result['endTime'] - $fetch_result['startTime']),
                                                  'time_interval_unit'=>($fetch_result['timeIntervalUnit']),
                                                  'tagCount'=>$tagCount,
                                                  'userDefinedTag'=>$userDefinedTag
                                                );
            $retention_time_windows[$count]['granularity'] = $this->convertToTime($retention_time_windows[$count]['granularity'],$retention_time_windows[$count]['granularity_unit']);
            $retention_time_windows[$count]['time_interval'] = $this->convertToTime($retention_time_windows[$count]['time_interval'],$retention_time_windows[$count]['time_interval_unit']);                                                
            $count++;
        }
        return $retention_time_windows;
    }
    /*
        * Function Name: get_logging_details
        *
        * Description: 
        *    This function returns retention window log details
        *
        * Parameters:
        *     Param 1  [IN]:$destHostId        
        *     Param 2  [IN]:$destDeviceName 
        * Return Value:
        *     Param [OUT]:$retention_time_windows
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function get_logging_details($destHostId,$destDeviceName)
    {
        /*Array to hold the retention window log details*/
        $log_management = array();
        /*Path validation for destination device name*/
        $destDeviceName = $this->commonfunctions_obj->verifyDatalogPath($destDeviceName);        
        $destDeviceName = $this->conn->sql_escape_string ($destDeviceName);
        /*Sql to fetch the retention ID for a pair*/
        $result_set = $this->get_retentionId($destHostId,$destDeviceName);
        $i=0;
        /*collect the retention ids in string*/
        while($row = $this->conn->sql_fetch_array($result_set))
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
        /*Get the policy type*/
        $policy_type = $this->get_policytype($strRetId);
        /*get the retention Window ID*/           
        $rs = $this->get_windowId($strRetId);
        $count = 1;    
        /*         policy_type = 0 --Space
                              policy_type = 1 --Time
                              policy_type = 2 --Composite
                   */
        if(($policy_type == 0)||($policy_type == 2))
        {
            $sql = "SELECT 
                        storagePath,
                        storageSpace,
                        storageDeviceName,
                        storagePruningPolicy,
                        alertThreshold,
                        moveRetentionPath,
                        catalogPath
                    FROM 
                        retentionSpacePolicy 
                    WHERE 
                        retId IN('$strRetId')";
            $result_set = $this->conn->sql_query($sql); 
            $fetch_data = $this->conn->sql_fetch_array($result_set);
            
            $log_management[$count] = array('on_insufficient_disk_space'=>$fetch_data['storagePruningPolicy'],
                                                'alert_threshold'=>$fetch_data['alertThreshold'],
                                                'storage_path'=>$fetch_data['storagePath'],
                                                'storage_device_name'=>$fetch_data['storageDeviceName'],
                                                'storage_space'=>$fetch_data['storageSpace'],
                                                'moveRetentionPath'=>$fetch_data['moveRetentionPath'],
                                                'catalogPath'=>$fetch_data['catalogPath']
                                            );
        }
        else
        {
            /*get the time policy details for each window*/
            while($row = $this->conn->sql_fetch_array($rs))
            {
                
                $sql = "SELECT 
                            storagePath,
                            storageDeviceName,
                            storagePruningPolicy,
                            consistencyTag,
                            alertThreshold,
                            moveRetentionPath,
                            catalogPath
                        FROM 
                            retentionEventPolicy 
                        WHERE 
                            Id = '$row[Id]'";
                $result_set = $this->conn->sql_query($sql); 
                $fetch_data = $this->conn->sql_fetch_array($result_set);
                $log_management[$count] = array('on_insufficient_disk_space'=>$fetch_data['storagePruningPolicy'],
                                                'alert_threshold'=>$fetch_data['alertThreshold'],
                                                'storage_path'=>$fetch_data['storagePath'],
                                                'storage_device_name'=>$fetch_data['storageDeviceName'],
                                                'moveRetentionPath'=>$fetch_data['moveRetentionPath'],
                                                'catalogPath'=>$fetch_data['catalogPath']
                                            );
            }
            $count++;
        }
        return $log_management;
    }
    /*
        * Function Name: getApplicationName
        *
        * Description: 
        *    This function returns application consistency tag name 
        *
        * Parameters:
        *     Param 1  [IN]:$retentionWindowId       
        * Return Value:
        *     Param [OUT]:application name
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    private function getApplicationName($retentionWindowId,$retpolId)
    {
        /*Sql to get the application name for specified window*/
        $sql = "SELECT 
                    consistencyTag
                FROM 
                    retentionEventPolicy 
                WHERE 
                    retentionWindowId = '$retentionWindowId'
				AND
					Id = '$retpolId'";

        $result_set = $this->conn->sql_query($sql);  
        $fetch_result = $this->conn->sql_fetch_array($result_set);
        return ($fetch_result['consistencyTag']);
    }
    
    private function getTagCount($retentionWindowId,$retpolId)
    {
        /*Sql to get the application name for specified window*/
        $sql = "SELECT 
                    tagCount
                FROM 
                    retentionEventPolicy 
                WHERE 
                    retentionWindowId = '$retentionWindowId'
				AND
					Id = '$retpolId'";
        $result_set = $this->conn->sql_query($sql);  
        $fetch_result = $this->conn->sql_fetch_array($result_set);
        return ($fetch_result['tagCount']);
    }
    
    private function getUserDefinedTag($retentionWindowId,$retpolId)
    {
        /*Sql to get the application name for specified window*/
        $sql = "SELECT 
                    userDefinedTag
                FROM 
                    retentionEventPolicy 
                WHERE 
                    retentionWindowId = ('$retentionWindowId')
				AND
					Id = '$retpolId'";
        $result_set = $this->conn->sql_query($sql);  
        $fetch_result = $this->conn->sql_fetch_array($result_set);
        return ($fetch_result['userDefinedTag']);
    }
    
    /*
        * Function Name: get_policytype
        *
        * Description: 
        *    This function returns policy type for configured pair
        *
        * Parameters:
        *     Param 1  [IN]:$retID      
        * Return Value:
        *     Param [OUT]:policy type
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function get_policytype($retID)
    {
        /*Sql fetches the policy type for a specified retention ID*/
        $sql = "SELECT 
                    retPolicyType
                FROM 
                    retLogPolicy 
                WHERE 
                    retId IN ('$retID')";
        
        $result_set = $this->conn->sql_query($sql); 
        $fetch_data = $this->conn->sql_fetch_array($result_set);
        return ($fetch_data['retPolicyType']);
    }
    /*
        * Function Name: remove_timePolicy
        *
        * Description: 
        *    This function deletes the time window
        *
        * Parameters:
        *     Param 1  [IN]:$retID      
        * Return Value:
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    private function remove_timePolicy($retID)
    {
        /*Sql deletes the time window for a specified retention ID*/
        $sql = "DELETE 
                FROM
                    retentionWindow
                WHERE 
                    retId =$retID";
        $result_set = $this->conn->sql_query($sql); 
    }
    /*
        * Function Name: remove_spacePolicy
        *
        * Description: 
        *    This function deletes the space policy details
        *
        * Parameters:
        *     Param 1  [IN]:$retID      
        * Return Value:
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    private function remove_spacePolicy($retID)
    {
        /*Sql deletes the space policy for a specified retention ID*/
        $sql = "DELETE 
                FROM
                    retentionSpacePolicy
                WHERE 
                    retId =$retID";
        $result_set = $this->conn->sql_query($sql); 
    }
    /*
        * Function Name:get_uniqueId
        *
        * Description: 
        *    This function get the unique Id for a policy 
        *
        * Parameters:
        *     Param 1  [IN]:$ruleId
        * Return Value:
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function get_uniqueId($ruleId)
    {
		global $logging_obj;
		$logging_obj->my_error_handler("DEBUG","get_uniqueId called with ruleId : $ruleId ");
		$logging_obj->my_error_handler("DEBUG","update_timeBased_moveRetention :: $catalog_path");
        $sql="SELECT
                uniqueId
            FROM
                retLogPolicy
            WHERE
                ruleid = '$ruleId'";
        $result_set = $this->conn->sql_query($sql); 
		$fetch_data = $this->conn->sql_fetch_array($result_set);
        return ($fetch_data['uniqueId']);
    }
    
    /*
        * Function Name: update_timeBased_moveRetention
        *
        * Description: 
        *    This function updates moveretention path for time based policy
        *
        * Parameters:
        *     Param 1  [IN]:$strRetID      
        *     Param 2  [IN]:$retention_path     
        *     Param 3  [IN]:$log_device  
        * Return Value:
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function update_timeBased_moveRetention($strRetId,$retention_path,$catalog_path,$log_device="")
    {
        global $logging_obj;
        $logging_obj->my_error_handler("INFO","update_timeBased_moveRetention :: $catalog_path");
        $select_sql = "SELECT 
                        event.retentionEventPolicyId  
                    FROM 
                        retentionEventPolicy event,
                        retentionWindow window 
                    WHERE
                        window.Id = event.Id 
                    AND
                        window.retId IN ($strRetId)";
        $rs =$this->conn->sql_query($select_sql);
        $i=0;
        $retEventIds="";
        while($row = $this->conn->sql_fetch_array($rs))
        {
            if ($i == 0)
            {
                $retEventIds= $row['retentionEventPolicyId'];  
            }
            else
            {
                $retEventIds = $retEventIds.",".$row['retentionEventPolicyId'];
            }
            $i++;
        }
        if($retEventIds != "")
        {
            if($log_device!="")
            {
                $update_sql="UPDATE
                                retentionEventPolicy
                            SET
                                storagePath = '$retention_path',
                                storageDeviceName ='$log_device',
                                catalogPath = '$catalog_path'
                            WHERE
                                retentionEventPolicyId IN ($retEventIds)";
            }
            else
            {
                $update_sql="UPDATE
                                retentionEventPolicy
                            SET
                                storagePath = '$retention_path',
                                catalogPath = '$catalog_path'
                            WHERE
                                retentionEventPolicyId IN ($retEventIds)";
                
            }
            $logging_obj->my_error_handler("DEBUG","update_sql::$update_sql");
            $query_status = $this->conn->sql_query($update_sql);
            return ($query_status);
        }
        else
        {
            return 0;
        }
        
    }
    
    /*
        * Function Name: update_spaceBased_moveRetention
        *
        * Description: 
        * This function updates moveretention path for space based policy
        *
        * Parameters:
        *     Param 1  [IN]:$strRetID      
        *     Param 2  [IN]:$retention_path     
        *     Param 3  [IN]:$log_device  
        * Return Value:
        *
        * Exceptions:
        *     <Specify the type of exception caught>
        */
    public function update_spaceBased_moveRetention($strRetId,$retention_path,$catalog_path,$log_device="")
    {
        global $logging_obj;
        $logging_obj->my_error_handler("DEBUG","update_spaceBased_moveRetention");
        $select_sql="SELECT 
                        space.retentionSpacePolicyId
                    FROM
                        retentionSpacePolicy space,
                        retLogPolicy ret
                    WHERE 
                        space.retId = ret.retId
                    AND
                        space.retId IN ($strRetId)";
        
        $rs =$this->conn->sql_query($select_sql);
        
        while($row = $this->conn->sql_fetch_array($rs))
        {
            if($row['retentionSpacePolicyId'] != "") 
            {
                $retentionSpacePolicyId = $row['retentionSpacePolicyId'];
                if($log_device!="")
                {
                    $update_sql="UPDATE
                                    retentionSpacePolicy
                                SET
                                    storagePath = '$retention_path',
                                    storageDeviceName ='$log_device',
                                    catalogPath = '$catalog_path'
                                    
                                WHERE
                                    retentionSpacePolicyId =$retentionSpacePolicyId";
                }
                else
                {
                    $update_sql="UPDATE
                                    retentionSpacePolicy
                                SET
                                    storagePath = '$retention_path',
                                    catalogPath = '$catalog_path'
                                WHERE
                                    retentionSpacePolicyId =$retentionSpacePolicyId";
                }
                $query_status = $this->conn->sql_query($update_sql);
                
            }
            else
            {
                return 0;
            }
            
        }
        return ($query_status);
    }
    
    public function reset_device_flag_in_use($dstId,$log_device)
    {
        global $logging_obj;
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
        $event_result_set =  $this->conn->sql_query($query);
        while ($event_row = $this->conn->sql_fetch_object($event_result_set))
        {
            $deviceName = $this->conn->sql_escape_string($event_row->storageDeviceName);
            $log_device_name[] = $deviceName;
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
        $space_result_set =  $this->conn->sql_query($query);
        while ($space_row = $this->conn->sql_fetch_object($space_result_set))
        {
            $deviceName = $this->conn->sql_escape_string($space_row->storageDeviceName);
            $log_device_name[] = $deviceName;
        }
        $reset=0;
        
        for($i=0;$i<count($log_device_name);$i++) 
        {
            $otherMetaVolumes = $log_device_name[$i];
            if ($log_device == $otherMetaVolumes)
            {
                $reset=1;
            }
        }
        if(!$reset)
        {
            $query="UPDATE
                        logicalVolumes "."
                    SET
                        deviceFlagInUse = 0 "."
                    WHERE
                        hostId = '$dstId' 
                    AND
                        (deviceName = '$log_device' OR mountPoint = '$log_device')";
            $result_set=$this->conn->sql_query($query);
        }
        return $result_set;
    }
    
    public function get_sparse_enable($ruleId)
    {
        $sql="SELECT
                sparseEnable
            FROM
                retLogPolicy
            WHERE
                ruleid = '$ruleId'";
        $result_set = $this->conn->sql_query($sql); 
        $fetch_data = $this->conn->sql_fetch_array($result_set);
        return ($fetch_data['sparseEnable']);
    }
	
	public function get_sparse_enable_new($pairId)
    {
        $sql="SELECT 
				a.sparseEnable 
			FROM 
				retLogPolicy AS a 
			INNER JOIN 
				srcLogicalVolumeDestLogicalVolume AS b on a.ruleid = b.ruleId 
			WHERE 
				b.pairId = '$pairId'";
        $result_set = $this->conn->sql_query($sql); 
        $fetch_data = $this->conn->sql_fetch_array($result_set);
        return ($fetch_data['sparseEnable']);
    }
	
	
}
?>
