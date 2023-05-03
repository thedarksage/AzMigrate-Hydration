<?php
error_reporting(E_ALL & ~E_NOTICE);
ini_set('memory_limit', '512M');
date_default_timezone_set(@date_default_timezone_get());

if (preg_match('/Windows/i', php_uname()))
{
  $constants_file = "..\\admin\\web\\app\\Constants.php";
  $config_file = "..\\admin\\web\\config.php"; 	
}
else
{
  $constants_file = "/home/svsystems/admin/web/app/Constants.php";
  $config_file    = "/home/svsystems/admin/web/config.php";
}

include_once $constants_file;
include_once $config_file;

try
{
	$conn = @mysqli_connect($DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME)or die("MYSQLI connection failed.");
	if(!$conn)
	{
		migratedebugLog("volume_resize_psnatip_db_migration ::mysqli connection failed");
	}

	$desc_vol_resize_sql = "desc volumeResizeHistory";
	$desc_vol_resize_rs = @mysqli_query($conn, $desc_vol_resize_sql) or migratedebugLog("desc volumeResizeHistory query failed  ".@mysqli_error($conn));
	$volumeResizeHistory_array = array('pairId' => 'false');
	while ($volumeResizeHistory_obj = @mysqli_fetch_object($desc_vol_resize_rs))
	{	 
		if($volumeResizeHistory_obj->Field == 'pairId')
		{  
			$volumeResizeHistory_array['pairId'] = "true";
		}
	}
	if($volumeResizeHistory_array['pairId'] == "false")
	{
		 $alter_vol_resize_sql = "ALTER TABLE volumeResizeHistory ADD pairId int(11) NOT NULL DEFAULT '0'";
		 @mysqli_query($conn, $alter_vol_resize_sql) or migratedebugLog("ALTER TABLE volumeResizeHistory ADD pairId query execution failed  ".@mysqli_error($conn));
	}
	
	$vol_resize_sql = "SELECT src.sourceHostId, src.sourceDeviceName, MAX(vrh.resizeTime) as resizeTime , vrh.oldCapacity, vrh.newCapacity, vrh.newCapacity, vrh.isValid, src.pairId FROM srcLogicalVolumeDestLogicalVolume src, volumeResizeHistory vrh WHERE src.sourceHostId = vrh.hostId AND src.sourceDeviceName = vrh.deviceName AND vrh.isValid = 1 GROUP BY src.sourceHostId, src.sourceDeviceName, src.destinationHostId, src.destinationDeviceName ORDER BY src.sourceHostId, src.sourceDeviceName";

	$vol_resize_result = @mysqli_query($conn, $vol_resize_sql) or migratedebugLog("$vol_resize_sql query failed ".@mysqli_error($conn));

	if($vol_resize_result)
	{
		$host_id = $device_name = '';
		
		while($vol_resize_obj = @mysqli_fetch_object($vol_resize_result))
		{
			if($host_id == $vol_resize_obj->sourceHostId && $device_name == $vol_resize_obj->sourceDeviceName)
			{
				$host_id =	$vol_resize_obj->sourceHostId;
				$device_name = $vol_resize_obj->sourceDeviceName;
				$device_escaped = addslashes($device_name);
				$resize_time = $vol_resize_obj->resizeTime;
				$old_capacity = $vol_resize_obj->oldCapacity;
				$new_capacity = $vol_resize_obj->newCapacity;
				$is_valid = $vol_resize_obj->isValid;
				$pair_id = $vol_resize_obj->pairId;
				
				$insert_update_vol_resize_sql = "INSERT INTO volumeResizeHistory VALUES ('$host_id', '$device_escaped', '$resize_time', '$old_capacity', '$new_capacity', '$is_valid', '$pair_id')";
			}
			else
			{
				$host_id =	$vol_resize_obj->sourceHostId;
				$device_name = $vol_resize_obj->sourceDeviceName;
				$device_escaped = addslashes($device_name);
				$pair_id = $vol_resize_obj->pairId;
				
				$insert_update_vol_resize_sql = "UPDATE volumeResizeHistory SET pairId= '$pair_id' WHERE hostId= '$host_id' AND deviceName= '$device_escaped'";
			}
			$insert_update_vol_resize_result = @mysqli_query($conn, $insert_update_vol_resize_sql) or migratedebugLog("$insert_update_vol_resize_sql query failed ".@mysqli_error($conn));
			
			migratedebugLog("insert_update_vol_resize_result :: $insert_update_vol_resize_sql \n");
		}
	}
	
	$sql = "DESC srcLogicalVolumeDestLogicalVolume";
	$result=@mysqli_query($conn,$sql) or migratedebugLog("DESC srcLogicalVolumeDestLogicalVolume query failed  ".@mysqli_error($conn));

	$natip_fields_array = array('usePsNatIpForSource'=>'false', 'usePsNatIpForTarget'=>'false');
	
	while ($data_obj = @mysqli_fetch_object($result))
	{
		if($data_obj->Field == 'usePsNatIpForSource')
		{
			$natip_fields_array['usePsNatIpForSource'] = 'true';
		}
		if($data_obj->Field == 'usePsNatIpForTarget')
		{
			$natip_fields_array['usePsNatIpForTarget'] = 'true';
		}
	}
	
	if($natip_fields_array['usePsNatIpForSource'] == "false")
	{
		$sql = "ALTER TABLE 
						srcLogicalVolumeDestLogicalVolume 
					ADD 
						usePsNatIpForSource tinyint(1)  NOT NULL DEFAULT '0'";
		@mysqli_query($conn,$sql) or  migratedebugLog("ALTER TABLE srcLogicalVolumeDestLogicalVolume add usePsNatIpForSource query execution failed  ".@mysqli_error($conn));
	}
	
	if($natip_fields_array['usePsNatIpForTarget'] == "false")
	{
		$sql = "ALTER TABLE 
						srcLogicalVolumeDestLogicalVolume 
					ADD 
						usePsNatIpForTarget tinyint(1)  NOT NULL DEFAULT '0'";
		@mysqli_query($conn,$sql) or  migratedebugLog("ALTER TABLE srcLogicalVolumeDestLogicalVolume add usePsNatIpForTarget query execution failed  ".@mysqli_error($conn));
	}
	
	$natip_selection_sql = "select id from hosts where name != 'InMageProfiler' and usepsnatip = 1";
	$natip_result=@mysqli_query($conn,$natip_selection_sql) or migratedebugLog("select usepsnatip, id from hosts query failed  ".@mysqli_error($conn));
	$hostid_array = array();
	while ($natip_obj = @mysqli_fetch_object($natip_result))
	{
		$hostid_array[] = $natip_obj->id;	
	}
	$hostid_array = array_unique($hostid_array);
	$hostids = implode("','", $hostid_array);
	
	if($hostids)
	{
		$update_src_nat_query = "UPDATE
							srcLogicalVolumeDestLogicalVolume
						SET
							usePsNatIpForSource = 1
						WHERE
							sourceHostId IN('$hostids')
						AND
							processServerId IN(SELECT
													processServerId 
												FROM 
													processServer
												WHERE	
													ps_natip != '')";
		$natip_result=@mysqli_query($conn, $update_src_nat_query) or migratedebugLog("update usePsNatIpForSource flag in srcLogicalVolumeDestLogicalVolume table query :: $update_src_nat_query failed  ".@mysqli_error($conn));		
		
		$update_tgt_nat_query = "UPDATE
							srcLogicalVolumeDestLogicalVolume
						SET
							usePsNatIpForTarget = 1
						WHERE
							destinationHostId IN('$hostids')
						AND
							processServerId IN(SELECT
													processServerId 
												FROM 
													processServer
												WHERE	
													ps_natip != '')";
		$natip_result=@mysqli_query($conn, $update_tgt_nat_query) or migratedebugLog("update usePsNatIpForTarget flag in srcLogicalVolumeDestLogicalVolume table query :: $update_tgt_nat_query failed  ".@mysqli_error($conn));		
	}
	migratedebugLog("update_src_nat_query :: $update_src_nat_query,  update_tgt_nat_query :: $update_tgt_nat_query \n");
}
catch(Exception $e)
{
	migratedebugLog("Exception occured while processing resize and ps natip db migration script Message::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
}

function migratedebugLog ($debugString)
{
	if (preg_match('/Windows/i', php_uname()))
	{
	  $version_file_path = "c:\\home\\svsystems\\etc\\version";
	  $log_path = "c:\\home\\svsystems\\var\\";
	}
	else
	{
	  $version_file_path = "/home/svsystems/etc/version";
	  $log_path = "/home/svsystems/var/";
	}
	$version = `cat $version_file_path | grep -i inmage`;
	$PHPDEBUG_FILE_PATH =  $log_path."migrate_volume_resize_ps_natip_data_to_".trim($version).".log";
	# Some debug
	$fr = fopen($PHPDEBUG_FILE_PATH, 'a+');
	if(!$fr) 
	{
		#echo "Error! Couldn't open the file.";
	}
	if (!fwrite($fr, date('m-d-Y H:i:s')." - ".$debugString . "\n")) 
	{
		#print "Cannot write to file ($filename)";
	}
	if(!fclose($fr)) 
	{
		#echo "Error! Couldn't close the file.";
	}
}

?>