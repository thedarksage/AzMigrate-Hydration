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
	$conn = @mysqli_connect($DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME)or die("MYSQLI connection failed.");
	if(!$conn)
	{
		migratedebugLog("retention_db_migration ::mysqli connection failed");
	}
	
	if(get_cs_version() < 7)
	{		
		$multi_query = array();

		$sql = "DESC srcLogicalVolumeDestLogicalVolume";
		$result=@mysqli_query($conn,$sql) or migratedebugLog("DESC srcLogicalVolumeDestLogicalVolume query failed  ".@mysqli_error($conn));

		$src_temp_filed_array = array(
						'pairId'=>'false');

		while ($src_temp_obj = @mysqli_fetch_object($result))
		{
			if($src_temp_obj->Field == 'pairId')
			{
				$src_temp_filed_array['pairId'] = 'true';
			}
		}

		//Perform miration process if pair identity column is not available in the pair table
		if($src_temp_filed_array['pairId'] == "false")
		{
			//removing stale entries from retentionTimeStamp dbtable
			$sql = "DELETE 
					FROM 
						retentionTimeStamp 
					WHERE 
						ruleId 
					NOT IN (SELECT 
								ruleId 
							FROM 
								srcLogicalVolumeDestLogicalVolume)";

			$result = @mysqli_query($conn,$sql) or migratedebugLog("DELETE FROM retentionTimeStamp WHERE ruleId NOT IN query failed  ".@mysqli_error($conn));

			//removing stale entries from retentionTag dbtable
			$sql = "DELETE 
					FROM 
						retentionTag 
					WHERE 
						ruleId 
					NOT IN 
						(SELECT 
							ruleId 
						FROM 
							srcLogicalVolumeDestLogicalVolume)";

			$result = @mysqli_query($conn,$sql) or migratedebugLog("DELETE FROM retentionTag WHERE ruleId NOT IN query failed  ".@mysqli_error($conn));
			
			$sql = "ALTER TABLE 
						srcLogicalVolumeDestLogicalVolume 
					ADD 
						pairId INT(11) DEFAULT '0'";
			@mysqli_query($conn,$sql) or  migratedebugLog("ALTER TABLE srcLogicalVolumeDestLogicalVolume add pairId query execution failed  ".@mysqli_error($conn));

			$alter_sql = "ALTER TABLE 
							srcLogicalVolumeDestLogicalVolume 
						ADD 
							KEY pairId (`pairId`)";
			$rs_alter_sql = @mysqli_query($conn,$alter_sql) or migratedebugLog("ALTER TABLE srcLogicalVolumeDestLogicalVolume ADD KEY pairId query failed  ".@mysqli_error($conn));

			$sql = "DESC retentionTimeStamp";
			$result = @mysqli_query($conn,$sql) or migratedebugLog("DESC retentionTimeStamp query failed  ".@mysqli_error($conn));
			$retention_timestamp_temp_filed_array = array(
							'pairId'=>'false');
			while ($retention_timestamp_temp_obj = @mysqli_fetch_object($result))
			{
				if($retention_timestamp_temp_obj->Field == 'pairId')
				{
					$retention_timestamp_temp_filed_array['pairId'] = 'true';
				}
			}

			if($retention_timestamp_temp_filed_array['pairId'] == "false")
			{
				$sql = "ALTER TABLE 
							retentionTimeStamp 
						ADD 
							pairId INT(11) DEFAULT '0'";
				@mysqli_query($conn,$sql) or  migratedebugLog("ALTER TABLE retentionTimeStamp add pairId query execution failed  ".@mysqli_error($conn));
			}

			$sql = "DESC retentionTag";
			$result = @mysqli_query($conn,$sql) or migratedebugLog("DESC retentionTag query failed  ".@mysqli_error($conn));
			$retention_tag_temp_filed_array = array(
							'pairId'=>'false');
			while ($retention_tag_temp_obj = @mysqli_fetch_object($result))
			{
				if($retention_tag_temp_obj->Field == 'pairId')
				{
					$retention_tag_temp_filed_array['pairId'] = 'true';
				}
			}

			if($retention_tag_temp_filed_array['pairId'] == "false")
			{
				$sql = "ALTER TABLE retentionTag ADD pairId INT(11) DEFAULT '0'";
				@mysqli_query($conn,$sql) or  migratedebugLog("ALTER TABLE retentionTag add pairId query execution failed  ".@mysqli_error($conn));
			}
			
			$sql_resync = "SELECT 
								DISTINCT 
									destinationHostId,
									destinationDeviceName,
									pairId
								FROM 
									srcLogicalVolumeDestLogicalVolume 
								WHERE 
									profilingMode !='1'";
			$rs_src = @mysqli_query($conn,$sql_resync);

			while($row = @mysqli_fetch_object($rs_src))
			{
				if($row->pairId == 0 || $row->pairId == '')
				{
					$dest_hostid = $row->destinationHostId;
					$dest_device = $row->destinationDeviceName;
					$pair_id = get_new_pairId($conn);
				
					$multi_query['update'][] = "UPDATE 
													srcLogicalVolumeDestLogicalVolume 
												SET 
													pairId = '$pair_id'
												WHERE 
													destinationHostId = '$dest_hostid'
												AND
													destinationDeviceName = '".addslashes($dest_device)."'";
				}
			}

			$sql_profiler = "SELECT 
									sourceHostId,
									sourceDeviceName,
									ruleId,
									pairId
								FROM
									srcLogicalVolumeDestLogicalVolume
								WHERE
									profilingMode =1";
			$rs_pro = @mysqli_query($conn,$sql_profiler);
			$profiler_details = array();
			while($row = @mysqli_fetch_object($rs_pro))
			{
				$profiler_details[$row->ruleId]['src_hostid'] = $row->sourceHostId;
				$profiler_details[$row->ruleId]['src_device'] = $row->sourceDeviceName;
				$profiler_details[$row->ruleId]['rule_id'] = $row->ruleId;
				$profiler_details[$row->ruleId]['pair_id'] = $row->pairId;
			}

			//print_r($profiler_details);
			foreach($profiler_details as $details)
			{
				if($details['pair_id'] == 0 || $details['pair_id'] == '')
				{
					$pair_id = get_new_pairId($conn);
					$sql_cluster_hostid = "SELECT 
												clusterId 
											FROM 
												clusters 
											WHERE 
												hostId = '".$details['src_hostid']."' 
											AND 
												deviceName = '".addslashes($details['src_device'])."'";
					
					$rs_clu_hostid = @mysqli_query($conn,$sql_cluster_hostid);

					if($row_cluster_hosts = @mysqli_fetch_object($rs_clu_hostid))
					{
						//This block is for cluster profiler pairs
						$sql_cluster_hosts = "SELECT DISTINCT 
													hostId
												FROM 
													clusters 
												WHERE 
													clusterId = '".$row_cluster_hosts->clusterId."'";

						$rs_cluster_hosts = @mysqli_query($conn,$sql_cluster_hosts);
						$cluster_hosts = array();					
						while($row_cluster_hosts = @mysqli_fetch_object($rs_cluster_hosts))
						{
							$cluster_hosts[] = $row_cluster_hosts->hostId;					
						}
						$cluster_host_ids = '';
						$cluster_host_ids = implode("','",$cluster_hosts);
						if($cluster_host_ids != '')
						{
							$multi_query['update'][] = "UPDATE 
															srcLogicalVolumeDestLogicalVolume 
															SET 
																pairId = '$pair_id'
															WHERE 
																sourceHostId IN('$cluster_host_ids')
															AND	
																sourceDeviceName = '".addslashes($details['src_device'])."'";
							
							
							$sql_rule_ids = "SELECT 
													ruleId 
												FROM 
													srcLogicalVolumeDestLogicalVolume 
												WHERE 
													sourceHostId IN('$cluster_host_ids')
												AND	
													sourceDeviceName = '".addslashes($details['src_device'])."'";
							
							while($row_rule_ids = @mysqli_fetch_object($sql_rule_ids))
							{
								$profiler_details[$row_rule_ids->ruleId]['pair_id'] = $pair_id;
							}
						}
					}
					else
					{
						//This block is for non-cluster profiler pairs
						$pair_id = get_new_pairId($conn);
						$details['pairId'] = $pair_id;
						
						$multi_query['update'][] = "UPDATE 
															srcLogicalVolumeDestLogicalVolume 
														SET 
															pairId = '$pair_id'
														WHERE 
															ruleId = '".$details['rule_id']."'";
					}
				}
			}

			//print_r($multi_query);
			if(sizeof($multi_query['update']))
			{
				$transaction_status = dobatchupdates_imp($multi_query);
				if($transaction_status)
				{
					echo "Retention data migration failed\n";
					migratedebugLog("Retention data migration failed");
					exit();
				}
				else
				{
					$update_ts = "UPDATE 
										retentionTag rt , srcLogicalVolumeDestLogicalVolume src 
									SET 
										rt.pairId=src.pairId 
									WHERE
										rt.ruleId = src.ruleId";
					$rs_ts = @mysqli_query($conn,$update_ts) or migratedebugLog("assign pairId to records in pair table query failed  ".@mysqli_error($conn));
					if($rs_ts)
					{
						//echo "\nPair id assigned to retentionTag successfully";
						#migratedebugLog("DEBUG ::MIGRATE_INFO Pair id assigned to retentionTag successfully");
						
						//----------------------------------------
						//Dumping the unique data to  retentionTag
						//-----------------------------------------
						$tmp_time = time();
						//Creating temp table with unique records
						$create_tmp_tbl = "CREATE TABLE 
											retentionTag_$tmp_time 
												SELECT 
													HostId,
													deviceName,
													tagTimeStamp,
													appName,
													userTag,
													paddedTagTimeStamp,
													accuracy,
													tagGuid,
													comment,
													tagVerfiication,
													tagTimeStampUTC,
													pairId,
													CONCAT(pairId,'_',tagTimeStampUTC,'_',appName,'_',tagGuid) as unique_tag
												FROM 
													retentionTag
												GROUP BY 
													unique_tag";
						$rs_create_tmp_tbl = @mysqli_query($conn,$create_tmp_tbl) or migratedebugLog("create table to retentionTag_temp query failed  ".@mysqli_error($conn));
						
						$delete_ts = "DELETE 
										FROM 
											retentionTag_$tmp_time 
										WHERE 
											appName='' OR tagTimeStampUTC=''";	
						$rs_del_ts = @mysqli_query($conn,$delete_ts) or migratedebugLog("DELETE FROM retentionTag WHERE appName query failed  ".@mysqli_error($conn));
						
						$del_tag_sql = "ALTER TABLE 
											retentionTag_$tmp_time 
										DROP 
											COLUMN unique_tag";
						$rs_alter_del_sql = @mysqli_query($conn,$del_tag_sql) or migratedebugLog("ALTER TABLE retentionTag DROP COLUMN unique_tag query failed  ".@mysqli_error($conn));	

						//Adding indexes to the temp table

						$alter_sql = "ALTER TABLE 
										retentionTag_$tmp_time 
									ADD 
										KEY HostId (`HostId`)";
						$rs_alter_sql = @mysqli_query($conn,$alter_sql) or migratedebugLog("ALTER TABLE retentionTag ADD KEY HostId query failed  ".@mysqli_error($conn));

						$alter_sql = "ALTER TABLE 
										retentionTag_$tmp_time 
									ADD 
										KEY deviceName (`deviceName`)";
						$rs_alter_sql = @mysqli_query($conn,$alter_sql) or migratedebugLog("ALTER TABLE retentionTag ADD KEY deviceName query failed  ".@mysqli_error($conn));

						$alter_sql = "ALTER TABLE 
										retentionTag_$tmp_time 
									ADD 
										CONSTRAINT PRIMARY KEY  (`pairId`,`tagTimeStampUTC`,`appName`,`tagGuid`)";
						$rs_alter_sql = @mysqli_query($conn,$alter_sql)  or migratedebugLog("ALTER TABLE retentionTag ADD CONSTRAINT PRIMARY KEY query failed  ".@mysqli_error($conn));
						
						//taking backup of original table
						$rename_old = "RENAME TABLE 
										retentionTag TO retentionTag_bkp_$tmp_time";
						$rs_rename = @mysqli_query($conn,$rename_old) or migratedebugLog("RENAME table retentionTag query failed  ".@mysqli_error($conn));
						
						//renaming temp table to retentionTag with unique records
						$rename_temp = "RENAME TABLE 
											retentionTag_$tmp_time TO retentionTag";
						$rs_rename = @mysqli_query($conn,$rename_temp) or migratedebugLog("RENAME table TO retentionTag query failed  ".@mysqli_error($conn));
						
						$rename_temp = "ALTER TABLE retentionTag ENGINE = InnoDB";
						$rs_rename = @mysqli_query($conn,$rename_temp) or migratedebugLog("ALTER TABLE retentionTag ENGINE query failed  ".@mysqli_error($conn));			
					}
					else
					{
						echo "\nProblem in updating retentionTag table";
						migratedebugLog("Problem in updating retentionTag table");
					}
						
					$update_tag = "UPDATE 
										retentionTimeStamp rt , srcLogicalVolumeDestLogicalVolume src 
									SET 
										rt.pairId=src.pairId 
									WHERE
										rt.ruleId = src.ruleId";

					$rs_tag = @mysqli_query($conn,$update_tag) or migratedebugLog("assign pairId to records in retentionTimeStamp table query failed  ".@mysqli_error($conn));
					if($rs_ts)
					{
						#echo "\nPair id assigned to retentionTimeStamp table successfully";
						#migratedebugLog("Pair id assigned to retentionTimeStamp table successfully");
						
						//-----------------------------------------------
						//Dumping the unique data to  retentionTimeStamp
						//-----------------------------------------------
						
						//Creating temp table with unique records
						$create_tmp_ts_tbl = "CREATE TABLE 
												retentionTimeStamp_$tmp_time 
												SELECT 
													hostId, 
													deviceName, 
													StartTime, 
													EndTime, 
													StartTimeUTC, 
													EndTimeUTC, 
													accuracy, 
													pairId, 
													concat(StartTimeUTC,'_',accuracy,'_',pairId) as unique_timestamp 
												FROM  
													retentionTimeStamp 
												GROUP BY 
													unique_timestamp";
						$rs_tmp_tag = @mysqli_query($conn,$create_tmp_ts_tbl) or migratedebugLog("CREATE TEMP retentionTimeStamp table query failed  ".@mysqli_error($conn));

						$drop_col = "ALTER TABLE 
										retentionTimeStamp_$tmp_time 
									DROP COLUMN unique_timestamp";	
						$rs_add_col = @mysqli_query($conn,$drop_col) or migratedebugLog("ALTER TABLE retentionTimeStamp DROP COLUMN unique_timestamp query failed  ".@mysqli_error($conn));
						
						//Adding indexes to the temp table
						$add_col = "ALTER TABLE 
										retentionTimeStamp_$tmp_time 
									ADD 
										COLUMN `id` BIGINT(20) NOT NULL AUTO_INCREMENT PRIMARY KEY";	
						$rs_add_col = @mysqli_query($conn,$add_col) or migratedebugLog("ALTER TABLE retentionTimeStamp ADD COLUMN `id` BIGINT(20) query failed  ".@mysqli_error($conn));

						$alter_sql ="ALTER TABLE 
										retentionTimeStamp_$tmp_time 
									ADD 
										CONSTRAINT UNIQUE KEY `pairid_starttimeutc_accuracy` (`pairId`,`StartTimeUTC`,`accuracy`)";
						$rs_alter_sql = @mysqli_query($conn,$alter_sql) or migratedebugLog("ALTER TABLE retentionTimeStamp ADD CONSTRAINT UNIQUE KEY query failed  ".@mysqli_error($conn));
					
						$alter_sql = "ALTER TABLE 
										retentionTimeStamp_$tmp_time 
									ADD 
										KEY `hostId` (`hostId`)";
						$rs_alter_sql = @mysqli_query($conn,$alter_sql) or migratedebugLog("ALTER TABLE retentionTimeStamp ADD KEY hostId query failed  ".@mysqli_error($conn));

						$alter_sql = "ALTER TABLE 
										retentionTimeStamp_$tmp_time 
									ADD 
										KEY `EndTimeUTC` (`EndTimeUTC`)";
						$rs_alter_sql = @mysqli_query($conn,$alter_sql) or migratedebugLog("ALTER TABLE retentionTimeStamp ADD KEY EndTimeUTC query failed  ".@mysqli_error($conn));
						
						
						//taking backup of original table
						
						$rename_old = "RENAME table retentionTimeStamp TO retentionTimeStamp_bkp_$tmp_time";
						$rs_rename = @mysqli_query($conn,$rename_old) or migratedebugLog("RENAME table retentionTimeStamp query failed  ".@mysqli_error($conn));
						
						//renaming temp table to retentionTimeStamp with unique records
						$rename_temp = "RENAME table retentionTimeStamp_$tmp_time TO retentionTimeStamp";
						$rs_rename = @mysqli_query($conn,$rename_temp) or migratedebugLog("RENAME table TO retentionTimeStamp query failed  ".@mysqli_error($conn));
						
						$rename_temp = "ALTER TABLE retentionTimeStamp ENGINE = InnoDB";
						$rs_rename = @mysqli_query($conn,$rename_temp) or migratedebugLog("ALTER TABLE retentionTimeStamp ENGINE query failed  ".@mysqli_error($conn));

					}
					else
					{
						echo "Problem in updating retentionTag table\n";
						migratedebugLog("Problem in updating retentionTag table\n");
					}
				}
			}
			else
			{
				migratedebugLog("DEBUG ::MIGRATE_INFO :: Replication pairs doesn't exist");
			}
		}
		else
		{
			//Pair Identity already exists
			#echo "\nPair Identity already exists \n";
			migratedebugLog('DEBUG ::MIGRATE_INFO :: Pair Identity already exists');
		}
		
		//This fix is added for 6.2+update3 (logRotationPolicy is having multiple foreign keys)
		$drop_tbl = "DROP TABLE logRotationPolicy";
		$rs_rename = @mysqli_query($conn,$drop_tbl) or migratedebugLog("DROP TABLE logRotationPolicy query failed  ".@mysqli_error($conn));
		
		//This fix is to handle 5.5 to 6.2.1 to 7.0 (throwing error while droping index
		// 'applicationHosts_fileServerInfo' on fileServerInfo)
		$key_exists_sql = "SHOW INDEXES FROM fileServerInfo WHERE KEY_NAME = 'applicationInstanceId'";
		migratedebugLog("DEBUG ::MIGRATE_INFO :: key_exists_sql - $key_exists_sql");
		$rs_key_exists = @mysqli_query($conn,$key_exists_sql) or migratedebugLog("SHOW INDEXS FROM fileServerInfo query failed  ".@mysqli_error($conn));
		$num_rows = @mysqli_num_rows($rs_key_exists);
		if(!$num_rows)
		{
			$add_index_sql = "ALTER TABLE `fileServerInfo` ADD INDEX `applicationInstanceId` (`applicationInstanceId`)";
			migratedebugLog("DEBUG ::MIGRATE_INFO :: add_index_sql - $add_index_sql");
			$rs_key_exists = @mysqli_query($conn,$add_index_sql) or migratedebugLog("ALTER TABLE `fileServerInfo` ADD INDEX `applicationInstanceId` (`applicationInstanceId`) query failed  ".@mysqli_error($conn));
		}
	}
	
	$deleteGuid = "DELETE FROM transbandSettings WHERE ValueName = 'GUID'";
	$templates_result = @mysqli_query($conn, $deleteGuid) or migratedebugLog("DELETE FROM  transbandSettings query failed ".@mysqli_error($conn));
}
catch(Exception $e)
{
	migratedebugLog("Exception occured while processing retention db migration script Message::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
}

function get_new_pairId($conn)
{
	$eids = array ();
	$query = "SELECT
					pairId
				FROM
					srcLogicalVolumeDestLogicalVolume";
	$result_set = @mysqli_query($conn,$query );
	while ($row = @mysqli_fetch_object($result_set))
	{
		$eids [] = $row->pairId;
	}

	$random = mt_rand (1, mt_getrandmax());
	while (in_array ($random, $eids))
	{
		$random = mt_rand (1, mt_getrandmax());
	}

	return $random;
}

function dobatchupdates_imp($multi_query_list)
{
    global $DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME;
	$return_status = 1;	
	$conn_i = @mysqli_connect($DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME)or die("MYSQLI connection failed.");
	
	if(!$conn_i)
	{
		migratedebugLog("doBatchUpdates::mysqli connection failed");
	}
	$multi_query_list = constructBatches($multi_query_list);
	migratedebugLog("DEBUG :: MIGRATE_INFO :: doBatchUpdates::started::  ". print_r($multi_query_list,TRUE));
	if (is_array($multi_query_list))
	{
	if (count($multi_query_list) > 0)
	{
		try
		{
			$transaction_status = FALSE;

			mysqli_multi_query($conn_i,"BEGIN");
			foreach($multi_query_list as $key=>$batch_query)
			{
				migratedebugLog("DEBUG ::MIGRATE_INFO :: doBatchUpdates::started executing the query $batch_query \n");

				if ($transaction_status = mysqli_multi_query($conn_i,$batch_query))
				{
				   do {
					   #migratedebugLog("DEBUG ::MIGRATE_INFO ::check1\n");
					   /* store first result set */
					   if ($result = mysqli_store_result($conn_i))
					   {
						   #migratedebugLog("DEBUG ::MIGRATE_INFO ::check2\n");
						   //do nothing since there's nothing to handle
						   mysqli_free_result($result);
					   }
					   #migratedebugLog("DEBUG ::MIGRATE_INFO ::check3\n");
					   /* print divider */
					   if (mysqli_more_results($conn_i))
					   {
							#migratedebugLog("DEBUG ::MIGRATE_INFO ::check4\n");
						   //I just kept this since it seems useful
						   //try removing and see for yourself
					   }
				   } while (mysqli_next_result($conn_i));
					
					migratedebugLog("DEBUG ::MIGRATE_INFO ::doBatchUpdates:: transaction status is:: $transaction_status \n");
				}
				if (!$transaction_status)
				{
					throw new Exception("Transaction failed from first batch","999");
				}
			}
		}
		catch(Exception $e)
		{
			migratedebugLog("doBatchUpdates::  Message::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
			$transaction_status = FALSE;
			$return_status = 1;
		}
		/*Transaction to update the database and based on that send the response as true or false*/
		if($transaction_status)
		{
			$commit_status = mysqli_multi_query($conn_i,"COMMIT");
			migratedebugLog("DEBUG ::MIGRATE_INFO ::doBatchUpdates:: Commited  with status $commit_status\n");
			if ($commit_status)
			{
				$return_status =  0;
			}
		}
		else
		{
			$rollback_status = mysqli_multi_query($conn_i,"ROLLBACK");
			migratedebugLog("doBatchUpdates:: Rolled back  with status $rollback_status\n");
			$return_status =  1;
		}
	}
	}
	return (bool)$return_status;
}

function constructBatches($batch_update_queries)
{	
	$multi_query_list = array();
	$insert_batches = array();
	$update_batches = array();
	$delete_batches = array();
	
	foreach($batch_update_queries as $key=>$value)
	{
		switch($key)
		{
			case "insert":
				$insert_batches = getBatchQueries($value);				
			break;
			case "update":
				$update_batches = getBatchQueries($value);
			break;
			case "delete":
				$delete_batches = getBatchQueries($value);
			break;
			default:
			break;			
		}
	}
	
	$multi_query_list = array_merge($insert_batches,$update_batches,$delete_batches);
	return $multi_query_list;
}

function getBatchQueries($multi_queries)
{		
	$partition_strings = array();
	#8MB defined as allowed data packet size for group queries execution.
	$packet_size = 8388608;

	$multi_queries = array_unique($multi_queries);
	$records_count = count($multi_queries);

	if ($records_count > 0)
	{
		$queries_string = implode(";",$multi_queries);
		$total_records_size = strlen($queries_string);
		$chunk_size = ceil($records_count/($total_records_size/$packet_size));
		$partitions = array_chunk($multi_queries,$chunk_size);
		foreach($partitions as $key=>$value)
		{
			$partition_strings[] = implode(";",$value);
		}
	}

	return $partition_strings;
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
  $PHPDEBUG_FILE_PATH =  $log_path."migrate_recovery_data_to_".trim($version).".log";
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
  $return_check_one = strripos($debugString, 'MIGRATE_INFO');
  if(!$return_check_one)
  {
	  exit();
  }
}

?>
