<?PHP
/*
 *================================================================= 
 * FILENAME
 *  PrismConfigurator.php
 *
 * DESCRIPTION
 *  This script contains all the Prism background API
 *  calls.	
 *
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/


class PrismConfigurator
{
	public $conn;
	function __construct()
	{
		global $conn_obj;
		
		$this->conn = $conn_obj;
	}
	
	/*
    * Function Name: get_lun_size
    *
    * Description:
    * This API will return lun capacity related sanlun.
    *  
    *
    * Parameters: 
    *			$lunId
    *    
    * Return Value:
    *     Array
    *
    */	
	public function get_lun_size($sharedDeviceId)
	{
		global $logging_obj;
		$sqlStmnt = "SELECT
					    capacity
					FROM
					    sharedDevices
					WHERE
					    sharedDeviceId = '$sharedDeviceId'";
		$rs = $this->conn->sql_query($sqlStmnt);								  
		if (!$rs)
		{
		
		/*
		*  report error
		*/
			return 0;
		}
	
		$row = $this->conn->sql_fetch_row($rs);
		return $row[0];
	}
	

	public function get_prism_settings($obj,$final_hash=array())
	{
		$host_id = $obj['host_id'];
		global $logging_obj;
		
		$getPrismSettings = array();
		
		$logging_obj->my_error_handler("DEBUG","get_prism_settings \n host_id = $host_id \n\n getPrismSettings :: ".print_r($getPrismSettings,true)." \n");
		return $getPrismSettings;
	}
	
	
	/*
	* Function Name: get_volume_level_info
	*
	* Description:
	* This function will return the volume info based on host id and
	* sharedDeviceId from logicalVolume table.
	*  
	* Parameters:
	*     [IN]: $host_id,$sharedDeviceId
	*    
	* Return Value:
	*     Ret Value: Integer
	*
	* Exceptions:
	*     
	*/		
	public function get_volume_level_info($host_id,$sharedDeviceId,$srcDeviceName,$option)
	{
		$sqlStmnt = "SELECT 
							volumeType  
						FROM
							logicalVolumes 
						WHERE 
							deviceName = '$srcDeviceName' AND 
							Phy_Lunid ='$sharedDeviceId'";
							
		// option 1 for At lun creation and 2 for mirror setting.
		if($option == 1)
		{
			// don't add host id
		}
		else if($option == 2)
		{
			$sqlStmnt = $sqlStmnt." AND hostId = '$host_id'";
		}
		
		
	
		$res = $this->conn->sql_query($sqlStmnt);
		if($this->conn->sql_num_row($res)>0)
		{
			$sqlRow = $this->conn->sql_fetch_array($res);
			$volumeType = $sqlRow["volumeType"];
			
			$selectSql = "SELECT 
								startingPhysicalOffset 
							FROM
								logicalVolumes 
							WHERE 
								volumeType = '$volumeType' AND 
								Phy_Lunid ='$sharedDeviceId'";
			if($option == 1)
			{
				// don't add host id
			}
			else if($option == 2)
			{
				$selectSql = $selectSql." AND hostId = '$host_id'";
			}
			
		
			$result = $this->conn->sql_query($selectSql);
			$row = $this->conn->sql_fetch_array($result);
			$startingPhysicalOffset = $row["startingPhysicalOffset"];
		}
		else
		{
			$startingPhysicalOffset = "0";
		}
		return $startingPhysicalOffset;
	}
	
	
	public function isScsiIdProtected($hostId,$scsiId, $host_shared_device , $logical_volumes_arary)
	{
		global $logging_obj;
		$commonfunctions_obj = new CommonFunctions();
		if(is_array($host_shared_device))
		{
			foreach ($host_shared_device as $key => $details)
			{
				
				if(array_key_exists($details['srcDeviceName'] , $logical_volumes_arary) && $logical_volumes_arary[$details['srcDeviceName']]['Phy_Lunid'] == $scsiId && $details['sharedDeviceId'] == $logical_volumes_arary[$details['srcDeviceName']]['Phy_Lunid'])
				{
					return $logical_volumes_arary[$details['srcDeviceName']]['volumeType'];
				}
				else
				{
					$selectScenarioDetails = "SELECT
										scenarioDetails,
										protectionDirection
								FROM
										srcLogicalVolumeDestLogicalVolume sldl,
										applicationScenario aps
								WHERE
										sldl.Phy_Lunid = '$scsiId' AND
										sldl.scenarioId = aps.scenarioId";

						$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected>>:".$selectScenarioDetails);	
						$rs1 = $this->conn->sql_query($selectScenarioDetails);
						$numRowsSrc = $this->conn->sql_num_row($rs1);
						$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected:else,numRowsSrc:".$numRowsSrc);							
						if ($numRowsSrc)
						{
						
							$row1 = $this->conn->sql_fetch_object($rs1);
							$cdata = $row1->scenarioDetails;
							$cdata_arr = $commonfunctions_obj->getArrayFromXML($cdata);
							$scenario_data = unserialize($cdata_arr['plan']['data']['value']);
							$protectionDirection = $row1->protectionDirection;
							$logging_obj->my_error_handler("DEBUG","isScsiIdProtected:protectionDirection>>".$protectionDirection);	
								
							if($protectionDirection =='' || $protectionDirection =='forward')
							{
									$logging_obj->my_error_handler("DEBUG","isScsiIdProtected:protectionDirection>>if");	
									$pairInfo =  $scenario_data['pairInfo'];
							}
							else
							{
									$logging_obj->my_error_handler("DEBUG","isScsiIdProtected:protectionDirection>>else");	
									$pairInfo =  $scenario_data['reversepairInfo'];
							}
							#
							# need to check hostid and sharedDevice is protected or not in session since we are not 
							# getting hostid and sharedDeviceId in hostSharedDeviceMapping.
							
							$serializePairInfo = serialize($pairInfo);
							$logging_obj->my_error_handler("DEBUG","isScsiIdProtected:serializePairInfo>>".$serializePairInfo);	
							$hostIdPos = strpos($serializePairInfo, $hostId);
							$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected:hostIdPos>>".$hostIdPos);	
							if ($hostIdPos) 
							{
								$sharedDevicePos = strpos($serializePairInfo, $scsiId);
								$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected:sharedDevicePos>>".$sharedDevicePos);	
								if ($sharedDevicePos) 
								{
									$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected:volumeType");	
									foreach($pairInfo as $key=>$data)
									{
										foreach($data['pairDetails'] as $vol=>$details)
										{
											$volumeType = $details['prismSpecificInfo']['volumeType'];
											$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected:volumeType".$volumeType);	
											return $volumeType;
										}
									}		
								} 
								else 
								{
									$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected:returning false");	
									return;
								}	
									
							} 
							else 
							{
								$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected,node not available:returning false");	
								return;
							}
						}
						else
						{
							$logging_obj->my_error_handler("DEBUG"," isScsiIdProtected:pair not protected,returning false");	
							return;
						}
				}
			}
		}
		return ;
	}
	
	public function get_sync_device($sharedDeviceId)
	{
		$hatlmSqlStmnt = "
						SELECT distinct srcDeviceName 
						FROM 
							hostApplianceTargetLunMapping  
						WHERE 
							sharedDeviceId = '$sharedDeviceId' 
						";
		$resHatlmSqlStmnt = $this->conn->sql_query($hatlmSqlStmnt);	
		if($this->conn->sql_num_row($resHatlmSqlStmnt)>0)
		{
			$row = $this->conn->sql_fetch_array($resHatlmSqlStmnt);
			$srcDeviceName = $row["srcDeviceName"];
		}
		else
		{
			$srcDeviceName = '';
		}
	
		return $srcDeviceName;	
	}
}
?>
