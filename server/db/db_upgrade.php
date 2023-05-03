<?
/*
 *================================================================= 
 * FILENAME
 *    db_upgrade.php
 *
 * DESCRIPTION
 *    This script used to do update Database values.
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
		migratedebugLog("db_upgrade ::mysqli connection failed");
	}
	
	$multi_query = array();
	
	$multi_query['update'][] = "ALTER TABLE infrastructurevms MODIFY COLUMN `OS` enum('WINDOWS','LINUX','SOLARIS','OTHER') NOT NULL DEFAULT 'WINDOWS' COMMENT 'WINDOWS, LINUX, SOLARIS, OTHER'";
	
	//As refreshHostInfo request ids are overwriting in hosts table refershStatus field because of bug #11468176, hence, such requests making it failed after CS upgrade done. So, next time whenever DRA makes a request to CS for the same MT for refreshHostInfo, CS generates the new request id.
	 $sql = "select apiRequestId from apiRequest where requestType = 'RefreshHostInfo' and state in (1,2)";
	 if ($result = mysqli_query($conn, $sql)) 
	 {
		/* fetch associative array */
		while ($obj = mysqli_fetch_object($result)) 
		{
			$api_request_id = $obj->apiRequestId;
			$sql = "select id from hosts where refreshStatus = '$api_request_id' and agentRole='MasterTarget'";
			$result_set = mysqli_query($conn, $sql);
			$num_rows = mysqli_num_rows($result_set);
			if ($num_rows == 0)
			{
				$multi_query['update'][] = "update apiRequest set state = '4' where apiRequestId = '$api_request_id'";
				$multi_query['update'][] = "update apiRequestDetail set state = '4', Status='Request id is $api_request_id has been overwritten, hence, marked this as failed request as part of CS upgrade db script.' where apiRequestId = '$api_request_id'";
			}
		}
		/* free result set */
		mysqli_free_result($result);
	}
	
	$host_ntw_sql = "SELECT
						hostId,
						macAddress
					FROM
						hostNetworkAddress";
	$host_data = mysqli_query($conn,$host_ntw_sql) or migratedebugLog("SELECT hostId, macAddress FROM hostNetworkAddress ".@mysqli_error($conn));
	$host_ntw_data = array();
	while ($host_obj = mysqli_fetch_object($host_data))
	{
		$host_ntw_data[$host_obj->hostId][] = $host_obj->macAddress;
	}
	migratedebugLog("SELECT from hostNetworkAddress".print_r($host_ntw_data, true));
	
	$infra_sql = "SELECT 
					infrastructureVMId,
					hostId,
					macAddressList
				FROM 
					infrastructureVMs
				WHERE
					hostId != ''
				AND
					macAddressList = ''";
	$physical_infra_data = mysqli_query($conn,$infra_sql) or migratedebugLog("select usepsnatip, id from hosts query failed  ".@mysqli_error($conn));
	$physical_host_infra_data = array();
	
	while ($physical_obj = mysqli_fetch_object($physical_infra_data))
	{
		$mac_address = join(",", array_unique($host_ntw_data[$physical_obj->hostId]));
		$multi_query['update'][] = "UPDATE infrastructureVMs SET macAddressList = '$mac_address' WHERE infrastructureVMId = '".$physical_obj->infrastructureVMId."'";
	}
	
	//Api requests pruning interval has been changed from 1month to 3months.
	$multi_query['update'][] = "update transbandSettings set ValueData = '7776000' where ValueName = 'CLEANUP_TIME_LIMIT'";
	
	// Modifying category for error code ECA0140 from pushinstall to API Errors.
	$multi_query['update'][] = "update eventCodes set category = 'API Errors' where errCode = 'ECA0140'";
	
	//Updating discoveryType as AZURE, for the Azure to on-premise protections. This column used to filter the Azure discovery records and to avoid those records in IP based comparison for IP retain case to work for physical machine discovery purpose.
	$multi_query['update'][] = "update infrastructureVms set discoveryType = 'AZURE' where hostId in (select distinct sourceHostId from srclogicalvolumedestlogicalvolume where TargetDataPlane = '1') and hostDetails = ''";
	
	// Modifying column length to support longer health issue.
	$multi_query['update'][] = "ALTER TABLE healthfactors MODIFY healthFactorCode varchar(255) not null";

	// Modifying column length to support longer health issue.
	$multi_query['update'][] = "ALTER TABLE healthfactors MODIFY errCode varchar(255) not null";
	
	//Modifying column length from varchar to text.
	$multi_query['update'][] = "alter table accounts MODIFY COLUMN userName text";
	
	//Modifying column length from varchar to text.
	$multi_query['update'][] = "alter table accounts MODIFY COLUMN password text";
	
	//Modifying column length to support longer error code string.
	$multi_query['update'][] = "alter table dashboardalerts MODIFY errCode varchar(255) not null DEFAULT ''";
	
	//Modifying column length to support longer error code string.
	$multi_query['update'][] = "alter table eventCodes MODIFY errCode varchar(255) not null";
	
	//Modifying column length from text to longtext.
	$multi_query['update'][] = "alter table policyinstance MODIFY COLUMN executionLog LONGTEXT";
	
	//Deleting hard coded data record from users table.
	$multi_query['delete'][] = "delete from users where UID = '1000000000'";

	//print_r($multi_query);
	if(sizeof($multi_query['update']))
	{
		$transaction_status = dobatchupdates_imp($multi_query);
		if($transaction_status)
		{
			#echo "Compression off data migration :: Failed\n";
			migratedebugLog("db_upgrade :: Failed");
			exit(999);
		}
		else
		{
			migratedebugLog("db_upgrade :: Success");
			exit(0);
		}
	}
}
catch(Exception $e)
{
	migratedebugLog("db_upgrade ::::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
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
	migratedebugLog("DEBUG :: db_upgrade MIGRATE_INFO :: doBatchUpdates::started::  ". print_r($multi_query_list,TRUE));
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
				migratedebugLog("DEBUG ::db_upgrade MIGRATE_INFO :: doBatchUpdates::started executing the query $batch_query \n");

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
					
					migratedebugLog("DEBUG ::db_upgrade MIGRATE_INFO ::doBatchUpdates:: transaction status is:: $transaction_status \n");
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
			migratedebugLog("DEBUG ::db_upgrade MIGRATE_INFO ::doBatchUpdates:: Committed with status $commit_status\n");
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
  $PHPDEBUG_FILE_PATH =  $log_path."db_upgrade.log";
 
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