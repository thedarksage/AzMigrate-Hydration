<?
/*
 *================================================================= 
 * FILENAME
 *    compression_off_for_a2e.php
 *
 * DESCRIPTION
 *    This script used to do the compression off for A2E in the work flow from Source to MT.
 *        
 *
 */
error_reporting(E_ALL & ~E_NOTICE);
ini_set('memory_limit', '512M');
date_default_timezone_set(@date_default_timezone_get());
$install_dir_value = read_installation_directory_data();
global $CS_INSTALLATION_PATH;
 $CS_INSTALLATION_PATH = $install_dir_value["INSTALLATION_PATH"];
$constants_file = $CS_INSTALLATION_PATH."\\home\svsystems\\admin\\web\\app\\Constants.php";
include_once $constants_file;
$config_file = $REPLICATION_ROOT.'admin'.$SLASH.'web'.$SLASH.'config.php';
#print $config_file;
include_once $config_file;

try
{
	$conn = @mysqli_connect($DB_HOST, $DB_USER, $DB_PASSWORD, $DB_DATABASE_NAME)or die("MYSQLI connection failed.");
	if(!$conn)
	{
		migratedebugLog("Compression off data migration ::mysqli connection failed");
	}
	
			$multi_query = array();

			#$multi_query['update'][] = "update srcLogicalVolumeDestLogicalVolume s, logicalVolumes l set s.compressionEnable = 0 where s.destinationDeviceName = l.deviceName and l.isImpersonate=1";

			#$multi_query['update'][] = "UPDATE applicationScenario SET scenarioDetails = REPLACE(scenarioDetails,'\"compression\";i:1','\"compression\";i:0') WHERE scenarioDetails REGEXP '\"protectionPath\";s:3:\"E2A\"'";	

			$multi_query['update'][] = "update srcLogicalVolumeDestLogicalVolume s, applicationScenario a set s.compressionEnable = 0, a.scenarioDetails = REPLACE(a.scenarioDetails,'\"compression\";i:1','\"compression\";i:0') where s.planId = a.planId and a.scenarioDetails REGEXP '\"protectionPath\";s:3:\"E2A\"'";			
			
			//print_r($multi_query);
			if(sizeof($multi_query['update']))
			{
				$transaction_status = dobatchupdates_imp($multi_query);
				if($transaction_status)
				{
					#echo "Compression off data migration :: Failed\n";
					migratedebugLog("Compression off data migration :: Failed");
					exit(999);
				}
				else
				{
					migratedebugLog("Compression off data migration :: Success");
					exit(0);
				}
			}
}
catch(Exception $e)
{
	migratedebugLog("Compression off data migration ::::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
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
	

function migratedebugLog ($debugString)
{
	global $CS_INSTALLATION_PATH;
	$log_path = $CS_INSTALLATION_PATH."\\home\svsystems\\var\\";
  $PHPDEBUG_FILE_PATH =  $log_path."migrate_compress_flags.log";
 
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
}

function read_installation_directory_data()
{
	$dirpath = getenv('ProgramData');	
    $result = array();
    if ($dirpath)
    {
        $installDirFile = join(DIRECTORY_SEPARATOR, array($dirpath, 'Microsoft Azure Site Recovery', 'Config', 'App.conf'));
        if (file_exists($installDirFile))
        {
            $file = fopen ($installDirFile, 'r');
            $conf = fread ($file, filesize($installDirFile));
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
    }
	
	return $result;
}

?>