<?php
/*
 *=================================================================
 * FILENAME
 *  SRM.php
 *
 * DESCRIPTION
 *  This script contains all the SRM 
 *  calls which are invoked by user.
 *
 *=================================================================
 *                 Copyright (c) InMage Systems
 *=================================================================
*/

class SRM
{
 
	/*
     * Function Name: VolumeProtection
     *
     * Description:
     *               Constructor
     *               It initializes the private variable
    */
	
	public function __construct()
	{
			global $conn_obj;
			$this->conn = $conn_obj;
			$this->commonfunctions_obj = new CommonFunctions();
			
	}
	
	
	 
     /*
	 * Function Name: insert_request_xml
	 *
	 * Description:
	 * this function will insert requested XML into the apiRequest.
	 *
	 * Parameters:
	 *      $requestXml,$requestType,$md5CheckSum,
	 *
	 *
	 * Return Value:
	 *     Ret Value: $apiRequestId
	 *
	 * Exceptions:
	 *
	 */

	public function insert_request_xml($requestXml,$requestType,$md5CheckSum)
	{
		// state : 1 = pending, 2 = in progress, 3 = success, 4 = failed
		$selectMd5Sql = "SELECT 
							md5CheckSum ,
							state,
							apiRequestId 	
						FROM 
							apiRequest 
						WHERE 
							md5CheckSum = '$md5CheckSum' AND 
							requestType = '$requestType' AND 
							state IN (1,2)";
		
		$rsMd5 = $this->conn->sql_query($selectMd5Sql);
		if (!$rsMd5)
		{
		  # return error.
		  throw new Exception('apiRequest SELECT');
		}
		$rowCount = $this->conn->sql_num_row($rsMd5);
		if(!$rowCount)
		{
			$eventTime = gmdate("Y m d H i s",time());
			$time    = explode(" ",$eventTime);
			$year    = str_pad($time[0],4,"0",STR_PAD_LEFT);
			$month   = str_pad($time[1],2,"0",STR_PAD_LEFT);
			$day     = str_pad($time[2],2,"0",STR_PAD_LEFT);
			$hour    = str_pad($time[3],2,"0",STR_PAD_LEFT);
			$minute  = str_pad($time[4],2,"0",STR_PAD_LEFT);
			$second  = str_pad($time[5],2,"0",STR_PAD_LEFT);
			$msec = $nsec = $misec ='00';
			$requestTime=$year.",".$month.",".$day.",".$hour.",".$minute.",".$second.",".$msec.",".$nsec.",".$misec;
			if($requestXml)
			{
				$requestSerializeXml = serialize($requestXml);
				$requestSerializeXml = $this->conn->sql_escape_string($requestSerializeXml);
			}
			else
			{
				$requestSerializeXml = $requestXml;			
			}
		
			$insertApiRequestSql = "INSERT INTO 
									apiRequest 
											(apiRequestId, 
											requestType, 
											requestXml, 
											requestTime, 
											md5CheckSum,
											state) 
									VALUES 
											('',
											 '$requestType',
											 '$requestSerializeXml',
											 '$requestTime',
											 '$md5CheckSum',
											 1)";
			
			$rsApiRequestSql = $this->conn->sql_query($insertApiRequestSql);
			$apiRequestId = $this->conn->sql_insert_id();
			if (!$rsApiRequestSql)
			{
			  # return error.
			  throw new Exception('apiRequest insertion');
			}
			return array('requestType' => 0, 'apiRequestId' => $apiRequestId);
		
		}
		else
		{
			$row = $this->conn->sql_fetch_object($rsMd5);
			$apiRequestId = $row->apiRequestId;
			return array('requestType' => 1, 'apiRequestId' => $apiRequestId);
		}
		
	}
	
	/*
	 * Function Name: insert_request_data
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

	public function insert_request_data($requestedData,$apiRequestId,$functionName)
	{
		
	// state : 1 = pending, 2 = in progress, 3 = success, 4 = failed
	// ConsistencyType : 1 = application consistency, 2 = Crash consistency
	$id = '';
	if($functionName == 'flushAndHoldWrites')
	{
		$requestedData = array('scsiId' => $requestedData, 'consistencyType' => 2, 'applicationName' => 0, 'bookmarkName' => '');
	}
	elseif($functionName == 'RefreshHostInfo')
	{
		$id = $requestedData['HostGUID'];
	}
	elseif($functionName == 'UpgradeAgents')
	{
		$requestedData = array("HostGUID" => $requestedData);		
	}
	elseif($functionName == 'InsertScriptPolicy')
	{
		$requestedData = array("PolicyId" => $requestedData);
	}
	elseif($functionName == 'PauseReplicationPairs' || $functionName == 'RemoveProtection' || $functionName == 'SendAlerts')
	{
		$requestedData = $requestedData;
	}
	else
	{
		$requestedData = array('scsiId' => $requestedData);
	}
	
	$requestdata = serialize($requestedData);
	$insertApiRequestDetailSql = "INSERT INTO 
									apiRequestDetail 
											(apiRequestDetailId, 
											apiRequestId, 
											requestData, 
											state, 
											status, 
											lastUpdateTime,
											hostId) 
									VALUES 
											('',
											 '$apiRequestId',
											 '".$this->conn->sql_escape_string($requestdata)."',
											 1,
											 '',
											 now(),
											 '$id')";
		
		
		$rsApiRequestDetailSql = $this->conn->sql_query($insertApiRequestDetailSql);
		$result = $this->conn->sql_insert_id();
		if (!$rsApiRequestDetailSql)
		{
		  # return error.
		  throw new Exception('apiRequestDetail insertion');
		}
		return $result;
	}

	public function get_request_data($requestApiId)
	{
		$select_sql = "SELECT 
							apiRequestDetailId
						FROM
							apiRequestDetail
						WHERE 
							apiRequestId = ?";
		$result = $this->conn->sql_query($select_sql, array($requestApiId));  
		for($i=0; isset($result[$i]); $i++)
		{
			$requestDetailId[] = $result[$i]['apiRequestDetailId'];
		}
		return $requestDetailId;						
	}
	
	public function get_api_request_data($request_id)
	{
		$sql = "SELECT
					requestdata
				FROM
					apiRequestDetail
				WHERE
					apiRequestId = ?";
		$result = $this->conn->sql_query($sql, array($request_id));
		for($i=0; isset($result[$i]); $i++)
		{
			$request_data = $result[$i]['requestdata'];
		}
		return $request_data;
	}
	
	public function update_api_request_data($request_id,$state,$error)
	{
		$error = trim($error);
		$sql = "UPDATE
					apiRequest
				SET
					state = ?
				WHERE
					apiRequestId = ?";
		$rs = $this->conn->sql_query($sql, array($state, $request_id));
		if(count($rs) > 0)
			$sql = "UPDATE
						apiRequestDetail
					SET
						state = ?,
						status = ?
					WHERE
						apiRequestId = ?";
			$rs = $this->conn->sql_query($sql, array($state, $error, $request_id));
		}
}
?>