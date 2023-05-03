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
		throw new Exception ("mysqli connection failed");
		#migratedebugLog("doBatchUpdates::mysqli connection failed");
	}

	//Migrate bandwidth data
	if (preg_match('/Windows/i', php_uname()))
	{
		$installation_dir = "c:\\home\\svsystems";
		$rrdcommand = "C:\\rrdtool\\rrdtool.exe";
		$SLASH = "\\";
	}
	else
	{
		$installation_dir = "/home/svsystems";
		$rrdcommand = $installation_dir."/rrd/bin/rrdtool";
		$SLASH = "/";
	}
	
	$create_bw_tbl_sql = "CREATE TABLE IF NOT EXISTS `bandwidthReport` (
				  `hostId` varchar(255) NOT NULL DEFAULT '',
				  `reportDate` date NOT NULL DEFAULT '0000-00-00',
				  `dataIn` double NOT NULL DEFAULT '0',
				  `dataOut` double NOT NULL DEFAULT '0',
				  UNIQUE KEY `hostId_reportDate` (`hostId`,`reportDate`)
				) ENGINE=InnoDB DEFAULT CHARSET=latin1";
			
	@mysqli_query($conn,$create_bw_tbl_sql);
	
	$sql = "SELECT id,unix_timestamp() as end_time,unix_timestamp(date_sub(now(),interval 1 year)) as start_time FROM hosts";

	$result = @mysqli_query($conn,$sql);
	if(!$result) throw new Exception ("SELECT query failed ".@mysqli_error($conn));
	
	while($row = @mysqli_fetch_object($result))
	{
		$hosts[] = $row->id;
		$end_timestamp = $row->end_time;
		$start_timestamp = $row->start_time;
	}
	
	#$start_timestamp = ($end_timestamp) ? $end_timestamp - 31536000;
	
	foreach($hosts as $host_id)
	{
		$bw_rrd = $installation_dir.$SLASH.$host_id.$SLASH."bandwidth.rrd";
		$insert_sql = array();
		$bw_data = array();
		
		if(file_exists($bw_rrd))
		{
			if($fp = popen("$rrdcommand fetch " . 
			$bw_rrd . 
			" AVERAGE -r 864000 -s $start_timestamp -e $end_timestamp", 'r'))
			{				
				while(!feof($fp))
				{
					$line = trim(fgets($fp, 4096));
					$in = $out = 0;
				
					if($line != '')
					{
						list($date, $in, $out) = preg_split('/( )+/', $line);
						list($date) = preg_split('/:/', $date);
						
						if($lastdate != 0 && (is_numeric($in) || is_numeric($out)))
						{
							$in = (is_numeric($in)) ? $in : 0;
							$out = (is_numeric($out)) ? $out : 0;
							
							$day = date('Y-m-d', $date);
							$bw_data[$day]['in']  += $in;
							$bw_data[$day]['out'] += $out;
						}
						$lastdate = $date;
					}
				}
				
				pclose($fp);
				
				if(sizeof($bw_data))
				{	
					foreach($bw_data as $day_data=>$val)
					{
						$bw_date = $day_data;
						$in_data = $val['in'];
						$out_data = $val['out'];
						
						$insert_sql['insert'][] = "INSERT 
												INTO 
												bandwidthReport
												VALUES 
													('$host_id',
													'$bw_date',
													'$in_data',
													'$out_data')";
					}
				}
				
				if(sizeof($insert_sql))
				{
					$transaction_status = dobatchupdates_imp($insert_sql);
					if($transaction_status)
					{
						echo "Bandwidth data migration failed\n";
						migratedebugLog("MIGRATE_INFO :: Bandwidth data migration failed for hostid - $host_id");
						exit();
					}
				}
			}
		}
	}
}
catch(Exception $e)
{
	migratedebugLog("Error while migrating report data ::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
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
						   mysqli_free_result($result);
					   }
					   #migratedebugLog("DEBUG ::MIGRATE_INFO ::check3\n");
					   
					   if (mysqli_more_results($conn_i))
					   {
							#migratedebugLog("DEBUG ::MIGRATE_INFO ::check4\n");
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