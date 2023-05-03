<?php

/* 
 *================================================================= 
 * FILENAME
 *  CommonFunctions.php
 *
 * DESCRIPTION
 *  This script contains all functions common to VX, FX
 *
 * HISTORY
 *      06-March-2008  Reena    Created by pulling functions from 
 *			     functions.php and conn.php
 *     <Date 2>  Author    Bug fix : <bug number> - Details of fix
 *     <Date 3>  Author    Bug fix : <bug number> - Details of fix
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/

class CommonFunctions
{
    /*
    * database handler
    */
    public $conn;
    /*
    * Function Name: Recovery
    *
    * Description:   
    *    This is the constructor of the class
    *    
    * Return Value:
    *    Ret Value:
    */
    public function __construct()
    {
        global $conn_obj;
		$this->conn = $conn_obj;
    }    
    
	/*
    * Function Name: memFormat
    *
    * Description: 
    * This function checks if $DB_RESOURCE exists if not then create and return
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *   No Exception handled
    */
	public function memFormat($data) 
	{
		$unim = array("B","KB","MB","GB","TB","PB");
        $c = 0;
        while ($data>=1024) {
           $c++;
           $data = $data/1024;
        }
        $data = number_format($data,($c ? 2 : 0),'.','');
		return $data.$unim[$c];
	}
	
    
     /*
    * Function Name: getHostIPAddr
    *
    * Description: 
    * This function get the ip adress of the CX 
    *
    * Return Value:
    *     Ret Value: ipadress
    *
    * Exceptions:
    *   No Exception handled
    */
    
    public function getHostIPAddr()
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
	
    /*
    * Function Name: helpHealthFlag
    *
    * Description:
    * This function get the ip adress of the CX
    *
    * Return Value:
    *     Ret Value: ipadress
    *
    * Exceptions:
    *   No Exception handled
    */

	public function helpHealthFlag($flag)
	{
		if ($_SESSION['health_flag'] < $flag ) 
		{
			 $_SESSION['health_flag'] = $flag;
		}
	}

    /*
    * Function Name: addcond
    *
    * Description: 
    *  This function add the where clause string in the exsting sql query
    *
    * Parameters:
    *     Param 1 [IN]:$newcond
    *     Param 2 [IN]:$origsql
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function addcond($newcond,$origsql)
    {
        $pos = stristr($newcond,"all");
        
        if (stristr($newcond,"all") == true)
        {
            $addcond = $origsql;
        }
        else
        {
            $pos = stristr($origsql,"where");
            
            if (stristr($origsql,"where") == true)
            {
                $addcond=$origsql . " and ". $newcond;
            }
            else
            {
                $addcond=$origsql . " where " . $newcond;
            }
            
        }
        
        return $addcond;
    
    }
    
    /*
    * Function Name: addcond2
    *
    * Description: 
    *  This function add the where clause string in the exsting sql query
    *
    * Parameters:
    *     Param 1 [IN]:$newcond
    *     Param 2 [IN]:$origsql
    *     Param 3 [IN]:$orcond
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function addcond2($newcond,$origsql,$orcond)
    {
        $pos = stristr($newcond,"all");
        
        if (stristr($newcond,"all") == true)
        {
            $addcond=$origsql;
        }
        else
        {
            $pos = stristr($origsql,"where");
            
            if (stristr($origsql,"where") == true)
            {
                if($orcond==0)
                {
                    $addcond=$origsql . " and ". $newcond;
                }
                if($orcond==1)
                {
                    $addcond=$origsql . " or ". $newcond;
                }
                if($orcond==2)
                {
                    $addcond=$origsql.$newcond;
                }
            }
            else
            {
                $addcond=$origsql . " where " . $newcond;
            }
        }        
        return $addcond;    
    }
	
    
    /*
    * Function Name: getHostName
    *
    * Description: 
    *  This function returns the host name on the bais of host id
    *
    * Parameters:
    *     Param 1 [IN]:$id
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function getHostName($id)
    {        
        $sql = "SELECT
                    name
                FROM
                    hosts
                WHERE
                    id='$id'";
        
        $rs= $this->conn->sql_query($sql);
        
        if( $myrow = $this->conn->sql_fetch_object($rs) ) 
        {
            return $myrow->name;
        }
        else{
            
            return 0;
        }
    }

    /*
    * Function Name: getHostIpaddress
    *
    * Description: 
    *  This function returns the host name on the bais of host id
    *
    * Parameters:
    *     Param 1 [IN]:$id
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function getHostIpaddress($id)
    {        
        $sql = "SELECT
                    distinct ipaddress
                FROM
                    hosts
                WHERE
                    id='$id'";
        
        $rs= $this->conn->sql_query($sql);
        
        if( $myrow = $this->conn->sql_fetch_object($rs) ) 
        {
            return $myrow->ipaddress;
        }
        else{
            
            return 0;
        }
    }
    
    /*
    * Function Name: get_host_info
    *
    * Description: 
    *  This function returns the host id on the bais of host name
    *
    * Parameters:
    *     Param 1 [IN]:$id
    *
    * Return Value:
    *     Ret Value: Array
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function get_host_info($id)
    {
        $sql_host = "SELECT
                        id,
                        name,
                        sentinelEnabled,
                        outpostAgentEnabled,
                        filereplicationAgentEnabled,
						appAgentEnabled,
						ipaddress,
					    processorArchitecture,
						processServerEnabled,
						agentRole,
						UNIX_TIMESTAMP(now()) as currentTime,
						lastHostUpdateTime,
						UNIX_TIMESTAMP(lastHostUpdateTimePs) as lastHostUpdateTimePs,
						osFlag,
						prod_version,
						osType,
						creationTime,
						TargetDataPlane,
						compatibilityNo 
                     FROM
                        hosts 
                     WHERE
                        id = ?";
        
        $details = $this->conn->sql_query($sql_host, array($id));

		if(!$details) return;
        $result = array(
                        'id' => $details[0]['id'],
                        'name' => $details[0]['name'],
                        'sentinelEnabled' => $details[0]['sentinelEnabled'],
                        'outpostAgentEnabled' => $details[0]['outpostAgentEnabled'],
                        'filereplicationAgentEnabled' => $details[0]['filereplicationAgentEnabled'],
						'ipaddress' => $details[0]['ipaddress'],
						'appAgentEnabled' => $details[0]['appAgentEnabled'],
						'processorArchitecture' => $details[0]['processorArchitecture'],
						'processServerEnabled' => $details[0]['processServerEnabled'],
						'agentRole' => $details[0]['agentRole'],
						'currentTime' => $details[0]['currentTime'],
						'lastHostUpdateTime' => $details[0]['lastHostUpdateTime'],
						'lastHostUpdateTimePs' => $details[0]['lastHostUpdateTimePs'],
						'osFlag' => $details[0]['osFlag'],
						'prodVersion' => $details[0]['prod_version'],
						'osType' => $details[0]['osType'],
						'creationTime' => $details[0]['creationTime'],
						'TargetDataPlane' => $details[0]['TargetDataPlane'],
						'compatibilityNo' => $details[0]['compatibilityNo']
                   );

        return $result;
    
    }

    /*
    * Function Name: getAgentOSType
    *
    * Description: 
    *  This function returns whether agent is windows or linux
    *
    * Parameters:
    *     Param 1 [IN]:$vardb
    *     Param 1 [IN]:$guid
    *
    * Return Value:
    *     Ret Value: Boolean
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function getAgentOSType($guid)
    {
        /*
         *  flag = 1 for windows and flag = 2 for other than windows
         *  #4144 & 4148 --  if - else block added
        */

       $hostInfo = $this->get_host_info($guid);
       
       $sql = "SELECT
                    compatibilityNo
               FROM
                    hosts
               WHERE
                id = '$guid';";
                
       $resultSet = $this->conn->sql_query($sql);
       $dataResult = $this->conn->sql_fetch_object($resultSet);
       
       $compatibilityNo = $dataResult->compatibilityNo;
        
        if (isset($compatibilityNo))
        {
            if ($compatibilityNo >= 400000)
            {
                $sql = "SELECT
                            osFlag as osFlag1
                        FROM
                            hosts
                        WHERE
                            id = '$guid';";
                            
            }
            else if ($compatibilityNo < 400000)
            {
                $sql = "SELECT
                            osType as osFlag1
                        FROM
                            hosts
                        WHERE
                            id = '$guid';";
                            
            }
        }

       $rs = $this->conn->sql_query($sql);
       $rsData = $this->conn->sql_fetch_object($rs);
       
       $osType = $rsData->osFlag1;
       
       if (isset($compatibilityNo))
       {
            if ($compatibilityNo >= 400000)
            {
                $flag = $osType;
            }
            else if ($compatibilityNo < 400000)
            {
                $flag = 1;
                $osType= strToUpper($osType);
                $pos = strpos($osType, "WINDOWS");
                
                if($pos === false)
                {
                    $flag = 2;
                }
                else
                {
                    $flag = 1;
                }
            }
       }        
        return $flag;    
    }
    
    
    /*
    * Function Name: is_host_linux
    *
    * Description: 
    *  This function returns '\\' in case of windows and '/' in case of linux
    *
    * Parameters:
    *     Param 1 [IN]:$hostId
    *
    * Return Value:
    *     Ret Value: Boolean
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function is_host_linux($hostId)
    {        
        $sql = "SELECT
                    osFlag
                FROM
                    hosts
                WHERE
                    id ='".$hostId."'" ;
                    
        $iter = $this->conn->sql_query ($sql);
        
        $myrow = $this->conn->sql_fetch_object($iter);
        
        $osFlag =$myrow->osFlag;
                   
        if($osFlag == 1 )
        {
            return false;			
        }
        else
        {
            return true;	
        }            
    }
    
    /*
    * Function Name: get_osFlag
    *
    * Description: 
    *  This function returns the osflag from the database
    *
    * Parameters:
    *     Param 1 [IN]:$hostId
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function get_osFlag($hostId)
    {        
        $sql = "SELECT
                    osFlag
                FROM
                    hosts
                WHERE
                    id ='".$hostId."'" ;
                    
        $iter = $this->conn->sql_query ($sql);
		        
        $myrow = $this->conn->sql_fetch_object($iter);
        
        $osFlag =$myrow->osFlag;        
        return $osFlag;    
    }
    
    /*
    * Function Name: getOSType
    *
    * Description: 
    *  This function returns the Os type
    *
    * Parameters:
    *     Param 1 [IN]:
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function getOSType()
    {
        $osType = '';

        if (preg_match('/Windows/i', php_uname()))
        {
            $osType = 'Windows';
        }
        else
        {
            $varCmd = 'cat /proc/version ';
            $fversion = file("/proc/version");

            $arrOS = array("Red Hat Linux","SuSE Linux");
            $i=0;
            
            while ($i <= count($arrOS))
            {
                $varPos = strpos($fversion[0],$arrOS[$i]);
                
                if ($varPos)
                {
                    $osType = $arrOS[$i];
                    break;
                }
                
                $i++;
            }
        }        
        return $osType;    
    }    
    
	public function get_psName_ipadress_by_id($id)
    {
       $resultArr = array();	
		 $sql = "SELECT
					ipaddress,
                    name
                FROM
                   hosts
                 WHERE
                    id = ?";
		$result = $this->conn->sql_query($sql, array($id));
		
        if(count($result) > 0)
        {
			$resultArr[] = array('ipaddress' => $result[0]['ipaddress'],
								'name' => $result[0]['name']);
            return $resultArr;
        }
		return false;
    }
	
       
    /*
    * Function Name: debugLog
    *
    * Description: 
    *  debugger, maintain logs in a file 
    *
    * Parameters:
    *     Param 1 [IN]:$debugString
    *
    * Return Value:
    *     Ret Value:
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function debugLog ( $debugString)
    {
        /*global $PHPDEBUG_FILE;
        
        $fr = fopen($PHPDEBUG_FILE, 'a+');
        
        if(!$fr)
        {
            #echo "Error! Couldn't open the file.";
        }
        if (!fwrite($fr, $debugString . "\n"))
        {
            #print "Cannot write to file ($filename)";
        }
        if(!fclose($fr))
        {
            #echo "Error! Couldn't close the file.";
        }*/
        
    }
    
     /*
    * Function Name: affects_today
    *
    * Description: 
    *  return 0/1 on the baisis of dayType
    *  
    * Parameters:
    *     Param 1 [IN]:$rule_row
    *     Param 2 [IN]:$date
    *
    * Return Value:
    *     Ret Value:Boolean
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function affects_today ($rule_row, $date)
    {
        $today  = strtotime (date ("Y-m-d", $date));
        $sd     = strtotime ($rule_row ["startDate"]);
        $ed     = strtotime ($rule_row ["endDate"]);
        
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
            elseif ($rule_row ["dayType"] == "WKD" &&
                    (1 <= date ("w", $date) && date ("w", $date) <= 5))
            {
                return 1;
            }
            elseif ($rule_row ["dayType"] == "WKE" &&
                    (0 == date ("w", $date) || date ("w", $date) == 6))
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
    
    /*
    * Function Name: is_deviaffects_time_of_dayce_profiler
    *
    * Description: 
    * return 1/0 on the basis of runType
    * 
    * Parameters:
    *     Param 1 [IN]:$rule_row
    *     Param 2 [IN]:$date
    *
    * Return Value:
    *     Ret Value:Boolean
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function affects_time_of_day ($rule_row, $date)
    {
        $now = strtotime (date ("H:i:s", $date));
        $start = strtotime ($rule_row ["startTime"]);
        $end = strtotime ($rule_row ["endTime"]);
        
        if ($rule_row ["runType"] == "ALLDAY")
        {
            return 1;
        }
        elseif ($rule_row ["runType"] == "NEVER")
        {
            return 0;
        }
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
    
	/*
    * Function Name: page_redirect
    *
    * Description: 
    * redirect to the page passed in paramater
    * 
    * Parameters:
    *     Param 1 [IN]:$url
    *
    * Return Value:
    *     Ret Value:Boolean
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function page_redirect($url)
    {
        if (headers_sent())
        {
            echo "<meta http-equiv=\"refresh\" content=\"0;URL=$url\">";
        }
        else
        {
            header("Location:$url");
        }
        
        exit;
        
    }
    
    /*
    * Function Name: page_redirect
    *
    * Description: 
    * return the plural word on the baiss of number
    * 
    * Parameters:
    *     Param 1 [IN]:$word
    *     Param 2 [IN]:$number
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function pluralize($word, $number)
    {        
       return( $number == 1 ? $word : $word . "s" );    
    }
    
    /*
    * Function Name: datetime_diff
    *
    * Description: 
    * return a string of d,h,m,s
    * 
    * Parameters:
    *     Param 1 [IN]:$datetime_from
    *     Param 2 [IN]:$datetime_to
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function datetime_diff( $datetime_from,$datetime_to )
    {
        //divide dateime in date and time
        $datetime_from_parts    =   explode (' ',$datetime_from);
        $datetime_to_parts      =   explode (' ',$datetime_to);
        $date_from              =   $datetime_from_parts[0];
        $date_to                =   $datetime_to_parts[0];
        $time_from              =   $datetime_from_parts[1];
        $time_to                =   $datetime_to_parts[1];
        
        //get parts of the time
        $time_from_parts        =   explode(':', $time_from);
        $time_to_parts          =   explode(':', $time_to);
        $hour_from              =   $time_from_parts[0];
        $minute_from            =   $time_from_parts[1];
        $second_from            =   $time_from_parts[2];
        $hour_to                =   $time_to_parts[0];
        $minute_to              =   $time_to_parts[1];
        $second_to              =   $time_to_parts[2];
        
        //get parts of the dates
        $date_from_parts        =   explode('-', $date_from);
        $date_to_parts          =   explode('-', $date_to);
        $day_from               =   $date_from_parts[2];
        $mon_from               =   $date_from_parts[1];
        $year_from              =   $date_from_parts[0];
        $day_to                 =   $date_to_parts[2];
        $mon_to                 =   $date_to_parts[1];
        $year_to                =   $date_to_parts[0];
        
        $mktime_from            =   mktime($hour_from,
                                           $minute_from,
                                           $second_from,
                                           $mon_from,
                                           $day_from,
                                           $year_from);
        
        $mktime_to              =   mktime($hour_to,
                                           $minute_to,
                                           $second_to,
                                           $mon_to,
                                           $day_to,
                                           $year_to);
        
        if($mktime_from >  $mktime_to)
        {
            $tmp            =   $mktime_to;
            $mktime_to      =   $mktime_from;
            $mktime_from    =   $tmp;
            $sign           =   "-";
        }
        else
        {
            $sign           =   "";    
        }
        
        /* if date_from is newer than date to, invert dates*/
        
        $diff       =   $mktime_to - $mktime_from;
        $offset     =   $diff % 86400;
        $days       =   (floor (($diff - $offset)/86400 ));
        
        $diff       =   $offset;
        $offset     =   $diff % 3600;
        $hours      =   (floor (($diff - $offset)/3600 ));
        
        $diff       =   $offset;
        $offset     =   $diff % 60;
        $minutes    =   (floor (($diff - $offset)/60 ));
        
        $seconds    =   $offset;
        
        $str        =   $sign;
        if($days)
        {
            $str .= $days." d ";    
        }
        
        if($hours)
        {
            $str .= $hours." h ";    
        }
        
        if($minutes)
        {
            $str .= $minutes." m ";    
        }
        
        if($seconds)
        {
            $str .= $seconds." s ";    
        }
        
        return $str;
    }
    
    /*
    * Function Name: get_inmage_version_array
    *
    * Description: 
    * reads the inmage version from file and return it
    * 
    * Parameters:
    *     Param 1 [IN]
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function get_inmage_version_array()
    {
        global $REPLICATION_ROOT, $INMAGE_VERSION_FILE;
        
		$has_lf = $this->file_read_contents("$REPLICATION_ROOT/$INMAGE_VERSION_FILE");

        $result = array();
        
        foreach( $has_lf as $line )
        {
            $result []= rtrim( $line );
        }
      
        return( $result );
    
    }
    
    /*
    * Function Name: get_cx_patch_history
    *
    * Description: 
    * reads the records from the patch.log and return in an array
    * 
    * Parameters:
    *     Param 1 [IN]
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function get_cx_patch_history ()
    {
        global $REPLICATION_ROOT;
        
        $result = "";
        
        if($this->is_windows())
        {
            $pathDelimiter = '\\';
        }                
        else
        {
            $pathDelimiter = '/';
        }
        
        $cxPatchLogPath = $REPLICATION_ROOT.$pathDelimiter."patch.log";
        
        if(file_exists($cxPatchLogPath))
        {
            $cxAppliedPatches = $this->file_read_contents($cxPatchLogPath);
            
            foreach ($cxAppliedPatches as $value)
            {
                $result[] = explode(",",$value);
            }
                    
        }
		
		$cshotfixLogPath = $REPLICATION_ROOT.$pathDelimiter."hotfix.log";
        
        if(file_exists($cshotfixLogPath))
        {
            $csAppliedhotfixes = $this->file_read_contents($cshotfixLogPath);
            
            foreach ($csAppliedhotfixes as $value)
            {
                $result[] = explode(",",$value);
            }
                    
        }
        
        return ($result);
    }
	
    
    /*
    * Function Name: update_agent_log
    *
    * Description: 
    * update the agent log file
    * 
    * Parameters:
    *     Param 1 [IN]: $hostId
    *     Param 2 [IN]: $volume
    *     Param 3 [IN]: $errorString
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
	public function update_agent_log ($hostId, $volume, $errorString, $agent_timestamp=0, $error_source='',$ipAddress='')
	{
	   global $REPLICATION_ROOT;
       global $CONFIG_WEB_ROOT;
	   global $logging_obj;
	   global $LN;
	   
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
				$errorMsg = explode("=",$errorList[3]);
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
			
			$errorString = "$TimeString Resync detected at"." $resyncRequestTimeStamp (TimeStamp sent by S2):"." Error message sent by node $ipAddress S2: $errStr";
		}
		elseif($agent_timestamp != 0)
		{
			$errorString = "$TimeString Resync detected at"." $resyncRequestTimeStamp (TimeStamp sent by S2):"." Error message sent by $hostId S2: $errStr";
		}
		elseif ($error_source == "vol_resize" or $error_source == 'cx_error_log' or $error_source == "ps_agent")
		{
			$errorString ="$TimeString $errorString";
		}
		elseif ($error_source == "reassign_primary")
		{
			$errorString ="$TimeString "."Error sent by $hostId for 1 to N pair with srcVolume $volume."."$errorString";
		}
        elseif($error_source == "process_server_failover")
        {
            $errorString ="$TimeString "."Error sent by $hostId for process server failover."."$errorString";
        }
		elseif($error_source == "target_volume_visible_in_read_write")
        {
            $errorString ="$TimeString $errorString";
		}
		elseif($error_source == "force_diff_sync")
        {
            $errorString ="$TimeString "."Error sent by $hostId for force diff sync."."$errorString";
        }
		else
		{
			$errorString = "$TimeString "."Error sent by $hostId DP while $volume pair deletion "."$errorString";
		}

		$volume = str_replace("\\", "/", $volume);
		$volume = str_replace(":", "", $volume);

		$filename = $REPLICATION_ROOT.'/'.$hostId.'/'.$volume.'/sentinel.txt';
		$fr = $this->file_open_handle($filename, 'a+');
		if(!$fr) 
		{
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
    
    /*
    * Function Name: is_windows
    *
    * Description: 
    * return true/false if windows
    * 
    * Parameters:
    *     Param 1 [IN]:
    *
    * Return Value:
    *     Ret Value:Boolean
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function is_windows ()
    {
        if (preg_match('/Windows/i', php_uname()))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    
    /*
    * Function Name: is_linux
    *
    * Description: 
    * return true/false if linux
    * 
    * Parameters:
    *     Param 1 [IN]:
    *
    * Return Value:
    *     Ret Value:Boolean
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function is_linux ()
    {
        if (preg_match('/Linux/i', php_uname()))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    
    /*
    * Function Name: make_path
    *
    * Description: 
    * return path in case of wind/linux
    * 
    * Parameters:
    *     Param 1 [IN]:
    *
    * Return Value:
    *     Ret Value:Scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function make_path ($path)
    {
        if ($this->is_linux ())
        {
            return $path;
        }
        else if ($this->is_windows ())
        {
            for ($i = 0; $i < strlen($path); $i++)
            {
                if ($path[$i] == '/')
                {
                    $path[$i] = '\\';
                }
            }
            return $path;
        }
    }

 
    /*
    * Function Name: encodePath
    *
    * Description: 
    * decode the path
    * 
    * Parameters:
    *     Param 1 [IN]:$path
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function encodePath($path)
    {
    
        if(strpos($path,' '))
        {
            $temp = explode(' ',$path);
            $path = implode('#06#',$temp);
        }
        
        return $path;
    }
    
    /*
    * Function Name: decodePath
    *
    * Description: 
    * decodes the path 
    * 
    * Parameters:
    *     Param 1 [IN]:$path
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function decodePath($path)
    {
        if(strpos($path,'#06#'))
        {
            $temp = explode('#06#',$path);
            $path = implode(' ',$temp);
        }
        
        return $path;
    
    }
    
    /*
    * Function Name: verifyDatalogPath
    *
    * Description: 
    * verify the passed path
    * 
    * Parameters:
    *     Param 1 [IN]:$dPath
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function verifyDatalogPath($dPath)
    {
        $folderCollection = array();
        $dPathList = explode(":",$dPath);
        $count =  count($dPathList);
        
        if ($count > 1)
        {
            $dPathTerms = explode("\\",$dPathList[1]);
            
            if (count($dPathTerms) > 1)
            {
                    
                    for($i=0;$i<count($dPathTerms);$i++)
                    {
                        if ($dPathTerms[$i]!='')
                        {
                            $folderCollection[] = $dPathTerms[$i];
                        }
                    }
                    
                    if (count($folderCollection) > 0)
                    {
                            $dPath = implode("\\",$folderCollection);
                            $dPath = $dPathList[0].':\\'.$dPath;
                    }
                    /*
                     This is the else block if the path doesn't contain any
                     sub folders and the path contains more slashes.
                     If folder collection is zero
                    */
                    else 
                    {
                        $dPath = $dPathList[0].':\\';
                    }
            }
			else
			{
			   $dPath = str_replace(':', ':\\', $dPath);
			}
        }
        
        return $dPath;
    }
    
    /*
    * Function Name: rename_and_or_delete
    *
    * Description: 
    * This function is used to remove additional slashes in the windows path
    * 
    * Parameters:
    *     Param 1 [IN$dPath
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function filterAdditionalSlashes($dPath)
    {
        $folderCollection = array();
        $dPathList = explode(":",$dPath);
        $count =  count($dPathList);
       
        if ($count > 1)
        {
            $dPathTerms = explode("\\",$dPathList[1]);
            
            if (count($dPathTerms) > 1)
            {
                
                for($i=0;$i<count($dPathTerms);$i++)
                {
                    if ($dPathTerms[$i]!='')
                    {
                        $folderCollection[] = $dPathTerms[$i];
                    }
                }
                
                if (count($folderCollection) > 0)
                {
                    $dPath = implode("\\",$folderCollection);
                    $dPath = $dPathList[0].':\\'.$dPath;
                }
                /*
                 This is the else block if the path doesn't contain any
                 sub folders and the path contains more slashes.
                 If folder collection is zero
                */
                else 
                {
                    $dPath = $dPathList[0].':\\';
                }
            }
        }
        
        return $dPath;
    
    }
    
    #1toN Fast change
	 /*
    * Function Name: get_compatibility
    *
    * Description: 
    * 
    * 
    * Parameters:
    *     host id
    *
    * Return Value:
    *     compatibility number
	*
    * Exceptions:
    *     <Specify the type of exception caught>
    */ 
	public function get_compatibility($id)
    {  
       /*Added In clause to handle cluster case*/ 
	   $query = "SELECT
                    compatibilityNo
                  FROM
                    hosts
                  WHERE
                    id IN('$id')";
       
        $iter = $this->conn->sql_query($query);
        
        while ($myrow = $this->conn->sql_fetch_row($iter))
        {
            $compatibilityNo = $myrow[0];
        }
		return $compatibilityNo;
	}
	
	/*
    * Function Name: writeLog
    *
    * Description: 
    * write a debug log to DB
    * 
    * Parameters:
    *     Param 1 [IN]:$line
    *
    * Return Value:
    *     Ret Value:scalar
    *     
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function writeLog($userName=NULL,$actionMsg=NULL,$filePath=NULL)
    {
		global $logging_obj;
		$trace = debug_backtrace();
		$GLOBALS['auditLog'] = TRUE;
		
		/*Capture DateTime in the format of:: 2006-12-11 05:25:26*/
        $now = date("Y-m-d H:i:s");                        
			
		/*Capture ClientIp*/
	    if($userName == 'System')
		{
		  $clientIp = " ";
		}
		else
		{
          $clientIp = $_SERVER['REMOTE_ADDR'];
		}
        
		$actionMsg_db = $this->conn->sql_escape_string($actionMsg);	
		
		$caller_info = '';
		$trace_size = sizeof($trace);
		for($i = 0; $i<$trace_size; $i++)
		{
			$file_name = basename($trace[$i]['file']);
			$fun_name = $trace[$i]['function'];
			$caller_info .= ($i+1).'#'.$file_name.'#'.$fun_name.'|';
		}	
		$caller_info = $this->conn->sql_escape_string($caller_info);	
		
		$sql_insert_query = "INSERT INTO
			    auditLog(
				    userName,
				    dateTime,
				    ipAddress,
				    details,
					callerInfo)
			    VALUES (
				'$userName',
				'$now',
				'$clientIp',
				'$actionMsg_db',
				'$caller_info')";
	    $sql = $this->conn->sql_query($sql_insert_query);
		
		$telemetry_log_string["userName"]=$userName;
		$telemetry_log_string["dateTime"]=$now;
		$telemetry_log_string["ipAddress"]=$clientIp;
		$telemetry_log_string["details"]=$actionMsg_db;
		$telemetry_log_string["callerInfo"]=$caller_info;
		$logging_obj->my_error_handler("INFO",$telemetry_log_string,Log::APPINSIGHTS);
	}
    
	public function process_agent_error($hostid,$elevel,$data,$ts,$module,$alert_type,$cx_logger='',$error_code='',$error_placeholders='')
	{
	    global $ABHAI_MIB, $TRAP_AMETHYSTGUID, $TRAP_HOSTGUID, $TRAP_AGENT_ERROR_MESSAGE, $TRAP_AGENT_LOGGED_ERROR_MESSAGE_TIME, $TRAP_AGENT_LOGGED_ERROR_LEVEL, $TRAP_AFFTECTED_SYSTEM_HOSTNAME, $TRAP_AGENT_LOGGED_ERROR_MESSAGE, $CXLOGGER_MULTI_QUERY, $CS_IP;
		$time_para_replace = array("(", "):");
	    $errReportTime = str_replace($time_para_replace, "", $ts );

		$errHostId = $hostid;
		$errlvl = $elevel;
		$errMessage = $this->conn->sql_escape_string($data);
		$errtime = $errReportTime;

		$sql = "SELECT name, ipaddress, agentRole from hosts where id = '$errHostId'";
		$result = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_array($result);
		
		$HostName = $row['name'];
		$HostIpAddress = $row['ipaddress'];
		$HostRole = $row['agentRole'];
		
		/*
		* For the agents not registered with CX
		*/
		if($HostName == "")
		{
			return;
		}

		$source = "monitor: $errlvl";
		$subject = "On $HostName: $errlvl Observed/Logged";
		
		if ($error_code == "EA0429")
		{
			$subject = "Recovery point tagging failed.";
		}
		if ($error_code == "EA0430")
		{
			$subject = "Recovery point tagging failed.";
		}
		if ($error_code == "EA0431")
		{
			$subject = "App-consistent recovery point tagging failed.";
		}
		/*
		* alert level 4 with supporting subject line changes depends on message
		*/
		if ($alert_type == 4) 
		{
			list($firstpart, $errMessage) = preg_split("\n",$errMessage, 2);
			$subject = "On $HostName: $firstpart";
		}
		
		if ($error_code == "EA0428" || $error_code == "EA0432" || $error_code == "EA0433")
		{
			if (is_array($error_placeholders))
			{
				$source_vm_host_id = $error_placeholders['SourceVmHostId'];
				$subject = "On $source_vm_host_id: $errlvl Observed/Logged";
				if ($error_code == "EA0432" || $error_code == "EA0433")
				{
					$subject = "Critical error in replication.";
					$hostid = $source_vm_host_id;
				}
				$sql = "SELECT name, ipaddress, agentRole from hosts where id = '$source_vm_host_id'";
				$result = $this->conn->sql_query($sql);
				$row = $this->conn->sql_fetch_object($result);				
				$HostName = $row->name;
				$HostIpAddress = $row->ipaddress;
				$HostRole = $row->agentRole;
				$error_placeholders['SourceName'] = $HostName;
				$error_placeholders['CSName'] = $CS_IP;
				$error_placeholders['VMName'] = $HostName;
			}
		}
		
        if($alert_type == 1 || $alert_type == 2 || $alert_type == 4)
        {
            $errlvl = "FATAL";
        }
        elseif($alert_type == 3)
        {
            $errlvl = "NOTICE";
        }
		$err_id = $errHostId;
		$err_template_id = "AGENT_ERROR";
		$err_summary = $subject;
		$err_message = "$errMessage on $HostName($HostIpAddress)";
		$error_placeholders['HostName'] = $HostName;
		$error_placeholders['IPAddress'] = $HostIpAddress;
        $error_placeholders = serialize($error_placeholders);
		
		// Exception case to handle error code differently for Source => PS / PS => MT connectivity errors. This should be implemented at Agent. But as of now implementing at CS.
		// EA0411 => PS to MT connectivity failed
		// EA0427 => Source to PS connectivity failed
		if ($error_code == 'EA0411' && $HostRole == 'Agent')	$error_code = 'EA0427';
		$this->add_error_message_v2($err_id, $err_template_id, $err_summary, $err_message, $hostid, $error_code, $error_placeholders, $errlvl);
	}
	
	/*
    * Function Name: get_host_type
    *
    * Description: 
    *  This function returns the host type on the basis of host id, to distinguish
    *  between host and fabric agent
    *
    * Parameters:
    *     Param 1 [IN]:unique host id
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *     <Specify the type of exception caught>
    */
    public function get_host_type($host_id)
    {
        $host_type = "";        
        $sql_host = "SELECT
                        type
                     FROM
                        hosts
                     WHERE
                        id = '$host_id'";
        
        $hosts = $this->conn->sql_query($sql_host);        
        $row = $this->conn->sql_fetch_object($hosts);        
        $host_type = $row->type;
        return $host_type;
    }
	
	/*
	*  Function Name: get_solution_type
	*
	* Description:
	* 	to get Solution type with given plan_id
	*/ 
	public function get_solution_type($plan_id)
	{
		$sql = "SELECT
                        solutionType
                     FROM
                        applicationPlan
                     WHERE
                        planId = '$plan_id'";
        $rs = $this->conn->sql_query($sql);        
        $row = $this->conn->sql_fetch_object($rs);        
        $solution_type = $row->solutionType;
        return $solution_type;
	}
	
	public function customer_branding_names($text_mod)
	{
		global $CUSTOM_BUILD_TYPE;
	
		if($CUSTOM_BUILD_TYPE == 2)
		{
			$find_str = array(
				"FX Agent", 
				"VX Agent", 
				"Application",
				
				"PS",
				"Process Server", 
				
				"CS", 
				"Configuration Server", 
				"Control Server",
				
				"CX", 
				"CX Server", 
				"Appliance",
				
				//"Inmage",
				
				"ISControl ServiceI"
				);
			$replace_str = array(
				"File Agent", 
				"Volume Agent", 
				"Application Agent",
				
				"Process Service",
				"Process Service",
				
				"Control Service", 
				"Control Service", 
				"Control Service",
				
				"Replication Engine", 
				"Replication Engine", 
				"Replication Engine",
				
				//"Oracle",
				
				"iSCSI"
				);
			$text_mod = str_ireplace($find_str, $replace_str, $text_mod);
			return $text_mod; 
		}
		else
		{
			return $text_mod;
		}
		
	}

	/*		
	* Function Name: add_error_message
	*
	* Description:
	*    Need to have more information from akshayhe
	*
	* Parameters:
	*     Param 1 [IN]: error_id
	*     Param 2 [IN]: error_template_id
	*     Param 3 [IN]: summary
	*     Param 4 [IN]: message
	*
	* Return Value:
	*     Ret Value: N/A
	*
	* Exceptions:
	*     <Specify the type of exception caught>
	*/
	public function add_error_message ($error_id, $error_template_id, $summary, $message, $host_id = '', $cx_logger='', $error_code='', $error_placeholders='', $error_level='')
	{	
		$summary = $this->customer_branding_names($summary);
		$message = $this->customer_branding_names($message);
			
		global $logging_obj;
		global $CXLOGGER_MULTI_QUERY, $DROPPED_ERROR_CODE_LIST;
		
		if(in_array($error_code, $DROPPED_ERROR_CODE_LIST))
		{
			// ignore the error code as health factor API is taking care this code.
			return;
		}
		
		$this->updateAgentAlertsDb($error_template_id, $host_id, $summary, $message, $cx_logger, $error_code, $error_placeholders, $error_level);
				
		$val = $this->is_cx_maintenance_mode();
				
		if($val != 1)
		{
			$summary = $this->conn->sql_escape_string(trim($summary));
			$message = $this->conn->sql_escape_string(trim($message));
			
			$sql = "SELECT user_id, send_mail FROM error_policy where error_template_id='$error_template_id'";
			$iter1 = $this->conn->sql_query ($sql );
			while ($res1 = $this->conn->sql_fetch_row ($iter1))
			{				
				$uid = $res1[0];
				$send_mail = $res1[1];
				
				if  ($send_mail)
				{
					$sql = "SELECT
							message_count
						FROM
							error_message
						WHERE 
							error_id='$error_id' AND
							error_template_id='$error_template_id' AND 
							summary='$summary' AND
							user_id='$uid'";
					$iter3 = $this->conn->sql_query ($sql );
					
					if ($res3 = $this->conn->sql_fetch_row ($iter3))
					{
						$count = $res3[0];
						$count++;
						$sql = "UPDATE
							error_message
							SET
							last_update_time=now(),
							message_count='$count',
							message='$message'
							WHERE 
							error_id='$error_id' AND
							summary='$summary' AND 
							error_template_id='$error_template_id' AND
							user_id='$uid'";
						
					}
					else
					{
						$sql = "INSERT INTO
							error_message (error_id, error_template_id, ".
							"summary, message,user_id,first_update_time,last_update_time,message_count)
							VALUES
							('$error_id', '$error_template_id', ".
								"'$summary', '$message', '$uid',now(),now(),'1')";						
					}
					if($cx_logger)
					{
						$CXLOGGER_MULTI_QUERY[] = $sql;
					}
					else
					{
						$rs = $this->conn->sql_query ($sql );
					}
				}
			}
        }
	}

	public function updateAgentAlertsDb($error_template_id, $host_id, $summary, $message, $cx_logger='', $error_code='', $error_placeholders='', $error_level='', $event_codes_dependency = '1', $component = 'Agent')
	{
		global $CXLOGGER_MULTI_QUERY, $logging_obj, $MDS_ERROR_CODE_LIST;
		
		$sql = "SELECT level,mail_subject FROM error_template WHERE error_template_id=?";
		$result_array = $this->conn->sql_query ($sql, array($error_template_id));
		
		if (empty($result_array))
		{
			$logging_obj->my_error_handler("INFO", "No data found for error template id: " . $error_template_id);
			return;
		}
		
		$result_array = $result_array[0];
		$error_level = trim($error_level);
		$errLevel = $error_level ? $error_level : $result_array['level'];
		$errCategory = $result_array['mail_subject'];
		
		$summary = trim($summary);
		$message = trim($message);
		$errSummary = $this->conn->sql_escape_string($summary);
		$errMessage = $this->conn->sql_escape_string($message);
		$error_placeholders = (trim($error_placeholders)) ? $this->conn->sql_escape_string(trim($error_placeholders)) : '';
		
		$is_event = 0;
		$cond = "";
		$sql_params = $resultset = array();
		
		if($error_code)
		{
			$is_event = $this->conn->sql_get_value('eventCodes', 'isEvent', 'errCode = ?', array($error_code));
			
			$cond = " errCode = ? AND ";
			$sql_params[] = $error_code;
		}
		
		if (! $is_event)
		{
			$sql = "SELECT
						alertId,
						errCount
					FROM 
						dashboardAlerts
					WHERE
						$cond errTemplateId=? AND errSummary=? AND hostId=?";
			
			$sql_params = array_merge($sql_params, array($error_template_id, $summary, $host_id));
			$resultset = $this->conn->sql_query ($sql, $sql_params);
		}
		$mds_flag = TRUE;
		/// Should not be an event and should have the counter.
		if (! $is_event && ! empty($resultset))
		{
			$data = $resultset[0];
			$count = $data['errCount'] + 1;
			$sql = "UPDATE 
						dashboardAlerts
					SET
						errCount='$count',
						errTime=now(),
						errMessage='$errMessage',
						errTemplateId='$error_template_id',
                        errCode='$error_code',
                        errPlaceholders='$error_placeholders'
					WHERE
						alertId='" . $data['alertId'] ."'";
			$mds_flag = FALSE;
		}
		else
		{
			$sql = "INSERT 
					INTO 
						dashboardAlerts(
						errTime,
						errLevel,
						errCategory,
						errSummary,
						errMessage,
						errTemplateId,
						hostId,
						errCount,
						errStartTime,
                        errCode,
                        errPlaceholders,
						eventCodesDependency,
						errComponent
						)
					VALUES (
						now(),
						'$errLevel',
						'$errCategory',
						'$errSummary',
						'$errMessage',
						'$error_template_id',
						'$host_id',
						'1',
						now(),
                        '$error_code',
                        '$error_placeholders',
						$event_codes_dependency,
						'$component'
						)";
		}
		if($cx_logger)
		{
			$CXLOGGER_MULTI_QUERY[] = $sql;
		}
		else
		{
			$rs = $this->conn->sql_query ($sql);
		}
		
		if ($mds_flag == TRUE)
		{
			if (in_array($error_code, $MDS_ERROR_CODE_LIST))
			{
				$eventName = "CS";	
				if ($error_code == "EA0429" || $error_code == "EA0430" || $error_code == "EA0431" || $error_code == "EA0432" || $error_code == "EA0433")
				{
					$eventName = "Source/Target";
				}
				else
				{
					$eventName = "PS";
				}
				$mds_data_array = array();
				$this->mds_obj = new MDSErrorLogging();
				$mds_data_array["activityId"] = "9cc8c6e1-2bfb-4544-a850-a23c9ef8162d";
				#$mds_data_array["jobId"] = $error_code;
				$mds_data_array["jobId"] = "";
				$mds_data_array["jobType"] = "Protection";
				$mds_data_array["errorString"] = $errMessage."Hostid:$host_id --Error code:".$error_code;
				$mds_data_array["eventName"] = $eventName;
				$mds_data_array["errorType"] = "ERROR";
				
				$this->mds_obj->saveMDSLogDetails($mds_data_array);
			}
		}
		
	}
	
	/*		
	* Function Name: add_error_message_v2
	*
	* Description:
	*    Function to push data to dashboard alerts table.
	*
	* Parameters:
	*     Param 1 [IN]: error_id
	*     Param 2 [IN]: error_template_id
	*     Param 3 [IN]: summary
	*     Param 4 [IN]: message
	*     Param 5 [IN]: host_id
	*     Param 6 [IN]: error_code
	*     Param 7 [IN]: error_placeholders
	*     Param 8 [IN]: error_level
	*
	* Return Value:
	*     Ret Value: N/A
	*
	*/
	public function add_error_message_v2($error_id, $error_template_id, $summary, $message, $host_id = '', $error_code='', $error_placeholders='', $error_level='')
	{	
		$summary = $this->customer_branding_names($summary);
		$message = $this->customer_branding_names($message);
			
		global $logging_obj;
		global $CXLOGGER_MULTI_QUERY, $DROPPED_ERROR_CODE_LIST;
		
		if(in_array($error_code, $DROPPED_ERROR_CODE_LIST))
		{
			// ignore the error code as health factor API is taking care this code.
			return;
		}
		
		$this->updateAgentAlertsDbV2($error_template_id, $host_id, $summary, $message, $error_code, $error_placeholders, $error_level);	
		$val = $this->is_cx_maintenance_mode();
		if($val != 1)
		{
			$summary = trim($summary);
			$message = trim($message);
			
			$sql = "SELECT user_id, send_mail FROM error_policy where error_template_id = ?";
			$search_results = $this->conn->sql_query($sql, array($error_template_id));
			if (is_array($search_results) && count($search_results) > 0)
			{
				foreach ($search_results as $key=>$obj)
				{			
					$uid = $obj['user_id'];
					$send_mail = $obj['send_mail'];
					if ($send_mail)
					{
						$sql = "SELECT
								message_count
							FROM
								error_message
							WHERE 
								error_id = ? AND
								error_template_id = ? AND 
								summary = ? AND
								user_id = ?";
						$search_results_msg = $this->conn->sql_query($sql, array($error_id, $error_template_id, $summary, $uid));
						
						if (is_array($search_results_msg) && count($search_results_msg) > 0)
						{
							foreach ($search_results_msg as $key=>$obj)
							{
								$count = $obj['message_count'];
								$count++;
								$sql = "UPDATE
									error_message
									SET
									last_update_time = now(),
									message_count = ?,
									message = ? 
									WHERE 
									error_id = ? AND
									summary = ? AND 
									error_template_id = ? AND
									user_id = ?";
								$rs = $this->conn->sql_query($sql, array($count, $message, $error_id, $summary, $error_template_id, $uid));
							}
						}
						else
						{
							$sql = "INSERT INTO
								error_message 
								(error_id, 
								error_template_id, 
								summary, 
								message,
								user_id,
								first_update_time,
								last_update_time,
								message_count)
								VALUES
								(?, 
								?, 
								?, 
								?, 
								?, 
								now(), 
								now(), 
								'1')";
							$rs = $this->conn->sql_query($sql, array($error_id, $error_template_id, $summary, $message, $uid));		
						}
					}
				}
			}
        }
	}

	public function updateAgentAlertsDbV2($error_template_id, $host_id, $summary, $message, $error_code='', $error_placeholders='', $error_level='', $event_codes_dependency = '1', $component = 'Agent')
	{
		global $logging_obj, $ONE;
		
		$sql = "SELECT level,mail_subject FROM error_template WHERE error_template_id=?";
		$result_array = $this->conn->sql_query ($sql, array($error_template_id));
		
		if (empty($result_array))
		{
			$logging_obj->my_error_handler("INFO", "No data found for error template id: " . $error_template_id);
			return;
		}
		
		$result_array = $result_array[0];
		$error_level = trim($error_level);
		$errLevel = $error_level ? $error_level : $result_array['level'];
		$errCategory = $result_array['mail_subject'];
		
		$summary = trim($summary);
		$message = trim($message);
		$error_placeholders = (trim($error_placeholders)) ? trim($error_placeholders) : '';
		$is_event = 0;
		$cond = "";
		$sql_params = $resultset = array();
		
		if($error_code)
		{
			$is_event = $this->conn->sql_get_value('eventCodes', 'isEvent', 'errCode = ?', array($error_code));
			
			$cond = " errCode = ? AND ";
			$sql_params[] = $error_code;
		}
		
		if (! $is_event)
		{
			$sql = "SELECT
						alertId,
						errCount
					FROM 
						dashboardAlerts
					WHERE
						$cond errTemplateId=? AND errSummary=? AND hostId=?";
			
			$sql_params = array_merge($sql_params, array($error_template_id, $summary, $host_id));
			$resultset = $this->conn->sql_query ($sql, $sql_params);
		}

		/// Should not be an event and should have the counter.
		if (! $is_event && ! empty($resultset))
		{
			$data = $resultset[0];
			$count = $data['errCount'] + 1;
			$sql = "UPDATE 
						dashboardAlerts
					SET
						errCount = ?,
						errTime=now(),
						errMessage = ?,
						errTemplateId = ?,
                        errCode = ?,
                        errPlaceholders = ? 
					WHERE
						alertId = ?";
			$args = array($count, $message, $error_template_id, $error_code, $error_placeholders, $data['alertId']);	
		}
		else
		{
			$sql = "INSERT 
					INTO 
						dashboardAlerts(
						errTime,
						errLevel,
						errCategory,
						errSummary,
						errMessage,
						errTemplateId,
						hostId,
						errCount,
						errStartTime,
                        errCode,
                        errPlaceholders,
						eventCodesDependency,
						errComponent
						)
					VALUES (
						now(),
						?,
						?,
						?,
						?,
						?,
						?,
						?,
						now(),
                        ?,
                        ?,
						?,
						?
						)";
			$args = array($errLevel, $errCategory, $summary, $message, $error_template_id, $host_id, $ONE, $error_code, $error_placeholders,  $event_codes_dependency, $component);
		}
		$rs = $this->conn->sql_query($sql, $args);
	}

	/*		
	* Function Name: send_trap
	*
	* Description:
	*    To send the trap.
	*
	* Parameters:
	*     Param 1 [IN]: Trap command
	*
	* Return Value:
	*     Ret Value: N/A
	*
	* Exceptions:
	*     <Specify the type of exception caught>
	*/ 
	public function send_trap($trap_command, $err_template_id, $host_id = '', $cx_logger='')
	{
        global $logging_obj;
		global $CXLOGGER_MULTI_QUERY;
		global $HOST_GUID;
		
        $trap_command = str_replace("<br>", " ", $trap_command);
        $trap_command = str_replace("\n", " ", $trap_command);
        $trap_command = $this->conn->sql_escape_string($trap_command);
		
		
		$err_template_id = $this->get_error_template($err_template_id);
        $trap_arr = $this->get_trap_listeners($err_template_id);
		
		if($host_id == '')
		{
			$cx_hostid = $HOST_GUID;
		}
		else
		{
			$cx_hostid = $host_id;
		}
		
		$val = $this->is_cx_maintenance_mode();
		if($val != 1)
		{
			foreach($trap_arr as $uid => $trap_ips)
			{
				foreach ($trap_ips['receiverIP'] as $trap_listener_key => $trap_listener_ip)
				{
					if($this->is_windows())
					{
						$trap_listener_port = $trap_ips['receiverPort'][$trap_listener_key];
						$trap_cmd = "snmptrapagent -v 2c -c public -ip $trap_listener_ip -port $trap_listener_port ".$trap_command;
					}
					else
					{
						$trap_cmd = "snmptrap -v 2c -c public $trap_listener_ip \"\" ".$trap_command;
					}
					$sql = "SELECT id FROM trapInfo WHERE trapCommand='".$trap_cmd."' and userId = '$uid'";
					$rs = $this->conn->sql_query($sql);
					
					if($this->conn->sql_num_row($rs) == 0)
					{
						$insert_sql = "INSERT INTO trapInfo (trapCommand, userId) VALUES ('".$trap_cmd."', '$uid')";
						$logging_obj->my_error_handler("DEBUG","trap command insertion query :: $insert_sql");
						if($cx_logger)
						{
							$CXLOGGER_MULTI_QUERY[] = $insert_sql;
						}
						else
						{
							$result_set = $this->conn->sql_query($insert_sql);
						}
					}
				}
			}
		}
	}

	/*		
	* Function Name: get_trap_listeners
	*
	* Description:
	*    To Get the user configured trap listener ip addresses.
	*
	* Parameters:
		Param 1 [IN] :	ErrorTemplate
	*
	* Return Value:
	*     Ret Value: Array of ip addresses
	*
	* Exceptions:
	*/
	private function get_trap_listeners($errorTemplate) 
	{
		$sql = "SELECT 
					tl.trapListenerIPv4,
					tl.userId,
					tl.trapListenerPort
				FROM
					trapListeners tl, 
					error_policy ep
				WHERE 
					ep.user_id = tl.userId 
					AND ep.error_template_id = '$errorTemplate'
					AND ep.send_trap = '1'";

		$result_set = $this->conn->sql_query ($sql );
		
		$data = array();
		while ($data_obj = $this->conn->sql_fetch_object($result_set))
		{
			$user_ids = $data_obj->userId;
			$data[$user_ids]['receiverIP'][] = $data_obj->trapListenerIPv4;
			$data[$user_ids]['receiverPort'][] = $data_obj->trapListenerPort;
		}
		return $data;
	}
	
	/*		
	* Function Name: getRecordLimit
	*
	* Description:
	*    This function is to used for pagination. Returns limit string
	*    
	* Parameters:
	*     Param 1 [IN]: Page number 
	*     Param 2 [IN]: Page Rows
	*     Param 3 [IN]: Total Records
	* Return Value: NA
	*
	* Exceptions:
	*/
	public function getRecordLimit($page_no,$page_rows,$snap_count)
	{
        $last_page = ceil($snap_count/$page_rows);
		if($page_no < 1)
		{
			$page_no = 1;
		}
		elseif($page_no > $last_page)
		{
			$page_no = $last_page;
		}
        $from =($page_no - 1)*$page_rows;
		$limit = 'LIMIT '.$from.','.$page_rows;
		return $limit;
	}
	
	/*
    * Function Name: getAllClusters
    *
    * Description: 
    * This function returns the all cluster ids and hostid's in hash format
    *
    * Return Value:
    *     Ret Value: Hash (with clusterId's as keys and hostId's as values
    *
    * Exceptions:
    *   No Exceptions handled
    */
    
 	public function getAllClusters()
  	{
		$result = array();

		$sql = "select hostId,clusterId from clusters group by hostId";
		$sth = $this->conn->sql_query ($sql);
		while( $row = $this->conn->sql_fetch_row( $sth) )
		{ 
			$result[$row[1]][] = $row[0];
		}
		return $result;
	}

	/*
    * Function Name: getEliminatedClusterIds
    *
    * Description: 
    * This function returns the eliminated hostId's in specific format
    *
    * Return Value:
    *     Ret Value: string contains eliminated hostid's
    *
    * Exceptions:
    *   No Exceptions handled
    */
	public function getEliminatedClusterIds($cluster_ids)
	{		
		if (count($cluster_ids) > 0)
		{			
			foreach($cluster_ids as $key=>$value)
			{
				$cluster_node_ids =  $cluster_ids[$key];
				array_pop($cluster_node_ids);
				if (is_array($cluster_node_ids))
				{
					$eliminated_cluster_ids[] = implode("','",$cluster_node_ids);
				}
			}
		}
		if (count($eliminated_cluster_ids) > 0)
		{
			$eliminated_cluster_ids = implode("','",$eliminated_cluster_ids);
		}
		return $eliminated_cluster_ids;
	}

	public function getTargetDeviceDetails($ruleId)
	{
            $destHostDetails = array();
            $query ="SELECT 
                        destinationHostId,
                        destinationDeviceName
                    FROM
                        srcLogicalVolumeDestLogicalVolume
                    WHERE
                       ruleId='$ruleId'";
            $result_set = $this->conn->sql_query ($query);
            $row = $this->conn->sql_fetch_object($result_set);
            $destHostDetails['destinationHostId'] = $row->destinationHostId;
            $destHostDetails['destinationDeviceName'] = $row->destinationDeviceName;
        
            return ($destHostDetails);
    }
	
	public function getTargetDeviceDetails_v2($pairId)
	{
            $destHostDetails = array();
            $query ="SELECT 
                        destinationHostId,
                        destinationDeviceName
                    FROM
                        srcLogicalVolumeDestLogicalVolume
                    WHERE
                       pairId='$pairId'";
            $result_set = $this->conn->sql_query ($query);
            $row = $this->conn->sql_fetch_object($result_set);
            $destHostDetails['destinationHostId'] = $row->destinationHostId;
            $destHostDetails['destinationDeviceName'] = $row->destinationDeviceName;
        
            return ($destHostDetails);
    }
	
    #Convert the unix time stamp to readable string format
	public function getTimeFormat($timeStamp)
    {
        $str = '';
        if($timeStamp != 0)
        {
            $units = $timeStamp;
            $nano_sec = substr($units, strlen($units)-1);
            $micro_sec = substr($units, strlen($units)-4,3);
            $mili_sec = substr($units, strlen($units)-7,3);
            $sec = substr($units, 0, 11);
            $time = gmdate("Y-m-d H:i:s", ($sec - 11644473600));
            $time_arr = explode(' ',$time);
            $time_arr1 = explode("-",$time_arr[0]);
            $time_str1 = implode(",",$time_arr1);
            $time_arr2 = explode(":",$time_arr[1]);
            $time_str2 = implode(",",$time_arr2);
            $str = $time_str1.",".$time_str2.",".$mili_sec.",".$micro_sec.",".$nano_sec;
        }
        return $str;
    }

	public function amethyst_values()
    {
		global $REPLICATION_ROOT,$AMETHYST_PATH_CONF, $logging_obj;
		$amethyst_conf = $REPLICATION_ROOT.$AMETHYST_PATH_CONF;
		if (file_exists($amethyst_conf))
		{
			$base_file_name = basename($_SERVER['PHP_SELF']);
			$action = "";
			$read_retry_cnt = 0;
			$fopen_retry_cnt = 0;
			$fread_retry_cnt = 0;
			//Prepending with @ to supress E_WARNING and to handle further based on read_status check to throw custom exception based on failure.
			readretry:
			$read_status = @is_readable($amethyst_conf);
			if ($read_status)
			{
				fopenretry:
				$file = @fopen ($amethyst_conf, 'r');
				if ($file)
				{
					freadretry:
					$conf = @fread ($file, filesize($amethyst_conf));
					if ($conf)
					{
						fclose ($file);
						$conf_array = explode ("\n", $conf);
						foreach ($conf_array as $line)
						{
							$line = trim($line);
							if (empty($line))	continue;
						
							if (! preg_match ("/^#/", $line))
							{
								list ($param, $value) = explode ("=", $line);
								$param = trim($param);
								$value = trim($value);
								$result[$param] = str_replace('"', '', $value);
							}
						}
					}
					else
					{
						$fread_retry_cnt++;
						if ($fread_retry_cnt < 5)
						{
							$sql = "SELECT now()";
							$sth = $this->conn->sql_query($sql);
							sleep(10);
							goto freadretry;
						}
						else
						{
							//Throw file content read exception.
							$action = "FREAD";
						}
					}
				}
				else
				{
					$fopen_retry_cnt++;
					if ($fopen_retry_cnt < 5)
					{
						$sql = "SELECT now()";
						$sth = $this->conn->sql_query($sql);
						sleep(10);
						goto fopenretry;
					}
					else
					{
						//Throw file open exception
						$action = "FOPEN";
					}
				}
			}
			else
			{
				$read_retry_cnt++;
				if ($read_retry_cnt < 5)
				{
					$sql = "SELECT now()";
					$sth = $this->conn->sql_query($sql);
					sleep(10);
					goto readretry;
				}
				else
				{
					//Throw permission denied exception.
					$action = "PERMISSION";
				}
			}
			if ($action != "")
			{
				switch ($action)
				{
					case "PERMISSION":
						$msg = "Amethyst.conf: Failed to open file, Permission denied.";
						$error_code = "EC_FILE_PERMISSION_DENIED";
					break;
					case "FOPEN":
						$msg = "Amethyst.conf: Failed to open file, No such file or directory.";
						$error_code = "EC_FILE_NOT_FOUND";
					break;
					case "FREAD":
						$msg = "Amethyst.conf: Failed to read file content.";
						$error_code = "EC_FILE_READ_FAILED";
					break;
					default:
					
				}
				
				switch($base_file_name)
				{
					case "CXAPI.php":
						ErrorCodeGenerator::raiseError('COMMON', $error_code, array(), $msg);
					break;
					case "request_handler.php":
						$logging_obj->my_error_handler("E_WARNING",$msg);
					break;
					default:
						$logging_obj->my_error_handler("E_WARNING",$msg);
				}
			}
		}
		$result['CS_HTTP'] = ($result["CS_SECURED_COMMUNICATION"])?"https" : "http";   
		$result['PS_CS_HTTP'] = ($result["PS_CS_SECURED_COMMUNICATION"])?"https" : "http";   
		return $result;
	}
   
	public function cxpsconf_values()
    {
        global $REPLICATION_ROOT, $logging_obj;

        if (preg_match('/Linux/i', php_uname())) 
        {
            $cspx_conf = $REPLICATION_ROOT.'transport/cxps.conf';
        }
        else
        {
            $cspx_conf = $REPLICATION_ROOT.'\\transport\\cxps.conf';
        }

        if (file_exists($cspx_conf))
        {
            $file = $this->file_open_handle($cspx_conf, 'r');
            $conf = fread ($file, filesize($cspx_conf));
            fclose ($file);
            $conf_array = explode ("\n", $conf);
            foreach ($conf_array as $line)
            {
                if (!preg_match ("/^#/", $line))
                {
                    list ($param, $value) = explode ("=", $line);
                    $param = trim($param);
                    $value = trim($value);
                    $result[$param] = $value;
                }
            }
        }
        return $result;
	}  
	
	public function tman_services_values()
	{
		global $REPLICATION_ROOT, $SLASH;
		
		$tman_service_file = $REPLICATION_ROOT.$SLASH."etc".$SLASH."tman_services.conf";
		
		if (file_exists($tman_service_file))
		{
			$file = fopen ($tman_service_file, 'r');
			$conf = fread ($file, filesize($tman_service_file));
			fclose ($file);
			$conf_array = explode ("\n", $conf);
			foreach ($conf_array as $line)
			{
				if (!preg_match ("/^#/", $line))
				{
					list ($param, $value) = explode ("=", $line);
					$param = trim($param);
					$value = trim($value);
					if($value != "OFF") $result[$param] = str_replace('"', '', $value);
				}
			}
		}
		
		return array_keys($result);
	}
	
	public function verify_agent($hostID)
	{
		$sql = "SELECT
					outpostAgentEnabled as vx
				FROM
					hosts
				WHERE
					id = '$hostID'
				OR
					ipaddress = '$hostID'";
		$sth = $this->conn->sql_query($sql);
		$row = $this->conn->sql_fetch_array($sth);
		return $row['vx'];
	}	

    public function NLS($str)
	{
		global $_, $CUSTOM_BUILD_TYPE, $REPLICATION_ROOT;
		
		if($CUSTOM_BUILD_TYPE == 3) return;
		require_once("$REPLICATION_ROOT/admin/web/ui/lang_nls.php");
		
	
		
		global $logging_obj;
		
		$str = trim($str);
		
		$str = $this->customerBrandingNames($str);
		if(array_key_exists($str,$_))
		{
			return $_[$str];
		}
		else
		{
			$data = "File->".$_SERVER['SCRIPT_NAME']."--Text->".$str;
			$logging_obj->my_error_handler("DEBUG","data::$data");
			return $str;
		}
    }
	
	public function customerBrandingNames($str)
	{
		global $CUSTOM_BUILD_TYPE;
	
		if($CUSTOM_BUILD_TYPE == 2)
		{			
			$branding = array(
							  "FX Agent" => "File Agent",
							  "VX Agent" => "Volume Agent",
							  "Application" => "Application Agent",
							  
							  "PS" => "Process Service",
							  "Process Server" => "Process Service",
							  "Process Servers" => "Process Service",
							  
							  "CS" => "Control Service",
							  "Configuration Server" => "Control Service",
							  "Control Server" => "Control Service",
							  
							  "CX" => "Replication Engine",
							  "CX Server" => "Replication Engine",
							  "Appliance" => "Replication Engine"							  
							);
			if(array_key_exists($str,$branding))
			{				
				return $branding[$str];
			}
			else
			{
				return $str;
			}		
		}
		else
		{
			return $str;
		}	
	}
	
	/*
	 * Function Name: get_volume_hash_data
	 *
	 * Description:
	 *	   Used to get all disks,partition,logicalVolumes etc in an hash.
	 *       
	 * Parameters:
	 *     Param 1  [IN]:$volumes array
	 *
	 * Return Value: volume hash
	*/
	public function get_volume_hash_data($volumes,$compatibilityNo=0)
	{
		$diskFlag=0;
		$vsnapMountFlag=0;
		$vsnapUnmountFlag=0;
		$unknownFlag=0;
		$partitionFlag=0;
		$logicalVolumeFlag=0;
		$volPackFlag=0;
		$customFlag=0;
		$extendedPartitionFlag=0;
		$diskPartitionFlag=0;
		
		$diskChildFlag=0;
		$vsnapMountChildFlag=0;
		$vsnapUnmountChildFlag=0;
		$unknownChildFlag=0;
		$partitionChildFlag=0;
		$logicalVolumeChildFlag=0;
		$volPackChildFlag=0;
		$customChildFlag=0;
		$extendedPartitionChildFlag=0;
		$diskPartitionChildFlag=0;
		
		$disk=0;
		$vsnapMounted=1;
		$vsnapUnmounted=2;
		$unknown=3;
		$partition=4;
		$logicalVolume=5;
		$volpack=6;
		$custom=7;
		$extendedPartition=8;
		$diskPartition =9;
		$children_volume_hash_arr=array();		
		
		foreach ($volumes as $volumesValue)
		{
			if($volumesValue[3] == $disk && (empty($diskFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$disk,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				
				$diskFlag=1;
			}
			else if($volumesValue[3] == $vsnapMounted  && (empty($vsnapMountFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$vsnapMounted,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				$vsnapMountFlag = 1;
			}
			else if($volumesValue[3] == $vsnapUnmounted && (empty($vsnapUnmountFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$vsnapUnmounted,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				$vsnapUnmountFlag = 1;
			}
			else if($volumesValue[3] == $unknown && (empty($unknownFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$unknown,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				$unknownFlag = 1;
			}
			else if($volumesValue[3] == $partition && (empty($partitionFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$partition,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				$partitionFlag = 1;
			}
			else if($volumesValue[3] == $logicalVolume && (empty($logicalVolumeFlag)))
			{
				$device_type = $volumesValue[3];
				if(!empty($volumesValue[1]))
				{
					$device_type = 0;
				}
				$inner_volume_hash=$this->create_hash_arr($volumes,$logicalVolume,"",$compatibilityNo);
				$outer_volume_hash[$device_type] = $inner_volume_hash;
				$logicalVolumeFlag = 1;
			}
			else if($volumesValue[3] == $volpack && (empty($volPackFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$volpack,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				$volPackFlag = 1;
			}
			else if($volumesValue[3] == $custom && (empty($customFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$custom,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				$customFlag = 1;
			}
			else if($volumesValue[3] == $extendedPartition && (empty($extendedPartitionFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$extendedPartition,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				$extendedPartitionFlag = 1;
			}
			else if($volumesValue[3] == $diskPartition && (empty($diskPartitionFlag)))
			{
				$inner_volume_hash=$this->create_hash_arr($volumes,$diskPartition,"",$compatibilityNo);
				$outer_volume_hash[$volumesValue[3]] = $inner_volume_hash;
				$extendedPartitionFlag = 1;
			}
			
			if($compatibilityNo>=650000)
				$childrenInfo=$volumesValue[24];
			else
				$childrenInfo=$volumesValue[23];
			 
			if(!empty($childrenInfo))
			{
				$diskChildFlag=0;
				$vsnapMountChildFlag=0;
				$vsnapUnmountChildFlag=0;
				$unknownChildFlag=0;
				$partitionChildFlag=0;
				$logicalVolumeChildFlag=0;
				$volPackChildFlag=0;
				$customChildFlag=0;
				$extendedPartitionChildFlag=0;
				$diskPartitionFlag=0;
				
				foreach ($childrenInfo as $childrenInfoValue)
				{
					
					if($childrenInfoValue[3] == $disk && (empty($diskChildFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$disk,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$diskChildFlag=1;
					}
					else if($childrenInfoValue[3] == $vsnapMounted  && (empty($vsnapMountChildFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$vsnapMounted,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$vsnapMountChildFlag = 1;
					}
					else if($childrenInfoValue[3] == $vsnapUnmounted && (empty($vsnapUnmountChildFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$vsnapUnmounted,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$vsnapUnmountChildFlag = 1;
					}
					else if($childrenInfoValue[3] == $unknown && (empty($unknownChildFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$unknown,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$unknownChildFlag = 1;
					}
					else if($childrenInfoValue[3] == $partition && (empty($partitionChildFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$partition,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$partitionChildFlag = 1;
					}
					else if($childrenInfoValue[3] == $logicalVolume && (empty($logicalVolumeChildFlag)))
					{
						$device_type = $childrenInfoValue[3];
						if(!empty($childrenInfoValue[1]))
						{
							$device_type = 0;
						}
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$logicalVolume,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$device_type][] = $inner_volume_hash;
						$logicalVolumeChildFlag = 1;
					}
					else if($childrenInfoValue[3] == $volpack && (empty($volPackChildFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$volpack,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$volPackChildFlag = 1;
					}
					else if($childrenInfoValue[3] == $custom && (empty($customChildFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$custom,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$customChildFlag = 1;
					}
					else if($childrenInfoValue[3] == $extendedPartition && (empty($extendedPartitionChildFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$extendedPartition,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$extendedPartitionChildFlag = 1;
					}
					else if($childrenInfoValue[3] == $diskPartition && (empty($diskPartitionFlag)))
					{
						$inner_volume_hash=$this->create_hash_arr($childrenInfo,$diskPartition,$volumesValue[2],$compatibilityNo);
						$children_volume_hash_arr[$childrenInfoValue[3]][] = $inner_volume_hash;
						$diskPartitionFlag = 1;
					}
					
					if($compatibilityNo>=650000)
						$nestedChildrenInfo=$childrenInfoValue[24];
					else
						$nestedChildrenInfo=$childrenInfoValue[23];				
					
					if(!empty($nestedChildrenInfo))
					{
						$nestedDiskChildFlag=0;
						$nestedVsnapMountChildFlag=0;
						$nestedVsnapUnmountChildFlag=0;
						$nestedUnknownChildFlag=0;
						$nestedPartitionChildFlag=0;
						$nestedLogicalVolumeChildFlag=0;
						$nestedVolPackChildFlag=0;
						$nestedCustomChildFlag=0;
						$nestedExtendedPartitionChildFlag=0;
						$nestedDiskPartitionFlag=0;
						
						foreach ($nestedChildrenInfo as $value)
						{
							
							if($value[3] == $disk && (empty($diskChildFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$disk,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$diskChildFlag=1;
							}
							else if($value[3] == $vsnapMounted  && (empty($vsnapMountChildFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$vsnapMounted,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$vsnapMountChildFlag = 1;
							}
							else if($value[3] == $vsnapUnmounted && (empty($vsnapUnmountChildFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$vsnapUnmounted,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$vsnapUnmountChildFlag = 1;
							}
							else if($value[3] == $unknown && (empty($unknownChildFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$unknown,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$unknownChildFlag = 1;
							}
							else if($value[3] == $partition && (empty($partitionChildFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$partition,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$partitionChildFlag = 1;
							}
							else if($value[3] == $logicalVolume && (empty($logicalVolumeChildFlag)))
							{
								$device_type = $value[3];
								if(!empty($value[1]))
								{
									$device_type = 0;
								}
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$logicalVolume,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$device_type][] = $inner_volume_hash;
								$logicalVolumeChildFlag = 1;
							}
							else if($value[3] == $volpack && (empty($volPackChildFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$volpack,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$volPackChildFlag = 1;
							}
							else if($value[3] == $custom && (empty($customChildFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$custom,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$customChildFlag = 1;
							}
							else if($value[3] == $extendedPartition && (empty($extendedPartitionChildFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$extendedPartition,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$extendedPartitionChildFlag = 1;
							}
							else if($value[3] == $diskPartition && (empty($diskPartitionFlag)))
							{
								$inner_volume_hash=$this->create_hash_arr($nestedChildrenInfo,$diskPartition,$childrenInfoValue[2],$compatibilityNo);
								$children_volume_hash_arr[$value[3]][] = $inner_volume_hash;
								$extendedPartitionChildFlag = 1;
							}							
						}	
					}
				}	
			}
		}
		$syncResChildArr = $this->syncChildrenArr($children_volume_hash_arr, $outer_volume_hash);
		
		return $syncResChildArr;
	}


	public function get_volume_hash_dynamic_data($volumes)
	{

		$diskFlag=0;
		$children_volume_hash_arr = array();	
		
		foreach ($volumes as $volumesValue)
		{
			if($diskFlag == 0)
			{
				$inner_volume_hash=$this->create_hash_dynamic_arr($volumes);
				$outer_volume_hash[$volumesValue[0]] = $inner_volume_hash;
				$diskFlag=1;
			}

			$childrenInfo=$volumesValue[4];
			if(!empty($childrenInfo))
			{
				$partition = 0;
				foreach ($childrenInfo as $childrenInfoValue)
				{
					if($partition == 0)
					{
						$inner_volume_hash=$this->create_hash_dynamic_arr($childrenInfo);
						$children_volume_hash_arr[$childrenInfoValue[0]][] = $inner_volume_hash;
						$partition = 1;
					}
					$nestedChildrenInfo=$childrenInfoValue[4];
					if(!empty($nestedChildrenInfo))
					{
						$nested_partition = 0;
						foreach ($nestedChildrenInfo as $value)
						{
							if($nested_partition == 0)
							{
								$inner_volume_hash=$this->create_hash_dynamic_arr($nestedChildrenInfo);
								$children_volume_hash_arr[$value[0]][] = $inner_volume_hash;
								$nested_partition = 1;
							}
						}	
					}
				}	
			}
		}
		$syncResChildArr = $this->syncChildrenArr($children_volume_hash_arr, $outer_volume_hash);
		
		return $syncResChildArr;
	}


	private function syncChildrenArr($volumes, $outer_volume_hash)
	{
		if(count($volumes) > 0)
		{
			foreach($volumes as $key => $volumesVal)
			{
			   $resArr = array();
			   foreach($volumesVal as $data)
				{
				   foreach($data as $dataValue)
					{
						if (array_key_exists($key, $outer_volume_hash)) 
						{
							array_push($outer_volume_hash[$key], $dataValue);
						}
						else
						{
							$outer_volume_hash[$key][]=$dataValue;
						}
					}
				}
			}
		}
		return $outer_volume_hash;
	}


	private function create_hash_dynamic_arr($volumes)
	{
		foreach($volumes as $volumesValue)
		{
			$arr["scsi_id"]=$volumesValue[1];
			$arr["device_name"]=$volumesValue[2];
			$arr["freespace"]=$volumesValue[3];
			$retArr[]=$arr;

		}
		return $retArr;
	}

	private function create_hash_arr($volumes,$deviceType,$parentId="",$compatibilityNo=0)
	{
		foreach($volumes as $volumesValue)
		{
			if($volumesValue[3] == $deviceType)
			{
				$arr["scsi_id"]=$volumesValue[1];
				if (isset($volumesValue[1]) && $volumesValue[1] != '') 
				{
					$arr["scsi_id"] = $this->conn->sql_escape_string($volumesValue[1]);
				}
				$arr["device_name"]=$volumesValue[2];
				$arr["device_type"]=$volumesValue[3];
				$arr["vendorOrigin"]=$volumesValue[4];
				if (isset($volumesValue[4]) && $volumesValue[4] != '') 
				{
					$arr["vendorOrigin"] = $this->conn->sql_escape_string($volumesValue[4]);
				}
				$arr["file_system"]=$volumesValue[5];
				if (isset($volumesValue[5]) && $volumesValue[5] != '') 
				{
					$arr["file_system"] = $this->conn->sql_escape_string($volumesValue[5]);
				}
				$arr["mount_point"]=$volumesValue[6];
				$arr["is_mounted"]=$volumesValue[7];
				$arr["is_system_volume"]=$volumesValue[8];
				$arr["is_cache_volume"]=$volumesValue[9];
				$arr["capacity"]=$volumesValue[10];
				$capacity_str = (string) $volumesValue[10];
				//If the string contains all digits, then only allow, otherwise return.
				if (isset($volumesValue[10]) && $volumesValue[10] != '' && (!preg_match('/^\d+$/',$capacity_str))) 
				{
					$this->bad_request_response("Invalid post data for capacity ".$volumesValue[10]." in create_hash_arr."); 
				}
				$arr["deviceLocked"]=$volumesValue[11];
				$arr["physical_offset"]=$volumesValue[12];
				$physical_offset_str = (string) $volumesValue[12];
				//If the string contains all digits, then only allow, otherwise return.
				if (isset($volumesValue[12]) && $volumesValue[12] != '' && (!preg_match('/^\d+$/',$physical_offset_str))) 
				{
					$this->bad_request_response("Invalid post data for physical_offset ".$volumesValue[12]." in create_hash_arr."); 
				}
				$arr["sector_size"]=$volumesValue[13];
				$arr["volumeLabel"]=$volumesValue[14];
				if (isset($volumesValue[14]) && $volumesValue[14] != '') 
				{
					$arr["volumeLabel"] = $this->conn->sql_escape_string($volumesValue[14]);
				}
				$arr["isUsedForPaging"]=$volumesValue[15];
				$is_used_for_paging_str = (string) $volumesValue[15];
				//If the string contains all digits, then only allow, otherwise return.
				if (isset($volumesValue[15]) && $volumesValue[15] != '' && (!preg_match('/^\d+$/',$is_used_for_paging_str))) 
				{
					$this->bad_request_response("Invalid post data for isUsedForPaging ".$volumesValue[15]." in create_hash_arr."); 
				}
				$arr["isPartitionAtBlockZero"]=$volumesValue[16];
				$is_partition_at_block_zero_str = (string) $volumesValue[16];
				//If the string contains all digits, then only allow, otherwise return.
				if (isset($volumesValue[16]) && $volumesValue[16] != '' && (!preg_match('/^\d+$/',$is_partition_at_block_zero_str))) 
				{
					$this->bad_request_response("Invalid post data for isPartitionAtBlockZero ".$volumesValue[16]." in create_hash_arr."); 
				}
				$arr["diskGroup"]=$volumesValue[17];
				$arr["diskGroupVendor"]=$volumesValue[18];
				$arr["isMultipath"]=$volumesValue[19];
				$is_multi_path_str = (string) $volumesValue[19];
				//If the string contains all digits, then only allow, otherwise return.
				if (isset($volumesValue[19]) && $volumesValue[19] != '' && (!preg_match('/^\d+$/',$is_multi_path_str))) 
				{
					$this->bad_request_response("Invalid post data for isMultipath ".$volumesValue[19]." in create_hash_arr."); 
				}			
				$arr["rawSize"]=$volumesValue[20];
				$raw_size_str = (string) $volumesValue[20];
				//If the string contains all digits, then only allow, otherwise return.
				if (isset($volumesValue[20]) && $volumesValue[20] != '' && (!preg_match('/^\d+$/',$raw_size_str))) 
				{
					$this->bad_request_response("Invalid post data for rawSize ".$volumesValue[20]." in create_hash_arr."); 
				}
				$arr["writeCacheState"]=$volumesValue[21];
				$write_cache_str = (string) $volumesValue[21];
				//If the string contains all digits, then only allow, otherwise return.
				if (isset($volumesValue[21]) && $volumesValue[21] != '' && (!preg_match('/^\d+$/',$write_cache_str))) 
				{
					$this->bad_request_response("Invalid post data for writeCacheState ".$volumesValue[21]." in create_hash_arr."); 
				}
				$arr["formatLabel"]=$volumesValue[22];
				$format_label_str = (string) $volumesValue[22];
				//If the string contains all digits, then only allow, otherwise return.
				if (isset($volumesValue[22]) && $volumesValue[22] != '' && (!preg_match('/^\d+$/',$format_label_str))) 
				{
					$this->bad_request_response("Invalid post data for formatLabel ".$volumesValue[22]." in create_hash_arr."); 
				}
				if($compatibilityNo>=650000)
				{
					$arr["deviceProperties"] = $volumesValue[23];
				}
				if($parentId != "")
				{
					$arr["parentId"]=$parentId;
				}
					
				$retArr[]=$arr;

			}
			
		}
		return $retArr;		
	}
	
	public function format_version_string($version_str)
    {  
		global $CUSTOM_BUILD_TYPE;
		if($version_str != '')
		{
			if($CUSTOM_BUILD_TYPE == 2)
			{     
				$version_str = str_replace("PARTNER_VERSION=", "", $version_str);
				$version_str = explode("_",$version_str);
				$version_str[1] = '2.00.03';
				$version_str = $version_str[0]."_".$version_str[1]."_".$version_str[2]."_".$version_str[3]."_".$version_str[4]."_".$version_str[5]."_".$version_str[6];
			}
			
			$ver = explode("_", $version_str);
			$version['ver'] = $ver[1];
			$version['str'] = $version_str;

			return $version;
		}
    }
	
	public function getLocalTime($format=0)
	{
		if(!$format)
		{
			$sql = "select UNIX_TIMESTAMP(now()) as present_time";
			$sth = $this->conn->sql_query($sql);
			$row = $this->conn->sql_fetch_object ($sth);
			$server_time = $row->present_time;
		}
		else
		{
			$sql = "select now() as present_time";
            $sth = $this->conn->sql_query($sql);
            $row = $this->conn->sql_fetch_object ($sth);
            $server_time = $row->present_time;
            $sql = "SELECT DATE_FORMAT('$server_time', '%M-%d-%Y% %H:%i') as present_time";
            $sth = $this->conn->sql_query($sql);
            $row = $this->conn->sql_fetch_object($sth);
            $server_time = $row->present_time;
		}
		return $server_time;
	}
	
	public function getApplianceGuid()
	{
		$appliance_guid = "";
		$sql = "SELECT
						applianceGuid
					FROM
						appliance";
		$rs = $this->conn->sql_query($sql);
		while($row = $this->conn->sql_fetch_object($rs))
		{
			$appliance_guid = $row->applianceGuid;
		}
		return $appliance_guid;
	}
	
	public function ByteSize($bytes) 
    {
		$size = $bytes / 1024;
		$value = $size / (1024 * 1024);
		if ($value > 1)  
		{
			return $value;
		} 
		else
		{
			return 0;
		}
	}
	
		/*		
	* Function Name: is_cx_maintenance_mode
	*
	* Description:
	*    To check whether CX is in stand by mode.
	*
	* Return Value:
	*     Ret Value: 0/1
	*
	* Exceptions:
	*     <Specify the type of exception caught>
	*/
	public function is_cx_maintenance_mode()
	{
		$sql = "SELECT ValueData FROM transbandSettings WHERE ValueName = 'MAINTENANCE_MODE'";
		$resultset = $this->conn->sql_query ($sql);
		
		if($data = $this->conn->sql_fetch_object ($resultset))
		{
			return $data->ValueData;
		}
		else return 0;
	}	

	
		
	public function serialise($data, $source_volumes, $target_volumes, $flag='', $scenario_id='')
	{
		$cond = "";
		if($flag == 1)
		{
			$cond = ", sourceVolumes='$source_volumes', targetVolumes='$target_volumes'";
		}
		$plan_name = $data['planName'];
				
		$cdata = serialize($data);
		$str = "<plan><header><parameters>";
		$str.= "<param name='name'>".$plan_name."</param>";
		$str.= "<param name='type'>Protection Plan</param>";
		$str.= "<param name='version'>1.1</param>";
		$str.= "</parameters></header>";
		$str.= "<data><![CDATA[";
		$str.= $cdata;
		$str.= "]]></data></plan>";
		$str = $this->conn->sql_escape_string($str);
		
		$sql = "UPDATE applicationScenario SET scenarioDetails='$str' $cond WHERE scenarioId='$scenario_id'";
		$this->conn->sql_query($sql);
	}
	
	public function getDeletedPairInfo($scenarioId)
	{
		$deleted_pairs = 0;
		$sql = "SELECT deleted FROM srcLogicalVolumeDestLogicalVolume WHERE scenarioId IN ('$scenarioId') and deleted=1" ;
		$rs = $this->conn->sql_query($sql);
		$deleted_pairs = $this->conn->sql_num_row($rs);
		return $deleted_pairs;
	}
	
	public function get_host_id_by_agent_role ($role) 
	{
		$result = array();
		
		$sql = "SELECT 
					id,
					latestMBRFileName
				FROM 
					hosts
				WHERE					
					agentRole='$role'";
		$result_set = $this->conn->sql_query($sql);
		while ($row = $this->conn->sql_fetch_object($result_set)) {
			$result[$row->id] =  $row->latestMBRFileName;
		}
		return $result;
	}
	
	public function is_retention_reuse($scenario_id)
	{
		$is_ret_reuse = $this->conn->sql_get_value('applicationScenario', 'isRetentionReuse',"scenarioId = '$scenario_id'");
		return $is_ret_reuse;
	}
	
	private function getProtectedHostList()
	{
		$protected_host_sql = "SELECT distinct sourceHostId FROM srcLogicalVolumeDestLogicalVolume";
		$protected_host_result = $this->conn->sql_exec($protected_host_sql);

		$protected_host_id = array();
		
		foreach($protected_host_result as $protected_host_info)
		{
			$protected_host_id[] = $protected_host_info['sourceHostId'];
		}
		return $protected_host_id;
	}
	
	/*
    * Function Name: convert_scientific_to_decimal
    *
    * Description: 
    * Convert Exponential data into equivalent decimal value
    *
    * Return Value:
    *     Ret Value: Scalar
    *
    * Exceptions:
    *   No Exception handled
    */
	
	public function convert_scientific_to_decimal($data)
	{
		$data = number_format(($data + 0),2,'.','');
		return $data;
	}
	
	/**
	 * This function will convert the single-dimensional array into a tree structure, 
	 * based on the delimiters found in it's keys.
	 *  
	 * @param array   $treeValues
	 * 
	 * @return array
	 */
	public function convertTree($treeValues)
	{
	    if(!is_array($treeValues)) return false;
		$resultArray = array();
	
		foreach ($treeValues as $key => $value) 
		{
			$treeElements = explode('$',$key);
			$leafElement = array_pop($treeElements);
			$treeElementArray = &$resultArray;
			$count = count($treeElements);
			for($i=0;$i<$count;$i++)
			{
				if (!isset($treeElementArray[$treeElements[$i]])) 
				{
					$treeElementArray[$treeElements[$i]] = array();
				} 
				$treeElementArray = &$treeElementArray[$treeElements[$i]];
			}
			
			if (!$treeElementArray[$leafElement]) 
			{
				$treeElementArray[$leafElement] = $value;
			} 
		}
		return $resultArray;
	}
	
	/*
	* This function will convert the XML string into array.
	*  
	* @param XML string
	* 
	* @return array corresponds to XML string
	*/
	public function getArrayFromXML($xml_str)
	{
		$result = array();
       		
		$obj = simplexml_load_string($xml_str,null,LIBXML_NOCDATA);
		if($obj === false) return $result;
		$rootElement = $obj->getName();
		$data = (array) $obj;
		$data = json_decode(json_encode($data), 1);
		if($rootElement == 'plan'){
			$result['plan']['data']['value'] = $data['data'];
		}elseif($rootElement == 'RulesInformation'){
			
			$index = 0;
			if(isset($data['RuleDetails'][$index]))
			{
				for($i=0;isset($data['RuleDetails'][$i]);$i++)
				{
					//For 'SourceId' Tag
					$result['RulesInformation']['RuleDetails'][$i]['SourceId'] = array();
					
					//For 'Rule' Tag
					if(isset($data['RuleDetails'][$i]['Rule'][$index]))
					{
						for($j=0;isset($data['RuleDetails'][$i]['Rule'][$j]);$j++)
						{
							foreach($data['RuleDetails'][$i]['Rule'][$j] as $k=>$v) {
								$result['RulesInformation']['RuleDetails'][$i]['Rule'][$j][$k]['value'] = $v;
							}
						}
					}
					else
					{
						foreach($data['RuleDetails'][$i]['Rule'] as $k=>$v) {
								$result['RulesInformation']['RuleDetails'][$i]['Rule'][$k]['value'] = $v;
							}
					}
				}
			}
			else
			{
				//For 'SourceId' Tag
				$result['RulesInformation']['RuleDetails']['SourceId'] = array();
				//For 'Rule' Tag
				if(isset($data['RuleDetails']['Rule'][$index]))
				{
					for($j=0;isset($data['RuleDetails']['Rule'][$j]);$j++)
					{
						foreach($data['RuleDetails']['Rule'][$j] as $k=>$v) {
							$result['RulesInformation']['RuleDetails']['Rule'][$j][$k]['value'] = $v;
						}
					}
				}
				else
				{
					foreach($data['RuleDetails']['Rule'] as $k=>$v) {
							$result['RulesInformation']['RuleDetails']['Rule'][$k]['value'] = $v;
						}
				}
				
			}
		}
		return $result;
	}
	
	public function get_error_template($template_id)
	{
		if(strpos($template_id,"RPOSLA_EXCD_") !== false)
		{
			$error_template_id = "RPOSLA_EXCD";
		}
		else if(strpos($template_id,"PROTECTION_") !== false)	
		{
			$error_template_id = "PROTECTION_ALERT";
		}
		else if(strpos($template_id,"NODE_DOWN_") !== false)
		{
			$error_template_id = "VXAGENT_DWN";
		}
		else
		{
			$error_template_id = $template_id;
		}
		return $error_template_id;
	}
    
	public function getServerTime()
	{
        $sql = "SELECT DATE_FORMAT(now(), '%M-%d-%Y% %H:%i') as present_time";
 	  	$sth = $this->conn->sql_query($sql);
	  	$row = $this->conn->sql_fetch_object($sth);
	    $server_time = $row->present_time;
        
        return $server_time;
	}
	
	public function getCertRootDir()
    {
		global $logging_obj;
		
		try
		{
			if (preg_match('/Windows/i', php_uname())) {
				
				$dirpath = getenv('ProgramData');
				
				if ($dirpath)
				{
					// check for program files (x86)
					$dirpath = join(DIRECTORY_SEPARATOR, array($dirpath, 'Microsoft Azure Site Recovery'));
					if (file_exists($dirpath))	return $dirpath;
					
					
					throw new Exception ('Unable to fetch Microsoft Azure Site Recovery under : ' . $dirpath);
				}
				else
				{
					throw new Exception ('Unable to find ProgramData path');
				}
			 } else {
				$dirpath = join(DIRECTORY_SEPARATOR, array(DIRECTORY_SEPARATOR . 'usr', 'local', 'InMage'));
				if (file_exists($dirpath))	return $dirpath;
				
				throw new Exception ('Unable to fetch the path: ' . $dirpath);
			}
		}
		catch (Exception $excep)
		{
			$logging_obj->my_error_handler("EXCEPTION", $excep->getMessage()."\n");			
		}
		
		return false;
    }
	
	/*
	* This function is used to read CS encryption key from global path
	*/	
	public function getCSEncryptionKey()
	{
		$topLevelDir = $this->getCertRootDir();		
		if ($topLevelDir)	return $this->readFile(join(DIRECTORY_SEPARATOR, array($topLevelDir, 'private', 'encryption.key')), true);
		
		return false;
	}

	/*
	* This function is used to read CS passphrase from global path
	*/	
	public function getPassphrase()
	{
		$topLevelDir = $this->getCertRootDir();		
		if ($topLevelDir)	return $this->readFile(join(DIRECTORY_SEPARATOR, array($topLevelDir, 'private', 'connection.passphrase')));
		
		return false;
	}
	
	/*
	* This function is used to give CS finger print
	*/	
	public function getCSFingerPrint()
	{
		global $CS_IP, $CS_PORT;
		$cs_ip = $CS_IP;
		$cs_port = 	$CS_PORT;
		$finger_print = $this->getServerFingerPrint($cs_ip,$cs_port);
		return $finger_print;
	}
	
	public function getServerFingerPrint($ip,$port)
	{
		global $logging_obj, $CX_TYPE, $CS_IP;
		$cx_type = $CX_TYPE;
		$cx_ip = $CS_IP;
		
		if ($cx_type == 2)//CX type == 2 PS alone
		{
			$finger_print_file_name = $ip.'_'.$port.'.fingerprint';
		}
		elseif ($cx_type == 3 || $cx_type == 1)//CX type == 3 CS and PS ,CX type == 1 CS alone
		{
			if($cx_ip == $ip)
			{
				$finger_print_file_name = 'cs.fingerprint';
			}
			else
			{
				$finger_print_file_name = $ip.'_'.$port.'.fingerprint';
			}	
		}
		else
		{
			$logging_obj->my_error_handler("INFO", "CX_TYPE is not set in amethyst"."\n");
			return false;
		}
		$topLevelDir = $this->getCertRootDir();		
		if ($topLevelDir)	return $this->readFile(join(DIRECTORY_SEPARATOR, array($topLevelDir, 'fingerprints', $finger_print_file_name)));
		return false;	
	}
	
	/*
	* This function is used to read file contents based on given file path
	*/	
	private function readFile($file_path, $is_binary = false)
	{
		global $logging_obj;
		try
		{
			$file_mode = $is_binary ? 'rb' : 'rt';
			
			if(! is_readable($file_path))	throw new Exception("File is not readable: $file_path");
			if(! $file_handle = $this->file_open_handle($file_path, $file_mode))	throw new Exception("Unable to open file: $file_path");
			$file_contents = fread($file_handle, 512);
			fclose($file_handle);
			
			return $is_binary ? $file_contents : trim($file_contents);
		}
		catch (Exception $excep){
			$logging_obj->my_error_handler("EXCEPTION", $excep->getMessage()."\n");
			return null;		
		}
	}
	
	/*
	* This function is used to validate the host 
	*/
	public function isValidComponent($id)
	{
        $result = FALSE;
		$sql = "SELECT count(*) AS validHost FROM hosts  WHERE id = ?";
 	  	$host_result = $this->conn->sql_query($sql, array($id));
		if($host_result[0]['validHost'] > 0) 
		{
			$result = TRUE;
		}
		return $result;
	}
	
	public function genRandNonce($count, $includeTimeStamp = false) 
	{
		global $logging_obj;
		try
		{
			$currentTime = gettimeofday();
			mt_srand($currentTime['sec'] * 1000000 + $currentTime['usec']);
			$nonce = '';
			if ($includeTimeStamp) {
				$nonce .= 'ts:' . time() . '-';
			}
			
			for ($i = strlen($nonce); $i < $count; $i++) {
				$randNum = rand();
				$nonce .= dechex(rand());
			}
			// could have generated more than count so return count bytes
			return substr($nonce, 0, $count - 1);
		}
		catch (Exception $exception){
			$logging_obj->my_error_handler("EXCEPTION", $exception->getMessage()."\n");
			return null;		
		}
	}
	
	/*
	* This function is used to get certification path 
	*/	
	public function getCertPath($ip,$port)
	{
		global $logging_obj, $CX_TYPE;
		
		$cx_type = $CX_TYPE;
		
		//CX type == 3 CS and PS ,CX type == 1 CS alone
		if ($cx_type == 3 || $cx_type == 1)
		{
			$cert_file_name = 'cs.crt';
		}
		else if ($cx_type == 2)//CX type == 2 PS alone
		{
			$cert_file_name = $ip.'_'.$port.'.crt';
		}
		else
		{
			$logging_obj->my_error_handler("INFO", "CX_TYPE is not set in amethyst"."\n");
			return false;
		}
		
		$topLevelDir = $this->getCertRootDir();		
		if ($topLevelDir)	
		{
			$cert_path = join(DIRECTORY_SEPARATOR, array($topLevelDir, 'certs', $cert_file_name));
			if (file_exists($cert_path))	return $cert_path;
			$logging_obj->my_error_handler("INFO", array("CertFileName"=>$cert_file_name, "Path"=>$cert_path), Log::BOTH);
		}	
		return false;
	}

    function insertRequestXml($requestXml,$functionName, $callerActivityId='', $callerClientRequestId='', $referenceId='')
	{
		global $activityId, $clientRequestId, $srsActivityId;
		// state : 1 = pending, 2 = in progress, 3 = success, 4 = failed
		
        if($requestXml)
        {
            $requestSerializeXml = serialize($requestXml);
        }
        else
        {
            $requestSerializeXml = $requestXml;			
        }
    
        $insertApiRequestSql = "INSERT INTO 
                                apiRequest 
                                        ( 
                                        requestType, 
                                        requestXml, 
                                        requestTime,
                                        state,
                                        activityId,
										clientRequestId,
										referenceId,
										srsActivityId) 
                                VALUES 
                                        (
                                         ?,
                                         ?,
                                        now(),
                                         1,
                                         ?,
										 ?,
										 ?,
										 ?)";
        
        $apiRequestId = $this->conn->sql_query($insertApiRequestSql, array($functionName, $requestSerializeXml, $activityId, $clientRequestId, $referenceId,$srsActivityId));
        
		return array('requestType' => $functionName, 'apiRequestId' => $apiRequestId);
	}
	
	/*
	 * Function Name: insertRequestData
	 *
	 * Description:
	 * this function will insert requested data into the apiRequestDetail.
	 *
	 * Parameters:
	 *      $scsiId,$apiRequestId,$functionName
	 *
	 *
	 * Return Value:
	 *     Ret Value: 
	 *
	 * Exceptions:
	 *
	 */

	public function insertRequestData($requestedData,$apiRequestId)
	{
        // state : 1 = pending, 2 = in progress, 3 = success, 4 = failed
        $requestdata = serialize($requestedData);
        $insertApiRequestDetailSql = "INSERT INTO 
                                        apiRequestDetail 
                                                ( 
                                                apiRequestId, 
                                                requestData, 
                                                state, 
                                                lastUpdateTime) 
                                        VALUES 
                                                (
                                                 ?,
                                                 ?,
                                                 1,
                                                 now()
                                                 )";  										
        $apiRequestDetailId = $this->conn->sql_query($insertApiRequestDetailSql, array($apiRequestId, $requestdata));
        
		return $apiRequestDetailId;
    }

	/* Function Name : set_agent_refresh_state
    *
    *   Description :
    *   This function will set refresh flag for all the hosts
    *
    *
    *   Parameters :
    *      
    *  Return Value:
    *   
    */
	public function set_agent_refresh_state()
	{
		global $REPLICATION_ROOT;
		$dbSourceHostList = $this->get_host_id_by_agent_role("Agent");
		$mbrPath = join(DIRECTORY_SEPARATOR, array($REPLICATION_ROOT, 'admin', 'web', 'SourceMBR'));
		
		if(count($dbSourceHostList) > 0) 
		{
			foreach($dbSourceHostList as $dbHostId => $latestMbrFile)
			{
				$src_os_flag = $this->get_osFlag($dbHostId);
				if($src_os_flag == 1)
				{
					if (empty($latestMbrFile) || !(file_exists($mbrPath . DIRECTORY_SEPARATOR . $dbHostId . DIRECTORY_SEPARATOR . $latestMbrFile))) 
					{	
						$update_refresh_status = "UPDATE hosts SET refreshStatus = 0, latestMBRFileName = '' WHERE id = ?";
						$update_status = $this->conn->sql_query($update_refresh_status,array($dbHostId));
					}
				}
			}
		}
	}
	
	/* Function Name : getErrorDetails
    *
    *   Description :
    *   This function will return the possibleCauses and correctiveAction
    *		based on the errCode passed.
    *
    *   Parameters :
    *      
    *  Return Value:
    *   
    */
	public function getErrorDetails($error_code, $place_holders=array(), $error_type = null, $category = 'API Errors')
	{
		$sql_params = array();
		$sql_where = null;
		
		if (! empty($error_code))
		{
			$sql_where = "errCode = ?";
			$sql_params[] = $error_code;
		}
		elseif (! empty($error_type))
		{
			$sql_where = "errType = ? AND category = ?";
			array_push($sql_params, $error_type, $category);
		}
		else
		{
			return array();
		}
	
		$sql = "SELECT errorMsg, apiCode, errCorrectiveAction, errPossibleCauses FROM eventCodes WHERE " . $sql_where;
		$error_details = $this->conn->sql_query($sql, $sql_params);
		
		if(count($error_details))
		{
			/* SQL query returns array. 
			*	As the error code is primary key we get only one record.
			*	or
			*	combination of errType AND category should be unique
			*	So just assigning the 0th element back
			*/		
			$error_details = $error_details[0];
			
			if (is_array($place_holders))
			{
				foreach($place_holders as $key => $value)
				{
					$error_details['errCorrectiveAction'] = str_replace("<$key>", $value, $error_details['errCorrectiveAction']);
					$error_details['errPossibleCauses'] = str_replace("<$key>", $value, $error_details['errPossibleCauses']);
					$error_details['errorMsg'] = str_replace("<$key>", $value, $error_details['errorMsg']);
				}
			}
			else
			{
				$error_details['errCorrectiveAction'] = str_replace("#text#", $value, $error_details['errCorrectiveAction']);
				$error_details['errPossibleCauses'] = str_replace("#text#", $value, $error_details['errPossibleCauses']);
				$error_details['errorMsg'] = str_replace("#text#", $value, $error_details['errorMsg']);
			}
		}
		
		return $error_details;
	}

	/* Function Name : getErrorCode
    *
    *   Description :
    *   This function will return the global shared error code and API error code
    *		based on the error type passed. Optionally category can be passed to fetch.
    *
    *   Parameters :
    *      
    *  Return Value:
	*	Array(<Global error code>, <API code>)
    *   
    */	
	public function getErrorCode($error_type, $category = 'API Errors')
	{
		$sql = "SELECT errCode, apiCode FROM eventCodes WHERE errType = ? AND category = ?";
		$error_details = $this->conn->sql_query($sql, array($error_type, $category));	

		/* SQL query returns array. 
		*	As the combination of errType AND category should be unique
		*	So just assigning the 0th element back
		*/	
		if(count($error_details))	$error_details = $error_details[0];

		return empty($error_details) ? array('UNKNOWN', ErrorCodes::EC_UNKNWN) : array($error_details['errCode'], $error_details['apiCode']);
	}
	
	public function getPatchDetails($list_components = 0)
	{
		$installer_details = array();
		$patch_sql = "SELECT * from patchDetails WHERE CAST(REPLACE(patchVersion,'.','') AS UNSIGNED) IN (SELECT max(cast(REPLACE(patchVersion,'.','') AS UNSIGNED)) FROM patchDetails GROUP BY targetOs) ORDER BY osType";
		$patch_data = $this->conn->sql_query($patch_sql);
		
		$linux_set = 0;
		foreach($patch_data as $patch_info)
		{
			if($list_components == 1 && $linux_set == 1) continue;
			
			$os_type = $patch_info["targetOs"];
			
			$installer_details[$os_type]["patchVersion"] = $patch_info["patchVersion"];
			$installer_details[$os_type]["patchFilename"] = $patch_info["patchFilename"];
			$installer_details[$os_type]["updateType"] = $patch_info["updateType"];
			$installer_details[$os_type]["rebootRequired"] = $patch_info["rebootRequired"];
			$installer_details[$os_type]["osType"] = $patch_info["osType"];
			if($patch_info["osType"] != 1) $linux_set = 1;
		}
		return $installer_details;
	}
	
	public function getPatchVersion($is_cx = 0)
	{
		if($is_cx == 1)
		{
			$cx_version_arr = array();
			
			# Fetching cx base version details.
			$cx_version = $this->get_inmage_version_array();
			$cs_base_version_details = explode("_",$cx_version[0]);
			$comp_version = $cs_base_version_details[1];
			
			# Fetching cx patch version details.
			$cx_patch_details = $this->get_cx_patch_history(); 
			if(!empty($cx_patch_details)) 
			{ 
				foreach($cx_patch_details as $update_info) 
				{ 
					$update_install_datetime = explode("_", $update_info[1]); 
					$patch_version_details = explode("_",$update_info[0]);
					$cx_version_arr[] = str_replace(".","",$patch_version_details[2]);
				}
			}
			array_push($cx_version_arr, str_replace(".","",$comp_version));
			$version_data = max($cx_version_arr);
		}
		else
		{
			$sql = "SELECT id, PatchHistoryVX FROM hosts";
			$agent_details = $this->conn->sql_get_array($sql, "id", "PatchHistoryVX");
			
			$version_data = array();
			foreach($agent_details as $host_id => $vxPatch)
			{
				$patch_version_arr = array();
				$patchHistoryVX = explode ("|", $vxPatch);
				foreach ($patchHistoryVX as $value) 
				{ 
					$patch_array = explode(",",$value); 
					$vx_patch_version_details = explode("_", $patch_array[0]);
					$patch_version_arr[] = str_replace(".","",$vx_patch_version_details[2]);
				}
				$latest_version = max($patch_version_arr);
				$version_data[$host_id] = $latest_version;
			}			
		}
		return $version_data;
	}
	
	// To check protection is under rollback progress or not.
	function is_protection_rollback_in_progress($id)
	{
		global $ROLLBACK_INPROGRESS, $logging_obj, $ROLLBACK_COMPLETED;
		$roll_back_in_progress = 0;
		$sql_state = "select scenarioId from applicationScenario where scenarioType = 'Rollback' and sourceId = '$id' and (executionStatus = $ROLLBACK_INPROGRESS OR executionStatus =$ROLLBACK_COMPLETED)";
		$result_set = $this->conn->sql_query($sql_state);
		$num_rows = $this->conn->sql_num_row($result_set);
		if($num_rows)
		{
			$roll_back_in_progress = 1;
		}
		$logging_obj->my_error_handler("INFO","is_protection_rollback_in_progress:: $sql_state, status: $roll_back_in_progress \n");
		return $roll_back_in_progress;
	}
	
	function get_disk_name($hostId, $deviceName, $osFlag)
	{
		$sql_state = "select deviceProperties from logicalVolumes where hostId = '$hostId' and deviceName = '$deviceName'";
		$result_set = $this->conn->sql_query($sql_state);
		$num_rows = $this->conn->sql_num_row($result_set);
		$disk_name = "";
		if($num_rows)
		{
			foreach($result_set as $vol_data)
			{
				$device_properties = $vol_data["deviceProperties"];
				$disk_properties = unserialize($device_properties);
				$display_name = $disk_properties['display_name'];
				if ($osFlag == "1")
				{
					$match_status = preg_match("/DRIVE(.*)/", $display_name, $match_res);
					if ($match_status)
					{
						$disk_number = $match_res[1];
					}
					else
					{
						$disk_number = $display_name;
					}
					$disk_name = "Disk".$disk_number;
				}
				else 
				{
					$disk_name = $display_name;
				}
			}
		}
		return $disk_name;
	}
	
	function get_encrypted_disks($hostId)
	{
		$diskList = array();
		$encryptedDisk = array();
		$sql = "select deviceName, deviceProperties from logicalVolumes where hostId = '$hostId' and systemVolume = 1 and volumeType = 5 and deviceProperties LIKE '%\"encryption\"%'";
		$result = $this->conn->sql_query($sql);
		$num_rows = $this->conn->sql_num_row($result);
		
		if($num_rows)
		{
			foreach($result as $data)
			{
				$device_properties = $data["deviceProperties"];
				$deviceName = $data["deviceName"];
				$disk_properties = unserialize($device_properties);
				if($disk_properties['encryption'])
				{
					$encryptedDisk["$deviceName"] = $this->conn->sql_get_value('diskgroups', 'diskGroupName', 'deviceName = ? AND hostId = ?', array($deviceName, $hostId));
				}
			}
		}
		return $encryptedDisk;
	}
	
	/*
	*Public function for file open to access across the class and procedural methods.
	*Siganture parameters:
	*$file_path = full file path of symbolic link(C:\ProgramData\ASR), not the real path of installation location.
	*$mode = File open modes(read, write, append etc..). Optional parameter and default is read mode.
	*$retry_limit = Default retry limit of 5 times. Optional parameter and default value is 5.
	*$sleep_between_retry = Sleep between retry is 1 sec. Optional parameter and defualt value is 1.
	*/
	public function file_open_handle($file_path, $mode = "r", $retry_limit = 5, $sleep_between_retry = 1)
	{
		global $MDS_ACTIVITY_ID, $conn_obj;
		$file_handle = FALSE;
		//Normalize to actual windows path.
		$file_path = $this->verifyDatalogPath($file_path);
		
		$directory = dirname($file_path);
		if(!file_exists($directory))
		{
			mkdir($directory,0777,true);
		}	
		clearstatcache();
		//File open retry block
		$retry = 0;
		while (($file_handle == FALSE) && ($retry < $retry_limit)) 
		{
			//Supressing warning with @ and capturing the status of file open in $file_handle.
			$file_handle = @fopen($file_path, $mode);
			if ($file_handle == FALSE)
			{
				sleep($sleep_between_retry);
				$retry++;
			}
		}
		//If file handler doesn't get after re-tries, then log to MDS.
		if (($retry == $retry_limit) && ($file_handle == FALSE))
		{
			$error_reason = print_r(error_get_last(),TRUE);
			$trace = debug_backtrace();
			$caller_info = '';
			$trace_size = sizeof($trace);
			for($i = 0; $i<$trace_size; $i++)
			{
				$file_name = basename($trace[$i]['file']);
				$fun_name = $trace[$i]['function'];
				$caller_info .= ($i+1).'#'.$file_name.'#'.$fun_name.'|';
			}	
			$caller_info = $caller_info."File $file_path open failed in mode $mode with retry $retry with reason $error_reason";
			$caller_info = $conn_obj->sql_escape_string($caller_info);
			$eventName = "CS";	
			$mds_data_array = array();
			$this->mds_obj = new MDSErrorLogging();
			$mds_data_array["activityId"] = $MDS_ACTIVITY_ID;
			$mds_data_array["jobId"] = "";
			$mds_data_array["jobType"] = "Protection";
			$mds_data_array["errorString"] = $caller_info;
			$mds_data_array["eventName"] = $eventName;
			$mds_data_array["errorType"] = "ERROR";
			$this->mds_obj->saveMDSLogDetails($mds_data_array);	
		}
		return $file_handle;
	}

	/*
	*Public function for file read content to access across the class and procedural methods.
	*Siganture parameters:
	*$file_path = full file path of symbolic link(C:\ProgramData\ASR), not the real path of installation location.
	*$retry_limit = Default retry limit of 5 times. Optional parameter and default value is 5.
	*$sleep_between_retry = Sleep between retry is 1 sec. Optional parameter and defualt value is 1.
	*Return value is file content structure if file and data exists, Otherwise empty structure of type structure.
	*/
	public function file_read_contents($file_path, $retry_limit = 5, $sleep_between_retry = 1)
	{
		global $MDS_ACTIVITY_ID, $conn_obj;
		$file_content = FALSE;
		//Normalize to actual windows path.
		$file_path = $this->verifyDatalogPath($file_path);
		$result = array();
		if(file_exists($file_path))
		{
			//File read retry block
			$retry = 0;
			while (($file_content == FALSE) && ($retry < $retry_limit)) 
			{
				//Supressing warning with @ and capturing the status of file read in $file_content.
				$file_content = @file($file_path);
				if ($file_content == FALSE)
				{
					sleep($sleep_between_retry);
					$retry++;
				}
			}
			//If file handler doesn't get after re-tries, then log to MDS.
			if (($retry == $retry_limit) && ($file_content == FALSE))
			{
				$error_reason = print_r(error_get_last(),TRUE);
				$trace = debug_backtrace();
				$caller_info = '';
				$trace_size = sizeof($trace);
				for($i = 0; $i<$trace_size; $i++)
				{
					$file_name = basename($trace[$i]['file']);
					$fun_name = $trace[$i]['function'];
					$caller_info .= ($i+1).'#'.$file_name.'#'.$fun_name.'|';
				}	
				$caller_info = $caller_info."File $file_path read content failed with retry $retry with reason $error_reason";
				$caller_info = $conn_obj->sql_escape_string($caller_info);
				$eventName = "CS";	
				$mds_data_array = array();
				$this->mds_obj = new MDSErrorLogging();
				$mds_data_array["activityId"] = $MDS_ACTIVITY_ID;
				$mds_data_array["jobId"] = "";
				$mds_data_array["jobType"] = "Protection";
				$mds_data_array["errorString"] = $caller_info;
				$mds_data_array["eventName"] = $eventName;
				$mds_data_array["errorType"] = "ERROR";
				$this->mds_obj->saveMDSLogDetails($mds_data_array);	
				// Erases the output buffer if any.
				ob_clean();
				
				$GLOBALS['http_header_500'] = TRUE;
				
				// Send 500 response 
				header("HTTP/1.0 500 Internal server error: File open failed with retry $retry. Please re-try job again.", true);
				
				exit(0);
			}
			clearstatcache();
		}
		if (is_array($file_content))
		{
			$result = $file_content; 
		}
		return $result;
	}

	public function captureDeadLockDump($requestXML, $activityId, $jobType)
	{
		global $DB_ROOT_USER, $DB_ROOT_PASSWD, $DB_DATABASE_NAME, $MDS_ACTIVITY_ID;
		$status = `mysql -e "show engine innodb status" -u $DB_ROOT_USER -p$DB_ROOT_PASSWD $DB_DATABASE_NAME`;
		$log_string = "";
		preg_match('/\<AccessKeyID\>(.*?)\<\/AccessKeyID\>/', $requestXML, $match);
		if (count($match) > 0)
		{
			$actual_string = $match[1];
			$full_string = substr_replace($actual_string, "#####",-5, 5);
			$log_string = preg_replace("/$actual_string/sm", $full_string, $requestXML);
		}
		preg_match('/\<AccessSignature\>(.*?)\<\/AccessSignature\>/', $requestXML, $match);
		if (count($match) > 0)
		{
			$actual_string = $match[1];
			$log_string = preg_replace("/$actual_string/sm", "xxxxx", $log_string);
		}
		$mds_data_array["jobId"] = "";
		if ($activityId)
		{
			$activity_id_to_log = $activityId;
		}
		else
		{
			$activity_id_to_log = $MDS_ACTIVITY_ID;
		}
		$mds_data_array["activityId"] = $activity_id_to_log;
		$mds_data_array["jobType"] = $jobType;
		$mds_log = "Deadlock dump: $status, Activity id: $activity_id_to_log, Request XML: $log_string";
		$mds_data_array["errorString"] = $mds_log;
		$mds_data_array["eventName"] = "CS_BACKEND_ERRORS";
		$mds_data_array["errorType"] = "ERROR";
		$this->mds_obj = new MDSErrorLogging();
		$this->mds_obj->saveMDSLogDetails($mds_data_array);
	}
	
	public function getAllPsDetails()
	{
		$ps_details = array();
		$ps_enabled_flag = 1;
		$ps_sql = "select id, name, ipaddress, compatibilityNo from hosts where processServerEnabled = ?";
		$ps_data=$this->conn->sql_query($ps_sql, array($ps_enabled_flag));
		foreach($ps_data as $key=>$ps_info)
		{
			$ps_id = $ps_info["id"];			
			$ps_details[$ps_id]["name"] = $ps_info["name"];
			$ps_details[$ps_id]["ipaddress"] = $ps_info["ipaddress"];
			$ps_details[$ps_id]["compatibilityNo"] = $ps_info["compatibilityNo"];
		}
		return $ps_details;
	}
	
	public function getUefiHosts()
	{
		$sql_args = array("0");
		$uefi_hosts_list = array();
        $get_uefi_hosts_query = "SELECT
                                hostId
                             FROM   
                                logicalVolumes
                             where 
								volumeType=? 
								and 
								deviceProperties REGEXP '\"has_uefi\";s:4:\"true\"'";
		
        $uefi_hosts = $this->conn->sql_query($get_uefi_hosts_query, $sql_args);
		foreach($uefi_hosts as $uefi_host_key=>$uefi_host)
		{
			$host_id = $uefi_host["hostId"];			
			$uefi_hosts_list[] = trim(strtoupper($host_id));
		}
		$uefi_hosts_list = array_unique($uefi_hosts_list);
        return $uefi_hosts_list;	
	}
	
	public function get_debug_back_trace()
	{
		$caller_info ='';
		$trace = debug_backtrace();
		$trace_size = sizeof($trace);
		for($i = 0; $i<$trace_size; $i++)
		{
			$file_name = basename($trace[$i]['file']);
			$fun_name = $trace[$i]['function'];
			$caller_info .= ($i+1).'#'.$file_name.'#'.$fun_name.'|';
		}
		return $caller_info;
	}
	
	public function mask_authentication_headers($requestXML)
	{
		$log_string = $requestXML;
		preg_match('/\<AccessKeyID\>(.*?)\<\/AccessKeyID\>/', $requestXML, $match);
		if (count($match) > 0)
		{
			$actual_string = $match[1];
			$full_string = substr_replace($actual_string, "#####",-5, 5);
			$log_string = preg_replace("/$actual_string/sm", $full_string, $requestXML);
		}
		preg_match('/\<AccessSignature\>(.*?)\<\/AccessSignature\>/', $log_string, $match);
		if (count($match) > 0)
		{
			$actual_string = $match[1];
			$log_string = preg_replace("/$actual_string/sm", "#####", $log_string);
		}
		return $log_string;
	}
	
	public function mask_credentials($XMLData)
	{
		$response_password_xml_pattern = '/(Name=\"Password\"\s+Value=\")(.*)(\")|(Parameter\s+Value=\")(.+)("\s+Name=\"Password\")/';
		$XMLData = preg_replace_callback($response_password_xml_pattern, function($matches) { return isset($matches[4]) ? $matches[4].'*****'.$matches[6] : $matches[1].'*****'.$matches[3]; } , $XMLData);
		
		$response_user_xml_pattern = '/(Name=\"UserName\"\s+Value=\")(.*)(\")|(Parameter\s+Value=\")(.+)("\s+Name=\"UserName\")/';
		$XMLData = preg_replace_callback($response_user_xml_pattern, function($matches) { return isset($matches[4]) ? $matches[4].'*****'.$matches[6] : $matches[1].'*****'.$matches[3]; } , $XMLData);
		return $XMLData;
	}
	
	/*
	 * Function Name: parallel_consistency_supported
	 *
	 * Description:
	 * this function get to know the parallel consistency support exists or not based on compatibility check across all agents. If any one of the agents are old, then it 
	 * sends parallel consistency support as FALSE, otherwise TRUE.
	 *
	 * Parameters:
	 * List of policy host ids.
	 *      $host_ids
	 *
	 *
	 * Return Value:
	 *     Ret Value: TRUE or FALSE
	 *
	 * Exceptions:
	 *
	 */
	public function parallel_consistency_supported($host_ids)
    {  
		global $PARALLEL_CONSISTENCY_SUPPORT_VERSION;
		$parallel_consistency_support = TRUE;
		$sql = "SELECT
					id, 
                    compatibilityNo
                  FROM
                    hosts
                  WHERE
                    FIND_IN_SET(id, ?)";		
		$resultset = $this->conn->sql_query ($sql, array($host_ids));
		foreach ($resultset as $key=>$value)
		{			
			if ($value[compatibilityNo] < $PARALLEL_CONSISTENCY_SUPPORT_VERSION)
			{
				$parallel_consistency_support = FALSE;
				break;
			}
		}
		return $parallel_consistency_support;
	}
	
	/*
	 * Function Name: get_infrastructure_id_for_hostid
	 *
	 * Description:
	 * this function get to the infrastructure vm id based on the given host id from discovery table.
	 *
	 * Parameters:
	 * Host identity.
	 *      $host_id
	 *
	 *
	 * Return Value:
	 *     Ret Value: Array of matched infrastructure VM id.
	 *
	 * Exceptions:
	 *
	 */
	public function get_infrastructure_id_for_hostid($host_id)
    {  
		$infra_vm_id_arr = array();
		$check_mapping_sql = "SELECT infrastructureVMId from infrastructureVms where hostId = ?";
		$infra_vm_ids = $this->conn->sql_query($check_mapping_sql,array($host_id));
		foreach ($infra_vm_ids as $key=>$value)
		{
			$infra_vm_id_arr[] = $value['infrastructureVMId'];
		}
		return $infra_vm_id_arr;
	}
	
	/* To get the application scenario data */
	public function get_application_scenario_data($host_id)
	{
		global $logging_obj;
		$app_scenario_data = array();
		$sql = "select scenarioId, planId from applicationScenario where sourceId = ?";
		$app_scenario_record = $this->conn->sql_query($sql,array($host_id));
		
		foreach ($app_scenario_record as $key=>$value)
		{
			$app_scenario_data['scenarioId'] = $value['scenarioId'];
			$app_scenario_data['planId'] = $value['planId'];
		}
		return $app_scenario_data;
	}
	
	/*
	Dropping multiple slashes and doing unique.
	*/
	public function multiple_slashes_unique($str)
	{
	  if(strpos($str,'\\\\')!==false)
	  {
		 return multiple_slashes_unique(str_replace('\\\\','\\',$str));
	  }
	  return $str;
	}

	/*
	Validting file path with required rules after sanitzation
	*/
	public function is_valid_file_path($file_path)
	{
		global $CS_INSTALLATION_PATH, $ACTUAL_INSTALLATION_DIR, $logging_obj;
		#$logging_obj->my_error_handler("INFO","is_valid_file_path status is: $CS_INSTALLATION_PATH,$ACTUAL_INSTALLATION_DIR  \n");
		
		//File path sanitzation
		$file_path = str_replace("/","\\",$file_path);
		$santized_file_path = $this->multiple_slashes_unique($file_path);
		
		//If file path is having . or .. not allowing further to continue.
		if ((strpos($santized_file_path, '.') !== false) ||
			(strpos($santized_file_path, '..') !== false))
		{	
			#$logging_obj->my_error_handler("INFO","Invalid file path $santized_file_path, hence returning from here. \n\n");
			return false;
		}
		
		//Preparing allowed directory list for both C and custom drive.
		//Check allowed directory list paths exists in file path or not.
		//C drive
		$allowed_dirs[] = $CS_INSTALLATION_PATH."\home\svsystems";
		//Custom dirve
		$allowed_dirs[] = $ACTUAL_INSTALLATION_DIR;		
		$allowed_dir_flag = false;
		foreach ($allowed_dirs as $value)
		{
			if (strpos($santized_file_path,$value) !== false)
			{
				$allowed_dir_flag = true;
			}
		}
		if ($allowed_dir_flag == true)
		{
			#$logging_obj->my_error_handler("INFO","$santized_file_path is available in allowed directory. \n");
		}
		else
		{
			#$logging_obj->my_error_handler("INFO","$santized_file_path is not available in allowed directory. Hence returning from here. \n\n");
			return false;
		}
		
		//Prepating disallowed list of directories of both C and custom drive.
		$actual_installation_dir_before_home = str_replace("\home\svsystems","",$ACTUAL_INSTALLATION_DIR);
		$disallowed_dirs = array("\\home\\svsystems\\admin","\\home\\svsystems\\admin\\web","\\home\\svsystems\\etc","\\home\\svsystems\\var","\\home\\svsystems\\PushInstallJobs","\\home\\svsystems\\bin","\\home\\svsystems\\pushinstallsvc", "\\home\\svsystems\\transport","\\home\\svsystems\\pm");
		foreach ($disallowed_dirs as $value)
		{
			$disallowed_dirs_in_c_drive[] = $CS_INSTALLATION_PATH.$value;
		}
		foreach ($disallowed_dirs as $value)
		{
			$disallowed_dirs_in_custom_drive[] = $actual_installation_dir_before_home.$value;
		}

		//Merging both the structures of C drive and custom drive disallowed directories.
		$disallowed_dirs_list = array_merge($disallowed_dirs_in_c_drive, $disallowed_dirs_in_custom_drive);
		//Doing filtering for unique values. In case customer installed in C drive instead custom drive.
		$disallowed_dirs = array_filter($disallowed_dirs_list);
		
		$disallowed_dir_flag = false;
		foreach ($disallowed_dirs as $value)
		{
			if (strpos($santized_file_path,	$value) !== false)
			{
				$disallowed_dir_flag = true;
			}
		}
		if ($disallowed_dir_flag == true)
		{
			#$logging_obj->my_error_handler("INFO","$santized_file_path is available in disallowed directories list, hence returning from here \n\n");
			return false;
		}
		
		#$logging_obj->my_error_handler("INFO","returning true");
		return true;
	}
	
	/*
	Validating GUID format
	*/
	public function is_guid_format_valid($guid)
	{
		$valid_guid = false;
		
		//Guid length is either 35 or 36 based on below two formats.
		//5a5617c3-dca6-478d-a446-08da1624f4d4 (or) 82839A07-4DEA-1B4F-AB36611F34A7E957 
		if (preg_match('/^\{?[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}\}?$/', $guid)
		||
		preg_match('/^\{?[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{16}\}?$/', $guid)
		) 
		{
			$valid_guid = true;
		} 
		
		return $valid_guid;
	}
	
	/*
	* This function is used to validate the host id of source component.
	*/
	public function isValidSourceComponent($id)
	{
        $result = FALSE;
		$sql = "SELECT 
					count(*) AS validHost 
				FROM 
					hosts  
				WHERE 
					id = ? 
				AND 
					sentinelEnabled = ?";
 	  	$host_result = $this->conn->sql_query($sql, array($id,1));
		if($host_result[0]['validHost'] > 0) 
		{
			$result = TRUE;
		}
		return $result;
	}
	
	/*
	* This function is used to validate it is valid protection or not.
	*/
	public function isValidProtection($id, $device_name, $type)
	{
		global $SOURCE_DATA, $TARGET_DATA;
		if ($type == $SOURCE_DATA)
		{
			$cond = "	sourcehostid = ? 
					AND 
						sourcedevicename = ?";
		}
		elseif ($type == $TARGET_DATA)
		{
			$cond = "	destinationhostid = ? 
					AND 
						destinationdevicename = ?";
		}
		$args = array($id, $device_name);
        $result = FALSE;
		
		$sql = "SELECT 
					count(*) AS validProtection  
				FROM 
					srclogicalvolumedestlogicalvolume 
				WHERE".$cond;
 	  	$protection_result = $this->conn->sql_query($sql, $args);
		if (is_array($protection_result) && count($protection_result) > 0)
		{
			if($protection_result[0]['validProtection'] > 0) 
			{
				$result = TRUE;
			}
		}
		return $result;
	}
	
	/**
	* find_by_pair_details_in_resync. A user defined or custom findbyMethod  
	*
	* @param params       associate array: sql will be searched and replaced by the matching values
	*					   associated with the token
	*/
	function find_by_pair_details_in_resync($params = array()) 
	{
		$cond = "sourceHostId = ?  and  sourceDeviceName = ? and destinationHostId= ? and destinationDeviceName = ? and isResync = 1 ";
		$sql = "SELECT  
			resyncStartTagtime, resyncStartTagtimeSequence, resyncEndTagtime, resyncEndTagtimeSequence
		FROM
			srcLogicalVolumeDestLogicalVolume 
		WHERE
			(" . $cond . ")";
		
		$searchResults = array();
		$result = $this->conn->sql_query($sql, array($params['srcid'], $params['srcdevicename'], $params['destid'], $params['destdevicename']));
		foreach ($result as $row) 
		{
			array_push($searchResults, $row);
		}
		
		return $searchResults;
	}
	
	/**
	* find_by_pair_details. A user defined or custom findbyMethod  
	*
	* @param params       associate array: sql will be searched and replaced by the matching values
	*					   associated with the token
	*/
	function find_by_pair_details($params = array()) 
	{
		$cond = "sourceHostId= ?  and  sourceDeviceName = ? and destinationHostId= ? and destinationDeviceName = ?";
		$sql = "SELECT  
			resyncEndTime, jobId, fullSyncStartTime, fullSyncBytesSent, resyncEndTagtime, lastResyncOffset, isQuasiflag, resyncVersion, fastResyncMatched, fastResyncUnmatched, fastResyncUnused    
		FROM
			srcLogicalVolumeDestLogicalVolume 
		WHERE
			(" . $cond . ")";
		
		$searchResults = array();
		$result = $this->conn->sql_query($sql, array($params['srcid'], $params['srcdevicename'], $params['destid'], $params['destdevicename']));
		foreach ($result as $row) 
		{
			array_push($searchResults, $row);
		}
		
		return $searchResults;
	}
	
	/**
	* getLvDetails. A user defined or custom findbyMethod  
	*
	* @param host_id       
	* Return values: associate array: sql will result required logicalvolumes columns data.
	*/
	public function getLvDetails($host_id)
	{
		$lv_details = array();
		$sql = "select devicename from logicalvolumes where hostid = ?";
		$lv_data=$this->conn->sql_query($sql, array($host_id));
		foreach($lv_data as $key=>$lv_info)
		{			
			$lv_details['devicename'][] = $lv_info["devicename"];
		}
		return $lv_details;
	}
	
	/* Header to throw 400 bad request for invalid post data */
	public function bad_request_response($str)
	{
		global $logging_obj;
		$logging_obj->my_error_handler("INFO", "bad_request_response reason:".$str, Log::BOTH);
		header("HTTP/1.0 400 Bad request: ".$str, TRUE, 400);
		header('Content-Type: text/plain');
		exit(0);
	}

	/*
	* This function is used to validate UpdateDB API input.
	*/
	public function validateUpdateDBInput(string $input)
	{
		global $INFRA_HF_ERR_LIST, $GUID_FMT_COL_LIST, $NUM_FMT_COL_LIST, $PAIR_HE_LIST;

		// Split $input with delimiter(=, IN, >)
		$condition = preg_split('/\s*=\s*|\s+IN\s+|\s*>\s*/i', $input);

		// Throw error if the condition doesn't contain delimiters(=, IN, >).
		if (count($condition) < 2)
		{
			ErrorCodeGenerator::raiseError(
				'COMMON', 'EC_INVALID_DATA', array('Parameter' => "Condition - ".$input));
		}

		$colName = trim($condition[0]);
		$colValue = trim($condition[1]);
		// Throw error if column name or value is empty
		if (!isset($colName) || $colName == "" ||
				!isset($colValue) || $colValue == "")
		{
			ErrorCodeGenerator::raiseError(
				'COMMON', 'EC_INVALID_DATA', array('Parameter' => "Condition - ".$input));
		}

		// Remove starting and trailing single quotes from value
		$colValue = rtrim($colValue, '\'');
		$colValue = ltrim($colValue, '\'');
		if (!isset($colValue) || $colValue == "")
		{
			ErrorCodeGenerator::raiseError(
				'COMMON', 'EC_INVALID_DATA', array('Parameter' => "Condition - ".$input));
		}

		$raiseError = false;

		if (in_array(strtolower($colName), $NUM_FMT_COL_LIST))
		{
			$colValStr = (string)$colValue;
			if (!preg_match('/^\d+$/',$colValStr))
			{
				$raiseError = true;
			}
		}
		elseif (in_array(strtolower($colName), $GUID_FMT_COL_LIST) &&
				!($this->is_guid_format_valid($colValue)))
		{
			$raiseError = true;
		}
		elseif (strcasecmp($colName, "component") == 0 &&
				$colValue != 'Space Constraint')
		{
			$raiseError = true;
		}
		elseif (strcasecmp($colName, "errCode") == 0 &&
				$colValue != $PAIR_HE_LIST)
		{
			$raiseError = true;
		}
		elseif (strcasecmp($colName, "healthFactorCode") == 0 &&
				(!in_array($colValue, $INFRA_HF_ERR_LIST)))
		{
			$raiseError = true;
		}
		elseif (strcasecmp($colName, "logName") == 0 &&
				(!is_string($colValue)))
		{
			$raiseError = true;
		}

		if ($raiseError)
		{
			ErrorCodeGenerator::raiseError(
				'COMMON', 'EC_INVALID_DATA', array('Parameter' => $colName."-".$colValue));
		}
	}
	
	/*
	* This function is used to validate UpdateDB API condition and additional info datas input.
	*/
	public function validateUpdateDBAdditionalInputs(string $input)
	{
		/*
			Example for one pattern \s*(;)\s*\bdrop\b\s* details are as below:
			one or more spaces + ; + exact match of drop in lower or uppercase + one or more spaces
			If the condition and additionalinfo strings contains any of below pattern data's identifies,
			the API throws the error as INVALID data.
			;drop table
			; drop table
			; DROP table
			;drop database
			; drop database
			;DROP database
			The patterns have been defined same as above for remaining MySQL SQL statements 
			key words(alter, create, show, lock, load, grant, revoke, rename) too.
		*/
		if (preg_match('/\s*(;)\s*\bdrop\b\s*|
						\s*(;)\s*\balter\b\s*|
						\s*(;)\s*\bcreate\b\s*|
						\s*(;)\s*\bshow\b\s*|
						\s*(;)\s*\block\b\s*|
						\s*(;)\s*\bload\b\s*|
						\s*(;)\s*\bgrant\b\s*|
						\s*(;)\s*\brevoke\b\s*|
						\s*(;)\s*\brename\b\s*/i', $input))
		{
			ErrorCodeGenerator::raiseError(
				'COMMON', 'EC_INVALID_DATA', array('Parameter' => $input));
		}
	}
}
?>
