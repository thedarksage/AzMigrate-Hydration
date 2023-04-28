<?php
/*
 *================================================================= 
 * FILENAME
 *    retention_db_migration.php
 *
 * DESCRIPTION
 *    This script used to migrate retention data at CXdb.
 *        
 *
 * HISTORY
 *     03/08/2012  Author   Raghavendar Jella.
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
 */
error_reporting(E_ALL & ~E_NOTICE);
ini_set('memory_limit', '512M');
date_default_timezone_set(@date_default_timezone_get());

if (preg_match('/Windows/i', php_uname()))
{
  $constants_file = "..\\admin\\web\\app\\Constants.php";
}
else
{
  $constants_file = "/home/svsystems/admin/web/app/Constants.php";
}
include_once $constants_file;
$config_file = $REPLICATION_ROOT.'admin'.$SLASH.'web'.$SLASH.'config.php';
include_once $config_file;

try
{
	if(get_cs_version() < 7)
	{
		$conn = @mysqli_connect($DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME)or die("MYSQLI connection failed.");
		if(!$conn)
		{
			throw new Exception ("mysqli connection failed");
			#migratedebugLog("doBatchUpdates::mysqli connection failed");
		}

		$sql = "UPDATE 
						healthReport 
					SET 
						pairId = (SELECT 
										pairId 
									FROM 
										srcLogicalVolumeDestLogicalVolume 
									WHERE 
										sourceHostId = healthReport.sourceHostId 
									AND 
										sourceDeviceName=healthReport.sourceDeviceName 
									AND 
										destinationHostId = healthReport.destinationHostId 
									AND 
										destinationDeviceName = healthReport.destinationDeviceName 
									LIMIT 0,1)";

		$result = @mysqli_query($conn,$sql); 
		if(!$result) throw new Exception ("UPDATE healthReport query failed  ".@mysqli_error($conn));
		
		$sql = "UPDATE 
						healthReport 
					SET 
						planId = (SELECT 
										planId 
									FROM 
										srcLogicalVolumeDestLogicalVolume 
									WHERE 
										sourceHostId = healthReport.sourceHostId 
									AND 
										sourceDeviceName=healthReport.sourceDeviceName 
									AND 
										destinationHostId = healthReport.destinationHostId 
									AND 
										destinationDeviceName = healthReport.destinationDeviceName 
									LIMIT 0,1)";

		$result = @mysqli_query($conn,$sql); 
		if(!$result) throw new Exception ("UPDATE healthReport query failed  ".@mysqli_error($conn));
		
		#$sql = "DESC trendingData";
		#$result=@mysqli_query($conn,$sql) ;
		#if(!$result) throw new Exception ("DESC trendingData query failed  ".@mysqli_error($conn));

		$sql = "UPDATE 
					trendingData 
				SET 
					pairId = (SELECT 
									pairId 
								FROM 
									srcLogicalVolumeDestLogicalVolume 
								WHERE 
									sourceHostId = trendingData.sourceHostId 
								AND 
									sourceDeviceName=trendingData.sourceDeviceName 
								AND 
									destinationHostId = trendingData.destinationHostId 
								and 
									destinationDeviceName = trendingData.destinationDeviceName 
								LIMIT 0,1)";

		$result = @mysqli_query($conn,$sql) ;
		if(!$result) throw new Exception ("UPDATE trendingData query failed  ".@mysqli_error($conn));
		
		$tmp_time = time();
		
		$create_temp_sql = "CREATE TABLE healthReport_$tmp_time SELECT 
	sourceHostId,sourceDeviceName,destinationHostId,destinationDeviceName,reportDate,sum(dataChangeWithCompression) as dataChangeWithCompression,sum(dataChangeWithoutCompression) as dataChangeWithoutCompression,maxRPO,retentionWindow,protectionCoverage,rpoHealth,throttleHealth,retentionHealth,resyncHealth,replicationHealth,rpoTreshold,retention_policy_duration,availableConsistencyPoints,pairId,planId,concat(pairId,'_',reportDate) as date_pair from healthReport group by date_pair";
		
		$create_temp_sql_result = @mysqli_query($conn,$create_temp_sql) or migratedebugLog("CREATE TABLE healthReport_$tmp_time query failed  ".@mysqli_error($conn));
		
		$drop_col = "ALTER TABLE healthReport_$tmp_time DROP COLUMN date_pair";	
		$rs_add_col = @mysqli_query($conn,$drop_col) or migratedebugLog("ALTER TABLE healthReport DROP COLUMN date_pair query failed  ".@mysqli_error($conn));
		
		
		
		$sql = "SELECT 
					sourceHostId,
					sourceDeviceName,
					destinationHostId,
					destinationDeviceName,
					DATE_FORMAT(pairConfigureTime,'%Y-%m-%d') as pairConfigureTime,
					pairId
				FROM 
					srcLogicalVolumeDestLogicalVolume
				WHERE 
					oneToManySource=0
				GROUP BY pairId";
					
		$result = @mysqli_query($conn,$sql);
		if(!$result) throw new Exception ("SELECT query failed ".@mysqli_error($conn));
		
		while($row = @mysqli_fetch_object($result))
		{
			$primary_details_sql = "SELECT pairId from srcLogicalVolumeDestLogicalVolume where sourceHostId='".$row->sourceHostId."' AND sourceDeviceName='".$row->sourceDeviceName."' AND oneToManySource=1";
			
			$primary_details_rs = @mysqli_query($conn,$primary_details_sql);
			if(!$result) throw new Exception ("SELECT query failed ".@mysqli_error($primary_details_rs));
			
			if($primary_details = @mysqli_fetch_object($primary_details_rs))
			{
				$parimary_pair_id = $primary_details->pairId;
				
				$update_health_report_sql = "INSERT INTO healthReport_$tmp_time (sourceHostId,sourceDeviceName,destinationHostId,destinationDeviceName,reportDate,dataChangeWithCompression,dataChangeWithoutCompression,maxRPO,retentionWindow,protectionCoverage,rpoHealth,throttleHealth,retentionHealth,resyncHealth,replicationHealth,rpoTreshold,retention_policy_duration,availableConsistencyPoints,pairId,planId) SELECT sourceHostId,sourceDeviceName,destinationHostId,destinationDeviceName,reportDate,dataChangeWithCompression,dataChangeWithoutCompression,maxRPO,retentionWindow,protectionCoverage,rpoHealth,throttleHealth,retentionHealth,resyncHealth,replicationHealth,rpoTreshold,retention_policy_duration,availableConsistencyPoints,$parimary_pair_id,planId from healthReport_$tmp_time where pairId=".$row->pairId." AND reportDate >= '".$row->pairConfigureTime."'";
				
				migratedebugLog("MIGRATE_INFO :: update_health_report_sql - $update_health_report_sql\n");
				
				$update_result = @mysqli_query($conn,$update_health_report_sql);
				if(!$update_result) throw new Exception ("SQL Query failed ".@mysqli_error($conn));
			}
		}
		
		//Adding indexes to healthReport table
		$alter_sql = "ALTER TABLE healthReport_$tmp_time ADD KEY `sourceHostId` (`sourceHostId`)";
		$rs_alter_sql = @mysqli_query($conn,$alter_sql);
		if(!$rs_alter_sql) throw new Exception ("ALTER TABLE healthReport ADD KEY sourceHostId query failed  ".@mysqli_error($conn));
		
		$alter_sql = "ALTER TABLE healthReport_$tmp_time ADD KEY `sourceDeviceName` (`sourceDeviceName`)";
		$rs_alter_sql = @mysqli_query($conn,$alter_sql);
		if(!$rs_alter_sql) throw new Exception ("ALTER TABLE healthReport ADD KEY sourceDeviceName query failed  ".@mysqli_error($conn));
		
		$alter_sql = "ALTER TABLE healthReport_$tmp_time ADD KEY `destinationHostId` (`destinationHostId`)";
		$rs_alter_sql = @mysqli_query($conn,$alter_sql);
		if(!$rs_alter_sql) throw new Exception ("ALTER TABLE healthReport ADD KEY destinationHostId query failed  ".@mysqli_error($conn));
		
		$alter_sql = "ALTER TABLE healthReport_$tmp_time ADD KEY `destinationDeviceName` (`destinationDeviceName`)";
		$rs_alter_sql = @mysqli_query($conn,$alter_sql);
		if(!$rs_alter_sql) throw new Exception ("ALTER TABLE healthReport ADD KEY destinationDeviceName query failed  ".@mysqli_error($conn));
		
		$alter_sql = "ALTER TABLE healthReport_$tmp_time ADD KEY `reportDate` (`reportDate`)";
		$rs_alter_sql = @mysqli_query($conn,$alter_sql);
		if(!$rs_alter_sql) throw new Exception ("ALTER TABLE healthReport ADD KEY reportDate query failed  ".@mysqli_error($conn));
		
		$alter_sql = "ALTER TABLE healthReport_$tmp_time ADD KEY `pairId` (`pairId`)";
		$rs_alter_sql = @mysqli_query($conn,$alter_sql);
		if(!$rs_alter_sql) throw new Exception ("ALTER TABLE healthReport ADD KEY pairId query failed  ".@mysqli_error($conn));	
						
		$rename_old = "DROP table healthReport";
		$rs_rename = @mysqli_query($conn,$rename_old);
		if(!$rs_rename) throw new Exception ("RENAME table healthReport query failed  ".@mysqli_error($conn));
		
		//renaming temp table to healthReport
		$rename_temp = "RENAME table healthReport_$tmp_time TO healthReport";
		$rs_rename = @mysqli_query($conn,$rename_temp);
		if(!$rs_rename) throw new Exception ("RENAME table TO healthReport query failed  ".@mysqli_error($conn));
		
		
		
		$alter_tbl = "ALTER TABLE healthReport ENGINE = InnoDB";
		$rs_rename = @mysqli_query($conn,$alter_tbl);
		if(!$rs_rename) throw new Exception ("ALTER TABLE healthReport ENGINE query failed  ".@mysqli_error($conn));
	}
}
catch(Exception $e)
{
	migratedebugLog("Error while migrating report data ::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
}

//get cs version
function get_cs_version()
{
	$return_val = 0;
	$version_file = "/home/svsystems/etc/version";
	if(file_exists($version_file))
	{
		$fh=fopen($version_file,"r");
		if($fh)
		{
			while(($buffer = fgets($fh,1024)) != false)
			{
				$str_arr = explode("=",$buffer);
				if($str_arr[0] == 'VERSION')
				{
					$ver = explode('.',$str_arr[1]);
					$return_val =  intval($ver[0]);
					break;
				}
			}
		}
	}
	
	return $return_val;	
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
  $PHPDEBUG_FILE_PATH =  $log_path."migrate_report_data_to_".trim($version).".log";
  # Some debug
  $fr = fopen($PHPDEBUG_FILE_PATH, 'a+');
  if(!$fr) {
    #echo "Error! Couldn't open the file.";
  }
  if (!fwrite($fr, date('m-d-Y H:i:s')." - ".$debugString . "\n")) {
    #print "Cannot write to file ($filename)";
  }
  if(!fclose($fr)) {
    #echo "Error! Couldn't close the file.";
  }
  $return_check_one = strripos($debugString, 'INFO');
  if(!$return_check_one)
  {	  
	  exit();
  }
}

?>